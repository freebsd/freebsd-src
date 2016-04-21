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
 * Various setup functions for truss.  Not the cleanest-written code,
 * I'm afraid.
 */

#include <sys/ptrace.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "truss.h"
#include "syscall.h"
#include "extern.h"

SET_DECLARE(procabi, struct procabi);

static sig_atomic_t detaching;

static void	new_proc(struct trussinfo *, pid_t);

/*
 * setup_and_wait() is called to start a process.  All it really does
 * is fork(), enable tracing in the child, and then exec the given
 * command.  At that point, the child process stops, and the parent
 * can wake up and deal with it.
 */
void
setup_and_wait(struct trussinfo *info, char *command[])
{
	pid_t pid;

	pid = vfork();
	if (pid == -1)
		err(1, "fork failed");
	if (pid == 0) {	/* Child */
		ptrace(PT_TRACE_ME, 0, 0, 0);
		execvp(command[0], command);
		err(1, "execvp %s", command[0]);
	}

	/* Only in the parent here */
	if (waitpid(pid, NULL, 0) < 0)
		err(1, "unexpect stop in waitpid");

	new_proc(info, pid);
}

/*
 * start_tracing is called to attach to an existing process.
 */
void
start_tracing(struct trussinfo *info, pid_t pid)
{
	int ret, retry;

	retry = 10;
	do {
		ret = ptrace(PT_ATTACH, pid, NULL, 0);
		usleep(200);
	} while (ret && retry-- > 0);
	if (ret)
		err(1, "can not attach to target process");

	if (waitpid(pid, NULL, 0) < 0)
		err(1, "Unexpect stop in waitpid");

	new_proc(info, pid);
}

/*
 * Restore a process back to it's pre-truss state.
 * Called for SIGINT, SIGTERM, SIGQUIT.  This only
 * applies if truss was told to monitor an already-existing
 * process.
 */
void
restore_proc(int signo __unused)
{

	detaching = 1;
}

static void
detach_proc(pid_t pid)
{

	/* stop the child so that we can detach */
	kill(pid, SIGSTOP);
	if (waitpid(pid, NULL, 0) < 0)
		err(1, "Unexpected stop in waitpid");

	if (ptrace(PT_DETACH, pid, (caddr_t)1, 0) < 0)
		err(1, "Can not detach the process");

	kill(pid, SIGCONT);
}

/*
 * Determine the ABI.  This is called after every exec, and when
 * a process is first monitored.
 */
static struct procabi *
find_abi(pid_t pid)
{
	struct procabi **pabi;
	size_t len;
	int error;
	int mib[4];
	char progt[32];

	len = sizeof(progt);
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_SV_NAME;
	mib[3] = pid;
	error = sysctl(mib, 4, progt, &len, NULL, 0);
	if (error != 0)
		err(2, "can not get sysvec name");

	SET_FOREACH(pabi, procabi) {
		if (strcmp((*pabi)->type, progt) == 0)
			return (*pabi);
	}
	warnx("ABI %s for pid %ld is not supported", progt, (long)pid);
	return (NULL);
}

static void
new_proc(struct trussinfo *info, pid_t pid)
{
	struct procinfo *np;

	/*
	 * If this happens it means there is a bug in truss.  Unfortunately
	 * this will kill any processes are attached to.
	 */
	LIST_FOREACH(np, &info->proclist, entries) {
		if (np->pid == pid)
			errx(1, "Duplicate process for pid %ld", (long)pid);
	}

	if (info->flags & FOLLOWFORKS)
		if (ptrace(PT_FOLLOW_FORK, pid, NULL, 1) == -1)
			err(1, "Unable to follow forks for pid %ld", (long)pid);
	np = calloc(1, sizeof(struct procinfo));
	np->pid = pid;
	np->abi = find_abi(pid);
	SLIST_INIT(&np->threadlist);
	LIST_INSERT_HEAD(&info->proclist, np, entries);
}

