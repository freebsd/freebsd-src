/*	$FreeBSD$	*/
/*	$KAME: getaddrinfo.c,v 1.15 2000/07/09 04:37:24 itojun Exp $	*/

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
 * "#ifdef FAITH" part is local hack for supporting IPv4-v6 translator.
 *
 * Issues to be discussed:
 * - Thread safe-ness must be checked.
 * - Return values.  There are nonstandard return values defined and used
 *   in the source code.  This is because RFC2553 is silent about which error
 *   code must be returned for which situation.
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
 * - To avoid search order issue, we have a big amount of code duplicate
 *   from gethnamaddr.c and some other places.  The issues that there's no
 *   lower layer function to lookup "IPv4 or IPv6" record.  Calling
 *   gethostbyname2 from getaddrinfo will end up in wrong search order, as
 *   follows:
 *	- The code makes use of following calls when asked to resolver with
 *	  ai_family  = PF_UNSPEC:
 *		getipnodebyname(host, AF_INET6);
 *		getipnodebyname(host, AF_INET);
 *	  This will result in the following queries if the node is configure to
 *	  prefer /etc/hosts than DNS:
 *		lookup /etc/hosts for IPv6 address
 *		lookup DNS for IPv6 address
 *		lookup /etc/hosts for IPv4 address
 *		lookup DNS for IPv4 address
 *	  which may not meet people's requirement.
 *	  The right thing to happen is to have underlying layer which does
 *	  PF_UNSPEC lookup (lookup both) and return chain of addrinfos.
 *	  This would result in a bit of code duplicate with _dns_ghbyname() and
 *	  friends.
 */
/*
 * diffs with other KAME platforms:
 * - other KAME platforms already nuked FAITH ($GAI), but as FreeBSD
 *   4.0-RELEASE supplies it, we still have the code here.
 * - EAI_RESNULL support
 * - AI_ADDRCONFIG support is supplied
 * - EDNS0 support is not available due to resolver differences
 * - some of FreeBSD style (#define tabify and others)
 * - AI_ADDRCONFIG is turned on by default.
 * - classful IPv4 numeric (127.1) is allowed.
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
#include <errno.h>

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
	int e_af;
	int e_socktype;
	int e_protocol;
	const char *e_protostr;
	int e_wild;
#define	WILD_AF(ex)		((ex)->e_wild & 0x01)
#define	WILD_SOCKTYPE(ex)	((ex)->e_wild & 0x02)
#define	WILD_PROTOCOL(ex)	((ex)->e_wild & 0x04)
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
	{ PF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP, "udp", 0x07 },
	{ PF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, "tcp", 0x07 },
	{ PF_UNSPEC, SOCK_RAW, ANY, NULL, 0x05 },
	{ -1, 0, 0, NULL, 0 },
};

#ifdef INET6
#define	PTON_MAX	16
#else
#define	PTON_MAX	4
#endif

#if PACKETSZ > 1024
#define MAXPACKET	PACKETSZ
#else
#define MAXPACKET	1024
#endif

typedef union {
	HEADER hdr;
	u_char buf[MAXPACKET];
} querybuf;

struct res_target {
	struct res_target *next;
	const char *name;	/* domain name */
	int qclass, qtype;	/* class and type of query */
	u_char *answer;		/* buffer to put answer */
	int anslen;		/* size of answer buffer */
	int n;			/* result length */
};

static int str_isnumber __P((const char *));
static int explore_fqdn __P((const struct addrinfo *, const char *,
	const char *, struct addrinfo **));
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
static int get_portmatch __P((const struct addrinfo *, const char *));
static int get_port __P((struct addrinfo *, const char *, int));
static const struct afd *find_afd __P((int));
static int addrconfig __P((struct addrinfo *));
#ifdef INET6
static int ip6_str2scopeid __P((char *, struct sockaddr_in6 *));
#endif

static struct addrinfo *getanswer __P((const querybuf *, int, const char *,
				       int, const struct addrinfo *));
static int _dns_getaddrinfo __P((const struct addrinfo *, const char *,
				 struct addrinfo **));
