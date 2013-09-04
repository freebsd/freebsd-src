/*
 * Copyright (C) 2009, 2012, 2013  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: getaddrinfo.c,v 1.3 2009/09/02 23:48:02 tbox Exp $ */

/*! \file */

/**
 *    getaddrinfo() is used to get a list of IP addresses and port
 *    numbers for host hostname and service servname as defined in RFC3493.
 *    hostname and servname are pointers to null-terminated strings
 *    or NULL. hostname is either a host name or a numeric host address
 *    string: a dotted decimal IPv4 address or an IPv6 address. servname is
 *    either a decimal port number or a service name as listed in
 *    /etc/services.
 *
 *    If the operating system does not provide a struct addrinfo, the
 *    following structure is used:
 *
 * \code
 * struct  addrinfo {
 *         int             ai_flags;       // AI_PASSIVE, AI_CANONNAME
 *         int             ai_family;      // PF_xxx
 *         int             ai_socktype;    // SOCK_xxx
 *         int             ai_protocol;    // 0 or IPPROTO_xxx for IPv4 and IPv6
 *         size_t          ai_addrlen;     // length of ai_addr
 *         char            *ai_canonname;  // canonical name for hostname
 *         struct sockaddr *ai_addr;       // binary address
 *         struct addrinfo *ai_next;       // next structure in linked list
 * };
 * \endcode
 *
 *
 *    hints is an optional pointer to a struct addrinfo. This structure can
 *    be used to provide hints concerning the type of socket that the caller
 *    supports or wishes to use. The caller can supply the following
 *    structure elements in *hints:
 *
 * <ul>
 *    <li>ai_family:
 *           The protocol family that should be used. When ai_family is set
 *           to PF_UNSPEC, it means the caller will accept any protocol
 *           family supported by the operating system.</li>
 *
 *    <li>ai_socktype:
 *           denotes the type of socket -- SOCK_STREAM, SOCK_DGRAM or
 *           SOCK_RAW -- that is wanted. When ai_socktype is zero the caller
 *           will accept any socket type.</li>
 *
 *    <li>ai_protocol:
 *           indicates which transport protocol is wanted: IPPROTO_UDP or
 *           IPPROTO_TCP. If ai_protocol is zero the caller will accept any
 *           protocol.</li>
 *
 *    <li>ai_flags:
 *           Flag bits. If the AI_CANONNAME bit is set, a successful call to
 *           getaddrinfo() will return a null-terminated string
 *           containing the canonical name of the specified hostname in
 *           ai_canonname of the first addrinfo structure returned. Setting
 *           the AI_PASSIVE bit indicates that the returned socket address
 *           structure is intended for used in a call to bind(2). In this
 *           case, if the hostname argument is a NULL pointer, then the IP
 *           address portion of the socket address structure will be set to
 *           INADDR_ANY for an IPv4 address or IN6ADDR_ANY_INIT for an IPv6
 *           address.<br /><br />
 *
 *           When ai_flags does not set the AI_PASSIVE bit, the returned
 *           socket address structure will be ready for use in a call to
 *           connect(2) for a connection-oriented protocol or connect(2),
 *           sendto(2), or sendmsg(2) if a connectionless protocol was
 *           chosen. The IP address portion of the socket address structure
 *           will be set to the loopback address if hostname is a NULL
 *           pointer and AI_PASSIVE is not set in ai_flags.<br /><br />
 *
 *           If ai_flags is set to AI_NUMERICHOST it indicates that hostname
 *           should be treated as a numeric string defining an IPv4 or IPv6
 *           address and no name resolution should be attempted.
 * </li></ul>
 *
 *    All other elements of the struct addrinfo passed via hints must be
 *    zero.
 *
 *    A hints of NULL is treated as if the caller provided a struct addrinfo
 *    initialized to zero with ai_familyset to PF_UNSPEC.
 *
 *    After a successful call to getaddrinfo(), *res is a pointer to a
 *    linked list of one or more addrinfo structures. Each struct addrinfo
 *    in this list cn be processed by following the ai_next pointer, until a
 *    NULL pointer is encountered. The three members ai_family, ai_socktype,
 *    and ai_protocol in each returned addrinfo structure contain the
 *    corresponding arguments for a call to socket(2). For each addrinfo
 *    structure in the list, the ai_addr member points to a filled-in socket
 *    address structure of length ai_addrlen.
 *
 *    All of the information returned by getaddrinfo() is dynamically
 *    allocated: the addrinfo structures, and the socket address structures
 *    and canonical host name strings pointed to by the addrinfostructures.
 *    Memory allocated for the dynamically allocated structures created by a
 *    successful call to getaddrinfo() is released by freeaddrinfo().
 *    ai is a pointer to a struct addrinfo created by a call to getaddrinfo().
 *
 * \section irsreturn RETURN VALUES
 *
 *    getaddrinfo() returns zero on success or one of the error codes
 *    listed in gai_strerror() if an error occurs. If both hostname and
 *    servname are NULL getaddrinfo() returns #EAI_NONAME.
 *
 * \section irssee SEE ALSO
 *
 *    getaddrinfo(), freeaddrinfo(),
 *    gai_strerror(), RFC3493, getservbyname(3), connect(2),
 *    sendto(2), sendmsg(2), socket(2).
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <isc/app.h>
#include <isc/buffer.h>
#include <isc/lib.h>
#include <isc/mem.h>
#include <isc/sockaddr.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/client.h>
#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>

#include <irs/context.h>
#include <irs/netdb.h>
#include <irs/resconf.h>

#define SA(addr)	((struct sockaddr *)(addr))
#define SIN(addr)	((struct sockaddr_in *)(addr))
#define SIN6(addr)	((struct sockaddr_in6 *)(addr))
#define SLOCAL(addr)	((struct sockaddr_un *)(addr))

/*! \struct addrinfo
 */
