/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 ConnectWise
 * Copyright (c) 2026 Mark Johnston <markj@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/types.h>
#include <sys/capsicum.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/procdesc.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <machine/atomic.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>
#include <kvm.h>

/* Tests for procdesc(4) that aren't specific to any one syscall */

/*
 * Block until a thread in the specified process is sleeping in the specified
 * wait message.
 */
static void
wait_for_naptime(pid_t pid, const char *wmesg)
{
	kvm_t *kd;
	int count;

	kd = kvm_openfiles(NULL, "/dev/null", NULL, O_RDONLY, NULL);
	ATF_REQUIRE(kd != NULL);
	for (;;) {
		struct kinfo_proc *kip;
		int i;

		usleep(1000);
		kip = kvm_getprocs(kd, KERN_PROC_PID | KERN_PROC_INC_THREAD,
		    pid, &count);
		ATF_REQUIRE(kip != NULL);
		for (i = 0; i < count; i++) {
			ATF_REQUIRE(kip[i].ki_stat != SZOMB);
			if (kip[i].ki_stat == SSLEEP &&
			    strcmp(kip[i].ki_wmesg, wmesg) == 0)
				break;
		}
		if (i < count)
			break;
	}

	kvm_close(kd);
}

/*
 * Even after waiting on a process descriptor with waitpid(2), the kernel will
 * not recycle the pid until after the process descriptor is closed.  That is
 * important to prevent users from trying to wait() twice, the second time
 * using a dangling pid.
 *
 * Whether this same anti-recycling behavior is used with pdwait() is
 * unimportant, because pdwait _always_ uses a process descriptor.
 */
ATF_TC_WITHOUT_HEAD(pid_recycle);
ATF_TC_BODY(pid_recycle, tc)
{
	size_t len;
	int i, pd, pid_max;
	pid_t dangle_pid;

	len = sizeof(pid_max);
	ATF_REQUIRE_EQ_MSG(0,
	    sysctlbyname("kern.pid_max", &pid_max, &len, NULL, 0),
	    "sysctlbyname: %s", strerror(errno));

	/* Create a process descriptor */
	dangle_pid = pdfork(&pd, PD_CLOEXEC | PD_DAEMON);
	ATF_REQUIRE_MSG(dangle_pid >= 0, "pdfork: %s", strerror(errno));
	if (dangle_pid == 0) {
		// In child
		_exit(0);
	}
	/*
	 * Reap the child, but don't close the pd, creating a dangling pid.
	 * Notably, it isn't a Zombie, because the process is reaped.
	 */
	ATF_REQUIRE_EQ(dangle_pid, waitpid(dangle_pid, NULL, WEXITED));

	/*
	 * Now create and kill pid_max additional children.  Test to see if pid
	 * gets reused.  If not, that means the kernel is correctly reserving
	 * the dangling pid from reuse.
	 */
	for (i = 0; i < pid_max; i++) {
		pid_t pid;

		pid = vfork();
		ATF_REQUIRE_MSG(pid >= 0, "vfork: %s", strerror(errno));
		if (pid == 0)
			_exit(0);
		ATF_REQUIRE_MSG(pid != dangle_pid,
		    "pid got recycled after %d forks", i);
		ATF_REQUIRE_EQ(pid, waitpid(pid, NULL, WEXITED));
	}
	close(pd);
}

static void *
poll_procdesc(void *arg)
{
	struct pollfd pfd;

	pfd.fd = *(int *)arg;
	pfd.events = POLLHUP;
	(void)poll(&pfd, 1, 5000);
	return ((void *)(uintptr_t)pfd.revents);
}

/*
 * Regression test to exercise the case where a procdesc is closed while a
 * thread is poll()ing it.
 */
ATF_TC_WITHOUT_HEAD(poll_close_race);
ATF_TC_BODY(poll_close_race, tc)
{
	pthread_t thr;
	pid_t pid;
	uintptr_t revents;
	int error, pd;

	pid = pdfork(&pd, PD_DAEMON);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork: %s", strerror(errno));
	if (pid == 0) {
		pause();
		_exit(0);
	}

	error = pthread_create(&thr, NULL, poll_procdesc, &pd);
	ATF_REQUIRE_MSG(error == 0, "pthread_create: %s", strerror(error));

	wait_for_naptime(getpid(), "select");

	ATF_REQUIRE_MSG(close(pd) == 0, "close: %s", strerror(errno));

	error = pthread_join(thr, (void *)&revents);
	ATF_REQUIRE_MSG(error == 0, "pthread_join: %s", strerror(error));
	ATF_REQUIRE_EQ(revents, POLLNVAL);
}

