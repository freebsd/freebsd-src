/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: nfs.c,v 1.5.2.20 1996/05/24 06:09:01 jkh Exp $
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

Boolean
mediaInitNFS(Device *dev)
{
    Device *netDevice = (Device *)dev->private;

    if (NFSMounted)
	return TRUE;

    if (!netDevice->init(netDevice))
	return FALSE;

    if (Mkdir("/dist"))
	return FALSE;

    msgNotify("Mounting %s over NFS.", dev->name);
    if (vsystem("mount_nfs %s %s %s /dist",
		variable_get(VAR_SLOW_ETHER) ? "-r 1024 -w 1024" : "",
		variable_get(VAR_NFS_SECURE) ? "-P" : "", dev->name)) {
	msgConfirm("Error mounting %s on /dist: %s (%u)", dev->name, strerror(errno), errno);
	netDevice->shutdown(netDevice);
	return FALSE;
    }
    NFSMounted = TRUE;
    msgDebug("Mounted NFS device %s onto /dist\n", dev->name);
    return TRUE;
}

int
mediaGetNFS(Device *dev, char *file, Boolean probe)
{
    char	buf[PATH_MAX];

    msgDebug("Request for %s from NFS\n", file);
    snprintf(buf, PATH_MAX, "/dist/%s", file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/dist/dists/%s", file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/dist/%s/%s", variable_get(VAR_RELNAME), file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/dist/%s/dists/%s", variable_get(VAR_RELNAME), file);
    return open(buf, O_RDONLY);
}

void
mediaShutdownNFS(Device *dev)
{
    /* Device *netdev = (Device *)dev->private; */

    if (!NFSMounted)
	return;
    msgNotify("Unmounting NFS partition on /dist");
    if (unmount("/dist", MNT_FORCE) != 0)
	msgConfirm("Could not unmount the NFS partition: %s", strerror(errno));
    msgDebug("Unmount of NFS partition successful\n");
    /* (*netdev->shutdown)(netdev); */
    NFSMounted = FALSE;
    return;
}
