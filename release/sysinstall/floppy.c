/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: floppy.c,v 1.7 1995/06/11 19:29:54 rgrimes Exp $
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

static char *distWanted;

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
	if (!cnt) {
	    msgConfirm("No floppy devices found!  Something is seriously wrong!");
	    return -1;
	}
	else if (cnt == 1) {
	    floppyDev = devs[0];
	    msgConfirm("Please insert the ROOT floppy in %s and press [ENTER]", floppyDev->description);
	}
	else  {
	    DMenu *menu;

	    menu = deviceCreateMenu(&MenuMediaFloppy, DEVICE_TYPE_FLOPPY, floppyChoiceHook);
	    menu->title = "Please insert the ROOT floppy";
	    if (!dmenuOpenSimple(menu))
		return -1;
	}
	if (!floppyDev)
	    continue;
	fd = open(floppyDev->devname, O_RDONLY);
	if (isDebug())
	    msgDebug("getRootFloppy on %s yields fd of %d\n", floppyDev->devname, fd);
    }
    return fd;
}

Boolean
mediaInitFloppy(Device *dev)
{
    struct msdosfs_args dosargs;

    if (floppyMounted)
	return TRUE;

    if (Mkdir("/mnt", NULL)) {
	msgConfirm("Unable to make directory mountpoint for %s!", dev->devname);
	return FALSE;
    }
    if (!distWanted)
    	msgConfirm("Please insert next floppy into %s", dev->description);
    else {
	msgConfirm("Please insert floppy containing %s into %s", distWanted, dev->description);
	distWanted = NULL;
    }
    memset(&dosargs, 0, sizeof dosargs);
    dosargs.fspec = dev->devname;
    dosargs.uid = dosargs.gid = 0;
    dosargs.mask = 0777;
    if (mount(MOUNT_MSDOS, "/mnt", 0, (caddr_t)&dosargs) == -1) {
	msgConfirm("Error mounting floppy %s (%s) on /mnt : %s", dev->name, dev->devname, strerror(errno));
	return FALSE;
    }
    if (isDebug())
	msgDebug("initFloppy: mounted floppy %s successfully on /mnt\n", dev->devname);
    floppyMounted = TRUE;
    return TRUE;
}

int
mediaGetFloppy(Device *dev, char *file, Attribs *dist_attrs)
{
    char		buf[PATH_MAX];
#ifdef DO_CRC_CHECK
    char		*extn, *var;
    const char 		*val;
    char		attrib[10];
    u_long		cval1, clen1, cval2, clen2;
#endif
    int			fd;
    int			nretries = 5;

    snprintf(buf, PATH_MAX, "/mnt/%s", file);

    if (file_readable(buf)) {
	if (dev->flags & OPT_EXPLORATORY_GET)
	    return -1;
	else {
	    while (!file_readable(buf)) {
		if (!--nretries) {
		    msgConfirm("GetFloppy: Failed to get %s after retries;\ngiving up.", file);
		    return -1;
		}
		distWanted = buf;
		(*dev->shutdown)(dev);
		if (!(dev->init)(dev))
		    return -1;
	    }
	}
    }

    fd = open(buf, O_RDONLY);
#ifdef DO_CRC_CHECK
    if (dist_attrs != NULL && fd != -1) {
	extn = rindex(buf, '.');
	snprintf(attrib, 10, "cksum%s", extn);
	val = attr_match(dist_attrs, attrib);
	if (val != NULL) {
	    if (isDebug())
		msgDebug("attr_match(%s,%s) returned `%s'\n", dist_attrs, attrib, val);
	    var = strdup(val);
	    
	    cval1 = strtol(var, &extn, 10);
	    clen1 = strtol(extn, NULL, 10);

	    if (crc(fd, &cval2, &clen2) != 0) {
		msgConfirm("crc() of file `%s' failed!", file);
		close(fd);
		return -1;
	    }
	    if ((cval1 != cval2) || (clen1 != clen2)) {
		msgConfirm("Invalid file `%s' (checksum `%u %u' should be %s)", file, cval2, clen2, var);
		close(fd);
		return -1;
	    }
	    lseek(fd, 0, 0);
	}
	else
	    msgNotify("No checksum information for file %s..", file);
    }
#endif
    return fd;
}

void
mediaShutdownFloppy(Device *dev)
{
    if (floppyMounted) {
	if (unmount("/mnt", MNT_FORCE) != 0)
	    msgDebug("Umount of floppy on /mnt failed: %s (%d)\n", strerror(errno), errno);
	else {
	    floppyMounted = FALSE;
	    msgConfirm("You may remove the floppy from %s", dev->description);
	}
    }
}
