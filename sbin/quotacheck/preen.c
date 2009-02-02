/*	$NetBSD: preen.c,v 1.18 1998/07/26 20:02:36 mycroft Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)preen.c	8.5 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: preen.c,v 1.18 1998/07/26 20:02:36 mycroft Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/queue.h>

#include <err.h>
#include <ctype.h>
#include <fstab.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "quotacheck.h"

struct partentry {
	TAILQ_ENTRY(partentry)	 p_entries;
	char			*p_devname;	/* device name */
	char			*p_mntpt;	/* mount point */
	char			*p_type;	/* file system type */
	struct quotaname	*p_quota;	/* quota file info ptr */
};

TAILQ_HEAD(part, partentry) badh;

struct diskentry {
	TAILQ_ENTRY(diskentry)	    d_entries;
	char			   *d_name;	/* disk base name */
	TAILQ_HEAD(prt, partentry)  d_part;	/* list of partitions on disk */
	int			    d_pid;	/* 0 or pid of fsck proc */
};

TAILQ_HEAD(disk, diskentry) diskh;

static struct diskentry *finddisk(const char *);
static void addpart(const char *, const char *, const char *,
    struct quotaname *);
static int startdisk(struct diskentry *);
extern void *emalloc(size_t);
extern char *estrdup(const char *);

int
checkfstab(void)
{
	struct fstab *fs;
	struct diskentry *d, *nextdisk;
	struct partentry *p;
	int ret, pid, retcode, passno, sumstatus, status, nextpass;
	char *name;
	struct quotaname *qnp;

	TAILQ_INIT(&badh);
	TAILQ_INIT(&diskh);

	sumstatus = 0;

	nextpass = 0;
	for (passno = 1; nextpass != INT_MAX; passno = nextpass) {
		nextpass = INT_MAX;
		if (setfsent() == 0) {
			warnx("Can't open checklist file: %s\n", _PATH_FSTAB);
			return (8);
		}
		while ((fs = getfsent()) != 0) {
			name = fs->fs_spec;
			if (fs->fs_passno > passno && fs->fs_passno < nextpass)
				nextpass = fs->fs_passno;

			if (passno != fs->fs_passno)
				continue;

			if ((qnp = needchk(fs)) == NULL)
				continue;

			if (passno == 1) {
				sumstatus = chkquota(name, fs->fs_file, qnp);

				if (sumstatus)
					return (sumstatus);
				continue;
			}
			if (name == NULL) {
				(void) fprintf(stderr,
				    "BAD DISK NAME %s\n", fs->fs_spec);
				sumstatus |= 8;
				continue;
			}
			addpart(fs->fs_vfstype, name, fs->fs_file, qnp);
		}

		if (passno == 1)
			continue;

		TAILQ_FOREACH(nextdisk, &diskh, d_entries) {
			if ((ret = startdisk(nextdisk)) != 0)
				return ret;
		}

		while ((pid = wait(&status)) != -1) {
			TAILQ_FOREACH(d, &diskh, d_entries)
				if (d->d_pid == pid)
					break;

			if (d == NULL) {
				warnx("Unknown pid %d\n", pid);
				continue;
			}

			if (WIFEXITED(status))
				retcode = WEXITSTATUS(status);
			else
				retcode = 0;

			p = TAILQ_FIRST(&d->d_part);

			if (WIFSIGNALED(status)) {
				(void) fprintf(stderr,
				    "%s: %s (%s): EXITED WITH SIGNAL %d\n",
				    p->p_type, p->p_devname, p->p_mntpt,
				    WTERMSIG(status));
				retcode = 8;
			}

			TAILQ_REMOVE(&d->d_part, p, p_entries);

			if (retcode != 0) {
				TAILQ_INSERT_TAIL(&badh, p, p_entries);
				sumstatus |= retcode;
			} else {
				free(p->p_type);
				free(p->p_devname);
				free(p);
			}
			d->d_pid = 0;

			if (TAILQ_EMPTY(&d->d_part)) {
				TAILQ_REMOVE(&diskh, d, d_entries);
			} else {
				if ((ret = startdisk(d)) != 0)
					return ret;
			}
		}
	}

	if (sumstatus) {
		p = TAILQ_FIRST(&badh);
		if (p == NULL)
			return (sumstatus);

		(void) fprintf(stderr,
			"THE FOLLOWING FILE SYSTEM%s HAD AN %s\n\t",
			TAILQ_NEXT(p, p_entries) ? "S" : "",
			"UNEXPECTED INCONSISTENCY:");

		for (; p; p = TAILQ_NEXT(p, p_entries))
			(void) fprintf(stderr,
			    "%s: %s (%s)%s", p->p_type, p->p_devname,
			    p->p_mntpt, TAILQ_NEXT(p, p_entries) ? ", " : "\n");

		return sumstatus;
	}
	(void) endfsent();
	return (0);
}


static struct diskentry *
finddisk(const char *name)
{
	const char *p;
	size_t len = 0;
	struct diskentry *d;

	p = strrchr(name, '/');
	if (p == NULL)
		p = name;
	else
		p++;
	for (; *p && !isdigit(*p); p++)
		continue;
	for (; *p && isdigit(*p); p++)
		continue;
	len = p - name;
	if (len == 0)
		len = strlen(name);

	TAILQ_FOREACH(d, &diskh, d_entries)
		if (strncmp(d->d_name, name, len) == 0 && d->d_name[len] == 0)
			return d;

	d = emalloc(sizeof(*d));
	d->d_name = estrdup(name);
	d->d_name[len] = '\0';
	TAILQ_INIT(&d->d_part);
	d->d_pid = 0;

	TAILQ_INSERT_TAIL(&diskh, d, d_entries);

	return d;
}

static void
addpart(const char *type, const char *devname, const char *mntpt,
    struct quotaname *qnp)
{
	struct diskentry *d = finddisk(devname);
	struct partentry *p;

	TAILQ_FOREACH(p, &d->d_part, p_entries)
		if (strcmp(p->p_devname, devname) == 0) {
			warnx("%s in fstab more than once!\n", devname);
			return;
		}

	p = emalloc(sizeof(*p));
	p->p_devname = estrdup(devname);
	p->p_mntpt = estrdup(mntpt);
	p->p_type = estrdup(type);
	p->p_quota = qnp;

	TAILQ_INSERT_TAIL(&d->d_part, p, p_entries);
}


static int
startdisk(struct diskentry *d)
{
	struct partentry *p = TAILQ_FIRST(&d->d_part);

	d->d_pid = fork();
	if (d->d_pid < 0) {
		perror("fork");
		return (8);
	}
	if (d->d_pid == 0)
		exit(chkquota(p->p_devname, p->p_mntpt, p->p_quota));
	return (0);
}
