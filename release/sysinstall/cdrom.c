/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: cdrom.c,v 1.7.2.12 1995/11/03 12:02:23 jkh Exp $
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

/* This isn't static, like the others, since it's often useful to know whether or not we have a CDROM
   available in some of the other installation screens. */
Boolean cdromMounted;

Boolean
mediaInitCDROM(Device *dev)
{
    struct iso_args	args;
    struct stat		sb;
    char specialrel[80];

    if (!RunningAsInit || cdromMounted)
	return TRUE;

    if (Mkdir("/cdrom", NULL))
	return FALSE;

    bzero(&args, sizeof(args));
    args.fspec = dev->devname;
    args.flags = 0;

    if (mount(MOUNT_CD9660, "/cdrom", MNT_RDONLY, (caddr_t) &args) == -1) {
	dialog_clear();
	msgConfirm("Error mounting %s on /cdrom: %s (%u)", dev->devname, strerror(errno), errno);
	return FALSE;
    }
    /*
     * Do a very simple check to see if this looks roughly like a FreeBSD CDROM
     * Unfortunately FreeBSD won't let us read the ``label'' AFAIK, which is one
     * sure way of telling the disc version :-(
     */
    snprintf(specialrel, 80, "/cdrom/%s/dists", variable_get(VAR_RELNAME));
    if (stat("/cdrom/dists", &sb) && stat(specialrel, &sb)) {
	if (errno == ENOENT) {
	    dialog_clear();
	    msgConfirm("Couldn't locate the directory `dists' anywhere on the CD.\n"
		       "Is this a FreeBSD CDROM?  Is the release version set properly\n"
		       "in the Options editor?");
	    return FALSE;
	}
	else {
	    dialog_clear();
	    msgConfirm("Error trying to stat the CDROM's dists directory: %s", strerror(errno));
	    return FALSE;
	}
    }
    cdromMounted = TRUE;
    msgDebug("Mounted CDROM device %s on /cdrom\n", dev->devname);
    return TRUE;
}

int
mediaGetCDROM(Device *dev, char *file, Boolean tentative)
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
    if (!RunningAsInit || !cdromMounted)
	return;
    msgDebug("Unmounting %s from /cdrom\n", dev->devname);
    if (unmount("/cdrom", MNT_FORCE) != 0) {
	dialog_clear();
	msgConfirm("Could not unmount the CDROM from /cdrom: %s", strerror(errno));
    }
    msgDebug("Unmount successful\n");
    cdromMounted = FALSE;
    return;
}
