/* $OpenBSD: sftp.c,v 1.123 2010/01/27 19:21:39 djm Exp $ */
/*
 * Copyright (c) 2001-2004 Damien Miller <djm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#include <ctype.h>
#include <errno.h>

#ifdef HAVE_PATHS_H
# include <paths.h>
#endif
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#ifdef USE_LIBEDIT
#include <histedit.h>
#else
typedef void EditLine;
#endif
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#ifdef HAVE_UTIL_H
# include <util.h>
#endif

#ifdef HAVE_LIBUTIL_H
# include <libutil.h>
#endif

#include "xmalloc.h"
#include "log.h"
#include "pathnames.h"
#include "misc.h"

#include "sftp.h"
#include "buffer.h"
#include "sftp-common.h"
#include "sftp-client.h"

#define DEFAULT_COPY_BUFLEN	32768	/* Size of buffer for up/download */
#define DEFAULT_NUM_REQUESTS	64	/* # concurrent outstanding requests */

/* File to read commands from */
FILE* infile;

/* Are we in batchfile mode? */
int batchmode = 0;

/* PID of ssh transport process */
static pid_t sshpid = -1;

/* This is set to 0 if the progressmeter is not desired. */
int showprogress = 1;

/* When this option is set, we always recursively download/upload directories */
int global_rflag = 0;

/* When this option is set, the file transfers will always preserve times */
int global_pflag = 0;

/* SIGINT received during command processing */
volatile sig_atomic_t interrupted = 0;

/* I wish qsort() took a separate ctx for the comparison function...*/
int sort_flag;

/* Context used for commandline completion */
struct complete_ctx {
	struct sftp_conn *conn;
	char **remote_pathp;
};

int remote_glob(struct sftp_conn *, const char *, int,
    int (*)(const char *, int), glob_t *); /* proto for sftp-glob.c */

extern char *__progname;

/* Separators for interactive commands */
#define WHITESPACE " \t\r\n"

/* ls flags */
#define LS_LONG_VIEW	0x0001	/* Full view ala ls -l */
#define LS_SHORT_VIEW	0x0002	/* Single row view ala ls -1 */
#define LS_NUMERIC_VIEW	0x0004	/* Long view with numeric uid/gid */
#define LS_NAME_SORT	0x0008	/* Sort by name (default) */
#define LS_TIME_SORT	0x0010	/* Sort by mtime */
#define LS_SIZE_SORT	0x0020	/* Sort by file size */
#define LS_REVERSE_SORT	0x0040	/* Reverse sort order */
#define LS_SHOW_ALL	0x0080	/* Don't skip filenames starting with '.' */
#define LS_SI_UNITS	0x0100	/* Display sizes as K, M, G, etc. */

#define VIEW_FLAGS	(LS_LONG_VIEW|LS_SHORT_VIEW|LS_NUMERIC_VIEW|LS_SI_UNITS)
#define SORT_FLAGS	(LS_NAME_SORT|LS_TIME_SORT|LS_SIZE_SORT)

/* Commands for interactive mode */
#define I_CHDIR		1
#define I_CHGRP		2
#define I_CHMOD		3
#define I_CHOWN		4
#define I_DF		24
#define I_GET		5
#define I_HELP		6
#define I_LCHDIR	7
#define I_LLS		8
#define I_LMKDIR	9
#define I_LPWD		10
#define I_LS		11
#define I_LUMASK	12
#define I_MKDIR		13
#define I_PUT		14
#define I_PWD		15
#define I_QUIT		16
#define I_RENAME	17
#define I_RM		18
#define I_RMDIR		19
#define I_SHELL		20
#define I_SYMLINK	21
#define I_VERSION	22
#define I_PROGRESS	23

struct CMD {
	const char *c;
	const int n;
	const int t;
};

/* Type of completion */
#define NOARGS	0
#define REMOTE	1
#define LOCAL	2

static const struct CMD cmds[] = {
	{ "bye",	I_QUIT,		NOARGS	},
	{ "cd",		I_CHDIR,	REMOTE	},
	{ "chdir",	I_CHDIR,	REMOTE	},
	{ "chgrp",	I_CHGRP,	REMOTE	},
	{ "chmod",	I_CHMOD,	REMOTE	},
	{ "chown",	I_CHOWN,	REMOTE	},
	{ "df",		I_DF,		REMOTE	},
	{ "dir",	I_LS,		REMOTE	},
	{ "exit",	I_QUIT,		NOARGS	},
	{ "get",	I_GET,		REMOTE	},
	{ "help",	I_HELP,		NOARGS	},
	{ "lcd",	I_LCHDIR,	LOCAL	},
	{ "lchdir",	I_LCHDIR,	LOCAL	},
	{ "lls",	I_LLS,		LOCAL	},
	{ "lmkdir",	I_LMKDIR,	LOCAL	},
	{ "ln",		I_SYMLINK,	REMOTE	},
	{ "lpwd",	I_LPWD,		LOCAL	},
	{ "ls",		I_LS,		REMOTE	},
	{ "lumask",	I_LUMASK,	NOARGS	},
	{ "mkdir",	I_MKDIR,	REMOTE	},
	{ "progress",	I_PROGRESS,	NOARGS	},
	{ "put",	I_PUT,		LOCAL	},
	{ "pwd",	I_PWD,		REMOTE	},
	{ "quit",	I_QUIT,		NOARGS	},
	{ "rename",	I_RENAME,	REMOTE	},
	{ "rm",		I_RM,		REMOTE	},
	{ "rmdir",	I_RMDIR,	REMOTE	},
	{ "symlink",	I_SYMLINK,	REMOTE	},
	{ "version",	I_VERSION,	NOARGS	},
	{ "!",		I_SHELL,	NOARGS	},
	{ "?",		I_HELP,		NOARGS	},
	{ NULL,		-1,		-1	}
};

int interactive_loop(struct sftp_conn *, char *file1, char *file2);

/* ARGSUSED */
static void
killchild(int signo)
{
	if (sshpid > 1) {
		kill(sshpid, SIGTERM);
		waitpid(sshpid, NULL, 0);
	}

	_exit(1);
}

/* ARGSUSED */
static void
cmd_interrupt(int signo)
{
	const char msg[] = "\rInterrupt  \n";
	int olderrno = errno;

	write(STDERR_FILENO, msg, sizeof(msg) - 1);
	interrupted = 1;
	errno = olderrno;
}

static void
help(void)
{
	printf("Available commands:\n"
	    "bye                                Quit sftp\n"
	    "cd path                            Change remote directory to 'path'\n"
	    "chgrp grp path                     Change group of file 'path' to 'grp'\n"
	    "chmod mode path                    Change permissions of file 'path' to 'mode'\n"
	    "chown own path                     Change owner of file 'path' to 'own'\n"
	    "df [-hi] [path]                    Display statistics for current directory or\n"
	    "                                   filesystem containing 'path'\n"
	    "exit                               Quit sftp\n"
	    "get [-Ppr] remote [local]          Download file\n"
	    "help                               Display this help text\n"
	    "lcd path                           Change local directory to 'path'\n"
	    "lls [ls-options [path]]            Display local directory listing\n"
	    "lmkdir path                        Create local directory\n"
	    "ln oldpath newpath                 Symlink remote file\n"
	    "lpwd                               Print local working directory\n"
	    "ls [-1afhlnrSt] [path]             Display remote directory listing\n"
	    "lumask umask                       Set local umask to 'umask'\n"
	    "mkdir path                         Create remote directory\n"
	    "progress                           Toggle display of progress meter\n"
	    "put [-Ppr] local [remote]          Upload file\n"
	    "pwd                                Display remote working directory\n"
	    "quit                               Quit sftp\n"
	    "rename oldpath newpath             Rename remote file\n"
	    "rm path                            Delete remote file\n"
	    "rmdir path                         Remove remote directory\n"
	    "symlink oldpath newpath            Symlink remote file\n"
	    "version                            Show SFTP version\n"
	    "!command                           Execute 'command' in local shell\n"
	    "!                                  Escape to local shell\n"
	    "?                                  Synonym for help\n");
}

