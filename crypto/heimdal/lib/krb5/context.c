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

#include "krb5_locl.h"
#include <com_err.h>

RCSID("$Id: context.c,v 1.80 2002/08/28 15:27:24 joda Exp $");

#define INIT_FIELD(C, T, E, D, F)					\
    (C)->E = krb5_config_get_ ## T ## _default ((C), NULL, (D), 	\
						"libdefaults", F, NULL)

/*
 * Set the list of etypes `ret_etypes' from the configuration variable
 * `name'
 */

static krb5_error_code
set_etypes (krb5_context context,
	    const char *name,
	    krb5_enctype **ret_enctypes)
{
    char **etypes_str;
    krb5_enctype *etypes = NULL;

    etypes_str = krb5_config_get_strings(context, NULL, "libdefaults", 
					 name, NULL);
    if(etypes_str){
	int i, j, k;
	for(i = 0; etypes_str[i]; i++);
	etypes = malloc((i+1) * sizeof(*etypes));
	if (etypes == NULL) {
	    krb5_config_free_strings (etypes_str);
	    krb5_set_error_string (context, "malloc: out of memory");
	    return ENOMEM;
	}
	for(j = 0, k = 0; j < i; j++) {
	    if(krb5_string_to_enctype(context, etypes_str[j], &etypes[k]) == 0)
		k++;
	}
	etypes[k] = ETYPE_NULL;
	krb5_config_free_strings(etypes_str);
    } 
    *ret_enctypes = etypes;
    return 0;
}

/*
 * read variables from the configuration file and set in `context'
 */

static krb5_error_code
init_context_from_config_file(krb5_context context)
{
    krb5_error_code ret;
    const char * tmp;
    krb5_enctype *tmptypes;

    INIT_FIELD(context, time, max_skew, 5 * 60, "clockskew");
    INIT_FIELD(context, time, kdc_timeout, 3, "kdc_timeout");
    INIT_FIELD(context, int, max_retries, 3, "max_retries");

    INIT_FIELD(context, string, http_proxy, NULL, "http_proxy");
    
    ret = set_etypes (context, "default_etypes", &tmptypes);
    if(ret)
	return ret;
    free(context->etypes);
    context->etypes = tmptypes;
    
    ret = set_etypes (context, "default_etypes_des", &tmptypes);
    if(ret)
	return ret;
    free(context->etypes_des);
    context->etypes_des = tmptypes;

    /* default keytab name */
    tmp = NULL;
    if(!issuid())
	tmp = getenv("KRB5_KTNAME");
    if(tmp != NULL)
	context->default_keytab = tmp;
    else
	INIT_FIELD(context, string, default_keytab, 
		   KEYTAB_DEFAULT, "default_keytab_name");

    INIT_FIELD(context, string, default_keytab_modify, 
	       NULL, "default_keytab_modify_name");

    INIT_FIELD(context, string, time_fmt, 
	       "%Y-%m-%dT%H:%M:%S", "time_format");

    INIT_FIELD(context, string, date_fmt, 
	       "%Y-%m-%d", "date_format");

    INIT_FIELD(context, bool, log_utc, 
	       FALSE, "log_utc");


    
    /* init dns-proxy slime */
    tmp = krb5_config_get_string(context, NULL, "libdefaults", 
				 "dns_proxy", NULL);
    if(tmp) 
	roken_gethostby_setup(context->http_proxy, tmp);
    krb5_free_host_realm (context, context->default_realms);
    context->default_realms = NULL;

    {
	krb5_addresses addresses;
	char **adr, **a;

	krb5_set_extra_addresses(context, NULL);
	adr = krb5_config_get_strings(context, NULL, 
				      "libdefaults", 
				      "extra_addresses", 
				      NULL);
	memset(&addresses, 0, sizeof(addresses));
	for(a = adr; a && *a; a++) {
	    ret = krb5_parse_address(context, *a, &addresses);
	    if (ret == 0) {
		krb5_add_extra_addresses(context, &addresses);
		krb5_free_addresses(context, &addresses);
	    }
	}
	krb5_config_free_strings(adr);

	krb5_set_ignore_addresses(context, NULL);
	adr = krb5_config_get_strings(context, NULL, 
				      "libdefaults", 
				      "ignore_addresses", 
				      NULL);
	memset(&addresses, 0, sizeof(addresses));
	for(a = adr; a && *a; a++) {
	    ret = krb5_parse_address(context, *a, &addresses);
	    if (ret == 0) {
		krb5_add_ignore_addresses(context, &addresses);
		krb5_free_addresses(context, &addresses);
	    }
	}
	krb5_config_free_strings(adr);
    }
    
    INIT_FIELD(context, bool, scan_interfaces, TRUE, "scan_interfaces");
    INIT_FIELD(context, int, fcache_vno, 0, "fcache_version");
    INIT_FIELD(context, bool, srv_lookup, TRUE, "dns_lookup_kdc");
    /* srv_lookup backwards compatibility. */
    { 
    const char **p;
    p = krb5_config_get_strings(context, NULL, "libdefaults", "srv_lookup", NULL);
    if (p != NULL)
	INIT_FIELD(context, bool, srv_lookup, TRUE, "srv_lookup");
    }
    return 0;
}

