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

#include "cyber/mainboard/module_controller.h"

#include <utility>

#include "cyber/common/environment.h"
#include "cyber/common/file.h"
#include "cyber/component/component_base.h"

namespace apollo {
namespace cyber {
namespace mainboard {

void ModuleController::Clear() {
  for (auto& component : component_list_) {
    component->Shutdown();
  }
  component_list_.clear();  // keep alive
  class_loader_manager_.UnloadAllLibrary();
}

bool ModuleController::LoadAll() {
  const std::string work_root = common::WorkRoot();
  const std::string current_path = common::GetCurrentPath();
  const std::string dag_root_path = common::GetAbsolutePath(work_root, "dag");
  std::vector<std::string> paths;
  // mark: 解析 dag 文件路径
  for (auto& dag_file : args_.GetDAGFileList()) {
    std::string dag_file_path = "";
    if (dag_file == common::GetFileName(dag_file)) {
      // case dag conf argument var is a filename
      dag_file_path = common::GetAbsolutePath(dag_root_path, dag_file);
    } else if (dag_file[0] == '/') {
      // case dag conf argument var is an absolute path
      dag_file_path = dag_file;
    } else {
      // case dag conf argument var is a relative path
      dag_file_path = common::GetAbsolutePath(current_path, dag_file);
      if (!common::PathExists(dag_file_path)) {
        dag_file_path = common::GetAbsolutePath(work_root, dag_file);
      }
    }
    total_component_nums += GetComponentNum(dag_file_path);
    paths.emplace_back(std::move(dag_file_path));
  }
  if (has_timer_component) {
    total_component_nums += scheduler::Instance()->TaskPoolSize();
  }
  common::GlobalData::Instance()->SetComponentNums(total_component_nums);
  // mark: 加载 module 共享库, 并创建和初始化 components
  for (auto dag_file_path : paths) {
    AINFO << "Start initialize dag: " << dag_file_path;
    if (!LoadModule(dag_file_path)) {
      AERROR << "Failed to load module: " << dag_file_path;
      return false;
    }
  }
  return true;
}

bool ModuleController::LoadModule(const DagConfig& dag_config) {
  const std::string work_root = common::WorkRoot();

  for (auto module_config : dag_config.module_config()) {
    std::string load_path;
    if (module_config.module_library().front() == '/') {
      load_path = module_config.module_library();
    } else {
      load_path =
          common::GetAbsolutePath(work_root, module_config.module_library());
    }

    if (!common::PathExists(load_path)) {
      AERROR << "Path does not exist: " << load_path;
      return false;
    }

    // mark: 加载 module 共享库
    class_loader_manager_.LoadLibrary(load_path);

    // mark: 通用 components 创建和初始化
    for (auto& component : module_config.components()) {
      const std::string& class_name = component.class_name();
      std::shared_ptr<ComponentBase> base =
          class_loader_manager_.CreateClassObj<ComponentBase>(class_name);
      // mark: 初始化
      if (base == nullptr || !base->Initialize(component.config())) {
        return false;
      }
      component_list_.emplace_back(std::move(base));
    }

    // mark: 定时器 timer_components 创建和初始化
    for (auto& component : module_config.timer_components()) {
      const std::string& class_name = component.class_name();
      std::shared_ptr<ComponentBase> base =
          class_loader_manager_.CreateClassObj<ComponentBase>(class_name);
      if (base == nullptr || !base->Initialize(component.config())) {
        return false;
      }
      component_list_.emplace_back(std::move(base));
    }
  }
  return true;
}

bool ModuleController::LoadModule(const std::string& path) {
  DagConfig dag_config;
  if (!common::GetProtoFromFile(path, &dag_config)) {
    AERROR << "Get proto failed, file: " << path;
    return false;
  }
  return LoadModule(dag_config);
}

int ModuleController::GetComponentNum(const std::string& path) {
  DagConfig dag_config;
  int component_nums = 0;
  if (common::GetProtoFromFile(path, &dag_config)) {
    for (auto module_config : dag_config.module_config()) {
      component_nums += module_config.components_size();
      if (module_config.timer_components_size() > 0) {
        has_timer_component = true;
      }
    }
  }
  return component_nums;
}

}  // namespace mainboard
}  // namespace cyber
}  // namespace apollo
