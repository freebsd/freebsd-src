#if defined(BSAFE) || defined(DNSSAFE)
static const char rcsid[] = "$Header: /proj/cvs/isc/bind8/src/lib/dst/bsafe_link.c,v 1.15 2001/09/25 04:50:28 marka Exp $";

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
 * 1. Interface to the BSAFE library to allow compilation of Bind 
 *    with TIS/DNSSEC when BSAFE is not available 
 *    all calls to BSAFE are contained inside this file.
 * 2. The glue to connvert RSA KEYS to and from external formats
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
#include <arpa/nameser.h>
#include "dst_internal.h"

#  ifdef __STDC__
#    define PROTOTYPES 1
#  else
#    define PROTOTYPES 0
#  endif

#  ifdef BSAFE
#    include <aglobal.h>
#    include <bsafe.h>
#  else
#    include <global.h>
#    include <bsafe2.h>
#    include <bigmaxes.h>
#  endif

#include "port_after.h"

typedef struct bsafekey {
	char *rk_signer;
	B_KEY_OBJ rk_Private_Key;
	B_KEY_OBJ rk_Public_Key;
} RSA_Key;

#ifndef MAX_RSA_MODULUS_BITS
#define MAX_RSA_MODULUS_BITS 4096
#define MAX_RSA_MODULUS_LEN (MAX_RSA_MODULUS_BITS/8)
#define MAX_RSA_PRIME_LEN (MAX_RSA_MODULUS_LEN/2)
#endif

#define NULL_SURRENDER (A_SURRENDER_CTX *)NULL_PTR
#define NULL_RANDOM (B_ALGORITHM_OBJ)NULL_PTR

B_ALGORITHM_METHOD *CHOOSER[] =
{
	&AM_MD5,
	&AM_MD5_RANDOM,
	&AM_RSA_KEY_GEN,
	&AM_RSA_ENCRYPT,
	&AM_RSA_DECRYPT,
	&AM_RSA_CRT_ENCRYPT,
	&AM_RSA_CRT_DECRYPT,
	(B_ALGORITHM_METHOD *) NULL_PTR
};

static u_char pkcs1[] =
{
	0x30, 0x20, 0x30, 0x0c, 0x06, 0x08, 0x2a, 0x86,
	0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05, 0x05, 0x00,
	0x04, 0x10
};

static int dst_bsafe_md5digest(const int mode, B_ALGORITHM_OBJ *digest_obj,
			       const u_char *data, const int len,
			       u_char *digest, const int digest_len);

static int dst_bsafe_key_size(RSA_Key *r_key);

static int dst_bsafe_sign(const int mode, DST_KEY *dkey, void **context,
			  const u_char *data, const int len,
			  u_char *signature, const int sig_len);
static int dst_bsafe_verify(const int mode, DST_KEY *dkey, void **context,
			    const u_char *data, const int len,
			    const u_char *signature, const int sig_len);
static int dst_bsafe_to_dns_key(const DST_KEY *in_key, u_char *out_str,
				const int out_len);
static int dst_bsafe_from_dns_key(DST_KEY *s_key, const u_char *key,
				  const int len);
static int dst_bsafe_key_to_file_format(const DST_KEY *key, char *buff,
					const int buff_len);
static int dst_bsafe_key_from_file_format(DST_KEY *d_key,
					  const char *buff,
					  const int buff_len);
static int dst_bsafe_generate_keypair(DST_KEY *key, int exp);
static int dst_bsafe_compare_keys(const DST_KEY *key1, const DST_KEY *key2);
static void *dst_bsafe_free_key_structure(void *key);

/*
 * dst_bsafe_init()  Function to answer set up function pointers for 
 *	   BSAFE/DNSSAFE related functions 
 */
int
dst_bsafe_init(void)
{
	if (dst_t_func[KEY_RSA] != NULL)
		return (1);
	dst_t_func[KEY_RSA] = malloc(sizeof(struct dst_func));
	if (dst_t_func[KEY_RSA] == NULL)
		return (0);
	memset(dst_t_func[KEY_RSA], 0, sizeof(struct dst_func));
	dst_t_func[KEY_RSA]->sign = dst_bsafe_sign;
	dst_t_func[KEY_RSA]->verify = dst_bsafe_verify;
	dst_t_func[KEY_RSA]->compare = dst_bsafe_compare_keys;
	dst_t_func[KEY_RSA]->generate = dst_bsafe_generate_keypair;
	dst_t_func[KEY_RSA]->destroy = dst_bsafe_free_key_structure;
	dst_t_func[KEY_RSA]->from_dns_key = dst_bsafe_from_dns_key;
	dst_t_func[KEY_RSA]->to_dns_key = dst_bsafe_to_dns_key;
	dst_t_func[KEY_RSA]->from_file_fmt = dst_bsafe_key_from_file_format;
	dst_t_func[KEY_RSA]->to_file_fmt = dst_bsafe_key_to_file_format;
	return (1);
}

