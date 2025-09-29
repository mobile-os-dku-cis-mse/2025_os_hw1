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


bool find_command_path(const char* program, char* full_path) {
    char* path_env = getenv("PATH");
    if (path_env == NULL) {
        return false;
    }

    char* path_copy = strdup(path_env);
    if (path_copy == NULL) {
        perror("find_command_path:strdup");
        return false;
    }

    char* dir = strtok(path_copy, ":");

    while (dir != NULL) {
        snprintf(full_path, MAX_PATH_LEN, "%s/%s", dir, program);

        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return true;
        }

        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return false;
}

int main(int argc, const char * argv[]) {

    while (1) {
        char* command_argv[MAX_ARGS];
        char* executable_path[MAX_PATH_LEN];

        char* command_line = read_user_command();
        parse_command(command_line, command_argv);
        bool tmp = find_command_path(command_argv[0], executable_path);
        // fork + execvp
    }
}