/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Cimarron D. Taylor of the University of California, Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#ifndef lint
#if 0
static const char sccsid[] = "@(#)function.c  8.10 (Berkeley) 5/4/95";
#else
static const char rcsid[] =
  "$FreeBSD$";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/timeb.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fnmatch.h>
#include <fts.h>
#include <grp.h>
#include <pwd.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "find.h"

time_t get_date __P((char *date, struct timeb *now));

#define	COMPARE(a, b) {							\
	switch (plan->flags & F_ELG_MASK) {				\
	case F_EQUAL:							\
		return (a == b);					\
	case F_LESSTHAN:						\
		return (a < b);						\
	case F_GREATER:							\
		return (a > b);						\
	default:							\
		abort();						\
	}								\
}

static PLAN *
palloc(option)
	OPTION *option;
{
	PLAN *new;

	if ((new = malloc(sizeof(PLAN))) == NULL)
		err(1, NULL);
	new->execute = option->execute;
	new->flags = option->flags;
	new->next = NULL;
	return new;
}

/*
 * find_parsenum --
 *	Parse a string of the form [+-]# and return the value.
 */
static long long
find_parsenum(plan, option, vp, endch)
	PLAN *plan;
	char *option, *vp, *endch;
{
	long long value;
	char *endchar, *str;	/* Pointer to character ending conversion. */

	/* Determine comparison from leading + or -. */
	str = vp;
	switch (*str) {
	case '+':
		++str;
		plan->flags |= F_GREATER;
		break;
	case '-':
		++str;
		plan->flags |= F_LESSTHAN;
		break;
	default:
		plan->flags |= F_EQUAL;
		break;
	}

	/*
	 * Convert the string with strtoq().  Note, if strtoq() returns zero
	 * and endchar points to the beginning of the string we know we have
	 * a syntax error.
	 */
	value = strtoq(str, &endchar, 10);
	if (value == 0 && endchar == str)
		errx(1, "%s: %s: illegal numeric value", option, vp);
	if (endchar[0] && (endch == NULL || endchar[0] != *endch))
		errx(1, "%s: %s: illegal trailing character", option, vp);
	if (endch)
		*endch = endchar[0];
	return value;
}

/*
 * nextarg --
 *	Check that another argument still exists, return a pointer to it,
 *	and increment the argument vector pointer.
 */
static char *
nextarg(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *arg;

	if ((arg = **argvp) == 0)
		errx(1, "%s: requires additional arguments", option->name);
	(*argvp)++;
	return arg;
} /* nextarg() */

/*
 * The value of n for the inode times (atime, ctime, and mtime) is a range,
 * i.e. n matches from (n - 1) to n 24 hour periods.  This interacts with
 * -n, such that "-mtime -1" would be less than 0 days, which isn't what the
 * user wanted.  Correct so that -1 is "less than 1".
 */
#define	TIME_CORRECT(p) \
	if (((p)->flags & F_ELG_MASK) == F_LESSTHAN) \
		++((p)->t_data);

/*
 * -[acm]min n functions --
 *
 *    True if the difference between the
 *		file access time (-amin)
 *		last change of file status information (-cmin)
 *		file modification time (-mmin)
 *    and the current time is n min periods.
 */
int
f_Xmin(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	extern time_t now;

	if (plan->flags & F_TIME_C) {
		COMPARE((now - entry->fts_statp->st_ctime +
		    60 - 1) / 60, plan->t_data);
	} else if (plan->flags & F_TIME_A) {
		COMPARE((now - entry->fts_statp->st_atime +
		    60 - 1) / 60, plan->t_data);
	} else {
		COMPARE((now - entry->fts_statp->st_mtime +
		    60 - 1) / 60, plan->t_data);
	}
}

PLAN *
c_Xmin(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *nmins;
	PLAN *new;

	nmins = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(option);
	new->t_data = find_parsenum(new, option->name, nmins, NULL);
	TIME_CORRECT(new);
	return new;
}

