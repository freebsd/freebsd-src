/*	$NetBSD: getaddrinfo.c,v 1.5 2007/07/22 05:19:01 lukem Exp $	*/
/*	from	?	*/

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

/*
 * Issues to be discussed:
 * - Thread safe-ness must be checked.
 * - Return values.  There are nonstandard return values defined and used
 *   in the source code.  This is because RFC2553 is silent about which error
 *   code must be returned for which situation.
 * - IPv4 classful (shortened) form.  RFC2553 is silent about it.  XNET 5.2
 *   says to use inet_aton() to convert IPv4 numeric to binary (alows
 *   classful form as a result).
 *   current code - disallow classful form for IPv4 (due to use of inet_pton).
 * - freeaddrinfo(NULL).  RFC2553 is silent about it.  XNET 5.2 says it is
 *   invalid.
 *   current code - SEGV on freeaddrinfo(NULL)
 * Note:
 * - We use getipnodebyname() just for thread-safeness.  There's no intent
 *   to let it do PF_UNSPEC (actually we never pass PF_UNSPEC to
 *   getipnodebyname().
 * - The code filters out AFs that are not supported by the kernel,
 *   when globbing NULL hostname (to loopback, or wildcard).  Is it the right
 *   thing to do?  What is the relationship with post-RFC2553 AI_ADDRCONFIG
 *   in ai_flags?
 * - (post-2553) semantics of AI_ADDRCONFIG itself is too vague.
 *   (1) what should we do against numeric hostname (2) what should we do
 *   against NULL hostname (3) what is AI_ADDRCONFIG itself.  AF not ready?
 *   non-loopback address configured?  global address configured?
 * - The code makes use of following calls when asked to resolver with
 *   ai_family  = PF_UNSPEC:
 *	getipnodebyname(host, AF_INET6);
 *	getipnodebyname(host, AF_INET);
 *   This will result in the following queries if the node is configure to
 *   prefer /etc/hosts than DNS:
 *	lookup /etc/hosts for IPv6 address
 *	lookup DNS for IPv6 address
 *	lookup /etc/hosts for IPv4 address
 *	lookup DNS for IPv4 address
 *   which may not meet people's requirement.
 *   The right thing to happen is to have underlying layer which does
 *   PF_UNSPEC lookup (lookup both) and return chain of addrinfos.
 *   This would result in a bit of code duplicate with _dns_ghbyname() and
 *   friends.
 */

#include "tnftp.h"

#define SUCCESS 0
#define ANY 0
#define YES 1
#define NO  0

