/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Allocating a pseudo-terminal, and making it the controlling tty.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: sshpty.c,v 1.1 2001/03/04 01:46:30 djm Exp $");
RCSID("$FreeBSD$");

#include <libutil.h>
#include "sshpty.h"
#include "log.h"

/* Pty allocated with _getpty gets broken if we do I_PUSH:es to it. */
#if defined(HAVE__GETPTY) || defined(HAVE_OPENPTY)
#undef HAVE_DEV_PTMX
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

/*
 * Allocates and opens a pty.  Returns 0 if no pty could be allocated, or
 * nonzero if a pty was successfully allocated.  On success, open file
 * descriptors for the pty and tty sides and the name of the tty side are
 * returned (the buffer must be able to hold at least 64 characters).
 */

int
pty_allocate(int *ptyfd, int *ttyfd, char *namebuf, int namebuflen)
{
#if defined(HAVE_OPENPTY) || defined(BSD4_4)
	/* openpty(3) exists in OSF/1 and some other os'es */
	char buf[64];
	int i;

	i = openpty(ptyfd, ttyfd, buf, NULL, NULL);
	if (i < 0) {
		error("openpty: %.100s", strerror(errno));
		return 0;
	}
	strlcpy(namebuf, buf, namebuflen);	/* possible truncation */
	return 1;
#else /* HAVE_OPENPTY */
#ifdef HAVE__GETPTY
	/*
	 * _getpty(3) exists in SGI Irix 4.x, 5.x & 6.x -- it generates more
	 * pty's automagically when needed
	 */
	char *slave;

	slave = _getpty(ptyfd, O_RDWR, 0622, 0);
	if (slave == NULL) {
		error("_getpty: %.100s", strerror(errno));
		return 0;
	}
	strlcpy(namebuf, slave, namebuflen);
	/* Open the slave side. */
	*ttyfd = open(namebuf, O_RDWR | O_NOCTTY);
	if (*ttyfd < 0) {
		error("%.200s: %.100s", namebuf, strerror(errno));
		close(*ptyfd);
		return 0;
	}
	return 1;
#else /* HAVE__GETPTY */
#ifdef HAVE_DEV_PTMX
	/*
	 * This code is used e.g. on Solaris 2.x.  (Note that Solaris 2.3
	 * also has bsd-style ptys, but they simply do not work.)
	 */
	int ptm;
	char *pts;

	ptm = open("/dev/ptmx", O_RDWR | O_NOCTTY);
	if (ptm < 0) {
		error("/dev/ptmx: %.100s", strerror(errno));
		return 0;
	}
	if (grantpt(ptm) < 0) {
		error("grantpt: %.100s", strerror(errno));
		return 0;
	}
	if (unlockpt(ptm) < 0) {
		error("unlockpt: %.100s", strerror(errno));
		return 0;
	}
	pts = ptsname(ptm);
	if (pts == NULL)
		error("Slave pty side name could not be obtained.");
	strlcpy(namebuf, pts, namebuflen);
	*ptyfd = ptm;

	/* Open the slave side. */
	*ttyfd = open(namebuf, O_RDWR | O_NOCTTY);
	if (*ttyfd < 0) {
		error("%.100s: %.100s", namebuf, strerror(errno));
		close(*ptyfd);
		return 0;
	}
	/* Push the appropriate streams modules, as described in Solaris pts(7). */
	if (ioctl(*ttyfd, I_PUSH, "ptem") < 0)
		error("ioctl I_PUSH ptem: %.100s", strerror(errno));
	if (ioctl(*ttyfd, I_PUSH, "ldterm") < 0)
		error("ioctl I_PUSH ldterm: %.100s", strerror(errno));
	if (ioctl(*ttyfd, I_PUSH, "ttcompat") < 0)
		error("ioctl I_PUSH ttcompat: %.100s", strerror(errno));
	return 1;
#else /* HAVE_DEV_PTMX */
#ifdef HAVE_DEV_PTS_AND_PTC
	/* AIX-style pty code. */
	const char *name;

	*ptyfd = open("/dev/ptc", O_RDWR | O_NOCTTY);
	if (*ptyfd < 0) {
		error("Could not open /dev/ptc: %.100s", strerror(errno));
		return 0;
	}
	name = ttyname(*ptyfd);
	if (!name)
		fatal("Open of /dev/ptc returns device for which ttyname fails.");
	strlcpy(namebuf, name, namebuflen);
	*ttyfd = open(name, O_RDWR | O_NOCTTY);
	if (*ttyfd < 0) {
		error("Could not open pty slave side %.100s: %.100s",
		      name, strerror(errno));
		close(*ptyfd);
		return 0;
	}
	return 1;
#else /* HAVE_DEV_PTS_AND_PTC */
	/* BSD-style pty code. */
	char buf[64];
	int i;
	const char *ptymajors = "pqrstuvwxyzabcdefghijklmnoABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const char *ptyminors = "0123456789abcdef";
	int num_minors = strlen(ptyminors);
	int num_ptys = strlen(ptymajors) * num_minors;

	for (i = 0; i < num_ptys; i++) {
		snprintf(buf, sizeof buf, "/dev/pty%c%c", ptymajors[i / num_minors],
			 ptyminors[i % num_minors]);
		*ptyfd = open(buf, O_RDWR | O_NOCTTY);
		if (*ptyfd < 0)
			continue;
		snprintf(namebuf, namebuflen, "/dev/tty%c%c",
		    ptymajors[i / num_minors], ptyminors[i % num_minors]);

		/* Open the slave side. */
		*ttyfd = open(namebuf, O_RDWR | O_NOCTTY);
		if (*ttyfd < 0) {
			error("%.100s: %.100s", namebuf, strerror(errno));
			close(*ptyfd);
			return 0;
		}
		return 1;
	}
	return 0;
#endif /* HAVE_DEV_PTS_AND_PTC */
#endif /* HAVE_DEV_PTMX */
#endif /* HAVE__GETPTY */
#endif /* HAVE_OPENPTY */
}

