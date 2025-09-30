#include <stdio.h>
#include <unistd.h>
#include "builtins.h"

int builtin_exit(char **args) {
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
