/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libder_private.h"

uint8_t
libder_type_simple_abi(const struct libder_tag *type)
{

	return (libder_type_simple(type));
}

/*
 * We'll likely expose this in the form of libder_type_import(), which validates
 * and allocates a tag.
 */
LIBDER_PRIVATE struct libder_tag *
libder_type_alloc(void)
{

	return (calloc(1, sizeof(struct libder_tag)));
}

struct libder_tag *
libder_type_dup(struct libder_ctx *ctx, const struct libder_tag *dtype)
{
	struct libder_tag *type;

	type = libder_type_alloc();
	if (type == NULL) {
		libder_set_error(ctx, LDE_NOMEM);
		return (NULL);
	}

	memcpy(type, dtype, sizeof(*dtype));

	if (type->tag_encoded) {
		uint8_t *tdata;

		/* Deep copy the tag data. */
		tdata = malloc(type->tag_size);
		if (tdata == NULL) {
			libder_set_error(ctx, LDE_NOMEM);

			/*
			 * Don't accidentally free the caller's buffer; it may
			 * be an external user of the API.
			 */
			type->tag_long = NULL;
			type->tag_size = 0;
			libder_type_free(type);
			return (NULL);
		}

		memcpy(tdata, dtype->tag_long, dtype->tag_size);
		type->tag_long = tdata;
	}

	return (type);
}

struct libder_tag *
libder_type_alloc_simple(struct libder_ctx *ctx, uint8_t typeval)
{
	struct libder_tag *type;

	type = libder_type_alloc();
	if (type == NULL) {
		libder_set_error(ctx, LDE_NOMEM);
		return (NULL);
	}

	type->tag_size = sizeof(typeval);
	type->tag_class = BER_TYPE_CLASS(typeval);
	type->tag_constructed = BER_TYPE_CONSTRUCTED(typeval);
	type->tag_short = BER_TYPE(typeval);
	return (type);
}

LIBDER_PRIVATE void
libder_type_release(struct libder_tag *type)
{

	if (type->tag_encoded) {
		free(type->tag_long);
		type->tag_long = NULL;

		/*
		 * Leaving type->tag_encoded set in case it helps us catch some
		 * bogus re-use of the type; we'd surface that as a null ptr
		 * deref as they think they should be using tag_long.
		 */
	}
}

void
libder_type_free(struct libder_tag *type)
{

	if (type == NULL)
		return;

	libder_type_release(type);
	free(type);
}

LIBDER_PRIVATE void
libder_normalize_type(struct libder_ctx *ctx, struct libder_tag *type)
{
	uint8_t tagval;
	size_t offset;

	if (!type->tag_encoded || !DER_NORMALIZING(ctx, TAGS))
		return;

	/*
	 * Strip any leading 0's off -- not possible in strict mode.
	 */
	for (offset = 0; offset < type->tag_size - 1; offset++) {
		if ((type->tag_long[offset] & 0x7f) != 0)
			break;
	}

	assert(offset == 0 || !ctx->strict);
	if (offset != 0) {
		type->tag_size -= offset;
		memmove(&type->tag_long[0], &type->tag_long[offset],
		    type->tag_size);
	}

	/*
	 * We might be able to strip it down to a unencoded tag_short, if only
	 * the lower 5 bits are in use.
	 */
	if (type->tag_size != 1 || (type->tag_long[0] & ~0x1e) != 0)
		return;

	tagval = type->tag_long[0];

	free(type->tag_long);
	type->tag_short = tagval;
	type->tag_encoded = false;
}