/*
 * -[acm]time n functions --
 *
 *	True if the difference between the
 *		file access time (-atime)
 *		last change of file status information (-ctime)
 *		file modification time (-mtime)
 *	and the current time is n 24 hour periods.
 */

int
f_Xtime(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	extern time_t now;

	if (plan->flags & F_TIME_C) {
		COMPARE((now - entry->fts_statp->st_ctime +
		    86400 - 1) / 86400, plan->t_data);
	} else if (plan->flags & F_TIME_A) {
		COMPARE((now - entry->fts_statp->st_atime +
		    86400 - 1) / 86400, plan->t_data);
	} else {
		COMPARE((now - entry->fts_statp->st_mtime +
		    86400 - 1) / 86400, plan->t_data);
	}
}

PLAN *
c_Xtime(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *ndays;
	PLAN *new;

	ndays = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(option);
	new->t_data = find_parsenum(new, option->name, ndays, NULL);
	TIME_CORRECT(new);
	return new;
}

/*
 * -maxdepth/-mindepth n functions --
 *
 *        Does the same as -prune if the level of the current file is
 *        greater/less than the specified maximum/minimum depth.
 *
 *        Note that -maxdepth and -mindepth are handled specially in
 *        find_execute() so their f_* functions are set to f_always_true().
 */
PLAN *
c_mXXdepth(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *dstr;
	PLAN *new;

	dstr = nextarg(option, argvp);
	if (dstr[0] == '-')
		/* all other errors handled by find_parsenum() */
		errx(1, "%s: %s: value must be positive", option->name, dstr);

	new = palloc(option);
	if (option->flags & F_MAXDEPTH)
		maxdepth = find_parsenum(new, option->name, dstr, NULL);
	else
		mindepth = find_parsenum(new, option->name, dstr, NULL);
	return new;
}

/*
 * -delete functions --
 *
 *	True always.  Makes its best shot and continues on regardless.
 */
int
f_delete(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	/* ignore these from fts */
	if (strcmp(entry->fts_accpath, ".") == 0 ||
	    strcmp(entry->fts_accpath, "..") == 0)
		return 1;

	/* sanity check */
	if (isdepth == 0 ||			/* depth off */
	    (ftsoptions & FTS_NOSTAT) ||	/* not stat()ing */
	    !(ftsoptions & FTS_PHYSICAL) ||	/* physical off */
	    (ftsoptions & FTS_LOGICAL))		/* or finally, logical on */
		errx(1, "-delete: insecure options got turned on");

	/* Potentially unsafe - do not accept relative paths whatsoever */
	if (strchr(entry->fts_accpath, '/') != NULL)
		errx(1, "-delete: %s: relative path potentially not safe",
			entry->fts_accpath);

	/* Turn off user immutable bits if running as root */
	if ((entry->fts_statp->st_flags & (UF_APPEND|UF_IMMUTABLE)) &&
	    !(entry->fts_statp->st_flags & (SF_APPEND|SF_IMMUTABLE)) &&
	    geteuid() == 0)
		chflags(entry->fts_accpath,
		       entry->fts_statp->st_flags &= ~(UF_APPEND|UF_IMMUTABLE));

	/* rmdir directories, unlink everything else */
	if (S_ISDIR(entry->fts_statp->st_mode)) {
		if (rmdir(entry->fts_accpath) < 0 && errno != ENOTEMPTY)
			warn("-delete: rmdir(%s)", entry->fts_path);
	} else {
		if (unlink(entry->fts_accpath) < 0)
			warn("-delete: unlink(%s)", entry->fts_path);
	}

	/* "succeed" */
	return 1;
}

PLAN *
c_delete(option, argvp)
	OPTION *option;
	char ***argvp;
{

	ftsoptions &= ~FTS_NOSTAT;	/* no optimise */
	ftsoptions |= FTS_PHYSICAL;	/* disable -follow */
	ftsoptions &= ~FTS_LOGICAL;	/* disable -follow */
	isoutput = 1;			/* possible output */
	isdepth = 1;			/* -depth implied */

	return palloc(option);
}