/* Releases the tty.  Its ownership is returned to root, and permissions to 0666. */

void
pty_release(const char *ttyname)
{
	if (chown(ttyname, (uid_t) 0, (gid_t) 0) < 0)
		error("chown %.100s 0 0 failed: %.100s", ttyname, strerror(errno));
	if (chmod(ttyname, (mode_t) 0666) < 0)
		error("chmod %.100s 0666 failed: %.100s", ttyname, strerror(errno));
}

/* Makes the tty the processes controlling tty and sets it to sane modes. */

void
pty_make_controlling_tty(int *ttyfd, const char *ttyname)
{
	int fd;

	/* First disconnect from the old controlling tty. */
#ifdef TIOCNOTTY
	fd = open(_PATH_TTY, O_RDWR | O_NOCTTY);
	if (fd >= 0) {
		(void) ioctl(fd, TIOCNOTTY, NULL);
		close(fd);
	}
#endif /* TIOCNOTTY */
	if (setsid() < 0)
		error("setsid: %.100s", strerror(errno));

	/*
	 * Verify that we are successfully disconnected from the controlling
	 * tty.
	 */
	fd = open(_PATH_TTY, O_RDWR | O_NOCTTY);
	if (fd >= 0) {
		error("Failed to disconnect from controlling tty.");
		close(fd);
	}
	/* Make it our controlling tty. */
#ifdef TIOCSCTTY
	debug("Setting controlling tty using TIOCSCTTY.");
	if (ioctl(*ttyfd, TIOCSCTTY, NULL) < 0)
		error("ioctl(TIOCSCTTY): %.100s", strerror(errno));
#endif /* TIOCSCTTY */
	fd = open(ttyname, O_RDWR);
	if (fd < 0)
		error("%.100s: %.100s", ttyname, strerror(errno));
	else
		close(fd);

	/* Verify that we now have a controlling tty. */
	fd = open(_PATH_TTY, O_WRONLY);
	if (fd < 0)
		error("open /dev/tty failed - could not set controlling tty: %.100s",
		      strerror(errno));
	else {
		close(fd);
	}
}

/* Changes the window size associated with the pty. */

void
pty_change_window_size(int ptyfd, int row, int col,
		       int xpixel, int ypixel)
{
	struct winsize w;
	w.ws_row = row;
	w.ws_col = col;
	w.ws_xpixel = xpixel;
	w.ws_ypixel = ypixel;
	(void) ioctl(ptyfd, TIOCSWINSZ, &w);
}

void
pty_setowner(struct passwd *pw, const char *ttyname)
{
	struct group *grp;
	gid_t gid;
	mode_t mode;
	struct stat st;

	/* Determine the group to make the owner of the tty. */
	grp = getgrnam("tty");
	if (grp) {
		gid = grp->gr_gid;
		mode = S_IRUSR | S_IWUSR | S_IWGRP;
	} else {
		gid = pw->pw_gid;
		mode = S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH;
	}

	/*
	 * Change owner and mode of the tty as required.
	 * Warn but continue if filesystem is read-only and the uids match.
	 */
	if (stat(ttyname, &st))
		fatal("stat(%.100s) failed: %.100s", ttyname,
		    strerror(errno));

	if (st.st_uid != pw->pw_uid || st.st_gid != gid) {
		if (chown(ttyname, pw->pw_uid, gid) < 0) {
			if (errno == EROFS && st.st_uid == pw->pw_uid)
				error("chown(%.100s, %d, %d) failed: %.100s",
				      ttyname, pw->pw_uid, gid,
				      strerror(errno));
			else
				fatal("chown(%.100s, %d, %d) failed: %.100s",
				      ttyname, pw->pw_uid, gid,
				      strerror(errno));
		}
	}

	if ((st.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO)) != mode) {
		if (chmod(ttyname, mode) < 0) {
			if (errno == EROFS &&
			    (st.st_mode & (S_IRGRP | S_IROTH)) == 0)
				error("chmod(%.100s, 0%o) failed: %.100s",
				      ttyname, mode, strerror(errno));
			else
				fatal("chmod(%.100s, 0%o) failed: %.100s",
				      ttyname, mode, strerror(errno));
		}
	}
}
