/*
 * Various setup functions for truss.  Not the cleanest-written code,
 * I'm afraid.
 */
/*
 * $Id: setup.c,v 1.2 1997/12/06 08:01:00 sef Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/pioctl.h>
#include <sys/types.h>
#include <sys/wait.h>

static int evflags = 0;

/*
 * setup_and_wait() is called to start a process.  All it really does
 * is vfork(), set itself up to stop on exec or exit, and then exec
 * the given command.  At that point, the child process stops, and
 * the parent can wake up and deal with it.
 */

int
setup_and_wait(char *command[]) {
  struct procfs_status pfs;
  char buf[32];
  int fd;
  int pid;
  extern char *prog;
  int flags;

  pid = vfork();
  if (pid == -1) {
    err(1, "vfork failed");
  }
  if (pid == 0) {	/* Child */
    int mask = S_EXEC | S_EXIT;
    fd = open("/proc/curproc/mem", O_WRONLY);
    if (fd == -1)
      err(2, "cannot open /proc/curproc/mem");
    fcntl(fd, F_SETFD, 1);
    if (ioctl(fd, PIOCBIS, &mask) == -1)
      err(3, "PIOCBIS");
    execvp(command[0], command);
    mask = ~0;
    ioctl(fd, PIOCBIC, &mask);
    err(4, "execvp %s", command[0]);
  }
  /* Only in the parent here */

  if (waitpid(pid, NULL, WNOHANG) != 0) {
    /*
     * Process exited before it got to us -- meaning the exec failed
     * miserably -- so we just quietly exit.
     */
    exit(1);
  }

  sprintf(buf, "/proc/%d/mem", pid);
  if ((fd = open(buf, O_RDWR)) == -1)
    err(5, "cannot open %s", buf);
  if (ioctl(fd, PIOCWAIT, &pfs) == -1)
    err(6, "PIOCWAIT");
  if (pfs.why == S_EXIT) {
    int zero = 0;
    fprintf(stderr, "process exited before exec'ing\n");
    ioctl(fd, PIOCCONT, &zero);
    wait(0);
    exit(7);
  }
  close(fd);
  return pid;
}

/*
 * start_tracing picks up where setup_and_wait() dropped off -- namely,
 * it sets the event mask for the given process id.  Called for both
 * monitoring an existing process and when we create our own.
 */

int
start_tracing(int pid, int flags) {
  int fd;
  char buf[32];
  struct procfs_status tmp;
  sprintf(buf, "/proc/%d/mem", pid);
  fd = open(buf, O_RDWR);
  if (fd == -1)
    err(8, "cannot open %s", buf);

  if (ioctl(fd, PIOCSTATUS, &tmp) == -1) {
    err(10, "cannot get procfs status struct");
  }
  evflags = tmp.events;

  if (ioctl(fd, PIOCBIS, &flags) == -1)
    err(9, "cannot set procfs event bit mask");

  return fd;
}

/*
 * Restore a process back to it's pre-truss state.
 * Called for SIGINT, SIGTERM, SIGQUIT.  This only
 * applies if truss was told to monitor an already-existing
 * process.
 */
void
restore_proc(int signo) {
  extern int Procfd;
  int i;

  i = ~0;
  ioctl(Procfd, PIOCBIC, &i);
  if (evflags)
    ioctl(Procfd, PIOCBIS, &evflags);
  exit(0);
}
