#ifdef EAY_DSS
static const char rcsid[] = "$Header: /proj/cvs/isc/bind8/src/lib/dst/eay_dss_link.c,v 1.5 2001/04/05 22:00:03 bwelling Exp $";

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
 * 1. Interface to the EAY libcrypto library to allow compilation of Bind 
 *    with TIS/DNSSEC when EAY libcrypto is not available 
 *    all calls to libcrypto are contained inside this file.
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

#include "crypto.h"
#include "bn.h"
#include "dsa.h"
#include "sha.h"

#include "port_after.h"

static int dst_eay_dss_sign(const int mode, DST_KEY *dkey, void **context,
			    const u_char *data, const int len,
			    u_char *signature, const int sig_len);

static int dst_eay_dss_verify(const int mode, DST_KEY *dkey, void **context,
			      const u_char *data, const int len,
			      const u_char *signature, const int sig_len);

static int dst_eay_dss_to_dns_key(const DST_KEY *in_key, u_char *out_str,
				  const int out_len);
static int dst_eay_dss_from_dns_key(DST_KEY *s_key, const u_char *key,
				    const int len);
static int dst_eay_dss_key_to_file_format(const DST_KEY *key, u_char *buff,
					  const int buff_len);
static int dst_eay_dss_key_from_file_format(DST_KEY *d_key,
					    const u_char *buff,
					    const int buff_len);
static void *dst_eay_dss_free_key_structure(void *key);

static int dst_eay_dss_generate_keypair(DST_KEY *key, int exp);
static int dst_eay_dss_compare_keys(const DST_KEY *key1, const DST_KEY *key2);

/*
 * dst_eay_dss_init()  Function to answer set up function pointers for 
 *	    EAY DSS related functions 
 */
int
dst_eay_dss_init()
{
	if (dst_t_func[KEY_DSA] != NULL)
		return (1);
	dst_t_func[KEY_DSA] = malloc(sizeof(struct dst_func));
	if (dst_t_func[KEY_DSA] == NULL)
		return (0);
	memset(dst_t_func[KEY_DSA], 0, sizeof(struct dst_func));
	dst_t_func[KEY_DSA]->sign = dst_eay_dss_sign;
	dst_t_func[KEY_DSA]->verify = dst_eay_dss_verify;
	dst_t_func[KEY_DSA]->compare = dst_eay_dss_compare_keys;
	dst_t_func[KEY_DSA]->generate = dst_eay_dss_generate_keypair;
	dst_t_func[KEY_DSA]->destroy = dst_eay_dss_free_key_structure;
	dst_t_func[KEY_DSA]->from_dns_key = dst_eay_dss_from_dns_key;
	dst_t_func[KEY_DSA]->to_dns_key = dst_eay_dss_to_dns_key;
	dst_t_func[KEY_DSA]->from_file_fmt = dst_eay_dss_key_from_file_format;
	dst_t_func[KEY_DSA]->to_file_fmt = dst_eay_dss_key_to_file_format;
	return (1);
}

