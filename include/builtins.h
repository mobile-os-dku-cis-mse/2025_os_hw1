#ifndef INC_2025_OS_HW1_BUILTINS_H
#define INC_2025_OS_HW1_BUILTINS_H

typedef int (*builtin_func)(char **args);

typedef struct {
    char *name;
    builtin_func func;
} BuiltinCommand;

extern BuiltinCommand builtins[];

#endif