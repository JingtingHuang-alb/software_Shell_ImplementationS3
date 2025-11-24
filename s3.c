#include "s3.h"

///Simple for now, but will be expanded in a following section
void construct_shell_prompt(char shell_prompt[])
{
    char cwd[MAX_PROMPT_LEN];
    // getcwd 获取当前工作目录，如果成功，把它拼到提示符里
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        sprintf(shell_prompt, "[s3] %s $ ", cwd);
    } else {
        strcpy(shell_prompt, "[s3]$ ");
    }
}

///Prints a shell prompt and reads input from the user
void read_command_line(char line[])
{
    char shell_prompt[MAX_PROMPT_LEN];
    construct_shell_prompt(shell_prompt);
    printf("%s", shell_prompt);

    ///See man page of fgets(...)
    if (fgets(line, MAX_LINE, stdin) == NULL)
    {
        perror("fgets failed");
        exit(1);
    }
    ///Remove newline (enter)
    line[strlen(line) - 1] = '\0';
}

void parse_command(char line[], char *args[], int *argsc)
{
    ///Implements simple tokenization (space delimited)
    ///Note: strtok puts '\0' (null) characters within the existing storage, 
    ///to split it into logical cstrings.
    ///There is no dynamic allocation.

    ///See the man page of strtok(...)
    char *token = strtok(line, " ");
    *argsc = 0;
    while (token != NULL && *argsc < MAX_ARGS - 1)
    {
        args[(*argsc)++] = token;
        token = strtok(NULL, " ");
    }
    
    args[*argsc] = NULL; ///args must be null terminated
}

///Launch related functions
void child(char *args[], int argsc)
{
    ///Implement this function:

    ///Use execvp to load the binary 
    ///of the command specified in args[ARG_PROGNAME].
    ///For reference, see the code in lecture 3.
    if (args[0] == NULL) return;
    
    execvp(args[ARG_PROGNAME], args);
    perror("execvp failed");
    exit(1);
}

void launch_program(char *args[], int argsc)
{
    ///Implement this function:

    ///fork() a child process.
    ///In the child part of the code,
    ///call child(args, argv)
    ///For reference, see the code in lecture 2.

    ///Handle the 'exit' command;
    ///so that the shell, not the child process,
    ///exits.

    ///type 'enter' for immdediate return
    if (argsc == 0)
    {
        return;
    }

    ///type 'exit' for exit the shell terminate the current process
    if (strcmp(args[ARG_PROGNAME], "exit") == 0)
    {
        exit(0);
    }

    ///fork() a child process.
    pid_t pid = fork();

    if (pid < 0)
    {
        ///fork failed
        perror("fork failed");
        exit(1);
    }
    else if (pid == 0)
    {
        ///Child part of the code: run the command
        child(args, argsc);
        exit(1);
    }
    else
    {
        ///Parent process: do nothing here.
        ///The main loop will call reap() to wait for this child.
        return;
    }
}

// === 以下是新增的 Section 2 重定向功能 ===

// 检查是否有重定向符号
int command_with_redirection(char *line) {
    if (strstr(line, ">>") != NULL) return 1;
    if (strchr(line, '>') != NULL) return 1;
    if (strchr(line, '<') != NULL) return 1;
    return 0;
}

// 执行带重定向的命令
void launch_program_with_redirection(char *args[], int argsc) {
    int fd = -1;
    int redirect_type = 0; // 1: >, 2: >>, 3: <
    char *filename = NULL;
    int i;

    // 1. 遍历参数，找到重定向符号和文件名
    for (i = 0; i < argsc; i++) {
        if (strcmp(args[i], ">") == 0) {
            redirect_type = 1;
            filename = args[i+1];
            args[i] = NULL; // 切断命令，execvp 只读到这里
            break;
        } else if (strcmp(args[i], ">>") == 0) {
            redirect_type = 2;
            filename = args[i+1];
            args[i] = NULL;
            break;
        } else if (strcmp(args[i], "<") == 0) {
            redirect_type = 3;
            filename = args[i+1];
            args[i] = NULL;
            break;
        }
    }

    // 如果没找到文件名（比如只输了 ls > ），报错返回
    if (filename == NULL) {
        fprintf(stderr, "s3: redirection syntax error\n");
        return;
    }

    // 2. Fork 进程
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
    } else if (pid == 0) {
        // === 子进程 ===
        
        // 3. 打开文件并使用 dup2 重定向
        if (redirect_type == 1) { // > (覆盖输出)
            fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { perror("open failed"); exit(1); }
            dup2(fd, STDOUT_FILENO); // 将标准输出指向 fd
        } 
        else if (redirect_type == 2) { // >> (追加输出)
            fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) { perror("open failed"); exit(1); }
            dup2(fd, STDOUT_FILENO);
        } 
        else if (redirect_type == 3) { // < (输入重定向)
            fd = open(filename, O_RDONLY);
            if (fd < 0) { perror("open failed"); exit(1); }
            dup2(fd, STDIN_FILENO); // 将标准输入指向 fd
        }

        // 关闭不再需要的文件描述符
        if (fd != -1) close(fd);

        // 4. 执行命令
        execvp(args[0], args);
        perror("execvp failed");
        exit(1);
    } 
    // 父进程直接返回，main 会调用 reap()
    
}
static char previous_dir[MAX_LINE] = "";

