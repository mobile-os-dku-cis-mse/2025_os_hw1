// command_splitter.c
#include <string.h>
#include "command_splitter.h"

// Split command
int split_command(char *input, char **argv){
    int argc = 0;

    // Split string
    for(char *token = strtok(input, " "); token != NULL; token = strtok(NULL, " ")){
        argv[argc++] = token;
    }    

    // Save NULL at the end of array argv to call execve
    argv[argc] = NULL;
    return argc;
}