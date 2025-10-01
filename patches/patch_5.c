@@
 #include <errno.h>
 #include <fcntl.h>
 #include <pthread.h>
 #include <sys/epoll.h>
 #include <sys/syscall.h>
+#include <termios.h>
 
@@
 /* ==================== pidfd + epoll based child management ==================== */
 static int g_epfd = -1;
+static int g_tty_fd = 0;           /* controlling TTY (use stdin by default) */
+static pid_t g_shell_pgid = -1;    /* our shell's process group */
 
@@
 #define lsh_reap_nonblocking() lsh_epoll_drain_ready()
 
+/* ==================== Job control helpers (tcsetpgrp) ==================== */
+static void lsh_tcsetpgrp(int fd, pid_t pgid) {
+  if (fd < 0 || pgid <= 0) return;
+  if (tcsetpgrp(fd, pgid) == -1) {
+    /* Not fatal; errors can occur if the job has already exited. */
+    if (errno != ENOTTY && errno != ESRCH)
+      perror("lsh: tcsetpgrp");
+  }
+}
+
 /* -------------------- Helpers for pipelines / redirection -------------------- */
 typedef struct {
   char **argv;    /* NULL-terminated */
   char *in_path;  /* or NULL */
   char *out_path; /* or NULL */
@@
 static int lsh_launch_pipeline(char **args) {
@@
-  for (int i = 0; i < ncmd; i++) {
+  pid_t pgid = 0; /* pipeline process group leader (first child's pid) */
+  for (int i = 0; i < ncmd; i++) {
     pid_t pid = fork();
     if (pid < 0) { perror("lsh: fork"); /* continue to try spawning others? */ continue; }
     if (pid == 0) {
+      /* ---- Child: enter job process group ---- */
+      if (i == 0) {
+        /* Make self group leader: pgid == pid (0 means "self"). */
+        if (setpgid(0, 0) == -1 && errno != EACCES) { /* EACCES if already in group after exec race */
+          perror("lsh: setpgid(child, self)");
+        }
+      } else {
+        /* Join leader's pgid (set by parent). */
+        if (setpgid(0, pgid) == -1 && errno != EACCES) {
+          perror("lsh: setpgid(child, pgid)");
+        }
+      }
@@
       execvp(cmds[i].argv[0], cmds[i].argv);
       perror("lsh");
       _exit(127);
     }
     pids[i] = pid;
+    /* ---- Parent: establish/propagate the process group ---- */
+    if (i == 0) {
+      pgid = pid;
+      if (setpgid(pid, pgid) == -1 && errno != EACCES) {
+        perror("lsh: setpgid(parent, leader)");
+      }
+    } else {
+      if (setpgid(pid, pgid) == -1 && errno != EACCES) {
+        perror("lsh: setpgid(parent, member)");
+      }
+    }
@@
   if (background) {
     /* Let epoll + pidfd handle reaping. */
     fprintf(stderr, "[bg] started pipeline (pids:");
     for (int i = 0; i < ncmd; i++) fprintf(stderr, " %d", (int)pids[i]);
     fprintf(stderr, " )\n");
   } else {
-    /* Foreground path: block SIGCHLD (still good hygiene) and wait each by pidfd. */
+    /* Foreground path:
+       1) Give terminal to the job's PGID.
+       2) Block SIGCHLD and wait each stage by pidfd.
+       3) Restore terminal to shell PGID and drain. */
+    if (pgid > 0) lsh_tcsetpgrp(g_tty_fd, pgid);
     sigset_t oldmask;
     lsh_block_sigchld(&oldmask);
 
     for (int i = 0; i < ncmd; i++) {
       if (pidfds[i] >= 0) {
         siginfo_t si = {0};
         /* Blocking wait for exactly this child via its pidfd. */
         while (waitid(P_PIDFD, pidfds[i], &si, WEXITED) == -1) {
           if (errno == EINTR) continue;
           perror("lsh: waitid(P_PIDFD) pipeline");
           break;
         }
         close(pidfds[i]); /* done with this pidfd */
       } else {
         /* Fallback if pidfd_open failed: blocking waitpid as last resort. */
         int st;
         while (waitpid(pids[i], &st, 0) == -1 && errno == EINTR) {}
       }
     }
 
-    /* Restore mask and drain ready pidfds. */
+    /* Restore terminal foreground to the shell, then restore mask & drain. */
+    if (g_shell_pgid > 0) lsh_tcsetpgrp(g_tty_fd, g_shell_pgid);
     lsh_restore_sigmask(&oldmask);
     lsh_reap_nonblocking();
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
       /* Background: track with pidfd via epoll instead of SIGCHLD. */
       int pidfd = lsh_pidfd_open(pid, 0);
       if (pidfd == -1) {
         perror("lsh: pidfd_open(bg)");
       } else if (g_epfd >= 0) {
         struct epoll_event ev = { .events = EPOLLIN, .data.fd = pidfd };
         if (epoll_ctl(g_epfd, EPOLL_CTL_ADD, pidfd, &ev) == -1) perror("lsh: epoll_ctl(add pidfd bg)");
       }
       /* Optional: job table printout could stay the same if you keep it. */
       fprintf(stderr, "[bg] %d\n", (int)pid);
     } else {
-      /* Foreground: block SIGCHLD (defensive) and wait via pidfd. */
+      /* Foreground single command:
+         - Put child in its own process group (PGID = pid).
+         - Give terminal to child's PGID.
+         - Block SIGCHLD; wait via pidfd; restore terminal; drain. */
+      if (setpgid(pid, pid) == -1 && errno != EACCES) {
+        perror("lsh: setpgid(parent, fg single)");
+      }
+      lsh_tcsetpgrp(g_tty_fd, pid);
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
         /* Fallback if pidfd_open is unavailable. */
         while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
       }
-      lsh_restore_sigmask(&oldmask);
+      /* Restore terminal foreground to shell before unblocking signals. */
+      if (g_shell_pgid > 0) lsh_tcsetpgrp(g_tty_fd, g_shell_pgid);
+      lsh_restore_sigmask(&oldmask);
       lsh_reap_nonblocking();
       /* prune if any job entry exists (not typical for fg single) */
     }
   }
 
   return 1;
 }
@@
 int main(int argc, char **argv)
 {
   /* Create epoll instance for pidfds (modern Linux only). */
   g_epfd = epoll_create1(EPOLL_CLOEXEC);
   if (g_epfd == -1) {
     /* Not fatal; we will still function with blocking waits and no bg epoll. */
     perror("lsh: epoll_create1");
   }
+  /* ---- Job control bootstrap ---- */
+  g_tty_fd = STDIN_FILENO;
+  /* Ignore job-control stop-on-tty signals so shell isn't stopped by tcsetpgrp */
+  signal(SIGTTOU, SIG_IGN);
+  signal(SIGTTIN, SIG_IGN);
+  /* Record / ensure shell is foreground pgid for its TTY */
+  g_shell_pgid = getpgrp();
+  if (g_shell_pgid <= 0) g_shell_pgid = getpid();
+  /* If another pgid owns the tty, try to claim it for the shell */
+  pid_t cur = -1;
+  if ((cur = tcgetpgrp(g_tty_fd)) != -1 && cur != g_shell_pgid) {
+    lsh_tcsetpgrp(g_tty_fd, g_shell_pgid);
+  }
+
   // Run command loop.
   lsh_loop();
 
   return EXIT_SUCCESS;
 }