static struct addrinfo
	*ai_concat(struct addrinfo *ai1, struct addrinfo *ai2),
	*ai_reverse(struct addrinfo *oai),
	*ai_clone(struct addrinfo *oai, int family),
	*ai_alloc(int family, int addrlen);
#ifdef AF_LOCAL
static int get_local(const char *name, int socktype, struct addrinfo **res);
#endif

static int
resolve_name(int family, const char *hostname, int flags,
	     struct addrinfo **aip, int socktype, int port);

static int add_ipv4(const char *hostname, int flags, struct addrinfo **aip,
		    int socktype, int port);
static int add_ipv6(const char *hostname, int flags, struct addrinfo **aip,
		    int socktype, int port);
static void set_order(int, int (**)(const char *, int, struct addrinfo **,
				    int, int));

#define FOUND_IPV4	0x1
#define FOUND_IPV6	0x2
#define FOUND_MAX	2

#define ISC_AI_MASK (AI_PASSIVE|AI_CANONNAME|AI_NUMERICHOST)
/*%
 * Get a list of IP addresses and port numbers for host hostname and
 * service servname.
 */
int
getaddrinfo(const char *hostname, const char *servname,
	    const struct addrinfo *hints, struct addrinfo **res)
{
	struct servent *sp;
	const char *proto;
	int family, socktype, flags, protocol;
	struct addrinfo *ai, *ai_list;
	int err = 0;
	int port, i;
	int (*net_order[FOUND_MAX+1])(const char *, int, struct addrinfo **,
				      int, int);

	if (hostname == NULL && servname == NULL)
		return (EAI_NONAME);

	proto = NULL;
	if (hints != NULL) {
		if ((hints->ai_flags & ~(ISC_AI_MASK)) != 0)
			return (EAI_BADFLAGS);
		if (hints->ai_addrlen || hints->ai_canonname ||
		    hints->ai_addr || hints->ai_next) {
			errno = EINVAL;
			return (EAI_SYSTEM);
		}
		family = hints->ai_family;
		socktype = hints->ai_socktype;
		protocol = hints->ai_protocol;
		flags = hints->ai_flags;
		switch (family) {
		case AF_UNSPEC:
			switch (hints->ai_socktype) {
			case SOCK_STREAM:
				proto = "tcp";
				break;
			case SOCK_DGRAM:
				proto = "udp";
				break;
			}
			break;
		case AF_INET:
		case AF_INET6:
			switch (hints->ai_socktype) {
			case 0:
				break;
			case SOCK_STREAM:
				proto = "tcp";
				break;
			case SOCK_DGRAM:
				proto = "udp";
				break;
			case SOCK_RAW:
				break;
			default:
				return (EAI_SOCKTYPE);
			}
			break;
#ifdef	AF_LOCAL
		case AF_LOCAL:
			switch (hints->ai_socktype) {
			case 0:
				break;
			case SOCK_STREAM:
				break;
			case SOCK_DGRAM:
				break;
			default:
				return (EAI_SOCKTYPE);
			}
			break;
#endif
		default:
			return (EAI_FAMILY);
		}
	} else {
		protocol = 0;
		family = 0;
		socktype = 0;
		flags = 0;
	}

#ifdef	AF_LOCAL
	/*!
	 * First, deal with AF_LOCAL.  If the family was not set,
	 * then assume AF_LOCAL if the first character of the
	 * hostname/servname is '/'.
	 */

	if (hostname != NULL &&
	    (family == AF_LOCAL || (family == 0 && *hostname == '/')))
		return (get_local(hostname, socktype, res));

	if (servname != NULL &&
	    (family == AF_LOCAL || (family == 0 && *servname == '/')))
		return (get_local(servname, socktype, res));
#endif

	/*
	 * Ok, only AF_INET and AF_INET6 left.
	 */
	ai_list = NULL;

	/*
	 * First, look up the service name (port) if it was
	 * requested.  If the socket type wasn't specified, then
	 * try and figure it out.
	 */
	if (servname != NULL) {
		char *e;

		port = strtol(servname, &e, 10);
		if (*e == '\0') {
			if (socktype == 0)
				return (EAI_SOCKTYPE);
			if (port < 0 || port > 65535)
				return (EAI_SERVICE);
			port = htons((unsigned short) port);
		} else {
			sp = getservbyname(servname, proto);
			if (sp == NULL)
				return (EAI_SERVICE);
			port = sp->s_port;
			if (socktype == 0) {
				if (strcmp(sp->s_proto, "tcp") == 0)
					socktype = SOCK_STREAM;
				else if (strcmp(sp->s_proto, "udp") == 0)
					socktype = SOCK_DGRAM;
			}
		}
	} else
		port = 0;

	/*
	 * Next, deal with just a service name, and no hostname.
	 * (we verified that one of them was non-null up above).
	 */
	if (hostname == NULL && (flags & AI_PASSIVE) != 0) {
		if (family == AF_INET || family == 0) {
			ai = ai_alloc(AF_INET, sizeof(struct sockaddr_in));
			if (ai == NULL)
				return (EAI_MEMORY);
			ai->ai_socktype = socktype;
			ai->ai_protocol = protocol;
			SIN(ai->ai_addr)->sin_port = port;
			ai->ai_next = ai_list;
			ai_list = ai;
		}

		if (family == AF_INET6 || family == 0) {
			ai = ai_alloc(AF_INET6, sizeof(struct sockaddr_in6));
			if (ai == NULL) {
				freeaddrinfo(ai_list);
				return (EAI_MEMORY);
			}
			ai->ai_socktype = socktype;
			ai->ai_protocol = protocol;
			SIN6(ai->ai_addr)->sin6_port = port;
			ai->ai_next = ai_list;
			ai_list = ai;
		}

		*res = ai_list;
		return (0);
	}

	/*
	 * If the family isn't specified or AI_NUMERICHOST specified, check
	 * first to see if it is a numeric address.
	 * Though the gethostbyname2() routine will recognize numeric addresses,
	 * it will only recognize the format that it is being called for.  Thus,
	 * a numeric AF_INET address will be treated by the AF_INET6 call as
	 * a domain name, and vice versa.  Checking for both numerics here
	 * avoids that.
	 */
	if (hostname != NULL &&
	    (family == 0 || (flags & AI_NUMERICHOST) != 0)) {
		char abuf[sizeof(struct in6_addr)];
		char nbuf[NI_MAXHOST];
		int addrsize, addroff;
#ifdef IRS_HAVE_SIN6_SCOPE_ID
		char *p, *ep;
		char ntmp[NI_MAXHOST];
		isc_uint32_t scopeid;
#endif

#ifdef IRS_HAVE_SIN6_SCOPE_ID
		/*
		 * Scope identifier portion.
		 */
		ntmp[0] = '\0';
		if (strchr(hostname, '%') != NULL) {
			strncpy(ntmp, hostname, sizeof(ntmp) - 1);
			ntmp[sizeof(ntmp) - 1] = '\0';
			p = strchr(ntmp, '%');
			ep = NULL;

			/*
			 * Vendors may want to support non-numeric
			 * scopeid around here.
			 */

			if (p != NULL)
				scopeid = (isc_uint32_t)strtoul(p + 1,
								&ep, 10);
			if (p != NULL && ep != NULL && ep[0] == '\0')
				*p = '\0';
			else {
				ntmp[0] = '\0';
				scopeid = 0;
			}
		} else
			scopeid = 0;
#endif

		if (inet_pton(AF_INET, hostname, (struct in_addr *)abuf)
		    == 1) {
			if (family == AF_INET6) {
				/*
				 * Convert to a V4 mapped address.
				 */
				struct in6_addr *a6 = (struct in6_addr *)abuf;
				memcpy(&a6->s6_addr[12], &a6->s6_addr[0], 4);
				memset(&a6->s6_addr[10], 0xff, 2);
				memset(&a6->s6_addr[0], 0, 10);
				goto inet6_addr;
			}
			addrsize = sizeof(struct in_addr);
			addroff = (char *)(&SIN(0)->sin_addr) - (char *)0;
			family = AF_INET;
			goto common;
#ifdef IRS_HAVE_SIN6_SCOPE_ID
		} else if (ntmp[0] != '\0' &&
			   inet_pton(AF_INET6, ntmp, abuf) == 1) {
			if (family && family != AF_INET6)
				return (EAI_NONAME);
			addrsize = sizeof(struct in6_addr);
			addroff = (char *)(&SIN6(0)->sin6_addr) - (char *)0;
			family = AF_INET6;
			goto common;
#endif
		} else if (inet_pton(AF_INET6, hostname, abuf) == 1) {
			if (family != 0 && family != AF_INET6)
				return (EAI_NONAME);
		inet6_addr:
			addrsize = sizeof(struct in6_addr);
			addroff = (char *)(&SIN6(0)->sin6_addr) - (char *)0;
			family = AF_INET6;

		common:
			ai = ai_alloc(family,
				      ((family == AF_INET6) ?
				       sizeof(struct sockaddr_in6) :
				       sizeof(struct sockaddr_in)));
			if (ai == NULL)
				return (EAI_MEMORY);
			ai_list = ai;
			ai->ai_socktype = socktype;
			SIN(ai->ai_addr)->sin_port = port;
			memcpy((char *)ai->ai_addr + addroff, abuf, addrsize);
			if ((flags & AI_CANONNAME) != 0) {
#ifdef IRS_HAVE_SIN6_SCOPE_ID
				if (ai->ai_family == AF_INET6)
					SIN6(ai->ai_addr)->sin6_scope_id =
						scopeid;
#endif
				if (getnameinfo(ai->ai_addr, ai->ai_addrlen,
						nbuf, sizeof(nbuf), NULL, 0,
						NI_NUMERICHOST) == 0) {
					ai->ai_canonname = strdup(nbuf);
					if (ai->ai_canonname == NULL) {
						freeaddrinfo(ai);
						return (EAI_MEMORY);
					}
				} else {
					/* XXX raise error? */
					ai->ai_canonname = NULL;
				}
			}
			goto done;
		} else if ((flags & AI_NUMERICHOST) != 0) {
			return (EAI_NONAME);
		}
	}

	if (hostname == NULL && (flags & AI_PASSIVE) == 0) {
		set_order(family, net_order);
		for (i = 0; i < FOUND_MAX; i++) {
			if (net_order[i] == NULL)
				break;
			err = (net_order[i])(hostname, flags, &ai_list,
					     socktype, port);
			if (err != 0) {
				if (ai_list != NULL) {
					freeaddrinfo(ai_list);
					ai_list = NULL;
				}
				break;
			}
		}
	} else
		err = resolve_name(family, hostname, flags, &ai_list,
				   socktype, port);

	if (ai_list == NULL) {
		if (err == 0)
			err = EAI_NONAME;
		return (err);
	}

done:
	ai_list = ai_reverse(ai_list);

	*res = ai_list;
	return (0);
}