/*
 * -depth functions --
 *
 *	Always true, causes descent of the directory hierarchy to be done
 *	so that all entries in a directory are acted on before the directory
 *	itself.
 */
int
f_always_true(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return 1;
}

PLAN *
c_depth(option, argvp)
	OPTION *option;
	char ***argvp;
{
	isdepth = 1;

	return palloc(option);
}

/*
 * -empty functions --
 *
 *	True if the file or directory is empty
 */
int
f_empty(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	if (S_ISREG(entry->fts_statp->st_mode) &&
	    entry->fts_statp->st_size == 0)
		return 1;
	if (S_ISDIR(entry->fts_statp->st_mode)) {
		struct dirent *dp;
		int empty;
		DIR *dir;

		empty = 1;
		dir = opendir(entry->fts_accpath);
		if (dir == NULL)
			err(1, "%s", entry->fts_accpath);
		for (dp = readdir(dir); dp; dp = readdir(dir))
			if (dp->d_name[0] != '.' ||
			    (dp->d_name[1] != '\0' &&
			     (dp->d_name[1] != '.' || dp->d_name[2] != '\0'))) {
				empty = 0;
				break;
			}
		closedir(dir);
		return empty;
	}
	return 0;
}

PLAN *
c_empty(option, argvp)
	OPTION *option;
	char ***argvp;
{
	ftsoptions &= ~FTS_NOSTAT;

	return palloc(option);
}

/*
 * [-exec | -execdir | -ok] utility [arg ... ] ; functions --
 *
 *	True if the executed utility returns a zero value as exit status.
 *	The end of the primary expression is delimited by a semicolon.  If
 *	"{}" occurs anywhere, it gets replaced by the current pathname,
 *	or, in the case of -execdir, the current basename (filename
 *	without leading directory prefix). For -exec and -ok,
 *	the current directory for the execution of utility is the same as
 *	the current directory when the find utility was started, whereas
 *	for -execdir, it is the directory the file resides in.
 *
 *	The primary -ok differs from -exec in that it requests affirmation
 *	of the user before executing the utility.
 */
int
f_exec(plan, entry)
	register PLAN *plan;
	FTSENT *entry;
{
	extern int dotfd;
	register int cnt;
	pid_t pid;
	int status;
	char *file;

	/* XXX - if file/dir ends in '/' this will not work -- can it? */
	if ((plan->flags & F_EXECDIR) && \
	    (file = strrchr(entry->fts_path, '/')))
		file++;
	else
		file = entry->fts_path;

	for (cnt = 0; plan->e_argv[cnt]; ++cnt)
		if (plan->e_len[cnt])
			brace_subst(plan->e_orig[cnt], &plan->e_argv[cnt],
			    file, plan->e_len[cnt]);

	if ((plan->flags & F_NEEDOK) && !queryuser(plan->e_argv))
		return 0;

	/* make sure find output is interspersed correctly with subprocesses */
	fflush(stdout);
	fflush(stderr);

	switch (pid = fork()) {
	case -1:
		err(1, "fork");
		/* NOTREACHED */
	case 0:
		/* change dir back from where we started */
		if (!(plan->flags & F_EXECDIR) && fchdir(dotfd)) {
			warn("chdir");
			_exit(1);
		}
		execvp(plan->e_argv[0], plan->e_argv);
		warn("%s", plan->e_argv[0]);
		_exit(1);
	}
	pid = waitpid(pid, &status, 0);
	return (pid != -1 && WIFEXITED(status) && !WEXITSTATUS(status));
}

/*
 * c_exec, c_execdir, c_ok --
 *	build three parallel arrays, one with pointers to the strings passed
 *	on the command line, one with (possibly duplicated) pointers to the
 *	argv array, and one with integer values that are lengths of the
 *	strings, but also flags meaning that the string has to be massaged.
 */
