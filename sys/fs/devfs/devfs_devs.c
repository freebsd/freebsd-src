#define DEBUG 1
/*
 * Copyright (c) 2000
 *	Poul-Henning Kamp.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the University nor the names of its contributors
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
 * From: FreeBSD: src/sys/miscfs/kernfs/kernfs_vfsops.c 1.36
 *
 * $FreeBSD$
 */

#include "opt_devfs.h"
#ifndef NODEVFS

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <machine/atomic.h>

#include <fs/devfs/devfs.h>

static dev_t devfs_inot[NDEVFSINO];
static dev_t *devfs_overflow;
static int devfs_ref[NDEVFSINO];
static int *devfs_refoverflow;
static int devfs_nextino = 3;
static int devfs_numino;
static int devfs_topino;
static int devfs_noverflowwant = NDEVFSOVERFLOW;
static int devfs_noverflow;
static unsigned devfs_generation;

static void devfs_attemptoverflow(int insist);
static struct devfs_dirent *devfs_find (struct devfs_dirent *dd, const char *name, int namelen);

SYSCTL_NODE(_vfs, OID_AUTO, devfs, CTLFLAG_RW, 0, "DEVFS filesystem");
SYSCTL_UINT(_vfs_devfs, OID_AUTO, noverflow, CTLFLAG_RW,
	&devfs_noverflowwant, 0, "Size of DEVFS overflow table");
SYSCTL_UINT(_vfs_devfs, OID_AUTO, generation, CTLFLAG_RD,
	&devfs_generation, 0, "DEVFS generation number");
SYSCTL_UINT(_vfs_devfs, OID_AUTO, inodes, CTLFLAG_RD,
	&devfs_numino, 0, "DEVFS inodes");
SYSCTL_UINT(_vfs_devfs, OID_AUTO, topinode, CTLFLAG_RD,
	&devfs_topino, 0, "DEVFS highest inode#");

static int *
devfs_itor(int inode)
{
	if (inode < NDEVFSINO)
		return (&devfs_ref[inode]);
	else if (inode < NDEVFSINO + devfs_noverflow)
		return (&devfs_refoverflow[inode - NDEVFSINO]);
	else
		panic ("YRK!");
}

static void
devfs_dropref(int inode)
{
	int *ip;

	ip = devfs_itor(inode);
	atomic_add_int(ip, -1);
}

static int
devfs_getref(int inode)
{
	int *ip, i, j;
	dev_t *dp;

	ip = devfs_itor(inode);
	dp = devfs_itod(inode);
	for (;;) {
		i = *ip;
		j = i + 1;
		if (!atomic_cmpset_int(ip, i, j))
			continue;
		if (*dp != NULL)
			return (1);
		atomic_add_int(ip, -1);
		return(0);
	}
}

struct devfs_dirent **
devfs_itode (struct devfs_mount *dm, int inode)
{

	if (inode < NDEVFSINO)
		return (&dm->dm_dirent[inode]);
	if (devfs_overflow == NULL)
		return (NULL);
	if (inode < NDEVFSINO + devfs_noverflow)
		return (&dm->dm_overflow[inode - NDEVFSINO]);
	return (NULL);
}

dev_t *
devfs_itod (int inode)
{

	if (inode < NDEVFSINO)
		return (&devfs_inot[inode]);
	if (devfs_overflow == NULL)
		return (NULL);
	if (inode < NDEVFSINO + devfs_noverflow)
		return (&devfs_overflow[inode - NDEVFSINO]);
	return (NULL);
}

static void
devfs_attemptoverflow(int insist)
{
	dev_t **ot;
	int *or;
	int n, nb;

	/* Check if somebody beat us to it */
	if (devfs_overflow != NULL)
		return;
	ot = NULL;
	or = NULL;
	n = devfs_noverflowwant;
	nb = sizeof (struct dev_t *) * n;
	MALLOC(ot, dev_t **, nb, M_DEVFS, (insist ? M_WAITOK : M_NOWAIT) | M_ZERO);
	if (ot == NULL)
		goto bail;
	nb = sizeof (int) * n;
	MALLOC(or, int *, nb, M_DEVFS, (insist ? M_WAITOK : M_NOWAIT) | M_ZERO);
	if (or == NULL)
		goto bail;
	if (!atomic_cmpset_ptr(&devfs_overflow, NULL, ot))
		goto bail;
	devfs_refoverflow = or;
	devfs_noverflow = n;
	printf("DEVFS Overflow table with %d entries allocated when %d in use\n", n, devfs_numino);
	return;

bail:
	/* Somebody beat us to it, or something went wrong. */
	if (ot != NULL)
		FREE(ot, M_DEVFS);
	if (or != NULL)
		FREE(or, M_DEVFS);
	return;
}