static struct addrinfo *_gethtent __P((FILE *fp, const char *,
				       const struct addrinfo *));
static int _files_getaddrinfo __P((const struct addrinfo *, const char *,
				   struct addrinfo **));
#ifdef YP
static int _nis_getaddrinfo __P((const struct addrinfo *, const char *,
				 struct addrinfo **));
#endif

static int res_queryN __P((const char *, struct res_target *));
static int res_searchN __P((const char *, struct res_target *));
static int res_querydomainN __P((const char *, const char *,
				 struct res_target *));


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
#ifdef EAI_RESNULL
	"Argument res is NULL",				/* EAI_RESNULL	  */
#endif
	"Unknown error", 				/* EAI_MAX        */
};

/*
 * Select order host function.
 */
#define	MAXHOSTCONF	4

#ifndef HOSTCONF
#  define	HOSTCONF	"/etc/host.conf"
#endif /* !HOSTCONF */

struct _hostconf {
	int (*byname)(const struct addrinfo *, const char *,
		      struct addrinfo **);
};

/* default order */
static struct _hostconf _hostconf[MAXHOSTCONF] = {
	_dns_getaddrinfo,
	_files_getaddrinfo,
#ifdef ICMPNL
	NULL,
#endif /* ICMPNL */
};

static int	_hostconf_init_done;
static void	_hostconf_init(void);

/* XXX macros that make external reference is BAD. */

#define	GET_AI(ai, afd, addr) \
do { \
	/* external reference: pai, error, and label free */ \
	(ai) = get_ai(pai, (afd), (addr)); \
	if ((ai) == NULL) { \
		error = EAI_MEMORY; \
		goto free; \
	} \
} while (/*CONSTCOND*/0)

#define	GET_PORT(ai, serv) \
do { \
	/* external reference: error and label free */ \
	error = get_port((ai), (serv), 0); \
	if (error != 0) \
		goto free; \
} while (/*CONSTCOND*/0)

#define	GET_CANONNAME(ai, str) \
do { \
	/* external reference: pai, error and label free */ \
	error = get_canonname(pai, (ai), (str)); \
	if (error != 0) \
		goto free; \
} while (/*CONSTCOND*/0)

#define	ERR(err) \
do { \
	/* external reference: error, and label bad */ \
	error = (err); \
	goto bad; \
	/*NOTREACHED*/ \
} while (/*CONSTCOND*/0)

#define	MATCH_FAMILY(x, y, w) \
	((x) == (y) || (/*CONSTCOND*/(w) && ((x) == PF_UNSPEC || (y) == PF_UNSPEC)))
#define	MATCH(x, y, w) \
	((x) == (y) || (/*CONSTCOND*/(w) && ((x) == ANY || (y) == ANY)))

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
	(void)strtoul(p, &ep, 10);
	if (ep && *ep == '\0')
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
	struct addrinfo ai;
	struct addrinfo ai0;
	struct addrinfo *pai;
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
#ifdef EAI_RESNULL
	if (res == NULL)
		return EAI_RESNULL; /* xxx */
#endif
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
		if (pai->ai_family != AF_INET6)
			pai->ai_flags &= ~(AI_ALL | AI_V4MAPPED);
		break;
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

		/* PF_UNSPEC entries are prepared for DNS queries only */
		if (ex->e_af == PF_UNSPEC)
			continue;

		if (!MATCH_FAMILY(pai->ai_family, ex->e_af, WILD_AF(ex)))
			continue;
		if (!MATCH(pai->ai_socktype, ex->e_socktype,
			   WILD_SOCKTYPE(ex)))
			continue;
		if (!MATCH(pai->ai_protocol, ex->e_protocol,
			   WILD_PROTOCOL(ex)))
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
		ERR(EAI_NODATA);
	if (hostname == NULL)
		ERR(EAI_NODATA);

#if 1
	/* XXX: temporarily, behave as if AI_ADDRCONFIG is specified */
	pai->ai_flags |= AI_ADDRCONFIG;
