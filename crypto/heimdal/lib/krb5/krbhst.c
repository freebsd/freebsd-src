/*
 * Copyright (c) 2001 Kungliga Tekniska Högskolan
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

RCSID("$Id: krbhst.c,v 1.41 2002/08/16 18:48:19 nectar Exp $");

static int
string_to_proto(const char *string)
{
    if(strcasecmp(string, "udp") == 0)
	return KRB5_KRBHST_UDP;
    else if(strcasecmp(string, "tcp") == 0) 
	return KRB5_KRBHST_TCP;
    else if(strcasecmp(string, "http") == 0) 
	return KRB5_KRBHST_HTTP;
    return -1;
}

/*
 * set `res' and `count' to the result of looking up SRV RR in DNS for
 * `proto', `proto', `realm' using `dns_type'.
 * if `port' != 0, force that port number
 */

static krb5_error_code
srv_find_realm(krb5_context context, krb5_krbhst_info ***res, int *count, 
	       const char *realm, const char *dns_type,
	       const char *proto, const char *service, int port)
{
    char domain[1024];
    struct dns_reply *r;
    struct resource_record *rr;
    int num_srv;
    int proto_num;
    int def_port;

    proto_num = string_to_proto(proto);
    if(proto_num < 0) {
	krb5_set_error_string(context, "unknown protocol `%s'", proto);
	return EINVAL;
    }

    if(proto_num == KRB5_KRBHST_HTTP)
	def_port = ntohs(krb5_getportbyname (context, "http", "tcp", 80));
    else if(port == 0)
	def_port = ntohs(krb5_getportbyname (context, service, proto, 88));
    else
	def_port = port;

    snprintf(domain, sizeof(domain), "_%s._%s.%s.", service, proto, realm);

    r = dns_lookup(domain, dns_type);
    if(r == NULL) {
	*res = NULL;
	*count = 0;
	return KRB5_KDC_UNREACH;
    }

    for(num_srv = 0, rr = r->head; rr; rr = rr->next) 
	if(rr->type == T_SRV)
	    num_srv++;

    *res = malloc(num_srv * sizeof(**res));
    if(*res == NULL) {
	dns_free_data(r);
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }

    dns_srv_order(r);

    for(num_srv = 0, rr = r->head; rr; rr = rr->next) 
	if(rr->type == T_SRV) {
	    krb5_krbhst_info *hi;
	    hi = calloc(1, sizeof(*hi) + strlen(rr->u.srv->target));
	    if(hi == NULL) {
		dns_free_data(r);
		while(--num_srv >= 0)
		    free((*res)[num_srv]);
		free(*res);
		return ENOMEM;
	    }
	    (*res)[num_srv++] = hi;

	    hi->proto = proto_num;
	    
	    hi->def_port = def_port;
	    if (port != 0)
		hi->port = port;
	    else
		hi->port = rr->u.srv->port;

	    strcpy(hi->hostname, rr->u.srv->target);
	}

    *count = num_srv;
	    
    dns_free_data(r);
    return 0;
}


struct krb5_krbhst_data {
    char *realm;
    unsigned int flags;
    int def_port;
    int port;			/* hardwired port number if != 0 */
#define KD_CONFIG		1
#define KD_SRV_UDP		2
#define KD_SRV_TCP		4
#define KD_SRV_HTTP		8
#define KD_FALLBACK	       16
#define KD_CONFIG_EXISTS       32

    krb5_error_code (*get_next)(krb5_context, struct krb5_krbhst_data *, 
				krb5_krbhst_info**);

    unsigned int fallback_count;

    struct krb5_krbhst_info *hosts, **index, **end;
};

static krb5_boolean
krbhst_empty(const struct krb5_krbhst_data *kd)
{
    return kd->index == &kd->hosts;
}

/*
 * parse `spec' into a krb5_krbhst_info, defaulting the port to `def_port'
 * and forcing it to `port' if port != 0
 */