static struct devfs_dirent *
devfs_find(struct devfs_dirent *dd, const char *name, int namelen)
{
	struct devfs_dirent *de;

	TAILQ_FOREACH(de, &dd->de_dlist, de_list) {
		if (namelen != de->de_dirent->d_namlen)
			continue;
		if (bcmp(name, de->de_dirent->d_name, namelen) != 0)
			continue;
		break;
	}
	return (de);
}

struct devfs_dirent *
devfs_newdirent(char *name, int namelen)
{
	int i;
	struct devfs_dirent *de;
	struct dirent d;

	d.d_namlen = namelen;
	i = sizeof (*de) + GENERIC_DIRSIZ(&d);
	MALLOC(de, struct devfs_dirent *, i, M_DEVFS, M_WAITOK | M_ZERO);
	de->de_dirent = (struct dirent *)(de + 1);
	de->de_dirent->d_namlen = namelen;
	de->de_dirent->d_reclen = GENERIC_DIRSIZ(&d);
	bcopy(name, de->de_dirent->d_name, namelen);
	de->de_dirent->d_name[namelen] = '\0';
	vfs_timestamp(&de->de_ctime);
	de->de_mtime = de->de_atime = de->de_ctime;
	de->de_links = 1;
	return (de);
}

struct devfs_dirent *
devfs_vmkdir(char *name, int namelen, struct devfs_dirent *dotdot)
{
	struct devfs_dirent *dd;
	struct devfs_dirent *de;

	dd = devfs_newdirent(name, namelen);

	TAILQ_INIT(&dd->de_dlist);

	dd->de_dirent->d_type = DT_DIR;
	dd->de_mode = 0555;
	dd->de_links = 2;
	dd->de_dir = dd;

	de = devfs_newdirent(".", 1);
	de->de_dirent->d_type = DT_DIR;
	de->de_dir = dd;
	de->de_flags |= DE_DOT;
	TAILQ_INSERT_TAIL(&dd->de_dlist, de, de_list);

	de = devfs_newdirent("..", 2);
	de->de_dirent->d_type = DT_DIR;
	if (dotdot == NULL)
		de->de_dir = dd;
	else
		de->de_dir = dotdot;
	de->de_flags |= DE_DOTDOT;
	TAILQ_INSERT_TAIL(&dd->de_dlist, de, de_list);

	return (dd);
}

static void
devfs_delete(struct devfs_dirent *dd, struct devfs_dirent *de)
{

	if (de->de_symlink) {
		FREE(de->de_symlink, M_DEVFS);
		de->de_symlink = NULL;
	}
	if (de->de_vnode)
		de->de_vnode->v_data = NULL;
	TAILQ_REMOVE(&dd->de_dlist, de, de_list);
	FREE(de, M_DEVFS);
}

void
devfs_purge(struct devfs_dirent *dd)
{
	struct devfs_dirent *de;

	for (;;) {
		de = TAILQ_FIRST(&dd->de_dlist);
		if (de == NULL)
			break;
		devfs_delete(dd, de);
	}
	FREE(dd, M_DEVFS);
}


