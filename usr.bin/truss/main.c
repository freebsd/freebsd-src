/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * The main module for truss.  Suprisingly simple, but, then, the other
 * files handle the bulk of the work.  And, of course, the kernel has to
 * do a lot of the work :).
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "truss.h"
#include "extern.h"
#include "syscall.h"

#define MAXARGS 6

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n",
	    "usage: truss [-cfaedDS] [-o file] [-s strsize] -p pid",
	    "       truss [-cfaedDS] [-o file] [-s strsize] command [args]");
	exit(1);
}

/*
 * WARNING! "FreeBSD a.out" must be first, or set_etype will not
 * work correctly.
 */
struct ex_types {
	const char *type;
	void (*enter_syscall)(struct trussinfo *, int);
	long (*exit_syscall)(struct trussinfo *, int);
} ex_types[] = {
#ifdef __amd64__
	{ "FreeBSD ELF64", amd64_syscall_entry, amd64_syscall_exit },
	{ "FreeBSD ELF32", amd64_fbsd32_syscall_entry, amd64_fbsd32_syscall_exit },
	{ "Linux ELF32", amd64_linux32_syscall_entry, amd64_linux32_syscall_exit },
#endif
#ifdef __i386__
	{ "FreeBSD a.out", i386_syscall_entry, i386_syscall_exit },
	{ "FreeBSD ELF", i386_syscall_entry, i386_syscall_exit },
	{ "FreeBSD ELF32", i386_syscall_entry, i386_syscall_exit },
	{ "Linux ELF", i386_linux_syscall_entry, i386_linux_syscall_exit },
#endif
#ifdef __ia64__
	{ "FreeBSD ELF64", ia64_syscall_entry, ia64_syscall_exit },
#endif
#ifdef __powerpc__
	{ "FreeBSD ELF", powerpc_syscall_entry, powerpc_syscall_exit },
	{ "FreeBSD ELF32", powerpc_syscall_entry, powerpc_syscall_exit },
#ifdef __powerpc64__
	{ "FreeBSD ELF64", powerpc64_syscall_entry, powerpc64_syscall_exit },
#endif
#endif
#ifdef __sparc64__
	{ "FreeBSD ELF64", sparc64_syscall_entry, sparc64_syscall_exit },
#endif
#ifdef __mips__
	{ "FreeBSD ELF", mips_syscall_entry, mips_syscall_exit },
	{ "FreeBSD ELF32", mips_syscall_entry, mips_syscall_exit },
	{ "FreeBSD ELF64", mips_syscall_entry, mips_syscall_exit }, // XXX
#endif
	{ 0, 0, 0 },
};

/*
 * Set the execution type.  This is called after every exec, and when
 * a process is first monitored. 
 */

static struct ex_types *
set_etype(struct trussinfo *trussinfo)
{
	struct ex_types *funcs;
	char progt[32];
	
	size_t len = sizeof(progt);
	int mib[4];
	int error;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_SV_NAME;
	mib[3] = trussinfo->pid;
	error = sysctl(mib, 4, progt, &len, NULL, 0);
	if (error != 0)
		err(2, "can not get etype");

	for (funcs = ex_types; funcs->type; funcs++)
		if (!strcmp(funcs->type, progt))
			break;

	if (funcs->type == NULL) {
		funcs = &ex_types[0];
		warn("execution type %s is not supported -- using %s",
		    progt, funcs->type);
	}
	return (funcs);
}

char *
strsig(int sig)
{
	char *ret;

	ret = NULL;
	if (sig > 0 && sig < NSIG) {
		int i;
		asprintf(&ret, "sig%s", sys_signame[sig]);
		if (ret == NULL)
			return (NULL);
		for (i = 0; ret[i] != '\0'; ++i)
			ret[i] = toupper(ret[i]);
	}
	return (ret);
}

