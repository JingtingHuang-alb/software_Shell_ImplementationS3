#**s3 Shell (Software Systems Shell)**

*Authors: Jingting, Fengye*

Date: November 2025

**1. Introduction**

The s3 (Software Systems Shell) is a custom command-line interpreter developed in C. It is designed to emulate the fundamental behaviors of standard Unix shells (such as Bash), providing a robust interface for process execution, file stream manipulation, and system navigation.

This project demonstrates a practical application of core Operating Systems concepts, specifically:

Process Management: Utilizing fork(), execvp(), and wait() for process creation and synchronization.

File Descriptors: Manipulating standard streams (stdin, stdout) via dup2() for I/O redirection.

Inter-Process Communication (IPC): Implementing pipelines using pipe() to chain process execution.

System Calls: Direct interaction with the kernel for file system operations (chdir, getcwd, open).

**2. Compilation and Usage**

The project is modularized and adheres to the C11 standard. It requires the GCC compiler.

Build
To compile the shell, run the following command in the source directory:

Bash

gcc -Wall -std=c11 s3main.c s3.c -o s3
Run
To start the shell session:

Bash

./s3

**3. Architecture**

3.1 File Structure

Plaintext


├── s3.h        # Header file: Function prototypes, includes, and constants (MAX_LINE, MAX_ARGS).

├── s3.c        # Implementation: Core logic for process launching, built-ins, and parsing.

├── s3main.c    # Entry point: Main REPL (Read-Eval-Print Loop) and high-level dispatching.

└── README.md   # Project documentation.

3.2 Control Flow & Decision Logic
The shell operates on a continuous REPL (Read-Eval-Print Loop) cycle.

Here is a step-by-step breakdown of the logic flow:

1-> The Main Loop (Input Cycle)

Start: The program enters an infinite while loop, ready to accept user input.

Reading Input: The function read_command_line captures the user's keystrokes from stdin.

Empty Input Check: Before processing, the shell checks if the input is empty or consists only of whitespace.

Yes: The shell ignores the input and immediately restarts the loop (returns to Start).

No: The shell proceeds to the parsing stage.

2-> Batch Processing (The ; Delimiter)

The shell first checks if the user intends to execute multiple commands in a single line (Batch Mode).

Detection: It scans the input string for the semicolon ; delimiter.

Action:

If found, run_batched_commands splits the long string into individual commands (e.g., mkdir a; ls becomes mkdir a and ls).

It then loops through these commands, sending them one by one to the execution engine (process_single_command).

3-> Execution Strategy (The Decision Tree)

The core logic resides in process_single_command. This function acts as a router, analyzing the command string to determine the correct execution method. It follows a strict priority hierarchy:

A. Pipe Detection (|) - High Priority

Check: Does the command contain a vertical bar |?

Logic: This indicates a pipeline where the output of one process must become the input of the next.

Action: The shell calls launch_program_with_pipes. This function handles complex multi-process management, setting up pipe() communication and using dup2() to connect stdout and stdin between processes.

B. Redirection Detection (>) - Medium Priority

Check: If no pipe is found, does the command contain a greater-than sign >?

Logic: This indicates that the output should be saved to a file rather than printed to the terminal.

Action: The shell calls launch_program_with_redirection. It parses the filename, opens the file, and redirects the standard output using dup2().

C. Built-in Commands (cd)

Check: Is the command strictly "cd"?

Logic: Changing directories affects the current process environment. It cannot be done by a child process (which would exit immediately after changing its own directory).

Action: The shell calls run_cd, which uses the system call chdir() to change the working directory of the shell itself.

D. Standard Execution (Default)

Check: If none of the above special characters or keywords are found.

Logic: This is treated as a standard external program (e.g., ls, grep, pwd).

Action: The shell calls launch_program, which performs the standard fork() → execvp() → wait() sequence to run the command.

**4. Supported Features**

4.1 Basic Command Execution
The shell parses user input from stdin and executes standard Unix commands (e.g., ls, pwd, echo, whoami).

Mechanism: The shell calls fork() to create a child process. The child replaces its memory image using execvp() with the requested binary. The parent process ensures synchronous execution by calling wait() (encapsulated in the reap() function), preventing zombie processes.

4.2 I/O Redirection
The shell supports manipulating standard input and output streams using open() and dup2().

