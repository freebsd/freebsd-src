/*
 * Copyright (c) 2004 Alfred Perlstein <alfred@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: autodriver.c,v 1.9 2004/09/08 08:12:21 bright Exp $
 * $FreeBSD$
 */
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/dirent.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/stat.h>

#include <libautofs.h>

struct autoentry {
	char *ae_mnt;		/* autofs mountpoint. */
	char *ae_path;		/* path under mount. */
	char *ae_type;		/* fs to be mounted type. */
	char *ae_opts;		/* options passed to mount. */
	char *ae_rpath;		/* remote path */
	char *ae_free;		/* freeme! */
	char *ae_fullpath;	/* full path to mount */
	int ae_line;		/* line it came from in the conf. */
	int ae_indirect;	/* is this an indirect mount? */
	int ae_direct;		/* is this a direct mount? */
	int ae_browse;		/* browseable? */
	struct autoentry *ae_next;	/* next. */
};

struct autoentry *entries;
const char *mount_prog = "mount";
const char *fstype = "autofs";

void *xmalloc(size_t);
void *xcalloc(size_t number, size_t size);
void parsetab(void);
void populate_tab(void);
void doreq(autoh_t, autoreq_t);
void dotheneedful(autoh_t);
void eventloop(void);
int poll_handles(autoh_t *array, int cnt);
int mount_indirect(struct autofs_userreq *req, struct autoentry *ent);
int mount_direct(struct autofs_userreq *req, struct autoentry *ent);
int mount_browse(struct autofs_userreq *req, struct autoentry *ent);

#define DSTR(s)	sizeof(s) - 1, s

struct dirent dumbents[] = {
	{50, sizeof(struct dirent), DT_DIR, DSTR("one") },
	{51, sizeof(struct dirent), DT_DIR, DSTR(".") },
	{52, sizeof(struct dirent), DT_DIR, DSTR("..") },
	{50, sizeof(struct dirent), DT_DIR, DSTR("two") },
};

void *
xmalloc(size_t size)
{
	void *ret;

	ret = malloc(size);
	if (ret == NULL)
		err(1, "malloc %d", (int) size);
	return (ret);
}

void *
xcalloc(size_t number, size_t size)
{
	void *ret;

	ret = calloc(number, size);
	if (ret == NULL)
		err(1, "calloc %d %d", (int)number, (int)size);
	return (ret);
}

void
parsetab(void)
{
	FILE *fp;
	const char *tab;
	char *cp, *p, *line, *opt;
	size_t len;
	struct autoentry *ent;
	int i, lineno, x, gotopt;
	const char *expecting = "expecting 'direct', 'indirect' or 'browse'";
	const char *tabfiles[] = {
		"/etc/autotab", "/usr/local/etc/autotab", "./autotab", NULL
	};

	lineno = 0;
	for (i = 0; (tab = tabfiles[i]) != NULL; i++) {
		tab = tabfiles[i];
		fp = fopen(tab, "r");
		if (fp == NULL)
			warn("fopen %s", tab);
		if (fp != NULL)
			break;
	}
	if (fp == NULL) {
		err(1, "no config file available.");
	}

	fprintf(stderr, "using config file: %s\n", tab);

	while ((cp = fgetln(fp, &len)) != NULL) {
		lineno++;
		while (len > 0 && isspace(cp[len - 1]))
			len--;
		line = xmalloc(len + 1);
		bcopy(cp, line, len);
		line[len] = '\0';
		cp = line;
		if ((cp = strchr(line, '#')) != NULL)
			*cp = '\0';
		cp = line;
		while (isspace(*cp))
			cp++;
		if (*cp == '\0') {
			free(line);
			continue;
		}
		ent = xcalloc(1, sizeof(*ent));
		if ((p = strsep(&cp, " \t")) == NULL)
			goto bad;
		ent->ae_mnt = p;
		if ((p = strsep(&cp, " \t")) == NULL)
			goto bad;
		ent->ae_path = p;
		if ((p = strsep(&cp, " \t")) == NULL)
			goto bad;
		ent->ae_type = p;
		if ((p = strsep(&cp, " \t")) == NULL)
			goto bad;
		ent->ae_opts = p;
		if ((p = strsep(&cp, " \t")) == NULL)
			goto bad;
		ent->ae_rpath = p;
		if ((p = strsep(&cp, " \t")) == NULL)
			goto bad;
		gotopt = 0;
		opt = p;
		while ((p = strsep(&opt, ",")) != NULL) {
			if (strcmp(p, "indirect") == 0) {
				ent->ae_indirect = 1;
				gotopt = 1;
			} else if (strcmp(p, "direct") == 0) {
				ent->ae_direct = 1;
				gotopt = 1;
			} else if (strcmp(p, "browse") == 0) {
				ent->ae_browse = 1;
				gotopt = 1;
			} else {
				warnx("unreconized option '%s', %s",
				    p, expecting);
				goto bad2;
			}
		}
		if (!gotopt) {
			warnx("no options specified %s", expecting);
			goto bad2;
		}
		if (ent->ae_direct && ent->ae_indirect) {
			warnx("direct and indirect are mutually exclusive");
			goto bad2;

		}
		x = asprintf(&ent->ae_fullpath, "%s/%s",
		    ent->ae_mnt, ent->ae_path);
		if (x == -1)
			err(1, "asprintf");

		if (strlen(ent->ae_fullpath) + 1 > PATH_MAX) {
			warnx("Error in file %s, line %d, "
			    "mountpath (%s) exceeds PATH_MAX (%d)",
			    tab, lineno, ent->ae_fullpath, PATH_MAX);
			goto bad2;
		}
		ent->ae_line = lineno;
		ent->ae_free = line;
		ent->ae_next = entries;
		entries = ent;
		continue;
bad:
		warnx("Parse error in file %s, line %d", tab, lineno);
bad2:
		free(ent->ae_fullpath);
		free(line);
		free(ent);
	}
	if (ferror(fp))
		err(1, "error with file %s", tab);
}

