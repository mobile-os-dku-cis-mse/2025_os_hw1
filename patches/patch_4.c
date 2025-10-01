@@
 #include <string.h>
 #include <signal.h>
 #include <errno.h>
 #include <fcntl.h>
-#include <pthread.h>
+#include <pthread.h>
+#include <sys/epoll.h>
+#include <sys/syscall.h>
 
@@
-int lsh_exit(char **args)
+int lsh_exit(char **args)
 {
   return 0;
 }
 
-/* -------------------- SIGCHLD handler (minimal, async-signal-safe) -------------------- */
-static void lsh_sigchld_handler(int sig) {
-  (void)sig;
-  int saved = errno;        /* preserve errno */
-  int st;
-  pid_t pid;
-  /* Drain all status changes without blocking; no stdio/malloc here. */
-  while ((pid = waitpid(-1, &st, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
-    /* Intentionally empty: just reaping / consuming state in handler.
-       If you want notifications, use a self-pipe or set a flag. */
-  }
-  errno = saved;
-}
-
-static void lsh_install_sigchld(void) {
-  struct sigaction sa;
-  memset(&sa, 0, sizeof(sa));
-  sa.sa_handler = lsh_sigchld_handler;
-  sigemptyset(&sa.sa_mask);
-  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; /* restart slow syscalls; ignore stop/cont notifications */
-  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
-    perror("lsh: sigaction(SIGCHLD)");
-    exit(EXIT_FAILURE);
-  }
-}
+/* ==================== pidfd + epoll based child management ==================== */
+static int g_epfd = -1;
+
+/* Fallback-friendly pidfd_open wrapper (works on modern Linux kernels). */
+static int lsh_pidfd_open(pid_t pid, unsigned int flags) {
+#ifdef SYS_pidfd_open
+  return (int)syscall(SYS_pidfd_open, pid, flags);
+#else
+  errno = ENOSYS;
+  return -1;
+#endif
+}
+
+/* Best-effort nonblocking consume for a single pidfd (use WNOWAIT to inspect,
+   then final reap without WNOWAIT when exited/killed). */
+static void lsh_consume_pidfd(int pidfd) {
+  siginfo_t si = {0};
+  /* Inspect current state without reaping to avoid accidental double free. */
+  if (waitid(P_PIDFD, pidfd, &si, WEXITED|WSTOPPED|WCONTINUED|WNOWAIT) == -1) {
+    if (errno != EAGAIN && errno != EINTR) perror("lsh: waitid(P_PIDFD, WNOWAIT)");
+    return;
+  }
+  /* Interested only in exit/kill for final reap; STOP/CONT are observable, but we
+     leave them to higher-level job table if added later. */
+  if (si.si_code == CLD_EXITED || si.si_code == CLD_KILLED || si.si_code == CLD_DUMPED) {
+    /* Final reap (no WNOWAIT). */
+    siginfo_t fin = {0};
+    if (waitid(P_PIDFD, pidfd, &fin, WEXITED) == -1) {
+      if (errno != EAGAIN && errno != EINTR) perror("lsh: waitid(P_PIDFD, reap)");
+    }
+    /* pidfd is now useless; close it. */
+    close(pidfd);
+  }
+}
+
+/* Drain any ready pidfds once; call this in places you previously used a drain loop. */
+static void lsh_epoll_drain_ready(void) {
+  if (g_epfd < 0) return;
+  struct epoll_event evs[32];
+  int n = epoll_wait(g_epfd, evs, (int)(sizeof(evs)/sizeof(evs[0])), 0);
+  if (n < 0) {
+    if (errno != EINTR) perror("lsh: epoll_wait");
+    return;
+  }
+  for (int i = 0; i < n; i++) {
+    if (evs[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR)) {
+      lsh_consume_pidfd(evs[i].data.fd);
+    }
+  }
+}
 
 /* -------------------- Foreground wait race control + drain helpers -------------------- */
 static void lsh_block_sigchld(sigset_t *oldmask) {
   sigset_t set;
   sigemptyset(&set);
   sigaddset(&set, SIGCHLD);
   if (pthread_sigmask(SIG_BLOCK, &set, oldmask) != 0) {
     perror("lsh: pthread_sigmask(SIG_BLOCK)");
     exit(EXIT_FAILURE);
   }
 }
 
 static void lsh_restore_sigmask(const sigset_t *oldmask) {
   if (pthread_sigmask(SIG_SETMASK, oldmask, NULL) != 0) {
     perror("lsh: pthread_sigmask(SIG_SETMASK)");
     exit(EXIT_FAILURE);
   }
 }
 
-/* Non-blocking drain of all pending child state changes. Safe to use outside handler. */
-static void lsh_reap_nonblocking(void) {
-  int st;
-  for (;;) {
-    pid_t pid = waitpid(-1, &st, WNOHANG | WUNTRACED | WCONTINUED);
-    if (pid > 0) {
-      /* Decode *after* checking WIF*: WEXITSTATUS is only valid when WIFEXITED. */
-      if (WIFEXITED(st)) {
-        /* int code = WEXITSTATUS(st); */
-        /* You can log/update job table here if you keep one. */
-      } else if (WIFSIGNALED(st)) {
-        /* int sig = WTERMSIG(st); int dumped = WCOREDUMP(st); */
-      } else if (WIFSTOPPED(st)) {
-        /* int sig = WSTOPSIG(st); */
-      } else if (WIFCONTINUED(st)) {
-        /* continued */
-      }
-      continue;
-    } else if (pid == 0) {
-      break;                 /* nothing more right now */
-    } else { /* pid < 0 */
-      if (errno == EINTR) continue;
-      if (errno == ECHILD) break; /* no children: normal */
-      perror("lsh: waitpid (drain)");
-      break;
-    }
-  }
-}
+/* With pidfds + epoll we don't need a generic waitpid drain. Keep a no-op wrapper
+   name so call sites remain simple. */
+#define lsh_reap_nonblocking() lsh_epoll_drain_ready()
 
 /* -------------------- Helpers for pipelines / redirection -------------------- */
 typedef struct {
   char **argv;    /* NULL-terminated */
   char *in_path;  /* or NULL */
@@
 static int lsh_launch_pipeline(char **args) {
@@
-  pid_t *pids = malloc(ncmd * sizeof(pid_t));
+  pid_t *pids = malloc(ncmd * sizeof(pid_t));
   if (!pids) { perror("lsh: malloc"); free_pipeline(cmds, ncmd); return 1; }
+  int   *pidfds = calloc(ncmd, sizeof(int));
+  if (!pidfds) { perror("lsh: calloc"); free(pids); free_pipeline(cmds, ncmd); return 1; }
 
   for (int i = 0; i < ncmd; i++) {
     pid_t pid = fork();
     if (pid < 0) { perror("lsh: fork"); /* continue to try spawning others? */ continue; }
     if (pid == 0) {
@@
       _exit(127);
     }
-    pids[i] = pid;
+    pids[i] = pid;
+    /* Parent: create pidfd and (for bg) arm epoll */
+    int pfd = lsh_pidfd_open(pid, 0);
+    if (pfd == -1) { perror("lsh: pidfd_open"); /* fallback: could still use waitpid */ }
+    pidfds[i] = pfd;
+    if (background && pfd >= 0 && g_epfd >= 0) {
+      struct epoll_event ev = { .events = EPOLLIN, .data.fd = pfd };
+      if (epoll_ctl(g_epfd, EPOLL_CTL_ADD, pfd, &ev) == -1) perror("lsh: epoll_ctl(add pidfd)");
+    }
   }
 
   /* Parent: close pipe fds */
   for (int k = 0; k < 2*pipes_needed; k++) close(pfds[k]);
 
   if (background) {
     /* Let epoll + pidfd handle reaping. */
     fprintf(stderr, "[bg] started pipeline (pids:");
     for (int i = 0; i < ncmd; i++) fprintf(stderr, " %d", (int)pids[i]);
     fprintf(stderr, " )\n");
   } else {
-    /* Foreground path: block SIGCHLD to avoid “double collection” races
-       with the handler/other threads while we synchronously wait. */
+    /* Foreground path: block SIGCHLD (still good hygiene) and wait each by pidfd. */
     sigset_t oldmask;
     lsh_block_sigchld(&oldmask);
 
-    int st;
     for (int i = 0; i < ncmd; i++) {
-      for (;;) {
-        pid_t w = waitpid(pids[i], &st, 0); /* blocking wait; EINTR-aware */
-        if (w == -1) {
-          if (errno == EINTR) continue;
-          if (errno == ECHILD) break; /* someone else reaped; tolerate */
-          perror("lsh: waitpid (pipeline fg)");
-          break;
-        }
-        if (WIFEXITED(st) || WIFSIGNALED(st)) break;
-      }
+      if (pidfds[i] >= 0) {
+        siginfo_t si = {0};
+        /* Blocking wait for exactly this child via its pidfd. */
+        while (waitid(P_PIDFD, pidfds[i], &si, WEXITED) == -1) {
+          if (errno == EINTR) continue;
+          perror("lsh: waitid(P_PIDFD) pipeline");
+          break;
+        }
+        close(pidfds[i]); /* done with this pidfd */
+      } else {
+        /* Fallback if pidfd_open failed: blocking waitpid as last resort. */
+        int st;
+        while (waitpid(pids[i], &st, 0) == -1 && errno == EINTR) {}
+      }
     }
 
-    /* Restore mask and drain any pending async events that accumulated
-       while SIGCHLD was blocked. */
+    /* Restore mask and drain ready pidfds. */
     lsh_restore_sigmask(&oldmask);
     lsh_reap_nonblocking();
   }
 
   free(pids);
+  free(pidfds);
   free_pipeline(cmds, ncmd);
   return 1;
 }
 
@@
 int lsh_launch(char **args)
 {
   pid_t pid;
   int status;
 
   /* If pipeline/redirection tokens exist, delegate */
   if (has_symbol(args, "|") || has_symbol(args, "<") || has_symbol(args, ">") || has_symbol(args, ">>")) {
     return lsh_launch_pipeline(args);
   }
@@
   } else {
     // Parent process
     if (background) {
-      /* Register as a background job; don't wait (reaped by SIGCHLD) */
-      pid_t one = pid;
-      int jobid = lsh_add_job(pid, &one, 1, args[0], 1);
-      fprintf(stderr, "[%d] %d\n", jobid, (int)pid);
+      /* Background: track with pidfd via epoll instead of SIGCHLD. */
+      int pidfd = lsh_pidfd_open(pid, 0);
+      if (pidfd == -1) {
+        perror("lsh: pidfd_open(bg)");
+      } else if (g_epfd >= 0) {
+        struct epoll_event ev = { .events = EPOLLIN, .data.fd = pidfd };
+        if (epoll_ctl(g_epfd, EPOLL_CTL_ADD, pidfd, &ev) == -1) perror("lsh: epoll_ctl(add pidfd bg)");
+      }
+      /* Optional: job table printout could stay the same if you keep it. */
+      fprintf(stderr, "[bg] %d\n", (int)pid);
     } else {
-      /* Foreground: block SIGCHLD to avoid a concurrent reaper stealing
-         this child's status; then do a blocking wait; then drain. */
+      /* Foreground: block SIGCHLD (defensive) and wait via pidfd. */
       sigset_t oldmask;
       lsh_block_sigchld(&oldmask);
-      for (;;) {
-        pid_t w = waitpid(pid, &status, 0);
-        if (w == -1) {
-          if (errno == EINTR) continue;     /* interrupted by signal, retry */
-          if (errno == ECHILD) break;       /* already reaped; tolerate */
-          perror("lsh: waitpid (fg)");
-          break;
-        }
-        if (WIFEXITED(status) || WIFSIGNALED(status)) break;
-      }
+      int pidfd = lsh_pidfd_open(pid, 0);
+      if (pidfd >= 0) {
+        siginfo_t si = {0};
+        while (waitid(P_PIDFD, pidfd, &si, WEXITED) == -1) {
+          if (errno == EINTR) continue;
+          perror("lsh: waitid(P_PIDFD fg)");
+          break;
+        }
+        close(pidfd);
+      } else {
+        /* Fallback if pidfd_open is unavailable. */
+        while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
+      }
       lsh_restore_sigmask(&oldmask);
       lsh_reap_nonblocking();
       /* prune if any job entry exists (not typical for fg single) */
     }
   }
 
   return 1;
 }
@@
 int main(int argc, char **argv)
 {
-  /* Install SIGCHLD reaper so background children/pipelines don't zombie */
-  lsh_install_sigchld();
+  /* Create epoll instance for pidfds (modern Linux only). */
+  g_epfd = epoll_create1(EPOLL_CLOEXEC);
+  if (g_epfd == -1) {
+    /* Not fatal; we will still function with blocking waits and no bg epoll. */
+    perror("lsh: epoll_create1");
+  }
   // Run command loop.
   lsh_loop();
 
   return EXIT_SUCCESS;
 }