static void
local_do_shell(const char *args)
{
	int status;
	char *shell;
	pid_t pid;

	if (!*args)
		args = NULL;

	if ((shell = getenv("SHELL")) == NULL)
		shell = _PATH_BSHELL;

	if ((pid = fork()) == -1)
		fatal("Couldn't fork: %s", strerror(errno));

	if (pid == 0) {
		/* XXX: child has pipe fds to ssh subproc open - issue? */
		if (args) {
			debug3("Executing %s -c \"%s\"", shell, args);
			execl(shell, shell, "-c", args, (char *)NULL);
		} else {
			debug3("Executing %s", shell);
			execl(shell, shell, (char *)NULL);
		}
		fprintf(stderr, "Couldn't execute \"%s\": %s\n", shell,
		    strerror(errno));
		_exit(1);
	}
	while (waitpid(pid, &status, 0) == -1)
		if (errno != EINTR)
			fatal("Couldn't wait for child: %s", strerror(errno));
	if (!WIFEXITED(status))
		error("Shell exited abnormally");
	else if (WEXITSTATUS(status))
		error("Shell exited with status %d", WEXITSTATUS(status));
}

static void
local_do_ls(const char *args)
{
	if (!args || !*args)
		local_do_shell(_PATH_LS);
	else {
		int len = strlen(_PATH_LS " ") + strlen(args) + 1;
		char *buf = xmalloc(len);

		/* XXX: quoting - rip quoting code from ftp? */
		snprintf(buf, len, _PATH_LS " %s", args);
		local_do_shell(buf);
		xfree(buf);
	}
}

/* Strip one path (usually the pwd) from the start of another */
static char *
path_strip(char *path, char *strip)
{
	size_t len;

	if (strip == NULL)
		return (xstrdup(path));

	len = strlen(strip);
	if (strncmp(path, strip, len) == 0) {
		if (strip[len - 1] != '/' && path[len] == '/')
			len++;
		return (xstrdup(path + len));
	}

	return (xstrdup(path));
}

static char *
make_absolute(char *p, char *pwd)
{
	char *abs_str;

	/* Derelativise */
	if (p && p[0] != '/') {
		abs_str = path_append(pwd, p);
		xfree(p);
		return(abs_str);
	} else
		return(p);
}

static int
parse_getput_flags(const char *cmd, char **argv, int argc, int *pflag,
    int *rflag)
{
	extern int opterr, optind, optopt, optreset;
	int ch;

	optind = optreset = 1;
	opterr = 0;

	*rflag = *pflag = 0;
	while ((ch = getopt(argc, argv, "PpRr")) != -1) {
		switch (ch) {
		case 'p':
		case 'P':
			*pflag = 1;
			break;
		case 'r':
		case 'R':
			*rflag = 1;
			break;
		default:
			error("%s: Invalid flag -%c", cmd, optopt);
			return -1;
		}
	}

	return optind;
}

static int
parse_ls_flags(char **argv, int argc, int *lflag)
{
	extern int opterr, optind, optopt, optreset;
	int ch;

	optind = optreset = 1;
	opterr = 0;

	*lflag = LS_NAME_SORT;
	while ((ch = getopt(argc, argv, "1Safhlnrt")) != -1) {
		switch (ch) {
		case '1':
			*lflag &= ~VIEW_FLAGS;
			*lflag |= LS_SHORT_VIEW;
			break;
		case 'S':
			*lflag &= ~SORT_FLAGS;
			*lflag |= LS_SIZE_SORT;
			break;
		case 'a':
			*lflag |= LS_SHOW_ALL;
			break;
		case 'f':
			*lflag &= ~SORT_FLAGS;
			break;
		case 'h':
			*lflag |= LS_SI_UNITS;
			break;
		case 'l':
			*lflag &= ~LS_SHORT_VIEW;
			*lflag |= LS_LONG_VIEW;
			break;
		case 'n':
			*lflag &= ~LS_SHORT_VIEW;
			*lflag |= LS_NUMERIC_VIEW|LS_LONG_VIEW;
			break;
		case 'r':
			*lflag |= LS_REVERSE_SORT;
			break;
		case 't':
			*lflag &= ~SORT_FLAGS;
			*lflag |= LS_TIME_SORT;
			break;
		default:
			error("ls: Invalid flag -%c", optopt);
			return -1;
		}
	}

	return optind;
}

static int
parse_df_flags(const char *cmd, char **argv, int argc, int *hflag, int *iflag)
{
	extern int opterr, optind, optopt, optreset;
	int ch;

	optind = optreset = 1;
	opterr = 0;

	*hflag = *iflag = 0;
	while ((ch = getopt(argc, argv, "hi")) != -1) {
		switch (ch) {
		case 'h':
			*hflag = 1;
			break;
		case 'i':
			*iflag = 1;
			break;
		default:
			error("%s: Invalid flag -%c", cmd, optopt);
			return -1;
		}
	}

	return optind;
}

static int
is_dir(char *path)
{
	struct stat sb;

	/* XXX: report errors? */
	if (stat(path, &sb) == -1)
		return(0);

	return(S_ISDIR(sb.st_mode));
}

static int
remote_is_dir(struct sftp_conn *conn, char *path)
{
	Attrib *a;

	/* XXX: report errors? */
	if ((a = do_stat(conn, path, 1)) == NULL)
		return(0);
	if (!(a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS))
		return(0);
	return(S_ISDIR(a->perm));
}

/* Check whether path returned from glob(..., GLOB_MARK, ...) is a directory */
static int
pathname_is_dir(char *pathname)
{
	size_t l = strlen(pathname);

	return l > 0 && pathname[l - 1] == '/';
}

static int
process_get(struct sftp_conn *conn, char *src, char *dst, char *pwd,
    int pflag, int rflag)
{
	char *abs_src = NULL;
	char *abs_dst = NULL;
	glob_t g;
	char *filename, *tmp=NULL;
	int i, err = 0;

	abs_src = xstrdup(src);
	abs_src = make_absolute(abs_src, pwd);
	memset(&g, 0, sizeof(g));

	debug3("Looking up %s", abs_src);
	if (remote_glob(conn, abs_src, GLOB_MARK, NULL, &g)) {
		error("File \"%s\" not found.", abs_src);
		err = -1;
		goto out;
	}

	/*
	 * If multiple matches then dst must be a directory or
	 * unspecified.
	 */
	if (g.gl_matchc > 1 && dst != NULL && !is_dir(dst)) {
		error("Multiple source paths, but destination "
		    "\"%s\" is not a directory", dst);
		err = -1;
		goto out;
	}

	for (i = 0; g.gl_pathv[i] && !interrupted; i++) {
		tmp = xstrdup(g.gl_pathv[i]);
		if ((filename = basename(tmp)) == NULL) {
			error("basename %s: %s", tmp, strerror(errno));
			xfree(tmp);
			err = -1;
			goto out;
		}

		if (g.gl_matchc == 1 && dst) {
			if (is_dir(dst)) {
				abs_dst = path_append(dst, filename);
			} else {
				abs_dst = xstrdup(dst);
			}
		} else if (dst) {
			abs_dst = path_append(dst, filename);
		} else {
			abs_dst = xstrdup(filename);
		}
		xfree(tmp);

		printf("Fetching %s to %s\n", g.gl_pathv[i], abs_dst);
		if (pathname_is_dir(g.gl_pathv[i]) && (rflag || global_rflag)) {
			if (download_dir(conn, g.gl_pathv[i], abs_dst, NULL, 
			    pflag || global_pflag, 1) == -1)
				err = -1;
		} else {
			if (do_download(conn, g.gl_pathv[i], abs_dst, NULL,
			    pflag || global_pflag) == -1)
				err = -1;
		}
		xfree(abs_dst);
		abs_dst = NULL;
	}

out:
	xfree(abs_src);
	globfree(&g);
	return(err);
}

