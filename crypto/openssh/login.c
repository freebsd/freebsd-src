/*
 * 
 * login.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Fri Mar 24 14:51:08 1995 ylo
 * 
 * This file performs some of the things login(1) normally does.  We cannot
 * easily use something like login -p -h host -f user, because there are
 * several different logins around, and it is hard to determined what kind of
 * login the current system has.  Also, we want to be able to execute commands
 * on a tty.
 * 
 */

#include "includes.h"
RCSID("$Id: login.c,v 1.11 2000/01/04 00:07:59 markus Exp $");

#include <util.h>
#include <utmp.h>
#include "ssh.h"

/*
 * Returns the time when the user last logged in.  Returns 0 if the
 * information is not available.  This must be called before record_login.
 * The host the user logged in from will be returned in buf.
 */

/*
 * Returns the time when the user last logged in (or 0 if no previous login
 * is found).  The name of the host used last time is returned in buf.
 */

unsigned long 
get_last_login_time(uid_t uid, const char *logname,
		    char *buf, unsigned int bufsize)
{
	struct lastlog ll;
	char *lastlog;
	int fd;

	lastlog = _PATH_LASTLOG;
	buf[0] = '\0';

	fd = open(lastlog, O_RDONLY);
	if (fd < 0)
		return 0;
	lseek(fd, (off_t) ((long) uid * sizeof(ll)), SEEK_SET);
	if (read(fd, &ll, sizeof(ll)) != sizeof(ll)) {
		close(fd);
		return 0;
	}
	close(fd);
	if (bufsize > sizeof(ll.ll_host) + 1)
		bufsize = sizeof(ll.ll_host) + 1;
	strncpy(buf, ll.ll_host, bufsize - 1);
	buf[bufsize - 1] = 0;
	return ll.ll_time;
}

/*
 * Records that the user has logged in.  I these parts of operating systems
 * were more standardized.
 */

void 
record_login(int pid, const char *ttyname, const char *user, uid_t uid,
	     const char *host, struct sockaddr * addr)
{
	int fd;
	struct lastlog ll;
	char *lastlog;
	struct utmp u;
	const char *utmp, *wtmp;

	/* Construct an utmp/wtmp entry. */
	memset(&u, 0, sizeof(u));
	strncpy(u.ut_line, ttyname + 5, sizeof(u.ut_line));
	u.ut_time = time(NULL);
	strncpy(u.ut_name, user, sizeof(u.ut_name));
	strncpy(u.ut_host, host, sizeof(u.ut_host));

	/* Figure out the file names. */
	utmp = _PATH_UTMP;
	wtmp = _PATH_WTMP;

	login(&u);
	lastlog = _PATH_LASTLOG;

	/* Update lastlog unless actually recording a logout. */
	if (strcmp(user, "") != 0) {
		/*
		 * It is safer to bzero the lastlog structure first because
		 * some systems might have some extra fields in it (e.g. SGI)
		 */
		memset(&ll, 0, sizeof(ll));

		/* Update lastlog. */
		ll.ll_time = time(NULL);
		strncpy(ll.ll_line, ttyname + 5, sizeof(ll.ll_line));
		strncpy(ll.ll_host, host, sizeof(ll.ll_host));
		fd = open(lastlog, O_RDWR);
		if (fd >= 0) {
			lseek(fd, (off_t) ((long) uid * sizeof(ll)), SEEK_SET);
			if (write(fd, &ll, sizeof(ll)) != sizeof(ll))
				log("Could not write %.100s: %.100s", lastlog, strerror(errno));
			close(fd);
		}
	}
}

/* Records that the user has logged out. */

void 
record_logout(int pid, const char *ttyname)
{
	const char *line = ttyname + 5;	/* /dev/ttyq8 -> ttyq8 */
	if (logout(line))
		logwtmp(line, "", "");
}
