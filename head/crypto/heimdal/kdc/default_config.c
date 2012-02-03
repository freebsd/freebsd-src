/*
 * Copyright (c) 1997-2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
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

#include "kdc_locl.h"
#include <getarg.h>
#include <parse_bytes.h>

RCSID("$Id: default_config.c 21405 2007-07-04 10:35:45Z lha $");

krb5_error_code
krb5_kdc_get_config(krb5_context context, krb5_kdc_configuration **config)
{
    krb5_kdc_configuration *c;

    c = calloc(1, sizeof(*c));
    if (c == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }

    c->require_preauth = TRUE;
    c->kdc_warn_pwexpire = 0;
    c->encode_as_rep_as_tgs_rep = FALSE;
    c->check_ticket_addresses = TRUE;
    c->allow_null_ticket_addresses = TRUE;
    c->allow_anonymous = FALSE;
    c->trpolicy = TRPOLICY_ALWAYS_CHECK;
    c->enable_v4 = FALSE;
    c->enable_kaserver = FALSE;
    c->enable_524 = FALSE;
    c->enable_v4_cross_realm = FALSE;
    c->enable_pkinit = FALSE;
    c->pkinit_princ_in_cert = TRUE;
    c->pkinit_require_binding = TRUE;
    c->db = NULL;
    c->num_db = 0;
    c->logf = NULL;

    c->require_preauth =
	krb5_config_get_bool_default(context, NULL, 
				     c->require_preauth,
				     "kdc", "require-preauth", NULL);
    c->enable_v4 = 
	krb5_config_get_bool_default(context, NULL, 
				     c->enable_v4, 
				     "kdc", "enable-kerberos4", NULL);
    c->enable_v4_cross_realm =
	krb5_config_get_bool_default(context, NULL,
				     c->enable_v4_cross_realm, 
				     "kdc",
				     "enable-kerberos4-cross-realm", NULL);
    c->enable_524 =
	krb5_config_get_bool_default(context, NULL, 
				     c->enable_v4, 
				     "kdc", "enable-524", NULL);
    c->enable_digest = 
	krb5_config_get_bool_default(context, NULL, 
				     FALSE,
				     "kdc", "enable-digest", NULL);

    {
	const char *digests;

	digests = krb5_config_get_string(context, NULL, 
					 "kdc", 
					 "digests_allowed", NULL);
	if (digests == NULL)
	    digests = "ntlm-v2";
	c->digests_allowed = parse_flags(digests,_kdc_digestunits, 0);
	if (c->digests_allowed == -1) {
	    kdc_log(context, c, 0,
		    "unparsable digest units (%s), turning off digest",
		    digests);
	    c->enable_digest = 0;
	} else if (c->digests_allowed == 0) {
	    kdc_log(context, c, 0,
		    "no digest enable, turning digest off",
		    digests);
	    c->enable_digest = 0;
	}
    }

    c->enable_kx509 = 
	krb5_config_get_bool_default(context, NULL, 
				     FALSE, 
				     "kdc", "enable-kx509", NULL);

    if (c->enable_kx509) {
	c->kx509_template =
	    krb5_config_get_string(context, NULL, 
				   "kdc", "kx509_template", NULL);
	c->kx509_ca =
	    krb5_config_get_string(context, NULL, 
				   "kdc", "kx509_ca", NULL);
	if (c->kx509_ca == NULL || c->kx509_template == NULL) {
	    kdc_log(context, c, 0,
		    "missing kx509 configuration, turning off");
	    c->enable_kx509 = FALSE;
	}
    }

    c->check_ticket_addresses = 
	krb5_config_get_bool_default(context, NULL, 
				     c->check_ticket_addresses, 
				     "kdc", 
				     "check-ticket-addresses", NULL);
    c->allow_null_ticket_addresses = 
	krb5_config_get_bool_default(context, NULL, 
				     c->allow_null_ticket_addresses, 
				     "kdc", 
				     "allow-null-ticket-addresses", NULL);

    c->allow_anonymous = 
	krb5_config_get_bool_default(context, NULL, 
				     c->allow_anonymous,
				     "kdc", 
				     "allow-anonymous", NULL);

    c->max_datagram_reply_length =
	krb5_config_get_int_default(context, 
				    NULL, 
				    1400,
				    "kdc",
				    "max-kdc-datagram-reply-length",
				    NULL);

    {
	const char *trpolicy_str;

	trpolicy_str = 
	    krb5_config_get_string_default(context, NULL, "DEFAULT", "kdc", 
					   "transited-policy", NULL);
	if(strcasecmp(trpolicy_str, "always-check") == 0) {
	    c->trpolicy = TRPOLICY_ALWAYS_CHECK;
	} else if(strcasecmp(trpolicy_str, "allow-per-principal") == 0) {
	    c->trpolicy = TRPOLICY_ALLOW_PER_PRINCIPAL;
	} else if(strcasecmp(trpolicy_str, "always-honour-request") == 0) {
	    c->trpolicy = TRPOLICY_ALWAYS_HONOUR_REQUEST;
	} else if(strcasecmp(trpolicy_str, "DEFAULT") == 0) { 
	    /* default */
	} else {
	    kdc_log(context, c, 0,
		    "unknown transited-policy: %s, "
		    "reverting to default (always-check)", 
		    trpolicy_str);
	}
    }

    {
	const char *p;
	p = krb5_config_get_string (context, NULL, 
				    "kdc",
				    "v4-realm",
				    NULL);
	if(p != NULL) {
	    c->v4_realm = strdup(p);
	    if (c->v4_realm == NULL)
		krb5_errx(context, 1, "out of memory");
	} else {
	    c->v4_realm = NULL;
	}
    }

    c->enable_kaserver = 
	krb5_config_get_bool_default(context, 
				     NULL, 
				     c->enable_kaserver,
				     "kdc", "enable-kaserver", NULL);


    c->encode_as_rep_as_tgs_rep =
	krb5_config_get_bool_default(context, NULL, 
				     c->encode_as_rep_as_tgs_rep, 
				     "kdc", 
				     "encode_as_rep_as_tgs_rep", NULL);
    
    c->kdc_warn_pwexpire =
	krb5_config_get_time_default (context, NULL,
				      c->kdc_warn_pwexpire,
				      "kdc", "kdc_warn_pwexpire", NULL);


