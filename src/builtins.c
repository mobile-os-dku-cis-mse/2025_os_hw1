#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "builtins.h"

typedef int (*builtin_func)(char **args);

typedef struct {
    char *name;
    builtin_func func;
} BuiltinCommand;

int builtin_exit(char **args) {
    (void)args;
    return 0;
}

int builtin_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "Don't you think you've forgotten something?\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("builtin_cd");
        }
    }
    return 1;
}

int builtin_echo(char **args) {
    int i = 1;
    while (args[i] != NULL) {
        printf("%s", args[i]);
        if (args[i+1] != NULL) {
            printf(" ");
        }
        i++;
    }
    printf("\n");
    return 1;
}


int builtin_pwd(char **args) {
    (void)args;
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("builtins_pwd");
    }
    return 1;
}

BuiltinCommand builtins[] = {
    { "exit", &builtin_exit },
    { "cd",   &builtin_cd   },
    { "echo", &builtin_echo },
    { "pwd",  &builtin_pwd  },
    { NULL,   NULL       }
};

bool check_builtins(char** command) {
    bool is_builtin = false;
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(command[0], builtins[i].name) == 0) {
            int status = builtins[i].func(command);
            is_builtin = true;

            if (status == 0) {
                exit(EXIT_SUCCESS);
            }
            break;
        }
    }
    return is_builtin;
}