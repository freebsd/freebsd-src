/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

RCSID("$Id: get_host.c,v 1.30 1997/05/02 14:29:13 assar Exp $");

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
parse_address(char *address, int *proto, char **host, int *port)
{
    char *p, *q;
    p = strchr(address, '/');
    *proto = IPPROTO_UDP;
    if(p){
	char prot[32];
	struct protoent *pp;
	strncpy(prot, address, p - address);
	prot[p - address] = 0;
	if((pp = getprotobyname(prot)))
	    *proto = pp->p_proto;
	else
	    krb_warning("Bad protocol name `%s', Using default `udp'.\n", 
			prot);
	p++;
    }else
	p = address;
    q = strchr(p, ':');
    if(q){
	*host = (char*)malloc(q - p + 1);
	strncpy(*host, p, q - p);
	(*host)[q - p] = 0;
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
    }else{
	*host = strdup(p);
	*port = krb_port;
    }
    return 0;
}

static int
add_host(char *realm, char *address, int admin, int validate)
{
    struct krb_host *host;
    struct host_list *p, **last = &hosts;
    host = (struct krb_host*)malloc(sizeof(struct krb_host));
    parse_address(address, &host->proto, &host->host, &host->port);
    if(validate && gethostbyname(host->host) == NULL){
	free(host->host);
	free(host);
	return 1;
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
    p = (struct host_list*)malloc(sizeof(struct host_list));
    p->this = host;
    p->next = NULL;
    *last = p;
    return 0;
}



static int
read_file(const char *filename, const char *r)
{
    char line[1024];
    char realm[1024];
    char address[1024];
    char scratch[1024];
    int n;
    int nhosts = 0;
    
    FILE *f = fopen(filename, "r");
    if(f == NULL)
	return -1;
    while(fgets(line, sizeof(line), f)){
	n = sscanf(line, "%s %s admin %s", realm, address, scratch);
	if(n == 2 || n == 3){
	    if(strcmp(realm, r))
		continue;
	    if(add_host(realm, address, n == 3, 0) == 0)
		nhosts++;
	}
    }
    fclose(f);
    return nhosts;
}

static int
init_hosts(char *realm)
{
    static const char *files[] = KRB_CNF_FILES;
    int i;
    char *dir = getenv("KRBCONFDIR");

    krb_port = ntohs(k_getportbyname (KRB_SERVICE, NULL, htons(KRB_PORT)));
    if(dir){
	char file[MaxPathLen];
	if(k_concat(file, sizeof(file), dir, "/krb.conf", NULL) == 0)
	    read_file(file, realm);
    }
    for(i = 0; files[i]; i++)
	read_file(files[i], realm);
    return 0;
}

static void
srv_find_realm(char *realm, char *proto, char *service)
{
    char *domain;
    struct dns_reply *r;
    struct resource_record *rr;
    
    k_mconcat(&domain, 1024, service, ".", proto, ".", realm, ".", NULL);
    
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
krb_get_host(int nth, char *realm, int admin)
{
    struct host_list *p;
    static char orealm[REALM_SZ];
    if(orealm[0] == 0 || strcmp(realm, orealm)){
	/* quick optimization */
	if(realm && realm[0]){
	    strncpy(orealm, realm, sizeof(orealm) - 1);
	    orealm[sizeof(orealm) - 1] = 0;
	}else{
	    int ret = krb_get_lrealm(orealm, 1);
	    if(ret != KSUCCESS)
		return NULL;
	}
	
	if(hosts){
	    free_hosts(hosts);
	    hosts = NULL;
	}
	
	init_hosts(orealm);
    
	srv_find_realm(orealm, "udp", KRB_SERVICE);
	srv_find_realm(orealm, "tcp", KRB_SERVICE);
	
	{
	    /* XXX this assumes no one has more than 99999 kerberos
	       servers */
	    char host[REALM_SZ + sizeof("kerberos-XXXXX..")];
	    int i = 0;
	    sprintf(host, "kerberos.%s.", orealm);
	    add_host(orealm, host, 1, 1);
	    do{
		i++;
		sprintf(host, "kerberos-%d.%s.", i, orealm);
	    }while(i < 100000 && add_host(orealm, host, 0, 1) == 0);
	}
    }
    
    for(p = hosts; p; p = p->next){
	if(strcmp(orealm, p->this->realm) == 0 &&
	   (!admin || p->this->admin))
	    if(nth == 1)
		return p->this;
	    else
		nth--;
    }
    return NULL;
}

int
krb_get_krbhst(char *host, char *realm, int nth)
{
    struct krb_host *p = krb_get_host(nth, realm, 0);
    if(p == NULL)
	return KFAILURE;
    strcpy(host, p->host);
    return KSUCCESS;
}

int
krb_get_admhst(char *host, char *realm, int nth)
{
    struct krb_host *p = krb_get_host(nth, realm, 1);
    if(p == NULL)
	return KFAILURE;
    strcpy(host, p->host);
    return KSUCCESS;
}
