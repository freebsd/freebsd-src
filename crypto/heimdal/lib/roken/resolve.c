/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "roken.h"
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
#include "resolve.h"

#include <assert.h>

RCSID("$Id: resolve.c,v 1.36.4.1 2002/10/21 14:48:15 joda Exp $");

#undef HAVE_RES_NSEARCH
#if (defined(HAVE_RES_SEARCH) || defined(HAVE_RES_NSEARCH)) && defined(HAVE_DN_EXPAND)

#define DECL(X) {#X, T_##X}

static struct stot{
    const char *name;
    int type;
}stot[] = {
    DECL(A),
    DECL(NS),
    DECL(CNAME),
    DECL(SOA),
    DECL(PTR),
    DECL(MX),
    DECL(TXT),
    DECL(AFSDB),
    DECL(SIG),
    DECL(KEY),
    DECL(SRV),
    DECL(NAPTR),
    {NULL, 	0}
};

int _resolve_debug = 0;

int
dns_string_to_type(const char *name)
{
    struct stot *p = stot;
    for(p = stot; p->name; p++)
	if(strcasecmp(name, p->name) == 0)
	    return p->type;
    return -1;
}

const char *
dns_type_to_string(int type)
{
    struct stot *p = stot;
    for(p = stot; p->name; p++)
	if(type == p->type)
	    return p->name;
    return NULL;
}

void
dns_free_data(struct dns_reply *r)
{
    struct resource_record *rr;
    if(r->q.domain)
	free(r->q.domain);
    for(rr = r->head; rr;){
	struct resource_record *tmp = rr;
	if(rr->domain)
	    free(rr->domain);
	if(rr->u.data)
	    free(rr->u.data);
	rr = rr->next;
	free(tmp);
    }
    free (r);
}