static const char in_addrany[] = { 0, 0, 0, 0 };
static const char in_loopback[] = { 127, 0, 0, 1 };
#ifdef INET6
static const char in6_addrany[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static const char in6_loopback[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};
#endif

static const struct afd {
	int a_af;
	int a_addrlen;
	int a_socklen;
	int a_off;
	const char *a_addrany;
	const char *a_loopback;	
	int a_scoped;
} afdl [] = {
#ifdef INET6
	{PF_INET6, sizeof(struct in6_addr),
	 sizeof(struct sockaddr_in6),
	 offsetof(struct sockaddr_in6, sin6_addr),
	 in6_addrany, in6_loopback, 1},
#endif
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
#ifdef INET6
	{ PF_INET6, SOCK_DGRAM, IPPROTO_UDP, "udp", 0x07 },
	{ PF_INET6, SOCK_STREAM, IPPROTO_TCP, "tcp", 0x07 },
	{ PF_INET6, SOCK_RAW, ANY, NULL, 0x05 },
#endif
	{ PF_INET, SOCK_DGRAM, IPPROTO_UDP, "udp", 0x07 },
	{ PF_INET, SOCK_STREAM, IPPROTO_TCP, "tcp", 0x07 },
	{ PF_INET, SOCK_RAW, ANY, NULL, 0x05 },
	{ -1, 0, 0, NULL, 0 },
};

#ifdef INET6
#define PTON_MAX	16
#else
#define PTON_MAX	4
#endif


static int str_isnumber(const char *);
static int explore_fqdn(const struct addrinfo *, const char *,
	const char *, struct addrinfo **);
static int explore_null(const struct addrinfo *,
	const char *, struct addrinfo **);
static int explore_numeric(const struct addrinfo *, const char *,
	const char *, struct addrinfo **);
static int explore_numeric_scope(const struct addrinfo *, const char *,
	const char *, struct addrinfo **);
static int get_canonname(const struct addrinfo *,
	struct addrinfo *, const char *);
static struct addrinfo *get_ai(const struct addrinfo *,
	const struct afd *, const char *);
static int get_portmatch(const struct addrinfo *, const char *);
static int get_port(struct addrinfo *, const char *, int);
static const struct afd *find_afd(int);
static int addrconfig(const struct addrinfo *);
#ifdef INET6
static int ip6_str2scopeid(char *, struct sockaddr_in6 *);
#endif

static char *ai_errlist[] = {
	"Success",
	"Address family for hostname not supported",	/* EAI_ADDRFAMILY */
	"Temporary failure in name resolution",		/* EAI_AGAIN      */
	"Invalid value for ai_flags",		       	/* EAI_BADFLAGS   */
	"Non-recoverable failure in name resolution", 	/* EAI_FAIL       */
	"ai_family not supported",			/* EAI_FAMILY     */
	"Memory allocation failure", 			/* EAI_MEMORY     */
	"No address associated with hostname", 		/* EAI_NODATA     */
	"hostname nor servname provided, or not known",	/* EAI_NONAME     */
	"servname not supported for ai_socktype",	/* EAI_SERVICE    */
	"ai_socktype not supported", 			/* EAI_SOCKTYPE   */
	"System error returned in errno", 		/* EAI_SYSTEM     */
	"Invalid value for hints",			/* EAI_BADHINTS	  */
	"Resolved protocol is unknown",			/* EAI_PROTOCOL   */
	"Unknown error", 				/* EAI_MAX        */
};

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

#define ERR(err) \
do { \
	/* external reference: error, and label bad */ \
	error = (err); \
	goto bad; \
	/*NOTREACHED*/ \
} while (/*CONSTCOND*/0)

#define MATCH_FAMILY(x, y, w) \
	((x) == (y) || (/*CONSTCOND*/(w) && ((x) == PF_UNSPEC || (y) == PF_UNSPEC)))
#define MATCH(x, y, w) \
	((x) == (y) || (/*CONSTCOND*/(w) && ((x) == ANY || (y) == ANY)))

const char *
gai_strerror(int ecode)
{
	if (ecode < 0 || ecode > EAI_MAX)
		ecode = EAI_MAX;
	return ai_errlist[ecode];
}

void
freeaddrinfo(struct addrinfo *ai)
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
str_isnumber(const char *p)
{
	char *ep;
	long l;

	if (*p == '\0')
		return NO;
	ep = NULL;
	l = strtol(p, &ep, 10);
	if (ep && *ep == '\0' && l >= 0)
		return YES;
	else
		return NO;
}

int
getaddrinfo(const char *hostname, const char *servname,
	const struct addrinfo *hints, struct addrinfo **res)
{
	struct addrinfo sentinel;
	struct addrinfo *cur;
	int error = 0;
	struct addrinfo ai;
	struct addrinfo ai0;
	struct addrinfo *pai;
	const struct afd *afd;
	const struct explore *ex;

	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;
	pai = &ai;
	pai->ai_flags = 0;
	pai->ai_family = PF_UNSPEC;
	pai->ai_socktype = ANY;
	pai->ai_protocol = ANY;
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
			ERR(EAI_BADHINTS); /* xxx */
		if (hints->ai_flags & ~AI_MASK)
			ERR(EAI_BADFLAGS);
		switch (hints->ai_family) {
		case PF_UNSPEC:
		case PF_INET:
#ifdef INET6
		case PF_INET6:
#endif
			break;
		default:
			ERR(EAI_FAMILY);
		}
		memcpy(pai, hints, sizeof(*pai));

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
				if (pai->ai_socktype == ex->e_socktype
				 && pai->ai_protocol != ex->e_protocol) {
					ERR(EAI_BADHINTS);
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
#ifdef INET6	/* XXXLUKEM */
		if (pai->ai_family != AF_INET6)
			pai->ai_flags &= ~(AI_ALL | AI_V4MAPPED);
		break;
#endif
	case AI_ALL:
#if 1
		/* illegal */
		ERR(EAI_BADFLAGS);
#else
		pai->ai_flags &= ~(AI_ALL | AI_V4MAPPED);
#endif
		break;
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
			ERR(error);

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

		if (hostname == NULL)
			error = explore_null(pai, servname, &cur->ai_next);
		else
			error = explore_numeric_scope(pai, hostname, servname, &cur->ai_next);

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
		ERR(EAI_NODATA);
	if (hostname == NULL)
		ERR(EAI_NODATA);

	/*
	 * hostname as alphabetical name.
	 * we would like to prefer AF_INET6 than AF_INET, so we'll make a
	 * outer loop by AFs.
	 */
	for (afd = afdl; afd->a_af; afd++) {
		*pai = ai0;

		if (!MATCH_FAMILY(pai->ai_family, afd->a_af, 1))
			continue;

		for (ex = explore; ex->e_af >= 0; ex++) {
			*pai = ai0;

			if (pai->ai_family == PF_UNSPEC)
				pai->ai_family = afd->a_af;

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

			if (pai->ai_family == PF_UNSPEC)
				pai->ai_family = ex->e_af;
			if (pai->ai_socktype == ANY && ex->e_socktype != ANY)
				pai->ai_socktype = ex->e_socktype;
			if (pai->ai_protocol == ANY && ex->e_protocol != ANY)
				pai->ai_protocol = ex->e_protocol;

			error = explore_fqdn(pai, hostname, servname,
				&cur->ai_next);

			while (cur && cur->ai_next)
				cur = cur->ai_next;
		}
	}

	/* XXX */
	if (sentinel.ai_next)
		error = 0;

	if (error)
		goto free;
	if (error == 0) {
		if (sentinel.ai_next) {
 good:
			*res = sentinel.ai_next;
			return SUCCESS;
		} else
			error = EAI_FAIL;
	}
 free:
 bad:
	if (sentinel.ai_next)
		freeaddrinfo(sentinel.ai_next);
	*res = NULL;
	return error;
}

/*
 * FQDN hostname, DNS lookup
 */
static int
explore_fqdn(const struct addrinfo *pai, const char *hostname,
	const char *servname, struct addrinfo **res)
{
	struct hostent *hp;
	int h_error;
	int af;
	char **aplist = NULL, *apbuf = NULL;
	char *ap;
	struct addrinfo sentinel, *cur;
	int i;
#ifndef USE_GETIPNODEBY
	int naddrs;
#endif
	const struct afd *afd;
	int error = 0;
#if 0
	struct addrinfo pai4;
#ifdef INET6
	struct addrinfo pai6;
#endif
#endif

	*res = NULL;
	sentinel.ai_next = NULL;
	cur = &sentinel;

	/*
	 * If AI_ADDRCONFIG is specified, check if we are expected to
	 * return the address family or not.
	 * assumes PF_UNSPEC = PF_INET + PF_INET6.
	 *
	 * NOTE: PF_UNSPEC case is for future use.
	 */
	if ((pai->ai_flags & AI_ADDRCONFIG) != 0) {
		switch (pai->ai_family) {
#if 0
		case PF_UNSPEC:
			pai4 = pai6 = *pai;
			pai4.ai_family = PF_INET;
#ifndef INET6
			if (!addrconfig(&pai4))
				return 0;
#else
			pai6.ai_family = PF_INET6;
			if (!addrconfig(&pai4)) {
				if (!addrconfig(&pai6))
					return 0;
				pai = &pai6;
			} else {
				if (!addrconfig(&pai6))
					pai = &pai4;
				else
					; /* as is */
			}
#endif
			break;
#endif
		default:
			if (!addrconfig(pai))
				return 0;
			break;
		}
	}

	/*
	 * if the servname does not match socktype/protocol, ignore it.
	 */
	if (get_portmatch(pai, servname) != 0)
		return 0;

	afd = find_afd(pai->ai_family);
	if (afd == NULL)
		return 0;

#ifdef USE_GETIPNODEBY
	hp = getipnodebyname(hostname, pai->ai_family,
	    pai->ai_flags & AI_ADDRCONFIG, &h_error);
#else
#if defined(HAVE_GETHOSTBYNAME2)
	hp = gethostbyname2(hostname, pai->ai_family);
#else
	if (pai->ai_family != AF_INET)
		return 0;
	hp = gethostbyname(hostname);
#endif /* defined(HAVE_GETHOSTBYNAME2) */
#if defined(HAVE_H_ERRNO)
	h_error = h_errno;
#else
	h_error = EINVAL;
#endif
#endif /*USE_GETIPNODEBY*/

	if (hp == NULL) {
		switch (h_error) {
		case HOST_NOT_FOUND:
		case NO_DATA:
			error = EAI_NODATA;
			break;
		case TRY_AGAIN:
			error = EAI_AGAIN;
			break;
		case NO_RECOVERY:
#ifdef NETDB_INTERNAL
		case NETDB_INTERNAL:
#endif
		default:
			error = EAI_FAIL;
			break;
		}
	} else if ((hp->h_name == NULL) || (hp->h_name[0] == 0)
			|| (hp->h_addr_list[0] == NULL)) {
#ifdef USE_GETIPNODEBY
		freehostent(hp);
#endif
		hp = NULL;
		error = EAI_FAIL;
	}

	if (hp == NULL)
		goto free;

#ifdef USE_GETIPNODEBY
	aplist = hp->h_addr_list;
#else
	/*
	 * hp will be overwritten if we use gethostbyname2().
	 * always deep copy for simplification.
	 */
	for (naddrs = 0; hp->h_addr_list[naddrs] != NULL; naddrs++)
		;
	naddrs++;
	aplist = (char **)malloc(sizeof(aplist[0]) * naddrs);
	apbuf = (char *)malloc((size_t)hp->h_length * naddrs);
	if (aplist == NULL || apbuf == NULL) {
		error = EAI_MEMORY;
		goto free;
	}
	memset(aplist, 0, sizeof(aplist[0]) * naddrs);
	for (i = 0; i < naddrs; i++) {
		if (hp->h_addr_list[i] == NULL) {
			aplist[i] = NULL;
			continue;
		}
		memcpy(&apbuf[i * hp->h_length], hp->h_addr_list[i],
			(size_t)hp->h_length);
		aplist[i] = &apbuf[i * hp->h_length];
	}
#endif

	for (i = 0; aplist[i] != NULL; i++) {
		af = hp->h_addrtype;
		ap = aplist[i];
#ifdef INET6
		if (af == AF_INET6
		 && IN6_IS_ADDR_V4MAPPED((struct in6_addr *)ap)) {
			af = AF_INET;
			ap = ap + sizeof(struct in6_addr)
				- sizeof(struct in_addr);
		}
#endif

		if (af != pai->ai_family)
			continue;

		GET_AI(cur->ai_next, afd, ap);
		GET_PORT(cur->ai_next, servname);
		if ((pai->ai_flags & AI_CANONNAME) != 0) {
			/*
			 * RFC2553 says that ai_canonname will be set only for
			 * the first element.  we do it for all the elements,
			 * just for convenience.
			 */
			GET_CANONNAME(cur->ai_next, hp->h_name);
		}

		while (cur && cur->ai_next)
			cur = cur->ai_next;
	}

	*res = sentinel.ai_next;
	return 0;

free:
#ifdef USE_GETIPNODEBY
	if (hp)
		freehostent(hp);
#endif
	if (aplist)
		free(aplist);
	if (apbuf)
		free(apbuf);
	if (sentinel.ai_next)
		freeaddrinfo(sentinel.ai_next);
	return error;
}

/*
 * hostname == NULL.
 * passive socket -> anyaddr (0.0.0.0 or ::)
 * non-passive socket -> localhost (127.0.0.1 or ::1)
 */
static int
explore_null(const struct addrinfo *pai, const char *servname,
	struct addrinfo **res)
{
	const struct afd *afd;
	struct addrinfo *cur;
	struct addrinfo sentinel;
	int error;

	*res = NULL;
	sentinel.ai_next = NULL;
	cur = &sentinel;

	/*
	 * filter out AFs that are not supported by the kernel
	 * XXX errno?
	 */
	if (!addrconfig(pai))
		return 0;

	/*
	 * if the servname does not match socktype/protocol, ignore it.
	 */
	if (get_portmatch(pai, servname) != 0)
		return 0;

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

/*
 * numeric hostname
 */
static int
explore_numeric(const struct addrinfo *pai, const char *hostname,
	const char *servname, struct addrinfo **res)
{
	const struct afd *afd;
	struct addrinfo *cur;
	struct addrinfo sentinel;
	int error;
	char pton[PTON_MAX];

	*res = NULL;
	sentinel.ai_next = NULL;
	cur = &sentinel;

	/*
	 * if the servname does not match socktype/protocol, ignore it.
	 */
	if (get_portmatch(pai, servname) != 0)
		return 0;

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
				while (cur && cur->ai_next)
					cur = cur->ai_next;
			} else
				ERR(EAI_FAMILY);	/*xxx*/
		}
		break;
#endif
	default:
		if (inet_pton(afd->a_af, hostname, pton) == 1) {
			if (pai->ai_family == afd->a_af ||
			    pai->ai_family == PF_UNSPEC /*?*/) {
				GET_AI(cur->ai_next, afd, pton);
				GET_PORT(cur->ai_next, servname);
				while (cur && cur->ai_next)
					cur = cur->ai_next;
			} else
				ERR(EAI_FAMILY);	/*xxx*/
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

/*
 * numeric hostname with scope
 */
static int
explore_numeric_scope(const struct addrinfo *pai, const char *hostname,
	const char *servname, struct addrinfo **res)
{
#if !defined(SCOPE_DELIMITER) || !defined(INET6)
	return explore_numeric(pai, hostname, servname, res);
#else
	const struct afd *afd;
	struct addrinfo *cur;
	int error;
	char *cp, *hostname2 = NULL, *scope, *addr;
	struct sockaddr_in6 *sin6;

	/*
	 * if the servname does not match socktype/protocol, ignore it.
	 */
	if (get_portmatch(pai, servname) != 0)
		return 0;

	afd = find_afd(pai->ai_family);
	if (afd == NULL)
		return 0;

	if (!afd->a_scoped)
		return explore_numeric(pai, hostname, servname, res);

	cp = strchr(hostname, SCOPE_DELIMITER);
	if (cp == NULL)
		return explore_numeric(pai, hostname, servname, res);

#if 0
	/*
	 * Handle special case of <scope id><delimiter><scoped_address>
	 */
	hostname2 = strdup(hostname);
	if (hostname2 == NULL)
		return EAI_MEMORY;
	/* terminate at the delimiter */
	hostname2[cp - hostname] = '\0';
	scope = hostname2;
	addr = cp + 1;
#else
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
#endif

	error = explore_numeric(pai, addr, servname, res);
	if (error == 0) {
		int scopeid;

		for (cur = *res; cur; cur = cur->ai_next) {
			if (cur->ai_family != AF_INET6)
				continue;
			sin6 = (struct sockaddr_in6 *)(void *)cur->ai_addr;
			if ((scopeid = ip6_str2scopeid(scope, sin6)) == -1) {
				free(hostname2);
				return(EAI_NODATA); /* XXX: is return OK? */
			}
			sin6->sin6_scope_id = scopeid;
		}
	}

	free(hostname2);

	return error;
#endif
}

static int
get_canonname(const struct addrinfo *pai, struct addrinfo *ai, const char *str)
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
get_ai(const struct addrinfo *pai, const struct afd *afd, const char *addr)
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
#if defined(HAVE_STRUCT_SOCKADDR_SA_LEN)
	ai->ai_addr->sa_len = afd->a_socklen;
#endif
	ai->ai_addrlen = afd->a_socklen;
	ai->ai_addr->sa_family = ai->ai_family = afd->a_af;
	p = (char *)(void *)(ai->ai_addr);
	memcpy(p + afd->a_off, addr, (size_t)afd->a_addrlen);
	return ai;
}

static int
get_portmatch(const struct addrinfo *ai, const char *servname)
{

	/* get_port does not touch first argument. when matchonly == 1. */
	/* LINTED const cast */
	return get_port((struct addrinfo *)ai, servname, 1);
}

static int
get_port(struct addrinfo *ai, const char *servname, int matchonly)
{
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
		allownumeric = 0;
		break;
	default:
		return EAI_SOCKTYPE;
	}

	if (str_isnumber(servname)) {
		if (!allownumeric)
			return EAI_SERVICE;
		port = htons(atoi(servname));
		if (port < 0 || port > 65535)
			return EAI_SERVICE;
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
#ifdef INET6
		case AF_INET6:
			((struct sockaddr_in6 *)(void *)
			    ai->ai_addr)->sin6_port = port;
			break;
#endif
		}
	}

	return 0;
}

