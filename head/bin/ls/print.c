/*-
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)print.c	8.4 (Berkeley) 4/17/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/acl.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <langinfo.h>
#include <libutil.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef COLORLS
#include <ctype.h>
#include <termcap.h>
#include <signal.h>
#endif
#include <libxo/xo.h>

#include "ls.h"
#include "extern.h"

static int	printaname(const FTSENT *, u_long, u_long);
static void	printdev(size_t, dev_t);
static void	printlink(const FTSENT *);
static void	printtime(const char *, time_t);
static int	printtype(u_int);
static void	printsize(const char *, size_t, off_t);
#ifdef COLORLS
static void	endcolor(int);
static int	colortype(mode_t);
#endif
static void	aclmode(char *, const FTSENT *);

#define	IS_NOPRINT(p)	((p)->fts_number == NO_PRINT)

#ifdef COLORLS
/* Most of these are taken from <sys/stat.h> */
typedef enum Colors {
	C_DIR,			/* directory */
	C_LNK,			/* symbolic link */
	C_SOCK,			/* socket */
	C_FIFO,			/* pipe */
	C_EXEC,			/* executable */
	C_BLK,			/* block special */
	C_CHR,			/* character special */
	C_SUID,			/* setuid executable */
	C_SGID,			/* setgid executable */
	C_WSDIR,		/* directory writeble to others, with sticky
				 * bit */
	C_WDIR,			/* directory writeble to others, without
				 * sticky bit */
	C_NUMCOLORS		/* just a place-holder */
} Colors;

static const char *defcolors = "exfxcxdxbxegedabagacad";

/* colors for file types */
static struct {
	int	num[2];
	int	bold;
} colors[C_NUMCOLORS];
#endif

void
printscol(const DISPLAY *dp)
{
	FTSENT *p;

	xo_open_list("entry");
	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		xo_open_instance("entry");
		(void)printaname(p, dp->s_inode, dp->s_block);
		xo_close_instance("entry");
		xo_emit("\n");
	}
	xo_close_list("entry");
}

/*
 * print name in current style
 */
int
printname(const char *field, const char *name)
{
	char fmt[BUFSIZ];
	char *s = getname(name);
	int rc;
	
	snprintf(fmt, sizeof(fmt), "{:%s/%%hs}", field);
	rc = xo_emit(fmt, s);
	free(s);
	return rc;
}

/*
 * print name in current style
 */
char *
getname(const char *name)
{
	if (f_octal || f_octal_escape)
		return get_octal(name);
	else if (f_nonprint)
		return get_printable(name);
	else
		return strdup(name);
}

