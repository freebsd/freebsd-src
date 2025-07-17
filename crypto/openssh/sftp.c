/* $OpenBSD: sftp.c,v 1.239 2024/06/26 23:14:14 deraadt Exp $ */
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
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif
#ifdef USE_LIBEDIT
#include <histedit.h>
#else
typedef void EditLine;
#endif
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_UTIL_H
# include <util.h>
#endif

#include "xmalloc.h"
#include "log.h"
#include "pathnames.h"
#include "misc.h"
#include "utf8.h"

#include "sftp.h"
#include "ssherr.h"
#include "sshbuf.h"
#include "sftp-common.h"
#include "sftp-client.h"
#include "sftp-usergroup.h"

/* File to read commands from */
FILE* infile;

/* Are we in batchfile mode? */
int batchmode = 0;

/* PID of ssh transport process */
static volatile pid_t sshpid = -1;

/* Suppress diagnostic messages */
int quiet = 0;

/* This is set to 0 if the progressmeter is not desired. */
int showprogress = 1;

/* When this option is set, we always recursively download/upload directories */
int global_rflag = 0;

/* When this option is set, we resume download or upload if possible */
int global_aflag = 0;

/* When this option is set, the file transfers will always preserve times */
int global_pflag = 0;

/* When this option is set, transfers will have fsync() called on each file */
int global_fflag = 0;

/* SIGINT received during command processing */
volatile sig_atomic_t interrupted = 0;

/* I wish qsort() took a separate ctx for the comparison function...*/
int sort_flag;
glob_t *sort_glob;

/* Context used for commandline completion */
struct complete_ctx {
	struct sftp_conn *conn;
	char **remote_pathp;
};