/*
 * Verify that poll(2) of a procdesc returns POLLHUP when the process exits.
 */
ATF_TC_WITHOUT_HEAD(poll_exit_wakeup);
ATF_TC_BODY(poll_exit_wakeup, tc)
{
	pthread_t thr;
	uintptr_t revents;
	pid_t pid;
	int error, pd;

	pid = pdfork(&pd, PD_DAEMON);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork: %s", strerror(errno));
	if (pid == 0) {
		pause();
		_exit(0);
	}

	error = pthread_create(&thr, NULL, poll_procdesc, &pd);
	ATF_REQUIRE_MSG(error == 0, "pthread_create: %s", strerror(error));

	wait_for_naptime(getpid(), "select");

	ATF_REQUIRE_MSG(pdkill(pd, SIGKILL) == 0,
	    "pdkill: %s", strerror(errno));

	error = pthread_join(thr, (void *)&revents);
	ATF_REQUIRE_MSG(error == 0, "pthread_join: %s", strerror(error));
	ATF_REQUIRE_EQ(revents, POLLHUP);

	ATF_REQUIRE_MSG(close(pd) == 0, "close: %s", strerror(errno));
}

/* Tests for pdopenpid(2) */

/*
 * Basic: open a process descriptor for a child, verify pdgetpid() returns the
 * right pid.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_basic);
ATF_TC_BODY(pdopenpid_basic, tc)
{
	pid_t child, queried;
	int fd;

	child = fork();
	ATF_REQUIRE_MSG(child >= 0, "fork: %s", strerror(errno));
	if (child == 0) {
		for (;;)
			pause();
		_exit(0);
	}

	fd = pdopenpid(child, 0);
	ATF_REQUIRE_MSG(fd >= 0, "pdopenpid: %s", strerror(errno));

	ATF_REQUIRE_MSG(pdgetpid(fd, &queried) == 0,
	    "pdgetpid: %s", strerror(errno));
	ATF_REQUIRE_EQ(child, queried);

	ATF_REQUIRE_MSG(pdkill(fd, SIGKILL) == 0,
	    "pdkill: %s", strerror(errno));
	ATF_REQUIRE_EQ(child, waitpid(child, NULL, 0));
	ATF_REQUIRE(close(fd) == 0);
}

/*
 * pdopenpid with PD_CLOEXEC should set the close-on-exec flag.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_cloexec);
ATF_TC_BODY(pdopenpid_cloexec, tc)
{
	pid_t child;
	int fd, flags;

	child = fork();
	ATF_REQUIRE_MSG(child >= 0, "fork: %s", strerror(errno));
	if (child == 0) {
		for (;;)
			pause();
		_exit(0);
	}

	fd = pdopenpid(child, PD_CLOEXEC);
	ATF_REQUIRE_MSG(fd >= 0, "pdopenpid: %s", strerror(errno));

	flags = fcntl(fd, F_GETFD);
	ATF_REQUIRE(flags >= 0);
	ATF_REQUIRE(flags & FD_CLOEXEC);

	ATF_REQUIRE_MSG(pdkill(fd, SIGKILL) == 0,
	    "pdkill: %s", strerror(errno));
	ATF_REQUIRE_EQ(child, waitpid(child, NULL, 0));
	ATF_REQUIRE(close(fd) == 0);
}

/*
 * Invalid flags should return EINVAL.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_einval);
ATF_TC_BODY(pdopenpid_einval, tc)
{
	ATF_REQUIRE_ERRNO(EINVAL, pdopenpid(getpid(), 0xdeadbeef) < 0);
	ATF_REQUIRE_ERRNO(EINVAL, pdopenpid(getpid(), ~PD_ALLOWED_AT_FORK) < 0);
}

/*
 * Validate handling of EMFILE.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_emfile);
ATF_TC_BODY(pdopenpid_emfile, tc)
{
	pid_t child;
	struct rlimit rl;
	int fd;

	child = fork();
	ATF_REQUIRE_MSG(child >= 0, "fork: %s", strerror(errno));
	if (child == 0) {
		for (;;)
			pause();
		_exit(0);
	}

	/*
	 * Determine the lowest unused fd, then set the fd limit to that
	 * value so that no new fds can be allocated.
	 */
	fd = dup(STDIN_FILENO);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE(close(fd) == 0);

	ATF_REQUIRE_EQ(getrlimit(RLIMIT_NOFILE, &rl), 0);
	rl.rlim_cur = fd;
	ATF_REQUIRE_EQ(setrlimit(RLIMIT_NOFILE, &rl), 0);

	ATF_REQUIRE_ERRNO(EMFILE, pdopenpid(child, 0) < 0);

	/*
	 * The child should not have been killed or reparented as a side
	 * effect of the failed syscall.
	 */
	ATF_REQUIRE_MSG(kill(child, 0) == 0,
	    "child was killed: %s", strerror(errno));
	ATF_REQUIRE_MSG(waitpid(child, NULL, WNOHANG) == 0,
	    "child was reparented");

	ATF_REQUIRE_MSG(kill(child, SIGKILL) == 0,
	    "kill: %s", strerror(errno));
	ATF_REQUIRE_EQ(child, waitpid(child, NULL, 0));
}

