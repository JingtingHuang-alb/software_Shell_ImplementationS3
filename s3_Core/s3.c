#include "s3.h"

///Simple for now, but will be expanded in a following section
void construct_shell_prompt(char shell_prompt[], char lwd[])
{
    char cwd[MAX_PROMPT_LEN];
    if(getcwd(cwd, sizeof(cwd)) == NULL){
        if(lwd && lwd[0] != '\0'){
            snprintf(shell_prompt, MAX_PROMPT_LEN, "[%s s3]$ ", lwd);
        }
        else {
            snprintf(shell_prompt, MAX_PROMPT_LEN, "[s3]$ ");
        }
    }
    else {
        snprintf(shell_prompt, MAX_PROMPT_LEN, "[%s s3]$ ", cwd);
    }
}

///Prints a shell prompt and reads input from the user
void read_command_line(char line[], char lwd[])
{
    char shell_prompt[MAX_PROMPT_LEN];
    construct_shell_prompt(shell_prompt, lwd);
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
    execvp(args[ARG_PROGNAME], args);
    ///Use execvp to load the binary 
    ///of the command specified in args[ARG_PROGNAME].
    ///For reference, see the code in lecture 3.
    _exit(127);
}

void launch_program(char *args[], int argsc)
{
    ///Implement this function:
    if (argsc == 0){
        return;
    }

    if (strcmp(args[ARG_PROGNAME], "exit") == 0) {
        exit(0);
    }
    ///fork() a child process.
    ///In the child part of the code,
    ///call child(args, argv)
    ///For reference, see the code in lecture 2.
    int rc = fork();
    if (rc < 0){
        printf("fork failed\n");
        return;
    }
    else if (rc == 0){
        child(args, argsc);
    }
    else{
        return;
    }
    ///Handle the 'exit' command;
    ///so that the shell, not the child process,
    ///exits.
}

static void remove_tokens(char *args[], int *argsc, int start, int count) {
    for (int i = start; i + count <= *argsc; ++i) {
        args[i] = args[i + count];
    }
    *argsc -= count;
    args[*argsc] = NULL;
}

bool command_with_redirection(char *line){
    for(char *a = line; *a; ++a){
        if(*a == '<' || *a == '>'){
            return true;
        }
    }
    return false;
}

void parse_command_redirection(char *args[], int *argsc, Redir *r){
    r->infile = NULL;
    r->outfile = NULL;
    r->append = 0;

    for (int i = 0; i < *argsc;) {
        if (strcmp(args[i], "<") == 0) {
            if (i + 1 >= *argsc) { fprintf(stderr, "s3: missing file after '<'\n"); break; }
            r->infile = args[i + 1];
            remove_tokens(args, argsc, i, 2);
            continue;
        } else if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
            if (i + 1 >= *argsc) { fprintf(stderr, "s3: missing file after '>'\n"); break; }
            r->outfile = args[i + 1];
            r->append = (args[i][1] == '>');
            remove_tokens(args, argsc, i, 2);
            continue;
        } else {
            ++i;
        }
    }
}

void child_with_redirection(char *args[], Redir *r){
    if (r->infile) {
        int fd = open(r->infile, O_RDONLY);
        if (fd < 0) { perror(r->infile); _Exit(1); }
        else if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2(stdin)"); _Exit(1); }
        close(fd);
    }
    if (r->outfile) {
        int flags = O_WRONLY | O_CREAT | (r->append ? O_APPEND : O_TRUNC);
        int fd = open(r->outfile, flags, 0644);
        if (fd < 0) { perror(r->outfile); _Exit(1); }
        else if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2(stdout)"); _Exit(1); }
        close(fd);
    }

    // exec
    execvp(args[ARG_PROGNAME], args);
    _Exit(127);
}

void launch_program_with_redirection(char *args[], int argsc){
    if (argsc == 0) return;

    if (strcmp(args[ARG_PROGNAME], "exit") == 0) {
        exit(0);
    }

    Redir r;
    parse_command_redirection(args, &argsc, &r);

    int rc = fork();
    if (rc < 0){
        return;
    } else if (rc == 0) {
        child_with_redirection(args, &r);
    } else {
        return;
    }
}

void init_lwd(char lwd[])
{
    if (getcwd(lwd, MAX_PROMPT_LEN) == NULL) {
        lwd[0] = '\0';
    }
}

bool is_cd(char *line)
{
    if (!line) return false;
    /* skip leading spaces/tabs */
    while (*line == ' ' || *line == '\t') ++line;
    if (line[0] == '\0') return false;
    /* check "cd" */
    if (line[0] == 'c' && line[1] == 'd') {
        char c = line[2];
        if (c == '\0' || c == ' ' || c == '\t') return true;
    }
    return false;
}

