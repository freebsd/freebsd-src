#ifdef RSAREF
static const char rcsid[] = "$Header: /proj/cvs/isc/bind8/src/lib/dst/rsaref_link.c,v 1.9 2001/04/05 22:00:04 bwelling Exp $";

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
 * 1. Interface to the rsaref library to allow compilation when RSAREF is
 *    not available all calls to RSAREF are contained inside this file.
 * 2. The glue to connvert RSA{REF} KEYS to and from external formats
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

# ifdef __STDC__
#   define PROTOTYPES 1
# else
#   define PROTOTYPES 0
# endif

# include <global.h>
# include <rsaref.h>

#include "port_after.h"


typedef struct rsakey {
	char *rk_signer;
	R_RSA_PRIVATE_KEY *rk_Private_Key;
	R_RSA_PUBLIC_KEY *rk_Public_Key;
} RSA_Key;


static int dst_rsaref_sign(const int mode, DST_KEY *key, void **context,
			   const u_char *data, const int len,
			   u_char *signature, const int sig_len);
static int dst_rsaref_verify(const int mode, DST_KEY *key, void **context,
			     const u_char *data, const int len,
			     const u_char *signature, const int sig_len);

static int dst_rsaref_to_dns_key(const DST_KEY *public, u_char *out_str,
				 const int out_len);
static int dst_rsaref_from_dns_key(DST_KEY *s_key, const u_char *key,
				   const int len);

static int dst_rsaref_key_to_file_format(const DST_KEY *dkey,
					 u_char *buff,
					 const int buff_len);
static int dst_rsaref_key_from_file_format(DST_KEY *dkey,
					   const u_char *buff,
					   const int buff_len);

static int dst_rsaref_compare_keys(const DST_KEY *rkey1,
				   const DST_KEY *rkey2);
static void *dst_rsaref_free_key_structure(void *d_key);

static int dst_rsaref_generate_keypair(DST_KEY *key, const int exp);

static void dst_rsaref_init_random_struct(R_RANDOM_STRUCT * randomstruct);

/*
 * dst_rsaref_init()  Function to answer set up function pointers for RSAREF
 *	     related functions 
 */
int
dst_rsaref_init()
{
	if (dst_t_func[KEY_RSA] != NULL)
		return (1);
	dst_t_func[KEY_RSA] = malloc(sizeof(struct dst_func));
	if (dst_t_func[KEY_RSA] == NULL)
		return (0);
	memset(dst_t_func[KEY_RSA], 0, sizeof(struct dst_func));
	dst_t_func[KEY_RSA]->sign = dst_rsaref_sign;
	dst_t_func[KEY_RSA]->verify = dst_rsaref_verify;
	dst_t_func[KEY_RSA]->compare = dst_rsaref_compare_keys;
	dst_t_func[KEY_RSA]->generate = dst_rsaref_generate_keypair;
	dst_t_func[KEY_RSA]->destroy = dst_rsaref_free_key_structure;
	dst_t_func[KEY_RSA]->to_dns_key = dst_rsaref_to_dns_key;
	dst_t_func[KEY_RSA]->from_dns_key = dst_rsaref_from_dns_key;
	dst_t_func[KEY_RSA]->to_file_fmt = dst_rsaref_key_to_file_format;
	dst_t_func[KEY_RSA]->from_file_fmt = dst_rsaref_key_from_file_format;
	return (1);
}

/*
 * dst_rsa_sign
 *     Call RSAREF signing functions to sign a block of data.
 *     There are three steps to signing, INIT (initialize structures),
 *     UPDATE (hash (more) data), FINAL (generate a signature).	 This
 *     routine performs one or more of these steps.
 * Parameters
 *     mode	   SIG_MODE_INIT, SIG_MODE_UPDATE and/or SIG_MODE_FINAL.
 *     key	   pointer to a RSA key structure that points to public key
 *		   and context to use.
 *     data	   data to be signed.
 *     len	   length in bytes of data.
 *     signature   location to store signature.
 *     sig_len	   size of the signature storage area
 * returns
 *	  N  Success on SIG_MODE_FINAL = returns signature length in bytes
 *	  0  Success on SIG_MODE_INIT  and UPDATE
 *	 <0  Failure
 */