/*
 * dst_bsafe_sign
 *     Call BSAFE signing functions to sign a block of data.
 *     There are three steps to signing, INIT (initialize structures), 
 *     UPDATE (hash (more) data), FINAL (generate a signature).  This
 *     routine performs one or more of these steps.
 * Parameters
 *     mode	SIG_MODE_INIT, SIG_MODE_UPDATE and/or SIG_MODE_FINAL.
 *     dkey      structure holds context for a sign done in multiple calls.
 *     context   the context to use for this computation
 *     data	data to be signed.
 *     len	 length in bytes of data.
 *     priv_key    key to use for signing.
 *     signature   location to store signature.
 *     sig_len     size in bytes of signature field.
 * returns 
 *	N  Success on SIG_MODE_FINAL = returns signature length in bytes
 *	0  Success on SIG_MODE_INIT  and UPDATE
 *	 <0  Failure
 */

static int
dst_bsafe_sign(const int mode, DST_KEY *dkey, void **context,
	       const u_char *data, const int len, 
	       u_char *signature, const int sig_len)
{
	u_int sign_len = 0;
	int status = 0;
	B_ALGORITHM_OBJ *md5_ctx = NULL;
	int w_bytes = 0;
	u_int u_bytes = 0;
	u_char work_area[NS_MD5RSA_MAX_SIZE];

	if (mode & SIG_MODE_INIT) { 
		md5_ctx = (B_ALGORITHM_OBJ *) malloc(sizeof(B_ALGORITHM_OBJ));
		if ((status = B_CreateAlgorithmObject(md5_ctx)))
			return (-1);
		if ((status = B_SetAlgorithmInfo(*md5_ctx, AI_MD5, NULL)))
			return (-1);
	}
	else if (context) 
		md5_ctx = (B_ALGORITHM_OBJ *) *context;
	if (md5_ctx == NULL) 
		return (-1);

	w_bytes = dst_bsafe_md5digest(mode, md5_ctx, 
				      data, len,work_area, sizeof(work_area));
        if (w_bytes < 0 || (mode & SIG_MODE_FINAL)) {
		B_DestroyAlgorithmObject(md5_ctx);
		SAFE_FREE(md5_ctx);
		if (w_bytes < 0)
			return (w_bytes);
	}

	if (mode & SIG_MODE_FINAL) {
		RSA_Key *key;
		int ret = 0;
		B_ALGORITHM_OBJ rsaEncryptor = (B_ALGORITHM_OBJ) NULL_PTR;
		
		if (dkey == NULL || dkey->dk_KEY_struct == NULL)
			return (-1);
		key = (RSA_Key *) dkey->dk_KEY_struct;
		if (key == NULL || key->rk_Private_Key == NULL)
			return (-1);

		if ((status = B_CreateAlgorithmObject(&rsaEncryptor)))
			return (SIGN_FINAL_FAILURE);
		if ((status = B_SetAlgorithmInfo(rsaEncryptor,
						 AI_PKCS_RSAPrivate,
						 NULL_PTR)))

			ret = SIGN_FINAL_FAILURE;
		if (ret == 0 && 
		    (status = B_EncryptInit(rsaEncryptor,
					    key->rk_Private_Key,
					    CHOOSER, NULL_SURRENDER)))
			ret = SIGN_FINAL_FAILURE;
		if (ret == 0 &&
		    (status = B_EncryptUpdate(rsaEncryptor, signature,
					      &u_bytes, sig_len, pkcs1,
					      sizeof(pkcs1), NULL_PTR,
					      NULL_SURRENDER)))
			ret = SIGN_FINAL_FAILURE;
		if (ret == 0 &&
		    (status = B_EncryptUpdate(rsaEncryptor, signature,
					      &u_bytes, sig_len, work_area,
					      w_bytes, NULL_PTR,
					      NULL_SURRENDER)))
			ret = SIGN_FINAL_FAILURE;

		if (ret == 0 &&
		    (status = B_EncryptFinal(rsaEncryptor, signature + u_bytes,
					     &sign_len, sig_len - u_bytes,
					     NULL_PTR, NULL_SURRENDER)))
			ret = SIGN_FINAL_FAILURE;
		B_DestroyAlgorithmObject(&rsaEncryptor);
		if (ret != 0) 
			return (ret);

	}
	else {
		if (context == NULL) 
			return (-1);
		*context = (void *) md5_ctx;
	}
	return (sign_len);
}


