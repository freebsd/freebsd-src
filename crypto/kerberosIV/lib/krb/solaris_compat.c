/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
/* $FreeBSD$ */

#include "krb_locl.h"

RCSID("$Id: solaris_compat.c,v 1.4 1999/12/02 16:58:44 joda Exp $");

#if (SunOS + 0) >= 50
/*
 * Compatibility with solaris' libkrb.
 */

int32_t
_C0095C2A(void *in, void *out, u_int32_t length, 
	  des_key_schedule schedule, des_cblock *key, 
	  struct sockaddr_in *sender, struct sockaddr_in *receiver)
{
    return krb_mk_priv (in, out, length, schedule, key, sender, receiver);
}

int32_t
_C0095C2B(void *in, u_int32_t in_length, 
	  des_key_schedule schedule, des_cblock *key, 
	  struct sockaddr_in *sender, struct sockaddr_in *receiver, 
	  MSG_DAT *m_data)
{
    return krb_rd_priv (in, in_length, schedule, key,
			sender, receiver, m_data);
}

void
_C0095B2B(des_cblock *input,des_cblock *output,
	  des_key_schedule ks,int enc)
{
    des_ecb_encrypt(input, output, ks, enc);
}

void
_C0095B2A(des_cblock (*input),
	  des_cblock (*output),
	  long length,
	  des_key_schedule schedule,
	  des_cblock (*ivec),
	  int encrypt)
{
  des_cbc_encrypt(input, output, length, schedule, ivec, encrypt);
}

void
_C0095B2C(des_cblock (*input),
	  des_cblock (*output),
	  long length,
	  des_key_schedule schedule,
	  des_cblock (*ivec),
	  int encrypt)
{
  des_pcbc_encrypt(input, output, length, schedule, ivec, encrypt);
}
#endif /* (SunOS-0) >= 50 */
