#ifdef CYLINK_DSS
static const char rcsid[] = "$Header: /proj/cvs/isc/bind8/src/lib/dst/cylink_link.c,v 1.10 2002/12/03 05:26:49 marka Exp $";

/*
 * Portions Copyright (c) 1995-1998 by Trusted Information Systems, Inc.
 *
 * Permission to use, copy modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND TRUSTED INFORMATION SYSTEMS
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * TRUSTED INFORMATION SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THE SOFTWARE.
 */
/* 
 * This file contains two components 
 * 1. Interface to the CYLINK library to allow compilation of Bind 
 *    with TIS/DNSSEC when CYLINK is not available 
 *    all calls to CYLINK are contained inside this file.
 * 2. The glue to connvert DSA KEYS to and from external formats
 */
#include "port_before.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "dst_internal.h"
#include <toolkit.h>

#include "port_after.h"

typedef struct cylinkkey {
	char *dk_signer;
	uchar *dk_p;
	uchar *dk_q;
	uchar *dk_g;
	uchar *dk_x;
	uchar *dk_y;
	ushort dk_p_bytes;
} DSA_Key;

#define NULL_PRIV_KEY(k)(k == NULL || k->dk_p == NULL || k->dk_q == NULL || \
			  k->dk_g == NULL || k->dk_x == NULL)
#define NULL_PUB_KEY(k)(k == NULL || k->dk_p == NULL || k->dk_q == NULL || \
			 k->dk_g == NULL || k->dk_y == NULL)

static int dst_cylink_sign(const int mode, DST_KEY *dkey, void **context,
			   const u_char *data, const int len,
			   u_char *signature, const int sig_len);

static int dst_cylink_verify(const int mode, DST_KEY *dkey, void **context,
			     const u_char *data, const int len,
			     const u_char *signature, const int sig_len);

static int dst_cylink_to_dns_key(const DST_KEY *in_key, u_char *out_str,
				 const int out_len);
static int dst_cylink_from_dns_key(DST_KEY *s_key, const u_char *key,
				   const int len);
static int dst_cylink_key_to_file_format(const DST_KEY *key, char *buff,
					 const int buff_len);
static int dst_cylink_key_from_file_format(DST_KEY *d_key,
					   const char *buff,
					   const int buff_len);
static void *dst_cylink_free_key_structure(void *key);

static int dst_cylink_generate_keypair(DST_KEY *key, int exp);
static int dst_cylink_compare_keys(const DST_KEY *key1, const DST_KEY *key2);

static void *memcpyend(void *dest, const void *src, size_t n, size_t size);

/*
 * dst_cylink_init()  Function to answer set up function pointers for 
 *	    CYLINK related functions 
 */
int
dst_cylink_init()
{
	if (dst_t_func[KEY_DSA] != NULL)
		return (1);
	dst_t_func[KEY_DSA] = malloc(sizeof(struct dst_func));
	if (dst_t_func[KEY_DSA] == NULL)
		return (0);
	memset(dst_t_func[KEY_DSA], 0, sizeof(struct dst_func));
	dst_t_func[KEY_DSA]->sign = dst_cylink_sign;
	dst_t_func[KEY_DSA]->verify = dst_cylink_verify;
	dst_t_func[KEY_DSA]->compare = dst_cylink_compare_keys;
	dst_t_func[KEY_DSA]->generate = dst_cylink_generate_keypair;
	dst_t_func[KEY_DSA]->destroy = dst_cylink_free_key_structure;
	dst_t_func[KEY_DSA]->from_dns_key = dst_cylink_from_dns_key;
	dst_t_func[KEY_DSA]->to_dns_key = dst_cylink_to_dns_key;
	dst_t_func[KEY_DSA]->from_file_fmt = dst_cylink_key_from_file_format;
	dst_t_func[KEY_DSA]->to_file_fmt = dst_cylink_key_to_file_format;
	SetDataOrder(1);
	return (1);
}

