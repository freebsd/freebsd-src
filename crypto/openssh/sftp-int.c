/*
 * Copyright (c) 2001 Damien Miller.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* XXX: globbed ls */
/* XXX: recursive operations */

#include "includes.h"
RCSID("$OpenBSD: sftp-int.c,v 1.36 2001/04/15 08:43:46 markus Exp $");

#include <glob.h>

#include "buffer.h"
#include "xmalloc.h"
#include "log.h"
#include "pathnames.h"

#include "sftp.h"
#include "sftp-common.h"
#include "sftp-glob.h"
#include "sftp-client.h"
#include "sftp-int.h"

/* File to read commands from */
extern FILE *infile;

/* Version of server we are speaking to */
int version;

/* Seperators for interactive commands */
#define WHITESPACE " \t\r\n"

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

struct CMD {
	const char *c;
	const int n;
};

const struct CMD cmds[] = {
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

void
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

void
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
			execl(shell, shell, "-c", args, NULL);
		} else {
			debug3("Executing %s", shell);
			execl(shell, shell, NULL);
		}
		fprintf(stderr, "Couldn't execute \"%s\": %s\n", shell,
		    strerror(errno));
		_exit(1);
	}
	if (waitpid(pid, &status, 0) == -1)
		fatal("Couldn't wait for child: %s", strerror(errno));
	if (!WIFEXITED(status))
		error("Shell exited abormally");
	else if (WEXITSTATUS(status))
		error("Shell exited with status %d", WEXITSTATUS(status));
}

void
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

char *
path_append(char *p1, char *p2)
{
	char *ret;
	int len = strlen(p1) + strlen(p2) + 2;

	ret = xmalloc(len);
	strlcpy(ret, p1, len);
	strlcat(ret, "/", len);
	strlcat(ret, p2, len);

	return(ret);
}

char *
make_absolute(char *p, char *pwd)
{
	char *abs;

	/* Derelativise */
	if (p && p[0] != '/') {
		abs = path_append(pwd, p);
		xfree(p);
		return(abs);
	} else
		return(p);
}

int
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

int
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

int
get_pathname(const char **cpp, char **path)
{
	const char *cp = *cpp, *end;
	char quot;
	int i;

	cp += strspn(cp, WHITESPACE);
	if (!*cp) {
		*cpp = cp;
		*path = NULL;
		return (0);
	}

	/* Check for quoted filenames */
	if (*cp == '\"' || *cp == '\'') {
		quot = *cp++;

		end = strchr(cp, quot);
		if (end == NULL) {
			error("Unterminated quote");
			goto fail;
		}
		if (cp == end) {
			error("Empty quotes");
			goto fail;
		}
		*cpp = end + 1 + strspn(end + 1, WHITESPACE);
	} else {
		/* Read to end of filename */
		end = strpbrk(cp, WHITESPACE);
		if (end == NULL)
			end = strchr(cp, '\0');
		*cpp = end + strspn(end, WHITESPACE);
	}

	i = end - cp;

	*path = xmalloc(i + 1);
	memcpy(*path, cp, i);
	(*path)[i] = '\0';
	return(0);

 fail:
	*path = NULL;
	return (-1);
}

int
is_dir(char *path)
{
	struct stat sb;

	/* XXX: report errors? */
	if (stat(path, &sb) == -1)
		return(0);

	return(sb.st_mode & S_IFDIR);
}

int
remote_is_dir(int in, int out, char *path)
{
	Attrib *a;

	/* XXX: report errors? */
	if ((a = do_stat(in, out, path, 1)) == NULL)
		return(0);
	if (!(a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS))
		return(0);
	return(a->perm & S_IFDIR);
}