void
populate_tab(void)
{
	struct autoentry *ent;
	char *path, *cmd;
	int error;
	autoh_t ah;

	path = cmd = NULL;

	for (ent = entries; ent != NULL; ent = ent->ae_next) {
		free(path);
		free(cmd);
		error = asprintf(&path, "%s/%s", ent->ae_mnt, ent->ae_path);
		if (error == -1)
			err(1, "asprintf");
		error = asprintf(&cmd, "mkdir -p %s", path);
		if (error == -1)
			err(1, "asprintf");
		error = system(cmd);
		if (error) {
			warn("system: %s", cmd);
			continue;
		}
		if (autoh_get(ent->ae_mnt, &ah)) {
			warn("autoh_get %s", path);
			continue;
		}
		error = autoh_togglepath(ah, AUTO_MOUNTER, getpid(), path);
		if (error) {
			err(1, "AUTO_MOUNTER %s", path);
			continue;
		}
		if (ent->ae_browse) {
			error = autoh_togglepath(ah, AUTO_BROWSE, getpid(),
			    path);
			if (error)
				err(1, "AUTO_BROWSE %s", path);
		}
		if (ent->ae_direct) {
			error = autoh_togglepath(ah, AUTO_DIRECT, getpid(),
			    path);
			if (error)
				err(1, "AUTO_DIRECT %s", path);
		}
		if (ent->ae_indirect) {
			error = autoh_togglepath(ah, AUTO_INDIRECT, getpid(),
			    path);
			if (error)
				err(1, "AUTO_INDIRECT %s", path);
		}
		autoh_free(ah);
	}
	free(path);
	free(cmd);
}

/*
 * Process an autofs request, scan the list of entries in the config
 * looking for our node, if found mount it.
 */
void
doreq(autoh_t ah, autoreq_t req)
{
	struct autoentry *ent;
	int error;
	int mcmp;
	int xid;
	const char *mnt;

	mnt = autoh_mp(ah);

	autoreq_seterrno(req, 0);
	for (ent = entries; ent != NULL; ent = ent->ae_next) {
		fprintf(stderr, "comparing {%s,%s} to {%s,%s}\n",
		    mnt, ent->ae_mnt, autoreq_getpath(req), ent->ae_path);
		fprintf(stderr, "comparing {%d,%d} to {%d,%d}\n",
		    (int)strlen(mnt),
		    (int)strlen(ent->ae_mnt),
		    (int)strlen(autoreq_getpath(req)),
		    (int)strlen(ent->ae_path));
		autoreq_getxid(req, &xid);
		fprintf(stderr, "req xid %d\n", xid);
		if ((mcmp = strcmp(mnt, ent->ae_mnt)) != 0) {
			fprintf(stderr, "mcmp = %d\n", mcmp);
			continue;
		}
		if (mount_direct(req, ent))
			goto serve;
		if (mount_indirect(req, ent))
			goto serve;
		if (mount_browse(req, ent))
			goto serve;
	}
	fprintf(stderr, "no entry found...\n");
	autoreq_seterrno(req, ENOENT);
serve:
	error = autoreq_serv(ah, req);
	if (error == -1) {
		warn("AUTOFS_CTL_SERVREQ");
	}
}

int
mount_indirect(req, ent)
	struct autofs_userreq *req;
	struct autoentry *ent;
{
	struct stat sb;
	char *path, *cmd;
	int error, x;

