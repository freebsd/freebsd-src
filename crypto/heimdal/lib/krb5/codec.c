/*
 * Copyright (c) 1998 - 1999 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"

RCSID("$Id: codec.c,v 1.6 1999/12/02 17:05:08 joda Exp $");

/* these functions does what the normal asn.1-functions does, but
   converts the keytype to/from the on-the-wire enctypes */

#if 1
#define DECODE(T, K) return decode_ ## T(data, length, t, len)
#define ENCODE(T, K) return encode_ ## T(data, length, t, len)
#else
#define DECODE(T, K)					\
{							\
    krb5_error_code ret;				\
    ret = decode_ ## T((void*)data, length, t, len);	\
    if(ret)						\
	return ret;					\
    if(K)						\
	ret = krb5_decode_keyblock(context, (K), 1);	\
    return ret;						\
}

#define ENCODE(T, K)					\
{							\
    krb5_error_code ret = 0;				\
    if(K)						\
	ret = krb5_decode_keyblock(context, (K), 0);	\
    if(ret)						\
	return ret;					\
    return encode_ ## T(data, length, t, len);		\
}
#endif

krb5_error_code
krb5_decode_EncTicketPart (krb5_context context,
			   const void *data,
			   size_t length,
			   EncTicketPart *t,
			   size_t *len)
{
    DECODE(EncTicketPart, &t->key);
}

krb5_error_code
krb5_encode_EncTicketPart (krb5_context context,
			   void *data,
			   size_t length,
			   EncTicketPart *t,
			   size_t *len)
{
    ENCODE(EncTicketPart, &t->key);
}

krb5_error_code
krb5_decode_EncASRepPart (krb5_context context,
			  const void *data,
			  size_t length,
			  EncASRepPart *t,
			  size_t *len)
{
    DECODE(EncASRepPart, &t->key);
}

krb5_error_code
krb5_encode_EncASRepPart (krb5_context context,
			  void *data,
			  size_t length,
			  EncASRepPart *t,
			  size_t *len)
{
    ENCODE(EncASRepPart, &t->key);
}

krb5_error_code
krb5_decode_EncTGSRepPart (krb5_context context,
			   const void *data,
			   size_t length,
			   EncTGSRepPart *t,
			   size_t *len)
{
    DECODE(EncTGSRepPart, &t->key);
}

krb5_error_code
krb5_encode_EncTGSRepPart (krb5_context context,
			   void *data,
			   size_t length,
			   EncTGSRepPart *t,
			   size_t *len)
{
    ENCODE(EncTGSRepPart, &t->key);
}

krb5_error_code
krb5_decode_EncAPRepPart (krb5_context context,
			  const void *data,
			  size_t length,
			  EncAPRepPart *t,
			  size_t *len)
{
    DECODE(EncAPRepPart, t->subkey);
}

krb5_error_code
krb5_encode_EncAPRepPart (krb5_context context,
			  void *data,
			  size_t length,
			  EncAPRepPart *t,
			  size_t *len)
{
    ENCODE(EncAPRepPart, t->subkey);
}

krb5_error_code
krb5_decode_Authenticator (krb5_context context,
			   const void *data,
			   size_t length,
			   Authenticator *t,
			   size_t *len)
{
    DECODE(Authenticator, t->subkey);
}

krb5_error_code
krb5_encode_Authenticator (krb5_context context,
			   void *data,
			   size_t length,
			   Authenticator *t,
			   size_t *len)
{
    ENCODE(Authenticator, t->subkey);
}

krb5_error_code
krb5_decode_EncKrbCredPart (krb5_context context,
			    const void *data,
			    size_t length,
			    EncKrbCredPart *t,
			    size_t *len)
{
#if 1
    return decode_EncKrbCredPart(data, length, t, len);
#else
    krb5_error_code ret;
    int i;
    ret = decode_EncKrbCredPart((void*)data, length, t, len);
    if(ret)
	return ret;
    for(i = 0; i < t->ticket_info.len; i++)
	if((ret = krb5_decode_keyblock(context, &t->ticket_info.val[i].key, 1)))
	    break;
    return ret;
#endif
}

krb5_error_code
krb5_encode_EncKrbCredPart (krb5_context context,
			    void *data,
			    size_t length,
			    EncKrbCredPart *t,
			    size_t *len)
{
#if 0
    krb5_error_code ret = 0;
    int i;

    for(i = 0; i < t->ticket_info.len; i++)
	if((ret = krb5_decode_keyblock(context, &t->ticket_info.val[i].key, 0)))
	    break;
    if(ret) return ret;
#endif
    return encode_EncKrbCredPart (data, length, t, len);
}

krb5_error_code
krb5_decode_ETYPE_INFO (krb5_context context,
			const void *data,
			size_t length,
			ETYPE_INFO *t,
			size_t *len)
{
#if 1
    return decode_ETYPE_INFO(data, length, t, len);
#else
    krb5_error_code ret;
    int i;

    ret = decode_ETYPE_INFO((void*)data, length, t, len);
    if(ret)
	return ret;
    for(i = 0; i < t->len; i++) {
	if((ret = krb5_decode_keytype(context, &t->val[i].etype, 1)))
	    break;
    }
    return ret;
#endif
}

krb5_error_code
krb5_encode_ETYPE_INFO (krb5_context context,
			void *data,
			size_t length,
			ETYPE_INFO *t,
			size_t *len)
{
#if 0
    krb5_error_code ret = 0;

    int i;
    /* XXX this will break, since we need one key-info for each enctype */
    /* XXX or do we? */
    for(i = 0; i < t->len; i++)
	if((ret = krb5_decode_keytype(context, &t->val[i].etype, 0)))
	    break;
    if(ret) return ret;
#endif
    return encode_ETYPE_INFO (data, length, t, len);
}