static int
dst_rsaref_sign(const int mode, DST_KEY *dkey, void **context,
		const u_char *data, const int len,
		u_char *signature, const int sig_len)
{
	int sign_len = 0;
	R_SIGNATURE_CTX *ctx = NULL;

	if (mode & SIG_MODE_INIT)
		ctx = malloc(sizeof(*ctx));
	else if (context) 
		ctx = (R_SIGNATURE_CTX *) *context;
	if (ctx == NULL) 
		return (-1);

	if ((mode & SIG_MODE_INIT) && R_SignInit(ctx, DA_MD5))
		return (SIGN_INIT_FAILURE);

	/* equivalent of SIG_MODE_UPDATE */
	if ((mode & SIG_MODE_UPDATE) && (data && len > 0) &&
	    R_SignUpdate(ctx, (u_char *) data, len))
		return (SIGN_UPDATE_FAILURE);

	if (mode & SIG_MODE_FINAL) {
		RSA_Key *key = (RSA_Key *) dkey->dk_KEY_struct;
		if (signature == NULL ||
		    sig_len < (int)(key->rk_Public_Key->bits + 7) / 8)
			return (SIGN_FINAL_FAILURE);
		if(key == NULL || key->rk_Private_Key == NULL)
			return (-1);
		if (R_SignFinal(ctx, signature, &sign_len,
				key->rk_Private_Key))
			return (SIGN_FINAL_FAILURE);
		SAFE_FREE(ctx);
	}
	else {
		if (context == NULL) 
			return (-1);
		*context = (void *) ctx;
	}
	return (sign_len);
}


/*
 * dst_rsaref_verify()
 *     Calls RSAREF verification routines.  There are three steps to
 *     verification, INIT (initialize structures), UPDATE (hash (more) data),
 *     FINAL (generate a signature).  This routine performs one or more of
 *     these steps.
 * Parameters
 *     mode	   SIG_MODE_INIT, SIG_MODE_UPDATE and/or SIG_MODE_FINAL.
 *     key	   pointer to a RSA key structure that points to public key
 *		   and context to use.
 *     data	   data signed.
 *     len	   length in bytes of data.
 *     signature   signature.
 *     sig_len	   length in bytes of signature.
 * returns
 *     0  Success
 *    <0  Failure
 */