/*
 * Dst_bsafe_verify 
 *     Calls BSAFE verification routines.  There are three steps to 
 *     verification, INIT (initialize structures), UPDATE (hash (more) data), 
 *     FINAL (generate a signature).  This routine performs one or more of 
 *     these steps.
 * Parameters
 *     mode	SIG_MODE_INIT, SIG_MODE_UPDATE and/or SIG_MODE_FINAL.
 *     dkey      structure holds context for a verify done in multiple calls.
 *     context   the context to use for this computation
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
dst_bsafe_verify(const int mode, DST_KEY *dkey, void **context,
		 const u_char *data, const int len,
		 const u_char *signature, const int sig_len)
{
	B_ALGORITHM_OBJ *md5_ctx = NULL;
	u_char digest[DST_HASH_SIZE];
	u_char work_area[DST_HASH_SIZE + sizeof(pkcs1)];
	int status = 0, w_bytes = 0;
	u_int u_bytes = 0;

	if (mode & SIG_MODE_INIT) { 
		md5_ctx = (B_ALGORITHM_OBJ *) malloc(sizeof(B_ALGORITHM_OBJ));
		if ((status = B_CreateAlgorithmObject(md5_ctx)))
			return (-1);
		if ((status = B_SetAlgorithmInfo(*md5_ctx, AI_MD5, NULL)))
			return (-1);
	}
	else if (context) 
		md5_ctx = (B_ALGORITHM_OBJ *) *context;
	if (md5_ctx == NULL) 
		return (-1);

	w_bytes = dst_bsafe_md5digest(mode, md5_ctx, data, len, 
				      digest, sizeof(digest));

	if (w_bytes < 0 || (mode & SIG_MODE_FINAL)) {
		B_DestroyAlgorithmObject(md5_ctx);
		SAFE_FREE(md5_ctx);
		if (w_bytes < 0)
			return (-1);
	}

	if (mode & SIG_MODE_FINAL) {
		RSA_Key *key;
		int ret = 0;
		B_ALGORITHM_OBJ rsaEncryptor = (B_ALGORITHM_OBJ) NULL_PTR;

		if (dkey == NULL || dkey->dk_KEY_struct == NULL)
			return (-1);
		key = (RSA_Key *) dkey->dk_KEY_struct;
		if (key->rk_Public_Key == NULL)
			return (-2);
		if (rsaEncryptor == NULL_PTR) {
			if ((status = B_CreateAlgorithmObject(&rsaEncryptor)))
				ret = SIGN_FINAL_FAILURE;
			if (ret == 0 &&
			    (status = B_SetAlgorithmInfo(rsaEncryptor,
							 AI_PKCS_RSAPublic,
							 NULL_PTR)))
				ret = VERIFY_FINAL_FAILURE;
		}
		if (ret == 0 &&
		    (status = B_DecryptInit(rsaEncryptor, key->rk_Public_Key,
					    CHOOSER, NULL_SURRENDER)))
			ret = VERIFY_FINAL_FAILURE;

		if (ret == 0 && 
		    (status = B_DecryptUpdate(rsaEncryptor, work_area,
					      &u_bytes, 0,
					      (const u_char *) signature,
					      sig_len,
					      NULL_PTR, NULL_SURRENDER)))
			ret = VERIFY_FINAL_FAILURE;

		if (ret == 0 && 
		    (status = B_DecryptFinal(rsaEncryptor, work_area + u_bytes,
					     &u_bytes,
					     sizeof(work_area) - u_bytes,
					     NULL_PTR, NULL_SURRENDER)))
			ret = VERIFY_FINAL_FAILURE;
		B_DestroyAlgorithmObject(&rsaEncryptor);
		/* skip PKCS#1 header in output from Decrypt function */
		if (ret)
			return (ret);
		ret = memcmp(digest, &work_area[sizeof(pkcs1)], w_bytes);
		if (ret == 0)
			return(0);
		else
			return(VERIFY_FINAL_FAILURE);
	}
	else {
		if (context == NULL) 
			return (-1);
		*context = (void *) md5_ctx;
	}
	return (0);
}


/*
 * dst_bsafe_to_dns_key
 *     Converts key from RSA to DNS distribution format
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
dst_bsafe_to_dns_key(const DST_KEY *in_key, u_char *out_str,
		     const int out_len)
{
	B_KEY_OBJ public;
	A_RSA_KEY *pub = NULL;
	u_char *op = out_str;
	int n = 0;

	if (in_key == NULL || in_key->dk_KEY_struct == NULL ||
	    out_len <= 0 || out_str == NULL)
		return (-1);
	public = (B_KEY_OBJ)((RSA_Key *) in_key->dk_KEY_struct)->rk_Public_Key;

	n = B_GetKeyInfo((POINTER *) &pub, public, KI_RSAPublic);
	if (n != 0)
		return (-1);

	if (pub->exponent.len < 256) {  /* key exponent is <= 2040 bits */
		if ((unsigned int)out_len < pub->exponent.len + 1)
			return (-1);
		*op++ = (u_int8_t) pub->exponent.len;
	} else {                       /*  key exponent is > 2040 bits */
		u_int16_t e = (u_int16_t) pub->exponent.len;
		if ((unsigned int)out_len < pub->exponent.len + 3)
			return (-1);
		*op++ = 0;          /* 3 byte length field */
		dst_s_put_int16(op, e);
		op += sizeof(e);
		n = 2;
	}
	n++;
	memcpy(op, pub->exponent.data, pub->exponent.len);
	op += pub->exponent.len;
	n += pub->exponent.len;

	if ((unsigned int)(out_len - n) >= pub->modulus.len) {
		/*copy exponent */
		memcpy(op, pub->modulus.data, pub->modulus.len);
		n += pub->modulus.len;
	}
	else 
		n = -1;
	return (n);
}


