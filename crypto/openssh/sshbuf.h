/*	$OpenBSD: sshbuf.h,v 1.25 2022/01/22 00:43:43 djm Exp $	*/
/*
 * Copyright (c) 2011 Damien Miller
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _SSHBUF_H
#define _SSHBUF_H

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef WITH_OPENSSL
# include <openssl/bn.h>
# ifdef OPENSSL_HAS_ECC
#  include <openssl/ec.h>
# endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */

#define SSHBUF_SIZE_MAX		0x8000000	/* Hard maximum size */
#define SSHBUF_REFS_MAX		0x100000	/* Max child buffers */
#define SSHBUF_MAX_BIGNUM	(16384 / 8)	/* Max bignum *bytes* */
#define SSHBUF_MAX_ECPOINT	((528 * 2 / 8) + 1) /* Max EC point *bytes* */

/*
 * NB. do not depend on the internals of this. It will be made opaque
 * one day.
 */
struct sshbuf {
	u_char *d;		/* Data */
	const u_char *cd;	/* Const data */
	size_t off;		/* First available byte is buf->d + buf->off */
	size_t size;		/* Last byte is buf->d + buf->size - 1 */
	size_t max_size;	/* Maximum size of buffer */
	size_t alloc;		/* Total bytes allocated to buf->d */
	int readonly;		/* Refers to external, const data */
	int dont_free;		/* Kludge to support sshbuf_init */
	u_int refcount;		/* Tracks self and number of child buffers */
	struct sshbuf *parent;	/* If child, pointer to parent */
};

/*
 * Create a new sshbuf buffer.
 * Returns pointer to buffer on success, or NULL on allocation failure.
 */
struct sshbuf *sshbuf_new(void);

/*
 * Create a new, read-only sshbuf buffer from existing data.
 * Returns pointer to buffer on success, or NULL on allocation failure.
 */
struct sshbuf *sshbuf_from(const void *blob, size_t len);

/*
 * Create a new, read-only sshbuf buffer from the contents of an existing
 * buffer. The contents of "buf" must not change in the lifetime of the
 * resultant buffer.
 * Returns pointer to buffer on success, or NULL on allocation failure.
 */
struct sshbuf *sshbuf_fromb(struct sshbuf *buf);

/*
 * Create a new, read-only sshbuf buffer from the contents of a string in
 * an existing buffer (the string is consumed in the process).
 * The contents of "buf" must not change in the lifetime of the resultant
 * buffer.
 * Returns pointer to buffer on success, or NULL on allocation failure.
 */
int	sshbuf_froms(struct sshbuf *buf, struct sshbuf **bufp);

/*
 * Clear and free buf
 */
void	sshbuf_free(struct sshbuf *buf);

/*
 * Reset buf, clearing its contents. NB. max_size is preserved.
 */
void	sshbuf_reset(struct sshbuf *buf);

/*
 * Return the maximum size of buf
 */
size_t	sshbuf_max_size(const struct sshbuf *buf);

/*
 * Set the maximum size of buf
 * Returns 0 on success, or a negative SSH_ERR_* error code on failure.
 */
int	sshbuf_set_max_size(struct sshbuf *buf, size_t max_size);

/*
 * Returns the length of data in buf
 */
size_t	sshbuf_len(const struct sshbuf *buf);

/*
 * Returns number of bytes left in buffer before hitting max_size.
 */
size_t	sshbuf_avail(const struct sshbuf *buf);

/*
 * Returns a read-only pointer to the start of the data in buf
 */
const u_char *sshbuf_ptr(const struct sshbuf *buf);

/*
 * Returns a mutable pointer to the start of the data in buf, or
 * NULL if the buffer is read-only.
 */
u_char *sshbuf_mutable_ptr(const struct sshbuf *buf);

/*
 * Check whether a reservation of size len will succeed in buf
 * Safer to use than direct comparisons again sshbuf_avail as it copes
 * with unsigned overflows correctly.
 * Returns 0 on success, or a negative SSH_ERR_* error code on failure.
 */
int	sshbuf_check_reserve(const struct sshbuf *buf, size_t len);

/*
 * Preallocates len additional bytes in buf.
 * Useful for cases where the caller knows how many bytes will ultimately be
 * required to avoid realloc in the buffer code.
 * Returns 0 on success, or a negative SSH_ERR_* error code on failure.
 */