static int
dst_rsaref_verify(const int mode, DST_KEY *dkey, void **context,
		  const u_char *data,	const int len, 
		  const u_char *signature, const int sig_len)
{
	R_SIGNATURE_CTX *ctx = NULL;

	if (mode & SIG_MODE_INIT)
		ctx = malloc(sizeof(*ctx));
	else if (context) 
		ctx = (R_SIGNATURE_CTX *) *context;
	if (ctx == NULL) 
		return (-1);

	if ((mode & SIG_MODE_INIT) && R_VerifyInit(ctx, DA_MD5))
		return (VERIFY_INIT_FAILURE);

	if ((mode & SIG_MODE_UPDATE) && (data && len > 0) &&
	    R_VerifyUpdate(ctx, (u_char *) data, len))
		return (VERIFY_UPDATE_FAILURE);

	if ((mode & SIG_MODE_FINAL)) {
		RSA_Key *key = (RSA_Key *) dkey->dk_KEY_struct;

		if (key == NULL || key->rk_Public_Key == NULL)
			return (-1);
		if (signature == NULL || sig_len <= 0)
			return (VERIFY_FINAL_FAILURE);
		if (R_VerifyFinal(ctx, (u_char *) signature, sig_len, 
				  key->rk_Public_Key))
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
 * dst_rsaref_to_dns_key
 *     Converts key in RSAREF to DNS distribution format
 *     This function gets in a pointer to the public key and a work area
 *     to write the key into.
 * Parameters
 *     public	 KEY structure
 *     out_str	 buffer to write encoded key into
 *     out_len	 size of out_str
 * Return
 *	N >= 0 length of encoded key
 *	n < 0  error
 */

static int
dst_rsaref_to_dns_key(const DST_KEY *in_key, u_char *out_str,
		      const int out_len)
{
	int n, loc;
	R_RSA_PUBLIC_KEY *public;
	u_char *op = (u_char *) out_str;

	if (in_key == NULL || in_key->dk_KEY_struct == NULL ||
	    out_len <= 0 || out_str == NULL)
		return (-1);
	public = (R_RSA_PUBLIC_KEY *)
		((RSA_Key *) in_key->dk_KEY_struct)->rk_Public_Key;
	if (public == NULL)
		return (-1);

	memset(op, 0, out_len);

	/* find first non zero */
	for (n = 0; public->exponent[n] == 0x0; n++) ;	

	n = (MAX_RSA_MODULUS_LEN - n);	/* find lenght of exponent */
	*op++ = (u_int8_t) n;

	if (n > (out_len - (op-out_str)))
		return (-1);
	memcpy(op, &public->exponent[MAX_RSA_MODULUS_LEN - n], n);
	op += n;
	n++;			/* include the lenght field in this count */

	/* find first non zero */
	for (loc = 0; public->modulus[loc] == 0x0; loc++) ;

	/*copy exponent */
	if ((MAX_RSA_MODULUS_LEN - loc) > (out_len - (op-out_str)))
		return (-1);
	memcpy(op, &public->modulus[loc], MAX_RSA_MODULUS_LEN - loc);
	n += (MAX_RSA_MODULUS_LEN - loc);
	return (n);
}


/*
 * dst_rsaref_from_dns_key
 *     Converts from a DNS KEY RR format to an RSA KEY.
 * Parameters
 *     len    Length in bytes of DNS key
 *     key    DNS key
 *     name   Key name
 *     s_key  DST structure that will point to the RSA key this routine
 *		  will build.
 * Return
 *    -1   The input key has fields that are larger than this package supports
 *     0   The input key, s_key or name was null.
 *     1   Success
 */
static int
dst_rsaref_from_dns_key(DST_KEY *s_key, const u_char *key, const int len)
{
	int bytes;
	u_char *key_ptr;
	RSA_Key *r_key;

	if (key == NULL || s_key == NULL || len < 0)
		return (0);

	if (s_key->dk_KEY_struct) {	/* do not reuse */
		dst_rsaref_free_key_structure(s_key->dk_KEY_struct);
		s_key->dk_KEY_struct = NULL;
	}
	if (len == 0)		/* null key no conversion needed */
		return (1);

	if ((r_key = (RSA_Key *) malloc(sizeof(RSA_Key))) == NULL) {
		EREPORT(("dst_rsaref_from_dns_key(): Memory allocation error 1\n"));
		return (0);
	}
	memset(r_key, 0, sizeof(RSA_Key));
	s_key->dk_KEY_struct = (void *) r_key;
	r_key->rk_signer = strdup(s_key->dk_key_name);
	r_key->rk_Public_Key = (R_RSA_PUBLIC_KEY *)
		malloc(sizeof(R_RSA_PUBLIC_KEY));
	if (r_key->rk_Public_Key == NULL) {
		EREPORT(("dst_rsaref_from_dns_key(): Memory allocation error 3\n"));
		return (0);
	}
	memset(r_key->rk_Public_Key, 0, sizeof(R_RSA_PUBLIC_KEY));
	key_ptr = (u_char *) key;
	bytes = (int) *key_ptr++;	/* length of exponent in bytes */
	if (bytes == 0) {		/* special case for long exponents */
		bytes = (int) dst_s_get_int16(key_ptr);
		key_ptr += sizeof(u_int16_t);
	}
	if (bytes > MAX_RSA_MODULUS_LEN) { 
		dst_rsaref_free_key_structure(r_key);
		return (-1);
	}
	memcpy(&r_key->rk_Public_Key->exponent[MAX_RSA_MODULUS_LEN - bytes],
	       key_ptr, bytes);

	key_ptr += bytes;	/* beginning of modulus */
	bytes = len - bytes - 1;	/* length of modulus */
	if (bytes > MAX_RSA_MODULUS_LEN) { 
		dst_rsaref_free_key_structure(r_key);
		return (-1);
	}
	memcpy(&r_key->rk_Public_Key->modulus[MAX_RSA_MODULUS_LEN - bytes],
	       key_ptr, bytes);
	r_key->rk_Public_Key->bits = bytes * 8;
	s_key->dk_key_size = r_key->rk_Public_Key->bits;

	return (1);
}


/*
 *  dst_rsaref_key_to_file_format
 *	Encodes an RSA Key into the portable file format.
 *  Parameters
 *	rkey	  RSA KEY structure
 *	buff	  output buffer
 *	buff_len  size of output buffer
 *  Return
 *	0  Failure - null input rkey
 *     -1  Failure - not enough space in output area
 *	N  Success - Length of data returned in buff
 */

static int
dst_rsaref_key_to_file_format(const DST_KEY *in_key, u_char *buff,
			      const int buff_len)
{
	u_char *bp;
	int len, b_len;
	R_RSA_PRIVATE_KEY *rkey;

	if (in_key == NULL || in_key->dk_KEY_struct == NULL)
		return (-1);
	rkey = (R_RSA_PRIVATE_KEY *)
		((RSA_Key *) in_key->dk_KEY_struct)->rk_Private_Key;
	if (rkey == NULL)	/* no output */
		return (0);
	if (buff == NULL || buff_len <= (int) strlen(key_file_fmt_str))
		return (-1);	/* no OR not enough space in output area */

	    memset(buff, 0, buff_len);	/* just in case */
	/* write file header */
	sprintf(buff, key_file_fmt_str, KEY_FILE_FORMAT, KEY_RSA, "RSA");

	bp = (char *) strchr(buff, '\0');
	b_len = buff_len - (bp - buff);
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Modulus: ",
					       rkey->modulus,
					       MAX_RSA_MODULUS_LEN)) <= 0)
		return (-1);

	bp += len;
	b_len -= len;
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "PublicExponent: ",
					       rkey->publicExponent,
					       MAX_RSA_MODULUS_LEN)) <= 0)
		return (-2);
	bp += len;
	b_len -= len;
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "PrivateExponent: ",
					       rkey->exponent,
					       MAX_RSA_MODULUS_LEN)) <= 0)
		return (-3);
	bp += len;
	b_len -= len;
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Prime1: ",
					       rkey->prime[0],
					       MAX_RSA_PRIME_LEN)) < 0)
		return (-4);
	bp += len;
	b_len -= len;
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Prime2: ",
					       rkey->prime[1],
					       MAX_RSA_PRIME_LEN)) < 0)
		return (-5);
	bp += len;
	b_len -= len;
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Exponent1: ",
					       rkey->primeExponent[0],
					       MAX_RSA_PRIME_LEN)) < 0)
		return (-6);
	bp += len;
	b_len -= len;
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Exponent2: ",
					       rkey->primeExponent[1],
					       MAX_RSA_PRIME_LEN)) < 0)
		return (-7);
	bp += len;
	b_len -= len;
	if ((len = dst_s_conv_bignum_u8_to_b64(bp, b_len, "Coefficient: ",
					       rkey->coefficient,
					       MAX_RSA_PRIME_LEN)) < 0)
		return (-8);
	bp += len;
	b_len -= len;
	return (buff_len - b_len);
}