#endif
	if ((pai->ai_flags & AI_ADDRCONFIG) != 0 && !addrconfig(&ai0))
		ERR(EAI_FAIL);

	/*
	 * hostname as alphabetical name.
	 * we would like to prefer AF_INET6 than AF_INET, so we'll make a
	 * outer loop by AFs.
	 */
	for (ex = explore; ex->e_af >= 0; ex++) {
		*pai = ai0;

		/* require exact match for family field */
		if (pai->ai_family != ex->e_af)
			continue;

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

		error = explore_fqdn(pai, hostname, servname, &cur->ai_next);

		while (cur && cur->ai_next)
			cur = cur->ai_next;
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

static char *
_hgetword(char **pp)
{
	char c, *p, *ret;
	const char *sp;
	static const char sep[] = "# \t\n";

	ret = NULL;
	for (p = *pp; (c = *p) != '\0'; p++) {
		for (sp = sep; *sp != '\0'; sp++) {
			if (c == *sp)
				break;
		}
		if (c == '#')
			p[1] = '\0';	/* ignore rest of line */
		if (ret == NULL) {
			if (*sp == '\0')
				ret = p;
		} else {
			if (*sp != '\0') {
				*p++ = '\0';
				break;
			}
		}
	}
	*pp = p;
	if (ret == NULL || *ret == '\0')
		return NULL;
	return ret;
}

/*
 * Initialize hostconf structure.
 */

static void
_hostconf_init(void)
{
	FILE *fp;
	int n;
	char *p, *line;
	char buf[BUFSIZ];

	_hostconf_init_done = 1;
	n = 0;
	p = HOSTCONF;
	if ((fp = fopen(p, "r")) == NULL)
		return;
	while (n < MAXHOSTCONF && fgets(buf, sizeof(buf), fp)) {
		line = buf;
		if ((p = _hgetword(&line)) == NULL)
			continue;
		do {
			if (strcmp(p, "hosts") == 0
			||  strcmp(p, "local") == 0
			||  strcmp(p, "file") == 0
			||  strcmp(p, "files") == 0)
				_hostconf[n++].byname = _files_getaddrinfo;
			else if (strcmp(p, "dns") == 0
			     ||  strcmp(p, "bind") == 0)
				_hostconf[n++].byname = _dns_getaddrinfo;
#ifdef YP
			else if (strcmp(p, "nis") == 0)
				_hostconf[n++].byname = _nis_getaddrinfo;
#endif
		} while ((p = _hgetword(&line)) != NULL);
	}
	fclose(fp);
	if (n < 0) {
		/* no keyword found. do not change default configuration */
		return;
	}
	for (; n < MAXHOSTCONF; n++)
		_hostconf[n].byname = NULL;
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
	struct addrinfo *result;
	struct addrinfo *cur;
	int error = 0, i;

	result = NULL;
	*res = NULL;

	/*
	 * if the servname does not match socktype/protocol, ignore it.
	 */
	if (get_portmatch(pai, servname) != 0)
		return 0;

	if (!_hostconf_init_done)
		_hostconf_init();

	for (i = 0; i < MAXHOSTCONF; i++) {
		if (!_hostconf[i].byname)
			continue;
		error = (*_hostconf[i].byname)(pai, hostname, &result);
		if (error != 0)
			continue;
		for (cur = result; cur; cur = cur->ai_next) {
			GET_PORT(cur, servname);
			/* canonname should be filled already */
		}
		*res = result;
		return 0;
	}

free:
	if (result)
		freeaddrinfo(result);
	return error;
}

/*
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
	if (s < 0) {
		if (errno != EMFILE)
			return 0;
	} else
		_close(s);

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

	/*
	 * if the servname does not match socktype/protocol, ignore it.
	 */
	if (get_portmatch(pai, servname) != 0)
		return 0;

	afd = find_afd(pai->ai_family);
	if (afd == NULL)
		return 0;

	switch (afd->a_af) {
#if 1 /*X/Open spec*/
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
explore_numeric_scope(pai, hostname, servname, res)
	const struct addrinfo *pai;
	const char *hostname;
	const char *servname;
	struct addrinfo **res;
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
	ai->ai_addr = (struct sockaddr *)(void *)(ai + 1);
	memset(ai->ai_addr, 0, (size_t)afd->a_socklen);
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
	/* LINTED const cast */
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

/*
 * post-2553: AI_ADDRCONFIG check.  if we use getipnodeby* as backend, backend
 * will take care of it.
 * the semantics of AI_ADDRCONFIG is not defined well.  we are not sure
 * if the code is right or not.
 *
 * XXX PF_UNSPEC -> PF_INET6 + PF_INET mapping needs to be in sync with
 * _dns_getaddrinfo.
 */
static int
addrconfig(pai)
	struct addrinfo *pai;
{
	int s, af;

	/*
	 * TODO:
	 * Note that implementation dependent test for address
	 * configuration should be done everytime called
	 * (or apropriate interval),
	 * because addresses will be dynamically assigned or deleted.
	 */
	af = pai->ai_family;
	if (af == AF_UNSPEC) {
		if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
			af = AF_INET;
		else {
			_close(s);
			if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
				af = AF_INET6;
			else
				_close(s);
		}

	}
	if (af != AF_UNSPEC) {
		if ((s = socket(af, SOCK_DGRAM, 0)) < 0)
			return 0;
		_close(s);
	}
	pai->ai_family = af;
	return 1;
}

#ifdef INET6
/* convert a string to a scope identifier. XXX: IPv6 specific */
static int
ip6_str2scopeid(scope, sin6)
	char *scope;
	struct sockaddr_in6 *sin6;
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

#ifdef DEBUG
static const char AskedForGot[] =
	"gethostby*.getanswer: asked for \"%s\", got \"%s\"";
#endif

static struct addrinfo *
getanswer(answer, anslen, qname, qtype, pai)
	const querybuf *answer;
	int anslen;
	const char *qname;
	int qtype;
	const struct addrinfo *pai;
{
	struct addrinfo sentinel, *cur;
	struct addrinfo ai;
	const struct afd *afd;
	char *canonname;
	const HEADER *hp;
	const u_char *cp;
	int n;
	const u_char *eom;
	char *bp;
	int type, class, buflen, ancount, qdcount;
	int haveanswer, had_error;
	char tbuf[MAXDNAME];
	int (*name_ok) __P((const char *));
	char hostbuf[8*1024];

	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	canonname = NULL;
	eom = answer->buf + anslen;
	switch (qtype) {
	case T_A:
	case T_AAAA:
	case T_ANY:	/*use T_ANY only for T_A/T_AAAA lookup*/
		name_ok = res_hnok;
		break;
	default:
		return (NULL);	/* XXX should be abort(); */
	}
	/*
	 * find first satisfactory answer
	 */
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	bp = hostbuf;
	buflen = sizeof hostbuf;
	cp = answer->buf + HFIXEDSZ;
	if (qdcount != 1) {
		h_errno = NO_RECOVERY;
		return (NULL);
	}
	n = dn_expand(answer->buf, eom, cp, bp, buflen);
	if ((n < 0) || !(*name_ok)(bp)) {
		h_errno = NO_RECOVERY;
		return (NULL);
	}
	cp += n + QFIXEDSZ;
	if (qtype == T_A || qtype == T_AAAA || qtype == T_ANY) {
		/* res_send() has already verified that the query name is the
		 * same as the one we sent; this just gets the expanded name
		 * (i.e., with the succeeding search-domain tacked on).
		 */
		n = strlen(bp) + 1;		/* for the \0 */
		if (n >= MAXHOSTNAMELEN) {
			h_errno = NO_RECOVERY;
			return (NULL);
		}
		canonname = bp;
		bp += n;
		buflen -= n;
		/* The qname can be abbreviated, but h_name is now absolute. */
		qname = canonname;
	}
	haveanswer = 0;
	had_error = 0;
	while (ancount-- > 0 && cp < eom && !had_error) {
		n = dn_expand(answer->buf, eom, cp, bp, buflen);
		if ((n < 0) || !(*name_ok)(bp)) {
			had_error++;
			continue;
		}
		cp += n;			/* name */
		type = _getshort(cp);
 		cp += INT16SZ;			/* type */
		class = _getshort(cp);
 		cp += INT16SZ + INT32SZ;	/* class, TTL */
		n = _getshort(cp);
		cp += INT16SZ;			/* len */
		if (class != C_IN) {
			/* XXX - debug? syslog? */
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		if ((qtype == T_A || qtype == T_AAAA || qtype == T_ANY) &&
		    type == T_CNAME) {
			n = dn_expand(answer->buf, eom, cp, tbuf, sizeof tbuf);
			if ((n < 0) || !(*name_ok)(tbuf)) {
				had_error++;
				continue;
			}
			cp += n;
			/* Get canonical name. */
			n = strlen(tbuf) + 1;	/* for the \0 */
			if (n > buflen || n >= MAXHOSTNAMELEN) {
				had_error++;
				continue;
			}
			strcpy(bp, tbuf);
			canonname = bp;
			bp += n;
			buflen -= n;
			continue;
		}
		if (qtype == T_ANY) {
			if (!(type == T_A || type == T_AAAA)) {
				cp += n;
				continue;
			}
		} else if (type != qtype) {
#ifdef DEBUG
			if (type != T_KEY && type != T_SIG)
				syslog(LOG_NOTICE|LOG_AUTH,
	       "gethostby*.getanswer: asked for \"%s %s %s\", got type \"%s\"",
				       qname, p_class(C_IN), p_type(qtype),
				       p_type(type));
#endif
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		switch (type) {
		case T_A:
		case T_AAAA:
			if (strcasecmp(canonname, bp) != 0) {
#ifdef DEBUG
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, canonname, bp);
#endif
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			if (type == T_A && n != INADDRSZ) {
				cp += n;
				continue;
			}
			if (type == T_AAAA && n != IN6ADDRSZ) {
				cp += n;
				continue;
			}
#ifdef FILTER_V4MAPPED
			if (type == T_AAAA) {
				struct in6_addr in6;
				memcpy(&in6, cp, sizeof(in6));
				if (IN6_IS_ADDR_V4MAPPED(&in6)) {
					cp += n;
					continue;
				}
			}
#endif
			if (!haveanswer) {
				int nn;

				canonname = bp;
				nn = strlen(bp) + 1;	/* for the \0 */
				bp += nn;
				buflen -= nn;
			}

			/* don't overwrite pai */
			ai = *pai;
			ai.ai_family = (type == T_A) ? AF_INET : AF_INET6;
			afd = find_afd(ai.ai_family);
			if (afd == NULL) {
				cp += n;
				continue;
			}
			cur->ai_next = get_ai(&ai, afd, (const char *)cp);
			if (cur->ai_next == NULL)
				had_error++;
			while (cur && cur->ai_next)
				cur = cur->ai_next;
			cp += n;
			break;
		default:
			abort();
		}
		if (!had_error)
			haveanswer++;
	}
	if (haveanswer) {
		if (!canonname)
			(void)get_canonname(pai, sentinel.ai_next, qname);
		else
			(void)get_canonname(pai, sentinel.ai_next, canonname);
		h_errno = NETDB_SUCCESS;
		return sentinel.ai_next;
	}

	h_errno = NO_RECOVERY;
	return NULL;
}

/*ARGSUSED*/
static int
_dns_getaddrinfo(pai, hostname, res)
	const struct addrinfo *pai;
	const char *hostname;
	struct addrinfo **res;
{
	struct addrinfo *ai;
	querybuf buf, buf2;
	const char *name;
	struct addrinfo sentinel, *cur;
	struct res_target q, q2;

	memset(&q, 0, sizeof(q2));
	memset(&q2, 0, sizeof(q2));
	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	switch (pai->ai_family) {
	case AF_UNSPEC:
		/* prefer IPv6 */
		q.qclass = C_IN;
		q.qtype = T_AAAA;
		q.answer = buf.buf;
		q.anslen = sizeof(buf);
		q.next = &q2;
		q2.qclass = C_IN;
		q2.qtype = T_A;
		q2.answer = buf2.buf;
		q2.anslen = sizeof(buf2);
		break;
	case AF_INET:
		q.qclass = C_IN;
		q.qtype = T_A;
		q.answer = buf.buf;
		q.anslen = sizeof(buf);
		break;
	case AF_INET6:
		q.qclass = C_IN;
		q.qtype = T_AAAA;
		q.answer = buf.buf;
		q.anslen = sizeof(buf);
		break;
	default:
		return EAI_FAIL;
	}
	if (res_searchN(hostname, &q) < 0)
		return EAI_NODATA;
	ai = getanswer(&buf, q.n, q.name, q.qtype, pai);
	if (ai) {
		cur->ai_next = ai;
		while (cur && cur->ai_next)
			cur = cur->ai_next;
	}
	if (q.next) {
		ai = getanswer(&buf2, q2.n, q2.name, q2.qtype, pai);
		if (ai)
			cur->ai_next = ai;
	}
	if (sentinel.ai_next == NULL)
		switch (h_errno) {
		case HOST_NOT_FOUND:
			return EAI_NODATA;
		case TRY_AGAIN:
			return EAI_AGAIN;
		default:
			return EAI_FAIL;
		}
	*res = sentinel.ai_next;
	return 0;
}

static struct addrinfo *
_gethtent(hostf, name, pai)
	FILE *hostf;
	const char *name;
	const struct addrinfo *pai;
{
	char *p;
	char *cp, *tname, *cname;
	struct addrinfo hints, *res0, *res;
	int error;
	const char *addr;
	char hostbuf[8*1024];

 again:
	if (!(p = fgets(hostbuf, sizeof hostbuf, hostf)))
		return (NULL);
	if (*p == '#')
		goto again;
	if (!(cp = strpbrk(p, "#\n")))
		goto again;
	*cp = '\0';
	if (!(cp = strpbrk(p, " \t")))
		goto again;
	*cp++ = '\0';
	addr = p;
	cname = NULL;
	/* if this is not something we're looking for, skip it. */
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		tname = cp;
		if (cname == NULL)
			cname = cp;
		if ((cp = strpbrk(cp, " \t")) != NULL)
			*cp++ = '\0';
		if (strcasecmp(name, tname) == 0)
			goto found;
	}
	goto again;

found:
	hints = *pai;
	hints.ai_flags = AI_NUMERICHOST;
	error = getaddrinfo(addr, NULL, &hints, &res0);
	if (error)
		goto again;
#ifdef FILTER_V4MAPPED
	/* XXX should check all items in the chain */
	if (res0->ai_family == AF_INET6 &&
	    IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)res0->ai_addr)->sin6_addr)) {
		freeaddrinfo(res0);
		goto again;
	}
#endif
	for (res = res0; res; res = res->ai_next) {
		/* cover it up */
		res->ai_flags = pai->ai_flags;

		if (pai->ai_flags & AI_CANONNAME) {
			if (get_canonname(pai, res, cname) != 0) {
				freeaddrinfo(res0);
				goto again;
			}
		}
	}
	return res0;
}

