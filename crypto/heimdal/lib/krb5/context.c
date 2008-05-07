/*
 * Copyright (c) 1997 - 2005 Kungliga Tekniska Högskolan
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

RCSID("$Id: context.c 22293 2007-12-14 05:25:59Z lha $");

#define INIT_FIELD(C, T, E, D, F)					\
    (C)->E = krb5_config_get_ ## T ## _default ((C), NULL, (D), 	\
						"libdefaults", F, NULL)

#define INIT_FLAG(C, O, V, D, F)					\
    do {								\
	if (krb5_config_get_bool_default((C), NULL, (D),"libdefaults", F, NULL)) { \
	    (C)->O |= V;						\
        }								\
    } while(0)

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
	    krb5_enctype e;
	    if(krb5_string_to_enctype(context, etypes_str[j], &e) != 0)
		continue;
	    if (krb5_enctype_valid(context, e) != 0)
		continue;
	    etypes[k++] = e;
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
    /* prefer dns_lookup_kdc over srv_lookup. */
    INIT_FIELD(context, bool, srv_lookup, TRUE, "srv_lookup");
    INIT_FIELD(context, bool, srv_lookup, context->srv_lookup, "dns_lookup_kdc");
    INIT_FIELD(context, int, large_msg_size, 1400, "large_message_size");
    INIT_FLAG(context, flags, KRB5_CTX_F_DNS_CANONICALIZE_HOSTNAME, TRUE, "dns_canonicalize_hostname");
    INIT_FLAG(context, flags, KRB5_CTX_F_CHECK_PAC, TRUE, "check_pac");
    context->default_cc_name = NULL;
    context->default_cc_name_set = 0;
    return 0;
}

/**
 * Initializes the context structure and reads the configuration file
 * /etc/krb5.conf. The structure should be freed by calling
 * krb5_free_context() when it is no longer being used.
 *
 * @param context pointer to returned context
 *
 * @return Returns 0 to indicate success.  Otherwise an errno code is
 * returned.  Failure means either that something bad happened during
 * initialization (typically ENOMEM) or that Kerberos should not be
 * used ENXIO.
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_init_context(krb5_context *context)
{
    krb5_context p;
    krb5_error_code ret;
    char **files;

    *context = NULL;

    p = calloc(1, sizeof(*p));
    if(!p)
	return ENOMEM;

    p->mutex = malloc(sizeof(HEIMDAL_MUTEX));
    if (p->mutex == NULL) {
	free(p);
	return ENOMEM;
    }
    HEIMDAL_MUTEX_init(p->mutex);

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
    krb5_cc_register(p, &krb5_acc_ops, TRUE);
    krb5_cc_register(p, &krb5_fcc_ops, TRUE);
    krb5_cc_register(p, &krb5_mcc_ops, TRUE);
#ifdef HAVE_KCM
    krb5_cc_register(p, &krb5_kcm_ops, TRUE);
#endif

    p->num_kt_types = 0;
    p->kt_types     = NULL;
    krb5_kt_register (p, &krb5_fkt_ops);
    krb5_kt_register (p, &krb5_wrfkt_ops);
    krb5_kt_register (p, &krb5_javakt_ops);
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

/**
 * Frees the krb5_context allocated by krb5_init_context().
 *
 * @param context context to be freed.
 *
 *  @ingroup krb5
*/

void KRB5_LIB_FUNCTION
krb5_free_context(krb5_context context)
{
    if (context->default_cc_name)
	free(context->default_cc_name);
    if (context->default_cc_name_env)
	free(context->default_cc_name_env);
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
    krb5_set_send_to_kdc_func(context, NULL, NULL);
    if (context->mutex != NULL) {
	HEIMDAL_MUTEX_destroy(context->mutex);
	free(context->mutex);
    }
    memset(context, 0, sizeof(*context));
    free(context);
}

/**
 * Reinit the context from a new set of filenames.
 *
 * @param context context to add configuration too.
 * @param filenames array of filenames, end of list is indicated with a NULL filename.
 *
 * @return Returns 0 to indicate success.  Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_set_config_files(krb5_context context, char **filenames)
{
    krb5_error_code ret;
    krb5_config_binding *tmp = NULL;
    while(filenames != NULL && *filenames != NULL && **filenames != '\0') {
	ret = krb5_config_parse_file_multi(context, *filenames, &tmp);
	if(ret != 0 && ret != ENOENT && ret != EACCES) {
	    krb5_config_file_free(context, tmp);
	    return ret;
	}
	filenames++;
    }
#if 0
    /* with this enabled and if there are no config files, Kerberos is
       considererd disabled */
    if(tmp == NULL)
	return ENXIO;