/*
 * dst_rsaref_key_from_file_format
 *     Converts contents of a private key file into a private RSA key.
 * Parameters
 *     r_key	  structure to put key into
 *     buff	  buffer containing the encoded key
 *     buff_len	  the length of the buffer
 * Return
 *     n >= 0 Foot print of the key converted
 *     n <  0 Error in conversion
 */

static int
dst_rsaref_key_from_file_format(DST_KEY *d_key, const u_char *buff,
				const int buff_len)
{
	const char *p = (char *) buff;
	R_RSA_PRIVATE_KEY key;
	int foot = -1;
	RSA_Key *r_key;

	if (d_key == NULL || buff == NULL || buff_len < 0)
		return (-1);

	memset(&key, 0, sizeof(key));

	if (!dst_s_verify_str(&p, "Modulus: "))
		return (-3);

	if (!dst_s_conv_bignum_b64_to_u8(&p, key.modulus, MAX_RSA_MODULUS_LEN))
		return (-4);

	key.bits = dst_s_calculate_bits(key.modulus, MAX_RSA_MODULUS_BITS);

	while (*++p && p < (char *) &buff[buff_len]) {
		if (dst_s_verify_str(&p, "PublicExponent: ")) {
			if (!dst_s_conv_bignum_b64_to_u8(&p,
							 key.publicExponent,
						       MAX_RSA_MODULUS_LEN))
				return (-5);
		} else if (dst_s_verify_str(&p, "PrivateExponent: ")) {
			if (!dst_s_conv_bignum_b64_to_u8(&p, key.exponent,
						       MAX_RSA_MODULUS_LEN))
				return (-6);
		} else if (dst_s_verify_str(&p, "Prime1: ")) {
			if (!dst_s_conv_bignum_b64_to_u8(&p, key.prime[0],
							 MAX_RSA_PRIME_LEN))
				return (-7);
		} else if (dst_s_verify_str(&p, "Prime2: ")) {
			if (!dst_s_conv_bignum_b64_to_u8(&p, key.prime[1],
							 MAX_RSA_PRIME_LEN))
				return (-8);
		} else if (dst_s_verify_str(&p, "Exponent1: ")) {
			if (!dst_s_conv_bignum_b64_to_u8(&p,
						       key.primeExponent[0],
							 MAX_RSA_PRIME_LEN))
				return (-9);
		} else if (dst_s_verify_str(&p, "Exponent2: ")) {
			if (!dst_s_conv_bignum_b64_to_u8(&p,
						       key.primeExponent[1],
							 MAX_RSA_PRIME_LEN))
				return (-10);
		} else if (dst_s_verify_str(&p, "Coefficient: ")) {
			if (!dst_s_conv_bignum_b64_to_u8(&p, key.coefficient,
							 MAX_RSA_PRIME_LEN))
				return (-11);
		} else {
			EREPORT(("dst_rsaref_key_from_file_format: Bad keyword %s\n", p));
			return (-12);
		}
	}			/* while p */

	r_key = (RSA_Key *) malloc(sizeof(RSA_Key));
	if (r_key == NULL) {
		return (-2);
	}
	memset(r_key, 0, sizeof(*r_key));

	r_key->rk_Private_Key =
		(R_RSA_PRIVATE_KEY *) malloc(sizeof(R_RSA_PRIVATE_KEY));
	if (r_key->rk_Private_Key == NULL) {
		EREPORT(("dst_rsaref_key_from_file_format: Memory allocation error\n"));
		return (-13);
	}
	r_key->rk_Public_Key = (R_RSA_PUBLIC_KEY *) r_key->rk_Private_Key;
	memcpy(r_key->rk_Private_Key, &key, sizeof(R_RSA_PRIVATE_KEY));

	r_key->rk_signer = strdup(d_key->dk_key_name);
	d_key->dk_KEY_struct = (void *) r_key;
	d_key->dk_key_size = r_key->rk_Private_Key->bits;

	return (0);
}