int	sshbuf_allocate(struct sshbuf *buf, size_t len);

/*
 * Reserve len bytes in buf.
 * Returns 0 on success and a pointer to the first reserved byte via the
 * optional dpp parameter or a negative SSH_ERR_* error code on failure.
 */
int	sshbuf_reserve(struct sshbuf *buf, size_t len, u_char **dpp);

/*
 * Consume len bytes from the start of buf
 * Returns 0 on success, or a negative SSH_ERR_* error code on failure.
 */
int	sshbuf_consume(struct sshbuf *buf, size_t len);

/*
 * Consume len bytes from the end of buf
 * Returns 0 on success, or a negative SSH_ERR_* error code on failure.
 */
int	sshbuf_consume_end(struct sshbuf *buf, size_t len);

/* Extract or deposit some bytes */
int	sshbuf_get(struct sshbuf *buf, void *v, size_t len);
int	sshbuf_put(struct sshbuf *buf, const void *v, size_t len);
int	sshbuf_putb(struct sshbuf *buf, const struct sshbuf *v);

/* Append using a printf(3) format */
int	sshbuf_putf(struct sshbuf *buf, const char *fmt, ...)
	    __attribute__((format(printf, 2, 3)));
int	sshbuf_putfv(struct sshbuf *buf, const char *fmt, va_list ap);

/* Functions to extract or store big-endian words of various sizes */
int	sshbuf_get_u64(struct sshbuf *buf, u_int64_t *valp);
int	sshbuf_get_u32(struct sshbuf *buf, u_int32_t *valp);
int	sshbuf_get_u16(struct sshbuf *buf, u_int16_t *valp);
int	sshbuf_get_u8(struct sshbuf *buf, u_char *valp);
int	sshbuf_put_u64(struct sshbuf *buf, u_int64_t val);
int	sshbuf_put_u32(struct sshbuf *buf, u_int32_t val);
int	sshbuf_put_u16(struct sshbuf *buf, u_int16_t val);
int	sshbuf_put_u8(struct sshbuf *buf, u_char val);

/* Functions to peek at the contents of a buffer without modifying it. */
int	sshbuf_peek_u64(const struct sshbuf *buf, size_t offset,
    u_int64_t *valp);
int	sshbuf_peek_u32(const struct sshbuf *buf, size_t offset,
    u_int32_t *valp);
int	sshbuf_peek_u16(const struct sshbuf *buf, size_t offset,
    u_int16_t *valp);
int	sshbuf_peek_u8(const struct sshbuf *buf, size_t offset,
    u_char *valp);

/*
 * Functions to poke values into an existing buffer (e.g. a length header
 * to a packet). The destination bytes must already exist in the buffer.
 */
int sshbuf_poke_u64(struct sshbuf *buf, size_t offset, u_int64_t val);
int sshbuf_poke_u32(struct sshbuf *buf, size_t offset, u_int32_t val);
int sshbuf_poke_u16(struct sshbuf *buf, size_t offset, u_int16_t val);
int sshbuf_poke_u8(struct sshbuf *buf, size_t offset, u_char val);
int sshbuf_poke(struct sshbuf *buf, size_t offset, void *v, size_t len);

/*
 * Functions to extract or store SSH wire encoded strings (u32 len || data)
 * The "cstring" variants admit no \0 characters in the string contents.
 * Caller must free *valp.
 */
int	sshbuf_get_string(struct sshbuf *buf, u_char **valp, size_t *lenp);
int	sshbuf_get_cstring(struct sshbuf *buf, char **valp, size_t *lenp);
int	sshbuf_get_stringb(struct sshbuf *buf, struct sshbuf *v);
int	sshbuf_put_string(struct sshbuf *buf, const void *v, size_t len);
int	sshbuf_put_cstring(struct sshbuf *buf, const char *v);
int	sshbuf_put_stringb(struct sshbuf *buf, const struct sshbuf *v);

/*
 * "Direct" variant of sshbuf_get_string, returns pointer into the sshbuf to
 * avoid an malloc+memcpy. The pointer is guaranteed to be valid until the
 * next sshbuf-modifying function call. Caller does not free.
 */
int	sshbuf_get_string_direct(struct sshbuf *buf, const u_char **valp,
	    size_t *lenp);

/* Skip past a string */
#define sshbuf_skip_string(buf) sshbuf_get_string_direct(buf, NULL, NULL)

