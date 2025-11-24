# s3 Shell (软件系统 Shell)

**作者:** Jingting, Fengye  
**日期:** 2025年11月  
**语言:** C (C11 标准)

## 1. 简介

**s3** (Software Systems Shell) 是一个用 C 语言开发的自定义命令行解释器。它的设计目标是模拟标准 Unix Shell（如 Bash）的核心行为，为用户提供进程执行、文件流操作和系统导航的接口。

本项目展示了操作系统核心概念的实际应用，具体包括：
* **进程管理:** 使用 `fork()`, `execvp()`, 和 `wait()` 进行进程的创建与同步。
* **文件描述符:** 通过 `dup2()` 操作标准流 (`stdin`, `stdout`) 实现 I/O 重定向。
* **进程间通信 (IPC):** 使用 `pipe()` 实现管道，将多个进程串联执行。
* **系统调用:** 直接与内核交互以进行文件系统操作 (`chdir`, `getcwd`, `open`)。

## 2. 编译与使用

本项目采用模块化设计，遵循 C11 标准，需要使用 GCC 编译器。

### 编译
在源代码目录下运行以下命令：
```bash
gcc -Wall -std=c11 s3main.c s3.c -o s3
运行
启动 Shell 会话：

Bash

./s3
3. 架构设计
3.1 文件结构
s3main.c: 入口点，包含主 REPL (读取-求值-打印循环)。负责处理高层输入读取，并将逻辑分发给 run_batched_commands 或 process_single_command。

s3.c: 核心逻辑实现，包括 launch_program (fork/exec)，内置命令 (run_cd)，解析辅助函数，以及复杂的执行逻辑（管道 launch_program_with_pipes 和重定向）。

s3.h: 头文件，包含函数原型、标准库引用和常量定义（如 MAX_LINE, MAX_ARGS）。

3.2 控制流概览
Shell 在一个无限循环 while(1) 中运行：

读取: read_command_line 获取用户输入。

批处理检查: 如果输入包含分号 (;)，则路由至 run_batched_commands，按顺序分割并执行每一段命令。

单条处理: 单个命令路由至 process_single_command。

管道检查: 检测到 | 则调用 launch_program_with_pipes。

重定向检查: 检测到重定向符号 (>, >>, <) 则调用 launch_program_with_redirection。

内置命令检查: 如果是 cd，则在父进程中执行 run_cd。

默认: 标准命令通过 launch_program 执行。

4. 支持的功能
4.1 基础命令执行
Shell 解析标准输入并执行 Unix 命令（如 ls, pwd, echo, whoami）。

机制: 调用 fork() 创建子进程。子进程使用 execvp() 替换内存镜像为请求的二进制文件。父进程调用 wait() (封装在 reap() 中) 确保同步执行，防止僵尸进程。

4.2 I/O 重定向
支持使用 open() 和 dup2() 操作标准输入输出流。

覆盖输出 (>): 重定向 stdout 到文件。如果文件存在则截断。(标志: O_WRONLY | O_CREAT | O_TRUNC)。

追加输出 (>>): 重定向 stdout 到文件，追加到末尾。(标志: O_WRONLY | O_CREAT | O_APPEND)。

输入重定向 (<): 重定向 stdin 从指定文件读取。(标志: O_RDONLY)。

4.3 内置目录导航 (cd)
与外部二进制程序不同，cd 作为内置命令实现，以修改 Shell 进程自身的工作目录 (CWD)。

cd: 切换到用户的 HOME 目录 (通过 getenv("HOME") 获取)。

cd <路径>: 切换到指定目录。

cd -: 切换到上一次的工作目录。此功能使用 static 变量 (previous_dir) 在函数调用间持久化状态。

动态提示符: 提示符 ([s3] /path $) 通过 getcwd() 动态更新以反映当前路径。

4.4 进程间通信 (管道)
支持管道操作（例如 ls | grep txt），允许前一个进程的输出作为后一个进程的输入。

实现: 使用 pipe() 生成文件描述符对。使用 dup2() 将写端映射到 STDOUT，读端映射到 STDIN。

逻辑: launch_program_with_pipes 函数按 | 分割命令，迭代各部分，为中间阶段创建管道，并 fork 子进程。它会仔细管理文件描述符的关闭以防止死锁。

4.5 批处理命令
用户可以使用分号 (;) 在一行中执行多个独立命令。

示例: mkdir test; cd test; ls

逻辑: 输入行按 ; 分割，每一段依次传递给 process_single_command 执行。

4.6 高级功能：管道与重定向混合
Shell 能正确处理管道与重定向结合的场景（例如 ls | grep s3 > out.txt）。

机制: 逻辑上优先建立管道连接。在管道的最后阶段（或特定阶段），检查是否有重定向符号。如果检测到 >，则使用 dup2 覆盖 STDOUT 指向文件而不是管道或终端，确保最终结果正确保存。

5. 实现挑战与解决方案
5.1 strtok 重入问题
问题: 在实现批处理命令时，嵌套使用 strtok（外层分割 ;，内层分割空格）导致外层循环过早终止，因为 strtok 使用静态内部缓冲区。

解决方案: 重构 run_batched_commands 逻辑，先将整行按分号解析存入 commands[] 数组，然后再遍历数组独立解析参数，避免状态冲突。

5.2 提示符输出缓冲
问题: Shell 提示符 [s3]$ 有时不会在命令结束后立即显示，因为 printf 默认是行缓冲的，而提示符没有换行符。

解决方案: 在 read_command_line 中的 printf 后立即添加 fflush(stdout);，强制刷新缓冲区。

5.3 子进程中的 cd 持久性
问题: 最初通过 fork 执行 cd。虽然子进程成功切换了目录，但父 Shell 在子进程退出后仍停留在原目录。

解决方案: 将 cd 识别为内置命令。在 process_single_command 中拦截 cd 调用，直接在父进程中执行 chdir()。

6. 局限性与已知问题
虽然 s3 是一个功能完备的 Shell，但作为教学实现，它缺少完整 POSIX Shell 的部分功能：

参数解析与引号: Shell 使用空格作为分隔符解析参数，不支持引号 ('' "") 或转义字符。

示例: echo "hello world" 会输出 "hello world"（带引号），而不是 hello world。文件名中若包含空格将无法正确处理。

环境变量: 不支持设置或展开环境变量（例如 echo $HOME 只会打印字面量 $HOME）。

通配符 (Globbing): 不支持通配符展开（例如 ls *.c），参数会原样传递给命令。

信号处理: 未实现自定义信号处理（如 SIGINT Ctrl+C）。按下 Ctrl+C 会终止整个 Shell 而不仅仅是当前运行的子进程。

后台作业: 不支持后台执行（例如 &）。所有命令均在前台运行，Shell 会阻塞直到命令完成。