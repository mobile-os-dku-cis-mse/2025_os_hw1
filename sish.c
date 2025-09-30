#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define DELIM " \t\r\n"

extern char **environ;


char* find_executable(char* cmd) {
    if (cmd[0] == '/') {
       
        if (access(cmd, X_OK) == 0) return cmd;
        else return NULL;
    }

    char* path_env = getenv("PATH");
    if (!path_env) return NULL;

    char* path = strdup(path_env);
    char* token;
    char* saveptr;
    token = strtok_r(path, ":", &saveptr);
    static char full_path[1024];

    while (token != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", token, cmd);
        if (access(full_path, X_OK) == 0) {
            free(path);
            return full_path;
        }
        token = strtok_r(NULL, ":", &saveptr);
    }

    free(path);
    return NULL;
}

int main() {
    char input[MAX_INPUT];
    char* args[MAX_ARGS];
    char* user = getenv("USER");

    while (1) {
        printf("%s@SiSH> ", user ? user : "user");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;

     
        if (strncmp(input, "quit", 4) == 0) break;


        char* token;
        char* saveptr;
        int i = 0;
        token = strtok_r(input, DELIM, &saveptr);
        while (token != NULL && i < MAX_ARGS - 1) {
            args[i++] = token;
            token = strtok_r(NULL, DELIM, &saveptr);
        }
        args[i] = NULL;
        if (i == 0) continue; // Empty input


        char* exec_path = find_executable(args[0]);
        if (!exec_path) {
            fprintf(stderr, "SiSH: command not found: %s\n", args[0]);
            continue;
        }


        pid_t pid = fork();
        if (pid == 0) {
        
            if (execve(exec_path, args, environ) == -1) {
                perror("SiSH execve failed");
                exit(EXIT_FAILURE);
            }
        } else if (pid > 0) {
            
            wait(NULL);
        } else {
            perror("SiSH fork failed");
        }
    }

    printf("Exiting SiSH...\n");
    return 0;
}