/*
 * dst_bsafe_from_dns_key
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
dst_bsafe_from_dns_key(DST_KEY *s_key, const u_char *key, const int len)
{
	int bytes;
	const u_char *key_ptr;
	RSA_Key *r_key;
	A_RSA_KEY *public;

	if (s_key == NULL || len < 0 || key == NULL)
		return (0);

	r_key = (RSA_Key *) s_key->dk_KEY_struct;
	if (r_key != NULL)	/* do not reuse */
		s_key->dk_func->destroy(r_key);

	if (len == 0)
		return (1);

	if ((r_key = (RSA_Key *) malloc(sizeof(RSA_Key))) == NULL) {
		EREPORT(("dst_bsafe_from_dns_key(): Memory allocation error 1"));
		return (0);
	}
	memset(r_key, 0, sizeof(RSA_Key));
	s_key->dk_KEY_struct = (void *) r_key;
	r_key->rk_signer = strdup(s_key->dk_key_name);

	if (B_CreateKeyObject(&r_key->rk_Public_Key) != 0) {
		EREPORT(("dst_bsafe_from_dns_key(): Memory allocation error 3"));
		s_key->dk_func->destroy(r_key);
		return (0);
	}
	key_ptr = key;
	bytes = (int) *key_ptr++;	/* length of exponent in bytes */
	if (bytes == 0)  {             /* special case for long exponents */
		bytes = (int) dst_s_get_int16(key_ptr);
		key_ptr += sizeof(u_int16_t);
	}
	if (bytes > MAX_RSA_MODULUS_LEN) { 
		dst_bsafe_free_key_structure(r_key);
		return (-1);
	}
	if ((public = (A_RSA_KEY *) malloc(sizeof(A_RSA_KEY))) == NULL)
		return (0);
	memset(public, 0, sizeof(*public));
	public->exponent.len = bytes;
	if ((public->exponent.data = (u_char *) malloc(bytes)) == NULL)
		return (0);
	memcpy(public->exponent.data, key_ptr, bytes);

	key_ptr += bytes;	/* beginning of modulus */
	bytes = len - bytes - 1;	/* length of modulus */

	if (bytes > MAX_RSA_MODULUS_LEN) { 
		dst_bsafe_free_key_structure(r_key);
		return (-1);
	}
	public->modulus.len = bytes;
	if ((public->modulus.data = (u_char *) malloc(bytes)) == NULL)
		return (0);
	memcpy(public->modulus.data, key_ptr, bytes);

	B_SetKeyInfo(r_key->rk_Public_Key, KI_RSAPublic, (POINTER) public);

	s_key->dk_key_size = dst_bsafe_key_size(r_key);
	SAFE_FREE(public->modulus.data);
	SAFE_FREE(public->exponent.data);
	SAFE_FREE(public);	
	return (1);
}


/*
 *  dst_bsafe_key_to_file_format
 *	Encodes an RSA Key into the portable file format.
 *  Parameters 
 *	rkey      RSA KEY structure 
 *	buff      output buffer
 *	buff_len  size of output buffer 
 *  Return
 *	0  Failure - null input rkey
 *     -1  Failure - not enough space in output area
 *	N  Success - Length of data returned in buff
 */

