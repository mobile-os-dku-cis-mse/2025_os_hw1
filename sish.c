// filepath: /home/thorbenschoenlein/2025_os_hw1/sish.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **environ;

#define MAX_INPUT 1024
#define MAX_ARGS 64

// Prints the shell prompt, showing user and current working directory if available
void print_prompt() {
    char *cwd = getenv("PWD");
    char *user = getenv("USER");
    if (user && cwd) {
        printf("[%s@%s]$ ", user, cwd);
    } else {
        printf("SiSH$ ");
    }
    fflush(stdout);
}

// Parses the input line into arguments for execve
void parse_input(char *input, char **args, int *argc) {
    char *token;
    *argc = 0;
    token = strtok(input, " \t\n"); // Split input by whitespace and newline
    while (token != NULL && *argc < MAX_ARGS - 1) {
        args[(*argc)++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[*argc] = NULL; // Null-terminate the argument list
}

// Finds the full path of the executable by searching PATH or using absolute/relative path
char *find_executable(char *cmd) {
    // If command contains '/', treat as path and check if executable
    if (strchr(cmd, '/')) {
        if (access(cmd, X_OK) == 0) {
            return cmd;
        }
        return NULL;
    }
    // Otherwise, search each directory in PATH
    char *path_env = getenv("PATH");
    if (!path_env) return NULL;
    char *paths = strdup(path_env);
    char *saveptr = NULL;
    char *dir = strtok_r(paths, ":", &saveptr);
    static char fullpath[512];
    while (dir) {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, cmd);
        if (access(fullpath, X_OK) == 0) {
            free(paths);
            return fullpath;
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }
    free(paths);
    return NULL;
}

int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];
    int argc;

    while (1) {
        print_prompt(); // Show prompt
        if (!fgets(input, sizeof(input), stdin)) { // Read user input
            printf("\n");
            break;
        }
        if (input[0] == '\n') continue; // Ignore empty input

        parse_input(input, args, &argc); // Parse input into arguments
        if (argc == 0) continue; // Ignore if no command

        if (strcmp(args[0], "quit") == 0) { // Exit on "quit"
            break;
        }

        char *exec_path = find_executable(args[0]); // Find executable path
        if (!exec_path) {
            fprintf(stderr, "SiSH: command not found: %s\n", args[0]);
            continue;
        }

        pid_t pid = fork(); // Create child process
        if (pid < 0) {
            perror("fork");
            continue;
        } else if (pid == 0) {
            // In child: execute the command
            execve(exec_path, args, environ);
            perror("execve"); // Only reached if execve fails
            exit(EXIT_FAILURE);
        } else {
            // In parent: wait for child to finish
            int status;
            waitpid(pid, &status, 0);
        }
    }
    return 0;
}