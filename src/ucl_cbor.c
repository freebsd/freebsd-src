/*
 * Copyright (c) 2026, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file ucl_cbor.c
 * CBOR (RFC 8949) reader and writer.
 *
 * The shape of this file mirrors ucl_msgpack.c: the emitter half exports a
 * handful of ucl_emitter_print_*_cbor entry points that ucl_emitter.c calls
 * through its ops table, and the parser half exports ucl_parse_cbor, driven
 * from ucl_parser_add_chunk_full.
 *
 * CBOR encodes every data item the same way - a head byte carrying a 3 bit
 * major type and 5 bits of additional information, optionally followed by an
 * argument of 1, 2, 4 or 8 bytes - so the msgpack prefix search table has no
 * counterpart here. What CBOR adds instead is indefinite length strings and
 * containers terminated by a break stop code, which the parser below handles
 * with an UCL_CBOR_INDEFINITE sentinel in the stack frame's element counter.
 *
 * Deviations from the format worth knowing about:
 *   - the top level item must be an array or a map, as it must be for
 *     msgpack, because ucl has nowhere to put a bare scalar;
 *   - tags (major type 6) are dropped and the item they wrap is decoded in
 *     their place, the way msgpack extension types are skipped;
 *   - map keys may be text strings, byte strings or integers; an integer key
 *     becomes its decimal spelling, since ucl keys are strings;
 *   - simple values other than false/true/null decode to a null object, as
 *     none of them carry anything ucl can represent.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ucl.h"
#include "ucl_internal.h"

/* Major types, in the high 3 bits of the head byte */
#define UCL_CBOR_UINT 0
#define UCL_CBOR_NINT 1
#define UCL_CBOR_BYTES 2
#define UCL_CBOR_TEXT 3
#define UCL_CBOR_ARRAY 4
#define UCL_CBOR_MAP 5
#define UCL_CBOR_TAG 6
#define UCL_CBOR_SIMPLE 7

/* Additional information values that mean something other than a small int */
#define UCL_CBOR_AI_1 24
#define UCL_CBOR_AI_2 25
#define UCL_CBOR_AI_4 26
#define UCL_CBOR_AI_8 27
#define UCL_CBOR_AI_INDEFINITE 31

/* Simple values */
#define UCL_CBOR_FALSE 20
#define UCL_CBOR_TRUE 21
#define UCL_CBOR_NULL 22
#define UCL_CBOR_UNDEFINED 23

/* The whole head byte for a few items we match or write literally */
#define UCL_CBOR_BREAK_BYTE 0xff
#define UCL_CBOR_FALSE_BYTE 0xf4
#define UCL_CBOR_TRUE_BYTE 0xf5
#define UCL_CBOR_NULL_BYTE 0xf6
#define UCL_CBOR_FLOAT32_BYTE 0xfa
#define UCL_CBOR_FLOAT64_BYTE 0xfb

/*
 * Stands in for the element count of a container that runs until a break
 * stop code. UINT64_MAX cannot collide with a real count: a definite length
 * container needs at least one input byte per element, so its count is always
 * bounded by the size of the chunk.
 */
#define UCL_CBOR_INDEFINITE UINT64_MAX

/*
 * Longest decimal spelling of an int64 plus the sign and the terminator
 */
#define UCL_CBOR_INTKEY_MAX 24

static inline void
ucl_cbor_put_be(unsigned char *buf, uint64_t val, unsigned nbytes)
{
	while (nbytes-- > 0) {
		buf[0] = (unsigned char) ((val >> (nbytes * 8)) & 0xff);
		buf++;
	}
}

static inline uint64_t
ucl_cbor_get_be(const unsigned char *buf, unsigned nbytes)
{
	uint64_t res = 0;

	while (nbytes-- > 0) {
		res = (res << 8) | (uint64_t) *buf++;
	}

	return res;
}

/*
 * Write the head of a data item: the major type together with its argument,
 * in the shortest of the five encodings that fits. Returns the number of
 * bytes written, at most 9.
 */
static unsigned
ucl_cbor_put_head(unsigned char *buf, unsigned char major, uint64_t arg)
{
	unsigned char mt = (unsigned char) (major << 5);

	if (arg < UCL_CBOR_AI_1) {
		buf[0] = mt | (unsigned char) arg;

		return 1;
	}
	else if (arg <= 0xffULL) {
		buf[0] = mt | UCL_CBOR_AI_1;
		buf[1] = (unsigned char) arg;

		return 2;
	}
	else if (arg <= 0xffffULL) {
		buf[0] = mt | UCL_CBOR_AI_2;
		ucl_cbor_put_be(&buf[1], arg, 2);

		return 3;
	}
	else if (arg <= 0xffffffffULL) {
		buf[0] = mt | UCL_CBOR_AI_4;
		ucl_cbor_put_be(&buf[1], arg, 4);

		return 5;
	}

	buf[0] = mt | UCL_CBOR_AI_8;
	ucl_cbor_put_be(&buf[1], arg, 8);

	return 9;
}

static inline void
ucl_cbor_emit_head(struct ucl_emitter_context *ctx, unsigned char major,
				   uint64_t arg)
{
	const struct ucl_emitter_functions *func = ctx->func;
	unsigned char buf[9];
	unsigned len;

	len = ucl_cbor_put_head(buf, major, arg);
	func->ucl_emitter_append_len(buf, len, func->ud);
}

