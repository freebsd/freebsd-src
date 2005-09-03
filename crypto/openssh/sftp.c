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

RCSID("$OpenBSD: sftp.c,v 1.66 2005/08/08 13:22:48 jaredy Exp $");

#ifdef USE_LIBEDIT
#include <histedit.h>
#else
typedef void EditLine;
#endif

#include "buffer.h"
#include "xmalloc.h"
#include "log.h"
#include "pathnames.h"
#include "misc.h"

#include "sftp.h"
#include "sftp-common.h"
#include "sftp-client.h"

/* File to read commands from */
FILE* infile;

/* Are we in batchfile mode? */
int batchmode = 0;

/* Size of buffer used when copying files */
size_t copy_buffer_len = 32768;

/* Number of concurrent outstanding requests */
size_t num_requests = 16;

/* PID of ssh transport process */
static pid_t sshpid = -1;

/* This is set to 0 if the progressmeter is not desired. */
int showprogress = 1;

/* SIGINT received during command processing */
volatile sig_atomic_t interrupted = 0;

/* I wish qsort() took a separate ctx for the comparison function...*/
int sort_flag;

int remote_glob(struct sftp_conn *, const char *, int,
    int (*)(const char *, int), glob_t *); /* proto for sftp-glob.c */

extern char *__progname;

/* Separators for interactive commands */
#define WHITESPACE " \t\r\n"

/* ls flags */
#define LS_LONG_VIEW	0x01	/* Full view ala ls -l */
#define LS_SHORT_VIEW	0x02	/* Single row view ala ls -1 */
#define LS_NUMERIC_VIEW	0x04	/* Long view with numeric uid/gid */
#define LS_NAME_SORT	0x08	/* Sort by name (default) */
#define LS_TIME_SORT	0x10	/* Sort by mtime */
#define LS_SIZE_SORT	0x20	/* Sort by file size */
#define LS_REVERSE_SORT	0x40	/* Reverse sort order */
#define LS_SHOW_ALL	0x80	/* Don't skip filenames starting with '.' */

#define VIEW_FLAGS	(LS_LONG_VIEW|LS_SHORT_VIEW|LS_NUMERIC_VIEW)
#define SORT_FLAGS	(LS_NAME_SORT|LS_TIME_SORT|LS_SIZE_SORT)

/* Commands for interactive mode */
#define I_CHDIR		1
#define I_CHGRP		2
#define I_CHMOD		3
#define I_CHOWN		4
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
};

static const struct CMD cmds[] = {
	{ "bye",	I_QUIT },
	{ "cd",		I_CHDIR },
	{ "chdir",	I_CHDIR },
	{ "chgrp",	I_CHGRP },
	{ "chmod",	I_CHMOD },
	{ "chown",	I_CHOWN },
	{ "dir",	I_LS },
	{ "exit",	I_QUIT },
	{ "get",	I_GET },
	{ "mget",	I_GET },
	{ "help",	I_HELP },
	{ "lcd",	I_LCHDIR },
	{ "lchdir",	I_LCHDIR },
	{ "lls",	I_LLS },
	{ "lmkdir",	I_LMKDIR },
	{ "ln",		I_SYMLINK },
	{ "lpwd",	I_LPWD },
	{ "ls",		I_LS },
	{ "lumask",	I_LUMASK },
	{ "mkdir",	I_MKDIR },
	{ "progress",	I_PROGRESS },
	{ "put",	I_PUT },
	{ "mput",	I_PUT },
	{ "pwd",	I_PWD },
	{ "quit",	I_QUIT },
	{ "rename",	I_RENAME },
	{ "rm",		I_RM },
	{ "rmdir",	I_RMDIR },
	{ "symlink",	I_SYMLINK },
	{ "version",	I_VERSION },
	{ "!",		I_SHELL },
	{ "?",		I_HELP },
	{ NULL,			-1}
};

int interactive_loop(int fd_in, int fd_out, char *file1, char *file2);