/*ARGSUSED*/
static int
_files_getaddrinfo(pai, hostname, res)
	const struct addrinfo *pai;
	const char *hostname;
	struct addrinfo **res;
{
	FILE *hostf;
	struct addrinfo sentinel, *cur;
	struct addrinfo *p;

	sentinel.ai_next = NULL;
	cur = &sentinel;

	if ((hostf = fopen(_PATH_HOSTS, "r")) == NULL)
		return EAI_FAIL;
	while ((p = _gethtent(hostf, hostname, pai)) != NULL) {
		cur->ai_next = p;
		while (cur && cur->ai_next)
			cur = cur->ai_next;
	}
	fclose(hostf);

	if (!sentinel.ai_next)
		return EAI_NODATA;

	*res = sentinel.ai_next;
	return 0;
}

#ifdef YP
/*ARGSUSED*/
static int
_nis_getaddrinfo(pai, hostname, res)
	const struct addrinfo *pai;
	const char *hostname;
	struct addrinfo **res;
{
	struct hostent *hp;
	int h_error;
	int af;
	struct addrinfo sentinel, *cur;
	int i;
	const struct afd *afd;
	int error;

	sentinel.ai_next = NULL;
	cur = &sentinel;

	af = (pai->ai_family == AF_UNSPEC) ? AF_INET : pai->ai_family;
	if (af != AF_INET)
		return (EAI_ADDRFAMILY);

