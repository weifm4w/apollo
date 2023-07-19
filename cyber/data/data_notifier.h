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

#ifndef CYBER_DATA_DATA_NOTIFIER_H_
#define CYBER_DATA_DATA_NOTIFIER_H_

#include <memory>
#include <mutex>
#include <vector>

#include "cyber/common/log.h"
#include "cyber/common/macros.h"
#include "cyber/data/cache_buffer.h"
#include "cyber/event/perf_event_cache.h"
#include "cyber/time/time.h"

namespace apollo {
namespace cyber {
namespace data {

using apollo::cyber::Time;
using apollo::cyber::base::AtomicHashMap;
using apollo::cyber::event::PerfEventCache;

// mark:通知监听器
struct Notifier {
  std::function<void()> callback;
};

class DataNotifier {
 public:
  using NotifyVector = std::vector<std::shared_ptr<Notifier>>;
  ~DataNotifier() {}

  void AddNotifier(uint64_t channel_id,
                   const std::shared_ptr<Notifier>& notifier);

  bool Notify(const uint64_t channel_id);

 private:
  std::mutex notifies_map_mutex_;
  // mark: 所有需要通知的监听器
  AtomicHashMap<uint64_t, NotifyVector> notifies_map_;

  DECLARE_SINGLETON(DataNotifier)
};

inline DataNotifier::DataNotifier() {}

inline void DataNotifier::AddNotifier(
    uint64_t channel_id, const std::shared_ptr<Notifier>& notifier) {
  std::lock_guard<std::mutex> lock(notifies_map_mutex_);
  NotifyVector* notifies = nullptr;
  if (notifies_map_.Get(channel_id, &notifies)) {
    notifies->emplace_back(notifier);
  } else {
    NotifyVector new_notify = {notifier};
    notifies_map_.Set(channel_id, new_notify);
  }
}

inline bool DataNotifier::Notify(const uint64_t channel_id) {
  NotifyVector* notifies = nullptr;
  if (notifies_map_.Get(channel_id, &notifies)) {
    for (auto& notifier : *notifies) {
      if (notifier && notifier->callback) {
        // mark:通知监听器
        notifier->callback();
        /* Scheduler::CreateTask =>
          visitor->RegisterNotifyCallback([this, task_id]() {
            if (cyber_unlikely(stop_.load())) {
              return;
            }
            mark: 2.观察到新消息, 通知唤醒线程去处理 CRoutine
            this->NotifyProcessor(task_id);
          });
        */
      }
    }
    return true;
  }
  return false;
}

}  // namespace data
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_DATA_DATA_NOTIFIER_H_
