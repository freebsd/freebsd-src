/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: dns_sv.c,v 1.4.18.1 2005/04/27 05:00:55 sra Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <isc/memcluster.h>
#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "hesiod.h"
#include "dns_p.h"

/* Definitions */

struct pvt {
	struct dns_p *		dns;
	struct servent		serv;
	char *			svbuf;
	struct __res_state *	res;
	void			(*free_res)(void *);
};

/* Forward. */

static void 			sv_close(struct irs_sv *);
static struct servent *		sv_byname(struct irs_sv *,
					  const char *, const char *);
static struct servent *		sv_byport(struct irs_sv *, int, const char *);
static struct servent *		sv_next(struct irs_sv *);
static void			sv_rewind(struct irs_sv *);
static void			sv_minimize(struct irs_sv *);
#ifdef SV_RES_SETGET
static struct __res_state *	sv_res_get(struct irs_sv *);
static void			sv_res_set(struct irs_sv *,
					   struct __res_state *,
					   void (*)(void *));
#endif

static struct servent *		parse_hes_list(struct irs_sv *,
					       char **, const char *);

/* Public */

struct irs_sv *
irs_dns_sv(struct irs_acc *this) {
	struct dns_p *dns = (struct dns_p *)this->private;
	struct irs_sv *sv;
	struct pvt *pvt;

	if (!dns || !dns->hes_ctx) {
		errno = ENODEV;
		return (NULL);
	}
	if (!(pvt = memget(sizeof *pvt))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->dns = dns;
	if (!(sv = memget(sizeof *sv))) {
		memput(pvt, sizeof *pvt);
		errno = ENOMEM;
		return (NULL);
	}
	memset(sv, 0x5e, sizeof *sv);
	sv->private = pvt;
	sv->byname = sv_byname;
	sv->byport = sv_byport;
	sv->next = sv_next;
	sv->rewind = sv_rewind;
	sv->close = sv_close;
	sv->minimize = sv_minimize;
#ifdef SV_RES_SETGET
	sv->res_get = sv_res_get;
	sv->res_set = sv_res_set;
#else
	sv->res_get = NULL; /*%< sv_res_get; */
	sv->res_set = NULL; /*%< sv_res_set; */
#endif
	return (sv);
}

/* Methods */

static void
sv_close(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->serv.s_aliases)
		free(pvt->serv.s_aliases);
	if (pvt->svbuf)
		free(pvt->svbuf);

	if (pvt->res && pvt->free_res)
		(*pvt->free_res)(pvt->res);
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct servent *
sv_byname(struct irs_sv *this, const char *name, const char *proto) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;
	struct servent *s;
	char **hes_list;

	if (!(hes_list = hesiod_resolve(dns->hes_ctx, name, "service")))
		return (NULL);

	s = parse_hes_list(this, hes_list, proto);
	hesiod_free_list(dns->hes_ctx, hes_list);
	return (s);
}

static struct servent *
sv_byport(struct irs_sv *this, int port, const char *proto) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;
	struct servent *s;
	char portstr[16];
	char **hes_list;

	sprintf(portstr, "%d", ntohs(port));
	if (!(hes_list = hesiod_resolve(dns->hes_ctx, portstr, "port")))
		return (NULL);
	
	s = parse_hes_list(this, hes_list, proto);
	hesiod_free_list(dns->hes_ctx, hes_list);
	return (s);
}

static struct servent *
sv_next(struct irs_sv *this) {
	UNUSED(this);
	errno = ENODEV;
	return (NULL);
}

static void
sv_rewind(struct irs_sv *this) {
	UNUSED(this);
	/* NOOP */
}

/* Private */

static struct servent *
parse_hes_list(struct irs_sv *this, char **hes_list, const char *proto) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *p, *cp, **cpp, **new;
	int proto_len;
	int num = 0;
	int max = 0;
	
	for (cpp = hes_list; *cpp; cpp++) {
		cp = *cpp;

		/* Strip away comments, if any. */
		if ((p = strchr(cp, '#')))
			*p = 0;

		/* Check to make sure the protocol matches. */
		p = cp;
		while (*p && !isspace((unsigned char)*p))
			p++;
		if (!*p)
			continue;
		if (proto) {
		     proto_len = strlen(proto);
		     if (strncasecmp(++p, proto, proto_len) != 0)
			  continue;
		     if (p[proto_len] && !isspace(p[proto_len]&0xff))
			  continue;
		}
		/* OK, we've got a live one.  Let's parse it for real. */
		if (pvt->svbuf)
			free(pvt->svbuf);
		pvt->svbuf = strdup(cp);

		p = pvt->svbuf;
		pvt->serv.s_name = p;
		while (*p && !isspace(*p&0xff))
			p++;
		if (!*p)
			continue;
		*p++ = '\0';

		pvt->serv.s_proto = p;
		while (*p && !isspace(*p&0xff))
			p++;
		if (!*p)
			continue;
		*p++ = '\0';

		pvt->serv.s_port = htons((u_short) atoi(p));
		while (*p && !isspace(*p&0xff))
			p++;
		if (*p)
			*p++ = '\0';

		while (*p) {
			if ((num + 1) >= max || !pvt->serv.s_aliases) {
				max += 10;
				new = realloc(pvt->serv.s_aliases,
					      max * sizeof(char *));
				if (!new) {
					errno = ENOMEM;
					goto cleanup;
				}
				pvt->serv.s_aliases = new;
			}
			pvt->serv.s_aliases[num++] = p;
			while (*p && !isspace(*p&0xff))
				p++;
			if (*p)
				*p++ = '\0';
		}
		if (!pvt->serv.s_aliases)
			pvt->serv.s_aliases = malloc(sizeof(char *));
		if (!pvt->serv.s_aliases)
			goto cleanup;
		pvt->serv.s_aliases[num] = NULL;
		return (&pvt->serv);
	}
	
 cleanup:
	if (pvt->serv.s_aliases) {
		free(pvt->serv.s_aliases);
		pvt->serv.s_aliases = NULL;
	}
	if (pvt->svbuf) {
		free(pvt->svbuf);
		pvt->svbuf = NULL;
	}
	return (NULL);
}

static void
sv_minimize(struct irs_sv *this) {
	UNUSED(this);
	/* NOOP */
}

#ifdef SV_RES_SETGET
static struct __res_state *
sv_res_get(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;

	return (__hesiod_res_get(dns->hes_ctx));
}

static void
sv_res_set(struct irs_sv *this, struct __res_state * res,
	   void (*free_res)(void *)) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;

	__hesiod_res_set(dns->hes_ctx, res, free_res);
}
#endif

/*! \file */
