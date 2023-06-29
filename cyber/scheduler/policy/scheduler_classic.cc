/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "cyber/scheduler/policy/scheduler_classic.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "cyber/common/environment.h"
#include "cyber/common/file.h"
#include "cyber/scheduler/policy/classic_context.h"
#include "cyber/scheduler/processor.h"

namespace apollo {
namespace cyber {
namespace scheduler {

using apollo::cyber::base::ReadLockGuard;
using apollo::cyber::base::WriteLockGuard;
using apollo::cyber::common::GetAbsolutePath;
using apollo::cyber::common::GetProtoFromFile;
using apollo::cyber::common::GlobalData;
using apollo::cyber::common::PathExists;
using apollo::cyber::common::WorkRoot;
using apollo::cyber::croutine::RoutineState;

SchedulerClassic::SchedulerClassic() {
  std::string conf("conf/");
  conf.append(GlobalData::Instance()->ProcessGroup()).append(".conf");
  auto cfg_file = GetAbsolutePath(WorkRoot(), conf);

  apollo::cyber::proto::CyberConfig cfg;
  if (PathExists(cfg_file) && GetProtoFromFile(cfg_file, &cfg)) {
    for (auto& thr : cfg.scheduler_conf().threads()) {
      inner_thr_confs_[thr.name()] = thr;
    }

    if (cfg.scheduler_conf().has_process_level_cpuset()) {
      process_level_cpuset_ = cfg.scheduler_conf().process_level_cpuset();
      ProcessLevelResourceControl();
    }

    classic_conf_ = cfg.scheduler_conf().classic_conf();
    for (auto& group : classic_conf_.groups()) {
      auto& group_name = group.name();
      for (auto task : group.tasks()) {
        task.set_group_name(group_name);
        cr_confs_[task.name()] = task;
      }
    }
  } else {
    // if do not set default_proc_num in scheduler conf
    // give a default value
    uint32_t proc_num = 2;
    auto& global_conf = GlobalData::Instance()->Config();
    if (global_conf.has_scheduler_conf() &&
        global_conf.scheduler_conf().has_default_proc_num()) {
      proc_num = global_conf.scheduler_conf().default_proc_num();
    }
    task_pool_size_ = proc_num;

    auto sched_group = classic_conf_.add_groups();
    sched_group->set_name(DEFAULT_GROUP_NAME);
    sched_group->set_processor_num(proc_num);
  }

  // mark: 创建线程和配置调度策略
  CreateProcessor();
}

void SchedulerClassic::CreateProcessor() {
  for (auto& group : classic_conf_.groups()) {
    auto& group_name = group.name();
    auto proc_num = group.processor_num();
    if (task_pool_size_ == 0) {
      task_pool_size_ = proc_num;
    }

    auto& affinity = group.affinity();
    auto& processor_policy = group.processor_policy();
    auto processor_prio = group.processor_prio();
    std::vector<int> cpuset;
    ParseCpuset(group.cpuset(), &cpuset);

    for (uint32_t i = 0; i < proc_num; i++) {
      auto ctx = std::make_shared<ClassicContext>(group_name);
      processor_ctxs_.emplace_back(ctx);

      // mark: Processor 代码cpu核, 绑定 Croutine 的同时, 创建线程
      auto proc = std::make_shared<Processor>();
      proc->BindContext(ctx);  // mark: note 创建线程

      // mark: 配置线程cpu亲和性与调度策略
      SetSchedAffinity(proc->Thread(), cpuset, affinity, i);
      SetSchedPolicy(proc->Thread(), processor_policy, processor_prio,
                     proc->Tid());
      processors_.emplace_back(proc);
    }
  }
}

bool SchedulerClassic::DispatchTask(const std::shared_ptr<CRoutine>& cr) {
  // we use multi-key mutex to prevent race condition
  // when del && add cr with same crid
  MutexWrapper* wrapper = nullptr;
  if (!id_mutex_map_.Get(cr->id(), &wrapper)) {
    {
      std::lock_guard<std::mutex> wl_lg(id_mutex_map_mtx_);
      if (!id_mutex_map_.Get(cr->id(), &wrapper)) {
        wrapper = new MutexWrapper();
        id_mutex_map_.Set(cr->id(), wrapper);
      }
    }
  }
  std::lock_guard<std::mutex> lg(wrapper->Mutex());

  {
    WriteLockGuard<AtomicRWLock> lk(id_cr_lock_);
    if (id_cr_map_.find(cr->id()) != id_cr_map_.end()) {
      return false;
    }
    // mark: 注册 Croutine
    id_cr_map_[cr->id()] = cr;
  }

  if (cr_confs_.find(cr->name()) != cr_confs_.end()) {
    ClassicTask task = cr_confs_[cr->name()];
    cr->set_priority(task.prio());
    cr->set_group_name(task.group_name());
  } else {
    // croutine that not exist in conf
    cr->set_group_name(classic_conf_.groups(0).name());
  }

  if (cr->priority() >= MAX_PRIO) {
    AWARN << cr->name() << " prio is greater than MAX_PRIO[ << " << MAX_PRIO
          << "].";
    cr->set_priority(MAX_PRIO - 1);
  }

  // Enqueue task.
  {
    WriteLockGuard<AtomicRWLock> lk(
        ClassicContext::locks_group_[cr->group_name()].at(cr->priority()));
    // mark: 根据 group_name 和 priority 插入 Croutine
    ClassicContext::croutines_group_[cr->group_name()]
        .at(cr->priority())
        .emplace_back(cr);
  }

  // mark: 有新 Croutine 插入, 通知唤醒
  ClassicContext::Notify(cr->group_name());
  return true;
}

bool SchedulerClassic::NotifyProcessor(uint64_t crid) {
  if (cyber_unlikely(stop_)) {
    return true;
  }

  {
    ReadLockGuard<AtomicRWLock> lk(id_cr_lock_);
    if (id_cr_map_.find(crid) != id_cr_map_.end()) {
      auto cr = id_cr_map_[crid];
      if (cr->state() == RoutineState::DATA_WAIT ||
          cr->state() == RoutineState::IO_WAIT) {
        cr->SetUpdateFlag();
      }

      ClassicContext::Notify(cr->group_name());
      return true;
    }
  }
  return false;
}

bool SchedulerClassic::RemoveTask(const std::string& name) {
  if (cyber_unlikely(stop_)) {
    return true;
  }

  auto crid = GlobalData::GenerateHashId(name);
  return RemoveCRoutine(crid);
}

bool SchedulerClassic::RemoveCRoutine(uint64_t crid) {
  // we use multi-key mutex to prevent race condition
  // when del && add cr with same crid
  MutexWrapper* wrapper = nullptr;
  if (!id_mutex_map_.Get(crid, &wrapper)) {
    {
      std::lock_guard<std::mutex> wl_lg(id_mutex_map_mtx_);
      if (!id_mutex_map_.Get(crid, &wrapper)) {
        wrapper = new MutexWrapper();
        id_mutex_map_.Set(crid, wrapper);
      }
    }
  }
  std::lock_guard<std::mutex> lg(wrapper->Mutex());

  std::shared_ptr<CRoutine> cr = nullptr;
  {
    WriteLockGuard<AtomicRWLock> lk(id_cr_lock_);
    if (id_cr_map_.find(crid) != id_cr_map_.end()) {
      cr = id_cr_map_[crid];
      id_cr_map_[crid]->Stop();
      id_cr_map_.erase(crid);
    } else {
      return false;
    }
  }
  return ClassicContext::RemoveCRoutine(cr);
}

}  // namespace scheduler
}  // namespace cyber
}  // namespace apollo