static int
parse_record(const unsigned char *data, const unsigned char *end_data, 
	     const unsigned char **pp, struct resource_record **rr)
{
    int type, class, ttl, size;
    int status;
    char host[MAXDNAME];
    const unsigned char *p = *pp;
    status = dn_expand(data, end_data, p, host, sizeof(host));
    if(status < 0) 
	return -1;
    if (p + status + 10 > end_data)
	return -1;
    p += status;
    type = (p[0] << 8) | p[1];
    p += 2;
    class = (p[0] << 8) | p[1];
    p += 2;
    ttl = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    p += 4;
    size = (p[0] << 8) | p[1];
    p += 2;

    if (p + size > end_data)
	return -1;

    *rr = calloc(1, sizeof(**rr));
    if(*rr == NULL) 
	return -1;
    (*rr)->domain = strdup(host);
    if((*rr)->domain == NULL) {
	free(*rr);
	return -1;
    }
    (*rr)->type = type;
    (*rr)->class = class;
    (*rr)->ttl = ttl;
    (*rr)->size = size;
    switch(type){
    case T_NS:
    case T_CNAME:
    case T_PTR:
	status = dn_expand(data, end_data, p, host, sizeof(host));
	if(status < 0) {
	    free(*rr);
	    return -1;
	}
	(*rr)->u.txt = strdup(host);
	if((*rr)->u.txt == NULL) {
	    free(*rr);
	    return -1;
	}
	break;
    case T_MX:
    case T_AFSDB:{
	status = dn_expand(data, end_data, p + 2, host, sizeof(host));
	if(status < 0){
	    free(*rr);
	    return -1;
	}
	if (status + 2 > size) {
	    free(*rr);
	    return -1;
	}

	(*rr)->u.mx = (struct mx_record*)malloc(sizeof(struct mx_record) + 
						strlen(host));
	if((*rr)->u.mx == NULL) {
	    free(*rr);
	    return -1;
	}
	(*rr)->u.mx->preference = (p[0] << 8) | p[1];
	strcpy((*rr)->u.mx->domain, host);
	break;
    }
    case T_SRV:{
	status = dn_expand(data, end_data, p + 6, host, sizeof(host));
	if(status < 0){
	    free(*rr);
	    return -1;
	}
	if (status + 6 > size) {
	    free(*rr);
	    return -1;
	}

	(*rr)->u.srv = 
	    (struct srv_record*)malloc(sizeof(struct srv_record) + 
				       strlen(host));
	if((*rr)->u.srv == NULL) {
	    free(*rr);
	    return -1;
	}
	(*rr)->u.srv->priority = (p[0] << 8) | p[1];
	(*rr)->u.srv->weight = (p[2] << 8) | p[3];
	(*rr)->u.srv->port = (p[4] << 8) | p[5];
	strcpy((*rr)->u.srv->target, host);
	break;
    }
    case T_TXT:{
	if(size == 0 || size < *p + 1) {
	    free(*rr);
	    return -1;
	}
	(*rr)->u.txt = (char*)malloc(*p + 1);
	if((*rr)->u.txt == NULL) {
	    free(*rr);
	    return -1;
	}
	strncpy((*rr)->u.txt, (char*)p + 1, *p);
	(*rr)->u.txt[*p] = '\0';
	break;
    }
    case T_KEY : {
	size_t key_len;

	if (size < 4) {
	    free(*rr);
	    return -1;
	}

	key_len = size - 4;
	(*rr)->u.key = malloc (sizeof(*(*rr)->u.key) + key_len - 1);
	if ((*rr)->u.key == NULL) {
	    free(*rr);
	    return -1;
	}

	(*rr)->u.key->flags     = (p[0] << 8) | p[1];
	(*rr)->u.key->protocol  = p[2];
	(*rr)->u.key->algorithm = p[3];
	(*rr)->u.key->key_len   = key_len;
	memcpy ((*rr)->u.key->key_data, p + 4, key_len);
	break;
    }
    case T_SIG : {
	size_t sig_len;

	if(size <= 18) {
	    free(*rr);
	    return -1;
	}
	status = dn_expand (data, end_data, p + 18, host, sizeof(host));
	if (status < 0) {
	    free(*rr);
	    return -1;
	}
	if (status + 18 > size) {
	    free(*rr);
	    return -1;
	}

	/* the signer name is placed after the sig_data, to make it
           easy to free this struture; the size calculation below
           includes the zero-termination if the structure itself.
	   don't you just love C?
	*/
	sig_len = size - 18 - status;
	(*rr)->u.sig = malloc(sizeof(*(*rr)->u.sig)
			      + strlen(host) + sig_len);
	if ((*rr)->u.sig == NULL) {
	    free(*rr);
	    return -1;
	}
	(*rr)->u.sig->type           = (p[0] << 8) | p[1];
	(*rr)->u.sig->algorithm      = p[2];
	(*rr)->u.sig->labels         = p[3];
	(*rr)->u.sig->orig_ttl       = (p[4] << 24) | (p[5] << 16)
	    | (p[6] << 8) | p[7];
	(*rr)->u.sig->sig_expiration = (p[8] << 24) | (p[9] << 16)
	    | (p[10] << 8) | p[11];
	(*rr)->u.sig->sig_inception  = (p[12] << 24) | (p[13] << 16)
	    | (p[14] << 8) | p[15];
	(*rr)->u.sig->key_tag        = (p[16] << 8) | p[17];
	(*rr)->u.sig->sig_len        = sig_len;
	memcpy ((*rr)->u.sig->sig_data, p + 18 + status, sig_len);
	(*rr)->u.sig->signer         = &(*rr)->u.sig->sig_data[sig_len];
	strcpy((*rr)->u.sig->signer, host);
	break;
    }

    case T_CERT : {
	size_t cert_len;

	if (size < 5) {
	    free(*rr);
	    return -1;
	}

	cert_len = size - 5;
	(*rr)->u.cert = malloc (sizeof(*(*rr)->u.cert) + cert_len - 1);
	if ((*rr)->u.cert == NULL) {
	    free(*rr);
	    return -1;
	}

	(*rr)->u.cert->type      = (p[0] << 8) | p[1];
	(*rr)->u.cert->tag       = (p[2] << 8) | p[3];
	(*rr)->u.cert->algorithm = p[4];
	(*rr)->u.cert->cert_len  = cert_len;
	memcpy ((*rr)->u.cert->cert_data, p + 5, cert_len);
	break;
    }
    default:
	(*rr)->u.data = (unsigned char*)malloc(size);
	if(size != 0 && (*rr)->u.data == NULL) {
	    free(*rr);
	    return -1;
	}
	memcpy((*rr)->u.data, p, size);
    }
    *pp = p + size;
    return 0;
}