static const struct afd *
find_afd(int af)
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

/*
 * post-2553: AI_ADDRCONFIG check.  if we use getipnodeby* as backend, backend
 * will take care of it.
 * the semantics of AI_ADDRCONFIG is not defined well.  we are not sure
 * if the code is right or not.
 */
static int
addrconfig(const struct addrinfo *pai)
{
#ifdef USE_GETIPNODEBY
	return 1;
#else
	int s;

	/* XXX errno */
	s = socket(pai->ai_family, SOCK_DGRAM, 0);
	if (s < 0) {
		if (errno != EMFILE)
			return 0;
	} else
		close(s);
	return 1;
#endif
}

#ifdef INET6
/* convert a string to a scope identifier. XXX: IPv6 specific */
static int
ip6_str2scopeid(char *scope, struct sockaddr_in6 *sin6)
{
	int scopeid;
	struct in6_addr *a6 = &sin6->sin6_addr;
	char *ep;

	/* empty scopeid portion is invalid */
	if (*scope == '\0')
		return -1;

	if (IN6_IS_ADDR_LINKLOCAL(a6) || IN6_IS_ADDR_MC_LINKLOCAL(a6)) {
		/*
		 * We currently assume a one-to-one mapping between links
		 * and interfaces, so we simply use interface indices for
		 * like-local scopes.
		 */
		scopeid = if_nametoindex(scope);
		if (scopeid == 0)
			goto trynumeric;
		return(scopeid);
	}

	/* still unclear about literal, allow numeric only - placeholder */
	if (IN6_IS_ADDR_SITELOCAL(a6) || IN6_IS_ADDR_MC_SITELOCAL(a6))
		goto trynumeric;
	if (IN6_IS_ADDR_MC_ORGLOCAL(a6))
		goto trynumeric;
	else
		goto trynumeric;	/* global */

	/* try to convert to a numeric id as a last resort */
  trynumeric:
	scopeid = (int)strtoul(scope, &ep, 10);
	if (*ep == '\0')
		return scopeid;
	else
		return -1;
}
#endif
