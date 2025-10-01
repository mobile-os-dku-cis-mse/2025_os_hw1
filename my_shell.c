/***************************************************************************//**

  @file         patch_5.c
  @brief        LSH (Libstephen SHell) — refactored:
                - Pipelines & I/O redirection
                - Background jobs (pidfd + epoll)
                - Foreground waits with SIGCHLD masking
                - Job control via setpgid()/tcsetpgrp()
                - Robust wait status decoding

*******************************************************************************/

#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

/* ============================== Builtins ================================== */

int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);

static char *builtin_str[] = {
  "cd",
  "help",
  "exit"
};

static int (*builtin_func[]) (char **) = {
  &lsh_cd,
  &lsh_help,
  &lsh_exit
};

static int lsh_num_builtins(void) {
  return (int)(sizeof(builtin_str) / sizeof(char *));
}

/* =========================== Globals / State ============================== */

static int   g_epfd        = -1;     /* epoll fd for pidfds */
static int   g_tty_fd      = 0;      /* controlling TTY (stdin by default) */
static pid_t g_shell_pgid  = -1;     /* shell's process group id */

/* ============================== Utilities ================================= */

static void die(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

/* Expand ~ or ~/path. "~user" not supported (kept as-is). */
static char *expand_tilde(const char *p) {
  if (!p || p[0] != '~') return (char*)p;
  const char *home = getenv("HOME");
  if (!home) return (char*)p;
  size_t hl = strlen(home), pl = strlen(p);
  char *r = malloc(hl + (p[1] == '/' ? pl : pl + 1));
  if (!r) die("lsh: malloc");
  if (p[1] == '\0') {
    strcpy(r, home);
  } else if (p[1] == '/') {
    strcpy(r, home);
    strcat(r, p + 1);
  } else {
    free(r);
    return (char*)p;
  }
  return r;
}

static int has_symbol(char **args, const char *sym) {
  for (int i = 0; args && args[i]; i++) if (strcmp(args[i], sym) == 0) return 1;
  return 0;
}

/* ============== Signal mask helpers (thread-local mask) =================== */

static void lsh_block_sigchld(sigset_t *oldmask) {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  if (pthread_sigmask(SIG_BLOCK, &set, oldmask) != 0) die("lsh: pthread_sigmask(SIG_BLOCK)");
}

static void lsh_restore_sigmask(const sigset_t *oldmask) {
  if (pthread_sigmask(SIG_SETMASK, oldmask, NULL) != 0) die("lsh: pthread_sigmask(SIG_SETMASK)");
}

/* ============================ pidfd helpers =============================== */

#ifndef SYS_pidfd_open
#  if defined(__x86_64__)
#    define SYS_pidfd_open 434
#  endif
#endif

static int lsh_pidfd_open(pid_t pid, unsigned int flags) {
#ifdef SYS_pidfd_open
  return (int)syscall(SYS_pidfd_open, pid, flags);
#else
  errno = ENOSYS;
  return -1;
#endif
}

/* Consume one pidfd state; on exit/kill, do final reap and close. */
static void lsh_consume_pidfd(int pidfd) {
  siginfo_t si = {0};
  if (waitid(P_PIDFD, pidfd, &si, WEXITED | WSTOPPED | WCONTINUED | WNOWAIT) == -1) {
    if (errno != EAGAIN && errno != EINTR) perror("lsh: waitid(P_PIDFD, WNOWAIT)");
    return;
  }
  if (si.si_code == CLD_EXITED || si.si_code == CLD_KILLED || si.si_code == CLD_DUMPED) {
    siginfo_t fin = {0};
    if (waitid(P_PIDFD, pidfd, &fin, WEXITED) == -1) {
      if (errno != EAGAIN && errno != EINTR) perror("lsh: waitid(P_PIDFD, reap)");
    }
    close(pidfd);
  }
}

static void lsh_epoll_drain_ready(void) {
  if (g_epfd < 0) return;
  struct epoll_event evs[32];
  int n = epoll_wait(g_epfd, evs, (int)(sizeof(evs)/sizeof(evs[0])), 0);
  if (n < 0) {
    if (errno != EINTR) perror("lsh: epoll_wait");
    return;
  }
  for (int i = 0; i < n; i++) {
    if (evs[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR)) {
      lsh_consume_pidfd(evs[i].data.fd);
    }
  }
}

/* Keep name compatibility with earlier “drain” calls. */
#define lsh_reap_nonblocking() lsh_epoll_drain_ready()

/* ============================ Job control ================================= */

static void lsh_tcsetpgrp(int fd, pid_t pgid) {
  if (fd < 0 || pgid <= 0) return;
  if (tcsetpgrp(fd, pgid) == -1) {
    if (errno != ENOTTY && errno != ESRCH) perror("lsh: tcsetpgrp");
  }
}

/* ====================== Line reading / tokenizing ========================= */

char *lsh_read_line(void) {
#define LSH_RL_BUFSIZE 1024
  int bufsize = LSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) die("lsh: allocation error");

  for (;;) {
    c = getchar();
    if (c == EOF) exit(EXIT_SUCCESS);
    else if (c == '\n') { buffer[position] = '\0'; return buffer; }
    else { buffer[position] = c; }
    position++;
    if (position >= bufsize) {
      bufsize += LSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) die("lsh: allocation error");
    }
  }
