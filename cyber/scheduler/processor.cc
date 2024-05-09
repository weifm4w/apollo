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

#include "cyber/scheduler/processor.h"

#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>

#include <chrono>

#include "cyber/common/global_data.h"
#include "cyber/common/log.h"
#include "cyber/croutine/croutine.h"
#include "cyber/scheduler/common/pin_thread.h"
#include "cyber/time/time.h"

namespace apollo {
namespace cyber {
namespace scheduler {

using apollo::cyber::common::GlobalData;

std::atomic<int> t_numb_{1};

Processor::Processor() { running_.store(true); }

Processor::~Processor() { Stop(); }

// mark: 每处理器线程
void Processor::Run() {
  auto tname = std::string("processor_") + std::to_string(t_numb_++);
  SET_THIS_THREAD_NAME(tname);
  tid_.store(static_cast<int>(syscall(SYS_gettid)));
  AFLOW << "processor_tid: " << tid_ << " tname: " << tname;
  snap_shot_->processor_id.store(tid_);

  while (cyber_likely(running_.load())) {
    if (cyber_likely(context_ != nullptr)) {
      // mark: 不停的消费协程任务,获取优先级最高并且准备就绪的协程
      auto croutine = context_->NextRoutine();  // 1.Acquire()协程加锁
      if (croutine) {
        snap_shot_->execute_start_time.store(cyber::Time::Now().ToNanosecond());
        snap_shot_->routine_name = croutine->name();
        croutine->Resume();   // 2.唤醒协程运行,成后释放协程
        croutine->Release();  // 3.Release()协程解锁
      } else {
        snap_shot_->execute_start_time.store(0);
        context_->Wait();  // 如果协程组中没有空闲的协程,则等待
      }
    } else {
      // 线程先创建运行,未BindContext时context_为nullptr,线程等待一会
      std::unique_lock<std::mutex> lk(mtx_ctx_);
      cv_ctx_.wait_for(lk, std::chrono::milliseconds(10));
    }
  }
}

void Processor::Stop() {
  if (!running_.exchange(false)) {
    return;
  }

  if (context_) {
    context_->Shutdown();
  }

  cv_ctx_.notify_one();
  if (thread_.joinable()) {
    thread_.join();
  }
}

void Processor::BindContext(const std::shared_ptr<ProcessorContext>& context) {
  context_ = context;
  // mark: 创建线程
  std::call_once(thread_flag_,
                 [this]() { thread_ = std::thread(&Processor::Run, this); });
}

std::atomic<pid_t>& Processor::Tid() {
  while (tid_.load() == -1) {
    cpu_relax();
  }
  return tid_;
}

}  // namespace scheduler
}  // namespace cyber
}  // namespace apollo
