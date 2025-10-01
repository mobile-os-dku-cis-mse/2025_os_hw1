# HW1. Simple Shell Program

^^^ DKU Mobile-System-Engineering Yunseo-Lee

## BRIEF SUMMARY

```
ISSUES :: CAUSES :: SOLUTIONS
  1) Shell Freezes :: Blocking `waitpid` call in main loop :: (+)Asynchronous reaping with SIGCHLD handler for background jobs.
     FIXED :: ISSUES (Introduced by SIGCHLD approach)
       + Fixed :: Misused `WEXITSTATUS` on signal-terminated processes.
       + Fixed :: Race condition between foreground `waitpid` and the SIGCHLD handler reaping the same child process.
     SOLUTIONS :: (Modern, more robust approach)
       + pidfd :: Using pidfd + waitid(P_PIDFD) to eliminate PID-reuse race conditions entirely.
  2) Stdio Deadlock / Unresponsive Shell :: Improper terminal control :: (->)Correctly manage foreground process group with `tcsetpgrp`.
```

```C
// my_shell.c
void loop(void)
{
  char *line;
  char **args;
  int status;

  do {
    printf("> ");
    line = read_line();
    args = split_line(line);
    status = launch(args); // below

    free(line);
    free(args);
  } while (status);
}

int launch(char **args)
{
  pid_t pid;
  int status;

  pid = fork(); // (1) FORK
  if (pid == 0) { // Child process
    if (execvp(args[0], args) == -1) { // (2) EXEC
      perror("lsh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) { // Error forking
    perror("lsh");
  } else { // Parent process
    do {
      waitpid(pid, &status, WUNTRACED); // (3) WAIT
    } while (!WIFEXITED(status) && !WIFSIGNALED(status)); // IMPORTANT
  }
  return 1;
}
```

## PROBLEM_1. Shell Freezes

In `my_shell.c`,

```C
pid = fork();
execvp(args[0], args);
do {
    waitpid(pid, &status, WUNTRACED);
} while(!WIFEXITED(status) && !WIFSIGNALED(status));
```

- Waits until child process exits normally.
- Waits until child process is killed by signal.
- **++> !! So Long-running tasks freeze the shell !!**

In Linux v_6.16 `kernel/exit.c`,

```C
// do_wait(struct wait_opts *wo) { ...
do {
    set_current_state(TASK_INTERRUPTIBLE);
    retval = __do_wait(wo);
    if (retval != -ERESTARTSYS) // Cond1
        break;
	if (signal_pending(current)) // Cond2
		break;
	schedule(); // blocked
} while (1);
__set_current_state(TASK_RUNNING);
```

Task **sleeps** until either<br>
`retval` is equal to `-ERESTARTSYS`, or<br>
`signal_pending(current)` is true.

> Step_1. when `retval` becomes `-ERESTARTSYS`?

```C
// __do_wait(struct wait_opts *wo) { ...
notask:
    // retval = (wo->wo_type == PIDTYPE_PID) ?
    //             do_wait_pid(wo) : do_wait_thread(wo, tsk);
	retval = wo->notask_error;
	if (!retval && !(wo->wo_flags & WNOHANG))
		return -ERESTARTSYS;
	return retval;
```

Retval becomes `-ERESTARTSYS` when,<br>
**>> "Have child but no event yet" or "Non-blocking mode"**.

To prevent **sleeping**, which makes frozen shell,<br>
how about replace with `waitpid(pid, &status, WNOHANG);` ?

Solves the problem?<br>
Not enough. Extra codes required.<br>

Reasons are,<br>
&-- **1) Not tracking background jobs yet,<br>**
&-- **2) Not Reaping zombie processes yet,<br>**
&-- 3) Not supporting user-level command '&'.

To deal with R1 and R2, can use **SIGCHLD Handler**.

```shell
+ // NEW: lightweight SIGCHLD handler
+ void sigchld_handler(int sig) {
+    int status;
+    pid_t pid;
+
+    // Non-blocking reap all children
+    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
+        printf("[shell] background process %d finished with status %d\n",
+               pid, WEXITSTATUS(status));
+    }
+}
```