typedef struct gai_restrans {
	dns_clientrestrans_t	*xid;
	isc_boolean_t		is_inprogress;
	int			error;
	struct addrinfo		ai_sentinel;
	struct gai_resstate	*resstate;
} gai_restrans_t;

typedef struct gai_resstate {
	isc_mem_t			*mctx;
	struct gai_statehead		*head;
	dns_fixedname_t			fixedname;
	dns_name_t			*qname;
	gai_restrans_t			*trans4;
	gai_restrans_t			*trans6;
	ISC_LINK(struct gai_resstate)	link;
} gai_resstate_t;

typedef struct gai_statehead {
	int				ai_family;
	int				ai_flags;
	int				ai_socktype;
	int				ai_port;
	isc_appctx_t			*actx;
	dns_client_t			*dnsclient;
	ISC_LIST(struct gai_resstate)	resstates;
	unsigned int			activestates;
} gai_statehead_t;

static isc_result_t
make_resstate(isc_mem_t *mctx, gai_statehead_t *head, const char *hostname,
	      const char *domain, gai_resstate_t **statep)
{
	isc_result_t result;
	gai_resstate_t *state;
	dns_fixedname_t fixeddomain;
	dns_name_t *qdomain;
	size_t namelen;
	isc_buffer_t b;
	isc_boolean_t need_v4 = ISC_FALSE;
	isc_boolean_t need_v6 = ISC_FALSE;

	state = isc_mem_get(mctx, sizeof(*state));
	if (state == NULL)
		return (ISC_R_NOMEMORY);

	/* Construct base domain name */
	namelen = strlen(domain);
	isc_buffer_constinit(&b, domain, namelen);
	isc_buffer_add(&b, namelen);
	dns_fixedname_init(&fixeddomain);
	qdomain = dns_fixedname_name(&fixeddomain);
	result = dns_name_fromtext(qdomain, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(mctx, state, sizeof(*state));
		return (result);
	}

	/* Construct query name */
	namelen = strlen(hostname);
	isc_buffer_constinit(&b, hostname, namelen);
	isc_buffer_add(&b, namelen);
	dns_fixedname_init(&state->fixedname);
	state->qname = dns_fixedname_name(&state->fixedname);
	result = dns_name_fromtext(state->qname, &b, qdomain, 0, NULL);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(mctx, state, sizeof(*state));
		return (result);
	}

	if (head->ai_family == AF_UNSPEC || head->ai_family == AF_INET)
		need_v4 = ISC_TRUE;
	if (head->ai_family == AF_UNSPEC || head->ai_family == AF_INET6)
		need_v6 = ISC_TRUE;

	state->trans6 = NULL;
	state->trans4 = NULL;
	if (need_v4) {
		state->trans4 = isc_mem_get(mctx, sizeof(gai_restrans_t));
		if (state->trans4 == NULL) {
			isc_mem_put(mctx, state, sizeof(*state));
			return (ISC_R_NOMEMORY);
		}
		state->trans4->error = 0;
		state->trans4->xid = NULL;
		state->trans4->resstate = state;
		state->trans4->is_inprogress = ISC_TRUE;
		state->trans4->ai_sentinel.ai_next = NULL;
	}
	if (need_v6) {
		state->trans6 = isc_mem_get(mctx, sizeof(gai_restrans_t));
		if (state->trans6 == NULL) {
			if (state->trans4 != NULL)
				isc_mem_put(mctx, state->trans4,
					    sizeof(*state->trans4));
			isc_mem_put(mctx, state, sizeof(*state));
			return (ISC_R_NOMEMORY);
		}
		state->trans6->error = 0;
		state->trans6->xid = NULL;
		state->trans6->resstate = state;
		state->trans6->is_inprogress = ISC_TRUE;
		state->trans6->ai_sentinel.ai_next = NULL;
	}

	state->mctx = mctx;
	state->head = head;
	ISC_LINK_INIT(state, link);

	*statep = state;

	return (ISC_R_SUCCESS);
}

