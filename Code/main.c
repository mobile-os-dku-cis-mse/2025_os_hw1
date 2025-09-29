// main.c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "command_executor.h"
#include "command_finder.h"
#include "command_splitter.h"
#include "error_printer.h"
#include "prompt_printer.h"

static const char* load_home(){
    return getenv("HOME");
}

int main(){
    char *argv[1024];
    char input[1024];

    while(1){
        // Print prompt
        print_prompt();

        // Get user input
        if(fgets(input, sizeof(input), stdin) == NULL){
            printf("\n");
            break;
        }

        // Remove newline character
        input[strcspn(input, "\n")] = '\0';

        // If user input is empty string, ignore user input
        if(strlen(input) == 0){
            continue;
        }

        // Built-in command: exit & quit
        if(strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0){
            break;
        }

        // Split command
        split_command(input, argv);

        // Built-in command: cd
        if(strcmp(argv[0], "cd") == 0){
            if(argv[1] == NULL){
                chdir(load_home());
            }else if(chdir(argv[1]) != 0){
                print_error("Failed to change directory");
            }
            continue;
        }

        // You can add more built-in commands here.

        // External command
        pid_t pid = fork();
        if(pid < 0){
            print_error("Failed to fork");
        }else if(pid == 0){
            execute_command(argv);

            // If execution fail, exit
            exit(EXIT_FAILURE);
        }else{
            wait(NULL);
        }
    }
    return 0;
}