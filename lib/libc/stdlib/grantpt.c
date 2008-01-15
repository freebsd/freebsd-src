/*
 * Copyright (c) 2002 The FreeBSD Project, Inc.
 * All rights reserved.
 *
 * This software includes code contributed to the FreeBSD Project
 * by Ryan Younce of North Carolina State University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the FreeBSD Project nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE FREEBSD PROJECT OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__FBSDID("$FreeBSD$");
#endif /* not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include "un-namespace.h"

#define PTYM_PREFIX	"pty"	/* pty(4) master naming convention */
#define PTYS_PREFIX	"tty"	/* pty(4) slave naming convention */
#define PTMXM_PREFIX	"ptc/"	/* pts(4) master naming convention */
#define PTMXS_PREFIX	"pts/"	/* pts(4) slave naming convention */
#define PTMX		"ptmx"

/*
 * The following are range values for pseudo TTY devices.  Pseudo TTYs have a
 * name of /dev/[pt]ty[l-sL-S][0-9a-v], yielding 256 combinations per major.
 */
#define PTY_MAX		256
#define	PTY_DEV1	"pqrsPQRSlmnoLMNO"
#define PTY_DEV2	"0123456789abcdefghijklmnopqrstuv"

/*
 * grantpt(3) support utility.
 */
#define _PATH_PTCHOWN	"/usr/libexec/pt_chown"

/*
 * ISPTM(x) returns 0 for struct stat x if x is not a pty master.
 * The bounds checking may be unnecessary but it does eliminate doubt.
 */
#define ISPTM(x)	(S_ISCHR((x).st_mode) && 			\
			 minor((x).st_rdev) >= 0 &&			\
			 minor((x).st_rdev) < PTY_MAX)


static int
is_pts(int fd)
{
	int nb;

	return (_ioctl(fd, TIOCGPTN, &nb) == 0);
}

int
__use_pts(void)
{
	int use_pts;
	size_t len;
	int error;

	len = sizeof(use_pts);
	error = sysctlbyname("kern.pts.enable", &use_pts, &len, NULL, 0);
	if (error) {
		struct stat sb;

		if (stat(_PATH_DEV PTMX, &sb) != 0)
			return (0);
		use_pts = 1;
	}
	return (use_pts);
}

/*
 * grantpt():  grant ownership of a slave pseudo-terminal device to the
 *             current user.
 */

int
grantpt(int fildes)
{
	int retval, serrno, status;
	pid_t pid, spid;
	gid_t gid;
	char *slave;
	sigset_t oblock, nblock;
	struct group *grp;

	retval = -1;
	serrno = errno;

	if ((slave = ptsname(fildes)) != NULL) {
		/*
		 * Block SIGCHLD.
		 */
		(void)sigemptyset(&nblock);
		(void)sigaddset(&nblock, SIGCHLD);
		(void)_sigprocmask(SIG_BLOCK, &nblock, &oblock);

		switch (pid = fork()) {
		case -1:
			break;
		case 0:		/* child */
			/*
			 * pt_chown expects the master pseudo TTY to be its
			 * standard input.
			 */
			(void)_dup2(fildes, STDIN_FILENO);
			(void)_sigprocmask(SIG_SETMASK, &oblock, NULL);
			execl(_PATH_PTCHOWN, _PATH_PTCHOWN, (char *)NULL);
			_exit(EX_UNAVAILABLE);
			/* NOTREACHED */
		default:	/* parent */
			/*
			 * Just wait for the process.  Error checking is
			 * done below.
			 */
			while ((spid = _waitpid(pid, &status, 0)) == -1 &&
			       (errno == EINTR))
				;
			if (spid != -1 && WIFEXITED(status) &&
			    WEXITSTATUS(status) == EX_OK)
				retval = 0;
			else
				errno = EACCES;
			break;
		}

		/*
		 * Restore process's signal mask.
		 */
		(void)_sigprocmask(SIG_SETMASK, &oblock, NULL);

		if (retval) {
			/*
			 * pt_chown failed.  Try to manually change the
			 * permissions for the slave.
			 */
			gid = (grp = getgrnam("tty")) ? grp->gr_gid : -1;
			if (chown(slave, getuid(), gid) == -1 ||
			    chmod(slave, S_IRUSR | S_IWUSR | S_IWGRP) == -1)
				errno = EACCES;
			else
				retval = 0;
		}
	}

	if (!retval)
		errno = serrno;

	return (retval);
}

/*
 * posix_openpt():  open the first available master pseudo-terminal device
 *                  and return descriptor.
 */
int
posix_openpt(int oflag)
{
	char *mc1, *mc2, master[] = _PATH_DEV PTYM_PREFIX "XY";
	const char *pc1, *pc2;
	int fildes, bflag, serrno;

	fildes = -1;
	bflag = 0;
	serrno = errno;

	/*
	 * Check flag validity.  POSIX doesn't require it,
	 * but we still do so.
	 */
	if (oflag & ~(O_RDWR | O_NOCTTY))
		errno = EINVAL;
	else {
		if (__use_pts()) {
			fildes = _open(_PATH_DEV PTMX, oflag);
			return (fildes);
		}
		mc1 = master + strlen(_PATH_DEV PTYM_PREFIX);
		mc2 = mc1 + 1;

		/* Cycle through all possible master PTY devices. */
		for (pc1 = PTY_DEV1; !bflag && (*mc1 = *pc1); ++pc1)
			for (pc2 = PTY_DEV2; (*mc2 = *pc2) != '\0'; ++pc2) {
				/*
				 * Break out if we successfully open a PTY,
				 * or if open() fails due to limits.
				 */
				if ((fildes = _open(master, oflag)) != -1 ||
				    (errno == EMFILE || errno == ENFILE)) {
					++bflag;
					break;
				}
			}

		if (fildes != -1)
			errno = serrno;
		else if (!bflag)
			errno = EAGAIN;
	}

	return (fildes);
}

/*
 * ptsname():  return the pathname of the slave pseudo-terminal device
 *             associated with the specified master.
 */
char *
ptsname(int fildes)
{
	static char pty_slave[] = _PATH_DEV PTYS_PREFIX "XY";
	static char ptmx_slave[] = _PATH_DEV PTMXS_PREFIX "4294967295";
	char *retval;
	struct stat sbuf;

	retval = NULL;

	if (_fstat(fildes, &sbuf) == 0) {
		if (!ISPTM(sbuf))
			errno = EINVAL;
		else {
			if (!is_pts(fildes)) {
				(void)snprintf(pty_slave, sizeof(pty_slave),
					       _PATH_DEV PTYS_PREFIX "%s",
					       devname(sbuf.st_rdev, S_IFCHR) +
					       strlen(PTYM_PREFIX));
				retval = pty_slave;
			} else {
				(void)snprintf(ptmx_slave, sizeof(ptmx_slave),
					       _PATH_DEV PTMXS_PREFIX "%s",
					       devname(sbuf.st_rdev, S_IFCHR) +
					       strlen(PTMXM_PREFIX));
				retval = ptmx_slave;
			}
		}
	}

	return (retval);
}

/*
 * unlockpt():  unlock a pseudo-terminal device pair.
 */
int
unlockpt(int fildes)
{
	int retval;
	struct stat sbuf;

	/*
	 * Unlocking a master/slave pseudo-terminal pair has no meaning in a
	 * non-streams PTY environment.  However, we do ensure fildes is a
	 * valid master pseudo-terminal device.
	 */
	if ((retval = _fstat(fildes, &sbuf)) == 0 && !ISPTM(sbuf)) {
		errno = EINVAL;
		retval = -1;
	}

	return (retval);
}
