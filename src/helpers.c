#include "../include/my.h"

char *strip_newline(char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }
    return s;
}

const char *get_prompt(void) {
    const char *ps1 = getenv("PS1");
    return ps1 ? ps1 : "SiSH> ";
}