#ifndef TEST_RESOLVE
static
#endif
struct dns_reply*
parse_reply(const unsigned char *data, size_t len)
{
    const unsigned char *p;
    int status;
    int i;
    char host[MAXDNAME];
    const unsigned char *end_data = data + len;
    struct dns_reply *r;
    struct resource_record **rr;
    
    r = calloc(1, sizeof(*r));
    if (r == NULL)
	return NULL;

    p = data;
#if 0
    /* doesn't work on Crays */
    memcpy(&r->h, p, sizeof(HEADER));
    p += sizeof(HEADER);
#else
    memcpy(&r->h, p, 12); /* XXX this will probably be mostly garbage */
    p += 12;
#endif
    if(ntohs(r->h.qdcount) != 1) {
	free(r);
	return NULL;
    }
    status = dn_expand(data, end_data, p, host, sizeof(host));
    if(status < 0){
	dns_free_data(r);
	return NULL;
    }
    r->q.domain = strdup(host);
    if(r->q.domain == NULL) {
	dns_free_data(r);
	return NULL;
    }
    if (p + status + 4 > end_data) {
	dns_free_data(r);
	return NULL;
    }
    p += status;
    r->q.type = (p[0] << 8 | p[1]);
    p += 2;
    r->q.class = (p[0] << 8 | p[1]);
    p += 2;
    
    rr = &r->head;
    for(i = 0; i < ntohs(r->h.ancount); i++) {
	if(parse_record(data, end_data, &p, rr) != 0) {
	    dns_free_data(r);
	    return NULL;
	}
	rr = &(*rr)->next;
    }
    for(i = 0; i < ntohs(r->h.nscount); i++) {
	if(parse_record(data, end_data, &p, rr) != 0) {
	    dns_free_data(r);
	    return NULL;
	}
	rr = &(*rr)->next;
    }
    for(i = 0; i < ntohs(r->h.arcount); i++) {
	if(parse_record(data, end_data, &p, rr) != 0) {
	    dns_free_data(r);
	    return NULL;
	}
	rr = &(*rr)->next;
    }
    *rr = NULL;
    return r;
}

static struct dns_reply *
dns_lookup_int(const char *domain, int rr_class, int rr_type)
{
    unsigned char reply[1024];
    int len;
#ifdef HAVE_RES_NSEARCH
    struct __res_state stat;
    memset(&stat, 0, sizeof(stat));
    if(res_ninit(&stat))
	return NULL; /* is this the best we can do? */
#elif defined(HAVE__RES)
    u_long old_options = 0;
#endif
    
    if (_resolve_debug) {
#ifdef HAVE_RES_NSEARCH
	stat.options |= RES_DEBUG;
#elif defined(HAVE__RES)
        old_options = _res.options;
	_res.options |= RES_DEBUG;
#endif
	fprintf(stderr, "dns_lookup(%s, %d, %s)\n", domain,
		rr_class, dns_type_to_string(rr_type));
    }
#ifdef HAVE_RES_NSEARCH
    len = res_nsearch(&stat, domain, rr_class, rr_type, reply, sizeof(reply));
#else
    len = res_search(domain, rr_class, rr_type, reply, sizeof(reply));
#endif
    if (_resolve_debug) {
#if defined(HAVE__RES) && !defined(HAVE_RES_NSEARCH)
        _res.options = old_options;
#endif
	fprintf(stderr, "dns_lookup(%s, %d, %s) --> %d\n",
		domain, rr_class, dns_type_to_string(rr_type), len);
    }
#ifdef HAVE_RES_NSEARCH
    res_nclose(&stat);
#endif    
    if(len < 0) {
	return NULL;
    } else {
	len = min(len, sizeof(reply));
	return parse_reply(reply, len);
    }
}

struct dns_reply *
dns_lookup(const char *domain, const char *type_name)
{
    int type;
    
    type = dns_string_to_type(type_name);
    if(type == -1) {
	if(_resolve_debug)
	    fprintf(stderr, "dns_lookup: unknown resource type: `%s'\n", 
		    type_name);
	return NULL;
    }
    return dns_lookup_int(domain, C_IN, type);
}

static int
compare_srv(const void *a, const void *b)
{
    const struct resource_record *const* aa = a, *const* bb = b;

    if((*aa)->u.srv->priority == (*bb)->u.srv->priority)
	return ((*aa)->u.srv->weight - (*bb)->u.srv->weight);
    return ((*aa)->u.srv->priority - (*bb)->u.srv->priority);
}

#ifndef HAVE_RANDOM
#define random() rand()
#endif

