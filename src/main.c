#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <limits.h>
#include "builtins.h"
#include "signal_handler.h"

#define MAX_CMD_LEN  1024
#define MAX_PATH_LEN 1024
#define MAX_ARGS     128

void print_prompt() {
    char cwd[PATH_MAX + 1];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        strncpy(cwd, "", sizeof(cwd));
    }

    char *home_dir = getenv("HOME");
    char *display_path = cwd;

    if (home_dir != NULL) {
        size_t home_len = strlen(home_dir);
        if (strncmp(cwd, home_dir, home_len) == 0) {
            display_path = cwd + home_len;
            printf("From ( ~%s ) sish> ", display_path);
            return;
        }
    }

    printf("From ( %s ) sish> ", display_path);
    fflush(stdout);
}

char *read_user_command() {
    static char line[MAX_CMD_LEN];
    print_prompt();
    fgets(line, sizeof(line), stdin);

    if (strchr(line, '\n') == NULL) {
        fprintf(stderr, "Error: Input command is too long.\n");

        int c;
        while ((c = getchar()) != '\n' && c != EOF);

        return NULL;
    }

    line[strcspn(line, "\n")] = '\0';
    return line;
}

int parse_command(char *line, char **command_argv) {
    int i = 0;
    char *token = strtok(line, " \t\r\n");

    while (token != NULL && i < MAX_ARGS - 1) {
        command_argv[i++] = token;

        token = strtok(NULL, " \t\r\n");
    }

    if (token != NULL) {
        fprintf(stderr, "sish: Error: Too many arguments.\n");
        return -1;
    }

    command_argv[i] = NULL;
    return 0;
}


bool find_command_path(const char *program, char *full_path) {
    char *path_env = getenv("PATH");
    if (path_env == NULL) {
        return false;
    }

    size_t len = strlen(path_env);
    char *path_copy = (char *) malloc(len + 1);
    if (path_copy == NULL) {
        perror("find_command_path:malloc");
        return false;
    }
    strcpy(path_copy, path_env);

    char *dir = strtok(path_copy, ":");

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

void launch_process(char **argv, const char *executable_path) {
    pid_t pid = fork();
    int status;

    if (pid < 0) {
        perror("launch_process:fork");
        return;
    }

    if (pid == 0) {
        reset_child_signals();

        if (execv(executable_path, argv) == -1) {
            perror("launch_process:pid==0");
            exit(EXIT_FAILURE);
        }
    } else {
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
}

int main(int argc, const char *argv[]) {
    setup_signal_handlers();

    while (1) {
        char *command_argv[MAX_ARGS];
        char executable_path[MAX_PATH_LEN];

        char *command_line = read_user_command();
        if (command_line == NULL) continue;

        if(parse_command(command_line, command_argv) != 0) continue;
        if (command_argv[0] == NULL) continue;

        bool is_builtin = check_builtins(command_argv);

        if (!is_builtin) {
            find_command_path(command_argv[0], executable_path);
            launch_process(command_argv, executable_path);
        }
    }

    return 0;
}
