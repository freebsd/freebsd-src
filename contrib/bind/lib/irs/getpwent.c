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

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: getpwent.c,v 1.21 2000/02/21 21:40:56 vixie Exp $";
#endif

/* Imports */

#include "port_before.h"

#if !defined(WANT_IRS_PW) || defined(__BIND_NOSTATIC)
static int __bind_irs_pw_unneeded;
#else

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <pwd.h>
#include <resolv.h>
#include <stdio.h>

#include <irs.h>

#include "port_after.h"

#include "irs_data.h"

/* Forward */

static struct net_data * init(void);

/* Public */

struct passwd *
getpwent(void) {
	struct net_data *net_data = init();

	return (getpwent_p(net_data));
}

struct passwd *
getpwnam(const char *name) {
	struct net_data *net_data = init();

	return (getpwnam_p(name, net_data));
}

struct passwd *
getpwuid(uid_t uid) {
	struct net_data *net_data = init();

	return (getpwuid_p(uid, net_data));
}

int
setpassent(int stayopen) {
	struct net_data *net_data = init();

	return (setpassent_p(stayopen, net_data));
}

#ifdef SETPWENT_VOID
void
setpwent() {
	struct net_data *net_data = init();

	setpwent_p(net_data);
}
#else
int
setpwent() {
	struct net_data *net_data = init();

	return (setpwent_p(net_data));
}
#endif

void
endpwent() {
	struct net_data *net_data = init();

	endpwent_p(net_data);
}

/* Shared private. */

struct passwd *
getpwent_p(struct net_data *net_data) {
	struct irs_pw *pw;

	if (!net_data || !(pw = net_data->pw))
		return (NULL);
	net_data->pw_last = (*pw->next)(pw);
	return (net_data->pw_last);
}

struct passwd *
getpwnam_p(const char *name, struct net_data *net_data) {
	struct irs_pw *pw;

	if (!net_data || !(pw = net_data->pw))
		return (NULL);
	if (net_data->pw_stayopen && net_data->pw_last &&
	    !strcmp(net_data->pw_last->pw_name, name))
		return (net_data->pw_last);
	net_data->pw_last = (*pw->byname)(pw, name);
	if (!net_data->pw_stayopen)
		endpwent();
	return (net_data->pw_last);
}

struct passwd *
getpwuid_p(uid_t uid, struct net_data *net_data) {
	struct irs_pw *pw;

	if (!net_data || !(pw = net_data->pw))
		return (NULL);
	if (net_data->pw_stayopen && net_data->pw_last &&
	    net_data->pw_last->pw_uid == uid)
		return (net_data->pw_last);
	net_data->pw_last = (*pw->byuid)(pw, uid);
	if (!net_data->pw_stayopen)
		endpwent();
	return (net_data->pw_last);
}

int
setpassent_p(int stayopen, struct net_data *net_data) {
	struct irs_pw *pw;

	if (!net_data || !(pw = net_data->pw))
		return (0);
	(*pw->rewind)(pw);
	net_data->pw_stayopen = (stayopen != 0);
	if (stayopen == 0)
		net_data_minimize(net_data);
	return (1);
}

#ifdef SETPWENT_VOID
void
setpwent_p(struct net_data *net_data) {
	(void) setpassent_p(0, net_data);
}
#else
int
setpwent_p(struct net_data *net_data) {
	return (setpassent_p(0, net_data));
}
#endif

void
endpwent_p(struct net_data *net_data) {
	struct irs_pw *pw;

	if ((net_data != NULL) && ((pw = net_data->pw) != NULL))
		(*pw->minimize)(pw);
}

/* Private */

static struct net_data *
init() {
	struct net_data *net_data;
	if (!(net_data = net_data_init(NULL)))
		goto error;
	if (!net_data->pw) {
		net_data->pw = (*net_data->irs->pw_map)(net_data->irs);

		if (!net_data->pw || !net_data->res) {
 error: 
			errno = EIO;
			return (NULL);
		}
		(*net_data->pw->res_set)(net_data->pw, net_data->res, NULL);
	}
	
	return (net_data);
}

#endif /* WANT_IRS_PW */
