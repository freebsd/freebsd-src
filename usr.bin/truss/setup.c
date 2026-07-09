/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
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

/*
 * Various setup functions for truss.  Not the cleanest-written code,
 * I'm afraid.
 */

#include <sys/capsicum.h>
#include <sys/event.h>
#include <sys/ptrace.h>
#include <sys/procdesc.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysdecode.h>
#include <time.h>
#include <unistd.h>

#include "truss.h"
#include "syscall.h"
#include "extern.h"

#define	WFLAGS	(WTRAPPED | WEXITED | WCONTINUED | WUNTRACED)

struct procabi_table {
	const char *name;
	struct procabi *abi;
};

static sig_atomic_t detaching;

static void	enter_syscall(struct trussinfo *, struct threadinfo *,
		    struct ptrace_lwpinfo *);
static bool	new_proc(struct trussinfo *, pid_t, lwpid_t, int, bool, bool);
static void	new_proc_register_kev(struct trussinfo *info, int pfd);
static struct procinfo *find_proc(struct trussinfo *info, pid_t pid);

static struct procabi freebsd = {
	.type = "FreeBSD",
	.abi = SYSDECODE_ABI_FREEBSD,
	.pointer_size = sizeof(void *),
	.extra_syscalls = STAILQ_HEAD_INITIALIZER(freebsd.extra_syscalls),
	.syscalls = { NULL }
};

#if !defined(__SIZEOF_POINTER__)
#error "Use a modern compiler."
#endif

#if __SIZEOF_POINTER__ > 4
static struct procabi freebsd32 = {
	.type = "FreeBSD32",
	.abi = SYSDECODE_ABI_FREEBSD32,
	.pointer_size = sizeof(uint32_t),
	.compat_prefix = "freebsd32_",
	.extra_syscalls = STAILQ_HEAD_INITIALIZER(freebsd32.extra_syscalls),
	.syscalls = { NULL }
};
#endif

static struct procabi linux = {
	.type = "Linux",
	.abi = SYSDECODE_ABI_LINUX,
	.pointer_size = sizeof(void *),
	.extra_syscalls = STAILQ_HEAD_INITIALIZER(linux.extra_syscalls),
	.syscalls = { NULL }
};

#if __SIZEOF_POINTER__ > 4
static struct procabi linux32 = {
	.type = "Linux32",
	.abi = SYSDECODE_ABI_LINUX32,
	.pointer_size = sizeof(uint32_t),
	.extra_syscalls = STAILQ_HEAD_INITIALIZER(linux32.extra_syscalls),
	.syscalls = { NULL }
};
#endif

static struct procabi_table abis[] = {
#if __SIZEOF_POINTER__ == 4
	{ "FreeBSD ELF32", &freebsd },
#elif __SIZEOF_POINTER__ == 8
	{ "FreeBSD ELF64", &freebsd },
	{ "FreeBSD ELF32", &freebsd32 },
#else
#error "Unsupported pointer size"
#endif
#if defined(__powerpc64__)
	{ "FreeBSD ELF64 V2", &freebsd },
#endif
#if defined(__amd64__)
	{ "FreeBSD a.out", &freebsd32 },
#endif
#if defined(__i386__)
	{ "FreeBSD a.out", &freebsd },
#endif
#if __SIZEOF_POINTER__ >= 8
	{ "Linux ELF64", &linux },
	{ "Linux ELF32", &linux32 },
#else
	{ "Linux ELF32", &linux },
#endif
};

int
truss_ptrace(struct trussinfo *info, int req, struct procinfo *p, void *addr,
    int data)
{
	if (info->cap_mode)
		return (pdptrace(req, p->pfd, -1, addr, data));
	return (ptrace(req, p->pid, addr, data));
}