static struct krb5_krbhst_info*
parse_hostspec(krb5_context context, const char *spec, int def_port, int port)
{
    const char *p = spec;
    struct krb5_krbhst_info *hi;
    
    hi = calloc(1, sizeof(*hi) + strlen(spec));
    if(hi == NULL)
	return NULL;
       
    hi->proto = KRB5_KRBHST_UDP;

    if(strncmp(p, "http://", 7) == 0){
	hi->proto = KRB5_KRBHST_HTTP;
	p += 7;
    } else if(strncmp(p, "http/", 5) == 0) {
	hi->proto = KRB5_KRBHST_HTTP;
	p += 5;
	def_port = ntohs(krb5_getportbyname (context, "http", "tcp", 80));
    }else if(strncmp(p, "tcp/", 4) == 0){
	hi->proto = KRB5_KRBHST_TCP;
	p += 4;
    } else if(strncmp(p, "udp/", 4) == 0) {
	p += 4;
    }

    if(strsep_copy(&p, ":", hi->hostname, strlen(spec) + 1) < 0) {
	free(hi);
	return NULL;
    }
    /* get rid of trailing /, and convert to lower case */
    hi->hostname[strcspn(hi->hostname, "/")] = '\0';
    strlwr(hi->hostname);

    hi->port = hi->def_port = def_port;
    if(p != NULL) {
	char *end;
	hi->port = strtol(p, &end, 0);
	if(end == p) {
	    free(hi);
	    return NULL;
	}
    }
    if (port)
	hi->port = port;
    return hi;
}

static void
free_krbhst_info(krb5_krbhst_info *hi)
{
    if (hi->ai != NULL)
	freeaddrinfo(hi->ai);
    free(hi);
}

static void
append_host_hostinfo(struct krb5_krbhst_data *kd, struct krb5_krbhst_info *host)
{
    struct krb5_krbhst_info *h;

    for(h = kd->hosts; h; h = h->next)
	if(h->proto == host->proto && 
	   h->port == host->port && 
	   strcmp(h->hostname, host->hostname) == 0) {
	    free_krbhst_info(host);
	    return;
	}
    *kd->end = host;
    kd->end = &host->next;
}

static krb5_error_code
append_host_string(krb5_context context, struct krb5_krbhst_data *kd,
		   const char *host, int def_port, int port)
{
    struct krb5_krbhst_info *hi;

    hi = parse_hostspec(context, host, def_port, port);
    if(hi == NULL)
	return ENOMEM;
    
    append_host_hostinfo(kd, hi);
    return 0;
}

/*
 * return a readable representation of `host' in `hostname, hostlen'
 */

krb5_error_code
krb5_krbhst_format_string(krb5_context context, const krb5_krbhst_info *host, 
			  char *hostname, size_t hostlen)
{
    const char *proto = "";
    char portstr[7] = "";
    if(host->proto == KRB5_KRBHST_TCP)
	proto = "tcp/";
    else if(host->proto == KRB5_KRBHST_HTTP)
	proto = "http://";
    if(host->port != host->def_port)
	snprintf(portstr, sizeof(portstr), ":%d", host->port);
    snprintf(hostname, hostlen, "%s%s%s", proto, host->hostname, portstr);
    return 0;
}

/*
 * create a getaddrinfo `hints' based on `proto'
 */

static void
make_hints(struct addrinfo *hints, int proto)
{
    memset(hints, 0, sizeof(*hints));
    hints->ai_family = AF_UNSPEC;
    switch(proto) {
    case KRB5_KRBHST_UDP :
	hints->ai_socktype = SOCK_DGRAM;
	break;
    case KRB5_KRBHST_HTTP :
    case KRB5_KRBHST_TCP :
	hints->ai_socktype = SOCK_STREAM;
	break;
    }
}