/*
 * dst_cylink_sign
 *     Call CYLINK signing functions to sign a block of data.
 *     There are three steps to signing, INIT (initialize structures), 
 *     UPDATE (hash (more) data), FINAL (generate a signature).  This
 *     routine performs one or more of these steps.
 * Parameters
 *     mode	SIG_MODE_INIT, SIG_MODE_UPDATE and/or SIG_MODE_FINAL.
 *     algobj      structure holds context for a sign done in multiple calls.
 *     context   the context to use for this computation
 *     data	data to be signed.
 *     len	 length in bytes of data.
 *     priv_key    key to use for signing.
 *     signature   location to store signature.
 *     sig_len     size in bytes of signature field.
 * returns 
 *	N  Success on SIG_MODE_FINAL = returns signature length in bytes
 *         N is 41 for DNS
 *	0  Success on SIG_MODE_INIT  and UPDATE
 *	 <0  Failure
 */

static int
dst_cylink_sign(const int mode, DST_KEY *dkey, void **context,
		const u_char *data, const int len, 
		u_char *signature, const int sig_len)
{
	int sign_len = 0;
	int status;
	SHA_context *ctx = NULL;

	if (mode & SIG_MODE_INIT) 
		ctx = (SHA_context *) malloc(sizeof(SHA_context));
	else if (context)
		ctx = (SHA_context *) *context;
	if (ctx == NULL)
		return (-1);

	if (mode & SIG_MODE_INIT)
		SHAInit(ctx);

	if ((mode & SIG_MODE_UPDATE) && (data && len > 0)) {
		status = SHAUpdate(ctx, data, len);
		if (status != SUCCESS)
			return (SIGN_UPDATE_FAILURE);
	}
	if (mode & SIG_MODE_FINAL) {
		DSA_Key *key;
		uchar digest[SHA_LENGTH];
		uchar rand[SHA_LENGTH];
		uchar r[SHA_LENGTH], s[SHA_LENGTH];

		if (signature == NULL || sig_len < 2 * SHA_LENGTH)
			return (SIGN_FINAL_FAILURE);
		if ((status = SHAFinal(ctx, digest)) != SUCCESS)
			return (SIGN_FINAL_FAILURE);
		SAFE_FREE(ctx);
		if (dkey == NULL || dkey->dk_KEY_struct == NULL)
			return (-1);
		key = (DSA_Key *) dkey->dk_KEY_struct;
		if (NULL_PRIV_KEY(key))
			return (-2);
		dst_random(DST_RAND_STD, sizeof(rand), rand);
		status = GenDSSSignature(key->dk_p_bytes, key->dk_p,
					 key->dk_q, key->dk_g, key->dk_x,
					 rand, r, s, digest);
		if (status != SUCCESS)
			return (SIGN_FINAL_FAILURE);
		*signature = (dkey->dk_key_size - 512)/64;
		sign_len = 1;
		memcpy(signature + sign_len, r, SHA_LENGTH);
		sign_len += SHA_LENGTH;
		memcpy(signature + sign_len, s, SHA_LENGTH);
		sign_len += SHA_LENGTH;
	}
	else {
		if (context == NULL) 
			return (-1);
		*context = (void *) ctx;
	}
	return (sign_len);
}


/*
 * Dst_cylink_verify 
 *     Calls CYLINK verification routines.  There are three steps to 
 *     verification, INIT (initialize structures), UPDATE (hash (more) data), 
 *     FINAL (generate a signature).  This routine performs one or more of 
 *     these steps.
 * Parameters
 *     mode	SIG_MODE_INIT, SIG_MODE_UPDATE and/or SIG_MODE_FINAL.
 *     dkey      structure holds context for a verify done in multiple calls.
 *     context   algorithm specific context for the current context processing
 *     data	data signed.
 *     len	 length in bytes of data.
 *     pub_key     key to use for verify.
 *     signature   signature.
 *     sig_len     length in bytes of signature.
 * returns 
 *     0  Success 
 *    <0  Failure
 */

