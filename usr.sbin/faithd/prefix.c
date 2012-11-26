/*	$KAME: prefix.c,v 1.13 2003/09/02 22:50:17 itojun Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (C) 2000 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>

#ifndef offsetof
#define	offsetof(type, member)	((size_t)(u_long)(&((type *)0)->member))
#endif

#include "faithd.h"
#include "prefix.h"

static int prefix_set(const char *, struct prefix *, int);
static struct config *config_load1(const char *);
#if 0
static void config_show1(const struct config *);
static void config_show(void);
#endif

struct config *config_list = NULL;
const int niflags = NI_NUMERICHOST;

static int
prefix_set(const char *s, struct prefix *prefix, int slash)
{
	char *p = NULL, *q, *r;
	struct addrinfo hints, *res = NULL;
	int max;

	p = strdup(s);
	if (!p)
		goto fail;
	q = strchr(p, '/');
	if (q) {
		if (!slash)
			goto fail;
		*q++ = '\0';
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(p, "0", &hints, &res))
		goto fail;
	if (res->ai_next || res->ai_addrlen > sizeof(prefix->a))
		goto fail;
	memcpy(&prefix->a, res->ai_addr, res->ai_addrlen);

	switch (prefix->a.ss_family) {
	case AF_INET:
		max = 32;
		break;
	case AF_INET6:
		max = 128;
		break;
	default:
		max = -1;
		break;
	}

	if (q) {
		r = NULL;
		prefix->l = (int)strtoul(q, &r, 10);
		if (!*q || *r)
			goto fail;
		if (prefix->l < 0 || prefix->l > max)
			goto fail;
	} else
		prefix->l = max;

	if (p)
		free(p);
	if (res)
		freeaddrinfo(res);
	return 0;

fail:
	if (p)
		free(p);
	if (res)
		freeaddrinfo(res);
	return -1;
}

const char *
prefix_string(const struct prefix *prefix)
{
	static char buf[NI_MAXHOST + 20];
	char hbuf[NI_MAXHOST];

	if (getnameinfo((const struct sockaddr *)&prefix->a, prefix->a.ss_len,
	    hbuf, sizeof(hbuf), NULL, 0, niflags))
		return NULL;
	snprintf(buf, sizeof(buf), "%s/%d", hbuf, prefix->l);
	return buf;
}

int
prefix_match(const struct prefix *prefix, const struct sockaddr *sa)
{
	struct sockaddr_storage a, b;
	char *pa, *pb;
	int off, l;

	if (prefix->a.ss_family != sa->sa_family ||
	    prefix->a.ss_len != sa->sa_len)
		return 0;

	if (prefix->a.ss_len > sizeof(a) || sa->sa_len > sizeof(b))
		return 0;

	switch (prefix->a.ss_family) {
	case AF_INET:
		off = offsetof(struct sockaddr_in, sin_addr);
		break;
	case AF_INET6:
		off = offsetof(struct sockaddr_in6, sin6_addr);
		break;
	default:
		if (memcmp(&prefix->a, sa, prefix->a.ss_len) != 0)
			return 0;
		else
			return 1;
	}

	memcpy(&a, &prefix->a, prefix->a.ss_len);
	memcpy(&b, sa, sa->sa_len);
	l = prefix->l / 8 + (prefix->l % 8 ? 1 : 0);

	/* overrun check */
	if (off + l > a.ss_len)
		return 0;

	pa = ((char *)&a) + off;
	pb = ((char *)&b) + off;
	if (prefix->l % 8) {
		pa[prefix->l / 8] &= 0xff00 >> (prefix->l % 8);
		pb[prefix->l / 8] &= 0xff00 >> (prefix->l % 8);
	}
	if (memcmp(pa, pb, l) != 0)
		return 0;
	else
		return 1;
}

/*
 * prefix/prefixlen permit/deny prefix/prefixlen [srcaddr]
 * 3ffe::/16 permit 10.0.0.0/8 10.1.1.1
 */
static struct config *
config_load1(const char *line)
{
	struct config *conf;
	char buf[BUFSIZ];
	char *p;
	char *token[4];
	int i;

	if (strlen(line) + 1 > sizeof(buf))
		return NULL;
	strlcpy(buf, line, sizeof(buf));

	p = strchr(buf, '\n');
	if (!p)
		return NULL;
	*p = '\0';
	p = strchr(buf, '#');
	if (p)
		*p = '\0';
	if (strlen(buf) == 0)
		return NULL;

	p = buf;
	memset(token, 0, sizeof(token));
	for (i = 0; i < sizeof(token) / sizeof(token[0]); i++) {
		token[i] = strtok(p, "\t ");
		p = NULL;
		if (token[i] == NULL)
			break;
	}
	/* extra tokens? */
	if (strtok(p, "\t ") != NULL)
		return NULL;
	/* insufficient tokens */
	switch (i) {
	case 3:
	case 4:
		break;
	default:
		return NULL;
	}

	conf = (struct config *)malloc(sizeof(*conf));
	if (conf == NULL)
		return NULL;
	memset(conf, 0, sizeof(*conf));

	if (strcasecmp(token[1], "permit") == 0)
		conf->permit = 1;
	else if (strcasecmp(token[1], "deny") == 0)
		conf->permit = 0;
	else {
		/* invalid keyword is considered as "deny" */
		conf->permit = 0;
	}

	if (prefix_set(token[0], &conf->match, 1) < 0)
		goto fail;
	if (prefix_set(token[2], &conf->dest, 1) < 0)
		goto fail;
	if (token[3]) {
		if (prefix_set(token[3], &conf->src, 0) < 0)
			goto fail;
	}

	return conf;

fail:
	free(conf);
	return NULL;
}

int
config_load(const char *configfile)
{
	FILE *fp;
	char buf[BUFSIZ];
	struct config *conf, *p;
	struct config sentinel;

	config_list = NULL;

	if (!configfile)
		configfile = _PATH_PREFIX_CONF;
	fp = fopen(configfile, "r");
	if (fp == NULL)
		return -1;

	p = &sentinel;
	sentinel.next = NULL;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		conf = config_load1(buf);
		if (conf) {
			p->next = conf;
			p = p->next;
		}
	}
	config_list = sentinel.next;

	fclose(fp);
	return 0;
}

#if 0
static void
config_show1(const struct config *conf)
{
	const char *p;

	p = prefix_string(&conf->match);
	printf("%s", p ? p : "?");

	if (conf->permit)
		printf(" permit");
	else
		printf(" deny");

	p = prefix_string(&conf->dest);
	printf(" %s", p ? p : "?");

	printf("\n");
}

static void
config_show()
{
	struct config *conf;

	for (conf = config_list; conf; conf = conf->next)
		config_show1(conf);
}
#endif

const struct config *
config_match(struct sockaddr *sa1, struct sockaddr *sa2)
{
	static struct config conf;
	const struct config *p;

	if (sa1->sa_len > sizeof(conf.match.a) ||
	    sa2->sa_len > sizeof(conf.dest.a))
		return NULL;

	memset(&conf, 0, sizeof(conf));
	if (!config_list) {
		conf.permit = 1;
		memcpy(&conf.match.a, sa1, sa1->sa_len);
		memcpy(&conf.dest.a, sa2, sa2->sa_len);
		return &conf;
	}

	for (p = config_list; p; p = p->next)
		if (prefix_match(&p->match, sa1) && prefix_match(&p->dest, sa2))
			return p;

	return NULL;
}