	if ((hp = _gethostbynisname(hostname, af)) == NULL) {
		switch (errno) {
		/* XXX: should be filled in */
		default:
			error = EAI_FAIL;
			break;
		}
	} else if (hp->h_name == NULL ||
		   hp->h_name[0] == 0 || hp->h_addr_list[0] == NULL) {
		hp = NULL;
		error = EAI_FAIL;
	}

	if (hp == NULL)
		return error;

	for (i = 0; hp->h_addr_list[i] != NULL; i++) {
		if (hp->h_addrtype != af)
			continue;

		afd = find_afd(hp->h_addrtype);
		if (afd == NULL)
			continue;

		GET_AI(cur->ai_next, afd, hp->h_addr_list[i]);
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
	if (sentinel.ai_next)
		freeaddrinfo(sentinel.ai_next);
	return error;
}
#endif

/* resolver logic */

extern const char *__hostalias __P((const char *));
extern int h_errno;

/*
 * Formulate a normal query, send, and await answer.
 * Returned answer is placed in supplied buffer "answer".
 * Perform preliminary check of answer, returning success only
 * if no error is indicated and the answer count is nonzero.
 * Return the size of the response on success, -1 on error.
 * Error number is left in h_errno.
 *
 * Caller must parse answer and determine whether it answers the question.
 */
static int
res_queryN(name, target)
	const char *name;	/* domain name */
	struct res_target *target;
{
	u_char buf[MAXPACKET];
	HEADER *hp;
	int n;
	struct res_target *t;
	int rcode;
	int ancount;

