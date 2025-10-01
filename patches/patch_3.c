@@
 #include <unistd.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>
 #include <signal.h>
 #include <errno.h>
 #include <fcntl.h>
+#include <pthread.h>
 
@@
 int lsh_exit(char **args)
 {
   return 0;
 }
 
-static void lsh_sigchld_handler(int sig) {
+/* -------------------- SIGCHLD handler (minimal, async-signal-safe) -------------------- */
+static void lsh_sigchld_handler(int sig) {
   (void)sig;
   int saved = errno;        /* preserve errno */
-  int status;
-  pid_t pid;
-  /* Reap all dead children without blocking. No stdio here! */
-  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
-        /* Avoid printf/malloc here. If you want notifications, buffer PIDs and print in main loop. */
+  int st;
+  pid_t pid;
+  /* Drain all status changes without blocking; no stdio/malloc here. */
+  while ((pid = waitpid(-1, &st, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
+    /* Intentionally empty: just reaping / consuming state in handler.
+       If you want notifications, use a self-pipe or set a flag. */
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
+/* -------------------- Foreground wait race control + drain helpers -------------------- */
+static void lsh_block_sigchld(sigset_t *oldmask) {
+  sigset_t set;
+  sigemptyset(&set);
+  sigaddset(&set, SIGCHLD);
+  if (pthread_sigmask(SIG_BLOCK, &set, oldmask) != 0) {
+    perror("lsh: pthread_sigmask(SIG_BLOCK)");
+    exit(EXIT_FAILURE);
+  }
+}
+
+static void lsh_restore_sigmask(const sigset_t *oldmask) {
+  if (pthread_sigmask(SIG_SETMASK, oldmask, NULL) != 0) {
+    perror("lsh: pthread_sigmask(SIG_SETMASK)");
+    exit(EXIT_FAILURE);
+  }
+}
+
+/* Non-blocking drain of all pending child state changes. Safe to use outside handler. */
+static void lsh_reap_nonblocking(void) {
+  int st;
+  for (;;) {
+    pid_t pid = waitpid(-1, &st, WNOHANG | WUNTRACED | WCONTINUED);
+    if (pid > 0) {
+      /* Decode *after* checking WIF*: WEXITSTATUS is only valid when WIFEXITED. */
+      if (WIFEXITED(st)) {
+        /* int code = WEXITSTATUS(st); */
+        /* You can log/update job table here if you keep one. */
+      } else if (WIFSIGNALED(st)) {
+        /* int sig = WTERMSIG(st); int dumped = WCOREDUMP(st); */
+      } else if (WIFSTOPPED(st)) {
+        /* int sig = WSTOPSIG(st); */
+      } else if (WIFCONTINUED(st)) {
+        /* continued */
+      }
+      continue;
+    } else if (pid == 0) {
+      break;                 /* nothing more right now */
+    } else { /* pid < 0 */
+      if (errno == EINTR) continue;
+      if (errno == ECHILD) break; /* no children: normal */
+      perror("lsh: waitpid (drain)");
+      break;
+    }
+  }
+}
 
 /* -------------------- Helpers for pipelines / redirection -------------------- */
 typedef struct {
   char **argv;    /* NULL-terminated */
   char *in_path;  /* or NULL */
@@
 static int lsh_launch_pipeline(char **args) {
@@
-  if (background) {
+  if (background) {
     /* Let SIGCHLD handler reap them. */
     fprintf(stderr, "[bg] started pipeline (pids:");
     for (int i = 0; i < ncmd; i++) fprintf(stderr, " %d", (int)pids[i]);
     fprintf(stderr, " )\n");
   } else {
-    int status;
-    /* Wait for the last stage; optionally wait all to avoid zombies if some fail fast */
-    for (int i = 0; i < ncmd; i++) {
-      for (;;) {
-        pid_t w = waitpid(pids[i], &status, 0);
-        if (w == -1) {
-          if (errno == EINTR) continue;
-          perror("lsh: waitpid");
-          break;
-        }
-        if (WIFEXITED(status) || WIFSIGNALED(status)) break;
-      }
-    }
+    /* Foreground path: block SIGCHLD to avoid “double collection” races
+       with the handler/other threads while we synchronously wait. */
+    sigset_t oldmask;
+    lsh_block_sigchld(&oldmask);
+
+    int st;
+    for (int i = 0; i < ncmd; i++) {
+      for (;;) {
+        pid_t w = waitpid(pids[i], &st, 0); /* blocking wait; EINTR-aware */
+        if (w == -1) {
+          if (errno == EINTR) continue;
+          if (errno == ECHILD) break; /* someone else reaped; tolerate */
+          perror("lsh: waitpid (pipeline fg)");
+          break;
+        }
+        if (WIFEXITED(st) || WIFSIGNALED(st)) break;
+      }
+    }
+
+    /* Restore mask and drain any pending async events that accumulated
+       while SIGCHLD was blocked. */
+    lsh_restore_sigmask(&oldmask);
+    lsh_reap_nonblocking();
   }
@@
   free(pids);
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
       /* Register as a background job; don't wait (reaped by SIGCHLD) */
       pid_t one = pid;
       int jobid = lsh_add_job(pid, &one, 1, args[0], 1);
       fprintf(stderr, "[%d] %d\n", jobid, (int)pid);
     } else {
-      /* Foreground: wait robustly (no WNOHANG) and handle EINTR */
-      for (;;) {
-        pid_t w = waitpid(pid, &status, 0);
-        if (w == -1) {
-          if (errno == EINTR) continue;     /* interrupted by signal, retry */
-          perror("lsh: waitpid");
-          break;
-        }
-        if (WIFEXITED(status) || WIFSIGNALED(status)) break;
-      }
+      /* Foreground: block SIGCHLD to avoid a concurrent reaper stealing
+         this child's status; then do a blocking wait; then drain. */
+      sigset_t oldmask;
+      lsh_block_sigchld(&oldmask);
+      for (;;) {
+        pid_t w = waitpid(pid, &status, 0);
+        if (w == -1) {
+          if (errno == EINTR) continue;     /* interrupted by signal, retry */
+          if (errno == ECHILD) break;       /* already reaped; tolerate */
+          perror("lsh: waitpid (fg)");
+          break;
+        }
+        if (WIFEXITED(status) || WIFSIGNALED(status)) break;
+      }
+      lsh_restore_sigmask(&oldmask);
+      lsh_reap_nonblocking();
       /* prune if any job entry exists (not typical for fg single) */
     }
   }
 
   return 1;
 }
@@
 int main(int argc, char **argv)
 {
   /* Install SIGCHLD reaper so background children/pipelines don't zombie */
   lsh_install_sigchld();
   // Run command loop.
   lsh_loop();
 
   return EXIT_SUCCESS;
 }