PLAN *
c_exec(option, argvp)
	OPTION *option;
	char ***argvp;
{
	PLAN *new;			/* node returned */
	register int cnt;
	register char **argv, **ap, *p;

	/* XXX - was in c_execdir, but seems unnecessary!?
	ftsoptions &= ~FTS_NOSTAT;
	*/
	isoutput = 1;

	/* XXX - this is a change from the previous coding */
	new = palloc(option);

	for (ap = argv = *argvp;; ++ap) {
		if (!*ap)
			errx(1,
			    "%s: no terminating \";\"", option->name);
		if (**ap == ';')
			break;
	}

	cnt = ap - *argvp + 1;
	new->e_argv = (char **)emalloc((u_int)cnt * sizeof(char *));
	new->e_orig = (char **)emalloc((u_int)cnt * sizeof(char *));
	new->e_len = (int *)emalloc((u_int)cnt * sizeof(int));

	for (argv = *argvp, cnt = 0; argv < ap; ++argv, ++cnt) {
		new->e_orig[cnt] = *argv;
		for (p = *argv; *p; ++p)
			if (p[0] == '{' && p[1] == '}') {
				new->e_argv[cnt] = emalloc((u_int)MAXPATHLEN);
				new->e_len[cnt] = MAXPATHLEN;
				break;
			}
		if (!*p) {
			new->e_argv[cnt] = *argv;
			new->e_len[cnt] = 0;
		}
	}
	new->e_argv[cnt] = new->e_orig[cnt] = NULL;

	*argvp = argv + 1;
	return new;
}

int
f_flags(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	u_long flags;

	flags = entry->fts_statp->st_flags &
	    (UF_NODUMP | UF_IMMUTABLE | UF_APPEND | UF_OPAQUE |
	     SF_ARCHIVED | SF_IMMUTABLE | SF_APPEND);
	if (plan->flags & F_ATLEAST)
		/* note that plan->fl_flags always is a subset of
		   plan->fl_mask */
		return (flags & plan->fl_mask) == plan->fl_flags;
	else if (plan->flags & F_ANY)
		return flags & plan->fl_mask;
	else
		return flags == plan->fl_flags;
	/* NOTREACHED */
}

PLAN *
c_flags(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *flags_str;
	PLAN *new;
	u_long flags, notflags;

	flags_str = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(option);

	if (*flags_str == '-') {
		new->flags |= F_ATLEAST;
		flags_str++;
	}
	if (strtofflags(&flags_str, &flags, &notflags) == 1)
		errx(1, "%s: %s: illegal flags string", option->name, flags_str);

	new->fl_flags = flags;
	new->fl_mask = flags | notflags;
	return new;
}

/*
 * -follow functions --
 *
 *	Always true, causes symbolic links to be followed on a global
 *	basis.
 */
PLAN *
c_follow(option, argvp)
	OPTION *option;
	char ***argvp;
{
	ftsoptions &= ~FTS_PHYSICAL;
	ftsoptions |= FTS_LOGICAL;

	return palloc(option);
}

/*
 * -fstype functions --
 *
 *	True if the file is of a certain type.
 */
