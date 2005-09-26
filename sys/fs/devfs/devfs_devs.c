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
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <sys/kdb.h>

#include <fs/devfs/devfs.h>
#include <fs/devfs/devfs_int.h>

/*
 * The one true (but secret) list of active devices in the system.
 * Locked by dev_lock()/devmtx
 */
static TAILQ_HEAD(,cdev_priv) cdevp_list = TAILQ_HEAD_INITIALIZER(cdevp_list);

struct unrhdr *devfs_inos;


static MALLOC_DEFINE(M_DEVFS2, "DEVFS2", "DEVFS data 2");
static MALLOC_DEFINE(M_DEVFS3, "DEVFS3", "DEVFS data 3");
static MALLOC_DEFINE(M_CDEVP, "DEVFS1", "DEVFS cdev_priv storage");

static SYSCTL_NODE(_vfs, OID_AUTO, devfs, CTLFLAG_RW, 0, "DEVFS filesystem");

static unsigned devfs_generation;
SYSCTL_UINT(_vfs_devfs, OID_AUTO, generation, CTLFLAG_RD,
	&devfs_generation, 0, "DEVFS generation number");

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
	struct cdev_priv *cdp;

	error = SYSCTL_IN(req, &ud, sizeof (ud));
	if (error)
		return (error);
	if (ud == NODEV)
		return(EINVAL);
/*
	ud ^ devfs_random();
*/
	dev_lock();
	TAILQ_FOREACH(cdp, &cdevp_list, cdp_list)
		if (cdp->cdp_inode == ud)
			break;
	dev_unlock();
	if (cdp == NULL)
		return(ENOENT);
	return(SYSCTL_OUT(req, cdp->cdp_c.si_name, strlen(cdp->cdp_c.si_name) + 1));
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, devname, CTLTYPE_OPAQUE|CTLFLAG_RW|CTLFLAG_ANYBODY,
	NULL, 0, sysctl_devname, "", "devname(3) handler");

SYSCTL_INT(_debug_sizeof, OID_AUTO, cdev, CTLFLAG_RD,
    0, sizeof(struct cdev), "sizeof(struct cdev)");

SYSCTL_INT(_debug_sizeof, OID_AUTO, cdev_priv, CTLFLAG_RD,
    0, sizeof(struct cdev_priv), "sizeof(struct cdev_priv)");

struct cdev *
devfs_alloc(void)
{
	struct cdev_priv *cdp;
	struct cdev *cdev;

	cdp = malloc(sizeof *cdp, M_CDEVP, M_USE_RESERVE | M_ZERO | M_WAITOK);

	cdp->cdp_dirents = &cdp->cdp_dirent0;
	cdp->cdp_dirent0 = NULL;
	cdp->cdp_maxdirent = 0;

	cdev = &cdp->cdp_c;
	cdev->si_priv = cdp;

	cdev->si_name = cdev->__si_namebuf;
	LIST_INIT(&cdev->si_children);
	return (cdev);
}

void
devfs_free(struct cdev *cdev)
{
	struct cdev_priv *cdp;

	cdp = cdev->si_priv;
	if (cdev->si_cred != NULL)
		crfree(cdev->si_cred);
	if (cdp->cdp_inode > 0)
		free_unr(devfs_inos, cdp->cdp_inode);
	if (cdp->cdp_maxdirent > 0) 
		free(cdp->cdp_dirents, M_DEVFS2);
	free(cdp, M_CDEVP);
}

struct devfs_dirent *
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
	de = malloc(i, M_DEVFS3, M_WAITOK | M_ZERO);
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
devfs_vmkdir(struct devfs_mount *dmp, char *name, int namelen, struct devfs_dirent *dotdot, u_int inode)
{
	struct devfs_dirent *dd;
	struct devfs_dirent *de;

	/* Create the new directory */
	dd = devfs_newdirent(name, namelen);
	TAILQ_INIT(&dd->de_dlist);
	dd->de_dirent->d_type = DT_DIR;
	dd->de_mode = 0555;
	dd->de_links = 2;
	dd->de_dir = dd;
	if (inode != 0)
		dd->de_inode = inode;
	else
		dd->de_inode = alloc_unr(devfs_inos);

	/* Create the "." entry in the new directory */
	de = devfs_newdirent(".", 1);
	de->de_dirent->d_type = DT_DIR;
	de->de_flags |= DE_DOT;
	TAILQ_INSERT_TAIL(&dd->de_dlist, de, de_list);
	de->de_dir = dd;

	/* Create the ".." entry in the new directory */
	de = devfs_newdirent("..", 2);
	de->de_dirent->d_type = DT_DIR;
	de->de_flags |= DE_DOTDOT;
	TAILQ_INSERT_TAIL(&dd->de_dlist, de, de_list);
	if (dotdot == NULL) {
		de->de_dir = dd;
	} else {
		de->de_dir = dotdot;
		TAILQ_INSERT_TAIL(&dotdot->de_dlist, dd, de_list);
		dotdot->de_links++;
	}

#ifdef MAC
	mac_create_devfs_directory(dmp->dm_mount, name, namelen, dd);
#endif
	return (dd);
}

