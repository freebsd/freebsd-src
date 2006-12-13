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
static const char rcsid[] = "$Id: gen_ho.c,v 1.1.206.3 2006/03/10 00:17:21 marka Exp $";
#endif /* LIBC_SCCS and not lint */

/* Imports */

#include "port_before.h"

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>

#include <isc/memcluster.h>
#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "gen_p.h"

/* Definitions */

struct pvt {
	struct irs_rule *	rules;
	struct irs_rule *	rule;
	struct irs_ho *		ho;
	struct __res_state *	res;
	void			(*free_res)(void *);
};

/* Forwards */

static void		ho_close(struct irs_ho *this);
static struct hostent *	ho_byname(struct irs_ho *this, const char *name);
static struct hostent *	ho_byname2(struct irs_ho *this, const char *name,
				   int af);
static struct hostent *	ho_byaddr(struct irs_ho *this, const void *addr,
				  int len, int af);
static struct hostent *	ho_next(struct irs_ho *this);
static void		ho_rewind(struct irs_ho *this);
static void		ho_minimize(struct irs_ho *this);
static struct __res_state * ho_res_get(struct irs_ho *this);
static void		ho_res_set(struct irs_ho *this,
				   struct __res_state *res,
				   void (*free_res)(void *));
static struct addrinfo * ho_addrinfo(struct irs_ho *this, const char *name,
				     const struct addrinfo *pai);

static int		init(struct irs_ho *this);

/* Exports */

struct irs_ho *
irs_gen_ho(struct irs_acc *this) {
	struct gen_p *accpvt = (struct gen_p *)this->private;
	struct irs_ho *ho;
	struct pvt *pvt;

	if (!(pvt = memget(sizeof *pvt))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	if (!(ho = memget(sizeof *ho))) {
		memput(pvt, sizeof *pvt);
		errno = ENOMEM;
		return (NULL);
	}
	memset(ho, 0x5e, sizeof *ho);
	pvt->rules = accpvt->map_rules[irs_ho];
	pvt->rule = pvt->rules;
	ho->private = pvt;
	ho->close = ho_close;
	ho->byname = ho_byname;
	ho->byname2 = ho_byname2;
	ho->byaddr = ho_byaddr;
	ho->next = ho_next;
	ho->rewind = ho_rewind;
	ho->minimize = ho_minimize;
	ho->res_get = ho_res_get;
	ho->res_set = ho_res_set;
	ho->addrinfo = ho_addrinfo;
	return (ho);
}

/* Methods. */

static void
ho_close(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	ho_minimize(this);
	if (pvt->res && pvt->free_res)
		(*pvt->free_res)(pvt->res);
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct hostent *
ho_byname(struct irs_ho *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct hostent *rval;
	struct irs_ho *ho;
	int therrno = NETDB_INTERNAL;
	int softerror = 0;

	if (init(this) == -1)
		return (NULL);

	for (rule = pvt->rules; rule; rule = rule->next) {
		ho = rule->inst->ho;
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		errno = 0;
		rval = (*ho->byname)(ho, name);
		if (rval != NULL)
			return (rval);
		if (softerror == 0 &&
		    pvt->res->res_h_errno != HOST_NOT_FOUND &&
		    pvt->res->res_h_errno != NETDB_INTERNAL) {
			softerror = 1;
			therrno = pvt->res->res_h_errno;
		}
		if (rule->flags & IRS_CONTINUE)
			continue;
		/*
		 * The value TRY_AGAIN can mean that the service
		 * is not available, or just that this particular name
		 * cannot be resolved now.  We use the errno ECONNREFUSED
		 * to distinguish.  If a lookup sets that errno when
		 * H_ERRNO is TRY_AGAIN, we continue to try other lookup
		 * functions, otherwise we return the TRY_AGAIN error.
		 */
		if (pvt->res->res_h_errno != TRY_AGAIN || errno != ECONNREFUSED)
			break;
	}
	if (softerror != 0 && pvt->res->res_h_errno == HOST_NOT_FOUND)
		RES_SET_H_ERRNO(pvt->res, therrno);
	return (NULL);
}

static struct hostent *
ho_byname2(struct irs_ho *this, const char *name, int af) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct hostent *rval;
	struct irs_ho *ho;
	int therrno = NETDB_INTERNAL;
	int softerror = 0;

	if (init(this) == -1)
		return (NULL);

	for (rule = pvt->rules; rule; rule = rule->next) {
		ho = rule->inst->ho;
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		errno = 0;
		rval = (*ho->byname2)(ho, name, af);
		if (rval != NULL)
			return (rval);
		if (softerror == 0 &&
		    pvt->res->res_h_errno != HOST_NOT_FOUND &&
		    pvt->res->res_h_errno != NETDB_INTERNAL) {
			softerror = 1;
			therrno = pvt->res->res_h_errno;
		}
		if (rule->flags & IRS_CONTINUE)
			continue;
		/*
		 * See the comments in ho_byname() explaining
		 * the interpretation of TRY_AGAIN and ECONNREFUSED.
		 */
		if (pvt->res->res_h_errno != TRY_AGAIN || errno != ECONNREFUSED)
			break;
	}
	if (softerror != 0 && pvt->res->res_h_errno == HOST_NOT_FOUND)
		RES_SET_H_ERRNO(pvt->res, therrno);
	return (NULL);
}

static struct hostent *
ho_byaddr(struct irs_ho *this, const void *addr, int len, int af) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct hostent *rval;
	struct irs_ho *ho;
	int therrno = NETDB_INTERNAL;
	int softerror = 0;


	if (init(this) == -1)
		return (NULL);

	for (rule = pvt->rules; rule; rule = rule->next) {
		ho = rule->inst->ho;
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		errno = 0;
		rval = (*ho->byaddr)(ho, addr, len, af);
		if (rval != NULL)
			return (rval);
		if (softerror == 0 &&
		    pvt->res->res_h_errno != HOST_NOT_FOUND &&
		    pvt->res->res_h_errno != NETDB_INTERNAL) {
			softerror = 1;
			therrno = pvt->res->res_h_errno;
		}

		if (rule->flags & IRS_CONTINUE)
			continue;
		/*
		 * See the comments in ho_byname() explaining
		 * the interpretation of TRY_AGAIN and ECONNREFUSED.
		 */
		if (pvt->res->res_h_errno != TRY_AGAIN || errno != ECONNREFUSED)
			break;
	}
	if (softerror != 0 && pvt->res->res_h_errno == HOST_NOT_FOUND)
		RES_SET_H_ERRNO(pvt->res, therrno);
	return (NULL);
}

