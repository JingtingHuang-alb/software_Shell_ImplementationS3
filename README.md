# s3 Shell (Software Systems Shell)
### Extension 2 Included: Nested Subshells

**Authors:** Jingting, Fengye  
**Date:** November 2025  
**Language:** C   
**Compiler:** GCC

---

## 1. Introduction

The **s3** (Software Systems Shell) is a custom command-line interpreter developed in C. It is designed to emulate the fundamental behaviors of standard Unix shells (such as Bash), providing a robust interface for process execution, file stream manipulation, and system navigation.

This project demonstrates a practical application of core Operating Systems concepts, specifically:

- **Process Management:** Utilizing `fork()`, `execvp()`, and `wait()` for process creation and synchronization.
* **File Descriptors:** Manipulating standard streams (`stdin`, `stdout`) via `dup2()` for I/O redirection.
* **Inter-Process Communication (IPC):** Implementing pipelines using `pipe()` to chain process execution.
* **System Calls:** Direct interaction with the kernel for file system operations (`chdir`, `getcwd`, `open`).
* **Recursion:** Implementing nested subshell logic to handle complex command grouping.

---

## 2. Compilation and Usage

The project is modularized and adheres to the C11 standard. It requires the GCC compiler.

### Build
To compile the shell, run the following command in the source directory:

```bash
gcc -Wall -std=c11 s3main.c s3.c -o s3
```

### Run
To start the shell session:

```bash
./s3
```

## 3. Architecture

### 3.1 File Structure

```text
├── s3.h        # Header file: Function prototypes, includes, and constants.
├── s3.c        # Implementation: Core logic for process launching, built-ins, and parsing.
├── s3main.c    # Entry point: Main REPL loop and high-level dispatching.
└── README.md   # Project documentation.
```


### 3.2 Control Flow & Decision Logic

The shell operates on a continuous **REPL** (Read-Eval-Print Loop) cycle. Here is the logical breakdown:

#### 1️⃣ The Main Loop (Input Cycle)
* **Start:** The program enters an infinite `while` loop.
* **Reading Input:** `read_command_line` captures keystrokes.
* **Empty Check:** Whitespace-only input is ignored, restarting the loop.

#### 2️⃣ Subshell Detection (Recursive Parsing)
* Before standard parsing, the shell checks if the command starts with `(`.
* **Logic:** If detected, the shell isolates the content within the matching parentheses and spawns a new child process to execute the inner command string recursively. This supports **Nested Subshells**.

#### 3️⃣ Batch Processing (`;`)
* The shell checks for the semicolon delimiter.
* **Action:** `run_batched_commands` splits the string and sequentially sends commands to the execution engine.

#### 4️⃣ Execution Strategy (Router)
The `process_single_command` function routes commands based on strict priority:

* **A. Pipe Detection (`|`) - High Priority:**
    Calls `launch_program_with_pipes` to manage multi-process communication via `pipe()` and `dup2()`.
* **B. Redirection Detection (`>`) - Medium Priority:**
    Calls `launch_program_with_redirection` to manage file I/O.
* **C. Built-in Commands (`cd`):**
    Calls `run_cd` to execute `chdir()` within the parent process.
* **D. Standard Execution:**
    Calls `launch_program` for standard `fork()` → `execvp()` → `wait()` sequences.

## 4. Supported Features

### 4.1 Basic Command Execution
Executes standard Unix commands (e.g., `ls`, `pwd`, `whoami`) by replacing the child process image with the requested binary.

### 4.2 I/O Redirection
Manipulates standard streams using `open()` and `dup2()`:
* `>` **Overwrite:** Redirects `stdout` to a file (Truncates).
* `>>` **Append:** Redirects `stdout` to a file (Appends).
* `<` **Input:** Redirects `stdin` from a specific file.

### 4.3 Built-in Directory Navigation (`cd`)
Implemented internally to modify the shell's Current Working Directory (CWD).
* `cd`: Go to HOME.
* `cd <path>`: Go to specific directory.
* `cd -`: Go to previous directory (persisted via static state).
* **Dynamic Prompt:** Updates to reflect the CWD via `getcwd()`.

### 4.4 Inter-Process Communication (Pipes)
Supports pipelines (e.g., `ls | grep txt`).
* **Logic:** Tokenizes by `|`, creates intermediate pipes, and manages file descriptor closure to prevent deadlocks.

### 4.5 Batched Commands
Executes multiple independent commands separated by `;`.
* **Example:** `mkdir test; cd test; ls`

### 4.6 Advanced Feature: Pipeline & Redirection Mixing
Handles combined scenarios like `ls | grep s3 > out.txt`.
* **Mechanism:** Establishes pipe connections first, then overrides the final stage's `STDOUT` if a redirection symbol is detected.

### 4.7 Subshells & Nested Subshells (Extension 2) 
The shell supports command grouping using parentheses `( ... )`.

* **Isolation:** Commands inside parentheses run in a separate child process (`fork`). Changes made inside (like `cd`) do not affect the main shell.
* **Nested Subshells:** The parsing logic tracks parenthesis depth, allowing for arbitrary levels of nesting.
* **Example:**
    ```bash
    echo "Start"; (echo "Level 1"; (cd /tmp; echo "Level 2"); pwd); echo "End"
    ```
    *In this example, the inner `cd /tmp` affects only the Level 2 scope. The outer `pwd` remains in the original directory.*

## 5. Implementation Challenges and Solutions

### 5.1 The `strtok` Re-entrancy Issue
* **Issue:** Nested `strtok` calls (for batching vs. arguments) caused loop termination.
* **Solution:** Refactored to parsing the entire line into an array first, then iterating independently.

### 5.2 Output Buffering on Prompt
* **Issue:** Prompt `[s3]$` delayed appearance.
* **Solution:** Added `fflush(stdout)` immediately after printing the prompt.

### 5.3 Parsing Nested Parentheses
* **Issue:** Standard string splitting would break `(cmd1; cmd2)` at the semicolon inappropriately.
* **Solution:** Implemented a "smart split" function that tracks the current parenthesis depth level. It only splits delimiters (`;` or `|`) when the depth level is zero, ensuring nested commands are preserved as a single unit.

---

## 6. Limitations and Known Issues

While `s3` is a functional shell, it is simplified for educational purposes:

* **Argument Parsing:** Does not handle quotes (`" "`) or escape characters. `echo "hello"` prints the quotes.
* **Environment Variables:** No expansion for `$HOME`, etc.
* **Globbing:** No wildcard support (`*.c`).
* **Signal Handling:** `Ctrl+C` terminates the shell, not just the child process.
* **Complex Redirection:** Does not support simultaneous input/output redirection (e.g., `cmd < in > out`).