// command_executor.c
#include <stdlib.h>
#include <unistd.h>
#include "command_executor.h"
#include "command_finder.h"
#include "error_printer.h"

// Execute command
void execute_command(char **argv){
    // Convert to absolute path
    char *path = find_command(argv[0]);

    // If command is not found, print error and exit
    if(!path){
        print_error("Command not found");
        exit(EXIT_FAILURE);
    }

    // Execute command
    execve(path, argv, NULL);

    // If execution fails, print error and exit 
    print_error("Failed to execute command");
    exit(EXIT_FAILURE);
}