static isc_result_t
make_resstates(isc_mem_t *mctx, const char *hostname, gai_statehead_t *head,
	       irs_resconf_t *resconf)
{
	isc_result_t result;
	irs_resconf_searchlist_t *searchlist;
	irs_resconf_search_t *searchent;
	gai_resstate_t *resstate, *resstate0;

	resstate0 = NULL;
	result = make_resstate(mctx, head, hostname, ".", &resstate0);
	if (result != ISC_R_SUCCESS)
		return (result);

	searchlist = irs_resconf_getsearchlist(resconf);
	for (searchent = ISC_LIST_HEAD(*searchlist); searchent != NULL;
	     searchent = ISC_LIST_NEXT(searchent, link)) {
		resstate = NULL;
		result = make_resstate(mctx, head, hostname,
				       (const char *)searchent->domain,
				       &resstate);
		if (result != ISC_R_SUCCESS)
			break;

		ISC_LIST_APPEND(head->resstates, resstate, link);
		head->activestates++;
	}

	/*
	 * Insert the original hostname either at the head or the tail of the
	 * state list, depending on the number of labels contained in the
	 * original name and the 'ndots' configuration parameter.
	 */
	if (dns_name_countlabels(resstate0->qname) >
	    irs_resconf_getndots(resconf) + 1) {
		ISC_LIST_PREPEND(head->resstates, resstate0, link);
	} else
		ISC_LIST_APPEND(head->resstates, resstate0, link);
	head->activestates++;

	if (result != ISC_R_SUCCESS) {
		while ((resstate = ISC_LIST_HEAD(head->resstates)) != NULL) {
			ISC_LIST_UNLINK(head->resstates, resstate, link);
			if (resstate->trans4 != NULL) {
				isc_mem_put(mctx, resstate->trans4,
					    sizeof(*resstate->trans4));
			}
			if (resstate->trans6 != NULL) {
				isc_mem_put(mctx, resstate->trans6,
					    sizeof(*resstate->trans6));
			}

			isc_mem_put(mctx, resstate, sizeof(*resstate));
		}
	}

	return (result);
}