static void
free_proc(struct procinfo *p)
{
	struct threadinfo *t, *t2;

	SLIST_FOREACH_SAFE(t, &p->threadlist, entries, t2) {
		free(t);
	}
	LIST_REMOVE(p, entries);
	free(p);
}

static void
detach_all_procs(struct trussinfo *info)
{
	struct procinfo *p, *p2;

	LIST_FOREACH_SAFE(p, &info->proclist, entries, p2) {
		detach_proc(p->pid);
		free_proc(p);
	}
}

static struct procinfo *
find_proc(struct trussinfo *info, pid_t pid)
{
	struct procinfo *np;

	LIST_FOREACH(np, &info->proclist, entries) {
		if (np->pid == pid)
			return (np);
	}

	return (NULL);
}

/*
 * Change curthread member based on (pid, lwpid).
 * If it is a new thread, create a threadinfo structure.
 */
static void
find_thread(struct trussinfo *info, pid_t pid, lwpid_t lwpid)
{
	struct procinfo *np;
	struct threadinfo *nt;

	np = find_proc(info, pid);
	assert(np != NULL);

	SLIST_FOREACH(nt, &np->threadlist, entries) {
		if (nt->tid == lwpid) {
			info->curthread = nt;
			return;
		}
	}

	nt = calloc(1, sizeof(struct threadinfo));
	if (nt == NULL)
		err(1, "calloc() failed");
	nt->proc = np;
	nt->tid = lwpid;
	SLIST_INSERT_HEAD(&np->threadlist, nt, entries);
	info->curthread = nt;
}

/*
 * When a process exits, it no longer has any threads left.  However,
 * the main loop expects a valid curthread.  In cases when a thread
 * triggers the termination (e.g. calling exit or triggering a fault)
 * we would ideally use that thread.  However, if a process is killed
 * by a signal sent from another process then there is no "correct"
 * thread.  We just punt and use the first thread.
 */
static void
find_exit_thread(struct trussinfo *info, pid_t pid)
{
	struct procinfo *np;
	struct threadinfo *nt;

	np = find_proc(info, pid);
	assert(np != NULL);

	if (SLIST_EMPTY(&np->threadlist)) {
		/*
		 * If an existing process exits right after we attach
		 * to it but before it posts any events, there won't
		 * be any threads.  Create a dummy thread and set its
		 * "before" time to the global start time.
		 */
		nt = calloc(1, sizeof(struct threadinfo));
		if (nt == NULL)
			err(1, "calloc() failed");
		nt->proc = np;
		nt->tid = 0;
		SLIST_INSERT_HEAD(&np->threadlist, nt, entries);
		nt->before = info->start_time;
	}
	info->curthread = SLIST_FIRST(&np->threadlist);
}

static void
alloc_syscall(struct threadinfo *t, struct ptrace_lwpinfo *pl)
{
	u_int i;

	assert(t->in_syscall == 0);
	assert(t->cs.number == 0);
	assert(t->cs.name == NULL);
	assert(t->cs.nargs == 0);
	for (i = 0; i < nitems(t->cs.s_args); i++)
		assert(t->cs.s_args[i] == NULL);
	memset(t->cs.args, 0, sizeof(t->cs.args));
	t->cs.number = pl->pl_syscall_code;
	t->in_syscall = 1;
}

static void
free_syscall(struct threadinfo *t)
{
	u_int i;

	for (i = 0; i < t->cs.nargs; i++)
		free(t->cs.s_args[i]);
	memset(&t->cs, 0, sizeof(t->cs));
	t->in_syscall = 0;
}