int
process_get(int in, int out, char *src, char *dst, char *pwd, int pflag)
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
	if (remote_glob(in, out, abs_src, 0, NULL, &g)) {
		error("File \"%s\" not found.", abs_src);
		err = -1;
		goto out;
	}

	/* Only one match, dst may be file, directory or unspecified */
	if (g.gl_pathv[0] && g.gl_matchc == 1) {
		if (dst) {
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
		} else if (infer_path(g.gl_pathv[0], &abs_dst)) {
			err = -1;
			goto out;
		}
		printf("Fetching %s to %s\n", g.gl_pathv[0], abs_dst);
		err = do_download(in, out, g.gl_pathv[0], abs_dst, pflag);
		goto out;
	}

	/* Multiple matches, dst may be directory or unspecified */
	if (dst && !is_dir(dst)) {
		error("Multiple files match, but \"%s\" is not a directory",
		    dst);
		err = -1;
		goto out;
	}

	for(i = 0; g.gl_pathv[i]; i++) {
		if (infer_path(g.gl_pathv[i], &tmp)) {
			err = -1;
			goto out;
		}
		if (dst) {
			abs_dst = path_append(dst, tmp);
			xfree(tmp);
		} else
			abs_dst = tmp;

		printf("Fetching %s to %s\n", g.gl_pathv[i], abs_dst);
		if (do_download(in, out, g.gl_pathv[i], abs_dst, pflag) == -1)
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

int
process_put(int in, int out, char *src, char *dst, char *pwd, int pflag)
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

	/* Only one match, dst may be file, directory or unspecified */
	if (g.gl_pathv[0] && g.gl_matchc == 1) {
		if (tmp_dst) {
			/* If directory specified, append filename */
			if (remote_is_dir(in, out, tmp_dst)) {
				if (infer_path(g.gl_pathv[0], &tmp)) {
					err = 1;
					goto out;
				}
				abs_dst = path_append(tmp_dst, tmp);
				xfree(tmp);
			} else
				abs_dst = xstrdup(tmp_dst);
		} else {
			if (infer_path(g.gl_pathv[0], &abs_dst)) {
				err = -1;
				goto out;
			}
			abs_dst = make_absolute(abs_dst, pwd);
		}
		printf("Uploading %s to %s\n", g.gl_pathv[0], abs_dst);
		err = do_upload(in, out, g.gl_pathv[0], abs_dst, pflag);
		goto out;
	}

	/* Multiple matches, dst may be directory or unspecified */
	if (tmp_dst && !remote_is_dir(in, out, tmp_dst)) {
		error("Multiple files match, but \"%s\" is not a directory",
		    tmp_dst);
		err = -1;
		goto out;
	}

	for(i = 0; g.gl_pathv[i]; i++) {
		if (infer_path(g.gl_pathv[i], &tmp)) {
			err = -1;
			goto out;
		}
		if (tmp_dst) {
			abs_dst = path_append(tmp_dst, tmp);
			xfree(tmp);
		} else
			abs_dst = make_absolute(tmp, pwd);

		printf("Uploading %s to %s\n", g.gl_pathv[i], abs_dst);
		if (do_upload(in, out, g.gl_pathv[i], abs_dst, pflag) == -1)
			err = -1;
	}

out:
	if (abs_dst)
		xfree(abs_dst);
	if (tmp_dst)
		xfree(tmp_dst);
	return(err);
}

int
parse_args(const char **cpp, int *pflag, unsigned long *n_arg,
    char **path1, char **path2)
{
	const char *cmd, *cp = *cpp;
	char *cp2;
	int base = 0;
	long l;
	int i, cmdnum;

	/* Skip leading whitespace */
	cp = cp + strspn(cp, WHITESPACE);

	/* Ignore blank lines */
	if (!*cp)
		return(-1);

	/* Figure out which command we have */
	for(i = 0; cmds[i].c; i++) {
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
		return(-1);
	}

	/* Get arguments and parse flags */
	*pflag = *n_arg = 0;
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
		break;
	default:
		fatal("Command not implemented");
	}

	*cpp = cp;
	return(cmdnum);
}

