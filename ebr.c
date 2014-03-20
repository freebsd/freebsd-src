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
#include <sys/linker_set.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <uuid.h>

#include "mkimg.h"
#include "scheme.h"

static struct mkimg_alias ebr_aliases[] = {
    {	NULL, 0 }
};

static off_t
ebr_get_leader(u_int parts __unused)
{

	return (1);
}

static off_t
ebr_get_trailer(u_int parts __unused)
{

	/*
	 * Compensate for having reserved a sector for the EBR after
	 * the last partition.
	 */
	return (-1);
}

static struct mkimg_scheme ebr_scheme = {
	.name = "ebr",
	.description = "Extended Boot Record",
	.nparts = 4096,
	.padding = 1,	/* See ebr_get_trailer() above */
	.aliases = ebr_aliases,
	.get_leader = ebr_get_leader,
	.get_trailer = ebr_get_trailer
};

SCHEME_DEFINE(ebr_scheme);
