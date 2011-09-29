/*
 * Copyright (c) 1997-2003 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 *
 * Copyright (c) 2005 Andrew Bartlett <abartlet@samba.org>
 * 
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

/* 
 * $Id: kdc.h 21287 2007-06-25 14:09:03Z lha $ 
 */

#ifndef __KDC_H__
#define __KDC_H__

#include <krb5.h>

enum krb5_kdc_trpolicy {
    TRPOLICY_ALWAYS_CHECK,
    TRPOLICY_ALLOW_PER_PRINCIPAL, 
    TRPOLICY_ALWAYS_HONOUR_REQUEST
};

typedef struct krb5_kdc_configuration {
    krb5_boolean require_preauth; /* require preauth for all principals */
    time_t kdc_warn_pwexpire; /* time before expiration to print a warning */

    struct HDB **db;
    int num_db;

    krb5_boolean encode_as_rep_as_tgs_rep; /* bug compatibility */
	
    krb5_boolean check_ticket_addresses;
    krb5_boolean allow_null_ticket_addresses;
    krb5_boolean allow_anonymous;
    enum krb5_kdc_trpolicy trpolicy;

    char *v4_realm;
    krb5_boolean enable_v4;
    krb5_boolean enable_v4_cross_realm;
    krb5_boolean enable_v4_per_principal;

    krb5_boolean enable_kaserver;

    krb5_boolean enable_524;

    krb5_boolean enable_pkinit;
    krb5_boolean pkinit_princ_in_cert;
    char *pkinit_kdc_ocsp_file;
    int pkinit_dh_min_bits;
    int pkinit_require_binding;

    krb5_log_facility *logf;

    int enable_digest;
    int digests_allowed;

    size_t max_datagram_reply_length;

    int enable_kx509;
    const char *kx509_template;
    const char *kx509_ca;

} krb5_kdc_configuration;

#include <kdc-protos.h>

#endif
