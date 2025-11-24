#include "s3.h"

int main(int argc, char *argv[]){

    ///Stores the command line input
    char line[MAX_LINE];

    ///The last (previous) working directory 
    char lwd[MAX_PROMPT_LEN-6]; 

    init_lwd(lwd);///Implement this function: initializes lwd with the cwd (using getcwd)

    //Stores pointers to command arguments.
    ///The first element of the array is the command name.
    char *args[MAX_ARGS];

    ///Stores the number of arguments
    int argsc;

    while (1) {

    read_command_line(line, lwd);

    char *p = line;

    while (1) {
        // 1. 跳过前导空格
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0') break;              // 整行都处理完了

        // 2. 当前这条子命令的开始位置
        char *start = p;

        // 3. 找到下一个 ';' 或字符串结束
        while (*p && *p != ';') ++p;

        if (*p == ';') {
            // 4a. 把 ';' 改成 '\0'，让 start 变成一个完整的 c-string
            *p = '\0';
            execute_single_command(start, lwd);  // 执行这一条
            ++p;                                 // 继续看下一条（; 后面）
            } 
        else {
            // 4b. 没有 ';' 了，这是最后一条命令
            execute_single_command(start, lwd);
            break;
            }
        }   
    }
}

