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

#ifndef CYBER_SCHEDULER_POLICY_CLASSIC_CONTEXT_H_
#define CYBER_SCHEDULER_POLICY_CLASSIC_CONTEXT_H_

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "cyber/base/atomic_rw_lock.h"
#include "cyber/croutine/croutine.h"
#include "cyber/scheduler/common/cv_wrapper.h"
#include "cyber/scheduler/common/mutex_wrapper.h"
#include "cyber/scheduler/processor_context.h"

namespace apollo {
namespace cyber {
namespace scheduler {

static constexpr uint32_t MAX_PRIO = 20;

#define DEFAULT_GROUP_NAME "default_grp"

// clang-format off
// mark: 管理 CRoutine 上下文, 根据 CRoutine 的 group_name 和 priority 设计
//       CroutineContextManager 命名更贴切
//       一个 group_name 对应一个线程和协程池

// mark: 每个 priority 优先级课可能有多个协程
using CroutineQueue = std::vector<std::shared_ptr<CRoutine>>;  // mark:协程数组
using PriorityCroutineQueue = std::array<CroutineQueue, MAX_PRIO>; // mark:数组下标代表priority

// mark: 不同用途的协程组,<group_name, PriorityCroutineQueue>
using CroutinesGroup = std::unordered_map<std::string, PriorityCroutineQueue>;

using LockQueue = std::array<base::AtomicRWLock, MAX_PRIO>; // mark:数组下标代表priority
// mark: 不同用途的协程组的锁,<group_name, PriorityCroutineQueue>
using LockQueueGroup = std::unordered_map<std::string, LockQueue>;

// mark: <group_name, xxx>
using CvGroup = std::unordered_map<std::string, CvWrapper>;
using MutexGroup = std::unordered_map<std::string, MutexWrapper>;
using NotifyGroup = std::unordered_map<std::string, int>;  // mark: int类型counter, Notify()时计数器+1, Wait()唤醒时计数器-1
// clang-format on

// mark: 经典模式的每处理器线程的上下文
class ClassicContext : public ProcessorContext {
 public:
  ClassicContext();
  explicit ClassicContext(const std::string &group_name);

  std::shared_ptr<CRoutine> NextRoutine() override;
  void Wait() override;
  void Shutdown() override;

  static void Notify(const std::string &group_name);
  static bool RemoveCRoutine(const std::shared_ptr<CRoutine> &cr);

  // MARK: 都是static全局数据,每处理器线程访问需要保护
  alignas(CACHELINE_SIZE) static CroutinesGroup croutines_group_;
  alignas(CACHELINE_SIZE) static LockQueueGroup locks_group_;

  alignas(CACHELINE_SIZE) static CvGroup cv_group_;
  alignas(CACHELINE_SIZE) static MutexGroup mutex_group_;
  alignas(CACHELINE_SIZE) static NotifyGroup notify_group_;

 private:
  void InitGroup(const std::string &group_name);

  std::chrono::steady_clock::time_point wake_time_;
  bool need_sleep_ = false;

  // mark: 以下指针根据当前的 group_name 取数组的地址得到
  PriorityCroutineQueue *priority_croutines_ = nullptr;
  LockQueue *lock_queue_ = nullptr;  // mark: 保护 priority_croutines_

  MutexWrapper *mtx_wrapper_ = nullptr;  // mark: 同步 notify_group_
  CvWrapper *cv_wrapper_ = nullptr;      // mark: 同步 notify_group_

  std::string current_group_;
};

}  // namespace scheduler
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_SCHEDULER_POLICY_CLASSIC_CONTEXT_H_