static int
dst_bsafe_key_to_file_format(const DST_KEY *key, char *buff,
			     const int buff_len)
{
	char *bp;
	int len, b_len;
	B_KEY_OBJ rkey;
	A_PKCS_RSA_PRIVATE_KEY *private = NULL;

	if (key == NULL || key->dk_KEY_struct == NULL)	/* no output */
		return (0);
	if (buff == NULL || buff_len <= (int) strlen(key_file_fmt_str))
		return (-1);	/* no OR not enough space in output area */

	rkey = (B_KEY_OBJ)((RSA_Key *) key->dk_KEY_struct)->rk_Private_Key;

	B_GetKeyInfo((POINTER *) &private, rkey, KI_PKCS_RSAPrivate);

	    memset(buff, 0, buff_len);	/* just in case */
	/* write file header */
	sprintf(buff, key_file_fmt_str, KEY_FILE_FORMAT, KEY_RSA, "RSA");

	bp = strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Modulus: ",
					       private->modulus.data,
					       private->modulus.len)) <= 0)
		return (-1);

	bp += len;
	b_len -= len;
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "PublicExponent: ",
					       private->publicExponent.data,
					 private->publicExponent.len)) <= 0)
		return (-2);

	bp += len;
	b_len -= len;
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "PrivateExponent: ",
					       private->privateExponent.data,
					private->privateExponent.len)) <= 0)
		return (-3);
	bp += len;
	b_len -= len;

	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Prime1: ",
					       private->prime[0].data,
					       private->prime[0].len)) < 0)
		return (-4);
	bp += len;
	b_len -= len;

	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Prime2: ",
					       private->prime[1].data,
					       private->prime[1].len)) < 0)
		return (-5);
	bp += len;
	b_len -= len;

	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Exponent1: ",
					     private->primeExponent[0].data,
					private->primeExponent[0].len)) < 0)
		return (-6);
	bp += len;
	b_len -= len;

	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Exponent2: ",
					     private->primeExponent[1].data,
					private->primeExponent[1].len)) < 0)
		return (-7);
	bp += len;
	b_len -= len;

	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Coefficient: ",
					       private->coefficient.data,
					     private->coefficient.len)) < 0)
		return (-8);
	bp += len;
	b_len -= len;
	return (buff_len - b_len);
}


/*
 * dst_bsafe_key_from_file_format
 *     Converts contents of a private key file into a private RSA key. 
 * Parameters 
 *     RSA_Key    structure to put key into 
 *     buff       buffer containing the encoded key 
 *     buff_len   the length of the buffer
 * Return
 *     n >= 0 Foot print of the key converted 
 *     n <  0 Error in conversion 
 */

static int
dst_bsafe_key_from_file_format(DST_KEY *d_key, const char *buff,
			       const int buff_len)
{
	int status;
	char s[RAW_KEY_SIZE];
	int len, s_len = sizeof(s);
	const char *p = buff;
	RSA_Key *b_key;
	A_RSA_KEY *public;
	A_PKCS_RSA_PRIVATE_KEY *private;

	if (d_key == NULL || buff == NULL || buff_len <= 0)
		return (-1);

	b_key = (RSA_Key *) malloc(sizeof(RSA_Key));
	public = (A_RSA_KEY *) malloc(sizeof(A_RSA_KEY));
	private = (A_PKCS_RSA_PRIVATE_KEY *)
		malloc(sizeof(A_PKCS_RSA_PRIVATE_KEY));
	if (b_key == NULL || private == NULL || public == NULL) {
		SAFE_FREE(b_key);
		SAFE_FREE(public);
		SAFE_FREE(private);
		return (-2);
	}
	memset(b_key, 0, sizeof(*b_key));
	memset(public, 0, sizeof(A_RSA_KEY));
	memset(private, 0, sizeof(A_PKCS_RSA_PRIVATE_KEY));
	d_key->dk_KEY_struct = (void *) b_key;
	if (!dst_s_verify_str(&p, "Modulus: "))
		return (-3);
	memset(s, 0, s_len);
	if ((len = dst_s_conv_bignum_b64_to_u8(&p, (u_char *)s, s_len)) == 0)
		return (-4);

	private->modulus.len = len;
	if ((private->modulus.data = malloc(len)) == NULL)
		return (-5);
	memcpy(private->modulus.data, s + s_len - len, len);

	while (*(++p) && p < (const char *) &buff[buff_len]) {
		if (dst_s_verify_str(&p, "PublicExponent: ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, (u_char *)s, s_len)))
				return (-5);
			private->publicExponent.len = len;
			if ((private->publicExponent.data = malloc(len))
			    == NULL)
				return (-6);
			memcpy(private->publicExponent.data,
			       s + s_len - len, len);
		} else if (dst_s_verify_str(&p, "PrivateExponent: ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, (u_char *)s, s_len)))
				return (-6);
			private->privateExponent.len = len;
			if ((private->privateExponent.data = malloc(len))
			    == NULL)
				return (-7);
			memcpy(private->privateExponent.data, s + s_len - len,
			       len);
		} else if (dst_s_verify_str(&p, "Prime1: ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, (u_char *)s,
							MAX_RSA_PRIME_LEN)))
				return (-7);
			private->prime[0].len = len;
			if ((private->prime[0].data = malloc(len)) == NULL)
				return (-8);
			memcpy(private->prime[0].data,
			       s + MAX_RSA_PRIME_LEN - len, len);
		} else if (dst_s_verify_str(&p, "Prime2: ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, (u_char *)s,
							MAX_RSA_PRIME_LEN)))
				return (-8);
			private->prime[1].len = len;
			if ((private->prime[1].data = malloc(len)) == NULL)
				return (-9);
			memcpy(private->prime[1].data,
			       s + MAX_RSA_PRIME_LEN - len, len);
		} else if (dst_s_verify_str(&p, "Exponent1: ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, (u_char *)s,
							MAX_RSA_PRIME_LEN)))
				return (-9);
			private->primeExponent[0].len = len;
			if ((private->primeExponent[0].data = malloc(len))
			    == NULL)
				return (-10);
			memcpy(private->primeExponent[0].data,
			       s + MAX_RSA_PRIME_LEN - len, len);
		} else if (dst_s_verify_str(&p, "Exponent2: ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, (u_char *)s,
							MAX_RSA_PRIME_LEN)))
				return (-10);
			private->primeExponent[1].len = len;
			if ((private->primeExponent[1].data = malloc(len))
			    == NULL)
				return (-11);
			memcpy(private->primeExponent[1].data,
			       s + MAX_RSA_PRIME_LEN - len, len);
		} else if (dst_s_verify_str(&p, "Coefficient: ")) {
			if (!(len = dst_s_conv_bignum_b64_to_u8(&p, (u_char *)s,
							MAX_RSA_PRIME_LEN)))
				return (-11);
			private->coefficient.len = len;
			if ((private->coefficient.data = malloc(len)) == NULL)
				return (-12);
			memcpy(private->coefficient.data,
			       s + MAX_RSA_PRIME_LEN - len, len);
		} else {
			EREPORT(("Decode_RSAKey(): Bad keyword %s\n", p));
			return (-12);
		}
	}			/* while p */

	public->modulus.len = private->modulus.len;
	if ((public->modulus.data = (u_char *) malloc(public->modulus.len)) ==
	    NULL)
		return (-13);
	memcpy(public->modulus.data, private->modulus.data,
	       private->modulus.len);

	public->exponent.len = private->publicExponent.len;
	if ((public->exponent.data = (u_char *) malloc(public->exponent.len))
	    == NULL)
		return (-14);
	memcpy(public->exponent.data, private->publicExponent.data,
	       private->publicExponent.len);

	status = B_CreateKeyObject(&(b_key->rk_Public_Key));
	if (status)
		return (-1);
	status = B_SetKeyInfo(b_key->rk_Public_Key, KI_RSAPublic,
			      (POINTER) public);
	if (status)
		return (-1);

	status = B_CreateKeyObject(&b_key->rk_Private_Key);
	if (status)
		return (-1);
	status = B_SetKeyInfo(b_key->rk_Private_Key, KI_PKCS_RSAPrivate,
			      (POINTER) private);
	if (status)
		return (-1);

	d_key->dk_key_size = dst_bsafe_key_size(b_key);

	SAFE_FREE(private->modulus.data);
	SAFE_FREE(private->publicExponent.data);
	SAFE_FREE(private->privateExponent.data);
	SAFE_FREE(private->prime[0].data);
	SAFE_FREE(private->prime[1].data);
	SAFE_FREE(private->primeExponent[0].data);
	SAFE_FREE(private->primeExponent[1].data);
	SAFE_FREE(private->coefficient.data);
	SAFE_FREE(private);	/* is this the right thing to do ??? XXXX */
	SAFE_FREE(public->modulus.data);
	SAFE_FREE(public->exponent.data);
	SAFE_FREE(public);
	return (0);
}


