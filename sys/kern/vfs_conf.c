/*-
 * Copyright (c) 1999 Michael Smith
 * All rights reserved.
 * Copyright (c) 1999 Poul-Henning Kamp
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
 *	$FreeBSD: src/sys/kern/vfs_conf.c,v 1.49.2.1 2000/05/22 17:26:36 msmith Exp $
 */

/*
 * Locate and mount the root filesystem.
 *
 * The root filesystem is detailed in the kernel environment variable
 * vfs.root.mountfrom, which is expected to be in the general format
 *
 * <vfsname>:[<path>]
 * vfsname   := the name of a VFS known to the kernel and capable
 *              of being mounted as root
 * path      := disk device name or other data used by the filesystem
 *              to locate its physical store
 *
 */

#include "opt_rootdevname.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/reboot.h>
#include <sys/diskslice.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/cons.h>

MALLOC_DEFINE(M_MOUNT, "mount", "vfs mount structure");

#define ROOTNAME	"root_device"

struct vnode	*rootvnode;

/* 
 * The root specifiers we will try if RB_CDROM is specified.
 */
static char *cdrom_rootdevnames[] = {
	"cd9660:cd0a",
	"cd9660:acd0a",
	"cd9660:wcd0a",
	NULL
};

static void	vfs_mountroot(void *junk);
static int	vfs_mountroot_try(char *mountfrom);
static int	vfs_mountroot_ask(void);
static void	gets(char *cp);

/* legacy find-root code */
char		*rootdevnames[2] = {NULL, NULL};
static int	setrootbyname(char *name);

SYSINIT(mountroot, SI_SUB_MOUNT_ROOT, SI_ORDER_SECOND, vfs_mountroot, NULL);
	
/*
 * Find and mount the root filesystem
 */
static void
vfs_mountroot(void *junk)
{
	int		i;
	
	/* 
	 * The root filesystem information is compiled in, and we are
	 * booted with instructions to use it.
	 */
#ifdef ROOTDEVNAME
	if ((boothowto & RB_DFLTROOT) && 
	    !vfs_mountroot_try(ROOTDEVNAME))
		return;
#endif
	/* 
	 * We are booted with instructions to prompt for the root filesystem,
	 * or to use the compiled-in default when it doesn't exist.
	 */
	if (boothowto & (RB_DFLTROOT | RB_ASKNAME)) {
		if (!vfs_mountroot_ask())
			return;
	}

	/*
	 * We've been given the generic "use CDROM as root" flag.  This is
	 * necessary because one media may be used in many different
	 * devices, so we need to search for them.
	 */
	if (boothowto & RB_CDROM) {
		for (i = 0; cdrom_rootdevnames[i] != NULL; i++) {
			if (!vfs_mountroot_try(cdrom_rootdevnames[i]))
				return;
		}
	}

	/*
	 * Try to use the value read by the loader from /etc/fstab, or
	 * supplied via some other means.  This is the preferred 
	 * mechanism.
	 */
	if (!vfs_mountroot_try(getenv("vfs.root.mountfrom")))
		return;

	/* 
	 * Try values that may have been computed by the machine-dependant
	 * legacy code.
	 */
	if (!vfs_mountroot_try(rootdevnames[0]))
		return;
	if (!vfs_mountroot_try(rootdevnames[1]))
		return;

	/*
	 * If we have a compiled-in default, and haven't already tried it, try
	 * it now.
	 */
#ifdef ROOTDEVNAME
	if (!(boothowto & RB_DFLTROOT))
		if (!vfs_mountroot_try(ROOTDEVNAME))
			return;
#endif

	/* 
	 * Everything so far has failed, prompt on the console if we haven't
	 * already tried that.
	 */
	if (!(boothowto & (RB_DFLTROOT | RB_ASKNAME)) && !vfs_mountroot_ask())
		return;
	panic("Root mount failed, startup aborted.");
}

/*
 * Mount (mountfrom) as the root filesystem.
 */