/*
 * Opening a nonexistent pid should return ESRCH.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_esrch);
ATF_TC_BODY(pdopenpid_esrch, tc)
{
	ATF_REQUIRE_ERRNO(ESRCH, pdopenpid(123456789, 0) < 0);
}

/*
 * pdopenpid should fail in capability mode.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_capmode);
ATF_TC_BODY(pdopenpid_capmode, tc)
{
	pid_t child, parent;

	parent = getpid();
	child = fork();
	ATF_REQUIRE_MSG(child >= 0, "fork: %s", strerror(errno));
	if (child == 0) {
		while (getppid() == parent)
			sleep(1);
		_exit(0);
	}

	ATF_REQUIRE_MSG(cap_enter() == 0, "cap_enter: %s", strerror(errno));
	ATF_REQUIRE_ERRNO(ECAPMODE, pdopenpid(child, 0) < 0);
}

/*
 * Open a process descriptor for a child that already has one from pdfork().
 * Both fds should refer to the same process.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_pdfork_then_open);
ATF_TC_BODY(pdopenpid_pdfork_then_open, tc)
{
	pid_t child, pid1, pid2;
	int fd1, fd2;

	child = pdfork(&fd1, PD_DAEMON);
	ATF_REQUIRE_MSG(child >= 0, "pdfork: %s", strerror(errno));
	if (child == 0) {
		for (;;)
			pause();
		_exit(0);
	}

	fd2 = pdopenpid(child, 0);
	ATF_REQUIRE_MSG(fd2 >= 0, "pdopenpid: %s", strerror(errno));

	ATF_REQUIRE(pdgetpid(fd1, &pid1) == 0);
	ATF_REQUIRE(pdgetpid(fd2, &pid2) == 0);
	ATF_REQUIRE_EQ(pid1, pid2);
	ATF_REQUIRE_EQ(child, pid1);

	/* Kill via the second fd, wait via the first. */
	ATF_REQUIRE_MSG(pdkill(fd2, SIGKILL) == 0,
	    "pdkill: %s", strerror(errno));
	ATF_REQUIRE_MSG(pdwait(fd1, NULL, WEXITED, NULL, NULL) == 0,
	    "pdwait: %s", strerror(errno));

	ATF_REQUIRE(close(fd1) == 0);
	ATF_REQUIRE(close(fd2) == 0);
}