static int
truss_ptrace_lwp(struct trussinfo *info, int req, struct procinfo *p,
    lwpid_t lwpid, void *addr, int data)
{
	if (info->cap_mode)
		return (pdptrace(req, p->pfd, lwpid, addr, data));
	return (ptrace(req, lwpid, addr, data));
}

static int
t_wait(struct trussinfo *info, int pid, int *status, int wflags)
{
	if (info->cap_mode)
		return (pdwait(pid, status, wflags, NULL, NULL));
	return (waitpid(pid, status, wflags));
}

static int
truss_wait(struct trussinfo *info, struct procinfo *p, int *status,
    int wflags)
{
	if (info->cap_mode)
		return (pdwait(p->pfd, status, wflags, NULL, NULL));
	return (waitpid(p->pid, status, wflags));
}

int
truss_kill(struct trussinfo *info, struct procinfo *p, int sig)
{
	if (info->cap_mode)
		return (pdkill(p->pfd, sig));
	return (kill(p->pid, sig));
}

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
	int fd, res;

	if (info->cap_mode) {
		pid = pdfork(&fd, PD_DAEMON | PD_CLOEXEC | PD_PTRACE_CAP);
		if (pid == -1)
			err(1, "fork failed");
	} else {
		pid = vfork();
		fd = -1;
	}
	if (pid == 0) {	/* Child */
		ptrace(PT_TRACE_ME, 0, 0, 0);
		execvp(command[0], command);
		err(1, "execvp %s", command[0]);
	}

	if (info->cap_mode) {
		if (caph_enter() == -1)
			err(1, "cap_enter");
		new_proc_register_kev(info, fd);
	}

	/* Only in the parent here */
	res = t_wait(info, info->cap_mode ? fd : pid, NULL, WFLAGS);
	if (res < 0)
		err(1, "unexpected stop in waitpid");

	new_proc(info, pid, 0, fd, false, false);
}

/*
 * start_tracing is called to attach to an existing process.
 */
