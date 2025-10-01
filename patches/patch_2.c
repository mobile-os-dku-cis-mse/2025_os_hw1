@@
 #include <unistd.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>
+#include <signal.h>
+#include <errno.h>
+#include <fcntl.h>
 
@@
 int lsh_exit(char **args)
 {
   return 0;
 }
 
 static void lsh_sigchld_handler(int sig) {
   (void)sig;
   int saved = errno;        /* preserve errno */
   int status;
   pid_t pid;
   /* Reap all dead children without blocking. No stdio here! */
   while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
-        printf("[shell] background process %d finished with status %d\n",
-               pid, WEXITSTATUS(status));
+        /* Avoid printf/malloc here. If you want notifications, buffer PIDs and print in main loop. */
   }
   errno = saved;
 }
 
static void lsh_install_sigchld(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = lsh_sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; /* restart slow syscalls; ignore stop/cont notifications */
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("lsh: sigaction(SIGCHLD)");
    exit(EXIT_FAILURE);
  }
}
+
+/* -------------------- Helpers for pipelines / redirection -------------------- */
+typedef struct {
+  char **argv;    /* NULL-terminated */
+  char *in_path;  /* or NULL */
+  char *out_path; /* or NULL */
+  int   append;   /* 0 => truncate, 1 => append */
+} LshCmd;
+
+static char *expand_tilde(const char *p) {
+  if (!p || p[0] != '~') return (char*)p;
+  const char *home = getenv("HOME");
+  if (!home) return (char*)p;
+  size_t hl = strlen(home), pl = strlen(p);
+  char *r = malloc(hl + (p[1] == '/' ? pl : pl + 1)); /* account for join */
+  if (!r) { perror("lsh: malloc"); exit(EXIT_FAILURE); }
+  if (p[1] == '\0') { /* "~" -> $HOME */
+    strcpy(r, home);
+  } else if (p[1] == '/') { /* "~/x" -> $HOME/x */
+    strcpy(r, home);
+    strcat(r, p + 1);
+  } else {
+    /* "~user" not supported (keep as-is) */
+    free(r);
+    return (char*)p;
+  }
+  return r;
+}
+
+static int has_symbol(char **args, const char *sym) {
+  for (int i = 0; args && args[i]; i++) if (strcmp(args[i], sym) == 0) return 1;
+  return 0;
+}
+
+/* Parse tokens into a sequence of pipeline stages with optional redirections. */
+static int parse_pipeline(char **args, LshCmd **out_cmds) {
+  int cap = 4, ncmd = 0;
+  LshCmd *cmds = calloc(cap, sizeof(LshCmd));
+  if (!cmds) { perror("lsh: calloc"); exit(EXIT_FAILURE); }
+
+  /* We will reuse the same token array, inserting NULLs to terminate argv segments. */
+  int i = 0;
+  while (args[i]) {
+    if (ncmd == cap) {
+      cap *= 2;
+      LshCmd *nc = realloc(cmds, cap * sizeof(LshCmd));
+      if (!nc) { perror("lsh: realloc"); exit(EXIT_FAILURE); }
+      cmds = nc;
+    }
+    /* start a new stage */
+    LshCmd c = {0};
+    int argv_cap = 8, argcnt = 0;
+    c.argv = malloc(argv_cap * sizeof(char*));
+    if (!c.argv) { perror("lsh: malloc"); exit(EXIT_FAILURE); }
+
+    for (; args[i]; i++) {
+      if (strcmp(args[i], "|") == 0) { /* end of this stage */
+        i++; /* consume '|' */
+        break;
+      } else if (strcmp(args[i], "<") == 0) {
+        if (!args[i+1]) { fprintf(stderr, "lsh: syntax error near '<'\n"); goto fail; }
+        c.in_path = expand_tilde(args[i+1]);
+        i += 2;
+        continue;
+      } else if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
+        if (!args[i+1]) { fprintf(stderr, "lsh: syntax error near '>'\n"); goto fail; }
+        c.append = (args[i][1] == '>');
+        c.out_path = expand_tilde(args[i+1]);
+        i += 2;
+        continue;
+      } else {
+        if (argcnt + 1 >= argv_cap) {
+          argv_cap *= 2;
+          char **na = realloc(c.argv, argv_cap * sizeof(char*));
+          if (!na) { perror("lsh: realloc"); exit(EXIT_FAILURE); }
+          c.argv = na;
+        }
+        c.argv[argcnt++] = args[i];
+      }
+    }
+    c.argv[argcnt] = NULL;
+    if (!c.argv[0]) {
+      fprintf(stderr, "lsh: empty command in pipeline\n");
+      goto fail;
+    }
+    cmds[ncmd++] = c;
+  }
+  *out_cmds = cmds;
+  return ncmd;
+
+fail:
+  for (int k = 0; k < ncmd; k++) free(cmds[k].argv);
+  free(cmds);
+  return -1;
+}
+
+static void free_pipeline(LshCmd *cmds, int ncmd) {
+  for (int i = 0; i < ncmd; i++) {
+    /* argv elements point into the original line buffer; do not free them. */
+    free(cmds[i].argv);
+    /* in_path/out_path may be expanded copies or original tokens.
+       We only free if they started with '~' and we expanded them. */
+    /* Simple heuristic: if it starts with '/', don't free; if it starts with '~', free. */
+    if (cmds[i].in_path  && cmds[i].in_path[0]  == '~') free(cmds[i].in_path);
+    if (cmds[i].out_path && cmds[i].out_path[0] == '~') free(cmds[i].out_path);
+  }
+  free(cmds);
+}
+
+/* Execute a pipeline (ncmd >= 1). Honors redirections on any stage. */
+static int lsh_launch_pipeline(char **args) {
+  /* Background: trailing '&' after the last command */
+  int last = 0;
+  while (args[last]) last++;
+  int background = 0;
+  if (last > 0 && strcmp(args[last - 1], "&") == 0) {
+    background = 1;
+    args[last - 1] = NULL;
+  }
+
+  LshCmd *cmds = NULL;
+  int ncmd = parse_pipeline(args, &cmds);
+  if (ncmd <= 0) return 1;
+
+  int pipes_needed = ncmd - 1;
+  int pfds[2 * (pipes_needed > 0 ? pipes_needed : 1)];
+  for (int i = 0; i < pipes_needed; i++) {
+    if (pipe(pfds + 2*i) == -1) { perror("lsh: pipe"); free_pipeline(cmds, ncmd); return 1; }
+  }
+
+  pid_t *pids = malloc(ncmd * sizeof(pid_t));
+  if (!pids) { perror("lsh: malloc"); free_pipeline(cmds, ncmd); return 1; }
+
+  for (int i = 0; i < ncmd; i++) {
+    pid_t pid = fork();
+    if (pid < 0) { perror("lsh: fork"); /* continue to try spawning others? */ continue; }
+    if (pid == 0) {
+      /* Child: set up stdin/stdout from pipes */
+      if (i > 0) {
+        if (dup2(pfds[2*(i-1)], STDIN_FILENO) == -1) { perror("lsh: dup2 in"); _exit(126); }
+      }
+      if (i < ncmd - 1) {
+        if (dup2(pfds[2*i + 1], STDOUT_FILENO) == -1) { perror("lsh: dup2 out"); _exit(126); }
+      }
+      /* Close all pipe fds in child */
+      for (int k = 0; k < 2*pipes_needed; k++) close(pfds[k]);
+      /* Redirections */
+      if (cmds[i].in_path) {
+        int fd = open(cmds[i].in_path, O_RDONLY);
+        if (fd == -1 || dup2(fd, STDIN_FILENO) == -1) { perror("lsh: input redir"); _exit(126); }
+        close(fd);
+      }
+      if (cmds[i].out_path) {
+        int flags = O_WRONLY | O_CREAT | (cmds[i].append ? O_APPEND : O_TRUNC);
+        int fd = open(cmds[i].out_path, flags, 0644);
+        if (fd == -1 || dup2(fd, STDOUT_FILENO) == -1) { perror("lsh: output redir"); _exit(126); }
+        close(fd);
+      }
+      /* Exec */
+      execvp(cmds[i].argv[0], cmds[i].argv);
+      perror("lsh");
+      _exit(127);
+    }
+    pids[i] = pid;
+  }
+
+  /* Parent: close pipe fds */
+  for (int k = 0; k < 2*pipes_needed; k++) close(pfds[k]);
+
+  if (background) {
+    /* Let SIGCHLD handler reap them. */
+    fprintf(stderr, "[bg] started pipeline (pids:");
+    for (int i = 0; i < ncmd; i++) fprintf(stderr, " %d", (int)pids[i]);
+    fprintf(stderr, " )\n");
+  } else {
+    int status;
+    /* Wait for the last stage; optionally wait all to avoid zombies if some fail fast */
+    for (int i = 0; i < ncmd; i++) {
+      for (;;) {
+        pid_t w = waitpid(pids[i], &status, 0);
+        if (w == -1) {
+          if (errno == EINTR) continue;
+          perror("lsh: waitpid");
+          break;
+        }
+        if (WIFEXITED(status) || WIFSIGNALED(status)) break;
+      }
+    }
+  }
+
+  free(pids);
+  free_pipeline(cmds, ncmd);
+  return 1;
+}
+
@@
 int lsh_launch(char **args)
 {
   pid_t pid;
   int status;
 
+  /* If pipeline/redirection tokens exist, delegate */
+  if (has_symbol(args, "|") || has_symbol(args, "<") || has_symbol(args, ">") || has_symbol(args, ">>")) {
+    return lsh_launch_pipeline(args);
+  }
+
   /* Detect background job: trailing "&" */
   int last = 0;
   while (args[last]) last++;
   int background = 0;
   if (last > 0 && strcmp(args[last - 1], "&") == 0) {
@@
 int main(int argc, char **argv)
 {
+  /* Install SIGCHLD reaper so background children/pipelines don't zombie */
+  lsh_install_sigchld();
   // Run command loop.
   lsh_loop();
 
   return EXIT_SUCCESS;
 }