static void
killchild(int signo)
{
	if (sshpid > 1) {
		kill(sshpid, SIGTERM);
		waitpid(sshpid, NULL, 0);
	}

	_exit(1);
}

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
	printf("Available commands:\n");
	printf("cd path                       Change remote directory to 'path'\n");
	printf("lcd path                      Change local directory to 'path'\n");
	printf("chgrp grp path                Change group of file 'path' to 'grp'\n");
	printf("chmod mode path               Change permissions of file 'path' to 'mode'\n");
	printf("chown own path                Change owner of file 'path' to 'own'\n");
	printf("help                          Display this help text\n");
	printf("get remote-path [local-path]  Download file\n");
	printf("lls [ls-options [path]]       Display local directory listing\n");
	printf("ln oldpath newpath            Symlink remote file\n");
	printf("lmkdir path                   Create local directory\n");
	printf("lpwd                          Print local working directory\n");
	printf("ls [path]                     Display remote directory listing\n");
	printf("lumask umask                  Set local umask to 'umask'\n");
	printf("mkdir path                    Create remote directory\n");
	printf("progress                      Toggle display of progress meter\n");
	printf("put local-path [remote-path]  Upload file\n");
	printf("pwd                           Display remote working directory\n");
	printf("exit                          Quit sftp\n");
	printf("quit                          Quit sftp\n");
	printf("rename oldpath newpath        Rename remote file\n");
	printf("rmdir path                    Remove remote directory\n");
	printf("rm path                       Delete remote file\n");
	printf("symlink oldpath newpath       Symlink remote file\n");
	printf("version                       Show SFTP version\n");
	printf("!command                      Execute 'command' in local shell\n");
	printf("!                             Escape to local shell\n");
	printf("?                             Synonym for help\n");
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
		error("Shell exited abormally");
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
path_append(char *p1, char *p2)
{
	char *ret;
	int len = strlen(p1) + strlen(p2) + 2;

	ret = xmalloc(len);
	strlcpy(ret, p1, len);
	if (p1[strlen(p1) - 1] != '/')
		strlcat(ret, "/", len);
	strlcat(ret, p2, len);

	return(ret);
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
infer_path(const char *p, char **ifp)
{
	char *cp;

	cp = strrchr(p, '/');
	if (cp == NULL) {
		*ifp = xstrdup(p);
		return(0);
	}

	if (!cp[1]) {
		error("Invalid path");
		return(-1);
	}

	*ifp = xstrdup(cp + 1);
	return(0);
}

static int
parse_getput_flags(const char **cpp, int *pflag)
{
	const char *cp = *cpp;

	/* Check for flags */
	if (cp[0] == '-' && cp[1] && strchr(WHITESPACE, cp[2])) {
		switch (cp[1]) {
		case 'p':
		case 'P':
			*pflag = 1;
			break;
		default:
			error("Invalid flag -%c", cp[1]);
			return(-1);
		}
		cp += 2;
		*cpp = cp + strspn(cp, WHITESPACE);
	}

	return(0);
}

static int
parse_ls_flags(const char **cpp, int *lflag)
{
	const char *cp = *cpp;

	/* Defaults */
	*lflag = LS_NAME_SORT;

	/* Check for flags */
	if (cp++[0] == '-') {
		for (; strchr(WHITESPACE, *cp) == NULL; cp++) {
			switch (*cp) {
			case 'l':
				*lflag &= ~VIEW_FLAGS;
				*lflag |= LS_LONG_VIEW;
				break;
			case '1':
				*lflag &= ~VIEW_FLAGS;
				*lflag |= LS_SHORT_VIEW;
				break;
			case 'n':
				*lflag &= ~VIEW_FLAGS;
				*lflag |= LS_NUMERIC_VIEW|LS_LONG_VIEW;
				break;
			case 'S':
				*lflag &= ~SORT_FLAGS;
				*lflag |= LS_SIZE_SORT;
				break;
			case 't':
				*lflag &= ~SORT_FLAGS;
				*lflag |= LS_TIME_SORT;
				break;
			case 'r':
				*lflag |= LS_REVERSE_SORT;
				break;
			case 'f':
				*lflag &= ~SORT_FLAGS;
				break;
			case 'a':
				*lflag |= LS_SHOW_ALL;
				break;
			default:
				error("Invalid flag -%c", *cp);
				return(-1);
			}
		}
		*cpp = cp + strspn(cp, WHITESPACE);
	}

	return(0);
}

static int
get_pathname(const char **cpp, char **path)
{
	const char *cp = *cpp, *end;
	char quot;
	u_int i, j;

	cp += strspn(cp, WHITESPACE);
	if (!*cp) {
		*cpp = cp;
		*path = NULL;
		return (0);
	}

	*path = xmalloc(strlen(cp) + 1);

	/* Check for quoted filenames */
	if (*cp == '\"' || *cp == '\'') {
		quot = *cp++;

		/* Search for terminating quote, unescape some chars */
		for (i = j = 0; i <= strlen(cp); i++) {
			if (cp[i] == quot) {	/* Found quote */
				i++;
				(*path)[j] = '\0';
				break;
			}
			if (cp[i] == '\0') {	/* End of string */
				error("Unterminated quote");
				goto fail;
			}
			if (cp[i] == '\\') {	/* Escaped characters */
				i++;
				if (cp[i] != '\'' && cp[i] != '\"' &&
				    cp[i] != '\\') {
					error("Bad escaped character '\\%c'",
					    cp[i]);
					goto fail;
				}
			}
			(*path)[j++] = cp[i];
		}

		if (j == 0) {
			error("Empty quotes");
			goto fail;
		}
		*cpp = cp + i + strspn(cp + i, WHITESPACE);
	} else {
		/* Read to end of filename */
		end = strpbrk(cp, WHITESPACE);
		if (end == NULL)
			end = strchr(cp, '\0');
		*cpp = end + strspn(end, WHITESPACE);

		memcpy(*path, cp, end - cp);
		(*path)[end - cp] = '\0';
	}
	return (0);

 fail:
	xfree(*path);
	*path = NULL;
	return (-1);
}

static int
is_dir(char *path)
{
	struct stat sb;

	/* XXX: report errors? */
	if (stat(path, &sb) == -1)
		return(0);

	return(sb.st_mode & S_IFDIR);
}

static int
is_reg(char *path)
{
	struct stat sb;

	if (stat(path, &sb) == -1)
		fatal("stat %s: %s", path, strerror(errno));

	return(S_ISREG(sb.st_mode));
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
	return(a->perm & S_IFDIR);
}

static int
process_get(struct sftp_conn *conn, char *src, char *dst, char *pwd, int pflag)
{
	char *abs_src = NULL;
	char *abs_dst = NULL;
	char *tmp;
	glob_t g;
	int err = 0;
	int i;

	abs_src = xstrdup(src);
	abs_src = make_absolute(abs_src, pwd);

	memset(&g, 0, sizeof(g));
	debug3("Looking up %s", abs_src);
	if (remote_glob(conn, abs_src, 0, NULL, &g)) {
		error("File \"%s\" not found.", abs_src);
		err = -1;
		goto out;
	}

	/* If multiple matches, dst must be a directory or unspecified */
	if (g.gl_matchc > 1 && dst && !is_dir(dst)) {
		error("Multiple files match, but \"%s\" is not a directory",
		    dst);
		err = -1;
		goto out;
	}

	for (i = 0; g.gl_pathv[i] && !interrupted; i++) {
		if (infer_path(g.gl_pathv[i], &tmp)) {
			err = -1;
			goto out;
		}

		if (g.gl_matchc == 1 && dst) {
			/* If directory specified, append filename */
			if (is_dir(dst)) {
				if (infer_path(g.gl_pathv[0], &tmp)) {
					err = 1;
					goto out;
				}
				abs_dst = path_append(dst, tmp);
				xfree(tmp);
			} else
				abs_dst = xstrdup(dst);
		} else if (dst) {
			abs_dst = path_append(dst, tmp);
			xfree(tmp);
		} else
			abs_dst = tmp;

		printf("Fetching %s to %s\n", g.gl_pathv[i], abs_dst);
		if (do_download(conn, g.gl_pathv[i], abs_dst, pflag) == -1)
			err = -1;
		xfree(abs_dst);
		abs_dst = NULL;
	}

out:
	xfree(abs_src);
	if (abs_dst)
		xfree(abs_dst);
	globfree(&g);
	return(err);
}

static int
process_put(struct sftp_conn *conn, char *src, char *dst, char *pwd, int pflag)
{
	char *tmp_dst = NULL;
	char *abs_dst = NULL;
	char *tmp;
	glob_t g;
	int err = 0;
	int i;

	if (dst) {
		tmp_dst = xstrdup(dst);
		tmp_dst = make_absolute(tmp_dst, pwd);
	}

	memset(&g, 0, sizeof(g));
	debug3("Looking up %s", src);
	if (glob(src, 0, NULL, &g)) {
		error("File \"%s\" not found.", src);
		err = -1;
		goto out;
	}

	/* If multiple matches, dst may be directory or unspecified */
	if (g.gl_matchc > 1 && tmp_dst && !remote_is_dir(conn, tmp_dst)) {
		error("Multiple files match, but \"%s\" is not a directory",
		    tmp_dst);
		err = -1;
		goto out;
	}

	for (i = 0; g.gl_pathv[i] && !interrupted; i++) {
		if (!is_reg(g.gl_pathv[i])) {
			error("skipping non-regular file %s",
			    g.gl_pathv[i]);
			continue;
		}
		if (infer_path(g.gl_pathv[i], &tmp)) {
			err = -1;
			goto out;
		}

		if (g.gl_matchc == 1 && tmp_dst) {
			/* If directory specified, append filename */
			if (remote_is_dir(conn, tmp_dst)) {
				if (infer_path(g.gl_pathv[0], &tmp)) {
					err = 1;
					goto out;
				}
				abs_dst = path_append(tmp_dst, tmp);
				xfree(tmp);
			} else
				abs_dst = xstrdup(tmp_dst);

		} else if (tmp_dst) {
			abs_dst = path_append(tmp_dst, tmp);
			xfree(tmp);
		} else
			abs_dst = make_absolute(tmp, pwd);

		printf("Uploading %s to %s\n", g.gl_pathv[i], abs_dst);
		if (do_upload(conn, g.gl_pathv[i], abs_dst, pflag) == -1)
			err = -1;
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
			if (lflag & LS_NUMERIC_VIEW) {
				char *lname;
				struct stat sb;

				memset(&sb, 0, sizeof(sb));
				attrib_to_stat(&d[n]->a, &sb);
				lname = ls_file(fname, &sb, 1);
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
			lname = ls_file(fname, &sb, 1);
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
parse_args(const char **cpp, int *pflag, int *lflag, int *iflag,
    unsigned long *n_arg, char **path1, char **path2)
{
	const char *cmd, *cp = *cpp;
	char *cp2;
	int base = 0;
	long l;
	int i, cmdnum;

	/* Skip leading whitespace */
	cp = cp + strspn(cp, WHITESPACE);

	/* Ignore blank lines and lines which begin with comment '#' char */
	if (*cp == '\0' || *cp == '#')
		return (0);

	/* Check for leading '-' (disable error processing) */
	*iflag = 0;
	if (*cp == '-') {
		*iflag = 1;
		cp++;
	}

	/* Figure out which command we have */
	for (i = 0; cmds[i].c; i++) {
		int cmdlen = strlen(cmds[i].c);

		/* Check for command followed by whitespace */
		if (!strncasecmp(cp, cmds[i].c, cmdlen) &&
		    strchr(WHITESPACE, cp[cmdlen])) {
			cp += cmdlen;
			cp = cp + strspn(cp, WHITESPACE);
			break;
		}
	}
	cmdnum = cmds[i].n;
	cmd = cmds[i].c;

	/* Special case */
	if (*cp == '!') {
		cp++;
		cmdnum = I_SHELL;
	} else if (cmdnum == -1) {
		error("Invalid command.");
		return (-1);
	}

	/* Get arguments and parse flags */
	*lflag = *pflag = *n_arg = 0;
	*path1 = *path2 = NULL;
	switch (cmdnum) {
	case I_GET:
	case I_PUT:
		if (parse_getput_flags(&cp, pflag))
			return(-1);
		/* Get first pathname (mandatory) */
		if (get_pathname(&cp, path1))
			return(-1);
		if (*path1 == NULL) {
			error("You must specify at least one path after a "
			    "%s command.", cmd);
			return(-1);
		}
		/* Try to get second pathname (optional) */
		if (get_pathname(&cp, path2))
			return(-1);
		break;
	case I_RENAME:
	case I_SYMLINK:
		if (get_pathname(&cp, path1))
			return(-1);
		if (get_pathname(&cp, path2))
			return(-1);
		if (!*path1 || !*path2) {
			error("You must specify two paths after a %s "
			    "command.", cmd);
			return(-1);
		}
		break;
	case I_RM:
	case I_MKDIR:
	case I_RMDIR:
	case I_CHDIR:
	case I_LCHDIR:
	case I_LMKDIR:
		/* Get pathname (mandatory) */
		if (get_pathname(&cp, path1))
			return(-1);
		if (*path1 == NULL) {
			error("You must specify a path after a %s command.",
			    cmd);
			return(-1);
		}
		break;
	case I_LS:
		if (parse_ls_flags(&cp, lflag))
			return(-1);
		/* Path is optional */
		if (get_pathname(&cp, path1))
			return(-1);
		break;
	case I_LLS:
	case I_SHELL:
		/* Uses the rest of the line */
		break;
	case I_LUMASK:
		base = 8;
	case I_CHMOD:
		base = 8;
	case I_CHOWN:
	case I_CHGRP:
		/* Get numeric arg (mandatory) */
		l = strtol(cp, &cp2, base);
		if (cp2 == cp || ((l == LONG_MIN || l == LONG_MAX) &&
		    errno == ERANGE) || l < 0) {
			error("You must supply a numeric argument "
			    "to the %s command.", cmd);
			return(-1);
		}
		cp = cp2;
		*n_arg = l;
		if (cmdnum == I_LUMASK && strchr(WHITESPACE, *cp))
			break;
		if (cmdnum == I_LUMASK || !strchr(WHITESPACE, *cp)) {
			error("You must supply a numeric argument "
			    "to the %s command.", cmd);
			return(-1);
		}
		cp += strspn(cp, WHITESPACE);

		/* Get pathname (mandatory) */
		if (get_pathname(&cp, path1))
			return(-1);
		if (*path1 == NULL) {
			error("You must specify a path after a %s command.",
			    cmd);
			return(-1);
		}
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
	int pflag, lflag, iflag, cmdnum, i;
	unsigned long n_arg;
	Attrib a, *aa;
	char path_buf[MAXPATHLEN];
	int err = 0;
	glob_t g;

	path1 = path2 = NULL;
	cmdnum = parse_args(&cmd, &pflag, &lflag, &iflag, &n_arg,
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
		err = process_get(conn, path1, path2, *pwd, pflag);
		break;
	case I_PUT:
		err = process_put(conn, path1, path2, *pwd, pflag);
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
		err = do_mkdir(conn, path1, &a);
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
				if (err != 0 && err_abort)
					break;
				else
					continue;
			}
			if (!(aa->flags & SSH2_FILEXFER_ATTR_UIDGID)) {
				error("Can't get current ownership of "
				    "remote file \"%s\"", g.gl_pathv[i]);
				if (err != 0 && err_abort)
					break;
				else
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
#endif

int
interactive_loop(int fd_in, int fd_out, char *file1, char *file2)
{
	char *pwd;
	char *dir = NULL;
	char cmd[2048];
	struct sftp_conn *conn;
	int err, interactive;
	EditLine *el = NULL;
#ifdef USE_LIBEDIT
	History *hl = NULL;
	HistEvent hev;
	extern char *__progname;

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
	}
#endif /* USE_LIBEDIT */

	conn = do_init(fd_in, fd_out, copy_buffer_len, num_requests);
	if (conn == NULL)
		fatal("Couldn't initialise connection to server");

	pwd = do_realpath(conn, ".");
	if (pwd == NULL)
		fatal("Need cwd");

	if (file1 != NULL) {
		dir = xstrdup(file1);
		dir = make_absolute(dir, pwd);

		if (remote_is_dir(conn, dir) && file2 == NULL) {
			printf("Changing to: %s\n", dir);
			snprintf(cmd, sizeof cmd, "cd \"%s\"", dir);
			if (parse_dispatch_command(conn, cmd, &pwd, 1) != 0) {
				xfree(dir);
				xfree(pwd);
				return (-1);
			}
		} else {
			if (file2 == NULL)
				snprintf(cmd, sizeof cmd, "get %s", dir);
			else
				snprintf(cmd, sizeof cmd, "get %s %s", dir,
				    file2);

			err = parse_dispatch_command(conn, cmd, &pwd, 1);
			xfree(dir);
			xfree(pwd);
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

			if ((line = el_gets(el, &count)) == NULL || count <= 0) {
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

		err = parse_dispatch_command(conn, cmd, &pwd, batchmode);
		if (err != 0)
			break;
	}
	xfree(pwd);

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
		 * kill it too
		 */
		signal(SIGINT, SIG_IGN);
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
	    "usage: %s [-1Cv] [-B buffer_size] [-b batchfile] [-F ssh_config]\n"
	    "            [-o ssh_option] [-P sftp_server_path] [-R num_requests]\n"
	    "            [-S program] [-s subsystem | sftp_server] host\n"
	    "       %s [[user@]host[:file [file]]]\n"
	    "       %s [[user@]host[:dir[/]]]\n"
	    "       %s -b batchfile [user@]host\n", __progname, __progname, __progname, __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int in, out, ch, err;
	char *host, *userhost, *cp, *file2 = NULL;
	int debug_level = 0, sshver = 2;
	char *file1 = NULL, *sftp_server = NULL;
	char *ssh_program = _PATH_SSH_PROGRAM, *sftp_direct = NULL;
	LogLevel ll = SYSLOG_LEVEL_INFO;
	arglist args;
	extern int optind;
	extern char *optarg;

	__progname = ssh_get_progname(argv[0]);
	args.list = NULL;
	addargs(&args, "ssh");		/* overwritten with ssh_program */
	addargs(&args, "-oForwardX11 no");
	addargs(&args, "-oForwardAgent no");
	addargs(&args, "-oClearAllForwardings yes");

	ll = SYSLOG_LEVEL_INFO;
	infile = stdin;

	while ((ch = getopt(argc, argv, "1hvCo:s:S:b:B:F:P:R:")) != -1) {
		switch (ch) {
		case 'C':
			addargs(&args, "-C");
			break;
		case 'v':
			if (debug_level < 3) {
				addargs(&args, "-v");
				ll = SYSLOG_LEVEL_DEBUG1 + debug_level;
			}
			debug_level++;
			break;
		case 'F':
		case 'o':
			addargs(&args, "-%c%s", ch, optarg);
			break;
		case '1':
			sshver = 1;
			if (sftp_server == NULL)
				sftp_server = _PATH_SFTP_SERVER;
			break;
		case 's':
			sftp_server = optarg;
			break;
		case 'S':
			ssh_program = optarg;
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
		case 'P':
			sftp_direct = optarg;
			break;
		case 'B':
			copy_buffer_len = strtol(optarg, &cp, 10);
			if (copy_buffer_len == 0 || *cp != '\0')
				fatal("Invalid buffer size \"%s\"", optarg);
			break;
		case 'R':
			num_requests = strtol(optarg, &cp, 10);
			if (num_requests == 0 || *cp != '\0')
				fatal("Invalid number of requests \"%s\"",
				    optarg);
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
			addargs(&args, "-l%s",userhost);
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

		addargs(&args, "%s", host);
		addargs(&args, "%s", (sftp_server != NULL ?
		    sftp_server : "sftp"));
		args.list[0] = ssh_program;

		if (!batchmode)
			fprintf(stderr, "Connecting to %s...\n", host);
		connect_to_server(ssh_program, args.list, &in, &out);
	} else {
		args.list = NULL;
		addargs(&args, "sftp-server");

		if (!batchmode)
			fprintf(stderr, "Attaching to %s...\n", sftp_direct);
		connect_to_server(sftp_direct, args.list, &in, &out);
	}

	err = interactive_loop(in, out, file1, file2);

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
