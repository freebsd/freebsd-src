/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libder_private.h"

#undef	DER_CHILDREN
#undef	DER_NEXT

#define	DER_CHILDREN(obj)	((obj)->children)
#define	DER_NEXT(obj)		((obj)->next)

static uint8_t *
libder_obj_alloc_copy_payload(struct libder_ctx *ctx, const uint8_t *payload_in,
    size_t length)
{
	uint8_t *payload;

	if ((length == 0 && payload_in != NULL) ||
	    (length != 0 && payload_in == NULL)) {
		libder_set_error(ctx, LDE_INVAL);
		return (NULL);
	}

	if (length > 0) {
		payload = malloc(length);
		if (payload == NULL) {
			libder_set_error(ctx, LDE_NOMEM);
			return (NULL);
		}

		memcpy(payload, payload_in, length);
	} else {
		payload = NULL;
	}

	return (payload);
}

static bool
libder_obj_alloc_check(struct libder_ctx *ctx, struct libder_tag *type,
    const uint8_t *payload_in, size_t length)
{
	/*
	 * In addition to our normal constraints, constructed objects coming in
	 * from lib users should not have payloads.
	 */
	if (!libder_is_valid_obj(ctx, type, payload_in, length, false) ||
	    (type->tag_constructed && length != 0)) {
		libder_set_error(ctx, LDE_BADOBJECT);
		return (false);
	}

	return (true);
}

struct libder_object *
libder_obj_alloc(struct libder_ctx *ctx, struct libder_tag *type,
    const uint8_t *payload_in, size_t length)
{
	struct libder_object *obj;
	uint8_t *payload;

	if (!libder_obj_alloc_check(ctx, type, payload_in, length))
		return (NULL);

	payload = libder_obj_alloc_copy_payload(ctx, payload_in, length);

	obj = libder_obj_alloc_internal(ctx, type, payload, length, 0);
	if (obj == NULL) {
		if (length != 0) {
			libder_bzero(payload, length);
			free(payload);
		}

		libder_set_error(ctx, LDE_NOMEM);
	}

	return (obj);
}

struct libder_object *
libder_obj_alloc_simple(struct libder_ctx *ctx, uint8_t stype,
    const uint8_t *payload_in, size_t length)
{
	struct libder_object *obj;
	struct libder_tag *type;
	uint8_t *payload;

	type = libder_type_alloc_simple(ctx, stype);
	if (type == NULL)
		return (NULL);

	if (!libder_obj_alloc_check(ctx, type, payload_in, length)) {
		libder_type_free(type);
		return (NULL);
	}

	payload = libder_obj_alloc_copy_payload(ctx, payload_in, length);

	obj = libder_obj_alloc_internal(ctx, type, payload, length, LDO_OWNTAG);
	if (obj == NULL) {
		if (length != 0) {
			libder_bzero(payload, length);
			free(payload);
		}

		libder_type_free(type);
		libder_set_error(ctx, LDE_NOMEM);
	}

	return (obj);
}

/*
 * Returns an obj on success, NULL if out of memory.  `obj` takes ownership of
 * the payload on success.
 */
LIBDER_PRIVATE struct libder_object *
libder_obj_alloc_internal(struct libder_ctx *ctx, struct libder_tag *type,
    uint8_t *payload, size_t length, uint32_t flags)
{
	struct libder_object *obj;

	assert((flags & ~(LDO_OWNTAG)) == 0);

	if (length != 0)
		assert(payload != NULL);
	else
		assert(payload == NULL);

	obj = malloc(sizeof(*obj));
	if (obj == NULL)
		return (NULL);

	if ((flags & LDO_OWNTAG) != 0) {
		obj->type = type;
	} else {
		/*
		 * Deep copies the tag data, so that the caller can predict what
		 * it can do with the buffer.
		 */
		obj->type = libder_type_dup(ctx, type);
		if (obj->type == NULL) {
			free(obj);
			return (NULL);
		}
	}

	obj->length = length;
	obj->payload = payload;
	obj->children = obj->next = obj->parent = NULL;
	obj->nchildren = 0;

	return (obj);
}

