/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: dns_pr.c,v 1.15 2001/05/29 05:48:31 marka Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>

#include <isc/memcluster.h>
#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "hesiod.h"
#include "dns_p.h"

/* Types. */

struct pvt {
	struct dns_p *		dns;
	struct protoent		proto;
	char *			prbuf;
};

/* Forward. */

static void 			pr_close(struct irs_pr *);
static struct protoent *	pr_byname(struct irs_pr *, const char *);
static struct protoent *	pr_bynumber(struct irs_pr *, int);
static struct protoent *	pr_next(struct irs_pr *);
static void			pr_rewind(struct irs_pr *);
static void			pr_minimize(struct irs_pr *);
static struct __res_state *	pr_res_get(struct irs_pr *);
static void			pr_res_set(struct irs_pr *,
					   struct __res_state *,
					   void (*)(void *));

static struct protoent *	parse_hes_list(struct irs_pr *, char **);

/* Public. */

struct irs_pr *
irs_dns_pr(struct irs_acc *this) {
	struct dns_p *dns = (struct dns_p *)this->private;
	struct pvt *pvt;
	struct irs_pr *pr;

	if (!dns->hes_ctx) {
		errno = ENODEV;
		return (NULL);
	}
	if (!(pvt = memget(sizeof *pvt))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	if (!(pr = memget(sizeof *pr))) {
		memput(pvt, sizeof *pvt);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pr, 0x5e, sizeof *pr);
	pvt->dns = dns;
	pr->private = pvt;
	pr->byname = pr_byname;
	pr->bynumber = pr_bynumber;
	pr->next = pr_next;
	pr->rewind = pr_rewind;
	pr->close = pr_close;
	pr->minimize = pr_minimize;
	pr->res_get = pr_res_get;
	pr->res_set = pr_res_set;
	return (pr);
}

/* Methods. */

static void
pr_close(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->proto.p_aliases)
		free(pvt->proto.p_aliases);
	if (pvt->prbuf)
		free(pvt->prbuf);

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct protoent *
pr_byname(struct irs_pr *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;
	struct protoent *proto;
	char **hes_list;

	if (!(hes_list = hesiod_resolve(dns->hes_ctx, name, "protocol")))
		return (NULL);

	proto = parse_hes_list(this, hes_list);
	hesiod_free_list(dns->hes_ctx, hes_list);
	return (proto);
}

static struct protoent *
pr_bynumber(struct irs_pr *this, int num) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;
	struct protoent *proto;
	char numstr[16];
	char **hes_list;

	sprintf(numstr, "%d", num);
	if (!(hes_list = hesiod_resolve(dns->hes_ctx, numstr, "protonum")))
		return (NULL);
	
	proto = parse_hes_list(this, hes_list);
	hesiod_free_list(dns->hes_ctx, hes_list);
	return (proto);
}

static struct protoent *
pr_next(struct irs_pr *this) {
	UNUSED(this);
	errno = ENODEV;
	return (NULL);
}

static void
pr_rewind(struct irs_pr *this) {
	UNUSED(this);
	/* NOOP */
}

static void
pr_minimize(struct irs_pr *this) {
	UNUSED(this);
	/* NOOP */
}

static struct __res_state *
pr_res_get(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;

	return (__hesiod_res_get(dns->hes_ctx));
}

static void
pr_res_set(struct irs_pr *this, struct __res_state * res,
	   void (*free_res)(void *)) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;

	__hesiod_res_set(dns->hes_ctx, res, free_res);
}

/* Private. */

static struct protoent *
parse_hes_list(struct irs_pr *this, char **hes_list) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *p, *cp, **cpp, **new;
	int num = 0;
	int max = 0;
	
	for (cpp = hes_list; *cpp; cpp++) {
		cp = *cpp;

		/* Strip away comments, if any. */
		if ((p = strchr(cp, '#')))
			*p = 0;

		/* Skip blank lines. */
		p = cp;
		while (*p && !isspace((unsigned char)*p))
			p++;
		if (!*p)
			continue;

		/* OK, we've got a live one.  Let's parse it for real. */
		if (pvt->prbuf)
			free(pvt->prbuf);
		pvt->prbuf = strdup(cp);

		p = pvt->prbuf;
		pvt->proto.p_name = p;
		while (*p && !isspace((unsigned char)*p))
			p++;
		if (!*p)
			continue;
		*p++ = '\0';

		pvt->proto.p_proto = atoi(p);
		while (*p && !isspace((unsigned char)*p))
			p++;
		if (*p)
			*p++ = '\0';

		while (*p) {
			if ((num + 1) >= max || !pvt->proto.p_aliases) {
				max += 10;
				new = realloc(pvt->proto.p_aliases,
					      max * sizeof(char *));
				if (!new) {
					errno = ENOMEM;
					goto cleanup;
				}
				pvt->proto.p_aliases = new;
			}
			pvt->proto.p_aliases[num++] = p;
			while (*p && !isspace((unsigned char)*p))
				p++;
			if (*p)
				*p++ = '\0';
		}
		if (!pvt->proto.p_aliases)
			pvt->proto.p_aliases = malloc(sizeof(char *));
		if (!pvt->proto.p_aliases)
			goto cleanup;
		pvt->proto.p_aliases[num] = NULL;
		return (&pvt->proto);
	}
	
 cleanup:
	if (pvt->proto.p_aliases) {
		free(pvt->proto.p_aliases);
		pvt->proto.p_aliases = NULL;
	}
	if (pvt->prbuf) {
		free(pvt->prbuf);
		pvt->prbuf = NULL;
	}
	return (NULL);
}
