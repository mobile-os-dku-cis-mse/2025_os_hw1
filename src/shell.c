#include "sish.h"


char *check_access(char *cmd, char **envp)
{
    char *path = NULL;
    char *token = NULL;
    char *full_path = NULL;

    for (int i = 0; envp[i] != NULL; i++) {
        if (strncmp(envp[i], "PATH=", 5) == 0) {
            path = calloc(1, strlen(envp[i]) - 3);
            if (!path)
                return NULL;
            
            strcat(path, envp[i] + 5);
            break;
        }
            
    }

    if (!path)
        return NULL;
    
    token = strtok(path, ":");

    while (token != NULL) {
        full_path = malloc(strlen(token) + strlen(cmd) + 2);
        sprintf(full_path, "%s/%s", token, cmd);
    
        if (!access(full_path, X_OK)) {
            return full_path;
        }
        token = strtok(NULL, ":");
        free(full_path);
    }
    free(path);
    return NULL;
}

int count_args(char *buf)
{
    int count = 0;
    
    for (size_t i = 0; buf[i] != '\0'; i++) {
        if (buf[i] == ' ')
            count++;
    }

    return count;
}

char **parse_args(char *buf, int nbr_args)
{
    char **args = malloc((nbr_args + 2) * sizeof(char *));
    if (!args)
        return NULL;

    char *token = strtok(buf, " ");
    int index = 0;

    while (token != NULL && index < nbr_args + 1) {
        args[index] = token;
        index++;
        token = strtok(NULL, " ");
    }
    args[index] = NULL;
    return args;
}

int execution(char *buf, char **envp)
{
    if (!buf || buf[0] == '\n')
        return -1;

    buf[strcspn(buf, "\n")] = 0;

    char *command_without_args = my_strncat(buf, 0, strcspn(buf, " "));
    char *command = check_access(command_without_args, envp);
    int nbr_args = count_args(buf);
    char **args = parse_args(buf, nbr_args);

    if (!command) {
        printf("Command not found: %s\n", buf);
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return -1;
    } else if (pid == 0) {
        if (execve(command, args, envp) == -1) {
            perror("Execution failed");
            exit(EXIT_FAILURE);
        }
    } else {
        int status = 0;
        waitpid(pid, &status, 0);
    }
    free(command_without_args);
    free(command);
    free(args);
    return 0;
}

int my_shell(char **envp)
{
    char *buf = NULL;
    size_t len = 0;
    int read = 0;

    while (1) {
        printf("sish> ");
        
        read = getline(&buf, &len, stdin);
        
        if (read == -1) {
            printf("\nexit\n");
            free(buf);
            exit(0);
        }
        execution(buf, envp);
        buf = NULL;
    }
    free(buf);
    return 0;
}