void
printlong(const DISPLAY *dp)
{
	struct stat *sp;
	FTSENT *p;
	NAMES *np;
	char buf[20];
#ifdef COLORLS
	int color_printed = 0;
#endif

	if ((dp->list == NULL || dp->list->fts_level != FTS_ROOTLEVEL) &&
	    (f_longform || f_size)) {
		xo_emit("{L:total} {:total-blocks/%lu}\n",
			howmany(dp->btotal, blocksize));
	}

	xo_open_list("entry");
	for (p = dp->list; p; p = p->fts_link) {
		char *name, *type;
		if (IS_NOPRINT(p))
			continue;
		xo_open_instance("entry");
		sp = p->fts_statp;
		name = getname(p->fts_name);
		if (name)
		    xo_emit("{ke:name/%hs}", name);
		if (f_inode)
			xo_emit("{t:inode/%*ju} ",
			    dp->s_inode, (uintmax_t)sp->st_ino);
		if (f_size)
			xo_emit("{t:blocks/%*jd} ",
			    dp->s_block, howmany(sp->st_blocks, blocksize));
		strmode(sp->st_mode, buf);
		aclmode(buf, p);
		np = p->fts_pointer;
		xo_attr("value", "%03o", (int) sp->st_mode & ALLPERMS);
		if (f_numericonly) {
			xo_emit("{t:mode/%s}{e:mode_octal/%03o} {t:links/%*u} {td:user/%-*s}{e:user/%ju}  {td:group/%-*s}{e:group/%ju}  ",
				buf, (int) sp->st_mode & ALLPERMS, dp->s_nlink, sp->st_nlink,
				dp->s_user, np->user, (uintmax_t)sp->st_uid, dp->s_group, np->group, (uintmax_t)sp->st_gid);
		} else {
			xo_emit("{t:mode/%s}{e:mode_octal/%03o} {t:links/%*u} {t:user/%-*s}  {t:group/%-*s}  ",
				buf, (int) sp->st_mode & ALLPERMS, dp->s_nlink, sp->st_nlink,
				dp->s_user, np->user, dp->s_group, np->group);
		}
		if (S_ISBLK(sp->st_mode))
			asprintf(&type, "block");
		if (S_ISCHR(sp->st_mode))
			asprintf(&type, "character");
		if (S_ISDIR(sp->st_mode))
			asprintf(&type, "directory");
		if (S_ISFIFO(sp->st_mode))
			asprintf(&type, "fifo");
		if (S_ISLNK(sp->st_mode))
			asprintf(&type, "symlink");
		if (S_ISREG(sp->st_mode))
			asprintf(&type, "regular");
		if (S_ISSOCK(sp->st_mode))
			asprintf(&type, "socket");
		if (S_ISWHT(sp->st_mode))
			asprintf(&type, "whiteout");
		xo_emit("{e:type/%s}", type);
		free(type);
		if (f_flags)
			xo_emit("{:flags/%-*s} ", dp->s_flags, np->flags);
		if (f_label)
			xo_emit("{t:label/%-*s} ", dp->s_label, np->label);
		if (S_ISCHR(sp->st_mode) || S_ISBLK(sp->st_mode))
			printdev(dp->s_size, sp->st_rdev);
		else
			printsize("size", dp->s_size, sp->st_size);
		if (f_accesstime)
			printtime("access-time", sp->st_atime);
		else if (f_birthtime)
			printtime("birth-time", sp->st_birthtime);
		else if (f_statustime)
			printtime("change-time", sp->st_ctime);
		else
			printtime("modify-time", sp->st_mtime);
#ifdef COLORLS
		if (f_color)
			color_printed = colortype(sp->st_mode);
#endif

		if (name) {
		    xo_emit("{dk:name/%hs}", name);
		    free(name);
		}
		
#ifdef COLORLS
		if (f_color && color_printed)
			endcolor(0);
#endif
		if (f_type)
			(void)printtype(sp->st_mode);
		if (S_ISLNK(sp->st_mode))
			printlink(p);
		xo_close_instance("entry");
		xo_emit("\n");
	}
	xo_close_list("entry");
}

void
printstream(const DISPLAY *dp)
{
	FTSENT *p;
	int chcnt;

	xo_open_list("entry");
	for (p = dp->list, chcnt = 0; p; p = p->fts_link) {
		if (p->fts_number == NO_PRINT)
			continue;
		/* XXX strlen does not take octal escapes into account. */
		if (strlen(p->fts_name) + chcnt +
		    (p->fts_link ? 2 : 0) >= (unsigned)termwidth) {
			xo_emit("\n");
			chcnt = 0;
		}
		xo_open_instance("file");
		chcnt += printaname(p, dp->s_inode, dp->s_block);
		xo_close_instance("file");
		if (p->fts_link) {
			xo_emit(", ");
			chcnt += 2;
		}
	}
	xo_close_list("entry");
	if (chcnt)
		xo_emit("\n");
}

