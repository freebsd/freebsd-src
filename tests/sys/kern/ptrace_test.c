/*-
 * Copyright (c) 2015 John Baldwin <jhb@FreeBSD.org>
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <atf-c.h>

/*
 * A variant of ATF_REQUIRE that is suitable for use in child
 * processes.  This only works if the parent process is tripped up by
 * the early exit and fails some requirement itself.
 */
#define	CHILD_REQUIRE(exp) do {						\
		if (!(exp))						\
			child_fail_require(__FILE__, __LINE__,		\
			    #exp " not met");				\
	} while (0)

static __dead2 void
child_fail_require(const char *file, int line, const char *str)
{
	char buf[128];

	snprintf(buf, sizeof(buf), "%s:%d: %s\n", file, line, str);
	write(2, buf, strlen(buf));
	_exit(32);
}

static void
trace_me(void)
{

	/* Attach the parent process as a tracer of this process. */
	CHILD_REQUIRE(ptrace(PT_TRACE_ME, 0, NULL, 0) != -1);

	/* Trigger a stop. */
	raise(SIGSTOP);
}

static void
attach_child(pid_t pid)
{
	pid_t wpid;
	int status;

	ATF_REQUIRE(ptrace(PT_ATTACH, pid, NULL, 0) == 0);

	wpid = waitpid(pid, &status, 0);
	ATF_REQUIRE(wpid == pid);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);
}

static void
wait_for_zombie(pid_t pid)
{

	/*
	 * Wait for a process to exit.  This is kind of gross, but
	 * there is not a better way.
	 */
	for (;;) {
		struct kinfo_proc kp;
		size_t len;
		int mib[4];

		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = KERN_PROC_PID;
		mib[3] = pid;
		len = sizeof(kp);
		if (sysctl(mib, nitems(mib), &kp, &len, NULL, 0) == -1) {
			/* The KERN_PROC_PID sysctl fails for zombies. */
			ATF_REQUIRE(errno == ESRCH);
			break;
		}
		usleep(5000);
	}
}

/*
 * Verify that a parent debugger process "sees" the exit of a debugged
 * process exactly once when attached via PT_TRACE_ME.
 */
ATF_TC_WITHOUT_HEAD(ptrace__parent_wait_after_trace_me);
ATF_TC_BODY(ptrace__parent_wait_after_trace_me, tc)
{
	pid_t child, wpid;
	int status;

	ATF_REQUIRE((child = fork()) != -1);
	if (child == 0) {
		/* Child process. */
		trace_me();

		_exit(1);
	}

	/* Parent process. */

	/* The first wait() should report the stop from SIGSTOP. */
	wpid = waitpid(child, &status, 0);
	ATF_REQUIRE(wpid == child);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);

	/* Continue the child ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, child, (caddr_t)1, 0) != -1);

	/* The second wait() should report the exit status. */
	wpid = waitpid(child, &status, 0);
	ATF_REQUIRE(wpid == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);

	/* The child should no longer exist. */
	wpid = waitpid(child, &status, 0);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

/*
 * Verify that a parent debugger process "sees" the exit of a debugged
 * process exactly once when attached via PT_ATTACH.
 */
