/* $XConsortium: lndir.c,v 1.14 95/01/09 20:08:20 kaleb Exp $ */
/* $XFree86: xc/config/util/lndir.c,v 3.3 1995/01/28 15:41:09 dawes Exp $ */
/* Create shadow link tree (after X11R4 script of the same name)
   Mark Reinhold (mbr@lcs.mit.edu)/3 January 1990 */

/* Hacked somewhat by Jordan Hubbard, The FreeBSD Project, to make it */
/* an invokable function from sysinstall rather than a stand-alone binary */

/* 
Copyright (c) 1990,  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

*/

/* From the original /bin/sh script:

  Used to create a copy of the a directory tree that has links for all
  non-directories (except those named RCS, SCCS or CVS.adm).  If you are
  building the distribution on more than one machine, you should use
  this technique.

  If your master sources are located in /usr/local/src/X and you would like
  your link tree to be in /usr/local/src/new-X, do the following:

   	%  mkdir /usr/local/src/new-X
	%  cd /usr/local/src/new-X
   	%  lndir ../X
*/

#include <sys/param.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "sysinstall.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 2048
#endif

static char *rcurdir;
static char *curdir;

static int
equivalent(char *lname, char *rname)
{
    char *s;

    if (!strcmp(lname, rname))
	return 1;
    for (s = lname; *s && (s = strchr(s, '/')); s++) {
	while (s[1] == '/')
	    strcpy(s+1, s+2);
    }
    return !strcmp(lname, rname);
}


/* Recursively create symbolic links from the current directory to the "from"
   directory.  Assumes that files described by fs and ts are directories. */

static int
dodir(char *fn, struct stat *fs, struct stat *ts, int rel)
{
    DIR *df;
    struct dirent *dp;
    char buf[MAXPATHLEN + 1], *p;
    char symbuf[MAXPATHLEN + 1];
    struct stat sb, sc;
    int n_dirs;
    int symlen;
    char *ocurdir;

    if ((fs->st_dev == ts->st_dev) && (fs->st_ino == ts->st_ino))
	return 1;

    if (rel)
	strcpy (buf, "../");
    else
	buf[0] = '\0';
    strcat (buf, fn);

    if (!(df = opendir (buf))) {
	msgDebug("%s: Cannot opendir\n", buf);
	return 1;
    }

    p = buf + strlen (buf);
    *p++ = '/';
    n_dirs = fs->st_nlink;
    while ((dp = readdir (df)) != NULL) {
	if (dp->d_name[strlen(dp->d_name) - 1] == '~')
	    continue;
	strcpy (p, dp->d_name);

	if (n_dirs > 0) {
	    if (stat (buf, &sb) < 0) {
		msgDebug("Can't stat: %s\n", buf);
		continue;
	    }

	    if (S_ISDIR(sb.st_mode)) {
		/* directory */
		n_dirs--;
		if (dp->d_name[0] == '.' &&
		    (dp->d_name[1] == '\0' || (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
		    continue;
		if (!strcmp (dp->d_name, "RCS"))
		    continue;
		if (!strcmp (dp->d_name, "SCCS"))
		    continue;
		if (!strcmp (dp->d_name, "CVS"))
		    continue;
		if (!strcmp (dp->d_name, "CVS.adm"))
		    continue;
		ocurdir = rcurdir;
		rcurdir = buf;
		curdir = isDebug() ? buf : (char *)0;
		if (isDebug())
		    msgDebug("%s:\n", buf);
		if ((stat(dp->d_name, &sc) < 0) && (errno == ENOENT)) {
		    if (mkdir(dp->d_name, 0777) < 0 ||
			stat (dp->d_name, &sc) < 0) {
			msgDebug("Unable to make or stat: %s\n", dp->d_name);
			curdir = rcurdir = ocurdir;
			continue;
		    }
		}
		if (readlink (dp->d_name, symbuf, sizeof(symbuf) - 1) >= 0) {
		    msgDebug("%s: is a link instead of a directory\n", dp->d_name);
		    curdir = rcurdir = ocurdir;
		    continue;
		}
		if (chdir (dp->d_name) < 0) {
		    msgDebug("Unable to chdir to: %s\n", dp->d_name);
		    curdir = rcurdir = ocurdir;
		    continue;
		}
		(void)dodir(buf, &sb, &sc, (buf[0] != '/'));
		if (chdir ("..") < 0) {
		    msgDebug("Unable to get back to ..\n");
		    return RET_FAIL;
		}
		curdir = rcurdir = ocurdir;
		continue;
	    }
	}

	/* non-directory */
	symlen = readlink (dp->d_name, symbuf, sizeof(symbuf) - 1);
	if (symlen >= 0) {
	    symbuf[symlen] = '\0';
	    if (!equivalent (symbuf, buf))
		msgDebug("%s: %s\n", dp->d_name, symbuf);
	} else if (symlink (buf, dp->d_name) < 0)
	    msgDebug("Unable to create symlink: %s\n", dp->d_name);
    }

    closedir (df);
    return 0;
}

int
lndir(char *from, char *to)
{
    struct stat fs, ts;

    if (!to)
	to = ".";

    /* to directory */
    if (stat(to, &ts) < 0) {
	msgDebug("Destination directory doesn't exist: %s\n", to);
	return RET_FAIL;
    }
    if (!(S_ISDIR(ts.st_mode))) {
	msgDebug ("%s: Not a directory\n", to);
	return RET_FAIL;
    }
    if (chdir(to) < 0) {
	msgDebug("Unable to chdir to %s\n", to);
	return RET_FAIL;
    }
    /* from directory */
    if (stat(from, &fs) < 0) {
	msgDebug("From directory doesn't exist: %s\n", from);
	return RET_FAIL;
    }
    if (!(S_ISDIR(fs.st_mode))) {
	msgDebug ("%s: Not a directory\n", from);
	return RET_FAIL;
    }
    return dodir(from, &fs, &ts, 0);
}
