/*	$KAME: getaddrinfo.c,v 1.14 2001/01/06 09:41:15 jinmei Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*! \file
 * Issues to be discussed:
 *\li  Thread safe-ness must be checked.
 *\li  Return values.  There are nonstandard return values defined and used
 *   in the source code.  This is because RFC2553 is silent about which error
 *   code must be returned for which situation.
 *\li  IPv4 classful (shortened) form.  RFC2553 is silent about it.  XNET 5.2
 *   says to use inet_aton() to convert IPv4 numeric to binary (allows
 *   classful form as a result).
 *   current code - disallow classful form for IPv4 (due to use of inet_pton).
 *\li  freeaddrinfo(NULL).  RFC2553 is silent about it.  XNET 5.2 says it is
 *   invalid.
 *   current code - SEGV on freeaddrinfo(NULL)
 * Note:
 *\li  We use getipnodebyname() just for thread-safeness.  There's no intent
 *   to let it do PF_UNSPEC (actually we never pass PF_UNSPEC to
 *   getipnodebyname().
 *\li  The code filters out AFs that are not supported by the kernel,
 *   when globbing NULL hostname (to loopback, or wildcard).  Is it the right
 *   thing to do?  What is the relationship with post-RFC2553 AI_ADDRCONFIG
 *   in ai_flags?
 *\li  (post-2553) semantics of AI_ADDRCONFIG itself is too vague.
 *   (1) what should we do against numeric hostname (2) what should we do
 *   against NULL hostname (3) what is AI_ADDRCONFIG itself.  AF not ready?
 *   non-loopback address configured?  global address configured?
 * \par Additional Issue:
 *  To avoid search order issue, we have a big amount of code duplicate
 *   from gethnamaddr.c and some other places.  The issues that there's no
 *   lower layer function to lookup "IPv4 or IPv6" record.  Calling
 *   gethostbyname2 from getaddrinfo will end up in wrong search order, as
 *   follows:
 *	\li The code makes use of following calls when asked to resolver with
 *	  ai_family  = PF_UNSPEC:
 *\code		getipnodebyname(host, AF_INET6);
 *		getipnodebyname(host, AF_INET);
 *\endcode
 *	\li  This will result in the following queries if the node is configure to
 *	  prefer /etc/hosts than DNS:
 *\code
 *		lookup /etc/hosts for IPv6 address
 *		lookup DNS for IPv6 address
 *		lookup /etc/hosts for IPv4 address
 *		lookup DNS for IPv4 address
 *\endcode
 *	  which may not meet people's requirement.
 *	 \li The right thing to happen is to have underlying layer which does
 *	  PF_UNSPEC lookup (lookup both) and return chain of addrinfos.
 *	  This would result in a bit of code duplicate with _dns_ghbyname() and
 *	  friends.
 */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>

#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <netdb.h>
#include <resolv.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include <stdarg.h>

#include <irs.h>
#include <isc/assertions.h>

#include "port_after.h"

#include "irs_data.h"

#define SUCCESS 0
#define ANY 0
#define YES 1
#define NO  0

