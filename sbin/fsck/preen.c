/*
 * Copyright (c) 1990 The Regents of the University of California.
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
static char sccsid[] = "@(#)preen.c	5.7 (Berkeley) 3/19/91";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/sbin/fsck/preen.c,v 1.2 1993/07/22 16:51:59 jkh Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fstab.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

char	*rawname(), *unrawname(), *blockcheck();

struct part {
	struct	part *next;		/* forward link of partitions on disk */
	char	*name;			/* device name */
	char	*fsname;		/* mounted filesystem name */
	long	auxdata;		/* auxillary data for application */
} *badlist, **badnext = &badlist;

struct disk {
	char	*name;			/* disk base name */
	struct	disk *next;		/* forward link for list of disks */
	struct	part *part;		/* head of list of partitions on disk */
	int	pid;			/* If != 0, pid of proc working on */
} *disks;

int	nrun, ndisks;
char	hotroot;

checkfstab(preen, maxrun, docheck, chkit)
	int preen, maxrun;
	int (*docheck)(), (*chkit)();
{
	register struct fstab *fsp;
	register struct disk *dk, *nextdisk;
	register struct part *pt;
	int ret, pid, retcode, passno, sumstatus, status;
	long auxdata;
	char *name;

	sumstatus = 0;
	for (passno = 1; passno <= 2; passno++) {
		if (setfsent() == 0) {
			fprintf(stderr, "Can't open checklist file: %s\n",
			    _PATH_FSTAB);
			return (8);
		}
		while ((fsp = getfsent()) != 0) {
			if ((auxdata = (*docheck)(fsp)) == 0)
				continue;
			if (preen == 0 || passno == 1 && fsp->fs_passno == 1) {
				if (name = blockcheck(fsp->fs_spec)) {
					if (sumstatus = (*chkit)(name,
					    fsp->fs_file, auxdata, 0))
						return (sumstatus);
				} else if (preen)
					return (8);
			} else if (passno == 2 && fsp->fs_passno > 1) {
				if ((name = blockcheck(fsp->fs_spec)) == NULL) {
					fprintf(stderr, "BAD DISK NAME %s\n",
						fsp->fs_spec);
					sumstatus |= 8;
					continue;
				}
				addpart(name, fsp->fs_file, auxdata);
			}
		}
		if (preen == 0)
			return (0);
	}
	if (preen) {
		if (maxrun == 0)
			maxrun = ndisks;
		if (maxrun > ndisks)
			maxrun = ndisks;
		nextdisk = disks;
		for (passno = 0; passno < maxrun; ++passno) {
			while (ret = startdisk(nextdisk, chkit) && nrun > 0)
				sleep(10);
			if (ret)
				return (ret);
			nextdisk = nextdisk->next;
		}
		while ((pid = wait(&status)) != -1) {
			for (dk = disks; dk; dk = dk->next)
				if (dk->pid == pid)
					break;
			if (dk == 0) {
				printf("Unknown pid %d\n", pid);
				continue;
			}
			if (WIFEXITED(status))
				retcode = WEXITSTATUS(status);
			else
				retcode = 0;
			if (WIFSIGNALED(status)) {
				printf("%s (%s): EXITED WITH SIGNAL %d\n",
					dk->part->name, dk->part->fsname,
					WTERMSIG(status));
				retcode = 8;
			}
			if (retcode != 0) {
				sumstatus |= retcode;
				*badnext = dk->part;
				badnext = &dk->part->next;
				dk->part = dk->part->next;
				*badnext = NULL;
			} else
				dk->part = dk->part->next;
			dk->pid = 0;
			nrun--;
			if (dk->part == NULL)
				ndisks--;

			if (nextdisk == NULL) {
				if (dk->part) {
					while (ret = startdisk(dk, chkit) &&
					    nrun > 0)
						sleep(10);
					if (ret)
						return (ret);
				}
			} else if (nrun < maxrun && nrun < ndisks) {
				for ( ;; ) {
					if ((nextdisk = nextdisk->next) == NULL)
						nextdisk = disks;
					if (nextdisk->part != NULL &&
					    nextdisk->pid == 0)
						break;
				}
				while (ret = startdisk(nextdisk, chkit) &&
				    nrun > 0)
					sleep(10);
				if (ret)
					return (ret);
			}
		}
	}
	if (sumstatus) {
		if (badlist == 0)
			return (sumstatus);
		fprintf(stderr, "THE FOLLOWING FILE SYSTEM%s HAD AN %s\n\t",
			badlist->next ? "S" : "", "UNEXPECTED INCONSISTENCY:");
		for (pt = badlist; pt; pt = pt->next)
			fprintf(stderr, "%s (%s)%s", pt->name, pt->fsname,
			    pt->next ? ", " : "\n");
		return (sumstatus);
	}
	(void)endfsent();
	return (0);
}

