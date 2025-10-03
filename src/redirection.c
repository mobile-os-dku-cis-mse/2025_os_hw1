#define _POSIX_C_SOURCE 200809L

#include "redirection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int apply_redirections(const redirection *redirs, int count) {
    for (int i = 0; i < count; i++) {
        const redirection *r = &redirs[i];
        int fd;
        
        switch (r->type) {
                fd = open(r->filename, O_RDONLY);
                if (fd < 0) {
                    fprintf(stderr, "%s: %s\n", r->filename, strerror(errno));
                    return -1;
                }
                
                if (dup2(fd, STDIN_FILENO) < 0) {
                    perror("redirP_indup2");
                    close(fd);
                    return -1;
                }
                close(fd);
                break;
                
            case REDIR_OUTPUT:
                fd = open(r->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    fprintf(stderr, "%s: %s\n", r->filename, strerror(errno));
                    return -1;
                }
                
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("redir_out:dup2");
                    close(fd);
                    return -1;
                }
                close(fd);
                break;
                
            case REDIR_APPEND:
                fd = open(r->filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) {
                    fprintf(stderr, "%s: %s\n", r->filename, strerror(errno));
                    return -1;
                }
                
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("redir_app:dup2");
                    close(fd);
                    return -1;
                }
                close(fd);
                break;
                
            default:
                break;
        }
    }
    
    return 0;
}