int
f_fstype(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	static dev_t curdev;	/* need a guaranteed illegal dev value */
	static int first = 1;
	struct statfs sb;
	static int val_type, val_flags;
	char *p, save[2];

	/* Only check when we cross mount point. */
	if (first || curdev != entry->fts_statp->st_dev) {
		curdev = entry->fts_statp->st_dev;

		/*
		 * Statfs follows symlinks; find wants the link's file system,
		 * not where it points.
		 */
		if (entry->fts_info == FTS_SL ||
		    entry->fts_info == FTS_SLNONE) {
			if ((p = strrchr(entry->fts_accpath, '/')) != NULL)
				++p;
			else
				p = entry->fts_accpath;
			save[0] = p[0];
			p[0] = '.';
			save[1] = p[1];
			p[1] = '\0';
		} else
			p = NULL;

		if (statfs(entry->fts_accpath, &sb))
			err(1, "%s", entry->fts_accpath);

		if (p) {
			p[0] = save[0];
			p[1] = save[1];
		}

		first = 0;

		/*
		 * Further tests may need both of these values, so
		 * always copy both of them.
		 */
		val_flags = sb.f_flags;
		val_type = sb.f_type;
	}
	switch (plan->flags & F_MTMASK) {
	case F_MTFLAG:
		return (val_flags & plan->mt_data) != 0;
	case F_MTTYPE:
		return (val_type == plan->mt_data);
	default:
		abort();
	}
}

#if !defined(__NetBSD__)
PLAN *
c_fstype(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *fsname;
	register PLAN *new;
	struct vfsconf vfc;

	fsname = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(option);

	/*
	 * Check first for a filesystem name.
	 */
	if (getvfsbyname(fsname, &vfc) == 0) {
		new->flags |= F_MTTYPE;
		new->mt_data = vfc.vfc_typenum;
		return new;
	}

	switch (*fsname) {
	case 'l':
		if (!strcmp(fsname, "local")) {
			new->flags |= F_MTFLAG;
			new->mt_data = MNT_LOCAL;
			return new;
		}
		break;
	case 'r':
		if (!strcmp(fsname, "rdonly")) {
			new->flags |= F_MTFLAG;
			new->mt_data = MNT_RDONLY;
			return new;
		}
		break;
	}

	errx(1, "%s: unknown file type", fsname);
	/* NOTREACHED */
}
#endif /* __NetBSD__ */

/*
 * -group gname functions --
 *
 *	True if the file belongs to the group gname.  If gname is numeric and
 *	an equivalent of the getgrnam() function does not return a valid group
 *	name, gname is taken as a group ID.
 */
int
f_group(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return entry->fts_statp->st_gid == plan->g_data;
}

PLAN *
c_group(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *gname;
	PLAN *new;
	struct group *g;
	gid_t gid;

	gname = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	g = getgrnam(gname);
	if (g == NULL) {
		gid = atoi(gname);
		if (gid == 0 && gname[0] != '0')
			errx(1, "%s: %s: no such group", option->name, gname);
	} else
		gid = g->gr_gid;

	new = palloc(option);
	new->g_data = gid;
	return new;
}

/*
 * -inum n functions --
 *
 *	True if the file has inode # n.
 */
int
f_inum(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	COMPARE(entry->fts_statp->st_ino, plan->i_data);
}

PLAN *
c_inum(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *inum_str;
	PLAN *new;

	inum_str = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(option);
	new->i_data = find_parsenum(new, option->name, inum_str, NULL);
	return new;
}

/*
 * -links n functions --
 *
 *	True if the file has n links.
 */
int
f_links(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	COMPARE(entry->fts_statp->st_nlink, plan->l_data);
}

PLAN *
c_links(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *nlinks;
	PLAN *new;

	nlinks = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(option);
	new->l_data = (nlink_t)find_parsenum(new, option->name, nlinks, NULL);
	return new;
}

/*
 * -ls functions --
 *
 *	Always true - prints the current entry to stdout in "ls" format.
 */
int
f_ls(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	printlong(entry->fts_path, entry->fts_accpath, entry->fts_statp);
	return 1;
}

PLAN *
c_ls(option, argvp)
	OPTION *option;
	char ***argvp;
{
	ftsoptions &= ~FTS_NOSTAT;
	isoutput = 1;

	return palloc(option);
}

/*
 * -name functions --
 *
 *	True if the basename of the filename being examined
 *	matches pattern using Pattern Matching Notation S3.14
 */
int
f_name(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return !fnmatch(plan->c_data, entry->fts_name,
	    plan->flags & F_IGNCASE ? FNM_CASEFOLD : 0);
}

