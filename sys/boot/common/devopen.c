/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/common/devopen.c,v 1.5 2006/11/02 00:02:22 marcel Exp $");

#include <stand.h>
#include <string.h>

#include "bootstrap.h"

int
devopen(struct open_file *f, const char *fname, const char **file) 
{
    struct devdesc *dev;
    int result;

    result = archsw.arch_getdev((void **)&dev, fname, file);
    if (result)
	return (result);

    /* point to device-specific data so that device open can use it */
    f->f_devdata = dev;
    result = dev->d_dev->dv_open(f, dev);
    if (result != 0) {
	f->f_devdata = NULL;
	free(dev);
	return (result);
    }

    /* reference the devsw entry from the open_file structure */
    f->f_dev = dev->d_dev;
    return (0);
}

int
devclose(struct open_file *f)
{
    if (f->f_devdata != NULL) {
	free(f->f_devdata);
    }
    return(0);
}