void ucl_emitter_print_int_cbor(struct ucl_emitter_context *ctx, int64_t val)
{
	if (val >= 0) {
		ucl_cbor_emit_head(ctx, UCL_CBOR_UINT, (uint64_t) val);
	}
	else {
		/*
		 * A negative integer is stored as -1 - n, so the argument stays
		 * non-negative even for INT64_MIN, where -(val + 1) is INT64_MAX
		 */
		ucl_cbor_emit_head(ctx, UCL_CBOR_NINT, (uint64_t) (-(val + 1)));
	}
}

void ucl_emitter_print_double_cbor(struct ucl_emitter_context *ctx, double val)
{
	const struct ucl_emitter_functions *func = ctx->func;
	union {
		double d;
		uint64_t i;
	} d64;
	union {
		float f;
		uint32_t i;
	} f32;
	unsigned char buf[9];

	f32.f = (float) val;

	if ((double) f32.f == val) {
		/*
		 * The value survives the round trip through single precision, so
		 * spend four bytes on it instead of eight. A NaN never takes this
		 * branch, as it compares unequal to itself.
		 */
		buf[0] = UCL_CBOR_FLOAT32_BYTE;
		ucl_cbor_put_be(&buf[1], f32.i, 4);
		func->ucl_emitter_append_len(buf, 5, func->ud);
	}
	else {
		d64.d = val;
		buf[0] = UCL_CBOR_FLOAT64_BYTE;
		ucl_cbor_put_be(&buf[1], d64.i, 8);
		func->ucl_emitter_append_len(buf, 9, func->ud);
	}
}

void ucl_emitter_print_bool_cbor(struct ucl_emitter_context *ctx, bool val)
{
	const struct ucl_emitter_functions *func = ctx->func;

	func->ucl_emitter_append_character(val ? UCL_CBOR_TRUE_BYTE : UCL_CBOR_FALSE_BYTE,
									   1, func->ud);
}

void ucl_emitter_print_string_cbor(struct ucl_emitter_context *ctx,
								   const char *s, size_t len)
{
	const struct ucl_emitter_functions *func = ctx->func;

	ucl_cbor_emit_head(ctx, UCL_CBOR_TEXT, (uint64_t) len);

	if (len > 0) {
		func->ucl_emitter_append_len((const unsigned char *) s, len, func->ud);
	}
}

void ucl_emitter_print_binary_string_cbor(struct ucl_emitter_context *ctx,
										  const char *s, size_t len)
{
	const struct ucl_emitter_functions *func = ctx->func;

	ucl_cbor_emit_head(ctx, UCL_CBOR_BYTES, (uint64_t) len);

	if (len > 0) {
		func->ucl_emitter_append_len((const unsigned char *) s, len, func->ud);
	}
}

void ucl_emitter_print_null_cbor(struct ucl_emitter_context *ctx)
{
	const struct ucl_emitter_functions *func = ctx->func;

	func->ucl_emitter_append_character(UCL_CBOR_NULL_BYTE, 1, func->ud);
}

void ucl_emitter_print_key_cbor(bool print_key, struct ucl_emitter_context *ctx,
								const ucl_object_t *obj)
{
	if (print_key) {
		ucl_emitter_print_string_cbor(ctx, obj->key, obj->keylen);
	}
}

void ucl_emitter_print_array_cbor(struct ucl_emitter_context *ctx, size_t len)
{
	ucl_cbor_emit_head(ctx, UCL_CBOR_ARRAY, (uint64_t) len);
}

void ucl_emitter_print_object_cbor(struct ucl_emitter_context *ctx, size_t len)
{
	ucl_cbor_emit_head(ctx, UCL_CBOR_MAP, (uint64_t) len);
}


/*
 * Parser
 */

struct ucl_cbor_head {
	uint64_t arg;        /* Argument, or the small value held in `ai`	*/
	unsigned char major; /* Major type, 0 to 7							*/
	unsigned char ai;    /* Additional information, 0 to 31				*/
	bool indefinite;     /* `ai` was 31								*/
	unsigned hdrlen;     /* Bytes the head occupies, 1 to 9				*/
};

/*
 * Turn a half precision float into a double without leaning on <math.h>:
 * every case is a straight rearrangement of the bits.
 */
static double
ucl_cbor_half_to_double(uint16_t h)
{
	union {
		uint64_t i;
		double d;
	} u;
	uint64_t sign = (uint64_t) (h >> 15) & 1;
	uint32_t exp = (h >> 10) & 0x1f;
	uint64_t mant = h & 0x3ff;

	if (exp == 0) {
		if (mant == 0) {
			/* Signed zero */
			u.i = sign << 63;
		}
		else {
			/*
			 * Subnormal half. Shift the mantissa up until the leading one
			 * appears where the double's implicit one lives: after `shift`
			 * steps the value reads 1.f x 2^(-14 - shift).
			 */
			unsigned shift = 0;

			while (!(mant & 0x400)) {
				mant <<= 1;
				shift++;
			}

			mant &= 0x3ff;
			u.i = (sign << 63) |
				  ((uint64_t) (1009 - shift) << 52) |
				  (mant << 42);
		}
	}
	else if (exp == 0x1f) {
		/* Infinity or NaN; the quiet bit lands on the double's quiet bit */
		u.i = (sign << 63) | (0x7ffULL << 52) | (mant << 42);
	}
	else {
		u.i = (sign << 63) |
			  ((uint64_t) (exp - 15 + 1023) << 52) |
			  (mant << 42);
	}

	return u.d;
}

