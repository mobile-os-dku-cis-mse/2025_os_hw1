#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <limits.h>
#include "builtins.h"
#include "command.h"
#include "signal_handler.h"

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

bool find_command_path(command_t *cmd) {
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
        snprintf(cmd->executable_path, MAX_PATH_LEN, "%s/%s", dir, cmd->args[0]);

        if (access(cmd->executable_path, X_OK) == 0) {
            free(path_copy);
            return true;
        }

        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return false;
}

void launch_process(command_t *cmd) {
    pid_t pid = fork();
    int status;

    if (pid < 0) {
        perror("launch_process:fork");
        return;
    }

    if (pid == 0) {
        reset_child_signals();

        if (apply_redirections(cmd->redirs, cmd->redir_count) < 0) {
            exit(1);
        }

        if (execv(cmd->executable_path, cmd->args) == -1) {
            perror("launch_process:pid==0");
            exit(EXIT_FAILURE);
        }
    } else {
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
}

int main() {
    setup_signal_handlers();

    while (1) {
        command_t cmd;

        char *command_line = read_user_command();
        if (command_line == NULL) continue;

        if (parse_command(command_line, &cmd) != 0) {
            cleanup_command(&cmd);
            continue;
        }

        if (cmd.args[0] == NULL) {
            cleanup_command(&cmd);
            continue;
        }

        bool is_builtin = check_builtins(cmd.args);

        if (!is_builtin) {
            find_command_path(&cmd);
            launch_process(&cmd);
        }

        cleanup_command(&cmd);
    }

    return 0;
}