static const char in_addrany[] = { 0, 0, 0, 0 };
static const char in6_addrany[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static const char in_loopback[] = { 127, 0, 0, 1 };
static const char in6_loopback[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};

static const struct afd {
	int a_af;
	int a_addrlen;
	int a_socklen;
	int a_off;
	const char *a_addrany;
	const char *a_loopback;
	int a_scoped;
} afdl [] = {
	{PF_INET6, sizeof(struct in6_addr),
	 sizeof(struct sockaddr_in6),
	 offsetof(struct sockaddr_in6, sin6_addr),
	 in6_addrany, in6_loopback, 1},
	{PF_INET, sizeof(struct in_addr),
	 sizeof(struct sockaddr_in),
	 offsetof(struct sockaddr_in, sin_addr),
	 in_addrany, in_loopback, 0},
	{0, 0, 0, 0, NULL, NULL, 0},
};

struct explore {
	int e_af;
	int e_socktype;
	int e_protocol;
	const char *e_protostr;
	int e_wild;
#define WILD_AF(ex)		((ex)->e_wild & 0x01)
#define WILD_SOCKTYPE(ex)	((ex)->e_wild & 0x02)
#define WILD_PROTOCOL(ex)	((ex)->e_wild & 0x04)
};

static const struct explore explore[] = {
#if 0
	{ PF_LOCAL, 0, ANY, ANY, NULL, 0x01 },
#endif
	{ PF_INET6, SOCK_DGRAM, IPPROTO_UDP, "udp", 0x07 },
	{ PF_INET6, SOCK_STREAM, IPPROTO_TCP, "tcp", 0x07 },
	{ PF_INET6, SOCK_RAW, ANY, NULL, 0x05 },
	{ PF_INET, SOCK_DGRAM, IPPROTO_UDP, "udp", 0x07 },
	{ PF_INET, SOCK_STREAM, IPPROTO_TCP, "tcp", 0x07 },
	{ PF_INET, SOCK_RAW, ANY, NULL, 0x05 },
	{ -1, 0, 0, NULL, 0 },
};

#define PTON_MAX	16

static int str_isnumber __P((const char *));
static int explore_fqdn __P((const struct addrinfo *, const char *,
	const char *, struct addrinfo **));
static int explore_copy __P((const struct addrinfo *, const struct addrinfo *,
	struct addrinfo **));
static int explore_null __P((const struct addrinfo *,
	const char *, struct addrinfo **));
static int explore_numeric __P((const struct addrinfo *, const char *,
	const char *, struct addrinfo **));
static int explore_numeric_scope __P((const struct addrinfo *, const char *,
	const char *, struct addrinfo **));
static int get_canonname __P((const struct addrinfo *,
	struct addrinfo *, const char *));
static struct addrinfo *get_ai __P((const struct addrinfo *,
	const struct afd *, const char *));
static struct addrinfo *copy_ai __P((const struct addrinfo *));
static int get_portmatch __P((const struct addrinfo *, const char *));
static int get_port __P((const struct addrinfo *, const char *, int));
static const struct afd *find_afd __P((int));
static int addrconfig __P((int));
static int ip6_str2scopeid __P((char *, struct sockaddr_in6 *,
				u_int32_t *scopeidp));
static struct net_data *init __P((void));

struct addrinfo *hostent2addrinfo __P((struct hostent *,
				       const struct addrinfo *));
struct addrinfo *addr2addrinfo __P((const struct addrinfo *,
				    const char *));

#if 0
static const char *ai_errlist[] = {
	"Success",
	"Address family for hostname not supported",	/*%< EAI_ADDRFAMILY */
	"Temporary failure in name resolution",		/*%< EAI_AGAIN */
	"Invalid value for ai_flags",		       	/*%< EAI_BADFLAGS */
	"Non-recoverable failure in name resolution", 	/*%< EAI_FAIL */
	"ai_family not supported",			/*%< EAI_FAMILY */
	"Memory allocation failure", 			/*%< EAI_MEMORY */
	"No address associated with hostname", 		/*%< EAI_NODATA */
	"hostname nor servname provided, or not known",	/*%< EAI_NONAME */
	"servname not supported for ai_socktype",	/*%< EAI_SERVICE */
	"ai_socktype not supported", 			/*%< EAI_SOCKTYPE */
	"System error returned in errno", 		/*%< EAI_SYSTEM */
	"Invalid value for hints",			/*%< EAI_BADHINTS */
	"Resolved protocol is unknown",			/*%< EAI_PROTOCOL */
	"Unknown error", 				/*%< EAI_MAX */
};
#endif

/* XXX macros that make external reference is BAD. */

#define GET_AI(ai, afd, addr) \
do { \
	/* external reference: pai, error, and label free */ \
	(ai) = get_ai(pai, (afd), (addr)); \
	if ((ai) == NULL) { \
		error = EAI_MEMORY; \
		goto free; \
	} \
} while (/*CONSTCOND*/0)

#define GET_PORT(ai, serv) \
do { \
	/* external reference: error and label free */ \
	error = get_port((ai), (serv), 0); \
	if (error != 0) \
		goto free; \
} while (/*CONSTCOND*/0)

#define GET_CANONNAME(ai, str) \
do { \
	/* external reference: pai, error and label free */ \
	error = get_canonname(pai, (ai), (str)); \
	if (error != 0) \
		goto free; \
} while (/*CONSTCOND*/0)

#ifndef SOLARIS2
#define SETERROR(err) \
do { \
	/* external reference: error, and label bad */ \
	error = (err); \
	goto bad; \
	/*NOTREACHED*/ \
} while (/*CONSTCOND*/0)
#else
#define SETERROR(err) \
do { \
	/* external reference: error, and label bad */ \
	error = (err); \
	if (error == error) \
		goto bad; \
} while (/*CONSTCOND*/0)
#endif


#define MATCH_FAMILY(x, y, w) \
	((x) == (y) || (/*CONSTCOND*/(w) && ((x) == PF_UNSPEC || (y) == PF_UNSPEC)))
#define MATCH(x, y, w) \
	((x) == (y) || (/*CONSTCOND*/(w) && ((x) == ANY || (y) == ANY)))

#if 0				/*%< bind8 has its own version */
char *
gai_strerror(ecode)
	int ecode;
{
	if (ecode < 0 || ecode > EAI_MAX)
		ecode = EAI_MAX;
	return ai_errlist[ecode];
}
#endif

void
freeaddrinfo(ai)
	struct addrinfo *ai;
{
	struct addrinfo *next;

	do {
		next = ai->ai_next;
		if (ai->ai_canonname)
			free(ai->ai_canonname);
		/* no need to free(ai->ai_addr) */
		free(ai);
		ai = next;
	} while (ai);
}

static int
str_isnumber(p)
	const char *p;
{
	char *ep;

	if (*p == '\0')
		return NO;
	ep = NULL;
	errno = 0;
	(void)strtoul(p, &ep, 10);
	if (errno == 0 && ep && *ep == '\0')
		return YES;
	else
		return NO;
}

int
getaddrinfo(hostname, servname, hints, res)
	const char *hostname, *servname;
	const struct addrinfo *hints;
	struct addrinfo **res;
{
	struct addrinfo sentinel;
	struct addrinfo *cur;
	int error = 0;
	struct addrinfo ai, ai0, *afai = NULL;
	struct addrinfo *pai;
	const struct explore *ex;

	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;
	pai = &ai;
	pai->ai_flags = 0;
	pai->ai_family = PF_UNSPEC;
	pai->ai_socktype = ANY;
	pai->ai_protocol = ANY;
#if defined(sun) && defined(_SOCKLEN_T) && defined(__sparcv9)
	/*
	 * clear _ai_pad to preserve binary
	 * compatibility with previously compiled 64-bit
	 * applications in a pre-SUSv3 environment by
	 * guaranteeing the upper 32-bits are empty.
	 */
	pai->_ai_pad = 0;
#endif
	pai->ai_addrlen = 0;
	pai->ai_canonname = NULL;
	pai->ai_addr = NULL;
	pai->ai_next = NULL;

	if (hostname == NULL && servname == NULL)
		return EAI_NONAME;
	if (hints) {
		/* error check for hints */
		if (hints->ai_addrlen || hints->ai_canonname ||
		    hints->ai_addr || hints->ai_next)
			SETERROR(EAI_BADHINTS); /*%< xxx */
		if (hints->ai_flags & ~AI_MASK)
			SETERROR(EAI_BADFLAGS);
		switch (hints->ai_family) {
		case PF_UNSPEC:
		case PF_INET:
		case PF_INET6:
			break;
		default:
			SETERROR(EAI_FAMILY);
		}
		memcpy(pai, hints, sizeof(*pai));

#if defined(sun) && defined(_SOCKLEN_T) && defined(__sparcv9)
		/*
		 * We need to clear _ai_pad to preserve binary
		 * compatibility.  See prior comment.
		 */
		pai->_ai_pad = 0;
#endif
		/*
		 * if both socktype/protocol are specified, check if they
		 * are meaningful combination.
		 */
		if (pai->ai_socktype != ANY && pai->ai_protocol != ANY) {
			for (ex = explore; ex->e_af >= 0; ex++) {
				if (pai->ai_family != ex->e_af)
					continue;
				if (ex->e_socktype == ANY)
					continue;
				if (ex->e_protocol == ANY)
					continue;
				if (pai->ai_socktype == ex->e_socktype &&
				    pai->ai_protocol != ex->e_protocol) {
					SETERROR(EAI_BADHINTS);
				}
			}
		}
	}

	/*
	 * post-2553: AI_ALL and AI_V4MAPPED are effective only against
	 * AF_INET6 query.  They needs to be ignored if specified in other
	 * occassions.
	 */
	switch (pai->ai_flags & (AI_ALL | AI_V4MAPPED)) {
	case AI_V4MAPPED:
	case AI_ALL | AI_V4MAPPED:
		if (pai->ai_family != AF_INET6)
			pai->ai_flags &= ~(AI_ALL | AI_V4MAPPED);
		break;
	case AI_ALL:
#if 1
		/* illegal */
		SETERROR(EAI_BADFLAGS);
#else
		pai->ai_flags &= ~(AI_ALL | AI_V4MAPPED);
		break;
#endif
	}

	/*
	 * check for special cases.  (1) numeric servname is disallowed if
	 * socktype/protocol are left unspecified. (2) servname is disallowed
	 * for raw and other inet{,6} sockets.
	 */
	if (MATCH_FAMILY(pai->ai_family, PF_INET, 1)
#ifdef PF_INET6
	 || MATCH_FAMILY(pai->ai_family, PF_INET6, 1)
#endif
	    ) {
		ai0 = *pai;	/* backup *pai */

		if (pai->ai_family == PF_UNSPEC) {
#ifdef PF_INET6
			pai->ai_family = PF_INET6;
#else
			pai->ai_family = PF_INET;
#endif
		}
		error = get_portmatch(pai, servname);
		if (error)
			SETERROR(error);

		*pai = ai0;
	}

	ai0 = *pai;

	/* NULL hostname, or numeric hostname */
	for (ex = explore; ex->e_af >= 0; ex++) {
		*pai = ai0;

		if (!MATCH_FAMILY(pai->ai_family, ex->e_af, WILD_AF(ex)))
			continue;
		if (!MATCH(pai->ai_socktype, ex->e_socktype, WILD_SOCKTYPE(ex)))
			continue;
		if (!MATCH(pai->ai_protocol, ex->e_protocol, WILD_PROTOCOL(ex)))
			continue;

		if (pai->ai_family == PF_UNSPEC)
			pai->ai_family = ex->e_af;
		if (pai->ai_socktype == ANY && ex->e_socktype != ANY)
			pai->ai_socktype = ex->e_socktype;
		if (pai->ai_protocol == ANY && ex->e_protocol != ANY)
			pai->ai_protocol = ex->e_protocol;

		/*
		 * if the servname does not match socktype/protocol, ignore it.
		 */
		if (get_portmatch(pai, servname) != 0)
			continue;

		if (hostname == NULL) {
			/*
			 * filter out AFs that are not supported by the kernel
			 * XXX errno?
			 */
			if (!addrconfig(pai->ai_family))
				continue;
			error = explore_null(pai, servname, &cur->ai_next);
		} else
			error = explore_numeric_scope(pai, hostname, servname,
			    &cur->ai_next);

		if (error)
			goto free;

		while (cur && cur->ai_next)
			cur = cur->ai_next;
	}

	/*
	 * XXX
	 * If numreic representation of AF1 can be interpreted as FQDN
	 * representation of AF2, we need to think again about the code below.
	 */
	if (sentinel.ai_next)
		goto good;

	if (pai->ai_flags & AI_NUMERICHOST)
		SETERROR(EAI_NONAME);
	if (hostname == NULL)
		SETERROR(EAI_NONAME);

	/*
	 * hostname as alphabetical name.
	 * We'll make sure that
	 * - if returning addrinfo list is empty, return non-zero error
	 *   value (already known one or EAI_NONAME).
	 * - otherwise, 
	 *   + if we haven't had any errors, return 0 (i.e. success).
	 *   + if we've had an error, free the list and return the error.
	 * without any assumption on the behavior of explore_fqdn().
	 */

	/* first, try to query DNS for all possible address families. */
	*pai = ai0;
	error = explore_fqdn(pai, hostname, servname, &afai);
	if (error) {
		if (afai != NULL)
			freeaddrinfo(afai);
		goto free;
	}
	if (afai == NULL) {
		error = EAI_NONAME; /*%< we've had no errors. */
		goto free;
	}

	/*
	 * we would like to prefer AF_INET6 than AF_INET, so we'll make an
	 * outer loop by AFs.
	 */
	for (ex = explore; ex->e_af >= 0; ex++) {
		*pai = ai0;

		if (pai->ai_family == PF_UNSPEC)
			pai->ai_family = ex->e_af;

		if (!MATCH_FAMILY(pai->ai_family, ex->e_af, WILD_AF(ex)))
			continue;
		if (!MATCH(pai->ai_socktype, ex->e_socktype,
			   WILD_SOCKTYPE(ex))) {
			continue;
		}
		if (!MATCH(pai->ai_protocol, ex->e_protocol,
			   WILD_PROTOCOL(ex))) {
			continue;
		}

#ifdef AI_ADDRCONFIG
		/*
		 * If AI_ADDRCONFIG is specified, check if we are
		 * expected to return the address family or not.
		 */
		if ((pai->ai_flags & AI_ADDRCONFIG) != 0 &&
		    !addrconfig(pai->ai_family))
			continue;
#endif

		if (pai->ai_family == PF_UNSPEC)
			pai->ai_family = ex->e_af;
		if (pai->ai_socktype == ANY && ex->e_socktype != ANY)
			pai->ai_socktype = ex->e_socktype;
		if (pai->ai_protocol == ANY && ex->e_protocol != ANY)
			pai->ai_protocol = ex->e_protocol;

		/*
		 * if the servname does not match socktype/protocol, ignore it.
		 */
		if (get_portmatch(pai, servname) != 0)
			continue;

		if ((error = explore_copy(pai, afai, &cur->ai_next)) != 0) {
			freeaddrinfo(afai);
			goto free;
		}

		while (cur && cur->ai_next)
			cur = cur->ai_next;
	}

	freeaddrinfo(afai);	/*%< afai must not be NULL at this point. */

	if (sentinel.ai_next) {
good:
		*res = sentinel.ai_next;
		return(SUCCESS);
	} else {
		/*
		 * All the process succeeded, but we've had an empty list. 
		 * This can happen if the given hints do not match our
		 * candidates.
		 */
		error = EAI_NONAME;
	}

free:
bad:
	if (sentinel.ai_next)
		freeaddrinfo(sentinel.ai_next);
	*res = NULL;
	return(error);
}

/*%
 * FQDN hostname, DNS lookup
 */
static int
explore_fqdn(pai, hostname, servname, res)
	const struct addrinfo *pai;
	const char *hostname;
	const char *servname;
	struct addrinfo **res;
{
	struct addrinfo *result;
	struct addrinfo *cur;
	struct net_data *net_data = init();
	struct irs_ho *ho;
	int error = 0;
	char tmp[NS_MAXDNAME];
	const char *cp;

	INSIST(res != NULL && *res == NULL);

	/*
	 * if the servname does not match socktype/protocol, ignore it.
	 */
	if (get_portmatch(pai, servname) != 0)
		return(0);

	if (!net_data || !(ho = net_data->ho))
		return(0);
#if 0				/*%< XXX (notyet) */
	if (net_data->ho_stayopen && net_data->ho_last &&
	    net_data->ho_last->h_addrtype == af) {
		if (ns_samename(name, net_data->ho_last->h_name) == 1)
			return (net_data->ho_last);
		for (hap = net_data->ho_last->h_aliases; hap && *hap; hap++)
			if (ns_samename(name, *hap) == 1)
				return (net_data->ho_last);
	}
#endif
	if (!strchr(hostname, '.') &&
	    (cp = res_hostalias(net_data->res, hostname,
				tmp, sizeof(tmp))))
		hostname = cp;
	result = (*ho->addrinfo)(ho, hostname, pai);
	if (!net_data->ho_stayopen) {
		(*ho->minimize)(ho);
	}
	if (result == NULL) {
		int e = h_errno;

		switch(e) {
		case NETDB_INTERNAL:
			error = EAI_SYSTEM;
			break;
		case TRY_AGAIN:
			error = EAI_AGAIN;
			break;
		case NO_RECOVERY:
			error = EAI_FAIL;
			break;
		case HOST_NOT_FOUND:
		case NO_DATA:
			error = EAI_NONAME;
			break;
		default:
		case NETDB_SUCCESS: /*%< should be impossible... */
			error = EAI_NONAME;
			break;
		}
		goto free;
	}

	for (cur = result; cur; cur = cur->ai_next) {
		GET_PORT(cur, servname); /*%< XXX: redundant lookups... */
		/* canonname should already be filled. */
	}

	*res = result;

	return(0);

free:
	if (result)
		freeaddrinfo(result);
	return error;
}

static int
explore_copy(pai, src0, res)
	const struct addrinfo *pai;	/*%< seed */
	const struct addrinfo *src0;	/*%< source */
	struct addrinfo **res;
{
	int error;
	struct addrinfo sentinel, *cur;
	const struct addrinfo *src;

	error = 0;
	sentinel.ai_next = NULL;
	cur = &sentinel;

	for (src = src0; src != NULL; src = src->ai_next) {
		if (src->ai_family != pai->ai_family)
			continue;

		cur->ai_next = copy_ai(src);
		if (!cur->ai_next) {
			error = EAI_MEMORY;
			goto fail;
		}

		cur->ai_next->ai_socktype = pai->ai_socktype;
		cur->ai_next->ai_protocol = pai->ai_protocol;
		cur = cur->ai_next;
	}

	*res = sentinel.ai_next;
	return 0;

fail:
	freeaddrinfo(sentinel.ai_next);
	return error;
}

/*%
 * hostname == NULL.
 * passive socket -> anyaddr (0.0.0.0 or ::)
 * non-passive socket -> localhost (127.0.0.1 or ::1)
 */
static int
explore_null(pai, servname, res)
	const struct addrinfo *pai;
	const char *servname;
	struct addrinfo **res;
{
	const struct afd *afd;
	struct addrinfo *cur;
	struct addrinfo sentinel;
	int error;

	*res = NULL;
	sentinel.ai_next = NULL;
	cur = &sentinel;

	afd = find_afd(pai->ai_family);
	if (afd == NULL)
		return 0;

	if (pai->ai_flags & AI_PASSIVE) {
		GET_AI(cur->ai_next, afd, afd->a_addrany);
		/* xxx meaningless?
		 * GET_CANONNAME(cur->ai_next, "anyaddr");
		 */
		GET_PORT(cur->ai_next, servname);
	} else {
		GET_AI(cur->ai_next, afd, afd->a_loopback);
		/* xxx meaningless?
		 * GET_CANONNAME(cur->ai_next, "localhost");
		 */
		GET_PORT(cur->ai_next, servname);
	}
	cur = cur->ai_next;

	*res = sentinel.ai_next;
	return 0;

free:
	if (sentinel.ai_next)
		freeaddrinfo(sentinel.ai_next);
	return error;
}

/*%
 * numeric hostname
 */
static int
explore_numeric(pai, hostname, servname, res)
	const struct addrinfo *pai;
	const char *hostname;
	const char *servname;
	struct addrinfo **res;
{
	const struct afd *afd;
	struct addrinfo *cur;
	struct addrinfo sentinel;
	int error;
	char pton[PTON_MAX];

	*res = NULL;
	sentinel.ai_next = NULL;
	cur = &sentinel;

	afd = find_afd(pai->ai_family);
	if (afd == NULL)
		return 0;

	switch (afd->a_af) {
#if 0 /*X/Open spec*/
	case AF_INET:
		if (inet_aton(hostname, (struct in_addr *)pton) == 1) {
			if (pai->ai_family == afd->a_af ||
			    pai->ai_family == PF_UNSPEC /*?*/) {
				GET_AI(cur->ai_next, afd, pton);
				GET_PORT(cur->ai_next, servname);
				while (cur->ai_next)
					cur = cur->ai_next;
			} else
				SETERROR(EAI_FAMILY);	/*xxx*/
		}
		break;
#endif
	default:
		if (inet_pton(afd->a_af, hostname, pton) == 1) {
			if (pai->ai_family == afd->a_af ||
			    pai->ai_family == PF_UNSPEC /*?*/) {
				GET_AI(cur->ai_next, afd, pton);
				GET_PORT(cur->ai_next, servname);
				while (cur->ai_next)
					cur = cur->ai_next;
			} else
				SETERROR(EAI_FAMILY);	/*xxx*/
		}
		break;
	}

	*res = sentinel.ai_next;
	return 0;

free:
bad:
	if (sentinel.ai_next)
		freeaddrinfo(sentinel.ai_next);
	return error;
}

/*%
 * numeric hostname with scope
 */
static int
explore_numeric_scope(pai, hostname, servname, res)
	const struct addrinfo *pai;
	const char *hostname;
	const char *servname;
	struct addrinfo **res;
{
#ifndef SCOPE_DELIMITER
	return explore_numeric(pai, hostname, servname, res);
#else
	const struct afd *afd;
	struct addrinfo *cur;
	int error;
	char *cp, *hostname2 = NULL, *scope, *addr;
	struct sockaddr_in6 *sin6;

	afd = find_afd(pai->ai_family);
	if (afd == NULL)
		return 0;

	if (!afd->a_scoped)
		return explore_numeric(pai, hostname, servname, res);

	cp = strchr(hostname, SCOPE_DELIMITER);
	if (cp == NULL)
		return explore_numeric(pai, hostname, servname, res);

	/*
	 * Handle special case of <scoped_address><delimiter><scope id>
	 */
	hostname2 = strdup(hostname);
	if (hostname2 == NULL)
		return EAI_MEMORY;
	/* terminate at the delimiter */
	hostname2[cp - hostname] = '\0';
	addr = hostname2;
	scope = cp + 1;

	error = explore_numeric(pai, addr, servname, res);
	if (error == 0) {
		u_int32_t scopeid = 0;

		for (cur = *res; cur; cur = cur->ai_next) {
			if (cur->ai_family != AF_INET6)
				continue;
			sin6 = (struct sockaddr_in6 *)(void *)cur->ai_addr;
			if (!ip6_str2scopeid(scope, sin6, &scopeid)) {
				free(hostname2);
				return(EAI_NONAME); /*%< XXX: is return OK? */
			}
#ifdef HAVE_SIN6_SCOPE_ID
			sin6->sin6_scope_id = scopeid;
#endif
		}
	}

	free(hostname2);

	return error;
#endif
}

static int
get_canonname(pai, ai, str)
	const struct addrinfo *pai;
	struct addrinfo *ai;
	const char *str;
{
	if ((pai->ai_flags & AI_CANONNAME) != 0) {
		ai->ai_canonname = (char *)malloc(strlen(str) + 1);
		if (ai->ai_canonname == NULL)
			return EAI_MEMORY;
		strcpy(ai->ai_canonname, str);
	}
	return 0;
}

static struct addrinfo *
get_ai(pai, afd, addr)
	const struct addrinfo *pai;
	const struct afd *afd;
	const char *addr;
{
	char *p;
	struct addrinfo *ai;

	ai = (struct addrinfo *)malloc(sizeof(struct addrinfo)
		+ (afd->a_socklen));
	if (ai == NULL)
		return NULL;

	memcpy(ai, pai, sizeof(struct addrinfo));
	ai->ai_addr = (struct sockaddr *)(void *)(ai + 1);
	memset(ai->ai_addr, 0, (size_t)afd->a_socklen);
#ifdef HAVE_SA_LEN
	ai->ai_addr->sa_len = afd->a_socklen;
#endif
	ai->ai_addrlen = afd->a_socklen;
	ai->ai_addr->sa_family = ai->ai_family = afd->a_af;
	p = (char *)(void *)(ai->ai_addr);
	memcpy(p + afd->a_off, addr, (size_t)afd->a_addrlen);
	return ai;
}

/* XXX need to malloc() the same way we do from other functions! */
static struct addrinfo *
copy_ai(pai)
	const struct addrinfo *pai;
{
	struct addrinfo *ai;
	size_t l;

	l = sizeof(*ai) + pai->ai_addrlen;
	if ((ai = (struct addrinfo *)malloc(l)) == NULL)
		return NULL;
	memset(ai, 0, l);
	memcpy(ai, pai, sizeof(*ai));
	ai->ai_addr = (struct sockaddr *)(void *)(ai + 1);
	memcpy(ai->ai_addr, pai->ai_addr, pai->ai_addrlen);

	if (pai->ai_canonname) {
		l = strlen(pai->ai_canonname) + 1;
		if ((ai->ai_canonname = malloc(l)) == NULL) {
			free(ai);
			return NULL;
		}
		strcpy(ai->ai_canonname, pai->ai_canonname);	/* (checked) */
	} else {
		/* just to make sure */
		ai->ai_canonname = NULL;
	}

	ai->ai_next = NULL;

	return ai;
}

static int
get_portmatch(const struct addrinfo *ai, const char *servname) {

	/* get_port does not touch first argument. when matchonly == 1. */
	/* LINTED const cast */
	return get_port((const struct addrinfo *)ai, servname, 1);
}

static int
get_port(const struct addrinfo *ai, const char *servname, int matchonly) {
	const char *proto;
	struct servent *sp;
	int port;
	int allownumeric;

	if (servname == NULL)
		return 0;
	switch (ai->ai_family) {
	case AF_INET:
#ifdef AF_INET6
	case AF_INET6:
#endif
		break;
	default:
		return 0;
	}

	switch (ai->ai_socktype) {
	case SOCK_RAW:
		return EAI_SERVICE;
	case SOCK_DGRAM:
	case SOCK_STREAM:
		allownumeric = 1;
		break;
	case ANY:
		switch (ai->ai_family) {
		case AF_INET:
#ifdef AF_INET6
		case AF_INET6:
#endif
			allownumeric = 1;
			break;
		default:
			allownumeric = 0;
			break;
		}
		break;
	default:
		return EAI_SOCKTYPE;
	}

	if (str_isnumber(servname)) {
		if (!allownumeric)
			return EAI_SERVICE;
		port = atoi(servname);
		if (port < 0 || port > 65535)
			return EAI_SERVICE;
		port = htons(port);
	} else {
		switch (ai->ai_socktype) {
		case SOCK_DGRAM:
			proto = "udp";
			break;
		case SOCK_STREAM:
			proto = "tcp";
			break;
		default:
			proto = NULL;
			break;
		}

		if ((sp = getservbyname(servname, proto)) == NULL)
			return EAI_SERVICE;
		port = sp->s_port;
	}

	if (!matchonly) {
		switch (ai->ai_family) {
		case AF_INET:
			((struct sockaddr_in *)(void *)
			    ai->ai_addr)->sin_port = port;
			break;
		case AF_INET6:
			((struct sockaddr_in6 *)(void *)
			    ai->ai_addr)->sin6_port = port;
			break;
		}
	}

	return 0;
}

static const struct afd *
find_afd(af)
	int af;
{
	const struct afd *afd;

	if (af == PF_UNSPEC)
		return NULL;
	for (afd = afdl; afd->a_af; afd++) {
		if (afd->a_af == af)
			return afd;
	}
	return NULL;
}

/*%
 * post-2553: AI_ADDRCONFIG check.  if we use getipnodeby* as backend, backend
 * will take care of it.
 * the semantics of AI_ADDRCONFIG is not defined well.  we are not sure
 * if the code is right or not.
 */
static int
addrconfig(af)
	int af;
{
	int s;

	/* XXX errno */
	s = socket(af, SOCK_DGRAM, 0);
	if (s < 0) {
		if (errno != EMFILE)
			return 0;
	} else
		close(s);
	return 1;
}

/* convert a string to a scope identifier. XXX: IPv6 specific */
static int
ip6_str2scopeid(char *scope, struct sockaddr_in6 *sin6,
		u_int32_t *scopeidp)
{
	u_int32_t scopeid;
	u_long lscopeid;
	struct in6_addr *a6 = &sin6->sin6_addr;
	char *ep;
	
	/* empty scopeid portion is invalid */
	if (*scope == '\0')
		return (0);

#ifdef USE_IFNAMELINKID
	if (IN6_IS_ADDR_LINKLOCAL(a6) || IN6_IS_ADDR_MC_LINKLOCAL(a6) ||
	    IN6_IS_ADDR_MC_NODELOCAL(a6)) {
		/*
		 * Using interface names as link indices can be allowed
		 * only when we can assume a one-to-one mappings between
		 * links and interfaces.  See comments in getnameinfo.c.
		 */
		scopeid = if_nametoindex(scope);
		if (scopeid == 0)
			goto trynumeric;
		*scopeidp = scopeid;
		return (1);
	}
#endif

	/* still unclear about literal, allow numeric only - placeholder */
	if (IN6_IS_ADDR_SITELOCAL(a6) || IN6_IS_ADDR_MC_SITELOCAL(a6))
		goto trynumeric;
	if (IN6_IS_ADDR_MC_ORGLOCAL(a6))
		goto trynumeric;
	else
		goto trynumeric;	/*%< global */
	/* try to convert to a numeric id as a last resort */
trynumeric:
	errno = 0;
	lscopeid = strtoul(scope, &ep, 10);
	scopeid = lscopeid & 0xffffffff;
	if (errno == 0 && ep && *ep == '\0' && scopeid == lscopeid) {
		*scopeidp = scopeid;
		return (1);
	} else
		return (0);
}

struct addrinfo *
hostent2addrinfo(hp, pai)
	struct hostent *hp;
	const struct addrinfo *pai;
{
	int i, af, error = 0;
	char **aplist = NULL, *ap;
	struct addrinfo sentinel, *cur;
	const struct afd *afd;

	af = hp->h_addrtype;
	if (pai->ai_family != AF_UNSPEC && af != pai->ai_family)
		return(NULL);

	afd = find_afd(af);
	if (afd == NULL)
		return(NULL);

	aplist = hp->h_addr_list;

	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	for (i = 0; (ap = aplist[i]) != NULL; i++) {
#if 0				/*%< the trick seems too much */
		af = hp->h_addr_list;
		if (af == AF_INET6 &&
		    IN6_IS_ADDR_V4MAPPED((struct in6_addr *)ap)) {
			af = AF_INET;
			ap = ap + sizeof(struct in6_addr)
				- sizeof(struct in_addr);
		}
		afd = find_afd(af);
		if (afd == NULL)
			continue;
#endif /* 0 */

		GET_AI(cur->ai_next, afd, ap);

		/* GET_PORT(cur->ai_next, servname); */
		if ((pai->ai_flags & AI_CANONNAME) != 0) {
			/*
			 * RFC2553 says that ai_canonname will be set only for
			 * the first element.  we do it for all the elements,
			 * just for convenience.
			 */
			GET_CANONNAME(cur->ai_next, hp->h_name);
		}
		while (cur->ai_next) /*%< no need to loop, actually. */
			cur = cur->ai_next;
		continue;

	free:
		if (cur->ai_next)
			freeaddrinfo(cur->ai_next);
		cur->ai_next = NULL;
		/* continue, without tht pointer CUR advanced. */
	}

	return(sentinel.ai_next);
}

struct addrinfo *
addr2addrinfo(pai, cp)
	const struct addrinfo *pai;
	const char *cp;
{
	const struct afd *afd;

	afd = find_afd(pai->ai_family);
	if (afd == NULL)
		return(NULL);

	return(get_ai(pai, afd, cp));
}

static struct net_data *
init()
{
	struct net_data *net_data;

	if (!(net_data = net_data_init(NULL)))
		goto error;
	if (!net_data->ho) {
		net_data->ho = (*net_data->irs->ho_map)(net_data->irs);
		if (!net_data->ho || !net_data->res) {
error:
			errno = EIO;
			if (net_data && net_data->res)
				RES_SET_H_ERRNO(net_data->res, NETDB_INTERNAL);
			return (NULL);
		}

		(*net_data->ho->res_set)(net_data->ho, net_data->res, NULL);
	}

	return (net_data);
}