static int
dst_cylink_verify(const int mode, DST_KEY *dkey, void **context,
		  const u_char *data, const int len,
		  const u_char *signature, const int sig_len)
{
	int status;
	SHA_context *ctx = NULL;

	if (mode & SIG_MODE_INIT) 
		ctx = (SHA_context *) malloc(sizeof(SHA_context));
	else if (context)
		ctx = (SHA_context *) *context;
	if (ctx == NULL)
		return (-1);

	if (mode & SIG_MODE_INIT)
		SHAInit(ctx);

	if ((mode & SIG_MODE_UPDATE) && (data && len > 0)) {
		status = SHAUpdate(ctx, data, len);
		if (status != SUCCESS)
			return (VERIFY_UPDATE_FAILURE);
	}
	if (mode & SIG_MODE_FINAL) {
		DSA_Key *key;
		uchar digest[SHA_LENGTH];
		uchar r[SHA_LENGTH], s[SHA_LENGTH];

		if (dkey == NULL || dkey->dk_KEY_struct == NULL)
			return (-1);
		key = (DSA_Key *) dkey->dk_KEY_struct;
		if (NULL_PUB_KEY(key))
			return (-2);
		if (signature == NULL || sig_len != (2 * SHA_LENGTH +1))
			return (SIGN_FINAL_FAILURE);
		status = SHAFinal(ctx, digest);
		SAFE_FREE(ctx);
		if (status != SUCCESS)
			return (SIGN_FINAL_FAILURE);
		if (((int)*signature) != ((key->dk_p_bytes -64)/8))
			return(VERIFY_FINAL_FAILURE);

		memcpy(r, signature +1, SHA_LENGTH);
		memcpy(s, signature + SHA_LENGTH +1, SHA_LENGTH);
		status = VerDSSSignature(key->dk_p_bytes, key->dk_p,
					 key->dk_q, key->dk_g, key->dk_y,
					 r, s, digest);
		if (status != SUCCESS)
			return (VERIFY_FINAL_FAILURE);
	}
	else {
		if (context == NULL) 
			return (-1);
		*context = (void *) ctx;
	}
	return (0);
}


/*
 * dst_cylink_to_dns_key
 *     Converts key from DSA to DNS distribution format
 *     This function gets in a pointer to the public key and a work area
 *     to write the key into.
 * Parameters
 *     public    KEY structure 
 *     out_str   buffer to write encoded key into 
 *     out_len   size of out_str
 * Return
 *	N >= 0 length of encoded key 
 *	n < 0  error 
 */

static int
dst_cylink_to_dns_key(const DST_KEY *in_key, u_char *out_str,
		      const int out_len)
{
	u_char *op = out_str;
	int t;
	DSA_Key *key;

	if (in_key == NULL || in_key->dk_KEY_struct == NULL ||
	    out_len <= 0 || out_str == NULL)
		return (-1);
	key = (DSA_Key *) in_key->dk_KEY_struct;

	t = (key->dk_p_bytes - 64) / 8;

	*op++ = t;
	memcpy(op, key->dk_q, SHA_LENGTH);
	op += SHA_LENGTH;
	memcpy(op, key->dk_p, key->dk_p_bytes);
	op += key->dk_p_bytes;
	memcpy(op, key->dk_g, key->dk_p_bytes);
	op += key->dk_p_bytes;
	memcpy(op, key->dk_y, key->dk_p_bytes);
	op += key->dk_p_bytes;

	return (op - out_str);
}


/*
 * dst_cylink_from_dns_key
 *     Converts from a DNS KEY RR format to an RSA KEY. 
 * Parameters
 *     len    Length in bytes of DNS key
 *     key    DNS key
 *     name   Key name
 *     s_key  DST structure that will point to the RSA key this routine
 *		will build.
 * Return
 *     0   The input key, s_key or name was null.
 *     1   Success
 */