Why `while ((pid = waitpid(-1, &status, WNOHANG)) > 0)` ?<br>
&-- **1) Order of child termination is uncertain ---> "-1"**<br>
&-- **2) waitpid() handles only 1 child ---> "while"**<br>
&-- **3) SIGCHLD Coalescing ---> "while"**<br>
&-- 4) Non blocking --> "WNOHANG"<br>
&-- 5) Loop until child process exists --> ">0"

To prove with Linux kernel code,

```C
waitpid(-1, &status, WNOHANG)
// glibc -> SYSCALL_DEFINE4 --> kernel_wait -->
long kernel_wait4(pid_t upid, int __user *stat_addr, int options,
		  struct rusage *ru)
{
    if (upid == -1)
		type = PIDTYPE_MAX;
// -->
static int eligible_pid(struct wait_opts *wo, struct task_struct *p)
{ // consider all child as eligible
	return	wo->wo_type == PIDTYPE_MAX ||
		task_pid_type(p, wo->wo_type) == wo->wo_pid;
}
// -->
long __do_wait(struct wait_opts *wo)
{
    ...
    read_lock(&tasklist_lock);
    if (wo->wo_type == PIDTYPE_PID) { ... }
    else {
        struct task_struct *tsk = current;

        do { // + loop threads in same group
            retval = do_wait_thread(wo, tsk);
            if (retval) return retval;
        } while_each_thread(current, tsk);
    }
    read_unlock(&tasklist_lock);
// -->
static int do_wait_thread(struct wait_opts *wo, struct task_struct *tsk)
{
    struct task_struct *p;

    list_for_each_entry(p, &tsk->children, sibling) { // + loop children
        int ret = wait_consider_task(wo, 0, p);
```

See `patch_1.c` for detailed implementation.

So,

> Step_1. when `retval` becomes `-ERESTARTSYS`?<br>
> Step_2. When `signal_pending(current)` becomes true?

Answer was,

> - **"Have child but no event yet" or "in Non-blocking mode"**
> - **when other process sended signal**

And using SIGCHLD was suggested.

---

### (+) ADDED FUNCTION. Pipelines and Redirections

Also, added more to deal with `cat out.txt | tr a-z A-Z > result.txt`,<br>
specifically,<br>
&-- **FUN_1) pipelines + redirections**
&-- **FUN_2) longer instructions**

To implement algorithm,<br>
defining requirements to organize conditions.

```
 // REQUIREMENTS

 - cmd 'unit' is "<instr> args[]".
 - cmd can be expanded with multiple cmds.
 - if "cmd1 | cmd2", then cmd1 and cmd2 are different.
    - else, treat as cmd unit.
 - if "cmd1 < arg", then arg goes to cmd1's input.
 - if "cmd1 > arg" or "cmd1 >> arg", then cmd1's output goes to arg.
```

Approached at 2-levels.

1. In aspect of **data structure**, to deal with
   - '|', should define cmd unit, struct `LshCmd`, and<br>
   - '>/>>' + '<', should define `char *in_path, *out_path`, and<br>
   - expanded cmds, shoud define `int append` field.

```C
typedef struct {
  char **argv;    /* NULL-terminated */
  char *in_path;  /* or NULL */
  char *out_path; /* or NULL */
  int   append;   /* 0 => truncate, 1 => append */
} LshCmd; //  Helpers for pipelines / redirection
LshCmd *cmds = calloc(cap, sizeof(LshCmd));

while (args[i]) {
    LshCmd c = {0};
    c.argv = malloc(argv_cap * sizeof(char*));

    for (; args[i]; i++) {
        if (strcmp(args[i], "|") == 0) { i++; break; }
        else if (strcmp(args[i], "<") == 0) { i+=2; continue; }
        else if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
            i+=2; continue;
        } else { c.argv[argcnt++] = args[i]; }
    }
    cmds[ncmd++] = c;
}
```

2. In aspect of **correlation with execvp()**,
   - All requires `dup2(fd, STDIN_FILENO)`, but
   - '>', + `open(..., O_WRONLY | O_CREAT | O_TRUNC)`
   - '>>', + `open(..., O_WRONLY | O_CREAT | O_APPEND)`
   - '<', + `open(..., O_RDONLY)`.