#endif
    krb5_config_file_free(context, context->cf);
    context->cf = tmp;
    ret = init_context_from_config_file(context);
    return ret;
}

static krb5_error_code
add_file(char ***pfilenames, int *len, char *file)
{
    char **pp = *pfilenames;
    int i;

    for(i = 0; i < *len; i++) {
	if(strcmp(pp[i], file) == 0) {
	    free(file);
	    return 0;
	}
    }

    pp = realloc(*pfilenames, (*len + 2) * sizeof(*pp));
    if (pp == NULL) {
	free(file);
	return ENOMEM;
    }

    pp[*len] = file;
    pp[*len + 1] = NULL;
    *pfilenames = pp;
    *len += 1;
    return 0;
}

/*
 *  `pq' isn't free, it's up the the caller
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_prepend_config_files(const char *filelist, char **pq, char ***ret_pp)
{
    krb5_error_code ret;
    const char *p, *q;
    char **pp;
    int len;
    char *fn;

    pp = NULL;

    len = 0;
    p = filelist;
    while(1) {
	ssize_t l;
	q = p;
	l = strsep_copy(&q, ":", NULL, 0);
	if(l == -1)
	    break;
	fn = malloc(l + 1);
	if(fn == NULL) {
	    krb5_free_config_files(pp);
	    return ENOMEM;
	}
	l = strsep_copy(&p, ":", fn, l + 1);
	ret = add_file(&pp, &len, fn);
	if (ret) {
	    krb5_free_config_files(pp);
	    return ret;
	}
    }

    if (pq != NULL) {
	int i;

	for (i = 0; pq[i] != NULL; i++) {
	    fn = strdup(pq[i]);
	    if (fn == NULL) {
		krb5_free_config_files(pp);
		return ENOMEM;
	    }
	    ret = add_file(&pp, &len, fn);
	    if (ret) {
		krb5_free_config_files(pp);
		return ret;
	    }
	}
    }

    *ret_pp = pp;
    return 0;
}

/**
 * Prepend the filename to the global configuration list.
 *
 * @param filelist a filename to add to the default list of filename
 * @param pfilenames return array of filenames, should be freed with krb5_free_config_files().
 *
 * @return Returns 0 to indicate success.  Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_prepend_config_files_default(const char *filelist, char ***pfilenames)
{
    krb5_error_code ret;
    char **defpp, **pp = NULL;
    
    ret = krb5_get_default_config_files(&defpp);
    if (ret)
	return ret;

    ret = krb5_prepend_config_files(filelist, defpp, &pp);
    krb5_free_config_files(defpp);
    if (ret) {
	return ret;
    }	
    *pfilenames = pp;
    return 0;
}

/**
 * Get the global configuration list.
 *
 * @param pfilenames return array of filenames, should be freed with krb5_free_config_files().
 *
 * @return Returns 0 to indicate success.  Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION 
krb5_get_default_config_files(char ***pfilenames)
{
    const char *files = NULL;

    if (pfilenames == NULL)
        return EINVAL;
    if(!issuid())
	files = getenv("KRB5_CONFIG");
    if (files == NULL)
	files = krb5_config_file;

    return krb5_prepend_config_files(files, NULL, pfilenames);
}

/**
 * Free a list of configuration files.
 *
 * @param filenames list to be freed.
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

void KRB5_LIB_FUNCTION
krb5_free_config_files(char **filenames)
{
    char **p;
    for(p = filenames; *p != NULL; p++)
	free(*p);
    free(filenames);
}

/**
 * Returns the list of Kerberos encryption types sorted in order of
 * most preferred to least preferred encryption type.  Note that some
 * encryption types might be disabled, so you need to check with
 * krb5_enctype_valid() before using the encryption type.
 *
 * @return list of enctypes, terminated with ETYPE_NULL. Its a static
 * array completed into the Kerberos library so the content doesn't
 * need to be freed.
 *
 * @ingroup krb5
 */

const krb5_enctype * KRB5_LIB_FUNCTION
krb5_kerberos_enctypes(krb5_context context)
{
    static const krb5_enctype p[] = {
	ETYPE_AES256_CTS_HMAC_SHA1_96,
	ETYPE_AES128_CTS_HMAC_SHA1_96,
	ETYPE_DES3_CBC_SHA1,
	ETYPE_DES3_CBC_MD5,
	ETYPE_ARCFOUR_HMAC_MD5,
	ETYPE_DES_CBC_MD5,
	ETYPE_DES_CBC_MD4,
	ETYPE_DES_CBC_CRC,
	ETYPE_NULL
    };
    return p;
}

