/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

#include "krb_locl.h"

RCSID("$Id: get_host.c,v 1.48 1999/12/02 16:58:41 joda Exp $");

static struct host_list {
    struct krb_host *this;
    struct host_list *next;
} *hosts;

static int krb_port = 0;

static void
free_hosts(struct host_list *h)
{
    struct host_list *t;
    while(h){
	if(h->this->realm)
	    free(h->this->realm);
	if(h->this->host)
	    free(h->this->host);
	t = h;
	h = h->next;
	free(t);
    }
}

static int
parse_address(char *address, enum krb_host_proto *proto,
	      char **host, int *port)
{
    char *p, *q;
    int default_port = krb_port;
    *proto = PROTO_UDP;
    if(strncmp(address, "http://", 7) == 0){
	p = address + 7;
	*proto = PROTO_HTTP;
	default_port = 80;
    }else{
	p = strchr(address, '/');
	if(p){
	    char prot[32];
	    strlcpy (prot, address,
			     min(p - address + 1, sizeof(prot)));
	    if(strcasecmp(prot, "udp") == 0) 
		*proto = PROTO_UDP;
	    else if(strcasecmp(prot, "tcp") == 0)
		*proto = PROTO_TCP;
	    else if(strcasecmp(prot, "http") == 0) {
		*proto = PROTO_HTTP;
		default_port = 80;
	    } else
		krb_warning("Unknown protocol `%s', Using default `udp'.\n", 
			    prot);
	    p++;
	}else
	    p = address;
    }
    q = strchr(p, ':');
    if(q) {
	*host = malloc(q - p + 1);
	if (*host == NULL)
	    return -1;
	strlcpy (*host, p, q - p + 1);
	q++;
	{
	    struct servent *sp = getservbyname(q, NULL);
	    if(sp)
		*port = ntohs(sp->s_port);
	    else
		if(sscanf(q, "%d", port) != 1){
		    krb_warning("Bad port specification `%s', using port %d.", 
				q, krb_port);
		    *port = krb_port;
		}
	}
    } else {
	*port = default_port;
	q = strchr(p, '/');
	if (q) {
	    *host = malloc(q - p + 1);
	    if (*host == NULL)
		return -1;
	    strlcpy (*host, p, q - p + 1);
	} else {
	    *host = strdup(p);
	    if(*host == NULL)
		return -1;
	}
    }
    return 0;
}

static int
add_host(const char *realm, char *address, int admin, int validate)
{
    struct krb_host *host;
    struct host_list *p, **last = &hosts;

    host = (struct krb_host*)malloc(sizeof(struct krb_host));
    if (host == NULL)
	return 1;
    if(parse_address(address, &host->proto, &host->host, &host->port) < 0) {
	free(host);
	return 1;
    }
    if (validate) {
	if (krb_dns_debug)
	    krb_warning("Getting host entry for %s...", host->host);
	if (gethostbyname(host->host) == NULL) {
	    if (krb_dns_debug)
		krb_warning("Didn't get it.\n");
	    free(host->host);
	    free(host);
	    return 1;
	}
	else if (krb_dns_debug)
	    krb_warning("Got it.\n");
    }
    host->admin = admin;
    for(p = hosts; p; p = p->next){
	if(strcmp(realm, p->this->realm) == 0 &&
	   strcmp(host->host, p->this->host) == 0 && 
	   host->proto == p->this->proto &&
	   host->port == p->this->port){
	    free(host->host);
	    free(host);
	    return 1;
	}
	last = &p->next;
    }
    host->realm = strdup(realm);
    if (host->realm == NULL) {
	free(host->host);
	free(host);
	return 1;
    }
    p = (struct host_list*)malloc(sizeof(struct host_list));
    if (p == NULL) {
	free(host->realm);
	free(host->host);
	free(host);
	return 1;
    }
    p->this = host;
    p->next = NULL;
    *last = p;
    return 0;
}

static int
read_file(const char *filename, const char *r)
{
    char line[1024];
    int nhosts = 0;
    FILE *f = fopen(filename, "r");

    if(f == NULL)
	return -1;
    while(fgets(line, sizeof(line), f) != NULL) {
	char *realm, *address, *admin;
	char *save;
	
	realm = strtok_r (line, " \t\n\r", &save);
	if (realm == NULL)
	    continue;
	if (strcmp(realm, r))
	    continue;
	address = strtok_r (NULL, " \t\n\r", &save);
	if (address == NULL)
	    continue;
	admin = strtok_r (NULL, " \t\n\r", &save);
	if (add_host(realm,
		     address,
		     admin != NULL && strcasecmp(admin, "admin") == 0,
		     0) == 0)
	    ++nhosts;
    }
    fclose(f);
    return nhosts;
}

