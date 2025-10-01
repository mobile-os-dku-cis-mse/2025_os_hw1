@@
 #include <sys/wait.h>
 #include <sys/types.h>
 #include <unistd.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>
+#include <signal.h>
+#include <errno.h>
 
@@
 int lsh_exit(char **args)
 {
   return 0;
 }
 
+/* -------------------- NEW: SIGCHLD reaper -------------------- */
+static void lsh_sigchld_handler(int sig) {
+  (void)sig;
+  int saved = errno;        /* preserve errno */
+  int status;
+  pid_t pid;
+  /* Reap all dead children without blocking. No stdio here! */
+  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
+        printf("[shell] background process %d finished with status %d\n",
+               pid, WEXITSTATUS(status));
+  }
+  errno = saved;
+}
+
+static void lsh_install_sigchld(void) {
+  struct sigaction sa;
+  memset(&sa, 0, sizeof(sa));
+  sa.sa_handler = lsh_sigchld_handler;
+  sigemptyset(&sa.sa_mask);
+  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; /* restart slow syscalls; ignore stop/cont notifications */
+  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
+    perror("lsh: sigaction(SIGCHLD)");
+    exit(EXIT_FAILURE);
+  }
+}
+
@@
 int lsh_launch(char **args)
 {
   pid_t pid;
   int status;
 
-  pid = fork();
+  /* Detect background job: trailing "&" */
+  int last = 0;
+  while (args[last]) last++;
+  int background = 0;
+  if (last > 0 && strcmp(args[last - 1], "&") == 0) {
+    background = 1;
+    args[last - 1] = NULL; /* remove "&" before execvp */
+    if (!args[0]) return 1; /* was just "&" */
+  }
+
+  pid = fork();
   if (pid == 0) {
     // Child process
+    /* Restore default SIGINT in child so Ctrl-C affects it, not the shell */
+    struct sigaction dfl;
+    memset(&dfl, 0, sizeof(dfl));
+    dfl.sa_handler = SIG_DFL;
+    sigemptyset(&dfl.sa_mask);
+    sigaction(SIGINT, &dfl, NULL);
     if (execvp(args[0], args) == -1) {
       perror("lsh");
     }
     exit(EXIT_FAILURE);
   } else if (pid < 0) {
     // Error forking
     perror("lsh");
   } else {
-    // Parent process
-    do {
-      waitpid(pid, &status, WUNTRACED);
-    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
+    // Parent process
+    if (background) {
+      /* Don't wait; child will be reaped by SIGCHLD handler */
+      fprintf(stderr, "[bg] started pid %d\n", pid);
+    } else {
+      /* Foreground: wait robustly (no WNOHANG) and handle EINTR */
+      for (;;) {
+        pid_t w = waitpid(pid, &status, 0);
+        if (w == -1) {
+          if (errno == EINTR) continue;     /* interrupted by signal, retry */
+          perror("lsh: waitpid");
+          break;
+        }
+        if (WIFEXITED(status) || WIFSIGNALED(status)) break;
+      }
+    }
   }
 
   return 1;
 }
@@
 int main(int argc, char **argv)
 {
   // Run command loop.
   lsh_loop();
 
   return EXIT_SUCCESS;
 }