ATF_TC_WITHOUT_HEAD(ptrace__parent_wait_after_attach);
ATF_TC_BODY(ptrace__parent_wait_after_attach, tc)
{
	pid_t child, wpid;
	int cpipe[2], status;
	char c;

	ATF_REQUIRE(pipe(cpipe) == 0);
	ATF_REQUIRE((child = fork()) != -1);
	if (child == 0) {
		/* Child process. */
		close(cpipe[0]);

		/* Wait for the parent to attach. */
		CHILD_REQUIRE(read(cpipe[1], &c, sizeof(c)) == 0);

		_exit(1);
	}
	close(cpipe[1]);

	/* Parent process. */

	/* Attach to the child process. */
	attach_child(child);

	/* Continue the child ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, child, (caddr_t)1, 0) != -1);

	/* Signal the child to exit. */
	close(cpipe[0]);

	/* The second wait() should report the exit status. */
	wpid = waitpid(child, &status, 0);
	ATF_REQUIRE(wpid == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);

	/* The child should no longer exist. */
	wpid = waitpid(child, &status, 0);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

/*
 * Verify that a parent process "sees" the exit of a debugged process only
 * after the debugger has seen it.
 */
ATF_TC_WITHOUT_HEAD(ptrace__parent_sees_exit_after_child_debugger);
ATF_TC_BODY(ptrace__parent_sees_exit_after_child_debugger, tc)
{
	pid_t child, debugger, wpid;
	int cpipe[2], dpipe[2], status;
	char c;

	ATF_REQUIRE(pipe(cpipe) == 0);
	ATF_REQUIRE((child = fork()) != -1);

	if (child == 0) {
		/* Child process. */
		close(cpipe[0]);

		/* Wait for parent to be ready. */
		CHILD_REQUIRE(read(cpipe[1], &c, sizeof(c)) == sizeof(c));

		_exit(1);
	}
	close(cpipe[1]);

	ATF_REQUIRE(pipe(dpipe) == 0);
	ATF_REQUIRE((debugger = fork()) != -1);

	if (debugger == 0) {
		/* Debugger process. */
		close(dpipe[0]);

		CHILD_REQUIRE(ptrace(PT_ATTACH, child, NULL, 0) != -1);

		wpid = waitpid(child, &status, 0);
		CHILD_REQUIRE(wpid == child);
		CHILD_REQUIRE(WIFSTOPPED(status));
		CHILD_REQUIRE(WSTOPSIG(status) == SIGSTOP);

		CHILD_REQUIRE(ptrace(PT_CONTINUE, child, (caddr_t)1, 0) != -1);

		/* Signal parent that debugger is attached. */
		CHILD_REQUIRE(write(dpipe[1], &c, sizeof(c)) == sizeof(c));

		/* Wait for parent's failed wait. */
		CHILD_REQUIRE(read(dpipe[1], &c, sizeof(c)) == 0);

		wpid = waitpid(child, &status, 0);
		CHILD_REQUIRE(wpid == child);
		CHILD_REQUIRE(WIFEXITED(status));
		CHILD_REQUIRE(WEXITSTATUS(status) == 1);

		_exit(0);
	}
	close(dpipe[1]);

	/* Parent process. */

	/* Wait for the debugger to attach to the child. */
	ATF_REQUIRE(read(dpipe[0], &c, sizeof(c)) == sizeof(c));

	/* Release the child. */
	ATF_REQUIRE(write(cpipe[0], &c, sizeof(c)) == sizeof(c));
	ATF_REQUIRE(read(cpipe[0], &c, sizeof(c)) == 0);
	close(cpipe[0]);

	wait_for_zombie(child);

	/*
	 * This wait should return a pid of 0 to indicate no status to
	 * report.  The parent should see the child as non-exited
	 * until the debugger sees the exit.
	 */
	wpid = waitpid(child, &status, WNOHANG);
	ATF_REQUIRE(wpid == 0);

	/* Signal the debugger to wait for the child. */
	close(dpipe[0]);

	/* Wait for the debugger. */
	wpid = waitpid(debugger, &status, 0);
	ATF_REQUIRE(wpid == debugger);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 0);

	/* The child process should now be ready. */
	wpid = waitpid(child, &status, WNOHANG);
	ATF_REQUIRE(wpid == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);
}

/*
 * Verify that a parent process "sees" the exit of a debugged process
 * only after a non-direct-child debugger has seen it.  In particular,
 * various wait() calls in the parent must avoid failing with ESRCH by
 * checking the parent's orphan list for the debugee.
 */
ATF_TC_WITHOUT_HEAD(ptrace__parent_sees_exit_after_unrelated_debugger);
ATF_TC_BODY(ptrace__parent_sees_exit_after_unrelated_debugger, tc)
{
	pid_t child, debugger, fpid, wpid;
	int cpipe[2], dpipe[2], status;
	char c;

	ATF_REQUIRE(pipe(cpipe) == 0);
	ATF_REQUIRE((child = fork()) != -1);

	if (child == 0) {
		/* Child process. */
		close(cpipe[0]);

		/* Wait for parent to be ready. */
		CHILD_REQUIRE(read(cpipe[1], &c, sizeof(c)) == sizeof(c));

		_exit(1);
	}
	close(cpipe[1]);

	ATF_REQUIRE(pipe(dpipe) == 0);
	ATF_REQUIRE((debugger = fork()) != -1);

	if (debugger == 0) {
		/* Debugger parent. */

		/*
		 * Fork again and drop the debugger parent so that the
		 * debugger is not a child of the main parent.
		 */
		CHILD_REQUIRE((fpid = fork()) != -1);
		if (fpid != 0)
			_exit(2);

		/* Debugger process. */
		close(dpipe[0]);

		CHILD_REQUIRE(ptrace(PT_ATTACH, child, NULL, 0) != -1);

		wpid = waitpid(child, &status, 0);
		CHILD_REQUIRE(wpid == child);
		CHILD_REQUIRE(WIFSTOPPED(status));
		CHILD_REQUIRE(WSTOPSIG(status) == SIGSTOP);

		CHILD_REQUIRE(ptrace(PT_CONTINUE, child, (caddr_t)1, 0) != -1);

		/* Signal parent that debugger is attached. */
		CHILD_REQUIRE(write(dpipe[1], &c, sizeof(c)) == sizeof(c));

		/* Wait for parent's failed wait. */
		CHILD_REQUIRE(read(dpipe[1], &c, sizeof(c)) == sizeof(c));

		wpid = waitpid(child, &status, 0);
		CHILD_REQUIRE(wpid == child);
		CHILD_REQUIRE(WIFEXITED(status));
		CHILD_REQUIRE(WEXITSTATUS(status) == 1);

		_exit(0);
	}
	close(dpipe[1]);

	/* Parent process. */

	/* Wait for the debugger parent process to exit. */
	wpid = waitpid(debugger, &status, 0);
	ATF_REQUIRE(wpid == debugger);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 2);

	/* A WNOHANG wait here should see the non-exited child. */
	wpid = waitpid(child, &status, WNOHANG);
	ATF_REQUIRE(wpid == 0);

	/* Wait for the debugger to attach to the child. */
	ATF_REQUIRE(read(dpipe[0], &c, sizeof(c)) == sizeof(c));

	/* Release the child. */
	ATF_REQUIRE(write(cpipe[0], &c, sizeof(c)) == sizeof(c));
	ATF_REQUIRE(read(cpipe[0], &c, sizeof(c)) == 0);
	close(cpipe[0]);

	wait_for_zombie(child);

	/*
	 * This wait should return a pid of 0 to indicate no status to
	 * report.  The parent should see the child as non-exited
	 * until the debugger sees the exit.
	 */
	wpid = waitpid(child, &status, WNOHANG);
	ATF_REQUIRE(wpid == 0);

	/* Signal the debugger to wait for the child. */
	ATF_REQUIRE(write(dpipe[0], &c, sizeof(c)) == sizeof(c));

	/* Wait for the debugger. */
	ATF_REQUIRE(read(dpipe[0], &c, sizeof(c)) == 0);
	close(dpipe[0]);

	/* The child process should now be ready. */
	wpid = waitpid(child, &status, WNOHANG);
	ATF_REQUIRE(wpid == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);
}