static void
process_answer(isc_task_t *task, isc_event_t *event) {
	int error = 0, family;
	gai_restrans_t *trans = event->ev_arg;
	gai_resstate_t *resstate;
	dns_clientresevent_t *rev = (dns_clientresevent_t *)event;
	dns_rdatatype_t qtype;
	dns_name_t *name;

	REQUIRE(trans != NULL);
	resstate = trans->resstate;
	REQUIRE(resstate != NULL);
	REQUIRE(task != NULL);

	if (trans == resstate->trans4) {
		family = AF_INET;
		qtype = dns_rdatatype_a;
	} else {
		INSIST(trans == resstate->trans6);
		family = AF_INET6;
		qtype = dns_rdatatype_aaaa;
	}

	INSIST(trans->is_inprogress);
	trans->is_inprogress = ISC_FALSE;

	switch (rev->result) {
	case ISC_R_SUCCESS:
	case DNS_R_NCACHENXDOMAIN: /* treat this as a fatal error? */
	case DNS_R_NCACHENXRRSET:
		break;
	default:
		switch (rev->vresult) {
		case DNS_R_SIGINVALID:
		case DNS_R_SIGEXPIRED:
		case DNS_R_SIGFUTURE:
		case DNS_R_KEYUNAUTHORIZED:
		case DNS_R_MUSTBESECURE:
		case DNS_R_COVERINGNSEC:
		case DNS_R_NOTAUTHORITATIVE:
		case DNS_R_NOVALIDKEY:
		case DNS_R_NOVALIDDS:
		case DNS_R_NOVALIDSIG:
			error = EAI_INSECUREDATA;
			break;
		default:
			error = EAI_FAIL;
		}
		goto done;
	}

	/* Parse the response and construct the addrinfo chain */
	for (name = ISC_LIST_HEAD(rev->answerlist); name != NULL;
	     name = ISC_LIST_NEXT(name, link)) {
		isc_result_t result;
		dns_rdataset_t *rdataset;
		isc_buffer_t b;
		isc_region_t r;
		char t[1024];

		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			if (!dns_rdataset_isassociated(rdataset))
				continue;
			if (rdataset->type != qtype)
				continue;

			if ((resstate->head->ai_flags & AI_CANONNAME) != 0) {
				isc_buffer_init(&b, t, sizeof(t));
				result = dns_name_totext(name, ISC_TRUE, &b);
				if (result != ISC_R_SUCCESS) {
					error = EAI_FAIL;
					goto done;
				}
				isc_buffer_putuint8(&b, '\0');
				isc_buffer_usedregion(&b, &r);
			}

			for (result = dns_rdataset_first(rdataset);
			     result == ISC_R_SUCCESS;
			     result = dns_rdataset_next(rdataset)) {
				struct addrinfo *ai;
				dns_rdata_t rdata;
				dns_rdata_in_a_t rdata_a;
				dns_rdata_in_aaaa_t rdata_aaaa;

				ai = ai_alloc(family,
					      ((family == AF_INET6) ?
					       sizeof(struct sockaddr_in6) :
					       sizeof(struct sockaddr_in)));
				if (ai == NULL) {
					error = EAI_MEMORY;
					goto done;
				}
				ai->ai_socktype = resstate->head->ai_socktype;
				ai->ai_next = trans->ai_sentinel.ai_next;
				trans->ai_sentinel.ai_next = ai;

				/*
				 * Set AF-specific parameters
				 * (IPv4/v6 address/port)
				 */
				dns_rdata_init(&rdata);
				switch (family) {
				case AF_INET:
					dns_rdataset_current(rdataset, &rdata);
					result = dns_rdata_tostruct(&rdata, &rdata_a,
								    NULL);
					RUNTIME_CHECK(result == ISC_R_SUCCESS);
					SIN(ai->ai_addr)->sin_port =
						resstate->head->ai_port;
					memcpy(&SIN(ai->ai_addr)->sin_addr,
					       &rdata_a.in_addr, 4);
					dns_rdata_freestruct(&rdata_a);
					break;
				case AF_INET6:
					dns_rdataset_current(rdataset, &rdata);
					result = dns_rdata_tostruct(&rdata, &rdata_aaaa,
								    NULL);
					RUNTIME_CHECK(result == ISC_R_SUCCESS);
					SIN6(ai->ai_addr)->sin6_port =
						resstate->head->ai_port;
					memcpy(&SIN6(ai->ai_addr)->sin6_addr,
					       &rdata_aaaa.in6_addr, 16);
					dns_rdata_freestruct(&rdata_aaaa);
					break;
				}

				if ((resstate->head->ai_flags & AI_CANONNAME)
				    != 0) {
					ai->ai_canonname =
						strdup((const char *)r.base);
					if (ai->ai_canonname == NULL) {
						error = EAI_MEMORY;
						goto done;
					}
				}
			}
		}
	}

 done:
	dns_client_freeresanswer(resstate->head->dnsclient, &rev->answerlist);
	dns_client_destroyrestrans(&trans->xid);

	isc_event_free(&event);

	/* Make sure that error == 0 iff we have a non-empty list */
	if (error == 0) {
		if (trans->ai_sentinel.ai_next == NULL)
			error = EAI_NONAME;
	} else {
		if (trans->ai_sentinel.ai_next != NULL) {
			freeaddrinfo(trans->ai_sentinel.ai_next);
			trans->ai_sentinel.ai_next = NULL;
		}
	}
	trans->error = error;

	/* Check whether we are done */
	if ((resstate->trans4 == NULL || !resstate->trans4->is_inprogress) &&
	    (resstate->trans6 == NULL || !resstate->trans6->is_inprogress)) {
		/*
		 * We're done for this state.  If there is no other outstanding
		 * state, we can exit.
		 */
		resstate->head->activestates--;
		if (resstate->head->activestates == 0) {
			isc_app_ctxsuspend(resstate->head->actx);
			return;
		}

		/*
		 * There are outstanding states, but if we are at the head
		 * of the state list (i.e., at the highest search priority)
		 * and have any answer, we can stop now by canceling the
		 * others.
		 */
		if (resstate == ISC_LIST_HEAD(resstate->head->resstates)) {
			if ((resstate->trans4 != NULL &&
			     resstate->trans4->ai_sentinel.ai_next != NULL) ||
			    (resstate->trans6 != NULL &&
			     resstate->trans6->ai_sentinel.ai_next != NULL)) {
				gai_resstate_t *rest;

				for (rest = ISC_LIST_NEXT(resstate, link);
				     rest != NULL;
				     rest = ISC_LIST_NEXT(rest, link)) {
					if (rest->trans4 != NULL &&
					    rest->trans4->xid != NULL)
						dns_client_cancelresolve(
							rest->trans4->xid);
					if (rest->trans6 != NULL &&
					    rest->trans6->xid != NULL)
						dns_client_cancelresolve(
							rest->trans6->xid);
				}
			} else {
				/*
				 * This search fails, so we move to the tail
				 * of the list so that the next entry will
				 * have the highest priority.
				 */
				ISC_LIST_UNLINK(resstate->head->resstates,
						resstate, link);
				ISC_LIST_APPEND(resstate->head->resstates,
						resstate, link);
			}
		}
	}
}