/*
 *  dst_rsaref_compare_keys
 *	Compare two keys for equality.
 *  Return
 *	0	   The keys are equal
 *	NON-ZERO   The keys are not equal
 */

static int
dst_rsaref_compare_keys(const DST_KEY *dkey1, const DST_KEY *dkey2)
{
	RSA_Key *rkey1 = (RSA_Key *) dkey1->dk_KEY_struct;
	RSA_Key *rkey2 = (RSA_Key *) dkey2->dk_KEY_struct;
       
	if (rkey1 == NULL && rkey2 == NULL)
		return (0); /* same */
	else if (rkey1 == NULL)
		return (1);
	else if (rkey2 == NULL)
		return (2);
	return (memcmp(rkey1->rk_Public_Key, rkey2->rk_Public_Key,
		       sizeof(R_RSA_PUBLIC_KEY)));
}

/*
 *  dst_rsaref_generate_keypair
 *	Generates unique keys that are hard to predict.
 *  Parameters
 *	key    generic Key structure
 *	exp    the public exponent
 *  Return
 *	0 Failure
 *	1 Success
 */

static int
dst_rsaref_generate_keypair(DST_KEY *key, const int exp)
{
	R_RSA_PUBLIC_KEY *public;
	R_RSA_PRIVATE_KEY *private;
	R_RSA_PROTO_KEY proto;
	R_RANDOM_STRUCT randomStruct;
	RSA_Key *rsa;
	int status;

	if (key == NULL || key->dk_alg != KEY_RSA)
		return (0);
	if (key->dk_key_size < MIN_RSA_MODULUS_BITS ||
	    key->dk_key_size > MAX_RSA_MODULUS_BITS) {
		EREPORT(("dst_rsaref_generate_keypair: Invalid key size\n"));
		return (0);	/* these are the limits on key size in RSAREF */
	}
	/* allocate space */
	if ((public = (R_RSA_PUBLIC_KEY *) malloc(sizeof(R_RSA_PUBLIC_KEY)))
	    == NULL) {
		EREPORT(("dst_rsaref_generate_keypair: Memory allocation error 1\n"));
		return (0);
	}
	if ((private = (R_RSA_PRIVATE_KEY *) malloc(sizeof(R_RSA_PRIVATE_KEY)))
	    == NULL) {
		EREPORT(("dst_rsaref_generate_keypair: Memory allocation error 2\n"));
		return (0);
	}
	if ((rsa = (RSA_Key *) malloc(sizeof(RSA_Key))) == NULL) {
		EREPORT(("dst_rsaref_generate_keypair: Memory allocation error 3\n"));
		return (0);
	}
	memset(public, 0, sizeof(*public));
	memset(private, 0, sizeof(*private));

	proto.bits = key->dk_key_size;
	proto.useFermat4 = exp ? 0x1 : 0x0;	/* 1 for f4=65537, 0 for f0=3 */
	EREPORT(("\ndst_rsaref_generate_keypair: Generating KEY for %s Please wait\n",
		 key->dk_key_name));

	/* set up random seed */
	dst_rsaref_init_random_struct(&randomStruct);

	/* generate keys */
	status = R_GeneratePEMKeys(public, private, &proto, &randomStruct);
	if (status) {
		EREPORT(("dst_rsaref_generate_keypair: No Key Pair generated %d\n",
			 status));
		SAFE_FREE(public);
		SAFE_FREE(private);
		SAFE_FREE(rsa);
		return (0);
	}
	memset(rsa, 0, sizeof(*rsa));
	rsa->rk_signer = key->dk_key_name;
	rsa->rk_Private_Key = private;
	rsa->rk_Public_Key = public;
	key->dk_KEY_struct = (void *) rsa;

	return (1);
}