/*
 * The parent process should always act the same regardless of how the
 * debugger is attached to it.
 */
static __dead2 void
follow_fork_parent(bool use_vfork)
{
	pid_t fpid, wpid;
	int status;

	if (use_vfork)
		CHILD_REQUIRE((fpid = vfork()) != -1);
	else
		CHILD_REQUIRE((fpid = fork()) != -1);

	if (fpid == 0)
		/* Child */
		_exit(2);

	wpid = waitpid(fpid, &status, 0);
	CHILD_REQUIRE(wpid == fpid);
	CHILD_REQUIRE(WIFEXITED(status));
	CHILD_REQUIRE(WEXITSTATUS(status) == 2);

	_exit(1);
}

/*
 * Helper routine for follow fork tests.  This waits for two stops
 * that report both "sides" of a fork.  It returns the pid of the new
 * child process.
 */
static pid_t
handle_fork_events(pid_t parent, struct ptrace_lwpinfo *ppl)
{
	struct ptrace_lwpinfo pl;
	bool fork_reported[2];
	pid_t child, wpid;
	int i, status;

	fork_reported[0] = false;
	fork_reported[1] = false;
	child = -1;
	
	/*
	 * Each process should report a fork event.  The parent should
	 * report a PL_FLAG_FORKED event, and the child should report
	 * a PL_FLAG_CHILD event.
	 */
	for (i = 0; i < 2; i++) {
		wpid = wait(&status);
		ATF_REQUIRE(wpid > 0);
		ATF_REQUIRE(WIFSTOPPED(status));

		ATF_REQUIRE(ptrace(PT_LWPINFO, wpid, (caddr_t)&pl,
		    sizeof(pl)) != -1);
		ATF_REQUIRE((pl.pl_flags & (PL_FLAG_FORKED | PL_FLAG_CHILD)) !=
		    0);
		ATF_REQUIRE((pl.pl_flags & (PL_FLAG_FORKED | PL_FLAG_CHILD)) !=
		    (PL_FLAG_FORKED | PL_FLAG_CHILD));
		if (pl.pl_flags & PL_FLAG_CHILD) {
			ATF_REQUIRE(wpid != parent);
			ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);
			ATF_REQUIRE(!fork_reported[1]);
			if (child == -1)
				child = wpid;
			else
				ATF_REQUIRE(child == wpid);
			if (ppl != NULL)
				ppl[1] = pl;
			fork_reported[1] = true;
		} else {
			ATF_REQUIRE(wpid == parent);
			ATF_REQUIRE(WSTOPSIG(status) == SIGTRAP);
			ATF_REQUIRE(!fork_reported[0]);
			if (child == -1)
				child = pl.pl_child_pid;
			else
				ATF_REQUIRE(child == pl.pl_child_pid);
			if (ppl != NULL)
				ppl[0] = pl;
			fork_reported[0] = true;
		}
	}

	return (child);
}

/*
 * Verify that a new child process is stopped after a followed fork and
 * that the traced parent sees the exit of the child after the debugger
 * when both processes remain attached to the debugger.
 */
ATF_TC_WITHOUT_HEAD(ptrace__follow_fork_both_attached);
ATF_TC_BODY(ptrace__follow_fork_both_attached, tc)
{
	pid_t children[2], fpid, wpid;
	int status;

	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		trace_me();
		follow_fork_parent(false);
	}

	/* Parent process. */
	children[0] = fpid;

	/* The first wait() should report the stop from SIGSTOP. */
	wpid = waitpid(children[0], &status, 0);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);

	ATF_REQUIRE(ptrace(PT_FOLLOW_FORK, children[0], NULL, 1) != -1);

	/* Continue the child ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);

	children[1] = handle_fork_events(children[0], NULL);
	ATF_REQUIRE(children[1] > 0);

	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[1], (caddr_t)1, 0) != -1);

	/*
	 * The child can't exit until the grandchild reports status, so the
	 * grandchild should report its exit first to the debugger.
	 */
	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[1]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 2);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