	rcode = NOERROR;
	ancount = 0;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return (-1);
	}

	for (t = target; t; t = t->next) {
		int class, type;
		u_char *answer;
		int anslen;

		hp = (HEADER *)(void *)t->answer;
		hp->rcode = NOERROR;	/* default */

		/* make it easier... */
		class = t->qclass;
		type = t->qtype;
		answer = t->answer;
		anslen = t->anslen;
#ifdef DEBUG
		if (_res.options & RES_DEBUG)
			printf(";; res_query(%s, %d, %d)\n", name, class, type);
#endif

		n = res_mkquery(QUERY, name, class, type, NULL, 0, NULL,
		    buf, sizeof(buf));
		if (n <= 0) {
#ifdef DEBUG
			if (_res.options & RES_DEBUG)
				printf(";; res_query: mkquery failed\n");
#endif
			h_errno = NO_RECOVERY;
			return (n);
		}
		n = res_send(buf, n, answer, anslen);
#if 0
		if (n < 0) {
#ifdef DEBUG
			if (_res.options & RES_DEBUG)
				printf(";; res_query: send error\n");
#endif
			h_errno = TRY_AGAIN;
			return (n);
		}
#endif

		if (n < 0 || hp->rcode != NOERROR || ntohs(hp->ancount) == 0) {
			rcode = hp->rcode;	/* record most recent error */
#ifdef DEBUG
			if (_res.options & RES_DEBUG)
				printf(";; rcode = %d, ancount=%d\n", hp->rcode,
				    ntohs(hp->ancount));
#endif
			continue;
		}

		ancount += ntohs(hp->ancount);

		t->n = n;
	}

