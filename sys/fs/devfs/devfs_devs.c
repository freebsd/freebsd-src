/*-
 * Copyright (c) 2000,2004
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
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <machine/atomic.h>

#include <fs/devfs/devfs.h>
#include <fs/devfs/devfs_int.h>

static struct cdev *devfs_inot[NDEVFSINO];
static struct cdev **devfs_overflow;
static int devfs_ref[NDEVFSINO];
static int *devfs_refoverflow;
static int devfs_nextino = 3;
static int devfs_numino;
static int devfs_topino;
static int devfs_noverflowwant = NDEVFSOVERFLOW;
static int devfs_noverflow;
static unsigned devfs_generation;

static struct devfs_dirent *devfs_find (struct devfs_dirent *dd, const char *name, int namelen);

static SYSCTL_NODE(_vfs, OID_AUTO, devfs, CTLFLAG_RW, 0, "DEVFS filesystem");
SYSCTL_UINT(_vfs_devfs, OID_AUTO, noverflow, CTLFLAG_RW,
	&devfs_noverflowwant, 0, "Size of DEVFS overflow table");
SYSCTL_UINT(_vfs_devfs, OID_AUTO, generation, CTLFLAG_RD,
	&devfs_generation, 0, "DEVFS generation number");
SYSCTL_UINT(_vfs_devfs, OID_AUTO, inodes, CTLFLAG_RD,
	&devfs_numino, 0, "DEVFS inodes");
SYSCTL_UINT(_vfs_devfs, OID_AUTO, topinode, CTLFLAG_RD,
	&devfs_topino, 0, "DEVFS highest inode#");

unsigned devfs_rule_depth = 1;
SYSCTL_UINT(_vfs_devfs, OID_AUTO, rule_depth, CTLFLAG_RW,
    &devfs_rule_depth, 0, "Max depth of ruleset include");

/*
 * Helper sysctl for devname(3).  We're given a struct cdev * and return
 * the name, if any, registered by the device driver.
 */
static int
sysctl_devname(SYSCTL_HANDLER_ARGS)
{
	int error;
	dev_t ud;
	struct cdev *dev, **dp;

	error = SYSCTL_IN(req, &ud, sizeof (ud));
	if (error)
		return (error);
	if (ud == NODEV)
		return(EINVAL);
	dp = devfs_itod(ud);
	if (dp == NULL)
		return(ENOENT);
	dev = *dp;
	if (dev == NULL)
		return(ENOENT);
	return(SYSCTL_OUT(req, dev->si_name, strlen(dev->si_name) + 1));
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, devname, CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_ANYBODY,
	NULL, 0, sysctl_devname, "", "devname(3) handler");

SYSCTL_INT(_debug_sizeof, OID_AUTO, cdev, CTLFLAG_RD,
    0, sizeof(struct cdev), "sizeof(struct cdev)");

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
	struct cdev **dp;

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

	if (inode < 0)
		return (NULL);
	if (inode < NDEVFSINO)
		return (&dm->dm_dirent[inode]);
	if (devfs_overflow == NULL)
		return (NULL);
	if (inode < NDEVFSINO + devfs_noverflow)
		return (&dm->dm_overflow[inode - NDEVFSINO]);
	return (NULL);
}

struct cdev **
devfs_itod (int inode)
{

	if (inode < 0)
		return (NULL);
	if (inode < NDEVFSINO)
		return (&devfs_inot[inode]);
	if (devfs_overflow == NULL)
		return (NULL);
	if (inode < NDEVFSINO + devfs_noverflow)
		return (&devfs_overflow[inode - NDEVFSINO]);
	return (NULL);
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
#ifdef MAC
	mac_init_devfsdirent(de);
#endif
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
#ifdef MAC
	mac_destroy_devfsdirent(de);
#endif
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
	struct cdev *dev, *pdev;
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
				devfs_delete(dd, de);
				devfs_dropref(i);
				continue;
			}
			if (dev == NULL)
				continue;
			if (de != NULL)
				continue;
			if (!devfs_getref(i))
				continue;
			dd = dm->dm_rootdir;
			s = dev->si_name;
			for (;;) {
				for (q = s; *q != '/' && *q != '\0'; q++)
					continue;
				if (*q != '/')
					break;
				de = devfs_find(dd, s, q - s);
				if (de == NULL) {
					de = devfs_vmkdir(s, q - s, dd);
#ifdef MAC
					mac_create_devfs_directory(
					    dm->dm_mount, s, q - s, de);
#endif
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
#ifdef MAC
			mac_create_devfs_device(dev->si_cred, dm->dm_mount,
			    dev, de);
#endif
			*dep = de;
			de->de_dir = dd;
			devfs_rules_apply(dm, de);
			TAILQ_INSERT_TAIL(&dd->de_dlist, de, de_list);
		}
	}
	lockmgr(&dm->dm_lock, LK_DOWNGRADE, 0, curthread);
	return (0);
}

/*
 * devfs_create() and devfs_destroy() are called from kern_conf.c and
 * in both cases the devlock() mutex is held, so no further locking
 * is necesary and no sleeping allowed.
 */

void
devfs_create(struct cdev *dev)
{
	int ino, i, *ip;
	struct cdev **dp;
	struct cdev **ot;
	int *or;
	int n;

	for (;;) {
		/* Grab the next inode number */
		ino = devfs_nextino;
		i = ino + 1;
		/* wrap around when we reach the end */
		if (i >= NDEVFSINO + devfs_noverflow)
			i = 3;
		devfs_nextino = i;

		/* see if it was occupied */
		dp = devfs_itod(ino);
		KASSERT(dp != NULL, ("DEVFS: No devptr inode %d", ino));
		if (*dp != NULL)
			continue;
		ip = devfs_itor(ino);
		KASSERT(ip != NULL, ("DEVFS: No iptr inode %d", ino));
		if (*ip != 0)
			continue;
		break;
	}

	*dp = dev;
	dev->si_inode = ino;
	if (i > devfs_topino)
		devfs_topino = i;

	devfs_numino++;
	devfs_generation++;

	if (devfs_overflow != NULL || devfs_numino + 100 < NDEVFSINO)
		return;

	/*
	 * Try to allocate overflow table
	 * XXX: we can probably be less panicy these days and a linked
	 * XXX: list of PAGESIZE/PTRSIZE entries might be a better idea.
	 *
	 * XXX: we may be into witness unlove here.
	 */
	n = devfs_noverflowwant;
	ot = malloc(sizeof(*ot) * n, M_DEVFS, M_NOWAIT | M_ZERO);
	if (ot == NULL)
		return;
	or = malloc(sizeof(*or) * n, M_DEVFS, M_NOWAIT | M_ZERO);
	if (or == NULL) {
		free(ot, M_DEVFS);
		return;
	}
	devfs_overflow = ot;
	devfs_refoverflow = or;
	devfs_noverflow = n;
	printf("DEVFS Overflow table with %d entries allocated\n", n);
	return;
}

void
devfs_destroy(struct cdev *dev)
{
	int ino;
	struct cdev **dp;

	ino = dev->si_inode;
	dev->si_inode = 0;
	if (ino == 0)
		return;
	dp = devfs_itod(ino);
	KASSERT(*dp == dev,
	    ("DEVFS: destroying wrong cdev ino %d", ino));
	*dp = NULL;
	devfs_numino--;
	devfs_generation++;
	if (ino < devfs_nextino)
		devfs_nextino = ino;
}
