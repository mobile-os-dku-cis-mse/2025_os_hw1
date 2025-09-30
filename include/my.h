#ifndef MY_H
#define MY_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_ARGS 128
#define PATH_BUF_SIZE PATH_MAX

char *strip_newline(char *s);
const char *get_prompt(void);

int parse_line(char *line, char *args[], int max_args);

void exec_with_path(char *cmd, char *args[]);
void run_command(char *args[]);

#endif
