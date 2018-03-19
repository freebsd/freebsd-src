/*
 * Copyright (c) 2017-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "pt_sb_context.h"
#include "pt_sb_session.h"

#include "libipt-sb.h"
#include "intel-pt.h"

#include <stdlib.h>


struct pt_sb_context *pt_sb_ctx_alloc(const char *name)
{
	struct pt_sb_context *context;
	struct pt_image *image;

	image = pt_image_alloc(name);
	if (!image)
		return NULL;

	context = malloc(sizeof(*context));
	if (!context) {
		pt_image_free(image);
		return NULL;
	}

	memset(context, 0, sizeof(*context));
	context->image = image;
	context->ucount = 1;

	return context;
}

int pt_sb_ctx_get(struct pt_sb_context *context)
{
	uint16_t ucount;

	if (!context)
		return -pte_invalid;

	ucount = context->ucount;
	if (UINT16_MAX <= ucount)
		return -pte_overflow;

	context->ucount = ucount + 1;

	return 0;
}

static void pt_sb_ctx_free(struct pt_sb_context *context)
{
	if (!context)
		return;

	pt_image_free(context->image);
	free(context);
}

int pt_sb_ctx_put(struct pt_sb_context *context)
{
	uint16_t ucount;

	if (!context)
		return -pte_invalid;

	ucount = context->ucount;
	if (ucount > 1) {
		context->ucount = ucount - 1;
		return 0;
	}

	if (!ucount)
		return -pte_internal;

	pt_sb_ctx_free(context);

	return 0;
}

struct pt_image *pt_sb_ctx_image(const struct pt_sb_context *context)
{
	if (!context)
		return NULL;

	return context->image;
}

int pt_sb_ctx_mmap(struct pt_sb_session *session, struct pt_sb_context *context,
		   const char *filename, uint64_t offset, uint64_t size,
		   uint64_t vaddr)
{
	struct pt_image_section_cache *iscache;
	struct pt_image *image;
	int isid;

	image = pt_sb_ctx_image(context);
	if (!image)
		return -pte_internal;

	iscache = pt_sb_iscache(session);
	if (!iscache)
		return pt_image_add_file(image, filename, offset, size, NULL,
					 vaddr);

	isid = pt_iscache_add_file(iscache, filename, offset, size, vaddr);
	if (isid < 0)
		return isid;

	return pt_image_add_cached(image, iscache, isid, NULL);
}

int pt_sb_ctx_switch_to(struct pt_image **pimage, struct pt_sb_session *session,
			const struct pt_sb_context *context)
{
	pt_sb_ctx_switch_notifier_t *notify_switch_to;
	struct pt_image *image;
	int errcode;

	if (!pimage || !session)
		return -pte_internal;

	image = pt_sb_ctx_image(context);
	if (!image)
		return -pte_internal;

	notify_switch_to = session->notify_switch_to;
	if (notify_switch_to) {
		errcode = notify_switch_to(context, session->priv_switch_to);
		if (errcode < 0)
			return errcode;
	}

	*pimage = image;

	return 0;
}