void
printcol(const DISPLAY *dp)
{
	static FTSENT **array;
	static int lastentries = -1;
	FTSENT *p;
	FTSENT **narray;
	int base;
	int chcnt;
	int cnt;
	int col;
	int colwidth;
	int endcol;
	int num;
	int numcols;
	int numrows;
	int row;
	int tabwidth;

	if (f_notabs)
		tabwidth = 1;
	else
		tabwidth = 8;

	/*
	 * Have to do random access in the linked list -- build a table
	 * of pointers.
	 */
	if (dp->entries > lastentries) {
		if ((narray =
		    realloc(array, dp->entries * sizeof(FTSENT *))) == NULL) {
			printscol(dp);
			return;
		}
		lastentries = dp->entries;
		array = narray;
	}
	for (p = dp->list, num = 0; p; p = p->fts_link)
		if (p->fts_number != NO_PRINT)
			array[num++] = p;

	colwidth = dp->maxlen;
	if (f_inode)
		colwidth += dp->s_inode + 1;
	if (f_size)
		colwidth += dp->s_block + 1;
	if (f_type)
		colwidth += 1;

	colwidth = (colwidth + tabwidth) & ~(tabwidth - 1);
	if (termwidth < 2 * colwidth) {
		printscol(dp);
		return;
	}
	numcols = termwidth / colwidth;
	numrows = num / numcols;
	if (num % numcols)
		++numrows;

	if ((dp->list == NULL || dp->list->fts_level != FTS_ROOTLEVEL) &&
	    (f_longform || f_size)) {
		xo_emit("{L:total} {:total-blocks/%lu}\n",
			howmany(dp->btotal, blocksize));
	}

	xo_open_list("entry");
	base = 0;
	for (row = 0; row < numrows; ++row) {
		endcol = colwidth;
		if (!f_sortacross)
			base = row;
		for (col = 0, chcnt = 0; col < numcols; ++col) {
			xo_open_instance("entry");
			chcnt += printaname(array[base], dp->s_inode,
			    dp->s_block);
			xo_close_instance("entry");
			if (f_sortacross)
				base++;
			else
				base += numrows;
			if (base >= num)
				break;
			while ((cnt = ((chcnt + tabwidth) & ~(tabwidth - 1)))
			    <= endcol) {
				if (f_sortacross && col + 1 >= numcols)
					break;
				xo_emit(f_notabs ? " " : "\t");
				chcnt = cnt;
			}
			endcol += colwidth;
		}
		xo_emit("\n");
	}
	xo_close_list("entry");
}

/*
 * print [inode] [size] name
 * return # of characters printed, no trailing characters.
 */
static int
printaname(const FTSENT *p, u_long inodefield, u_long sizefield)
{
	struct stat *sp;
	int chcnt;
#ifdef COLORLS
	int color_printed = 0;
#endif

	sp = p->fts_statp;
	chcnt = 0;
	if (f_inode)
		chcnt += xo_emit("{t:inode/%*ju} ",
		    (int)inodefield, (uintmax_t)sp->st_ino);
	if (f_size)
		chcnt += xo_emit("{t:size/%*jd} ",
		    (int)sizefield, howmany(sp->st_blocks, blocksize));
#ifdef COLORLS
	if (f_color)
		color_printed = colortype(sp->st_mode);
#endif
	chcnt += printname("name", p->fts_name);
#ifdef COLORLS
	if (f_color && color_printed)
		endcolor(0);
#endif
	if (f_type)
		chcnt += printtype(sp->st_mode);
	return (chcnt);
}

/*
 * Print device special file major and minor numbers.
 */
static void
printdev(size_t width, dev_t dev)
{
	xo_emit("{:device/%#*jx} ", (u_int)width, (uintmax_t)dev);
}

static void
printtime(const char *field, time_t ftime)
{
	char longstring[80];
	char fmt[BUFSIZ];
	static time_t now = 0;
	const char *format;
	static int d_first = -1;

	if (d_first < 0)
		d_first = (*nl_langinfo(D_MD_ORDER) == 'd');
	if (now == 0)
		now = time(NULL);

#define	SIXMONTHS	((365 / 2) * 86400)
	if (f_timeformat)  /* user specified format */
		format = f_timeformat;
	else if (f_sectime)
		/* mmm dd hh:mm:ss yyyy || dd mmm hh:mm:ss yyyy */
		format = d_first ? "%e %b %T %Y" : "%b %e %T %Y";
	else if (ftime + SIXMONTHS > now && ftime < now + SIXMONTHS)
		/* mmm dd hh:mm || dd mmm hh:mm */
		format = d_first ? "%e %b %R" : "%b %e %R";
	else
		/* mmm dd  yyyy || dd mmm  yyyy */
		format = d_first ? "%e %b  %Y" : "%b %e  %Y";
	strftime(longstring, sizeof(longstring), format, localtime(&ftime));

	snprintf(fmt, sizeof(fmt), "{d:%s/%%hs} ", field);
	xo_attr("value", "%ld", (long) ftime);
	xo_emit(fmt, longstring);
	snprintf(fmt, sizeof(fmt), "{en:%s/%%ld}", field);
	xo_emit(fmt, (long) ftime);
}

