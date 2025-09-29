#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_CMD_LEN 1024
#define MAX_PATH_LEN 1024
#define MAX_ARGS    128

// todo 넘을 때, 예외처리
char* read_user_command() {
    static char line[MAX_CMD_LEN];
    printf("sish> ");
    fgets(line, sizeof(line), stdin);
    line[strcspn(line, "\n")] = '\0';
    return line;
}


int main(int argc, const char * argv[]) {

    while (1) {
        char* command_argv[MAX_ARGS];
        char* executable_path[MAX_PATH_LEN];

        char* command_line = read_user_command();
    }
}