/*
 * Verify that a new child process is stopped after a followed fork
 * and that the traced parent sees the exit of the child when the new
 * child process is detached after it reports its fork.
 */
ATF_TC_WITHOUT_HEAD(ptrace__follow_fork_child_detached);
ATF_TC_BODY(ptrace__follow_fork_child_detached, tc)
{
	pid_t children[2], fpid, wpid;
	int status;

	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		trace_me();
		follow_fork_parent(false);
	}

	/* Parent process. */
	children[0] = fpid;

	/* The first wait() should report the stop from SIGSTOP. */
	wpid = waitpid(children[0], &status, 0);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);

	ATF_REQUIRE(ptrace(PT_FOLLOW_FORK, children[0], NULL, 1) != -1);

	/* Continue the child ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);

	children[1] = handle_fork_events(children[0], NULL);
	ATF_REQUIRE(children[1] > 0);

	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);
	ATF_REQUIRE(ptrace(PT_DETACH, children[1], (caddr_t)1, 0) != -1);

	/*
	 * Should not see any status from the grandchild now, only the
	 * child.
	 */
	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

/*
 * Verify that a new child process is stopped after a followed fork
 * and that the traced parent sees the exit of the child when the
 * traced parent is detached after the fork.
 */
ATF_TC_WITHOUT_HEAD(ptrace__follow_fork_parent_detached);
ATF_TC_BODY(ptrace__follow_fork_parent_detached, tc)
{
	pid_t children[2], fpid, wpid;
	int status;

	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		trace_me();
		follow_fork_parent(false);
	}

	/* Parent process. */
	children[0] = fpid;

	/* The first wait() should report the stop from SIGSTOP. */
	wpid = waitpid(children[0], &status, 0);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);

	ATF_REQUIRE(ptrace(PT_FOLLOW_FORK, children[0], NULL, 1) != -1);

	/* Continue the child ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);

	children[1] = handle_fork_events(children[0], NULL);
	ATF_REQUIRE(children[1] > 0);

	ATF_REQUIRE(ptrace(PT_DETACH, children[0], (caddr_t)1, 0) != -1);
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[1], (caddr_t)1, 0) != -1);

	/*
	 * The child can't exit until the grandchild reports status, so the
	 * grandchild should report its exit first to the debugger.
	 *
	 * Even though the child process is detached, it is still a
	 * child of the debugger, so it will still report it's exit
	 * after the grandchild.
	 */
	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[1]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 2);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

static void
attach_fork_parent(int cpipe[2])
{
	pid_t fpid;

	close(cpipe[0]);

	/* Double-fork to disassociate from the debugger. */
	CHILD_REQUIRE((fpid = fork()) != -1);
	if (fpid != 0)
		_exit(3);
	
	/* Send the pid of the disassociated child to the debugger. */
	fpid = getpid();
	CHILD_REQUIRE(write(cpipe[1], &fpid, sizeof(fpid)) == sizeof(fpid));

	/* Wait for the debugger to attach. */
	CHILD_REQUIRE(read(cpipe[1], &fpid, sizeof(fpid)) == 0);
}

/*
 * Verify that a new child process is stopped after a followed fork and
 * that the traced parent sees the exit of the child after the debugger
 * when both processes remain attached to the debugger.  In this test
 * the parent that forks is not a direct child of the debugger.
 */
ATF_TC_WITHOUT_HEAD(ptrace__follow_fork_both_attached_unrelated_debugger);
ATF_TC_BODY(ptrace__follow_fork_both_attached_unrelated_debugger, tc)
{
	pid_t children[2], fpid, wpid;
	int cpipe[2], status;

	ATF_REQUIRE(pipe(cpipe) == 0);
	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		attach_fork_parent(cpipe);
		follow_fork_parent(false);
	}

	/* Parent process. */
	close(cpipe[1]);

	/* Wait for the direct child to exit. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(wpid == fpid);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 3);

	/* Read the pid of the fork parent. */
	ATF_REQUIRE(read(cpipe[0], &children[0], sizeof(children[0])) ==
	    sizeof(children[0]));

	/* Attach to the fork parent. */
	attach_child(children[0]);

	ATF_REQUIRE(ptrace(PT_FOLLOW_FORK, children[0], NULL, 1) != -1);

	/* Continue the fork parent ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);

	/* Signal the fork parent to continue. */
	close(cpipe[0]);

	children[1] = handle_fork_events(children[0], NULL);
	ATF_REQUIRE(children[1] > 0);

	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[1], (caddr_t)1, 0) != -1);

	/*
	 * The fork parent can't exit until the child reports status,
	 * so the child should report its exit first to the debugger.
	 */
	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[1]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 2);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

/*
 * Verify that a new child process is stopped after a followed fork
 * and that the traced parent sees the exit of the child when the new
 * child process is detached after it reports its fork.  In this test
 * the parent that forks is not a direct child of the debugger.
 */
