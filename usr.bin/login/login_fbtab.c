/************************************************************************
* Copyright 1995 by Wietse Venema.  All rights reserved.
*
* This material was originally written and compiled by Wietse Venema at
* Eindhoven University of Technology, The Netherlands, in 1990, 1991,
* 1992, 1993, 1994 and 1995.
*
* Redistribution and use in source and binary forms are permitted
* provided that this entire copyright notice is duplicated in all such
* copies.
*
* This software is provided "as is" and without any expressed or implied
* warranties, including, without limitation, the implied warranties of
* merchantibility and fitness for any particular purpose.
************************************************************************/
/* $FreeBSD$ */
/*
    SYNOPSIS
	void login_fbtab(tty, uid, gid)
	char *tty;
	uid_t uid;
	gid_t gid;

    DESCRIPTION
	This module implements device security as described in the
	SunOS 4.1.x fbtab(5) and SunOS 5.x logindevperm(4) manual
	pages. The program first looks for /etc/fbtab. If that file
	cannot be opened it attempts to process /etc/logindevperm.
	We expect entries with the folowing format:

	    Comments start with a # and extend to the end of the line.

	    Blank lines or lines with only a comment are ignored.

	    All other lines consist of three fields delimited by
	    whitespace: a login device (/dev/console), an octal
	    permission number (0600), and a ":"-delimited list of
	    devices (/dev/kbd:/dev/mouse). All device names are
	    absolute paths. A path that ends in "*" refers to all
	    directory entries except "." and "..".

	    If the tty argument (relative path) matches a login device
	    name (absolute path), the permissions of the devices in the
	    ":"-delimited list are set as specified in the second
	    field, and their ownership is changed to that of the uid
	    and gid arguments.

    DIAGNOSTICS
	Problems are reported via the syslog daemon with severity
	LOG_ERR.

    BUGS
	This module uses strtok(3), which may cause conflicts with other
	uses of that same routine.

    AUTHOR
	Wietse Venema (wietse@wzv.win.tue.nl)
	Eindhoven University of Technology
	The Netherlands
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <glob.h>
#include <paths.h>
#include <unistd.h>
#include "pathnames.h"

void	login_protect	__P((char *, char *, int, uid_t, gid_t));
void	login_fbtab	__P((char *tty, uid_t uid, gid_t gid));

#define	WSPACE		" \t\n"

/* login_fbtab - apply protections specified in /etc/fbtab or logindevperm */

void
login_fbtab(tty, uid, gid)
char   *tty;
uid_t   uid;
gid_t   gid;
{
    FILE   *fp;
    char    buf[BUFSIZ];
    char   *devname;
    char   *cp;
    int     prot;
    char *table;

    if ((fp = fopen(table = _PATH_FBTAB, "r")) == 0
    && (fp = fopen(table = _PATH_LOGINDEVPERM, "r")) == 0)
	return;

    while (fgets(buf, sizeof(buf), fp)) {
	if ((cp = strchr(buf, '#')))
	    *cp = 0;				/* strip comment */
	if ((cp = devname = strtok(buf, WSPACE)) == 0)
	    continue;				/* empty or comment */
	if (strncmp(devname, _PATH_DEV, sizeof _PATH_DEV - 1) != 0
	       || (cp = strtok((char *) 0, WSPACE)) == 0
	       || *cp != '0'
	       || sscanf(cp, "%o", &prot) == 0
	       || prot == 0
	       || (prot & 0777) != prot
	       || (cp = strtok((char *) 0, WSPACE)) == 0) {
	    syslog(LOG_ERR, "%s: bad entry: %s", table, cp ? cp : "(null)");
	    continue;
	}
	if (strcmp(devname + 5, tty) == 0) {
	    for (cp = strtok(cp, ":"); cp; cp = strtok((char *) 0, ":")) {
		login_protect(table, cp, prot, uid, gid);
	    }
	}
    }
    fclose(fp);
}

/* login_protect - protect one device entry */

void
login_protect(table, pattern, mask, uid, gid)
	char	*table;
	char	*pattern;
	int	mask;
	uid_t	uid;
	gid_t	gid;
{
	glob_t  gl;
	char	*path;
	int     i;

	if (glob(pattern, GLOB_NOSORT, NULL, &gl) != 0)
		return;
	for (i = 0; i < gl.gl_pathc; i++) {
		path = gl.gl_pathv[i];
		/* clear flags of the device */
		if (chflags(path, 0) && errno != ENOENT && errno != EOPNOTSUPP)
			syslog(LOG_ERR, "%s: chflags(%s): %m", table, path);
		if (chmod(path, mask) && errno != ENOENT)
			syslog(LOG_ERR, "%s: chmod(%s): %m", table, path);
		if (chown(path, uid, gid) && errno != ENOENT)
			syslog(LOG_ERR, "%s: chown(%s): %m", table, path);
	}
	globfree(&gl);
}