void run_cd(char *args[], int argsc) {
    char current_dir[MAX_LINE];
    char *target_dir = NULL;

    // 1. 先保存现在的目录位置 (为了给下一次 cd - 用)
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        perror("s3: getcwd failed");
        return;
    }

    // 2. 判断用户想去哪
    if (argsc == 1) {
        // 情况A: 只输了 cd -> 回家 (HOME)
        target_dir = getenv("HOME");
        if (target_dir == NULL) {
            fprintf(stderr, "s3: HOME not set\n");
            return;
        }
    } 
    else if (strcmp(args[1], "-") == 0) {
        // 情况B: 输了 cd - -> 去上次的目录
        if (strlen(previous_dir) == 0) {
            fprintf(stderr, "s3: OLDPWD not set\n"); // 还没移动过目录
            return;
        }
        target_dir = previous_dir;
        printf("%s\n", target_dir); // cd - 的标准行为是打印目录名
    } 
    else {
        // 情况C: 输了 cd some_folder -> 去指定目录
        target_dir = args[1];
    }

    // 3. 真正执行切换 (chdir 是系统调用)
    if (chdir(target_dir) != 0) {
        perror("s3: cd failed"); // 比如目录不存在
    } else {
        // 只有切换成功了，才把刚才保存的 current_dir 写入 previous_dir
        strcpy(previous_dir, current_dir);
    }
}

// s3.c - Section 4 Implementation ----- pipe

// 检查是否有管道符号 |
int command_with_pipes(char *line) {
    if (strchr(line, '|') != NULL) return 1;
    return 0;
}

