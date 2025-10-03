#ifndef INC_2025_OS_HW1_REDIRECTION_H
#define INC_2025_OS_HW1_REDIRECTION_H

typedef enum {
    REDIR_NONE = 0,
    REDIR_INPUT,
    REDIR_OUTPUT,
    REDIR_APPEND
} redir_type;

typedef struct {
    redir_type type;
    char *filename;
} redirection;

int apply_redirections(const redirection *redirs, int count);

#endif