1.  安装apollo
参考 https://github.com/ApolloAuto/apollo/blob/master/docs/01_Installation%20Instructions/apollo_software_installation_guide_cn.md

2. 启动apollo的docker
```sh
# host端运行
./docker/scripts/cyber_start.sh
```

3. vscode安装插件 Remote - Containers

4. vscode附加进docker
vscode会在docker里安装插件，需要稍等，右下角有进度。
vscode附加进docker后，加载代码。

5. 在步骤4进入docker后，在打开的配置文件中改成下面内容。注意：remoteUser要改成自己docker的用户名。
```sh
{
    "extensions": [
        "BazelBuild.vscode-bazel",
        "DamianKoper.gdb-debug",
        "eamodio.gitlens",
        "GitHub.vscode-pull-request-github",
        "Gruntfuggly.todo-tree",
        "jeff-hykin.better-cpp-syntax",
        "mhutchie.git-graph",
        "MS-CEINTL.vscode-language-pack-zh-hans",
        "ms-vscode-remote.remote-containers",
        "ms-vscode-remote.remote-ssh",
        "ms-vscode-remote.remote-ssh-edit",
        "ms-vscode-remote.remote-wsl",
        "ms-vscode.cmake-tools",
        "ms-vscode.cpptools",
        "ms-vscode.cpptools-extension-pack",
        "ms-vscode.cpptools-themes",
        "ms-vsliveshare.vsliveshare",
        "twxs.cmake"
    ],
    "workspaceFolder": "/apollo",
    "remoteUser": "xx",
    "remoteEnv": {
        "HISTFILE": "/apollo/.dev_bash_hist"
    }
}
```

6. 编译cyber调试版
下面命令只编译cyber目录下的模块，带调试信息。
```sh
bash apollo.sh build cyber --config=dbg
# 如果编译错误，安装glog和gflag
sudo apt update
sudo apt install libgoogle-glog-dev libgflags-dev
```

7. vscode配置文件launch.json
配置vscode调试launch.json文件支持调试，主要是cyber相关。
program: 要调试的程序路径
args: 程序运行的参数
```sh
{
    // 配置说明: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
            "name": "g++ - 生成和调试活动文件",
            "type": "cppdbg",
            "request": "launch",
            "program": "/apollo/bazel-bin/cyber/mainboard",
            "args": ["-d","/apollo/cyber/examples/common_component_example/common.dag"],
            "stopAtEntry": false,
            "cwd": "/apollo",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "setupCommands": [
                {
                    "description": "为 gdb 启用整齐打印",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ],
            // "preLaunchTask": "C/C++: g++ 生成活动文件",
            "miDebuggerPath": "/usr/bin/gdb"
        }
    ]
}
```