/* Another variant: "peeks" into the buffer without modifying it */
int	sshbuf_peek_string_direct(const struct sshbuf *buf, const u_char **valp,
	    size_t *lenp);

/*
 * Functions to extract or store SSH wire encoded bignums and elliptic
 * curve points.
 */
int	sshbuf_put_bignum2_bytes(struct sshbuf *buf, const void *v, size_t len);
int	sshbuf_get_bignum2_bytes_direct(struct sshbuf *buf,
	    const u_char **valp, size_t *lenp);
#ifdef WITH_OPENSSL
int	sshbuf_get_bignum2(struct sshbuf *buf, BIGNUM **valp);
int	sshbuf_put_bignum2(struct sshbuf *buf, const BIGNUM *v);
# ifdef OPENSSL_HAS_ECC
int	sshbuf_get_ec(struct sshbuf *buf, EC_POINT *v, const EC_GROUP *g);
int	sshbuf_get_eckey(struct sshbuf *buf, EC_KEY *v);
int	sshbuf_put_ec(struct sshbuf *buf, const EC_POINT *v, const EC_GROUP *g);
int	sshbuf_put_eckey(struct sshbuf *buf, const EC_KEY *v);
# endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */

/* Dump the contents of the buffer in a human-readable format */
void	sshbuf_dump(const struct sshbuf *buf, FILE *f);

/* Dump specified memory in a human-readable format */
void	sshbuf_dump_data(const void *s, size_t len, FILE *f);

/* Return the hexadecimal representation of the contents of the buffer */
char	*sshbuf_dtob16(struct sshbuf *buf);

/* Encode the contents of the buffer as base64 */
char	*sshbuf_dtob64_string(const struct sshbuf *buf, int wrap);
int	sshbuf_dtob64(const struct sshbuf *d, struct sshbuf *b64, int wrap);
/* RFC4648 "base64url" encoding variant */
int	sshbuf_dtourlb64(const struct sshbuf *d, struct sshbuf *b64, int wrap);

/* Decode base64 data and append it to the buffer */
int	sshbuf_b64tod(struct sshbuf *buf, const char *b64);

/*
 * Tests whether the buffer contains the specified byte sequence at the
 * specified offset. Returns 0 on successful match, or a ssherr.h code
 * otherwise. SSH_ERR_INVALID_FORMAT indicates sufficient bytes were
 * present but the buffer contents did not match those supplied. Zero-
 * length comparisons are not allowed.
 *
 * If sufficient data is present to make a comparison, then it is
 * performed with timing independent of the value of the data. If
 * insufficient data is present then the comparison is not attempted at
 * all.
 */
int	sshbuf_cmp(const struct sshbuf *b, size_t offset,
    const void *s, size_t len);

/*
 * Searches the buffer for the specified string. Returns 0 on success
 * and updates *offsetp with the offset of the first match, relative to
 * the start of the buffer. Otherwise sshbuf_find will return a ssherr.h
 * error code. SSH_ERR_INVALID_FORMAT indicates sufficient bytes were
 * present in the buffer for a match to be possible but none was found.
 * Searches for zero-length data are not allowed.
 */
int
sshbuf_find(const struct sshbuf *b, size_t start_offset,
    const void *s, size_t len, size_t *offsetp);

/*
 * Duplicate the contents of a buffer to a string (caller to free).
 * Returns NULL on buffer error, or if the buffer contains a premature
 * nul character.
 */
char *sshbuf_dup_string(struct sshbuf *buf);

/*
 * Fill a buffer from a file descriptor or filename. Both allocate the
 * buffer for the caller.
 */
int sshbuf_load_fd(int, struct sshbuf **)
    __attribute__((__nonnull__ (2)));
int sshbuf_load_file(const char *, struct sshbuf **)
    __attribute__((__nonnull__ (2)));

/*
 * Write a buffer to a path, creating/truncating as needed (mode 0644,
 * subject to umask). The buffer contents are not modified.
 */
int sshbuf_write_file(const char *path, struct sshbuf *buf)
    __attribute__((__nonnull__ (2)));

/* Read up to maxlen bytes from a fd directly to a buffer */
int sshbuf_read(int, struct sshbuf *, size_t, size_t *)
    __attribute__((__nonnull__ (2)));

