/*
 *  Copyright (C) 2021 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#ifndef __SSS_H__
#define __SSS_H__

/* NOTE: we redefine some attributes if they are not already defined */
#ifndef ATTRIBUTE_PACKED
  #ifdef __GNUC__
    #define ATTRIBUTE_PACKED __attribute__((packed))
  #else
    #define ATTRIBUTE_PACKED
  #endif
#endif
#ifndef ATTRIBUTE_WARN_UNUSED_RET
  #ifdef __GNUC__
    #ifdef USE_WARN_UNUSED_RET
      #define ATTRIBUTE_WARN_UNUSED_RET __attribute__((warn_unused_result))
    #else
      #define ATTRIBUTE_WARN_UNUSED_RET
    #endif
  #else
    #define ATTRIBUTE_WARN_UNUSED_RET
  #endif
#endif


typedef enum { SSS_FALSE = 0, SSS_TRUE = 1 } boolean;

/* The final secret size in bytes, corresponding to the
 * size of an element in Fp with ~256 bit prime.
 */
#define SSS_SECRET_SIZE	32

/* Secrets and shares typedefs for "raw" SSS */
typedef struct ATTRIBUTE_PACKED {
	unsigned char secret[SSS_SECRET_SIZE];
} sss_secret;
typedef struct ATTRIBUTE_PACKED {
	/* Index x of the share on two byts (a short) */
	unsigned char index[2];
	/* Value of the share */
	unsigned char share[SSS_SECRET_SIZE];
} _sss_raw_share;

#define SSS_SESSION_ID_SIZE 16
/* We use SHA-256 for HMAC, so the size is 32 bytes */
#define SSS_HMAC_SIZE 32

/* Security wrapper for the secret for "secured" SSS */
typedef struct ATTRIBUTE_PACKED {
	_sss_raw_share raw_share;
	/* 128 bits session id */
	unsigned char session_id[SSS_SESSION_ID_SIZE];
	unsigned char raw_share_hmac[SSS_HMAC_SIZE];
} sss_share;

/* SSS shares and secret generation:
 *     Inputs:
 *         - n: is the number of shares to generate
 *         - k: the quorum of shares to regenerate the secret (of course k <= n)
 *         - secret: the secret value when input_secret is set to 'true'
 *     Output:
 *         - shares: a pointer to the generated n shares
 *         - secret: the secret value when input_secret is set to 'false', this
 *           value being randomly generated
 */
ATTRIBUTE_WARN_UNUSED_RET int sss_generate(sss_share *shares, unsigned short k, unsigned short n, sss_secret *secret, boolean input_secret);

/* SSS shares and secret combination
 *     Inputs:
 *         - k: the quorum of shares to regenerate the secret
 *         - shares: a pointer to the k shares
 *     Output:
 *         - secret: the secret value computed from the k shares
 */
ATTRIBUTE_WARN_UNUSED_RET int sss_combine(const sss_share *shares, unsigned short k, sss_secret *secret);

/* SSS shares regeneration from existing shares
 *     Inputs:
 *         - shares: a pointer to the input k shares allowing the regeneration
 *         - n: is the number of shares to regenerate
 *         - k: the input shares (of course k <= n)
 *     Output:
 *         - shares: a pointer to the generated n shares (among which the k first are
 *           the ones provided as inputs)
 *         - secret: the recomputed secret value
 */
ATTRIBUTE_WARN_UNUSED_RET int sss_regenerate(sss_share *shares, unsigned short k, unsigned short n, sss_secret *secret);

#endif /* __SSS_H__ */