static void
enter_syscall(struct trussinfo *info, struct ptrace_lwpinfo *pl)
{
	struct threadinfo *t;
	struct syscall *sc;
	u_int i, narg;

	t = info->curthread;
	alloc_syscall(t, pl);
	narg = MIN(pl->pl_syscall_narg, nitems(t->cs.args));
	if (narg != 0 && t->proc->abi->fetch_args(info, narg) != 0) {
		free_syscall(t);
		return;
	}

	if (t->cs.number >= 0 && t->cs.number < t->proc->abi->nsyscalls)
		t->cs.name = t->proc->abi->syscallnames[t->cs.number];
	if (t->cs.name == NULL)
		fprintf(info->outfile, "-- UNKNOWN %s SYSCALL %d --\n",
		    t->proc->abi->type, t->cs.number);

	sc = get_syscall(t->cs.name, narg);
	t->cs.nargs = sc->nargs;
	assert(sc->nargs <= nitems(t->cs.s_args));

	t->cs.sc = sc;

	/*
	 * At this point, we set up the system call arguments.
	 * We ignore any OUT ones, however -- those are arguments that
	 * are set by the system call, and so are probably meaningless
	 * now.	This doesn't currently support arguments that are
	 * passed in *and* out, however.
	 */
	if (t->cs.name != NULL) {
#if DEBUG
		fprintf(stderr, "syscall %s(", t->cs.name);
#endif
		for (i = 0; i < t->cs.nargs; i++) {
#if DEBUG
			fprintf(stderr, "0x%lx%s", sc ?
			    t->cs.args[sc->args[i].offset] : t->cs.args[i],
			    i < (t->cs.nargs - 1) ? "," : "");
#endif
			if (!(sc->args[i].type & OUT)) {
				t->cs.s_args[i] = print_arg(&sc->args[i],
				    t->cs.args, 0, info);
			}
		}
#if DEBUG
		fprintf(stderr, ")\n");
#endif
	}

	clock_gettime(CLOCK_REALTIME, &t->before);
}

static void
exit_syscall(struct trussinfo *info, struct ptrace_lwpinfo *pl)
{
	struct threadinfo *t;
	struct procinfo *p;
	struct syscall *sc;
	long retval[2];
	u_int i;
	int errorp;

	t = info->curthread;
	if (!t->in_syscall)
		return;

	clock_gettime(CLOCK_REALTIME, &t->after);
	p = t->proc;
	if (p->abi->fetch_retval(info, retval, &errorp) < 0) {
		free_syscall(t);
		return;
	}

	sc = t->cs.sc;
	/*
	 * Here, we only look for arguments that have OUT masked in --
	 * otherwise, they were handled in enter_syscall().
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
				    t->cs.args[sc->args[i].offset]);
			} else {
				temp = print_arg(&sc->args[i],
				    t->cs.args, retval, info);
			}
			t->cs.s_args[i] = temp;
		}
	}

	print_syscall_ret(info, errorp, retval);
	free_syscall(t);

	/*
	 * If the process executed a new image, check the ABI.  If the
	 * new ABI isn't supported, stop tracing this process.
	 */
	if (pl->pl_flags & PL_FLAG_EXEC) {
		p->abi = find_abi(p->pid);
		if (p->abi == NULL) {
			if (ptrace(PT_DETACH, p->pid, (caddr_t)1, 0) < 0)
				err(1, "Can not detach the process");
			free_proc(p);
		}
	}
}

int
print_line_prefix(struct trussinfo *info)
{
	struct timespec timediff;
	struct threadinfo *t;
	int len;

	len = 0;
	t = info->curthread;
	if (info->flags & (FOLLOWFORKS | DISPLAYTIDS)) {
		if (info->flags & FOLLOWFORKS)
			len += fprintf(info->outfile, "%5d", t->proc->pid);
		if ((info->flags & (FOLLOWFORKS | DISPLAYTIDS)) ==
		    (FOLLOWFORKS | DISPLAYTIDS))
			len += fprintf(info->outfile, " ");
		if (info->flags & DISPLAYTIDS)
			len += fprintf(info->outfile, "%6d", t->tid);
		len += fprintf(info->outfile, ": ");
	}
	if (info->flags & ABSOLUTETIMESTAMPS) {
		timespecsubt(&t->after, &info->start_time, &timediff);
		len += fprintf(info->outfile, "%jd.%09ld ",
		    (intmax_t)timediff.tv_sec, timediff.tv_nsec);
	}
	if (info->flags & RELATIVETIMESTAMPS) {
		timespecsubt(&t->after, &t->before, &timediff);
		len += fprintf(info->outfile, "%jd.%09ld ",
		    (intmax_t)timediff.tv_sec, timediff.tv_nsec);
	}
	return (len);
}