/* Macros for decoding/encoding integers */
#define PEEK_U64(p) \
	(((u_int64_t)(((const u_char *)(p))[0]) << 56) | \
	 ((u_int64_t)(((const u_char *)(p))[1]) << 48) | \
	 ((u_int64_t)(((const u_char *)(p))[2]) << 40) | \
	 ((u_int64_t)(((const u_char *)(p))[3]) << 32) | \
	 ((u_int64_t)(((const u_char *)(p))[4]) << 24) | \
	 ((u_int64_t)(((const u_char *)(p))[5]) << 16) | \
	 ((u_int64_t)(((const u_char *)(p))[6]) << 8) | \
	  (u_int64_t)(((const u_char *)(p))[7]))
#define PEEK_U32(p) \
	(((u_int32_t)(((const u_char *)(p))[0]) << 24) | \
	 ((u_int32_t)(((const u_char *)(p))[1]) << 16) | \
	 ((u_int32_t)(((const u_char *)(p))[2]) << 8) | \
	  (u_int32_t)(((const u_char *)(p))[3]))
#define PEEK_U16(p) \
	(((u_int16_t)(((const u_char *)(p))[0]) << 8) | \
	  (u_int16_t)(((const u_char *)(p))[1]))

#define POKE_U64(p, v) \
	do { \
		const u_int64_t __v = (v); \
		((u_char *)(p))[0] = (__v >> 56) & 0xff; \
		((u_char *)(p))[1] = (__v >> 48) & 0xff; \
		((u_char *)(p))[2] = (__v >> 40) & 0xff; \
		((u_char *)(p))[3] = (__v >> 32) & 0xff; \
		((u_char *)(p))[4] = (__v >> 24) & 0xff; \
		((u_char *)(p))[5] = (__v >> 16) & 0xff; \
		((u_char *)(p))[6] = (__v >> 8) & 0xff; \
		((u_char *)(p))[7] = __v & 0xff; \
	} while (0)
#define POKE_U32(p, v) \
	do { \
		const u_int32_t __v = (v); \
		((u_char *)(p))[0] = (__v >> 24) & 0xff; \
		((u_char *)(p))[1] = (__v >> 16) & 0xff; \
		((u_char *)(p))[2] = (__v >> 8) & 0xff; \
		((u_char *)(p))[3] = __v & 0xff; \
	} while (0)
#define POKE_U16(p, v) \
	do { \
		const u_int16_t __v = (v); \
		((u_char *)(p))[0] = (__v >> 8) & 0xff; \
		((u_char *)(p))[1] = __v & 0xff; \
	} while (0)

/* Internal definitions follow. Exposed for regress tests */
#ifdef SSHBUF_INTERNAL

/*
 * Return the allocation size of buf
 */
size_t	sshbuf_alloc(const struct sshbuf *buf);

/*
 * Increment the reference count of buf.
 */
int	sshbuf_set_parent(struct sshbuf *child, struct sshbuf *parent);

/*
 * Return the parent buffer of buf, or NULL if it has no parent.
 */
const struct sshbuf *sshbuf_parent(const struct sshbuf *buf);

/*
 * Return the reference count of buf
 */
u_int	sshbuf_refcount(const struct sshbuf *buf);

# define SSHBUF_SIZE_INIT	256		/* Initial allocation */
# define SSHBUF_SIZE_INC	256		/* Preferred increment length */
# define SSHBUF_PACK_MIN	8192		/* Minimum packable offset */

/* # define SSHBUF_ABORT abort */
/* # define SSHBUF_DEBUG */

# ifndef SSHBUF_ABORT
#  define SSHBUF_ABORT()
# endif

# ifdef SSHBUF_DEBUG
#  define SSHBUF_TELL(what) do { \
		printf("%s:%d %s: %s size %zu alloc %zu off %zu max %zu\n", \
		    __FILE__, __LINE__, __func__, what, \
		    buf->size, buf->alloc, buf->off, buf->max_size); \
		fflush(stdout); \
	} while (0)
#  define SSHBUF_DBG(x) do { \
		printf("%s:%d %s: ", __FILE__, __LINE__, __func__); \
		printf x; \
		printf("\n"); \
		fflush(stdout); \
	} while (0)
# else
#  define SSHBUF_TELL(what)
#  define SSHBUF_DBG(x)
# endif
#endif /* SSHBUF_INTERNAL */

#endif /* _SSHBUF_H */
