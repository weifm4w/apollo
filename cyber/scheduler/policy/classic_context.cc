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

#include "cyber/scheduler/policy/classic_context.h"

#include <limits>

namespace apollo {
namespace cyber {
namespace scheduler {

using apollo::cyber::base::AtomicRWLock;
using apollo::cyber::base::ReadLockGuard;
using apollo::cyber::base::WriteLockGuard;
using apollo::cyber::croutine::CRoutine;
using apollo::cyber::croutine::RoutineState;

alignas(CACHELINE_SIZE) MutexGroup ClassicContext::mutex_group_;
alignas(CACHELINE_SIZE) CvGroup ClassicContext::cv_group_;
alignas(CACHELINE_SIZE) LockQueueGroup ClassicContext::locks_group_;
alignas(CACHELINE_SIZE) CroutinesGroup ClassicContext::croutines_group_;
alignas(CACHELINE_SIZE) NotifyGroup ClassicContext::notify_group_;

ClassicContext::ClassicContext() { InitGroup(DEFAULT_GROUP_NAME); }

ClassicContext::ClassicContext(const std::string& group_name) {
  InitGroup(group_name);
}

void ClassicContext::InitGroup(const std::string& group_name) {
  priority_croutines_ = &croutines_group_[group_name];
  lock_queue_ = &locks_group_[group_name];
  mtx_wrapper_ = &mutex_group_[group_name];
  cv_wrapper_ = &cv_group_[group_name];
  notify_group_[group_name] = 0;
  current_group_ = group_name;
}

// mark: priority_croutines_ 插入Croutine的时机:SchedulerClassic::DispatchTask()

// mark: 协程调度,根据优先队列 priority_croutines_ 调度
std::shared_ptr<CRoutine> ClassicContext::NextRoutine() {
  if (cyber_unlikely(stop_.load())) {
    return nullptr;
  }

  for (int i = MAX_PRIO - 1; i >= 0; --i) {
    ReadLockGuard<AtomicRWLock> lk(lock_queue_->at(i));
    for (auto& cr : priority_croutines_->at(i)) {
      if (!cr->Acquire()) {
        continue;
      }

      // mark: 返回 READY 待运行状态的协程, 注意:上面已经 Acquire, 在返回处理完后要 Release
      if (cr->UpdateState() == RoutineState::READY) {
        return cr;
      }

      cr->Release();
    }
  }

  return nullptr;
}

void ClassicContext::Wait() {
  std::unique_lock<std::mutex> lk(mtx_wrapper_->Mutex());
  cv_wrapper_->Cv().wait_for(lk, std::chrono::milliseconds(1000), [&]() {
    return notify_group_[current_group_] > 0;
  });
  if (notify_group_[current_group_] > 0) {
    notify_group_[current_group_]--;
  }
}

void ClassicContext::Shutdown() {
  stop_.store(true);
  mtx_wrapper_->Mutex().lock();
  notify_group_[current_group_] = std::numeric_limits<unsigned char>::max();
  mtx_wrapper_->Mutex().unlock();
  cv_wrapper_->Cv().notify_all();
}

void ClassicContext::Notify(const std::string& group_name) {
  (&mutex_group_[group_name])->Mutex().lock();
  notify_group_[group_name]++;
  (&mutex_group_[group_name])->Mutex().unlock();
  cv_group_[group_name].Cv().notify_one();
}

bool ClassicContext::RemoveCRoutine(const std::shared_ptr<CRoutine>& cr) {
  auto grp = cr->group_name();
  auto prio = cr->priority();
  auto crid = cr->id();
  WriteLockGuard<AtomicRWLock> lk(ClassicContext::locks_group_[grp].at(prio));
  auto& croutines = ClassicContext::croutines_group_[grp].at(prio);
  for (auto it = croutines.begin(); it != croutines.end(); ++it) {
    if ((*it)->id() == crid) {
      auto cr = *it;
      cr->Stop();
      while (!cr->Acquire()) {
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        AINFO_EVERY(1000) << "waiting for task " << cr->name() << " completion";
      }
      croutines.erase(it);
      cr->Release();
      return true;
    }
  }
  return false;
}

}  // namespace scheduler
}  // namespace cyber
}  // namespace apollo