LIBDER_PRIVATE size_t
libder_size_length(size_t sz)
{
	size_t nbytes;

	/*
	 * With DER, we use the smallest encoding necessary: less than 0x80
	 * can be encoded in one byte.
	 */
	if (sz < 0x80)
		return (1);

	/*
	 * We can support up to 0x7f size bytes, but we don't really have a way
	 * to represent that right now.  It's a good thing this function only
	 * takes a size_t, otherwise this would be a bit wrong.
	 */
	for (nbytes = 1; nbytes < sizeof(size_t); nbytes++) {
		if ((sz & ~((1ULL << 8 * nbytes) - 1)) == 0)
			break;
	}

	/* Add one for the lead byte describing the length of the length. */
	return (nbytes + 1);
}

/*
 * Returns the size on-disk.  If an object has children, we encode the size as
 * the sum of their lengths recursively.  Otherwise, we use the object's size.
 *
 * Returns 0 if the object size would overflow a size_t... perhaps we could
 * lift this restriction later.
 *
 * Note that the size of the object will be set/updated to simplify later write
 * calculations.
 */
LIBDER_PRIVATE size_t
libder_obj_disk_size(struct libder_object *obj, bool include_header)
{
	struct libder_object *walker;
	size_t disk_size, header_size;

	disk_size = obj->length;
	if (obj->children != NULL) {
		/* We should have rejected these. */
		assert(obj->length == 0);

		DER_FOREACH_CHILD(walker, obj) {
			size_t child_size;

			child_size = libder_obj_disk_size(walker, true);
			if (SIZE_MAX - child_size < disk_size)
				return (0);	/* Overflow */
			disk_size += child_size;
		}
	}

	obj->disk_size = disk_size;

	/*
	 * Children always include the header above, we only include the header
	 * at the root if we're calculating how much space we need in total.
	 */
	if (include_header) {
		/* Size of the length + the tag (arbitrary length) */
		header_size = libder_size_length(disk_size) + obj->type->tag_size;
		if (obj->type->tag_encoded)
			header_size++;	/* Lead byte */
		if (SIZE_MAX - header_size < disk_size)
			return (0);

		disk_size += header_size;
	}

	return (disk_size);
}

void
libder_obj_free(struct libder_object *obj)
{
	struct libder_object *child, *tmp;

	if (obj == NULL)
		return;

	DER_FOREACH_CHILD_SAFE(child, obj, tmp)
		libder_obj_free(child);

	if (obj->payload != NULL) {
		libder_bzero(obj->payload, obj->length);
		free(obj->payload);
	}

	libder_type_free(obj->type);
	free(obj);
}

static void
libder_obj_unlink(struct libder_object *obj)
{
	struct libder_object *child, *parent, *prev;

	parent = obj->parent;
	if (parent == NULL)
		return;

	prev = NULL;
	assert(parent->nchildren > 0);
	DER_FOREACH_CHILD(child, parent) {
		if (child == obj) {
			if (prev == NULL)
				parent->children = child->next;
			else
				prev->next = child->next;
			parent->nchildren--;
			child->parent = NULL;
			return;
		}

		prev = child;
	}

	assert(0 && "Internal inconsistency: parent set, but child not found");
}

bool
libder_obj_append(struct libder_object *parent, struct libder_object *child)
{
	struct libder_object *end, *walker;

	if (!parent->type->tag_constructed)
		return (false);

	/* XXX Type check */

	if (child->parent != NULL)
		libder_obj_unlink(child);

	if (parent->nchildren == 0) {
		parent->children = child;
		parent->nchildren++;
		return (true);
	}

	/* Walk the chain */
	DER_FOREACH_CHILD(walker, parent) {
		end = walker;
	}

	assert(end != NULL);
	end->next = child;
	parent->nchildren++;
	child->parent = parent;
	return (true);
}

struct libder_object *
libder_obj_child(const struct libder_object *obj, size_t idx)
{
	struct libder_object *cur;

	DER_FOREACH_CHILD(cur, obj) {
		if (idx-- == 0)
			return (cur);
	}

	return (NULL);
}

