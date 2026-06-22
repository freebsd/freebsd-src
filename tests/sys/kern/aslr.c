/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from the
 * FreeBSD Foundation.
 */

#include <sys/param.h>
#include <sys/procctl.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <libutil.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>
#include "freebsd_test_suite/macros.h"

/*
 * Spawn an unprivileged child with ASLR force-disabled, which then execs
 * /sbin/ping (setuid root).
 */
static pid_t
spawn_ping(const atf_tc_t *tc)
{
	const char *user;
	struct passwd *passwd;
	pid_t child;
	int arg, error, ctl[2];
	char dummy;

	user = atf_tc_get_config_var(tc, "unprivileged_user");
	passwd = getpwnam(user);
	ATF_REQUIRE(passwd != NULL);

	ATF_REQUIRE(pipe2(ctl, O_CLOEXEC) == 0);
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		if (close(ctl[0]) != 0 ||
		    close(STDOUT_FILENO) != 0 ||
		    open("/dev/null", O_WRONLY | O_APPEND) != STDOUT_FILENO ||
		    seteuid(passwd->pw_uid) != 0)
			_exit(1);

		arg = PROC_ASLR_FORCE_DISABLE;
		error = procctl(P_PID, getpid(), PROC_ASLR_CTL, &arg);
		if (error != 0)
			_exit(2);

		execl("/sbin/ping", "ping", "127.0.0.1", NULL);
		_exit(127);
	}
	ATF_REQUIRE(close(ctl[1]) == 0);
	ATF_REQUIRE(read(ctl[0], &dummy, 1) == 0);
	ATF_REQUIRE(close(ctl[0]) == 0);

	return (child);
}

/*
 * Return the base address of the first mapping backed by the specified
 * executable in the given process, or 0 if not found.
 */
static uint64_t
text_base(pid_t pid, const char *path)
{
	struct kinfo_vmentry *vmmap;
	uint64_t base;
	int cnt;

	base = 0;
	vmmap = kinfo_getvmmap(pid, &cnt);
	if (vmmap == NULL)
		return (0);
	for (int i = 0; i < cnt; i++) {
		if (vmmap[i].kve_type == KVME_TYPE_VNODE &&
		    strcmp(vmmap[i].kve_path, path) == 0) {
			base = vmmap[i].kve_start;
			break;
		}
	}
	free(vmmap);
	return (base);
}

/*
 * Make sure that ASLR can't be disabled for a setuid executable by an
 * unprivileged user.
 */
ATF_TC(aslr_setuid);
ATF_TC_HEAD(aslr_setuid, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.config", "unprivileged_user");
}
ATF_TC_BODY(aslr_setuid, tc)
{
	struct stat sb;
	uint64_t bases[5];
	pid_t child, pid;
	int arg, error, st;

	ATF_REQUIRE_FEATURE("inet");
	if (!atf_tc_has_config_var(tc, "unprivileged_user"))
		atf_tc_skip("unprivileged_user not set");

	error = stat("/sbin/ping", &sb);
	ATF_REQUIRE(error == 0);
	ATF_REQUIRE_MSG(sb.st_uid == 0 && (sb.st_mode & S_ISUID) != 0,
	    "/sbin/ping is not setuid root");

	child = spawn_ping(tc);
	bases[0] = text_base(child, "/sbin/ping");
	ATF_REQUIRE_MSG(bases[0] != 0,
	    "failed to find /sbin/ping text segment");

	arg = 0;
	error = procctl(P_PID, child, PROC_ASLR_STATUS, &arg);
	ATF_REQUIRE_MSG(error == 0, "procctl ASLR_STATUS failed: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG((arg & PROC_ASLR_ACTIVE) != 0,
	    "ASLR is not active for setuid child");
	ATF_REQUIRE_MSG((arg & ~PROC_ASLR_ACTIVE) == PROC_ASLR_NOFORCE,
	    "expected NOFORCE for setuid child, got %d",
	    arg & ~PROC_ASLR_ACTIVE);

	error = kill(child, SIGTERM);
	ATF_REQUIRE(error == 0);
	pid = waitpid(child, &st, 0);
	ATF_REQUIRE(pid == child);
	ATF_REQUIRE(WIFSIGNALED(st) && WTERMSIG(st) == SIGTERM);

	for (size_t i = 1; i < nitems(bases); i++) {
		child = spawn_ping(tc);
		bases[i] = text_base(child, "/sbin/ping");
		ATF_REQUIRE_MSG(bases[i] != 0,
		    "failed to find /sbin/ping text segment");
		error = kill(child, SIGTERM);
		ATF_REQUIRE(error == 0);
		pid = waitpid(child, &st, 0);
		ATF_REQUIRE(pid == child);
		ATF_REQUIRE(WIFSIGNALED(st) && WTERMSIG(st) == SIGTERM);
	}

	/* Verify that the text base is different across all runs. */
	for (size_t i = 0; i < nitems(bases); i++) {
		for (size_t j = i + 1; j < nitems(bases); j++) {
			ATF_REQUIRE_MSG(bases[i] != bases[j],
			    "ping text base collision 0x%jx",
			    (uintmax_t)bases[i]);
		}
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, aslr_setuid);

	return (atf_no_error());
}