/*
 * return an `struct addrinfo *' in `ai' corresponding to the information
 * in `host'.  free:ing is handled by krb5_krbhst_free.
 */

krb5_error_code
krb5_krbhst_get_addrinfo(krb5_context context, krb5_krbhst_info *host,
			 struct addrinfo **ai)
{
    struct addrinfo hints;
    char portstr[NI_MAXSERV];
    int ret;

    if (host->ai == NULL) {
	make_hints(&hints, host->proto);
	snprintf (portstr, sizeof(portstr), "%d", host->port);
	ret = getaddrinfo(host->hostname, portstr, &hints, &host->ai);
	if (ret)
	    return krb5_eai_to_heim_errno(ret, errno);
    }
    *ai = host->ai;
    return 0;
}

static krb5_boolean
get_next(struct krb5_krbhst_data *kd, krb5_krbhst_info **host)
{
    struct krb5_krbhst_info *hi = *kd->index;
    if(hi != NULL) {
	*host = hi;
	kd->index = &(*kd->index)->next;
	return TRUE;
    }
    return FALSE;
}

static void
srv_get_hosts(krb5_context context, struct krb5_krbhst_data *kd, 
	const char *proto, const char *service)
{
    krb5_krbhst_info **res;
    int count, i;

    srv_find_realm(context, &res, &count, kd->realm, "SRV", proto, service,
		   kd->port);
    for(i = 0; i < count; i++)
	append_host_hostinfo(kd, res[i]);
    free(res);
}

/*
 * read the configuration for `conf_string', defaulting to kd->def_port and
 * forcing it to `kd->port' if kd->port != 0
 */

static void
config_get_hosts(krb5_context context, struct krb5_krbhst_data *kd, 
		 const char *conf_string)
{
    int i;
	
    char **hostlist;
    hostlist = krb5_config_get_strings(context, NULL, 
				       "realms", kd->realm, conf_string, NULL);

    if(hostlist == NULL)
	return;
    kd->flags |= KD_CONFIG_EXISTS;
    for(i = 0; hostlist && hostlist[i] != NULL; i++)
	append_host_string(context, kd, hostlist[i], kd->def_port, kd->port);

    krb5_config_free_strings(hostlist);
}

/*
 * as a fallback, look for `serv_string.kd->realm' (typically
 * kerberos.REALM, kerberos-1.REALM, ...
 * `port' is the default port for the service, and `proto' the 
 * protocol
 */

static krb5_error_code
fallback_get_hosts(krb5_context context, struct krb5_krbhst_data *kd, 
		   const char *serv_string, int port, int proto)
{
    char *host;
    int ret;
    struct addrinfo *ai;
    struct addrinfo hints;
    char portstr[NI_MAXSERV];

    if(kd->fallback_count == 0)
	asprintf(&host, "%s.%s.", serv_string, kd->realm);
    else
	asprintf(&host, "%s-%d.%s.", 
		 serv_string, kd->fallback_count, kd->realm);	    

    if (host == NULL)
	return ENOMEM;
    
    make_hints(&hints, proto);
    snprintf(portstr, sizeof(portstr), "%d", port);
    ret = getaddrinfo(host, portstr, &hints, &ai);
    if (ret) {
	/* no more hosts, so we're done here */
	free(host);
	kd->flags |= KD_FALLBACK;
    } else {
	struct krb5_krbhst_info *hi;
	size_t hostlen = strlen(host);

	hi = calloc(1, sizeof(*hi) + hostlen);
	if(hi == NULL) {
	    free(host);
	    return ENOMEM;
	}

	hi->proto = proto;
	hi->port  = hi->def_port = port;
	hi->ai    = ai;
	memmove(hi->hostname, host, hostlen - 1);
	hi->hostname[hostlen - 1] = '\0';
	free(host);
	append_host_hostinfo(kd, hi);
	kd->fallback_count++;
    }
    return 0;
}