struct libder_object *
libder_obj_children(const struct libder_object *obj)
{

	return (obj->children);
}

struct libder_object *
libder_obj_next(const struct libder_object *obj)
{

	return (obj->next);
}

struct libder_tag *
libder_obj_type(const struct libder_object *obj)
{

	return (obj->type);
}

uint8_t
libder_obj_type_simple(const struct libder_object *obj)
{
	struct libder_tag *type = obj->type;
	uint8_t simple = type->tag_class << 6;

	if (type->tag_constructed)
		simple |= BER_TYPE_CONSTRUCTED_MASK;

	if (type->tag_encoded)
		simple |= 0x1f;	/* Encode the "long tag" tag. */
	else
		simple |= type->tag_short;
	return (simple);
}

const uint8_t *
libder_obj_data(const struct libder_object *obj, size_t *osz)
{

	if (obj->type->tag_constructed)
		return (NULL);

	*osz = obj->length;
	return (obj->payload);
}

static const char *
libder_type_name(const struct libder_tag *type)
{
	static char namebuf[128];

	if (type->tag_encoded) {
		return ("{ ... }");
	}

	if (type->tag_class != BC_UNIVERSAL)
		goto fallback;

#define	UTYPE(val)	case val: return (&(#val)[3])
	switch (type->tag_short) {
	UTYPE(BT_RESERVED);
	UTYPE(BT_BOOLEAN);
	UTYPE(BT_INTEGER);
	UTYPE(BT_BITSTRING);
	UTYPE(BT_OCTETSTRING);
	UTYPE(BT_NULL);
	UTYPE(BT_OID);
	UTYPE(BT_OBJDESC);
	UTYPE(BT_EXTERNAL);
	UTYPE(BT_REAL);
	UTYPE(BT_ENUMERATED);
	UTYPE(BT_PDV);
	UTYPE(BT_UTF8STRING);
	UTYPE(BT_RELOID);
	UTYPE(BT_NUMERICSTRING);
	UTYPE(BT_STRING);
	UTYPE(BT_TELEXSTRING);
	UTYPE(BT_VIDEOTEXSTRING);
	UTYPE(BT_IA5STRING);
	UTYPE(BT_UTCTIME);
	UTYPE(BT_GENTIME);
	UTYPE(BT_GFXSTRING);
	UTYPE(BT_VISSTRING);
	UTYPE(BT_GENSTRING);
	UTYPE(BT_UNIVSTRING);
	UTYPE(BT_CHARSTRING);
	UTYPE(BT_BMPSTRING);
	case BT_SEQUENCE & ~BER_TYPE_CONSTRUCTED_MASK:
	case BT_SEQUENCE: return "SEQUENCE";
	case BT_SET & ~BER_TYPE_CONSTRUCTED_MASK:
	case BT_SET: return "SET";
	}

fallback:
	snprintf(namebuf, sizeof(namebuf), "%.02x", libder_type_simple(type));
	return (&namebuf[0]);
}

static void
libder_obj_dump_internal(const struct libder_object *obj, FILE *fp, int lvl)
{
	static char spacer[4096];
	const struct libder_object *child;

	/* Primitive, goofy, but functional. */
	if (spacer[0] == '\0')
		memset(spacer, '\t', sizeof(spacer));

	if (lvl >= (int)sizeof(spacer)) {
		/* Too large, truncate the display. */
		fprintf(fp, "%.*s...\n", (int)sizeof(spacer), spacer);
		return;
	}

	if (obj->children == NULL) {
		size_t col = lvl * 8;

		col += fprintf(fp, "%.*sOBJECT[type=%s, size=%zx]%s",
		    lvl, spacer, libder_type_name(obj->type),
		    obj->length, obj->length != 0 ? ": " : "");

		if (obj->length != 0) {
			uint8_t printb;

#define	LIBDER_CONTENTS_WRAP	80
			for (size_t i = 0; i < obj->length; i++) {
				if (col + 3 >= LIBDER_CONTENTS_WRAP) {
					fprintf(fp, "\n%.*s    ", lvl, spacer);
					col = (lvl * 8) + 4;
				}

				if (obj->payload == NULL)
					printb = 0;
				else
					printb = obj->payload[i];

				col += fprintf(fp, "%.02x ", printb);
			}
		}

		fprintf(fp, "\n");

		return;
	}

	fprintf(fp, "%.*sOBJECT[type=%s]\n", lvl, spacer,
	    libder_type_name(obj->type));
	DER_FOREACH_CHILD(child, obj)
		libder_obj_dump_internal(child, fp, lvl + 1);
}