/*
 * set `etype' to a malloced list of the default enctypes
 */

static krb5_error_code
default_etypes(krb5_context context, krb5_enctype **etype)
{
    const krb5_enctype *p;
    krb5_enctype *e = NULL, *ep;
    int i, n = 0;

    p = krb5_kerberos_enctypes(context);

    for (i = 0; p[i] != ETYPE_NULL; i++) {
	if (krb5_enctype_valid(context, p[i]) != 0)
	    continue;
	ep = realloc(e, (n + 2) * sizeof(*e));
	if (ep == NULL) {
	    free(e);
	    krb5_set_error_string (context, "malloc: out of memory");
	    return ENOMEM;
	}
	e = ep;
	e[n] = p[i];
	e[n + 1] = ETYPE_NULL;
	n++;
    }
    *etype = e;
    return 0;
}

/**
 * Set the default encryption types that will be use in communcation
 * with the KDC, clients and servers.
 *
 * @param context Kerberos 5 context.
 * @param etypes Encryption types, array terminated with ETYPE_NULL (0).
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_set_default_in_tkt_etypes(krb5_context context, 
			       const krb5_enctype *etypes)
{
    krb5_enctype *p = NULL;
    int i;

    if(etypes) {
	for (i = 0; etypes[i]; ++i) {
	    krb5_error_code ret;
	    ret = krb5_enctype_valid(context, etypes[i]);
	    if (ret)
		return ret;
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

/**
 * Get the default encryption types that will be use in communcation
 * with the KDC, clients and servers.
 *
 * @param context Kerberos 5 context.
 * @param etypes Encryption types, array terminated with
 * ETYPE_NULL(0), caller should free array with krb5_xfree():
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
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

/**
 * Return the error string for the error code. The caller must not
 * free the string.
 *
 * @param context Kerberos 5 context.
 * @param code Kerberos error code.
 *
 * @return the error message matching code
 *
 * @ingroup krb5
 */

const char* KRB5_LIB_FUNCTION
krb5_get_err_text(krb5_context context, krb5_error_code code)
{
    const char *p = NULL;
    if(context != NULL)
	p = com_right(context->et_list, code);
    if(p == NULL)
	p = strerror(code);
    if (p == NULL)
	p = "Unknown error";
    return p;
}

/**
 * Init the built-in ets in the Kerberos library. 
 *
 * @param context kerberos context to add the ets too
 *
 * @ingroup krb5
 */

void KRB5_LIB_FUNCTION
krb5_init_ets(krb5_context context)
{
    if(context->et_list == NULL){
	krb5_add_et_list(context, initialize_krb5_error_table_r);
	krb5_add_et_list(context, initialize_asn1_error_table_r);
	krb5_add_et_list(context, initialize_heim_error_table_r);
	krb5_add_et_list(context, initialize_k524_error_table_r);
#ifdef PKINIT
	krb5_add_et_list(context, initialize_hx_error_table_r);
#endif
    }
}

/**
 * Make the kerberos library default to the admin KDC.
 *
 * @param context Kerberos 5 context.
 * @param flag boolean flag to select if the use the admin KDC or not.
 *
 * @ingroup krb5
 */

void KRB5_LIB_FUNCTION
krb5_set_use_admin_kdc (krb5_context context, krb5_boolean flag)
{
    context->use_admin_kdc = flag;
}

/**
 * Make the kerberos library default to the admin KDC.
 *
 * @param context Kerberos 5 context.
 *
 * @return boolean flag to telling the context will use admin KDC as the default KDC.
 *
 * @ingroup krb5
 */

krb5_boolean KRB5_LIB_FUNCTION
krb5_get_use_admin_kdc (krb5_context context)
{
    return context->use_admin_kdc;
}