PLAN *
c_name(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *pattern;
	PLAN *new;

	pattern = nextarg(option, argvp);
	new = palloc(option);
	new->c_data = pattern;
	return new;
}

/*
 * -newer file functions --
 *
 *	True if the current file has been modified more recently
 *	then the modification time of the file named by the pathname
 *	file.
 */
int
f_newer(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	if (plan->flags & F_TIME_C)
		return entry->fts_statp->st_ctime > plan->t_data;
	else if (plan->flags & F_TIME_A)
		return entry->fts_statp->st_atime > plan->t_data;
	else
		return entry->fts_statp->st_mtime > plan->t_data;
}

PLAN *
c_newer(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *fn_or_tspec;
	PLAN *new;
	struct stat sb;

	fn_or_tspec = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(option);
	/* compare against what */
	if (option->flags & F_TIME2_T) {
		new->t_data = get_date(fn_or_tspec, (struct timeb *) 0);
		if (new->t_data == (time_t) -1)
			errx(1, "Can't parse date/time: %s", fn_or_tspec);
	} else {
		if (stat(fn_or_tspec, &sb))
			err(1, "%s", fn_or_tspec);
		if (option->flags & F_TIME2_C)
			new->t_data = sb.st_ctime;
		else if (option->flags & F_TIME2_A)
			new->t_data = sb.st_atime;
		else
			new->t_data = sb.st_mtime;
	}
	return new;
}

/*
 * -nogroup functions --
 *
 *	True if file belongs to a user ID for which the equivalent
 *	of the getgrnam() 9.2.1 [POSIX.1] function returns NULL.
 */
int
f_nogroup(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return group_from_gid(entry->fts_statp->st_gid, 1) == NULL;
}

PLAN *
c_nogroup(option, argvp)
	OPTION *option;
	char ***argvp;
{
	ftsoptions &= ~FTS_NOSTAT;

	return palloc(option);
}

/*
 * -nouser functions --
 *
 *	True if file belongs to a user ID for which the equivalent
 *	of the getpwuid() 9.2.2 [POSIX.1] function returns NULL.
 */
int
f_nouser(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return user_from_uid(entry->fts_statp->st_uid, 1) == NULL;
}

PLAN *
c_nouser(option, argvp)
	OPTION *option;
	char ***argvp;
{
	ftsoptions &= ~FTS_NOSTAT;

	return palloc(option);
}

/*
 * -path functions --
 *
 *	True if the path of the filename being examined
 *	matches pattern using Pattern Matching Notation S3.14
 */
int
f_path(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return !fnmatch(plan->c_data, entry->fts_path,
	    plan->flags & F_IGNCASE ? FNM_CASEFOLD : 0);
}

/* c_path is the same as c_name */

/*
 * -perm functions --
 *
 *	The mode argument is used to represent file mode bits.  If it starts
 *	with a leading digit, it's treated as an octal mode, otherwise as a
 *	symbolic mode.
 */
int
f_perm(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	mode_t mode;

	mode = entry->fts_statp->st_mode &
	    (S_ISUID|S_ISGID|S_ISTXT|S_IRWXU|S_IRWXG|S_IRWXO);
	if (plan->flags & F_ATLEAST)
		return (plan->m_data | mode) == mode;
	else
		return mode == plan->m_data;
	/* NOTREACHED */
}

PLAN *
c_perm(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *perm;
	PLAN *new;
	mode_t *set;

	perm = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(option);

	if (*perm == '-') {
		new->flags |= F_ATLEAST;
		++perm;
	} else if (*perm == '+') {
		new->flags |= F_ANY;
		++perm;
	}

	if ((set = setmode(perm)) == NULL)
		errx(1, "%s: %s: illegal mode string", option->name, perm);

	new->m_data = getmode(set, 0);
	free(set);
	return new;
}

/*
 * -print functions --
 *
 *	Always true, causes the current pathame to be written to
 *	standard output.
 */
