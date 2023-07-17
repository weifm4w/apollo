/******************************************************************************
 * Copyright 2019 The Apollo Authors. All Rights Reserved.
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

#include "cyber/timer/timing_wheel.h"

#include <cmath>

#include "cyber/scheduler/common/pin_thread.h"
#include "cyber/task/task.h"

namespace apollo {
namespace cyber {

void TimingWheel::Start() {
  std::lock_guard<std::mutex> lock(running_mutex_);
  if (!running_) {
    ADEBUG << "TimeWheel start ok";
    running_ = true;
    tick_thread_ = std::thread([this]() { this->TickFunc(); });
    scheduler::Instance()->SetInnerThreadAttr("timer", &tick_thread_);
  }
}

void TimingWheel::Shutdown() {
  std::lock_guard<std::mutex> lock(running_mutex_);
  if (running_) {
    running_ = false;
    if (tick_thread_.joinable()) {
      tick_thread_.join();
    }
  }
}

void TimingWheel::Tick() {
  // mark: 取出内圈当前index任务队列
  auto& bucket = work_wheel_[current_work_wheel_index_];
  {
    std::lock_guard<std::mutex> lock(bucket.mutex());
    auto ite = bucket.task_list().begin();
    while (ite != bucket.task_list().end()) {
      auto task = ite->lock();
      if (task) {
        ADEBUG << "index: " << current_work_wheel_index_
               << " timer id: " << task->timer_id_;
        auto* callback =
            reinterpret_cast<std::function<void()>*>(&(task->callback));
        // mark: 丢到协程池中异步运行
        cyber::Async([this, callback] {
          if (this->running_) {
            (*callback)();
          }
        });
      }
      ite = bucket.task_list().erase(ite);
    }
  }
}

void TimingWheel::AddTask(const std::shared_ptr<TimerTask>& task) {
  AddTask(task, current_work_wheel_index_);
}

void TimingWheel::AddTask(const std::shared_ptr<TimerTask>& task,
                          const uint64_t current_work_wheel_index) {
  if (!running_) {
    Start();
  }
  auto work_wheel_index = current_work_wheel_index +
                          static_cast<uint64_t>(std::ceil(
                              static_cast<double>(task->next_fire_duration_ms) /
                              TIMER_RESOLUTION_MS));
  if (work_wheel_index >= WORK_WHEEL_SIZE) {
    // mark: index大于内圈最大index
    auto real_work_wheel_index = GetWorkWheelIndex(work_wheel_index);
    task->remainder_interval_ms = real_work_wheel_index;
    auto assistant_ticks = work_wheel_index / WORK_WHEEL_SIZE;
    if (assistant_ticks == 1 &&
        // mark: 在内圈本周期内可运行
        real_work_wheel_index < current_work_wheel_index_) {
      work_wheel_[real_work_wheel_index].AddTask(task);
      ADEBUG << "add task to work wheel. index :" << real_work_wheel_index;
    } else {
      // 超出内圈或者在内圈下个周期可运行
      auto assistant_wheel_index = 0;
      {
        std::lock_guard<std::mutex> lock(current_assistant_wheel_index_mutex_);
        assistant_wheel_index = GetAssistantWheelIndex(
            current_assistant_wheel_index_ + assistant_ticks);
        assistant_wheel_[assistant_wheel_index].AddTask(task);
      }
      ADEBUG << "add task to assistant wheel. index : "
             << assistant_wheel_index;
    }
  } else {
    // mark: index小于内圈最大index
    work_wheel_[work_wheel_index].AddTask(task);
    ADEBUG << "add task [" << task->timer_id_
           << "] to work wheel. index :" << work_wheel_index;
  }
}

void TimingWheel::Cascade(const uint64_t assistant_wheel_index) {
  auto& bucket = assistant_wheel_[assistant_wheel_index];
  std::lock_guard<std::mutex> lock(bucket.mutex());
  auto ite = bucket.task_list().begin();
  while (ite != bucket.task_list().end()) {
    auto task = ite->lock();
    if (task) {
      // mark: 取出外圈任务插入内圈
      work_wheel_[task->remainder_interval_ms].AddTask(task);
    }
    ite = bucket.task_list().erase(ite);
  }
}

void TimingWheel::TickFunc() {
  SET_THIS_THREAD_NAME("timing_wheel");
  Rate rate(TIMER_RESOLUTION_MS * 1000000);  // ms to ns
  while (running_) {
    Tick();  // mark: 运行内圈当前index任务队列
    // AINFO_EVERY(1000) << "Tick " << TickCount();
    tick_count_++;
    rate.Sleep();

    {
      std::lock_guard<std::mutex> lock(current_work_wheel_index_mutex_);
      current_work_wheel_index_ =
          GetWorkWheelIndex(current_work_wheel_index_ + 1);
    }

    if (current_work_wheel_index_ == 0) {
      // mark: 跑完内圈一圈,处理外圈
      {
        std::lock_guard<std::mutex> lock(current_assistant_wheel_index_mutex_);
        current_assistant_wheel_index_ =
            GetAssistantWheelIndex(current_assistant_wheel_index_ + 1);
      }
      Cascade(current_assistant_wheel_index_);
    }
  }
}

TimingWheel::TimingWheel() {}

}  // namespace cyber
}  // namespace apollo