static int
printtype(u_int mode)
{

	if (f_slash) {
		if ((mode & S_IFMT) == S_IFDIR) {
			xo_emit("{D:\\/}{e:type/directory}");
			return (1);
		}
		return (0);
	}

	switch (mode & S_IFMT) {
	case S_IFDIR:
		xo_emit("{D:/\\/}{e:type/directory}");
		return (1);
	case S_IFIFO:
		xo_emit("{D:|}{e:type/fifo}");
		return (1);
	case S_IFLNK:
		xo_emit("{D:@}{e:type/link}");
		return (1);
	case S_IFSOCK:
		xo_emit("{D:=}{e:type/socket}");
		return (1);
	case S_IFWHT:
		xo_emit("{D:%%}{e:type/whiteout}");
		return (1);
	default:
		break;
	}
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
		xo_emit("{D:*}{e:executable/}");
		return (1);
	}
	return (0);
}

#ifdef COLORLS
static int
putch(int c)
{
	xo_emit("{D:/%c}", c);
	return 0;
}

static int
writech(int c)
{
	char tmp = (char)c;

	(void)write(STDOUT_FILENO, &tmp, 1);
	return 0;
}

static void
printcolor(Colors c)
{
	char *ansiseq;

	if (colors[c].bold)
		tputs(enter_bold, 1, putch);

	if (colors[c].num[0] != -1) {
		ansiseq = tgoto(ansi_fgcol, 0, colors[c].num[0]);
		if (ansiseq)
			tputs(ansiseq, 1, putch);
	}
	if (colors[c].num[1] != -1) {
		ansiseq = tgoto(ansi_bgcol, 0, colors[c].num[1]);
		if (ansiseq)
			tputs(ansiseq, 1, putch);
	}
}

static void
endcolor(int sig)
{
	tputs(ansi_coloff, 1, sig ? writech : putch);
	tputs(attrs_off, 1, sig ? writech : putch);
}

static int
colortype(mode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFDIR:
		if (mode & S_IWOTH)
			if (mode & S_ISTXT)
				printcolor(C_WSDIR);
			else
				printcolor(C_WDIR);
		else
			printcolor(C_DIR);
		return (1);
	case S_IFLNK:
		printcolor(C_LNK);
		return (1);
	case S_IFSOCK:
		printcolor(C_SOCK);
		return (1);
	case S_IFIFO:
		printcolor(C_FIFO);
		return (1);
	case S_IFBLK:
		printcolor(C_BLK);
		return (1);
	case S_IFCHR:
		printcolor(C_CHR);
		return (1);
	default:;
	}
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
		if (mode & S_ISUID)
			printcolor(C_SUID);
		else if (mode & S_ISGID)
			printcolor(C_SGID);
		else
			printcolor(C_EXEC);
		return (1);
	}
	return (0);
}

void
parsecolors(const char *cs)
{
	int i;
	int j;
	size_t len;
	char c[2];
	short legacy_warn = 0;

	if (cs == NULL)
		cs = "";	/* LSCOLORS not set */
	len = strlen(cs);
	for (i = 0; i < (int)C_NUMCOLORS; i++) {
		colors[i].bold = 0;

		if (len <= 2 * (size_t)i) {
			c[0] = defcolors[2 * i];
			c[1] = defcolors[2 * i + 1];
		} else {
			c[0] = cs[2 * i];
			c[1] = cs[2 * i + 1];
		}
		for (j = 0; j < 2; j++) {
			/* Legacy colours used 0-7 */
			if (c[j] >= '0' && c[j] <= '7') {
				colors[i].num[j] = c[j] - '0';
				if (!legacy_warn) {
					xo_warnx("LSCOLORS should use "
					    "characters a-h instead of 0-9 ("
					    "see the manual page)");
				}
				legacy_warn = 1;
			} else if (c[j] >= 'a' && c[j] <= 'h')
				colors[i].num[j] = c[j] - 'a';
			else if (c[j] >= 'A' && c[j] <= 'H') {
				colors[i].num[j] = c[j] - 'A';
				colors[i].bold = 1;
			} else if (tolower((unsigned char)c[j]) == 'x')
				colors[i].num[j] = -1;
			else {
				xo_warnx("invalid character '%c' in LSCOLORS"
				    " env var", c[j]);
				colors[i].num[j] = -1;
			}
		}
	}
}