	if (ent->ae_indirect != 1) {
		fprintf(stderr, "not indirect.\n");
		return (0);
	}
	fprintf(stderr, "indirect mount...\n");
	/*
	 * handle lookups, fake all stat(2) requests... this is bad,
	 * but we're a driver so we don't care...
	 * If we don't care about the type of request, then just return.
	 */
	switch (autoreq_getop(req)) {
	case AUTOREQ_OP_LOOKUP:
		break;
	case AUTOREQ_OP_STAT:
		fprintf(stderr, "stat\n");
		return (1);
	default:
		fprintf(stderr, "unknown\n");
		return (0);
	}
	if (stat(ent->ae_fullpath, &sb))
		return (0);
	if (sb.st_ino != autoreq_getdirino(req)) {
		fprintf(stderr, "st_ino %d != dirino %d\n",
		    (int)sb.st_ino, (int)autoreq_getdirino(req));
		return (0);
	}
	x = asprintf(&path, "%s/%s", ent->ae_fullpath, autoreq_getpath(req));
	if (x > PATH_MAX) {
		autoreq_seterrno(req, ENAMETOOLONG);
		return (1);
	}
	if (mkdir(path, 0555) == -1)
		warn("mkdir %s", path);
	error = asprintf(&cmd, "%s -t %s -o %s %s/%s %s", mount_prog,
	    ent->ae_type, ent->ae_opts, ent->ae_rpath, autoreq_getpath(req), path);
	fprintf(stderr, "running:\n\t%s\n", cmd);
	error = system(cmd);
	fprintf(stderr, "error = %d\n", error);
	free(cmd);
	if (error) {
		if (rmdir(path) == -1)
			warn("rmdir %s", path);
		autoreq_seterrno(req, ENOENT);
	} else {
		if (stat(path, &sb) != -1)
			autoreq_setino(req, sb.st_ino);
		/* XXX !!! */
		/* req->au_flags = 1; */
	}
	free(path);
	return (1);
}

int
mount_direct(req, ent)
	struct autofs_userreq *req;
	struct autoentry *ent;
{
	struct stat sb;
	char *cmd;
	int error;

	if (ent->ae_direct != 1) {
		fprintf(stderr, "not direct.\n");
		return (0);
	}
	fprintf(stderr, "direct mount...\n");
	/*
	 * handle lookups, fake all stat(2) requests... this is bad,
	 * but we're a driver so we don't care...
	 * If we don't care about the type of request, then just return.
	 */
	switch (autoreq_getop(req)) {
	case AUTOREQ_OP_LOOKUP:
		break;
	case AUTOREQ_OP_STAT:
		return (1);
	default:
		return (0);
	}
	if (stat(ent->ae_fullpath, &sb))
		return (0);
	if (sb.st_ino != autoreq_getino(req))
		return (0);
	error = asprintf(&cmd, "%s -t %s -o %s %s %s", mount_prog,
	    ent->ae_type, ent->ae_opts, ent->ae_rpath, ent->ae_fullpath);
	if (error == -1)
		err(1, "asprintf");
	fprintf(stderr, "running:\n\t%s\n", cmd);
	error = system(cmd);
	fprintf(stderr, "error = %d\n", error);
	free(cmd);
	if (error) {
		autoreq_seterrno(req, ENOENT);
		return (1);
	}
	/* XXX: fix ONLIST in kernel */
	/* req->au_flags = 1; */
	return (1);
}

int
mount_browse(req, ent)
	struct autofs_userreq *req;
	struct autoentry *ent;
{
	off_t off;

	if (ent->ae_browse != 1)
		return (0);
	if (autoreq_getop(req) != AUTOREQ_OP_READDIR)
		return (0);
	autoreq_getoffset(req, &off);
	if (off < sizeof(dumbents))
		autoreq_setaux(req, dumbents, sizeof(dumbents));
	fprintf(stderr, "mount_browse: offset %d, size %d\n",
	    (int)off, (int)sizeof(dumbents));
	autoreq_seteof(req, 1);
	return (1);
}

/*
 * Ask the filesystem passed in if it has a pending request.
 * if so process them.
 */
void
dotheneedful(autoh_t ah)
{
	int cnt, i;
	autoreq_t *reqs;

	if (autoreq_get(ah, &reqs, &cnt))
		err(1, "autoreq_get");

	for (i = 0; i < cnt; i++) {
		fprintf(stderr, "processing request for '%s' '%s'\n",
		    autoh_mp(ah), autoreq_getpath(reqs[i]));
		doreq(ah, reqs[i]);
	}
	free(reqs);
}

int
poll_handles(autoh_t *array, int cnt)
{
	int i, saved_errno, x;
	static struct pollfd *pfd = NULL;

	pfd = reallocf(pfd, cnt * sizeof(*pfd));
	if (pfd == NULL)
		return (-1);
	for (i = 0; i < cnt; i++) {
		pfd[i].fd = autoh_fd(array[i]);
		pfd[i].events = POLLPRI;
		pfd[i].revents = 0;
	}
	fprintf(stderr, "start polling...\n");
	x = poll(pfd, cnt, 10000);
	saved_errno = errno;
	fprintf(stderr, "done polling...\n");
	errno = saved_errno;
	if (x == -1)
		return (-1);
	/* at least one fs is ready... */
	if (x > 0)
		return (0);
	return (0);
}

void
eventloop(void)
{
	autoh_t *array;
	int cnt, i;

	fprintf(stderr, "starting event loop...\n");
	for ( ;; ) {
		if (autoh_getall(&array, &cnt))
			err(1, "autoh_getall");
		if (poll_handles(array, cnt))
			err(1, "poll_handles");
		for (i = 0; i < cnt; i++) {
			dotheneedful(array[i]);
		}
		autoh_freeall(array);
	}
}

int
main(int argc __unused, char **argv __unused)
{

	if (getuid() != 0)
		errx(1, "autodriver needs to be run as root to work.");
	parsetab();
	populate_tab();
	eventloop();
	return (0);
}