void
libder_obj_dump(const struct libder_object *root, FILE *fp)
{

	libder_obj_dump_internal(root, fp, 0);
}

LIBDER_PRIVATE bool
libder_is_valid_obj(struct libder_ctx *ctx, const struct libder_tag *type,
    const uint8_t *payload, size_t payloadsz, bool varlen)
{

	if (payload != NULL) {
		assert(payloadsz > 0);
		assert(!varlen);
	} else {
		assert(payloadsz == 0);
	}

	/* No rules for non-universal types. */
	if (type->tag_class != BC_UNIVERSAL || type->tag_encoded)
		return (true);

	if (ctx->strict && type->tag_constructed) {
		/* Types that don't allow constructed */
		switch (libder_type_simple(type) & ~BER_TYPE_CONSTRUCTED_MASK) {
		case BT_BOOLEAN:
		case BT_INTEGER:
		case BT_REAL:
		case BT_NULL:
			libder_set_error(ctx, LDE_STRICT_PRIMITIVE);
			return (false);
		default:
			break;
		}
	} else if (ctx->strict) {
		/* Types that cannot be primitive */
		switch (libder_type_simple(type) | BER_TYPE_CONSTRUCTED_MASK) {
		case BT_SEQUENCE:
		case BT_SET:
			libder_set_error(ctx, LDE_STRICT_CONSTRUCTED);
			return (false);
		default:
			break;
		}
	}

	/* Further validation */
	switch (libder_type_simple(type)) {
	case BT_BOOLEAN:
		if (ctx->strict && payloadsz != 1) {
			libder_set_error(ctx, LDE_STRICT_BOOLEAN);
			return (false);
		}
		break;
	case BT_NULL:
		if (ctx->strict && (payloadsz != 0 || varlen)) {
			libder_set_error(ctx, LDE_STRICT_NULL);
			return (false);
		}
		break;
	case BT_BITSTRING:	/* Primitive */
		/*
		 * Bit strings require more invasive parsing later during child
		 * coalescing or normalization, so we alway strictly enforce
		 * their form.
		 */
		if (payloadsz == 1 && payload[0] != 0)
			return (false);

		/* We can't have more than seven unused bits. */
		return (payloadsz < 2 || payload[0] < 8);
	case BT_RESERVED:
		if (payloadsz != 0) {
			libder_set_error(ctx, LDE_STRICT_EOC);
			return (false);
		}
		break;
	default:
		break;
	}

	return (true);
}

LIBDER_PRIVATE bool
libder_obj_may_coalesce_children(const struct libder_object *obj)
{

	/* No clue about non-universal types. */
	if (obj->type->tag_class != BC_UNIVERSAL || obj->type->tag_encoded)
		return (false);

	/* Constructed types don't have children. */
	if (!obj->type->tag_constructed)
		return (false);

	/* Strip the constructed bit off. */
	switch (libder_type_simple(obj->type)) {
	case BT_OCTETSTRING:	/* Raw data types */
	case BT_BITSTRING:
		return (true);
	case BT_UTF8STRING:	/* String types */
	case BT_NUMERICSTRING:
	case BT_STRING:
	case BT_TELEXSTRING:
	case BT_VIDEOTEXSTRING:
	case BT_IA5STRING:
	case BT_GFXSTRING:
	case BT_VISSTRING:
	case BT_GENSTRING:
	case BT_UNIVSTRING:
	case BT_CHARSTRING:
	case BT_BMPSTRING:
		return (true);
	case BT_UTCTIME:	/* Time types */
	case BT_GENTIME:
		return (true);
	default:
		return (false);
	}
}