int
main(int ac, char **av)
{
	int c;
	int i;
	pid_t childpid;
	int status;
	char **command;
	struct ex_types *funcs;
	int initial_open;
	char *fname;
	struct trussinfo *trussinfo;
	char *signame;

	fname = NULL;
	initial_open = 1;

	/* Initialize the trussinfo struct */
	trussinfo = (struct trussinfo *)calloc(1, sizeof(struct trussinfo));
	if (trussinfo == NULL)
		errx(1, "calloc() failed");

	trussinfo->outfile = stderr;
	trussinfo->strsize = 32;
	trussinfo->pr_why = S_NONE;
	trussinfo->curthread = NULL;
	SLIST_INIT(&trussinfo->threadlist);
	while ((c = getopt(ac, av, "p:o:facedDs:S")) != -1) {
		switch (c) {
		case 'p':	/* specified pid */
			trussinfo->pid = atoi(optarg);
			/* make sure i don't trace me */
			if(trussinfo->pid == getpid()) {
				fprintf(stderr, "attempt to grab self.\n");
				exit(2);
			}
			break;
		case 'f': /* Follow fork()'s */
			trussinfo->flags |= FOLLOWFORKS;
			break;
		case 'a': /* Print execve() argument strings. */
			trussinfo->flags |= EXECVEARGS;
			break;
		case 'c': /* Count number of system calls and time. */
			trussinfo->flags |= COUNTONLY;
			break;
		case 'e': /* Print execve() environment strings. */
			trussinfo->flags |= EXECVEENVS;
			break;
		case 'd': /* Absolute timestamps */
			trussinfo->flags |= ABSOLUTETIMESTAMPS;
			break;
		case 'D': /* Relative timestamps */
			trussinfo->flags |= RELATIVETIMESTAMPS;
			break;
		case 'o':	/* Specified output file */
			fname = optarg;
			break;
		case 's':	/* Specified string size */
			trussinfo->strsize = atoi(optarg);
			break;
		case 'S':	/* Don't trace signals */ 
			trussinfo->flags |= NOSIGS;
			break;
		default:
			usage();
		}
	}

	ac -= optind; av += optind;
	if ((trussinfo->pid == 0 && ac == 0) ||
	    (trussinfo->pid != 0 && ac != 0))
		usage();

	if (fname != NULL) { /* Use output file */
		if ((trussinfo->outfile = fopen(fname, "w")) == NULL)
			errx(1, "cannot open %s", fname);
		/*
		 * Set FD_CLOEXEC, so that the output file is not shared with
		 * the traced process.
		 */
		if (fcntl(fileno(trussinfo->outfile), F_SETFD, FD_CLOEXEC) ==
		    -1)
			warn("fcntl()");
	}

	/*
	 * If truss starts the process itself, it will ignore some signals --
	 * they should be passed off to the process, which may or may not
	 * exit.  If, however, we are examining an already-running process,
	 * then we restore the event mask on these same signals.
	 */

	if (trussinfo->pid == 0) {	/* Start a command ourselves */
		command = av;
		trussinfo->pid = setup_and_wait(command);
		signal(SIGINT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
	} else {
		start_tracing(trussinfo->pid);
		signal(SIGINT, restore_proc);
		signal(SIGTERM, restore_proc);
		signal(SIGQUIT, restore_proc);
	}


	/*
	 * At this point, if we started the process, it is stopped waiting to
	 * be woken up, either in exit() or in execve().
	 */

START_TRACE:
	funcs = set_etype(trussinfo);

	initial_open = 0;
	/*
	 * At this point, it's a simple loop, waiting for the process to
	 * stop, finding out why, printing out why, and then continuing it.
	 * All of the grunt work is done in the support routines.
	 */

	clock_gettime(CLOCK_REALTIME, &trussinfo->start_time);

	do {
		struct timespec timediff;
		waitevent(trussinfo);

		switch(i = trussinfo->pr_why) {
		case S_SCE:
			funcs->enter_syscall(trussinfo, MAXARGS);
			clock_gettime(CLOCK_REALTIME,
			    &trussinfo->before);
			break;
		case S_SCX:
			clock_gettime(CLOCK_REALTIME,
			    &trussinfo->after);

			if (trussinfo->curthread->in_fork &&
			    (trussinfo->flags & FOLLOWFORKS)) {
				trussinfo->curthread->in_fork = 0;
				childpid =
				    funcs->exit_syscall(trussinfo,
					trussinfo->pr_data);

				/*
				 * Fork a new copy of ourself to trace
				 * the child of the original traced
				 * process.
				 */
				if (fork() == 0) {
					trussinfo->pid = childpid;
					start_tracing(trussinfo->pid);
					goto START_TRACE;
				}
				break;
			}
			funcs->exit_syscall(trussinfo, MAXARGS);
			break;
		case S_SIG:
			if (trussinfo->flags & NOSIGS)
				break;
			if (trussinfo->flags & FOLLOWFORKS)
				fprintf(trussinfo->outfile, "%5d: ",
				    trussinfo->pid);
			if (trussinfo->flags & ABSOLUTETIMESTAMPS) {
				timespecsubt(&trussinfo->after,
				    &trussinfo->start_time, &timediff);
				fprintf(trussinfo->outfile, "%ld.%09ld ",
				    (long)timediff.tv_sec,
				    timediff.tv_nsec);
			}
			if (trussinfo->flags & RELATIVETIMESTAMPS) {
				timespecsubt(&trussinfo->after,
				    &trussinfo->before, &timediff);
				fprintf(trussinfo->outfile, "%ld.%09ld ",
				    (long)timediff.tv_sec,
				    timediff.tv_nsec);
			}
			signame = strsig(trussinfo->pr_data);
			fprintf(trussinfo->outfile,
			    "SIGNAL %u (%s)\n", trussinfo->pr_data,
			    signame == NULL ? "?" : signame);
			free(signame);
			break;
		case S_EXIT:
			if (trussinfo->flags & COUNTONLY)
				break;
			if (trussinfo->flags & FOLLOWFORKS)
				fprintf(trussinfo->outfile, "%5d: ",
				    trussinfo->pid);
			if (trussinfo->flags & ABSOLUTETIMESTAMPS) {
				timespecsubt(&trussinfo->after,
				    &trussinfo->start_time, &timediff);
				fprintf(trussinfo->outfile, "%ld.%09ld ",
				    (long)timediff.tv_sec,
				    timediff.tv_nsec);
			}
			if (trussinfo->flags & RELATIVETIMESTAMPS) {
			  timespecsubt(&trussinfo->after,
			      &trussinfo->before, &timediff);
			  fprintf(trussinfo->outfile, "%ld.%09ld ",
			    (long)timediff.tv_sec, timediff.tv_nsec);
			}
			fprintf(trussinfo->outfile,
			    "process exit, rval = %u\n", trussinfo->pr_data);
			break;
		default:
			break;
		}
	} while (trussinfo->pr_why != S_EXIT);

	if (trussinfo->flags & FOLLOWFORKS)
		do {
			childpid = wait(&status);
		} while (childpid != -1);

 	if (trussinfo->flags & COUNTONLY)
 		print_summary(trussinfo);

	fflush(trussinfo->outfile);

	return (0);
}