static struct hostent *
ho_next(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct hostent *rval;
	struct irs_ho *ho;

	while (pvt->rule) {
		ho = pvt->rule->inst->ho;
		rval = (*ho->next)(ho);
		if (rval)
			return (rval);
		if (!(pvt->rule->flags & IRS_CONTINUE))
			break;
		pvt->rule = pvt->rule->next;
		if (pvt->rule) {
			ho = pvt->rule->inst->ho;
			(*ho->rewind)(ho);
		}
	}
	return (NULL);
}

static void
ho_rewind(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_ho *ho;

	pvt->rule = pvt->rules;
	if (pvt->rule) {
		ho = pvt->rule->inst->ho;
		(*ho->rewind)(ho);
	}
}

static void
ho_minimize(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;

	if (pvt->res)
		res_nclose(pvt->res);
	for (rule = pvt->rules; rule != NULL; rule = rule->next) {
		struct irs_ho *ho = rule->inst->ho;

		(*ho->minimize)(ho);
	}
}

static struct __res_state *
ho_res_get(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (!pvt->res) {
		struct __res_state *res;
		res = (struct __res_state *)malloc(sizeof *res);
		if (!res) {
			errno = ENOMEM;
			return (NULL);
		}
		memset(res, 0, sizeof *res);
		ho_res_set(this, res, free);
	}

	return (pvt->res);
}

static void
ho_res_set(struct irs_ho *this, struct __res_state *res,
		void (*free_res)(void *)) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;

	if (pvt->res && pvt->free_res) {
		res_nclose(pvt->res);
		(*pvt->free_res)(pvt->res);
	}

	pvt->res = res;
	pvt->free_res = free_res;

	for (rule = pvt->rules; rule != NULL; rule = rule->next) {
		struct irs_ho *ho = rule->inst->ho;

		(*ho->res_set)(ho, pvt->res, NULL);
	}
}

static struct addrinfo *
ho_addrinfo(struct irs_ho *this, const char *name, const struct addrinfo *pai)
{
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct addrinfo *rval = NULL;
	struct irs_ho *ho;
	int therrno = NETDB_INTERNAL;
	int softerror = 0;

	if (init(this) == -1)
		return (NULL);

	for (rule = pvt->rules; rule; rule = rule->next) {
		ho = rule->inst->ho;
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		errno = 0;
		if (ho->addrinfo == NULL) /* for safety */
			continue;
		rval = (*ho->addrinfo)(ho, name, pai);
		if (rval != NULL)
			return (rval);
		if (softerror == 0 &&
		    pvt->res->res_h_errno != HOST_NOT_FOUND &&
		    pvt->res->res_h_errno != NETDB_INTERNAL) {
			softerror = 1;
			therrno = pvt->res->res_h_errno;
		}
		if (rule->flags & IRS_CONTINUE)
			continue;
		/*
		 * See the comments in ho_byname() explaining
		 * the interpretation of TRY_AGAIN and ECONNREFUSED.
		 */
		if (pvt->res->res_h_errno != TRY_AGAIN ||
		    errno != ECONNREFUSED)
			break;
	}
	if (softerror != 0 && pvt->res->res_h_errno == HOST_NOT_FOUND)
		RES_SET_H_ERRNO(pvt->res, therrno);
	return (NULL);
}

static int
init(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

        if (!pvt->res && !ho_res_get(this))
                return (-1);

        if (((pvt->res->options & RES_INIT) == 0U) &&
            (res_ninit(pvt->res) == -1))
                return (-1);

        return (0);
}