static krb5_error_code
kdc_get_next(krb5_context context,
	     struct krb5_krbhst_data *kd,
	     krb5_krbhst_info **host)
{
    krb5_error_code ret;

    if((kd->flags & KD_CONFIG) == 0) {
	config_get_hosts(context, kd, "kdc");
	kd->flags |= KD_CONFIG;
	if(get_next(kd, host))
	    return 0;
    }

    if (kd->flags & KD_CONFIG_EXISTS)
	return KRB5_KDC_UNREACH; /* XXX */

    if(context->srv_lookup) {
	if((kd->flags & KD_SRV_UDP) == 0) {
	    srv_get_hosts(context, kd, "udp", "kerberos");
	    kd->flags |= KD_SRV_UDP;
	    if(get_next(kd, host))
		return 0;
	}

	if((kd->flags & KD_SRV_TCP) == 0) {
	    srv_get_hosts(context, kd, "tcp", "kerberos");
	    kd->flags |= KD_SRV_TCP;
	    if(get_next(kd, host))
		return 0;
	}
	if((kd->flags & KD_SRV_HTTP) == 0) {
	    srv_get_hosts(context, kd, "http", "kerberos");
	    kd->flags |= KD_SRV_HTTP;
	    if(get_next(kd, host))
		return 0;
	}
    }

    while((kd->flags & KD_FALLBACK) == 0) {
	ret = fallback_get_hosts(context, kd, "kerberos",
				 kd->def_port, KRB5_KRBHST_UDP);
	if(ret)
	    return ret;
	if(get_next(kd, host))
	    return 0;
    }

    return KRB5_KDC_UNREACH; /* XXX */
}

static krb5_error_code
admin_get_next(krb5_context context,
	       struct krb5_krbhst_data *kd,
	       krb5_krbhst_info **host)
{
    krb5_error_code ret;

    if((kd->flags & KD_CONFIG) == 0) {
	config_get_hosts(context, kd, "admin_server");
	kd->flags |= KD_CONFIG;
	if(get_next(kd, host))
	    return 0;
    }

    if (kd->flags & KD_CONFIG_EXISTS)
	return KRB5_KDC_UNREACH; /* XXX */

    if(context->srv_lookup) {
	if((kd->flags & KD_SRV_TCP) == 0) {
	    srv_get_hosts(context, kd, "tcp", "kerberos-adm");
	    kd->flags |= KD_SRV_TCP;
	    if(get_next(kd, host))
		return 0;
	}
    }

    if (krbhst_empty(kd)
	&& (kd->flags & KD_FALLBACK) == 0) {
	ret = fallback_get_hosts(context, kd, "kerberos",
				 kd->def_port, KRB5_KRBHST_UDP);
	if(ret)
	    return ret;
	kd->flags |= KD_FALLBACK;
	if(get_next(kd, host))
	    return 0;
    }

    return KRB5_KDC_UNREACH;	/* XXX */
}

static krb5_error_code
kpasswd_get_next(krb5_context context,
		 struct krb5_krbhst_data *kd,
		 krb5_krbhst_info **host)
{
    krb5_error_code ret;

    if((kd->flags & KD_CONFIG) == 0) {
	config_get_hosts(context, kd, "kpasswd_server");
	if(get_next(kd, host))
	    return 0;
    }

    if (kd->flags & KD_CONFIG_EXISTS)
	return KRB5_KDC_UNREACH; /* XXX */

    if(context->srv_lookup) {
	if((kd->flags & KD_SRV_UDP) == 0) {
	    srv_get_hosts(context, kd, "udp", "kpasswd");
	    kd->flags |= KD_SRV_UDP;
	    if(get_next(kd, host))
		return 0;
	}
    }

    /* no matches -> try admin */

    if (krbhst_empty(kd)) {
	kd->flags = 0;
	kd->port  = kd->def_port;
	kd->get_next = admin_get_next;
	ret = (*kd->get_next)(context, kd, host);
	if (ret == 0)
	    (*host)->proto = KRB5_KRBHST_UDP;
	return ret;
    }

    return KRB5_KDC_UNREACH; /* XXX */
}