/*
 * Open a process descriptor for a child created with fork(), which has no
 * pre-existing procdesc.  Then use pdkill() and pdwait() on it.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_fork_then_open);
ATF_TC_BODY(pdopenpid_fork_then_open, tc)
{
	pid_t child;
	int fd, status;

	child = fork();
	ATF_REQUIRE_MSG(child >= 0, "fork: %s", strerror(errno));
	if (child == 0) {
		for (;;)
			pause();
		_exit(0);
	}

	fd = pdopenpid(child, 0);
	ATF_REQUIRE_MSG(fd >= 0, "pdopenpid: %s", strerror(errno));

	ATF_REQUIRE_MSG(pdkill(fd, SIGKILL) == 0,
	    "pdkill: %s", strerror(errno));
	ATF_REQUIRE_MSG(pdwait(fd, &status, WEXITED, NULL, NULL) == 0,
	    "pdwait: %s", strerror(errno));
	ATF_REQUIRE(WIFSIGNALED(status));
	ATF_REQUIRE_EQ(WTERMSIG(status), SIGKILL);

	ATF_REQUIRE(close(fd) == 0);
}

/*
 * Closing one fd should not kill the process when another fd still references
 * the same procdesc.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_close_one_of_two);
ATF_TC_BODY(pdopenpid_close_one_of_two, tc)
{
	pid_t child, queried;
	int fd1, fd2, status;

	child = pdfork(&fd1, PD_DAEMON);
	ATF_REQUIRE_MSG(child >= 0, "pdfork: %s", strerror(errno));
	if (child == 0) {
		for (;;)
			pause();
		_exit(0);
	}

	fd2 = pdopenpid(child, 0);
	ATF_REQUIRE_MSG(fd2 >= 0, "pdopenpid: %s", strerror(errno));

	/* Close the first fd; the process should remain alive. */
	ATF_REQUIRE(close(fd1) == 0);

	/*
	 * Verify the process is still reachable via the second fd and still
	 * alive.
	 */
	ATF_REQUIRE_EQ(0, pdgetpid(fd2, &queried));
	ATF_REQUIRE_EQ(child, queried);
	ATF_REQUIRE_MSG(pdkill(fd2, 0) == 0,
	    "pdkill(0) after closing first fd: %s", strerror(errno));

	/* Now kill and reap via the second fd. */
	ATF_REQUIRE_MSG(pdkill(fd2, SIGKILL) == 0,
	    "pdkill: %s", strerror(errno));
	ATF_REQUIRE_MSG(pdwait(fd2, &status, WEXITED, NULL, NULL) == 0,
	    "pdwait: %s", strerror(errno));
	ATF_REQUIRE(WIFSIGNALED(status));
	ATF_REQUIRE_EQ(WTERMSIG(status), SIGKILL);

	ATF_REQUIRE(close(fd2) == 0);
}

/*
 * Two calls to pdopenpid for the same process (no pdfork) should both work.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_open_twice);
ATF_TC_BODY(pdopenpid_open_twice, tc)
{
	pid_t child, pid1, pid2;
	int fd1, fd2;

	child = fork();
	ATF_REQUIRE_MSG(child >= 0, "fork: %s", strerror(errno));
	if (child == 0) {
		for (;;)
			pause();
		_exit(0);
	}

	fd1 = pdopenpid(child, 0);
	ATF_REQUIRE_MSG(fd1 >= 0, "pdopenpid(1): %s", strerror(errno));

	fd2 = pdopenpid(child, 0);
	ATF_REQUIRE_MSG(fd2 >= 0, "pdopenpid(2): %s", strerror(errno));

	ATF_REQUIRE_EQ(0, pdgetpid(fd1, &pid1));
	ATF_REQUIRE_EQ(0, pdgetpid(fd2, &pid2));
	ATF_REQUIRE_EQ(pid1, pid2);

	/* Close the first, process should survive. */
	ATF_REQUIRE(close(fd1) == 0);
	ATF_REQUIRE_MSG(pdkill(fd2, 0) == 0,
	    "pdkill(0) after closing first fd: %s", strerror(errno));

	ATF_REQUIRE_MSG(pdkill(fd2, SIGKILL) == 0,
	    "pdkill: %s", strerror(errno));
	ATF_REQUIRE_MSG(pdwait(fd2, NULL, WEXITED, NULL, NULL) == 0,
	    "pdwait: %s", strerror(errno));
	ATF_REQUIRE(close(fd2) == 0);
}

