#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>

extern char **environ;  

char **split_line(char *line, int *argc){
    int cap = 16, n = 0;
    char **argv = malloc(sizeof(char*)*cap);
    char *tok, *saveptr; 
    for (tok = strtok_r(line, " \t\n", &saveptr); tok; tok = strtok_r(NULL, " \t\n", &saveptr)) {
        if (n+1>= cap) argv = realloc(argv, sizeof(char*)*(cap*=2));
        argv[n++] = strdup(tok);
    }
    argv[n] = NULL;
    if (argc) *argc = n;
    return argv;
}

void free_argv(char **argv) {
    for (int i=0; argv[i]; i++) free(argv[i]);
    free(argv);
}

void try_exec(const char *path, char *const argv[]) {
    execve(path, argv, environ);
    fprintf(stderr,"sish failed '%s': %s\n", path, strerror(errno));
    _exit(127);
}

int main(void) {
    char *line = NULL;
    size_t cap = 0;
    printf("Shell SiSH. To exit, type quit\n");
    while (1) {
        printf("sish> "); fflush(stdout);
        if (getline(&line,&cap,stdin)<0) { if(feof(stdin)) break; perror("getline"); continue;}

        int argc = 0;
        char **argv = split_line(line,&argc);
        if (!argv || argc==0) { free_argv(argv); continue;}
        if (strcmp(argv[0],"quit")==0) {free_argv(argv); break;}
        pid_t pid = fork();
        if (pid < 0) {perror("fork"); free_argv(argv); continue;}
        if (pid==0) {
            if (strchr(argv[0],'/')) {
                if (access(argv[0],X_OK)==0) try_exec(argv[0],argv);
                fprintf(stderr,"sish: %s: %s\n", argv[0], strerror(errno)); _exit(127);
            } else {
                char *path = getenv("PATH"); if(!path) path="/bin:/usr/bin";
                char *copy = strdup(path), *dir, *saveptr;
                char candidate[PATH_MAX]; int found=0;
                for (dir = strtok_r(copy, ":", &saveptr); dir; dir=strtok_r(NULL,":",&saveptr)) {
                    if(!*dir) dir=".";
                    if(snprintf(candidate,sizeof(candidate),"%s/%s",dir,argv[0])<PATH_MAX && access(candidate,X_OK)==0)
                        try_exec(candidate,argv);
                }
                free(copy);
                fprintf(stderr,"%s not found",argv[0]);
                _exit(127);
            }
        } else {
            int status;
            wait(&status);
        }
        free_argv(argv);
    }
    free(line);
    return 0;
}