#undef LSH_RL_BUFSIZE
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
char **lsh_split_line(char *line) {
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token, **tokens_backup;
  if (!tokens) die("lsh: allocation error");

  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position++] = token;
    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        free(tokens_backup);
        die("lsh: allocation error");
      }
    }
    token = strtok(NULL, LSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

/* ==================== Pipelines & redirection parsing ===================== */

typedef struct {
  char **argv;    /* NULL-terminated */
  char *in_path;  /* or NULL */
  char *out_path; /* or NULL */
  int   append;   /* 0 => truncate, 1 => append */
} LshCmd;

static int parse_pipeline(char **args, LshCmd **out_cmds) {
  int cap = 4, ncmd = 0;
  LshCmd *cmds = calloc(cap, sizeof(LshCmd));
  if (!cmds) die("lsh: calloc");

  int i = 0;
  while (args[i]) {
    if (ncmd == cap) {
      cap *= 2;
      LshCmd *nc = realloc(cmds, cap * sizeof(LshCmd));
      if (!nc) die("lsh: realloc");
      cmds = nc;
    }
    LshCmd c = {0};
    int argv_cap = 8, argcnt = 0;
    c.argv = malloc(argv_cap * sizeof(char*));
    if (!c.argv) die("lsh: malloc");

    for (; args[i]; i++) {
      if (strcmp(args[i], "|") == 0) {
        i++;
        break;
      } else if (strcmp(args[i], "<") == 0) {
        if (!args[i+1]) { fprintf(stderr, "lsh: syntax error near '<'\n"); goto fail; }
        c.in_path = expand_tilde(args[i+1]);
        i += 2;
        continue;
      } else if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
        if (!args[i+1]) { fprintf(stderr, "lsh: syntax error near '>'\n"); goto fail; }
        c.append = (args[i][1] == '>');
        c.out_path = expand_tilde(args[i+1]);
        i += 2;
        continue;
      } else {
        if (argcnt + 1 >= argv_cap) {
          argv_cap *= 2;
          char **na = realloc(c.argv, argv_cap * sizeof(char*));
          if (!na) die("lsh: realloc");
          c.argv = na;
        }
        c.argv[argcnt++] = args[i];
      }
    }
    c.argv[argcnt] = NULL;
    if (!c.argv[0]) { fprintf(stderr, "lsh: empty command in pipeline\n"); goto fail; }
    cmds[ncmd++] = c;
  }
  *out_cmds = cmds;
  return ncmd;

fail:
  for (int k = 0; k < ncmd; k++) free(cmds[k].argv);
  free(cmds);
  return -1;
}

static void free_pipeline(LshCmd *cmds, int ncmd) {
  for (int i = 0; i < ncmd; i++) {
    free(cmds[i].argv);
    if (cmds[i].in_path  && cmds[i].in_path[0]  == '~') free(cmds[i].in_path);
    if (cmds[i].out_path && cmds[i].out_path[0] == '~') free(cmds[i].out_path);
  }
  free(cmds);
}

/* ========================= Command launching ============================== */

