/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: ufs.c,v 1.8 1996/04/13 13:32:14 jkh Exp $
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

/* No init or shutdown routines necessary - all done in mediaSetUFS() */

int
mediaGetUFS(Device *dev, char *file, Boolean probe)
{
    char		buf[PATH_MAX];

    if (isDebug())
	msgDebug("Request for %s from UFS\n", file);
    snprintf(buf, PATH_MAX, "%s/%s", dev->private, file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "%s/dists/%s", dev->private, file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "%s/%s/%s", dev->private, variable_get(VAR_RELNAME), file);
    if (file_readable(buf))
	return open(buf, O_RDONLY);
    snprintf(buf, PATH_MAX, "%s/%s/dists/%s", dev->private, variable_get(VAR_RELNAME), file);
    return open(buf, O_RDONLY);
}
