/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: floppy.c,v 1.1 1995/05/27 10:38:52 jkh Exp $
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
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

/*
 * These routines deal with getting things off of floppy media, though
 * with one exception:  genericGetDist() is actually used from lots of places
 * since it can think of the world as just "one big floppy" too if that's
 * appropriate.
 */

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

/* For finding floppies */
static int
floppyChoiceHook(char *str)
{
    Device **devs;

    /* Clip garbage off the ends */
    string_prune(str);
    str = string_skipwhite(str);
    if (!*str)
	return 0;
    devs = deviceFind(str, DEVICE_TYPE_FLOPPY);
    if (devs)
	floppyDev = devs[0];
    return devs ? 1 : 0;
}

/* Our last-ditch routine for getting ROOT from a floppy */
int
getRootFloppy(void)
{
    int fd = -1;

    while (floppyDev == NULL || fd == -1) {
	Device **devs;
	int cnt;
		    
	devs = deviceFind(NULL, DEVICE_TYPE_FLOPPY);
	cnt = deviceCount(devs);
	if (cnt == 1)
	    floppyDev = devs[0];
	else if (cnt > 1) {
	    DMenu *menu;
	    
	    menu = deviceCreateMenu(&MenuMediaFloppy, DEVICE_TYPE_FLOPPY, floppyChoiceHook);
	    menu->title = "Please insert the ROOT floppy";
	    dmenuOpenSimple(menu);
	}
	else {
	    msgConfirm("No floppy devices found!  Something is seriously wrong!");
	    return -1;
	}
	if (!floppyDev)
	    continue;
	fd = open(floppyDev->devname, O_RDONLY);
    }
    return fd;
}

Boolean
mediaInitFloppy(Device *dev)
{
    struct msdosfs_args dosargs;
    char mountpoint[FILENAME_MAX];

    if (floppyMounted)
	return TRUE;
    memset(&dosargs, 0, sizeof dosargs);

    if (Mkdir("/mnt", NULL)) {
	msgConfirm("Unable to make directory mountpoint for %s!", mountpoint);
	return FALSE;
    }
    msgDebug("initFloppy:  mount floppy %s on /mnt\n", dev->devname); 
    dosargs.fspec = dev->devname;
    if (mount(MOUNT_MSDOS, "/mnt", 0, (caddr_t)&dosargs) == -1) {
	msgConfirm("Error mounting floppy %s (%s) on /mnt : %s\n", dev->name,
		   dev->devname, mountpoint, strerror(errno));
	return FALSE;
    }
    floppyMounted = TRUE;
    return TRUE;
}

int
mediaGetFloppy(char *file)
{
    char		buf[PATH_MAX];

    snprintf(buf, PATH_MAX, "/mnt/%s", file);

    return open(buf,O_RDONLY);
}

void
mediaShutdownFloppy(Device *dev)
{
    if (floppyMounted) {
	if (vsystem("umount /mnt") != 0)
	    msgDebug("Umount of floppy on /mnt failed: %s (%d)\n", strerror(errno), errno);
	else
	    floppyMounted = FALSE;
    }
}