int sftp_glob(struct sftp_conn *, const char *, int,
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
enum sftp_command {
	I_CHDIR = 1,
	I_CHGRP,
	I_CHMOD,
	I_CHOWN,
	I_COPY,
	I_DF,
	I_GET,
	I_HELP,
	I_LCHDIR,
	I_LINK,
	I_LLS,
	I_LMKDIR,
	I_LPWD,
	I_LS,
	I_LUMASK,
	I_MKDIR,
	I_PUT,
	I_PWD,
	I_QUIT,
	I_REGET,
	I_RENAME,
	I_REPUT,
	I_RM,
	I_RMDIR,
	I_SHELL,
	I_SYMLINK,
	I_VERSION,
	I_PROGRESS,
};

struct CMD {
	const char *c;
	const int n;
	const int t;	/* Completion type for the first argument */
	const int t2;	/* completion type for the optional second argument */
};

/* Type of completion */
#define NOARGS	0
#define REMOTE	1
#define LOCAL	2

static const struct CMD cmds[] = {
	{ "bye",	I_QUIT,		NOARGS,		NOARGS	},
	{ "cd",		I_CHDIR,	REMOTE,		NOARGS	},
	{ "chdir",	I_CHDIR,	REMOTE,		NOARGS	},
	{ "chgrp",	I_CHGRP,	REMOTE,		NOARGS	},
	{ "chmod",	I_CHMOD,	REMOTE,		NOARGS	},
	{ "chown",	I_CHOWN,	REMOTE,		NOARGS	},
	{ "copy",	I_COPY,		REMOTE,		LOCAL	},
	{ "cp",		I_COPY,		REMOTE,		LOCAL	},
	{ "df",		I_DF,		REMOTE,		NOARGS	},
	{ "dir",	I_LS,		REMOTE,		NOARGS	},
	{ "exit",	I_QUIT,		NOARGS,		NOARGS	},
	{ "get",	I_GET,		REMOTE,		LOCAL	},
	{ "help",	I_HELP,		NOARGS,		NOARGS	},
	{ "lcd",	I_LCHDIR,	LOCAL,		NOARGS	},
	{ "lchdir",	I_LCHDIR,	LOCAL,		NOARGS	},
	{ "lls",	I_LLS,		LOCAL,		NOARGS	},
	{ "lmkdir",	I_LMKDIR,	LOCAL,		NOARGS	},
	{ "ln",		I_LINK,		REMOTE,		REMOTE	},
	{ "lpwd",	I_LPWD,		LOCAL,		NOARGS	},
	{ "ls",		I_LS,		REMOTE,		NOARGS	},
	{ "lumask",	I_LUMASK,	NOARGS,		NOARGS	},
	{ "mkdir",	I_MKDIR,	REMOTE,		NOARGS	},
	{ "mget",	I_GET,		REMOTE,		LOCAL	},
	{ "mput",	I_PUT,		LOCAL,		REMOTE	},
	{ "progress",	I_PROGRESS,	NOARGS,		NOARGS	},
	{ "put",	I_PUT,		LOCAL,		REMOTE	},
	{ "pwd",	I_PWD,		REMOTE,		NOARGS	},
	{ "quit",	I_QUIT,		NOARGS,		NOARGS	},
	{ "reget",	I_REGET,	REMOTE,		LOCAL	},
	{ "rename",	I_RENAME,	REMOTE,		REMOTE	},
	{ "reput",	I_REPUT,	LOCAL,		REMOTE	},
	{ "rm",		I_RM,		REMOTE,		NOARGS	},
	{ "rmdir",	I_RMDIR,	REMOTE,		NOARGS	},
	{ "symlink",	I_SYMLINK,	REMOTE,		REMOTE	},
	{ "version",	I_VERSION,	NOARGS,		NOARGS	},
	{ "!",		I_SHELL,	NOARGS,		NOARGS	},
	{ "?",		I_HELP,		NOARGS,		NOARGS	},
	{ NULL,		-1,		-1,		-1	}
};

static void
killchild(int signo)
{
	pid_t pid;

	pid = sshpid;
	if (pid > 1) {
		kill(pid, SIGTERM);
		(void)waitpid(pid, NULL, 0);
	}

	_exit(1);
}

static void
suspchild(int signo)
{
	int save_errno = errno;
	if (sshpid > 1) {
		kill(sshpid, signo);
		while (waitpid(sshpid, NULL, WUNTRACED) == -1 && errno == EINTR)
			continue;
	}
	kill(getpid(), SIGSTOP);
	errno = save_errno;
}

static void
cmd_interrupt(int signo)
{
	const char msg[] = "\rInterrupt  \n";
	int olderrno = errno;

	(void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
	interrupted = 1;
	errno = olderrno;
}

static void
read_interrupt(int signo)
{
	interrupted = 1;
}

static void
sigchld_handler(int sig)
{
	int save_errno = errno;
	pid_t pid;
	const char msg[] = "\rConnection closed.  \n";

	/* Report if ssh transport process dies. */
	while ((pid = waitpid(sshpid, NULL, WNOHANG)) == -1 && errno == EINTR)
		continue;
	if (pid == sshpid) {
		if (!quiet)
		    (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
		sshpid = -1;
	}

	errno = save_errno;
}

static void
help(void)
{
	printf("Available commands:\n"
	    "bye                                Quit sftp\n"
	    "cd path                            Change remote directory to 'path'\n"
	    "chgrp [-h] grp path                Change group of file 'path' to 'grp'\n"
	    "chmod [-h] mode path               Change permissions of file 'path' to 'mode'\n"
	    "chown [-h] own path                Change owner of file 'path' to 'own'\n"
	    "copy oldpath newpath               Copy remote file\n"
	    "cp oldpath newpath                 Copy remote file\n"
	    "df [-hi] [path]                    Display statistics for current directory or\n"
	    "                                   filesystem containing 'path'\n"
	    "exit                               Quit sftp\n"
	    "get [-afpR] remote [local]         Download file\n"
	    "help                               Display this help text\n"
	    "lcd path                           Change local directory to 'path'\n"
	    "lls [ls-options [path]]            Display local directory listing\n"
	    "lmkdir path                        Create local directory\n"
	    "ln [-s] oldpath newpath            Link remote file (-s for symlink)\n"
	    "lpwd                               Print local working directory\n"
	    "ls [-1afhlnrSt] [path]             Display remote directory listing\n"
	    "lumask umask                       Set local umask to 'umask'\n"
	    "mkdir path                         Create remote directory\n"
	    "progress                           Toggle display of progress meter\n"
	    "put [-afpR] local [remote]         Upload file\n"
	    "pwd                                Display remote working directory\n"
	    "quit                               Quit sftp\n"
	    "reget [-fpR] remote [local]        Resume download file\n"
	    "rename oldpath newpath             Rename remote file\n"
	    "reput [-fpR] local [remote]        Resume upload file\n"
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

	if ((shell = getenv("SHELL")) == NULL || *shell == '\0')
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
		free(buf);
	}
}

/* Strip one path (usually the pwd) from the start of another */
static char *
path_strip(const char *path, const char *strip)
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

static int
parse_getput_flags(const char *cmd, char **argv, int argc,
    int *aflag, int *fflag, int *pflag, int *rflag)
{
	extern int opterr, optind, optopt, optreset;
	int ch;

	optind = optreset = 1;
	opterr = 0;

	*aflag = *fflag = *rflag = *pflag = 0;
	while ((ch = getopt(argc, argv, "afPpRr")) != -1) {
		switch (ch) {
		case 'a':
			*aflag = 1;
			break;
		case 'f':
			*fflag = 1;
			break;
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
parse_link_flags(const char *cmd, char **argv, int argc, int *sflag)
{
	extern int opterr, optind, optopt, optreset;
	int ch;

	optind = optreset = 1;
	opterr = 0;

	*sflag = 0;
	while ((ch = getopt(argc, argv, "s")) != -1) {
		switch (ch) {
		case 's':
			*sflag = 1;
			break;
		default:
			error("%s: Invalid flag -%c", cmd, optopt);
			return -1;
		}
	}

	return optind;
}

static int
parse_rename_flags(const char *cmd, char **argv, int argc, int *lflag)
{
	extern int opterr, optind, optopt, optreset;
	int ch;

	optind = optreset = 1;
	opterr = 0;

	*lflag = 0;
	while ((ch = getopt(argc, argv, "l")) != -1) {
		switch (ch) {
		case 'l':
			*lflag = 1;
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
parse_ch_flags(const char *cmd, char **argv, int argc, int *hflag)
{
	extern int opterr, optind, optopt, optreset;
	int ch;

	optind = optreset = 1;
	opterr = 0;

	*hflag = 0;
	while ((ch = getopt(argc, argv, "h")) != -1) {
		switch (ch) {
		case 'h':
			*hflag = 1;
			break;
		default:
			error("%s: Invalid flag -%c", cmd, optopt);
			return -1;
		}
	}

	return optind;
}

static int
parse_no_flags(const char *cmd, char **argv, int argc)
{
	extern int opterr, optind, optopt, optreset;
	int ch;

	optind = optreset = 1;
	opterr = 0;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			error("%s: Invalid flag -%c", cmd, optopt);
			return -1;
		}
	}

	return optind;
}

static char *
escape_glob(const char *s)
{
	size_t i, o, len;
	char *ret;

	len = strlen(s);
	ret = xcalloc(2, len + 1);
	for (i = o = 0; i < len; i++) {
		if (strchr("[]?*\\", s[i]) != NULL)
			ret[o++] = '\\';
		ret[o++] = s[i];
	}
	ret[o++] = '\0';
	return ret;
}

/*
 * Arg p must be dynamically allocated.  make_absolute will either return it
 * or free it and allocate a new one.  Caller must free returned string.
 */
static char *
make_absolute_pwd_glob(char *p, const char *pwd)
{
	char *ret, *escpwd;

	escpwd = escape_glob(pwd);
	if (p == NULL)
		return escpwd;
	ret = sftp_make_absolute(p, escpwd);
	free(escpwd);
	return ret;
}

static int
local_is_dir(const char *path)
{
	struct stat sb;

	if (stat(path, &sb) == -1)
		return 0;
	return S_ISDIR(sb.st_mode);
}

static int
process_get(struct sftp_conn *conn, const char *src, const char *dst,
    const char *pwd, int pflag, int rflag, int resume, int fflag)
{
	char *filename, *abs_src = NULL, *abs_dst = NULL, *tmp = NULL;
	glob_t g;
	int i, r, err = 0;

	abs_src = make_absolute_pwd_glob(xstrdup(src), pwd);
	memset(&g, 0, sizeof(g));

	debug3("Looking up %s", abs_src);
	if ((r = sftp_glob(conn, abs_src, GLOB_MARK, NULL, &g)) != 0) {
		if (r == GLOB_NOSPACE) {
			error("Too many matches for \"%s\".", abs_src);
		} else {
			error("File \"%s\" not found.", abs_src);
		}
		err = -1;
		goto out;
	}

	/*
	 * If multiple matches then dst must be a directory or
	 * unspecified.
	 */
	if (g.gl_matchc > 1 && dst != NULL && !local_is_dir(dst)) {
		error("Multiple source paths, but destination "
		    "\"%s\" is not a directory", dst);
		err = -1;
		goto out;
	}

	for (i = 0; g.gl_pathv[i] && !interrupted; i++) {
		tmp = xstrdup(g.gl_pathv[i]);
		if ((filename = basename(tmp)) == NULL) {
			error("basename %s: %s", tmp, strerror(errno));
			free(tmp);
			err = -1;
			goto out;
		}

		if (g.gl_matchc == 1 && dst) {
			if (local_is_dir(dst)) {
				abs_dst = sftp_path_append(dst, filename);
			} else {
				abs_dst = xstrdup(dst);
			}
		} else if (dst) {
			abs_dst = sftp_path_append(dst, filename);
		} else {
			abs_dst = xstrdup(filename);
		}
		free(tmp);

		resume |= global_aflag;
		if (!quiet && resume)
			mprintf("Resuming %s to %s\n",
			    g.gl_pathv[i], abs_dst);
		else if (!quiet && !resume)
			mprintf("Fetching %s to %s\n",
			    g.gl_pathv[i], abs_dst);
		/* XXX follow link flag */
		if (sftp_globpath_is_dir(g.gl_pathv[i]) &&
		    (rflag || global_rflag)) {
			if (sftp_download_dir(conn, g.gl_pathv[i], abs_dst,
			    NULL, pflag || global_pflag, 1, resume,
			    fflag || global_fflag, 0, 0) == -1)
				err = -1;
		} else {
			if (sftp_download(conn, g.gl_pathv[i], abs_dst, NULL,
			    pflag || global_pflag, resume,
			    fflag || global_fflag, 0) == -1)
				err = -1;
		}
		free(abs_dst);
		abs_dst = NULL;
	}

out:
	free(abs_src);
	globfree(&g);
	return(err);
}

static int
process_put(struct sftp_conn *conn, const char *src, const char *dst,
    const char *pwd, int pflag, int rflag, int resume, int fflag)
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
		tmp_dst = sftp_make_absolute(tmp_dst, pwd);
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
		dst_is_dir = sftp_remote_is_dir(conn, tmp_dst);

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
			free(tmp);
			err = -1;
			goto out;
		}

		free(abs_dst);
		abs_dst = NULL;
		if (g.gl_matchc == 1 && tmp_dst) {
			/* If directory specified, append filename */
			if (dst_is_dir)
				abs_dst = sftp_path_append(tmp_dst, filename);
			else
				abs_dst = xstrdup(tmp_dst);
		} else if (tmp_dst) {
			abs_dst = sftp_path_append(tmp_dst, filename);
		} else {
			abs_dst = sftp_make_absolute(xstrdup(filename), pwd);
		}
		free(tmp);

		resume |= global_aflag;
		if (!quiet && resume)
			mprintf("Resuming upload of %s to %s\n",
			    g.gl_pathv[i], abs_dst);
		else if (!quiet && !resume)
			mprintf("Uploading %s to %s\n",
			    g.gl_pathv[i], abs_dst);
		/* XXX follow_link_flag */
		if (sftp_globpath_is_dir(g.gl_pathv[i]) &&
		    (rflag || global_rflag)) {
			if (sftp_upload_dir(conn, g.gl_pathv[i], abs_dst,
			    pflag || global_pflag, 1, resume,
			    fflag || global_fflag, 0, 0) == -1)
				err = -1;
		} else {
			if (sftp_upload(conn, g.gl_pathv[i], abs_dst,
			    pflag || global_pflag, resume,
			    fflag || global_fflag, 0) == -1)
				err = -1;
		}
	}

out:
	free(abs_dst);
	free(tmp_dst);
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
do_ls_dir(struct sftp_conn *conn, const char *path,
    const char *strip_path, int lflag)
{
	int n;
	u_int c = 1, colspace = 0, columns = 1;
	SFTP_DIRENT **d;

	if ((n = sftp_readdir(conn, path, &d)) != 0)
		return (n);

	if (!(lflag & LS_SHORT_VIEW)) {
		u_int m = 0, width = 80;
		struct winsize ws;
		char *tmp;

		/* Count entries for sort and find longest filename */
		for (n = 0; d[n] != NULL; n++) {
			if (d[n]->filename[0] != '.' || (lflag & LS_SHOW_ALL))
				m = MAXIMUM(m, strlen(d[n]->filename));
		}

		/* Add any subpath that also needs to be counted */
		tmp = path_strip(path, strip_path);
		m += strlen(tmp);
		free(tmp);

		if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) != -1)
			width = ws.ws_col;

		columns = width / (m + 2);
		columns = MAXIMUM(columns, 1);
		colspace = width / columns;
		colspace = MINIMUM(colspace, width);
	}

	if (lflag & SORT_FLAGS) {
		for (n = 0; d[n] != NULL; n++)
			;	/* count entries */
		sort_flag = lflag & (SORT_FLAGS|LS_REVERSE_SORT);
		qsort(d, n, sizeof(*d), sdirent_comp);
	}

	get_remote_user_groups_from_dirents(conn, d);
	for (n = 0; d[n] != NULL && !interrupted; n++) {
		char *tmp, *fname;

		if (d[n]->filename[0] == '.' && !(lflag & LS_SHOW_ALL))
			continue;

		tmp = sftp_path_append(path, d[n]->filename);
		fname = path_strip(tmp, strip_path);
		free(tmp);

		if (lflag & LS_LONG_VIEW) {
			if ((lflag & (LS_NUMERIC_VIEW|LS_SI_UNITS)) != 0 ||
			    sftp_can_get_users_groups_by_id(conn)) {
				char *lname;
				struct stat sb;

				memset(&sb, 0, sizeof(sb));
				attrib_to_stat(&d[n]->a, &sb);
				lname = ls_file(fname, &sb, 1,
				    (lflag & LS_SI_UNITS),
				    ruser_name(sb.st_uid),
				    rgroup_name(sb.st_gid));
				mprintf("%s\n", lname);
				free(lname);
			} else
				mprintf("%s\n", d[n]->longname);
		} else {
			mprintf("%-*s", colspace, fname);
			if (c >= columns) {
				printf("\n");
				c = 1;
			} else
				c++;
		}

		free(fname);
	}

	if (!(lflag & LS_LONG_VIEW) && (c != 1))
		printf("\n");

	sftp_free_dirents(d);
	return (0);
}

static int
sglob_comp(const void *aa, const void *bb)
{
	u_int a = *(const u_int *)aa;
	u_int b = *(const u_int *)bb;
	const char *ap = sort_glob->gl_pathv[a];
	const char *bp = sort_glob->gl_pathv[b];
	const struct stat *as = sort_glob->gl_statv[a];
	const struct stat *bs = sort_glob->gl_statv[b];
	int rmul = sort_flag & LS_REVERSE_SORT ? -1 : 1;

#define NCMP(a,b) (a == b ? 0 : (a < b ? 1 : -1))
	if (sort_flag & LS_NAME_SORT)
		return (rmul * strcmp(ap, bp));
	else if (sort_flag & LS_TIME_SORT) {
#if defined(HAVE_STRUCT_STAT_ST_MTIM)
		if (timespeccmp(&as->st_mtim, &bs->st_mtim, ==))
			return 0;
		return timespeccmp(&as->st_mtim, &bs->st_mtim, <) ?
		    rmul : -rmul;
#elif defined(HAVE_STRUCT_STAT_ST_MTIME)
		return (rmul * NCMP(as->st_mtime, bs->st_mtime));
#else
	return rmul * 1;
#endif
	} else if (sort_flag & LS_SIZE_SORT)
		return (rmul * NCMP(as->st_size, bs->st_size));

	fatal("Unknown ls sort type");
}

/* sftp ls.1 replacement which handles path globs */
static int
do_globbed_ls(struct sftp_conn *conn, const char *path,
    const char *strip_path, int lflag)
{
	char *fname, *lname;
	glob_t g;
	int err, r;
	struct winsize ws;
	u_int i, j, nentries, *indices = NULL, c = 1;
	u_int colspace = 0, columns = 1, m = 0, width = 80;

	memset(&g, 0, sizeof(g));

	if ((r = sftp_glob(conn, path,
	    GLOB_MARK|GLOB_NOCHECK|GLOB_BRACE|GLOB_KEEPSTAT|GLOB_NOSORT,
	    NULL, &g)) != 0 ||
	    (g.gl_pathc && !g.gl_matchc)) {
		if (g.gl_pathc)
			globfree(&g);
		if (r == GLOB_NOSPACE) {
			error("Can't ls: Too many matches for \"%s\"", path);
		} else {
			error("Can't ls: \"%s\" not found", path);
		}
		return -1;
	}

	if (interrupted)
		goto out;

	/*
	 * If the glob returns a single match and it is a directory,
	 * then just list its contents.
	 */
	if (g.gl_matchc == 1 && g.gl_statv[0] != NULL &&
	    S_ISDIR(g.gl_statv[0]->st_mode)) {
		err = do_ls_dir(conn, g.gl_pathv[0], strip_path, lflag);
		globfree(&g);
		return err;
	}

	if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) != -1)
		width = ws.ws_col;

	if (!(lflag & LS_SHORT_VIEW)) {
		/* Count entries for sort and find longest filename */
		for (i = 0; g.gl_pathv[i]; i++)
			m = MAXIMUM(m, strlen(g.gl_pathv[i]));

		columns = width / (m + 2);
		columns = MAXIMUM(columns, 1);
		colspace = width / columns;
	}

	/*
	 * Sorting: rather than mess with the contents of glob_t, prepare
	 * an array of indices into it and sort that. For the usual
	 * unsorted case, the indices are just the identity 1=1, 2=2, etc.
	 */
	for (nentries = 0; g.gl_pathv[nentries] != NULL; nentries++)
		;	/* count entries */
	indices = xcalloc(nentries, sizeof(*indices));
	for (i = 0; i < nentries; i++)
		indices[i] = i;

	if (lflag & SORT_FLAGS) {
		sort_glob = &g;
		sort_flag = lflag & (SORT_FLAGS|LS_REVERSE_SORT);
		qsort(indices, nentries, sizeof(*indices), sglob_comp);
		sort_glob = NULL;
	}

	get_remote_user_groups_from_glob(conn, &g);
	for (j = 0; j < nentries && !interrupted; j++) {
		i = indices[j];
		fname = path_strip(g.gl_pathv[i], strip_path);
		if (lflag & LS_LONG_VIEW) {
			if (g.gl_statv[i] == NULL) {
				error("no stat information for %s", fname);
				free(fname);
				continue;
			}
			lname = ls_file(fname, g.gl_statv[i], 1,
			    (lflag & LS_SI_UNITS),
			    ruser_name(g.gl_statv[i]->st_uid),
			    rgroup_name(g.gl_statv[i]->st_gid));
			mprintf("%s\n", lname);
			free(lname);
		} else {
			mprintf("%-*s", colspace, fname);
			if (c >= columns) {
				printf("\n");
				c = 1;
			} else
				c++;
		}
		free(fname);
	}

	if (!(lflag & LS_LONG_VIEW) && (c != 1))
		printf("\n");

 out:
	if (g.gl_pathc)
		globfree(&g);
	free(indices);

	return 0;
}

static int
do_df(struct sftp_conn *conn, const char *path, int hflag, int iflag)
{
	struct sftp_statvfs st;
	char s_used[FMT_SCALED_STRSIZE], s_avail[FMT_SCALED_STRSIZE];
	char s_root[FMT_SCALED_STRSIZE], s_total[FMT_SCALED_STRSIZE];
	char s_icapacity[16], s_dcapacity[16];

	if (sftp_statvfs(conn, path, &st, 1) == -1)
		return -1;
	if (st.f_files == 0)
		strlcpy(s_icapacity, "ERR", sizeof(s_icapacity));
	else {
		snprintf(s_icapacity, sizeof(s_icapacity), "%3llu%%",
		    (unsigned long long)(100 * (st.f_files - st.f_ffree) /
		    st.f_files));
	}
	if (st.f_blocks == 0)
		strlcpy(s_dcapacity, "ERR", sizeof(s_dcapacity));
	else {
		snprintf(s_dcapacity, sizeof(s_dcapacity), "%3llu%%",
		    (unsigned long long)(100 * (st.f_blocks - st.f_bfree) /
		    st.f_blocks));
	}
	if (iflag) {
		printf("     Inodes        Used       Avail      "
		    "(root)    %%Capacity\n");
		printf("%11llu %11llu %11llu %11llu         %s\n",
		    (unsigned long long)st.f_files,
		    (unsigned long long)(st.f_files - st.f_ffree),
		    (unsigned long long)st.f_favail,
		    (unsigned long long)st.f_ffree, s_icapacity);
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
		printf("%7sB %7sB %7sB %7sB         %s\n",
		    s_total, s_used, s_avail, s_root, s_dcapacity);
	} else {
		printf("        Size         Used        Avail       "
		    "(root)    %%Capacity\n");
		printf("%12llu %12llu %12llu %12llu         %s\n",
		    (unsigned long long)(st.f_frsize * st.f_blocks / 1024),
		    (unsigned long long)(st.f_frsize *
		    (st.f_blocks - st.f_bfree) / 1024),
		    (unsigned long long)(st.f_frsize * st.f_bavail / 1024),
		    (unsigned long long)(st.f_frsize * st.f_bfree / 1024),
		    s_dcapacity);
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
#define MAXARGS		128
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
		if ((size_t)argc >= sizeof(argv) / sizeof(*argv)){
			error("Too many arguments.");
			return NULL;
		}
		if (isspace((unsigned char)arg[i])) {
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
parse_args(const char **cpp, int *ignore_errors, int *disable_echo, int *aflag,
	  int *fflag, int *hflag, int *iflag, int *lflag, int *pflag,
	  int *rflag, int *sflag,
    unsigned long *n_arg, char **path1, char **path2)
{
	const char *cmd, *cp = *cpp;
	char *cp2, **argv;
	int base = 0;
	long long ll;
	int path1_mandatory = 0, i, cmdnum, optidx, argc;

	/* Skip leading whitespace */
	cp = cp + strspn(cp, WHITESPACE);

	/*
	 * Check for leading '-' (disable error processing) and '@' (suppress
	 * command echo)
	 */
	*ignore_errors = 0;
	*disable_echo = 0;
	for (;*cp != '\0'; cp++) {
		if (*cp == '-') {
			*ignore_errors = 1;
		} else if (*cp == '@') {
			*disable_echo = 1;
		} else {
			/* all other characters terminate prefix processing */
			break;
		}
	}
	cp = cp + strspn(cp, WHITESPACE);

	/* Ignore blank lines and lines which begin with comment '#' char */
	if (*cp == '\0' || *cp == '#')
		return (0);

	if ((argv = makeargv(cp, &argc, 0, NULL, NULL)) == NULL)
		return -1;

	/* Figure out which command we have */
	for (i = 0; cmds[i].c != NULL; i++) {
		if (argv[0] != NULL && strcasecmp(cmds[i].c, argv[0]) == 0)
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
	*aflag = *fflag = *hflag = *iflag = *lflag = *pflag = 0;
	*rflag = *sflag = 0;
	*path1 = *path2 = NULL;
	optidx = 1;
	switch (cmdnum) {
	case I_GET:
	case I_REGET:
	case I_REPUT:
	case I_PUT:
		if ((optidx = parse_getput_flags(cmd, argv, argc,
		    aflag, fflag, pflag, rflag)) == -1)
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
	case I_LINK:
		if ((optidx = parse_link_flags(cmd, argv, argc, sflag)) == -1)
			return -1;
		goto parse_two_paths;
	case I_COPY:
		if ((optidx = parse_no_flags(cmd, argv, argc)) == -1)
			return -1;
		goto parse_two_paths;
	case I_RENAME:
		if ((optidx = parse_rename_flags(cmd, argv, argc, lflag)) == -1)
			return -1;
		goto parse_two_paths;
	case I_SYMLINK:
		if ((optidx = parse_no_flags(cmd, argv, argc)) == -1)
			return -1;
 parse_two_paths:
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
	case I_LMKDIR:
		path1_mandatory = 1;
		/* FALLTHROUGH */
	case I_CHDIR:
	case I_LCHDIR:
		if ((optidx = parse_no_flags(cmd, argv, argc)) == -1)
			return -1;
		/* Get pathname (mandatory) */
		if (argc - optidx < 1) {
			if (!path1_mandatory)
				break; /* return a NULL path1 */
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
		/* FALLTHROUGH */
	case I_CHOWN:
	case I_CHGRP:
		if ((optidx = parse_ch_flags(cmd, argv, argc, hflag)) == -1)
			return -1;
		/* Get numeric arg (mandatory) */
		if (argc - optidx < 1)
			goto need_num_arg;
		errno = 0;
		ll = strtoll(argv[optidx], &cp2, base);
		if (cp2 == argv[optidx] || *cp2 != '\0' ||
		    ((ll == LLONG_MIN || ll == LLONG_MAX) && errno == ERANGE) ||
		    ll < 0 || ll > UINT32_MAX) {
 need_num_arg:
			error("You must supply a numeric argument "
			    "to the %s command.", cmd);
			return -1;
		}
		*n_arg = ll;
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
		if ((optidx = parse_no_flags(cmd, argv, argc)) == -1)
			return -1;
		break;
	default:
		fatal("Command not implemented");
	}

	*cpp = cp;
	return(cmdnum);
}

static int
parse_dispatch_command(struct sftp_conn *conn, const char *cmd, char **pwd,
    const char *startdir, int err_abort, int echo_command)
{
	const char *ocmd = cmd;
	char *path1, *path2, *tmp;
	int ignore_errors = 0, disable_echo = 1;
	int aflag = 0, fflag = 0, hflag = 0, iflag = 0;
	int lflag = 0, pflag = 0, rflag = 0, sflag = 0;
	int cmdnum, i;
	unsigned long n_arg = 0;
	Attrib a, aa;
	char path_buf[PATH_MAX];
	int err = 0;
	glob_t g;

	path1 = path2 = NULL;
	cmdnum = parse_args(&cmd, &ignore_errors, &disable_echo, &aflag, &fflag,
	    &hflag, &iflag, &lflag, &pflag, &rflag, &sflag, &n_arg,
	    &path1, &path2);
	if (ignore_errors != 0)
		err_abort = 0;

	if (echo_command && !disable_echo)
		mprintf("sftp> %s\n", ocmd);

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
	case I_REGET:
		aflag = 1;
		/* FALLTHROUGH */
	case I_GET:
		err = process_get(conn, path1, path2, *pwd, pflag,
		    rflag, aflag, fflag);
		break;
	case I_REPUT:
		aflag = 1;
		/* FALLTHROUGH */
	case I_PUT:
		err = process_put(conn, path1, path2, *pwd, pflag,
		    rflag, aflag, fflag);
		break;
	case I_COPY:
		path1 = sftp_make_absolute(path1, *pwd);
		path2 = sftp_make_absolute(path2, *pwd);
		err = sftp_copy(conn, path1, path2);
		break;
	case I_RENAME:
		path1 = sftp_make_absolute(path1, *pwd);
		path2 = sftp_make_absolute(path2, *pwd);
		err = sftp_rename(conn, path1, path2, lflag);
		break;
	case I_SYMLINK:
		sflag = 1;
		/* FALLTHROUGH */
	case I_LINK:
		if (!sflag)
			path1 = sftp_make_absolute(path1, *pwd);
		path2 = sftp_make_absolute(path2, *pwd);
		err = (sflag ? sftp_symlink : sftp_hardlink)(conn,
		    path1, path2);
		break;
	case I_RM:
		path1 = make_absolute_pwd_glob(path1, *pwd);
		sftp_glob(conn, path1, GLOB_NOCHECK, NULL, &g);
		for (i = 0; g.gl_pathv[i] && !interrupted; i++) {
			if (!quiet)
				mprintf("Removing %s\n", g.gl_pathv[i]);
			err = sftp_rm(conn, g.gl_pathv[i]);
			if (err != 0 && err_abort)
				break;
		}
		break;
	case I_MKDIR:
		path1 = sftp_make_absolute(path1, *pwd);
		attrib_clear(&a);
		a.flags |= SSH2_FILEXFER_ATTR_PERMISSIONS;
		a.perm = 0777;
		err = sftp_mkdir(conn, path1, &a, 1);
		break;
	case I_RMDIR:
		path1 = sftp_make_absolute(path1, *pwd);
		err = sftp_rmdir(conn, path1);
		break;
	case I_CHDIR:
		if (path1 == NULL || *path1 == '\0')
			path1 = xstrdup(startdir);
		path1 = sftp_make_absolute(path1, *pwd);
		if ((tmp = sftp_realpath(conn, path1)) == NULL) {
			err = 1;
			break;
		}
		if (sftp_stat(conn, tmp, 0, &aa) != 0) {
			free(tmp);
			err = 1;
			break;
		}
		if (!(aa.flags & SSH2_FILEXFER_ATTR_PERMISSIONS)) {
			error("Can't change directory: Can't check target");
			free(tmp);
			err = 1;
			break;
		}
		if (!S_ISDIR(aa.perm)) {
			error("Can't change directory: \"%s\" is not "
			    "a directory", tmp);
			free(tmp);
			err = 1;
			break;
		}
		free(*pwd);
		*pwd = tmp;
		break;
	case I_LS:
		if (!path1) {
			do_ls_dir(conn, *pwd, *pwd, lflag);
			break;
		}

		/* Strip pwd off beginning of non-absolute paths */
		tmp = NULL;
		if (!path_absolute(path1))
			tmp = *pwd;

		path1 = make_absolute_pwd_glob(path1, *pwd);
		err = do_globbed_ls(conn, path1, tmp, lflag);
		break;
	case I_DF:
		/* Default to current directory if no path specified */
		if (path1 == NULL)
			path1 = xstrdup(*pwd);
		path1 = sftp_make_absolute(path1, *pwd);
		err = do_df(conn, path1, hflag, iflag);
		break;
	case I_LCHDIR:
		if (path1 == NULL || *path1 == '\0')
			path1 = xstrdup("~");
		tmp = tilde_expand_filename(path1, getuid());
		free(path1);
		path1 = tmp;
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
		path1 = make_absolute_pwd_glob(path1, *pwd);
		attrib_clear(&a);
		a.flags |= SSH2_FILEXFER_ATTR_PERMISSIONS;
		a.perm = n_arg;
		sftp_glob(conn, path1, GLOB_NOCHECK, NULL, &g);
		for (i = 0; g.gl_pathv[i] && !interrupted; i++) {
			if (!quiet)
				mprintf("Changing mode on %s\n",
				    g.gl_pathv[i]);
			err = (hflag ? sftp_lsetstat : sftp_setstat)(conn,
			    g.gl_pathv[i], &a);
			if (err != 0 && err_abort)
				break;
		}
		break;
	case I_CHOWN:
	case I_CHGRP:
		path1 = make_absolute_pwd_glob(path1, *pwd);
		sftp_glob(conn, path1, GLOB_NOCHECK, NULL, &g);
		for (i = 0; g.gl_pathv[i] && !interrupted; i++) {
			if ((hflag ? sftp_lstat : sftp_stat)(conn,
			    g.gl_pathv[i], 0, &aa) != 0) {
				if (err_abort) {
					err = -1;
					break;
				} else
					continue;
			}
			if (!(aa.flags & SSH2_FILEXFER_ATTR_UIDGID)) {
				error("Can't get current ownership of "
				    "remote file \"%s\"", g.gl_pathv[i]);
				if (err_abort) {
					err = -1;
					break;
				} else
					continue;
			}
			aa.flags &= SSH2_FILEXFER_ATTR_UIDGID;
			if (cmdnum == I_CHOWN) {
				if (!quiet)
					mprintf("Changing owner on %s\n",
					    g.gl_pathv[i]);
				aa.uid = n_arg;
			} else {
				if (!quiet)
					mprintf("Changing group on %s\n",
					    g.gl_pathv[i]);
				aa.gid = n_arg;
			}
			err = (hflag ? sftp_lsetstat : sftp_setstat)(conn,
			    g.gl_pathv[i], &aa);
			if (err != 0 && err_abort)
				break;
		}
		break;
	case I_PWD:
		mprintf("Remote working directory: %s\n", *pwd);
		break;
	case I_LPWD:
		if (!getcwd(path_buf, sizeof(path_buf))) {
			error("Couldn't get local cwd: %s", strerror(errno));
			err = -1;
			break;
		}
		mprintf("Local working directory: %s\n", path_buf);
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
	free(path1);
	free(path2);

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
		m = MAXIMUM(m, strlen(list[y]));

	if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) != -1)
		width = ws.ws_col;

	m = m > len ? m - len : 0;
	columns = width / (m + 2);
	columns = MAXIMUM(columns, 1);
	colspace = width / columns;
	colspace = MINIMUM(colspace, width);

	printf("\n");
	m = 1;
	for (y = 0; list[y]; y++) {
		llen = strlen(list[y]);
		tmp = llen > len ? list[y] + len : "";
		mprintf("%-*s", colspace, tmp);
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
			free(list[y]);
		free(list);
		return count;
	}

	/* Prepare subset of commands that start with "cmd" */
	cmdlen = strlen(cmd);
	for (y = 0; cmds[y].c; y++)  {
		if (!strncasecmp(cmd, cmds[y].c, cmdlen))
			list[count++] = xstrdup(cmds[y].c);
	}
	list[count] = NULL;

	if (count == 0) {
		free(list);
		return 0;
	}

	/* Complete ambiguous command */
	tmp = complete_ambiguous(cmd, list, count);
	if (count > 1)
		complete_display(list, 0);

	for (y = 0; list[y]; y++)
		free(list[y]);
	free(list);

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
		free(tmp);
	}

	return count;
}

/*
 * Determine whether a particular sftp command's arguments (if any) represent
 * local or remote files. The "cmdarg" argument specifies the actual argument
 * and accepts values 1 or 2.
 */
static int
complete_is_remote(char *cmd, int cmdarg) {
	int i;

	if (cmd == NULL)
		return -1;

	for (i = 0; cmds[i].c; i++) {
		if (!strncasecmp(cmd, cmds[i].c, strlen(cmds[i].c))) {
			if (cmdarg == 1)
				return cmds[i].t;
			else if (cmdarg == 2)
				return cmds[i].t2;
			break;
		}
	}

	return -1;
}

/* Autocomplete a filename "file" */
static int
complete_match(EditLine *el, struct sftp_conn *conn, char *remote_path,
    char *file, int remote, int lastarg, char quote, int terminated)
{
	glob_t g;
	char *tmp, *tmp2, ins[8];
	u_int i, hadglob, pwdlen, len, tmplen, filelen, cesc, isesc, isabs;
	int clen;
	const LineInfo *lf;

	/* Glob from "file" location */
	if (file == NULL)
		tmp = xstrdup("*");
	else
		xasprintf(&tmp, "%s*", file);

	/* Check if the path is absolute. */
	isabs = path_absolute(tmp);

	memset(&g, 0, sizeof(g));
	if (remote != LOCAL) {
		tmp = make_absolute_pwd_glob(tmp, remote_path);
		sftp_glob(conn, tmp, GLOB_DOOFFS|GLOB_MARK, NULL, &g);
	} else
		(void)glob(tmp, GLOB_DOOFFS|GLOB_MARK, NULL, &g);

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
	free(tmp);
	tmp = NULL;

	if (g.gl_matchc == 0)
		goto out;

	if (g.gl_matchc > 1)
		complete_display(g.gl_pathv, pwdlen);

	/* Don't try to extend globs */
	if (file == NULL || hadglob)
		goto out;

	tmp2 = complete_ambiguous(file, g.gl_pathv, g.gl_matchc);
	tmp = path_strip(tmp2, isabs ? NULL : remote_path);
	free(tmp2);

	if (tmp == NULL)
		goto out;

	tmplen = strlen(tmp);
	filelen = strlen(file);

	/* Count the number of escaped characters in the input string. */
	cesc = isesc = 0;
	for (i = 0; i < filelen; i++) {
		if (!isesc && file[i] == '\\' && i + 1 < filelen){
			isesc = 1;
			cesc++;
		} else
			isesc = 0;
	}

	if (tmplen > (filelen - cesc)) {
		tmp2 = tmp + filelen - cesc;
		len = strlen(tmp2);
		/* quote argument on way out */
		for (i = 0; i < len; i += clen) {
			if ((clen = mblen(tmp2 + i, len - i)) < 0 ||
			    (size_t)clen > sizeof(ins) - 2)
				fatal("invalid multibyte character");
			ins[0] = '\\';
			memcpy(ins + 1, tmp2 + i, clen);
			ins[clen + 1] = '\0';
			switch (tmp2[i]) {
			case '\'':
			case '"':
			case '\\':
			case '\t':
			case '[':
			case ' ':
			case '#':
			case '*':
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
		if (!terminated && quote != '\0')
			ins[i++] = quote;
		if (*(lf->cursor - 1) != '/' &&
		    (lastarg || *(lf->cursor) != ' '))
			ins[i++] = ' ';
		ins[i] = '\0';
		if (i > 0 && el_insertstr(el, ins) == -1)
			fatal("el_insertstr failed.");
	}
	free(tmp);

 out:
	globfree(&g);
	return g.gl_matchc;
}

/* tab-completion hook function, called via libedit */
static unsigned char
complete(EditLine *el, int ch)
{
	char **argv, *line, quote;
	int argc, carg;
	u_int cursor, len, terminated, ret = CC_ERROR;
	const LineInfo *lf;
	struct complete_ctx *complete_ctx;

	lf = el_line(el);
	if (el_get(el, EL_CLIENTDATA, (void**)&complete_ctx) != 0)
		fatal_f("el_get failed");

	/* Figure out which argument the cursor points to */
	cursor = lf->cursor - lf->buffer;
	line = xmalloc(cursor + 1);
	memcpy(line, lf->buffer, cursor);
	line[cursor] = '\0';
	argv = makeargv(line, &carg, 1, &quote, &terminated);
	free(line);

	/* Get all the arguments on the line */
	len = lf->lastchar - lf->buffer;
	line = xmalloc(len + 1);
	memcpy(line, lf->buffer, len);
	line[len] = '\0';
	argv = makeargv(line, &argc, 1, NULL, NULL);

	/* Ensure cursor is at EOL or a argument boundary */
	if (line[cursor] != ' ' && line[cursor] != '\0' &&
	    line[cursor] != '\n') {
		free(line);
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
		int remote = 0;
		int i = 0, cmdarg = 0;
		char *filematch = NULL;

		if (carg > 1 && line[cursor-1] != ' ')
			filematch = argv[carg - 1];

		for (i = 1; i < carg; i++) {
			/* Skip flags */
			if (argv[i][0] != '-')
				cmdarg++;
		}

		/*
		 * If previous argument is complete, then offer completion
		 * on the next one.
		 */
		if (line[cursor - 1] == ' ')
			cmdarg++;

		remote = complete_is_remote(argv[0], cmdarg);

		if ((remote == REMOTE || remote == LOCAL) &&
		    complete_match(el, complete_ctx->conn,
		    *complete_ctx->remote_pathp, filematch,
		    remote, carg == argc, quote, terminated) != 0)
			ret = CC_REDISPLAY;
	}

	free(line);
	return ret;
}
#endif /* USE_LIBEDIT */

static int
interactive_loop(struct sftp_conn *conn, char *file1, char *file2)
{
	char *remote_path;
	char *dir = NULL, *startdir = NULL;
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
		    "Context sensitive argument completion", complete);
		complete_ctx.conn = conn;
		complete_ctx.remote_pathp = &remote_path;
		el_set(el, EL_CLIENTDATA, (void*)&complete_ctx);
		el_set(el, EL_BIND, "^I", "ftp-complete", NULL);
		/* enable ctrl-left-arrow and ctrl-right-arrow */
		el_set(el, EL_BIND, "\\e[1;5C", "em-next-word", NULL);
		el_set(el, EL_BIND, "\\e\\e[C", "em-next-word", NULL);
		el_set(el, EL_BIND, "\\e[1;5D", "ed-prev-word", NULL);
		el_set(el, EL_BIND, "\\e\\e[D", "ed-prev-word", NULL);
		/* make ^w match ksh behaviour */
		el_set(el, EL_BIND, "^w", "ed-delete-prev-word", NULL);
	}
#endif /* USE_LIBEDIT */

	if ((remote_path = sftp_realpath(conn, ".")) == NULL)
		fatal("Need cwd");
	startdir = xstrdup(remote_path);

	if (file1 != NULL) {
		dir = xstrdup(file1);
		dir = sftp_make_absolute(dir, remote_path);

		if (sftp_remote_is_dir(conn, dir) && file2 == NULL) {
			if (!quiet)
				mprintf("Changing to: %s\n", dir);
			snprintf(cmd, sizeof cmd, "cd \"%s\"", dir);
			if (parse_dispatch_command(conn, cmd,
			    &remote_path, startdir, 1, 0) != 0) {
				free(dir);
				free(startdir);
				free(remote_path);
				free(conn);
				return (-1);
			}
		} else {
			/* XXX this is wrong wrt quoting */
			snprintf(cmd, sizeof cmd, "get%s %s%s%s",
			    global_aflag ? " -a" : "", dir,
			    file2 == NULL ? "" : " ",
			    file2 == NULL ? "" : file2);
			err = parse_dispatch_command(conn, cmd,
			    &remote_path, startdir, 1, 0);
			free(dir);
			free(startdir);
			free(remote_path);
			free(conn);
			return (err);
		}
		free(dir);
	}

	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(infile, NULL, _IOLBF, 0);

	interactive = !batchmode && isatty(STDIN_FILENO);
	err = 0;
	for (;;) {
		struct sigaction sa;

		interrupted = 0;
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = interactive ? read_interrupt : killchild;
		if (sigaction(SIGINT, &sa, NULL) == -1) {
			debug3("sigaction(%s): %s", strsignal(SIGINT),
			    strerror(errno));
			break;
		}
		if (el == NULL) {
			if (interactive) {
				printf("sftp> ");
				fflush(stdout);
			}
			if (fgets(cmd, sizeof(cmd), infile) == NULL) {
				if (interactive)
					printf("\n");
				if (interrupted)
					continue;
				break;
			}
		} else {
#ifdef USE_LIBEDIT
			const char *line;
			int count = 0;

			if ((line = el_gets(el, &count)) == NULL ||
			    count <= 0) {
				printf("\n");
				if (interrupted)
					continue;
				break;
			}
			history(hl, &hev, H_ENTER, line);
			if (strlcpy(cmd, line, sizeof(cmd)) >= sizeof(cmd)) {
				fprintf(stderr, "Error: input line too long\n");
				continue;
			}
#endif /* USE_LIBEDIT */
		}

		cmd[strcspn(cmd, "\n")] = '\0';

		/* Handle user interrupts gracefully during commands */
		interrupted = 0;
		ssh_signal(SIGINT, cmd_interrupt);

		err = parse_dispatch_command(conn, cmd, &remote_path,
		    startdir, batchmode, !interactive && el == NULL);
		if (err != 0)
			break;
	}
	ssh_signal(SIGCHLD, SIG_DFL);
	free(remote_path);
	free(startdir);
	free(conn);

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
		ssh_signal(SIGINT, SIG_IGN);
		ssh_signal(SIGTERM, SIG_DFL);
		execvp(path, args);
		fprintf(stderr, "exec: %s: %s\n", path, strerror(errno));
		_exit(1);
	}

	ssh_signal(SIGTERM, killchild);
	ssh_signal(SIGINT, killchild);
	ssh_signal(SIGHUP, killchild);
	ssh_signal(SIGTSTP, suspchild);
	ssh_signal(SIGTTIN, suspchild);
	ssh_signal(SIGTTOU, suspchild);
	ssh_signal(SIGCHLD, sigchld_handler);
	close(c_in);
	close(c_out);
}

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-46AaCfNpqrv] [-B buffer_size] [-b batchfile] [-c cipher]\n"
	    "          [-D sftp_server_command] [-F ssh_config] [-i identity_file]\n"
	    "          [-J destination] [-l limit] [-o ssh_option] [-P port]\n"
	    "          [-R num_requests] [-S program] [-s subsystem | sftp_server]\n"
	    "          [-X sftp_option] destination\n",
	    __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int r, in, out, ch, err, tmp, port = -1, noisy = 0;
	char *host = NULL, *user, *cp, **cpp, *file2 = NULL;
	int debug_level = 0;
	char *file1 = NULL, *sftp_server = NULL;
	char *ssh_program = _PATH_SSH_PROGRAM, *sftp_direct = NULL;
	const char *errstr;
	LogLevel ll = SYSLOG_LEVEL_INFO;
	arglist args;
	extern int optind;
	extern char *optarg;
	struct sftp_conn *conn;
	size_t copy_buffer_len = 0;
	size_t num_requests = 0;
	long long llv, limit_kbps = 0;

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
	sanitise_stdfd();
	msetlocale();

	__progname = ssh_get_progname(argv[0]);
	memset(&args, '\0', sizeof(args));
	args.list = NULL;
	addargs(&args, "%s", ssh_program);
	addargs(&args, "-oForwardX11 no");
	addargs(&args, "-oPermitLocalCommand no");
	addargs(&args, "-oClearAllForwardings yes");

	ll = SYSLOG_LEVEL_INFO;
	infile = stdin;

	while ((ch = getopt(argc, argv,
	    "1246AafhNpqrvCc:D:i:l:o:s:S:b:B:F:J:P:R:X:")) != -1) {
		switch (ch) {
		/* Passed through to ssh(1) */
		case 'A':
		case '4':
		case '6':
		case 'C':
			addargs(&args, "-%c", ch);
			break;
		/* Passed through to ssh(1) with argument */
		case 'F':
		case 'J':
		case 'c':
		case 'i':
		case 'o':
			addargs(&args, "-%c", ch);
			addargs(&args, "%s", optarg);
			break;
		case 'q':
			ll = SYSLOG_LEVEL_ERROR;
			quiet = 1;
			showprogress = 0;
			addargs(&args, "-%c", ch);
			break;
		case 'P':
			port = a2port(optarg);
			if (port <= 0)
				fatal("Bad port \"%s\"\n", optarg);
			break;
		case 'v':
			if (debug_level < 3) {
				addargs(&args, "-v");
				ll = SYSLOG_LEVEL_DEBUG1 + debug_level;
			}
			debug_level++;
			break;
		case '1':
			fatal("SSH protocol v.1 is no longer supported");
			break;
		case '2':
			/* accept silently */
			break;
		case 'a':
			global_aflag = 1;
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
			quiet = batchmode = 1;
			addargs(&args, "-obatchmode yes");
			break;
		case 'f':
			global_fflag = 1;
			break;
		case 'N':
			noisy = 1; /* Used to clear quiet mode after getopt */
			break;
		case 'p':
			global_pflag = 1;
			break;
		case 'D':
			sftp_direct = optarg;
			break;
		case 'l':
			limit_kbps = strtonum(optarg, 1, 100 * 1024 * 1024,
			    &errstr);
			if (errstr != NULL)
				usage();
			limit_kbps *= 1024; /* kbps */
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
		case 'X':
			/* Please keep in sync with ssh.c -X */
			if (strncmp(optarg, "buffer=", 7) == 0) {
				r = scan_scaled(optarg + 7, &llv);
				if (r == 0 && (llv <= 0 || llv > 256 * 1024)) {
					r = -1;
					errno = EINVAL;
				}
				if (r == -1) {
					fatal("Invalid buffer size \"%s\": %s",
					     optarg + 7, strerror(errno));
				}
				copy_buffer_len = (size_t)llv;
			} else if (strncmp(optarg, "nrequests=", 10) == 0) {
				llv = strtonum(optarg + 10, 1, 256 * 1024,
				    &errstr);
				if (errstr != NULL) {
					fatal("Invalid number of requests "
					    "\"%s\": %s", optarg + 10, errstr);
				}
				num_requests = (size_t)llv;
			} else {
				fatal("Invalid -X option");
			}
			break;
		case 'h':
		default:
			usage();
		}
	}

	/* Do this last because we want the user to be able to override it */
	addargs(&args, "-oForwardAgent no");

	if (!isatty(STDERR_FILENO))
		showprogress = 0;

	if (noisy)
		quiet = 0;

	log_init(argv[0], ll, SYSLOG_FACILITY_USER, 1);

	if (sftp_direct == NULL) {
		if (optind == argc || argc > (optind + 2))
			usage();
		argv += optind;

		switch (parse_uri("sftp", *argv, &user, &host, &tmp, &file1)) {
		case -1:
			usage();
			break;
		case 0:
			if (tmp != -1)
				port = tmp;
			break;
		default:
			/* Try with user, host and path. */
			if (parse_user_host_path(*argv, &user, &host,
			    &file1) == 0)
				break;
			/* Try with user and host. */
			if (parse_user_host_port(*argv, &user, &host, NULL)
			    == 0)
				break;
			/* Treat as a plain hostname. */
			host = xstrdup(*argv);
			host = cleanhostname(host);
			break;
		}
		file2 = *(argv + 1);

		if (!*host) {
			fprintf(stderr, "Missing hostname\n");
			usage();
		}

		if (port != -1)
			addargs(&args, "-oPort %d", port);
		if (user != NULL) {
			addargs(&args, "-l");
			addargs(&args, "%s", user);
		}

		/* no subsystem if the server-spec contains a '/' */
		if (sftp_server == NULL || strchr(sftp_server, '/') == NULL)
			addargs(&args, "-s");

		addargs(&args, "--");
		addargs(&args, "%s", host);
		addargs(&args, "%s", (sftp_server != NULL ?
		    sftp_server : "sftp"));

		connect_to_server(ssh_program, args.list, &in, &out);
	} else {
		if ((r = argv_split(sftp_direct, &tmp, &cpp, 1)) != 0)
			fatal_r(r, "Parse -D arguments");
		if (cpp[0] == 0)
			fatal("No sftp server specified via -D");
		connect_to_server(cpp[0], cpp, &in, &out);
		argv_free(cpp, tmp);
	}
	freeargs(&args);

	conn = sftp_init(in, out, copy_buffer_len, num_requests, limit_kbps);
	if (conn == NULL)
		fatal("Couldn't initialise connection to server");

	if (!quiet) {
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

	while (waitpid(sshpid, NULL, 0) == -1 && sshpid > 1)
		if (errno != EINTR)
			fatal("Couldn't wait for ssh process: %s",
			    strerror(errno));

	exit(err == 0 ? 0 : 1);
}