static int
process_put(struct sftp_conn *conn, char *src, char *dst, char *pwd,
    int pflag, int rflag)
{
	char *tmp_dst = NULL;
	char *abs_dst = NULL;
	char *tmp = NULL, *filename = NULL;
	glob_t g;
	int err = 0;
	int i, dst_is_dir = 1;
	struct stat sb;

	if (dst) {
		tmp_dst = xstrdup(dst);
		tmp_dst = make_absolute(tmp_dst, pwd);
	}

	memset(&g, 0, sizeof(g));
	debug3("Looking up %s", src);
	if (glob(src, GLOB_NOCHECK | GLOB_MARK, NULL, &g)) {
		error("File \"%s\" not found.", src);
		err = -1;
		goto out;
	}

	/* If we aren't fetching to pwd then stash this status for later */
	if (tmp_dst != NULL)
		dst_is_dir = remote_is_dir(conn, tmp_dst);

	/* If multiple matches, dst may be directory or unspecified */
	if (g.gl_matchc > 1 && tmp_dst && !dst_is_dir) {
		error("Multiple paths match, but destination "
		    "\"%s\" is not a directory", tmp_dst);
		err = -1;
		goto out;
	}

	for (i = 0; g.gl_pathv[i] && !interrupted; i++) {
		if (stat(g.gl_pathv[i], &sb) == -1) {
			err = -1;
			error("stat %s: %s", g.gl_pathv[i], strerror(errno));
			continue;
		}
		
		tmp = xstrdup(g.gl_pathv[i]);
		if ((filename = basename(tmp)) == NULL) {
			error("basename %s: %s", tmp, strerror(errno));
			xfree(tmp);
			err = -1;
			goto out;
		}

		if (g.gl_matchc == 1 && tmp_dst) {
			/* If directory specified, append filename */
			if (dst_is_dir)
				abs_dst = path_append(tmp_dst, filename);
			else
				abs_dst = xstrdup(tmp_dst);
		} else if (tmp_dst) {
			abs_dst = path_append(tmp_dst, filename);
		} else {
			abs_dst = make_absolute(xstrdup(filename), pwd);
		}
		xfree(tmp);

		printf("Uploading %s to %s\n", g.gl_pathv[i], abs_dst);
		if (pathname_is_dir(g.gl_pathv[i]) && (rflag || global_rflag)) {
			if (upload_dir(conn, g.gl_pathv[i], abs_dst,
			    pflag || global_pflag, 1) == -1)
				err = -1;
		} else {
			if (do_upload(conn, g.gl_pathv[i], abs_dst,
			    pflag || global_pflag) == -1)
				err = -1;
		}
	}

out:
	if (abs_dst)
		xfree(abs_dst);
	if (tmp_dst)
		xfree(tmp_dst);
	globfree(&g);
	return(err);
}

static int
sdirent_comp(const void *aa, const void *bb)
{
	SFTP_DIRENT *a = *(SFTP_DIRENT **)aa;
	SFTP_DIRENT *b = *(SFTP_DIRENT **)bb;
	int rmul = sort_flag & LS_REVERSE_SORT ? -1 : 1;

#define NCMP(a,b) (a == b ? 0 : (a < b ? 1 : -1))
	if (sort_flag & LS_NAME_SORT)
		return (rmul * strcmp(a->filename, b->filename));
	else if (sort_flag & LS_TIME_SORT)
		return (rmul * NCMP(a->a.mtime, b->a.mtime));
	else if (sort_flag & LS_SIZE_SORT)
		return (rmul * NCMP(a->a.size, b->a.size));

	fatal("Unknown ls sort type");
}

/* sftp ls.1 replacement for directories */
static int
do_ls_dir(struct sftp_conn *conn, char *path, char *strip_path, int lflag)
{
	int n;
	u_int c = 1, colspace = 0, columns = 1;
	SFTP_DIRENT **d;

	if ((n = do_readdir(conn, path, &d)) != 0)
		return (n);

	if (!(lflag & LS_SHORT_VIEW)) {
		u_int m = 0, width = 80;
		struct winsize ws;
		char *tmp;

		/* Count entries for sort and find longest filename */
		for (n = 0; d[n] != NULL; n++) {
			if (d[n]->filename[0] != '.' || (lflag & LS_SHOW_ALL))
				m = MAX(m, strlen(d[n]->filename));
		}

		/* Add any subpath that also needs to be counted */
		tmp = path_strip(path, strip_path);
		m += strlen(tmp);
		xfree(tmp);

		if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) != -1)
			width = ws.ws_col;

		columns = width / (m + 2);
		columns = MAX(columns, 1);
		colspace = width / columns;
		colspace = MIN(colspace, width);
	}

	if (lflag & SORT_FLAGS) {
		for (n = 0; d[n] != NULL; n++)
			;	/* count entries */
		sort_flag = lflag & (SORT_FLAGS|LS_REVERSE_SORT);
		qsort(d, n, sizeof(*d), sdirent_comp);
	}

	for (n = 0; d[n] != NULL && !interrupted; n++) {
		char *tmp, *fname;

		if (d[n]->filename[0] == '.' && !(lflag & LS_SHOW_ALL))
			continue;

		tmp = path_append(path, d[n]->filename);
		fname = path_strip(tmp, strip_path);
		xfree(tmp);

		if (lflag & LS_LONG_VIEW) {
			if (lflag & (LS_NUMERIC_VIEW|LS_SI_UNITS)) {
				char *lname;
				struct stat sb;

				memset(&sb, 0, sizeof(sb));
				attrib_to_stat(&d[n]->a, &sb);
				lname = ls_file(fname, &sb, 1,
				    (lflag & LS_SI_UNITS));
				printf("%s\n", lname);
				xfree(lname);
			} else
				printf("%s\n", d[n]->longname);
		} else {
			printf("%-*s", colspace, fname);
			if (c >= columns) {
				printf("\n");
				c = 1;
			} else
				c++;
		}

		xfree(fname);
	}

	if (!(lflag & LS_LONG_VIEW) && (c != 1))
		printf("\n");

	free_sftp_dirents(d);
	return (0);
}

/* sftp ls.1 replacement which handles path globs */
static int
do_globbed_ls(struct sftp_conn *conn, char *path, char *strip_path,
    int lflag)
{
	glob_t g;
	u_int i, c = 1, colspace = 0, columns = 1;
	Attrib *a = NULL;

	memset(&g, 0, sizeof(g));

	if (remote_glob(conn, path, GLOB_MARK|GLOB_NOCHECK|GLOB_BRACE,
	    NULL, &g) || (g.gl_pathc && !g.gl_matchc)) {
		if (g.gl_pathc)
			globfree(&g);
		error("Can't ls: \"%s\" not found", path);
		return (-1);
	}

	if (interrupted)
		goto out;

	/*
	 * If the glob returns a single match and it is a directory,
	 * then just list its contents.
	 */
	if (g.gl_matchc == 1) {
		if ((a = do_lstat(conn, g.gl_pathv[0], 1)) == NULL) {
			globfree(&g);
			return (-1);
		}
		if ((a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS) &&
		    S_ISDIR(a->perm)) {
			int err;

			err = do_ls_dir(conn, g.gl_pathv[0], strip_path, lflag);
			globfree(&g);
			return (err);
		}
	}

	if (!(lflag & LS_SHORT_VIEW)) {
		u_int m = 0, width = 80;
		struct winsize ws;

		/* Count entries for sort and find longest filename */
		for (i = 0; g.gl_pathv[i]; i++)
			m = MAX(m, strlen(g.gl_pathv[i]));

		if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) != -1)
			width = ws.ws_col;

		columns = width / (m + 2);
		columns = MAX(columns, 1);
		colspace = width / columns;
	}

	for (i = 0; g.gl_pathv[i] && !interrupted; i++, a = NULL) {
		char *fname;

		fname = path_strip(g.gl_pathv[i], strip_path);

		if (lflag & LS_LONG_VIEW) {
			char *lname;
			struct stat sb;

			/*
			 * XXX: this is slow - 1 roundtrip per path
			 * A solution to this is to fork glob() and
			 * build a sftp specific version which keeps the
			 * attribs (which currently get thrown away)
			 * that the server returns as well as the filenames.
			 */
			memset(&sb, 0, sizeof(sb));
			if (a == NULL)
				a = do_lstat(conn, g.gl_pathv[i], 1);
			if (a != NULL)
				attrib_to_stat(a, &sb);
			lname = ls_file(fname, &sb, 1, (lflag & LS_SI_UNITS));
			printf("%s\n", lname);
			xfree(lname);
		} else {
			printf("%-*s", colspace, fname);
			if (c >= columns) {
				printf("\n");
				c = 1;
			} else
				c++;
		}
		xfree(fname);
	}

	if (!(lflag & LS_LONG_VIEW) && (c != 1))
		printf("\n");

 out:
	if (g.gl_pathc)
		globfree(&g);

	return (0);
}

