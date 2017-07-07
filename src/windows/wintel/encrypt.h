/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)encrypt.h	8.1 (Berkeley) 6/4/93
 */

/*
 * Copyright (C) 1990 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#ifdef	ENCRYPTION

#ifndef __ENCRYPTION__
#define __ENCRYPTION__

#define	DIR_DECRYPT		1
#define	DIR_ENCRYPT		2

typedef	unsigned char Block[8];
typedef unsigned char *BlockT;
typedef struct { Block _; } Schedule[16];

#define	VALIDKEY(key)	( key[0] | key[1] | key[2] | key[3] | key[4] | key[5] | key[6] | key[7])

#define	SAMEKEY(k1, k2)	(!memcmp((void *)k1, (void *)k2, sizeof(Block)))

typedef	struct {
  short		type;
  int		length;
  unsigned char	*data;
} Session_Key;

#ifdef DEBUG
int printsub(char, unsigned char *, size_t);
#endif

void encrypt_parse(kstream, unsigned char *, int);

typedef struct {
  char	*name;
  int	type;
  void	(*output) (unsigned char *, int);
  int	(*input) (int);
  void	(*init) (int);
  int	(*start) (int, int);
  int	(*is) (unsigned char *, int);
  int	(*reply) (unsigned char *, int);
  void	(*session) (Session_Key *, int);
  int	(*keyid) (int, unsigned char *, int *);
  void	(*printsub) (unsigned char *, int, unsigned char *, int);
} Encryptions;

#define	SK_DES		1	/* Matched Kerberos v5 ENCTYPE_DES */

void encrypt_init (kstream, kstream_ptr);
Encryptions *findencryption (int);
void encrypt_auto (int);
void decrypt_auto (int);
void encrypt_is (unsigned char *, int);
void encrypt_reply (unsigned char *, int);
void encrypt_start_input (int);
void encrypt_session_key (Session_Key *, int);
void encrypt_end_input (void);
void encrypt_start_output (int);
void encrypt_end_output (void);
void encrypt_send_request_start (void);
void encrypt_send_request_end (void);
void encrypt_send_end (void);
void encrypt_wait (void);
int encrypt_is_encrypting (void);
void encrypt_send_support (void);
void encrypt_send_keyid (int, unsigned char *, int, int);
int net_write (unsigned char *, int);

int encrypt_cmd (int, char **);
void encrypt_display (void);

void krbdes_encrypt (unsigned char *, int);
int krbdes_decrypt (int);
int krbdes_is (unsigned char *, int);
int krbdes_reply (unsigned char *, int);
void krbdes_init (int);
int krbdes_start (int, int);
void krbdes_session (Session_Key *, int);
void krbdes_printsub (unsigned char *, int, unsigned char *, int);

void cfb64_encrypt (unsigned char *, int);
int cfb64_decrypt (int);
void cfb64_init (int);
int cfb64_start (int, int);
int cfb64_is (unsigned char *, int);
int cfb64_reply (unsigned char *, int);
void cfb64_session (Session_Key *, int);
int cfb64_keyid (int, unsigned char *, int *);
void cfb64_printsub (unsigned char *, int, unsigned char *, int);

void ofb64_encrypt (unsigned char *, int);
int ofb64_decrypt (int);
void ofb64_init (int);
int ofb64_start (int, int);
int ofb64_is (unsigned char *, int);
int ofb64_reply (unsigned char *, int);
void ofb64_session (Session_Key *, int);
int ofb64_keyid (int, unsigned char *, int *);
void ofb64_printsub (unsigned char *, int, unsigned char *, int);

int KRB5_CALLCONV
	des_new_random_key (Block);
void KRB5_CALLCONV
	des_set_random_generator_seed (Block);
void KRB5_CALLCONV
	des_key_sched (Block, Schedule);
void KRB5_CALLCONV
	des_ecb_encrypt (Block, Block, Schedule, int);

/* int	des_string_to_key (char *, Block); */


#ifdef DEBUG
extern int encrypt_debug_mode;
#endif

extern int (*decrypt_input) (int);
extern void (*encrypt_output) (unsigned char *, int);

int decrypt_ks_hack(unsigned char *, int);

#endif /* __ENCRYPTION__ */

#endif /* ENCRYPTION */