struct disk *
finddisk(name)
	char *name;
{
	register struct disk *dk, **dkp;
	register char *p;
	size_t len;

	for (p = name + strlen(name) - 1; p >= name; --p)
		if (isdigit(*p)) {
			len = p - name + 1;
			break;
		}
	if (p < name)
		len = strlen(name);

	for (dk = disks, dkp = &disks; dk; dkp = &dk->next, dk = dk->next) {
		if (strncmp(dk->name, name, len) == 0 &&
		    dk->name[len] == 0)
			return (dk);
	}
	if ((*dkp = (struct disk *)malloc(sizeof(struct disk))) == NULL) {
		fprintf(stderr, "out of memory");
		exit (8);
	}
	dk = *dkp;
	if ((dk->name = malloc(len + 1)) == NULL) {
		fprintf(stderr, "out of memory");
		exit (8);
	}
	(void)strncpy(dk->name, name, len);
	dk->name[len] = '\0';
	dk->part = NULL;
	dk->next = NULL;
	dk->pid = 0;
	ndisks++;
	return (dk);
}

addpart(name, fsname, auxdata)
	char *name, *fsname;
	long auxdata;
{
	struct disk *dk = finddisk(name);
	register struct part *pt, **ppt = &dk->part;

	for (pt = dk->part; pt; ppt = &pt->next, pt = pt->next)
		if (strcmp(pt->name, name) == 0) {
			printf("%s in fstab more than once!\n", name);
			return;
		}
	if ((*ppt = (struct part *)malloc(sizeof(struct part))) == NULL) {
		fprintf(stderr, "out of memory");
		exit (8);
	}
	pt = *ppt;
	if ((pt->name = malloc(strlen(name) + 1)) == NULL) {
		fprintf(stderr, "out of memory");
		exit (8);
	}
	(void)strcpy(pt->name, name);
	if ((pt->fsname = malloc(strlen(fsname) + 1)) == NULL) {
		fprintf(stderr, "out of memory");
		exit (8);
	}
	(void)strcpy(pt->fsname, fsname);
	pt->next = NULL;
	pt->auxdata = auxdata;
}

startdisk(dk, checkit)
	register struct disk *dk;
	int (*checkit)();
{
	register struct part *pt = dk->part;

	dk->pid = fork();
	if (dk->pid < 0) {
		perror("fork");
		return (8);
	}
	if (dk->pid == 0)
		exit((*checkit)(pt->name, pt->fsname, pt->auxdata, 1));
	nrun++;
	return (0);
}

char *
blockcheck(name)
	char *name;
{
	struct stat stslash, stblock, stchar;
	char *raw;
	int retried = 0;

	hotroot = 0;
	if (stat("/", &stslash) < 0) {
		perror("/");
		printf("Can't stat root\n");
		return (0);
	}
retry:
	if (stat(name, &stblock) < 0) {
		perror(name);
		printf("Can't stat %s\n", name);
		return (0);
	}
	if ((stblock.st_mode & S_IFMT) == S_IFBLK) {
		if (stslash.st_dev == stblock.st_rdev)
			hotroot++;
		raw = rawname(name);
		if (stat(raw, &stchar) < 0) {
			perror(raw);
			printf("Can't stat %s\n", raw);
			return (name);
		}
		if ((stchar.st_mode & S_IFMT) == S_IFCHR) {
			return (raw);
		} else {
			printf("%s is not a character device\n", raw);
			return (name);
		}
	} else if ((stblock.st_mode & S_IFMT) == S_IFCHR && !retried) {
		name = unrawname(name);
		retried++;
		goto retry;
	}
	printf("Can't make sense out of name %s\n", name);
	return (0);
}

char *
unrawname(name)
	char *name;
{
	char *dp;
	struct stat stb;

	if ((dp = rindex(name, '/')) == 0)
		return (name);
	if (stat(name, &stb) < 0)
		return (name);
	if ((stb.st_mode & S_IFMT) != S_IFCHR)
		return (name);
	if (dp[1] != 'r')
		return (name);
	(void)strcpy(&dp[1], &dp[2]);
	return (name);
}

char *
rawname(name)
	char *name;
{
	static char rawbuf[32];
	char *dp;

	if ((dp = rindex(name, '/')) == 0)
		return (0);
	*dp = 0;
	(void)strcpy(rawbuf, name);
	*dp = '/';
	(void)strcat(rawbuf, "/r");
	(void)strcat(rawbuf, &dp[1]);
	return (rawbuf);
}
