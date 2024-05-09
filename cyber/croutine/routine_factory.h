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

#ifndef CYBER_CROUTINE_ROUTINE_FACTORY_H_
#define CYBER_CROUTINE_ROUTINE_FACTORY_H_

#include <memory>
#include <utility>

#include "cyber/common/global_data.h"
#include "cyber/common/log.h"
#include "cyber/croutine/croutine.h"
#include "cyber/data/data_visitor.h"
#include "cyber/event/perf_event_cache.h"

namespace apollo {
namespace cyber {
namespace croutine {

class RoutineFactory {
 public:
  using VoidFunc = std::function<void()>;
  using CreateRoutineFunc = std::function<VoidFunc()>;
  // We can use routine_func directly.
  CreateRoutineFunc create_routine;  // mark: 回调函数
  inline std::shared_ptr<data::DataVisitorBase> GetDataVisitor() const {
    return data_visitor_;
  }
  inline void SetDataVisitor(const std::shared_ptr<data::DataVisitorBase>& dv) {
    data_visitor_ = dv;
  }

 private:
  std::shared_ptr<data::DataVisitorBase> data_visitor_ = nullptr;
};

template <typename M0, typename F>
RoutineFactory CreateRoutineFactory(
    F&& f, const std::shared_ptr<data::DataVisitor<M0>>& dv) {
  RoutineFactory factory;
  factory.SetDataVisitor(dv);
  factory.create_routine = [=]() {
    return [=]() {
      std::shared_ptr<M0> msg;
      for (;;) {
        // mark:先设置:DATA_WAIT,等待数据,第一次可往下执行,第二次之后需调度才执行
        CRoutine::GetCurrentRoutine()->set_state(RoutineState::DATA_WAIT);
        // 尝试看有没数据
        if (dv->TryFetch(msg)) {
          f(msg);  // 有数据则回调,结束后设置:READY,并让出cpu
          CRoutine::Yield(RoutineState::READY);
        } else {
          CRoutine::Yield();  // 无数据则让出cpu
        }
        // 此时已让出cpu,被调度时才继续执行
      }
    };
  };
  return factory;
}

template <typename M0, typename M1, typename F>
RoutineFactory CreateRoutineFactory(
    F&& f, const std::shared_ptr<data::DataVisitor<M0, M1>>& dv) {
  RoutineFactory factory;
  factory.SetDataVisitor(dv);
  factory.create_routine = [=]() {
    return [=]() {
      std::shared_ptr<M0> msg0;
      std::shared_ptr<M1> msg1;
      for (;;) {
        CRoutine::GetCurrentRoutine()->set_state(RoutineState::DATA_WAIT);
        if (dv->TryFetch(msg0, msg1)) {
          f(msg0, msg1);
          CRoutine::Yield(RoutineState::READY);
        } else {
          CRoutine::Yield();
        }
      }
    };
  };
  return factory;
}

template <typename M0, typename M1, typename M2, typename F>
RoutineFactory CreateRoutineFactory(
    F&& f, const std::shared_ptr<data::DataVisitor<M0, M1, M2>>& dv) {
  RoutineFactory factory;
  factory.SetDataVisitor(dv);
  factory.create_routine = [=]() {
    return [=]() {
      std::shared_ptr<M0> msg0;
      std::shared_ptr<M1> msg1;
      std::shared_ptr<M2> msg2;
      for (;;) {
        CRoutine::GetCurrentRoutine()->set_state(RoutineState::DATA_WAIT);
        if (dv->TryFetch(msg0, msg1, msg2)) {
          f(msg0, msg1, msg2);
          CRoutine::Yield(RoutineState::READY);
        } else {
          CRoutine::Yield();
        }
      }
    };
  };
  return factory;
}

template <typename M0, typename M1, typename M2, typename M3, typename F>
RoutineFactory CreateRoutineFactory(
    F&& f, const std::shared_ptr<data::DataVisitor<M0, M1, M2, M3>>& dv) {
  RoutineFactory factory;
  factory.SetDataVisitor(dv);
  factory.create_routine = [=]() {
    return [=]() {
      std::shared_ptr<M0> msg0;
      std::shared_ptr<M1> msg1;
      std::shared_ptr<M2> msg2;
      std::shared_ptr<M3> msg3;
      // mark: 协程运行体
      for (;;) {
        CRoutine::GetCurrentRoutine()->set_state(RoutineState::DATA_WAIT);
        if (dv->TryFetch(msg0, msg1, msg2, msg3)) {
          // mark: 回调注册函数 f
          f(msg0, msg1, msg2, msg3);
          CRoutine::Yield(RoutineState::READY);
        } else {
          CRoutine::Yield();
        }
      }
    };
  };
  return factory;
}

template <typename Function>
RoutineFactory CreateRoutineFactory(Function&& f) {
  RoutineFactory factory;
  // mark: 协程工厂函数, create_routine 是 function, create_routine()返回用户回调函数
  factory.create_routine = [f = std::forward<Function&&>(f)]() { return f; };
  return factory;
}

}  // namespace croutine
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_CROUTINE_ROUTINE_FACTORY_H_