void
start_tracing(struct trussinfo *info, pid_t pid)
{
	int fd, ret, retry;

	if (info->cap_mode) {
		fd = pdopenpid(pid, PD_DAEMON | PD_CLOEXEC | PD_PTRACE_CAP);
		if (fd == -1)
			err(1, "Cannot open the target process");
		if (caph_enter() == -1)
			err(1, "cap_enter");
		new_proc_register_kev(info, fd);
	} else {
		fd = -1;
	}

	retry = 10;
	do {
		ret = info->cap_mode ? pdptrace(PT_ATTACH, fd, -1,
		    NULL, 0) : ptrace(PT_ATTACH, pid, NULL, 0);
		usleep(200);
	} while (ret && retry-- > 0);
	if (ret)
		err(1, "Cannot attach to target process");

	ret = t_wait(info, info->cap_mode ? fd : pid, NULL, WFLAGS);
	if (ret < 0)
		err(1, "Unexpected stop in waitpid");

	new_proc(info, pid, 0, fd, false, false);
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
detach_proc(struct trussinfo *info, struct procinfo *p)
{
	int error, sig, status;

	/*
	 * Stop the child so that we can detach.  Filter out possible
	 * lingering SIGTRAP events buffered in the threads.
	 */
	truss_kill(info, p, SIGSTOP);
	for (;;) {
		error = truss_wait(info, p, &status, WFLAGS);
		if (error < 0)
			err(1, "Unexpected error in waitpid");
		sig = WIFSTOPPED(status) ? WSTOPSIG(status) : 0;
		if (sig == SIGSTOP)
			break;
		if (sig == SIGTRAP)
			sig = 0;
		if (truss_ptrace(info, PT_CONTINUE, p, (caddr_t)1, sig) < 0)
			err(1, "Can not continue for detach");
	}

	if (truss_ptrace(info, PT_DETACH, p, (caddr_t)1, 0) < 0)
		err(1, "Can not detach the process");

	truss_kill(info, p, SIGCONT);
}

/*
 * Determine the ABI.  This is called after every exec, and when
 * a process is first monitored.
 */
static struct procabi *
find_abi(struct trussinfo *info, struct procinfo *p)
{
	unsigned i;
	char progt[32];

	if (truss_ptrace(info, PT_GET_ABI_NAME, p, progt,
	    sizeof(progt)) == -1) {
		warn("cannot get ABI for proc %ld", (long)p->pid);
		return (NULL);
	}

	for (i = 0; i < nitems(abis); i++) {
		if (strcmp(abis[i].name, progt) == 0)
			return (abis[i].abi);
	}
	warnx("ABI %s for pid %ld is not supported", progt, (long)p->pid);
	return (NULL);
}

static struct threadinfo *
new_thread(struct procinfo *p, lwpid_t lwpid)
{
	struct threadinfo *nt;

	/*
	 * If this happens it means there is a bug in truss.  Unfortunately
	 * this will kill any processes truss is attached to.
	 */
	LIST_FOREACH(nt, &p->threadlist, entries) {
		if (nt->tid == lwpid)
			errx(1, "Duplicate thread for LWP %ld", (long)lwpid);
	}

	nt = calloc(1, sizeof(struct threadinfo));
	if (nt == NULL)
		err(1, "calloc() failed");
	nt->proc = p;
	nt->tid = lwpid;
	LIST_INSERT_HEAD(&p->threadlist, nt, entries);
	return (nt);
}

static void
free_thread(struct threadinfo *t)
{

	LIST_REMOVE(t, entries);
	free(t);
}

static void
add_threads(struct trussinfo *info, struct procinfo *p)
{
	struct ptrace_lwpinfo pl;
	struct threadinfo *t;
	lwpid_t *lwps;
	int i, nlwps;

	nlwps = truss_ptrace(info, PT_GETNUMLWPS, p, NULL, 0);
	if (nlwps == -1)
		err(1, "Unable to fetch number of LWPs");
	assert(nlwps > 0);
	lwps = calloc(nlwps, sizeof(*lwps));
	nlwps = truss_ptrace(info, PT_GETLWPLIST, p, lwps, nlwps);
	if (nlwps == -1)
		err(1, "Unable to fetch LWP list");
	for (i = 0; i < nlwps; i++) {
		t = new_thread(p, lwps[i]);
		if (truss_ptrace_lwp(info, PT_LWPINFO, p, lwps[i], &pl,
		    sizeof(pl)) == -1)
			err(1, "ptrace(PT_LWPINFO)");
		if (pl.pl_flags & PL_FLAG_SCE) {
			info->curthread = t;
			enter_syscall(info, t, &pl);
		}
	}
	free(lwps);
}

static void
new_proc_register_kev(struct trussinfo *info, int pfd)
{
	struct kevent ev[1];
	int error;

	if (!info->cap_mode)
		return;
	EV_SET(&ev[0], pfd, EVFILT_PROCDESC, EV_ADD, NOTE_EXIT |
	    NOTE_PDSIGCHLD | NOTE_FORK, 0, 0);
	error = kevent(info->pdkq, ev, nitems(ev), NULL, 0, NULL);
	if (error == -1)
		err(1, "Unable to register pfd %d for notifications", pfd);
}

static bool
new_proc(struct trussinfo *info, pid_t pid, lwpid_t lwpid, int pfd,
    bool allow_known, bool wait_for)
{
	struct procinfo *np;

	if (find_proc(info, pid) != NULL) {
		if (allow_known)
			return (false);

		/*
		 * If this happens it means there is a bug in truss.
		 * Unfortunately this will kill any processes truss is
		 * attached to.
		 */
		errx(1, "Duplicate process for pid %ld", (long)pid);
	}
	if (pfd == -1 && info->cap_mode) {
		pfd = pdopenpid(pid, PD_DAEMON | PD_CLOEXEC | PD_PTRACE_CAP);
		if (pfd == -1)
			err(1, "pdopenpid %d", pid);
		if (wait_for && t_wait(info, pfd, NULL, WFLAGS) < 0)
			err(1, "waitpid on attach to %d", pid);
		new_proc_register_kev(info, pfd);
	}

	np = calloc(1, sizeof(struct procinfo));
	np->pid = pid;
	np->pfd = pfd;
	np->abi = find_abi(info, np);
	np->herald_printed = false;
	if ((info->flags & FOLLOWFORKS) != 0 && truss_ptrace(info,
	    PT_FOLLOW_FORK, np, NULL, 1) == -1)
		err(1, "Unable to follow forks for pid %ld", (long)pid);
	if (truss_ptrace(info, PT_LWP_EVENTS, np, NULL, 1) == -1)
		err(1, "Unable to enable LWP events for pid %ld", (long)pid);
	LIST_INIT(&np->threadlist);
	LIST_INIT(&np->fdlist);
	LIST_INSERT_HEAD(&info->proclist, np, entries);

	if (lwpid != 0)
		new_thread(np, lwpid);
	else
		add_threads(info, np);
	return (true);
}

static void
free_proc(struct trussinfo *info, struct procinfo *p)
{
	struct threadinfo *t, *t2;
	struct fd_domain *f, *f2;
	struct kevent ev[1];

	if (info->cap_mode) {
		EV_SET(&ev[0], p->pfd, EVFILT_PROCDESC, EV_DELETE, 0, 0, 0);
		(void)kevent(info->pdkq, ev, nitems(ev), NULL, 0, 0);
		close(p->pfd);
	}

	LIST_FOREACH_SAFE(t, &p->threadlist, entries, t2) {
		free(t);
	}

	LIST_FOREACH_SAFE(f, &p->fdlist, entries, f2) {
		free(f);
	}

	LIST_REMOVE(p, entries);
	free(p);
}

static void
detach_all_procs(struct trussinfo *info)
{
	struct procinfo *p, *p2;

	LIST_FOREACH_SAFE(p, &info->proclist, entries, p2) {
		detach_proc(info, p);
		free_proc(info, p);
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
 */
static void
find_thread(struct trussinfo *info, pid_t pid, lwpid_t lwpid)
{
	struct procinfo *np;
	struct threadinfo *nt;

	np = find_proc(info, pid);
	assert(np != NULL);

	LIST_FOREACH(nt, &np->threadlist, entries) {
		if (nt->tid == lwpid) {
			info->curthread = nt;
			return;
		}
	}
	errx(1, "could not find thread");
}

/*
 * When a process exits, it should have exactly one thread left.
 * All of the other threads should have reported thread exit events.
 */
static void
find_exit_thread(struct trussinfo *info, pid_t pid)
{
	struct procinfo *p;

	p = find_proc(info, pid);
	assert(p != NULL);

	info->curthread = LIST_FIRST(&p->threadlist);
	assert(info->curthread != NULL);
	assert(LIST_NEXT(info->curthread, entries) == NULL);
}

static void
alloc_syscall(struct threadinfo *t, struct ptrace_lwpinfo *pl)
{
	u_int i;

	assert(t->in_syscall == 0);
	assert(t->cs.number == 0);
	assert(t->cs.sc == NULL);
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
enter_syscall(struct trussinfo *info, struct threadinfo *t,
    struct ptrace_lwpinfo *pl)
{
	struct syscall *sc;
	u_int i, narg;

	alloc_syscall(t, pl);
	narg = MIN(pl->pl_syscall_narg, nitems(t->cs.args));
	if (narg != 0 && truss_ptrace_lwp(info, PT_GET_SC_ARGS, t->proc,
	    t->tid, (caddr_t)t->cs.args, sizeof(t->cs.args)) != 0) {
		free_syscall(t);
		return;
	}

	sc = get_syscall(t, t->cs.number, narg);
	if (sc->unknown && sc->trace)
		fprintf(info->outfile, "-- UNKNOWN %s SYSCALL %d --\n",
		    t->proc->abi->type, t->cs.number);

	t->cs.nargs = sc->decode.nargs;
	assert(sc->decode.nargs <= nitems(t->cs.s_args));

	t->cs.sc = sc;

	/*
	 * A system call excluded by -t is never printed, so there is no
	 * point in formatting its arguments.
	 */
	if (!sc->trace) {
		clock_gettime(CLOCK_REALTIME, &t->before);
		return;
	}

	/*
	 * At this point, we set up the system call arguments.
	 * We ignore any OUT ones, however -- those are arguments that
	 * are set by the system call, and so are probably meaningless
	 * now.	This doesn't currently support arguments that are
	 * passed in *and* out, however.
	 */
#if DEBUG
	fprintf(stderr, "syscall %s(", sc->name);
#endif
	for (i = 0; i < t->cs.nargs; i++) {
#if DEBUG
		fprintf(stderr, "0x%lx%s",
		    t->cs.args[sc->decode.args[i].offset],
		    i < (t->cs.nargs - 1) ? "," : "");
#endif
		if (!(sc->decode.args[i].type & OUT)) {
			t->cs.s_args[i] = print_arg(&sc->decode.args[i],
			    t->cs.args, NULL, info, &sc->decode);
		}
	}
#if DEBUG
	fprintf(stderr, ")\n");
#endif

	clock_gettime(CLOCK_REALTIME, &t->before);
}

/*
 * When a thread exits voluntarily (including when a thread calls
 * exit() to trigger a process exit), the thread's internal state
 * holds the arguments passed to the exit system call.  When the
 * thread's exit is reported, log that system call without a return
 * value.
 */
static void
thread_exit_syscall(struct trussinfo *info)
{
	struct threadinfo *t;

	t = info->curthread;
	if (!t->in_syscall)
		return;

	clock_gettime(CLOCK_REALTIME, &t->after);

	print_syscall_ret(info, 0, NULL);
	free_syscall(t);
}

static void
exit_syscall(struct trussinfo *info, struct ptrace_lwpinfo *pl)
{
	struct threadinfo *t;
	struct procinfo *p;
	struct syscall *sc;
	struct ptrace_sc_ret psr;
	u_int i;

	t = info->curthread;
	if (!t->in_syscall)
		return;

	clock_gettime(CLOCK_REALTIME, &t->after);
	p = t->proc;
	if (truss_ptrace_lwp(info, PT_GET_SC_RET, p, t->tid, &psr,
	    sizeof(psr)) != 0) {
		free_syscall(t);
		return;
	}

	sc = t->cs.sc;
	/*
	 * Here, we only look for arguments that have OUT masked in --
	 * otherwise, they were handled in enter_syscall().  A system call
	 * excluded by -t is never printed, so none of them are needed.
	 */
	for (i = 0; i < sc->decode.nargs && sc->trace; i++) {
		char *temp;

		if (sc->decode.args[i].type & OUT) {
			/*
			 * If an error occurred, then don't bother
			 * getting the data; it may not be valid.
			 */
			if (psr.sr_error != 0) {
				asprintf(&temp, "0x%lx",
				    (long)t->cs.args[sc->decode.args[i].offset]);
			} else {
				temp = print_arg(&sc->decode.args[i],
				    t->cs.args, psr.sr_retval, info,
				    &sc->decode);
			}
			t->cs.s_args[i] = temp;
		}
	}

	/*
	 * Track successfully created sockets so later syscalls using the
	 * returned file descriptor can be identified by socket domain.
	 */
	if (strcmp(sc->name, "socket") == 0 &&
	    psr.sr_error == 0) {

		struct fd_domain *f = calloc(1, sizeof(*f));

		f->fd = (int)psr.sr_retval[0];
		f->domain = (int)t->cs.args[0];
		f->protocol = (int)t->cs.args[2];

		LIST_INSERT_HEAD(&p->fdlist, f, entries);
	}

	if (strcmp(sc->name, "close") == 0 &&
	    psr.sr_error == 0) {

		struct fd_domain *f, *f2;

		LIST_FOREACH_SAFE(f, &p->fdlist, entries, f2) {
			if (f->fd == (int)t->cs.args[0]) {
				LIST_REMOVE(f, entries);
				free(f);
			}
		}
	}

	print_syscall_ret(info, psr.sr_error, psr.sr_retval);
	free_syscall(t);

	/*
	 * If the process executed a new image, check the ABI.  If the
	 * new ABI isn't supported, stop tracing this process.
	 */
	if (pl->pl_flags & PL_FLAG_EXEC) {
		assert(LIST_NEXT(LIST_FIRST(&p->threadlist), entries) == NULL);
		p->abi = find_abi(info, p);
		if (p->abi == NULL) {
			if (truss_ptrace(info, PT_DETACH, p, (caddr_t)1, 0) < 0)
				err(1, "Can not detach the process");
			free_proc(info, p);
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
		timespecsub(&t->after, &info->start_time, &timediff);
		len += fprintf(info->outfile, "%jd.%09ld ",
		    (intmax_t)timediff.tv_sec, timediff.tv_nsec);
	}
	if (info->flags & RELATIVETIMESTAMPS) {
		timespecsub(&t->after, &t->before, &timediff);
		len += fprintf(info->outfile, "%jd.%09ld ",
		    (intmax_t)timediff.tv_sec, timediff.tv_nsec);
	}
	return (len);
}

static void
report_thread_death(struct trussinfo *info)
{
	struct threadinfo *t;

	t = info->curthread;
	clock_gettime(CLOCK_REALTIME, &t->after);
	print_line_prefix(info);
	fprintf(info->outfile, "<thread %ld exited>\n", (long)t->tid);
}

static void
report_thread_birth(struct trussinfo *info)
{
	struct threadinfo *t;

	t = info->curthread;
	clock_gettime(CLOCK_REALTIME, &t->after);
	t->before = t->after;
	print_line_prefix(info);
	fprintf(info->outfile, "<new thread %ld>\n", (long)t->tid);
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
	if (t->proc->herald_printed)
		return;
	t->proc->herald_printed = true;
	clock_gettime(CLOCK_REALTIME, &t->after);
	t->before = t->after;
	print_line_prefix(info);
	fprintf(info->outfile, "<new process>\n");
}

void
decode_siginfo(FILE *fp, siginfo_t *si)
{
	const char *str;

	fprintf(fp, " code=");
	str = sysdecode_sigcode(si->si_signo, si->si_code);
	if (str == NULL)
		fprintf(fp, "%d", si->si_code);
	else
		fprintf(fp, "%s", str);
	switch (si->si_code) {
	case SI_NOINFO:
		break;
	case SI_QUEUE:
		fprintf(fp, " value=%p", si->si_value.sival_ptr);
		/* FALLTHROUGH */
	case SI_USER:
	case SI_LWP:
		fprintf(fp, " pid=%jd uid=%jd", (intmax_t)si->si_pid,
		    (intmax_t)si->si_uid);
		break;
	case SI_TIMER:
		fprintf(fp, " value=%p", si->si_value.sival_ptr);
		fprintf(fp, " timerid=%d", si->si_timerid);
		fprintf(fp, " overrun=%d", si->si_overrun);
		if (si->si_errno != 0)
			fprintf(fp, " errno=%d", si->si_errno);
		break;
	case SI_ASYNCIO:
		fprintf(fp, " value=%p", si->si_value.sival_ptr);
		break;
	case SI_MESGQ:
		fprintf(fp, " value=%p", si->si_value.sival_ptr);
		fprintf(fp, " mqd=%d", si->si_mqd);
		break;
	default:
		switch (si->si_signo) {
		case SIGILL:
		case SIGFPE:
		case SIGSEGV:
		case SIGBUS:
			fprintf(fp, " trapno=%d", si->si_trapno);
			fprintf(fp, " addr=%p", si->si_addr);
			break;
		case SIGCHLD:
			fprintf(fp, " pid=%jd uid=%jd", (intmax_t)si->si_pid,
			    (intmax_t)si->si_uid);
			fprintf(fp, " status=%d", si->si_status);
			break;
		}
	}
}

static void
report_signal(struct trussinfo *info, siginfo_t *si, struct ptrace_lwpinfo *pl)
{
	struct threadinfo *t;
	const char *signame;

	t = info->curthread;
	clock_gettime(CLOCK_REALTIME, &t->after);
	print_line_prefix(info);
	signame = sysdecode_signal(si->si_status);
	if (signame == NULL)
		signame = "?";
	fprintf(info->outfile, "SIGNAL %u (%s)", si->si_status, signame);
	if (pl->pl_event == PL_EVENT_SIGNAL && pl->pl_flags & PL_FLAG_SI)
		decode_siginfo(info->outfile, &pl->pl_siginfo);
	fprintf(info->outfile, "\n");
	
}

static void
eventloop_handle_trapped(struct trussinfo *info, pid_t si_pid, int si_status,
    siginfo_t *si)
{
	struct procinfo *np;
	struct ptrace_lwpinfo pl;
	int pending_signal;

	np = find_proc(info, si_pid);
	if (np == NULL) {
		new_proc(info, si_pid, 0, -1, true, false);
		np = find_proc(info, si_pid);
	}
	if (truss_ptrace(info, PT_LWPINFO, np, &pl, sizeof(pl)) == -1)
		err(1, "ptrace(PT_LWPINFO)");

	if ((pl.pl_flags & PL_FLAG_CHILD) != 0) {
		assert(LIST_FIRST(&info->proclist)->abi != NULL);
	} else if ((pl.pl_flags & PL_FLAG_BORN) != 0) {
		new_thread(np, pl.pl_lwpid);
	}
	find_thread(info, si_pid, pl.pl_lwpid);

	pending_signal = 0;
	if (si_status == SIGTRAP && (pl.pl_flags & (PL_FLAG_BORN |
	    PL_FLAG_EXITED | PL_FLAG_SCE | PL_FLAG_SCX)) != 0) {
		if ((pl.pl_flags & PL_FLAG_BORN) != 0) {
			if ((info->flags & COUNTONLY) == 0)
				report_thread_birth(info);
		} else if ((pl.pl_flags & PL_FLAG_EXITED) != 0) {
			if ((info->flags & COUNTONLY) == 0)
				report_thread_death(info);
			free_thread(info->curthread);
			info->curthread = NULL;
		} else if ((pl.pl_flags & PL_FLAG_SCE) != 0) {
			enter_syscall(info, info->curthread, &pl);
		} else if ((pl.pl_flags & PL_FLAG_SCX) != 0) {
			exit_syscall(info, &pl);
		}
	} else if ((pl.pl_flags & PL_FLAG_CHILD) != 0) {
		if ((info->flags & COUNTONLY) == 0)
			report_new_child(info);
	} else if (si != NULL) {
		if ((info->flags & NOSIGS) == 0)
			report_signal(info, si, &pl);
		pending_signal = si->si_status;
	}
	if (truss_ptrace(info, PT_SYSCALL, np, (caddr_t)1,
	    pending_signal) == -1)
		err(1, "ptrace(PT_SYSCALL)");
}

static void
eventloop_handle_note_fork(struct trussinfo *info)
{
	struct ptrace_child *ptcs;
	int cnt, i;

again:
	cnt = ptrace(PT_GET_CHILDREN, getpid(), NULL, 0);
	if (cnt == -1)
		err(1, "Unexpected error from ptrace(PT_GET_CHILDREN) size");
	if (cnt == 0)
		return;
	ptcs = calloc(cnt, sizeof(*ptcs));
	if (ptcs == NULL)
		err(1, "No memory");
	cnt = ptrace(PT_GET_CHILDREN, getpid(), (caddr_t)ptcs,
	    cnt * sizeof(*ptcs));
	if (cnt == -1) {
		if (errno == ENOMEM) {
			free(ptcs);
			goto again;
		}
		err(1, "Unexpected error from ptrace(PT_GET_CHILDREN) data");
	}
	for (i = 0; i < cnt; i++) {
		if ((ptcs[i].flags & (PTCHLD_TRACED | PTCHLD_TRACED_BY_ME |
		    PTCHLD_EXITED)) != (PTCHLD_TRACED | PTCHLD_TRACED_BY_ME))
			continue;
		if (new_proc(info, ptcs[i].pid, 0, -1, true, true)) {
			if ((info->flags & COUNTONLY) == 0)
				report_new_child(info);
			eventloop_handle_trapped(info, ptcs[i].pid, SIGTRAP,
			    NULL);
		}
	}
	free(ptcs);
}

/*
 * Wait for events until all the processes have exited or truss has been
 * asked to stop.
 */
void
eventloop(struct trussinfo *info)
{
	siginfo_t si;
	struct kevent ev[1];
	int cnt, error;
	bool has_si;

	while (!LIST_EMPTY(&info->proclist)) {
		if (detaching) {
			detach_all_procs(info);
			return;
		}

		has_si = false;
		if (info->cap_mode) {
			cnt = kevent(info->pdkq, NULL, 0, ev, nitems(ev),
			    NULL);
			if (cnt == -1) {
				if (errno == EINTR)
					continue;
				err(1, "Unexpected error from kevent");
			}
			if (cnt == 0) {
				/* XXXKIB ? */
				continue;
			}
			if ((ev[0].fflags & (NOTE_EXIT | NOTE_PDSIGCHLD)) !=
			    0) {
				error = pdwait(ev[0].ident, NULL,
				    WFLAGS | WNOHANG, NULL, &si);
				if (error == -1) {
					if (errno == EINTR ||
					    errno == EWOULDBLOCK)
						continue;
					err(1, "Unexpected error from pdwait");
				}
				has_si = true;

				/*
				 * To get rid of zombie, we need to
				 * waitpid() on it in addition to the
				 * pdwait() above, because we are the
				 * debugger, and the child was
				 * reparented to us.
				 */
				waitpid(si.si_pid, NULL, WEXITED | WNOHANG);
			}
			if ((ev[0].fflags & NOTE_FORK) != 0)
				eventloop_handle_note_fork(info);
		} else {
			if (waitid(P_ALL, 0, &si, WTRAPPED | WEXITED) == -1) {
				if (errno == EINTR)
					continue;
				err(1, "Unexpected error from waitid");
			}
			has_si = true;
		}
		if (!has_si)
			continue;

		assert(si.si_signo == SIGCHLD);

		switch (si.si_code) {
		case CLD_EXITED:
		case CLD_KILLED:
		case CLD_DUMPED:
			find_exit_thread(info, si.si_pid);
			if ((info->flags & COUNTONLY) == 0) {
				if (si.si_code == CLD_EXITED)
					thread_exit_syscall(info);
				report_exit(info, &si);
			}
			free_proc(info, info->curthread->proc);
			info->curthread = NULL;
			break;
		case CLD_TRAPPED:
			eventloop_handle_trapped(info, si.si_pid,
			    si.si_status, &si);
			break;
		case CLD_STOPPED:
			errx(1, "waitid reported CLD_STOPPED");
		case CLD_CONTINUED:
			break;
		}
	}
}
