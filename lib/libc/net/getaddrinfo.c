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
 *
 * $FreeBSD$
 */

/*
 * "#ifdef FAITH" part is local hack for supporting IPv4-v6 translator.
 *
 * Issues to be discussed:
 * - Thread safe-ness must be checked.
 * - Return values.  There are nonstandard return values defined and used
 *   in the source code.  This is because RFC2553 is silent about which error
 *   code must be returned for which situation.
 * Note:
 * - We use getipnodebyname() just for thread-safeness.  There's no intent
 *   to let it do PF_UNSPEC (actually we never pass PF_UNSPEC to
 *   getipnodebyname().
 * - The code filters out AFs that are not supported by the kernel,
 *   when globbing NULL hostname (to loopback, or wildcard).  Is it the right
 *   thing to do?  What is the relationship with post-RFC2553 AI_ADDRCONFIG
 *   in ai_flags?
 */

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

#if defined(__KAME__) && defined(INET6)
# define FAITH
#endif

#define	SUCCESS 0
#define	ANY 0
#define	YES 1
#define	NO  0

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
#ifdef INET6
#define	N_INET6 0
	{PF_INET6, sizeof(struct in6_addr),
	 sizeof(struct sockaddr_in6),
	 offsetof(struct sockaddr_in6, sin6_addr),
	 in6_addrany, in6_loopback, 1},
#define	N_INET 1
#else
#define	N_INET 0
#endif
	{PF_INET, sizeof(struct in_addr),
	 sizeof(struct sockaddr_in),
	 offsetof(struct sockaddr_in, sin_addr),
	 in_addrany, in_loopback, 0},
	{0, 0, 0, 0, NULL, NULL, 0},
};

struct explore {
	int e_socktype;
	int e_protocol;
	const char *e_protostr;
	int e_wild;
#define	WILD_AF(ex)		((ex)->e_wild & 0x01)
#define	WILD_SOCKTYPE(ex)	((ex)->e_wild & 0x02)
#define	WILD_PROTOCOL(ex)	((ex)->e_wild & 0x04)
};

static const struct explore explore[] = {
	{ SOCK_DGRAM, IPPROTO_UDP, "udp", 0x07 },
	{ SOCK_STREAM, IPPROTO_TCP, "tcp", 0x07 },
	{ SOCK_RAW, ANY, NULL, 0x05 },
	{ 0, 0, NULL, 0 },
};

#ifdef INET6
#define	PTON_MAX	16
#else
#define	PTON_MAX	4
#endif

extern struct	hostent * _getipnodebyname_multi __P((const char *name,
	int af, int flags, int *errp));
static int str_isnumber __P((const char *));
static int explore_fqdn __P((const struct addrinfo *, const char *,
	const char *, struct addrinfo **));
static int explore_null __P((const struct addrinfo *, const char *,
	const char *, struct addrinfo **));
static int explore_numeric __P((const struct addrinfo *, const char *,
	const char *, struct addrinfo **));
static int explore_numeric_scope __P((const struct addrinfo *, const char *,
	const char *, struct addrinfo **));
static int get_canonname __P((const struct addrinfo *,
	struct addrinfo *, const char *));
static struct addrinfo *get_ai __P((const struct addrinfo *,
	const struct afd *, const char *));
static int get_portmatch __P((const struct addrinfo *, const char *));
static int get_port __P((struct addrinfo *, const char *, int));
static const struct afd *find_afd __P((int));

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
	"Argument res is NULL",				/* EAI_RESNULL	  */
	"Unknown error", 				/* EAI_MAX        */
};

/* XXX macros that make external reference is BAD. */

#define	GET_AI(ai, afd, addr) \
do { \
	/* external reference: pai, error, and label free */ \
	(ai) = get_ai(pai, (afd), (addr)); \
	if ((ai) == NULL) { \
		error = EAI_MEMORY; \
		goto free; \
	} \
} while (0)

#define	GET_PORT(ai, serv) \
do { \
	/* external reference: error and label free */ \
	error = get_port((ai), (serv), 0); \
	if (error != 0) \
		goto free; \
} while (0)