ATF_TC_WITHOUT_HEAD(ptrace__follow_fork_child_detached_unrelated_debugger);
ATF_TC_BODY(ptrace__follow_fork_child_detached_unrelated_debugger, tc)
{
	pid_t children[2], fpid, wpid;
	int cpipe[2], status;

	ATF_REQUIRE(pipe(cpipe) == 0);
	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		attach_fork_parent(cpipe);
		follow_fork_parent(false);
	}

	/* Parent process. */
	close(cpipe[1]);

	/* Wait for the direct child to exit. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(wpid == fpid);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 3);

	/* Read the pid of the fork parent. */
	ATF_REQUIRE(read(cpipe[0], &children[0], sizeof(children[0])) ==
	    sizeof(children[0]));

	/* Attach to the fork parent. */
	attach_child(children[0]);

	ATF_REQUIRE(ptrace(PT_FOLLOW_FORK, children[0], NULL, 1) != -1);

	/* Continue the fork parent ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);

	/* Signal the fork parent to continue. */
	close(cpipe[0]);

	children[1] = handle_fork_events(children[0], NULL);
	ATF_REQUIRE(children[1] > 0);

	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);
	ATF_REQUIRE(ptrace(PT_DETACH, children[1], (caddr_t)1, 0) != -1);

	/*
	 * Should not see any status from the child now, only the fork
	 * parent.
	 */
	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

/*
 * Verify that a new child process is stopped after a followed fork
 * and that the traced parent sees the exit of the child when the
 * traced parent is detached after the fork.  In this test the parent
 * that forks is not a direct child of the debugger.
 */
ATF_TC_WITHOUT_HEAD(ptrace__follow_fork_parent_detached_unrelated_debugger);
ATF_TC_BODY(ptrace__follow_fork_parent_detached_unrelated_debugger, tc)
{
	pid_t children[2], fpid, wpid;
	int cpipe[2], status;

	ATF_REQUIRE(pipe(cpipe) == 0);
	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		attach_fork_parent(cpipe);
		follow_fork_parent(false);
	}

	/* Parent process. */
	close(cpipe[1]);

	/* Wait for the direct child to exit. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(wpid == fpid);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 3);

	/* Read the pid of the fork parent. */
	ATF_REQUIRE(read(cpipe[0], &children[0], sizeof(children[0])) ==
	    sizeof(children[0]));

	/* Attach to the fork parent. */
	attach_child(children[0]);

	ATF_REQUIRE(ptrace(PT_FOLLOW_FORK, children[0], NULL, 1) != -1);

	/* Continue the fork parent ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);

	/* Signal the fork parent to continue. */
	close(cpipe[0]);

	children[1] = handle_fork_events(children[0], NULL);
	ATF_REQUIRE(children[1] > 0);

	ATF_REQUIRE(ptrace(PT_DETACH, children[0], (caddr_t)1, 0) != -1);
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[1], (caddr_t)1, 0) != -1);

	/*
	 * Should not see any status from the fork parent now, only
	 * the child.
	 */
	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[1]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 2);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

/*
 * Verify that a child process does not see an unrelated debugger as its
 * parent but sees its original parent process.
 */
ATF_TC_WITHOUT_HEAD(ptrace__getppid);
ATF_TC_BODY(ptrace__getppid, tc)
{
	pid_t child, debugger, ppid, wpid;
	int cpipe[2], dpipe[2], status;
	char c;

	ATF_REQUIRE(pipe(cpipe) == 0);
	ATF_REQUIRE((child = fork()) != -1);

	if (child == 0) {
		/* Child process. */
		close(cpipe[0]);

		/* Wait for parent to be ready. */
		CHILD_REQUIRE(read(cpipe[1], &c, sizeof(c)) == sizeof(c));

		/* Report the parent PID to the parent. */
		ppid = getppid();
		CHILD_REQUIRE(write(cpipe[1], &ppid, sizeof(ppid)) ==
		    sizeof(ppid));

		_exit(1);
	}
	close(cpipe[1]);

	ATF_REQUIRE(pipe(dpipe) == 0);
	ATF_REQUIRE((debugger = fork()) != -1);

	if (debugger == 0) {
		/* Debugger process. */
		close(dpipe[0]);

		CHILD_REQUIRE(ptrace(PT_ATTACH, child, NULL, 0) != -1);

		wpid = waitpid(child, &status, 0);
		CHILD_REQUIRE(wpid == child);
		CHILD_REQUIRE(WIFSTOPPED(status));
		CHILD_REQUIRE(WSTOPSIG(status) == SIGSTOP);

		CHILD_REQUIRE(ptrace(PT_CONTINUE, child, (caddr_t)1, 0) != -1);

		/* Signal parent that debugger is attached. */
		CHILD_REQUIRE(write(dpipe[1], &c, sizeof(c)) == sizeof(c));

		/* Wait for traced child to exit. */
		wpid = waitpid(child, &status, 0);
		CHILD_REQUIRE(wpid == child);
		CHILD_REQUIRE(WIFEXITED(status));
		CHILD_REQUIRE(WEXITSTATUS(status) == 1);

		_exit(0);
	}
	close(dpipe[1]);

	/* Parent process. */

	/* Wait for the debugger to attach to the child. */
	ATF_REQUIRE(read(dpipe[0], &c, sizeof(c)) == sizeof(c));

	/* Release the child. */
	ATF_REQUIRE(write(cpipe[0], &c, sizeof(c)) == sizeof(c));

	/* Read the parent PID from the child. */
	ATF_REQUIRE(read(cpipe[0], &ppid, sizeof(ppid)) == sizeof(ppid));
	close(cpipe[0]);

	ATF_REQUIRE(ppid == getpid());

	/* Wait for the debugger. */
	wpid = waitpid(debugger, &status, 0);
	ATF_REQUIRE(wpid == debugger);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 0);

	/* The child process should now be ready. */
	wpid = waitpid(child, &status, WNOHANG);
	ATF_REQUIRE(wpid == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);
}