/*
 * When a process with two procdesc fds exits, only one pdwait should succeed
 * in collecting the exit status.  The other should get ESRCH.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_pdwait_only_one);
ATF_TC_BODY(pdopenpid_pdwait_only_one, tc)
{
	pid_t child;
	int fd1, fd2, status;

	child = pdfork(&fd1, PD_DAEMON);
	ATF_REQUIRE_MSG(child >= 0, "pdfork: %s", strerror(errno));
	if (child == 0)
		_exit(42);

	fd2 = pdopenpid(child, 0);
	ATF_REQUIRE_MSG(fd2 >= 0, "pdopenpid: %s", strerror(errno));

	/* Collect via the first fd. */
	ATF_REQUIRE_MSG(pdwait(fd1, &status, WEXITED, NULL, NULL) == 0,
	    "pdwait(fd1): %s", strerror(errno));
	ATF_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 42);

	/* The second fd should no longer be able to collect. */
	ATF_REQUIRE_ERRNO(ESRCH, pdwait(fd2, &status, WEXITED, NULL, NULL) < 0);

	ATF_REQUIRE(close(fd1) == 0);
	ATF_REQUIRE(close(fd2) == 0);
}

/*
 * Opening a process descriptor for ourselves should work.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_self);
ATF_TC_BODY(pdopenpid_self, tc)
{
	pid_t child, queried;
	int childfd, fd, status;

	/*
	 * Closing self-referencing procdesc would steal the exit
	 * status from the parent.  Keep one more procdesc for the
	 * child in the parent so that we can query the state.
	 *
	 * For the same reason we must use fork() and test in the
	 * child, otherwise kyua does not get the SIGCHILD nor the
	 * exit status.
	 */
	child = pdfork(&childfd, 0);
	if (child == 0) {
		fd = pdopenpid(getpid(), PD_DAEMON);
		if (fd < 0)
			_exit(21);
		if (pdgetpid(fd, &queried) == -1)
			_exit(22);
		if (getpid() != queried)
			_exit(23);
		if (close(fd) != 0)
			_exit(24);
		_exit(0);
	}

	ATF_REQUIRE(pdgetpid(childfd, &queried) == 0);
	ATF_REQUIRE_EQ(queried, child);
	ATF_REQUIRE(pdwait(childfd, &status, WEXITED, NULL, NULL) == 0);

	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE_EQ(WEXITSTATUS(status), 0);
	ATF_REQUIRE(close(childfd) == 0);
}

/*
 * pdopenpid for a process that is exiting (P_WEXIT set) should fail with EBUSY.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_exiting);
ATF_TC_BODY(pdopenpid_exiting, tc)
{
	pid_t child;
	int fd, pip[2], status;

	ATF_REQUIRE_EQ(pipe(pip), 0);

	child = fork();
	ATF_REQUIRE_MSG(child >= 0, "fork: %s", strerror(errno));
	if (child == 0) {
		char c;

		close(pip[1]);
		(void)read(pip[0], &c, 1);
		_exit(0);
	}
	ATF_REQUIRE(close(pip[0]) == 0);

	/* Tell the child to exit. */
	ATF_REQUIRE(close(pip[1]) == 0);

	/* Might still race. */
	usleep(1000);
	fd = pdopenpid(child, 0);
	if (fd >= 0) {
		ATF_REQUIRE(close(fd) == 0);
	} else {
		ATF_REQUIRE_MSG(errno == EBUSY,
		    "unexpected errno %d (%s)", errno, strerror(errno));
	}

	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE_EQ(WEXITSTATUS(status), 0);
}

/*
 * Make sure that opening a process descriptor doesn't cause a wakeup in the
 * target process.
 */
ATF_TC_WITHOUT_HEAD(pdopenpid_no_wakeup);
ATF_TC_BODY(pdopenpid_no_wakeup, tc)
{
	pid_t child;
	int fd, pip[2];

	ATF_REQUIRE(pipe(pip) == 0);

	child = fork();
	ATF_REQUIRE_MSG(child >= 0, "fork: %s", strerror(errno));
	if (child == 0) {
		char buf[1];

		close(pip[1]);
		(void)read(pip[0], buf, 1);
		_exit(0);
	}
	close(pip[0]);

	wait_for_naptime(child, "piperd");

	fd = pdopenpid(child, 0);
	ATF_REQUIRE_MSG(fd >= 0, "pdopenpid: %s", strerror(errno));

	/*
	 * If pdopenpid() caused a wakeup, read() returned and the child exited.
	 */
	wait_for_naptime(child, "piperd");

	ATF_REQUIRE(close(pip[1]) == 0);
	ATF_REQUIRE(close(fd) == 0);
}

