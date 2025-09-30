#include "../include/my.h"

int main(void) {
    char *line = NULL;
    size_t linecap = 0;

    while (1) {
        printf("%s", get_prompt());
        fflush(stdout);

        if (getline(&line, &linecap, stdin) == -1) {
            putchar('\n');
            break;
        }

        strip_newline(line);
        if (*line == '\0') continue;

        if (strcmp(line, "quit") == 0) break;

        char *args[MAX_ARGS];
        int argc = parse_line(line, args, MAX_ARGS);
        if (argc > 0) {
            run_command(args);
        }
    }

    free(line);
    return 0;
}