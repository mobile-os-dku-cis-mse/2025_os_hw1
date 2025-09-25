//
// OS PROJECT, 2025
// MiniShell
// File description:
// signals_handling
//

#include "my.h"

void handle_sigint(int sig)
{
    (void)sig;
    write(STDOUT_FILENO, "\n$> ", 4);
}

void handle_sigquit(int sig)
{
    (void)sig;
    write(STDOUT_FILENO, "\nQuit: 3\n$> ", 12);
}

void setup_signal_handlers(void)
{
    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigquit);
}