/**
 * Add extra address to the address list that the library will add to
 * the client's address list when communicating with the KDC.
 *
 * @param context Kerberos 5 context.
 * @param addresses addreses to add
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_add_extra_addresses(krb5_context context, krb5_addresses *addresses)
{

    if(context->extra_addresses)
	return krb5_append_addresses(context, 
				     context->extra_addresses, addresses);
    else
	return krb5_set_extra_addresses(context, addresses);
}

/**
 * Set extra address to the address list that the library will add to
 * the client's address list when communicating with the KDC.
 *
 * @param context Kerberos 5 context.
 * @param addresses addreses to set
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
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

/**
 * Get extra address to the address list that the library will add to
 * the client's address list when communicating with the KDC.
 *
 * @param context Kerberos 5 context.
 * @param addresses addreses to set
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_extra_addresses(krb5_context context, krb5_addresses *addresses)
{
    if(context->extra_addresses == NULL) {
	memset(addresses, 0, sizeof(*addresses));
	return 0;
    }
    return krb5_copy_addresses(context,context->extra_addresses, addresses);
}

/**
 * Add extra addresses to ignore when fetching addresses from the
 * underlaying operating system.
 *
 * @param context Kerberos 5 context.
 * @param addresses addreses to ignore
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_add_ignore_addresses(krb5_context context, krb5_addresses *addresses)
{

    if(context->ignore_addresses)
	return krb5_append_addresses(context, 
				     context->ignore_addresses, addresses);
    else
	return krb5_set_ignore_addresses(context, addresses);
}

/**
 * Set extra addresses to ignore when fetching addresses from the
 * underlaying operating system.
 *
 * @param context Kerberos 5 context.
 * @param addresses addreses to ignore
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
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

/**
 * Get extra addresses to ignore when fetching addresses from the
 * underlaying operating system.
 *
 * @param context Kerberos 5 context.
 * @param addresses list addreses ignored
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_ignore_addresses(krb5_context context, krb5_addresses *addresses)
{
    if(context->ignore_addresses == NULL) {
	memset(addresses, 0, sizeof(*addresses));
	return 0;
    }
    return krb5_copy_addresses(context, context->ignore_addresses, addresses);
}

/**
 * Set version of fcache that the library should use.
 *
 * @param context Kerberos 5 context.
 * @param version version number.
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_set_fcache_version(krb5_context context, int version)
{
    context->fcache_vno = version;
    return 0;
}

/**
 * Get version of fcache that the library should use.
 *
 * @param context Kerberos 5 context.
 * @param version version number.
 *
 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_fcache_version(krb5_context context, int *version)
{
    *version = context->fcache_vno;
    return 0;
}

/**
 * Runtime check if the Kerberos library was complied with thread support.
 *
 * @return TRUE if the library was compiled with thread support, FALSE if not.
 *
 * @ingroup krb5
 */


krb5_boolean KRB5_LIB_FUNCTION
krb5_is_thread_safe(void)
{
#ifdef ENABLE_PTHREAD_SUPPORT
    return TRUE;
#else
    return FALSE;
#endif
}

/**
 * Set if the library should use DNS to canonicalize hostnames.
 *
 * @param context Kerberos 5 context.
 * @param flag if its dns canonicalizion is used or not.
 *
 * @ingroup krb5
 */

void KRB5_LIB_FUNCTION
krb5_set_dns_canonicalize_hostname (krb5_context context, krb5_boolean flag)
{
    if (flag)
	context->flags |= KRB5_CTX_F_DNS_CANONICALIZE_HOSTNAME;
    else
	context->flags &= ~KRB5_CTX_F_DNS_CANONICALIZE_HOSTNAME;
}

/**
 * Get if the library uses DNS to canonicalize hostnames.
 *
 * @param context Kerberos 5 context.
 *
 * @return return non zero if the library uses DNS to canonicalize hostnames.
 *
 * @ingroup krb5
 */

krb5_boolean KRB5_LIB_FUNCTION
krb5_get_dns_canonicalize_hostname (krb5_context context)
{
    return (context->flags & KRB5_CTX_F_DNS_CANONICALIZE_HOSTNAME) ? 1 : 0;
}

/**
 * Get current offset in time to the KDC.
 *
 * @param context Kerberos 5 context.
 * @param sec seconds part of offset.
 * @param usec micro seconds part of offset.
 *
 * @return return non zero if the library uses DNS to canonicalize hostnames.
 *
 * @ingroup krb5
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_kdc_sec_offset (krb5_context context, int32_t *sec, int32_t *usec)
{
    if (sec)
	*sec = context->kdc_sec_offset;
    if (usec)
	*usec = context->kdc_usec_offset;
    return 0;
}

/**
 * Get max time skew allowed.
 *
 * @param context Kerberos 5 context.
 *
 * @return timeskew in seconds.
 *
 * @ingroup krb5
 */

time_t KRB5_LIB_FUNCTION
krb5_get_max_time_skew (krb5_context context)
{
    return context->max_skew;
}

/**
 * Set max time skew allowed.
 *
 * @param context Kerberos 5 context.
 * @param t timeskew in seconds.
 *
 * @ingroup krb5
 */

void KRB5_LIB_FUNCTION
krb5_set_max_time_skew (krb5_context context, time_t t)
{
    context->max_skew = t;
}