// 处理管道命令
void launch_program_with_pipes(char *line) {
    // 1. 先把长命令按 | 切割成多个子命令字符串
    // 例如 "ls | grep a | wc" -> ["ls", " grep a", " wc"]
    char *commands[MAX_ARGS]; 
    int num_cmds = 0;
    
    char *token = strtok(line, "|");
    while (token != NULL && num_cmds < MAX_ARGS) {
        commands[num_cmds++] = token;
        token = strtok(NULL, "|");
    }

    int i;
    int pipefd[2]; 
    int prev_pipe_read = -1; // 保存上一个管道的读取端

    for (i = 0; i < num_cmds; i++) {
        // 如果不是最后一个命令，就需要创建一个新管道
        // 用来把数据传给下一个命令
        if (i < num_cmds - 1) {
            if (pipe(pipefd) == -1) {
                perror("pipe failed");
                return;
            }
        }

        // Fork 一个子进程来跑当前的命令
        pid_t pid = fork();

        if (pid == 0) {
            // === 子进程开始 ===
            
            // --- Step 1: 基础管道建设 ---
            
            // A. 处理输入 (耳朵)：如果有“上一棒”传过来的数据
            if (prev_pipe_read != -1) {
                // 把 stdin (0) 接到上一个管道的出口上
                dup2(prev_pipe_read, STDIN_FILENO);
                close(prev_pipe_read); // 接完就可以关掉旧的了
            }

            // B. 处理输出 (嘴巴)：如果不是最后一个命令，默认喊给下一棒听
            if (i < num_cmds - 1) {
                // 把 stdout (1) 接到当前管道的入口上
                dup2(pipefd[1], STDOUT_FILENO);
                
                // 关掉管道两端（手里拿着没用了，嘴巴已经接好了）
                close(pipefd[0]); 
                close(pipefd[1]);
            }

            // --- Step 2: 重定向检查 ---
            // 如果这行命令里有 '>'，说明要把嘴巴从管道(或屏幕)改接到文件上
            // 这会覆盖掉刚才 Step 1 里的设置，这是正确的逻辑！
            
            if (strchr(commands[i], '>') != NULL) {
                // 1. 拿出刀 (strtok)，以 '>' 为界，把命令切成两半
                // 前半段："grep a " (命令)
                // 后半段：" out.txt" (文件名)
                char *cmd_part = strtok(commands[i], ">"); 
                char *file_part = strtok(NULL, ">"); // 拿到 '>' 后面的部分

                if (file_part != NULL) {
                    // 2. 清理文件名 (去掉前后的空格)
                    char *filename = strtok(file_part, " \t\n");

                    // 3. 打开文件 (准备好接收数据的桶)
                    // O_TRUNC 表示清空重写；O_CREAT 表示没有就创建
                    int fd_out = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out == -1) {
                        perror("open redirection file failed");
                        exit(1);
                    }

                    // 4. 做搭桥手术 (dup2)
                    // 这一步最关键：它把 STDOUT 强行改到了文件上
                    dup2(fd_out, STDOUT_FILENO); 
                    
                    // 5. 手术做完了，把工具扔掉
                    close(fd_out);
                }
            }

            // --- Step 3: 最终执行 ---
            
            // C. 解析并执行命令
            // 这里的 commands[i] 已经被 Step 2 的 strtok 干净利落地切断了
            // 它现在只剩下 ">" 前面的部分 (比如 "grep a")
            char *cmd_args[MAX_ARGS];
            int cmd_argsc;
            parse_command(commands[i], cmd_args, &cmd_argsc);
            
            // 只有到了所有准备工作做完的最后一步，才真正exec
            child(cmd_args, cmd_argsc);
            exit(1); // child 应该不会返回，如果返回了就是出错了
        } 
        else if (pid < 0) {
            perror("fork failed");
            return;
        }

        // === 父进程逻辑  ===
        
        // 1. 关掉上一个管道的读取端
        if (prev_pipe_read != -1) {
            close(prev_pipe_read);
        }

        // 2. 准备下一轮循环
        if (i < num_cmds - 1) {
            prev_pipe_read = pipefd[0];
            close(pipefd[1]); // 必须关掉写端，否则死锁
        }
    }

    // 等待所有子进程结束
    for (i = 0; i < num_cmds; i++) {
        wait(NULL);
    }
}
// s3.c - Section 5 Implementation

// 检查是否有分号 ;
int command_with_batch(char *line) {
    if (strchr(line, ';') != NULL) return 1;
    return 0;
}

// === 这是把原来 main 函数里的逻辑提取出来的 ===
// 这样 batched commands 就可以循环调用它了
void process_single_command(char *line) {
    char *args[MAX_ARGS];
    int argsc;

    // 1. 检查管道 (Section 4)
    if (command_with_pipes(line)) {
        launch_program_with_pipes(line);
    }
    // 2. 检查重定向 (Section 2)
    else if (command_with_redirection(line)) {
        parse_command(line, args, &argsc);
        launch_program_with_redirection(args, argsc);
        reap(); // 重定向是单个子进程，需要 wait
    }
    // 3. 普通命令 & cd (Section 1 & 3)
    else {
        parse_command(line, args, &argsc);
        
        // 防止空命令 (比如输入了很多空格)
        if (argsc == 0) return;

        if (strcmp(args[0], "cd") == 0) {
            run_cd(args, argsc);
        }
        else {
            launch_program(args, argsc);
            reap(); // 普通命令需要 wait
        }
    }
}

    // 执行批处理命令
    void run_batched_commands(char *line) {
    // 策略：为了避免 strtok 冲突，我们先把所有子命令切出来，存到数组里
    char *commands[MAX_ARGS];
    int num_cmds = 0;
    
    // 按分号切割
    char *token = strtok(line, ";");
    while (token != NULL && num_cmds < MAX_ARGS) {
        commands[num_cmds++] = token;
        token = strtok(NULL, ";");
    }

    // 切割完毕后，再挨个执行
    // 这样内层的 strtok 就不会干扰外层的逻辑了
    int i;
    for (i = 0; i < num_cmds; i++) {
        // 这里的 commands[i] 可能包含前后空格，但 parse_command 还是能处理的
        process_single_command(commands[i]);
    }
}