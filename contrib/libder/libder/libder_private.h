/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sys/param.h>

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#ifdef __APPLE__
#define	__STDC_WANT_LIB_EXT1__	1	/* memset_s */
#endif
/* explicit_bzero is in one of these... */
#include <string.h>
#include <strings.h>
#include "libder.h"

/* FreeBSD's sys/cdefs.h */
#ifndef __DECONST
#define	__DECONST(type, var)	((type)(uintptr_t)(const void *)(var))
#endif
#ifndef __unused
#define	__unused		__attribute__((__unused__))
#endif

/* FreeBSD's sys/params.h */
#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif
#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif

struct libder_ctx;
struct libder_object;

struct libder_ctx {
	uint64_t		 normalize;
	size_t			 buffer_size;
	enum libder_error	 error;
	int			 verbose;
	bool			 strict;
	volatile sig_atomic_t	 abort;
};

struct libder_tag {
	union {
		uint8_t		 tag_short;
		uint8_t		*tag_long;
	};
	size_t			 tag_size;
	enum libder_ber_class	 tag_class;
	bool			 tag_constructed;
	bool			 tag_encoded;
};

struct libder_object {
	struct libder_tag	*type;
	size_t			 length;
	size_t			 nchildren;
	size_t			 disk_size;
	uint8_t			*payload;	/* NULL for sequences */
	struct libder_object	*children;
	struct libder_object	*parent;
	struct libder_object	*next;
};

static inline sig_atomic_t
libder_check_abort(struct libder_ctx *ctx)
{

	return (ctx->abort);
}

static inline void
libder_clear_abort(struct libder_ctx *ctx)
{

	ctx->abort = 1;
}

#define	LIBDER_PRIVATE	__attribute__((__visibility__("hidden")))

#define	DER_NORMALIZING(ctx, bit)	\
    (((ctx)->normalize & (LIBDER_NORMALIZE_ ## bit)) != 0)

static inline bool
libder_normalizing_type(const struct libder_ctx *ctx, const struct libder_tag *type)
{
	uint8_t tagval;

	assert(!type->tag_constructed);
	assert(!type->tag_encoded);
	assert(type->tag_class == BC_UNIVERSAL);
	assert(type->tag_short < 0x1f);

	tagval = type->tag_short;
	return ((ctx->normalize & LIBDER_NORMALIZE_TYPE_FLAG(tagval)) != 0);
}

/* All of the lower bits set. */
#define	BER_TYPE_LONG_MASK	0x1f

/*
 * Check if the type matches one of our universal types.
 */
static inline bool
libder_type_is(const struct libder_tag *type, uint8_t utype)
{

	if (type->tag_class != BC_UNIVERSAL || type->tag_encoded)
		return (false);
	if ((utype & BER_TYPE_CONSTRUCTED_MASK) != type->tag_constructed)
		return (false);

	utype &= ~BER_TYPE_CONSTRUCTED_MASK;
	return (utype == type->tag_short);
}

/*
 * We'll use this one a decent amount, so we'll keep it inline.  There's also
 * an _abi version that we expose as public interface via a 'libder_type_simple'
 * macro.
 */
#undef libder_type_simple

static inline uint8_t
libder_type_simple(const struct libder_tag *type)
{
	uint8_t encoded = type->tag_class << 6;

	assert(!type->tag_encoded);
	if (type->tag_constructed)
		encoded |= BER_TYPE_CONSTRUCTED_MASK;

	encoded |= type->tag_short;
	return (encoded);
}

static inline void
libder_bzero(uint8_t *buf, size_t bufsz)
{

#ifdef __APPLE__
	memset_s(buf, bufsz, 0, bufsz);
#else
	explicit_bzero(buf, bufsz);
#endif
}

size_t	 libder_get_buffer_size(struct libder_ctx *);
void	 libder_set_error(struct libder_ctx *, int, const char *, int);

#define	libder_set_error(ctx, error)	\
	libder_set_error((ctx), (error), __FILE__, __LINE__)

struct libder_object	*libder_obj_alloc_internal(struct libder_ctx *,
			    struct libder_tag *, uint8_t *, size_t, uint32_t);
#define	LDO_OWNTAG	0x0001	/* Object owns passed in tag */

size_t			 libder_size_length(size_t);
bool			 libder_is_valid_obj(struct libder_ctx *,
			    const struct libder_tag *, const uint8_t *, size_t, bool);
size_t			 libder_obj_disk_size(struct libder_object *, bool);
bool			 libder_obj_may_coalesce_children(const struct libder_object *);
bool			 libder_obj_coalesce_children(struct libder_object *, struct libder_ctx *);
bool			 libder_obj_normalize(struct libder_object *, struct libder_ctx *);

struct libder_tag	*libder_type_alloc(void);
void			 libder_type_release(struct libder_tag *);
void			 libder_normalize_type(struct libder_ctx *, struct libder_tag *);