void
colorquit(int sig)
{
	endcolor(sig);

	(void)signal(sig, SIG_DFL);
	(void)kill(getpid(), sig);
}

#endif /* COLORLS */

static void
printlink(const FTSENT *p)
{
	int lnklen;
	char name[MAXPATHLEN + 1];
	char path[MAXPATHLEN + 1];

	if (p->fts_level == FTS_ROOTLEVEL)
		(void)snprintf(name, sizeof(name), "%s", p->fts_name);
	else
		(void)snprintf(name, sizeof(name),
		    "%s/%s", p->fts_parent->fts_accpath, p->fts_name);
	if ((lnklen = readlink(name, path, sizeof(path) - 1)) == -1) {
		xo_error("\nls: %s: %s\n", name, strerror(errno));
		return;
	}
	path[lnklen] = '\0';
	xo_emit(" -> ");
	(void)printname("target", path);
}

static void
printsize(const char *field, size_t width, off_t bytes)
{
	char fmt[BUFSIZ];
	
	if (f_humanval) {
		/*
		 * Reserve one space before the size and allocate room for
		 * the trailing '\0'.
		 */
		char buf[HUMANVALSTR_LEN - 1 + 1];

		humanize_number(buf, sizeof(buf), (int64_t)bytes, "",
		    HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
		snprintf(fmt, sizeof(fmt), "{:%s/%%%ds} ", field, (int) width);
		xo_attr("value", "%jd", (intmax_t) bytes);
		xo_emit(fmt, buf);
	} else {		/* with commas */
		/* This format assignment needed to work round gcc bug. */
		snprintf(fmt, sizeof(fmt), "{:%s/%%%dj%sd} ",
		     field, (int) width, f_thousands ? "'" : "");
		xo_emit(fmt, (intmax_t) bytes);
	}
}

/*
 * Add a + after the standard rwxrwxrwx mode if the file has an
 * ACL. strmode() reserves space at the end of the string.
 */
static void
aclmode(char *buf, const FTSENT *p)
{
	char name[MAXPATHLEN + 1];
	int ret, trivial;
	static dev_t previous_dev = NODEV;
	static int supports_acls = -1;
	static int type = ACL_TYPE_ACCESS;
	acl_t facl;

	/*
	 * XXX: ACLs are not supported on whiteouts and device files
	 * residing on UFS.
	 */
	if (S_ISCHR(p->fts_statp->st_mode) || S_ISBLK(p->fts_statp->st_mode) ||
	    S_ISWHT(p->fts_statp->st_mode))
		return;

	if (previous_dev == p->fts_statp->st_dev && supports_acls == 0)
		return;

	if (p->fts_level == FTS_ROOTLEVEL)
		snprintf(name, sizeof(name), "%s", p->fts_name);
	else
		snprintf(name, sizeof(name), "%s/%s",
		    p->fts_parent->fts_accpath, p->fts_name);

	if (previous_dev != p->fts_statp->st_dev) {
		previous_dev = p->fts_statp->st_dev;
		supports_acls = 0;

		ret = lpathconf(name, _PC_ACL_NFS4);
		if (ret > 0) {
			type = ACL_TYPE_NFS4;
			supports_acls = 1;
		} else if (ret < 0 && errno != EINVAL) {
			xo_warn("%s", name);
			return;
		}
		if (supports_acls == 0) {
			ret = lpathconf(name, _PC_ACL_EXTENDED);
			if (ret > 0) {
				type = ACL_TYPE_ACCESS;
				supports_acls = 1;
			} else if (ret < 0 && errno != EINVAL) {
				xo_warn("%s", name);
				return;
			}
		}
	}
	if (supports_acls == 0)
		return;
	facl = acl_get_link_np(name, type);
	if (facl == NULL) {
		xo_warn("%s", name);
		return;
	}
	if (acl_is_trivial_np(facl, &trivial)) {
		acl_free(facl);
		xo_warn("%s", name);
		return;
	}
	if (!trivial)
		buf[10] = '+';
	acl_free(facl);
}
