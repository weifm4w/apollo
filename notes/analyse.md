
### 编译问题
#### jdk安装失败: 网络问题，切换能访问外网的网络
```log
[weifm@in-cyber-docker:/apollo]$ ./apollo.sh build cyber
[WARNING] nvidia-smi not found. CPU will be used.
[WARNING] nvidia-smi not found. CPU will be used.
[INFO] Apollo Environment Settings:
[INFO]     APOLLO_ROOT_DIR: /apollo
[INFO]     APOLLO_CACHE_DIR: /apollo/.cache
[INFO]     APOLLO_IN_DOCKER: true
[INFO]     APOLLO_VERSION: comment-2023-09-12-d7b702d9bc
[INFO]     DOCKER_IMG: cyber-x86_64-18.04-20210315_1535
[INFO]     APOLLO_ENV:  STAGE=dev USE_ESD_CAN=false
[INFO]     USE_GPU: USE_GPU_HOST=0 USE_GPU_TARGET=0
[WARNING] nvidia-smi not found. CPU will be used.
[WARNING] nvidia-smi not found. CPU will be used.
[ OK ] Running CPU build on x86_64 platform.
[INFO] Build Overview:
[INFO]     USE_GPU: 0  [ 0 for CPU, 1 for GPU ]
[INFO]     Bazel Options: --config=cpu
[INFO]     Build Targets: //cyber/...
[INFO]     Disabled:
(05:14:01) INFO: Invocation ID: dda7f1ef-7e9a-4ed4-89a0-11b76200c69e
(05:14:02) INFO: Current date is 2023-09-12
(05:14:02) INFO: Repository rules_java instantiated at:
  /apollo/WORKSPACE:103:16: in <toplevel>
  /apollo/.cache/bazel/540135163923dd7d5820f3ee4b306b32/external/com_github_grpc_grpc/bazel/grpc_extra_deps.bzl:29:18: in grpc_extra_deps
  /apollo/.cache/bazel/540135163923dd7d5820f3ee4b306b32/external/com_google_protobuf/protobuf_deps.bzl:44:21: in protobuf_deps
Repository rule http_archive defined at:
  /apollo/.cache/bazel/540135163923dd7d5820f3ee4b306b32/external/bazel_tools/tools/build_defs/repo/http.bzl:336:31: in <toplevel>
(05:14:02) WARNING: Download from https://github.com/bazelbuild/rules_java/archive/981f06c3d2bd10225e85209904090eb7b5fb26bd.tar.gz failed: class com.google.devtools.build.lib.bazel.repository.downloader.UnrecoverableHttpException Unknown host: github.com
(05:14:02) ERROR: An error occurred during the fetch of repository 'rules_java':
   Traceback (most recent call last):
	File "/apollo/.cache/bazel/540135163923dd7d5820f3ee4b306b32/external/bazel_tools/tools/build_defs/repo/http.bzl", line 111, column 45, in _http_archive_impl
		download_info = ctx.download_and_extract(
Error in download_and_extract: java.io.IOException: Error downloading [https://github.com/bazelbuild/rules_java/archive/981f06c3d2bd10225e85209904090eb7b5fb26bd.tar.gz] to /apollo/.cache/bazel/540135163923dd7d5820f3ee4b306b32/external/rules_java/temp15597709538229588043/981f06c3d2bd10225e85209904090eb7b5fb26bd.tar.gz: Unknown host: github.com
(05:14:02) ERROR: While resolving toolchains for target @com_google_protobuf//:protoc: invalid registered toolchain '@bazel_tools//tools/jdk:all': while parsing '@bazel_tools//tools/jdk:all': no such package '@rules_java//java': java.io.IOException: Error downloading [https://github.com/bazelbuild/rules_java/archive/981f06c3d2bd10225e85209904090eb7b5fb26bd.tar.gz] to /apollo/.cache/bazel/540135163923dd7d5820f3ee4b306b32/external/rules_java/temp15597709538229588043/981f06c3d2bd10225e85209904090eb7b5fb26bd.tar.gz: Unknown host: github.com
(05:14:02) ERROR: Analysis of target '//cyber/proto:_classic_conf_cc_proto_codegen' failed; build aborted: invalid registered toolchain '@bazel_tools//tools/jdk:all': while parsing '@bazel_tools//tools/jdk:all': no such package '@rules_java//java': java.io.IOException: Error downloading [https://github.com/bazelbuild/rules_java/archive/981f06c3d2bd10225e85209904090eb7b5fb26bd.tar.gz] to /apollo/.cache/bazel/540135163923dd7d5820f3ee4b306b32/external/rules_java/temp15597709538229588043/981f06c3d2bd10225e85209904090eb7b5fb26bd.tar.gz: Unknown host: github.com
(05:14:02) INFO: Elapsed time: 0.857s
(05:14:02) INFO: 0 processes.
(05:14:02) FAILED: Build did NOT complete successfully (0 packages loaded, 0 targets configured)
    currently loading: @bazel_tools//tools/jdk
```
#### 报错找不到glog头文件
```sh
sudo apt update
sudo apt install libgoogle-glog-dev
```

