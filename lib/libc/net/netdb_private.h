/*-
 * Copyright (C) 2005 The FreeBSD Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NETDB_PRIVATE_H_
#define _NETDB_PRIVATE_H_

#include <stdio.h>				/* XXX: for BUFSIZ */

#define	PROTOENT_MAXALIASES	35
#define	SERVENT_MAXALIASES	35

struct protoent_data {
	FILE *fp;
	char *aliases[PROTOENT_MAXALIASES];
	int stayopen;
	char line[BUFSIZ + 1];
};

struct protodata {
	struct protoent proto;
	struct protoent_data data;
};

struct servent_data {
	FILE *fp;
	char *aliases[SERVENT_MAXALIASES];
	int stayopen;
	char line[BUFSIZ + 1];
#ifdef YP
	int yp_stepping;
	char *yp_name;
	char *yp_proto;
	int yp_port;
	char *yp_domain;
	char *yp_key;
	int yp_keylen;
#endif
};

struct servdata {
	struct servent serv;
	struct servent_data data;
};

#define	endprotoent_r		__endprotoent_r
#define	endservent_r		__endservent_r
#define	getprotobyname_r	__getprotobyname_r
#define	getprotobynumber_r	__getprotobynumber_r
#define	getprotoent_r		__getprotoent_r
#define	getservbyname_r		__getservbyname_r
#define	getservbyport_r		__getservbyport_r
#define	getservent_r		__getservent_r
#define	setprotoent_r		__setprotoent_r
#define	setservent_r		__setservent_r

struct protodata *__protodata_init(void);
struct servdata *__servdata_init(void);
void endprotoent_r(struct protoent_data *);
void endservent_r(struct servent_data *);
int getprotobyname_r(const char *, struct protoent *, struct protoent_data *);
int getprotobynumber_r(int, struct protoent *, struct protoent_data *);
int getprotoent_r(struct protoent *, struct protoent_data *);
int getservbyname_r(const char *, const char *, struct servent *,
	struct servent_data *);
int getservbyport_r(int, const char *, struct servent *,
	struct servent_data *);
int getservent_r(struct servent *, struct servent_data *);
void setprotoent_r(int, struct protoent_data *);
void setservent_r(int, struct servent_data *);

#endif /* _NETDB_PRIVATE_H_ */