#ifdef PKINIT
    c->enable_pkinit = 
	krb5_config_get_bool_default(context, 
				     NULL, 
				     c->enable_pkinit,
				     "kdc",
				     "enable-pkinit",
				     NULL);
    if (c->enable_pkinit) {
	const char *user_id, *anchors, *ocsp_file;
	char **pool_list, **revoke_list;

	user_id = 
	    krb5_config_get_string(context, NULL,
				   "kdc", "pkinit_identity", NULL);
	if (user_id == NULL)
	    krb5_errx(context, 1, "pkinit enabled but no identity");

	anchors = krb5_config_get_string(context, NULL,
					 "kdc", "pkinit_anchors", NULL);
	if (anchors == NULL)
	    krb5_errx(context, 1, "pkinit enabled but no X509 anchors");

	pool_list =
	    krb5_config_get_strings(context, NULL,
				    "kdc", "pkinit_pool", NULL);

	revoke_list =
	    krb5_config_get_strings(context, NULL,
				    "kdc", "pkinit_revoke", NULL);

	ocsp_file = 
	    krb5_config_get_string(context, NULL,
				   "kdc", "pkinit_kdc_ocsp", NULL);
	if (ocsp_file) {
	    c->pkinit_kdc_ocsp_file = strdup(ocsp_file);
	    if (c->pkinit_kdc_ocsp_file == NULL)
		krb5_errx(context, 1, "out of memory");
	}

	_kdc_pk_initialize(context, c, user_id, anchors, 
			   pool_list, revoke_list);

	krb5_config_free_strings(pool_list);
	krb5_config_free_strings(revoke_list);

	c->pkinit_princ_in_cert = 
	    krb5_config_get_bool_default(context, NULL,
					 c->pkinit_princ_in_cert,
					 "kdc",
					 "pkinit_principal_in_certificate",
					 NULL);

	c->pkinit_require_binding = 
	    krb5_config_get_bool_default(context, NULL,
					 c->pkinit_require_binding,
					 "kdc",
					 "pkinit_win2k_require_binding",
					 NULL);
    }

    c->pkinit_dh_min_bits =
	krb5_config_get_int_default(context, NULL, 
				    0,
				    "kdc", "pkinit_dh_min_bits", NULL);

#endif

    *config = c;

    return 0;
}
