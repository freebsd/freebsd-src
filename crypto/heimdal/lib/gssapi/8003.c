/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
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

RCSID("$Id: 8003.c,v 1.12 2002/10/31 14:38:49 joda Exp $");

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

  MD5_Init(&md5);
  encode_om_uint32 (b->initiator_addrtype, num);
  MD5_Update (&md5, num, sizeof(num));
  encode_om_uint32 (b->initiator_address.length, num);
  MD5_Update (&md5, num, sizeof(num));
  if (b->initiator_address.length)
    MD5_Update (&md5,
		b->initiator_address.value,
		b->initiator_address.length);
  encode_om_uint32 (b->acceptor_addrtype, num);
  MD5_Update (&md5, num, sizeof(num));
  encode_om_uint32 (b->acceptor_address.length, num);
  MD5_Update (&md5, num, sizeof(num));
  if (b->acceptor_address.length)
    MD5_Update (&md5,
		b->acceptor_address.value,
		b->acceptor_address.length);
  encode_om_uint32 (b->application_data.length, num);
  MD5_Update (&md5, num, sizeof(num));
  if (b->application_data.length)
    MD5_Update (&md5,
		b->application_data.value,
		b->application_data.length);
  MD5_Final (p, &md5);
  return 0;
}

/*
 * create a checksum over the chanel bindings in
 * `input_chan_bindings', `flags' and `fwd_data' and return it in
 * `result'
 */

OM_uint32
gssapi_krb5_create_8003_checksum (
		      OM_uint32 *minor_status,    
		      const gss_channel_bindings_t input_chan_bindings,
		      OM_uint32 flags,
		      const krb5_data *fwd_data,
		      Checksum *result)
{
    u_char *p;

    /* 
     * see rfc1964 (section 1.1.1 (Initial Token), and the checksum value 
     * field's format) */
    result->cksumtype = 0x8003;
    if (fwd_data->length > 0 && (flags & GSS_C_DELEG_FLAG))
	result->checksum.length = 24 + 4 + fwd_data->length;
    else 
	result->checksum.length = 24;
    result->checksum.data   = malloc (result->checksum.length);
    if (result->checksum.data == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
  
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

    if (fwd_data->length > 0 && (flags & GSS_C_DELEG_FLAG)) {
#if 0
	u_char *tmp;

	result->checksum.length = 28 + fwd_data->length;
	tmp = realloc(result->checksum.data, result->checksum.length);
	if (tmp == NULL)
	    return ENOMEM;
	result->checksum.data = tmp;

	p = (u_char*)result->checksum.data + 24;  
#endif
	*p++ = (1 >> 0) & 0xFF;                   /* DlgOpt */ /* == 1 */
	*p++ = (1 >> 8) & 0xFF;                   /* DlgOpt */ /* == 0 */
	*p++ = (fwd_data->length >> 0) & 0xFF;    /* Dlgth  */
	*p++ = (fwd_data->length >> 8) & 0xFF;    /* Dlgth  */
	memcpy(p, (unsigned char *) fwd_data->data, fwd_data->length);

	p += fwd_data->length;
    }
     
    return GSS_S_COMPLETE;
}

/*
 * verify the checksum in `cksum' over `input_chan_bindings'
 * returning  `flags' and `fwd_data'
 */

OM_uint32
gssapi_krb5_verify_8003_checksum(
		      OM_uint32 *minor_status,    
		      const gss_channel_bindings_t input_chan_bindings,
		      const Checksum *cksum,
		      OM_uint32 *flags,
		      krb5_data *fwd_data)
{
    unsigned char hash[16];
    unsigned char *p;
    OM_uint32 length;
    int DlgOpt;
    static unsigned char zeros[16];

    /* XXX should handle checksums > 24 bytes */
    if(cksum->cksumtype != 0x8003 || cksum->checksum.length < 24) {
	*minor_status = 0;
	return GSS_S_BAD_BINDINGS;
    }
    
    p = cksum->checksum.data;
    decode_om_uint32(p, &length);
    if(length != sizeof(hash)) {
	*minor_status = 0;
	return GSS_S_BAD_BINDINGS;
    }
    
    p += 4;
    
    if (input_chan_bindings != GSS_C_NO_CHANNEL_BINDINGS
	&& memcmp(p, zeros, sizeof(zeros)) != 0) {
	if(hash_input_chan_bindings(input_chan_bindings, hash) != 0) {
	    *minor_status = 0;
	    return GSS_S_BAD_BINDINGS;
	}
	if(memcmp(hash, p, sizeof(hash)) != 0) {
	    *minor_status = 0;
	    return GSS_S_BAD_BINDINGS;
	}
    }
    
    p += sizeof(hash);
    
    decode_om_uint32(p, flags);
    p += 4;

    if (cksum->checksum.length > 24 && (*flags & GSS_C_DELEG_FLAG)) {
	if(cksum->checksum.length < 28) {
	    *minor_status = 0;
	    return GSS_S_BAD_BINDINGS;
	}
    
	DlgOpt = (p[0] << 0) | (p[1] << 8);
	p += 2;
	if (DlgOpt != 1) {
	    *minor_status = 0;
	    return GSS_S_BAD_BINDINGS;
	}

	fwd_data->length = (p[0] << 0) | (p[1] << 8);
	p += 2;
	if(cksum->checksum.length < 28 + fwd_data->length) {
	    *minor_status = 0;
	    return GSS_S_BAD_BINDINGS;
	}
	fwd_data->data = malloc(fwd_data->length);
	if (fwd_data->data == NULL) {
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
	memcpy(fwd_data->data, p, fwd_data->length);
    }
    
    return GSS_S_COMPLETE;
}