static int
resolve_name(int family, const char *hostname, int flags,
	     struct addrinfo **aip, int socktype, int port)
{
	isc_result_t result;
	irs_context_t *irsctx;
	irs_resconf_t *conf;
	isc_mem_t *mctx;
	isc_appctx_t *actx;
	isc_task_t *task;
	int terror = 0;
	int error = 0;
	dns_client_t *client;
	gai_resstate_t *resstate;
	gai_statehead_t head;
	isc_boolean_t all_fail = ISC_TRUE;

	/* get IRS context and the associated parameters */
	irsctx = NULL;
	result = irs_context_get(&irsctx);
	if (result != ISC_R_SUCCESS)
		return (EAI_FAIL);
	actx = irs_context_getappctx(irsctx);

	mctx = irs_context_getmctx(irsctx);
	task = irs_context_gettask(irsctx);
	conf = irs_context_getresconf(irsctx);
	client = irs_context_getdnsclient(irsctx);

	/* construct resolution states */
	head.activestates = 0;
	head.ai_family = family;
	head.ai_socktype = socktype;
	head.ai_flags = flags;
	head.ai_port = port;
	head.actx = actx;
	head.dnsclient = client;
	ISC_LIST_INIT(head.resstates);
	result = make_resstates(mctx, hostname, &head, conf);
	if (result != ISC_R_SUCCESS)
		return (EAI_FAIL);

