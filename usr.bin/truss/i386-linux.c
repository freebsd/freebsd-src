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
 * Linux/i386-specific system call handling.  Given how much of this code
 * is taken from the freebsd equivalent, I can probably put even more of
 * it in support routines that can be used by any personality support.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/pioctl.h>

#include <machine/reg.h>
#include <machine/psl.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "truss.h"
#include "extern.h"
#include "syscall.h"

static int fd = -1;
static int cpid = -1;
extern int Procfd;

#include "linux_syscalls.h"

static int nsyscalls =
	sizeof(linux_syscallnames) / sizeof(linux_syscallnames[0]);

/* See the comment in i386-fbsd.c about this structure. */
static struct linux_syscall {
	struct syscall *sc;
	char *name;
	int number;
	unsigned long args[5];
	int nargs;	/* number of arguments -- *not* number of words! */
	char **s_args;	/* the printable arguments */
} lsc;

static __inline void
clear_lsc() {
  if (lsc.s_args) {
    int i;
    for (i = 0; i < lsc.nargs; i++)
      if (lsc.s_args[i])
	free(lsc.s_args[i]);
    free(lsc.s_args);
  }
  memset(&lsc, 0, sizeof(lsc));
}

void
i386_linux_syscall_entry(struct trussinfo *trussinfo, int nargs) {
  char buf[32];
  struct reg regs = { 0 };
  int syscall;
  int i;
  struct syscall *sc;

  if (fd == -1 || trussinfo->pid != cpid) {
    sprintf(buf, "/proc/%d/regs", trussinfo->pid);
    fd = open(buf, O_RDWR);
    if (fd == -1) {
      fprintf(trussinfo->outfile, "-- CANNOT READ REGISTERS --\n");
      return;
    }
    cpid = trussinfo->pid;
  }

  clear_lsc();
  lseek(fd, 0L, 0);
  i = read(fd, &regs, sizeof(regs));
  syscall = regs.r_eax;

  lsc.number = syscall;
  lsc.name =
    (syscall < 0 || syscall > nsyscalls) ? NULL : linux_syscallnames[syscall];
  if (!lsc.name) {
    fprintf(trussinfo->outfile, "-- UNKNOWN SYSCALL %d\n", syscall);
  }

  if (lsc.name && (trussinfo->flags & FOLLOWFORKS)
   && ((!strcmp(lsc.name, "linux_fork")
    || !strcmp(lsc.name, "linux_vfork"))))
  {
    trussinfo->in_fork = 1;
  }

  if (nargs == 0)
    return;

  /*
   * Linux passes syscall arguments in registers, not
   * on the stack.  Fortunately, we've got access to the
   * register set.  Note that we don't bother checking the
   * number of arguments.  And what does linux do for syscalls
   * that have more than five arguments?
   */

  lsc.args[0] = regs.r_ebx;
  lsc.args[1] = regs.r_ecx;
  lsc.args[2] = regs.r_edx;
  lsc.args[3] = regs.r_esi;
  lsc.args[4] = regs.r_edi;

  sc = get_syscall(lsc.name);
  if (sc) {
    lsc.nargs = sc->nargs;
  } else {
#ifdef DEBUG
    fprintf(trussinfo->outfile, "unknown syscall %s -- setting args to %d\n",
	    lsc.name, nargs);
#endif
    lsc.nargs = nargs;
  }

  lsc.s_args = malloc((1+lsc.nargs) * sizeof(char*));
  memset(lsc.s_args, 0, lsc.nargs * sizeof(char*));
  lsc.sc = sc;

  if (lsc.name) {

#ifdef DEBUG
    fprintf(stderr, "syscall %s(", lsc.name);
#endif
    for (i = 0; i < lsc.nargs ; i++) {
#ifdef DEBUG
      fprintf(stderr, "0x%x%s",
	      sc ?
	      lsc.args[sc->args[i].offset]
	      : lsc.args[i],
	      i < (lsc.nargs - 1) ? "," : "");
#endif
      if (sc && !(sc->args[i].type & OUT)) {
	lsc.s_args[i] = print_arg(Procfd, &sc->args[i], lsc.args);
      }
    }
#ifdef DEBUG
    fprintf(stderr, ")\n");
#endif
  }

  if (!strcmp(lsc.name, "linux_execve") || !strcmp(lsc.name, "exit")) {
    print_syscall(trussinfo, lsc.name, lsc.nargs, lsc.s_args);
  }

  return;
}

/*
 * Linux syscalls return negative errno's, we do positive and map them
 */
const int bsd_to_linux_errno[] = {
  	-0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9,
 	-10, -35, -12, -13, -14, -15, -16, -17, -18, -19,
 	-20, -21, -22, -23, -24, -25, -26, -27, -28, -29,
 	-30, -31, -32, -33, -34, -11,-115,-114, -88, -89,
 	-90, -91, -92, -93, -94, -95, -96, -97, -98, -99,
	-100,-101,-102,-103,-104,-105,-106,-107,-108,-109,
	-110,-111, -40, -36,-112,-113, -39, -11, -87,-122,
	-116, -66,  -6,  -6,  -6,  -6,  -6, -37, -38,  -9,
  	-6, 
};

int
i386_linux_syscall_exit(struct trussinfo *trussinfo, int syscall) {
  char buf[32];
  struct reg regs;
  int retval;
  int i;
  int errorp;
  struct syscall *sc;

  if (fd == -1 || trussinfo->pid != cpid) {
    sprintf(buf, "/proc/%d/regs", trussinfo->pid);
    fd = open(buf, O_RDONLY);
    if (fd == -1) {
      fprintf(trussinfo->outfile, "-- CANNOT READ REGISTERS --\n");
      return;
    }
    cpid = trussinfo->pid;
  }

  lseek(fd, 0L, 0);
  if (read(fd, &regs, sizeof(regs)) != sizeof(regs)) {
    fprintf(trussinfo->outfile, "\n");
    return;
  }
  retval = regs.r_eax;
  errorp = !!(regs.r_eflags & PSL_C);

  sc = lsc.sc;
  if (!sc) {
    for (i = 0; i < lsc.nargs; i++) {
      lsc.s_args[i] = malloc(12);
      sprintf(lsc.s_args[i], "0x%lx", lsc.args[i]);
    }
  } else {
    for (i = 0; i < sc->nargs; i++) {
      char *temp;
      if (sc->args[i].type & OUT) {
	if (errorp) {
	  temp = malloc(12);
	  sprintf(temp, "0x%lx", lsc.args[sc->args[i].offset]);
	} else {
	  temp = print_arg(Procfd, &sc->args[i], lsc.args);
	}
	lsc.s_args[i] = temp;
      }
    }
  }
  if (errorp) {
    for (i = 0; i < sizeof(bsd_to_linux_errno) / sizeof(int); i++)
      if (retval == bsd_to_linux_errno[i])
      break;
  }
  print_syscall_ret(trussinfo, lsc.name, lsc.nargs, lsc.s_args, errorp,
                    errorp ? i : retval);
  clear_lsc();

  return (retval);
}
