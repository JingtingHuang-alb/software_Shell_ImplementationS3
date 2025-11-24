#ifndef _S3_H_
#define _S3_H_

///See reference for what these libraries provide
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

///Constants for array sizes, defined for clarity and code readability
#define MAX_LINE 1024
#define MAX_ARGS 128
#define MAX_PROMPT_LEN 256

///Enum for readable argument indices (use where required)
enum ArgIndex
{
    ARG_PROGNAME,
    ARG_1,
    ARG_2,
    ARG_3,
};

///With inline functions, the compiler replaces the function call 
///with the actual function code;
///inline improves speed and readability; meant for short functions (a few lines).
///the static here avoids linker errors from multiple definitions (needed with inline).
static inline void reap()
{
    wait(NULL);
}

///Shell I/O and related functions (add more as appropriate)
void read_command_line(char line[]);
void construct_shell_prompt(char shell_prompt[]);
void parse_command(char line[], char *args[], int *argsc);

///Child functions
void child(char *args[], int argsc);

///Program launching functions
void launch_program(char *args[], int argsc);
// redirection
int command_with_redirection(char *line);
void launch_program_with_redirection(char *args[], int argsc);
// cd
void run_cd(char *args[], int argsc);

//s4---pipe
// check for a pipe 
int command_with_pipes(char *line);
// Execute a pipeline command
void launch_program_with_pipes(char *line);

// s3.h - Section 5
// 检查是否有分号 ;
int command_with_batch(char *line);
// 执行批处理命令
void run_batched_commands(char *line);
// 处理单个命令的核心逻辑 (提取自 main)
void process_single_command(char *line);

// s3.h - Section 6: PE1 Subshells
// 检查并执行子 Shell (如果命令以 '(' 开头)
// 返回 1 表示已作为子 Shell 执行，返回 0 表示不是子 Shell
int run_subshell_if_needed(char *line);

#endif