static int
do_df(struct sftp_conn *conn, char *path, int hflag, int iflag)
{
	struct sftp_statvfs st;
	char s_used[FMT_SCALED_STRSIZE];
	char s_avail[FMT_SCALED_STRSIZE];
	char s_root[FMT_SCALED_STRSIZE];
	char s_total[FMT_SCALED_STRSIZE];
	unsigned long long ffree;

	if (do_statvfs(conn, path, &st, 1) == -1)
		return -1;
	if (iflag) {
		ffree = st.f_files ? (100 * (st.f_files - st.f_ffree) / st.f_files) : 0;
		printf("     Inodes        Used       Avail      "
		    "(root)    %%Capacity\n");
		printf("%11llu %11llu %11llu %11llu         %3llu%%\n",
		    (unsigned long long)st.f_files,
		    (unsigned long long)(st.f_files - st.f_ffree),
		    (unsigned long long)st.f_favail,
		    (unsigned long long)st.f_ffree, ffree);
	} else if (hflag) {
		strlcpy(s_used, "error", sizeof(s_used));
		strlcpy(s_avail, "error", sizeof(s_avail));
		strlcpy(s_root, "error", sizeof(s_root));
		strlcpy(s_total, "error", sizeof(s_total));
		fmt_scaled((st.f_blocks - st.f_bfree) * st.f_frsize, s_used);
		fmt_scaled(st.f_bavail * st.f_frsize, s_avail);
		fmt_scaled(st.f_bfree * st.f_frsize, s_root);
		fmt_scaled(st.f_blocks * st.f_frsize, s_total);
		printf("    Size     Used    Avail   (root)    %%Capacity\n");
		printf("%7sB %7sB %7sB %7sB         %3llu%%\n",
		    s_total, s_used, s_avail, s_root,
		    (unsigned long long)(100 * (st.f_blocks - st.f_bfree) /
		    st.f_blocks));
	} else {
		printf("        Size         Used        Avail       "
		    "(root)    %%Capacity\n");
		printf("%12llu %12llu %12llu %12llu         %3llu%%\n",
		    (unsigned long long)(st.f_frsize * st.f_blocks / 1024),
		    (unsigned long long)(st.f_frsize *
		    (st.f_blocks - st.f_bfree) / 1024),
		    (unsigned long long)(st.f_frsize * st.f_bavail / 1024),
		    (unsigned long long)(st.f_frsize * st.f_bfree / 1024),
		    (unsigned long long)(100 * (st.f_blocks - st.f_bfree) /
		    st.f_blocks));
	}
	return 0;
}

/*
 * Undo escaping of glob sequences in place. Used to undo extra escaping
 * applied in makeargv() when the string is destined for a function that
 * does not glob it.
 */
static void
undo_glob_escape(char *s)
{
	size_t i, j;

	for (i = j = 0;;) {
		if (s[i] == '\0') {
			s[j] = '\0';
			return;
		}
		if (s[i] != '\\') {
			s[j++] = s[i++];
			continue;
		}
		/* s[i] == '\\' */
		++i;
		switch (s[i]) {
		case '?':
		case '[':
		case '*':
		case '\\':
			s[j++] = s[i++];
			break;
		case '\0':
			s[j++] = '\\';
			s[j] = '\0';
			return;
		default:
			s[j++] = '\\';
			s[j++] = s[i++];
			break;
		}
	}
}

/*
 * Split a string into an argument vector using sh(1)-style quoting,
 * comment and escaping rules, but with some tweaks to handle glob(3)
 * wildcards.
 * The "sloppy" flag allows for recovery from missing terminating quote, for
 * use in parsing incomplete commandlines during tab autocompletion.
 *
 * Returns NULL on error or a NULL-terminated array of arguments.
 *
 * If "lastquote" is not NULL, the quoting character used for the last
 * argument is placed in *lastquote ("\0", "'" or "\"").
 * 
 * If "terminated" is not NULL, *terminated will be set to 1 when the
 * last argument's quote has been properly terminated or 0 otherwise.
 * This parameter is only of use if "sloppy" is set.
 */
#define MAXARGS 	128
#define MAXARGLEN	8192
static char **
makeargv(const char *arg, int *argcp, int sloppy, char *lastquote,
    u_int *terminated)
{
	int argc, quot;
	size_t i, j;
	static char argvs[MAXARGLEN];
	static char *argv[MAXARGS + 1];
	enum { MA_START, MA_SQUOTE, MA_DQUOTE, MA_UNQUOTED } state, q;

	*argcp = argc = 0;
	if (strlen(arg) > sizeof(argvs) - 1) {
 args_too_longs:
		error("string too long");
		return NULL;
	}
	if (terminated != NULL)
		*terminated = 1;
	if (lastquote != NULL)
		*lastquote = '\0';
	state = MA_START;
	i = j = 0;
	for (;;) {
		if (isspace(arg[i])) {
			if (state == MA_UNQUOTED) {
				/* Terminate current argument */
				argvs[j++] = '\0';
				argc++;
				state = MA_START;
			} else if (state != MA_START)
				argvs[j++] = arg[i];
		} else if (arg[i] == '"' || arg[i] == '\'') {
			q = arg[i] == '"' ? MA_DQUOTE : MA_SQUOTE;
			if (state == MA_START) {
				argv[argc] = argvs + j;
				state = q;
				if (lastquote != NULL)
					*lastquote = arg[i];
			} else if (state == MA_UNQUOTED) 
				state = q;
			else if (state == q)
				state = MA_UNQUOTED;
			else
				argvs[j++] = arg[i];
		} else if (arg[i] == '\\') {
			if (state == MA_SQUOTE || state == MA_DQUOTE) {
				quot = state == MA_SQUOTE ? '\'' : '"';
				/* Unescape quote we are in */
				/* XXX support \n and friends? */
				if (arg[i + 1] == quot) {
					i++;
					argvs[j++] = arg[i];
				} else if (arg[i + 1] == '?' ||
				    arg[i + 1] == '[' || arg[i + 1] == '*') {
					/*
					 * Special case for sftp: append
					 * double-escaped glob sequence -
					 * glob will undo one level of
					 * escaping. NB. string can grow here.
					 */
					if (j >= sizeof(argvs) - 5)
						goto args_too_longs;
					argvs[j++] = '\\';
					argvs[j++] = arg[i++];
					argvs[j++] = '\\';
					argvs[j++] = arg[i];
				} else {
					argvs[j++] = arg[i++];
					argvs[j++] = arg[i];
				}
			} else {
				if (state == MA_START) {
					argv[argc] = argvs + j;
					state = MA_UNQUOTED;
					if (lastquote != NULL)
						*lastquote = '\0';
				}
				if (arg[i + 1] == '?' || arg[i + 1] == '[' ||
				    arg[i + 1] == '*' || arg[i + 1] == '\\') {
					/*
					 * Special case for sftp: append
					 * escaped glob sequence -
					 * glob will undo one level of
					 * escaping.
					 */
					argvs[j++] = arg[i++];
					argvs[j++] = arg[i];
				} else {
					/* Unescape everything */
					/* XXX support \n and friends? */
					i++;
					argvs[j++] = arg[i];
				}
			}
		} else if (arg[i] == '#') {
			if (state == MA_SQUOTE || state == MA_DQUOTE)
				argvs[j++] = arg[i];
			else
				goto string_done;
		} else if (arg[i] == '\0') {
			if (state == MA_SQUOTE || state == MA_DQUOTE) {
				if (sloppy) {
					state = MA_UNQUOTED;
					if (terminated != NULL)
						*terminated = 0;
					goto string_done;
				}
				error("Unterminated quoted argument");
				return NULL;
			}
 string_done:
			if (state == MA_UNQUOTED) {
				argvs[j++] = '\0';
				argc++;
			}
			break;
		} else {
			if (state == MA_START) {
				argv[argc] = argvs + j;
				state = MA_UNQUOTED;
				if (lastquote != NULL)
					*lastquote = '\0';
			}
			if ((state == MA_SQUOTE || state == MA_DQUOTE) &&
			    (arg[i] == '?' || arg[i] == '[' || arg[i] == '*')) {
				/*
				 * Special case for sftp: escape quoted
				 * glob(3) wildcards. NB. string can grow
				 * here.
				 */
				if (j >= sizeof(argvs) - 3)
					goto args_too_longs;
				argvs[j++] = '\\';
				argvs[j++] = arg[i];
			} else
				argvs[j++] = arg[i];
		}
		i++;
	}
	*argcp = argc;
	return argv;
}

