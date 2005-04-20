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
RCSID("$OpenBSD: sshpty.c,v 1.11 2004/01/11 21:55:06 deraadt Exp $");

#ifdef HAVE_UTIL_H
# include <util.h>
#endif /* HAVE_UTIL_H */

#include "sshpty.h"
#include "log.h"
#include "misc.h"

#ifdef HAVE_PTY_H
# include <pty.h>
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
	/* openpty(3) exists in OSF/1 and some other os'es */
	char *name;
	int i;

	i = openpty(ptyfd, ttyfd, NULL, NULL, NULL);
	if (i < 0) {
		error("openpty: %.100s", strerror(errno));
		return 0;
	}
	name = ttyname(*ttyfd);
	if (!name)
		fatal("openpty returns device for which ttyname fails.");

	strlcpy(namebuf, name, namebuflen);	/* possible truncation */
	return 1;
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

/* Makes the tty the process's controlling tty and sets it to sane modes. */

void
pty_make_controlling_tty(int *ttyfd, const char *ttyname)
{
	int fd;
#ifdef USE_VHANGUP
	void *old;
#endif /* USE_VHANGUP */

#ifdef _UNICOS
	if (setsid() < 0)
		error("setsid: %.100s", strerror(errno));

	fd = open(ttyname, O_RDWR|O_NOCTTY);
	if (fd != -1) {
		signal(SIGHUP, SIG_IGN);
		ioctl(fd, TCVHUP, (char *)NULL);
		signal(SIGHUP, SIG_DFL);
		setpgid(0, 0);
		close(fd);
	} else {
		error("Failed to disconnect from controlling tty.");
	}

	debug("Setting controlling tty using TCSETCTTY.");
	ioctl(*ttyfd, TCSETCTTY, NULL);
	fd = open("/dev/tty", O_RDWR);
	if (fd < 0)
		error("%.100s: %.100s", ttyname, strerror(errno));
	close(*ttyfd);
	*ttyfd = fd;
#else /* _UNICOS */

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
#ifdef HAVE_NEWS4
	if (setpgrp(0,0) < 0)
		error("SETPGRP %s",strerror(errno));
#endif /* HAVE_NEWS4 */
#ifdef USE_VHANGUP
	old = signal(SIGHUP, SIG_IGN);
	vhangup();
	signal(SIGHUP, old);
#endif /* USE_VHANGUP */
	fd = open(ttyname, O_RDWR);
	if (fd < 0) {
		error("%.100s: %.100s", ttyname, strerror(errno));
	} else {
#ifdef USE_VHANGUP
		close(*ttyfd);
		*ttyfd = fd;
#else /* USE_VHANGUP */
		close(fd);
#endif /* USE_VHANGUP */
	}
	/* Verify that we now have a controlling tty. */
	fd = open(_PATH_TTY, O_WRONLY);
	if (fd < 0)
		error("open /dev/tty failed - could not set controlling tty: %.100s",
		    strerror(errno));
	else
		close(fd);
#endif /* _UNICOS */
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
	 * Warn but continue if filesystem is read-only and the uids match/
	 * tty is owned by root.
	 */
	if (stat(ttyname, &st))
		fatal("stat(%.100s) failed: %.100s", ttyname,
		    strerror(errno));

	if (st.st_uid != pw->pw_uid || st.st_gid != gid) {
		if (chown(ttyname, pw->pw_uid, gid) < 0) {
			if (errno == EROFS &&
			    (st.st_uid == pw->pw_uid || st.st_uid == 0))
				debug("chown(%.100s, %u, %u) failed: %.100s",
				    ttyname, (u_int)pw->pw_uid, (u_int)gid,
				    strerror(errno));
			else
				fatal("chown(%.100s, %u, %u) failed: %.100s",
				    ttyname, (u_int)pw->pw_uid, (u_int)gid,
				    strerror(errno));
		}
	}

	if ((st.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO)) != mode) {
		if (chmod(ttyname, mode) < 0) {
			if (errno == EROFS &&
			    (st.st_mode & (S_IRGRP | S_IROTH)) == 0)
				debug("chmod(%.100s, 0%o) failed: %.100s",
				    ttyname, (u_int)mode, strerror(errno));
			else
				fatal("chmod(%.100s, 0%o) failed: %.100s",
				    ttyname, (u_int)mode, strerror(errno));
		}
	}
}