krb5_error_code
krb5_init_context(krb5_context *context)
{
    krb5_context p;
    krb5_error_code ret;
    char **files;

    p = calloc(1, sizeof(*p));
    if(!p)
	return ENOMEM;

    ret = krb5_get_default_config_files(&files);
    if(ret) 
	goto out;
    ret = krb5_set_config_files(p, files);
    krb5_free_config_files(files);
    if(ret) 
	goto out;

    /* init error tables */
    krb5_init_ets(p);

    p->cc_ops = NULL;
    p->num_cc_ops = 0;
    krb5_cc_register(p, &krb5_fcc_ops, TRUE);
    krb5_cc_register(p, &krb5_mcc_ops, TRUE);

    p->num_kt_types = 0;
    p->kt_types     = NULL;
    krb5_kt_register (p, &krb5_fkt_ops);
    krb5_kt_register (p, &krb5_mkt_ops);
    krb5_kt_register (p, &krb5_akf_ops);
    krb5_kt_register (p, &krb4_fkt_ops);
    krb5_kt_register (p, &krb5_srvtab_fkt_ops);
    krb5_kt_register (p, &krb5_any_ops);

out:
    if(ret) {
	krb5_free_context(p);
	p = NULL;
    }
    *context = p;
    return ret;
}

void
krb5_free_context(krb5_context context)
{
    free(context->etypes);
    free(context->etypes_des);
    krb5_free_host_realm (context, context->default_realms);
    krb5_config_file_free (context, context->cf);
    free_error_table (context->et_list);
    free(context->cc_ops);
    free(context->kt_types);
    krb5_clear_error_string(context);
    if(context->warn_dest != NULL)
	krb5_closelog(context, context->warn_dest);
    krb5_set_extra_addresses(context, NULL);
    krb5_set_ignore_addresses(context, NULL);
    free(context);
}

krb5_error_code
krb5_set_config_files(krb5_context context, char **filenames)
{
    krb5_error_code ret;
    krb5_config_binding *tmp = NULL;
    while(filenames != NULL && *filenames != NULL && **filenames != '\0') {
	ret = krb5_config_parse_file_multi(context, *filenames, &tmp);
	if(ret != 0 && ret != ENOENT) {
	    krb5_config_file_free(context, tmp);
	    return ret;
	}
	filenames++;
    }
#if 0
    /* with this enabled and if there are no config files, Kerberos is
       considererd disabled */
    if(tmp == NULL)
	return ENOENT;
#endif
    krb5_config_file_free(context, context->cf);
    context->cf = tmp;
    ret = init_context_from_config_file(context);
    return ret;
}

krb5_error_code 
krb5_get_default_config_files(char ***pfilenames)
{
    const char *p, *q;
    char **pp;
    int n, i;

    const char *files = NULL;
    if (pfilenames == NULL)
        return EINVAL;
    if(!issuid())
	files = getenv("KRB5_CONFIG");
    if (files == NULL)
	files = krb5_config_file;

    for(n = 0, p = files; strsep_copy(&p, ":", NULL, 0) != -1; n++);
    pp = malloc((n + 1) * sizeof(*pp));
    if(pp == NULL)
	return ENOMEM;

    n = 0;
    p = files;
    while(1) {
	ssize_t l;
	q = p;
	l = strsep_copy(&q, ":", NULL, 0);
	if(l == -1)
	    break;
	pp[n] = malloc(l + 1);
	if(pp[n] == NULL) {
	    krb5_free_config_files(pp);
	    return ENOMEM;
	}
	l = strsep_copy(&p, ":", pp[n], l + 1);
	for(i = 0; i < n; i++)
	    if(strcmp(pp[i], pp[n]) == 0) {
		free(pp[n]);
		goto skip;
	    }
	n++;
    skip:;
    }
    pp[n] = NULL;
    *pfilenames = pp;
    return 0;
}

void
krb5_free_config_files(char **filenames)
{
    char **p;
    for(p = filenames; *p != NULL; p++)
	free(*p);
    free(filenames);
}

/*
 * set `etype' to a malloced list of the default enctypes
 */

