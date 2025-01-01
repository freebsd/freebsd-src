/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libder.h"
#include "libder_private.h"

struct memory_write_data {
	uint8_t		*buf;
	size_t		 bufsz;
	size_t		 offset;
};

typedef bool (write_buffer_t)(void *, const uint8_t *, size_t);

static bool
libder_write_object_tag(struct libder_ctx *ctx __unused,
    const struct libder_object *obj, write_buffer_t *write_buffer, void *cookie)
{
	const struct libder_tag *type = obj->type;
	uint8_t value;

	if (!type->tag_encoded) {
		value = libder_type_simple(type);
		return (write_buffer(cookie, &value, sizeof(value)));
	}

	/* Write out the tag info first. */
	value = BER_TYPE_LONG_MASK;
	value |= type->tag_class << 6;
	if (type->tag_constructed)
		value |= BER_TYPE_CONSTRUCTED_MASK;

	if (!write_buffer(cookie, &value, sizeof(value)))
		return (false);

	/* Write out the encoded tag next. */
	return (write_buffer(cookie, type->tag_long, type->tag_size));
}

static bool
libder_write_object_header(struct libder_ctx *ctx, struct libder_object *obj,
    write_buffer_t *write_buffer, void *cookie)
{
	size_t size;
	uint8_t sizelen, value;

	if (!libder_write_object_tag(ctx, obj, write_buffer, cookie))
		return (false);

	size = obj->disk_size;
	sizelen = libder_size_length(size);

	if (sizelen == 1) {
		assert((size & ~0x7f) == 0);

		value = size;
		if (!write_buffer(cookie, &value, sizeof(value)))
			return (false);
	} else {
		/*
		 * Protocol supports at most 0x7f size bytes, but we can only
		 * do up to a size_t.
		 */
		uint8_t sizebuf[sizeof(size_t)], *sizep;

		sizelen--;	/* Remove the lead byte. */

		value = 0x80 | sizelen;
		if (!write_buffer(cookie, &value, sizeof(value)))
			return (false);

		sizep = &sizebuf[0];
		for (uint8_t i = 0; i < sizelen; i++)
			*sizep++ = (size >> ((sizelen - i - 1) * 8)) & 0xff;

		if (!write_buffer(cookie, &sizebuf[0], sizelen))
			return (false);
	}

	return (true);
}

static bool
libder_write_object_payload(struct libder_ctx *ctx __unused,
    struct libder_object *obj, write_buffer_t *write_buffer, void *cookie)
{
	uint8_t *payload = obj->payload;
	size_t length = obj->length;

	/* We don't expect `obj->payload` to be valid for a zero-size value. */
	if (length == 0)
		return (true);

	/*
	 * We allow a NULL payload with a non-zero length to indicate that an
	 * object should write zeroes out, we just didn't waste the memory on
	 * these small allocations.  Ideally if it's more than just one or two
	 * zeroes we're instead allocating a buffer for it and doing some more
	 * efficient copying from there.
	 */
	if (payload == NULL) {
		uint8_t zero = 0;

		for (size_t i = 0; i < length; i++) {
			if (!write_buffer(cookie, &zero, 1))
				return (false);
		}

		return (true);
	}

	return (write_buffer(cookie, payload, length));
}

static bool
libder_write_object(struct libder_ctx *ctx, struct libder_object *obj,
    write_buffer_t *write_buffer, void *cookie)
{
	struct libder_object *child;

	if (DER_NORMALIZING(ctx, CONSTRUCTED) && !libder_obj_coalesce_children(obj, ctx))
		return (false);

	/* Write out this object's header first */
	if (!libder_write_object_header(ctx, obj, write_buffer, cookie))
		return (false);

	/* Write out the payload. */
	if (obj->children == NULL)
		return (libder_write_object_payload(ctx, obj, write_buffer, cookie));

	assert(obj->type->tag_constructed);

	/* Recurse on each child. */
	DER_FOREACH_CHILD(child, obj) {
		if (!libder_write_object(ctx, child, write_buffer, cookie))
			return (false);
	}

	return (true);
}

static bool
memory_write(void *cookie, const uint8_t *data, size_t datasz)
{
	struct memory_write_data *mwrite = cookie;
	uint8_t *dst = &mwrite->buf[mwrite->offset];
	size_t left;

	/* Small buffers should have been rejected long before now. */
	left = mwrite->bufsz - mwrite->offset;
	assert(datasz <= left);

	memcpy(dst, data, datasz);
	mwrite->offset += datasz;
	return (true);
}

/*
 * Writes the object rooted at `root` to the buffer.  If `buf` == NULL and
 * `*bufsz` == 0, we'll allocate a buffer just large enough to hold the result
 * and pass the size back via `*bufsz`.  If a pre-allocated buffer is passed,
 * we may still update `*bufsz` if normalization made the buffer smaller.
 *
 * If the buffer is too small, *bufsz will be set to the size of buffer needed.
 */
uint8_t *
libder_write(struct libder_ctx *ctx, struct libder_object *root, uint8_t *buf,
    size_t *bufsz)
{
	struct memory_write_data mwrite = { 0 };
	size_t needed;

	/*
	 * We shouldn't really see buf == NULL with *bufsz != 0 or vice-versa.
	 * Combined, they mean that we should allocate whatever buffer size we
	 * need.
	 */
	if ((buf == NULL && *bufsz != 0) || (buf != NULL && *bufsz == 0))
		return (NULL);	/* XXX Surface error? */

	/*
	 * If we're doing any normalization beyond our standard size
	 * normalization, we apply those rules up front since they may alter our
	 * disk size every so slightly.
	 */
	if (ctx->normalize != 0 && !libder_obj_normalize(root, ctx))
		return (NULL);

	needed = libder_obj_disk_size(root, true);
	if (needed == 0)
		return (NULL);	/* Overflow */

	/* Allocate if we weren't passed a buffer. */
	if (*bufsz == 0) {
		*bufsz = needed;
		buf = malloc(needed);
		if (buf == NULL)
			return (NULL);
	} else if (needed > *bufsz) {
		*bufsz = needed;
		return (NULL);	/* Insufficient space */
	}

	/* Buffer large enough, write into it. */
	mwrite.buf = buf;
	mwrite.bufsz = *bufsz;
	if (!libder_write_object(ctx, root, &memory_write, &mwrite)) {
		libder_bzero(mwrite.buf, mwrite.offset);
		free(buf);
		return (NULL);	/* XXX Error */
	}

	/*
	 * We don't normalize the in-memory representation of the tree, we do
	 * that as we're writing into the buffer.  It could be the case that we
	 * didn't need the full buffer as a result of normalization.
	 */
	*bufsz = mwrite.offset;

	return (buf);
}
