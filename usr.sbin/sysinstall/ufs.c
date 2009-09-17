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

#include "sysinstall.h"
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <ufs/ufs/ufsmount.h>

static Boolean UFSMounted;
static char mountpoint[] = "/dist";

Boolean
mediaInitUFS(Device *dev)
{
    struct ufs_args args;

    if (UFSMounted)
	return TRUE;
     
    Mkdir(mountpoint);
    memset(&args, 0, sizeof(args));
    args.fspec = dev->devname;

    if (mount("ufs", mountpoint, MNT_RDONLY, (caddr_t)&args) == -1) {
	msgConfirm("Error mounting %s on %s: %s (%u)", args.fspec, mountpoint, strerror(errno), errno);
	return FALSE;
    }
    UFSMounted = TRUE;
    return TRUE;
}

FILE *
mediaGetUFS(Device *dev, char *file, Boolean probe)
{
    return mediaGenericGet((char *)dev->private, file);
}

void
mediaShutdownUFS(Device *dev)
{
    if (!UFSMounted)
	return;
    if (unmount(mountpoint, MNT_FORCE) != 0)
	msgConfirm("Could not unmount the UFS partition from %s: %s",
		   mountpoint, strerror(errno));
    else
	UFSMounted = FALSE;
    return;
}