int
f_print(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	(void)puts(entry->fts_path);
	return 1;
}

PLAN *
c_print(option, argvp)
	OPTION *option;
	char ***argvp;
{
	isoutput = 1;

	return palloc(option);
}

/*
 * -print0 functions --
 *
 *	Always true, causes the current pathame to be written to
 *	standard output followed by a NUL character
 */
int
f_print0(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	fputs(entry->fts_path, stdout);
	fputc('\0', stdout);
	return 1;
}

/* c_print0 is the same as c_print */

/*
 * -prune functions --
 *
 *	Prune a portion of the hierarchy.
 */
int
f_prune(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	extern FTS *tree;

	if (fts_set(tree, entry, FTS_SKIP))
		err(1, "%s", entry->fts_path);
	return 1;
}

/* c_prune == c_simple */

/*
 * -regex functions --
 *
 *	True if the whole path of the file matches pattern using
 *	regular expression.
 */
int
f_regex(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	char *str;
	size_t len;
	regex_t *pre;
	regmatch_t pmatch;
	int errcode;
	char errbuf[LINE_MAX];
	int matched;

	pre = plan->re_data;
	str = entry->fts_path;
	len = strlen(str);
	matched = 0;

	pmatch.rm_so = 0;
	pmatch.rm_eo = len;

	errcode = regexec(pre, str, 1, &pmatch, REG_STARTEND);

	if (errcode != 0 && errcode != REG_NOMATCH) {
		regerror(errcode, pre, errbuf, sizeof errbuf);
		errx(1, "%s: %s",
		     plan->flags & F_IGNCASE ? "-iregex" : "-regex", errbuf);
	}

	if (errcode == 0 && pmatch.rm_so == 0 && pmatch.rm_eo == len)
		matched = 1;

	return matched;
}

PLAN *
c_regex(option, argvp)
	OPTION *option;
	char ***argvp;
{
	PLAN *new;
	char *pattern;
	regex_t *pre;
	int errcode;
	char errbuf[LINE_MAX];

	if ((pre = malloc(sizeof(regex_t))) == NULL)
		err(1, NULL);

	pattern = nextarg(option, argvp);

	if ((errcode = regcomp(pre, pattern,
	    regexp_flags | (option->flags & F_IGNCASE ? REG_ICASE : 0))) != 0) {
		regerror(errcode, pre, errbuf, sizeof errbuf);
		errx(1, "%s: %s: %s",
		     option->flags & F_IGNCASE ? "-iregex" : "-regex",
		     pattern, errbuf);
	}

	new = palloc(option);
	new->re_data = pre;

	return new;
}

/* c_simple covers c_prune, c_openparen, c_closeparen, c_not, c_or */

PLAN *
c_simple(option, argvp)
	OPTION *option;
	char ***argvp;
{
	return palloc(option);
}

/*
 * -size n[c] functions --
 *
 *	True if the file size in bytes, divided by an implementation defined
 *	value and rounded up to the next integer, is n.  If n is followed by
 *	a c, the size is in bytes.
 */
#define	FIND_SIZE	512
static int divsize = 1;

int
f_size(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	off_t size;

	size = divsize ? (entry->fts_statp->st_size + FIND_SIZE - 1) /
	    FIND_SIZE : entry->fts_statp->st_size;
	COMPARE(size, plan->o_data);
}

PLAN *
c_size(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *size_str;
	PLAN *new;
	char endch;

	size_str = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(option);
	endch = 'c';
	new->o_data = find_parsenum(new, option->name, size_str, &endch);
	if (endch == 'c')
		divsize = 0;
	return new;
}

/*
 * -type c functions --
 *
 *	True if the type of the file is c, where c is b, c, d, p, f or w
 *	for block special file, character special file, directory, FIFO,
 *	regular file or whiteout respectively.
 */
int
f_type(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return (entry->fts_statp->st_mode & S_IFMT) == plan->m_data;
}

