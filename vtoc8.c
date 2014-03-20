/*-
 * Copyright (c) 2014 Juniper Networks, Inc.
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
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/vtoc.h>
#include <stdlib.h>

#include "mkimg.h"
#include "scheme.h"

static struct mkimg_alias vtoc8_aliases[] = {
    {	NULL, 0 }
};

static u_int
vtoc8_metadata(u_int where, u_int parts __unused, u_int secsz __unused)
{
	u_int secs;

	secs = (where == SCHEME_META_IMG_START) ? 1 : 0;
	return (secs);
}

static int
vtoc8_write(int fd __unused, off_t imgsz __unused, u_int parts __unused, 
    u_int secsz __unused)
{
	return (ENOSYS);
}

static struct mkimg_scheme vtoc8_scheme = {
	.name = "vtoc8",
	.description = "SMI VTOC8 disk labels",
	.aliases = vtoc8_aliases,
	.metadata = vtoc8_metadata,
	.write = vtoc8_write,
	.nparts = VTOC8_NPARTS
};

SCHEME_DEFINE(vtoc8_scheme);
