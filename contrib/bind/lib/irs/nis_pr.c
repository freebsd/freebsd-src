/*
 * Copyright (c) 1996 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: nis_pr.c,v 1.9 1997/12/04 04:58:00 halley Exp $";
#endif

/* Imports */

#include "port_before.h"

#ifndef WANT_IRS_NIS
static int __bind_irs_nis_unneeded;
#else

#include <sys/types.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "nis_p.h"

/* Definitions */

struct pvt {
	int		needrewind;
	char *		nis_domain;
	char *		curkey_data;
	int		curkey_len;
	char *		curval_data;
	int		curval_len;
	struct protoent	proto;
	char *		prbuf;
};

enum do_what { do_none = 0x0, do_key = 0x1, do_val = 0x2, do_all = 0x3 };

static /*const*/ char protocols_byname[] =	"protocols.byname";
static /*const*/ char protocols_bynumber[] =	"protocols.bynumber";

/* Forward */

static void 			pr_close(struct irs_pr *);
static struct protoent *	pr_byname(struct irs_pr *, const char *);
static struct protoent *	pr_bynumber(struct irs_pr *, int);
static struct protoent *	pr_next(struct irs_pr *);
static void			pr_rewind(struct irs_pr *);
static void			pr_minimize(struct irs_pr *);

static struct protoent *	makeprotoent(struct irs_pr *this);
static void			nisfree(struct pvt *, enum do_what);

/* Public */

struct irs_pr *
irs_nis_pr(struct irs_acc *this) {
	struct irs_pr *pr;
	struct pvt *pvt;

	if (!(pr = malloc(sizeof *pr))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pr, 0x5e, sizeof *pr);
	if (!(pvt = malloc(sizeof *pvt))) {
		free(pr);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->needrewind = 1;
	pvt->nis_domain = ((struct nis_p *)this->private)->domain;
	pr->private = pvt;
	pr->byname = pr_byname;
	pr->bynumber = pr_bynumber;
	pr->next = pr_next;
	pr->rewind = pr_rewind;
	pr->close = pr_close;
	pr->minimize = pr_minimize;
	return (pr);
}

/* Methods. */

static void
pr_close(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	nisfree(pvt, do_all);
	if (pvt->proto.p_aliases)
		free(pvt->proto.p_aliases);
	if (pvt->prbuf)
		free(pvt->prbuf);
	free(pvt);
	free(this);
}

static struct protoent *
pr_byname(struct irs_pr *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	int r;
	
	nisfree(pvt, do_val);
	r = yp_match(pvt->nis_domain, protocols_byname, (char *)name,
		     strlen(name), &pvt->curval_data, &pvt->curval_len);
	if (r != 0) {
		errno = ENOENT;
		return (NULL);
	}
	return (makeprotoent(this));
}

static struct protoent *
pr_bynumber(struct irs_pr *this, int num) {
	struct pvt *pvt = (struct pvt *)this->private;
	char tmp[sizeof "-4294967295"];
	int r;
	
	nisfree(pvt, do_val);
	(void) sprintf(tmp, "%d", num);
	r = yp_match(pvt->nis_domain, protocols_bynumber, tmp, strlen(tmp),
		     &pvt->curval_data, &pvt->curval_len);
	if (r != 0) {
		errno = ENOENT;
		return (NULL);
	}
	return (makeprotoent(this));
}

static struct protoent *
pr_next(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct protoent *rval;
	int r;

	do {
		if (pvt->needrewind) {
			nisfree(pvt, do_all);
			r = yp_first(pvt->nis_domain, protocols_bynumber,
				     &pvt->curkey_data, &pvt->curkey_len,
				     &pvt->curval_data, &pvt->curval_len);
			pvt->needrewind = 0;
		} else {
			char *newkey_data;
			int newkey_len;

			nisfree(pvt, do_val);
			r = yp_next(pvt->nis_domain, protocols_bynumber,
				    pvt->curkey_data, pvt->curkey_len,
				    &newkey_data, &newkey_len,
				    &pvt->curval_data, &pvt->curval_len);
			nisfree(pvt, do_key);
			pvt->curkey_data = newkey_data;
			pvt->curkey_len = newkey_len;
		}
		if (r != 0) {
			h_errno = HOST_NOT_FOUND;
			return (NULL);
		}
		rval = makeprotoent(this);
	} while (rval == NULL);
	return (rval);
}

static void
pr_rewind(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	pvt->needrewind = 1;
}

static void
pr_minimize(struct irs_pr *this) {
	/* NOOP */
}

/* Private */

static struct protoent *
makeprotoent(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *p, **t;
	int n, m;

	if (pvt->prbuf)
		free(pvt->prbuf);
	pvt->prbuf = pvt->curval_data;
	pvt->curval_data = NULL;

	for (p = pvt->prbuf; *p && *p != '#'; p++)
		NULL;
	while (p > pvt->prbuf && isspace(p[-1]))
		p--;
	*p = '\0';

	p = pvt->prbuf;
	n = m = 0;

	pvt->proto.p_name = p;
	while (*p && !isspace(*p))
		p++;
	if (!*p)
		return (NULL);
	*p++ = '\0';

	while (*p && isspace(*p))
		p++;
	pvt->proto.p_proto = atoi(p);
	while (*p && !isspace(*p))
		p++;
	*p++ = '\0';

	while (*p) {
		if ((n + 1) >= m || !pvt->proto.p_aliases) {
			m += 10;
			t = realloc(pvt->proto.p_aliases,
				      m * sizeof(char *));
			if (!t) {
				errno = ENOMEM;
				goto cleanup;
			}
			pvt->proto.p_aliases = t;
		}
		pvt->proto.p_aliases[n++] = p;
		while (*p && !isspace(*p))
			p++;
		if (*p)
			*p++ = '\0';
	}
	if (!pvt->proto.p_aliases)
		pvt->proto.p_aliases = malloc(sizeof(char *));
	if (!pvt->proto.p_aliases)
		goto cleanup;
	pvt->proto.p_aliases[n] = NULL;
	return (&pvt->proto);
	
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

static void
nisfree(struct pvt *pvt, enum do_what do_what) {
	if ((do_what & do_key) && pvt->curkey_data) {
		free(pvt->curkey_data);
		pvt->curkey_data = NULL;
	}
	if ((do_what & do_val) && pvt->curval_data) {
		free(pvt->curval_data);
		pvt->curval_data = NULL;
	}
}

#endif /*WANT_IRS_NIS*/
