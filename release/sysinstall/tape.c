/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $FreeBSD: src/release/sysinstall/tape.c,v 1.22 1999/12/17 02:46:04 jkh Exp $
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
    /* This is REALLY gross, but we need to do the init later in get due to the fact
     * that media is initialized BEFORE a filesystem is mounted now.
     */
    return TRUE;
}

FILE *
mediaGetTape(Device *dev, char *file, Boolean probe)
{
    char buf[PATH_MAX];
    FILE *fp;

    int i;

    if (!tapeInitted) {
	WINDOW *w = savescr();

	msgDebug("Tape init routine called for %s (private dir is %s)\n", dev->name, dev->private);
	Mkdir(dev->private);
	if (chdir(dev->private)) {
	    msgConfirm("Unable to CD to %s before extracting tape!\n"
		       "Tape media is not selected and thus cannot be installed from.", dev->private);
	    return (FILE *)IO_ERROR;
	}
	/* We know the tape is already in the drive, so go for it */
	msgNotify("First extracting distributions from %s...", dev->description);
	if (!strcmp(dev->name, "rft0"))
	    i = vsystem("ft | cpio -idum %s --block-size %s", cpioVerbosity(), mediaTapeBlocksize());
	else
	    i = vsystem("cpio -idum %s --block-size %s -I %s", cpioVerbosity(), mediaTapeBlocksize(), dev->devname);
	if (!i) {
	    tapeInitted = TRUE;
	    msgDebug("Tape initialized successfully.\n");
	}
	else {
	    msgConfirm("Tape extract command failed with status %d!\n"
		       "Unable to use tape media.", i);
	    restorescr(w);
	    return (FILE *)IO_ERROR;
	}
	restorescr(w);
    }

    sprintf(buf, "%s/%s", (char *)dev->private, file);
    if (isDebug())
	msgDebug("Request for %s from tape (looking in %s)\n", file, buf);
    if (file_readable(buf))
	fp = fopen(buf, "r");
    else {
	sprintf(buf, "%s/releases/%s", (char *)dev->private, file);
	fp = fopen(buf, "r");
    }
    /* Nuke the files behind us to save space */
    if (fp)
	unlink(buf);
    return fp;
}

void
mediaShutdownTape(Device *dev)
{
    if (!tapeInitted)
	return;
    if (file_readable((char *)dev->private)) {
	msgDebug("Cleaning up results of tape extract in %s..",
		  (char *)dev->private);
	(void)vsystem("rm -rf %s", (char *)dev->private);
    }
    tapeInitted = FALSE;
}
