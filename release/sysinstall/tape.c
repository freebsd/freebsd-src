/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: tape.c,v 1.6.2.1 1995/09/18 17:00:27 peter Exp $
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

/* These routines deal with getting things off of tape media */

#include "sysinstall.h"
#include <sys/fcntl.h>
#include <sys/param.h>

static Boolean tapeInitted;

Boolean
mediaInitTape(Device *dev)
{
    int i;

    if (tapeInitted)
	return TRUE;

    Mkdir(dev->private, NULL);
    if (chdir(dev->private))
	    return FALSE;
    msgConfirm("Insert tape into %s and press return", dev->description);
    msgNotify("Attempting to extract from %s...", dev->description);
    if (!strcmp(dev->name, "ft0"))
	i = vsystem("ft | cpio -iduvm -H tar");
    else
	i = vsystem("cpio -iduvm -H tar -I %s", dev->devname);
    if (!i) {
	tapeInitted = TRUE;
	return TRUE;
    }
    else
	msgConfirm("Tape extract command failed with status %d!", i);
    return FALSE;
}

int
mediaGetTape(Device *dev, char *file, Attribs *dist_attrs)
{
    char buf[PATH_MAX];

    sprintf(buf, "%s/%s", (char *)dev->private, file);
    if (file_readable(buf))
    	return open(buf, O_RDONLY);
    sprintf(buf, "%s/dists/%s", (char *)dev->private, file);
    return open(buf, O_RDONLY);
}

void
mediaShutdownTape(Device *dev)
{
    if (!tapeInitted)
	return;
    if (!access(dev->private, X_OK)) {
	msgNotify("Cleaning up results of tape extract..");
	(void)vsystem("rm -rf %s", (char *)dev->private);
    }
    tapeInitted = FALSE;
}