void
devfs_delete(struct devfs_mount *dm, struct devfs_dirent *de)
{

	if (de->de_symlink) {
		free(de->de_symlink, M_DEVFS);
		de->de_symlink = NULL;
	}
	if (de->de_vnode != NULL) {
		de->de_vnode->v_data = NULL;
		vgone(de->de_vnode);
		de->de_vnode = NULL;
	}
#ifdef MAC
	mac_destroy_devfsdirent(de);
#endif
	if (de->de_inode > DEVFS_ROOTINO) {
		free_unr(devfs_inos, de->de_inode);
		de->de_inode = 0;
	}
	free(de, M_DEVFS3);
}

/*
 * Called on unmount.
 * Recursively removes the entire tree
 */

static void
devfs_purge(struct devfs_mount *dm, struct devfs_dirent *dd)
{
	struct devfs_dirent *de;

	sx_assert(&dm->dm_lock, SX_XLOCKED);
	for (;;) {
		de = TAILQ_FIRST(&dd->de_dlist);
		if (de == NULL)
			break;
		TAILQ_REMOVE(&dd->de_dlist, de, de_list);
		if (de->de_flags & (DE_DOT|DE_DOTDOT))
			devfs_delete(dm, de);
		else if (de->de_dirent->d_type == DT_DIR)
			devfs_purge(dm, de);
		else 
			devfs_delete(dm, de);
	}
	devfs_delete(dm, dd);
}

/*
 * Each cdev_priv has an array of pointers to devfs_dirent which is indexed
 * by the mount points dm_idx.
 * This function extends the array when necessary, taking into account that
 * the default array is 1 element and not malloc'ed.
 */
static void
devfs_metoo(struct cdev_priv *cdp, struct devfs_mount *dm)
{
	struct devfs_dirent **dep;
	int siz;

	siz = (dm->dm_idx + 1) * sizeof *dep;
	dep = malloc(siz, M_DEVFS2, M_WAITOK | M_ZERO);
	dev_lock();
	if (dm->dm_idx <= cdp->cdp_maxdirent) {
		/* We got raced */
		dev_unlock();
		free(dep, M_DEVFS2);
		return;
	} 
	memcpy(dep, cdp->cdp_dirents, (cdp->cdp_maxdirent + 1) * sizeof *dep);
	if (cdp->cdp_maxdirent > 0)
		free(cdp->cdp_dirents, M_DEVFS2);
	cdp->cdp_dirents = dep;
	/*
	 * XXX: if malloc told us how much we actually got this could
	 * XXX: be optimized.
	 */
	cdp->cdp_maxdirent = dm->dm_idx;
	dev_unlock();
}

