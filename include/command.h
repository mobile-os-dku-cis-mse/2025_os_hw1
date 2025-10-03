#ifndef INC_2025_OS_HW1_COMMAND_H
#define INC_2025_OS_HW1_COMMAND_H

#define MAX_CMD_LEN  1024
#define MAX_REDIR    3
#define MAX_ARGS     128

#include "redirection.h"

typedef struct {
    char *args[MAX_ARGS];
    redirection redirs[MAX_REDIR];
    int redir_count;
} command_t;

int parse_command(char *line, command_t *cmd);
void cleanup_command(command_t *cmd);

#endif