static void
report_exit(struct trussinfo *info, siginfo_t *si)
{
	struct threadinfo *t;

	t = info->curthread;
	clock_gettime(CLOCK_REALTIME, &t->after);
	print_line_prefix(info);
	if (si->si_code == CLD_EXITED)
		fprintf(info->outfile, "process exit, rval = %u\n",
		    si->si_status);
	else
		fprintf(info->outfile, "process killed, signal = %u%s\n",
		    si->si_status, si->si_code == CLD_DUMPED ?
		    " (core dumped)" : "");
}

static void
report_new_child(struct trussinfo *info)
{
	struct threadinfo *t;

	t = info->curthread;
	clock_gettime(CLOCK_REALTIME, &t->after);
	t->before = t->after;
	print_line_prefix(info);
	fprintf(info->outfile, "<new process>\n");
}

static void
report_signal(struct trussinfo *info, siginfo_t *si)
{
	struct threadinfo *t;
	char *signame;

	t = info->curthread;
	clock_gettime(CLOCK_REALTIME, &t->after);
	print_line_prefix(info);
	signame = strsig(si->si_status);
	fprintf(info->outfile, "SIGNAL %u (%s)\n", si->si_status,
	    signame == NULL ? "?" : signame);
}

/*
 * Wait for events until all the processes have exited or truss has been
 * asked to stop.
 */
void
eventloop(struct trussinfo *info)
{
	struct ptrace_lwpinfo pl;
	siginfo_t si;
	int pending_signal;

	while (!LIST_EMPTY(&info->proclist)) {
		if (detaching) {
			detach_all_procs(info);
			return;
		}

		if (waitid(P_ALL, 0, &si, WTRAPPED | WEXITED) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "Unexpected error from waitid");
		}

		assert(si.si_signo == SIGCHLD);

		switch (si.si_code) {
		case CLD_EXITED:
		case CLD_KILLED:
		case CLD_DUMPED:
			find_exit_thread(info, si.si_pid);
			if ((info->flags & COUNTONLY) == 0)
				report_exit(info, &si);
			free_proc(info->curthread->proc);
			info->curthread = NULL;
			break;
		case CLD_TRAPPED:
			if (ptrace(PT_LWPINFO, si.si_pid, (caddr_t)&pl,
			    sizeof(pl)) == -1)
				err(1, "ptrace(PT_LWPINFO)");

			if (pl.pl_flags & PL_FLAG_CHILD) {
				new_proc(info, si.si_pid);
				assert(LIST_FIRST(&info->proclist)->abi !=
				    NULL);
			}
			find_thread(info, si.si_pid, pl.pl_lwpid);

			if (si.si_status == SIGTRAP &&
			    (pl.pl_flags & (PL_FLAG_SCE|PL_FLAG_SCX)) != 0) {
				if (pl.pl_flags & PL_FLAG_SCE)
					enter_syscall(info, &pl);
				else if (pl.pl_flags & PL_FLAG_SCX)
					exit_syscall(info, &pl);
				pending_signal = 0;
			} else if (pl.pl_flags & PL_FLAG_CHILD) {
				if ((info->flags & COUNTONLY) == 0)
					report_new_child(info);
				pending_signal = 0;
			} else {
				if ((info->flags & NOSIGS) == 0)
					report_signal(info, &si);
				pending_signal = si.si_status;
			}
			ptrace(PT_SYSCALL, si.si_pid, (caddr_t)1,
			    pending_signal);
			break;
		case CLD_STOPPED:
			errx(1, "waitid reported CLD_STOPPED");
		case CLD_CONTINUED:
			break;
		}
	}
}
