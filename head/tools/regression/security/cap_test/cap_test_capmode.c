/*-
 * Copyright (c) 2008-2009 Robert N. M. Watson
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
 *
 * $FreeBSD$
 */

/*
 * Test routines to make sure a variety of system calls are or are not
 * available in capability mode.  The goal is not to see if they work, just
 * whether or not they return the expected ECAPMODE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/sysarch.h>
#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Need to check machine-dependent sysarch(). */
#define	ARCH_IS(s)	(!strncmp(s, MACHINE, sizeof(s) + 1))

#include "cap_test.h"

void
test_capmode(void)
{
	struct sockaddr_in sin;
	struct statfs statfs;
	struct stat sb;
	ssize_t len;
	long sysarch_arg = 0;
	int fd, fd_close, fd_dir, fd_file, fd_socket, fd2[2], ret;
	pid_t pid, wpid;
	char ch;

	fd_file = open("/tmp/cap_test_syscalls", O_RDWR|O_CREAT, 0644);
	if (fd_file < 0)
		err(-1, "test_syscalls:prep: open cap_test_syscalls");

	fd_close = open("/dev/null", O_RDWR);
	if (fd_close < 0)
		err(-1, "test_syscalls:prep: open /dev/null");

	fd_dir = open("/tmp", O_RDONLY);
	if (fd_dir < 0)
		err(-1, "test_syscalls:prep: open /tmp");

	fd_socket = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd_socket < 0)
		err(-1, "test_syscalls:prep: socket");

	if (cap_enter() < 0)
		err(-1, "test_syscalls:prep: cap_enter");


	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;

	/*
	 * Here begin the tests, sorted roughly alphabetically by system call
	 * name.
	 */
	fd = accept(fd_socket, NULL, NULL);
	if (fd < 0) {
		if (errno == ECAPMODE)
			warnx("test_syscalls:accept");
	} else {
		warnx("test_syscalls:accept succeeded");
		close(fd);
	}

	if (access("/tmp/cap_test_syscalls_access", F_OK) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:access");
	} else
		warnx("test_syscalls:access succeeded");

	if (acct("/tmp/cap_test_syscalls_acct") < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:acct");
	} else
		warnx("test_syscalls:acct succeeded");

	if (bind(PF_INET, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscall:bind");
	} else
		warnx("test_syscall:bind succeeded");

	if (chdir("/tmp/cap_test_syscalls_chdir") < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:chdir");
	} else
		warnx("test_syscalls:chdir succeeded");

	if (chflags("/tmp/cap_test_syscalls_chflags", UF_NODUMP) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:chflags");
	} else
		warnx("test_syscalls:chflags succeeded");

	if (chmod("/tmp/cap_test_syscalls_chmod", 0644) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:chmod");
	} else
		warnx("test_syscalls:chmod succeeded");

	if (chown("/tmp/cap_test_syscalls_chown", -1, -1) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:chown");
	} else
		warnx("test_syscalls:chown succeeded");

	if (chroot("/tmp/cap_test_syscalls_chroot") < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:chroot");
	} else
		warnx("test_syscalls:chroot succeeded");

	if (close(fd_close)) {
		if (errno == ECAPMODE)
			warnx("test_syscalls:close");
		else
			warn("test_syscalls:close");
	}

	if (connect(PF_INET, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscall:connect");
	} else
		warnx("test_syscall:connect succeeded");

	fd = creat("/tmp/cap_test_syscalls_creat", 0644);
	if (fd >= 0) {
		warnx("test_syscalls:creat succeeded");
		close(fd);
	} else if (errno != ECAPMODE)
		warn("test_syscalls:creat");

	fd = dup(fd_file);
	if (fd < 0) {
		if (errno == ECAPMODE)
			warnx("test_syscalls:dup");
	} else
		close(fd);

	if (fchdir(fd_dir) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscall:fchdir");
	} else
		warnx("test_syscalls:fchdir succeeded");

	if (fchflags(fd_file, UF_NODUMP) < 0) {
		if (errno == ECAPMODE)
			warnx("test_syscall:fchflags");
	}

	pid = fork();
	if (pid >= 0) {
		if (pid == 0) {
			exit(0);
		} else if (pid > 0) {
			wpid = waitpid(pid, NULL, 0);
			if (wpid < 0) {
				if (errno != ECAPMODE)
					warn("test_syscalls:waitpid");
			} else
				warnx("test_syscalls:waitpid succeeded");
		}
	} else
		warn("test_syscalls:fork");

	if (fstat(fd_file, &sb) < 0) {
		if (errno == ECAPMODE)
			warnx("test_syscalls:fstat");
	}

	/*
	 * getegid() can't return an error but check for it anyway.
	 */
	errno = 0;
	(void)getegid();
	if (errno == ECAPMODE)
		warnx("test_syscalls:getegid");

	/*
	 * geteuid() can't return an error but check for it anyway.
	 */
	errno = 0;
	geteuid();
	if (errno == ECAPMODE)
		warnx("test_syscalls:geteuid");

	if (getfsstat(&statfs, sizeof(statfs), MNT_NOWAIT) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:getfsstat");
	} else
		warnx("test_syscalls:getfsstat succeeded");

	/*
	 * getgid() can't return an error but check for it anyway.
	 */
	errno = 0;
	getgid();
	if (errno == ECAPMODE)
		warnx("test_syscalls:getgid");

	if (getpeername(fd_socket, NULL, NULL) < 0) {
		if (errno == ECAPMODE)
			warnx("test_syscalls:getpeername");
	}

	if (getlogin() == NULL)
		warn("test_sycalls:getlogin %d", errno);

	/*
	 * getpid() can't return an error but check for it anyway.
	 */
	errno = 0;
	(void)getpid();
	if (errno == ECAPMODE)
		warnx("test_syscalls:getpid");

	/*
	 * getppid() can't return an error but check for it anyway.
	 */
	errno = 0;
	(void)getppid();
	if (errno == ECAPMODE)
		warnx("test_syscalls:getppid");

	if (getsockname(fd_socket, NULL, NULL) < 0) {
		if (errno == ECAPMODE)
			warnx("test_syscalls:getsockname");
	}

	/*
	 * getuid() can't return an error but check for it anyway.
	 */
	errno = 0;
	(void)getuid();
	if (errno == ECAPMODE)
		warnx("test_syscalls:getuid");

	/* XXXRW: ktrace */

	if (link("/tmp/foo", "/tmp/bar") < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:link");
	} else
		warnx("test_syscalls:link succeeded");

	ret = lseek(fd_file, SEEK_SET, 0);
	if (ret < 0) {
		if (errno == ECAPMODE)
			warnx("test_syscalls:lseek");
		else
			warn("test_syscalls:lseek");
	}

	if (lstat("/tmp/cap_test_syscalls_lstat", &sb) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:lstat");
	} else
		warnx("test_syscalls:lstat succeeded");

	if (mknod("/tmp/test_syscalls_mknod", 06440, 0) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:mknod");
	} else
		warnx("test_syscalls:mknod succeeded");

	/*
	 * mount() is a bit tricky but do our best.
	 */
	if (mount("procfs", "/not_mounted", 0, NULL) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:mount");
	} else
		warnx("test_syscalls:mount succeeded");

	if (msync(&fd_file, 8192, MS_ASYNC) < 0) {
		if (errno == ECAPMODE)
			warnx("test_syscalls:msync");
	}

	fd = open("/dev/null", O_RDWR);
	if (fd >= 0) {
		warnx("test_syscalls:open succeeded");
		close(fd);
	}

	if (pipe(fd2) == 0) {
		close(fd2[0]);
		close(fd2[1]);
	} else if (errno == ECAPMODE)
		warnx("test_syscalls:pipe");

	if (profil(NULL, 0, 0, 0) < 0) {
		if (errno == ECAPMODE)
			warnx("test_syscalls:profile");
	}

	/* XXXRW: ptrace. */

	len = read(fd_file, &ch, sizeof(ch));
	if (len < 0 && errno == ECAPMODE)
		warnx("test_syscalls:read");

	if (readlink("/tmp/cap_test_syscalls_readlink", NULL, 0) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:readlink");
	} else
		warnx("test_syscalls:readlink succeeded");

	len = recvfrom(fd_socket, NULL, 0, 0, NULL, NULL);
	if (len < 0 && errno == ECAPMODE)
		warnx("test_syscalls:recvfrom");

	len = recvmsg(fd_socket, NULL, 0);
	if (len < 0 && errno == ECAPMODE)
		warnx("test_syscalls:recvmsg");

	if (revoke("/tmp/cap_test_syscalls_revoke") < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:revoke");
	} else
		warnx("test_syscalls:revoke succeeded");

	len = sendmsg(fd_socket, NULL, 0);
	if (len < 0 && errno == ECAPMODE)
		warnx("test_syscalls:sendmsg");

	len = sendto(fd_socket, NULL, 0, 0, NULL, 0);
	if (len < 0 && errno == ECAPMODE)
		warn("test_syscalls:sendto(NULL)");

	if (setuid(getuid()) < 0) {
		if (errno == ECAPMODE)
			warnx("test_syscalls:setuid");
	}

	if (stat("/tmp/cap_test_syscalls_stat", &sb) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:stat");
	} else
		warnx("test_syscalls:stat succeeded");

	if (symlink("/tmp/cap_test_syscalls_symlink_from",
	    "/tmp/cap_test_syscalls_symlink_to") < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:symlink");
	} else
		warnx("test_syscalls:symlink succeeded");

	/* sysarch() is, by definition, architecture-dependent */
	if (ARCH_IS("i386") || ARCH_IS("amd64")) {
		if (sysarch(I386_SET_IOPERM, &sysarch_arg) != -1)
			warnx("test_syscalls:sysarch succeeded");
		else if (errno != ECAPMODE)
			warn("test_syscalls:sysarch errno != ECAPMODE");

		/* XXXJA: write a test for arm */
	} else {
		warnx("test_syscalls:no sysarch() test for architecture '%s'", MACHINE);
	}

	/* XXXRW: No error return from sync(2) to test. */

	if (unlink("/tmp/cap_test_syscalls_unlink") < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:unlink");
	} else
		warnx("test_syscalls:unlink succeeded");

	if (unmount("/not_mounted", 0) < 0) {
		if (errno != ECAPMODE)
			warn("test_syscalls:unmount");
	} else
		warnx("test_syscalls:unmount succeeded");

	len = write(fd_file, &ch, sizeof(ch));
	if (len < 0 && errno == ECAPMODE)
		warnx("test_syscalls:write");

	exit(0);
}
