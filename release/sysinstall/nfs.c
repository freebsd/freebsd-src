/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: nfs.c,v 1.5.2.2 1995/10/04 07:54:56 jkh Exp $
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

    if (!(*netDevice->init)(netDevice))
	return FALSE;

    if (Mkdir("/nfs", NULL))
	return FALSE;

    if (vsystem("mount_nfs %s %s %s /nfs",
		optionIsSet(OPT_SLOW_ETHER) ? "-r 1024 -w 1024" : "",
		optionIsSet(OPT_NFS_SECURE) ? "-P" : "", dev->name)) {
	msgConfirm("Error mounting %s on /nfs: %s (%u)\n", dev->name, strerror(errno), errno);
	return FALSE;
    }
    NFSMounted = TRUE;
    return TRUE;
}

int
mediaGetNFS(Device *dev, char *file, Attribs *dist_attrs)
{
    char	buf[PATH_MAX];

    snprintf(buf, PATH_MAX, "/nfs/%s", file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/nfs/dists/%s", file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/nfs/%s/%s", getenv(RELNAME), file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "/nfs/%s/dists/%s", getenv(RELNAME), file);
    return open(buf, O_RDONLY);
}

void
mediaShutdownNFS(Device *dev)
{
    /* Device *netdev = (Device *)dev->private; */

    if (!NFSMounted)
	return;
    msgDebug("Unmounting /nfs\n");
    if (unmount("/nfs", MNT_FORCE) != 0)
	msgConfirm("Could not unmount the NFS partition: %s\n", strerror(errno));
    if (isDebug())
	msgDebug("Unmount returned\n");
    /* (*netdev->shutdown)(netdev); */
    NFSMounted = FALSE;
    return;
}