/*
 * Verify that pl_syscall_code in struct ptrace_lwpinfo for a new
 * child process created via fork() reports the correct value.
 */
ATF_TC_WITHOUT_HEAD(ptrace__new_child_pl_syscall_code_fork);
ATF_TC_BODY(ptrace__new_child_pl_syscall_code_fork, tc)
{
	struct ptrace_lwpinfo pl[2];
	pid_t children[2], fpid, wpid;
	int status;

	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		trace_me();
		follow_fork_parent(false);
	}

	/* Parent process. */
	children[0] = fpid;

	/* The first wait() should report the stop from SIGSTOP. */
	wpid = waitpid(children[0], &status, 0);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);

	ATF_REQUIRE(ptrace(PT_FOLLOW_FORK, children[0], NULL, 1) != -1);

	/* Continue the child ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);

	/* Wait for both halves of the fork event to get reported. */
	children[1] = handle_fork_events(children[0], pl);
	ATF_REQUIRE(children[1] > 0);

	ATF_REQUIRE((pl[0].pl_flags & PL_FLAG_SCX) != 0);
	ATF_REQUIRE((pl[1].pl_flags & PL_FLAG_SCX) != 0);
	ATF_REQUIRE(pl[0].pl_syscall_code == SYS_fork);
	ATF_REQUIRE(pl[0].pl_syscall_code == pl[1].pl_syscall_code);
	ATF_REQUIRE(pl[0].pl_syscall_narg == pl[1].pl_syscall_narg);

	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[1], (caddr_t)1, 0) != -1);

	/*
	 * The child can't exit until the grandchild reports status, so the
	 * grandchild should report its exit first to the debugger.
	 */
	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[1]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 2);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

/*
 * Verify that pl_syscall_code in struct ptrace_lwpinfo for a new
 * child process created via vfork() reports the correct value.
 */
ATF_TC_WITHOUT_HEAD(ptrace__new_child_pl_syscall_code_vfork);
ATF_TC_BODY(ptrace__new_child_pl_syscall_code_vfork, tc)
{
	struct ptrace_lwpinfo pl[2];
	pid_t children[2], fpid, wpid;
	int status;

	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		trace_me();
		follow_fork_parent(true);
	}

	/* Parent process. */
	children[0] = fpid;

	/* The first wait() should report the stop from SIGSTOP. */
	wpid = waitpid(children[0], &status, 0);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);

	ATF_REQUIRE(ptrace(PT_FOLLOW_FORK, children[0], NULL, 1) != -1);

	/* Continue the child ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);

	/* Wait for both halves of the fork event to get reported. */
	children[1] = handle_fork_events(children[0], pl);
	ATF_REQUIRE(children[1] > 0);

	ATF_REQUIRE((pl[0].pl_flags & PL_FLAG_SCX) != 0);
	ATF_REQUIRE((pl[1].pl_flags & PL_FLAG_SCX) != 0);
	ATF_REQUIRE(pl[0].pl_syscall_code == SYS_vfork);
	ATF_REQUIRE(pl[0].pl_syscall_code == pl[1].pl_syscall_code);
	ATF_REQUIRE(pl[0].pl_syscall_narg == pl[1].pl_syscall_narg);

	ATF_REQUIRE(ptrace(PT_CONTINUE, children[0], (caddr_t)1, 0) != -1);
	ATF_REQUIRE(ptrace(PT_CONTINUE, children[1], (caddr_t)1, 0) != -1);

	/*
	 * The child can't exit until the grandchild reports status, so the
	 * grandchild should report its exit first to the debugger.
	 */
	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[1]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 2);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == children[0]);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

static void *
simple_thread(void *arg __unused)
{

	pthread_exit(NULL);
}

static __dead2 void
simple_thread_main(void)
{
	pthread_t thread;

	CHILD_REQUIRE(pthread_create(&thread, NULL, simple_thread, NULL) == 0);
	CHILD_REQUIRE(pthread_join(thread, NULL) == 0);
	exit(1);
}

/*
 * Verify that pl_syscall_code in struct ptrace_lwpinfo for a new
 * thread reports the correct value.
 */