	for (resstate = ISC_LIST_HEAD(head.resstates);
	     resstate != NULL; resstate = ISC_LIST_NEXT(resstate, link)) {
		if (resstate->trans4 != NULL) {
			result = dns_client_startresolve(client,
							 resstate->qname,
							 dns_rdataclass_in,
							 dns_rdatatype_a,
							 0, task,
							 process_answer,
							 resstate->trans4,
							 &resstate->trans4->xid);
			if (result == ISC_R_SUCCESS) {
				resstate->trans4->is_inprogress = ISC_TRUE;
				all_fail = ISC_FALSE;
			} else
				resstate->trans4->is_inprogress = ISC_FALSE;
		}
		if (resstate->trans6 != NULL) {
			result = dns_client_startresolve(client,
							 resstate->qname,
							 dns_rdataclass_in,
							 dns_rdatatype_aaaa,
							 0, task,
							 process_answer,
							 resstate->trans6,
							 &resstate->trans6->xid);
			if (result == ISC_R_SUCCESS) {
				resstate->trans6->is_inprogress = ISC_TRUE;
				all_fail = ISC_FALSE;
			} else
				resstate->trans6->is_inprogress= ISC_FALSE;
		}
	}
	if (!all_fail) {
		/* Start all the events */
		isc_app_ctxrun(actx);
	} else
		error = EAI_FAIL;

	/* Cleanup */
	while ((resstate = ISC_LIST_HEAD(head.resstates)) != NULL) {
		int terror4 = 0, terror6 = 0;

		ISC_LIST_UNLINK(head.resstates, resstate, link);

		if (*aip == NULL) {
			struct addrinfo *sentinel4 = NULL;
			struct addrinfo *sentinel6 = NULL;

			if (resstate->trans4 != NULL) {
				sentinel4 =
					resstate->trans4->ai_sentinel.ai_next;
				resstate->trans4->ai_sentinel.ai_next = NULL;
			}
			if (resstate->trans6 != NULL) {
				sentinel6 =
					resstate->trans6->ai_sentinel.ai_next;
				resstate->trans6->ai_sentinel.ai_next = NULL;
			}
			*aip = ai_concat(sentinel4, sentinel6);
		}

		if (resstate->trans4 != NULL) {
			INSIST(resstate->trans4->xid == NULL);
			terror4 = resstate->trans4->error;
			isc_mem_put(mctx, resstate->trans4,
				    sizeof(*resstate->trans4));
		}
		if (resstate->trans6 != NULL) {
			INSIST(resstate->trans6->xid == NULL);
			terror6 = resstate->trans6->error;
			isc_mem_put(mctx, resstate->trans6,
				    sizeof(*resstate->trans6));
		}

		/*
		 * If the entire lookup fails, we need to choose an appropriate
		 * error code from individual codes.  We'll try to provide as
		 * specific a code as possible.  In general, we are going to
		 * find an error code other than EAI_NONAME (which is too
		 * generic and may actually not be problematic in some cases).
		 * EAI_NONAME will be set below if no better code is found.
		 */
		if (terror == 0 || terror == EAI_NONAME) {
			if (terror4 != 0 && terror4 != EAI_NONAME)
				terror = terror4;
			else if (terror6 != 0 && terror6 != EAI_NONAME)
				terror = terror6;
		}

		isc_mem_put(mctx, resstate, sizeof(*resstate));
	}

	if (*aip == NULL) {
		error = terror;
		if (error == 0)
			error = EAI_NONAME;
	}

#if 1	/*  XXX: enabled for finding leaks.  should be cleaned up later. */
	isc_app_ctxfinish(actx);
	irs_context_destroy(&irsctx);
#endif

	return (error);
}

static char *
irs_strsep(char **stringp, const char *delim) {
	char *string = *stringp;
	char *s;
	const char *d;
	char sc, dc;

	if (string == NULL)
		return (NULL);

	for (s = string; *s != '\0'; s++) {
		sc = *s;
		for (d = delim; (dc = *d) != '\0'; d++)
			if (sc == dc) {
				*s++ = '\0';
				*stringp = s;
				return (string);
			}
	}
	*stringp = NULL;
	return (string);
}

static void
set_order(int family, int (**net_order)(const char *, int, struct addrinfo **,
					int, int))
{
	char *order, *tok;
	int found;

	if (family) {
		switch (family) {
		case AF_INET:
			*net_order++ = add_ipv4;
			break;
		case AF_INET6:
			*net_order++ = add_ipv6;
			break;
		}
	} else {
		order = getenv("NET_ORDER");
		found = 0;
		while (order != NULL) {
			/*
			 * We ignore any unknown names.
			 */
			tok = irs_strsep(&order, ":");
			if (strcasecmp(tok, "inet6") == 0) {
				if ((found & FOUND_IPV6) == 0)
					*net_order++ = add_ipv6;
				found |= FOUND_IPV6;
			} else if (strcasecmp(tok, "inet") == 0 ||
			    strcasecmp(tok, "inet4") == 0) {
				if ((found & FOUND_IPV4) == 0)
					*net_order++ = add_ipv4;
				found |= FOUND_IPV4;
			}
		}

		/*
		 * Add in anything that we didn't find.
		 */
		if ((found & FOUND_IPV4) == 0)
			*net_order++ = add_ipv4;
		if ((found & FOUND_IPV6) == 0)
			*net_order++ = add_ipv6;
	}
	*net_order = NULL;
	return;
}

