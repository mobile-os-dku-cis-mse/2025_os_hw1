#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include "builtins.h"

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

void launch_process(char** argv, const char* executable_path) {
    pid_t pid= fork();
    int status;

    if (pid < 0) {
        perror("launch_process:fork");
        return;
    }

    if (pid == 0) {
        if (execv(executable_path, argv) == -1) {
            perror("launch_process:pid==0");
            exit(EXIT_FAILURE);
        }
    } else {
        // --- 👨‍👩‍👧 부모 프로세스 ---
        // 3. 자식이 끝날 때까지 대기 (wait)
        // 자식 프로세스가 정상적으로 종료(WIFEXITED)되거나
        // 시그널에 의해 종료(WIFSIGNALED)될 때까지 계속 대기한다.
        // 이는 waitpid가 시그널에 의해 중단(interrupted)되는 경우를 처리하는 견고한 방법이다.
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
}

int main(int argc, const char * argv[]) {

    while (1) {
        int is_builtin = 0;
        char* command_argv[MAX_ARGS];
        char executable_path[MAX_PATH_LEN];

        char* command_line = read_user_command();
        parse_command(command_line, command_argv);
        if (command_argv[0]==NULL) continue;

        for (int i = 0; builtins[i].name != NULL; i++) {
            if (strcmp(command_argv[0], builtins[i].name) == 0) {
                int status = builtins[i].func(command_argv);
                is_builtin = 1;

                if (status == 0) {
                    exit(EXIT_SUCCESS);
                }
                break;
            }
        }
        if (!is_builtin) {
            bool tmp = find_command_path(command_argv[0], executable_path);
            launch_process(command_argv, executable_path);
        }
    }
}