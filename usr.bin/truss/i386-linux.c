/*
 * Copyright 1997 Sean Eric Fagan
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
#include <sys/ptrace.h>

#include <machine/reg.h>
#include <machine/psl.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "truss.h"
#include "syscall.h"
#include "extern.h"

#include "linux_syscalls.h"

static int nsyscalls =
	sizeof(linux_syscallnames) / sizeof(linux_syscallnames[0]);

/*
 * This is what this particular file uses to keep track of a system call.
 * It is probably not quite sufficient -- I can probably use the same
 * structure for the various syscall personalities, and I also probably
 * need to nest system calls (for signal handlers).
 *
 * 'struct syscall' describes the system call; it may be NULL, however,
 * if we don't know about this particular system call yet.
 */
struct linux_syscall {
	struct syscall *sc;
	const char *name;
	int number;
	unsigned long args[5];
	int nargs;	/* number of arguments -- *not* number of words! */
	char **s_args;	/* the printable arguments */
};

static struct linux_syscall *
alloc_fsc(void)
{

	return (malloc(sizeof(struct linux_syscall)));
}

/* Clear up and free parts of the fsc structure. */
static void
free_fsc(struct linux_syscall *fsc)
{
	int i;

	if (fsc->s_args) {
		for (i = 0; i < fsc->nargs; i++)
			free(fsc->s_args[i]);
		free(fsc->s_args);
	}
	free(fsc);
}

/*
 * Called when a process has entered a system call.  nargs is the
 * number of words, not number of arguments (a necessary distinction
 * in some cases).  Note that if the STOPEVENT() code in i386/i386/trap.c
 * is ever changed these functions need to keep up.
 */

void
i386_linux_syscall_entry(struct trussinfo *trussinfo, int nargs)
{
	struct reg regs;
	struct linux_syscall *fsc;
	struct syscall *sc;
	lwpid_t tid;
	int i, syscall_num;

	tid = trussinfo->curthread->tid;

	if (ptrace(PT_GETREGS, tid, (caddr_t)&regs, 0) < 0) {
		fprintf(trussinfo->outfile, "-- CANNOT READ REGISTERS --\n");
		return;
	}

	syscall_num = regs.r_eax;

	fsc = alloc_fsc();
	if (fsc == NULL)
		return;
	fsc->number = syscall_num;
	fsc->name = (syscall_num < 0 || syscall_num >= nsyscalls) ?
	    NULL : linux_syscallnames[syscall_num];
	if (!fsc->name) {
		fprintf(trussinfo->outfile, "-- UNKNOWN SYSCALL %d --\n",
		    syscall_num);
	}

	if (fsc->name && (trussinfo->flags & FOLLOWFORKS) &&
	    (strcmp(fsc->name, "linux_fork") == 0 ||
	    strcmp(fsc->name, "linux_vfork") == 0))
		trussinfo->curthread->in_fork = 1;

	if (nargs == 0)
		return;

	/*
	 * Linux passes syscall arguments in registers, not
	 * on the stack.  Fortunately, we've got access to the
	 * register set.  Note that we don't bother checking the
	 * number of arguments.	And what does linux do for syscalls
	 * that have more than five arguments?
	 */

	fsc->args[0] = regs.r_ebx;
	fsc->args[1] = regs.r_ecx;
	fsc->args[2] = regs.r_edx;
	fsc->args[3] = regs.r_esi;
	fsc->args[4] = regs.r_edi;

	sc = get_syscall(fsc->name);
	if (sc)
		fsc->nargs = sc->nargs;
	else {
#if DEBUG
		fprintf(trussinfo->outfile, "unknown syscall %s -- setting "
		    "args to %d\n", fsc->name, nargs);
#endif
		fsc->nargs = nargs;
	}

	fsc->s_args = calloc(1, (1 + fsc->nargs) * sizeof(char *));
	fsc->sc = sc;

	/*
	 * At this point, we set up the system call arguments.
	 * We ignore any OUT ones, however -- those are arguments that
	 * are set by the system call, and so are probably meaningless
	 * now.	This doesn't currently support arguments that are
	 * passed in *and* out, however.
	 */

	if (fsc->name) {
#if DEBUG
		fprintf(stderr, "syscall %s(", fsc->name);
#endif
		for (i = 0; i < fsc->nargs; i++) {
#if DEBUG
			fprintf(stderr, "0x%x%s", sc ?
			    fsc->args[sc->args[i].offset] : fsc->args[i],
			    i < (fsc->nargs - 1) ? "," : "");
#endif
			if (sc && !(sc->args[i].type & OUT)) {
				fsc->s_args[i] = print_arg(&sc->args[i],
				    fsc->args, 0, trussinfo);
			}
		}
#if DEBUG
		fprintf(stderr, ")\n");
#endif
	}

#if DEBUG
	fprintf(trussinfo->outfile, "\n");
#endif

	if (fsc->name != NULL && (strcmp(fsc->name, "linux_execve") == 0 ||
	    strcmp(fsc->name, "exit") == 0)) {
		/*
		 * XXX
		 * This could be done in a more general
		 * manner but it still wouldn't be very pretty.
		 */
		if (strcmp(fsc->name, "linux_execve") == 0) {
			if ((trussinfo->flags & EXECVEARGS) == 0) {
				if (fsc->s_args[1]) {
					free(fsc->s_args[1]);
					fsc->s_args[1] = NULL;
				}
			}
			if ((trussinfo->flags & EXECVEENVS) == 0) {
				if (fsc->s_args[2]) {
					free(fsc->s_args[2]);
					fsc->s_args[2] = NULL;
				}
			}
		}
	}
	trussinfo->curthread->fsc = fsc;
}

