//
// OS PROJECT, 2025
// MiniShell
// File description:
// Makefile
//

#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>

typedef struct command_s {
    char **args;
    struct command_s *next;
} command_t;

typedef struct shell_s {
    char *input;
    command_t *commands;
    char **env;
    int last_status;
} shell_t;

void setup_signal_handlers(void);
shell_t *init_shell(char **env);
void free_shell(shell_t *shell);
char *read_input();
command_t *parse_input(char *input);
void execute_commands(shell_t *shell);
void free_commands(command_t *head);
