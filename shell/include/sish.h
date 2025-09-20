#ifndef SISH_H
    #define SISH_H
    
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/wait.h>

    int my_shell(char **envp);
    char *my_strncat(char *src, size_t start, size_t end);

#endif // SISH_H