static int
dst_cylink_from_dns_key(DST_KEY *s_key, const u_char *key, const int len)
{
	int t;
	const u_char *key_ptr = key;
	DSA_Key *d_key;

	if (s_key == NULL || len < 0 || key == NULL)
		return (0);

	if (len == 0)  /* process null key */
		return (1);

	if (key_ptr == NULL)  
		return (0);
	t = (int) *key_ptr++;	/* length of exponent in bytes */

	if ((3 * (t * 8 + 64) + SHA_LENGTH + 1) != len)
		return (0);

	if ((d_key = (DSA_Key *) malloc(sizeof(DSA_Key))) == NULL) {
		EREPORT(("dst_cylink_from_dns_key(): Memory allocation error 1"));
		return (0);
	}
	memset(d_key, 0, sizeof(DSA_Key));
	s_key->dk_KEY_struct = (void *) d_key;
	d_key->dk_signer = strdup(s_key->dk_key_name);
	d_key->dk_p_bytes = 64 + 8 * t;

	if ((d_key->dk_q = (uchar *) malloc(SHA_LENGTH)) == NULL)
		return (0);
	memcpy(d_key->dk_q, key_ptr, SHA_LENGTH);
	key_ptr += SHA_LENGTH;

	if ((d_key->dk_p = (uchar *) malloc(d_key->dk_p_bytes)) == NULL)
		return (0);
	memcpy(d_key->dk_p, key_ptr, d_key->dk_p_bytes);
	key_ptr += d_key->dk_p_bytes;

	if ((d_key->dk_g = (uchar *) malloc(d_key->dk_p_bytes)) == NULL)
		return (0);
	memcpy(d_key->dk_g, key_ptr, d_key->dk_p_bytes);
	key_ptr += d_key->dk_p_bytes;

	if ((d_key->dk_y = (uchar *) malloc(d_key->dk_p_bytes)) == NULL)
		return (0);
	memcpy(d_key->dk_y, key_ptr, d_key->dk_p_bytes);
	key_ptr += d_key->dk_p_bytes;

	s_key->dk_key_size = d_key->dk_p_bytes * 8;
	return (1);
}


/************************************************************************** 
 *  dst_cylink_key_to_file_format
 *	Encodes an DSA Key into the portable file format.
 *  Parameters 
 *	key      DSA KEY structure 
 *	buff      output buffer
 *	buff_len  size of output buffer 
 *  Return
 *	0  Failure - null input rkey
 *     -1  Failure - not enough space in output area
 *	N  Success - Length of data returned in buff
 */

static int
dst_cylink_key_to_file_format(const DST_KEY *key, char *buff,
			      const int buff_len)
{
	char *bp;
	int len, b_len;
	DSA_Key *dkey;
	u_char num[256]; /* More than long enough for DSA keys */

	if (key == NULL || key->dk_KEY_struct == NULL)	/* no output */
		return (0);
	if (buff == NULL || buff_len <= (int) strlen(key_file_fmt_str))
		return (-1);	/* no OR not enough space in output area */

	dkey = (DSA_Key *) key->dk_KEY_struct;

	    memset(buff, 0, buff_len);	/* just in case */
	/* write file header */
	sprintf(buff, key_file_fmt_str, KEY_FILE_FORMAT, KEY_DSA, "DSA");

	bp = (char *) strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	memcpy(num, dkey->dk_p, dkey->dk_p_bytes);
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Prime(p): ",
					       num, dkey->dk_p_bytes)) <= 0)
		return (-1);

	bp = (char *) strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	memcpy(num, dkey->dk_q, dkey->dk_p_bytes);
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Subprime(q): ",
					       num, SHA_LENGTH)) <= 0)
		return (-2);

	bp = (char *) strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	memcpy(num, dkey->dk_g, dkey->dk_p_bytes);
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Base(g): ",
					       num, dkey->dk_p_bytes)) <= 0)
		return (-3);

	bp = (char *) strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	memcpy(num, dkey->dk_x, dkey->dk_p_bytes);
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Private_value(x): ",
					       num, SHA_LENGTH)) <= 0)
		return (-4);

	bp = (char *) strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	memcpy(num, dkey->dk_y, dkey->dk_p_bytes);
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Public_value(y): ",
					       num, dkey->dk_p_bytes)) <= 0)
		return (-4);

	bp += len;
	b_len -= len;
	return (buff_len - b_len);
}


