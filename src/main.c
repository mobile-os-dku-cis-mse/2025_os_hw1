#include "sish.h"

void execute_cmd(char full[], char *slice_buffer[], char **env)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        execve(full, slice_buffer, env);
        perror("execve");
        _exit(127);
    } else {
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
        }
    }
}


void check_path(char **env, char *slice_buffer[]) {

    for (int i = 0; env[i] != NULL; i++) {
        if (!strncmp(env[i], PATH_ENV, strlen(PATH_ENV))) {
            char *paths = strdup(env[i]);
            if (!paths) {
                perror("strdup"); return;
            }
            char *path = strtok(paths + strlen(PATH_ENV), ":");

            while (path != NULL) {
                if (*path == '\0') path = ".";
                char full[1024];
                snprintf(full, sizeof(full), "%s/%s", path, slice_buffer[0]);
                if (access(full, X_OK) == 0) {
                    execute_cmd(full, slice_buffer, env);
                    free(paths);
                    return;
                }
                path = strtok(NULL, ":");
            }
            free(paths);
            fprintf(stderr, "%s: command not found\n", slice_buffer[0]);
            return;
        }
    }
    fprintf(stderr, "%s: command not found (no PATH)\n", slice_buffer[0]);
}


void handle_cmd(char *buffer, char **env) {
    char *slice_buffer[64];
    int len_buffer = 0;
    buffer[strcspn(buffer, "\n")] = '\0';


    char *tok = strtok(buffer, " \t");
    if (!tok) { free(buffer); return; }

    while (tok != NULL && len_buffer < 63) {
        slice_buffer[len_buffer++] = tok;
        tok = strtok(NULL, " \t");
    }
    slice_buffer[len_buffer] = NULL;

    if (strcmp(slice_buffer[0], "exit") == 0) {
        free(buffer);
        exit(0);
    }

    if (strchr(slice_buffer[0], '/')) {
        if (access(slice_buffer[0], X_OK) == 0) {
            execute_cmd(slice_buffer[0], slice_buffer, env);
            free(buffer);
            return;
        }
        fprintf(stderr, "%s: not found or not executable\n", slice_buffer[0]);
        free(buffer);
        return;
    }

    check_path(env, slice_buffer);
    free(buffer);
}


int main(UNUSED int argc,UNUSED char *argv[], char **env)
{
    signal(SIGINT, SIG_IGN);

    while (1)
    {
        char *buffer = NULL;
        size_t line = 0;

        printf("&> ");
        fflush(stdout);
        if (getline(&buffer, &line, stdin) == -1) {
            free(buffer);
            break;
        }
        handle_cmd(buffer, env);
    }

}