/*
 * dst_eay_dss_sign
 *     Call EAY DSS signing functions to sign a block of data.
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
dst_eay_dss_sign(const int mode, DST_KEY *dkey, void **context,
		 const u_char *data, const int len, 
		 u_char *signature, const int sig_len)
{
	int sign_len = 0;
	int status;
	SHA_CTX *ctx = NULL;

	if (mode & SIG_MODE_INIT) 
		ctx = (SHA_CTX *) malloc(sizeof(SHA_CTX));
	else if (context)
		ctx = (SHA_CTX *) *context;
	if (ctx == NULL)
		return (-1);

	if (mode & SIG_MODE_INIT)
		SHA1_Init(ctx);

	if ((mode & SIG_MODE_UPDATE) && (data && len > 0)) {
		SHA1_Update(ctx, (u_char *) data, len);
	}
	if (mode & SIG_MODE_FINAL) {
		DSA *key;
		u_char digest[SHA_DIGEST_LENGTH];
		u_char rand[SHA_DIGEST_LENGTH];
		u_char r[SHA_DIGEST_LENGTH], s[SHA_DIGEST_LENGTH];

		if (dkey == NULL || dkey->dk_KEY_struct == NULL)
			return (-1);
		key = dkey->dk_KEY_struct;
		if (key == NULL)
			return(-2);
		SHA1_Final(digest, ctx);
		status = DSA_sign(0, digest, SHA_DIGEST_LENGTH,
				  signature, &sign_len, key);
		if (status != 0)
			return (SIGN_FINAL_FAILURE);

		*signature = (dkey->dk_key_size - 512)/64;
		sign_len = 1;
		memcpy(signature + sign_len, r, SHA_DIGEST_LENGTH);
		sign_len += SHA_DIGEST_LENGTH;
		memcpy(signature + sign_len, s, SHA_DIGEST_LENGTH);
		sign_len += SHA_DIGEST_LENGTH;
	}
	else {
		if (context == NULL) 
			return (-1);
		*context = (void *) ctx;
	}
	return (sign_len);
}


/*
 * dst_eay_dss_verify 
 *     Calls EAY DSS verification routines.  There are three steps to 
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
dst_eay_dss_verify(const int mode, DST_KEY *dkey, void **context,
		   const u_char *data, const int len,
		   const u_char *signature, const int sig_len)
{
	int status;
	SHA_CTX *ctx = NULL;

	if (mode & SIG_MODE_INIT) 
		ctx = (SHA_CTX *) malloc(sizeof(SHA_CTX));
	else if (context)
		ctx = (SHA_CTX *) *context;
	if (ctx == NULL)
		return (-1);

	if (mode & SIG_MODE_INIT)
		SHA1_Init(ctx);

	if ((mode & SIG_MODE_UPDATE) && (data && len > 0)) {
		SHA1_Update(ctx, (u_char *) data, len);
	}
	if (mode & SIG_MODE_FINAL) {
		DSA *key;
		u_char digest[SHA_DIGEST_LENGTH];
		u_char r[SHA_DIGEST_LENGTH], s[SHA_DIGEST_LENGTH];

		if (dkey == NULL || dkey->dk_KEY_struct == NULL)
			return (-1);
		key = (DSA *) dkey->dk_KEY_struct;
		if (key = NULL)
			return (-2);
		if (signature == NULL || sig_len != (2 * SHA_DIGEST_LENGTH +1))
			return (SIGN_FINAL_FAILURE);
		SHA1_Final(digest, ctx);
		SAFE_FREE(ctx);
		if (status != 0)
			return (SIGN_FINAL_FAILURE);
		if (((int)*signature) != ((BN_num_bytes(key->p) -64)/8))
			return(VERIFY_FINAL_FAILURE);

		memcpy(r, signature +1, SHA_DIGEST_LENGTH);
		memcpy(s, signature + SHA_DIGEST_LENGTH +1, SHA_DIGEST_LENGTH);
		status = DSA_verify(0, digest, SHA_DIGEST_LENGTH,
				    (u_char *)signature, sig_len, key);
		if (status != 0)
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
 * dst_eay_dss_to_dns_key
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
dst_eay_dss_to_dns_key(const DST_KEY *in_key, u_char *out_str,
		       const int out_len)
{
	u_char *op = out_str;
	int t;
	DSA *key;

	if (in_key == NULL || in_key->dk_KEY_struct == NULL ||
	    out_len <= 0 || out_str == NULL)
		return (-1);
	key = (DSA *) in_key->dk_KEY_struct;

	t = (BN_num_bytes(key->p) - 64) / 8;

	*op++ = t;
	BN_bn2bin(key->q, op);
	op += BN_num_bytes(key->q);
	BN_bn2bin(key->p, op);
	op += BN_num_bytes(key->p);
	BN_bn2bin(key->g, op);
	op += BN_num_bytes(key->g);
	BN_bn2bin(key->pub_key, op);
	op += BN_num_bytes(key->pub_key);

	return (op - out_str);
}


/*
 * dst_eay_dss_from_dns_key
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
dst_eay_dss_from_dns_key(DST_KEY *s_key, const u_char *key, const int len)
{
	int t;
	u_char *key_ptr = (u_char *)key;
	DSA *d_key;
	int p_bytes;

	if (s_key == NULL || len < 0 || key == NULL)
		return (0);

	if (len == 0)  /* process null key */
		return (1);

	if (key_ptr == NULL)  
		return (0);
	t = (int) *key_ptr++;	/* length of exponent in bytes */
	p_bytes = 64 + 8 * t;

	if ((3 * (t * 8 + 64) + SHA_DIGEST_LENGTH + 1) != len)
		return (0);

	if ((d_key = (DSA *) malloc(sizeof(DSA))) == NULL) {
		EREPORT(("dst_eay_dss_from_dns_key(): Memory allocation error 1"));
		return (0);
	}
	memset(d_key, 0, sizeof(DSA));
	s_key->dk_KEY_struct = (void *) d_key;

	d_key->q = BN_bin2bn(key_ptr, SHA_DIGEST_LENGTH, NULL);
	key_ptr += SHA_DIGEST_LENGTH;

	d_key->p = BN_bin2bn(key_ptr, p_bytes, NULL);
	key_ptr += p_bytes;

	d_key->g = BN_bin2bn(key_ptr, p_bytes, NULL);
	key_ptr += p_bytes;

	d_key->pub_key = BN_bin2bn(key_ptr, p_bytes, NULL);
	key_ptr += p_bytes;

	s_key->dk_key_size = p_bytes * 8;
	return (1);
}


