/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media_strategy.c,v 1.1 1995/05/17 14:39:53 jkh Exp $
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

#include <stdio.h>
#include "sysinstall.h"
#include <ctype.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/dkbad.h>

#define MSDOSFS
#define CD9660
#define NFS
#include <sys/mount.h>
#undef MSDOSFS
#undef CD9660
#undef NFS

int
genericGetDist(char *dist, char *source)
{
    return 0;
}

/* Various media "strategy" routines */

Boolean
mediaInitCDROM(Device *dev)
{
    struct iso_args	args;
    struct stat		sb;

    if (Mkdir("/mnt/cdrom", NULL))
	return FALSE;

    args.fspec = dev->devname;
    if (mount(MOUNT_CD9660, "/mnt/cdrom", 0, (caddr_t) &args) == -1)
    {
	msgConfirm("Error mounting %s on /mnt/cdrom: %s\n",
		   dev, strerror(errno));
	return FALSE;
    }

    /* Do a very simple check to see if this looks roughly like a 2.0.5 CDROM
       Unfortunately FreeBSD won't let us read the ``label'' AFAIK, which is one
       sure way of telling the disc version :-( */
    if (stat("/mnt/cdrom/dists", &sb))
    {
	if (errno == ENOENT)
	{
	    msgConfirm("Couldn't locate the directory `dists' on the cdrom\n\
Is this a 2.0.5 CDROM?\n");
	    return FALSE;
	} else {
	    msgConfirm("Couldn't stat directory %s: %s", "/mnt/cdrom/dists", strerror(errno));
	    return FALSE;
	}
    }
    
    return TRUE;
}

Boolean
mediaGetCDROM(char *dist)
{
    return TRUE;
}

void
mediaCloseCDROM(Device *dev)
{
    return;
}

Boolean
mediaInitFloppy(Device *dev)
{
    return TRUE;
}

Boolean
mediaGetFloppy(char *dist)
{
    return TRUE;
}

void
mediaCloseFloppy(Device *dev)
{
    return;
}

Boolean
mediaInitTape(Device *dev)
{
    return TRUE;
}

Boolean
mediaGetTape(char *dist)
{
    return TRUE;
}

void
mediaCloseTape(Device *dev)
{
    return;
}

Boolean
mediaInitNetwork(Device *dev)
{
    return TRUE;
}

Boolean
mediaGetNetwork(char *dist)
{
    return TRUE;
}

void
mediaCloseNetwork(Device *dev)
{
}

Boolean
mediaInitUFS(Device *dev)
{
    return TRUE;
}

Boolean
mediaGetUFS(char *dist)
{
    return TRUE;
}

/* UFS has no close routine since this is handled at the device level */

