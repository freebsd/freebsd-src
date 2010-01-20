/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1998-1999 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: getservent_r.c,v 1.3.206.2 2006/08/01 01:19:28 marka Exp $";
#endif /* LIBC_SCCS and not lint */

#include <port_before.h>
#if !defined(_REENTRANT) || !defined(DO_PTHREADS)
	static int getservent_r_not_required = 0;
#else
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <port_after.h>

#ifdef SERV_R_RETURN

static SERV_R_RETURN 
copy_servent(struct servent *, struct servent *, SERV_R_COPY_ARGS);

SERV_R_RETURN
getservbyname_r(const char *name, const char *proto,
		struct servent *sptr, SERV_R_ARGS) {
	struct servent *se = getservbyname(name, proto);
#ifdef SERV_R_SETANSWER
	int n = 0;
	
	if (se == NULL || (n = copy_servent(se, sptr, SERV_R_COPY)) != 0)
		*answerp = NULL;
	else
		*answerp = sptr;

	return (n);
#else
	if (se == NULL)
		return (SERV_R_BAD);

	return (copy_servent(se, sptr, SERV_R_COPY));
#endif
}

SERV_R_RETURN
getservbyport_r(int port, const char *proto,
		struct servent *sptr, SERV_R_ARGS) {
	struct servent *se = getservbyport(port, proto);
#ifdef SERV_R_SETANSWER
	int n = 0;
	
	if (se == NULL || (n = copy_servent(se, sptr, SERV_R_COPY)) != 0)
		*answerp = NULL;
	else
		*answerp = sptr;

	return (n);
#else
	if (se == NULL)
		return (SERV_R_BAD);

	return (copy_servent(se, sptr, SERV_R_COPY));
#endif
}

/*
 *	These assume a single context is in operation per thread.
 *	If this is not the case we will need to call irs directly
 *	rather than through the base functions.
 */

SERV_R_RETURN
getservent_r(struct servent *sptr, SERV_R_ARGS) {
	struct servent *se = getservent();
#ifdef SERV_R_SETANSWER
	int n = 0;
	
	if (se == NULL || (n = copy_servent(se, sptr, SERV_R_COPY)) != 0)
		*answerp = NULL;
	else
		*answerp = sptr;

	return (n);
#else
	if (se == NULL)
		return (SERV_R_BAD);

	return (copy_servent(se, sptr, SERV_R_COPY));
#endif
}

SERV_R_SET_RETURN
#ifdef SERV_R_ENT_ARGS
setservent_r(int stay_open, SERV_R_ENT_ARGS)
#else
setservent_r(int stay_open)
#endif
{
#ifdef SERV_R_ENT_UNUSED
	SERV_R_ENT_UNUSED;
#endif
	setservent(stay_open);
#ifdef SERV_R_SET_RESULT
	return (SERV_R_SET_RESULT);
#endif
}

SERV_R_END_RETURN
#ifdef SERV_R_ENT_ARGS
endservent_r(SERV_R_ENT_ARGS)
#else
endservent_r()
#endif
{
#ifdef SERV_R_ENT_UNUSED
	SERV_R_ENT_UNUSED;
#endif
	endservent();
	SERV_R_END_RESULT(SERV_R_OK);
}

/* Private */

#ifndef SERVENT_DATA
static SERV_R_RETURN
copy_servent(struct servent *se, struct servent *sptr, SERV_R_COPY_ARGS) {
	char *cp;
	int i, n;
	int numptr, len;

	/* Find out the amount of space required to store the answer. */
	numptr = 1; /* NULL ptr */
	len = (char *)ALIGN(buf) - buf;
	for (i = 0; se->s_aliases[i]; i++, numptr++) {
		len += strlen(se->s_aliases[i]) + 1;
	}
	len += strlen(se->s_name) + 1;
	len += strlen(se->s_proto) + 1;
	len += numptr * sizeof(char*);
	
	if (len > (int)buflen) {
		errno = ERANGE;
		return (SERV_R_BAD);
	}

	/* copy port value */
	sptr->s_port = se->s_port;

	cp = (char *)ALIGN(buf) + numptr * sizeof(char *);

	/* copy official name */
	n = strlen(se->s_name) + 1;
	strcpy(cp, se->s_name);
	sptr->s_name = cp;
	cp += n;

	/* copy aliases */
	sptr->s_aliases = (char **)ALIGN(buf);
	for (i = 0 ; se->s_aliases[i]; i++) {
		n = strlen(se->s_aliases[i]) + 1;
		strcpy(cp, se->s_aliases[i]);
		sptr->s_aliases[i] = cp;
		cp += n;
	}
	sptr->s_aliases[i] = NULL;

	/* copy proto */
	n = strlen(se->s_proto) + 1;
	strcpy(cp, se->s_proto);
	sptr->s_proto = cp;
	cp += n;

	return (SERV_R_OK);
}
#else /* !SERVENT_DATA */
static int
copy_servent(struct servent *se, struct servent *sptr, SERV_R_COPY_ARGS) {
	char *cp, *eob;
	int i, n;

	/* copy port value */
	sptr->s_port = se->s_port;

	/* copy official name */
	cp = sdptr->line;
	eob = sdptr->line + sizeof(sdptr->line);
	if ((n = strlen(se->s_name) + 1) < (eob - cp)) {
		strcpy(cp, se->s_name);
		sptr->s_name = cp;
		cp += n;
	} else {
		return (-1);
	}

	/* copy aliases */
	i = 0;
	sptr->s_aliases = sdptr->serv_aliases;
	while (se->s_aliases[i] && i < (_MAXALIASES-1)) {
		if ((n = strlen(se->s_aliases[i]) + 1) < (eob - cp)) {
			strcpy(cp, se->s_aliases[i]);
			sptr->s_aliases[i] = cp;
			cp += n;
		} else {
			break;
		}
		i++;
	}
	sptr->s_aliases[i] = NULL;

	/* copy proto */
	if ((n = strlen(se->s_proto) + 1) < (eob - cp)) {
		strcpy(cp, se->s_proto);
		sptr->s_proto = cp;
		cp += n;
	} else {
		return (-1);
	}

	return (SERV_R_OK);
}
#endif /* !SERVENT_DATA */
#else /*SERV_R_RETURN */
	static int getservent_r_unknown_system = 0;
#endif /*SERV_R_RETURN */
#endif /* !defined(_REENTRANT) || !defined(DO_PTHREADS) */
