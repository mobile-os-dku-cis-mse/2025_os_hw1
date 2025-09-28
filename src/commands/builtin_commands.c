//
// OS PROJECT, 2025
// MiniShell
// File description:
// commands_handling
//

#include "my.h"

int cd_command(char **args, char **env)
{
    (void)env;
    if (args[1] == NULL) {
        const char *home = getenv("HOME");
        if (home == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
        if (chdir(home) != 0) {
            perror("cd");
            return 1;
        }
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
            return 1;
        }
    }
    return 0;
}

int quit_command(char **args, char **env)
{
    (void)args;
    (void)env;
    quit_executed = true;
    return 0;
}

builtin_command_t builtins[] = {
    {"cd", cd_command},
    {"quit", quit_command},
    {NULL, NULL}
};

bool is_builtin(command_t *cmd)
{
    if (cmd->args == NULL || cmd->args[0] == NULL) {
        return false;
    }
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(cmd->args[0], builtins[i].name) == 0) {
            return true;
        }
    }
    return false;
}

void execute_builtin(command_t *cmd, shell_t *shell)
{
    if (cmd->args == NULL || cmd->args[0] == NULL) {
        return;
    }
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(cmd->args[0], builtins[i].name) == 0) {
            shell->last_status = builtins[i].func(cmd->args, shell->env);
            return;
        }
    }
    fprintf(stderr, "%s: command not found\n", cmd->args[0]);
    shell->last_status = 127;
}
