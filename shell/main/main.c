#include "sish.h"

int main(int argc, char **argv, char **envp)
{
    if (argc != 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s\n", argv[0]);
        return 0;

    }
    my_shell(envp);
    return 0;   
}   