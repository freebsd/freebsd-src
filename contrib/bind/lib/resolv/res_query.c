/*
 * Copyright (c) 1988, 1993
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
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
static const char sccsid[] = "@(#)res_query.c	8.1 (Berkeley) 6/4/93";
static const char rcsid[] = "$Id: res_query.c,v 8.20 2000/02/29 05:39:12 vixie Exp $";
#endif /* LIBC_SCCS and not lint */

#include "port_before.h"
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_after.h"

/* Options.  Leave them on. */
#define DEBUG

#if PACKETSZ > 1024
#define MAXPACKET	PACKETSZ
#else
#define MAXPACKET	1024
#endif

/*
 * Formulate a normal query, send, and await answer.
 * Returned answer is placed in supplied buffer "answer".
 * Perform preliminary check of answer, returning success only
 * if no error is indicated and the answer count is nonzero.
 * Return the size of the response on success, -1 on error.
 * Error number is left in H_ERRNO.
 *
 * Caller must parse answer and determine whether it answers the question.
 */
int
res_nquery(res_state statp,
	   const char *name,	/* domain name */
	   int class, int type,	/* class and type of query */
	   u_char *answer,	/* buffer to put answer */
	   int anslen)		/* size of answer buffer */
{
	u_char buf[MAXPACKET];
	HEADER *hp = (HEADER *) answer;
	int n;

	hp->rcode = NOERROR;	/* default */

#ifdef DEBUG
	if (statp->options & RES_DEBUG)
		printf(";; res_query(%s, %d, %d)\n", name, class, type);
#endif

	n = res_nmkquery(statp, QUERY, name, class, type, NULL, 0, NULL,
			 buf, sizeof(buf));
	if (n <= 0) {
#ifdef DEBUG
		if (statp->options & RES_DEBUG)
			printf(";; res_query: mkquery failed\n");
#endif
		RES_SET_H_ERRNO(statp, NO_RECOVERY);
		return (n);
	}
	n = res_nsend(statp, buf, n, answer, anslen);
	if (n < 0) {
#ifdef DEBUG
		if (statp->options & RES_DEBUG)
			printf(";; res_query: send error\n");
#endif
		RES_SET_H_ERRNO(statp, TRY_AGAIN);
		return (n);
	}

	if (hp->rcode != NOERROR || ntohs(hp->ancount) == 0) {
#ifdef DEBUG
		if (statp->options & RES_DEBUG)
			printf(";; rcode = %d, ancount=%d\n", hp->rcode,
			    ntohs(hp->ancount));
#endif
		switch (hp->rcode) {
		case NXDOMAIN:
			RES_SET_H_ERRNO(statp, HOST_NOT_FOUND);
			break;
		case SERVFAIL:
			RES_SET_H_ERRNO(statp, TRY_AGAIN);
			break;
		case NOERROR:
			RES_SET_H_ERRNO(statp, NO_DATA);
			break;
		case FORMERR:
		case NOTIMP:
		case REFUSED:
		default:
			RES_SET_H_ERRNO(statp, NO_RECOVERY);
			break;
		}
		return (-1);
	}
	return (n);
}

/*
 * Formulate a normal query, send, and retrieve answer in supplied buffer.
 * Return the size of the response on success, -1 on error.
 * If enabled, implement search rules until answer or unrecoverable failure
 * is detected.  Error code, if any, is left in H_ERRNO.
 */