/*
 * Linux syscalls return negative errno's, we do positive and map them
 */
static const int bsd_to_linux_errno[] = {
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

long
i386_linux_syscall_exit(struct trussinfo *trussinfo, int syscall_num __unused)
{
	struct reg regs;
	struct linux_syscall *fsc;
	struct syscall *sc;
	lwpid_t tid;
	long retval;
	int errorp, i;

	if (trussinfo->curthread->fsc == NULL)
		return (-1);

	tid = trussinfo->curthread->tid;

	if (ptrace(PT_GETREGS, tid, (caddr_t)&regs, 0) < 0) {
		fprintf(trussinfo->outfile, "-- CANNOT READ REGISTERS --\n");
		return (-1);
	}

	retval = regs.r_eax;
	errorp = !!(regs.r_eflags & PSL_C);

	/*
	 * This code, while simpler than the initial versions I used, could
	 * stand some significant cleaning.
	 */

	fsc = trussinfo->curthread->fsc;
	sc = fsc->sc;
	if (!sc) {
		for (i = 0; i < fsc->nargs; i++)
			asprintf(&fsc->s_args[i], "0x%lx", fsc->args[i]);
	} else {
		/*
		 * Here, we only look for arguments that have OUT masked in --
		 * otherwise, they were handled in the syscall_entry function.
		 */
		for (i = 0; i < sc->nargs; i++) {
			char *temp;
			if (sc->args[i].type & OUT) {
				/*
				 * If an error occurred, then don't bother
				 * getting the data; it may not be valid.
				 */
				if (errorp) {
					asprintf(&temp, "0x%lx",
					    fsc->args[sc->args[i].offset]);
				} else {
					temp = print_arg(&sc->args[i],
					    fsc->args, retval, trussinfo);
				}
				fsc->s_args[i] = temp;
			}
		}
	}

	/*
	 * It would probably be a good idea to merge the error handling,
	 * but that complicates things considerably.
	 */
	if (errorp) {
		for (i = 0;
		    (size_t)i < sizeof(bsd_to_linux_errno) / sizeof(int); i++) {
			if (retval == bsd_to_linux_errno[i])
				break;
		}
	}

	if (fsc->name != NULL && (strcmp(fsc->name, "linux_execve") == 0 ||
	    strcmp(fsc->name, "exit") == 0))
		trussinfo->curthread->in_syscall = 1;

	print_syscall_ret(trussinfo, fsc->name, fsc->nargs, fsc->s_args, errorp,
	    errorp ? i : retval, fsc->sc);
	free_fsc(fsc);

	return (retval);
}
