/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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
#include <resolve.h>

RCSID("$Id: get_host_realm.c,v 1.28 2001/05/14 06:14:47 assar Exp $");

/* To automagically find the correct realm of a host (without
 * [domain_realm] in krb5.conf) add a text record for your domain with
 * the name of your realm, like this:
 *
 * krb5-realm	IN	TXT	FOO.SE
 *
 * The search is recursive, so you can add entries for specific
 * hosts. To find the realm of host a.b.c, it first tries
 * krb5-realm.a.b.c, then krb5-realm.b.c and so on.
 *
 * Also supported is _kerberos (following draft-ietf-cat-krb-dns-locate-01.txt)
 *
 */

static int
copy_txt_to_realms (struct resource_record *head,
		    krb5_realm **realms)
{
    struct resource_record *rr;
    int n, i;

    for(n = 0, rr = head; rr; rr = rr->next)
	if (rr->type == T_TXT)
	    ++n;

    if (n == 0)
	return -1;

    *realms = malloc ((n + 1) * sizeof(krb5_realm));
    if (*realms == NULL)
	return -1;

    for (i = 0; i < n + 1; ++i)
	(*realms)[i] = NULL;

    for (i = 0, rr = head; rr; rr = rr->next) {
	if (rr->type == T_TXT) {
	    char *tmp;

	    tmp = strdup(rr->u.txt);
	    if (tmp == NULL) {
		for (i = 0; i < n; ++i)
		    free ((*realms)[i]);
		free (*realms);
		return -1;
	    }
	    (*realms)[i] = tmp;
	    ++i;
	}
    }
    return 0;
}

static int
dns_find_realm(krb5_context context,
	       const char *domain,
	       const char *dom_string,
	       krb5_realm **realms)
{
    char dom[MAXHOSTNAMELEN];
    struct dns_reply *r;
    int ret;
    
    if(*domain == '.')
	domain++;
    snprintf(dom, sizeof(dom), "%s.%s.", dom_string, domain);
    r = dns_lookup(dom, "TXT");
    if(r == NULL)
	return -1;

    ret = copy_txt_to_realms (r->head, realms);
    dns_free_data(r);
    return ret;
}

/*
 * Try to figure out what realms host in `domain' belong to from the
 * configuration file.
 */

static int
config_find_realm(krb5_context context, 
		  const char *domain, 
		  krb5_realm **realms)
{
    char **tmp = krb5_config_get_strings (context, NULL,
					  "domain_realm",
					  domain,
					  NULL);

    if (tmp == NULL)
	return -1;
    *realms = tmp;
    return 0;
}

/*
 * This function assumes that `host' is a FQDN (and doesn't handle the
 * special case of host == NULL either).
 * Try to find mapping in the config file or DNS and it that fails,
 * fall back to guessing
 */

krb5_error_code
krb5_get_host_realm_int (krb5_context context,
			 const char *host,
			 krb5_boolean use_dns,
			 krb5_realm **realms)
{
    const char *p;

    for (p = host; p != NULL; p = strchr (p + 1, '.')) {
	if(config_find_realm(context, p, realms) == 0)
	    return 0;
	else if(use_dns) {
	    if(dns_find_realm(context, p, "krb5-realm", realms) == 0)
		return 0;
	    if(dns_find_realm(context, p, "_kerberos", realms) == 0)
		return 0;
	}
    }
    p = strchr(host, '.');
    if(p != NULL) {
	p++;
	*realms = malloc(2 * sizeof(krb5_realm));
	if (*realms == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}

	(*realms)[0] = strdup(p);
	if((*realms)[0] == NULL) {
	    free(*realms);
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
	strupr((*realms)[0]);
	(*realms)[1] = NULL;
	return 0;
    }
    krb5_set_error_string(context, "unable to find realm of host %s", host);
    return KRB5_ERR_HOST_REALM_UNKNOWN;
}

/*
 * Return the realm(s) of `host' as a NULL-terminated list in `realms'.
 */

krb5_error_code
krb5_get_host_realm(krb5_context context,
		    const char *host,
		    krb5_realm **realms)
{
    char hostname[MAXHOSTNAMELEN];

    if (host == NULL) {
	if (gethostname (hostname, sizeof(hostname)))
	    return errno;
	host = hostname;
    }

    return krb5_get_host_realm_int (context, host, 1, realms);
}