```C
/* Close all pipe fds in child */
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
/* Exec */
execvp(cmds[i].argv[0], cmds[i].argv);
```

See `patch_2.c` for detailed implementation.

## Go back to PROBLEM_1. Shell Freezes

But patched code has some **CRITICAL PROBLEMS**

1. Misused `WEXITSTATUS`
2. Unhandled Race between "foreground vs. sigchld handler"

### 1. Misused `WEXITSTATUS`

How kernel deals zombie process during wait?

```C
static int wait_task_zombie(struct wait_opts *wo, struct task_struct *p)
{
    status = (p->signal->flags & SIGNAL_GROUP_EXIT)
		? p->signal->group_exit_code : p->exit_code;
// -->
static int wait_task_zombie(struct wait_opts *wo, struct task_struct *p)
{
    if (unlikely(wo->wo_flags & WNOWAIT)) {
        goto out_info;
// -->
out_info:
	infop = wo->wo_info;
	if (infop) {
		if ((status & 0x7f) == 0) { // normal termination
			infop->cause = CLD_EXITED;
			infop->status = status >> 8;
		} else { // signal termination
			infop->cause = (status & 0x80) ? CLD_DUMPED : CLD_KILLED;
			infop->status = status & 0x7f;
		}
		infop->pid = pid;
		infop->uid = uid;
	}
	return pid;
}
```

Answer is,<br>
&-- 1) In normal termination, (big){status_code},0000000<br>
&-- 2) In signal termination, `(big){??},{W_COREDUMP},{status_code}.<br>

**So when you read `WEXITSTATUS` when it is not a normal shutdown,**<br>
**the output will be meaningless values of the top 8 bits.**

But situation is not just for `wait_task_zombie`.<br>
By looking at below macro of glibc,<br>
it can be expanded to other cases such as WIFSTOPPED and WIFCONTINUED.

```C
//glibc/bits.waitstatus.h

/* Nonzero if STATUS indicates normal termination.  */
#define	__WIFEXITED(status)	(__WTERMSIG(status) == 0)
/* If WIFSIGNALED(STATUS), the terminating signal.  */
#define	__WTERMSIG(status)	((status) & 0x7f)

/* If WIFSTOPPED(STATUS), the signal that stopped the child.  */
#define	__WSTOPSIG(status)	__WEXITSTATUS(status)
/* If WIFEXITED(STATUS), the low-order 8 bits of the status.  */
#define	__WEXITSTATUS(status)	(((status) & 0xff00) >> 8)

#define __WIFSIGNALED(status) \
  (((signed char) (((status) & 0x7f) + 1) >> 1) > 0)

#define	__WIFSTOPPED(status)	(((status) & 0xff) == 0x7f)
#ifdef WCONTINUED
# define __WIFCONTINUED(status)	((status) == __W_CONTINUED)
#define __W_CONTINUED		0xffff