/************************************************************************** 
 *  dst_eay_dss_key_to_file_format
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
dst_eay_dss_key_to_file_format(const DST_KEY *key, u_char *buff,
			       const int buff_len)
{
	u_char *bp;
	int len, b_len;
	DSA *dkey;
	char num[256]; /* More than long enough for DSA keys */

	if (key == NULL || key->dk_KEY_struct == NULL)	/* no output */
		return (0);
	if (buff == NULL || buff_len <= (int) strlen(key_file_fmt_str))
		return (-1);	/* no OR not enough space in output area */

	dkey = (DSA *) key->dk_KEY_struct;

	memset(buff, 0, buff_len);	/* just in case */
	/* write file header */
	sprintf(buff, key_file_fmt_str, KEY_FILE_FORMAT, KEY_DSA, "DSA");

	bp = (char *) strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	memcpy(num, dkey->p, BN_num_bytes(dkey->p));
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Prime(p): ", num,
					       BN_num_bytes(dkey->p))) <= 0)
		return (-1);

	bp = (char *) strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	memcpy(num, dkey->q, BN_num_bytes(dkey->q));
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Subprime(q): ", num,
					       BN_num_bytes(dkey->q))) <= 0)
		return (-2);

	bp = (char *) strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	memcpy(num, dkey->g, BN_num_bytes(dkey->g));
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Base(g): ", num,
					       BN_num_bytes(dkey->g))) <= 0)
		return (-3);

	bp = (char *) strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	memcpy(num, dkey->priv_key, BN_num_bytes(dkey->priv_key));
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Private_value(x): ",
					       num,
					       BN_num_bytes(dkey->priv_key)))
	    <= 0)
		return (-4);

	bp = (char *) strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	memcpy(num, dkey->pub_key, BN_num_bytes(dkey->pub_key));
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Public_value(y): ",
					       num,
					       BN_num_bytes(dkey->pub_key)))
	    <= 0)
		return (-5);

	bp += len;
	b_len -= len;
	return (buff_len - b_len);
}


/************************************************************************** 
 * dst_eay_dss_key_from_file_format
 *     Converts contents of a private key file into a private DSA key. 
 * Parameters 
 *     d_key    structure to put key into 
 *     buff       buffer containing the encoded key 
 *     buff_len   the length of the buffer
 * Return
 *     n >= 0 Foot print of the key converted 
 *     n <  0 Error in conversion 
 */

