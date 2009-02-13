/* $FreeBSD$ */

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
