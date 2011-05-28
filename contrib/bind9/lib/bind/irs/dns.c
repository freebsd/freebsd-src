/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-1999 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: dns.c,v 1.3.18.2 2006-03-10 00:20:08 marka Exp $";
#endif

/*! \file
 * \brief
 * dns.c --- this is the top-level accessor function for the dns
 */

#include "port_before.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <resolv.h>

#include <isc/memcluster.h>
#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "hesiod.h"
#include "dns_p.h"

/* forward */

static void		dns_close(struct irs_acc *);
static struct __res_state *	dns_res_get(struct irs_acc *);
static void		dns_res_set(struct irs_acc *, struct __res_state *,
				void (*)(void *));

/* public */

struct irs_acc *
irs_dns_acc(const char *options) {
	struct irs_acc *acc;
	struct dns_p *dns;

	UNUSED(options);

	if (!(acc = memget(sizeof *acc))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(acc, 0x5e, sizeof *acc);
	if (!(dns = memget(sizeof *dns))) {
		errno = ENOMEM;
		memput(acc, sizeof *acc);
		return (NULL);
	}
	memset(dns, 0x5e, sizeof *dns);
	dns->res = NULL;
	dns->free_res = NULL;
	if (hesiod_init(&dns->hes_ctx) < 0) {
		/*
		 * We allow the dns accessor class to initialize
		 * despite hesiod failing to initialize correctly,
		 * since dns host queries don't depend on hesiod.
		 */
		dns->hes_ctx = NULL;
	}
	acc->private = dns;
#ifdef WANT_IRS_GR
	acc->gr_map = irs_dns_gr;
#else
	acc->gr_map = NULL;
#endif
#ifdef WANT_IRS_PW
	acc->pw_map = irs_dns_pw;
#else
	acc->pw_map = NULL;
#endif
	acc->sv_map = irs_dns_sv;
	acc->pr_map = irs_dns_pr;
	acc->ho_map = irs_dns_ho;
	acc->nw_map = irs_dns_nw;
	acc->ng_map = irs_nul_ng;
	acc->res_get = dns_res_get;
	acc->res_set = dns_res_set;
	acc->close = dns_close;
	return (acc);
}

/* methods */
static struct __res_state *
dns_res_get(struct irs_acc *this) {
	struct dns_p *dns = (struct dns_p *)this->private;

	if (dns->res == NULL) {
		struct __res_state *res;
		res = (struct __res_state *)malloc(sizeof *res);
		if (res == NULL)
			return (NULL);
		memset(res, 0, sizeof *res);
		dns_res_set(this, res, free);
	}

	if ((dns->res->options & RES_INIT) == 0U &&
	    res_ninit(dns->res) < 0)
		return (NULL);

	return (dns->res);
}

static void
dns_res_set(struct irs_acc *this, struct __res_state *res,
	    void (*free_res)(void *)) {
	struct dns_p *dns = (struct dns_p *)this->private;

	if (dns->res && dns->free_res) {
		res_nclose(dns->res);
		(*dns->free_res)(dns->res);
	}
	dns->res = res;
	dns->free_res = free_res;
}

static void
dns_close(struct irs_acc *this) {
	struct dns_p *dns;

	dns = (struct dns_p *)this->private;
	if (dns->res && dns->free_res)
		(*dns->free_res)(dns->res);
	if (dns->hes_ctx)
		hesiod_end(dns->hes_ctx);
	memput(dns, sizeof *dns);
	memput(this, sizeof *this);
}