static int
parse_args(const char **cpp, int *pflag, int *rflag, int *lflag, int *iflag,
    int *hflag, unsigned long *n_arg, char **path1, char **path2)
{
	const char *cmd, *cp = *cpp;
	char *cp2, **argv;
	int base = 0;
	long l;
	int i, cmdnum, optidx, argc;

	/* Skip leading whitespace */
	cp = cp + strspn(cp, WHITESPACE);

	/* Check for leading '-' (disable error processing) */
	*iflag = 0;
	if (*cp == '-') {
		*iflag = 1;
		cp++;
		cp = cp + strspn(cp, WHITESPACE);
	}

	/* Ignore blank lines and lines which begin with comment '#' char */
	if (*cp == '\0' || *cp == '#')
		return (0);

	if ((argv = makeargv(cp, &argc, 0, NULL, NULL)) == NULL)
		return -1;

	/* Figure out which command we have */
	for (i = 0; cmds[i].c != NULL; i++) {
		if (strcasecmp(cmds[i].c, argv[0]) == 0)
			break;
	}
	cmdnum = cmds[i].n;
	cmd = cmds[i].c;

	/* Special case */
	if (*cp == '!') {
		cp++;
		cmdnum = I_SHELL;
	} else if (cmdnum == -1) {
		error("Invalid command.");
		return -1;
	}

	/* Get arguments and parse flags */
	*lflag = *pflag = *rflag = *hflag = *n_arg = 0;
	*path1 = *path2 = NULL;
	optidx = 1;
	switch (cmdnum) {
	case I_GET:
	case I_PUT:
		if ((optidx = parse_getput_flags(cmd, argv, argc, pflag, rflag)) == -1)
			return -1;
		/* Get first pathname (mandatory) */
		if (argc - optidx < 1) {
			error("You must specify at least one path after a "
			    "%s command.", cmd);
			return -1;
		}
		*path1 = xstrdup(argv[optidx]);
		/* Get second pathname (optional) */
		if (argc - optidx > 1) {
			*path2 = xstrdup(argv[optidx + 1]);
			/* Destination is not globbed */
			undo_glob_escape(*path2);
		}
		break;
	case I_RENAME:
	case I_SYMLINK:
		if (argc - optidx < 2) {
			error("You must specify two paths after a %s "
			    "command.", cmd);
			return -1;
		}
		*path1 = xstrdup(argv[optidx]);
		*path2 = xstrdup(argv[optidx + 1]);
		/* Paths are not globbed */
		undo_glob_escape(*path1);
		undo_glob_escape(*path2);
		break;
	case I_RM:
	case I_MKDIR:
	case I_RMDIR:
	case I_CHDIR:
	case I_LCHDIR:
	case I_LMKDIR:
		/* Get pathname (mandatory) */
		if (argc - optidx < 1) {
			error("You must specify a path after a %s command.",
			    cmd);
			return -1;
		}
		*path1 = xstrdup(argv[optidx]);
		/* Only "rm" globs */
		if (cmdnum != I_RM)
			undo_glob_escape(*path1);
		break;
	case I_DF:
		if ((optidx = parse_df_flags(cmd, argv, argc, hflag,
		    iflag)) == -1)
			return -1;
		/* Default to current directory if no path specified */
		if (argc - optidx < 1)
			*path1 = NULL;
		else {
			*path1 = xstrdup(argv[optidx]);
			undo_glob_escape(*path1);
		}
		break;
	case I_LS:
		if ((optidx = parse_ls_flags(argv, argc, lflag)) == -1)
			return(-1);
		/* Path is optional */
		if (argc - optidx > 0)
			*path1 = xstrdup(argv[optidx]);
		break;
	case I_LLS:
		/* Skip ls command and following whitespace */
		cp = cp + strlen(cmd) + strspn(cp, WHITESPACE);
	case I_SHELL:
		/* Uses the rest of the line */
		break;
	case I_LUMASK:
	case I_CHMOD:
		base = 8;
	case I_CHOWN:
	case I_CHGRP:
		/* Get numeric arg (mandatory) */
		if (argc - optidx < 1)
			goto need_num_arg;
		errno = 0;
		l = strtol(argv[optidx], &cp2, base);
		if (cp2 == argv[optidx] || *cp2 != '\0' ||
		    ((l == LONG_MIN || l == LONG_MAX) && errno == ERANGE) ||
		    l < 0) {
 need_num_arg:
			error("You must supply a numeric argument "
			    "to the %s command.", cmd);
			return -1;
		}
		*n_arg = l;
		if (cmdnum == I_LUMASK)
			break;
		/* Get pathname (mandatory) */
		if (argc - optidx < 2) {
			error("You must specify a path after a %s command.",
			    cmd);
			return -1;
		}
		*path1 = xstrdup(argv[optidx + 1]);
		break;
	case I_QUIT:
	case I_PWD:
	case I_LPWD:
	case I_HELP:
	case I_VERSION:
	case I_PROGRESS:
		break;
	default:
		fatal("Command not implemented");
	}

	*cpp = cp;
	return(cmdnum);
}

static int
parse_dispatch_command(struct sftp_conn *conn, const char *cmd, char **pwd,
    int err_abort)
{
	char *path1, *path2, *tmp;
	int pflag = 0, rflag = 0, lflag = 0, iflag = 0, hflag = 0, cmdnum, i;
	unsigned long n_arg = 0;
	Attrib a, *aa;
	char path_buf[MAXPATHLEN];
	int err = 0;
	glob_t g;

	path1 = path2 = NULL;
	cmdnum = parse_args(&cmd, &pflag, &rflag, &lflag, &iflag, &hflag, &n_arg,
	    &path1, &path2);

	if (iflag != 0)
		err_abort = 0;

	memset(&g, 0, sizeof(g));

	/* Perform command */
	switch (cmdnum) {
	case 0:
		/* Blank line */
		break;
	case -1:
		/* Unrecognized command */
		err = -1;
		break;
	case I_GET:
		err = process_get(conn, path1, path2, *pwd, pflag, rflag);
		break;
	case I_PUT:
		err = process_put(conn, path1, path2, *pwd, pflag, rflag);
		break;
	case I_RENAME:
		path1 = make_absolute(path1, *pwd);
		path2 = make_absolute(path2, *pwd);
		err = do_rename(conn, path1, path2);
		break;
	case I_SYMLINK:
		path2 = make_absolute(path2, *pwd);
		err = do_symlink(conn, path1, path2);
		break;
	case I_RM:
		path1 = make_absolute(path1, *pwd);
		remote_glob(conn, path1, GLOB_NOCHECK, NULL, &g);
		for (i = 0; g.gl_pathv[i] && !interrupted; i++) {
			printf("Removing %s\n", g.gl_pathv[i]);
			err = do_rm(conn, g.gl_pathv[i]);
			if (err != 0 && err_abort)
				break;
		}
		break;
	case I_MKDIR:
		path1 = make_absolute(path1, *pwd);
		attrib_clear(&a);
		a.flags |= SSH2_FILEXFER_ATTR_PERMISSIONS;
		a.perm = 0777;
		err = do_mkdir(conn, path1, &a, 1);
		break;
	case I_RMDIR:
		path1 = make_absolute(path1, *pwd);
		err = do_rmdir(conn, path1);
		break;
	case I_CHDIR:
		path1 = make_absolute(path1, *pwd);
		if ((tmp = do_realpath(conn, path1)) == NULL) {
			err = 1;
			break;
		}
		if ((aa = do_stat(conn, tmp, 0)) == NULL) {
			xfree(tmp);
			err = 1;
			break;
		}
		if (!(aa->flags & SSH2_FILEXFER_ATTR_PERMISSIONS)) {
			error("Can't change directory: Can't check target");
			xfree(tmp);
			err = 1;
			break;
		}
		if (!S_ISDIR(aa->perm)) {
			error("Can't change directory: \"%s\" is not "
			    "a directory", tmp);
			xfree(tmp);
			err = 1;
			break;
		}
		xfree(*pwd);
		*pwd = tmp;
		break;
	case I_LS:
		if (!path1) {
			do_globbed_ls(conn, *pwd, *pwd, lflag);
			break;
		}

		/* Strip pwd off beginning of non-absolute paths */
		tmp = NULL;
		if (*path1 != '/')
			tmp = *pwd;

		path1 = make_absolute(path1, *pwd);
		err = do_globbed_ls(conn, path1, tmp, lflag);
		break;
	case I_DF:
		/* Default to current directory if no path specified */
		if (path1 == NULL)
			path1 = xstrdup(*pwd);
		path1 = make_absolute(path1, *pwd);
		err = do_df(conn, path1, hflag, iflag);
		break;
	case I_LCHDIR:
		if (chdir(path1) == -1) {
			error("Couldn't change local directory to "
			    "\"%s\": %s", path1, strerror(errno));
			err = 1;
		}
		break;
	case I_LMKDIR:
		if (mkdir(path1, 0777) == -1) {
			error("Couldn't create local directory "
			    "\"%s\": %s", path1, strerror(errno));
			err = 1;
		}
		break;
	case I_LLS:
		local_do_ls(cmd);
		break;
	case I_SHELL:
		local_do_shell(cmd);
		break;
	case I_LUMASK:
		umask(n_arg);
		printf("Local umask: %03lo\n", n_arg);
		break;
	case I_CHMOD:
		path1 = make_absolute(path1, *pwd);
		attrib_clear(&a);
		a.flags |= SSH2_FILEXFER_ATTR_PERMISSIONS;
		a.perm = n_arg;
		remote_glob(conn, path1, GLOB_NOCHECK, NULL, &g);
		for (i = 0; g.gl_pathv[i] && !interrupted; i++) {
			printf("Changing mode on %s\n", g.gl_pathv[i]);
			err = do_setstat(conn, g.gl_pathv[i], &a);
			if (err != 0 && err_abort)
				break;
		}
		break;
	case I_CHOWN:
	case I_CHGRP:
		path1 = make_absolute(path1, *pwd);
		remote_glob(conn, path1, GLOB_NOCHECK, NULL, &g);
		for (i = 0; g.gl_pathv[i] && !interrupted; i++) {
			if (!(aa = do_stat(conn, g.gl_pathv[i], 0))) {
				if (err_abort) {
					err = -1;
					break;
				} else
					continue;
			}
			if (!(aa->flags & SSH2_FILEXFER_ATTR_UIDGID)) {
				error("Can't get current ownership of "
				    "remote file \"%s\"", g.gl_pathv[i]);
				if (err_abort) {
					err = -1;
					break;
				} else
					continue;
			}
			aa->flags &= SSH2_FILEXFER_ATTR_UIDGID;
			if (cmdnum == I_CHOWN) {
				printf("Changing owner on %s\n", g.gl_pathv[i]);
				aa->uid = n_arg;
			} else {
				printf("Changing group on %s\n", g.gl_pathv[i]);
				aa->gid = n_arg;
			}
			err = do_setstat(conn, g.gl_pathv[i], aa);
			if (err != 0 && err_abort)
				break;
		}
		break;
	case I_PWD:
		printf("Remote working directory: %s\n", *pwd);
		break;
	case I_LPWD:
		if (!getcwd(path_buf, sizeof(path_buf))) {
			error("Couldn't get local cwd: %s", strerror(errno));
			err = -1;
			break;
		}
		printf("Local working directory: %s\n", path_buf);
		break;
	case I_QUIT:
		/* Processed below */
		break;
	case I_HELP:
		help();
		break;
	case I_VERSION:
		printf("SFTP protocol version %u\n", sftp_proto_version(conn));
		break;
	case I_PROGRESS:
		showprogress = !showprogress;
		if (showprogress)
			printf("Progress meter enabled\n");
		else
			printf("Progress meter disabled\n");
		break;
	default:
		fatal("%d is not implemented", cmdnum);
	}

	if (g.gl_pathc)
		globfree(&g);
	if (path1)
		xfree(path1);
	if (path2)
		xfree(path2);

	/* If an unignored error occurs in batch mode we should abort. */
	if (err_abort && err != 0)
		return (-1);
	else if (cmdnum == I_QUIT)
		return (1);

	return (0);
}