/*
 * Create an object for a cbor element, charging it against the parser budgets
 * first. As with msgpack, a whole container can cost a single input byte, so
 * the input amplifies far harder than text does and needs the accounting.
 */
static inline ucl_object_t *
ucl_cbor_new_object(struct ucl_parser *parser, ucl_type_t type)
{
	ucl_object_t *obj;

	if (!ucl_parser_account_node(parser)) {
		return NULL;
	}

	obj = ucl_object_new_full(type, parser->chunks->priority);

	if (obj == NULL) {
		ucl_create_err(&parser->err, "no memory for a cbor object");
	}

	return obj;
}

/*
 * Decode the head of a data item at `p`. Everything downstream relies on this
 * having validated that `hdrlen` bytes are actually there.
 */
static bool
ucl_cbor_read_head(struct ucl_parser *parser, const unsigned char *p,
				   size_t remain, struct ucl_cbor_head *head)
{
	unsigned char b;
	unsigned nbytes;

	if (remain == 0) {
		ucl_create_err(&parser->err, "truncated cbor input");

		return false;
	}

	b = *p;
	head->major = b >> 5;
	head->ai = b & 0x1f;
	head->indefinite = false;
	head->arg = 0;

	if (head->ai < UCL_CBOR_AI_1) {
		head->arg = head->ai;
		head->hdrlen = 1;

		return true;
	}

	if (head->ai == UCL_CBOR_AI_INDEFINITE) {
		/*
		 * Strings and containers may run until a break stop code; for the
		 * simple type this byte *is* the break stop code. The remaining
		 * major types have nothing to leave open.
		 */
		if (head->major == UCL_CBOR_UINT || head->major == UCL_CBOR_NINT ||
			head->major == UCL_CBOR_TAG) {
			ucl_create_err(&parser->err,
						   "cbor major type %u cannot have an indefinite length",
						   (unsigned) head->major);

			return false;
		}

		head->indefinite = true;
		head->arg = UCL_CBOR_INDEFINITE;
		head->hdrlen = 1;

		return true;
	}

	if (head->ai > UCL_CBOR_AI_8) {
		ucl_create_err(&parser->err,
					   "reserved cbor additional information: %u",
					   (unsigned) head->ai);

		return false;
	}

	/* 24 -> 1 byte, 25 -> 2, 26 -> 4, 27 -> 8 */
	nbytes = 1u << (head->ai - UCL_CBOR_AI_1);

	if (remain < (size_t) nbytes + 1) {
		ucl_create_err(&parser->err,
					   "truncated cbor head: %ju bytes remain, %u needed",
					   (uintmax_t) remain, nbytes + 1);

		return false;
	}

	head->arg = ucl_cbor_get_be(p + 1, nbytes);
	head->hdrlen = nbytes + 1;

	return true;
}

/*
 * Push a container onto the parser stack, honouring the depth limit.
 * `nitems` counts elements for an array and key/value pairs for a map, or is
 * UCL_CBOR_INDEFINITE when the container runs until a break.
 */
static bool
ucl_cbor_push_container(struct ucl_parser *parser, ucl_object_t *obj,
						uint64_t nitems)
{
	struct ucl_stack *frame;

	if (parser->limits.max_depth > 0 &&
		(uint64_t) parser->cur_depth >= parser->limits.max_depth) {
		ucl_create_err(&parser->err,
					   "cbor containers are nested too deep (over %ju)",
					   (uintmax_t) parser->limits.max_depth);
		parser->err_code = UCL_ENESTED;

		return false;
	}

	/*
	 * Popped by ucl_parser_pop_container and by ucl_parser_free, both of
	 * which release the frame with UCL_FREE, so it has to come from
	 * UCL_ALLOC. UCL_ALLOC has no zeroing counterpart, hence the memset.
	 */
	frame = UCL_ALLOC(sizeof(struct ucl_stack));

	if (frame == NULL) {
		ucl_create_err(&parser->err, "no memory");
		parser->err_code = UCL_EINTERNAL;

		return false;
	}

	memset(frame, 0, sizeof(struct ucl_stack));
	frame->chunk = parser->chunks;
	frame->obj = obj;
	frame->e.len = nitems;
	frame->next = parser->stack;
	parser->stack = frame;
	parser->cur_depth++;

	return true;
}

/*
 * Insert a finished value into the container on top of the stack. On failure
 * the object is released here, so callers never have to unwind it themselves.
 * `key_is_temp` marks a key that lives in scratch storage and therefore has
 * to be copied even in zero-copy mode. `inserted`, when not NULL, receives
 * the object carrying the value once the insertion is done: merging a
 * duplicate key, or losing a priority contest to an older one, can hand that
 * role to an existing object and release the fresh one, so a caller that
 * goes on using the value - a container receiving more elements - has to
 * switch over to it.
 */
