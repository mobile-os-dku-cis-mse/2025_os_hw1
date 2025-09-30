#include "../include/my.h"

void exec_with_path(char *cmd, char *args[]) {
    char *pathenv = getenv("PATH");
    if (!pathenv) pathenv = "/bin:/usr/bin";

    char *pathdup = strdup(pathenv);
    if (!pathdup) {
        fprintf(stderr, "sish: strdup failed\n");
        _exit(127);
    }

    char *dir, *saveptr = NULL;
    char candidate[PATH_BUF_SIZE];
    dir = strtok_r(pathdup, ":", &saveptr);

    while (dir) {
        snprintf(candidate, sizeof(candidate), "%s/%s", dir, cmd);
        if (access(candidate, X_OK) == 0) {
            execv(candidate, args);
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(pathdup);
    fprintf(stderr, "sish: command not found: %s\n", cmd);
    _exit(127);
}

void run_command(char *args[]) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        char *cmd = args[0];
        if (strchr(cmd, '/')) {
            execv(cmd, args);
            fprintf(stderr, "sish: exec failed for %s: %s\n", cmd, strerror(errno));
            _exit(127);
        } else {
            exec_with_path(cmd, args);
        }
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
        }
    }
}