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

#if !defined(LINT) && !defined(CODECENTER)
static char rcsid[] = "$Id: getgrent.c,v 1.13 1998/03/21 00:59:47 halley Exp $";
#endif

/* Imports */

#include "port_before.h"

#ifndef WANT_IRS_GR
static int __bind_irs_gr_unneeded;
#else

#include <sys/types.h>

#include <errno.h>
#include <grp.h>
#include <stdio.h>

#include <irs.h>

#include "port_after.h"

#include "irs_data.h"

/* Forward */

static struct irs_gr *	init(void);
void			endgrent(void);

/* Public */

struct group *
getgrent() {
	struct irs_gr *gr = init();
	
	if (!gr)
		return (NULL);
	net_data.gr_last = (*gr->next)(gr);
	return (net_data.gr_last);
}

struct group *
getgrnam(const char *name) {
	struct irs_gr *gr = init();
	
	if (!gr)
		return (NULL);
	if (net_data.gr_stayopen && net_data.gr_last &&
	    !strcmp(net_data.gr_last->gr_name, name))
		return (net_data.gr_last);
	net_data.gr_last = (*gr->byname)(gr, name);
	if (!net_data.gr_stayopen)
		endgrent();
	return (net_data.gr_last);
}

struct group *
getgrgid(gid_t gid) {
	struct irs_gr *gr = init();
	
	if (!gr)
		return (NULL);
	if (net_data.gr_stayopen && net_data.gr_last &&
	    net_data.gr_last->gr_gid == gid)
		return (net_data.gr_last);
	net_data.gr_last = (*gr->bygid)(gr, gid);
	if (!net_data.gr_stayopen)
		endgrent();
	return (net_data.gr_last);
}

int
setgroupent(int stayopen) {
	struct irs_gr *gr = init();
	
	if (!gr)
		return (0);
	(*gr->rewind)(gr);
	net_data.gr_stayopen = (stayopen != 0);
	return (1);
}

#ifdef SETGRENT_VOID
void
setgrent() {
	(void)setgroupent(0);
}
#else
int
setgrent() {
	return (setgroupent(0));
}
#endif /* SETGRENT_VOID */

void
endgrent() {
	struct irs_gr *gr = init();

	if (gr != NULL)
		(*gr->minimize)(gr);
}

int
getgrouplist(const char *name, gid_t basegid, gid_t *groups, int *ngroups) {
	struct irs_gr *gr = init();
	
	if (!gr) {
		*ngroups = 0;
		return (-1);
	}
	return ((*gr->list)(gr, name, basegid, groups, ngroups));
}

/* Private */

static struct irs_gr *
init() {
	if (!net_data_init())
		goto error;
	if (!net_data.gr)
		net_data.gr = (*net_data.irs->gr_map)(net_data.irs);
	if (!net_data.gr) {
 error: 
		errno = EIO;
		return (NULL);
	}
	return (net_data.gr);
}

#endif /* WANT_IRS_GR */
