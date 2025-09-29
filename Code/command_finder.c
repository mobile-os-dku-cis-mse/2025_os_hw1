// command_finder.c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "command_finder.h"

// Load path
static const char* load_path(){
    return getenv("PATH");
}

// Find command
char* find_command(char *cmd){
    // If command starts with '/' or '.', return as-is
    if(cmd[0] == '/' || cmd[0] == '.'){
        return cmd;
    }

    // Copy environment variable PATH
    char path_copy[1024];
    strncpy(path_copy, load_path(), sizeof(path_copy));
    path_copy[sizeof(path_copy)-1] = '\0';

    // Tokenize environment variable PATH
    char *token = strtok(path_copy, ":");

    // Find command
    static char path[1024];
    while(token){
        snprintf(path, sizeof(path), "%s/%s", token, cmd);
        // If command is executable, return path
        if(access(path, X_OK) == 0){
            return path;
        }

        // Try next directory
        token = strtok(NULL, ":");
    }

    // If command is not found, return NULL
    return NULL;
}