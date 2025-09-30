#ifndef SISH_H
#define SISH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <errno.h>
#include <stdbool.h>

#ifndef UNUSED
    #define UNUSED __attribute__((unused))
#endif

#define PROMPT "&> "
#define MAX_ARGS 64
#define PATH_ENV "PATH="

char *read_line(void);

int parse_args(char *line, char *argv_out[MAX_ARGS]);

const char *get_path_value(char **env);

bool find_in_path(const char *cmd, const char *path_value, char full[PATH_MAX]);

int exec_command(const char *full, char *const argv[], char *const envp[]);

#endif /* SISH_H */