static krb5_error_code
krb524_get_next(krb5_context context,
		struct krb5_krbhst_data *kd,
		krb5_krbhst_info **host)
{
    if((kd->flags & KD_CONFIG) == 0) {
	config_get_hosts(context, kd, "krb524_server");
	if(get_next(kd, host))
	    return 0;
	kd->flags |= KD_CONFIG;
    }

    if (kd->flags & KD_CONFIG_EXISTS)
	return KRB5_KDC_UNREACH; /* XXX */

    if(context->srv_lookup) {
	if((kd->flags & KD_SRV_UDP) == 0) {
	    srv_get_hosts(context, kd, "udp", "krb524");
	    kd->flags |= KD_SRV_UDP;
	    if(get_next(kd, host))
		return 0;
	}

	if((kd->flags & KD_SRV_TCP) == 0) {
	    srv_get_hosts(context, kd, "tcp", "krb524");
	    kd->flags |= KD_SRV_TCP;
	    if(get_next(kd, host))
		return 0;
	}
    }

    /* no matches -> try kdc */

    if (krbhst_empty(kd)) {
	kd->flags = 0;
	kd->port  = kd->def_port;
	kd->get_next = kdc_get_next;
	return (*kd->get_next)(context, kd, host);
    }

    return KRB5_KDC_UNREACH; /* XXX */
}

static struct krb5_krbhst_data*
common_init(krb5_context context,
	    const char *realm)
{
    struct krb5_krbhst_data *kd;

    if((kd = calloc(1, sizeof(*kd))) == NULL)
	return NULL;

    if((kd->realm = strdup(realm)) == NULL) {
	free(kd);
	return NULL;
    }

    kd->end = kd->index = &kd->hosts;
    return kd;
}

/*
 * initialize `handle' to look for hosts of type `type' in realm `realm'
 */

krb5_error_code
krb5_krbhst_init(krb5_context context,
		 const char *realm,
		 unsigned int type,
		 krb5_krbhst_handle *handle)
{
    struct krb5_krbhst_data *kd;
    krb5_error_code (*get_next)(krb5_context, struct krb5_krbhst_data *, 
				krb5_krbhst_info **);
    int def_port;

    switch(type) {
    case KRB5_KRBHST_KDC:
	get_next = kdc_get_next;
	def_port = ntohs(krb5_getportbyname (context, "kerberos", "udp", 88));
	break;
    case KRB5_KRBHST_ADMIN:
	get_next = admin_get_next;
	def_port = ntohs(krb5_getportbyname (context, "kerberos-adm",
					     "tcp", 749));
	break;
    case KRB5_KRBHST_CHANGEPW:
	get_next = kpasswd_get_next;
	def_port = ntohs(krb5_getportbyname (context, "kpasswd", "udp",
					     KPASSWD_PORT));
	break;
    case KRB5_KRBHST_KRB524:
	get_next = krb524_get_next;
	def_port = ntohs(krb5_getportbyname (context, "krb524", "udp", 4444));
	break;
    default:
	krb5_set_error_string(context, "unknown krbhst type (%u)", type);
	return ENOTTY;
    }
    if((kd = common_init(context, realm)) == NULL)
	return ENOMEM;
    kd->get_next = get_next;
    kd->def_port = def_port;
    *handle = kd;
    return 0;
}

/*
 * return the next host information from `handle' in `host'
 */

krb5_error_code
krb5_krbhst_next(krb5_context context,
		 krb5_krbhst_handle handle,
		 krb5_krbhst_info **host)
{
    if(get_next(handle, host))
	return 0;

    return (*handle->get_next)(context, handle, host);
}

