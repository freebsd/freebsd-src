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
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/syslimits.h>
#include <sys/param.h>
#include <sys/mount.h>

Boolean NFSMounted;
static char mountpoint[] = "/dist";

Boolean
mediaInitNFS(Device *dev)
{
    Device *netDevice = (Device *)dev->private;
    WINDOW *w = savescr();

    if (NFSMounted)
	return TRUE;

    if (!DEVICE_INIT(netDevice))
	return FALSE;

    if (Mkdir(mountpoint))
	return FALSE;

    msgNotify("Mounting %s over NFS on %s", dev->name, mountpoint);
    if (vsystem("mount_nfs %s %s %s %s",
		variable_get(VAR_SLOW_ETHER) ? "-r 1024 -w 1024" : "",
		variable_get(VAR_NFS_SECURE) ? "-P" : "", dev->name, mountpoint)) {
	msgConfirm("Error mounting %s on %s: %s.", dev->name, mountpoint, strerror(errno));
	if (netDevice)
	    DEVICE_SHUTDOWN(netDevice);
	restorescr(w);
	return FALSE;
    }
    NFSMounted = TRUE;
    if (isDebug())
	msgDebug("Mounted NFS device %s onto %s\n", dev->name, mountpoint);
    restorescr(w);
    return TRUE;
}

FILE *
mediaGetNFS(Device *dev, char *file, Boolean probe)
{
    return mediaGenericGet(mountpoint, file);
}

void
mediaShutdownNFS(Device *dev)
{
    if (!NFSMounted)
	return;

    msgDebug("Unmounting NFS partition on %s\n", mountpoint);
    if (unmount(mountpoint, MNT_FORCE) != 0)
	msgConfirm("Could not unmount the NFS partition: %s", strerror(errno));
    NFSMounted = FALSE;
    return;
}
