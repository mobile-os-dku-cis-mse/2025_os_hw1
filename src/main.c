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

// todo MAX_ARGS에 대한 예외처리
void parse_command(char* line, char** command_argv) {
    int i = 0;
    char* token = strtok(line, " \t\r\n");

    while (token != NULL && i < MAX_ARGS - 1) {
        command_argv[i++] = token;

        token = strtok(NULL, " \t\r\n");
    }

    command_argv[i] = NULL;
}


int main(int argc, const char * argv[]) {

    while (1) {
        char* command_argv[MAX_ARGS];
        char* executable_path[MAX_PATH_LEN];

        char* command_line = read_user_command();
        parse_command(command_line, command_argv);
    }
}