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
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <errno.h>
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
follow_fork_parent(void)
{
	pid_t fpid, wpid;
	int status;

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
handle_fork_events(pid_t parent)
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
			fork_reported[1] = true;
		} else {
			ATF_REQUIRE(wpid == parent);
			ATF_REQUIRE(WSTOPSIG(status) == SIGTRAP);
			ATF_REQUIRE(!fork_reported[0]);
			if (child == -1)
				child = pl.pl_child_pid;
			else
				ATF_REQUIRE(child == pl.pl_child_pid);
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
		follow_fork_parent();
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

	children[1] = handle_fork_events(children[0]);
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
		follow_fork_parent();
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

	children[1] = handle_fork_events(children[0]);
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
		follow_fork_parent();
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

	children[1] = handle_fork_events(children[0]);
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
		follow_fork_parent();
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

	children[1] = handle_fork_events(children[0]);
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
		follow_fork_parent();
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

	children[1] = handle_fork_events(children[0]);
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
		follow_fork_parent();
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

	children[1] = handle_fork_events(children[0]);
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

	return (atf_no_error());
}