static bool
ucl_cbor_insert_object(struct ucl_parser *parser, const unsigned char *key,
					   size_t keylen, bool key_is_temp, ucl_object_t *obj,
					   ucl_object_t **inserted)
{
	struct ucl_stack *container = parser->stack;

	assert(container != NULL);
	assert(container->obj != NULL);
	assert(obj != NULL);

	if (container->obj->type == UCL_ARRAY) {
		ucl_array_append(container->obj, obj);

		if (inserted != NULL) {
			*inserted = obj;
		}
	}
	else if (container->obj->type == UCL_OBJECT) {
		if (key == NULL || keylen == 0) {
			ucl_create_err(&parser->err, "cannot insert a cbor value with no key");
			ucl_object_unref(obj);

			return false;
		}

		obj->key = (const char *) key;
		obj->keylen = keylen;

		if (key_is_temp || !(parser->flags & UCL_PARSER_ZEROCOPY)) {
			if (!ucl_parser_account_alloc(parser, (uint64_t) keylen + 1)) {
				ucl_object_unref(obj);

				return false;
			}

			if (ucl_copy_key_trash(obj) == NULL) {
				ucl_create_err(&parser->err, "cannot allocate memory for a key");
				ucl_object_unref(obj);

				return false;
			}
		}

		if (!ucl_parser_process_object_element(parser, obj,
											   obj->type == UCL_OBJECT ||
												   obj->type == UCL_ARRAY)) {
			/* None of its failure paths take ownership of the element */
			ucl_object_unref(obj);

			return false;
		}

		/*
		 * process_object_element leaves the surviving object in cur_obj,
		 * which may not be the one handed in once a merge has taken it
		 */
		if (inserted != NULL) {
			*inserted = parser->cur_obj;
		}
	}
	else {
		ucl_create_err(&parser->err, "bad cbor container type");
		ucl_object_unref(obj);

		return false;
	}

	if (container->e.len != UCL_CBOR_INDEFINITE) {
		container->e.len--;
	}

	return true;
}

/*
 * Walk the chunks of an indefinite length string, checking that each one is a
 * definite length string of the same major type. Returns the number of input
 * bytes the whole item spans, break stop code included, or -1 on a malformed
 * or truncated item, and reports the concatenated payload size in `total`.
 */
static ssize_t
ucl_cbor_scan_string_chunks(struct ucl_parser *parser, unsigned char major,
							const unsigned char *p, size_t remain, size_t *total)
{
	const unsigned char *start = p;
	size_t acc = 0;

	for (;;) {
		struct ucl_cbor_head chunk;
		size_t taken;

		if (remain == 0) {
			ucl_create_err(&parser->err,
						   "unterminated indefinite length cbor string");

			return -1;
		}

		if (*p == UCL_CBOR_BREAK_BYTE) {
			p++;
			break;
		}

		if (!ucl_cbor_read_head(parser, p, remain, &chunk)) {
			return -1;
		}

		if (chunk.major != major || chunk.indefinite) {
			ucl_create_err(&parser->err,
						   "indefinite length cbor string holds a chunk of "
						   "the wrong type");

			return -1;
		}

		if (chunk.arg > (uint64_t) (remain - chunk.hdrlen)) {
			ucl_create_err(&parser->err,
						   "truncated chunk in an indefinite length cbor string");

			return -1;
		}

		/*
		 * Cannot overflow: every chunk's payload is bounded by what is left
		 * of the input, and the cursor moves past it each time round
		 */
		acc += (size_t) chunk.arg;
		taken = chunk.hdrlen + (size_t) chunk.arg;
		p += taken;
		remain -= taken;
	}

	*total = acc;

	return (ssize_t) (p - start);
}

/*
 * Concatenate the chunks of an indefinite length string into one freshly
 * allocated, NUL terminated buffer. The pieces are scattered through the
 * input, so the result always owns its bytes, zero-copy mode or not.
 *
 * `body` points just past the indefinite length head and `avail` is what is
 * left of the chunk from there. On success `total` gets the payload size
 * (excluding the terminator) and `span` the number of input bytes the chunks
 * occupy, break stop code included.
 */
static unsigned char *
ucl_cbor_flatten_string(struct ucl_parser *parser, unsigned char major,
						const unsigned char *body, size_t avail,
						size_t *total, size_t *span)
{
	ssize_t scanned;
	size_t len = 0, alloc_len;
	unsigned char *buf = NULL, *dst;
	const unsigned char *p;

	scanned = ucl_cbor_scan_string_chunks(parser, major, body, avail, &len);

	if (scanned < 0) {
		return NULL;
	}

	if (parser->limits.max_string_length > 0 &&
		(uint64_t) len > parser->limits.max_string_length) {
		ucl_create_err(&parser->err, "cbor string is too long: %ju",
					   (uintmax_t) len);
		parser->err_code = UCL_ELIMIT;

		return NULL;
	}

	if (!ucl_parser_account_alloc(parser, (uint64_t) len + 1)) {
		return NULL;
	}

	if (ucl_size_add(len, 1, &alloc_len)) {
		buf = UCL_ALLOC(alloc_len);
	}

	if (buf == NULL) {
		ucl_create_err(&parser->err, "no memory");
		parser->err_code = UCL_EINTERNAL;

		return NULL;
	}

	/* Second pass over the same, already validated, bytes */
	dst = buf;
	p = body;

	while (*p != UCL_CBOR_BREAK_BYTE) {
		struct ucl_cbor_head chunk;

		/* Validated by the scan above, so this cannot fail now */
		ucl_cbor_read_head(parser, p, avail - (size_t) (p - body), &chunk);
		memcpy(dst, p + chunk.hdrlen, (size_t) chunk.arg);
		dst += (size_t) chunk.arg;
		p += chunk.hdrlen + (size_t) chunk.arg;
	}

	buf[len] = '\0';
	*total = len;
	*span = (size_t) scanned;

	return buf;
}

