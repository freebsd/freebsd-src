/*
 * Copyright (c) 1997-2001 Kungliga Tekniska Högskolan
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
#ifdef HAVE_RES_SEARCH
#define USE_RESOLVER
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#include <fnmatch.h>
#include "resolve.h"

RCSID("$Id: principal.c,v 1.74 2001/05/14 06:14:50 assar Exp $");

#define princ_num_comp(P) ((P)->name.name_string.len)
#define princ_type(P) ((P)->name.name_type)
#define princ_comp(P) ((P)->name.name_string.val)
#define princ_ncomp(P, N) ((P)->name.name_string.val[(N)])
#define princ_realm(P) ((P)->realm)

void
krb5_free_principal(krb5_context context,
		    krb5_principal p)
{
    if(p){
	free_Principal(p);
	free(p);
    }
}

krb5_error_code
krb5_parse_name(krb5_context context,
		const char *name,
		krb5_principal *principal)
{
    krb5_error_code ret;
    general_string *comp;
    general_string realm;
    int ncomp;

    char *p;
    char *q;
    char *s;
    char *start;

    int n;
    char c;
    int got_realm = 0;
  
    /* count number of component */
    ncomp = 1;
    for(p = (char*)name; *p; p++){
	if(*p=='\\'){
	    if(!p[1]) {
		krb5_set_error_string (context,
				       "trailing \\ in principal name");
		return KRB5_PARSE_MALFORMED;
	    }
	    p++;
	} else if(*p == '/')
	    ncomp++;
    }
    comp = calloc(ncomp, sizeof(*comp));
    if (comp == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
  
    n = 0;
    start = q = p = s = strdup(name);
    if (start == NULL) {
	free (comp);
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    while(*p){
	c = *p++;
	if(c == '\\'){
	    c = *p++;
	    if(c == 'n')
		c = '\n';
	    else if(c == 't')
		c = '\t';
	    else if(c == 'b')
		c = '\b';
	    else if(c == '0')
		c = '\0';
	}else if(c == '/' || c == '@'){
	    if(got_realm){
		krb5_set_error_string (context,
				       "part after realm in principal name");
		ret = KRB5_PARSE_MALFORMED;
		goto exit;
	    }else{
		comp[n] = malloc(q - start + 1);
		if (comp[n] == NULL) {
		    krb5_set_error_string (context, "malloc: out of memory");
		    ret = ENOMEM;
		    goto exit;
		}
		memcpy(comp[n], start, q - start);
		comp[n][q - start] = 0;
		n++;
	    }
	    if(c == '@')
		got_realm = 1;
	    start = q;
	    continue;
	}
	if(got_realm && (c == ':' || c == '/' || c == '\0')) {
	    krb5_set_error_string (context,
				   "part after realm in principal name");
	    ret = KRB5_PARSE_MALFORMED;
	    goto exit;
	}
	*q++ = c;
    }
    if(got_realm){
	realm = malloc(q - start + 1);
	if (realm == NULL) {
	    krb5_set_error_string (context, "malloc: out of memory");
	    ret = ENOMEM;
	    goto exit;
	}
	memcpy(realm, start, q - start);
	realm[q - start] = 0;
    }else{
	ret = krb5_get_default_realm (context, &realm);
	if (ret)
	    goto exit;

	comp[n] = malloc(q - start + 1);
	if (comp[n] == NULL) {
	    krb5_set_error_string (context, "malloc: out of memory");
	    ret = ENOMEM;
	    goto exit;
	}
	memcpy(comp[n], start, q - start);
	comp[n][q - start] = 0;
	n++;
    }
    *principal = malloc(sizeof(**principal));
    if (*principal == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	ret = ENOMEM;
	goto exit;
    }
    (*principal)->name.name_type = KRB5_NT_PRINCIPAL;
    (*principal)->name.name_string.val = comp;
    princ_num_comp(*principal) = n;
    (*principal)->realm = realm;
    free(s);
    return 0;
exit:
    while(n>0){
	free(comp[--n]);
    }
    free(comp);
    free(s);
    return ret;
}

static const char quotable_chars[] = " \n\t\b\\/@";
static const char replace_chars[] = " ntb\\/@";

#define add_char(BASE, INDEX, LEN, C) do { if((INDEX) < (LEN)) (BASE)[(INDEX)++] = (C); }while(0);

static size_t
quote_string(const char *s, char *out, size_t index, size_t len)
{
    const char *p, *q;
    for(p = s; *p && index < len; p++){
	if((q = strchr(quotable_chars, *p))){
	    add_char(out, index, len, '\\');
	    add_char(out, index, len, replace_chars[q - quotable_chars]);
	}else
	    add_char(out, index, len, *p);
    }
    if(index < len)
	out[index] = '\0';
    return index;
}


static krb5_error_code
unparse_name_fixed(krb5_context context,
		   krb5_const_principal principal,
		   char *name,
		   size_t len,
		   krb5_boolean short_form)
{
    size_t index = 0;
    int i;
    for(i = 0; i < princ_num_comp(principal); i++){
	if(i)
	    add_char(name, index, len, '/');
	index = quote_string(princ_ncomp(principal, i), name, index, len);
	if(index == len)
	    return ERANGE;
    } 
    /* add realm if different from default realm */
    if(short_form) {
	krb5_realm r;
	krb5_error_code ret;
	ret = krb5_get_default_realm(context, &r);
	if(ret)
	    return ret;
	if(strcmp(princ_realm(principal), r) != 0)
	    short_form = 0;
	free(r);
    }
    if(!short_form) {
	add_char(name, index, len, '@');
	index = quote_string(princ_realm(principal), name, index, len);
	if(index == len)
	    return ERANGE;
    }
    return 0;
}

krb5_error_code
krb5_unparse_name_fixed(krb5_context context,
			krb5_const_principal principal,
			char *name,
			size_t len)
{
    return unparse_name_fixed(context, principal, name, len, FALSE);
}

krb5_error_code
krb5_unparse_name_fixed_short(krb5_context context,
			      krb5_const_principal principal,
			      char *name,
			      size_t len)
{
    return unparse_name_fixed(context, principal, name, len, TRUE);
}

static krb5_error_code
unparse_name(krb5_context context,
	     krb5_const_principal principal,
	     char **name,
	     krb5_boolean short_flag)
{
    size_t len = 0, plen;
    int i;
    krb5_error_code ret;
    /* count length */
    plen = strlen(princ_realm(principal));
    if(strcspn(princ_realm(principal), quotable_chars) == plen)
	len += plen;
    else
	len += 2*plen;
    len++;
    for(i = 0; i < princ_num_comp(principal); i++){
	plen = strlen(princ_ncomp(principal, i));
	if(strcspn(princ_ncomp(principal, i), quotable_chars) == plen)
	    len += plen;
	else
	    len += 2*plen;
	len++;
    }
    *name = malloc(len);
    if(len != 0 && *name == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    ret = unparse_name_fixed(context, principal, *name, len, short_flag);
    if(ret)
	free(*name);
    return ret;
}

krb5_error_code
krb5_unparse_name(krb5_context context,
		  krb5_const_principal principal,
		  char **name)
{
    return unparse_name(context, principal, name, FALSE);
}

krb5_error_code
krb5_unparse_name_short(krb5_context context,
			krb5_const_principal principal,
			char **name)
{
    return unparse_name(context, principal, name, TRUE);
}

#if 0 /* not implemented */

krb5_error_code
krb5_unparse_name_ext(krb5_context context,
		      krb5_const_principal principal,
		      char **name,
		      size_t *size)
{
    krb5_abortx(context, "unimplemented krb5_unparse_name_ext called");
}

#endif

krb5_realm*
krb5_princ_realm(krb5_context context,
		 krb5_principal principal)
{
    return &princ_realm(principal);
}


void
krb5_princ_set_realm(krb5_context context,
		     krb5_principal principal,
		     krb5_realm *realm)
{
    princ_realm(principal) = *realm;
}


krb5_error_code
krb5_build_principal(krb5_context context,
		     krb5_principal *principal,
		     int rlen,
		     krb5_const_realm realm,
		     ...)
{
    krb5_error_code ret;
    va_list ap;
    va_start(ap, realm);
    ret = krb5_build_principal_va(context, principal, rlen, realm, ap);
    va_end(ap);
    return ret;
}

static krb5_error_code
append_component(krb5_context context, krb5_principal p, 
		 const char *comp,
		 size_t comp_len)
{
    general_string *tmp;
    size_t len = princ_num_comp(p);

    tmp = realloc(princ_comp(p), (len + 1) * sizeof(*tmp));
    if(tmp == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    princ_comp(p) = tmp;
    princ_ncomp(p, len) = malloc(comp_len + 1);
    if (princ_ncomp(p, len) == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    memcpy (princ_ncomp(p, len), comp, comp_len);
    princ_ncomp(p, len)[comp_len] = '\0';
    princ_num_comp(p)++;
    return 0;
}

static void
va_ext_princ(krb5_context context, krb5_principal p, va_list ap)
{
    while(1){
	const char *s;
	int len;
	len = va_arg(ap, int);
	if(len == 0)
	    break;
	s = va_arg(ap, const char*);
	append_component(context, p, s, len);
    }
}

static void
va_princ(krb5_context context, krb5_principal p, va_list ap)
{
    while(1){
	const char *s;
	s = va_arg(ap, const char*);
	if(s == NULL)
	    break;
	append_component(context, p, s, strlen(s));
    }
}


static krb5_error_code
build_principal(krb5_context context,
		krb5_principal *principal,
		int rlen,
		krb5_const_realm realm,
		void (*func)(krb5_context, krb5_principal, va_list),
		va_list ap)
{
    krb5_principal p;
  
    p = calloc(1, sizeof(*p));
    if (p == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    princ_type(p) = KRB5_NT_PRINCIPAL;

    princ_realm(p) = strdup(realm);
    if(p->realm == NULL){
	free(p);
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
  
    (*func)(context, p, ap);
    *principal = p;
    return 0;
}

krb5_error_code
krb5_make_principal(krb5_context context,
		    krb5_principal *principal,
		    krb5_const_realm realm,
		    ...)
{
    krb5_error_code ret;
    krb5_realm r = NULL;
    va_list ap;
    if(realm == NULL) {
	ret = krb5_get_default_realm(context, &r);
	if(ret)
	    return ret;
	realm = r;
    }
    va_start(ap, realm);
    ret = krb5_build_principal_va(context, principal, strlen(realm), realm, ap);
    va_end(ap);
    if(r)
	free(r);
    return ret;
}

krb5_error_code
krb5_build_principal_va(krb5_context context, 
			krb5_principal *principal, 
			int rlen,
			krb5_const_realm realm,
			va_list ap)
{
    return build_principal(context, principal, rlen, realm, va_princ, ap);
}

krb5_error_code
krb5_build_principal_va_ext(krb5_context context, 
			    krb5_principal *principal, 
			    int rlen,
			    krb5_const_realm realm,
			    va_list ap)
{
    return build_principal(context, principal, rlen, realm, va_ext_princ, ap);
}


krb5_error_code
krb5_build_principal_ext(krb5_context context,
			 krb5_principal *principal,
			 int rlen,
			 krb5_const_realm realm,
			 ...)
{
    krb5_error_code ret;
    va_list ap;
    va_start(ap, realm);
    ret = krb5_build_principal_va_ext(context, principal, rlen, realm, ap);
    va_end(ap);
    return ret;
}


krb5_error_code
krb5_copy_principal(krb5_context context,
		    krb5_const_principal inprinc,
		    krb5_principal *outprinc)
{
    krb5_principal p = malloc(sizeof(*p));
    if (p == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    if(copy_Principal(inprinc, p)) {
	free(p);
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    *outprinc = p;
    return 0;
}

/*
 * return TRUE iff princ1 == princ2 (without considering the realm)
 */

krb5_boolean
krb5_principal_compare_any_realm(krb5_context context,
				 krb5_const_principal princ1,
				 krb5_const_principal princ2)
{
    int i;
    if(princ_num_comp(princ1) != princ_num_comp(princ2))
	return FALSE;
    for(i = 0; i < princ_num_comp(princ1); i++){
	if(strcmp(princ_ncomp(princ1, i), princ_ncomp(princ2, i)) != 0)
	    return FALSE;
    }
    return TRUE;
}

/*
 * return TRUE iff princ1 == princ2
 */

krb5_boolean
krb5_principal_compare(krb5_context context,
		       krb5_const_principal princ1,
		       krb5_const_principal princ2)
{
    if(!krb5_realm_compare(context, princ1, princ2))
	return FALSE;
    return krb5_principal_compare_any_realm(context, princ1, princ2);
}

/*
 * return TRUE iff realm(princ1) == realm(princ2)
 */

krb5_boolean
krb5_realm_compare(krb5_context context,
		   krb5_const_principal princ1,
		   krb5_const_principal princ2)
{
    return strcmp(princ_realm(princ1), princ_realm(princ2)) == 0;
}

/*
 * return TRUE iff princ matches pattern
 */

krb5_boolean
krb5_principal_match(krb5_context context,
		     krb5_const_principal princ,
		     krb5_const_principal pattern)
{
    int i;
    if(princ_num_comp(princ) != princ_num_comp(pattern))
	return FALSE;
    if(fnmatch(princ_realm(pattern), princ_realm(princ), 0) != 0)
	return FALSE;
    for(i = 0; i < princ_num_comp(princ); i++){
	if(fnmatch(princ_ncomp(pattern, i), princ_ncomp(princ, i), 0) != 0)
	    return FALSE;
    }
    return TRUE;
}


struct v4_name_convert {
    const char *from;
    const char *to; 
} default_v4_name_convert[] = {
    { "ftp",	"ftp" },
    { "hprop",	"hprop" },
    { "pop",	"pop" },
    { "imap",	"imap" },
    { "rcmd",	"host" },
    { NULL, NULL }
};

/*
 * return the converted instance name of `name' in `realm'.
 * look in the configuration file and then in the default set above.
 * return NULL if no conversion is appropriate.
 */

static const char*
get_name_conversion(krb5_context context, const char *realm, const char *name)
{
    struct v4_name_convert *q;
    const char *p;

    p = krb5_config_get_string(context, NULL, "realms", realm,
			       "v4_name_convert", "host", name, NULL);
    if(p == NULL)
	p = krb5_config_get_string(context, NULL, "libdefaults", 
				   "v4_name_convert", "host", name, NULL);
    if(p)
	return p;

    /* XXX should be possible to override default list */
    p = krb5_config_get_string(context, NULL,
			       "realms",
			       realm,
			       "v4_name_convert",
			       "plain",
			       name,
			       NULL);
    if(p)
	return NULL;
    p = krb5_config_get_string(context, NULL,
			       "libdefaults",
			       "v4_name_convert",
			       "plain",
			       name,
			       NULL);
    if(p)
	return NULL;
    for(q = default_v4_name_convert; q->from; q++)
	if(strcmp(q->from, name) == 0)
	    return q->to;
    return NULL;
}

/*
 * convert the v4 principal `name.instance@realm' to a v5 principal in `princ'.
 * if `resolve', use DNS.
 * if `func', use that function for validating the conversion
 */

krb5_error_code
krb5_425_conv_principal_ext(krb5_context context,
			    const char *name,
			    const char *instance,
			    const char *realm,
			    krb5_boolean (*func)(krb5_context, krb5_principal),
			    krb5_boolean resolve,
			    krb5_principal *princ)
{
    const char *p;
    krb5_error_code ret;
    krb5_principal pr;
    char host[MAXHOSTNAMELEN];

    /* do the following: if the name is found in the
       `v4_name_convert:host' part, is is assumed to be a `host' type
       principal, and the instance is looked up in the
       `v4_instance_convert' part. if not found there the name is
       (optionally) looked up as a hostname, and if that doesn't yield
       anything, the `default_domain' is appended to the instance
       */

    if(instance == NULL)
	goto no_host;
    if(instance[0] == 0){
	instance = NULL;
	goto no_host;
    }
    p = get_name_conversion(context, realm, name);
    if(p == NULL)
	goto no_host;
    name = p;
    p = krb5_config_get_string(context, NULL, "realms", realm, 
			       "v4_instance_convert", instance, NULL);
    if(p){
	instance = p;
	ret = krb5_make_principal(context, &pr, realm, name, instance, NULL);
	if(func == NULL || (*func)(context, pr)){
	    *princ = pr;
	    return 0;
	}
	krb5_free_principal(context, pr);
	*princ = NULL;
	krb5_clear_error_string (context);
	return HEIM_ERR_V4_PRINC_NO_CONV;
    }
    if(resolve){
	const char *inst = NULL;
#ifdef USE_RESOLVER
	struct dns_reply *r;
	r = dns_lookup(instance, "a");
	if(r && r->head && r->head->type == T_A)
	    inst = r->head->domain;
#else
	struct hostent *hp = roken_gethostbyname(instance);
	if(hp)
	    inst = hp->h_name;
#endif
	if(inst) {
	    char *low_inst = strdup(inst);

	    if (low_inst == NULL) {
#ifdef USE_RESOLVER
		dns_free_data(r);
#endif
		krb5_set_error_string (context, "malloc: out of memory");
		return ENOMEM;
	    }
	    ret = krb5_make_principal(context, &pr, realm, name, low_inst,
				      NULL);
	    free (low_inst);
	    if(ret == 0) {
		if(func == NULL || (*func)(context, pr)){
		    *princ = pr;
#ifdef USE_RESOLVER
		    dns_free_data(r);
#endif
		    return 0;
		}
		krb5_free_principal(context, pr);
	    }
	}
#ifdef USE_RESOLVER
	if(r) 
	    dns_free_data(r);
#endif
    }
    {
	char **domains, **d;
	domains = krb5_config_get_strings(context, NULL, "realms", realm,
					  "v4_domains", NULL);
	for(d = domains; d && *d; d++){
	    snprintf(host, sizeof(host), "%s.%s", instance, *d);
	    ret = krb5_make_principal(context, &pr, realm, name, host, NULL);
	    if(func == NULL || (*func)(context, pr)){
		*princ = pr;
		krb5_config_free_strings(domains);
		return 0;
	    }
	    krb5_free_principal(context, pr);
	}
	krb5_config_free_strings(domains);
    }
    
    
    p = krb5_config_get_string(context, NULL, "realms", realm, 
			       "default_domain", NULL);
    if(p == NULL){
	/* this should be an error, just faking a name is not good */
	krb5_clear_error_string (context);
	return HEIM_ERR_V4_PRINC_NO_CONV;
    }
	
    if (*p == '.')
	++p;
    snprintf(host, sizeof(host), "%s.%s", instance, p);
    ret = krb5_make_principal(context, &pr, realm, name, host, NULL);
    if(func == NULL || (*func)(context, pr)){
	*princ = pr;
	return 0;
    }
    krb5_free_principal(context, pr);
    krb5_clear_error_string (context);
    return HEIM_ERR_V4_PRINC_NO_CONV;
no_host:
    p = krb5_config_get_string(context, NULL,
			       "realms",
			       realm,
			       "v4_name_convert",
			       "plain",
			       name,
			       NULL);
    if(p == NULL)
	p = krb5_config_get_string(context, NULL,
				   "libdefaults",
				   "v4_name_convert",
				   "plain",
				   name,
				   NULL);
    if(p)
	name = p;
    
    ret = krb5_make_principal(context, &pr, realm, name, instance, NULL);
    if(func == NULL || (*func)(context, pr)){
	*princ = pr;
	return 0;
    }
    krb5_free_principal(context, pr);
    krb5_clear_error_string (context);
    return HEIM_ERR_V4_PRINC_NO_CONV;
}

krb5_error_code
krb5_425_conv_principal(krb5_context context,
			const char *name,
			const char *instance,
			const char *realm,
			krb5_principal *princ)
{
    krb5_boolean resolve = krb5_config_get_bool(context,
						NULL,
						"libdefaults", 
						"v4_instance_resolve", 
						NULL);

    return krb5_425_conv_principal_ext(context, name, instance, realm, 
				       NULL, resolve, princ);
}


static int
check_list(const krb5_config_binding *l, const char *name, const char **out)
{
    while(l){
	if (l->type != krb5_config_string)
	    continue;
	if(strcmp(name, l->u.string) == 0) {
	    *out = l->name;
	    return 1;
	}
	l = l->next;
    }
    return 0;
}

static int
name_convert(krb5_context context, const char *name, const char *realm, 
	     const char **out)
{
    const krb5_config_binding *l;
    l = krb5_config_get_list (context,
			      NULL,
			      "realms",
			      realm,
			      "v4_name_convert",
			      "host",
			      NULL);
    if(l && check_list(l, name, out))
	return KRB5_NT_SRV_HST;
    l = krb5_config_get_list (context,
			      NULL,
			      "libdefaults",
			      "v4_name_convert",
			      "host",
			      NULL);
    if(l && check_list(l, name, out))
	return KRB5_NT_SRV_HST;
    l = krb5_config_get_list (context,
			      NULL,
			      "realms",
			      realm,
			      "v4_name_convert",
			      "plain",
			      NULL);
    if(l && check_list(l, name, out))
	return KRB5_NT_UNKNOWN;
    l = krb5_config_get_list (context,
			      NULL,
			      "libdefaults",
			      "v4_name_convert",
			      "host",
			      NULL);
    if(l && check_list(l, name, out))
	return KRB5_NT_UNKNOWN;
    
    /* didn't find it in config file, try built-in list */
    {
	struct v4_name_convert *q;
	for(q = default_v4_name_convert; q->from; q++) {
	    if(strcmp(name, q->to) == 0) {
		*out = q->from;
		return KRB5_NT_SRV_HST;
	    }
	}
    }
    return -1;
}

/*
 * convert the v5 principal in `principal' into a v4 corresponding one
 * in `name, instance, realm'
 * this is limited interface since there's no length given for these
 * three parameters.  They have to be 40 bytes each (ANAME_SZ).
 */

krb5_error_code
krb5_524_conv_principal(krb5_context context,
			const krb5_principal principal,
			char *name, 
			char *instance,
			char *realm)
{
    const char *n, *i, *r;
    char tmpinst[40];
    int type = princ_type(principal);
    const int aname_sz = 40;

    r = principal->realm;

    switch(principal->name.name_string.len){
    case 1:
	n = principal->name.name_string.val[0];
	i = "";
	break;
    case 2:
	n = principal->name.name_string.val[0];
	i = principal->name.name_string.val[1];
	break;
    default:
	krb5_set_error_string (context,
			       "cannot convert a %d component principal",
			       principal->name.name_string.len);
	return KRB5_PARSE_MALFORMED;
    }

    {
	const char *tmp;
	int t = name_convert(context, n, r, &tmp);
	if(t >= 0) {
	    type = t;
	    n = tmp;
	}
    }

    if(type == KRB5_NT_SRV_HST){
	char *p;

	strlcpy (tmpinst, i, sizeof(tmpinst));
	p = strchr(tmpinst, '.');
	if(p)
	    *p = 0;
	i = tmpinst;
    }
    
    if (strlcpy (name, n, aname_sz) >= aname_sz) {
	krb5_set_error_string (context,
			       "too long name component to convert");
	return KRB5_PARSE_MALFORMED;
    }
    if (strlcpy (instance, i, aname_sz) >= aname_sz) {
	krb5_set_error_string (context,
			       "too long instance component to convert");
	return KRB5_PARSE_MALFORMED;
    }
    if (strlcpy (realm, r, aname_sz) >= aname_sz) {
	krb5_set_error_string (context,
			       "too long realm component to convert");
	return KRB5_PARSE_MALFORMED;
    }
    return 0;
}

/*
 * Create a principal in `ret_princ' for the service `sname' running
 * on host `hostname'.  */
			
krb5_error_code
krb5_sname_to_principal (krb5_context context,
			 const char *hostname,
			 const char *sname,
			 int32_t type,
			 krb5_principal *ret_princ)
{
    krb5_error_code ret;
    char localhost[MAXHOSTNAMELEN];
    char **realms, *host = NULL;
	
    if(type != KRB5_NT_SRV_HST && type != KRB5_NT_UNKNOWN) {
	krb5_set_error_string (context, "unsupported name type %d",
			       type);
	return KRB5_SNAME_UNSUPP_NAMETYPE;
    }
    if(hostname == NULL) {
	gethostname(localhost, sizeof(localhost));
	hostname = localhost;
    }
    if(sname == NULL)
	sname = "host";
    if(type == KRB5_NT_SRV_HST) {
	ret = krb5_expand_hostname_realms (context, hostname,
					   &host, &realms);
	if (ret)
	    return ret;
	strlwr(host);
	hostname = host;
    } else {
	ret = krb5_get_host_realm(context, hostname, &realms);
	if(ret)
	    return ret;
    }

    ret = krb5_make_principal(context, ret_princ, realms[0], sname,
			      hostname, NULL);
    if(host)
	free(host);
    krb5_free_host_realm(context, realms);
    return ret;
}