/*
 * dst_bsafe_free_key_structure
 *     Frees all dynamicly allocated structures in RSA_Key.
 */

static void *
dst_bsafe_free_key_structure(void *key)
{
	RSA_Key *r_key = (RSA_Key *) key;
	if (r_key != NULL) {
		if (r_key->rk_Private_Key)
			B_DestroyKeyObject(&r_key->rk_Private_Key);
		if (r_key->rk_Public_Key)
			B_DestroyKeyObject(&r_key->rk_Public_Key);
		SAFE_FREE2(r_key->rk_signer, strlen(r_key->rk_signer));
		SAFE_FREE(r_key);
	}
	return (NULL);
}


/*
 *  dst_bsafe_generate_keypair
 *	Generates unique keys that are hard to predict.
 *  Parameters
 *	key    generic Key structure
 *	exp    the public exponent
 *  Return 
 *	0 Failure 
 *	1 Success
 */

static int
dst_bsafe_generate_keypair(DST_KEY *key, int exp)
{
	int i, status;
	B_KEY_OBJ private;
	B_KEY_OBJ public;
	B_ALGORITHM_OBJ keypairGenerator;
	B_ALGORITHM_OBJ randomAlgorithm;
	A_RSA_KEY_GEN_PARAMS keygenParams;
	char exponent[4];
	int exponent_len;
	RSA_Key *rsa;
	POINTER randomSeed = NULL_PTR;
	int randomSeedLen;
	A_RSA_KEY *pk_access = NULL;

	if (key == NULL || key->dk_alg != KEY_RSA)
		return (0);

	if ((rsa = (RSA_Key *) malloc(sizeof(RSA_Key))) == NULL) {
		EREPORT(("dst_bsafe_generate_keypair: Memory allocation error 3"));
		return (0);
	}
	memset(rsa, 0, sizeof(*rsa));

	if ((status = B_CreateAlgorithmObject(&keypairGenerator)) != 0)
		return (0);

	keygenParams.modulusBits = key->dk_key_size;

	/* exp = 0 or 1 are special (mean 3 or F4) */
	if (exp == 0)
		exp = 3;
	else if (exp == 1)
		exp = 65537;

	/* Now encode the exponent and its length */
	if (exp < 256) {
		exponent_len = 1;
		exponent[0] = exp;
	} else if (exp < (1 << 16)) {
		exponent_len = 2;
		exponent[0] = exp >> 8;
		exponent[1] = exp;
	} else if (exp < (1 << 24)) {
		exponent_len = 3;
		exponent[0] = exp >> 16;
		exponent[1] = exp >> 8;
		exponent[2] = exp;
	} else {
		exponent_len = 4;
		exponent[0] = exp >> 24;
		exponent[1] = exp >> 16;
		exponent[2] = exp >> 8;
		exponent[3] = exp;
	}

	if ((keygenParams.publicExponent.data = (u_char *) malloc(exponent_len))
	    == NULL)
		return (0);
	memcpy(keygenParams.publicExponent.data, exponent, exponent_len);
	keygenParams.publicExponent.len = exponent_len;
	if ((status = B_SetAlgorithmInfo
	     (keypairGenerator, AI_RSAKeyGen, (POINTER) &keygenParams)) != 0)
		return (0);

	if ((status = B_GenerateInit(keypairGenerator, CHOOSER,
				     NULL_SURRENDER)) != 0)
		return (0);

	if ((status = B_CreateKeyObject(&public)) != 0)
		return (0);

	if ((status = B_CreateKeyObject(&private)) != 0)
		return (0);

	if ((status = B_CreateAlgorithmObject(&randomAlgorithm)) != 0)
		return (0);

	if ((status = B_SetAlgorithmInfo(randomAlgorithm, AI_MD5Random,
					 NULL_PTR))
	    != 0)
		return (0);

	if ((status = B_RandomInit(randomAlgorithm, CHOOSER,
				   NULL_SURRENDER)) != 0)
		return (0);

	randomSeedLen = 256;
	if ((randomSeed = malloc(randomSeedLen)) == NULL)
		return (0);
	if ((status = (randomSeed == NULL_PTR)) != 0)
		return (0);

	/* gets random seed from /dev/random if present, generates random
	 * values if it is not present. 
	 * first fill the buffer with semi random data 
	 * then fill as much as possible with good random data 
	 */
	i = dst_random(DST_RAND_SEMI, randomSeedLen, randomSeed);
	i += dst_random(DST_RAND_KEY,  randomSeedLen, randomSeed);

	if (i <= randomSeedLen) {
		SAFE_FREE(rsa);
		return(0);
	}
	if ((status = B_RandomUpdate(randomAlgorithm, randomSeed, 
				     randomSeedLen, NULL_SURRENDER)) != 0) {
		SAFE_FREE(rsa);
		return (0);
	}
	SAFE_FREE2(randomSeed, randomSeedLen);
	if ((status = B_GenerateKeypair(keypairGenerator, public, private,
					randomAlgorithm, NULL_SURRENDER))
	    != 0) {
		SAFE_FREE(rsa);
		return (0);
	}
	rsa->rk_signer = strdup(key->dk_key_name);
	rsa->rk_Private_Key = private;
	rsa->rk_Public_Key = public;
	key->dk_KEY_struct = (void *) rsa;

	B_GetKeyInfo((POINTER *) &pk_access, public, KI_RSAPublic);
	return (1);
}


