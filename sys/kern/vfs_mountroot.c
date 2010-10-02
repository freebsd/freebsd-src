/*-
 * Copyright (c) 1999-2004 Poul-Henning Kamp
 * Copyright (c) 1999 Michael Smith
 * Copyright (c) 1989, 1993
 *      The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/reboot.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <vm/uma.h>

#include <geom/geom.h>

#include <machine/stdarg.h>

#include "opt_rootdevname.h"

#define	ROOTNAME		"root_device"

static int	vfs_mountroot_ask(void);
static int	vfs_mountroot_try(const char *mountfrom, const char *options);

/*
 * The vnode of the system's root (/ in the filesystem, without chroot
 * active.)
 */
struct vnode	*rootvnode;

/*
 * The root filesystem is detailed in the kernel environment variable
 * vfs.root.mountfrom, which is expected to be in the general format
 *
 * <vfsname>:[<path>][	<vfsname>:[<path>] ...]
 * vfsname   := the name of a VFS known to the kernel and capable
 *              of being mounted as root
 * path      := disk device name or other data used by the filesystem
 *              to locate its physical store
 *
 * If the environment variable vfs.root.mountfrom is a space separated list,
 * each list element is tried in turn and the root filesystem will be mounted
 * from the first one that suceeds.
 *
 * The environment variable vfs.root.mountfrom.options is a comma delimited
 * set of string mount options.  These mount options must be parseable
 * by nmount() in the kernel.
 */

/*
 * The root specifiers we will try if RB_CDROM is specified.
 */
static char *cdrom_rootdevnames[] = {
	"cd9660:cd0",
	"cd9660:acd0",
	NULL
};

/* legacy find-root code */
char		*rootdevnames[2] = {NULL, NULL};
#ifndef ROOTDEVNAME
#  define ROOTDEVNAME NULL
#endif
static const char	*ctrootdevname = ROOTDEVNAME;

struct root_hold_token {
	const char			*who;
	LIST_ENTRY(root_hold_token)	list;
};

static LIST_HEAD(, root_hold_token)	root_holds =
    LIST_HEAD_INITIALIZER(root_holds);

static int root_mount_complete;

struct root_hold_token *
root_mount_hold(const char *identifier)
{
	struct root_hold_token *h;

	if (root_mounted())
		return (NULL);

	h = malloc(sizeof *h, M_DEVBUF, M_ZERO | M_WAITOK);
	h->who = identifier;
	mtx_lock(&mountlist_mtx);
	LIST_INSERT_HEAD(&root_holds, h, list);
	mtx_unlock(&mountlist_mtx);
	return (h);
}

void
root_mount_rel(struct root_hold_token *h)
{

	if (h == NULL)
		return;
	mtx_lock(&mountlist_mtx);
	LIST_REMOVE(h, list);
	wakeup(&root_holds);
	mtx_unlock(&mountlist_mtx);
	free(h, M_DEVBUF);
}

static void
root_mount_prepare(void)
{
	struct root_hold_token *h;
	struct timeval lastfail;
	int curfail = 0;

	for (;;) {
		DROP_GIANT();
		g_waitidle();
		PICKUP_GIANT();
		mtx_lock(&mountlist_mtx);
		if (LIST_EMPTY(&root_holds)) {
			mtx_unlock(&mountlist_mtx);
			break;
		}
		if (ppsratecheck(&lastfail, &curfail, 1)) {
			printf("Root mount waiting for:");
			LIST_FOREACH(h, &root_holds, list)
				printf(" %s", h->who);
			printf("\n");
		}
		msleep(&root_holds, &mountlist_mtx, PZERO | PDROP, "roothold",
		    hz);
	}
}

static void
root_mount_done(void)
{

	/* Keep prison0's root in sync with the global rootvnode. */
	mtx_lock(&prison0.pr_mtx);
	prison0.pr_root = rootvnode;
	vref(prison0.pr_root);
	mtx_unlock(&prison0.pr_mtx);
	/*
	 * Use a mutex to prevent the wakeup being missed and waiting for
	 * an extra 1 second sleep.
	 */
	mtx_lock(&mountlist_mtx);
	root_mount_complete = 1;
	wakeup(&root_mount_complete);
	mtx_unlock(&mountlist_mtx);
}

