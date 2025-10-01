#define _XOPEN_SOURCE 700
#include "signal_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

static void sigint_handler(int sig) {
    int saved_errno = errno;

    const char newline[] = "\n";
    write(STDOUT_FILENO, newline, 1);

    errno = saved_errno;
}

// Note : 나중에 잡 컨트롤 작업을 넣게 된다면 본격적으로 사용될 것.
static void sigchld_handler(int sig) {
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }

    errno = saved_errno;
}

static int setup_sigaction(int signum, void (*handler)(int), int flags) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sa.sa_flags = flags;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGQUIT);
    sigaddset(&sa.sa_mask, SIGCHLD);
    sigaddset(&sa.sa_mask, SIGTSTP);

    return sigaction(signum, &sa, NULL);
}

void reset_child_signals(void) {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);

    sigset_t set;
    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, &set, NULL);
}

void setup_signal_handlers(void) {
    if (setup_sigaction(SIGINT, sigint_handler, SA_RESTART) < 0) {
        perror("sigaction SIGINT");
        exit(1);
    }

    if (setup_sigaction(SIGQUIT, SIG_IGN, 0) < 0) {
        perror("sigaction SIGQUIT");
        exit(1);
    }

    if (setup_sigaction(SIGTSTP, SIG_IGN, 0) < 0) {
        perror("sigaction SIGTSTP");
        exit(1);
    }

    if (setup_sigaction(SIGCHLD, sigchld_handler, 0) < 0) {
        perror("sigaction SIGCHLD");
        exit(1);
    }
}
