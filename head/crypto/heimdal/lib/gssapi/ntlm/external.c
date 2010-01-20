/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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

#include "ntlm/ntlm.h"

RCSID("$Id: external.c 19359 2006-12-15 20:01:48Z lha $");

static gssapi_mech_interface_desc ntlm_mech = {
    GMI_VERSION,
    "ntlm",
    {10, rk_UNCONST("\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a") },
    _gss_ntlm_acquire_cred,
    _gss_ntlm_release_cred,
    _gss_ntlm_init_sec_context,
    _gss_ntlm_accept_sec_context,
    _gss_ntlm_process_context_token,
    _gss_ntlm_delete_sec_context,
    _gss_ntlm_context_time,
    _gss_ntlm_get_mic,
    _gss_ntlm_verify_mic,
    _gss_ntlm_wrap,
    _gss_ntlm_unwrap,
    _gss_ntlm_display_status,
    NULL,
    _gss_ntlm_compare_name,
    _gss_ntlm_display_name,
    _gss_ntlm_import_name,
    _gss_ntlm_export_name,
    _gss_ntlm_release_name,
    _gss_ntlm_inquire_cred,
    _gss_ntlm_inquire_context,
    _gss_ntlm_wrap_size_limit,
    _gss_ntlm_add_cred,
    _gss_ntlm_inquire_cred_by_mech,
    _gss_ntlm_export_sec_context,
    _gss_ntlm_import_sec_context,
    _gss_ntlm_inquire_names_for_mech,
    _gss_ntlm_inquire_mechs_for_name,
    _gss_ntlm_canonicalize_name,
    _gss_ntlm_duplicate_name
};

gssapi_mech_interface
__gss_ntlm_initialize(void)
{
	return &ntlm_mech;
}

static gss_OID_desc _gss_ntlm_mechanism_desc = 
{10, rk_UNCONST("\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a") };

gss_OID GSS_NTLM_MECHANISM = &_gss_ntlm_mechanism_desc;