static size_t
libder_merge_bitstrings(uint8_t *buf, size_t offset, size_t bufsz,
    const struct libder_object *child)
{
	const uint8_t *rhs = child->payload;
	size_t rsz = child->disk_size, startoff = offset;
	uint8_t rhsunused, unused;

	rhsunused = (rhs != NULL ? rhs[0] : 0);

	/* We have no unused bits if the buffer's empty as of yet. */
	if (offset == 0)
		unused = 0;
	else
		unused = buf[0];

	/* Shave the lead byte off if we have one. */
	if (rsz > 1) {
		if (rhs != NULL)
			rhs++;
		rsz--;
	}

	if (unused == 0) {
		size_t extra = 0;

		/*
		 * In all cases we'll just write the unused byte separately,
		 * since we're copying way past it in the common case and can't
		 * just overwrite it as part of the memcpy().
		 */
		if (offset == 0) {
			offset = 1;
			extra++;
		}

		assert(rhsunused < 8);
		assert(offset + rsz <= bufsz);

		buf[0] = rhsunused;
		if (rhs == NULL)
			memset(&buf[offset], 0, rsz);
		else
			memcpy(&buf[offset], rhs, rsz);

		return (rsz + extra);
	}

	for (size_t i = 0; i < rsz; i++) {
		uint8_t bits, next;

		if (rhs == NULL)
			next = 0;
		else
			next = rhs[i];

		/* Rotate the leading bits into the byte before it. */
		assert(unused < 8);
		bits = next >> (8 - unused);
		buf[offset - 1] |= bits;

		next <<= unused;

		/*
		 * Copy the new valid bits in; we shift over the old unused
		 * amount up until the very last bit, then we have to recalculate
		 * because we may be dropping it entirely.
		 */
		if (i == rsz - 1) {
			assert(rhsunused < 8);

			/*
			 * Figure out how many unused bits we have between the two
			 * buffers, sum % 8 is the new # unused bits.  It will be
			 * somewhere in the range of [0, 14], and if it's at or
			 * higher than a single byte then that's a clear indicator
			 * that we shifted some unused bits into the previous byte and
			 * can just halt here.
			 */
			unused += rhsunused;
			buf[0] = unused % 8;
			if (unused >= 8)
				break;
		}

		assert(offset < bufsz);
		buf[offset++] = next;
	}

	return (offset - startoff);
}

