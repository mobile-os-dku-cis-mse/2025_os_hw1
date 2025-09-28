#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100  
///////////////////////////////////////////////
char *history[MAX_HISTORY];
int history_count = 0;

void add_history(const char *cmd) {
    if (history_count < MAX_HISTORY) {
        history[history_count++] = strdup(cmd);
    } else {
        free(history[0]);
        for (int i = 1; i < MAX_HISTORY; i++) {
            history[i - 1] = history[i];
        }
        history[MAX_HISTORY - 1] = strdup(cmd);
    }
}

void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
}

void parse_input(char *input, char **args) {
    int i = 0;
    char *token = strtok(input, " \t\n");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
}
//////////////////////////////////////////////////
int main() {
    char input[MAX_LINE];
    char *args[MAX_ARGS];
    pid_t pid;
    int status;
	
    while (1) {
        char *user = getenv("USER");
        if (!user) user = "unknown";

        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd ERR");
            strcpy(cwd, "?");
        }

        printf("%s@goronoSH:%s$ ", user, cwd);
        fflush(stdout);
	//////////////////////////////////////////
	if (fgets(input, MAX_LINE, stdin) == NULL) {
            printf("\n");
            break;
        }

        if (input[strlen(input) - 1] == '\n')
            input[strlen(input) - 1] = '\0';

        if (strcmp(input, "quit") == 0) {
            printf("Exiting goronoSH...\n");
            break;
	}

	parse_input(input, args);
	if (args[0] == NULL) continue;
	//////////////////////////////////////////
	if (strcmp(args[0], "cd") == 0) {
		if (args[1] == NULL) {
               		char *home = getenv("HOME");
                	if (home == NULL) home = "/";
                	if (chdir(home) != 0) perror("cd");
           	} else {
                if (chdir(args[1]) != 0) perror("cd");
            }
	    add_history(input);
            continue;
        }

	if (strcmp(args[0], "history") == 0) {
    	    print_history();
            continue;
	}
	///////////////////////////////////////////
        pid = fork();
     	if (pid == 0) {
            execvp(args[0], args);
            perror("execvp ERR");
            exit(1);
        }
        else {
	    add_history(input);
            waitpid(pid, &status, 0);
        }
	///////////////////////////////////////////
    }
    return 0;
}