/*
 * Build a string object out of an indefinite length string.
 */
static ucl_object_t *
ucl_cbor_parse_indefinite_string(struct ucl_parser *parser,
								 const struct ucl_cbor_head *head,
								 const unsigned char *p, size_t remain,
								 size_t *consumed)
{
	size_t total = 0, span = 0;
	unsigned char *buf;
	ucl_object_t *obj;

	buf = ucl_cbor_flatten_string(parser, head->major, p + head->hdrlen,
								  remain - head->hdrlen, &total, &span);

	if (buf == NULL) {
		return NULL;
	}

	obj = ucl_cbor_new_object(parser, UCL_STRING);

	if (obj == NULL) {
		UCL_FREE(total + 1, buf);

		return NULL;
	}

	/* Released by ucl_object_dtor_free through UCL_FREE */
	obj->trash_stack[UCL_TRASH_VALUE] = buf;
	obj->value.sv = (const char *) buf;
	obj->len = total;
	obj->flags |= UCL_OBJECT_ALLOCATED_VALUE;

	if (head->major == UCL_CBOR_BYTES) {
		obj->flags |= UCL_OBJECT_BINARY;
	}

	*consumed = head->hdrlen + span;

	return obj;
}

/*
 * Build a string object out of a definite or indefinite length string,
 * reporting how many input bytes the item occupies through `consumed`.
 */
static ucl_object_t *
ucl_cbor_parse_string(struct ucl_parser *parser,
					  const struct ucl_cbor_head *head,
					  const unsigned char *p, size_t remain, size_t *consumed)
{
	ucl_object_t *obj;
	const unsigned char *data;
	size_t len, avail;

	if (head->indefinite) {
		return ucl_cbor_parse_indefinite_string(parser, head, p, remain,
												consumed);
	}

	data = p + head->hdrlen;
	avail = remain - head->hdrlen;

	if (head->arg > (uint64_t) avail) {
		ucl_create_err(&parser->err,
					   "truncated cbor string: %ju bytes remain, %ju needed",
					   (uintmax_t) avail, (uintmax_t) head->arg);

		return NULL;
	}

	len = (size_t) head->arg;

	if (parser->limits.max_string_length > 0 &&
		(uint64_t) len > parser->limits.max_string_length) {
		ucl_create_err(&parser->err, "cbor string is too long: %ju",
					   (uintmax_t) len);
		parser->err_code = UCL_ELIMIT;

		return NULL;
	}

	obj = ucl_cbor_new_object(parser, UCL_STRING);

	if (obj == NULL) {
		return NULL;
	}

	obj->value.sv = (const char *) data;
	obj->len = len;

	if (head->major == UCL_CBOR_BYTES) {
		obj->flags |= UCL_OBJECT_BINARY;
	}

	if (!(parser->flags & UCL_PARSER_ZEROCOPY)) {
		/* The contents are about to be copied, so they cost us memory too */
		if (!ucl_parser_account_alloc(parser, (uint64_t) len + 1)) {
			ucl_object_unref(obj);

			return NULL;
		}

		if (obj->flags & UCL_OBJECT_BINARY) {
			/*
			 * Byte strings are not NUL terminated and may be empty, so they
			 * cannot go through ucl_copy_value_trash. The value has to be
			 * moved onto the copy: without that the object keeps pointing
			 * into the input buffer, which the caller is free to release
			 * once parsing returns, and the trash slot being taken stops
			 * ucl_copy_value_trash from ever repairing it. An empty string
			 * owns nothing and gets a literal instead.
			 */
			if (len > 0) {
				/* Released by ucl_object_dtor_free through UCL_FREE */
				obj->trash_stack[UCL_TRASH_VALUE] = UCL_ALLOC(len);

				if (obj->trash_stack[UCL_TRASH_VALUE] == NULL) {
					ucl_create_err(&parser->err, "no memory");
					parser->err_code = UCL_EINTERNAL;
					ucl_object_unref(obj);

					return NULL;
				}

				memcpy(obj->trash_stack[UCL_TRASH_VALUE], data, len);
				obj->value.sv = (const char *) obj->trash_stack[UCL_TRASH_VALUE];
				obj->flags |= UCL_OBJECT_ALLOCATED_VALUE;
			}
			else {
				obj->value.sv = "";
			}
		}
		else {
			ucl_copy_value_trash(obj);
		}
	}

	*consumed = head->hdrlen + len;

	return obj;
}

/*
 * Decode major type 7: the booleans, null, the floats and the simple values.
 * The break stop code is handled by the caller and never reaches here.
 */