LIBDER_PRIVATE bool
libder_obj_coalesce_children(struct libder_object *obj, struct libder_ctx *ctx)
{
	struct libder_object *child, *last_child, *tmp;
	size_t new_size = 0, offset = 0;
	uint8_t *coalesced_data;
	uint8_t type;
	bool need_payload = false, strict_violation = false;

	if (obj->nchildren == 0 || !libder_obj_may_coalesce_children(obj))
		return (true);

	assert(obj->type->tag_class == BC_UNIVERSAL);
	assert(obj->type->tag_constructed);
	assert(!obj->type->tag_encoded);
	type = obj->type->tag_short;

	last_child = NULL;
	DER_FOREACH_CHILD(child, obj) {
		/* Sanity check and coalesce our children. */
		if (child->type->tag_class != BC_UNIVERSAL ||
		    child->type->tag_short != obj->type->tag_short) {
			libder_set_error(ctx, LDE_COALESCE_BADCHILD);
			return (false);
		}

		/* Recursively coalesce everything. */
		if (!libder_obj_coalesce_children(child, ctx))
			return (false);

		/*
		 * The child node will be disappearing anyways, so we stash the
		 * disk size sans header in its disk_size to reuse in the later
		 * loop.
		 */
		child->disk_size = libder_obj_disk_size(child, false);

		/*
		 * We strip the lead byte off of every element, and add it back
		 * in pre-allocation.
		 */
		if (type == BT_BITSTRING && child->disk_size > 1)
			child->disk_size--;
		if (child->disk_size > 0)
			last_child = child;

		new_size += child->disk_size;

		if (child->payload != NULL)
			need_payload = true;
	}

	if (new_size != 0 && need_payload) {
		if (type == BT_BITSTRING)
			new_size++;
		coalesced_data = malloc(new_size);
		if (coalesced_data == NULL) {
			libder_set_error(ctx, LDE_NOMEM);
			return (false);
		}
	} else {
		/*
		 * This would perhaps be a bit weird, but that's normalization
		 * for you.  We shouldn't really have a UTF-8 string that's
		 * composed of a series of zero-length UTF-8 strings, but
		 * weirder things have happened.
		 */
		coalesced_data = NULL;
	}

	/* Avoid leaking any children as we coalesce. */
	DER_FOREACH_CHILD_SAFE(child, obj, tmp) {
		if (child->disk_size != 0)
			assert(coalesced_data != NULL || !need_payload);

		/*
		 * Just free everything when we violate strict rules.
		 */
		if (strict_violation)
			goto violated;

		if (child->disk_size != 0 && need_payload) {
			assert(coalesced_data != NULL);
			assert(offset + child->disk_size <= new_size);

			/*
			 * Bit strings are special, in that the first byte
			 * contains the number of unused bits at the end.  We
			 * need to trim that off when concatenating bit strings
			 */
			if (type == BT_BITSTRING) {
				if (ctx->strict && child != last_child &&
				    child->disk_size > 1 && child->payload != NULL) {
					/*
					 * Each child must have a multiple of 8,
					 * up until the final one.
					 */
					if (child->payload[0] != 0) {
						libder_set_error(ctx, LDE_STRICT_BITSTRING);
						strict_violation = true;
						goto violated;
					}
				}

				offset += libder_merge_bitstrings(coalesced_data,
				    offset, new_size, child);
			} else {
				/*
				 * Write zeroes out if we don't have a payload.
				 */
				if (child->payload == NULL) {
					memset(&coalesced_data[offset], 0, child->disk_size);
					offset += child->disk_size;
				} else {
					memcpy(&coalesced_data[offset], child->payload,
					    child->disk_size);
					offset += child->disk_size;
				}
			}
		}

violated:
		libder_obj_free(child);
	}

	assert(offset <= new_size);

	/* Zap the children, we've absorbed their bodies. */
	obj->children = NULL;

	if (strict_violation) {
		if (coalesced_data != NULL) {
			libder_bzero(coalesced_data, offset);
			free(coalesced_data);
		}

		return (false);
	}

	/* Finally, swap out the payload. */
	if (obj->payload != NULL) {
		libder_bzero(obj->payload, obj->length);
		free(obj->payload);
	}

	obj->length = offset;
	obj->payload = coalesced_data;
	obj->type->tag_constructed = false;

	return (true);
}

static bool
libder_obj_normalize_bitstring(struct libder_object *obj)
{
	uint8_t *payload = obj->payload;
	size_t length = obj->length;
	uint8_t unused;

	if (payload == NULL || length < 2)
		return (true);

	unused = payload[0];
	if (unused == 0)
		return (true);

	/* Clear the unused bits completely. */
	payload[length - 1] &= ~((1 << unused) - 1);
	return (true);
}

static bool
libder_obj_normalize_boolean(struct libder_object *obj)
{
	uint8_t *payload = obj->payload;
	size_t length = obj->length;
	int sense = 0;

	assert(length > 0);

	/*
	 * Booleans must be collapsed down to a single byte, 0x00 or 0xff,
	 * indicating false or true respectively.
	 */
	if (length == 1 && (payload[0] == 0x00 || payload[0] == 0xff))
		return (true);

	for (size_t bpos = 0; bpos < length; bpos++) {
		sense |= payload[bpos];
		if (sense != 0)
			break;
	}

	payload[0] = sense != 0 ? 0xff : 0x00;
	obj->length = 1;
	return (true);
}

