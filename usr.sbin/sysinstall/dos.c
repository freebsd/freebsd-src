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
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <grp.h>
#include "dosio.h"

static DOS_FS DOSFS;
static Boolean DOSMounted;

Boolean
mediaInitDOS(Device *dev)
{
    if (!RunningAsInit || DOSMounted)
	return TRUE;

    if (dos_mount(&DOSFS, dev->devname)) {
	msgConfirm("Error mounting DOS partition %s : %s (%u)", dev->devname, strerror(errno), errno);
	return FALSE;
    }
    DOSMounted = TRUE;
    if (isDebug())
	msgDebug("DOS partition %s mounted\n", dev->devname);
    return TRUE;
}

FILE *
mediaGetDOS(Device *dev, char *file, Boolean probe)
{
    char buf[PATH_MAX];
    FILE *fp;

    if (!DOSMounted) {
	msgDebug("Can't get DOS file %s - DOSFS currently unmounted!\n", file);
	return NULL;
    }

    if (isDebug())
	msgDebug("Request for %s from DOS\n", file);
    snprintf(buf, PATH_MAX, "/freebsd/%s", file);
    if ((fp = dos_open(&DOSFS, buf)))
	return fp;
    snprintf(buf, PATH_MAX, "/freebsd/dists/%s", file);
    if ((fp = dos_open(&DOSFS, buf)))
	return fp;
    snprintf(buf, PATH_MAX, "/%s", file);
    if ((fp = dos_open(&DOSFS, buf)))
	return fp;
    snprintf(buf, PATH_MAX, "/dists/%s", file);
    if ((fp = dos_open(&DOSFS, buf)))
	return fp;
    return NULL;
}

void
mediaShutdownDOS(Device *dev)
{
    if (!RunningAsInit || !DOSMounted)
	return;
    if (dos_unmount(&DOSFS))
	msgConfirm("Could not unmount DOS partition %s : %s", dev->devname, strerror(errno));
    else if (isDebug())
	msgDebug("Unmount of DOS partition on %s successful\n", dev->devname);
    DOSMounted = FALSE;
    return;
}
