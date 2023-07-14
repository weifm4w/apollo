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

#include "cyber/scheduler/common/pin_thread.h"

#include <sched.h>
#include <sys/resource.h>

namespace apollo {
namespace cyber {
namespace scheduler {

#include <pthread.h>
#include <sys/syscall.h>

long ThisTid() {
  static thread_local long thread_id = syscall(SYS_gettid);
  return thread_id;
}

bool SetThreadName(pthread_t tid, const std::string& name) {
  char thread_name[16];
  memset(thread_name, 0, sizeof(thread_name));
  strncpy(thread_name, name.c_str(), sizeof(thread_name) - 1);
  int ret = pthread_setname_np(tid, thread_name);
  if (ret != 0) {
    AERROR << "SetThreadName: " << thread_name << " failed.";
    return false;
  }
  AINFO << "SetThreadName: " << thread_name << " success.";
  return true;
}

bool SetThisThreadName(const std::string& name) {
  return SetThreadName(pthread_self(), name);
}

void ParseCpuset(const std::string& str, std::vector<int>* cpuset) {
  std::vector<std::string> lines;
  std::stringstream ss(str);
  std::string l;
  while (getline(ss, l, ',')) {
    lines.push_back(l);
  }
  for (auto line : lines) {
    std::stringstream ss(line);
    std::vector<std::string> range;
    while (getline(ss, l, '-')) {
      range.push_back(l);
    }
    if (range.size() == 1) {
      cpuset->push_back(std::stoi(range[0]));
    } else if (range.size() == 2) {
      for (int i = std::stoi(range[0]), e = std::stoi(range[1]); i <= e; i++) {
        cpuset->push_back(i);
      }
    } else {
      ADEBUG << "Parsing cpuset format error.";
      exit(0);
    }
  }
}

void SetSchedAffinity(std::thread* thread, const std::vector<int>& cpus,
                      const std::string& affinity, int cpu_id) {
  cpu_set_t set;
  CPU_ZERO(&set);

  if (cpus.size()) {
    if (!affinity.compare("range")) {
      for (const auto cpu : cpus) {
        CPU_SET(cpu, &set);  // mark: 绑到多个核
      }
      pthread_setaffinity_np(thread->native_handle(), sizeof(set), &set);
      AINFO << "thread " << thread->get_id() << " set range affinity";
    } else if (!affinity.compare("1to1")) {
      if (cpu_id == -1 || (uint32_t)cpu_id >= cpus.size()) {
        return;
      }
      CPU_SET(cpus[cpu_id], &set);  // mark: 绑到指定核,一对一
      pthread_setaffinity_np(thread->native_handle(), sizeof(set), &set);
      AINFO << "thread " << thread->get_id() << " set 1to1 affinity";
    }
  }
}

void SetSchedPolicy(std::thread* thread, std::string spolicy,
                    int sched_priority, pid_t tid) {
  struct sched_param sp;
  int policy;

  memset(reinterpret_cast<void*>(&sp), 0, sizeof(sp));
  sp.sched_priority = sched_priority;

  if (!spolicy.compare("SCHED_FIFO")) {
    policy = SCHED_FIFO;
    pthread_setschedparam(thread->native_handle(), policy, &sp);
    AINFO << "thread " << tid << " set sched_policy: " << spolicy;
  } else if (!spolicy.compare("SCHED_RR")) {
    policy = SCHED_RR;
    pthread_setschedparam(thread->native_handle(), policy, &sp);
    AINFO << "thread " << tid << " set sched_policy: " << spolicy;
  } else if (!spolicy.compare("SCHED_OTHER")) {
    setpriority(PRIO_PROCESS, tid, sched_priority);
    AINFO << "thread " << tid << " set sched_policy: " << spolicy;
  }
}

}  // namespace scheduler
}  // namespace cyber
}  // namespace apollo