/************************************************************************** 
 * dst_cylink_key_from_file_format
 *     Converts contents of a private key file into a private DSA key. 
 * Parameters 
 *     DSA_Key    structure to put key into 
 *     buff       buffer containing the encoded key 
 *     buff_len   the length of the buffer
 * Return
 *     n >= 0 Foot print of the key converted 
 *     n <  0 Error in conversion 
 */

static int
dst_cylink_key_from_file_format(DST_KEY *d_key, const char *buff,
				const int buff_len)
{
	u_char s[DSS_LENGTH_MAX];
	int len, s_len = sizeof(s);
	const char *p = buff;
	DSA_Key *dsa_key;

	if (d_key == NULL || buff == NULL || buff_len <= 0)
		return (-1);

	dsa_key = (DSA_Key *) malloc(sizeof(DSA_Key));
	if (dsa_key == NULL) {
		return (-2);
	}
	memset(dsa_key, 0, sizeof(*dsa_key));
	d_key->dk_KEY_struct = (void *) dsa_key;

	if (!dst_s_verify_str(&p, "Prime(p): "))
		return (-3);
	memset(s, 0, s_len);
	if ((len = dst_s_conv_bignum_b64_to_u8(&p, s, s_len)) == 0)
		return (-4);
	dsa_key->dk_p_bytes = len;
	if ((dsa_key->dk_p = malloc(len)) == NULL)
		return (-5);
	memcpy(dsa_key->dk_p, s + s_len - len, len);

	while (*++p && p < (const char *) &buff[buff_len]) {
		if (dst_s_verify_str(&p, "Subprime(q): ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, s, s_len)))
				return (-6);
			if ((dsa_key->dk_q = malloc(SHA_LENGTH)) == NULL)
				return (-7);
			memcpyend(dsa_key->dk_q, s + s_len - len, len,
				  SHA_LENGTH);
		} else if (dst_s_verify_str(&p, "Base(g): ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, s, s_len)))
				return (-8);
			if ((dsa_key->dk_g = malloc(dsa_key->dk_p_bytes))
			    == NULL)
				return (-9);
			memcpyend(dsa_key->dk_g, s + s_len - len, len,
				  dsa_key->dk_p_bytes);
		} else if (dst_s_verify_str(&p, "Private_value(x): ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, s, s_len)))
				return (-10);
			if ((dsa_key->dk_x = malloc(SHA_LENGTH)) == NULL)
				return (-11);
			memcpyend(dsa_key->dk_x, s + s_len - len, len,
				  SHA_LENGTH);
		} else if (dst_s_verify_str(&p, "Public_value(y): ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, s, s_len)))
				return (-10);
			if ((dsa_key->dk_y = malloc(dsa_key->dk_p_bytes))
			    == NULL)
				return (-11);
			memcpyend(dsa_key->dk_y, s + s_len - len, len,
				  dsa_key->dk_p_bytes);
		} else {
			EREPORT(("Decode_DSAKey(): Bad keyword %s\n", p));
			return (-12);
		}
	}			/* while p */

	d_key->dk_key_size = dsa_key->dk_p_bytes * 8;

	return (0);
}


/************************************************************************** 
 * dst_cylink_free_key_structure
 *     Frees all dynamicly allocated structures in DSA_Key.
 */

static void *
dst_cylink_free_key_structure(void *key)
{
	DSA_Key *d_key = (DSA_Key *) key;
	if (d_key != NULL) {
		SAFE_FREE(d_key->dk_signer);
		SAFE_FREE(d_key->dk_p);
		SAFE_FREE(d_key->dk_q);
		SAFE_FREE(d_key->dk_g);
		SAFE_FREE(d_key->dk_x);
		SAFE_FREE(d_key->dk_y);
		SAFE_FREE(d_key);
	}
	return (NULL);
}


/************************************************************************** 
 *  dst_cylink_generate_keypair
 *	Generates unique keys that are hard to predict.
 *  Parameters
 *	key    generic Key structure
 *	exp    the public exponent
 *  Return 
 *	0 Failure 
 *	1 Success
 */

static int
dst_cylink_generate_keypair(DST_KEY *key, int nothing)
{
	int status, n;
	DSA_Key *dsa;
	u_char rand[SHA_LENGTH];

	UNUSED(nothing);

	if (key == NULL || key->dk_alg != KEY_DSA)
		return (0);

	if ((dsa = (DSA_Key *) malloc(sizeof(DSA_Key))) == NULL) {
		EREPORT(("dst_cylink_generate_keypair: Memory allocation error 3"));
		return (0);
	}
	memset(dsa, 0, sizeof(*dsa));

	dsa->dk_p_bytes = key->dk_key_size / 8;
	dsa->dk_p = (uchar *) malloc(dsa->dk_p_bytes);
	dsa->dk_q = (uchar *) malloc(SHA_LENGTH);
	dsa->dk_g = (uchar *) malloc(dsa->dk_p_bytes);
	dsa->dk_x = (uchar *) malloc(SHA_LENGTH);
	dsa->dk_y = (uchar *) malloc(dsa->dk_p_bytes);
	if (!dsa->dk_p || !dsa->dk_q || !dsa->dk_g || !dsa->dk_x || !dsa->dk_y) {
		EREPORT(("dst_cylink_generate_keypair: Memory allocation error 4"));
		return (0);
	}
	n = dst_random(DST_RAND_KEY, sizeof(rand), rand);
	if (n != sizeof(rand))
		return (0);
	status = GenDSSParameters(dsa->dk_p_bytes, dsa->dk_p, dsa->dk_q,
				  dsa->dk_g, rand, NULL);
	if (status != SUCCESS)
		return (0);

	status = GenDSSKey(dsa->dk_p_bytes, dsa->dk_p, dsa->dk_q, dsa->dk_g,
			   dsa->dk_x, dsa->dk_y, rand);
	if (status != SUCCESS)
		return (0);
	memset(rand, 0, sizeof(rand));
	key->dk_KEY_struct = (void *) dsa;
	return (1);
}


/*
 *  dst_cylink_compare_keys
 *	Compare two keys for equality.
 *  Return
 *	0	  The keys are equal
 *	NON-ZERO   The keys are not equal
 */

static int
dst_cylink_compare_keys(const DST_KEY *key1, const DST_KEY *key2)
{
	int status;
	DSA_Key *dkey1 = (DSA_Key *) key1->dk_KEY_struct;
	DSA_Key *dkey2 = (DSA_Key *) key2->dk_KEY_struct;

	if (dkey1 == NULL && dkey2 == NULL)
		return (0);
	else if (dkey1 == NULL) 
		return (2);
	else if (dkey2 == NULL)
		return(1);

	if (dkey1->dk_p_bytes != dkey2->dk_p_bytes)
		return (201);
	status = memcmp(dkey1->dk_p, dkey2->dk_p, dkey1->dk_p_bytes) ||
		memcmp(dkey1->dk_q, dkey2->dk_q, SHA_LENGTH) ||
		memcmp(dkey1->dk_g, dkey2->dk_g, dkey1->dk_p_bytes) ||
		memcmp(dkey1->dk_y, dkey2->dk_y, dkey1->dk_p_bytes);
	if (status)
		return (status);
	if (dkey1->dk_x || dkey2->dk_x) {
		if (dkey1->dk_x == NULL || dkey2->dk_x == NULL)
			return (202);
		return (memcmp(dkey1->dk_x, dkey2->dk_x, dkey1->dk_p_bytes));
	} else
		return (0);
}

static void *
memcpyend(void *dest, const void *src, size_t n, size_t size) {
	if (n < size)
		memset(dest, 0, size - n);
	memcpy((char *)dest + size - n, src, n);
	return dest;
}

#else 
#define	dst_cylink_init	__dst_cylink_init

int
dst_cylink_init() 
{
	return (0);
}
#endif /* CYLINK */