static int
dst_eay_dss_key_from_file_format(DST_KEY *d_key, const u_char *buff,
				const int buff_len)
{
	char s[128];
	int len, s_len = sizeof(s);
	const char *p = buff;
	DSA *dsa_key;

	if (d_key == NULL || buff == NULL || buff_len <= 0)
		return (-1);

	dsa_key = (DSA *) malloc(sizeof(DSA));
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
	dsa_key->p = BN_bin2bn (s, len, NULL);
	if (dsa_key->p == NULL)
		return(-5);

	while (*++p && p < (const char *) &buff[buff_len]) {
		if (dst_s_verify_str(&p, "Subprime(q): ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, s, s_len)))
				return (-6);
			dsa_key->q = BN_bin2bn (s, len, NULL);
			if (dsa_key->q == NULL)
				return (-7);	
		} else if (dst_s_verify_str(&p, "Base(g): ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, s, s_len)))
				return (-8);
			dsa_key->g = BN_bin2bn (s, len, NULL);
			if (dsa_key->g == NULL)
				return (-9);	
		} else if (dst_s_verify_str(&p, "Private_value(x): ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, s, s_len)))
				return (-10);
			dsa_key->priv_key = BN_bin2bn (s, len, NULL);
			if (dsa_key->priv_key == NULL)
				return (-11);	
		} else if (dst_s_verify_str(&p, "Public_value(y): ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, s, s_len)))
				return (-12);
			dsa_key->pub_key = BN_bin2bn (s, len, NULL);
			if (dsa_key->pub_key == NULL)
				return (-13);	
		} else {
			EREPORT(("Decode_DSAKey(): Bad keyword %s\n", p));
			return (-14);
		}
	}			/* while p */

	d_key->dk_key_size = BN_num_bytes(dsa_key->p);

	return (0);
}


/************************************************************************** 
 * dst_eay_dss_free_key_structure
 *     Frees all dynamicly allocated structures in DSA.
 */

static void *
dst_eay_dss_free_key_structure(void *key)
{
	DSA *d_key = (DSA *) key;
	if (d_key != NULL) {
		BN_free(d_key->p);
		BN_free(d_key->q);
		BN_free(d_key->g);
		if (d_key->pub_key)
			BN_free(d_key->pub_key);
		if (d_key->priv_key)
			BN_free(d_key->priv_key);
		SAFE_FREE(d_key);
	}
	return (NULL);
}


/************************************************************************** 
 *  dst_eay_dss_generate_keypair
 *	Generates unique keys that are hard to predict.
 *  Parameters
 *	key    generic Key structure
 *	exp    the public exponent
 *  Return 
 *	0 Failure 
 *	1 Success
 */

static int
dst_eay_dss_generate_keypair(DST_KEY *key, int nothing)
{
	int status, n;
	DSA *dsa;
	u_char rand[SHA_DIGEST_LENGTH];

	if (key == NULL || key->dk_alg != KEY_DSA)
		return (0);

	if ((dsa = (DSA *) malloc(sizeof(DSA))) == NULL) {
		EREPORT(("dst_eay_dss_generate_keypair: Memory allocation error 3"));
		return (0);
	}
	memset(dsa, 0, sizeof(*dsa));

	n = dst_random(DST_RAND_KEY, sizeof(rand), rand);
	if (n != sizeof(rand))
		return (0);
	dsa = DSA_generate_parameters(key->dk_key_size, rand, 20, NULL, NULL,
				      NULL, NULL);

	if (!dsa) {
		EREPORT(("dst_eay_dss_generate_keypair: Generate Parameters failed"));
		return (0);
	}
	if (DSA_generate_key(dsa) == 0) {
		EREPORT(("dst_eay_dss_generate_keypair: Generate Key failed"));
		return(0);
	}
	key->dk_KEY_struct = (void *) dsa;
	return (1);
}


/*
 *  dst_eay_dss_compare_keys
 *	Compare two keys for equality.
 *  Return
 *	0	  The keys are equal
 *	NON-ZERO   The keys are not equal
 */

static int
dst_eay_dss_compare_keys(const DST_KEY *key1, const DST_KEY *key2)
{
	int status;
	DSA *dkey1 = (DSA *) key1->dk_KEY_struct;
	DSA *dkey2 = (DSA *) key2->dk_KEY_struct;

	if (dkey1 == NULL && dkey2 == NULL)
		return (0);
	else if (dkey1 == NULL) 
		return (2);
	else if (dkey2 == NULL)
		return(1);

	status = BN_cmp(dkey1->p, dkey2->p) ||
		 BN_cmp(dkey1->q, dkey2->q) ||
		 BN_cmp(dkey1->g, dkey2->g) ||
		 BN_cmp(dkey1->pub_key, dkey2->pub_key);
	
	if (status)
		return (status);

	if (dkey1->priv_key || dkey2->priv_key) {
		if (dkey1->priv_key == NULL || dkey2->priv_key == NULL)
			return (202);
		return (BN_cmp(dkey1->priv_key, dkey2->priv_key));
	} else
		return (0);
}
#else 
int
dst_eay_dss_init() 
{
	return (0);
}
#endif /* EAY_DSS */