### 启动方式
- cyber_launch start xxx.launch
- mainboard -d xxx.dag
- ./xxx.bin
- 例子： cyber/examples/common_component_example/common.launch

### cyber_launch start 参数解析
```py
File: cyber\tools\cyber_launch\cyber_launch.py
113: class ProcessWrapper(object):
---
114:
115:     def __init__(self, binary_path, dag_num, dag_list, process_name,
116:                  process_type, sched_name, exception_handler=''):
119:         self.binary_path = binary_path
120:         self.dag_num = dag_num
121:         self.dag_list = dag_list
122:         self.name = process_name
123:         self.sched_name = sched_name
124:         self.process_type = process_type
---
135:     def start(self):
136:         """
137:         Start a manager in process name
138:         """
139:         if self.process_type == 'binary':
140:             args_list = self.name.split()
141:         else:
142:             args_list = [self.binary_path]
143:             for i in self.dag_list:
144:                 args_list.append('-d')  # mark: -d dag config file
145:                 args_list.append(i)
146:             if len(self.name) != 0:
147:                 args_list.append('-p')  # mark: -p process_name:the process namespace for running this module
148:                 args_list.append(self.name)
149:             if len(self.sched_name) != 0:
150:                 args_list.append('-s')  # mark: -s sched_name: sched policy
151:                 args_list.append(self.sched_name)
```

### mainbord 参数解析
```c++
File: cyber\mainboard\module_argument.cc
28: void ModuleArgument::DisplayUsage() {
29:   AINFO << "Usage: \n    " << binary_name_ << " [OPTION]...\n"
30:         << "Description: \n"
31:         << "    -h, --help : help information \n"
32:         << "    -d, --dag_conf=CONFIG_FILE : module dag config file\n"
33:         << "    -p, --process_group=process_group: the process "
34:            "namespace for running this module, default in manager process\n"
35:         << "    -s, --sched_name=sched_name: sched policy "
36:            "conf for hole process, sched_name should be conf in cyber.pb.conf\n"
37:         << "Example:\n"
38:         << "    " << binary_name_ << " -h\n"
39:         << "    " << binary_name_ << " -d dag_conf_file1 -d dag_conf_file2 "
40:         << "-p process_group -s sched_name\n";
41: }
```

### 启动配置文件
1. 例子解析
   - cyber\examples\common_component_example\common.launch
   - cyber\examples\common_component_example\common.dag
2. protobuf协议详尽说明
   - cyber\proto\dag_conf.proto
   - cyber\proto\component_conf.proto

### 设计
一个进程可以包含多个`component`
一个`component`在`Init`时创建一个Node

### 调度
1. 调度初始化
```c++
File: cyber\scheduler\scheduler_factory.cc
49: // mark: 调用方式 scheduler::Instance()
50: Scheduler* Instance() {
---
57:       std::string policy("classic");  // mark:默认调度策略
58:
59:       std::string conf("conf/");
60:       // mark:尝试解析 process_group.conf全局cyber配置,通过-p指定 process_group
61:       conf.append(GlobalData::Instance()->ProcessGroup()).append(".conf");
---
73:       if (!policy.compare("classic")) {
74:         // mark: new ctor 中 创建线程 并 配置调度策略
75:         obj = new SchedulerClassic();
76:       } else if (!policy.compare("choreography")) {
77:         obj = new SchedulerChoreography();
78:       } else {
79:         AWARN << "Invalid scheduler policy: " << policy;
80:         obj = new SchedulerClassic();  // mark:默认调度策略
81:       }

```
2. 配置
   - 启动配置: cyber\proto\cyber_conf.proto `message CyberConfig`
   - 配置模板: cyber\proto\scheduler_conf.proto `message SchedulerConf`
   - cyber\proto\classic_conf.proto
   - cyber\proto\choreography_conf.proto
