/*
 * Copyright (c) 1998-1999 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: getnetgrent_r.c,v 8.4 1999/01/18 07:46:52 vixie Exp $";
#endif /* LIBC_SCCS and not lint */

#include <port_before.h>
#if !defined(_REENTRANT) || !defined(DO_PTHREADS)
	static int getnetgrent_r_not_required = 0;
#else
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <port_after.h>

#ifdef NGR_R_RETURN

static NGR_R_RETURN 
copy_protoent(char **, char **, char **, char *, char *, char *,
		NGR_R_COPY_ARGS);

NGR_R_RETURN
innetgr_r(const char *netgroup, const char *host, const char *user,
	const char *domain) {

	return (innetgr(netgroup, host, user, domain));
}

/*
 *	These assume a single context is in operation per thread.
 *	If this is not the case we will need to call irs directly
 *	rather than through the base functions.
 */

NGR_R_RETURN
getnetgrent_r(char **machinep, char **userp, char **domainp, NGR_R_ARGS) {
	char *mp, *up, *dp;
	int res = getnetgrent(&mp, &up, &dp);

	if (res != 1) 
		return (res);

	return (copy_protoent(machinep, userp, domainp,
				mp, up, dp, NGR_R_COPY));
}

NGR_R_SET_RETURN
#ifdef NGR_R_ENT_ARGS
setnetgrent_r(const char *netgroup, NGR_R_ENT_ARGS)
#else
setnetgrent_r(const char *netgroup)
#endif
{
	setnetgrent(netgroup);
#ifdef NGR_R_SET_RESULT
	return (NGR_R_SET_RESULT);
#endif
}

NGR_R_END_RETURN
endnetgrent_r(NGR_R_ENT_ARGS) {
	endnetgrent();
	NGR_R_END_RESULT(NGR_R_OK);
}

/* Private */

static int
copy_protoent(char **machinep, char **userp, char **domainp,
		char *mp, char *up, char *dp, NGR_R_COPY_ARGS) {
	char *cp;
	int i, n;
	int len;

	/* Find out the amount of space required to store the answer. */
	len = 0;
	if (mp != NULL) len += strlen(mp) + 1;
	if (up != NULL) len += strlen(up) + 1;
	if (dp != NULL) len += strlen(dp) + 1;
	
	if (len > buflen) {
		errno = ERANGE;
		return (NGR_R_BAD);
	}

	cp = buf;

	if (mp != NULL) {
		n = strlen(mp) + 1;
		strcpy(cp, mp);
		*machinep = cp;
		cp += n;
	} else
		*machinep = NULL;

	if (up != NULL) {
		n = strlen(up) + 1;
		strcpy(cp, up);
		*userp = cp;
		cp += n;
	} else
		*userp = NULL;

	if (dp != NULL) {
		n = strlen(dp) + 1;
		strcpy(cp, dp);
		*domainp = cp;
		cp += n;
	} else
		*domainp = NULL;

	return (NGR_R_OK);
}
#else /* NGR_R_RETURN */
	static int getnetgrent_r_unknown_system = 0;
#endif /* NGR_R_RETURN */
#endif /* !defined(_REENTRANT) || !defined(DO_PTHREADS) */
