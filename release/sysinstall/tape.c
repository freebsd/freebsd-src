/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: tape.c,v 1.11 1996/04/23 01:29:34 jkh Exp $
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

/* These routines deal with getting things off of tape media */

#include "sysinstall.h"
#include <sys/fcntl.h>
#include <sys/param.h>

static Boolean tapeInitted;

char *
mediaTapeBlocksize(void)
{
    char *cp = variable_get(VAR_TAPE_BLOCKSIZE);

    return cp ? cp : DEFAULT_TAPE_BLOCKSIZE;
}

Boolean
mediaInitTape(Device *dev)
{
    int i;
    if (tapeInitted)
	return TRUE;

    msgDebug("Tape init routine called for %s (private dir is %s)\n", dev->name, dev->private);
    Mkdir(dev->private, NULL);
    if (chdir(dev->private))
	return FALSE;
    /* We know the tape is already in the drive, so go for it */
    msgNotify("Attempting to extract from %s...", dev->description);
    if (!strcmp(dev->name, "rft0"))
	i = vsystem("ft | cpio -idum %s --block-size %s", cpioVerbosity(), mediaTapeBlocksize());
    else
	i = vsystem("cpio -idum %s --block-size %s -I %s", cpioVerbosity(), mediaTapeBlocksize(), dev->devname);
    if (!i) {
	tapeInitted = TRUE;
	msgDebug("Tape initialized successfully.\n");
	return TRUE;
    }
    else
	msgConfirm("Tape extract command failed with status %d!", i);
    return FALSE;
}

int
mediaGetTape(Device *dev, char *file, Boolean probe)
{
    char buf[PATH_MAX];
    int fd;

    sprintf(buf, "%s/%s", (char *)dev->private, file);
    msgDebug("Request for %s from tape (looking in %s)\n", file, buf);
    if (file_readable(buf))
	fd = open(buf, O_RDONLY);
    else {
	sprintf(buf, "%s/dists/%s", (char *)dev->private, file);
	fd = open(buf, O_RDONLY);
    }
    /* Nuke the files behind us to save space */
    if (fd != -1)
	unlink(buf);
    return fd;
}

void
mediaShutdownTape(Device *dev)
{
    if (!tapeInitted)
	return;
    msgDebug("Shutdown of tape device - %s will be cleaned\n", dev->private);
    if (file_readable(dev->private)) {
	msgNotify("Cleaning up results of tape extract..");
	(void)vsystem("rm -rf %s", (char *)dev->private);
    }
    tapeInitted = FALSE;
}
