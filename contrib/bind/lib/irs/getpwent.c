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

#if !defined(LINT) && !defined(CODECENTER)
static char rcsid[] = "$Id: getpwent.c,v 1.13 1998/03/21 00:59:48 halley Exp $";
#endif

/* Imports */

#include "port_before.h"

#ifndef WANT_IRS_PW
static int __bind_irs_pw_unneeded;
#else

#include <sys/types.h>

#include <errno.h>
#include <pwd.h>
#include <stdio.h>

#include <irs.h>

#include "port_after.h"

#include "irs_data.h"

/* Forward */

static struct irs_pw *	init(void);

/* Public */

struct passwd *
getpwent(void) {
	struct irs_pw *pw = init();

	if (!pw)
		return (NULL);
	net_data.pw_last = (*pw->next)(pw);
	return (net_data.pw_last);
}

struct passwd *
getpwnam(const char *name) {
	struct irs_pw *pw = init();
	
	if (!pw)
		return (NULL);
	if (net_data.pw_stayopen && net_data.pw_last &&
	    !strcmp(net_data.pw_last->pw_name, name))
		return (net_data.pw_last);
	net_data.pw_last = (*pw->byname)(pw, name);
	if (!net_data.pw_stayopen)
		endpwent();
	return (net_data.pw_last);
}

struct passwd *
getpwuid(uid_t uid) {
	struct irs_pw *pw = init();

	if (!pw)
		return (NULL);
	if (net_data.pw_stayopen && net_data.pw_last &&
	    net_data.pw_last->pw_uid == uid)
		return (net_data.pw_last);
	net_data.pw_last = (*pw->byuid)(pw, uid);
	if (!net_data.pw_stayopen)
		endpwent();
	return (net_data.pw_last);
}

int
setpassent(int stayopen) {
	struct irs_pw *pw = init();
	
	if (!pw)
		return (0);
	(*pw->rewind)(pw);
	net_data.pw_stayopen = (stayopen != 0);
	return (1);
}

#ifdef SETPWENT_VOID
void
setpwent() {
	(void) setpassent(0);
}
#else
int 
setpwent() {
	return (setpassent(0));
}
#endif

void
endpwent() {
	struct irs_pw *pw = init();

	if (pw != NULL)
		(*pw->minimize)(pw);
}

/* Private */

static struct irs_pw *
init() {
	if (!net_data_init())
		goto error;
	if (!net_data.pw)
		net_data.pw = (*net_data.irs->pw_map)(net_data.irs);
	if (!net_data.pw) {
 error: 
		errno = EIO;
		return (NULL);
	}
	return (net_data.pw);
}

#endif /* WANT_IRS_PW */