void run_cd(char *args[], int argsc, char lwd[])
{
    char prev_cwd[MAX_PROMPT_LEN];
    char new_cwd[MAX_PROMPT_LEN];

    if (getcwd(prev_cwd, sizeof(prev_cwd)) == NULL) {
        prev_cwd[0] = '\0';
    }

    const char *target = NULL;
    if (argsc <= 1) {
        target = getenv("HOME");
    } else if (argsc >= 2 && strcmp(args[1], "-") == 0) {
        if (lwd[0] == '\0') {
            fprintf(stderr, "s3: OLDPWD not set\n");
            return;
        }
        target = lwd;
    } else {
        target = args[1];
    }

    if (chdir(target) != 0) {
        perror("chdir");
        return;
    }

    if (prev_cwd[0] != '\0') {
        strncpy(lwd, prev_cwd, MAX_PROMPT_LEN);
        lwd[MAX_PROMPT_LEN - 1] = '\0';
    } else {
        lwd[0] = '\0';
    }
}
bool command_with_pipe(char *line)
{
    for (char *p = line; *p; ++p) {
        if (*p == '|') return true;
    }
    return false;
}

bool command_with_pipe(char *line)
{
    for (char *p = line; *p; ++p) {
        if (*p == '|') return true;
    }
    return false;
}

void launch_pipeline(char line[])
{
    char *cmds[MAX_CMDS];
    int ncmds = 0;

    // --- 1. 把 line 原地切成多段，用 '\0' 代替 '|' ---
    char *p = line;

    // 跳过前导空格
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '\0') return;

    cmds[ncmds++] = p;

    for (; *p; ++p) {
        if (*p == '|') {
            *p = '\0';    // 把 '|' 变成字符串结束
            ++p;
            // 跳过后面那段前导空格
            while (*p == ' ' || *p == '\t') ++p;
            if (*p == '\0') break;
            if (ncmds < MAX_CMDS) {
                cmds[ncmds++] = p;
            }
            else {
                fprintf(stderr, "s3: too many commands in pipeline\n");
                break;
            }
        }
    }

    if (ncmds == 1) {
        // 只有一个命令，其实不用管道，走原来的逻辑也行：
        char *args[MAX_ARGS];
        int argsc;
        parse_command(cmds[0], args, &argsc);
        launch_program_with_redirection(args, argsc);
        reap();
        return;
    }

    // --- 2. 创建 pipe 数组 ---
    int pipes[MAX_CMDS - 1][2];
    for (int i = 0; i < ncmds - 1; ++i) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            return;
        }
    }

    // --- 3. 为每个子命令 fork 一个子进程 ---
    for (int i = 0; i < ncmds; ++i) {

        char *cmdline = cmds[i];

        char *args[MAX_ARGS];
        int argsc;
        parse_command(cmdline, args, &argsc);
        if (argsc == 0) continue;

        Redir r;
        parse_command_redirection(args, &argsc, &r);

        int pid = fork();
        if (pid < 0) {
            perror("fork");
            // 出错：父进程简单 break，后面统一 close + wait
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {
            // ---- 子进程 ----

            // 如果不是第一个命令：stdin <- 前一个 pipe 的读端
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                    perror("dup2 stdin");
                    _Exit(1);
                }
            }

            // 如果不是最后一个命令：stdout -> 当前 pipe 的写端
            if (i < ncmds - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2 stdout");
                    _Exit(1);
                }
            }

            // 子进程里关掉所有 pipe 的读写端（不再需要原来的 fd）
            for (int j = 0; j < ncmds - 1; ++j) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            child_with_redirection(args, &r);
            // child_with_redirection 里已经 execvp + _Exit，不会返回
        }
        // 父进程什么都不用做，继续创建下一个子进程
    }

    // --- 4. 父进程：关掉所有 pipe fd ---
    for (int i = 0; i < ncmds - 1; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // --- 5. 父进程：等待所有子进程结束 ---
    for (int i = 0; i < ncmds; ++i) {
        reap();
    }
}

void execute_single_command(char *cmdline, char lwd[])
{
    while (*cmdline == ' ' || *cmdline == '\t') ++cmdline;
    if (*cmdline == '\0') return;

    char *args[MAX_ARGS];
    int argsc;

    if (is_cd(cmdline)) {
        parse_command(cmdline, args, &argsc);
        run_cd(args, argsc, lwd);
        return;
    }

    if (command_with_pipe(cmdline)) {
        launch_pipeline(cmdline);
        return;
    }


    if (command_with_redirection(cmdline)) {
        parse_command(cmdline, args, &argsc);
        launch_program_with_redirection(args, argsc);
        reap();
        return;
    }

    parse_command(cmdline, args, &argsc);
    launch_program(args, argsc);
    reap();

}