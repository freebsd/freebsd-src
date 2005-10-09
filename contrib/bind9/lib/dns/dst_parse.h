/*
 * Portions Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dst_parse.h,v 1.1.4.1 2004/12/09 04:07:17 marka Exp $ */

#ifndef DST_DST_PARSE_H
#define DST_DST_PARSE_H 1

#include <isc/lang.h>

#include <dst/dst.h>

#define MAJOR_VERSION		1
#define MINOR_VERSION		2

#define MAXFIELDSIZE		512
#define MAXFIELDS		12

#define TAG_SHIFT		4
#define TAG_ALG(tag)		((unsigned int)(tag) >> TAG_SHIFT)
#define TAG(alg, off)		(((alg) << TAG_SHIFT) + (off))

/* These are used by both RSA-MD5 and RSA-SHA1 */
#define RSA_NTAGS		8
#define TAG_RSA_MODULUS		((DST_ALG_RSAMD5 << TAG_SHIFT) + 0)
#define TAG_RSA_PUBLICEXPONENT	((DST_ALG_RSAMD5 << TAG_SHIFT) + 1)
#define TAG_RSA_PRIVATEEXPONENT	((DST_ALG_RSAMD5 << TAG_SHIFT) + 2)
#define TAG_RSA_PRIME1		((DST_ALG_RSAMD5 << TAG_SHIFT) + 3)
#define TAG_RSA_PRIME2		((DST_ALG_RSAMD5 << TAG_SHIFT) + 4)
#define TAG_RSA_EXPONENT1	((DST_ALG_RSAMD5 << TAG_SHIFT) + 5)
#define TAG_RSA_EXPONENT2	((DST_ALG_RSAMD5 << TAG_SHIFT) + 6)
#define TAG_RSA_COEFFICIENT	((DST_ALG_RSAMD5 << TAG_SHIFT) + 7)

#define DH_NTAGS		4
#define TAG_DH_PRIME		((DST_ALG_DH << TAG_SHIFT) + 0)
#define TAG_DH_GENERATOR	((DST_ALG_DH << TAG_SHIFT) + 1)
#define TAG_DH_PRIVATE		((DST_ALG_DH << TAG_SHIFT) + 2)
#define TAG_DH_PUBLIC		((DST_ALG_DH << TAG_SHIFT) + 3)

#define DSA_NTAGS		5
#define TAG_DSA_PRIME		((DST_ALG_DSA << TAG_SHIFT) + 0)
#define TAG_DSA_SUBPRIME	((DST_ALG_DSA << TAG_SHIFT) + 1)
#define TAG_DSA_BASE		((DST_ALG_DSA << TAG_SHIFT) + 2)
#define TAG_DSA_PRIVATE		((DST_ALG_DSA << TAG_SHIFT) + 3)
#define TAG_DSA_PUBLIC		((DST_ALG_DSA << TAG_SHIFT) + 4)

#define HMACMD5_NTAGS		1
#define TAG_HMACMD5_KEY		((DST_ALG_HMACMD5 << TAG_SHIFT) + 0)

struct dst_private_element {
	unsigned short tag;
	unsigned short length;
	unsigned char *data;
};

typedef struct dst_private_element dst_private_element_t;

struct dst_private {
	unsigned short nelements;
	dst_private_element_t elements[MAXFIELDS];
};

typedef struct dst_private dst_private_t;

ISC_LANG_BEGINDECLS

void
dst__privstruct_free(dst_private_t *priv, isc_mem_t *mctx);

int
dst__privstruct_parse(dst_key_t *key, unsigned int alg, isc_lex_t *lex,
		      isc_mem_t *mctx, dst_private_t *priv);

int
dst__privstruct_writefile(const dst_key_t *key, const dst_private_t *priv,
			  const char *directory);

ISC_LANG_ENDDECLS

#endif /* DST_DST_PARSE_H */