static int lsh_launch_pipeline(char **args) {
  int last = 0;
  while (args[last]) last++;
  int background = 0;
  if (last > 0 && strcmp(args[last - 1], "&") == 0) {
    background = 1;
    args[last - 1] = NULL;
  }

  LshCmd *cmds = NULL;
  int ncmd = parse_pipeline(args, &cmds);
  if (ncmd <= 0) return 1;

  int pipes_needed = ncmd - 1;
  int pfds[2 * (pipes_needed > 0 ? pipes_needed : 1)];
  for (int i = 0; i < pipes_needed; i++) {
    if (pipe(pfds + 2*i) == -1) { perror("lsh: pipe"); free_pipeline(cmds, ncmd); return 1; }
  }

  pid_t *pids = malloc(ncmd * sizeof(pid_t));
  if (!pids) { perror("lsh: malloc"); free_pipeline(cmds, ncmd); return 1; }
  int *pidfds = calloc(ncmd, sizeof(int));
  if (!pidfds) { perror("lsh: calloc"); free(pids); free_pipeline(cmds, ncmd); return 1; }

  pid_t pgid = 0;

  for (int i = 0; i < ncmd; i++) {
    pid_t pid = fork();
    if (pid < 0) { perror("lsh: fork"); continue; }
    if (pid == 0) {
      /* Child: join/create process group */
      if (i == 0) {
        if (setpgid(0, 0) == -1 && errno != EACCES) perror("lsh: setpgid(child,self)");
      } else {
        if (setpgid(0, pgid) == -1 && errno != EACCES) perror("lsh: setpgid(child,pgid)");
      }

      /* Pipe endpoints */
      if (i > 0) {
        if (dup2(pfds[2*(i-1)], STDIN_FILENO) == -1) { perror("lsh: dup2 in"); _exit(126); }
      }
      if (i < ncmd - 1) {
        if (dup2(pfds[2*i + 1], STDOUT_FILENO) == -1) { perror("lsh: dup2 out"); _exit(126); }
      }
      for (int k = 0; k < 2*pipes_needed; k++) close(pfds[k]);

      /* Redirections */
      if (cmds[i].in_path) {
        int fd = open(cmds[i].in_path, O_RDONLY);
        if (fd == -1 || dup2(fd, STDIN_FILENO) == -1) { perror("lsh: input redir"); _exit(126); }
        close(fd);
      }
      if (cmds[i].out_path) {
        int flags = O_WRONLY | O_CREAT | (cmds[i].append ? O_APPEND : O_TRUNC);
        int fd = open(cmds[i].out_path, flags, 0644);
        if (fd == -1 || dup2(fd, STDOUT_FILENO) == -1) { perror("lsh: output redir"); _exit(126); }
        close(fd);
      }

      /* Reset SIGINT in child so Ctrl-C affects it (not the shell) */
      struct sigaction dfl;
      memset(&dfl, 0, sizeof(dfl));
      dfl.sa_handler = SIG_DFL;
      sigemptyset(&dfl.sa_mask);
      sigaction(SIGINT, &dfl, NULL);

      execvp(cmds[i].argv[0], cmds[i].argv);
      perror("lsh");
      _exit(127);
    }
    pids[i] = pid;

    /* Parent: establish process group */
    if (i == 0) {
      pgid = pid;
      if (setpgid(pid, pgid) == -1 && errno != EACCES) perror("lsh: setpgid(parent,leader)");
    } else {
      if (setpgid(pid, pgid) == -1 && errno != EACCES) perror("lsh: setpgid(parent,member)");
    }

    /* pidfd */
    int pfd = lsh_pidfd_open(pid, 0);
    if (pfd == -1) perror("lsh: pidfd_open");
    pidfds[i] = pfd;
    if (background && pfd >= 0 && g_epfd >= 0) {
      struct epoll_event ev = { .events = EPOLLIN, .data.fd = pfd };
      if (epoll_ctl(g_epfd, EPOLL_CTL_ADD, pfd, &ev) == -1) perror("lsh: epoll_ctl(pidfd bg)");
    }
  }

  for (int k = 0; k < 2*pipes_needed; k++) close(pfds[k]);

  if (background) {
    fprintf(stderr, "[bg] started pipeline (pids:");
    for (int i = 0; i < ncmd; i++) fprintf(stderr, " %d", (int)pids[i]);
    fprintf(stderr, " )\n");
  } else {
    /* Foreground: give tty to job PGID, block SIGCHLD, wait each via pidfd, restore tty. */
    if (pgid > 0) lsh_tcsetpgrp(g_tty_fd, pgid);

    sigset_t oldmask;
    lsh_block_sigchld(&oldmask);

    for (int i = 0; i < ncmd; i++) {
      if (pidfds[i] >= 0) {
        siginfo_t si = {0};
        while (waitid(P_PIDFD, pidfds[i], &si, WEXITED) == -1) {
          if (errno == EINTR) continue;
          perror("lsh: waitid(P_PIDFD) pipeline");
          break;
        }
        close(pidfds[i]);
      } else {
        int st;
        while (waitpid(pids[i], &st, 0) == -1 && errno == EINTR) {}
      }
    }

    if (g_shell_pgid > 0) lsh_tcsetpgrp(g_tty_fd, g_shell_pgid);
    lsh_restore_sigmask(&oldmask);
    lsh_reap_nonblocking();
  }

  free(pids);
  free(pidfds);
  free_pipeline(cmds, ncmd);
  return 1;
}