static int
devfs_populate_loop(struct devfs_mount *dm, int cleanup)
{
	struct cdev_priv *cdp;
	struct devfs_dirent *de;
	struct devfs_dirent *dd;
	struct cdev *pdev;
	int j;
	char *q, *s;

	sx_assert(&dm->dm_lock, SX_XLOCKED);
	dev_lock();
	TAILQ_FOREACH(cdp, &cdevp_list, cdp_list) {

		KASSERT(cdp->cdp_dirents != NULL, ("NULL cdp_dirents"));

		/*
		 * If we are unmounting, or the device has been destroyed,
		 * clean up our dirent.
		 */
		if ((cleanup || !(cdp->cdp_flags & CDP_ACTIVE)) &&
		    dm->dm_idx <= cdp->cdp_maxdirent &&
		    cdp->cdp_dirents[dm->dm_idx] != NULL) {
			de = cdp->cdp_dirents[dm->dm_idx];
			cdp->cdp_dirents[dm->dm_idx] = NULL;
			cdp->cdp_inuse--;
			KASSERT(cdp == de->de_cdp,
			    ("%s %d %s %p %p", __func__, __LINE__,
			    cdp->cdp_c.si_name, cdp, de->de_cdp));
			KASSERT(de->de_dir != NULL, ("Null de->de_dir"));
			dev_unlock();

			TAILQ_REMOVE(&de->de_dir->de_dlist, de, de_list);
			de->de_cdp = NULL;
			de->de_inode = 0;
			devfs_delete(dm, de);
			return (1);
		}
		/*
	 	 * GC any lingering devices
		 */
		if (!(cdp->cdp_flags & CDP_ACTIVE)) {
			if (cdp->cdp_inuse > 0)
				continue;
			TAILQ_REMOVE(&cdevp_list, cdp, cdp_list);
			dev_unlock();
			dev_rel(&cdp->cdp_c);
			return (1);
		}
		/*
		 * Don't create any new dirents if we are unmounting
		 */
		if (cleanup)
			continue;
		KASSERT((cdp->cdp_flags & CDP_ACTIVE), ("Bogons, I tell ya'!"));

		if (dm->dm_idx <= cdp->cdp_maxdirent &&
		    cdp->cdp_dirents[dm->dm_idx] != NULL) {
			de = cdp->cdp_dirents[dm->dm_idx];
			KASSERT(cdp == de->de_cdp, ("inconsistent cdp"));
			continue;
		}


		cdp->cdp_inuse++;
		dev_unlock();

		if (dm->dm_idx > cdp->cdp_maxdirent)
		        devfs_metoo(cdp, dm);

		dd = dm->dm_rootdir;
		s = cdp->cdp_c.si_name;
		for (;;) {
			for (q = s; *q != '/' && *q != '\0'; q++)
				continue;
			if (*q != '/')
				break;
			de = devfs_find(dd, s, q - s);
			if (de == NULL)
				de = devfs_vmkdir(dm, s, q - s, dd, 0);
			s = q + 1;
			dd = de;
		}

		de = devfs_newdirent(s, q - s);
		if (cdp->cdp_c.si_flags & SI_ALIAS) {
			de->de_uid = 0;
			de->de_gid = 0;
			de->de_mode = 0755;
			de->de_dirent->d_type = DT_LNK;
			pdev = cdp->cdp_c.si_parent;
			j = strlen(pdev->si_name) + 1;
			de->de_symlink = malloc(j, M_DEVFS, M_WAITOK);
			bcopy(pdev->si_name, de->de_symlink, j);
		} else {
			de->de_uid = cdp->cdp_c.si_uid;
			de->de_gid = cdp->cdp_c.si_gid;
			de->de_mode = cdp->cdp_c.si_mode;
			de->de_dirent->d_type = DT_CHR;
		}
		de->de_inode = cdp->cdp_inode;
		de->de_cdp = cdp;
#ifdef MAC
		mac_create_devfs_device(cdp->cdp_c.si_cred, dm->dm_mount,
		    &cdp->cdp_c, de);
#endif
		de->de_dir = dd;
		TAILQ_INSERT_TAIL(&dd->de_dlist, de, de_list);
		devfs_rules_apply(dm, de);
		dev_lock();
		/* XXX: could check that cdp is still active here */
		KASSERT(cdp->cdp_dirents[dm->dm_idx] == NULL,
		    ("%s %d\n", __func__, __LINE__));
		cdp->cdp_dirents[dm->dm_idx] = de;
		KASSERT(de->de_cdp != (void *)0xdeadc0de,
		    ("%s %d\n", __func__, __LINE__));
		dev_unlock();
		return (1);
	}
	dev_unlock();
	return (0);
}

void
devfs_populate(struct devfs_mount *dm)
{

	sx_assert(&dm->dm_lock, SX_XLOCKED);
	if (dm->dm_generation == devfs_generation)
		return;
	while (devfs_populate_loop(dm, 0))
		continue;
	dm->dm_generation = devfs_generation;
}

void
devfs_cleanup(struct devfs_mount *dm)
{

	sx_assert(&dm->dm_lock, SX_XLOCKED);
	while (devfs_populate_loop(dm, 1))
		continue;
	devfs_purge(dm, dm->dm_rootdir);
}

/*
 * devfs_create() and devfs_destroy() are called from kern_conf.c and
 * in both cases the devlock() mutex is held, so no further locking
 * is necesary and no sleeping allowed.
 */

void
devfs_create(struct cdev *dev)
{
	struct cdev_priv *cdp;

	mtx_assert(&devmtx, MA_OWNED);
	cdp = dev->si_priv;
	cdp->cdp_flags |= CDP_ACTIVE;
	cdp->cdp_inode = alloc_unrl(devfs_inos);
	dev_refl(dev);
	TAILQ_INSERT_TAIL(&cdevp_list, cdp, cdp_list);
	devfs_generation++;
}

void
devfs_destroy(struct cdev *dev)
{
	struct cdev_priv *cdp;

	mtx_assert(&devmtx, MA_OWNED);
	cdp = dev->si_priv;
	cdp->cdp_flags &= ~CDP_ACTIVE;
	devfs_generation++;
}

static void
devfs_devs_init(void *junk __unused)
{

	devfs_inos = new_unrhdr(DEVFS_ROOTINO + 1, INT_MAX, &devmtx);
}

SYSINIT(devfs_devs, SI_SUB_DEVFS, SI_ORDER_FIRST, devfs_devs_init, NULL);
