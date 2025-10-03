#define _POSIX_C_SOURCE 200809L

#include "command.h"
#include "redirection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static bool is_redir_token(const char *token) {
    return (strcmp(token, ">") == 0 ||
            strcmp(token, ">>") == 0 ||
            strcmp(token, "<") == 0);
}

static redir_type get_redir_type(const char *token) {
    if (strcmp(token, ">") == 0) return REDIR_OUTPUT;
    if (strcmp(token, ">>") == 0) return REDIR_APPEND;
    if (strcmp(token, "<") == 0) return REDIR_INPUT;
    return REDIR_NONE;
}

int parse_command(char *line, command_t *cmd) {
    char *token;
    int arg_idx = 0;
    
    memset(cmd, 0, sizeof(command_t));

    token = strtok(line, " \t\n");
    
    while (token != NULL) {
        if (is_redir_token(token)) {
            redir_type type = get_redir_type(token);
            
            token = strtok(NULL, " \t\n");
            if (token == NULL) {
                fprintf(stderr, "syntax error: missing filename after redirection\n");
                cleanup_command(cmd);
                return -1;
            }
            
            if (cmd->redir_count > MAX_REDIR) {
                fprintf(stderr, "too many redirections\n");
                cleanup_command(cmd);
                return -1;
            }
            
            cmd->redirs[cmd->redir_count].type = type;
            cmd->redirs[cmd->redir_count].filename = strdup(token);
            
            if (cmd->redirs[cmd->redir_count].filename == NULL) {
                perror("strdup");
                cleanup_command(cmd);
                return -1;
            }
            
            cmd->redir_count++;
            
        } else {
            if (arg_idx > MAX_ARGS) {
                fprintf(stderr, "too many arguments\n");
                cleanup_command(cmd);
                return -1;
            }
            cmd->args[arg_idx++] = token;
        }
        
        token = strtok(NULL, " \t\n");
    }
    
    cmd->args[arg_idx] = NULL;
    
    if (arg_idx == 0) {
        cleanup_command(cmd);
        return -1;
    }
    
    return 0;
}

void cleanup_command(command_t *cmd) {
    for (int i = 0; i < cmd->redir_count; i++) {
        if (cmd->redirs[i].filename != NULL) {
            free(cmd->redirs[i].filename);
            cmd->redirs[i].filename = NULL;
        }
    }
    cmd->redir_count = 0;
}