int
root_mounted(void)
{

	/* No mutex is acquired here because int stores are atomic. */
	return (root_mount_complete);
}

void
root_mount_wait(void)
{

	/*
	 * Panic on an obvious deadlock - the function can't be called from
	 * a thread which is doing the whole SYSINIT stuff.
	 */
	KASSERT(curthread->td_proc->p_pid != 0,
	    ("root_mount_wait: cannot be called from the swapper thread"));
	mtx_lock(&mountlist_mtx);
	while (!root_mount_complete) {
		msleep(&root_mount_complete, &mountlist_mtx, PZERO, "rootwait",
		    hz);
	}
	mtx_unlock(&mountlist_mtx);
}

static void
set_rootvnode(void)
{
	struct proc *p;

	if (VFS_ROOT(TAILQ_FIRST(&mountlist), LK_EXCLUSIVE, &rootvnode))
		panic("Cannot find root vnode");

	VOP_UNLOCK(rootvnode, 0);

	p = curthread->td_proc;
	FILEDESC_XLOCK(p->p_fd);

	if (p->p_fd->fd_cdir != NULL)
		vrele(p->p_fd->fd_cdir);
	p->p_fd->fd_cdir = rootvnode;
	VREF(rootvnode);

	if (p->p_fd->fd_rdir != NULL)
		vrele(p->p_fd->fd_rdir);
	p->p_fd->fd_rdir = rootvnode;
	VREF(rootvnode);

	FILEDESC_XUNLOCK(p->p_fd);

	EVENTHANDLER_INVOKE(mountroot);
}

static void
devfs_first(void)
{
	struct thread *td = curthread;
	struct vfsoptlist *opts;
	struct vfsconf *vfsp;
	struct mount *mp = NULL;
	int error;

	vfsp = vfs_byname("devfs");
	KASSERT(vfsp != NULL, ("Could not find devfs by name"));
	if (vfsp == NULL)
		return;

	mp = vfs_mount_alloc(NULLVP, vfsp, "/dev", td->td_ucred);

	error = VFS_MOUNT(mp);
	KASSERT(error == 0, ("VFS_MOUNT(devfs) failed %d", error));
	if (error)
		return;

	opts = malloc(sizeof(struct vfsoptlist), M_MOUNT, M_WAITOK);
	TAILQ_INIT(opts);
	mp->mnt_opt = opts;

	mtx_lock(&mountlist_mtx);
	TAILQ_INSERT_HEAD(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);

	set_rootvnode();

	error = kern_symlink(td, "/", "dev", UIO_SYSSPACE);
	if (error)
		printf("kern_symlink /dev -> / returns %d\n", error);
}

static void
devfs_fixup(struct thread *td)
{
	struct nameidata nd;
	struct vnode *vp, *dvp;
	struct mount *mp;
	int error;

	/* Remove our devfs mount from the mountlist and purge the cache */
	mtx_lock(&mountlist_mtx);
	mp = TAILQ_FIRST(&mountlist);
	TAILQ_REMOVE(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);
	cache_purgevfs(mp);

	VFS_ROOT(mp, LK_EXCLUSIVE, &dvp);
	VI_LOCK(dvp);
	dvp->v_iflag &= ~VI_MOUNT;
	VI_UNLOCK(dvp);
	dvp->v_mountedhere = NULL;

	/* Set up the real rootvnode, and purge the cache */
	TAILQ_FIRST(&mountlist)->mnt_vnodecovered = NULL;
	set_rootvnode();
	cache_purgevfs(rootvnode->v_mount);

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, "/dev", td);
	error = namei(&nd);
	if (error) {
		printf("Lookup of /dev for devfs, error: %d\n", error);
		vput(dvp);
		vfs_unbusy(mp);
		return;
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	if (vp->v_type != VDIR) {
		printf("/dev is not a directory\n");
		vput(dvp);
		vput(vp);
		vfs_unbusy(mp);
		return;
	}
	error = vinvalbuf(vp, V_SAVE, 0, 0);
	if (error) {
		printf("vinvalbuf() of /dev failed, error: %d\n", error);
		vput(dvp);
		vput(vp);
		vfs_unbusy(mp);
		return;
	}
	cache_purge(vp);
	mp->mnt_vnodecovered = vp;
	vp->v_mountedhere = mp;
	mtx_lock(&mountlist_mtx);
	TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mtx_unlock(&mountlist_mtx);
	VOP_UNLOCK(vp, 0);
	vput(dvp);
	vfs_unbusy(mp);

	/* Unlink the no longer needed /dev/dev -> / symlink */
	error = kern_unlink(td, "/dev/dev", UIO_SYSSPACE);
	if (error)
		printf("kern_unlink of /dev/dev failed, error: %d\n", error);
}

