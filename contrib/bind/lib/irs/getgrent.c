/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: getgrent.c,v 1.20 2001/05/29 05:48:41 marka Exp $";
#endif

/* Imports */

#include "port_before.h"

#if !defined(WANT_IRS_GR) || defined(__BIND_NOSTATIC)
static int __bind_irs_gr_unneeded;
#else

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <grp.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <irs.h>

#include "port_after.h"

#include "irs_data.h"

/* Forward */

static struct net_data *init(void);
void			endgrent(void);

/* Public */

struct group *
getgrent() {
	struct net_data *net_data = init();

	return (getgrent_p(net_data));
}

struct group *
getgrnam(const char *name) {
	struct net_data *net_data = init();

	return (getgrnam_p(name, net_data));
}

struct group *
getgrgid(gid_t gid) {
	struct net_data *net_data = init();

	return (getgrgid_p(gid, net_data));
}

int
setgroupent(int stayopen) {
	struct net_data *net_data = init();

	return (setgroupent_p(stayopen, net_data));
}

#ifdef SETGRENT_VOID
void
setgrent(void) {
	struct net_data *net_data = init();

	setgrent_p(net_data);
}
#else
int
setgrent(void) {
	struct net_data *net_data = init();

	return (setgrent_p(net_data));
}
#endif /* SETGRENT_VOID */

void
endgrent() {
	struct net_data *net_data = init();

	endgrent_p(net_data);
}

int
getgrouplist(GETGROUPLIST_ARGS) {
	struct net_data *net_data = init();

	return (getgrouplist_p(name, basegid, groups, ngroups, net_data));
}

/* Shared private. */

struct group *
getgrent_p(struct net_data *net_data) {
	struct irs_gr *gr;

	if (!net_data || !(gr = net_data->gr))
		return (NULL);
	net_data->gr_last = (*gr->next)(gr);
	return (net_data->gr_last);
}

struct group *
getgrnam_p(const char *name, struct net_data *net_data) {
	struct irs_gr *gr;

	if (!net_data || !(gr = net_data->gr))
		return (NULL);
	if (net_data->gr_stayopen && net_data->gr_last &&
	    !strcmp(net_data->gr_last->gr_name, name))
		return (net_data->gr_last);
	net_data->gr_last = (*gr->byname)(gr, name);
	if (!net_data->gr_stayopen)
		endgrent();
	return (net_data->gr_last);
}

struct group *
getgrgid_p(gid_t gid, struct net_data *net_data) {
	struct irs_gr *gr;

	if (!net_data || !(gr = net_data->gr))
		return (NULL);
	if (net_data->gr_stayopen && net_data->gr_last &&
	    (gid_t)net_data->gr_last->gr_gid == gid)
		return (net_data->gr_last);
	net_data->gr_last = (*gr->bygid)(gr, gid);
	if (!net_data->gr_stayopen)
		endgrent();
	return (net_data->gr_last);
}

int
setgroupent_p(int stayopen, struct net_data *net_data) {
	struct irs_gr *gr;

	if (!net_data || !(gr = net_data->gr))
		return (0);
	(*gr->rewind)(gr);
	net_data->gr_stayopen = (stayopen != 0);
	if (stayopen == 0)
		net_data_minimize(net_data);
	return (1);
}

#ifdef SETGRENT_VOID
void
setgrent_p(struct net_data *net_data) {
	(void)setgroupent_p(0, net_data);
}
#else
int
setgrent_p(struct net_data *net_data) {
	return (setgroupent_p(0, net_data));
}
#endif /* SETGRENT_VOID */

void
endgrent_p(struct net_data *net_data) {
	struct irs_gr *gr;

	if ((net_data != NULL) && ((gr = net_data->gr) != NULL))
		(*gr->minimize)(gr);
}

int
getgrouplist_p(const char *name, gid_t basegid, gid_t *groups, int *ngroups,
	       struct net_data *net_data) {
	struct irs_gr *gr;

	if (!net_data || !(gr = net_data->gr)) {
		*ngroups = 0;
		return (-1);
	}
	return ((*gr->list)(gr, name, basegid, groups, ngroups));
}

/* Private */

static struct net_data *
init() {
	struct net_data *net_data;

	if (!(net_data = net_data_init(NULL)))
		goto error;
	if (!net_data->gr) {
		net_data->gr = (*net_data->irs->gr_map)(net_data->irs);

		if (!net_data->gr || !net_data->res) {
 error: 
			errno = EIO;
			return (NULL);
		}
		(*net_data->gr->res_set)(net_data->gr, net_data->res,
					 NULL);
	}
	
	return (net_data);
}

#endif /* WANT_IRS_GR */