int
parse_dispatch_command(int in, int out, const char *cmd, char **pwd)
{
	char *path1, *path2, *tmp;
	int pflag, cmdnum, i;
	unsigned long n_arg;
	Attrib a, *aa;
	char path_buf[MAXPATHLEN];
	int err = 0;
	glob_t g;

	path1 = path2 = NULL;
	cmdnum = parse_args(&cmd, &pflag, &n_arg, &path1, &path2);

	memset(&g, 0, sizeof(g));

	/* Perform command */
	switch (cmdnum) {
	case -1:
		break;
	case I_GET:
		err = process_get(in, out, path1, path2, *pwd, pflag);
		break;
	case I_PUT:
		err = process_put(in, out, path1, path2, *pwd, pflag);
		break;
	case I_RENAME:
		path1 = make_absolute(path1, *pwd);
		path2 = make_absolute(path2, *pwd);
		err = do_rename(in, out, path1, path2);
		break;
	case I_SYMLINK:
		if (version < 3) {
			error("The server (version %d) does not support "
			    "this operation", version);
			err = -1;
		} else {
			path2 = make_absolute(path2, *pwd);
			err = do_symlink(in, out, path1, path2);
		}
		break;
	case I_RM:
		path1 = make_absolute(path1, *pwd);
		remote_glob(in, out, path1, GLOB_NOCHECK, NULL, &g);
		for(i = 0; g.gl_pathv[i]; i++) {
			printf("Removing %s\n", g.gl_pathv[i]);
			if (do_rm(in, out, g.gl_pathv[i]) == -1)
				err = -1;
		}
		break;
	case I_MKDIR:
		path1 = make_absolute(path1, *pwd);
		attrib_clear(&a);
		a.flags |= SSH2_FILEXFER_ATTR_PERMISSIONS;
		a.perm = 0777;
		err = do_mkdir(in, out, path1, &a);
		break;
	case I_RMDIR:
		path1 = make_absolute(path1, *pwd);
		err = do_rmdir(in, out, path1);
		break;
	case I_CHDIR:
		path1 = make_absolute(path1, *pwd);
		if ((tmp = do_realpath(in, out, path1)) == NULL) {
			err = 1;
			break;
		}
		if ((aa = do_stat(in, out, tmp, 0)) == NULL) {
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
			do_ls(in, out, *pwd);
			break;
		}
		path1 = make_absolute(path1, *pwd);
		if ((tmp = do_realpath(in, out, path1)) == NULL)
			break;
		xfree(path1);
		path1 = tmp;
		if ((aa = do_stat(in, out, path1, 0)) == NULL)
			break;
		if ((aa->flags & SSH2_FILEXFER_ATTR_PERMISSIONS) &&
		    !S_ISDIR(aa->perm)) {
			error("Can't ls: \"%s\" is not a directory", path1);
			break;
		}
		do_ls(in, out, path1);
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
		remote_glob(in, out, path1, GLOB_NOCHECK, NULL, &g);
		for(i = 0; g.gl_pathv[i]; i++) {
			printf("Changing mode on %s\n", g.gl_pathv[i]);
			do_setstat(in, out, g.gl_pathv[i], &a);
		}
		break;
	case I_CHOWN:
		path1 = make_absolute(path1, *pwd);
		remote_glob(in, out, path1, GLOB_NOCHECK, NULL, &g);
		for(i = 0; g.gl_pathv[i]; i++) {
			if (!(aa = do_stat(in, out, g.gl_pathv[i], 0)))
				continue;
			if (!(aa->flags & SSH2_FILEXFER_ATTR_UIDGID)) {
				error("Can't get current ownership of "
				    "remote file \"%s\"", g.gl_pathv[i]);
				continue;
			}
			printf("Changing owner on %s\n", g.gl_pathv[i]);
			aa->flags &= SSH2_FILEXFER_ATTR_UIDGID;
			aa->uid = n_arg;
			do_setstat(in, out, g.gl_pathv[i], aa);
		}
		break;
	case I_CHGRP:
		path1 = make_absolute(path1, *pwd);
		remote_glob(in, out, path1, GLOB_NOCHECK, NULL, &g);
		for(i = 0; g.gl_pathv[i]; i++) {
			if (!(aa = do_stat(in, out, g.gl_pathv[i], 0)))
				continue;
			if (!(aa->flags & SSH2_FILEXFER_ATTR_UIDGID)) {
				error("Can't get current ownership of "
				    "remote file \"%s\"", g.gl_pathv[i]);
				continue;
			}
			printf("Changing group on %s\n", g.gl_pathv[i]);
			aa->flags &= SSH2_FILEXFER_ATTR_UIDGID;
			aa->gid = n_arg;
			do_setstat(in, out, g.gl_pathv[i], aa);
		}
		break;
	case I_PWD:
		printf("Remote working directory: %s\n", *pwd);
		break;
	case I_LPWD:
		if (!getcwd(path_buf, sizeof(path_buf)))
			error("Couldn't get local cwd: %s",
			    strerror(errno));
		else
			printf("Local working directory: %s\n",
			    path_buf);
		break;
	case I_QUIT:
		return(-1);
	case I_HELP:
		help();
		break;
	case I_VERSION:
		printf("SFTP protocol version %d\n", version);
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

	/* If an error occurs in batch mode we should abort. */
	if (infile != stdin && err > 0)
		return -1;

	return(0);
}

void
interactive_loop(int fd_in, int fd_out, char *file1, char *file2)
{
	char *pwd;
	char *dir = NULL;
	char cmd[2048];

	version = do_init(fd_in, fd_out);
	if (version == -1)
		fatal("Couldn't initialise connection to server");

	pwd = do_realpath(fd_in, fd_out, ".");
	if (pwd == NULL)
		fatal("Need cwd");

	if (file1 != NULL) {
		dir = xstrdup(file1);
		dir = make_absolute(dir, pwd);

		if (remote_is_dir(fd_in, fd_out, dir) && file2 == NULL) {
			printf("Changing to: %s\n", dir);
			snprintf(cmd, sizeof cmd, "cd \"%s\"", dir);
			parse_dispatch_command(fd_in, fd_out, cmd, &pwd);
		} else {
			if (file2 == NULL)
				snprintf(cmd, sizeof cmd, "get %s", dir);
			else
				snprintf(cmd, sizeof cmd, "get %s %s", dir,
				    file2);

			parse_dispatch_command(fd_in, fd_out, cmd, &pwd);
			return;
		}
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(infile, NULL, _IOLBF, 0);

	for(;;) {
		char *cp;

		printf("sftp> ");

		/* XXX: use libedit */
		if (fgets(cmd, sizeof(cmd), infile) == NULL) {
			printf("\n");
			break;
		} else if (infile != stdin) /* Bluff typing */
			printf("%s", cmd);

		cp = strrchr(cmd, '\n');
		if (cp)
			*cp = '\0';

		if (parse_dispatch_command(fd_in, fd_out, cmd, &pwd))
			break;
	}
	xfree(pwd);
}
