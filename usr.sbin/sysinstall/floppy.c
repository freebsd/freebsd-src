/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $FreeBSD$
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 * Copyright (c) 1995
 * 	Gary J Palmer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* These routines deal with getting things off of floppy media */

#include "sysinstall.h"
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>
#include <grp.h>

#define MSDOSFS
#include <sys/mount.h>
#include <fs/msdosfs/msdosfsmount.h>
#undef MSDOSFS

#include <ufs/ufs/ufsmount.h>
static Boolean floppyMounted;

char *distWanted;
static char mountpoint[] = "/dist";

Boolean
mediaInitFloppy(Device *dev)
{
    struct msdosfs_args dosargs;
    struct ufs_args u_args;
    char *mp;
#ifdef PC98
    char fddev[24];
#endif

    if (floppyMounted)
	return TRUE;

    mp = dev->private ? (char *)dev->private : mountpoint;
    if (Mkdir(mp)) {
	msgConfirm("Unable to make %s directory mountpoint for %s!", mp, dev->devname);
	return FALSE;
    }

    msgDebug("Init floppy called for %s distribution.\n", distWanted ? distWanted : "some");

    if (!variable_get(VAR_NONINTERACTIVE)) {
	if (!distWanted)
	    msgConfirm("Please insert floppy in %s", dev->description);
	else
	    msgConfirm("Please insert floppy containing %s in %s",
			distWanted, dev->description);
    }

    memset(&dosargs, 0, sizeof dosargs);
#ifdef PC98
    dosargs.fspec = fddev;
#else
    dosargs.fspec = dev->devname;
#endif
    dosargs.uid = dosargs.gid = 0;
    dosargs.mask = 0777;

    memset(&u_args, 0, sizeof(u_args));
#ifdef PC98
    u_args.fspec = fddev;
#else
    u_args.fspec = dev->devname;
#endif

#ifdef PC98
    sprintf(fddev, "%s.1200", dev->devname);
    if (mount("msdosfs", mp, MNT_RDONLY, (caddr_t)&dosargs) != -1)
	goto success;
    if (mount("ufs", mp, MNT_RDONLY, (caddr_t)&u_args) != -1)
	goto success;

    sprintf(fddev, "%s.1232", dev->devname);
    if (mount("msdosfs", mp, MNT_RDONLY, (caddr_t)&dosargs) != -1)
	goto success;
    if (mount("ufs", mp, MNT_RDONLY, (caddr_t)&u_args) != -1)
	goto success;

    sprintf(fddev, "%s.1440", dev->devname);
    if (mount("msdosfs", mp, MNT_RDONLY, (caddr_t)&dosargs) != -1)
	goto success;
    if (mount("ufs", mp, MNT_RDONLY, (caddr_t)&u_args) != -1)
	goto success;
#else
    if (mount("msdosfs", mp, MNT_RDONLY, (caddr_t)&dosargs) != -1)
	goto success;
    if (mount("ufs", mp, MNT_RDONLY, (caddr_t)&u_args) != -1)
	goto success;
#endif /* PC98 */

    msgConfirm("Error mounting floppy %s (%s) on %s : %s",
	       dev->name, dev->devname, mp, strerror(errno));
    return FALSE;

success:
    floppyMounted = TRUE;
    distWanted = NULL;
    return TRUE;
}

FILE *
mediaGetFloppy(Device *dev, char *file, Boolean probe)
{
    char	buf[PATH_MAX], *mp;
    FILE	*fp;
    int		nretries = 5;

    /*
     * floppies don't use mediaGenericGet() because it's too expensive
     * to speculatively open files on a floppy disk.  Make user get it
     * right or give up with floppies.
     */
    mp = dev->private ? (char *)dev->private : mountpoint;
    snprintf(buf, PATH_MAX, "%s/%s", mp, file);
    if (!file_readable(buf)) {
	if (probe)
	    return NULL;
	else {
	    while (!file_readable(buf)) {
		if (!--nretries) {
		    msgConfirm("GetFloppy: Failed to get %s after retries;\ngiving up.", buf);
		    return NULL;
		}
		distWanted = buf;
		mediaShutdownFloppy(dev);
		if (!mediaInitFloppy(dev))
		    return NULL;
	    }
	}
    }
    fp = fopen(buf, "r");
    return fp;
}

void
mediaShutdownFloppy(Device *dev)
{
    if (floppyMounted) {
    	char *mp = dev->private ? (char *)dev->private : mountpoint;

	if (unmount(mp, MNT_FORCE) != 0)
	    msgDebug("Umount of floppy on %s failed: %s (%d)\n", mp, strerror(errno), errno);
	else {
	    floppyMounted = FALSE;
	    if (!variable_get(VAR_NONINTERACTIVE) && variable_cmp(SYSTEM_STATE, "fixit"))
		msgConfirm("You may remove the floppy from %s", dev->description);
	}
    }
}