/*
 * basic pddupfd functionality
 */

#define	EXPECTED_OFFSET		123

ATF_TC_WITHOUT_HEAD(pddupfd_basic);
ATF_TC_BODY(pddupfd_basic, tc)
{
	int *comm;
	struct __wrusage wu;
	struct __siginfo si;
	int fd, pp, rfd, status, workfd, r, r1;
	off_t off;
	pid_t child;

	comm = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED |
	    MAP_ANONYMOUS, -1, 0);
	ATF_REQUIRE(comm != MAP_FAILED);
	atomic_store_int(comm, -1);

	child = fork();
	ATF_REQUIRE(child != -1);
	if (child == 0) {
		workfd = open("/etc/passwd", O_RDONLY);
		ATF_REQUIRE(workfd != -1);
		atomic_store_int(comm, workfd);
		while ((r = (int)atomic_load_int(comm)) >= 0)
			sleep(1);
		lseek(workfd, EXPECTED_OFFSET, SEEK_SET);
		r -= 1;
		atomic_store_int(comm, r);
		while (r == (int)atomic_load_int(comm))
			sleep(1);
		_exit(0x23);
	}

	fd = pdopenpid(child, 0);
	ATF_REQUIRE(fd != -1);
	ATF_REQUIRE(pdgetpid(fd, &pp) == 0);

	while ((rfd = (int)atomic_load_int(comm)) == -1)
		;
	ATF_REQUIRE(rfd >= 0);
	workfd = pddupfd(fd, rfd, 0);
	ATF_REQUIRE(workfd != -1);

	/*
	 * pddupfd-ed file must dup-ed, i.e. the underlying file must
	 * be same between the remote and local filedescriptors.
	 * Check that the initial seek offset is zero.  Then observe
	 * the updated offset, which is only possible if the file is
	 * indeed shared.
	 */
	off = lseek(workfd, 0, SEEK_CUR);
	ATF_REQUIRE_EQ(off, 0);
	r = -1;
	atomic_store_int(comm, r);
	while ((r1 = (int)atomic_load_int(comm)) == r)
		sleep(1);
	off = lseek(workfd, 0, SEEK_CUR);
	ATF_REQUIRE_EQ(off, EXPECTED_OFFSET);
	r1 -= 1;
	atomic_store_int(comm, r1);
	ATF_REQUIRE(close(workfd) == 0);

	ATF_REQUIRE(pdwait(fd, &status, WEXITED, &wu, &si) != -1);
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pid_recycle);
	ATF_TP_ADD_TC(tp, poll_close_race);
	ATF_TP_ADD_TC(tp, poll_exit_wakeup);

	ATF_TP_ADD_TC(tp, pdopenpid_basic);
	ATF_TP_ADD_TC(tp, pdopenpid_cloexec);
	ATF_TP_ADD_TC(tp, pdopenpid_einval);
	ATF_TP_ADD_TC(tp, pdopenpid_emfile);
	ATF_TP_ADD_TC(tp, pdopenpid_esrch);
	ATF_TP_ADD_TC(tp, pdopenpid_capmode);
	ATF_TP_ADD_TC(tp, pdopenpid_pdfork_then_open);
	ATF_TP_ADD_TC(tp, pdopenpid_fork_then_open);
	ATF_TP_ADD_TC(tp, pdopenpid_close_one_of_two);
	ATF_TP_ADD_TC(tp, pdopenpid_open_twice);
	ATF_TP_ADD_TC(tp, pdopenpid_pdwait_only_one);
	ATF_TP_ADD_TC(tp, pdopenpid_self);
	ATF_TP_ADD_TC(tp, pdopenpid_exiting);
	ATF_TP_ADD_TC(tp, pdopenpid_no_wakeup);

	ATF_TP_ADD_TC(tp, pddupfd_basic);

	return (atf_no_error());
}
