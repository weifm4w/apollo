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

#ifndef CYBER_CROUTINE_CROUTINE_H_
#define CYBER_CROUTINE_CROUTINE_H_

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include "cyber/common/log.h"
#include "cyber/croutine/detail/routine_context.h"

namespace apollo {
namespace cyber {
namespace croutine {

using RoutineFunc = std::function<void()>;
using Duration = std::chrono::microseconds;

enum class RoutineState { READY, FINISHED, SLEEP, IO_WAIT, DATA_WAIT };

class CRoutine {
 public:
  explicit CRoutine(const RoutineFunc &func);
  virtual ~CRoutine();

  // static interfaces
  static void Yield();
  static void Yield(const RoutineState &state);
  static void SetMainContext(const std::shared_ptr<RoutineContext> &context);
  static CRoutine *GetCurrentRoutine();
  static char **GetMainStack();

  // public interfaces
  bool Acquire();
  void Release();

  // It is caller's responsibility to check if state_ is valid before calling
  // SetUpdateFlag().
  void SetUpdateFlag();

  // acquire && release should be called before Resume
  // when work-steal like mechanism used
  RoutineState Resume();
  RoutineState UpdateState();
  RoutineContext *GetContext();
  char **GetStack();

  void Run();
  void Stop();
  void Wake();
  void HangUp();
  void Sleep(const Duration &sleep_duration);

  // getter and setter
  RoutineState state() const;
  void set_state(const RoutineState &state);

  uint64_t id() const;
  void set_id(uint64_t id);

  const std::string &name() const;
  void set_name(const std::string &name);

  int processor_id() const;
  void set_processor_id(int processor_id);

  uint32_t priority() const;
  void set_priority(uint32_t priority);

  std::chrono::steady_clock::time_point wake_time() const;

  void set_group_name(const std::string &group_name) {
    group_name_ = group_name;
  }

  const std::string &group_name() { return group_name_; }

 private:
  CRoutine(CRoutine &) = delete;
  CRoutine &operator=(CRoutine &) = delete;

  std::string name_;
  std::chrono::steady_clock::time_point wake_time_ =
      std::chrono::steady_clock::now();

  RoutineFunc func_;  // mark: 协程函数
  RoutineState state_;

  std::shared_ptr<RoutineContext> context_;  // mark: 协程上下文

  // mark:协程锁,Acquire()获取锁资源,Release()释放锁资源,协程被Acquire()之后再次被Acquire()会失败
  std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
  std::atomic_flag updated_ = ATOMIC_FLAG_INIT;  // mark:数据更新标识

  bool force_stop_ = false;

  int processor_id_ = -1;
  uint32_t priority_ = 0;
  uint64_t id_ = 0;

  std::string group_name_;

  // mark: 线程变量, 指向当前线程正在执行的协程对应的CRoutine对象
  static thread_local CRoutine *current_routine_;
  // mark: 线程变量, 保存主执行体的栈, 也就是系统栈
  static thread_local char *main_stack_;
};

// mark:
// Yield()负责把当前寄存器中的信息即协程执行上下文保存至context_中，然后把main_stack_中线程主体的上下文恢复到寄存器中，将控制权返回给主体调度线程
inline void CRoutine::Yield(const RoutineState &state) {
  auto routine = GetCurrentRoutine();
  routine->set_state(state);
  SwapContext(routine->GetStack(), GetMainStack());
}

// mark: 挂起当前协程
inline void CRoutine::Yield() {
  SwapContext(GetCurrentRoutine()->GetStack(), GetMainStack());
}

inline CRoutine *CRoutine::GetCurrentRoutine() { return current_routine_; }

inline char **CRoutine::GetMainStack() { return &main_stack_; }

inline RoutineContext *CRoutine::GetContext() { return context_.get(); }

inline char **CRoutine::GetStack() { return &(context_->sp); }

inline void CRoutine::Run() { func_(); }

inline void CRoutine::set_state(const RoutineState &state) { state_ = state; }

inline RoutineState CRoutine::state() const { return state_; }

inline std::chrono::steady_clock::time_point CRoutine::wake_time() const {
  return wake_time_;
}

inline void CRoutine::Wake() { state_ = RoutineState::READY; }

inline void CRoutine::HangUp() { CRoutine::Yield(RoutineState::DATA_WAIT); }

inline void CRoutine::Sleep(const Duration &sleep_duration) {
  wake_time_ = std::chrono::steady_clock::now() + sleep_duration;
  CRoutine::Yield(RoutineState::SLEEP);
}

inline uint64_t CRoutine::id() const { return id_; }

inline void CRoutine::set_id(uint64_t id) { id_ = id; }

inline const std::string &CRoutine::name() const { return name_; }

inline void CRoutine::set_name(const std::string &name) { name_ = name; }

inline int CRoutine::processor_id() const { return processor_id_; }

inline void CRoutine::set_processor_id(int processor_id) {
  processor_id_ = processor_id;
}

inline RoutineState CRoutine::UpdateState() {
  // Synchronous Event Mechanism
  if (state_ == RoutineState::SLEEP &&
      std::chrono::steady_clock::now() > wake_time_) {
    state_ = RoutineState::READY;
    return state_;
  }

  // Asynchronous Event Mechanism
  // mark: SetUpdateFlag()中clear(),说明有数据更新,state_:置为:READY
  if (!updated_.test_and_set(std::memory_order_release)) {
    if (state_ == RoutineState::DATA_WAIT || state_ == RoutineState::IO_WAIT) {
      state_ = RoutineState::READY;
    }
  }
  return state_;
}

inline uint32_t CRoutine::priority() const { return priority_; }

inline void CRoutine::set_priority(uint32_t priority) { priority_ = priority; }

inline bool CRoutine::Acquire() {
  return !lock_.test_and_set(std::memory_order_acquire);
}

inline void CRoutine::Release() {
  return lock_.clear(std::memory_order_release);
}

inline void CRoutine::SetUpdateFlag() {
  updated_.clear(std::memory_order_release);
}

}  // namespace croutine
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_CROUTINE_CROUTINE_H_