static bool
libder_obj_normalize_integer(struct libder_object *obj)
{
	uint8_t *payload = obj->payload;
	size_t length = obj->length;
	size_t strip = 0;

	/*
	 * Strip any leading sign-extended looking bytes, but note that
	 * we can't strip a leading byte unless it matches the sign bit
	 * on the next byte.
	 */
	for (size_t bpos = 0; bpos < length - 1; bpos++) {
		if (payload[bpos] != 0 && payload[bpos] != 0xff)
			break;

		if (payload[bpos] == 0xff) {
			/* Only if next byte indicates signed. */
			if ((payload[bpos + 1] & 0x80) == 0)
				break;
		} else {
			/* Only if next byte indicates unsigned. */
			if ((payload[bpos + 1] & 0x80) != 0)
				break;
		}

		strip++;
	}

	if (strip != 0) {
		payload += strip;
		length -= strip;

		memmove(&obj->payload[0], payload, length);
		obj->length = length;
	}

	return (true);
}

static int
libder_obj_tag_compare(const struct libder_tag *lhs, const struct libder_tag *rhs)
{
	const uint8_t *lbits, *rbits;
	size_t delta, end, lsz, rsz;
	uint8_t lbyte, rbyte;

	/* Highest bits: tag class, libder_ber_class has the same bit ordering. */
	if (lhs->tag_class < rhs->tag_class)
		return (-1);
	if (lhs->tag_class > rhs->tag_class)
		return (1);

	/* Next bit: constructed vs. primitive */
	if (!lhs->tag_constructed && rhs->tag_constructed)
		return (-1);
	if (lhs->tag_constructed && rhs->tag_constructed)
		return (1);

	/*
	 * Finally: tag data; we can use the size as a first-order heuristic
	 * because we store tags in the shortest possible representation.
	 */
	if (lhs->tag_size < rhs->tag_size)
		return (-1);
	else if (lhs->tag_size > rhs->tag_size)
		return (1);

	if (!lhs->tag_encoded) {
		lbits = (const void *)&lhs->tag_short;
		lsz = sizeof(uint64_t);
	} else {
		lbits = lhs->tag_long;
		lsz = lhs->tag_size;
	}

	if (!rhs->tag_encoded) {
		rbits = (const void *)&rhs->tag_short;
		rsz = sizeof(uint64_t);
	} else {
		rbits = rhs->tag_long;
		rsz = rhs->tag_size;
	}

	delta = 0;
	end = MAX(lsz, rsz);
	if (lsz > rsz)
		delta = lsz - rsz;
	else if (lsz < rsz)
		delta = rsz - lsz;
	for (size_t i = 0; i < end; i++) {
		/* Zero-extend the short one the difference. */
		if (lsz < rsz && i < delta)
			lbyte = 0;
		else
			lbyte = lbits[i - delta];

		if (lsz > rsz && i < delta)
			rbyte = 0;
		else
			rbyte = rbits[i - delta];

		if (lbyte < rbyte)
			return (-1);
		else if (lbyte > rbyte)
			return (-1);
	}

	return (0);
}

/*
 * Similar to strcmp(), returns -1, 0, or 1.
 */
static int
libder_obj_compare(const struct libder_object *lhs, const struct libder_object *rhs)
{
	size_t end;
	int cmp;
	uint8_t lbyte, rbyte;

	cmp = libder_obj_tag_compare(lhs->type, rhs->type);
	if (cmp != 0)
		return (cmp);

	/*
	 * We'll compare up to the longer of the two; the shorter payload is
	 * zero-extended at the end for comparison purposes.
	 */
	end = MAX(lhs->length, rhs->length);
	for (size_t pos = 0; pos < end; pos++) {
		if (lhs->payload != NULL && pos < lhs->length)
			lbyte = lhs->payload[pos];
		else
			lbyte = 0;
		if (rhs->payload != NULL && pos < rhs->length)
			rbyte = rhs->payload[pos];
		else
			rbyte = 0;

		if (lbyte < rbyte)
			return (-1);
		else if (lbyte > rbyte)
			return (1);
	}

	return (0);
}

