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
#include <sys/gpt.h>
#include <sys/linker_set.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <uuid.h>

#include "mkimg.h"
#include "scheme.h"

static uuid_t gpt_uuid_efi = GPT_ENT_TYPE_EFI;

static struct mkimg_alias gpt_aliases[] = {
    {	"efi", ALIAS_PTR(&gpt_uuid_efi) },
    {	NULL, 0 }
};

static u_int
gpt_metadata(u_int where, u_int parts, u_int secsz)
{
	u_int ents, secs;

	if (where != SCHEME_META_IMG_START && where != SCHEME_META_IMG_START)
		return (0);

	ents = secsz / sizeof(struct gpt_ent);
	secs = (parts + ents - 1) / ents;
	secs += (where == SCHEME_META_IMG_START) ? 2 : 1;
	return (secs);
}

static struct mkimg_scheme gpt_scheme = {
	.name = "gpt",
	.description = "GUID Partition Table",
	.aliases = gpt_aliases,
	.metadata = gpt_metadata,
	.nparts = 4096
};

SCHEME_DEFINE(gpt_scheme);