static int
vfs_mountroot_try(char *mountfrom)
{
        struct mount	*mp;
	char		*vfsname, *path;
	int		error;
	char		patt[32];
	int		s;

	vfsname = NULL;
	path    = NULL;
	mp      = NULL;
	error   = EINVAL;

	if (mountfrom == NULL)
		return(error);		/* don't complain */

	s = splcam();			/* Overkill, but annoying without it */
	printf("Mounting root from %s\n", mountfrom);
	splx(s);

	/* parse vfs name and path */
	vfsname = malloc(MFSNAMELEN, M_MOUNT, M_WAITOK);
	path = malloc(MNAMELEN, M_MOUNT, M_WAITOK);
	vfsname[0] = path[0] = 0;
	sprintf(patt, "%%%d[a-z0-9]:%%%ds", MFSNAMELEN, MNAMELEN);
	if (sscanf(mountfrom, patt, vfsname, path) < 1)
		goto done;

	/* allocate a root mount */
	error = vfs_rootmountalloc(vfsname, path[0] != 0 ? path : ROOTNAME,
				   &mp);
	if (error != 0) {
		printf("Can't allocate root mount for filesystem '%s': %d\n",
		       vfsname, error);
		goto done;
	}
	mp->mnt_flag |= MNT_ROOTFS;

	/* do our best to set rootdev */
	if ((path[0] != 0) && setrootbyname(path))
		printf("setrootbyname failed\n");

	/* If the root device is a type "memory disk", mount RW */
	if (rootdev != NODEV && devsw(rootdev) &&
	    (devsw(rootdev)->d_flags & D_MEMDISK))
		mp->mnt_flag &= ~MNT_RDONLY;

	error = VFS_MOUNT(mp, NULL, NULL, NULL, curproc);

done:
	if (vfsname != NULL)
		free(vfsname, M_MOUNT);
	if (path != NULL)
		free(path, M_MOUNT);
	if (error != 0) {
		if (mp != NULL) {
			vfs_unbusy(mp, curproc);
			free(mp, M_MOUNT);
		}
		printf("Root mount failed: %d\n", error);
	} else {

		/* register with list of mounted filesystems */
		simple_lock(&mountlist_slock);
		TAILQ_INSERT_HEAD(&mountlist, mp, mnt_list);
		simple_unlock(&mountlist_slock);

		/* sanity check system clock against root filesystem timestamp */
		inittodr(mp->mnt_time);
		vfs_unbusy(mp, curproc);
	}
	return(error);
}

/*
 * Spin prompting on the console for a suitable root filesystem
 */
static int
vfs_mountroot_ask(void)
{
	char name[128];
	int i;
	dev_t dev;

	for(;;) {
		printf("\nManual root filesystem specification:\n");
		printf("  <fstype>:<device>  Mount <device> using filesystem <fstype>\n");
		printf("                       eg. ufs:/dev/da0s1a\n");
		printf("  ?                  List valid disk boot devices\n");
		printf("  <empty line>       Abort manual input\n");
		printf("\nmountroot> ");
		gets(name);
		if (name[0] == 0)
			return(1);
		if (name[0] == '?') {
			printf("Possibly valid devices for 'ufs' root:\n");
			for (i = 0; i < NUMCDEVSW; i++) {
				dev = makedev(i, 0);
				if (devsw(dev) != NULL)
					printf(" \"%s\"", devsw(dev)->d_name);
			}
			printf("\n");
			continue;
		}
		if (!vfs_mountroot_try(name))
			return(0);
	}
}

static void
gets(char *cp)
{
	char *lp;
	int c;

	lp = cp;
	for (;;) {
		printf("%c", c = cngetc() & 0177);
		switch (c) {
		case -1:
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
		case '\177':
			if (lp > cp) {
				printf(" \b");
				lp--;
			}
			continue;
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;
		case '@':
		case 'u' & 037:
			lp = cp;
			printf("%c", '\n');
			continue;
		default:
			*lp++ = c;
		}
	}
}

/*
 * Set rootdev to match (name), given that we expect it to
 * refer to a disk-like device.
 */
static int
setrootbyname(char *name)
{
	char *cp;
	int cd, unit, slice, part;
	dev_t dev;

	slice = 0;
	part = 0;
	cp = rindex(name, '/');
	if (cp != NULL) {
		name = cp + 1;
	}
	cp = name;
	while (cp != '\0' && (*cp < '0' || *cp > '9'))
		cp++;
	if (cp == name) {
		printf("missing device name\n");
		return(1);
	}
	if (*cp == '\0') {
		printf("missing unit number\n");
		return(1);
	}
	unit = *cp - '0';
	*cp++ = '\0';
	for (cd = 0; cd < NUMCDEVSW; cd++) {
		dev = makedev(cd, 0);
		if (devsw(dev) != NULL &&
		    strcmp(devsw(dev)->d_name, name) == 0)
			goto gotit;
	}
	printf("no such device '%s'\n", name);
	return (2);
gotit:
	while (*cp >= '0' && *cp <= '9')
		unit += 10 * unit + *cp++ - '0';
	if (*cp == 's' && cp[1] >= '0' && cp[1] <= '9') {
		slice = cp[1] - '0' + 1;
		cp += 2;
	}
	if (*cp >= 'a' && *cp <= 'h') {
		part = *cp - 'a';
		cp++;
	}
	if (*cp != '\0') {
		printf("junk after name\n");
		return (1);
	}
	rootdev = makedev(cd, dkmakeminor(unit, slice, part));
	return 0;
}