static int
libder_obj_normalize_set_cmp(const void *lhs_entry, const void *rhs_entry)
{
	const struct libder_object *lhs =
	    *__DECONST(const struct libder_object **, lhs_entry);
	const struct libder_object *rhs =
	    *__DECONST(const struct libder_object **, rhs_entry);

	return (libder_obj_compare(lhs, rhs));
}

static bool
libder_obj_normalize_set(struct libder_object *obj, struct libder_ctx *ctx)
{
	struct libder_object **sorting;
	struct libder_object *child;
	size_t offset = 0;

	if (obj->nchildren < 2)
		return (true);

	/*
	 * Kind of goofy, but we'll just take advantage of a standardized
	 * qsort() rather than rolling our own sort -- we have no idea how large
	 * of a dataset we're working with.
	 */
	sorting = calloc(obj->nchildren, sizeof(*sorting));
	if (sorting == NULL) {
		libder_set_error(ctx, LDE_NOMEM);
		return (false);
	}

	DER_FOREACH_CHILD(child, obj) {
		sorting[offset++] = child;
	}

	assert(offset == obj->nchildren);
	qsort(sorting, offset, sizeof(*sorting), libder_obj_normalize_set_cmp);

	obj->children = sorting[0];
	sorting[offset - 1]->next = NULL;
	for (size_t i = 0; i < offset - 1; i++) {
		sorting[i]->next = sorting[i + 1];
	}

	free(sorting);

	return (true);
}

LIBDER_PRIVATE bool
libder_obj_normalize(struct libder_object *obj, struct libder_ctx *ctx)
{
	uint8_t *payload = obj->payload;
	size_t length = obj->length;

	if (obj->type->tag_constructed) {
		/*
		 * For constructed types, we'll see if we can coalesce their
		 * children into them, then we'll proceed with whatever normalization
		 * rules we can apply to the children.
		 */
		if (DER_NORMALIZING(ctx, CONSTRUCTED) && !libder_obj_coalesce_children(obj, ctx))
			return (false);

		/*
		 * We may not be a constructed object anymore after the above coalescing
		 * happened, so we check it again here.  Constructed objects need not go
		 * any further, but the now-primitive coalesced types still need to be
		 * normalized.
		 */
		if (obj->type->tag_constructed) {
			struct libder_object *child;

			DER_FOREACH_CHILD(child, obj) {
				if (!libder_obj_normalize(child, ctx))
					return (false);
			}

			/* Sets must be sorted. */
			if (obj->type->tag_short != BT_SET)
				return (true);

			return (libder_obj_normalize_set(obj, ctx));
		}
	}

	/* We only have normalization rules for universal types. */
	if (obj->type->tag_class != BC_UNIVERSAL || obj->type->tag_encoded)
		return (true);

	if (!libder_normalizing_type(ctx, obj->type))
		return (true);

	/*
	 * We are clear to normalize this object, check for some easy cases that
	 * don't need normalization.
	 */
	switch (libder_type_simple(obj->type)) {
	case BT_BITSTRING:
	case BT_BOOLEAN:
	case BT_INTEGER:
		/*
		 * If we have a zero payload, then we need to encode them as a
		 * single zero byte.
		 */
		if (payload == NULL) {
			if (length != 1)
				obj->length = 1;

			return (true);
		}

		break;
	case BT_NULL:
		if (payload != NULL) {
			free(payload);

			obj->payload = NULL;
			obj->length = 0;
		}

		return (true);
	default:
		/*
		 * If we don't have a payload, we'll just leave it alone.
		 */
		if (payload == NULL)
			return (true);
		break;
	}

	switch (libder_type_simple(obj->type)) {
	case BT_BITSTRING:
		return (libder_obj_normalize_bitstring(obj));
	case BT_BOOLEAN:
		return (libder_obj_normalize_boolean(obj));
	case BT_INTEGER:
		return (libder_obj_normalize_integer(obj));
	default:
		break;
	}

	return (true);
}
