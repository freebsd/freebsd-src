/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: dos.c,v 1.11 1996/04/23 01:29:19 jkh Exp $
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

#include "sysinstall.h"
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <grp.h>

#define MSDOSFS
#include <sys/mount.h>
#undef MSDOSFS

static Boolean DOSMounted;

Boolean
mediaInitDOS(Device *dev)
{
    struct msdosfs_args	args;

    if (!RunningAsInit || DOSMounted)
	return TRUE;

    if (DITEM_STATUS(Mkdir("/dos", NULL)) != DITEM_SUCCESS)
	return FALSE;

    memset(&args, 0, sizeof(args));
    args.fspec = dev->devname;
    args.uid = args.gid = 0;
    args.mask = 0777;

    if (mount(MOUNT_MSDOS, "/dos", MNT_RDONLY, (caddr_t)&args) == -1) {
	msgConfirm("Error mounting %s on /dos: %s (%u)", args.fspec, strerror(errno), errno);
	return FALSE;
    }
    else
	msgDebug("Mounted DOS device (%s) on /dos.\n", args.fspec);
    DOSMounted = TRUE;
    return TRUE;
}

int
mediaGetDOS(Device *dev, char *file, Boolean probe)
{
    char		buf[PATH_MAX];

    msgDebug("Request for %s from DOS\n", file);
    snprintf(buf, PATH_MAX, "/dos/freebsd/%s", file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/dos/freebsd/dists/%s", file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/dos/%s", file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/dos/dists/%s", file);
    return open(buf, O_RDONLY);
}

void
mediaShutdownDOS(Device *dev)
{
    if (!RunningAsInit || !DOSMounted)
	return;
    msgDebug("Unmounting %s from /dos\n", dev->name);
    if (unmount("/dos", MNT_FORCE) != 0)
	msgConfirm("Could not unmount the DOS partition: %s", strerror(errno));
    if (isDebug())
	msgDebug("Unmount successful\n");
    DOSMounted = FALSE;
    return;
}