/*
 * return the next host information from `handle' as a host name
 * in `hostname' (or length `hostlen)
 */

krb5_error_code
krb5_krbhst_next_as_string(krb5_context context,
			   krb5_krbhst_handle handle,
			   char *hostname,
			   size_t hostlen)
{
    krb5_error_code ret;
    krb5_krbhst_info *host;
    ret = krb5_krbhst_next(context, handle, &host);
    if(ret)
	return ret;
    return krb5_krbhst_format_string(context, host, hostname, hostlen);
}


void
krb5_krbhst_reset(krb5_context context, krb5_krbhst_handle handle)
{
    handle->index = &handle->hosts;
}

void
krb5_krbhst_free(krb5_context context, krb5_krbhst_handle handle)
{
    krb5_krbhst_info *h, *next;

    if (handle == NULL)
	return;

    for (h = handle->hosts; h != NULL; h = next) {
	next = h->next;
	free_krbhst_info(h);
    }

    free(handle->realm);
    free(handle);
}

/* backwards compatibility ahead */

static krb5_error_code
gethostlist(krb5_context context, const char *realm, 
	    unsigned int type, char ***hostlist)
{
    krb5_error_code ret;
    int nhost = 0;
    krb5_krbhst_handle handle;
    char host[MAXHOSTNAMELEN];
    krb5_krbhst_info *hostinfo;

    ret = krb5_krbhst_init(context, realm, type, &handle);
    if (ret)
	return ret;

    while(krb5_krbhst_next(context, handle, &hostinfo) == 0)
	nhost++;
    if(nhost == 0)
	return KRB5_KDC_UNREACH;
    *hostlist = calloc(nhost + 1, sizeof(**hostlist));
    if(*hostlist == NULL) {
	krb5_krbhst_free(context, handle);
	return ENOMEM;
    }

    krb5_krbhst_reset(context, handle);
    nhost = 0;
    while(krb5_krbhst_next_as_string(context, handle, 
				     host, sizeof(host)) == 0) {
	if(((*hostlist)[nhost++] = strdup(host)) == NULL) {
	    krb5_free_krbhst(context, *hostlist);
	    krb5_krbhst_free(context, handle);
	    return ENOMEM;
	}
    }
    (*hostlist)[nhost++] = NULL;
    krb5_krbhst_free(context, handle);
    return 0;
}

/*
 * return an malloced list of kadmin-hosts for `realm' in `hostlist'
 */

krb5_error_code
krb5_get_krb_admin_hst (krb5_context context,
			const krb5_realm *realm,
			char ***hostlist)
{
    return gethostlist(context, *realm, KRB5_KRBHST_ADMIN, hostlist);
}

/*
 * return an malloced list of changepw-hosts for `realm' in `hostlist'
 */

krb5_error_code
krb5_get_krb_changepw_hst (krb5_context context,
			   const krb5_realm *realm,
			   char ***hostlist)
{
    return gethostlist(context, *realm, KRB5_KRBHST_CHANGEPW, hostlist);
}

/*
 * return an malloced list of 524-hosts for `realm' in `hostlist'
 */

krb5_error_code
krb5_get_krb524hst (krb5_context context,
		    const krb5_realm *realm,
		    char ***hostlist)
{
    return gethostlist(context, *realm, KRB5_KRBHST_KRB524, hostlist);
}


/*
 * return an malloced list of KDC's for `realm' in `hostlist'
 */

krb5_error_code
krb5_get_krbhst (krb5_context context,
		 const krb5_realm *realm,
		 char ***hostlist)
{
    return gethostlist(context, *realm, KRB5_KRBHST_KDC, hostlist);
}

/*
 * free all the memory allocated in `hostlist'
 */

krb5_error_code
krb5_free_krbhst (krb5_context context,
		  char **hostlist)
{
    char **p;

    for (p = hostlist; *p; ++p)
	free (*p);
    free (hostlist);
    return 0;
}