PLAN *
c_type(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *typestring;
	PLAN *new;
	mode_t  mask;

	typestring = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	switch (typestring[0]) {
	case 'b':
		mask = S_IFBLK;
		break;
	case 'c':
		mask = S_IFCHR;
		break;
	case 'd':
		mask = S_IFDIR;
		break;
	case 'f':
		mask = S_IFREG;
		break;
	case 'l':
		mask = S_IFLNK;
		break;
	case 'p':
		mask = S_IFIFO;
		break;
	case 's':
		mask = S_IFSOCK;
		break;
#ifdef FTS_WHITEOUT
	case 'w':
		mask = S_IFWHT;
		ftsoptions |= FTS_WHITEOUT;
		break;
#endif /* FTS_WHITEOUT */
	default:
		errx(1, "%s: %s: unknown type", option->name, typestring);
	}

	new = palloc(option);
	new->m_data = mask;
	return new;
}

/*
 * -user uname functions --
 *
 *	True if the file belongs to the user uname.  If uname is numeric and
 *	an equivalent of the getpwnam() S9.2.2 [POSIX.1] function does not
 *	return a valid user name, uname is taken as a user ID.
 */
int
f_user(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return entry->fts_statp->st_uid == plan->u_data;
}

PLAN *
c_user(option, argvp)
	OPTION *option;
	char ***argvp;
{
	char *username;
	PLAN *new;
	struct passwd *p;
	uid_t uid;

	username = nextarg(option, argvp);
	ftsoptions &= ~FTS_NOSTAT;

	p = getpwnam(username);
	if (p == NULL) {
		uid = atoi(username);
		if (uid == 0 && username[0] != '0')
			errx(1, "%s: %s: no such user", option->name, username);
	} else
		uid = p->pw_uid;

	new = palloc(option);
	new->u_data = uid;
	return new;
}

/*
 * -xdev functions --
 *
 *	Always true, causes find not to decend past directories that have a
 *	different device ID (st_dev, see stat() S5.6.2 [POSIX.1])
 */
PLAN *
c_xdev(option, argvp)
	OPTION *option;
	char ***argvp;
{
	ftsoptions |= FTS_XDEV;

	return palloc(option);
}

/*
 * ( expression ) functions --
 *
 *	True if expression is true.
 */
int
f_expr(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	register PLAN *p;
	register int state = 0;

	for (p = plan->p_data[0];
	    p && (state = (p->execute)(p, entry)); p = p->next);
	return state;
}

/*
 * f_openparen and f_closeparen nodes are temporary place markers.  They are
 * eliminated during phase 2 of find_formplan() --- the '(' node is converted
 * to a f_expr node containing the expression and the ')' node is discarded.
 * The functions themselves are only used as constants.
 */

int
f_openparen(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	abort();
}

int
f_closeparen(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	abort();
}

/* c_openparen == c_simple */
/* c_closeparen == c_simple */

/*
 * AND operator. Since AND is implicit, no node is allocated.
 */
PLAN *
c_and(option, argvp)
	OPTION *option;
	char ***argvp;
{
	return NULL;
}

/*
 * ! expression functions --
 *
 *	Negation of a primary; the unary NOT operator.
 */
int
f_not(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	register PLAN *p;
	register int state = 0;

	for (p = plan->p_data[0];
	    p && (state = (p->execute)(p, entry)); p = p->next);
	return !state;
}

/* c_not == c_simple */

/*
 * expression -o expression functions --
 *
 *	Alternation of primaries; the OR operator.  The second expression is
 * not evaluated if the first expression is true.
 */
int
f_or(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	register PLAN *p;
	register int state = 0;

	for (p = plan->p_data[0];
	    p && (state = (p->execute)(p, entry)); p = p->next);

	if (state)
		return 1;

	for (p = plan->p_data[1];
	    p && (state = (p->execute)(p, entry)); p = p->next);
	return state;
}

/* c_or == c_simple */
