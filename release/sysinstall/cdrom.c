/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: cdrom.c,v 1.7.2.18 1996/05/24 06:08:13 jkh Exp $
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

/* These routines deal with getting things off of CDROM media */

#include "sysinstall.h"
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>
#include <grp.h>
#include <fcntl.h>

#define CD9660
#include <sys/mount.h>
#undef CD9660

#define CD_UNMOUNTED		0
#define CD_ALREADY_MOUNTED	1
#define CD_WE_MOUNTED_IT	2

/*
 * This isn't static, like the others, since it's often useful to know whether
 * or not we have a CDROM available in some of the other installation screens.
 * This also isn't a boolean like the others since we have 3 states for it:
 * 0 = cdrom isn't mounted, 1 = cdrom is mounted and we mounted it, 2 = cdrom
 * was already mounted when we came in and we should leave it that way when
 * we leave.
 */
int cdromMounted;

Boolean
mediaInitCDROM(Device *dev)
{
    struct iso_args	args;

    if (cdromMounted != CD_UNMOUNTED)
	return TRUE;

    if (Mkdir("/cdrom", NULL))
	return FALSE;

    bzero(&args, sizeof(args));
    args.fspec = dev->devname;
    args.flags = 0;

    /* If this cdrom's not already mounted or can't be mounted, yell */
    if (!directory_exists("/cdrom/dists")) {
	if (mount(MOUNT_CD9660, "/cdrom", MNT_RDONLY, (caddr_t) &args) == -1) {
	    msgConfirm("Error mounting %s on /cdrom: %s (%u)", dev->devname, strerror(errno), errno);
	    return FALSE;
	}
	else
	    cdromMounted = CD_WE_MOUNTED_IT;
    }
    else
	cdromMounted = CD_ALREADY_MOUNTED;
    msgDebug("Mounted CDROM device %s on /cdrom\n", dev->devname);
    return TRUE;
}

int
mediaGetCDROM(Device *dev, char *file, Boolean probe)
{
    char	buf[PATH_MAX];

    msgDebug("Request for %s from CDROM\n", file);
    snprintf(buf, PATH_MAX, "/cdrom/%s", file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/cdrom/dists/%s", file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/cdrom/%s/%s", variable_get(VAR_RELNAME), file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/cdrom/%s/dists/%s", variable_get(VAR_RELNAME), file);
    return open(buf, O_RDONLY);
}

void
mediaShutdownCDROM(Device *dev)
{
    /* Only undo it if we did it */
    if (cdromMounted != CD_WE_MOUNTED_IT)
	return;
    msgDebug("Unmounting %s from /cdrom\n", dev->devname);
    if (unmount("/cdrom", MNT_FORCE) != 0) {
	msgConfirm("Could not unmount the CDROM from /cdrom: %s", strerror(errno));
	cdromMounted = CD_ALREADY_MOUNTED;	/* Guess somebody else got it */
    }
    else {
	msgDebug("Unmount successful\n");
	cdromMounted = CD_UNMOUNTED;
    }
}