ATF_TC_WITHOUT_HEAD(ptrace__new_child_pl_syscall_code_thread);
ATF_TC_BODY(ptrace__new_child_pl_syscall_code_thread, tc)
{
	struct ptrace_lwpinfo pl;
	pid_t fpid, wpid;
	lwpid_t mainlwp;
	int status;

	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		trace_me();
		simple_thread_main();
	}

	/* The first wait() should report the stop from SIGSTOP. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(wpid == fpid);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);

	ATF_REQUIRE(ptrace(PT_LWPINFO, wpid, (caddr_t)&pl,
	    sizeof(pl)) != -1);
	mainlwp = pl.pl_lwpid;

	/*
	 * Continue the child ignoring the SIGSTOP and tracing all
	 * system call exits.
	 */
	ATF_REQUIRE(ptrace(PT_TO_SCX, fpid, (caddr_t)1, 0) != -1);

	/*
	 * Wait for the new thread to arrive.  pthread_create() might
	 * invoke any number of system calls.  For now we just wait
	 * for the new thread to arrive and make sure it reports a
	 * valid system call code.  If ptrace grows thread event
	 * reporting then this test can be made more precise.
	 */
	for (;;) {
		wpid = waitpid(fpid, &status, 0);
		ATF_REQUIRE(wpid == fpid);
		ATF_REQUIRE(WIFSTOPPED(status));
		ATF_REQUIRE(WSTOPSIG(status) == SIGTRAP);
		
		ATF_REQUIRE(ptrace(PT_LWPINFO, wpid, (caddr_t)&pl,
		    sizeof(pl)) != -1);
		ATF_REQUIRE((pl.pl_flags & PL_FLAG_SCX) != 0);
		ATF_REQUIRE(pl.pl_syscall_code != 0);
		if (pl.pl_lwpid != mainlwp)
			/* New thread seen. */
			break;

		ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);
	}

	/* Wait for the child to exit. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);
	for (;;) {
		wpid = waitpid(fpid, &status, 0);
		ATF_REQUIRE(wpid == fpid);
		if (WIFEXITED(status))
			break;
		
		ATF_REQUIRE(WIFSTOPPED(status));
		ATF_REQUIRE(WSTOPSIG(status) == SIGTRAP);
		ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);
	}
		
	ATF_REQUIRE(WEXITSTATUS(status) == 1);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

/*
 * Verify that the expected LWP events are reported for a child thread.
 */
ATF_TC_WITHOUT_HEAD(ptrace__lwp_events);
ATF_TC_BODY(ptrace__lwp_events, tc)
{
	struct ptrace_lwpinfo pl;
	pid_t fpid, wpid;
	lwpid_t lwps[2];
	int status;

	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		trace_me();
		simple_thread_main();
	}

	/* The first wait() should report the stop from SIGSTOP. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(wpid == fpid);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);

	ATF_REQUIRE(ptrace(PT_LWPINFO, wpid, (caddr_t)&pl,
	    sizeof(pl)) != -1);
	lwps[0] = pl.pl_lwpid;

	ATF_REQUIRE(ptrace(PT_LWP_EVENTS, wpid, NULL, 1) == 0);

	/* Continue the child ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);

	/* The first event should be for the child thread's birth. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(wpid == fpid);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGTRAP);
		
	ATF_REQUIRE(ptrace(PT_LWPINFO, wpid, (caddr_t)&pl, sizeof(pl)) != -1);
	ATF_REQUIRE((pl.pl_flags & (PL_FLAG_BORN | PL_FLAG_SCX)) ==
	    (PL_FLAG_BORN | PL_FLAG_SCX));
	ATF_REQUIRE(pl.pl_lwpid != lwps[0]);
	lwps[1] = pl.pl_lwpid;

	ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);

	/* The next event should be for the child thread's death. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(wpid == fpid);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGTRAP);
		
	ATF_REQUIRE(ptrace(PT_LWPINFO, wpid, (caddr_t)&pl, sizeof(pl)) != -1);
	ATF_REQUIRE((pl.pl_flags & (PL_FLAG_EXITED | PL_FLAG_SCE)) ==
	    (PL_FLAG_EXITED | PL_FLAG_SCE));
	ATF_REQUIRE(pl.pl_lwpid == lwps[1]);

	ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);

	/* The last event should be for the child process's exit. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 1);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

static void *
exec_thread(void *arg __unused)
{

	execl("/usr/bin/true", "true", NULL);
	exit(127);
}

static __dead2 void
exec_thread_main(void)
{
	pthread_t thread;

	CHILD_REQUIRE(pthread_create(&thread, NULL, exec_thread, NULL) == 0);
	for (;;)
		sleep(60);
	exit(1);
}

/*
 * Verify that the expected LWP events are reported for a multithreaded
 * process that calls execve(2).
 */