static ucl_object_t *
ucl_cbor_parse_simple(struct ucl_parser *parser,
					  const struct ucl_cbor_head *head)
{
	ucl_object_t *obj;
	union {
		uint32_t i;
		float f;
	} f32;
	union {
		uint64_t i;
		double d;
	} d64;

	switch (head->ai) {
	case UCL_CBOR_FALSE:
	case UCL_CBOR_TRUE:
		obj = ucl_cbor_new_object(parser, UCL_BOOLEAN);

		if (obj == NULL) {
			return NULL;
		}

		obj->value.iv = (head->ai == UCL_CBOR_TRUE);
		break;

	case UCL_CBOR_AI_2:
		obj = ucl_cbor_new_object(parser, UCL_FLOAT);

		if (obj == NULL) {
			return NULL;
		}

		obj->value.dv = ucl_cbor_half_to_double((uint16_t) head->arg);
		break;

	case UCL_CBOR_AI_4:
		obj = ucl_cbor_new_object(parser, UCL_FLOAT);

		if (obj == NULL) {
			return NULL;
		}

		f32.i = (uint32_t) head->arg;
		obj->value.dv = f32.f;
		break;

	case UCL_CBOR_AI_8:
		obj = ucl_cbor_new_object(parser, UCL_FLOAT);

		if (obj == NULL) {
			return NULL;
		}

		d64.i = head->arg;
		obj->value.dv = d64.d;
		break;

	case UCL_CBOR_AI_1:
		/*
		 * A simple value spelled out in a second byte. Values below 32 have
		 * a one byte encoding and are not well formed in this one.
		 */
		if (head->arg < 32) {
			ucl_create_err(&parser->err,
						   "cbor simple value %ju must use the one byte form",
						   (uintmax_t) head->arg);

			return NULL;
		}

		obj = ucl_cbor_new_object(parser, UCL_NULL);
		break;

	default:
		/*
		 * null, undefined, and the unassigned simple values below 20: none
		 * of them carries anything ucl can hold on to
		 */
		obj = ucl_cbor_new_object(parser, UCL_NULL);
		break;
	}

	return obj;
}

/*
 * Render an integer map key as its decimal spelling. Integer keys are the
 * norm in cbor based protocols, and ucl only addresses objects by string.
 */
static bool
ucl_cbor_int_key(struct ucl_parser *parser, const struct ucl_cbor_head *head,
				 char *buf, size_t buflen, size_t *keylen)
{
	int n;

	if (head->arg > (uint64_t) INT64_MAX) {
		ucl_create_err(&parser->err,
					   "cbor integer key does not fit into int64: %ju",
					   (uintmax_t) head->arg);

		return false;
	}

	if (head->major == UCL_CBOR_UINT) {
		n = snprintf(buf, buflen, "%ju", (uintmax_t) head->arg);
	}
	else {
		n = snprintf(buf, buflen, "%jd",
					 (intmax_t) (-1 - (int64_t) head->arg));
	}

	if (n <= 0 || (size_t) n >= buflen) {
		ucl_create_err(&parser->err, "cannot render a cbor integer key");

		return false;
	}

	*keylen = (size_t) n;

	return true;
}

/*
 * Pop every container that has received all of its elements, leaving
 * parser->cur_obj on the last one closed.
 */
static void
ucl_cbor_unwind(struct ucl_parser *parser)
{
	/*
	 * Iterative rather than tail recursive: closing a deeply nested document
	 * unwinds every finished container in one go, and doing that on the C
	 * stack would make parse depth a stack depth problem again.
	 */
	while (parser->stack != NULL && parser->stack->e.len == 0) {
		parser->cur_obj = parser->stack->obj;
		ucl_parser_pop_container(parser, parser->stack);
	}
}

/*
 * Drop every remaining stack frame without touching the objects they hold.
 * Used on the error path, where the whole tree is released through its root.
 */
static void
ucl_cbor_discard_stack(struct ucl_parser *parser)
{
	while (parser->stack != NULL) {
		ucl_parser_pop_container(parser, parser->stack);
	}
}

