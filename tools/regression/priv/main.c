/*-
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson for the TrustedBSD
 * Project under contract to nCircle Network Security, Inc.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR, NCIRCLE NETWORK SECURITY,
 * INC., OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Privilege test framework.  Each test is encapsulated on a .c file exporting
 * a function that implements the test.  Each test is run from its own child
 * process, and they are run in sequence one at a time.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "main.h"

/*
 * Common routines used across many tests.
 */
void
assert_root(void)
{

	if (getuid() != UID_ROOT || geteuid() != UID_ROOT)
		err(-1, "must be run as root");
}

void
setup_file(char *fpathp, uid_t uid, gid_t gid, mode_t mode)
{
	int fd;

	strcpy(fpathp, "/tmp/priv.XXXXXXXXXXX");
	fd = mkstemp(fpathp);
	if (fd < 0)
		err(-1, "mkstemp");

	if (fchown(fd, uid, gid) < 0)
		err(-1, "fchown(%s, %d, %d)", fpathp, uid, gid);

	if (fchmod(fd, mode) < 0)
		err(-1, "chmod(%s, 0%o)", fpathp, mode);

	close(fd);
}

/*
 * When downgrading privileges, set the gid before the uid; when upgrading,
 * set uid before gid.
 */
void
set_creds(uid_t uid, gid_t gid)
{

	if (setegid(gid) < 0)
		err(-1, "setegid(%d)", gid);
	if (seteuid(uid) < 0)
		err(-1, "seteuid(%d)", uid);
}

void
set_euid(uid_t uid)
{

	if (seteuid(uid) < 0)
		err(-1, "seteuid(%d)", uid);
}

void
restore_creds(void)
{

	if (seteuid(UID_ROOT) < 0)
		err(-1, "seteuid(%d)", UID_ROOT);
	if (setegid(GID_WHEEL) < 0)
		err(-1, "setegid(%d)", GID_WHEEL);
}

/*
 * Execute tests in a child process so they don't contaminate each other,
 * especially with regard to file descriptors, credentials, working
 * directories, and chroot status.
 */
static void
run(const char *funcname, void (*func)(void))
{
	pid_t childpid, pid;

	printf("running %s\n", funcname);
	fflush(stdout);
	fflush(stderr);
	childpid = fork();
	if (childpid == -1)
		err(-1, "test %s unable to fork", funcname);
	if (childpid == 0) {
		setprogname(funcname);
		func();
		fflush(stdout);
		fflush(stderr);
		exit(0);
	} else {
		while (1) {
			pid = waitpid(childpid, NULL, 0);
			if (pid == -1)
				warn("waitpid %s", funcname);
			if (pid == childpid)
				break;
		}
	}
	fflush(stdout);
	fflush(stderr);
}

int
main(int argc, char *argv[])
{

	run("priv_acct", priv_acct);
	run("priv_adjtime", priv_adjtime);
	run("priv_clock_settime", priv_clock_settime);
	run("priv_io", priv_io);
	run("priv_kenv_set", priv_kenv_set);
	run("priv_kenv_unset", priv_kenv_unset);
	run("priv_proc_setlogin", priv_proc_setlogin);
	run("priv_proc_setrlimit", priv_proc_setrlimit);
	run("priv_sched_rtprio", priv_sched_rtprio);
	run("priv_sched_setpriority", priv_sched_setpriority);
	run("priv_settimeofday", priv_settimeofday);
	run("priv_sysctl_write", priv_sysctl_write);
	run("priv_vfs_admin", priv_vfs_admin);
	run("priv_vfs_chown", priv_vfs_chown);
	run("priv_vfs_chroot", priv_vfs_chroot);
	run("priv_vfs_clearsugid", priv_vfs_clearsugid);
	run("priv_vfs_extattr_system", priv_vfs_extattr_system);
	run("priv_vfs_fhopen", priv_vfs_fhopen);
	run("priv_vfs_fhstat", priv_vfs_fhstat);
	run("priv_vfs_fhstatfs", priv_vfs_fhstatfs);
	run("priv_vfs_generation", priv_vfs_generation);
	run("priv_vfs_getfh", priv_vfs_getfh);
	run("priv_vfs_read", priv_vfs_read);
	run("priv_vfs_setgid", priv_vfs_setgid);
	run("priv_vfs_stickyfile", priv_vfs_stickyfile);
	run("priv_vfs_write", priv_vfs_write);
	run("priv_vm_madv_protect", priv_vm_madv_protect);
	run("priv_vm_mlock", priv_vm_mlock);
	run("priv_vm_munlock", priv_vm_munlock);

	run("test_utimes", test_utimes);

	return (0);
}