void
vfs_mountroot(void)
{
	char *cp, *cpt, *options, *tmpdev;
	int error, i, asked = 0;

	options = NULL;

	root_mount_prepare();

	devfs_first();

	/*
	 * We are booted with instructions to prompt for the root filesystem.
	 */
	if (boothowto & RB_ASKNAME) {
		if (!vfs_mountroot_ask())
			goto mounted;
		asked = 1;
	}

	options = getenv("vfs.root.mountfrom.options");

	/*
	 * The root filesystem information is compiled in, and we are
	 * booted with instructions to use it.
	 */
	if (ctrootdevname != NULL && (boothowto & RB_DFLTROOT)) {
		if (!vfs_mountroot_try(ctrootdevname, options))
			goto mounted;
		ctrootdevname = NULL;
	}

	/*
	 * We've been given the generic "use CDROM as root" flag.  This is
	 * necessary because one media may be used in many different
	 * devices, so we need to search for them.
	 */
	if (boothowto & RB_CDROM) {
		for (i = 0; cdrom_rootdevnames[i] != NULL; i++) {
			if (!vfs_mountroot_try(cdrom_rootdevnames[i], options))
				goto mounted;
		}
	}

	/*
	 * Try to use the value read by the loader from /etc/fstab, or
	 * supplied via some other means.  This is the preferred
	 * mechanism.
	 */
	cp = getenv("vfs.root.mountfrom");
	if (cp != NULL) {
		cpt = cp;
		while ((tmpdev = strsep(&cpt, " \t")) != NULL) {
			error = vfs_mountroot_try(tmpdev, options);
			if (error == 0) {
				freeenv(cp);
				goto mounted;
			}
		}
		freeenv(cp);
	}

	/*
	 * Try values that may have been computed by code during boot
	 */
	if (!vfs_mountroot_try(rootdevnames[0], options))
		goto mounted;
	if (!vfs_mountroot_try(rootdevnames[1], options))
		goto mounted;

	/*
	 * If we (still) have a compiled-in default, try it.
	 */
	if (ctrootdevname != NULL)
		if (!vfs_mountroot_try(ctrootdevname, options))
			goto mounted;
	/*
	 * Everything so far has failed, prompt on the console if we haven't
	 * already tried that.
	 */
	if (!asked)
		if (!vfs_mountroot_ask())
			goto mounted;

	panic("Root mount failed, startup aborted.");

mounted:
	root_mount_done();
	freeenv(options);
}

static struct mntarg *
parse_mountroot_options(struct mntarg *ma, const char *options)
{
	char *p;
	char *name, *name_arg;
	char *val, *val_arg;
	char *opts;

	if (options == NULL || options[0] == '\0')
		return (ma);

	p = opts = strdup(options, M_MOUNT);
	if (opts == NULL) {
		return (ma);
	} 

	while((name = strsep(&p, ",")) != NULL) {
		if (name[0] == '\0')
			break;

		val = strchr(name, '=');
		if (val != NULL) {
			*val = '\0';
			++val;
		}
		if( strcmp(name, "rw") == 0 ||
		    strcmp(name, "noro") == 0) {
			/*
			 * The first time we mount the root file system,
			 * we need to mount 'ro', so We need to ignore
			 * 'rw' and 'noro' mount options.
			 */
			continue;
		}
		name_arg = strdup(name, M_MOUNT);
		val_arg = NULL;
		if (val != NULL) 
			val_arg = strdup(val, M_MOUNT);

		ma = mount_arg(ma, name_arg, val_arg,
		    (val_arg != NULL ? -1 : 0));
	}
	free(opts, M_MOUNT);
	return (ma);
}

/*
 * Mount (mountfrom) as the root filesystem.
 */
