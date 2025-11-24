#include "s3.h"

int main(int argc, char *argv[]){
    ///Stores the command line input
    char line[MAX_LINE];

    while (1) {
        read_command_line(line);

        // 如果是空行，直接跳过
        if (strlen(line) == 0) continue;

        // 1. 检查是不是批处理命令 (含 ;)
        if (command_with_batch(line)) {
            run_batched_commands(line);
        }
        else {
            // 2. 如果没有分号，就当做单个命令处理
            process_single_command(line);
        }
    }
    return 0;
}