	if (ancount == 0) {
		switch (rcode) {
		case NXDOMAIN:
			h_errno = HOST_NOT_FOUND;
			break;
		case SERVFAIL:
			h_errno = TRY_AGAIN;
			break;
		case NOERROR:
			h_errno = NO_DATA;
			break;
		case FORMERR:
		case NOTIMP:
		case REFUSED:
		default:
			h_errno = NO_RECOVERY;
			break;
		}
		return (-1);
	}
	return (ancount);
}

/*
 * Formulate a normal query, send, and retrieve answer in supplied buffer.
 * Return the size of the response on success, -1 on error.
 * If enabled, implement search rules until answer or unrecoverable failure
 * is detected.  Error code, if any, is left in h_errno.
 */
static int
res_searchN(name, target)
	const char *name;	/* domain name */
	struct res_target *target;
{
	const char *cp, * const *domain;
	HEADER *hp = (HEADER *)(void *)target->answer;	/*XXX*/
	u_int dots;
	int trailing_dot, ret, saved_herrno;
	int got_nodata = 0, got_servfail = 0, tried_as_is = 0;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return (-1);
	}

	errno = 0;
	h_errno = HOST_NOT_FOUND;	/* default, if we never query */
	dots = 0;
	for (cp = name; *cp; cp++)
		dots += (*cp == '.');
	trailing_dot = 0;
	if (cp > name && *--cp == '.')
		trailing_dot++;

	/*
	 * if there aren't any dots, it could be a user-level alias
	 */
	if (!dots && (cp = __hostalias(name)) != NULL)
		return (res_queryN(cp, target));

	/*
	 * If there are dots in the name already, let's just give it a try
	 * 'as is'.  The threshold can be set with the "ndots" option.
	 */
	saved_herrno = -1;
	if (dots >= _res.ndots) {
		ret = res_querydomainN(name, NULL, target);
		if (ret > 0)
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
	if ((!dots && (_res.options & RES_DEFNAMES)) ||
	    (dots && !trailing_dot && (_res.options & RES_DNSRCH))) {
		int done = 0;

		for (domain = (const char * const *)_res.dnsrch;
		   *domain && !done;
		   domain++) {

			ret = res_querydomainN(name, *domain, target);
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
				h_errno = TRY_AGAIN;
				return (-1);
			}

			switch (h_errno) {
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
			/*
			 * if we got here for some reason other than DNSRCH,
			 * we only wanted one iteration of the loop, so stop.
			 */
			if (!(_res.options & RES_DNSRCH))
			        done++;
		}
	}

	/*
	 * if we have not already tried the name "as is", do that now.
	 * note that we do this regardless of how many dots were in the
	 * name or whether it ends with a dot.
	 */
	if (!tried_as_is && (dots || !(_res.options & RES_NOTLDQUERY))) {
		ret = res_querydomainN(name, NULL, target);
		if (ret > 0)
			return (ret);
	}

	/*
	 * if we got here, we didn't satisfy the search.
	 * if we did an initial full query, return that query's h_errno
	 * (note that we wouldn't be here if that query had succeeded).
	 * else if we ever got a nodata, send that back as the reason.
	 * else send back meaningless h_errno, that being the one from
	 * the last DNSRCH we did.
	 */
	if (saved_herrno != -1)
		h_errno = saved_herrno;
	else if (got_nodata)
		h_errno = NO_DATA;
	else if (got_servfail)
		h_errno = TRY_AGAIN;
	return (-1);
}

/*
 * Perform a call on res_query on the concatenation of name and domain,
 * removing a trailing dot from name if domain is NULL.
 */
static int
res_querydomainN(name, domain, target)
	const char *name, *domain;
	struct res_target *target;
{
	char nbuf[MAXDNAME];
	const char *longname = nbuf;
	size_t n, d;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return (-1);
	}
#ifdef DEBUG
	if (_res.options & RES_DEBUG)
		printf(";; res_querydomain(%s, %s)\n",
			name, domain?domain:"<Nil>");
#endif
	if (domain == NULL) {
		/*
		 * Check for trailing '.';
		 * copy without '.' if present.
		 */
		n = strlen(name);
		if (n >= MAXDNAME) {
			h_errno = NO_RECOVERY;
			return (-1);
		}
		if (n > 0 && name[--n] == '.') {
			strncpy(nbuf, name, n);
			nbuf[n] = '\0';
		} else
			longname = name;
	} else {
		n = strlen(name);
		d = strlen(domain);
		if (n + d + 1 >= MAXDNAME) {
			h_errno = NO_RECOVERY;
			return (-1);
		}
		sprintf(nbuf, "%s.%s", name, domain);
	}
	return (res_queryN(longname, target));
}