static bool
ucl_cbor_consume(struct ucl_parser *parser)
{
	const unsigned char *p;
	size_t remain;
	const unsigned char *key = NULL;
	unsigned char *key_alloc = NULL;
	char intkey[UCL_CBOR_INTKEY_MAX];
	size_t keylen = 0;
	bool have_key = false, key_is_temp = false;
	ucl_object_t *root = NULL, *obj;
	struct ucl_stack *container;
	struct ucl_cbor_head head;
	size_t consumed;

	p = parser->chunks->begin;
	remain = parser->chunks->remain;

	while (remain > 0) {
		ucl_cbor_unwind(parser);
		container = parser->stack;

		if (container == NULL && root != NULL) {
			ucl_create_err(&parser->err,
						   "trailing data after the top level cbor object");
			goto fail;
		}

		if (!ucl_cbor_read_head(parser, p, remain, &head)) {
			goto fail;
		}

		if (head.major == UCL_CBOR_SIMPLE &&
			head.ai == UCL_CBOR_AI_INDEFINITE) {
			/* Break stop code: close the innermost indefinite container */
			if (container == NULL || container->e.len != UCL_CBOR_INDEFINITE) {
				ucl_create_err(&parser->err, "unexpected cbor break stop code");
				goto fail;
			}

			if (have_key) {
				ucl_create_err(&parser->err,
							   "cbor map ends with a key and no value");
				goto fail;
			}

			container->e.len = 0;
			p++;
			remain--;
			continue;
		}

		if (head.major == UCL_CBOR_TAG) {
			/*
			 * Tags carry semantics ucl has no room for, so the tag is
			 * dropped and the item it wraps takes its place. Each one costs
			 * input bytes, so a run of them terminates by itself.
			 */
			p += head.hdrlen;
			remain -= head.hdrlen;
			continue;
		}

		if (container != NULL && container->obj->type == UCL_OBJECT &&
			!have_key) {
			switch (head.major) {
			case UCL_CBOR_TEXT:
			case UCL_CBOR_BYTES:
				if (head.indefinite) {
					size_t total = 0, span = 0;

					key_alloc = ucl_cbor_flatten_string(parser, head.major,
														p + head.hdrlen,
														remain - head.hdrlen,
														&total, &span);

					if (key_alloc == NULL) {
						goto fail;
					}

					key = key_alloc;
					keylen = total;
					key_is_temp = true;
					p += head.hdrlen + span;
					remain -= head.hdrlen + span;
				}
				else {
					if (head.arg > (uint64_t) (remain - head.hdrlen)) {
						ucl_create_err(&parser->err, "truncated cbor key");
						goto fail;
					}

					key = p + head.hdrlen;
					keylen = (size_t) head.arg;
					key_is_temp = false;
					p += head.hdrlen + keylen;
					remain -= head.hdrlen + keylen;
				}
				break;

			case UCL_CBOR_UINT:
			case UCL_CBOR_NINT:
				if (!ucl_cbor_int_key(parser, &head, intkey, sizeof(intkey),
									  &keylen)) {
					goto fail;
				}

				key = (const unsigned char *) intkey;
				key_is_temp = true;
				p += head.hdrlen;
				remain -= head.hdrlen;
				break;

			default:
				ucl_create_err(&parser->err,
							   "unsupported cbor map key of major type %u, "
							   "expected a string or an integer",
							   (unsigned) head.major);
				goto fail;
			}

			if (keylen == 0) {
				ucl_create_err(&parser->err, "empty cbor key");
				goto fail;
			}

			if (parser->limits.max_key_length > 0 &&
				(uint64_t) keylen > parser->limits.max_key_length) {
				ucl_create_err(&parser->err, "cbor key is too long: %ju",
							   (uintmax_t) keylen);
				parser->err_code = UCL_ELIMIT;
				goto fail;
			}

			have_key = true;
			continue;
		}

		obj = NULL;

		switch (head.major) {
		case UCL_CBOR_UINT:
			if (head.arg > (uint64_t) INT64_MAX) {
				ucl_create_err(&parser->err,
							   "cbor integer does not fit into int64: %ju",
							   (uintmax_t) head.arg);
				goto fail;
			}

			obj = ucl_cbor_new_object(parser, UCL_INT);

			if (obj == NULL) {
				goto fail;
			}

			obj->value.iv = (int64_t) head.arg;
			p += head.hdrlen;
			remain -= head.hdrlen;
			break;

		case UCL_CBOR_NINT:
			if (head.arg > (uint64_t) INT64_MAX) {
				ucl_create_err(&parser->err,
							   "cbor integer does not fit into int64: -1 - %ju",
							   (uintmax_t) head.arg);
				goto fail;
			}

			obj = ucl_cbor_new_object(parser, UCL_INT);

			if (obj == NULL) {
				goto fail;
			}

			/* -1 - INT64_MAX is INT64_MIN, so the whole range is reachable */
			obj->value.iv = -1 - (int64_t) head.arg;
			p += head.hdrlen;
			remain -= head.hdrlen;
			break;

		case UCL_CBOR_TEXT:
		case UCL_CBOR_BYTES:
			obj = ucl_cbor_parse_string(parser, &head, p, remain, &consumed);

			if (obj == NULL) {
				goto fail;
			}

			p += consumed;
			remain -= consumed;
			break;

		case UCL_CBOR_SIMPLE:
			obj = ucl_cbor_parse_simple(parser, &head);

			if (obj == NULL) {
				goto fail;
			}

			p += head.hdrlen;
			remain -= head.hdrlen;
			break;

		case UCL_CBOR_ARRAY:
		case UCL_CBOR_MAP: {
			bool is_map = (head.major == UCL_CBOR_MAP);
			size_t avail = remain - head.hdrlen;

			/*
			 * An element needs at least one input byte and a key/value pair
			 * at least two, so a count larger than that is truncated input.
			 * Checking it up front keeps a nine byte head from making the
			 * parser chase a count no chunk could ever satisfy.
			 */
			if (!head.indefinite &&
				head.arg > (uint64_t) (is_map ? avail / 2 : avail)) {
				ucl_create_err(&parser->err,
							   "truncated cbor %s: %ju elements claimed, "
							   "%ju bytes remain",
							   is_map ? "map" : "array",
							   (uintmax_t) head.arg, (uintmax_t) avail);
				goto fail;
			}

			obj = ucl_cbor_new_object(parser, is_map ? UCL_OBJECT : UCL_ARRAY);

			if (obj == NULL) {
				goto fail;
			}

			if (container != NULL) {
				ucl_object_t *survivor = NULL;

				if (!ucl_cbor_insert_object(parser, key, keylen, key_is_temp,
											obj, &survivor)) {
					goto fail;
				}

				/*
				 * The value may have merged into an older object of the same
				 * key, releasing this one, so the stack must follow whoever
				 * now holds it. A merge only makes sense between containers
				 * of the same kind: this container's elements are about to be
				 * decoded as pairs or as single values depending on the
				 * incoming item, so an older container of the other kind
				 * would quietly decode one as the other.
				 */
				if (survivor->type != (is_map ? UCL_OBJECT : UCL_ARRAY)) {
					ucl_create_err(&parser->err,
								   "cannot merge a cbor %s into an %s",
								   is_map ? "map" : "array",
								   is_map ? "array" : "object");

					goto fail;
				}

				obj = survivor;
			}
			else {
				/*
				 * The top level container owns the whole tree, so hold on to
				 * it: on the error path it is the only handle we have to
				 * release everything built so far.
				 */
				root = obj;
			}

			p += head.hdrlen;
			remain -= head.hdrlen;

			if (!ucl_cbor_push_container(parser, obj, head.arg)) {
				goto fail;
			}

			if (key_alloc != NULL) {
				UCL_FREE(keylen + 1, key_alloc);
				key_alloc = NULL;
			}

			key = NULL;
			keylen = 0;
			key_is_temp = false;
			have_key = false;
			continue;
		}

		default:
			/* Tags and the break stop code are dealt with above */
			ucl_create_err(&parser->err, "unexpected cbor major type: %u",
						   (unsigned) head.major);
			goto fail;
		}

		if (container == NULL) {
			ucl_create_err(&parser->err,
						   "bad top level object for cbor: an array or a map "
						   "is required");
			ucl_object_unref(obj);
			goto fail;
		}

		if (!ucl_cbor_insert_object(parser, key, keylen, key_is_temp, obj,
									NULL)) {
			goto fail;
		}

		if (key_alloc != NULL) {
			UCL_FREE(keylen + 1, key_alloc);
			key_alloc = NULL;
		}

		key = NULL;
		keylen = 0;
		key_is_temp = false;
		have_key = false;
	}

	ucl_cbor_unwind(parser);

	if (have_key) {
		ucl_create_err(&parser->err, "cbor map ends with a key and no value");
		goto fail;
	}

	if (parser->stack != NULL) {
		ucl_create_err(&parser->err, "incomplete cbor container");
		goto fail;
	}

	if (root == NULL) {
		ucl_create_err(&parser->err, "empty cbor input");
		goto fail;
	}

	parser->cur_obj = root;

	return true;

fail:
	if (key_alloc != NULL) {
		UCL_FREE(keylen + 1, key_alloc);
	}

	/*
	 * Frames only borrow their objects, so drop them first and then release
	 * the tree through its root. Without this the partially built tree would
	 * leak: ucl_parser_free only unrefs parser->top_obj, which is not set
	 * until the parse succeeds.
	 */
	ucl_cbor_discard_stack(parser);
	parser->cur_obj = NULL;

	if (root != NULL) {
		ucl_object_unref(root);
	}

	return false;
}

