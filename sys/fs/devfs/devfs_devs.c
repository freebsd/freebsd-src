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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/dirent.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/eventhandler.h>
#include <sys/ctype.h>

#define DEVFS_INTERN
#include <fs/devfs/devfs.h>

struct devfs_dirent *
devfs_newdirent(char *name, int namelen)
{
	int i;
	struct devfs_dirent *de;
	struct dirent d;

	d.d_namlen = namelen;
	i = sizeof (*de) + GENERIC_DIRSIZ(&d);
	MALLOC(de, struct devfs_dirent *, i, M_DEVFS, M_WAITOK);
	bzero(de, i);
	de->de_dirent = (struct dirent *)(de + 1);
	de->de_dirent->d_namlen = namelen;
	de->de_dirent->d_reclen = GENERIC_DIRSIZ(&d);
	bcopy(name, de->de_dirent->d_name, namelen + 1);
	nanotime(&de->de_ctime);
	return (de);
}

struct devfs_dir *
devfs_vmkdir(void)
{
	struct devfs_dir *dd;
	struct devfs_dirent *de;

	MALLOC(dd, struct devfs_dir *, sizeof(*dd), M_DEVFS, M_WAITOK);
	bzero(dd, sizeof(*dd));
	TAILQ_INIT(&dd->dd_list);

	de = devfs_newdirent(".", 1);
	de->de_dirent->d_type = DT_DIR;
	TAILQ_INSERT_TAIL(&dd->dd_list, de, de_list);
	de = TAILQ_FIRST(&dd->dd_list);
	de->de_mode = 0755;
	return (dd);
}

void
devfs_delete(struct devfs_dir *dd, struct devfs_dirent *de)
{

	if (de->de_symlink) {
		FREE(de->de_symlink, M_DEVFS);
		de->de_symlink = NULL;
	}
	if (de->de_vnode) {
		de->de_vnode->v_data = NULL;
		vdrop(de->de_vnode);
	}
	TAILQ_REMOVE(&dd->dd_list, de, de_list);
	FREE(de, M_DEVFS);
}

void
devfs_purge(struct devfs_dir *dd)
{
	struct devfs_dirent *de;

	for (;;) {
		de = TAILQ_FIRST(&dd->dd_list);
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
	struct devfs_dir *dd;
	struct devfs_dirent *de;
	char *q, *s;

	while (dm->dm_generation != devfs_generation) {
		dm->dm_generation = devfs_generation;
		for (i = 0; i < NDEVINO; i++) {
			dev = devfs_inot[i];
			de = dm->dm_dirent[i];
			if (dev == NULL && de != NULL) {
#if 0
				printf("Del ino%d %s\n", i, de->de_dirent->d_name);
#endif
				dd = de->de_dir;
				dm->dm_dirent[i] = NULL;
				TAILQ_REMOVE(&dd->dd_list, de, de_list);
				if (de->de_vnode) {
					de->de_vnode->v_data = NULL;
					vdrop(de->de_vnode);
				}
				FREE(de, M_DEVFS);
				continue;
			}
			if (dev == NULL)
				continue;
			if (de != NULL)
				continue;
			dd = dm->dm_basedir;
			s = dev->si_name;
			for (q = s; *q != '/' && *q != '\0'; q++)
				continue;
			if (*q == '/') {
				continue;
			}
			de = devfs_newdirent(s, q - s);
			if (dev->si_flags & SI_ALIAS) {
				de->de_inode = dm->dm_inode++;
				de->de_uid = 0;
				de->de_gid = 0;
				de->de_mode = 0642;
				de->de_dirent->d_type = DT_LNK;
				pdev = dev->si_drv1;
				j = strlen(pdev->si_name) + 1;
				MALLOC(de->de_symlink, char *, j, M_DEVFS, M_WAITOK);
				bcopy(pdev->si_name, de->de_symlink, j);
			} else {
				de->de_inode = i;
				de->de_uid = dev->si_uid;
				de->de_gid = dev->si_gid;
				de->de_mode = dev->si_mode;
				de->de_dirent->d_type = DT_CHR;
				dm->dm_dirent[i] = de;
			}
			TAILQ_INSERT_TAIL(&dd->dd_list, de, de_list);
#if 0
			printf("Add ino%d %s\n", i, dev->si_name);
#endif
		}
	}
	return (0);
}

dev_t devfs_inot[NDEVINO];
int devfs_nino = 3;
unsigned devfs_generation;

static void
devfs_create(dev_t dev)
{
	if (dev->si_inode == 0 && devfs_nino < NDEVINO)
		dev->si_inode = devfs_nino++;
	if (dev->si_inode == 0) {
		printf("NDEVINO too small\n");
		return;
	}
	devfs_inot[dev->si_inode] = dev;
	devfs_generation++;
}

static void
devfs_remove(dev_t dev)
{
	devfs_inot[dev->si_inode] = NULL;
	devfs_generation++;
}

devfs_create_t *devfs_create_hook = devfs_create;
devfs_remove_t *devfs_remove_hook = devfs_remove;

int
devfs_stdclone(char *name, char **namep, char *stem, int *unit)
{
	int u, i;

	if (bcmp(stem, name, strlen(stem)) != 0)
		return (0);
	i = strlen(stem);
	if (!isdigit(name[i]))
		return (0);
	u = 0;
	while (isdigit(name[i])) {
		u *= 10;
		u += name[i++] - '0';
	}
	*unit = u;
	if (namep)
		*namep = &name[i];
	if (name[i]) 
		return (2);
	return (1);
}