ATF_TC_WITHOUT_HEAD(ptrace__lwp_events_exec);
ATF_TC_BODY(ptrace__lwp_events_exec, tc)
{
	struct ptrace_lwpinfo pl;
	pid_t fpid, wpid;
	lwpid_t lwps[2];
	int status;

	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		trace_me();
		exec_thread_main();
	}

	/* The first wait() should report the stop from SIGSTOP. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(wpid == fpid);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);

	ATF_REQUIRE(ptrace(PT_LWPINFO, wpid, (caddr_t)&pl,
	    sizeof(pl)) != -1);
	lwps[0] = pl.pl_lwpid;

	ATF_REQUIRE(ptrace(PT_LWP_EVENTS, wpid, NULL, 1) == 0);

	/* Continue the child ignoring the SIGSTOP. */
	ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);

	/* The first event should be for the child thread's birth. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(wpid == fpid);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGTRAP);
		
	ATF_REQUIRE(ptrace(PT_LWPINFO, wpid, (caddr_t)&pl, sizeof(pl)) != -1);
	ATF_REQUIRE((pl.pl_flags & (PL_FLAG_BORN | PL_FLAG_SCX)) ==
	    (PL_FLAG_BORN | PL_FLAG_SCX));
	ATF_REQUIRE(pl.pl_lwpid != lwps[0]);
	lwps[1] = pl.pl_lwpid;

	ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);

	/*
	 * The next event should be for the main thread's death due to
	 * single threading from execve().
	 */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(wpid == fpid);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGTRAP);

	ATF_REQUIRE(ptrace(PT_LWPINFO, wpid, (caddr_t)&pl, sizeof(pl)) != -1);
	ATF_REQUIRE((pl.pl_flags & (PL_FLAG_EXITED | PL_FLAG_SCE)) ==
	    (PL_FLAG_EXITED));
	ATF_REQUIRE(pl.pl_lwpid == lwps[0]);

	ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);

	/* The next event should be for the child process's exec. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGTRAP);

	ATF_REQUIRE(ptrace(PT_LWPINFO, wpid, (caddr_t)&pl, sizeof(pl)) != -1);
	ATF_REQUIRE((pl.pl_flags & (PL_FLAG_EXEC | PL_FLAG_SCX)) ==
	    (PL_FLAG_EXEC | PL_FLAG_SCX));
	ATF_REQUIRE(pl.pl_lwpid == lwps[1]);

	ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);

	/* The last event should be for the child process's exit. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 0);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

static void
handler(int sig __unused)
{
}

static void
signal_main(void)
{

	signal(SIGINFO, handler);
	raise(SIGINFO);
	exit(0);
}

/*
 * Verify that the expected ptrace event is reported for a signal.
 */
ATF_TC_WITHOUT_HEAD(ptrace__siginfo);
ATF_TC_BODY(ptrace__siginfo, tc)
{
	struct ptrace_lwpinfo pl;
	pid_t fpid, wpid;
	int status;

	ATF_REQUIRE((fpid = fork()) != -1);
	if (fpid == 0) {
		trace_me();
		signal_main();
	}

	/* The first wait() should report the stop from SIGSTOP. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(wpid == fpid);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);

	ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);

	/* The next event should be for the SIGINFO. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGINFO);

	ATF_REQUIRE(ptrace(PT_LWPINFO, wpid, (caddr_t)&pl, sizeof(pl)) != -1);
	ATF_REQUIRE(pl.pl_event == PL_EVENT_SIGNAL);
	ATF_REQUIRE(pl.pl_flags & PL_FLAG_SI);
	ATF_REQUIRE(pl.pl_siginfo.si_code == SI_LWP);
	ATF_REQUIRE(pl.pl_siginfo.si_pid == wpid);

	ATF_REQUIRE(ptrace(PT_CONTINUE, fpid, (caddr_t)1, 0) == 0);

	/* The last event should be for the child process's exit. */
	wpid = waitpid(fpid, &status, 0);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 0);

	wpid = wait(&status);
	ATF_REQUIRE(wpid == -1);
	ATF_REQUIRE(errno == ECHILD);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, ptrace__parent_wait_after_trace_me);
	ATF_TP_ADD_TC(tp, ptrace__parent_wait_after_attach);
	ATF_TP_ADD_TC(tp, ptrace__parent_sees_exit_after_child_debugger);
	ATF_TP_ADD_TC(tp, ptrace__parent_sees_exit_after_unrelated_debugger);
	ATF_TP_ADD_TC(tp, ptrace__follow_fork_both_attached);
	ATF_TP_ADD_TC(tp, ptrace__follow_fork_child_detached);
	ATF_TP_ADD_TC(tp, ptrace__follow_fork_parent_detached);
	ATF_TP_ADD_TC(tp, ptrace__follow_fork_both_attached_unrelated_debugger);
	ATF_TP_ADD_TC(tp,
	    ptrace__follow_fork_child_detached_unrelated_debugger);
	ATF_TP_ADD_TC(tp,
	    ptrace__follow_fork_parent_detached_unrelated_debugger);
	ATF_TP_ADD_TC(tp, ptrace__getppid);
	ATF_TP_ADD_TC(tp, ptrace__new_child_pl_syscall_code_fork);
	ATF_TP_ADD_TC(tp, ptrace__new_child_pl_syscall_code_vfork);
	ATF_TP_ADD_TC(tp, ptrace__new_child_pl_syscall_code_thread);
	ATF_TP_ADD_TC(tp, ptrace__lwp_events);
	ATF_TP_ADD_TC(tp, ptrace__lwp_events_exec);
	ATF_TP_ADD_TC(tp, ptrace__siginfo);

	return (atf_no_error());
}
