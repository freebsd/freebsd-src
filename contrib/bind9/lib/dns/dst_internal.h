/*
 * Portions Copyright (C) 2004-2006  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 2000-2002  Internet Software Consortium.
 * Portions Copyright (C) 1995-2000 by Network Associates, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: dst_internal.h,v 1.1.6.5 2006-01-27 23:57:44 marka Exp $ */

#ifndef DST_DST_INTERNAL_H
#define DST_DST_INTERNAL_H 1

#include <isc/lang.h>
#include <isc/buffer.h>
#include <isc/int.h>
#include <isc/magic.h>
#include <isc/region.h>
#include <isc/types.h>

#include <dst/dst.h>

ISC_LANG_BEGINDECLS

#define KEY_MAGIC       ISC_MAGIC('D','S','T','K')
#define CTX_MAGIC       ISC_MAGIC('D','S','T','C')

#define VALID_KEY(x) ISC_MAGIC_VALID(x, KEY_MAGIC)
#define VALID_CTX(x) ISC_MAGIC_VALID(x, CTX_MAGIC)

extern isc_mem_t *dst__memory_pool;

/***
 *** Types
 ***/

typedef struct dst_func dst_func_t;

/*% DST Key Structure */
struct dst_key {
	unsigned int	magic;
	dns_name_t *	key_name;	/*%< name of the key */
	unsigned int	key_size;	/*%< size of the key in bits */
	unsigned int	key_proto;	/*%< protocols this key is used for */
	unsigned int	key_alg;	/*%< algorithm of the key */
	isc_uint32_t	key_flags;	/*%< flags of the public key */
	isc_uint16_t	key_id;		/*%< identifier of the key */
	isc_uint16_t	key_bits;	/*%< hmac digest bits */
	dns_rdataclass_t key_class;	/*%< class of the key record */
	isc_mem_t	*mctx;		/*%< memory context */
	void *		opaque;		/*%< pointer to key in crypto pkg fmt */
	dst_func_t *	func;		/*%< crypto package specific functions */
};

struct dst_context {
	unsigned int magic;
	dst_key_t *key;
	isc_mem_t *mctx;
	void *opaque;
};

struct dst_func {
	/*
	 * Context functions
	 */
	isc_result_t (*createctx)(dst_key_t *key, dst_context_t *dctx);
	void (*destroyctx)(dst_context_t *dctx);
	isc_result_t (*adddata)(dst_context_t *dctx, const isc_region_t *data);

	/*
	 * Key operations
	 */
	isc_result_t (*sign)(dst_context_t *dctx, isc_buffer_t *sig);
	isc_result_t (*verify)(dst_context_t *dctx, const isc_region_t *sig);
	isc_result_t (*computesecret)(const dst_key_t *pub,
				      const dst_key_t *priv,
				      isc_buffer_t *secret);
	isc_boolean_t (*compare)(const dst_key_t *key1, const dst_key_t *key2);
	isc_boolean_t (*paramcompare)(const dst_key_t *key1,
				      const dst_key_t *key2);
	isc_result_t (*generate)(dst_key_t *key, int parms);
	isc_boolean_t (*isprivate)(const dst_key_t *key);
	void (*destroy)(dst_key_t *key);

	/* conversion functions */
	isc_result_t (*todns)(const dst_key_t *key, isc_buffer_t *data);
	isc_result_t (*fromdns)(dst_key_t *key, isc_buffer_t *data);
	isc_result_t (*tofile)(const dst_key_t *key, const char *directory);
	isc_result_t (*parse)(dst_key_t *key, isc_lex_t *lexer);

	/* cleanup */
	void (*cleanup)(void);
};

/*%
 * Initializers
 */
isc_result_t dst__openssl_init(void);

isc_result_t dst__hmacmd5_init(struct dst_func **funcp);
isc_result_t dst__hmacsha1_init(struct dst_func **funcp);
isc_result_t dst__hmacsha224_init(struct dst_func **funcp);
isc_result_t dst__hmacsha256_init(struct dst_func **funcp);
isc_result_t dst__hmacsha384_init(struct dst_func **funcp);
isc_result_t dst__hmacsha512_init(struct dst_func **funcp);
isc_result_t dst__opensslrsa_init(struct dst_func **funcp);
isc_result_t dst__openssldsa_init(struct dst_func **funcp);
isc_result_t dst__openssldh_init(struct dst_func **funcp);
isc_result_t dst__gssapi_init(struct dst_func **funcp);

/*%
 * Destructors
 */
void dst__openssl_destroy(void);

/*%
 * Memory allocators using the DST memory pool.
 */
void * dst__mem_alloc(size_t size);
void   dst__mem_free(void *ptr);
void * dst__mem_realloc(void *ptr, size_t size);

/*%
 * Entropy retriever using the DST entropy pool.
 */
isc_result_t dst__entropy_getdata(void *buf, unsigned int len,
				  isc_boolean_t pseudo);

ISC_LANG_ENDDECLS

#endif /* DST_DST_INTERNAL_H */
/*! \file */