Overwrite Output (>): Redirects stdout to a file. If the file exists, it is truncated. (Flags: O_WRONLY | O_CREAT | O_TRUNC).

Append Output (>>): Redirects stdout to a file, appending new data to the end. (Flags: O_WRONLY | O_CREAT | O_APPEND).

Input Redirection (<): Redirects stdin to read from a specific file. (Flag: O_RDONLY).

4.3 Built-in Directory Navigation (cd)
Unlike standard external binaries, cd is implemented as a built-in command to modify the shell process's own Current Working Directory (CWD).

cd: Switches to the user's HOME directory (retrieved via getenv("HOME")).

cd <path>: Switches to the specified directory using chdir().

cd -: Switches to the previous working directory. This feature uses a static variable (previous_dir) to persist state across function calls.

Dynamic Prompt: The prompt ([s3] /path $) updates dynamically via getcwd() to reflect the CWD.

4.4 Inter-Process Communication (Pipes)
The shell supports pipelines (e.g., ls | grep txt), allowing the standard output of one process to serve as the standard input for the next.

Implementation: It uses pipe() to generate file descriptor pairs. dup2() maps the write-end of the pipe to STDOUT and the read-end to STDIN.

Logic: The launch_program_with_pipes function tokenizes the command by |, iterates through the segments, creates pipes for intermediate stages, and forks child processes. It carefully manages file descriptor closure to prevent deadlocks.

4.5 Batched Commands
Users can execute multiple independent commands in a single line using the semicolon delimiter (;).

Example: mkdir test; cd test; ls

Logic: The input line is split by the ; delimiter, and each segment is passed sequentially to process_single_command.

4.6 Advanced Feature: Pipeline & Redirection Mixing
The shell handles combined pipelines and redirection (e.g., ls | grep s3 > out.txt).

Mechanism: The logic first establishes the pipe connections. In the final stage of the pipeline (or any specific stage), it checks for redirection symbols. If a redirection symbol (>) is detected, the STDOUT file descriptor is overridden using dup2 to point to the file instead of the pipe or terminal, ensuring the final output is saved correctly.

**5. Implementation Challenges and Solutions**

5.1 The strtok Re-entrancy Issue
Issue: While implementing batched commands, nesting strtok calls (one for splitting ;, one for splitting spaces) caused the outer loop to terminate prematurely because strtok uses a static internal buffer.

Solution: The logic in run_batched_commands was refactored to first parse the entire line into a commands[] array based on the semicolon delimiter. The function then iterates through this array to parse arguments independently, avoiding state conflict.

5.2 Output Buffering on Prompt
Issue: The shell prompt [s3]$ occasionally did not appear immediately after a command finished because printf is line-buffered and the prompt lacks a newline character.

Solution: fflush(stdout); was added immediately after the printf statement in read_command_line to force the buffer to flush to the terminal.

5.3 cd Persistence in Child Processes
Issue: Initially, cd was executed via fork and execvp. While the child process successfully changed directories, the parent shell remained in the original directory upon the child's exit.

Solution: cd was identified as a built-in command. The logic in process_single_command intercepts cd calls before forking, executing chdir() directly in the parent process.

**6. Limitations and Known Issues**

While s3 is a functional shell, it is a simplified implementation for educational purposes and lacks several features found in fully POSIX-compliant shells:

Argument Parsing & Quoting: The shell uses strtok with space delimiters for parsing. It does not handle quotes (" " or ' ') or escape characters.

Example: echo "hello world" will output "hello world" (with quotes included) rather than hello world.

Example: Arguments containing spaces (e.g., filenames like my file.txt) cannot be processed correctly.

Environment Variables: There is no support for setting or expanding environment variables (e.g., echo $HOME will simply print $HOME).

Globbing: Wildcard expansion (e.g., ls *.c) is not supported; the arguments are passed literally to the command.

Signal Handling: The shell does not implement custom signal handlers for SIGINT (Ctrl+C) or SIGTSTP (Ctrl+Z). Pressing Ctrl+C will terminate the shell itself, not just the running child process.

Background Jobs: There is no support for background execution (e.g., &). All commands run in the foreground, and the shell blocks until they complete.

Complex Redirection Syntax: The shell assumes spaces around redirection operators for parsing logic in some contexts and only supports single redirection operators per command segment (e.g., cmd < in > out simultaneous redirection is not supported).

