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
 * FreeBSD/amd64-specific system call handling.  This is probably the most
 * complex part of the entire truss program, although I've got lots of
 * it handled relatively cleanly now.  The system call names are generated
 * automatically, thanks to /usr/src/sys/kern/syscalls.master.  The
 * names used for the various structures are confusing, I sadly admit.
 */

#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>

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

#include "syscalls.h"

static int nsyscalls = nitems(syscallnames);

/*
 * This is what this particular file uses to keep track of a system call.
 * It is probably not quite sufficient -- I can probably use the same
 * structure for the various syscall personalities, and I also probably
 * need to nest system calls (for signal handlers).
 *
 * 'struct syscall' describes the system call; it may be NULL, however,
 * if we don't know about this particular system call yet.
 */
struct freebsd_syscall {
	struct syscall *sc;
	const char *name;
	int number;
	unsigned long *args;
	int nargs;	/* number of arguments -- *not* number of words! */
	char **s_args;	/* the printable arguments */
};

static struct freebsd_syscall *
alloc_fsc(void)
{

	return (malloc(sizeof(struct freebsd_syscall)));
}

/* Clear up and free parts of the fsc structure. */
static void
free_fsc(struct freebsd_syscall *fsc)
{
	int i;

	free(fsc->args);
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
 * in some cases).  Note that if the STOPEVENT() code in amd64/amd64/trap.c
 * is ever changed these functions need to keep up.
 */

void
amd64_syscall_entry(struct trussinfo *trussinfo, int nargs)
{
	struct ptrace_io_desc iorequest;
	struct reg regs;
	struct freebsd_syscall *fsc;
	struct syscall *sc;
	lwpid_t tid;
	int i, reg, syscall_num;

	tid = trussinfo->curthread->tid;

	if (ptrace(PT_GETREGS, tid, (caddr_t)&regs, 0) < 0) {
		fprintf(trussinfo->outfile, "-- CANNOT READ REGISTERS --\n");
		return;
	}

	/*
	 * FreeBSD has two special kinds of system call redirctions --
	 * SYS_syscall, and SYS___syscall.  The former is the old syscall()
	 * routine, basically; the latter is for quad-aligned arguments.
	 */
	reg = 0;
	syscall_num = regs.r_rax;
	switch (syscall_num) {
	case SYS_syscall:
	case SYS___syscall:
		syscall_num = regs.r_rdi;
		reg++;
		break;
	}

	fsc = alloc_fsc();
	if (fsc == NULL)
		return;
	fsc->number = syscall_num;
	fsc->name = (syscall_num < 0 || syscall_num >= nsyscalls) ?
	    NULL : syscallnames[syscall_num];
	if (!fsc->name) {
		fprintf(trussinfo->outfile, "-- UNKNOWN SYSCALL %d --\n",
		    syscall_num);
	}

	if (fsc->name && (trussinfo->flags & FOLLOWFORKS) &&
	    (strcmp(fsc->name, "fork") == 0 ||
	    strcmp(fsc->name, "pdfork") == 0 ||
	    strcmp(fsc->name, "rfork") == 0 ||
	    strcmp(fsc->name, "vfork") == 0))
		trussinfo->curthread->in_fork = 1;

	if (nargs == 0)
		return;

	fsc->args = malloc((1 + nargs) * sizeof(unsigned long));
	for (i = 0; i < nargs && reg < 6; i++, reg++) {
		switch (reg) {
		case 0: fsc->args[i] = regs.r_rdi; break;
		case 1: fsc->args[i] = regs.r_rsi; break;
		case 2: fsc->args[i] = regs.r_rdx; break;
		case 3: fsc->args[i] = regs.r_rcx; break;
		case 4: fsc->args[i] = regs.r_r8; break;
		case 5: fsc->args[i] = regs.r_r9; break;
		}
	}
	if (nargs > i) {
		iorequest.piod_op = PIOD_READ_D;
		iorequest.piod_offs = (void *)(regs.r_rsp + sizeof(register_t));
		iorequest.piod_addr = &fsc->args[i];
		iorequest.piod_len = (nargs - i) * sizeof(register_t);
		ptrace(PT_IO, tid, (caddr_t)&iorequest, 0);
		if (iorequest.piod_len == 0)
			return;
	}

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
			fprintf(stderr, "0x%lx%s", sc ?
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

	if (fsc->name != NULL && (strcmp(fsc->name, "execve") == 0 ||
	    strcmp(fsc->name, "exit") == 0)) {
		/*
		 * XXX
		 * This could be done in a more general
		 * manner but it still wouldn't be very pretty.
		 */
		if (strcmp(fsc->name, "execve") == 0) {
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
 * And when the system call is done, we handle it here.
 * Currently, no attempt is made to ensure that the system calls
 * match -- this needs to be fixed (and is, in fact, why S_SCX includes
 * the system call number instead of, say, an error status).
 */

long
amd64_syscall_exit(struct trussinfo *trussinfo, int syscall_num __unused)
{
	struct reg regs;
	struct freebsd_syscall *fsc;
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

	retval = regs.r_rax;
	errorp = !!(regs.r_rflags & PSL_C);

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

	if (fsc->name != NULL && (strcmp(fsc->name, "execve") == 0 ||
	    strcmp(fsc->name, "exit") == 0))
		trussinfo->curthread->in_syscall = 1;

	/*
	 * It would probably be a good idea to merge the error handling,
	 * but that complicates things considerably.
	 */

	print_syscall_ret(trussinfo, fsc->name, fsc->nargs, fsc->s_args, errorp,
	    retval, fsc->sc);
	free_fsc(fsc);

	return (retval);
}
