/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: floppy.c,v 1.16 1996/10/09 09:53:30 jkh Exp $
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
#undef MSDOSFS

static Device *floppyDev;
static Boolean floppyMounted;

char *distWanted;

/* For finding floppies */
static int
floppyChoiceHook(dialogMenuItem *self)
{
    Device **devs;

    devs = deviceFind(self->prompt, DEVICE_TYPE_FLOPPY);
    if (devs)
	floppyDev = devs[0];
    return devs ? DITEM_LEAVE_MENU : DITEM_FAILURE;
}

/* Our last-ditch routine for getting ROOT from a floppy */
int
getRootFloppy(void)
{
    int fd = -1;

    if (mediaDevice->type == DEVICE_TYPE_FLOPPY)
	floppyDev = mediaDevice;
    else {
	Device **devs;
	int cnt;

	devs = deviceFind(NULL, DEVICE_TYPE_FLOPPY);
	cnt = deviceCount(devs);
	if (!cnt) {
	    msgConfirm("No floppy devices found!  Something is seriously wrong!");
	    return -1;
	}
	else if (cnt == 1)
	    floppyDev = devs[0];
	else {
	    DMenu *menu;
	    int ret;
	    WINDOW *save = savescr();

	    menu = deviceCreateMenu(&MenuMediaFloppy, DEVICE_TYPE_FLOPPY, floppyChoiceHook, NULL);
	    menu->title = "Please insert the ROOT floppy";
	    ret = dmenuOpenSimple(menu, FALSE);
	    restorescr(save);
	    if (!ret)
		return -1;
	}
    }
    while (fd == -1) {
	msgConfirm("Please insert the ROOT floppy in %s and press [ENTER]", floppyDev->description);
	fd = open(floppyDev->devname, O_RDONLY);
	if (isDebug())
	    msgDebug("getRootFloppy on %s yields fd of %d\n", floppyDev->devname, fd);
	if (fd == -1 && msgYesNo("Couldn't open the floppy - do you want to try again?") != 0)
	    break;
    }
    return fd;
}

Boolean
mediaInitFloppy(Device *dev)
{
    struct msdosfs_args dosargs;
    struct ufs_args u_args;

    if (floppyMounted)
	return TRUE;

    if (Mkdir("/dist")) {
	msgConfirm("Unable to make directory mountpoint for %s!", dev->devname);
	return FALSE;
    }
    msgDebug("Init floppy called for %s distribution.\n", distWanted ? distWanted : "some");
    if (!distWanted)
    	msgConfirm("Please insert floppy for %s", dev->description);
    else if (distWanted != (char *)1)	/* 1 is kludge for "don't ask!" */
	msgConfirm("Please insert floppy containing %s for %s", distWanted, dev->description);

    memset(&dosargs, 0, sizeof dosargs);
    dosargs.fspec = dev->devname;
    dosargs.uid = dosargs.gid = 0;
    dosargs.mask = 0777;

    memset(&u_args, 0, sizeof(u_args));
    u_args.fspec = dev->devname;

    if (mount(MOUNT_MSDOS, "/dist", MNT_RDONLY, (caddr_t)&dosargs) == -1) {
	if (mount(MOUNT_UFS, "/dist", MNT_RDONLY, (caddr_t)&u_args) == -1) {
	    if (distWanted != (char *)1)
		msgConfirm("Error mounting floppy %s (%s) on /dist : %s", dev->name, dev->devname, strerror(errno));
	    return FALSE;
	}
    }
    msgDebug("initFloppy: mounted floppy %s successfully on /dist\n", dev->devname);
    floppyMounted = TRUE;
    distWanted = NULL;
    return TRUE;
}

int
mediaGetFloppy(Device *dev, char *file, Boolean probe)
{
    char		buf[PATH_MAX];
    int			fd;
    int			nretries = 5;

    snprintf(buf, PATH_MAX, "/dist/%s", file);

    msgDebug("Request for %s from floppy on /dist, probe is %d.\n", buf, probe);
    if (!file_readable(buf)) {
	if (probe)
	    return -1;
	else {
	    while (!file_readable(buf)) {
		if (!--nretries) {
		    msgConfirm("GetFloppy: Failed to get %s after retries;\ngiving up.", buf);
		    return -1;
		}
		distWanted = buf;
		mediaShutdownFloppy(dev);
		if (!mediaInitFloppy(dev))
		    return -1;
	    }
	}
    }
    fd = open(buf, O_RDONLY);
    return fd;
}

void
mediaShutdownFloppy(Device *dev)
{
    if (floppyMounted) {
	if (unmount("/dist", MNT_FORCE) != 0)
	    msgDebug("Umount of floppy on /dist failed: %s (%d)\n", strerror(errno), errno);
	else {
	    floppyMounted = FALSE;
	    msgDebug("Floppy unmounted successfully.\n");
	    msgConfirm("You may remove the floppy from %s", dev->description);
	}
    }
}