#ifdef USE_LIBEDIT
static char *
prompt(EditLine *el)
{
	return ("sftp> ");
}

/* Display entries in 'list' after skipping the first 'len' chars */
static void
complete_display(char **list, u_int len)
{
	u_int y, m = 0, width = 80, columns = 1, colspace = 0, llen;
	struct winsize ws;
	char *tmp;

	/* Count entries for sort and find longest */
	for (y = 0; list[y]; y++) 
		m = MAX(m, strlen(list[y]));

	if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) != -1)
		width = ws.ws_col;

	m = m > len ? m - len : 0;
	columns = width / (m + 2);
	columns = MAX(columns, 1);
	colspace = width / columns;
	colspace = MIN(colspace, width);

	printf("\n");
	m = 1;
	for (y = 0; list[y]; y++) {
		llen = strlen(list[y]);
		tmp = llen > len ? list[y] + len : "";
		printf("%-*s", colspace, tmp);
		if (m >= columns) {
			printf("\n");
			m = 1;
		} else
			m++;
	}
	printf("\n");
}

/*
 * Given a "list" of words that begin with a common prefix of "word",
 * attempt to find an autocompletion to extends "word" by the next
 * characters common to all entries in "list".
 */
static char *
complete_ambiguous(const char *word, char **list, size_t count)
{
	if (word == NULL)
		return NULL;

	if (count > 0) {
		u_int y, matchlen = strlen(list[0]);

		/* Find length of common stem */
		for (y = 1; list[y]; y++) {
			u_int x;

			for (x = 0; x < matchlen; x++) 
				if (list[0][x] != list[y][x]) 
					break;

			matchlen = x;
		}

		if (matchlen > strlen(word)) {
			char *tmp = xstrdup(list[0]);

			tmp[matchlen] = '\0';
			return tmp;
		}
	} 

	return xstrdup(word);
}

/* Autocomplete a sftp command */
static int
complete_cmd_parse(EditLine *el, char *cmd, int lastarg, char quote,
    int terminated)
{
	u_int y, count = 0, cmdlen, tmplen;
	char *tmp, **list, argterm[3];
	const LineInfo *lf;

	list = xcalloc((sizeof(cmds) / sizeof(*cmds)) + 1, sizeof(char *));

	/* No command specified: display all available commands */
	if (cmd == NULL) {
		for (y = 0; cmds[y].c; y++)
			list[count++] = xstrdup(cmds[y].c);
		
		list[count] = NULL;
		complete_display(list, 0);

		for (y = 0; list[y] != NULL; y++)  
			xfree(list[y]);	
		xfree(list);
		return count;
	}

	/* Prepare subset of commands that start with "cmd" */
	cmdlen = strlen(cmd);
	for (y = 0; cmds[y].c; y++)  {
		if (!strncasecmp(cmd, cmds[y].c, cmdlen)) 
			list[count++] = xstrdup(cmds[y].c);
	}
	list[count] = NULL;

	if (count == 0)
		return 0;

	/* Complete ambigious command */
	tmp = complete_ambiguous(cmd, list, count);
	if (count > 1)
		complete_display(list, 0);

	for (y = 0; list[y]; y++)  
		xfree(list[y]);	
	xfree(list);

	if (tmp != NULL) {
		tmplen = strlen(tmp);
		cmdlen = strlen(cmd);
		/* If cmd may be extended then do so */
		if (tmplen > cmdlen)
			if (el_insertstr(el, tmp + cmdlen) == -1)
				fatal("el_insertstr failed.");
		lf = el_line(el);
		/* Terminate argument cleanly */
		if (count == 1) {
			y = 0;
			if (!terminated)
				argterm[y++] = quote;
			if (lastarg || *(lf->cursor) != ' ')
				argterm[y++] = ' ';
			argterm[y] = '\0';
			if (y > 0 && el_insertstr(el, argterm) == -1)
				fatal("el_insertstr failed.");
		}
		xfree(tmp);
	}

	return count;
}

/*
 * Determine whether a particular sftp command's arguments (if any)
 * represent local or remote files.
 */
static int
complete_is_remote(char *cmd) {
	int i;

	if (cmd == NULL)
		return -1;

	for (i = 0; cmds[i].c; i++) {
		if (!strncasecmp(cmd, cmds[i].c, strlen(cmds[i].c))) 
			return cmds[i].t;
	}

	return -1;
}

