/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

#include "gssapi_locl.h"

RCSID("$Id: 8003.c,v 1.6 2000/01/25 23:10:13 assar Exp $");

static krb5_error_code
encode_om_uint32(OM_uint32 n, u_char *p)
{
  p[0] = (n >> 0)  & 0xFF;
  p[1] = (n >> 8)  & 0xFF;
  p[2] = (n >> 16) & 0xFF;
  p[3] = (n >> 24) & 0xFF;
  return 0;
}

static krb5_error_code
decode_om_uint32(u_char *p, OM_uint32 *n)
{
    *n = (p[0] << 0) | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
    return 0;
}

static krb5_error_code
hash_input_chan_bindings (const gss_channel_bindings_t b,
			  u_char *p)
{
  u_char num[4];
  MD5_CTX md5;

  MD5Init(&md5);
  encode_om_uint32 (b->initiator_addrtype, num);
  MD5Update (&md5, num, sizeof(num));
  encode_om_uint32 (b->initiator_address.length, num);
  MD5Update (&md5, num, sizeof(num));
  if (b->initiator_address.length)
    MD5Update (&md5,
		b->initiator_address.value,
		b->initiator_address.length);
  encode_om_uint32 (b->acceptor_addrtype, num);
  MD5Update (&md5, num, sizeof(num));
  encode_om_uint32 (b->acceptor_address.length, num);
  MD5Update (&md5, num, sizeof(num));
  if (b->acceptor_address.length)
    MD5Update (&md5,
		b->acceptor_address.value,
		b->acceptor_address.length);
  encode_om_uint32 (b->application_data.length, num);
  MD5Update (&md5, num, sizeof(num));
  if (b->application_data.length)
    MD5Update (&md5,
		b->application_data.value,
		b->application_data.length);
  MD5Final (p, &md5);
  return 0;
}

krb5_error_code
gssapi_krb5_create_8003_checksum (
		      const gss_channel_bindings_t input_chan_bindings,
		      OM_uint32 flags,
		      Checksum *result)
{
  u_char *p;

  result->cksumtype = 0x8003;
  result->checksum.length = 24;
  result->checksum.data   = malloc (result->checksum.length);
  if (result->checksum.data == NULL)
    return ENOMEM;
  
  p = result->checksum.data;
  encode_om_uint32 (16, p);
  p += 4;
  if (input_chan_bindings == GSS_C_NO_CHANNEL_BINDINGS) {
    memset (p, 0, 16);
  } else {
    hash_input_chan_bindings (input_chan_bindings, p);
  }
  p += 16;
  encode_om_uint32 (flags, p);
  p += 4;
  if (p - (u_char *)result->checksum.data != result->checksum.length)
    abort ();
  return 0;
}

krb5_error_code
gssapi_krb5_verify_8003_checksum(
		      const gss_channel_bindings_t input_chan_bindings,
		      Checksum *cksum,
		      OM_uint32 *flags)
{
    unsigned char hash[16];
    unsigned char *p;
    OM_uint32 length;

    /* XXX should handle checksums > 24 bytes */
    if(cksum->cksumtype != 0x8003 || cksum->checksum.length != 24)
	return GSS_S_BAD_BINDINGS;
    
    p = cksum->checksum.data;
    decode_om_uint32(p, &length);
    if(length != sizeof(hash))
	return GSS_S_FAILURE;
    
    p += 4;
    
    if (input_chan_bindings != GSS_C_NO_CHANNEL_BINDINGS) {
	if(hash_input_chan_bindings(input_chan_bindings, hash) != 0)
	    return GSS_S_FAILURE;
	if(memcmp(hash, p, sizeof(hash)) != 0)
	    return GSS_S_FAILURE;
    }
    
    p += sizeof(hash);
    
    decode_om_uint32(p, flags);
    
    return 0;
}