static int lsh_launch(char **args) {
  pid_t pid;
  int status;

  if (has_symbol(args, "|") || has_symbol(args, "<") || has_symbol(args, ">") || has_symbol(args, ">>")) {
    return lsh_launch_pipeline(args);
  }

  int last = 0;
  while (args[last]) last++;
  int background = 0;
  if (last > 0 && strcmp(args[last - 1], "&") == 0) {
    background = 1;
    args[last - 1] = NULL;
    if (!args[0]) return 1;
  }

  pid = fork();
  if (pid == 0) {
    /* Child process: its own group if foreground; bg vs fg decided in parent. */
    struct sigaction dfl;
    memset(&dfl, 0, sizeof(dfl));
    dfl.sa_handler = SIG_DFL;
    sigemptyset(&dfl.sa_mask);
    sigaction(SIGINT, &dfl, NULL);

    execvp(args[0], args);
    perror("lsh");
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    perror("lsh");
  } else {
    if (background) {
      int pidfd = lsh_pidfd_open(pid, 0);
      if (pidfd == -1) perror("lsh: pidfd_open(bg)");
      else if (g_epfd >= 0) {
        struct epoll_event ev = { .events = EPOLLIN, .data.fd = pidfd };
        if (epoll_ctl(g_epfd, EPOLL_CTL_ADD, pidfd, &ev) == -1) perror("lsh: epoll_ctl(add pidfd bg)");
      }
      fprintf(stderr, "[bg] %d\n", (int)pid);
    } else {
      /* Foreground single cmd: own PGID, give tty, block SIGCHLD, wait pidfd, restore tty. */
      if (setpgid(pid, pid) == -1 && errno != EACCES) perror("lsh: setpgid(parent, fg single)");
      lsh_tcsetpgrp(g_tty_fd, pid);

      sigset_t oldmask;
      lsh_block_sigchld(&oldmask);

      int pidfd = lsh_pidfd_open(pid, 0);
      if (pidfd >= 0) {
        siginfo_t si = {0};
        while (waitid(P_PIDFD, pidfd, &si, WEXITED) == -1) {
          if (errno == EINTR) continue;
          perror("lsh: waitid(P_PIDFD fg)");
          break;
        }
        close(pidfd);
      } else {
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
      }

      if (g_shell_pgid > 0) lsh_tcsetpgrp(g_tty_fd, g_shell_pgid);
      lsh_restore_sigmask(&oldmask);
      lsh_reap_nonblocking();
    }
  }

  return 1;
}

/* ======================== Execute / REPL Loop ============================= */

static int lsh_execute(char **args) {
  if (args[0] == NULL) return 1;

  for (int i = 0; i < lsh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }
  return lsh_launch(args);
}

static void lsh_loop(void) {
  char *line;
  char **args;
  int status;

  do {
    printf("> ");
    fflush(stdout);
    line = lsh_read_line();
    args = lsh_split_line(line);
    status = lsh_execute(args);

    free(line);
    free(args);
    /* Drain any ready pidfds (background completions). */
    lsh_reap_nonblocking();
  } while (status);
}

/* =============================== Builtins ================================= */

int lsh_cd(char **args) {
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) perror("lsh");
  }
  return 1;
}

int lsh_help(char **args) {
  (void)args;
  printf("LSH — simple shell\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("Builtins:\n");
  for (int i = 0; i < lsh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }
  printf("Redirection: <, >, >>  |  Background: &  |  Pipelines: cmd1 | cmd2\n");
  return 1;
}

int lsh_exit(char **args) {
  (void)args;
  return 0;
}

/* ================================= Main =================================== */

int main(int argc, char **argv) {
  (void)argc; (void)argv;

  /* epoll for pidfds (optional but preferred) */
  g_epfd = epoll_create1(EPOLL_CLOEXEC);
  if (g_epfd == -1) perror("lsh: epoll_create1");

  /* Job control bootstrap */
  g_tty_fd = STDIN_FILENO;
  signal(SIGTTOU, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  g_shell_pgid = getpgrp();
  if (g_shell_pgid <= 0) g_shell_pgid = getpid();
  pid_t cur = -1;
  if ((cur = tcgetpgrp(g_tty_fd)) != -1 && cur != g_shell_pgid) {
    lsh_tcsetpgrp(g_tty_fd, g_shell_pgid);
  }

  lsh_loop();
  return EXIT_SUCCESS;
}