/*
 * dst_rsaref_free_key_structure
 *     Frees all dynamicly allocated structures in r_key
 */

static void *
dst_rsaref_free_key_structure(void *v_key)
{
	RSA_Key *r_key = (RSA_Key *) v_key;

	if (r_key != NULL) {
		if ((void *) r_key->rk_Private_Key == (void *) r_key->rk_Public_Key)
			r_key->rk_Public_Key = NULL;
		SAFE_FREE(r_key->rk_Private_Key);
		SAFE_FREE(r_key->rk_Public_Key);
		SAFE_FREE(r_key->rk_signer);
		SAFE_FREE(r_key);
	}
	return (NULL);
}


/*
 * dst_rsaref_init_random_struct
 *     A random seed value is used in key generation.
 *     This routine gets a bunch of system values to randomize the
 *     randomstruct.  A number of system calls are used to get somewhat
 *     unpredicable values, then a special function dst_s_prandom() is called
 *     that will do some magic depending on the system used.
 *     If this function is executed on reasonably busy machine then the values
 *     that prandom uses are hard to
 *	 1. Predict
 *	 2. Regenerate
 *	 3. Hard to spy on as nothing is stored to disk and data is consumed
 *	    as fast as it is generated.
 */

static void
dst_rsaref_init_random_struct(R_RANDOM_STRUCT * randomstruct)
{
	unsigned bytesNeeded;
	struct timeval tv;
	u_char *array;
	int n;

	R_RandomInit(randomstruct);

	/* The runtime of the script is unpredictable within some range
	 * thus I'm getting the time of day again as this is an hard to guess
	 * value and the number of characters of the output from the script is
	 * hard to guess.
	 * This must be the FIRST CALL
	 */
	gettimeofday(&tv, 0);
	R_RandomUpdate(randomstruct, (u_char *) &tv,
		       sizeof(struct timeval));

	/*
	 * first find out how many bytes I need
	 */
	R_GetRandomBytesNeeded(&bytesNeeded, randomstruct);

	/*
	 * get a storage area for it  addjust the area for the possible
	 * side effects of digest functions writing out in blocks
	 */
	array = (u_char *) malloc(bytesNeeded);

	/* extract the random data from /dev/random if present, generate
	 *   it if not present 
	 * first fill the buffer with semi random data 
	 *  then fill as much as possible with good random data 
	 */
	n = dst_random(DST_RAND_SEMI, bytesNeeded, array);
	n += dst_random(DST_RAND_KEY, bytesNeeded, array);
	if (n <= bytesNeeded) {
		SAFE_FREE(array);
		return(0);
	}

	/* supply the random data (even if it is larger than requested) */
	R_RandomUpdate(randomstruct, array, bytesNeeded);

	SAFE_FREE(array);

	R_GetRandomBytesNeeded(&bytesNeeded, randomstruct);
	if (bytesNeeded) {
		EREPORT(("InitRandomStruct() didn't initialize enough randomness\n"));
		exit(33);
	}
}


#else 
int /* rsaref is not available */
dst_rsaref_init()
{
	return (0);
}
#endif /* RSAREF */
