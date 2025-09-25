//
// OS PROJECT, 2025
// MiniShell
// File description:
// init
//

#include "my.h"

shell_t *init_shell(char **env)
{
    shell_t *shell = malloc(sizeof(shell_t));
    if (shell == NULL) {
        perror("malloc");
        exit(84);
    }
    shell->input = NULL;
    shell->commands = NULL;
    shell->env = NULL;
    shell->last_status = 0;

    // Count environment variables
    size_t env_count = 0;
    while (env[env_count] != NULL) {
        env_count++;
    }

    // Allocate memory for environment variables
    shell->env = malloc((env_count + 1) * sizeof(char *));
    if (shell->env == NULL) {
        perror("malloc");
        free(shell);
        exit(84);
    }

    // Copy environment variables
    for (size_t i = 0; i < env_count; i++) {
        shell->env[i] = strdup(env[i]);
        if (shell->env[i] == NULL) {
            perror("strdup");
            for (size_t j = 0; j < i; j++) {
                free(shell->env[j]);
            }
            free(shell->env);
            free(shell);
            exit(84);
        }
    }
    shell->env[env_count] = NULL;
    return shell;
}

void free_shell(shell_t *shell)
{
    if (shell->env != NULL) {
        for (size_t i = 0; shell->env[i] != NULL; i++) {
            free(shell->env[i]);
        }
        free(shell->env);
    }
    free(shell);
}
