#ifndef DST_INTERNAL_H
#define DST_INTERNAL_H

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
#include <limits.h>
#include <sys/param.h>

#ifndef PATH_MAX
# ifdef POSIX_PATH_MAX
#  define PATH_MAX POSIX_PATH_MAX
# else
#  define PATH_MAX 255 /* this is the value of POSIX_PATH_MAX */
# endif
#endif 

typedef struct dst_key {
	char	*dk_key_name;   /* name of the key */
	int	dk_key_size;    /* this is the size of the key in bits */
	int	dk_proto;       /* what protocols this key can be used for */
	int	dk_alg;         /* algorithm number from key record */
	unsigned dk_flags;     /* and the flags of the public key */
	unsigned dk_id;        /* identifier of the key */
	void	*dk_KEY_struct; /* pointer to key in crypto pkg fmt */
	struct dst_func *dk_func; /* point to cryptto pgk specific function table */
} DST_KEY;
#define HAS_DST_KEY 

#include <isc-dhcp/dst.h>
/* 
 * define what crypto systems are supported for RSA, 
 * BSAFE is prefered over RSAREF; only one can be set at any time
 */
#if defined(BSAFE) && defined(RSAREF)
# error "Cannot have both BSAFE and RSAREF defined"
#endif

/* Declare dst_lib specific constants */
#define KEY_FILE_FORMAT "1.2"

/* suffixes for key file names */
#define PRIVATE_KEY		"private"
#define PUBLIC_KEY		"key"

/* error handling */
#ifdef REPORT_ERRORS
#define EREPORT(str)		printf str
#else
#define EREPORT(str)
#endif

/* use our own special macro to FRRE memory */

#ifndef SAFE_FREE
#define SAFE_FREE(a) if(a != NULL){memset(a,0, sizeof(*a)); free(a); a=NULL;}
#define SAFE_FREE2(a,s) if (a != NULL && s > 0){memset(a,0, s);free(a); a=NULL;}
#endif

typedef struct dst_func {
	int (*sign)(const int mode, DST_KEY *key, void **context,
		     const u_int8_t *data, const unsigned len,
		     u_int8_t *signature, const unsigned sig_len);
	int (*verify)(const int mode, DST_KEY *key, void **context,
		       const u_int8_t *data, const unsigned len,
		       const u_int8_t *signature, const unsigned sig_len);
	int (*compare)(const DST_KEY *key1, const DST_KEY *key2);
	int (*generate)(DST_KEY *key, int parms);
	void *(*destroy)(void *key);
	/* conversion functions */
	int (*to_dns_key)(const DST_KEY *key, u_int8_t *out,
			  const unsigned out_len);
	int (*from_dns_key)(DST_KEY *key, const u_int8_t *str,
			    const unsigned str_len);
	int (*to_file_fmt)(const DST_KEY *key, char *out,
			    const unsigned out_len);
	int (*from_file_fmt)(DST_KEY *key, const char *out,
			      const unsigned out_len);

} dst_func;

extern dst_func *dst_t_func[DST_MAX_ALGS];
extern const char *key_file_fmt_str;
extern const char *dst_path;

#ifndef DST_HASH_SIZE
#define DST_HASH_SIZE 20	/* RIPEMD160 and SHA-1 are 20 bytes MD5 is 16 */
#endif

#if 0
int dst_bsafe_init(void);
int dst_rsaref_init(void);
#endif

int dst_hmac_md5_init(void);

#if 0
int dst_cylink_init(void);
int dst_eay_dss_init(void);
#endif

/* support functions */
/* base64 to bignum conversion routines */
int       dst_s_conv_bignum_u8_to_b64( char *out_buf, const unsigned out_len, 
			               const char *header,
				       const u_int8_t *bin_data,
				       const unsigned bin_len);
int       dst_s_conv_bignum_b64_to_u8( const char **buf, u_int8_t *loc,
				       const unsigned loclen) ;
/* from higher level support routines */
int       dst_s_calculate_bits( const u_int8_t *str, const int max_bits); 
int       dst_s_verify_str( const char **buf, const char *str);


/* conversion between dns names and key file names */
size_t    dst_s_filename_length( const char *name, const char *suffix); 
int       dst_s_build_filename(  char *filename, const char *name, 
			         unsigned id, int alg, const char *suffix, 
			         size_t filename_length);

FILE      *dst_s_fopen (const char *filename, const char *mode, unsigned perm);

/* from file prandom.c */
int       dst_s_random( u_int8_t *output, unsigned size);
int       dst_s_semi_random( u_int8_t *output, unsigned size);
u_int32_t dst_s_quick_random( int inc);
void	  dst_s_quick_random_set( u_int32_t val, u_int32_t cnt);

/* 
 * read and write network byte order into u_int?_t  
 *  all of these should be retired
 */
u_int16_t dst_s_get_int16( const u_int8_t *buf);
void      dst_s_put_int16( u_int8_t *buf, const u_int16_t val);

u_int32_t dst_s_get_int32( const u_int8_t *buf);
void      dst_s_put_int32( u_int8_t *buf, const u_int32_t val);

#ifdef DUMP
# undef DUMP
# define DUMP(a,b,c,d) dst_s_dump(a,b,c,d)
#else
# define DUMP(a,b,c,d)
#endif


#endif /* DST_INTERNAL_H */
