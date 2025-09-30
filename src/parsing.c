#include "../include/my.h"

int parse_line(char *line, char *args[], int max_args) {
    int argc = 0;
    char *saveptr = NULL;
    char *token = strtok_r(line, " \t", &saveptr);

    while (token && argc < max_args - 1) {
        args[argc++] = token;
        token = strtok_r(NULL, " \t", &saveptr);
    }
    args[argc] = NULL;
    return argc;
}