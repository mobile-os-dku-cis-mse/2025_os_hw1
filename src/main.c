#include "sish.h"

int main(UNUSED int argc,UNUSED char *argv[], char **env)
{
    while (1) {
        char *buffer = NULL;
        size_t line = 0;

        printf("&> ");
        if (getline(&buffer, &line, stdin) == -1) {
            free(buffer);
            break;
        }

        char *slice_buffer[64];
        int len_buffer = 0;

        char *tok = strtok(buffer, " \t");
        while (tok != NULL && len_buffer < 63) {
            slice_buffer[len_buffer++] = tok;
            tok = strtok(NULL, " \t");
        }
        slice_buffer[len_buffer] = NULL;

        for (int i = 0; env[i] != NULL; i++) {
            if (!strncmp(env[i], PATH_ENV, strlen(PATH_ENV))) {
                char *paths = strdup(env[i]);
                char *path = strtok(paths + strlen(PATH_ENV), ":");

                while (path != NULL) {
                    slice_buffer[len_buffer - 1][strcspn(slice_buffer[0], "\n")] = 0;
                    char full[1024];
                    snprintf(full, sizeof(full), "%s/%s", path, slice_buffer[0]);
                    if (access(full, F_OK) == 0) {
                        int pid = fork();
                        if (pid == 0) {
                            execve(full, slice_buffer, env);
                            _exit(127);
                        } else {
                            waitpid(pid, 0, 0);
                        }
                    }
                    path = strtok(NULL, ":");
                }
                free(paths);
            }
        }
        free(buffer);
    }
}
