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
static const char rcsid[] = "$Id: nis.c,v 1.10 1998/03/21 00:59:50 halley Exp $";
#endif

/* Imports */

#include "port_before.h"

#ifdef WANT_IRS_NIS

#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "hesiod.h"
#include "nis_p.h"

/* Forward */

static void		nis_close(struct irs_acc *);

/* Public */

struct irs_acc *
irs_nis_acc(const char *options) {
	struct nis_p *nis;
	struct irs_acc *acc;
	char *domain;

	if (yp_get_default_domain(&domain) != 0)
		return (NULL);
	if (!(nis = malloc(sizeof *nis))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(nis, 0, sizeof *nis);
	if (!(acc = malloc(sizeof *acc))) {
		free(nis);
		errno = ENOMEM;
		return (NULL);
	}
	memset(acc, 0x5e, sizeof *acc);
	acc->private = nis;
	nis->domain = strdup(domain);
#ifdef WANT_IRS_GR
	acc->gr_map = irs_nis_gr;
#else
	acc->gr_map = NULL;
#endif
#ifdef WANT_IRS_PW
	acc->pw_map = irs_nis_pw;
#else
	acc->pw_map = NULL;
#endif
	acc->sv_map = irs_nis_sv;
	acc->pr_map = irs_nis_pr;
	acc->ho_map = irs_nis_ho;
	acc->nw_map = irs_nis_nw;
	acc->ng_map = irs_nis_ng;
	acc->close = nis_close;
	return (acc);
}

/* Methods */

static void
nis_close(struct irs_acc *this) {
	struct nis_p *nis = (struct nis_p *)this->private;

	free(nis->domain);
	free(nis);
	free(this);
}

#endif /*WANT_IRS_NIS*/