/* try to rearrange the srv-records by the algorithm in RFC2782 */
void
dns_srv_order(struct dns_reply *r)
{
    struct resource_record **srvs, **ss, **headp;
    struct resource_record *rr;
    int num_srv = 0;

#if defined(HAVE_INITSTATE) && defined(HAVE_SETSTATE)
    int state[256 / sizeof(int)];
    char *oldstate;
#endif

    for(rr = r->head; rr; rr = rr->next) 
	if(rr->type == T_SRV)
	    num_srv++;

    if(num_srv == 0)
	return;

    srvs = malloc(num_srv * sizeof(*srvs));
    if(srvs == NULL)
	return; /* XXX not much to do here */
    
    /* unlink all srv-records from the linked list and put them in
       a vector */
    for(ss = srvs, headp = &r->head; *headp; )
	if((*headp)->type == T_SRV) {
	    *ss = *headp;
	    *headp = (*headp)->next;
	    (*ss)->next = NULL;
	    ss++;
	} else
	    headp = &(*headp)->next;
    
    /* sort them by priority and weight */
    qsort(srvs, num_srv, sizeof(*srvs), compare_srv);

#if defined(HAVE_INITSTATE) && defined(HAVE_SETSTATE)
    oldstate = initstate(time(NULL), (char*)state, sizeof(state));
#endif

    headp = &r->head;
    
    for(ss = srvs; ss < srvs + num_srv; ) {
	int sum, rnd, count;
	struct resource_record **ee, **tt;
	/* find the last record with the same priority and count the
           sum of all weights */
	for(sum = 0, tt = ss; tt < srvs + num_srv; tt++) {
	    if(*tt == NULL)
		continue;
	    if((*tt)->u.srv->priority != (*ss)->u.srv->priority)
		break;
	    sum += (*tt)->u.srv->weight;
	}
	ee = tt;
	/* ss is now the first record of this priority and ee is the
           first of the next */
	while(ss < ee) {
	    rnd = random() % (sum + 1);
	    for(count = 0, tt = ss; ; tt++) {
		if(*tt == NULL)
		    continue;
		count += (*tt)->u.srv->weight;
		if(count >= rnd)
		    break;
	    }

	    assert(tt < ee);

	    /* insert the selected record at the tail (of the head) of
               the list */
	    (*tt)->next = *headp;
	    *headp = *tt;
	    headp = &(*tt)->next;
	    sum -= (*tt)->u.srv->weight;
	    *tt = NULL;
	    while(ss < ee && *ss == NULL)
		ss++;
	}
    }
    
#if defined(HAVE_INITSTATE) && defined(HAVE_SETSTATE)
    setstate(oldstate);
#endif
    free(srvs);
    return;
}

#else /* NOT defined(HAVE_RES_SEARCH) && defined(HAVE_DN_EXPAND) */

struct dns_reply *
dns_lookup(const char *domain, const char *type_name)
{
    return NULL;
}

void
dns_free_data(struct dns_reply *r)
{
}

void
dns_srv_order(struct dns_reply *r)
{
}

#endif

#ifdef TEST
int 
main(int argc, char **argv)
{
    struct dns_reply *r;
    struct resource_record *rr;
    r = dns_lookup(argv[1], argv[2]);
    if(r == NULL){
	printf("No reply.\n");
	return 1;
    }
    if(r->q.type == T_SRV)
	dns_srv_order(r);

    for(rr = r->head; rr;rr=rr->next){
	printf("%-30s %-5s %-6d ", rr->domain, dns_type_to_string(rr->type), rr->ttl);
	switch(rr->type){
	case T_NS:
	case T_CNAME:
	case T_PTR:
	    printf("%s\n", (char*)rr->u.data);
	    break;
	case T_A:
	    printf("%s\n", inet_ntoa(*rr->u.a));
	    break;
	case T_MX:
	case T_AFSDB:{
	    printf("%d %s\n", rr->u.mx->preference, rr->u.mx->domain);
	    break;
	}
	case T_SRV:{
	    struct srv_record *srv = rr->u.srv;
	    printf("%d %d %d %s\n", srv->priority, srv->weight, 
		   srv->port, srv->target);
	    break;
	}
	case T_TXT: {
	    printf("%s\n", rr->u.txt);
	    break;
	}
	case T_SIG : {
	    struct sig_record *sig = rr->u.sig;
	    const char *type_string = dns_type_to_string (sig->type);

	    printf ("type %u (%s), algorithm %u, labels %u, orig_ttl %u, sig_expiration %u, sig_inception %u, key_tag %u, signer %s\n",
		    sig->type, type_string ? type_string : "",
		    sig->algorithm, sig->labels, sig->orig_ttl,
		    sig->sig_expiration, sig->sig_inception, sig->key_tag,
		    sig->signer);
	    break;
	}
	case T_KEY : {
	    struct key_record *key = rr->u.key;

	    printf ("flags %u, protocol %u, algorithm %u\n",
		    key->flags, key->protocol, key->algorithm);
	    break;
	}
	default:
	    printf("\n");
	    break;
	}
    }
    
    return 0;
}
#endif