int
devfs_populate(struct devfs_mount *dm)
{
	int i, j;
	dev_t dev, pdev;
	struct devfs_dirent *dd;
	struct devfs_dirent *de, **dep;
	char *q, *s;

	if (dm->dm_generation == devfs_generation)
		return (0);
	lockmgr(&dm->dm_lock, LK_UPGRADE, 0, curthread);
	if (devfs_noverflow && dm->dm_overflow == NULL) {
		i = devfs_noverflow * sizeof (struct devfs_dirent *);
		MALLOC(dm->dm_overflow, struct devfs_dirent **, i,
			M_DEVFS, M_WAITOK | M_ZERO);
	}
	while (dm->dm_generation != devfs_generation) {
		dm->dm_generation = devfs_generation;
		for (i = 0; i <= devfs_topino; i++) {
			dev = *devfs_itod(i);
			dep = devfs_itode(dm, i);
			de = *dep;
			if (dev == NULL && de == DE_DELETED) {
				*dep = NULL;
				continue;
			}
			if (dev == NULL && de != NULL) {
				dd = de->de_dir;
				*dep = NULL;
				TAILQ_REMOVE(&dd->de_dlist, de, de_list);
				if (de->de_vnode)
					de->de_vnode->v_data = NULL;
				FREE(de, M_DEVFS);
				devfs_dropref(i);
				continue;
			}
			if (dev == NULL)
				continue;
			if (de != NULL)
				continue;
			if (!devfs_getref(i))
				continue;
			dd = dm->dm_basedir;
			s = dev->si_name;
			for (;;) {
				for (q = s; *q != '/' && *q != '\0'; q++)
					continue;
				if (*q != '/')
					break;
				de = devfs_find(dd, s, q - s);
				if (de == NULL) {
					de = devfs_vmkdir(s, q - s, dd);
					de->de_inode = dm->dm_inode++;
					TAILQ_INSERT_TAIL(&dd->de_dlist, de, de_list);
					dd->de_links++;
				}
				s = q + 1;
				dd = de;
			}
			de = devfs_newdirent(s, q - s);
			if (dev->si_flags & SI_ALIAS) {
				de->de_inode = dm->dm_inode++;
				de->de_uid = 0;
				de->de_gid = 0;
				de->de_mode = 0755;
				de->de_dirent->d_type = DT_LNK;
				pdev = dev->si_parent;
				j = strlen(pdev->si_name) + 1;
				MALLOC(de->de_symlink, char *, j, M_DEVFS, M_WAITOK);
				bcopy(pdev->si_name, de->de_symlink, j);
			} else {
				de->de_inode = i;
				de->de_uid = dev->si_uid;
				de->de_gid = dev->si_gid;
				de->de_mode = dev->si_mode;
				de->de_dirent->d_type = DT_CHR;
			}
			*dep = de;
			de->de_dir = dd;
			TAILQ_INSERT_TAIL(&dd->de_dlist, de, de_list);
#if 0
			printf("Add ino%d %s\n", i, dev->si_name);
#endif
		}
	}
	lockmgr(&dm->dm_lock, LK_DOWNGRADE, 0, curthread);
	return (0);
}

static void
devfs_create(dev_t dev)
{
	int ino, i, *ip;
	dev_t *dp;

	for (;;) {
		/* Grab the next inode number */
		ino = devfs_nextino;
		i = ino + 1;
		/* wrap around when we reach the end */
		if (i >= NDEVFSINO + devfs_noverflow)
			i = 3;
		if (!atomic_cmpset_int(&devfs_nextino, ino, i))
			continue;

		/* see if it was occupied */
		dp = devfs_itod(ino);
		if (dp == NULL)
			Debugger("dp == NULL\n");
		if (*dp != NULL)
			continue;
		ip = devfs_itor(ino);
		if (ip == NULL)
			Debugger("ip == NULL\n");
		if (*ip != 0)
			continue;

		if (!atomic_cmpset_ptr(dp, NULL, dev))
			continue;

		dev->si_inode = ino;
		for (;;) {
			i = devfs_topino;
			if (i >= ino)
				break;
			if (atomic_cmpset_int(&devfs_topino, i, ino))
				break;
			printf("failed topino %d %d\n", i, ino);
		}
		break;
	}

	atomic_add_int(&devfs_numino, 1);
	atomic_add_int(&devfs_generation, 1);
	if (devfs_overflow == NULL && devfs_numino + 100 > NDEVFSINO)
		devfs_attemptoverflow(0);
}

static void
devfs_destroy(dev_t dev)
{
	int ino, i;

	ino = dev->si_inode;
	dev->si_inode = 0;
	if (ino == 0)
		return;
	if (atomic_cmpset_ptr(devfs_itod(ino), dev, NULL)) {
		atomic_add_int(&devfs_generation, 1);
		atomic_add_int(&devfs_numino, -1);
		i = devfs_nextino;
		if (ino < i)
			atomic_cmpset_int(&devfs_nextino, i, ino);
	}
}

devfs_create_t *devfs_create_hook = devfs_create;
devfs_destroy_t *devfs_destroy_hook = devfs_destroy;
int devfs_present = 1;
#endif