#define	GET_CANONNAME(ai, str) \
do { \
	/* external reference: pai, error and label free */ \
	error = get_canonname(pai, (ai), (str)); \
	if (error != 0) \
		goto free; \
} while (0)

#define	ERR(err) \
do { \
	/* external reference: error, and label bad */ \
	error = (err); \
	goto bad; \
} while (0)

#define	MATCH_FAMILY(x, y, w) \
	((x) == (y) || ((w) && ((x) == PF_UNSPEC || (y) == PF_UNSPEC)))
#define	MATCH(x, y, w) \
	((x) == (y) || ((w) && ((x) == ANY || (y) == ANY)))

char *
gai_strerror(ecode)
	int ecode;
{
	if (ecode < 0 || ecode > EAI_MAX)
		ecode = EAI_MAX;
	return ai_errlist[ecode];
}

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
	} while ((ai = next) != NULL);
}

static int
str_isnumber(p)
	const char *p;
{
	char *q = (char *)p;
	while (*q) {
		if (! isdigit(*q))
			return NO;
		q++;
	}
	return YES;
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
	struct addrinfo ai;
	struct addrinfo ai0;
	struct addrinfo *pai;
	const struct afd *afd;
	const struct explore *ex;

	sentinel.ai_next = NULL;
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
	if (res == NULL)
		return EAI_RESNULL; /* xxx */
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
			int matched = 0;

			for (ex = explore; ex->e_socktype; ex++) {
				if (ex->e_socktype == ANY)
					continue;
				if (ex->e_protocol == ANY)
					continue;
				if (pai->ai_socktype == ex->e_socktype
				 && pai->ai_protocol == ex->e_protocol)
					matched = 1;
				else
					continue;
				if (matched == 0)
					ERR(EAI_BADHINTS);
			}
		}
	}

	/* backup original pai contents */
	ai0 = *pai;

	/*
	 * special cases check for inet and inet6 sockets.
	 * (1) servname is disallowed for raw sockets.
	 * (2) numeric servname is disallowed if socktype/protocol is left
	 *     unspecified.
	 */
	if (MATCH_FAMILY(pai->ai_family, PF_INET, 1)
#ifdef INET6
	    || MATCH_FAMILY(pai->ai_family, PF_INET6, 1)
#endif
	    ) {
		*pai = ai0;

		if (pai->ai_family == PF_UNSPEC)
#ifdef INET6
			pai->ai_family = PF_INET6;
#else
			pai->ai_family = PF_INET;
#endif
		error = get_portmatch(pai, servname);
		if (error)
			ERR(error);
	}

	/* NULL hostname, or numeric hostname */
	for (afd = afdl; afd->a_af; afd++)
      {
	for (ex = explore; ex->e_socktype; ex++) {
		*pai = ai0;

		if (!MATCH_FAMILY(pai->ai_family, afd->a_af, WILD_AF(ex)))
			continue;
		if (!MATCH(pai->ai_socktype, ex->e_socktype, WILD_SOCKTYPE(ex)))
			continue;
		if (!MATCH(pai->ai_protocol, ex->e_protocol, WILD_PROTOCOL(ex)))
			continue;

		if (pai->ai_family == PF_UNSPEC)
			pai->ai_family = afd->a_af;
		if (pai->ai_socktype == ANY && ex->e_socktype != ANY)
			pai->ai_socktype = ex->e_socktype;
		if (pai->ai_protocol == ANY && ex->e_protocol != ANY)
			pai->ai_protocol = ex->e_protocol;

		if (hostname == NULL)
			error = explore_null(pai, hostname, servname, &cur->ai_next);
		else
			error = explore_numeric_scope(pai, hostname, servname, &cur->ai_next);

		if (error)
			goto free;

		while (cur && cur->ai_next)
			cur = cur->ai_next;
	}
      }

	/*
	 * XXX
	 * If numreic representation of AF1 can be interpreted as FQDN
	 * representation of AF2, we need to think again about the code below.
	 */
	if (sentinel.ai_next)
		goto good;

	if (pai->ai_flags & AI_NUMERICHOST)
		ERR(EAI_NONAME);
	if (hostname == NULL)
		ERR(EAI_NONAME);

	/* hostname as alphabetical name. */
	{
		for (ex = explore; ex->e_socktype; ex++) {
			*pai = ai0;

			if (!MATCH(pai->ai_socktype, ex->e_socktype,
					WILD_SOCKTYPE(ex))) {
				continue;
			}
			if (!MATCH(pai->ai_protocol, ex->e_protocol,
					WILD_PROTOCOL(ex))) {
				continue;
			}

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

	/* XXX: if any addrinfo found, SUCCESS return even if (error != 0) */
	if (sentinel.ai_next) {
 good:
		*res = sentinel.ai_next;
		return SUCCESS;
	}
	/* else, failed */
 free:
 bad:
	if (error == 0)
		error = EAI_FAIL;
	if (sentinel.ai_next)
		freeaddrinfo(sentinel.ai_next);
	*res = NULL;
	return error;
}

/*
 * FQDN hostname, DNS lookup
 */
static int
explore_fqdn(pai, hostname, servname, res)
	const struct addrinfo *pai;
	const char *hostname;
	const char *servname;
	struct addrinfo **res;
{
	struct hostent *hp;
	int h_error;
	int af;
	char *ap;
	struct addrinfo sentinel, *cur;
	int i;
	const struct afd *afd;
	int error;

	*res = NULL;
	sentinel.ai_next = NULL;
	cur = &sentinel;

	/*
	 * if the servname does not match socktype/protocol, ignore it.
	 */
	if (get_portmatch(pai, servname) != 0)
		return 0;

	hp = _getipnodebyname_multi(hostname, pai->ai_family, AI_ADDRCONFIG,
				       &h_error);
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
		case NETDB_INTERNAL:
		default:
			error = EAI_FAIL;
			break;
		}
	} else if ((hp->h_name == NULL) || (hp->h_name[0] == 0)
			|| (hp->h_addr_list[0] == NULL)) {
		freehostent(hp);
		hp = NULL;
		error = EAI_FAIL;
	}

	if (hp == NULL)
		goto free;

	for (i = 0; hp->h_addr_list[i] != NULL; i++) {
		af = hp->h_addrtype;
		ap = hp->h_addr_list[i];

		if (pai->ai_family != AF_UNSPEC && af != pai->ai_family)
			continue;

		afd = find_afd(af);
		if (afd == NULL)
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
	if (hp)
		freehostent(hp);
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
explore_null(pai, hostname, servname, res)
	const struct addrinfo *pai;
	const char *hostname;
	const char *servname;
	struct addrinfo **res;
{
	int s;
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
	s = socket(pai->ai_family, SOCK_DGRAM, 0);
	if (s < 0)
		return 0;
	_close(s);
	afd = find_afd(pai->ai_family);
	if (afd == NULL)
		return 0;

	GET_AI(cur->ai_next, afd,
	       (pai->ai_flags & AI_PASSIVE) ? afd->a_addrany : afd->a_loopback
	       );
	/* xxx meaningless?
	 * GET_CANONNAME(cur->ai_next, "anyaddr");
	 * or
	 * GET_CANONNAME(cur->ai_next, "localhost");
	 */
	/* if the servname does not match socktype/protocol, ignored */
	GET_PORT(cur->ai_next, servname);

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
	int flags;

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
	flags = pai->ai_flags;

	if ((afd->a_af == AF_INET
	     ? inet_aton(hostname, (struct in_addr *)pton)
	     : inet_pton(afd->a_af, hostname, pton)) == 1) {
		if (pai->ai_family == afd->a_af ||
		    pai->ai_family == PF_UNSPEC /*?*/) {
			GET_AI(cur->ai_next, afd, pton);
			GET_PORT(cur->ai_next, servname);
			while (cur && cur->ai_next)
				cur = cur->ai_next;
		} else
			ERR(EAI_FAMILY);	/*xxx*/
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
	char *cp, *hostname2 = NULL;
	int scope;
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif

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

	/*
	 * Handle special case of <scoped_address><delimiter><scope id>
	 */
	hostname2 = strdup(hostname);
	if (hostname2 == NULL)
		return EAI_MEMORY;
	/* terminate at the delimiter */
	hostname2[cp - hostname] = '\0';

	cp++;
	switch (pai->ai_family) {
#ifdef INET6
	case AF_INET6:
		scope = if_nametoindex(cp);
		if (scope == 0) {
			error = EAI_SYSTEM;
			goto free;
		}
		break;
#endif
	}

	error = explore_numeric(pai, hostname2, servname, res);
	if (error == 0) {
		for (cur = *res; cur; cur = cur->ai_next) {
#ifdef INET6
			if (cur->ai_family != AF_INET6)
				continue;
			sin6 = (struct sockaddr_in6 *)cur->ai_addr;
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) ||
			    IN6_IS_ADDR_MC_LINKLOCAL(&sin6->sin6_addr))
				sin6->sin6_scope_id = scope;
#endif
		}
	}

#ifdef INET6
free:
#endif
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
#ifdef FAITH
	struct in6_addr faith_prefix;
	char *fp_str;
	int translate = 0;
#endif

#ifdef FAITH
	/*
	 * Transfrom an IPv4 addr into a special IPv6 addr format for
	 * IPv6->IPv4 translation gateway. (only TCP is supported now)
	 *
	 * +-----------------------------------+------------+
	 * | faith prefix part (12 bytes)      | embedded   |
	 * |                                   | IPv4 addr part (4 bytes)
	 * +-----------------------------------+------------+
	 *
	 * faith prefix part is specified as ascii IPv6 addr format
	 * in environmental variable GAI.
	 * For FAITH to work correctly, routing to faith prefix must be
	 * setup toward a machine where a FAITH daemon operates.
	 * Also, the machine must enable some mechanizm
	 * (e.g. faith interface hack) to divert those packet with
	 * faith prefixed destination addr to user-land FAITH daemon.
	 */
	fp_str = getenv("GAI");
	if (fp_str && inet_pton(AF_INET6, fp_str, &faith_prefix) == 1 &&
	    afd->a_af == AF_INET && pai->ai_socktype == SOCK_STREAM) {
		u_int32_t v4a;
		u_int8_t v4a_top;

		memcpy(&v4a, addr, sizeof v4a);
		v4a_top = v4a >> IN_CLASSA_NSHIFT;
		if (!IN_MULTICAST(v4a) && !IN_EXPERIMENTAL(v4a) &&
		    v4a_top != 0 && v4a != IN_LOOPBACKNET) {
			afd = &afdl[N_INET6];
			memcpy(&faith_prefix.s6_addr[12], addr,
			       sizeof(struct in_addr));
			translate = 1;
		}
	}
#endif

	ai = (struct addrinfo *)malloc(sizeof(struct addrinfo)
		+ (afd->a_socklen));
	if (ai == NULL)
		return NULL;

	memcpy(ai, pai, sizeof(struct addrinfo));
	ai->ai_addr = (struct sockaddr *)(ai + 1);
	memset(ai->ai_addr, 0, afd->a_socklen);
	ai->ai_addr->sa_len = afd->a_socklen;
	ai->ai_addrlen = afd->a_socklen;
	ai->ai_addr->sa_family = ai->ai_family = afd->a_af;
	p = (char *)(ai->ai_addr);
#ifdef FAITH
	if (translate == 1)
		memcpy(p + afd->a_off, &faith_prefix, afd->a_addrlen);
	else
#endif
	memcpy(p + afd->a_off, addr, afd->a_addrlen);

	return ai;
}

static int
get_portmatch(ai, servname)
	const struct addrinfo *ai;
	const char *servname;
{
	/* get_port does not touch first argument. when matchonly == 1. */
	return get_port((struct addrinfo *)ai, servname, 1);
}

static int
get_port(ai, servname, matchonly)
	struct addrinfo *ai;
	const char *servname;
	int matchonly;
{
	const char *proto;
	struct servent *sp;
	int port;
	int allownumeric;

	if (servname == NULL)
		return 0;
	if (ai->ai_family != AF_INET
#ifdef INET6
	    && ai->ai_family != AF_INET6
#endif
	    )
		return 0;

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
			((struct sockaddr_in *)ai->ai_addr)->sin_port = port;
			break;
#ifdef INET6
		case AF_INET6:
			((struct sockaddr_in6 *)ai->ai_addr)->sin6_port = port;
			break;
#endif
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
