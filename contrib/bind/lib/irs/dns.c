/*
 * Copyright (c) 1996, 1998 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: dns.c,v 1.11 1998/03/21 00:59:45 halley Exp $";
#endif

/*
 * dns.c --- this is the top-level accessor function for the dns
 */

#include "port_before.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "hesiod.h"
#include "dns_p.h"

/* forward */

static void		dns_close(struct irs_acc *);

/* public */

struct irs_acc *
irs_dns_acc(const char *options) {
	struct irs_acc *acc;
	struct dns_p *dns;

	if (!(acc = malloc(sizeof *acc))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(acc, 0x5e, sizeof *acc);
	if (!(dns = malloc(sizeof *dns))) {
		errno = ENOMEM;
		free(acc);
		return (NULL);
	}
	memset(dns, 0x5e, sizeof *dns);
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
	acc->close = dns_close;
	return (acc);
}

/* methods */

static void
dns_close(struct irs_acc *this) {
	struct dns_p *dns;

	dns = (struct dns_p *)this->private;
	if (dns->hes_ctx)
		hesiod_end(dns->hes_ctx);
	free(dns);
	free(this);
}