int
res_nsearch(res_state statp,
	    const char *name,	/* domain name */
	    int class, int type,	/* class and type of query */
	    u_char *answer,	/* buffer to put answer */
	    int anslen)		/* size of answer */
{
	const char *cp, * const *domain;
	HEADER *hp = (HEADER *) answer;
	char tmp[NS_MAXDNAME];
	u_int dots;
	int trailing_dot, ret, saved_herrno;
	int got_nodata = 0, got_servfail = 0, root_on_list = 0;
	int tried_as_is = 0;

	errno = 0;
	RES_SET_H_ERRNO(statp, HOST_NOT_FOUND);  /* True if we never query. */

	dots = 0;
	for (cp = name; *cp != '\0'; cp++)
		dots += (*cp == '.');
	trailing_dot = 0;
	if (cp > name && *--cp == '.')
		trailing_dot++;

	/* If there aren't any dots, it could be a user-level alias. */
	if (!dots && (cp = res_hostalias(statp, name, tmp, sizeof tmp))!= NULL)
		return (res_nquery(statp, cp, class, type, answer, anslen));

	/*
	 * If there are enough dots in the name, let's just give it a
	 * try 'as is'. The threshold can be set with the "ndots" option.
	 * Also, query 'as is', if there is a trailing dot in the name.
	 */
	saved_herrno = -1;
	if (dots >= statp->ndots || trailing_dot) {
		ret = res_nquerydomain(statp, name, NULL, class, type,
					 answer, anslen);
		if (ret > 0 || trailing_dot)
			return (ret);
		saved_herrno = h_errno;
		tried_as_is++;
	}

	/*
	 * We do at least one level of search if
	 *	- there is no dot and RES_DEFNAME is set, or
	 *	- there is at least one dot, there is no trailing dot,
	 *	  and RES_DNSRCH is set.
	 */
	if ((!dots && (statp->options & RES_DEFNAMES) != 0) ||
	    (dots && !trailing_dot && (statp->options & RES_DNSRCH) != 0)) {
		int done = 0;

		for (domain = (const char * const *)statp->dnsrch;
		     *domain && !done;
		     domain++) {

			if (domain[0][0] == '\0' ||
			    (domain[0][0] == '.' && domain[0][1] == '\0'))
				root_on_list++;

			ret = res_nquerydomain(statp, name, *domain,
					       class, type,
					       answer, anslen);
			if (ret > 0)
				return (ret);

			/*
			 * If no server present, give up.
			 * If name isn't found in this domain,
			 * keep trying higher domains in the search list
			 * (if that's enabled).
			 * On a NO_DATA error, keep trying, otherwise
			 * a wildcard entry of another type could keep us
			 * from finding this entry higher in the domain.
			 * If we get some other error (negative answer or
			 * server failure), then stop searching up,
			 * but try the input name below in case it's
			 * fully-qualified.
			 */
			if (errno == ECONNREFUSED) {
				RES_SET_H_ERRNO(statp, TRY_AGAIN);
				return (-1);
			}

			switch (statp->res_h_errno) {
			case NO_DATA:
				got_nodata++;
				/* FALLTHROUGH */
			case HOST_NOT_FOUND:
				/* keep trying */
				break;
			case TRY_AGAIN:
				if (hp->rcode == SERVFAIL) {
					/* try next search element, if any */
					got_servfail++;
					break;
				}
				/* FALLTHROUGH */
			default:
				/* anything else implies that we're done */
				done++;
			}

			/* if we got here for some reason other than DNSRCH,
			 * we only wanted one iteration of the loop, so stop.
			 */
			if ((statp->options & RES_DNSRCH) == 0)
				done++;
		}
	}

	/*
	 * If the name has any dots at all, and no earlier 'as-is' query 
	 * for the name, and "." is not on the search list, then try an as-is
	 * query now.
	 */
	if (statp->ndots && !(tried_as_is || root_on_list)) {
		ret = res_nquerydomain(statp, name, NULL, class, type,
				       answer, anslen);
		if (ret > 0)
			return (ret);
	}

	/* if we got here, we didn't satisfy the search.
	 * if we did an initial full query, return that query's H_ERRNO
	 * (note that we wouldn't be here if that query had succeeded).
	 * else if we ever got a nodata, send that back as the reason.
	 * else send back meaningless H_ERRNO, that being the one from
	 * the last DNSRCH we did.
	 */
	if (saved_herrno != -1)
		RES_SET_H_ERRNO(statp, saved_herrno);
	else if (got_nodata)
		RES_SET_H_ERRNO(statp, NO_DATA);
	else if (got_servfail)
		RES_SET_H_ERRNO(statp, TRY_AGAIN);
	return (-1);
}

/*
 * Perform a call on res_query on the concatenation of name and domain,
 * removing a trailing dot from name if domain is NULL.
 */
int
res_nquerydomain(res_state statp,
	    const char *name,
	    const char *domain,
	    int class, int type,	/* class and type of query */
	    u_char *answer,		/* buffer to put answer */
	    int anslen)		/* size of answer */
{
	char nbuf[MAXDNAME];
	const char *longname = nbuf;
	int n, d;

#ifdef DEBUG
	if (statp->options & RES_DEBUG)
		printf(";; res_nquerydomain(%s, %s, %d, %d)\n",
		       name, domain?domain:"<Nil>", class, type);
#endif
	if (domain == NULL) {
		/*
		 * Check for trailing '.';
		 * copy without '.' if present.
		 */
		n = strlen(name);
		if (n >= MAXDNAME) {
			RES_SET_H_ERRNO(statp, NO_RECOVERY);
			return (-1);
		}
		n--;
		if (n >= 0 && name[n] == '.') {
			strncpy(nbuf, name, n);
			nbuf[n] = '\0';
		} else
			longname = name;
	} else {
		n = strlen(name);
		d = strlen(domain);
		if (n + d + 1 >= MAXDNAME) {
			RES_SET_H_ERRNO(statp, NO_RECOVERY);
			return (-1);
		}
		sprintf(nbuf, "%s.%s", name, domain);
	}
	return (res_nquery(statp, longname, class, type, answer, anslen));
}

const char *
res_hostalias(const res_state statp, const char *name, char *dst, size_t siz) {
	char *file, *cp1, *cp2;
	char buf[BUFSIZ];
	FILE *fp;

	if (statp->options & RES_NOALIASES)
		return (NULL);
	file = getenv("HOSTALIASES");
	if (file == NULL || (fp = fopen(file, "r")) == NULL)
		return (NULL);
	setbuf(fp, NULL);
	buf[sizeof(buf) - 1] = '\0';
	while (fgets(buf, sizeof(buf), fp)) {
		for (cp1 = buf; *cp1 && !isspace(*cp1); ++cp1)
			;
		if (!*cp1)
			break;
		*cp1 = '\0';
		if (ns_samename(buf, name) == 1) {
			while (isspace(*++cp1))
				;
			if (!*cp1)
				break;
			for (cp2 = cp1 + 1; *cp2 && !isspace(*cp2); ++cp2)
				;
			*cp2 = '\0';
			strncpy(dst, cp1, siz - 1);
			dst[siz - 1] = '\0';
			fclose(fp);
			return (dst);
		}
	}
	fclose(fp);
	return (NULL);
}