bool ucl_parse_cbor(struct ucl_parser *parser)
{
	const unsigned char *p;
	unsigned char major;
	bool ret;

	assert(parser != NULL);
	assert(parser->chunks != NULL);
	assert(parser->chunks->begin != NULL);
	assert(parser->chunks->remain != 0);

	p = parser->chunks->begin;

	/*
	 * A cbor parse never leaves frames behind: a success unwinds every
	 * container and a failure drops them all in ucl_cbor_discard_stack.
	 * Frames still on the stack therefore belong to another parser - the ucl
	 * state machine's implicit top object, whose frames count elements in a
	 * different union member. Continuing such a container from cbor cannot
	 * work, so refuse rather than corrupt its bookkeeping.
	 */
	if (parser->stack != NULL) {
		ucl_create_err(&parser->err,
					   "cbor cannot continue inside a container opened by "
					   "another parser");

		return false;
	}

	/*
	 * A cbor document carries its own root, so a second one has nowhere to
	 * go: there is no defined way to merge it into a tree that is already
	 * built. Saying so is better than parsing it and dropping it on the
	 * floor, which would leak everything it allocated.
	 */
	if (parser->top_obj != NULL) {
		ucl_create_err(&parser->err,
					   "cbor documents cannot be concatenated: the parser "
					   "already holds a top level object");

		return false;
	}

	/*
	 * The document has to start with a container: ucl has nowhere to keep a
	 * bare scalar. A leading tag is allowed through, as the self-described
	 * cbor tag is a common prefix and the consumer skips it.
	 */
	major = *p >> 5;

	if (major != UCL_CBOR_ARRAY && major != UCL_CBOR_MAP &&
		major != UCL_CBOR_TAG) {
		ucl_create_err(&parser->err,
					   "bad top level object for cbor: an array or a map "
					   "is required");

		return false;
	}

	ret = ucl_cbor_consume(parser);

	if (ret && parser->top_obj == NULL) {
		parser->top_obj = parser->cur_obj;
	}

	return ret;
}