static char v4_loop[4] = { 127, 0, 0, 1 };

static int
add_ipv4(const char *hostname, int flags, struct addrinfo **aip,
	 int socktype, int port)
{
	struct addrinfo *ai;

	UNUSED(hostname);
	UNUSED(flags);

	ai = ai_clone(*aip, AF_INET); /* don't use ai_clone() */
	if (ai == NULL) {
		freeaddrinfo(*aip);
		return (EAI_MEMORY);
	}

	*aip = ai;
	ai->ai_socktype = socktype;
	SIN(ai->ai_addr)->sin_port = port;
	memcpy(&SIN(ai->ai_addr)->sin_addr, v4_loop, 4);

	return (0);
}

static char v6_loop[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

static int
add_ipv6(const char *hostname, int flags, struct addrinfo **aip,
	 int socktype, int port)
{
	struct addrinfo *ai;

	UNUSED(hostname);
	UNUSED(flags);

	ai = ai_clone(*aip, AF_INET6); /* don't use ai_clone() */
	if (ai == NULL)
		return (EAI_MEMORY);

	*aip = ai;
	ai->ai_socktype = socktype;
	SIN6(ai->ai_addr)->sin6_port = port;
	memcpy(&SIN6(ai->ai_addr)->sin6_addr, v6_loop, 16);

	return (0);
}

/*% Free address info. */
void
freeaddrinfo(struct addrinfo *ai) {
	struct addrinfo *ai_next;

	while (ai != NULL) {
		ai_next = ai->ai_next;
		if (ai->ai_addr != NULL)
			free(ai->ai_addr);
		if (ai->ai_canonname)
			free(ai->ai_canonname);
		free(ai);
		ai = ai_next;
	}
}

#ifdef AF_LOCAL
static int
get_local(const char *name, int socktype, struct addrinfo **res) {
	struct addrinfo *ai;
	struct sockaddr_un *slocal;

	if (socktype == 0)
		return (EAI_SOCKTYPE);

	ai = ai_alloc(AF_LOCAL, sizeof(*slocal));
	if (ai == NULL)
		return (EAI_MEMORY);

	slocal = SLOCAL(ai->ai_addr);
	strlcpy(slocal->sun_path, name, sizeof(slocal->sun_path));

	ai->ai_socktype = socktype;
	/*
	 * ai->ai_flags, ai->ai_protocol, ai->ai_canonname,
	 * and ai->ai_next were initialized to zero.
	 */

	*res = ai;
	return (0);
}
#endif

/*!
 * Allocate an addrinfo structure, and a sockaddr structure
 * of the specificed length.  We initialize:
 *	ai_addrlen
 *	ai_family
 *	ai_addr
 *	ai_addr->sa_family
 *	ai_addr->sa_len	(IRS_PLATFORM_HAVESALEN)
 * and everything else is initialized to zero.
 */
static struct addrinfo *
ai_alloc(int family, int addrlen) {
	struct addrinfo *ai;

	ai = (struct addrinfo *)calloc(1, sizeof(*ai));
	if (ai == NULL)
		return (NULL);

	ai->ai_addr = SA(calloc(1, addrlen));
	if (ai->ai_addr == NULL) {
		free(ai);
		return (NULL);
	}
	ai->ai_addrlen = addrlen;
	ai->ai_family = family;
	ai->ai_addr->sa_family = family;
#ifdef IRS_PLATFORM_HAVESALEN
	ai->ai_addr->sa_len = addrlen;
#endif
	return (ai);
}

static struct addrinfo *
ai_clone(struct addrinfo *oai, int family) {
	struct addrinfo *ai;

	ai = ai_alloc(family, ((family == AF_INET6) ?
	    sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in)));

	if (ai == NULL) {
		if (oai != NULL)
			freeaddrinfo(oai);
		return (NULL);
	}
	if (oai == NULL)
		return (ai);

	ai->ai_flags = oai->ai_flags;
	ai->ai_socktype = oai->ai_socktype;
	ai->ai_protocol = oai->ai_protocol;
	ai->ai_canonname = NULL;
	ai->ai_next = oai;
	return (ai);
}

static struct addrinfo *
ai_reverse(struct addrinfo *oai) {
	struct addrinfo *nai, *tai;

	nai = NULL;

	while (oai != NULL) {
		/*
		 * Grab one off the old list.
		 */
		tai = oai;
		oai = oai->ai_next;
		/*
		 * Put it on the front of the new list.
		 */
		tai->ai_next = nai;
		nai = tai;
	}
	return (nai);
}


static struct addrinfo *
ai_concat(struct addrinfo *ai1, struct addrinfo *ai2) {
	struct addrinfo *ai_tmp;

	if (ai1 == NULL)
		return (ai2);
	else if (ai2 == NULL)
		return (ai1);

	for (ai_tmp = ai1; ai_tmp != NULL && ai_tmp->ai_next != NULL;
	     ai_tmp = ai_tmp->ai_next)
		;

	ai_tmp->ai_next = ai2;

	return (ai1);
}
