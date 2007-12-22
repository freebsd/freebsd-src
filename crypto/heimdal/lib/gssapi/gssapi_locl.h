/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska Högskolan
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

/* $Id: gssapi_locl.h,v 1.24.2.5 2003/09/18 22:01:52 lha Exp $ */
/* $FreeBSD$ */

#ifndef GSSAPI_LOCL_H
#define GSSAPI_LOCL_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <krb5_locl.h>
#include "gssapi.h"
#include <assert.h>

#include "arcfour.h"

extern krb5_context gssapi_krb5_context;

extern krb5_keytab gssapi_krb5_keytab;

krb5_error_code gssapi_krb5_init (void);

#define GSSAPI_KRB5_INIT() do {					\
    krb5_error_code kret;					\
    if((kret = gssapi_krb5_init ()) != 0) {	\
	*minor_status = kret;					\
	return GSS_S_FAILURE;					\
    }								\
} while (0)

OM_uint32
gssapi_krb5_create_8003_checksum (
		      OM_uint32 *minor_status,
		      const gss_channel_bindings_t input_chan_bindings,
		      OM_uint32 flags,
                      const krb5_data *fwd_data,
		      Checksum *result);

OM_uint32
gssapi_krb5_verify_8003_checksum (
		      OM_uint32 *minor_status,
		      const gss_channel_bindings_t input_chan_bindings,
		      const Checksum *cksum,
		      OM_uint32 *flags,
                      krb5_data *fwd_data);

OM_uint32
gssapi_krb5_encapsulate(
			OM_uint32 *minor_status,
			const krb5_data *in_data,
			gss_buffer_t output_token,
			u_char *type);

u_char *
_gssapi_make_mech_header(u_char *p,
			 size_t len);

OM_uint32
gssapi_krb5_decapsulate(
			OM_uint32 *minor_status,
			gss_buffer_t input_token_buffer,
			krb5_data *out_data,
			char *type);

void
gssapi_krb5_encap_length (size_t data_len,
			  size_t *len,
			  size_t *total_len);

u_char *
gssapi_krb5_make_header (u_char *p,
			 size_t len,
			 u_char *type);

OM_uint32
gssapi_krb5_verify_header(u_char **str,
			  size_t total_len,
			  char *type);


OM_uint32
_gssapi_verify_mech_header(u_char **str,
			   size_t total_len);

OM_uint32
_gssapi_verify_pad(gss_buffer_t, size_t, size_t *);

OM_uint32
gss_verify_mic_internal(OM_uint32 * minor_status,
			const gss_ctx_id_t context_handle,
			const gss_buffer_t message_buffer,
			const gss_buffer_t token_buffer,
			gss_qop_t * qop_state,
			char * type);

OM_uint32
gss_krb5_get_remotekey(const gss_ctx_id_t context_handle,
		       krb5_keyblock **key);

OM_uint32
gss_krb5_get_localkey(const gss_ctx_id_t context_handle,
		      krb5_keyblock **key);

krb5_error_code
gss_address_to_krb5addr(OM_uint32 gss_addr_type,
                        gss_buffer_desc *gss_addr,
                        int16_t port,
                        krb5_address *address);

/* sec_context flags */

#define SC_LOCAL_ADDRESS  0x01
#define SC_REMOTE_ADDRESS 0x02
#define SC_KEYBLOCK	  0x04
#define SC_LOCAL_SUBKEY	  0x08
#define SC_REMOTE_SUBKEY  0x10

int
gss_oid_equal(const gss_OID a, const gss_OID b);

void
gssapi_krb5_set_error_string (void);

char *
gssapi_krb5_get_error_string (void);

OM_uint32
_gss_DES3_get_mic_compat(OM_uint32 *minor_status, gss_ctx_id_t ctx);

OM_uint32
gssapi_lifetime_left(OM_uint32 *, OM_uint32, OM_uint32 *);

/* 8003 */

krb5_error_code
gssapi_encode_om_uint32(OM_uint32, u_char *);

krb5_error_code
gssapi_encode_be_om_uint32(OM_uint32, u_char *);

krb5_error_code
gssapi_decode_om_uint32(u_char *, OM_uint32 *);

krb5_error_code
gssapi_decode_be_om_uint32(u_char *, OM_uint32 *);

#endif
