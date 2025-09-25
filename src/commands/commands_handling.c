//
// OS PROJECT, 2025
// MiniShell
// File description:
// commands_handling
//

#include "my.h"

void free_commands(command_t *head)
{
    command_t *current = head;
    command_t *next;

    while (current != NULL) {
        next = current->next;
        for (size_t i = 0; current->args[i] != NULL; i++) {
            free(current->args[i]);
        }
        free(current->args);
        free(current);
        current = next;
    }
}

char *read_input()
{
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    printf("$> ");
    read = getline(&line, &len, stdin);
    if (read == -1) {
        free(line);
        return NULL;
    }
    // Remove trailing newline
    if (read > 0 && line[read - 1] == '\n') {
        line[read - 1] = '\0';
    }
    return line;
}

command_t *parse_input(char *input)
{
    command_t *head = NULL;
    command_t *current = NULL;
    char *token;
    char *rest = input;

    while ((token = strtok_r(rest, "|", &rest))) {
        command_t *new_cmd = malloc(sizeof(command_t));
        if (new_cmd == NULL) {
            perror("malloc");
            free_commands(head);
            return NULL;
        }
        new_cmd->args = NULL;
        new_cmd->next = NULL;

        // Split command into arguments by spaces and tabs
        char *arg;
        char *arg_rest = token;
        size_t arg_count = 0;
        char **args = NULL;
        while ((arg = strtok_r(arg_rest, " \t", &arg_rest))) {
            char **temp = realloc(args, sizeof(char *) * (arg_count + 2));
            if (temp == NULL) {
                perror("realloc");
                free(args);
                free(new_cmd);
                free_commands(head);
                return NULL;
            }
            args = temp;
            args[arg_count] = strdup(arg);
            if (args[arg_count] == NULL) {
                perror("strdup");
                for (size_t i = 0; i < arg_count; i++) {
                    free(args[i]);
                }
                free(args);
                free(new_cmd);
                free_commands(head);
                return NULL;
            }
            arg_count++;
        }
        if (args != NULL) {
            args[arg_count] = NULL; // Null-terminate the args array
        }
        new_cmd->args = args;
        if (head == NULL) {
            head = new_cmd;
            current = new_cmd;
        } else {
            current->next = new_cmd;
            current = new_cmd;
        }
    }
    return head;
}

void execute_commands(shell_t *shell)
{
    command_t *cmd = shell->commands;
    int num_cmds = 0;
    for (command_t *c = cmd; c != NULL; c = c->next) {
        num_cmds++;
    }

    int **pipes = malloc(sizeof(int *) * (num_cmds - 1));
    if (pipes == NULL && num_cmds > 1) {
        perror("malloc");
        return;
    }
    for (int i = 0; i < num_cmds - 1; i++) {
        pipes[i] = malloc(sizeof(int) * 2);
        if (pipes[i] == NULL) {
            perror("malloc");
            for (int j = 0; j < i; j++) {
                free(pipes[j]);
            }
            free(pipes);
            return;
        }
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            for (int j = 0; j <= i; j++) {
                free(pipes[j]);
            }
            free(pipes);
            return;
        }
    }

    pid_t *pids = malloc(sizeof(pid_t) * num_cmds);
    if (pids == NULL) {
        perror("malloc");
        for (int i = 0; i < num_cmds - 1; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
            free(pipes[i]);
        }
        free(pipes);
        return;
    }

    int i = 0;
    for (command_t *c = cmd; c != NULL; c = c->next, i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("fork");
            break;
        } else if (pids[i] == 0) { // Child process
            // Set up pipes
            if (i > 0) { // Not the first command
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            if (i < num_cmds - 1) { // Not the last command
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Close all pipe fds in child
            for (int j = 0; j < num_cmds - 1 ; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            if (execvp(c->args[0], c->args) == -1) {
                fprintf(stderr, "%s: command not found\n", c->args[0]);
                exit(127);
            }
            exit(0); // Should never reach here
        }
        // Parent process
        if (i > 0) { // Close read end of previous pipe
            close(pipes[i - 1][0]);
            close(pipes[i - 1][1]);
        }
    }
    // Close last pipe in parent
    if (num_cmds > 1) {
        close(pipes[num_cmds - 2][0]);
        close(pipes[num_cmds - 2][1]);
    }
    // Free pipes array
    for (int j = 0; j < num_cmds - 1; j++) {
        free(pipes[j]);
    }
    free(pipes);
    // Wait for all children
    for (int j = 0; j < i; j++) {
        int status;
        waitpid(pids[j], &status, 0);
        if (WIFEXITED(status)) {
            shell->last_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            shell->last_status = 128 + WTERMSIG(status);
        }
    }
    free(pids);
    return;
}
