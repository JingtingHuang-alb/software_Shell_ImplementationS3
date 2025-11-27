#include "s3.h"
int split_ignoring_parentheses(char *line, char separator, char *parts[], int max_parts);
void run_batched_commands(char *line);
int run_subshell_if_needed(char *line);
char* skip_whitespace(char* s);
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

//Section 2 重定向

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
        //子进程
        
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
// 同样升级为智能切割，防止切断 "(ls | wc)" 这种子管道
    void launch_program_with_pipes(char *line) {
    char *commands[MAX_ARGS];
    // 使用智能切割，而不是 strtok
    int num_cmds = split_ignoring_parentheses(line, '|', commands, MAX_ARGS);

    int i;
    int pipefd[2]; 
    int prev_pipe_read = -1;

    for (i = 0; i < num_cmds; i++) {
        if (i < num_cmds - 1) {
            if (pipe(pipefd) == -1) {
                perror("pipe failed");
                return;
            }
        }

        pid_t pid = fork();

        if (pid == 0) {
            // === 子进程 ===
            if (prev_pipe_read != -1) {
                dup2(prev_pipe_read, STDIN_FILENO);
                close(prev_pipe_read);
            }
            if (i < num_cmds - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]); 
                close(pipefd[1]);
            }

            // 核心改动：把解析逻辑委托给 process_single_command
            // 以前这里是直接 execvp。
            // 比如管道的一环可能是一个 subshell: ls | (cd txt; cat f.txt) | wc
            
            // 为了避免无限递归 fork (process_single_command 会再次 fork)，
            // 实际上，process_single_command 是设计来处理 "line" 的，它会根据情况 fork。
            // 但我们现在已经在 child 里面了。
            
            // 简单方案：直接当做单个命令处理。
            // 如果 commands[i] 是 subshell "(...)"，run_subshell_if_needed 会处理它。
            // 如果是普通命令 "ls -l"，launch_program 会处理它。
            // 注意：process_single_command 会 wait/reap。在子进程里 wait 没问题。
            // 唯一的副作用是多了一层 fork (管道子进程 -> 命令子进程)，但逻辑是正确的。
            
            process_single_command(commands[i]);
            exit(0); 
        } 
        else if (pid < 0) {
            perror("fork failed");
            return;
        }

        // === 父进程 ===
        if (prev_pipe_read != -1) close(prev_pipe_read);
        if (i < num_cmds - 1) {
            prev_pipe_read = pipefd[0];
            close(pipefd[1]);
        }
    }

    for (i = 0; i < num_cmds; i++) {
        wait(NULL);
    }
}
//Section 5 Implementation

// 检查是否有分号 ;
int command_with_batch(char *line) {
    if (strchr(line, ';') != NULL) return 1;
    return 0;
}

//这是把原来 main 函数里的逻辑提取出来的
// 这样 batched commands 就可以循环调用它了

// s3.c - Update process_single_command

void process_single_command(char *line) {
    char *args[MAX_ARGS];
    int argsc;

    // 优先检查 Subshell
    // 如果 line 是 "(cd txt; ls)"，它会被 run_subshell_if_needed 捕获并处理
    if (run_subshell_if_needed(line)) {
        return; // 如果已作为子 Shell 执行，直接返回
    }

    // 1. 检查管道 (Section 4)
    // 注意：launch_program_with_pipes 现在也支持包含 subshell 的管道了
    if (command_with_pipes(line)) {
        launch_program_with_pipes(line);
    }
    // 2. 检查重定向 (Section 2)
    else if (command_with_redirection(line)) {
        parse_command(line, args, &argsc);
        launch_program_with_redirection(args, argsc);
        reap();
    }
    // 3. 普通命令 & cd (Section 1 & 3)
    else {
        parse_command(line, args, &argsc);
        if (argsc == 0) return;

        if (strcmp(args[0], "cd") == 0) {
            run_cd(args, argsc);
        }
        else {
            launch_program(args, argsc);
            reap();
        }
    }
}

 // 支持 subshell 的run_bacthed_commands
void run_batched_commands(char *line) {
    char *commands[MAX_ARGS];
    // 使用智能切割，而不是 strtok
    int num_cmds = split_ignoring_parentheses(line, ';', commands, MAX_ARGS);

    int i;
    for (i = 0; i < num_cmds; i++) {
        process_single_command(commands[i]);
    }
}

char* skip_whitespace(char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

int run_subshell_if_needed(char *line) {
    char *p = skip_whitespace(line);
    
    // 1. 检查是不是以 '(' 开头
    if (*p != '(') return 0; // 不是 Subshell

    // 2. 找匹配的最后一个 ')'
    // 假设最后面的 ')' 是匹配的
    char *end = strrchr(p, ')');
    if (end == NULL) {
        fprintf(stderr, "s3: unmatched parentheses\n");
        return 1; // 虽然格式错，但也算处理了（报错）
    }

    // 3. 提取括号里面的内容
    // 比如 "(cd txt; ls)" -> "cd txt; ls"
    *end = '\0'; // 把右括号改成结束符
    char *inner_cmd = p + 1; // 跳过左括号

    // 4. Fork 一个子进程来跑这个“子世界"
    pid_t pid = fork();

    if (pid == 0) {
        //子进程
        // 我们递归调用 run_batched_commands。
        // 因为是在子进程里调用的，所以里面的 cd 不会影响外面的父 Shell。
        run_batched_commands(inner_cmd);
        
        exit(0); // 跑完必须退出，否则子进程会变成另一个 Shell
    } 
    else if (pid > 0) {
        //父进程 
        wait(NULL); // 等待子世界结束
    } else {
        perror("fork failed");
    }

    return 1; // 表示已处理
}

//智能切割字符串
// 它可以识别括号，不会切断括号里的分号或管道
// separator: 要切割的字符 (比如 ';' 或 '|')
// max_parts: 数组最大容量
// 返回切割出的数量
int split_ignoring_parentheses(char *line, char separator, char *parts[], int max_parts) {
    int count = 0;
    int paren_level = 0; // 记录括号深度
    char *start = line;
    char *p = line;

    while (*p != '\0' && count < max_parts) {
        if (*p == '(') {
            paren_level++;
        } 
        else if (*p == ')') {
            if (paren_level > 0) paren_level--;
        }
        else if (*p == separator && paren_level == 0) {
            // 只有当不在括号里时，才进行切割
            *p = '\0'; // 切断字符串
            parts[count++] = start;
            start = p + 1; // 下一段的开始
        }
        p++;
    }
    // 处理最后一段
    if (start != NULL && count < max_parts) {
        parts[count++] = start;
    }
    return count;
}