/************************************************************************** 
 *  dst_bsafe_compare_keys
 *	Compare two keys for equality.
 *  Return
 *	0	  The keys are equal
 *	NON-ZERO   The keys are not equal
 */

static int
dst_s_bsafe_itemcmp(ITEM i1, ITEM i2)
{
	if (i1.len != i2.len || memcmp (i1.data, i2.data, i1.len))
		return (1);
	else
		return (0);
}

static int
dst_bsafe_compare_keys(const DST_KEY *key1, const DST_KEY *key2)
{
	int status, s1 = 0, s2 = 0;
	RSA_Key *rkey1 = (RSA_Key *) key1->dk_KEY_struct;
	RSA_Key *rkey2 = (RSA_Key *) key2->dk_KEY_struct;
	A_RSA_KEY *public1 = NULL, *public2 = NULL;
	A_PKCS_RSA_PRIVATE_KEY *p1 = NULL, *p2 = NULL;

	if (rkey1 == NULL && rkey2 == NULL) 
		return(0);
	else if (rkey1 == NULL) 
		return (1);
	else if (rkey2 == NULL)
		return (2);

	if (rkey1->rk_Public_Key) 
		B_GetKeyInfo((POINTER *) &public1, rkey1->rk_Public_Key, 
			     KI_RSAPublic);
	if (rkey2->rk_Public_Key) 
		B_GetKeyInfo((POINTER *) &public2, rkey2->rk_Public_Key, 
			     KI_RSAPublic);
	if (public1 == NULL && public2 == NULL)
		return (0);
	else if (public1 == NULL || public2 == NULL)
		return (1);

	status = dst_s_bsafe_itemcmp(public1->modulus, public2->modulus) ||
		 dst_s_bsafe_itemcmp(public1->exponent, public2->exponent);

	if (status) 
		return (status);

	if (rkey1->rk_Private_Key == NULL || rkey2->rk_Private_Key == NULL)
		/* if neither or only one is private key consider identical */
		return (status);  
	if (rkey1->rk_Private_Key)
		s1 = B_GetKeyInfo((POINTER *) &p1, rkey1->rk_Private_Key,
				  KI_PKCS_RSAPrivate);
	if (rkey2->rk_Private_Key)
		s2 = B_GetKeyInfo((POINTER *) &p2, rkey2->rk_Private_Key,
				  KI_PKCS_RSAPrivate);
	if (p1 == NULL || p2 == NULL) 
		return (0);

	status = dst_s_bsafe_itemcmp(p1->modulus, p2->modulus) ||
		dst_s_bsafe_itemcmp (p1->publicExponent, 
				     p2->publicExponent) ||
		dst_s_bsafe_itemcmp (p1->privateExponent, 
				     p2->privateExponent) ||
		dst_s_bsafe_itemcmp (p1->prime[0], p2->prime[0]) ||
		dst_s_bsafe_itemcmp (p1->prime[1], p2->prime[1]) ||
		dst_s_bsafe_itemcmp (p1->primeExponent[0], 
				     p2->primeExponent[0])||
		dst_s_bsafe_itemcmp (p1->primeExponent[1], 
				     p2->primeExponent[1])||
		dst_s_bsafe_itemcmp (p1->coefficient, p2->coefficient);
	return (status);
}