/* Autocomplete a filename "file" */
static int
complete_match(EditLine *el, struct sftp_conn *conn, char *remote_path,
    char *file, int remote, int lastarg, char quote, int terminated)
{
	glob_t g;
	char *tmp, *tmp2, ins[3];
	u_int i, hadglob, pwdlen, len, tmplen, filelen;
	const LineInfo *lf;
	
	/* Glob from "file" location */
	if (file == NULL)
		tmp = xstrdup("*");
	else
		xasprintf(&tmp, "%s*", file);

	memset(&g, 0, sizeof(g));
	if (remote != LOCAL) {
		tmp = make_absolute(tmp, remote_path);
		remote_glob(conn, tmp, GLOB_DOOFFS|GLOB_MARK, NULL, &g);
	} else 
		glob(tmp, GLOB_DOOFFS|GLOB_MARK, NULL, &g);
	
	/* Determine length of pwd so we can trim completion display */
	for (hadglob = tmplen = pwdlen = 0; tmp[tmplen] != 0; tmplen++) {
		/* Terminate counting on first unescaped glob metacharacter */
		if (tmp[tmplen] == '*' || tmp[tmplen] == '?') {
			if (tmp[tmplen] != '*' || tmp[tmplen + 1] != '\0')
				hadglob = 1;
			break;
		}
		if (tmp[tmplen] == '\\' && tmp[tmplen + 1] != '\0')
			tmplen++;
		if (tmp[tmplen] == '/')
			pwdlen = tmplen + 1;	/* track last seen '/' */
	}
	xfree(tmp);

	if (g.gl_matchc == 0) 
		goto out;

	if (g.gl_matchc > 1)
		complete_display(g.gl_pathv, pwdlen);

	tmp = NULL;
	/* Don't try to extend globs */
	if (file == NULL || hadglob)
		goto out;

	tmp2 = complete_ambiguous(file, g.gl_pathv, g.gl_matchc);
	tmp = path_strip(tmp2, remote_path);
	xfree(tmp2);

	if (tmp == NULL)
		goto out;

	tmplen = strlen(tmp);
	filelen = strlen(file);

	if (tmplen > filelen)  {
		tmp2 = tmp + filelen;
		len = strlen(tmp2); 
		/* quote argument on way out */
		for (i = 0; i < len; i++) {
			ins[0] = '\\';
			ins[1] = tmp2[i];
			ins[2] = '\0';
			switch (tmp2[i]) {
			case '\'':
			case '"':
			case '\\':
			case '\t':
			case ' ':
				if (quote == '\0' || tmp2[i] == quote) {
					if (el_insertstr(el, ins) == -1)
						fatal("el_insertstr "
						    "failed.");
					break;
				}
				/* FALLTHROUGH */
			default:
				if (el_insertstr(el, ins + 1) == -1)
					fatal("el_insertstr failed.");
				break;
			}
		}
	}

	lf = el_line(el);
	if (g.gl_matchc == 1) {
		i = 0;
		if (!terminated)
			ins[i++] = quote;
		if (*(lf->cursor - 1) != '/' &&
		    (lastarg || *(lf->cursor) != ' '))
			ins[i++] = ' ';
		ins[i] = '\0';
		if (i > 0 && el_insertstr(el, ins) == -1)
			fatal("el_insertstr failed.");
	}
	xfree(tmp);

 out:
	globfree(&g);
	return g.gl_matchc;
}

/* tab-completion hook function, called via libedit */
static unsigned char
complete(EditLine *el, int ch)
{
	char **argv, *line, quote; 
	u_int argc, carg, cursor, len, terminated, ret = CC_ERROR;
	const LineInfo *lf;
	struct complete_ctx *complete_ctx;

	lf = el_line(el);
	if (el_get(el, EL_CLIENTDATA, (void**)&complete_ctx) != 0)
		fatal("%s: el_get failed", __func__);

	/* Figure out which argument the cursor points to */
	cursor = lf->cursor - lf->buffer;
	line = (char *)xmalloc(cursor + 1);
	memcpy(line, lf->buffer, cursor);
	line[cursor] = '\0';
	argv = makeargv(line, &carg, 1, &quote, &terminated);
	xfree(line);

	/* Get all the arguments on the line */
	len = lf->lastchar - lf->buffer;
	line = (char *)xmalloc(len + 1);
	memcpy(line, lf->buffer, len);
	line[len] = '\0';
	argv = makeargv(line, &argc, 1, NULL, NULL);

	/* Ensure cursor is at EOL or a argument boundary */
	if (line[cursor] != ' ' && line[cursor] != '\0' &&
	    line[cursor] != '\n') {
		xfree(line);
		return ret;
	}

	if (carg == 0) {
		/* Show all available commands */
		complete_cmd_parse(el, NULL, argc == carg, '\0', 1);
		ret = CC_REDISPLAY;
	} else if (carg == 1 && cursor > 0 && line[cursor - 1] != ' ')  {
		/* Handle the command parsing */
		if (complete_cmd_parse(el, argv[0], argc == carg,
		    quote, terminated) != 0) 
			ret = CC_REDISPLAY;
	} else if (carg >= 1) {
		/* Handle file parsing */
		int remote = complete_is_remote(argv[0]);
		char *filematch = NULL;

		if (carg > 1 && line[cursor-1] != ' ')
			filematch = argv[carg - 1];

		if (remote != 0 &&
		    complete_match(el, complete_ctx->conn,
		    *complete_ctx->remote_pathp, filematch,
		    remote, carg == argc, quote, terminated) != 0) 
			ret = CC_REDISPLAY;
	}

	xfree(line);	
	return ret;
}
#endif /* USE_LIBEDIT */

int
interactive_loop(struct sftp_conn *conn, char *file1, char *file2)
{
	char *remote_path;
	char *dir = NULL;
	char cmd[2048];
	int err, interactive;
	EditLine *el = NULL;
#ifdef USE_LIBEDIT
	History *hl = NULL;
	HistEvent hev;
	extern char *__progname;
	struct complete_ctx complete_ctx;

	if (!batchmode && isatty(STDIN_FILENO)) {
		if ((el = el_init(__progname, stdin, stdout, stderr)) == NULL)
			fatal("Couldn't initialise editline");
		if ((hl = history_init()) == NULL)
			fatal("Couldn't initialise editline history");
		history(hl, &hev, H_SETSIZE, 100);
		el_set(el, EL_HIST, history, hl);

		el_set(el, EL_PROMPT, prompt);
		el_set(el, EL_EDITOR, "emacs");
		el_set(el, EL_TERMINAL, NULL);
		el_set(el, EL_SIGNAL, 1);
		el_source(el, NULL);

		/* Tab Completion */
		el_set(el, EL_ADDFN, "ftp-complete", 
		    "Context senstive argument completion", complete);
		complete_ctx.conn = conn;
		complete_ctx.remote_pathp = &remote_path;
		el_set(el, EL_CLIENTDATA, (void*)&complete_ctx);
		el_set(el, EL_BIND, "^I", "ftp-complete", NULL);
	}
#endif /* USE_LIBEDIT */

	remote_path = do_realpath(conn, ".");
	if (remote_path == NULL)
		fatal("Need cwd");

	if (file1 != NULL) {
		dir = xstrdup(file1);
		dir = make_absolute(dir, remote_path);

		if (remote_is_dir(conn, dir) && file2 == NULL) {
			printf("Changing to: %s\n", dir);
			snprintf(cmd, sizeof cmd, "cd \"%s\"", dir);
			if (parse_dispatch_command(conn, cmd,
			    &remote_path, 1) != 0) {
				xfree(dir);
				xfree(remote_path);
				xfree(conn);
				return (-1);
			}
		} else {
			if (file2 == NULL)
				snprintf(cmd, sizeof cmd, "get %s", dir);
			else
				snprintf(cmd, sizeof cmd, "get %s %s", dir,
				    file2);

			err = parse_dispatch_command(conn, cmd,
			    &remote_path, 1);
			xfree(dir);
			xfree(remote_path);
			xfree(conn);
			return (err);
		}
		xfree(dir);
	}

#if defined(HAVE_SETVBUF) && !defined(BROKEN_SETVBUF)
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(infile, NULL, _IOLBF, 0);
#else
	setlinebuf(stdout);
	setlinebuf(infile);
#endif

	interactive = !batchmode && isatty(STDIN_FILENO);
	err = 0;
	for (;;) {
		char *cp;

		signal(SIGINT, SIG_IGN);

		if (el == NULL) {
			if (interactive)
				printf("sftp> ");
			if (fgets(cmd, sizeof(cmd), infile) == NULL) {
				if (interactive)
					printf("\n");
				break;
			}
			if (!interactive) { /* Echo command */
				printf("sftp> %s", cmd);
				if (strlen(cmd) > 0 &&
				    cmd[strlen(cmd) - 1] != '\n')
					printf("\n");
			}
		} else {
#ifdef USE_LIBEDIT
			const char *line;
			int count = 0;

			if ((line = el_gets(el, &count)) == NULL ||
			    count <= 0) {
				printf("\n");
 				break;
			}
			history(hl, &hev, H_ENTER, line);
			if (strlcpy(cmd, line, sizeof(cmd)) >= sizeof(cmd)) {
				fprintf(stderr, "Error: input line too long\n");
				continue;
			}
#endif /* USE_LIBEDIT */
		}

		cp = strrchr(cmd, '\n');
		if (cp)
			*cp = '\0';

		/* Handle user interrupts gracefully during commands */
		interrupted = 0;
		signal(SIGINT, cmd_interrupt);

		err = parse_dispatch_command(conn, cmd, &remote_path,
		    batchmode);
		if (err != 0)
			break;
	}
	xfree(remote_path);
	xfree(conn);

#ifdef USE_LIBEDIT
	if (el != NULL)
		el_end(el);
#endif /* USE_LIBEDIT */

	/* err == 1 signifies normal "quit" exit */
	return (err >= 0 ? 0 : -1);
}