#if 0
static int
read_cellservdb (const char *filename, const char *realm)
{
    char line[1024];
    FILE *f = fopen (filename, "r");
    int nhosts = 0;

    if (f == NULL)
	return -1;
    while (fgets (line, sizeof(line), f) != NULL) {
	if (line[0] == '>'
	    && strncasecmp (line + 1, realm, strlen(realm)) == 0) {
	    while (fgets (line, sizeof(line), f) != NULL && *line != '>') {
		char *hash;

		if (line [strlen(line) - 1] == '\n')
		    line [strlen(line) - 1] = '\0';

		hash = strchr (line, '#');

		if (hash != NULL
		    && add_host (realm, hash + 1, 0, 0) == 0)
		    ++nhosts;
	    }
	    break;
	}
    }
    fclose (f);
    return nhosts;
}
#endif

static int
init_hosts(char *realm)
{
    int i, j, ret = 0;
    char file[MaxPathLen];
    
    /*
     * proto should really be NULL, but there are libraries out there
     * that don't like that so we use "udp" instead.
     */

    krb_port = ntohs(k_getportbyname (KRB_SERVICE, "udp", htons(KRB_PORT)));
    for(i = 0; krb_get_krbconf(i, file, sizeof(file)) == 0; i++) {
      j = read_file(file, realm);
      if (j > 0) ret += j;
    }
    return ret;
}

static void
srv_find_realm(char *realm, char *proto, char *service)
{
    char *domain;
    struct dns_reply *r;
    struct resource_record *rr;
    
    roken_mconcat(&domain, 1024, service, ".", proto, ".", realm, ".", NULL);
    
    if(domain == NULL)
	return;
    
    r = dns_lookup(domain, "srv");
    if(r == NULL)
	r = dns_lookup(domain, "txt");
    if(r == NULL){
	free(domain);
	return;
    }
    for(rr = r->head; rr; rr = rr->next){
	if(rr->type == T_SRV){
	    char buf[1024];

	    if (snprintf (buf,
			  sizeof(buf),
			  "%s/%s:%u",
			  proto,
			  rr->u.srv->target,
			  rr->u.srv->port) < sizeof(buf))
		add_host(realm, buf, 0, 0);
	}else if(rr->type == T_TXT)
	    add_host(realm, rr->u.txt, 0, 0);
    }
    dns_free_data(r);
    free(domain);
}

struct krb_host*
krb_get_host(int nth, const char *realm, int admin)
{
    struct host_list *p;
    static char orealm[REALM_SZ];

    if(orealm[0] == 0 || strcmp(realm, orealm)){
	/* quick optimization */
	if(realm && realm[0]){
	    strlcpy (orealm, realm, sizeof(orealm));
	}else{
	    int ret = krb_get_lrealm(orealm, 1);
	    if(ret != KSUCCESS)
		return NULL;
	}
	
	if(hosts){
	    free_hosts(hosts);
	    hosts = NULL;
	}
	
	if (init_hosts(orealm) < nth) {
	  srv_find_realm(orealm, "udp", KRB_SERVICE);
	  srv_find_realm(orealm, "tcp", KRB_SERVICE);
	  srv_find_realm(orealm, "http", KRB_SERVICE);
	
	  {
	    char *host;
	    int i = 0;

	    asprintf(&host, "kerberos.%s.", orealm);
	    if (host == NULL) {
		free_hosts(hosts);
		hosts = NULL;
		return NULL;
	    }
	    add_host(orealm, host, 1, 1);
	    do {
		i++;
		free(host);
		asprintf(&host, "kerberos-%d.%s.", i, orealm);
	    } while(host != NULL
		   && i < 100000
		   && add_host(orealm, host, 0, 1) == 0);
	    free(host);
	  }
	}
#if 0
	read_cellservdb ("/usr/vice/etc/CellServDB", orealm);
	read_cellservdb ("/usr/arla/etc/CellServDB", orealm);
#endif
    }
    
    for(p = hosts; p; p = p->next){
	if(strcmp(orealm, p->this->realm) == 0 &&
	   (!admin || p->this->admin)) {
	    if(nth == 1)
		return p->this;
	    else
		nth--;
	}
    }
    return NULL;
}

int
krb_get_krbhst(char *host, char *realm, int nth)
{
    struct krb_host *p = krb_get_host(nth, realm, 0);
    if(p == NULL)
	return KFAILURE;
    strlcpy (host, p->host, MaxHostNameLen);
    return KSUCCESS;
}

int
krb_get_admhst(char *host, char *realm, int nth)
{
    struct krb_host *p = krb_get_host(nth, realm, 1);
    if(p == NULL)
	return KFAILURE;
    strlcpy (host, p->host, MaxHostNameLen);
    return KSUCCESS;
}