```proto
File: cyber\proto\classic_conf.proto
05: message ClassicTask {
06:   optional string name = 1;
07:   optional uint32 prio = 2 [default = 1]; // 值越大, 表明优先级越高
08:   optional string group_name = 3;
09: }
10:
11: message SchedGroup {
12:   required string name = 1 [default = "default_grp"];
13:   // mark: 核数,一核一线程,
14:   //       SchedulerClassic() => CreateProcessor() => BindContext(ctx) => std::thread
15:   optional uint32 processor_num = 2;
16:   // mark: range: 每个线程都可以在cpuset:0-7, 16-23号核上执行
17:   //       1to1:  每个线程和一个cpu进行亲和性设置
18:   optional string affinity = 3;
19:   optional string cpuset = 4;
20:   // mark: SCHED_FIFO(实时调度策略, 先到先服务),
21:   //       SCHED_RR(实时调度策略, 时间片轮转),
22:   //       SCHED_OTHER(分时调度策略, 为默认策略)
23:   optional string processor_policy = 5;
24:   /* mark: 1.如果 processor_policy 设置为 SCHED_FIFO/SCHED_RR,
25:              processor_prio取值为(1-99), 值越大, 表明优先级越高, 抢到cpu概率越大。
26:            2.如果 processor_policy 设置为 SCHED_OTHER,
27:              processor_prio 取值为（-20-19, 0为默认值）, 这里为 nice 值,
28:              nice 值不影响分配到cpu的优先级, 但是影响分到cpu时间片的大小,
29:              如果 nice 值越小, 分到的时间片越多 */
30:   optional int32 processor_prio = 6 [default = 0];
31:   // mark: Croutine 数组
32:   repeated ClassicTask tasks = 7;
33: }
34:
35: message ClassicConf {
36:   // mark: 多个group对应多个numa节点, 跨numa节点会给系统带来额外的开销
37:   repeated SchedGroup groups = 1;
38: }
---
File: cyber\proto\choreography_conf.proto
05: message ChoreographyTask {
06:   optional string name = 1;
07:   optional int32 processor = 2;
08:   optional uint32 prio = 3 [default = 1];
09: }
10:
11: message ChoreographyConf {
12:   optional uint32 choreography_processor_num = 1;
13:   optional string choreography_affinity = 2;
14:   optional string choreography_processor_policy = 3;
15:   optional int32 choreography_processor_prio = 4;
16:   optional string choreography_cpuset = 5;
17:   optional uint32 pool_processor_num = 6;
18:   optional string pool_affinity = 7;
19:   optional string pool_processor_policy = 8;
20:   optional int32 pool_processor_prio = 9;
21:   optional string pool_cpuset = 10;
22:   repeated ChoreographyTask tasks = 11;
23: }
```

### shm
写端：
writer —> Segment::AcquireBlockToWrite —> notifier::Notify

读端：
ShmDispatcher::ThreadFunc —> notifier::Listen —> Segment::AcquireBlockToRead —> ListenerHandler::Run —> (*signals_[oppo_id])(msg, msg_info) —> DataDispatcher —> DataVisitor::buffer —> DataNotifier::Instance()::Notify —> callback —> Scheduler::NotifyProcessor —> 执行协程 —> 从 DataVisitor::buffer 取出数据 —> 放入reader的blocker —> 执行用户传入的回调(如果有)

### 消息传递顶层流程
Reader如何从Writer方读取到数据的整个上层数据流
Reader::Init() -> ReceiverManager<MessageT>::GetReceiver() -> transport::Transport::Instance()->CreateReceiver<MessageT>() 创建消息回调 —> DataDispatcher —> DataVisitor::buffer —> DataNotifier::Instance()::Notify —> callback —> Scheduler::NotifyProcessor —> 执行协程 —> 从DataVisitor::buffer拿出数据 —> 放入reader的blocker —>执行用户传入的回调(如果有)
