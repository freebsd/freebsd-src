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
static const char rcsid[] = "$Id: dns_gr.c,v 1.3.18.1 2005-04-27 05:00:54 sra Exp $";
#endif

/*! \file
 * \brief
 * dns_gr.c --- this file contains the functions for accessing
 * 	group information from Hesiod.
 */

#include "port_before.h"

#ifndef WANT_IRS_GR
static int __bind_irs_gr_unneeded;
#else

#include <sys/param.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

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

/* Types. */

struct pvt {
	/*
	 * This is our private accessor data.  It has a shared hesiod context.
	 */
	struct dns_p *	dns;
	/*
	 * Need space to store the entries read from the group file.
	 * The members list also needs space per member, and the
	 * strings making up the user names must be allocated
	 * somewhere.  Rather than doing lots of small allocations,
	 * we keep one buffer and resize it as needed.
	 */
	struct group	group;
	size_t		nmemb;		/*%< Malloc'd max index of gr_mem[]. */
	char *		membuf;
	size_t		membufsize;
};

/* Forward. */

static struct group *	gr_next(struct irs_gr *);
static struct group *	gr_byname(struct irs_gr *, const char *);
static struct group *	gr_bygid(struct irs_gr *, gid_t);
static void		gr_rewind(struct irs_gr *);
static void		gr_close(struct irs_gr *);
static int		gr_list(struct irs_gr *, const char *,
				gid_t, gid_t *, int *);
static void		gr_minimize(struct irs_gr *);
static struct __res_state * gr_res_get(struct irs_gr *);
static void		gr_res_set(struct irs_gr *,
				   struct __res_state *,
				   void (*)(void *));

static struct group *	get_hes_group(struct irs_gr *this,
				      const char *name,
				      const char *type);

/* Public. */

struct irs_gr *
irs_dns_gr(struct irs_acc *this) {
	struct dns_p *dns = (struct dns_p *)this->private;
	struct irs_gr *gr;
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
	if (!(gr = memget(sizeof *gr))) {
		memput(pvt, sizeof *pvt);
		errno = ENOMEM;
		return (NULL);
	}
	memset(gr, 0x5e, sizeof *gr);
	gr->private = pvt;
	gr->next = gr_next;
	gr->byname = gr_byname;
	gr->bygid = gr_bygid;
	gr->rewind = gr_rewind;
	gr->close = gr_close;
	gr->list = gr_list;
	gr->minimize = gr_minimize;
	gr->res_get = gr_res_get;
	gr->res_set = gr_res_set;
	return (gr);
}

/* methods */

static void
gr_close(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->group.gr_mem)
		free(pvt->group.gr_mem);
	if (pvt->membuf)
		free(pvt->membuf);
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct group *
gr_next(struct irs_gr *this) {

	UNUSED(this);

	return (NULL);
}

static struct group *
gr_byname(struct irs_gr *this, const char *name) {
	return (get_hes_group(this, name, "group"));
}

static struct group *
gr_bygid(struct irs_gr *this, gid_t gid) {
	char name[32];

	sprintf(name, "%ld", (long)gid);
	return (get_hes_group(this, name, "gid"));
}

static void
gr_rewind(struct irs_gr *this) {

	UNUSED(this);

	/* NOOP */
}

static int
gr_list(struct irs_gr *this, const char *name,
	gid_t basegid, gid_t *groups, int *ngroups)
{
	UNUSED(this);
	UNUSED(name);
	UNUSED(basegid);
	UNUSED(groups);

	*ngroups = 0;
	/* There's some way to do this in Hesiod. */
	return (-1);
}

static void
gr_minimize(struct irs_gr *this) {

	UNUSED(this);
	/* NOOP */
}

/* Private. */

static struct group *
get_hes_group(struct irs_gr *this, const char *name, const char *type) {
	struct pvt *pvt = (struct pvt *)this->private;
	char **hes_list, *cp, **new;
	size_t num_members = 0;
	u_long t;

	hes_list = hesiod_resolve(pvt->dns->hes_ctx, name, type);
	if (!hes_list)
		return (NULL);

	/*
	 * Copy the returned hesiod string into storage space.
	 */
	if (pvt->membuf)
		free(pvt->membuf);
	pvt->membuf = strdup(*hes_list);
	hesiod_free_list(pvt->dns->hes_ctx, hes_list);

	cp = pvt->membuf;
	pvt->group.gr_name = cp;
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
	*cp++ = '\0';
	
	pvt->group.gr_passwd = cp;
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
	*cp++ = '\0';

	errno = 0;
	t = strtoul(cp, NULL, 10);
	if (errno == ERANGE)
		goto cleanup;
	pvt->group.gr_gid = (gid_t) t;
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
	cp++;

	/*
	 * Parse the members out.
	 */
	while (*cp) {
		if (num_members+1 >= pvt->nmemb || pvt->group.gr_mem == NULL) {
			pvt->nmemb += 10;
			new = realloc(pvt->group.gr_mem,
				      pvt->nmemb * sizeof(char *));
			if (new == NULL)
				goto cleanup;
			pvt->group.gr_mem = new;
		}
		pvt->group.gr_mem[num_members++] = cp;
		if (!(cp = strchr(cp, ',')))
			break;
		*cp++ = '\0';
	}
	if (!pvt->group.gr_mem) {
		pvt->group.gr_mem = malloc(sizeof(char*));
		if (!pvt->group.gr_mem)
			goto cleanup;
	}
	pvt->group.gr_mem[num_members] = NULL;
	
	return (&pvt->group);
	
 cleanup:	
	if (pvt->group.gr_mem) {
		free(pvt->group.gr_mem);
		pvt->group.gr_mem = NULL;
	}
	if (pvt->membuf) {
		free(pvt->membuf);
		pvt->membuf = NULL;
	}
	return (NULL);
}

static struct __res_state *
gr_res_get(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;

	return (__hesiod_res_get(dns->hes_ctx));
}

static void
gr_res_set(struct irs_gr *this, struct __res_state * res,
	   void (*free_res)(void *)) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct dns_p *dns = pvt->dns;

	__hesiod_res_set(dns->hes_ctx, res, free_res);
}

#endif /* WANT_IRS_GR */
