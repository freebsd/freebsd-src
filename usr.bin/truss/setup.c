/*
 * Copryight 1997 Sean Eric Fagan
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Sean Eric Fagan
 * 4. Neither the name of the author may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * Various setup functions for truss.  Not the cleanest-written code,
 * I'm afraid.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/pioctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "truss.h"
#include "extern.h"

static int evflags = 0;

/*
 * setup_and_wait() is called to start a process.  All it really does
 * is fork(), set itself up to stop on exec or exit, and then exec
 * the given command.  At that point, the child process stops, and
 * the parent can wake up and deal with it.
 */

int
setup_and_wait(char *command[]) {
  struct procfs_status pfs;
  char buf[32];
  int fd;
  int pid;
  int flags;

  pid = fork();
  if (pid == -1) {
    err(1, "fork failed");
  }
  if (pid == 0) {	/* Child */
    int mask = S_EXEC | S_EXIT;
    fd = open("/proc/curproc/mem", O_WRONLY);
    if (fd == -1)
      err(2, "cannot open /proc/curproc/mem");
    fcntl(fd, F_SETFD, 1);
    if (ioctl(fd, PIOCBIS, mask) == -1)
      err(3, "PIOCBIS");
    flags = PF_LINGER;
    /*
     * The PF_LINGER flag tells procfs not to wake up the
     * process on last close; normally, this is the behaviour
     * we want.
     */
    if (ioctl(fd, PIOCSFL, flags) == -1)
      warn("cannot set PF_LINGER");
    execvp(command[0], command);
    mask = ~0;
    ioctl(fd, PIOCBIC, ~0);
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
    fprintf(stderr, "process exited before exec'ing\n");
    ioctl(fd, PIOCCONT, 0);
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
start_tracing(int pid, int eventflags, int flags) {
  int fd;
  char buf[32];
  struct procfs_status tmp;
  sprintf(buf, "/proc/%d/mem", pid);

  fd = open(buf, O_RDWR);
  if (fd == -1) {
    /*
     * The process may have run away before we could start -- this
     * happens with SUGID programs.  So we need to see if it still
     * exists before we complain bitterly.
     */
    if (kill(pid, 0) == -1)
      return -1;
    err(8, "cannot open %s", buf);
  }

  if (ioctl(fd, PIOCSTATUS, &tmp) == -1) {
    err(10, "cannot get procfs status struct");
  }
  evflags = tmp.events;

  if (ioctl(fd, PIOCBIS, eventflags) == -1)
    err(9, "cannot set procfs event bit mask");

  /*
   * This clears the PF_LINGER set above in setup_and_wait();
   * if truss happens to die before this, then the process
   * needs to be woken up via procctl.
   */

  if (ioctl(fd, PIOCSFL, flags) == -1)
    warn("cannot clear PF_LINGER");

  return fd;
}

/*
 * Restore a process back to it's pre-truss state.
 * Called for SIGINT, SIGTERM, SIGQUIT.  This only
 * applies if truss was told to monitor an already-existing
 * process.
 */
void
restore_proc(int signo __unused) {
  extern int Procfd;

  ioctl(Procfd, PIOCBIC, ~0);
  if (evflags)
    ioctl(Procfd, PIOCBIS, evflags);
  exit(0);
}