static int
vfs_mountroot_try(const char *mountfrom, const char *options)
{
	struct mount	*mp;
	struct mntarg	*ma;
	char		*vfsname, *path;
	time_t		timebase;
	int		error;
	char		patt[32];
	char		errmsg[255];

	vfsname = NULL;
	path    = NULL;
	mp      = NULL;
	ma	= NULL;
	error   = EINVAL;
	bzero(errmsg, sizeof(errmsg));

	if (mountfrom == NULL)
		return (error);		/* don't complain */
	printf("Trying to mount root from %s\n", mountfrom);

	/* parse vfs name and path */
	vfsname = malloc(MFSNAMELEN, M_MOUNT, M_WAITOK);
	path = malloc(MNAMELEN, M_MOUNT, M_WAITOK);
	vfsname[0] = path[0] = 0;
	sprintf(patt, "%%%d[a-z0-9]:%%%ds", MFSNAMELEN, MNAMELEN);
	if (sscanf(mountfrom, patt, vfsname, path) < 1)
		goto out;

	if (path[0] == '\0')
		strcpy(path, ROOTNAME);

	ma = mount_arg(ma, "fstype", vfsname, -1);
	ma = mount_arg(ma, "fspath", "/", -1);
	ma = mount_arg(ma, "from", path, -1);
	ma = mount_arg(ma, "errmsg", errmsg, sizeof(errmsg));
	ma = mount_arg(ma, "ro", NULL, 0);
	ma = parse_mountroot_options(ma, options);
	error = kernel_mount(ma, MNT_ROOTFS);

	if (error == 0) {
		/*
		 * We mount devfs prior to mounting the / FS, so the first
		 * entry will typically be devfs.
		 */
		mp = TAILQ_FIRST(&mountlist);
		KASSERT(mp != NULL, ("%s: mountlist is empty", __func__));

		/*
		 * Iterate over all currently mounted file systems and use
		 * the time stamp found to check and/or initialize the RTC.
		 * Typically devfs has no time stamp and the only other FS
		 * is the actual / FS.
		 * Call inittodr() only once and pass it the largest of the
		 * timestamps we encounter.
		 */
		timebase = 0;
		do {
			if (mp->mnt_time > timebase)
				timebase = mp->mnt_time;
			mp = TAILQ_NEXT(mp, mnt_list);
		} while (mp != NULL);
		inittodr(timebase);

		devfs_fixup(curthread);
	}

	if (error != 0 ) {
		printf("ROOT MOUNT ERROR: %s\n", errmsg);
		printf("If you have invalid mount options, reboot, and ");
		printf("first try the following from\n");
		printf("the loader prompt:\n\n");
		printf("     set vfs.root.mountfrom.options=rw\n\n");
		printf("and then remove invalid mount options from ");
		printf("/etc/fstab.\n\n");
	}
out:
	free(path, M_MOUNT);
	free(vfsname, M_MOUNT);
	return (error);
}

static int
vfs_mountroot_ask(void)
{
	char name[128];
	char *mountfrom;
	char *options;

	for(;;) {
		printf("Loader variables:\n");
		printf("vfs.root.mountfrom=");
		mountfrom = getenv("vfs.root.mountfrom");
		if (mountfrom != NULL) {
			printf("%s", mountfrom);
		}
		printf("\n");
		printf("vfs.root.mountfrom.options=");
		options = getenv("vfs.root.mountfrom.options");
		if (options != NULL) {
			printf("%s", options);
		}
		printf("\n");
		freeenv(mountfrom);
		freeenv(options);
		printf("\nManual root filesystem specification:\n");
		printf("  <fstype>:<device>  Mount <device> using filesystem <fstype>\n");
		printf("                       eg. zfs:tank\n");
		printf("                       eg. ufs:/dev/da0s1a\n");
		printf("                       eg. cd9660:/dev/acd0\n");
		printf("                       This is equivalent to: ");
		printf("mount -t cd9660 /dev/acd0 /\n"); 
		printf("\n");
		printf("  ?                  List valid disk boot devices\n");
		printf("  <empty line>       Abort manual input\n");
		printf("\nmountroot> ");
		gets(name, sizeof(name), 1);
		if (name[0] == '\0')
			return (1);
		if (name[0] == '?') {
			printf("\nList of GEOM managed disk devices:\n  ");
			g_dev_print();
			continue;
		}
		if (!vfs_mountroot_try(name, NULL))
			return (0);
	}
}