/* Nonzero if STATUS indicates the child dumped core.  */
#define	__WCOREDUMP(status)	((status) & __WCOREFLAG)
/* Nonzero if STATUS indicates the child dumped core.  */
#define	__WCOREDUMP(status)	((status) & __WCOREFLAG)
#define	__WCOREFLAG		0x80
```

So change code into below snippet is better.

```C
int st;
pid_t pid;
while ((pid = waitpid(-1, &st, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
    if (WIFEXITED(st)) {
        int code = WEXITSTATUS(st);
        on_exit(pid, code);
    } else if (WIFSIGNALED(st)) {
        int sig = WTERMSIG(st);
        bool dumped = WCOREDUMP(st);
        on_killed(pid, sig, dumped);
    } else if (WIFSTOPPED(st)) {
        on_stopped(pid, WSTOPSIG(st));
    } else if (WIFCONTINUED(st)) {
        on_continued(pid);
    }
}
```

### 2. Unhandled Race between "foreground vs. sigchld handler"

Differ from PROBLEM_2(Pid Race), it's not an error.

It arises from `cmpxchg(&p->exit_state, EXIT_ZOMBIE, state)`.

```C
// CALL STACK: __do_exit --> do_wait_thread --> wait_consider_task -->
static int wait_task_zombie(struct wait_opts *wo, struct task_struct *p)
{
  if (cmpxchg(&p->exit_state, EXIT_ZOMBIE, state) != EXIT_ZOMBIE)
		return 0;
```

@- 1. Suppose there is thread A, which is the foreground path.<br>
@- 2. A is blocking in `waitpid(pid, 0)` to wait for a specific child.<br>
@- 3. Then, a concurrent reaper (either a SIGCHLD handler or thread B) comes.<br>
@- 4. It may call `waitpid(-1, WNOHANG)` and reap that same child first.

The kernel enforces **single ownership of a zombie**,<br>
via an **atomic state change** (EXIT_ZOMBIE → EXIT_DEAD/EXIT_TRACE).

So when A finally enters the kernel,<br>
the child is no longer waitable,<br>
and `waitpid(pid, 0)` **fails with -1 and errno == ECHILD**.

OK, then<br>
`pid = waitpid(-1, &st, WNOHANG | WUNTRACED | WCONTINUED)` is not enough.

> We have to **add more details to returned pid**.

```C
pid_t pid = waitpid(-1, &st, WNOHANG | WUNTRACED | WCONTINUED);
if (rc == pid) { ... } // we reaped it ourselves
else if (rc == -1 && errno == ECHILD) { ... } // same as, W-branching
else if (rc == 0) break; //
else {
  if (errno == EINTR) continue;
  if (errno == ECHILD) break;
  perror("waitpid");
  break; }
```

But this is not enough.

The reason is,<br>
dirst success caller only takes the child's end state, signal and etc.

Related to<br>
&-- **1) Pipeline, triggers lack of exit_code.<br>**
&-- **2) tcsetpgrp(), conflicts with foreground restoration.**

#### 1. Pipeline, Triggers Lack of Exit_code

**Case-1)**<br>
Think case of `A | notexist | C`.<br>
notexist should return 127/126 due to execve fail,<br>
but other process already reaped PID then<br>
fails to detect "command not found/unexecutable" and<br>
@ - **continues like a success.**

**Case-2)**<br>
Think case of `fetch | validate | publish`.<br>
Shell does not receive the validate exit code.<br>
Retry trigger not triggered, and<br>
@ - **corrupted data transitioned to public or pipeline silent failure.**

#### 2. tcsetpgrp(), Conflicts with Foreground Restoration

**Case-1) tty continues to bind to Job PGID**<br>
**Case-1 Result) Shell "Can't Talk"**<br>
&-- <br>
**&-- P1.** The shell runs a pipeline in a process group (say PGID 2000) and<br>
&---- makes that group the terminal foreground with tcsetpgrp(tty, 2000).<br>
**&-- P2.** Another thread reaps the last process first using waitpid(-1, WNOHANG, ...).<br>
**&-- P3.** The shell’s foreground thread then calls waitpid(last_pid, 0, ...)<br>
&----- but gets -1/ECHILD because that process was already reaped.<br>
**&-- P4.** The code that should restore the terminal to the shell <br>
(e.g., tcsetpgrp(tty, shell_pgid)) only runs on a successful waitpid, so it never runs here.<br>
&--<br>
**&-- Result:** the terminal’s foreground stays on PGID 2000.<br>
&------ Your prompt may print,<br>
&------ but keyboard input and Ctrl-C/Ctrl-Z still go to the (now-finished) job,<br>
&------ not the shell—so the shell looks unresponsive.

Case-2) job is still running but tty is returned to shell<br>
Case-2 Result) SIGTTIN/SIGTTOU when entered<br>

Case-3) twisted stop (CTRL-Z) treatment<br>
Case-3 Result) prompts do not return, incorrect status recorded<br>

> Before and after the foreground `waitpid(fg_pid, 0)`,<br>
> the "double collection" race should be **blocked**.

So `pthread_sigmask(SIG_BLOCK, &set, &old);` and<br>
`pthread_sigmask(SIG_SETMASK, &old, NULL);` should be added between waitpid.

See `patch_3.c` for more implementations.

---

### Sug_1. Use `pidfd + waitid(P_PIDFD)` instead of `waitpid`

Or can apply modern kernel features.

Because even patch_3 version code is not enough.<br>
`pidfd + waitid(P_PIDFD)` can,<br>
&-- **1) Eliminate PID-reuse bugs**<br>
&-- **2) Can deal with ECHILD issue more simpler**<br>
&-- **3) Better job control plumbing**<br>

On Linux, a PID is just a small integer.<br>
After a process fully exits and is reaped,<br>
the kernel may recycle that PID for a brand-new, unrelated process.<br>

> This creates classic time-of-check to time-of-use (TOCTOU) races, whenever
>
> - look up / remember a PID, and
> - only later signal / wait / poll on “that PID”.

Epoll tells exactly which stage changed, so<br>
can compute $?, PIPESTATUS, and pipefail deterministically.

```C
// 1) After fork (or with clone3 flags), get a pidfd
int pidfd = pidfd_open(child_pid, 0);        // or clone3(..., CLONE_PIDFD) to get it directly
// 2) Add to epoll
struct epoll_event ev = { .events = EPOLLIN, .data.fd = pidfd };
epoll_ctl(epfd, EPOLL_CTL_ADD, pidfd, &ev);

// 3) Event loop
for (;;) {
  int n = epoll_wait(epfd, events, MAX, -1);
  for (int i = 0; i < n; i++) {
    int cfd = events[i].data.fd;
    siginfo_t si = {0};

    // Inspect status; use WNOWAIT if you want a two-phase flow
    if (waitid(P_PIDFD, cfd, &si, WEXITED|WSTOPPED|WCONTINUED|WNOWAIT) == 0) {
      // si.si_code: CLD_EXITED / CLD_KILLED / CLD_DUMPED / CLD_STOPPED / CLD_CONTINUED
      // si.si_status: exit code or signal
      // Update job table, PIPESTATUS, etc.
    }
  }
}
```

See `patch_4.c` for more details.

## PROBLEM_2. Stdio Deadlock

Applied `tcsetpgrp()`.

See `patch_5.c` for more details.

Of course. Here is a rewritten summary and conclusion for your analysis in English, based on the document you provided.

## Summary

This project documents the development of a simple shell, focusing on overcoming two critical challenges: **shell freezes** caused by process waiting and **I/O deadlocks** related to terminal control.

The initial implementation used a basic `fork-exec-waitpid` loop, which caused the shell to freeze while waiting for long-running foreground commands. To address this and enable background job support, a `SIGCHLD` signal handler was implemented to asynchronously reap terminated child processes. This non-blocking approach, however, introduced more subtle and severe issues:

1.  **Race Condition**: A critical race condition was discovered between the main thread waiting for a specific foreground process and the `SIGCHLD` handler potentially reaping that same process first. If the handler "wins" the race, the main thread's `waitpid` call fails with `ECHILD`. This failure leads to two major bugs:

    - **Incorrect Pipeline Status**: The shell fails to retrieve the exit status of processes in a pipeline, potentially masking critical errors (e.g., "command not found").
    - **Terminal Control Failure**: The shell fails to restore terminal control to itself via `tcsetpgrp`, leaving the terminal attached to the now-defunct process group and making the shell unresponsive to user input.

2.  **Incorrect Status Handling**: An initial flaw in the `SIGCHLD` handler involved incorrectly using the `WEXITSTATUS` macro on processes that were terminated by a signal, leading to meaningless status codes.

The race condition was mitigated by using `pthread_sigmask` to block `SIGCHLD` during the critical section where the shell waits for a foreground job. The status handling was fixed by using the correct macros (`WIFEXITED`, `WIFSIGNALED`, etc.) to properly interpret the child's exit reason.

Finally, the analysis proposes a superior, modern solution using `pidfd` combined with `waitid(P_PIDFD)`. This kernel feature provides a stable file descriptor for a specific process instance, which **eliminates PID-reuse race conditions** entirely and allows for a more robust, event-driven design using `epoll`, simplifying job control logic significantly.
