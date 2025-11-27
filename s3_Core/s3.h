#ifndef _S3_H_
#define _S3_H_

///See reference for what these libraries provide
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

///Constants for array sizes, defined for clarity and code readability
#define MAX_LINE 1024
#define MAX_ARGS 128
#define MAX_PROMPT_LEN 256
#define MAX_CMDS 16

///Enum for readable argument indices (use where required)
enum ArgIndex
{
    ARG_PROGNAME,
    ARG_1,
    ARG_2,
    ARG_3,
};

typedef struct {
    char *infile;
    char *outfile;
    int append;
}Redir;

///With inline functions, the compiler replaces the function call 
///with the actual function code;
///inline improves speed and readability; meant for short functions (a few lines).
///the static here avoids linker errors from multiple definitions (needed with inline).
static inline void reap()
{
    wait(NULL);
}

///Shell I/O and related functions (add more as appropriate)
void read_command_line(char line[], char lwd[]);
void construct_shell_prompt(char shell_prompt[], char lwd[]);
void parse_command(char line[], char *args[], int *argsc);

///Child functions (add more as appropriate)
void child(char *args[], int argsc);

///Program launching functions (add more as appropriate)
void launch_program(char *args[], int argsc);

//redirection
bool command_with_redirection(char *line);
void launch_program_with_redirection(char *args[], int argsc);
void parse_command_redirection(char *args[], int *argsc, Redir *r);
void child_with_redirection(char *args[], Redir *r);

//cd
void init_lwd(char lwd[]);
bool is_cd(char *line);
void run_cd(char *args[], int argsc, char lwd[]);

//pipe
bool command_with_pipe(char *line);
void launch_pipeline(char line[]);

//batched commands
void execute_single_command(char *cmdline, char lwd[]);
#endif