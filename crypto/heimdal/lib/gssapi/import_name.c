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

#include "gssapi_locl.h"

RCSID("$Id: import_name.c,v 1.13 2003/03/16 17:33:31 lha Exp $");

static OM_uint32
parse_krb5_name (OM_uint32 *minor_status,
		 const char *name,
		 gss_name_t *output_name)
{
    krb5_error_code kerr;

    kerr = krb5_parse_name (gssapi_krb5_context, name, output_name);

    if (kerr == 0)
	return GSS_S_COMPLETE;
    else if (kerr == KRB5_PARSE_ILLCHAR || kerr == KRB5_PARSE_MALFORMED) {
	gssapi_krb5_set_error_string ();
	*minor_status = kerr;
	return GSS_S_BAD_NAME;
    } else {
	gssapi_krb5_set_error_string ();
	*minor_status = kerr;
	return GSS_S_FAILURE;
    }
}

static OM_uint32
import_krb5_name (OM_uint32 *minor_status,
		  const gss_buffer_t input_name_buffer,
		  gss_name_t *output_name)
{
    OM_uint32 ret;
    char *tmp;

    tmp = malloc (input_name_buffer->length + 1);
    if (tmp == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    memcpy (tmp,
	    input_name_buffer->value,
	    input_name_buffer->length);
    tmp[input_name_buffer->length] = '\0';

    ret = parse_krb5_name(minor_status, tmp, output_name);
    free(tmp);

    return ret;
}

static OM_uint32
import_hostbased_name (OM_uint32 *minor_status,
		       const gss_buffer_t input_name_buffer,
		       gss_name_t *output_name)
{
    krb5_error_code kerr;
    char *tmp;
    char *p;
    char *host;
    char local_hostname[MAXHOSTNAMELEN];

    *output_name = NULL;

    tmp = malloc (input_name_buffer->length + 1);
    if (tmp == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    memcpy (tmp,
	    input_name_buffer->value,
	    input_name_buffer->length);
    tmp[input_name_buffer->length] = '\0';

    p = strchr (tmp, '@');
    if (p != NULL) {
	*p = '\0';
	host = p + 1;
    } else {
	if (gethostname(local_hostname, sizeof(local_hostname)) < 0) {
	    *minor_status = errno;
	    free (tmp);
	    return GSS_S_FAILURE;
	}
	host = local_hostname;
    }

    kerr = krb5_sname_to_principal (gssapi_krb5_context,
				    host,
				    tmp,
				    KRB5_NT_SRV_HST,
				    output_name);
    free (tmp);
    *minor_status = kerr;
    if (kerr == 0)
	return GSS_S_COMPLETE;
    else if (kerr == KRB5_PARSE_ILLCHAR || kerr == KRB5_PARSE_MALFORMED) {
	gssapi_krb5_set_error_string ();
	*minor_status = kerr;
	return GSS_S_BAD_NAME;
    } else {
	gssapi_krb5_set_error_string ();
	*minor_status = kerr;
	return GSS_S_FAILURE;
    }
}

static OM_uint32
import_export_name (OM_uint32 *minor_status,
		    const gss_buffer_t input_name_buffer,
		    gss_name_t *output_name)
{
    unsigned char *p;
    uint32_t length;
    OM_uint32 ret;
    char *name;

    if (input_name_buffer->length < 10 + GSS_KRB5_MECHANISM->length)
	return GSS_S_BAD_NAME;

    /* TOK, MECH_OID_LEN, DER(MECH_OID), NAME_LEN, NAME */

    p = input_name_buffer->value;

    if (memcmp(&p[0], "\x04\x01\x00", 3) != 0 ||
	p[3] != GSS_KRB5_MECHANISM->length + 2 ||
	p[4] != 0x06 ||
	p[5] != GSS_KRB5_MECHANISM->length ||
	memcmp(&p[6], GSS_KRB5_MECHANISM->elements, 
	       GSS_KRB5_MECHANISM->length) != 0)
	return GSS_S_BAD_NAME;

    p += 6 + GSS_KRB5_MECHANISM->length;

    length = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
    p += 4;

    if (length > input_name_buffer->length - 10 - GSS_KRB5_MECHANISM->length)
	return GSS_S_BAD_NAME;

    name = malloc(length + 1);
    if (name == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    memcpy(name, p, length);
    name[length] = '\0';

    ret = parse_krb5_name(minor_status, name, output_name);
    free(name);

    return ret;
}

int
gss_oid_equal(const gss_OID a, const gss_OID b)
{
	if (a == b)
		return 1;
	else if (a == GSS_C_NO_OID || b == GSS_C_NO_OID || a->length != b->length)
		return 0;
	else
		return memcmp(a->elements, b->elements, a->length) == 0;
}

OM_uint32 gss_import_name
           (OM_uint32 * minor_status,
            const gss_buffer_t input_name_buffer,
            const gss_OID input_name_type,
            gss_name_t * output_name
           )
{
    GSSAPI_KRB5_INIT ();

    *minor_status = 0;
    *output_name = GSS_C_NO_NAME;
    
    if (gss_oid_equal(input_name_type, GSS_C_NT_HOSTBASED_SERVICE))
	return import_hostbased_name (minor_status,
				      input_name_buffer,
				      output_name);
    else if (gss_oid_equal(input_name_type, GSS_C_NO_OID)
	     || gss_oid_equal(input_name_type, GSS_C_NT_USER_NAME)
	     || gss_oid_equal(input_name_type, GSS_KRB5_NT_PRINCIPAL_NAME))
 	/* default printable syntax */
	return import_krb5_name (minor_status,
				 input_name_buffer,
				 output_name);
    else if (gss_oid_equal(input_name_type, GSS_C_NT_EXPORT_NAME)) {
	return import_export_name(minor_status,
				  input_name_buffer, 
				  output_name);
    } else {
	*minor_status = 0;
	return GSS_S_BAD_NAMETYPE;
    }
}