/* 
 * dst_bsafe_key_size() 
 * Function to calculate how the size of the key in bits
 */
static int
dst_bsafe_key_size(RSA_Key *r_key)
{
	int size;
	A_PKCS_RSA_PRIVATE_KEY *private = NULL;

	if (r_key == NULL)
		return (-1);
	if (r_key->rk_Private_Key)
		B_GetKeyInfo((POINTER *) &private, r_key->rk_Private_Key,
			     KI_PKCS_RSAPrivate);
	else if (r_key->rk_Public_Key)
		B_GetKeyInfo((POINTER *) &private, r_key->rk_Public_Key,
			     KI_RSAPublic);
	size = dst_s_calculate_bits(private->modulus.data,
				    private->modulus.len * 8);
	return (size);
}

/* 
 * dst_bsafe_md5digest(): function to digest data using MD5 digest function 
 * if needed 
 */
static int
dst_bsafe_md5digest(const int mode, B_ALGORITHM_OBJ *digest_obj,
		    const u_char *data, const int len,
		    u_char *digest, const int digest_len)
{
	int status = 0;
	u_int work_size = 0;

	if (digest_obj == NULL || *digest_obj == NULL) {
		printf("NO digest obj\n");
		exit(33);
	}

	if ((mode & SIG_MODE_INIT) &&
	    (status = B_DigestInit(*digest_obj, (B_KEY_OBJ) NULL,
				   CHOOSER, NULL_SURRENDER)))
		return (SIGN_INIT_FAILURE);

	if ((mode & SIG_MODE_UPDATE) && data && (len > 0) &&
	    (status = B_DigestUpdate(*digest_obj, data, len, NULL_SURRENDER)))
		return (SIGN_UPDATE_FAILURE);

	if (mode & SIG_MODE_FINAL) {
		if (digest == NULL ||
		    (status = B_DigestFinal(*digest_obj, digest, &work_size,
					    digest_len, NULL_SURRENDER)))
			return (SIGN_FINAL_FAILURE);
		return (work_size);
	}
	return (0);
}

/* 
 * just use the standard memory functions for bsafe 
 */
void
T_free(POINTER block)
{
	free(block);
}

POINTER
T_malloc(unsigned int len)
{
	return (malloc(len));
}

int
T_memcmp(CPOINTER firstBlock, CPOINTER secondBlock, unsigned int len)
{
	return (memcmp(firstBlock, secondBlock, len));
}

void
T_memcpy(POINTER output, CPOINTER input, unsigned int len)
{
	memcpy(output, input, len);
}

void
T_memmove(POINTER output, POINTER input, unsigned int len)
{
	memmove(output, input, len);
}

void
T_memset(POINTER output, int value, unsigned int len)
{
	memset(output, value, len);
}

POINTER
T_realloc(POINTER block, unsigned int len)
{
	return (realloc(block, len));
}

#else  /* BSAFE NOT available */
int
dst_bsafe_init()
{
	return (0);
}
#endif /* BSAFE */
