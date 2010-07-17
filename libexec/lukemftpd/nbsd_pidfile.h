/* $FreeBSD: src/libexec/lukemftpd/nbsd_pidfile.h,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */

#include <sys/stdint.h>
#include <sysexits.h>

static int
pidfile(const char *basename)
{
	struct pidfh *pfh;
	pid_t otherpid, childpid;

	if (basename != NULL) {
		errx(EX_USAGE, "Need to impliment NetBSD semantics.");
	}

	pfh = pidfile_open(basename, 0644, &otherpid);
	if (pfh == NULL) {
		if (errno == EEXIST) {
			errx(EXIT_FAILURE, "Daemon already running, pid: %jd.",
			    (intmax_t)otherpid);
		}
		/* If we cannot create pidfile from other reasons, only warn. */
		warn("Cannot open or create pidfile");
		return -1;
	}

	pidfile_write(pfh);
	pidfile_close(pfh);
	return 0;
}