static void
connect_to_server(char *path, char **args, int *in, int *out)
{
	int c_in, c_out;

#ifdef USE_PIPES
	int pin[2], pout[2];

	if ((pipe(pin) == -1) || (pipe(pout) == -1))
		fatal("pipe: %s", strerror(errno));
	*in = pin[0];
	*out = pout[1];
	c_in = pout[0];
	c_out = pin[1];
#else /* USE_PIPES */
	int inout[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, inout) == -1)
		fatal("socketpair: %s", strerror(errno));
	*in = *out = inout[0];
	c_in = c_out = inout[1];
#endif /* USE_PIPES */

	if ((sshpid = fork()) == -1)
		fatal("fork: %s", strerror(errno));
	else if (sshpid == 0) {
		if ((dup2(c_in, STDIN_FILENO) == -1) ||
		    (dup2(c_out, STDOUT_FILENO) == -1)) {
			fprintf(stderr, "dup2: %s\n", strerror(errno));
			_exit(1);
		}
		close(*in);
		close(*out);
		close(c_in);
		close(c_out);

		/*
		 * The underlying ssh is in the same process group, so we must
		 * ignore SIGINT if we want to gracefully abort commands,
		 * otherwise the signal will make it to the ssh process and
		 * kill it too.  Contrawise, since sftp sends SIGTERMs to the
		 * underlying ssh, it must *not* ignore that signal.
		 */
		signal(SIGINT, SIG_IGN);
		signal(SIGTERM, SIG_DFL);
		execvp(path, args);
		fprintf(stderr, "exec: %s: %s\n", path, strerror(errno));
		_exit(1);
	}

	signal(SIGTERM, killchild);
	signal(SIGINT, killchild);
	signal(SIGHUP, killchild);
	close(c_in);
	close(c_out);
}

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-1246Cpqrv] [-B buffer_size] [-b batchfile] [-c cipher]\n"
	    "          [-D sftp_server_path] [-F ssh_config] "
	    "[-i identity_file]\n"
	    "          [-o ssh_option] [-P port] [-R num_requests] "
	    "[-S program]\n"
	    "          [-s subsystem | sftp_server] host\n"
	    "       %s [user@]host[:file ...]\n"
	    "       %s [user@]host[:dir[/]]\n"
	    "       %s -b batchfile [user@]host\n",
	    __progname, __progname, __progname, __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int in, out, ch, err;
	char *host = NULL, *userhost, *cp, *file2 = NULL;
	int debug_level = 0, sshver = 2;
	char *file1 = NULL, *sftp_server = NULL;
	char *ssh_program = _PATH_SSH_PROGRAM, *sftp_direct = NULL;
	LogLevel ll = SYSLOG_LEVEL_INFO;
	arglist args;
	extern int optind;
	extern char *optarg;
	struct sftp_conn *conn;
	size_t copy_buffer_len = DEFAULT_COPY_BUFLEN;
	size_t num_requests = DEFAULT_NUM_REQUESTS;

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
	sanitise_stdfd();

	__progname = ssh_get_progname(argv[0]);
	memset(&args, '\0', sizeof(args));
	args.list = NULL;
	addargs(&args, "%s", ssh_program);
	addargs(&args, "-oForwardX11 no");
	addargs(&args, "-oForwardAgent no");
	addargs(&args, "-oPermitLocalCommand no");
	addargs(&args, "-oClearAllForwardings yes");

	ll = SYSLOG_LEVEL_INFO;
	infile = stdin;

	while ((ch = getopt(argc, argv,
	    "1246hpqrvCc:D:i:o:s:S:b:B:F:P:R:")) != -1) {
		switch (ch) {
		/* Passed through to ssh(1) */
		case '4':
		case '6':
		case 'C':
			addargs(&args, "-%c", ch);
			break;
		/* Passed through to ssh(1) with argument */
		case 'F':
		case 'c':
		case 'i':
		case 'o':
			addargs(&args, "-%c", ch);
			addargs(&args, "%s", optarg);
			break;
		case 'q':
			showprogress = 0;
			addargs(&args, "-%c", ch);
			break;
		case 'P':
			addargs(&args, "-oPort %s", optarg);
			break;
		case 'v':
			if (debug_level < 3) {
				addargs(&args, "-v");
				ll = SYSLOG_LEVEL_DEBUG1 + debug_level;
			}
			debug_level++;
			break;
		case '1':
			sshver = 1;
			if (sftp_server == NULL)
				sftp_server = _PATH_SFTP_SERVER;
			break;
		case '2':
			sshver = 2;
			break;
		case 'B':
			copy_buffer_len = strtol(optarg, &cp, 10);
			if (copy_buffer_len == 0 || *cp != '\0')
				fatal("Invalid buffer size \"%s\"", optarg);
			break;
		case 'b':
			if (batchmode)
				fatal("Batch file already specified.");

			/* Allow "-" as stdin */
			if (strcmp(optarg, "-") != 0 &&
			    (infile = fopen(optarg, "r")) == NULL)
				fatal("%s (%s).", strerror(errno), optarg);
			showprogress = 0;
			batchmode = 1;
			addargs(&args, "-obatchmode yes");
			break;
		case 'p':
			global_pflag = 1;
			break;
		case 'D':
			sftp_direct = optarg;
			break;
		case 'r':
			global_rflag = 1;
			break;
		case 'R':
			num_requests = strtol(optarg, &cp, 10);
			if (num_requests == 0 || *cp != '\0')
				fatal("Invalid number of requests \"%s\"",
				    optarg);
			break;
		case 's':
			sftp_server = optarg;
			break;
		case 'S':
			ssh_program = optarg;
			replacearg(&args, 0, "%s", ssh_program);
			break;
		case 'h':
		default:
			usage();
		}
	}

	if (!isatty(STDERR_FILENO))
		showprogress = 0;

	log_init(argv[0], ll, SYSLOG_FACILITY_USER, 1);

	if (sftp_direct == NULL) {
		if (optind == argc || argc > (optind + 2))
			usage();

		userhost = xstrdup(argv[optind]);
		file2 = argv[optind+1];

		if ((host = strrchr(userhost, '@')) == NULL)
			host = userhost;
		else {
			*host++ = '\0';
			if (!userhost[0]) {
				fprintf(stderr, "Missing username\n");
				usage();
			}
			addargs(&args, "-l");
			addargs(&args, "%s", userhost);
		}

		if ((cp = colon(host)) != NULL) {
			*cp++ = '\0';
			file1 = cp;
		}

		host = cleanhostname(host);
		if (!*host) {
			fprintf(stderr, "Missing hostname\n");
			usage();
		}

		addargs(&args, "-oProtocol %d", sshver);

		/* no subsystem if the server-spec contains a '/' */
		if (sftp_server == NULL || strchr(sftp_server, '/') == NULL)
			addargs(&args, "-s");

		addargs(&args, "--");
		addargs(&args, "%s", host);
		addargs(&args, "%s", (sftp_server != NULL ?
		    sftp_server : "sftp"));

		connect_to_server(ssh_program, args.list, &in, &out);
	} else {
		args.list = NULL;
		addargs(&args, "sftp-server");

		connect_to_server(sftp_direct, args.list, &in, &out);
	}
	freeargs(&args);

	conn = do_init(in, out, copy_buffer_len, num_requests);
	if (conn == NULL)
		fatal("Couldn't initialise connection to server");

	if (!batchmode) {
		if (sftp_direct == NULL)
			fprintf(stderr, "Connected to %s.\n", host);
		else
			fprintf(stderr, "Attached to %s.\n", sftp_direct);
	}

	err = interactive_loop(conn, file1, file2);

#if !defined(USE_PIPES)
	shutdown(in, SHUT_RDWR);
	shutdown(out, SHUT_RDWR);
#endif

	close(in);
	close(out);
	if (batchmode)
		fclose(infile);

	while (waitpid(sshpid, NULL, 0) == -1)
		if (errno != EINTR)
			fatal("Couldn't wait for ssh process: %s",
			    strerror(errno));

	exit(err == 0 ? 0 : 1);
}
