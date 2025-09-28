//
// OS PROJECT, 2025
// MiniShell
// File description:
// main
//

#include "my.h"

int main(int ac, char **av, char **env)
{
    shell_t *shell = init_shell(env);

    if (ac != 1 || av[1] != NULL) {
        fprintf(stderr, "Usage: %s\n", av[0]);
        return 84;
    }
    setup_signal_handlers();
    while (1) {
        shell->input = read_input();
        if (shell->input == NULL) {
            printf("exit\n");
            break;
        }
        if (strlen(shell->input) > 0) {
            shell->commands = parse_input(shell->input);
            if (shell->commands != NULL) {
                execute_commands(shell);
                free_commands(shell->commands);
            }
        }
        free(shell->input);
        shell->input = NULL;
        if (quit_executed) break;
    }
    free_shell(shell);
    return 0;
}

// a faire le cd !!!
// if (verif_cd(shell->buffer)) {
//         if (access(shell->entry[1], F_OK) == -1)
//             write(2, "cd: No such file or directory.\n", 31);
//         else if (access(shell->entry[1], R_OK) == -1)
//             write(2, "cd: Permission denied.\n", 23);
//         else
//             chdir(shell->entry[1]);
//         return 1;
//     