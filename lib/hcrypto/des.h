/*
 * Copyright (c) 2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id$ */

#ifndef _DESperate_H
#define _DESperate_H 1

/* symbol renaming */
#define	_DES_ipfp_test _hc_DES_ipfp_test
#define DES_cbc_cksum hc_DES_cbc_cksum
#define DES_cbc_encrypt hc_DES_cbc_encrypt
#define DES_cfb64_encrypt hc_DES_cfb64_encrypt
#define DES_check_key_parity hc_DES_check_key_parity
#define DES_ecb3_encrypt hc_DES_ecb3_encrypt
#define DES_ecb_encrypt hc_DES_ecb_encrypt
#define DES_ede3_cbc_encrypt hc_DES_ede3_cbc_encrypt
#define DES_encrypt hc_DES_encrypt
#define DES_generate_random_block hc_DES_generate_random_block
#define DES_init_random_number_generator hc_DES_init_random_number_generator
#define DES_is_weak_key hc_DES_is_weak_key
#define DES_key_sched hc_DES_key_sched
#define DES_new_random_key hc_DES_new_random_key
#define DES_pcbc_encrypt hc_DES_pcbc_encrypt
#define DES_rand_data hc_DES_rand_data
#define DES_random_key hc_DES_random_key
#define DES_read_password hc_DES_read_password
#define DES_set_key hc_DES_set_key
#define DES_set_key_checked hc_DES_set_key_checked
#define DES_set_key_unchecked hc_DES_set_key_unchecked
#define DES_set_key_sched hc_DES_set_key_sched
#define DES_set_odd_parity hc_DES_set_odd_parity
#define DES_set_random_generator_seed hc_DES_set_random_generator_seed
#define DES_set_sequence_number hc_DES_set_sequence_number
#define DES_string_to_key hc_DES_string_to_key

/*
 *
 */

#define DES_CBLOCK_LEN 8
#define DES_KEY_SZ 8

#define DES_ENCRYPT 1
#define DES_DECRYPT 0

typedef unsigned char DES_cblock[DES_CBLOCK_LEN];
typedef struct DES_key_schedule
{
	uint32_t ks[32];
} DES_key_schedule;

/*
 *
 */

#ifndef HC_DEPRECATED
#if defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1 )))
#define HC_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER) && (_MSC_VER>1200)
#define HC_DEPRECATED __declspec(deprecated)
#else
#define HC_DEPRECATED
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

void	DES_set_odd_parity(DES_cblock *);
int	DES_check_key_parity(DES_cblock *);
int	DES_is_weak_key(DES_cblock *);
int	HC_DEPRECATED DES_set_key(DES_cblock *, DES_key_schedule *);
int	DES_set_key_checked(DES_cblock *, DES_key_schedule *);
int	DES_set_key_unchecked(DES_cblock *, DES_key_schedule *);
int	DES_key_sched(DES_cblock *, DES_key_schedule *);
void	DES_string_to_key(const char *, DES_cblock *);
int	DES_read_password(DES_cblock *, char *, int);

void	HC_DEPRECATED DES_rand_data(void *, int);
void	HC_DEPRECATED DES_set_random_generator_seed(DES_cblock *);
void	HC_DEPRECATED DES_generate_random_block(DES_cblock *);
void	HC_DEPRECATED DES_set_sequence_number(void *);
void 	HC_DEPRECATED DES_init_random_number_generator(DES_cblock *);
void	HC_DEPRECATED DES_random_key(DES_cblock *);
int	HC_DEPRECATED DES_new_random_key(DES_cblock *);


void	DES_encrypt(uint32_t [2], DES_key_schedule *, int);
void	DES_ecb_encrypt(DES_cblock *, DES_cblock *, DES_key_schedule *, int);
void	DES_ecb3_encrypt(DES_cblock *,DES_cblock *, DES_key_schedule *,
			 DES_key_schedule *, DES_key_schedule *, int);
void	DES_pcbc_encrypt(const void *, void *, long,
			 DES_key_schedule *, DES_cblock *, int);
void	DES_cbc_encrypt(const void *, void *, long,
			DES_key_schedule *, DES_cblock *, int);
void	DES_ede3_cbc_encrypt(const void *, void *, long,
			     DES_key_schedule *, DES_key_schedule *,
			     DES_key_schedule *, DES_cblock *, int);
void	DES_cfb64_encrypt(const void *, void *, long,
			  DES_key_schedule *, DES_cblock *, int *, int);


uint32_t DES_cbc_cksum(const void *, DES_cblock *,
		      long, DES_key_schedule *, DES_cblock *);


void	_DES_ipfp_test(void);

#ifdef  __cplusplus
}
#endif


#endif /* _DESperate_H */