static krb5_error_code
default_etypes(krb5_context context, krb5_enctype **etype)
{
    krb5_enctype p[] = {
	ETYPE_DES3_CBC_SHA1,
	ETYPE_DES3_CBC_MD5,
	ETYPE_ARCFOUR_HMAC_MD5,
	ETYPE_DES_CBC_MD5,
	ETYPE_DES_CBC_MD4,
	ETYPE_DES_CBC_CRC,
	ETYPE_NULL
    };

    *etype = malloc(sizeof(p));
    if(*etype == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    memcpy(*etype, p, sizeof(p));
    return 0;
}

krb5_error_code
krb5_set_default_in_tkt_etypes(krb5_context context, 
			       const krb5_enctype *etypes)
{
    int i;
    krb5_enctype *p = NULL;

    if(etypes) {
	for (i = 0; etypes[i]; ++i)
	    if(!krb5_enctype_valid(context, etypes[i])) {
		krb5_set_error_string(context, "enctype %d not supported",
				      etypes[i]);
		return KRB5_PROG_ETYPE_NOSUPP;
	    }
	++i;
	ALLOC(p, i);
	if(!p) {
	    krb5_set_error_string (context, "malloc: out of memory");
	    return ENOMEM;
	}
	memmove(p, etypes, i * sizeof(krb5_enctype));
    }
    if(context->etypes)
	free(context->etypes);
    context->etypes = p;
    return 0;
}


krb5_error_code
krb5_get_default_in_tkt_etypes(krb5_context context,
			       krb5_enctype **etypes)
{
  krb5_enctype *p;
  int i;
  krb5_error_code ret;

  if(context->etypes) {
    for(i = 0; context->etypes[i]; i++);
    ++i;
    ALLOC(p, i);
    if(!p) {
      krb5_set_error_string (context, "malloc: out of memory");
      return ENOMEM;
    }
    memmove(p, context->etypes, i * sizeof(krb5_enctype));
  } else {
    ret = default_etypes(context, &p);
    if (ret)
      return ret;
  }
  *etypes = p;
  return 0;
}

const char *
krb5_get_err_text(krb5_context context, krb5_error_code code)
{
    const char *p = NULL;
    if(context != NULL)
	p = com_right(context->et_list, code);
    if(p == NULL)
	p = strerror(code);
    return p;
}

void
krb5_init_ets(krb5_context context)
{
    if(context->et_list == NULL){
	krb5_add_et_list(context, initialize_krb5_error_table_r);
	krb5_add_et_list(context, initialize_asn1_error_table_r);
	krb5_add_et_list(context, initialize_heim_error_table_r);
	krb5_add_et_list(context, initialize_k524_error_table_r);
    }
}

void
krb5_set_use_admin_kdc (krb5_context context, krb5_boolean flag)
{
    context->use_admin_kdc = flag;
}

krb5_boolean
krb5_get_use_admin_kdc (krb5_context context)
{
    return context->use_admin_kdc;
}

krb5_error_code
krb5_add_extra_addresses(krb5_context context, krb5_addresses *addresses)
{

    if(context->extra_addresses)
	return krb5_append_addresses(context, 
				     context->extra_addresses, addresses);
    else
	return krb5_set_extra_addresses(context, addresses);
}

krb5_error_code
krb5_set_extra_addresses(krb5_context context, const krb5_addresses *addresses)
{
    if(context->extra_addresses)
	krb5_free_addresses(context, context->extra_addresses);

    if(addresses == NULL) {
	if(context->extra_addresses != NULL) {
	    free(context->extra_addresses);
	    context->extra_addresses = NULL;
	}
	return 0;
    }
    if(context->extra_addresses == NULL) {
	context->extra_addresses = malloc(sizeof(*context->extra_addresses));
	if(context->extra_addresses == NULL) {
	    krb5_set_error_string (context, "malloc: out of memory");
	    return ENOMEM;
	}
    }
    return krb5_copy_addresses(context, addresses, context->extra_addresses);
}

krb5_error_code
krb5_get_extra_addresses(krb5_context context, krb5_addresses *addresses)
{
    if(context->extra_addresses == NULL) {
	memset(addresses, 0, sizeof(*addresses));
	return 0;
    }
    return krb5_copy_addresses(context,context->extra_addresses, addresses);
}

krb5_error_code
krb5_add_ignore_addresses(krb5_context context, krb5_addresses *addresses)
{

    if(context->ignore_addresses)
	return krb5_append_addresses(context, 
				     context->ignore_addresses, addresses);
    else
	return krb5_set_ignore_addresses(context, addresses);
}

krb5_error_code
krb5_set_ignore_addresses(krb5_context context, const krb5_addresses *addresses)
{
    if(context->ignore_addresses)
	krb5_free_addresses(context, context->ignore_addresses);
    if(addresses == NULL) {
	if(context->ignore_addresses != NULL) {
	    free(context->ignore_addresses);
	    context->ignore_addresses = NULL;
	}
	return 0;
    }
    if(context->ignore_addresses == NULL) {
	context->ignore_addresses = malloc(sizeof(*context->ignore_addresses));
	if(context->ignore_addresses == NULL) {
	    krb5_set_error_string (context, "malloc: out of memory");
	    return ENOMEM;
	}
    }
    return krb5_copy_addresses(context, addresses, context->ignore_addresses);
}

krb5_error_code
krb5_get_ignore_addresses(krb5_context context, krb5_addresses *addresses)
{
    if(context->ignore_addresses == NULL) {
	memset(addresses, 0, sizeof(*addresses));
	return 0;
    }
    return krb5_copy_addresses(context, context->ignore_addresses, addresses);
}

krb5_error_code
krb5_set_fcache_version(krb5_context context, int version)
{
    context->fcache_vno = version;
    return 0;
}

krb5_error_code
krb5_get_fcache_version(krb5_context context, int *version)
{
    *version = context->fcache_vno;
    return 0;
}
