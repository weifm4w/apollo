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

#include "cyber/common/global_data.h"
#include "cyber/common/log.h"
#include "cyber/init.h"
#include "cyber/mainboard/module_argument.h"
#include "cyber/mainboard/module_controller.h"
#include "cyber/state.h"

using apollo::cyber::mainboard::ModuleArgument;
using apollo::cyber::mainboard::ModuleController;

int main(int argc, char** argv) {
  FLOW2MSG("main");
  // parse the argument
  ModuleArgument module_args;
  // mark:参数解析,并初始化 GlobalData
  module_args.ParseArgument(argc, argv);

  AFLOW << "Init cyber";
  // initialize cyber
  // mark:初始化log,scheduler,设置调度策略和启动线程
  apollo::cyber::Init(argv[0]);

  AFLOW << "Init ModuleController";
  // start module
  // mark:模块初始化,动态库加载,注册component,根据dag注册协程和绑定用户回调
  ModuleController controller(module_args);
  if (!controller.Init()) {
    controller.Clear();
    AERROR << "module start error.";
    return -1;
  }

  AFLOW << "WaitForShutdown......";
  apollo::cyber::WaitForShutdown();
  AFLOW << "controller.Clear";
  controller.Clear();
  AINFO << "exit mainboard.";

  return 0;
}
