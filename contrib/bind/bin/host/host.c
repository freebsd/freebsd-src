#ifndef lint
static const char rcsid[] = "$Id: host.c,v 8.55.8.1 2003/06/02 09:24:38 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986
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
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1986 Regents of the University of California.\n\
 Portions Copyright (c) 1993 Digital Equipment Corporation.\n\
 Portions Copyright (c) 1996-1999 Internet Software Consortium.\n\
 All rights reserved.\n";
#endif /* not lint */

/*
 * Actually, this program is from Rutgers University, however it is 
 * based on nslookup and other pieces of named tools, so it needs
 * the above copyright notices.
 */

/* Import. */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory.h>
#include <errno.h>
#include <sys/stat.h>
#include <isc/dst.h>

#include "port_after.h"

#include <resolv.h>

/* Global. */

#ifndef PATH_SEP
#define PATH_SEP '/'
#endif
#define SIG_RDATA_BY_NAME	18
#define NS_HEADERDATA_SIZE 10	/* type + class + ttl + rdlen */

#define NUMNS		8
#define NUMNSADDR	16
#define NUMMX		50
#define NUMRR		127   /* max rr's per node to verify signatures for */

#define SUCCESS		0
#define TIME_OUT	-1
#define NO_INFO 	-2
#ifdef ERROR
#undef ERROR
#endif
#define ERROR 		-3
#define NONAUTH 	-4

#define MY_PACKETSZ    NS_MAXMSG

typedef union {
	HEADER	qb1;
	u_char	qb2[MY_PACKETSZ];
} querybuf;

#define SD_RR        1
#define SD_SIG       2
#define SD_BADSIG    4

typedef struct {
	u_char data[MY_PACKETSZ];
	size_t len;
} rrstruct;

static char		chase_domain[NS_MAXDNAME];
static int		chase_class;
static int		chase_type;
static char		chase_sigorigttl[NS_INT32SZ];
static rrstruct		chase_rr[NUMRR];
static int		chase_rr_num;
static char		chase_lastgoodkey[NS_MAXDNAME];
static char		chase_signer[NS_MAXDNAME];
static u_char		chase_sigrdata[MY_PACKETSZ];
static size_t		chase_sigrdata_len;
static u_char		chase_signature[MY_PACKETSZ];
static size_t		chase_signature_len;
static int		chase_step;
static int		sigchase;

static char		cnamebuf[NS_MAXDNAME];
static u_char		hostbuf[NS_MAXDNAME];

static int		sockFD;
static FILE		*filePtr;

static struct __res_state  res;
static char		*cname = NULL;
static const char	*progname = "amnesia";
static int		getclass = ns_c_in, verbose = 0, list = 0;
static int		server_specified = 0;
static int		gettype = 0;
static int		querytype = 0;
static char		getdomain[NS_MAXDNAME];

/* Forward. */

static int		parsetype(const char *s);
static int		parseclass(const char *s);
static void		hperror(int errnum);
static int		addrinfo(struct sockaddr_storage *addr);
static int		gethostinfo(char *name);
static int		getdomaininfo(const char *name, const char *domain);
static int		getinfo(const char *name, const char *domain,
				int type);
static int		printinfo(const querybuf *answer, const u_char *eom,
				  int filter, int isls, int isinaddr);
static const u_char *	pr_rr(const u_char *cp, const u_char *msg, FILE *file,
			      int filter);
static const char *	pr_type(int type);
static const char *	pr_class(int class);
static const u_char *	pr_cdname(const u_char *cp, const u_char *msg,
				  char *name, int namelen);
static int		ListHosts(char *namePtr, int queryType);
static const char *	DecodeError(int result);

static void
usage(const char *msg) {
	fprintf(stderr, "%s: usage error (%s)\n", progname, msg);
	fprintf(stderr, "\
Usage: %s [-adlrwv] [-t querytype] [-c class] host [server]\n\
\t-a is equivalent to '-v -t *'\n\
\t-c class to look for non-Internet data\n\
\t-d to turn on debugging output\n\
\t-l to turn on 'list mode'\n\
\t-r to disable recursive processing\n\
\t-s recursively chase signature found in answers\n\
\t-t querytype to look for a specific type of information\n\
\t-v for verbose output\n\
\t-w to wait forever until reply\n\
", progname);
	exit(1);
}

/* Public. */

int
main(int argc, char **argv) {
	struct sockaddr_storage addr;
	int ok = 0;
	char *s;
	int waitmode = 0;
	int ncnames, ch;
	int nkeychains;
	struct addrinfo *answer = NULL;
	struct addrinfo *cur = NULL;
	struct addrinfo hint;
	int ip = 0;

	dst_init();

	if ((progname = strrchr(argv[0], PATH_SEP)) == NULL)
		progname = argv[0];
	else
		progname++;
	res_ninit(&res);
	res.retrans = 5;
	while ((ch = getopt(argc, argv, "ac:dlrst:vw")) != -1) {
		switch (ch) {
		case 'a':
			verbose = 1;
			querytype = ns_t_any;
			break;
		case 'c':
			getclass = parseclass(optarg);
			break;
		case 'd':
			res.options |= RES_DEBUG;
			break;
		case 'l':
			list = 1;
			break;
		case 'r':
			res.options &= ~RES_RECURSE;
			break;
		case 's':
			sigchase = 1;
			break;
		case 't':
			querytype = parsetype(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			res.retry = 1;
			res.retrans = 15;
			waitmode = 1;
			break;
		default:
			usage("unrecogized switch");
			/*NOTREACHED*/
		}
	}
	if ((querytype == 0) && (sigchase)) {
		if (verbose)
			printf ("Forcing `-t a' for signature trace.\n");
		querytype = ns_t_a;
	}
	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage("missing host argument");
	strncpy(getdomain, *argv++, NS_MAXDNAME);
	getdomain[NS_MAXDNAME-1] = 0;
	argc--;
	if (argc > 1)
		usage("extra undefined arguments");
	if (argc == 1) {
		union res_sockaddr_union u[MAXNS];
		int nscount;

		s = *argv++;
		argc--;
		server_specified++;
		memset(&hint, 0, sizeof(hint));
		hint.ai_flags = AI_CANONNAME;
		hint.ai_family = PF_UNSPEC;
		hint.ai_socktype = SOCK_DGRAM;
		
		if (!getaddrinfo(s, NULL, &hint, &answer)) {
			nscount = 0;
			if (answer->ai_canonname != NULL) {
				printf("Using domain server:\n");
				printf("Name: %s\n", answer->ai_canonname);
				printf("Addresses:");
			} else
				printf("Using domain server");
			
			for (cur = answer; cur != NULL; cur = cur->ai_next) {
				char buf[80];
				struct sockaddr_in6 *sin6;
				struct sockaddr_in *sin;
				
				switch (cur->ai_addr->sa_family) {
				case AF_INET6:
					sin6 = 
					   (struct sockaddr_in6 *)cur->ai_addr;
					inet_ntop(cur->ai_addr->sa_family,
					          &sin6->sin6_addr,
						  buf, sizeof(buf));
					printf(" %s", buf);
					if (nscount >= MAXNS)
						break;
					u[nscount].sin6 = *sin6;
					u[nscount++].sin6.sin6_port =
						htons(NAMESERVER_PORT);
					break;
				case AF_INET:
					sin =
					     (struct sockaddr_in*)cur->ai_addr;
					inet_ntop(cur->ai_addr->sa_family,
					          &sin->sin_addr,
						  buf, sizeof(buf));
					printf(" %s", buf);
					if (nscount >= MAXNS)
						break;
					u[nscount].sin = *sin;
					u[nscount++].sin6.sin6_port =
						htons(NAMESERVER_PORT);
					break;
				}
			}
			if (nscount != 0) {
				res_setservers(&res, u, nscount);
			}
			if (answer->ai_canonname != NULL)
				printf("\n\n");
			else
				printf(":\n\n");
			freeaddrinfo(answer);
		} else {
			fprintf(stderr, "Error in looking up server name:\n");
			exit(1);
		}
		res.retry = 2;
	}
	memset(&hint, 0, sizeof(hint));
	hint.ai_flags = AI_NUMERICHOST;
	hint.ai_socktype = SOCK_DGRAM;
	if(!getaddrinfo(getdomain, NULL, &hint, &answer)) { 
		memset(&addr, 0, sizeof(addr));
		switch (answer->ai_family) {
		case AF_INET:
			memcpy(&addr, answer->ai_addr,
			       sizeof(struct sockaddr_in));
			ip = 1;
			break;
		case AF_INET6:
			memcpy(&addr, answer->ai_addr,
			       sizeof(struct sockaddr_in6));
			ip = 1;
			break;
		}
		freeaddrinfo(answer);
	} 
	res.res_h_errno = TRY_AGAIN;
/*
 * We handle default domains ourselves, thank you.
 */
	res.options &= ~RES_DEFNAMES;

        if (list)
		exit(ListHosts(getdomain, querytype ? querytype : ns_t_a));
	ncnames = 5; nkeychains = 18;
	while (ok == 0 && res.res_h_errno == TRY_AGAIN) {
		if (!ip) {
			cname = NULL;
			ok = gethostinfo(getdomain);
			getdomain[0] = 0; /* clear this query */
			if (sigchase && (chase_step & SD_RR)) {
				if (nkeychains-- == 0) {
					printf("Too many sig/key chains. Loop?\n");
					exit(1);
				}
				if (chase_step & SD_SIG) {
					/* start new query, for KEY */
					strcpy (getdomain, chase_signer);
					strcat (getdomain, ".");
					querytype = ns_t_key;
				} else if (!(chase_step & SD_BADSIG)) { 
					/* start new query, for SIG */
					strcpy (getdomain, chase_domain);
					strcat (getdomain, ".");
					querytype = ns_t_sig;
				} else if (ok != 0 && !(chase_step & SD_SIG) && 
					   (chase_step & SD_BADSIG)) {
					printf ("%s for %s not found, last verified key %s\n",
						chase_step & SD_SIG ? "Key" : "Signature",
						chase_step & SD_SIG ? chase_signer : chase_domain, 
						chase_lastgoodkey[0] ? chase_lastgoodkey : "None");
				}
			}
			if (!getdomain[0] && cname) {
				if (ncnames-- == 0) {
					printf("Too many cnames.  Loop?\n");
					exit(1);
				}
				strcpy(getdomain, cname);
				strcat(getdomain, ".");
			}
			if (getdomain[0]) {
				if (chase_step & SD_SIG) {
					printf ("Locating key for %s\n", getdomain);
				} else if (chase_step & SD_SIG) {
					printf ("Locating signature for %s record(s) on %s\n",
						sym_ntos(__p_type_syms, chase_type, NULL),
						getdomain);
				}
				ok = 0;
				res.res_h_errno = TRY_AGAIN;
				continue;
			}
		} else 
			ok = addrinfo(&addr);
		if (!waitmode)
			break;
	}

	if (ok == 0) {
		hperror(res.res_h_errno);
		exit(1);
	}

	exit(0);
}

/* Private. */

static int
parsetype(const char *s) {
	int type, success;

	type = sym_ston(__p_type_syms, s, &success);
	if (success)
		return (type);
	if (strcmp(s, "*") == 0)
		return (ns_t_any);
	if (atoi(s))
		return (atoi(s));
	fprintf(stderr, "Invalid query type: %s\n", s);
	exit(2);
	/*NOTREACHED*/
}

static int
parseclass(const char *s) {
	int class, success;

	class = sym_ston(__p_class_syms, s, &success);
	if (success)
		return (class);
	if (atoi(s))
		return (atoi(s));
	fprintf(stderr, "Invalid query class: %s\n", s);
	exit(2);
	/*NOTREACHED*/
}

static void
hperror(int errnum) {
	switch(errnum) {
	case HOST_NOT_FOUND:
		fprintf(stderr, "Host not found.\n");
		break;
	case TRY_AGAIN:
		fprintf(stderr, "Host not found, try again.\n");
		break;
	case NO_RECOVERY:
		fprintf(stderr, "No recovery, Host not found.\n");
		break;
	case NO_ADDRESS:
		fprintf(stderr,
			"There is an entry for this host, but it doesn't have "
			);
		switch (gettype) {
		case ns_t_a:
			fprintf(stderr, "an Internet address.\n");
			break;
		case ns_t_ns:
			fprintf(stderr, "a Name Server.\n");
			break;
		case ns_t_md:
			fprintf(stderr, "a Mail Destination.\n");
			break;
		case ns_t_mf:
			fprintf(stderr, "a Mail Forwarder.\n");
			break;
		case ns_t_cname:
			fprintf(stderr, "a Canonical Name.\n");
			break;
		case ns_t_soa:
			fprintf(stderr, "a Start of Authority record.\n");
			break;
		case ns_t_mb:
			fprintf(stderr, "a Mailbox Domain Name.\n");
			break;
		case ns_t_mg:
			fprintf(stderr, "a Mail Group Member.\n");
			break;
		case ns_t_mr:
			fprintf(stderr, "a Mail Rename Name.\n");
			break;
		case ns_t_null:
			fprintf(stderr, "a Null Resource record.\n");
			break;
		case ns_t_wks:
			fprintf(stderr, "any Well Known Service information.\n");
			break;
		case ns_t_ptr:
			fprintf(stderr, "a Pointer record.\n");
			break;
		case ns_t_hinfo:
			fprintf(stderr, "any Host Information.\n");
			break;
		case ns_t_minfo:
			fprintf(stderr, "any Mailbox Information.\n");
			break;
		case ns_t_mx:
			fprintf(stderr, "a Mail Exchanger record.\n");
			break;
		case ns_t_txt:
			fprintf(stderr, "a Text record.\n");
			break;
		case ns_t_rp:
			fprintf(stderr, "a Responsible Person.\n");
			break;
		case ns_t_srv:
			fprintf(stderr, "a Server Selector.\n");
			break;
		case ns_t_naptr:
			fprintf(stderr, "a URN Naming Authority.\n");
			break;
		default:
			fprintf(stderr, "the information you requested.\n");
			break;
		}
		break;
	}
}

static int
addrinfo(struct sockaddr_storage *addr) {
	char name[NS_MAXDNAME];
	unsigned char *p;
	struct in6_addr *addr6;

	switch(addr->ss_family) {
	case AF_INET:
		p = (unsigned char*)&((struct sockaddr_in *)addr)->sin_addr;
	mapped:
		sprintf(name, "%u.%u.%u.%u.IN-ADDR.ARPA.",
			p[3], p[2], p[1], p[0]);
		break;
	case AF_INET6:
		addr6 = &((struct sockaddr_in6 *)addr)->sin6_addr;
		p = (unsigned char *)addr6;
		if (IN6_IS_ADDR_V4MAPPED(addr6) ||
		    IN6_IS_ADDR_V4COMPAT(addr6)) {
			p += 12;
			goto mapped;
		}
		sprintf(name,
			"%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
			"%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
			"IP6.ARPA",
			p[15] & 0xf, (p[15] >> 4) & 0xf,
			p[14] & 0xf, (p[14] >> 4) & 0xf,
			p[13] & 0xf, (p[13] >> 4) & 0xf,
			p[12] & 0xf, (p[12] >> 4) & 0xf,
			p[11] & 0xf, (p[11] >> 4) & 0xf,
			p[10] & 0xf, (p[10] >> 4) & 0xf,
			p[9] & 0xf, (p[9] >> 4) & 0xf,
			p[8] & 0xf, (p[8] >> 4) & 0xf,
			p[7] & 0xf, (p[7] >> 4) & 0xf,
			p[6] & 0xf, (p[6] >> 4) & 0xf,
			p[5] & 0xf, (p[5] >> 4) & 0xf,
			p[4] & 0xf, (p[4] >> 4) & 0xf,
			p[3] & 0xf, (p[3] >> 4) & 0xf,
			p[2] & 0xf, (p[2] >> 4) & 0xf,
			p[1] & 0xf, (p[1] >> 4) & 0xf,
			p[0] & 0xf, (p[0] >> 4) & 0xf);
		break;
	default:
		abort();
	}
	return (getinfo(name, NULL, ns_t_ptr));
}

static int
gethostinfo(char *name) {
	char *cp, **domain;
	char tmp[NS_MAXDNAME];
	const char *tp;
	int hp;
	int asis = 0;
	u_int n;

	if (strcmp(name, ".") == 0)
		return (getdomaininfo(name, NULL));
	for (cp = name, n = 0; *cp; cp++)
		if (*cp == '.')
			n++;
	if (n && cp[-1] == '.') {
		if (cp[-1] == '.')
			cp[-1] = 0;
		hp = getdomaininfo(name, (char *)NULL);
		if (cp[-1] == 0)
			cp[-1] = '.';
		return (hp);
	}
	if (n == 0 && (tp = res_hostalias(&res, name, tmp, sizeof tmp))) {
	        if (verbose)
		    printf("Aliased to \"%s\"\n", tp);
		res.options |= RES_DEFNAMES;	  
		return (getdomaininfo(tp, (char *)NULL));
	}
	if (n >= res.ndots) {
		asis = 1;
		if (verbose)
		    printf("Trying null domain\n");
		hp = getdomaininfo(name, (char*)NULL);
		if (hp)
			return (hp);
	}
	for (domain = res.dnsrch; *domain; domain++) {
		if (verbose)
			printf("Trying domain \"%s\"\n", *domain);
		hp = getdomaininfo(name, *domain);
		if (hp)
			return (hp);
	}
	if (res.res_h_errno != HOST_NOT_FOUND || (res.options & RES_DNSRCH) == 0)
		return (0);
	if (!asis)
		return (0);
	if (verbose)
		printf("Trying null domain\n");
	return (getdomaininfo(name, (char *)NULL));
}

static int
getdomaininfo(const char *name, const char *domain) {
	int val1, val2, val3;

	if (querytype)
		return (getinfo(name, domain, gettype=querytype));
	else {
		val1 = getinfo(name, domain, gettype=ns_t_a);
		if (cname || verbose)
			return (val1);
		val2 = getinfo(name, domain, gettype=ns_t_aaaa);
		val3 = getinfo(name, domain, gettype=ns_t_mx);
		return (val1 || val2 || val3);
	}
}

static int
getinfo(const char *name, const char *domain, int type) {
	u_char *eom;
	querybuf buf, answer;
	int n;
	char host[NS_MAXDNAME];

	if (domain == NULL ||
	    (domain[0] == '.' && domain[1] == '\0'))
		sprintf(host, "%.*s", NS_MAXDNAME, name);
	else
		sprintf(host, "%.*s.%.*s",
			NS_MAXDNAME, name, NS_MAXDNAME, domain);

	n = res_nmkquery(&res, QUERY, host, getclass, type, NULL, 0, NULL,
			 buf.qb2, sizeof buf);
	if (n < 0) {
		if (res.options & RES_DEBUG)
			printf("res_nmkquery failed\n");
		res.res_h_errno = NO_RECOVERY;
		return (0);
	}
	n = res_nsend(&res, buf.qb2, n, answer.qb2, sizeof answer);
	if (n < 0) {
		if (res.options & RES_DEBUG)
			printf("res_nsend failed\n");
		res.res_h_errno = TRY_AGAIN;
		return (0);
	}
	eom = answer.qb2 + n;
	return (printinfo(&answer, eom, ns_t_any, 0, (type == ns_t_ptr)));
}

static int
printinfo(const querybuf *answer, const u_char *eom, int filter, int isls,
	  int isinaddr)
{
	int n, nmx, ancount, nscount, arcount, qdcount, buflen, savesigchase;
	const u_char *bp, *cp;
	const HEADER *hp;

	/*
	 * Find first satisfactory answer.
	 */
	hp = (const HEADER *) answer;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	nscount = ntohs(hp->nscount);
	arcount = ntohs(hp->arcount);
	if (res.options & RES_DEBUG || (verbose && isls == 0))
		printf("rcode = %d (%s), ancount=%d\n", 
		       hp->rcode, DecodeError(hp->rcode), ancount);
	if (hp->rcode != NOERROR || (ancount+nscount+arcount) == 0) {
		switch (hp->rcode) {
		case NXDOMAIN:
			res.res_h_errno = HOST_NOT_FOUND;
			return (0);
		case SERVFAIL:
			res.res_h_errno = TRY_AGAIN;
			return (0);
		case NOERROR:
			res.res_h_errno = NO_DATA;
			return (0);
		case FORMERR:
		case NOTIMP:
		case REFUSED:
			res.res_h_errno = NO_RECOVERY;
			return (0);
		}
		return (0);
	}
	bp = hostbuf;
	nmx = 0;
	buflen = sizeof(hostbuf);
	cp = answer->qb2 + HFIXEDSZ;
	if (qdcount > 0) {
		while (qdcount-- > 0) {
			n = dn_skipname(cp, eom);
			if (n < 0) {
				printf("Form error.\n");
				return (0);
			}
			cp += n + QFIXEDSZ;
			if (cp > eom) {
				printf("Form error.\n");
				return (0);
			}
		}
	}
	if (ancount) {
		if (!hp->aa)
			if (verbose && isls == 0)
				printf(
				 "The following answer is not authoritative:\n"
				       );
		if (!hp->ad)
		    if (verbose && isls == 0)
			printf("The following answer is not verified as authentic by the server:\n");
		while (--ancount >= 0 && cp && cp < eom) {
			cp = pr_rr(cp, answer->qb2, stdout, filter);
			/*
			 * When we ask for address and there is a CNAME, it
			 * seems to return both the CNAME and the address.
			 * Since we trace down the CNAME chain ourselves, we
			 * don't really want to print the address at this
			 * point.
			 */
                        if (cname && (!verbose) && (!isinaddr))
                                return (1);
                }
	}
	if (!verbose)
		return (1);

	/* don't chase signatures for non-answer stuff */

	savesigchase = sigchase;
	sigchase = 0;

	if (nscount) {
		printf("For authoritative answers, see:\n");
		while (--nscount >= 0 && cp && cp < eom)
			cp = pr_rr(cp, answer->qb2, stdout, filter);
	}
	if (arcount) {
		printf("Additional information:\n");
		while (--arcount >= 0 && cp && cp < eom)
			cp = pr_rr(cp, answer->qb2, stdout, filter);
	}

	/* restore sigchase value */
	
	sigchase = savesigchase;

	return (1);
}

static void print_hex_field (u_int8_t field[], int length, int width,
			     const char *pref)
{
	/* Prints an arbitrary bit field, from one address for some number of
		bytes.  Output is formatted via the width, and includes the raw
		hex value and (if printable) the printed value underneath.  "pref"
		is a string used to start each line, e.g., "   " to indent.

		This is very useful in gdb to see what's in a memory field.
	*/
	int		i, start, stop;

	start=0;
	do
	{
		stop=(start+width)<length?(start+width):length;
		printf ("%s", pref);
 		for (i = start; i < stop; i++)
			printf ("%02x ", (u_char) field[i]);
		printf ("\n");

		printf ("%s", pref);
		for (i = start; i < stop; i++)
			if (isprint(field[i]))
				printf (" %c ", (u_char) field[i]);
			else
				printf ("   ");
			printf ("\n");

		start = stop;
	} while (start < length);
}

static void memswap (void *s1, void *s2, size_t n)
{
	void *tmp;

	tmp = malloc(n);
	if (!tmp) {
		printf ("Out of memory\n");
		exit (1);
	}
		
	memcpy(tmp, s1, n);
	memcpy(s1, s2, n);
	memcpy(s2, tmp, n);

	free (tmp);
}

/*
 * Print resource record fields in human readable form.
 */
static const u_char *
pr_rr(const u_char *cp, const u_char *msg, FILE *file, int filter) {
	int type, class, dlen, n, c, proto, ttl;
	struct in_addr inaddr;
	u_char in6addr[NS_IN6ADDRSZ];
	const u_char *savecp = cp;
	const u_char *cp1;
	struct protoent *protop;
	struct servent *servp;
	char punc = ' ';
	int doprint;
	char name[NS_MAXDNAME];
	char thisdomain[NS_MAXDNAME];
	char tmpbuf[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
	u_char canonrr[MY_PACKETSZ];
	size_t canonrr_len = 0;

	cp = pr_cdname(cp, msg, name, sizeof(name));
	if (cp == NULL)
		return (NULL);			/* compression error */
	strcpy(thisdomain, name);

	type = ns_get16(cp);
	cp += INT16SZ;

	class = ns_get16(cp);
	cp += INT16SZ;

	ttl = ns_get32(cp);
	cp += INT32SZ;

	if (filter == type || filter == ns_t_any ||
	    (filter == ns_t_a && (type == ns_t_ptr || type == ns_t_ns)))
		doprint = 1;
	else
		doprint = 0;

	if (doprint) {
		if (verbose)
			fprintf(file, "%s\t%d%s\t%s",
				name, ttl, pr_class(class), pr_type(type));
		else
			fprintf(file, "%s%s %s",
				name, pr_class(class), pr_type(type));
		if (verbose)
			punc = '\t';
		else
			punc = ' ';
	}
	dlen = ns_get16(cp);
	cp += INT16SZ;
	cp1 = cp;

	/*
	 * Print type specific data, if appropriate.
	 */
	switch (type) {
	case ns_t_a:
		memcpy(&inaddr, cp, NS_INADDRSZ);
	        if (doprint)
			fprintf(file,"%c%s", punc, inet_ntoa(inaddr));
		cp += dlen;
		break;
	case ns_t_aaaa:
		memcpy(in6addr, cp, NS_IN6ADDRSZ);
	        if (doprint) {
			if (inet_ntop(AF_INET6, in6addr, tmpbuf,
				      sizeof tmpbuf) != NULL)
				fprintf(file,"%c%s", punc, tmpbuf);
			else
				fprintf(file,"%c???", punc);
		}
		cp += dlen;
		break;
	case ns_t_cname:
		if (dn_expand(msg, msg + 512, cp, cnamebuf, 
			      sizeof(cnamebuf)) >= 0)
			cname = cnamebuf;				
	case ns_t_mb:
	case ns_t_mg:
	case ns_t_mr:
	case ns_t_ns:
	case ns_t_ptr:
	{
		const u_char *startrdata = cp;
		u_char cdname[NS_MAXCDNAME];

		cp = pr_cdname(cp, msg, name, sizeof name);
		if (doprint)
			fprintf(file, "%c%s", punc, name);

		/* Extract DNSSEC canonical RR. */

		n = ns_name_unpack(msg, msg+MY_PACKETSZ, startrdata,
				   cdname, sizeof cdname);
		if (n >= 0)
			n = ns_name_ntol(cdname, cdname, sizeof cdname);
		if (n >= 0) {
			/* Copy header. */
			memcpy(canonrr, cp1 - NS_HEADERDATA_SIZE, NS_HEADERDATA_SIZE);
			/* Overwrite length field. */
			ns_put16(n, canonrr + NS_HEADERDATA_SIZE - NS_INT16SZ);
			/* Copy unpacked name. */
			memcpy(canonrr + NS_HEADERDATA_SIZE, cdname, n);
			canonrr_len = NS_HEADERDATA_SIZE + n;
		}
		break;
	}

	case ns_t_hinfo:
	case ns_t_isdn:
		{
			const u_char *cp2 = cp + dlen;
			n = *cp++;
			if (n != 0) {
				if (doprint)
					fprintf(file,"%c%.*s", punc, n, cp);
				cp += n;
			}
			if ((cp < cp2) && (n = *cp++)) {
				if (doprint)
					fprintf(file,"%c%.*s", punc, n, cp);
				cp += n;
			} else if (type == ns_t_hinfo)
				if (doprint)
					fprintf(file,
					  "\n; *** Warning *** OS-type missing"
						);
		}
		break;

	case ns_t_soa:
	{
		const u_char *startname = cp;
		u_char cdname[NS_MAXCDNAME];

		cp = pr_cdname(cp, msg, name, sizeof name);
		if (doprint)
			fprintf(file, "\t%s", name);

		n = ns_name_unpack(msg, msg + 512, startname,
				   cdname, sizeof cdname);
		if (n >= 0)
			n = ns_name_ntol(cdname, cdname, sizeof cdname);
		if (n >= 0) {
			/* Copy header. */
			memcpy(canonrr, cp1 - NS_HEADERDATA_SIZE, NS_HEADERDATA_SIZE);
			/* Copy expanded name. */
			memcpy(canonrr + NS_HEADERDATA_SIZE, cdname, n);
			canonrr_len = NS_HEADERDATA_SIZE + n;
		}

		startname = cp;
		cp = pr_cdname(cp, msg, name, sizeof name);
		if (doprint)
			fprintf(file, " %s", name);

		n = ns_name_unpack(msg, msg + 512, startname,
				   cdname, sizeof cdname);
		if (n >= 0)
			n = ns_name_ntol(cdname, cdname, sizeof cdname);
		if (n >= 0) {
			/* Copy expanded name. */
			memcpy(canonrr + canonrr_len, cdname, n);
			canonrr_len += n;
			/* Copy rest of SOA. */
			memcpy(canonrr + canonrr_len, cp, 5 * INT32SZ);
			canonrr_len += 5 * INT32SZ;
			/* Overwrite length field. */
			ns_put16(canonrr_len - NS_HEADERDATA_SIZE,
				 canonrr + NS_HEADERDATA_SIZE - NS_INT16SZ);
		}

		if (doprint)
			fprintf(file, " (\n\t\t\t%lu\t;serial (version)",
				ns_get32(cp));
		cp += INT32SZ;
		if (doprint)
			fprintf(file, "\n\t\t\t%lu\t;refresh period",
				ns_get32(cp));
		cp += INT32SZ;
		if (doprint)
			fprintf(file,
				"\n\t\t\t%lu\t;retry refresh this often",
				ns_get32(cp));
		cp += INT32SZ;
		if (doprint)
			fprintf(file, "\n\t\t\t%lu\t;expiration period",
				ns_get32(cp));
		cp += INT32SZ;
		if (doprint)
			fprintf(file, "\n\t\t\t%lu\t;minimum TTL\n\t\t\t)",
				ns_get32(cp));
		cp += INT32SZ;
		break;
	}
	case ns_t_mx:
	case ns_t_afsdb:
	case ns_t_rt:
	{
		const u_char *startrdata = cp;
		u_char cdname[NS_MAXCDNAME];

		if (doprint) {
			if (type == ns_t_mx && !verbose)
				fprintf(file," (pri=%d) by ", ns_get16(cp));
			else if (verbose)
				fprintf(file,"\t%d ", ns_get16(cp));
			else
				fprintf(file," ");
		}
		cp += sizeof(u_short);
		cp = pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
			fprintf(file, "%s", name);

		n = ns_name_unpack(msg, msg+512, startrdata + sizeof(u_short),
				   cdname, sizeof cdname);
		if (n >= 0)
			n = ns_name_ntol(cdname, cdname, sizeof cdname);
		if (n >= 0) {
			/* Copy header. */
			memcpy(canonrr, cp1 - NS_HEADERDATA_SIZE,
			       NS_HEADERDATA_SIZE);
			/* Overwrite length field. */
			ns_put16(sizeof(u_short) + n,
				 canonrr + NS_HEADERDATA_SIZE - NS_INT16SZ);
			/* Copy u_short. */
			memcpy(canonrr + NS_HEADERDATA_SIZE, startrdata,
			       sizeof(u_short));
			/* Copy expanded name. */
			memcpy(canonrr + NS_HEADERDATA_SIZE + sizeof(u_short),
			       cdname, n);
			canonrr_len = NS_HEADERDATA_SIZE + sizeof(u_short) + n;
		}
		break;
	}

	case ns_t_srv:
		if (doprint)
			fprintf(file," %d", ns_get16(cp));
		cp += sizeof(u_short);
		if (doprint)
			fprintf(file," %d", ns_get16(cp));
		cp += sizeof(u_short);
		if (doprint)
			fprintf(file," %d", ns_get16(cp));
		cp += sizeof(u_short);
		cp = pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
			fprintf(file, " %s", name);
		break;

	case ns_t_naptr:
		/* order */
		if (doprint)
			fprintf(file, " %d", ns_get16(cp));
		cp += sizeof(u_short);
		/* preference */
		if (doprint)
			fprintf(file, " %d", ns_get16(cp));
		cp += NS_INT16SZ;
		/* Flags */
		n = *cp++;
		if (doprint) {
			if (n)
				fprintf(file, "%c%.*s", punc, n, cp);
			else 
				fprintf(file, "%c\"\"",punc);
		}
		cp += n;
		/* Service */
		n = *cp++;
		if (doprint) {
			if (n)
				fprintf(file, "%c%.*s", punc, n, cp);
			else 
				fprintf(file,"%c\"\"",punc);
		}
		cp += n;
		/* Regexp  */
		n = *cp++;
		if (doprint) {
			if (n)
				fprintf(file, "%c%.*s", punc, n, cp);
			else 
				fprintf(file, "%c\"\"",punc);
		}
		cp += n;
		/* replacement  */
		cp = pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
			fprintf(file, "%s", name);
		break;

	case ns_t_minfo:
	case ns_t_rp:
		cp = pr_cdname(cp, msg, name, sizeof name);
		if (doprint) {
			if (type == ns_t_rp) {
				char *p;

				p = strchr(name, '.');
				if (p != NULL)
					*p = '@';
			}
			fprintf(file, "%c%s", punc, name);
		}
		cp = pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
			fprintf(file, " %s", name);
		break;

	case ns_t_x25:
		n = *cp++;
		if (n != 0) {
			if (doprint)
				fprintf(file, "%c%.*s", punc, n, cp);
			cp += n;
		}
		break;

	case ns_t_txt:
		{
			int n, j;
			const u_char *end = cp + dlen;
		 
			while (cp < end) {
				if (doprint)
					(void) fputs(" \"", file);
				n = *cp++;
				if (n != 0)
					for (j = n; j > 0 && cp < end ; j --) {
						if (doprint) {
							if (*cp == '\n' ||
							    *cp == '"' ||
							    *cp == '\\')
								putc('\\',
								     file);
							putc(*cp, file);
						}
						cp++;
					}
				if (doprint)
					putc('"', file);
			}
		}
		break;

	case ns_t_wks:
		if (dlen < INT32SZ + 1)
			break;
		memcpy(&inaddr, cp, INADDRSZ);
		cp += INT32SZ;
		proto = *cp++;
		protop = getprotobynumber(proto);
		if (doprint) {
			if (protop)
				fprintf(file, "%c%s %s", punc,
					inet_ntoa(inaddr), protop->p_name);
			else
				fprintf(file, "%c%s %d", punc,
					inet_ntoa(inaddr), proto);
		}
		n = 0;
		while (cp < cp1 + dlen) {
			c = *cp++;
			do {
 				if (c & 0200) {
					servp = NULL;
					if (protop)
						servp = getservbyport(htons(n),
								      protop->
								       p_name);
					if (doprint) {
						if (servp)
							fprintf(file, " %s",
								servp->s_name);
						else
							fprintf(file, " %d",
								n);
					}
				}
				c <<= 1;
			} while (++n & 07);
		}
		break;
	case ns_t_nxt:
	{
		const u_char *startrdata = cp;
		u_char cdname[NS_MAXCDNAME];
		size_t bitmaplen;

		cp = pr_cdname(cp, msg, name, sizeof name);
		if (doprint)
			fprintf(file, "%c%s", punc, name);
		bitmaplen = dlen - (cp - startrdata);

		/* extract dnssec canonical rr */

		n = ns_name_unpack(msg, msg+MY_PACKETSZ, startrdata,
				   cdname, sizeof cdname);
		if (n >= 0)
			n = ns_name_ntol(cdname, cdname, sizeof cdname);
		if (n >= 0) {
			/* Copy header. */
			memcpy(canonrr, cp1 - NS_HEADERDATA_SIZE,
			       NS_HEADERDATA_SIZE);
			/* Overwrite length field. */
			ns_put16(n + bitmaplen, 
				 canonrr + NS_HEADERDATA_SIZE - NS_INT16SZ);
			/* Copy expanded name. */
			memcpy(canonrr + NS_HEADERDATA_SIZE, cdname, n);
			/* Copy type bit map. */
			memcpy(canonrr + NS_HEADERDATA_SIZE + n, cp,
			       bitmaplen);
			canonrr_len = NS_HEADERDATA_SIZE + n + bitmaplen;
		}
		cp += bitmaplen;
		break;
	}
	case ns_t_sig:
	{
		int tc;
		const u_char *origttl;

		/* type covered */
		tc = ns_get16(cp);
		if (doprint && verbose)
			fprintf(file, "%c%s", punc, sym_ntos(__p_type_syms, tc, NULL));
		cp += sizeof(u_short);
		/* algorithm */
		if (doprint && verbose)
			fprintf(file, " %d", *cp);
		cp++;
		/* labels */
		if (doprint && verbose)
			fprintf(file, " %d", *cp);
		cp++;
		/* original ttl */
		origttl = cp;
		if (doprint && verbose)
			fprintf(file, " %lu", ns_get32(cp));
		cp += INT32SZ;
		/* signature expiration */
		if (doprint && verbose)
			fprintf(file, " %lu", ns_get32(cp));
		cp += INT32SZ;
		/* time signed */
		if (doprint && verbose)
			fprintf(file, " %lu", ns_get32(cp));
		cp += INT32SZ;
		/* key footprint */
		if (doprint && verbose)
			fprintf(file, " %d", ns_get16(cp));
		cp += sizeof(u_short);
		/* signer's name */
		cp = pr_cdname(cp, msg, name, sizeof(name));
		if (doprint && verbose)
			fprintf(file, " %s", name);
		else if (doprint && !verbose)
			fprintf (file, " %s for type %s", name, 
				 sym_ntos(__p_type_syms, tc, NULL));
		/* signature */
		{
			char str[MY_PACKETSZ];
			size_t len = cp1-cp+dlen;
			
			b64_ntop (cp, len, str, MY_PACKETSZ-1);

			if (sigchase && !(chase_step & SD_SIG) && 
			    strcmp (chase_domain, thisdomain) == 0 && 
			    chase_class == class && chase_type == tc)
			{
				u_char cdname[NS_MAXCDNAME];

				if (doprint && !verbose)
					fprintf(file, " (chasing key)");
				
				strcpy(chase_signer, name);

				memcpy(&chase_sigorigttl[0], origttl, 
				       NS_INT32SZ);

				n = ns_name_ntol(cp1 + SIG_RDATA_BY_NAME, 
						 cdname, sizeof cdname);
				if (n >= 0) {
					memcpy(chase_sigrdata, cp1,
					       SIG_RDATA_BY_NAME);
					memcpy(chase_sigrdata + SIG_RDATA_BY_NAME,
					       cdname, n);
					chase_sigrdata_len = SIG_RDATA_BY_NAME + n;
					memcpy(chase_signature, cp, len);
					chase_signature_len = len;

					chase_step |= SD_SIG;
				}
			} else if (sigchase) {
				chase_step |= SD_BADSIG;
			}

			cp += len;
			if (doprint && verbose)
				fprintf (file, " %s", str);
		}
		break;
	}
	case ns_t_key:
		/* flags */
		if (doprint && verbose)
			fprintf(file, "%c%d", punc, ns_get16(cp));
		cp += sizeof(u_short);
		/* protocol */
		if (doprint && verbose)
			fprintf(file, " %d", *cp);
		cp++;
		/* algorithm */
		n = *cp;
		if (doprint && verbose)
			fprintf(file, " %d", *cp);
		cp++;
		switch (n) {
		case 1: /* MD5/RSA */
		{
			char str[MY_PACKETSZ];
			size_t len = cp1-cp+dlen;

			b64_ntop (cp, len, str, MY_PACKETSZ-1);
			cp += len;
			
			if (doprint && verbose)
				fprintf (file, " %s", str);
			break;
		}
		
		default:
			if (doprint && verbose)
				fprintf (stderr, "Unknown algorithm %d\n", n);
			cp = cp1 + dlen;
			break;
		}
		
		if (sigchase && (chase_step & (SD_SIG|SD_RR)) && 
		    strcmp (getdomain, name) == 0 && 
		    getclass == class && gettype == type)
		{
			DST_KEY *dstkey;
			int rc, len, i, j;
			
			/* convert dnskey to dstkey */
		
			dstkey = dst_dnskey_to_key (name, cp1, dlen);
		
			/* fix ttl in rr */
			
			for (i = 0; i < NUMRR && chase_rr[i].len; i++)
			{
				len = dn_skipname(chase_rr[i].data, 
						  chase_rr[i].data + 
						  chase_rr[i].len);
				if (len>=0) 
					memcpy(chase_rr[i].data + len + NS_INT16SZ +
					       NS_INT16SZ,
					       &chase_sigorigttl, INT32SZ);
			}

			/* sort rr's (qsort() is too slow) */
			
			for (i = 0; i < NUMRR && chase_rr[i].len; i++)
				for (j = i + 1; j < NUMRR && chase_rr[j].len;
				     j++)
					if (memcmp(chase_rr[i].data,
						   chase_rr[j].data,
						   MY_PACKETSZ) < 0)
						memswap(&chase_rr[i],
						        &chase_rr[j],
							sizeof(rrstruct));
			/* append rr's to sigrdata */

			for (i = 0; i < NUMRR && chase_rr[i].len; i++)
			{
				memcpy (chase_sigrdata + chase_sigrdata_len, 
					chase_rr[i].data, chase_rr[i].len);
				chase_sigrdata_len += chase_rr[i].len;
			}

			/* print rr-data and signature */

			if (verbose) {
				fprintf(file, "\n");
				print_hex_field(chase_sigrdata,
						chase_sigrdata_len,
						21,"DATA: ");
				print_hex_field(chase_signature,
						chase_signature_len,
						21,"SIG: ");
			}
			
			/* do the works */

			if (dstkey)
				rc = dst_verify_data(SIG_MODE_ALL, dstkey, NULL,
						     chase_sigrdata, 
						     chase_sigrdata_len,
						     chase_signature, 
						     chase_signature_len);
			else
				rc = 1;

			dst_free_key(dstkey);

			if (verbose)
			{
				fprintf(file, "\nVerification %s", rc == 0 ? 
					"was SUCCESSFULL" :
					"FAILED");
			}
			else
			{
				fprintf (file, 
					 " that %s verify our %s "
					 "record(s) on %s", 
					 rc == 0 ? "successfully" :
					 "DOES NOT", 
					 sym_ntos(__p_type_syms, chase_type, 
						  NULL),
					 chase_domain);
			}

			if (rc == 0)
			{
				strcpy (chase_lastgoodkey, name);
			}
			else
			{
				/* don't trace further after a failure */
				sigchase = 0; 
			}

			chase_step = 0;
			chase_signature_len = 0;
			chase_sigrdata_len = 0;
			memset(chase_sigorigttl, 0, NS_INT32SZ);
			memset(chase_rr, 0, sizeof(chase_rr));
			chase_rr_num = 0;
		}
		break;

	default:
		if (doprint)
			fprintf(file, "%c???", punc);
		cp += dlen;
		break;
	}
	if (cp != cp1 + dlen)
		fprintf(file, "packet size error (%p != %p)\n",
			cp, cp1 + dlen);

	if (sigchase && !(chase_step & SD_SIG) && 
	    strcmp (getdomain, thisdomain) == 0 && getclass == class && 
	    gettype == type && type != ns_t_sig)
	{
		u_char cdname[NS_MAXCDNAME];

		if (doprint && !verbose)
			fprintf(file, " (chasing signature)");

		/* unpack rr */

		n = ns_name_unpack(msg, msg + MY_PACKETSZ, savecp,
				   cdname, sizeof cdname);
		if (n >= 0)
			n = ns_name_ntol(cdname, cdname, sizeof cdname);
		if (n >= 0) {
			memcpy(chase_rr[chase_rr_num].data, cdname, n);
			memcpy(chase_rr[chase_rr_num].data + n,
			       canonrr_len ? canonrr : cp1 - NS_HEADERDATA_SIZE,
			       canonrr_len ? canonrr_len : dlen + NS_HEADERDATA_SIZE);
			chase_rr[chase_rr_num].len = 
				n + (canonrr_len != 0 ? canonrr_len : 
				     dlen + NS_HEADERDATA_SIZE);
			
			strcpy(chase_domain, getdomain);
			chase_class = class;
			chase_type = type;
			chase_step |= SD_RR;
			chase_rr_num++;
		}
	}

	if (doprint)
		fprintf(file, "\n");

	return (cp);
}

/*
 * Return a string for the type.  A few get special treatment when
 * not in verbose mode, to make the program more chatty and easier to
 * understand.
 */
static const char *
pr_type(int type) {
	if (!verbose) switch (type) {
	case ns_t_a:
	case ns_t_aaaa:
		return ("has address");
	case ns_t_cname:
		return ("is a nickname for");
	case ns_t_mx:
		return ("mail is handled");
	case ns_t_txt:
		return ("descriptive text");
	case ns_t_sig:
                return ("has a signature signed by");
	case ns_t_key:
                return ("has a key");
	case ns_t_nxt:
		return ("next valid name");
	case ns_t_afsdb:
		return ("DCE or AFS service from");
	}
	if (verbose)
		return (sym_ntos(__p_type_syms, type, NULL));
	else
		return (sym_ntop(__p_type_syms, type, NULL));
}

/*
 * Return a mnemonic for class
 */
static const char *
pr_class(int class) {
	static char spacestr[20];

	if (!verbose) switch (class) {
	case ns_c_in:		/* internet class */
		return ("");
	case ns_c_hs:		/* hesiod class */
		return ("");
	}

	spacestr[0] = ' ';
	strcpy(&spacestr[1], p_class(class));
	return (spacestr);
}

static const u_char *
pr_cdname(const u_char *cp, const u_char *msg, char *name, int namelen) {
	int n = dn_expand(msg, msg + MY_PACKETSZ, cp, name, namelen - 2);

	if (n < 0)
		return (NULL);
	if (name[0] == '\0') {
		name[0] = '.';
		name[1] = '\0';
	}
	return (cp + n);
}

static void
add(union res_sockaddr_union *u, int type, void *p) {
	memset(u, 0, sizeof(*u));
	switch (type) {
	case ns_t_a:
		memcpy(&u->sin.sin_addr, p, NS_INADDRSZ);
		u->sin.sin_family = AF_INET;
		u->sin.sin_port = htons(NAMESERVER_PORT);
#ifdef HAVE_SA_LEN
		u->sin.sin_len = sizeof(u->sin);
#endif
		break;

	case ns_t_aaaa:
		memcpy(&u->sin6.sin6_addr, p, 16);
		u->sin6.sin6_family = AF_INET6;
		u->sin6.sin6_port = htons(NAMESERVER_PORT);
#ifdef HAVE_SA_LEN
		u->sin6.sin6_len = sizeof(u->sin6);
#endif
		break;
	}
}

static int
salen(union res_sockaddr_union *u) {
	switch (u->sin.sin_family) {
	case AF_INET6: return (sizeof(u->sin6));
	case AF_INET: return (sizeof(u->sin));
	}
	return (0);
}

static int
ListHosts(char *namePtr, int queryType) {
	querybuf buf, answer;
	struct sockaddr_in sin;
	const HEADER *headerPtr;
	enum { NO_ERRORS, ERR_READING_LEN, ERR_READING_MSG, ERR_PRINTING }
		error = NO_ERRORS;

	int msglen, amtToRead, numRead, i, len, dlen, type, nscount, n;
	int numAnswers = 0, soacnt = 0, result = 0;
	u_char tmp[NS_INT16SZ];
	char name[NS_MAXDNAME], dname[2][NS_MAXDNAME], domain[NS_MAXDNAME];
	u_char *cp, *nmp, *eom;

	/* Names and addresses of name servers to try. */
	char nsname[NUMNS][NS_MAXDNAME];
	int nshaveaddr[NUMNS];
	union res_sockaddr_union nsipaddr[NUMNSADDR];
	int numns, numnsaddr, thisns;
	int qdcount, ancount;

	/*
	 * Normalize to not have trailing dot.  We do string compares below
	 * of info from name server, and it won't have trailing dots.
	 */
	i = strlen(namePtr);
	if (namePtr[i-1] == '.')
		namePtr[i-1] = 0;

	if (server_specified)
		numnsaddr = res_getservers(&res, nsipaddr, NUMNSADDR);
	else {
		/*
		 * First we have to find out where to look.  This needs a NS
		 * query, possibly followed by looking up addresses for some
		 * of the names.
		 */
		msglen = res_nmkquery(&res, ns_o_query, namePtr,
				      ns_c_in, ns_t_ns,
				      NULL, 0, NULL,
				      buf.qb2, sizeof buf);
		if (msglen < 0) {
			printf("res_nmkquery failed\n");
			return (ERROR);
		}

		msglen = res_nsend(&res, buf.qb2, msglen,
				   answer.qb2, sizeof answer);
		if (msglen < 0) {
			printf("Cannot find nameserver -- try again later\n");
			return (ERROR);
		}
		if (res.options & RES_DEBUG || verbose)
			printf("rcode = %d (%s), ancount=%d\n", 
			       answer.qb1.rcode, DecodeError(answer.qb1.rcode),
			       ntohs(answer.qb1.ancount));

		/*
		 * Analyze response to our NS lookup.
		 */

		nscount = ntohs(answer.qb1.ancount) +
			  ntohs(answer.qb1.nscount) +
			  ntohs(answer.qb1.arcount);

		if (answer.qb1.rcode != NOERROR || nscount == 0) {
			switch (answer.qb1.rcode) {
			case NXDOMAIN:
				/* Check if it's an authoritive answer */
				if (answer.qb1.aa)
					printf("No such domain\n");
				else
					printf("Unable to get information about domain -- try again later.\n");
				break;
			case SERVFAIL:
				printf("Unable to get information about that domain -- try again later.\n");
				break;
			case NOERROR:
				printf("That domain exists, but seems to be a leaf node.\n");
				break;
			case FORMERR:
			case NOTIMP:
			case REFUSED:
				printf("Unrecoverable error looking up domain name.\n");
				break;
			}
			return (0);
		}

		cp = answer.qb2 + HFIXEDSZ;
		eom = answer.qb2 + msglen;
		qdcount = ntohs(answer.qb1.qdcount);
		while (qdcount-- > 0) {
			n = dn_skipname(cp, eom);
			if (n < 0) {
				printf("Form error.\n");
				return (ERROR);
			}
			cp += n + QFIXEDSZ;
			if (cp > eom) {
				printf("Form error.\n");
				return (ERROR);
			}
		}

		numns = 0;
		numnsaddr = 0;

		/*
		 * Look at response from NS lookup for NS and A records.
		 */

		for ((void)NULL; nscount; nscount--) {
			cp += dn_expand(answer.qb2, answer.qb2 + msglen, cp,
					domain, sizeof(domain));
			if (cp + 3 * INT16SZ + INT32SZ > eom) {
				printf("Form error.\n");
				return (ERROR);
			}
			type = ns_get16(cp);
			cp += INT16SZ + INT16SZ + INT32SZ;
			dlen = ns_get16(cp);
			cp += INT16SZ;
			if (cp + dlen > eom) {
				printf("Form error.\n");
				return (ERROR);
			}
			if (type == ns_t_ns) {
				if (dn_expand(answer.qb2, eom,
					      cp, name, sizeof(name)) >= 0) {
					if (numns < NUMNS &&
					    ns_samename((char *)domain, 
							namePtr) == 1) {
						for (i = 0; i < numns; i++)
							if (ns_samename(
								     nsname[i],
								   (char *)name
								       ) == 1)
								/* duplicate */
								break;
						if (i >= numns) {
							strncpy(nsname[numns],
								(char *)name,
								sizeof(name));
							nshaveaddr[numns] = 0;
							numns++;
						}
					}
				}
			} else if ((type == ns_t_a || type == ns_t_aaaa) &&
				   numnsaddr < NUMNSADDR) {
				for (i = 0; i < numns; i++) {
					if (ns_samename(nsname[i],
							       (char *)domain)
					    != 1)
						continue;
					nshaveaddr[i]++;
					add(&nsipaddr[numnsaddr++], type, cp);
					break;
				}
			}
			cp += dlen;
		}

		/*
		 * Usually we'll get addresses for all the servers in the
		 * additional info section.  But in case we don't, look up
		 * their addresses.
		 */

		for (i = 0; i < numns; i++) {
			struct addrinfo *answer = NULL;
			struct addrinfo *cur = NULL;
			struct addrinfo hint;

			memset(&hint, 0, sizeof(hint));
			hint.ai_family = PF_UNSPEC;
			hint.ai_socktype = SOCK_STREAM;

			if (nshaveaddr[i] == 0 &&
			    !getaddrinfo(nsname[i], NULL, &hint, &answer)) {
				int numaddrs = 0;
				for (cur = answer;
				     cur != NULL;
				     cur = cur->ai_next) {
					union res_sockaddr_union *u;

					if (numnsaddr >= NUMNSADDR)
						break;

					u = &nsipaddr[numnsaddr];
					switch (cur->ai_addr->sa_family) {
					case AF_INET6:
						u->sin6 =
					   *(struct sockaddr_in6 *)cur->ai_addr;
						u->sin6.sin6_port =
							htons(NAMESERVER_PORT);
						numnsaddr++;
						numaddrs++;
						break;
					case AF_INET:
						u->sin =
					    *(struct sockaddr_in*)cur->ai_addr;
						u->sin6.sin6_port =
							htons(NAMESERVER_PORT);
						numnsaddr++;
						numaddrs++;
						break;
					}
				}
				if (res.options & RES_DEBUG || verbose)
					printf(
				  "Found %d addresses for %s by extra query\n",
					       numaddrs, nsname[i]);
				freeaddrinfo(answer);
			} else if (res.options & RES_DEBUG || verbose)
				printf("Found %d addresses for %s\n",
				       nshaveaddr[i], nsname[i]);
		}
	}
	/*
	 * Now nsipaddr has numnsaddr addresses for name servers that
	 * serve the requested domain.  Now try to find one that will
	 * accept a zone transfer.
	 */
	thisns = 0;

 again:
	numAnswers = 0;
	soacnt = 0;

	/*
	 * Create a query packet for the requested domain name.
	 */
	msglen = res_nmkquery(&res, QUERY, namePtr, getclass, ns_t_axfr, NULL,
			      0, NULL, buf.qb2, sizeof buf);
	if (msglen < 0) {
		if (res.options & RES_DEBUG)
			fprintf(stderr, "ListHosts: Res_mkquery failed\n");
		return (ERROR);
	}

	memset(&sin, 0, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_port =  htons(NAMESERVER_PORT);

	/*
	 * Set up a virtual circuit to the server.
	 */

	for ((void)NULL; thisns < numnsaddr; thisns++) {
		if ((sockFD = socket(nsipaddr[thisns].sin.sin_family,
				     SOCK_STREAM, 0)) < 0) {
			if (errno == EPROTONOSUPPORT)
				continue;
			perror("ListHosts");
			return (ERROR);
		}
		if (res.options & RES_DEBUG || verbose) {
			char buf[80];
			switch (nsipaddr[thisns].sin.sin_family) {
			case AF_INET:
				inet_ntop(nsipaddr[thisns].sin.sin_family,
					  &nsipaddr[thisns].sin.sin_addr,
					  buf, sizeof(buf));
				break;
			case AF_INET6:
				inet_ntop(nsipaddr[thisns].sin6.sin6_family,
					  &nsipaddr[thisns].sin6.sin6_addr,
					  buf, sizeof(buf));
				break;
			}
			printf("Trying %s\n", buf);
		}
		if (connect(sockFD, (struct sockaddr *)&nsipaddr[thisns],
		            salen(&nsipaddr[thisns])) >= 0)
			break;
		if (verbose)
			perror("Connection failed, trying next server");
		close(sockFD);
		sockFD = -1;
	}	
	if (thisns >= numnsaddr) {
		printf("No server for that domain responded\n");
		if (!verbose)
			perror("Error from the last server was");
		return (ERROR);
	}

	/*
	 * Send length & message for zone transfer 
	 */

	ns_put16(msglen, tmp);
        if (write(sockFD, (char *)tmp, INT16SZ) != INT16SZ ||
	    write(sockFD, (char *)buf.qb2, msglen) != msglen) {
		perror("ListHosts");
		(void) close(sockFD);
		sockFD = -1;
		return (ERROR);
	}

	filePtr = stdout;

	for (;;) {
		/*
		 * Read the length of the response.
		 */
		cp = buf.qb2;
		amtToRead = INT16SZ;
		while (amtToRead > 0 &&
		       (numRead = read(sockFD, cp, amtToRead)) > 0) {
			cp += numRead;
			amtToRead -= numRead;
		}
		if (numRead <= 0) {
			error = ERR_READING_LEN;
			break;
		}	

		if ((len = ns_get16(buf.qb2)) == 0)
			break;	/* Protocol violation. */

		/*
		 * Read the response.
		 */

		amtToRead = len;
		cp = buf.qb2;
		while (amtToRead > 0 &&
		       (numRead = read(sockFD, cp, amtToRead)) > 0) {
			cp += numRead;
			amtToRead -= numRead;
		}
		if (numRead <= 0) {
			error = ERR_READING_MSG;
			break;
		}

		i = buf.qb1.rcode;
		if (i != NOERROR || ntohs(buf.qb1.ancount) == 0) {
			if (thisns + 1 < numnsaddr &&
			    (i == SERVFAIL || i == NOTIMP || i == REFUSED)) {
				if (res.options & RES_DEBUG || verbose)
					printf(
				     "Server failed, trying next server: %s\n",
					       i != NOERROR
					       ? DecodeError(i)
					       : "Premature end of data");
				(void) close(sockFD);
				sockFD = -1;
				thisns++;
				goto again;
			}
			printf("Server failed: %s\n", i != NOERROR
			       ? DecodeError(i) : "Premature end of data");
			break;
		}

		result = printinfo(&buf, cp, queryType, 1, 0);
		if (! result) {
			error = ERR_PRINTING;
			break;
		}
		numAnswers++;
		cp = buf.qb2 + HFIXEDSZ;
		qdcount = ntohs(buf.qb1.qdcount);
		while (qdcount-- > 0) {
			n = dn_skipname(cp, buf.qb2 + len);
			if (n <= 0) {
				error = ERR_PRINTING;
				break;
			}
			if (cp + n + QFIXEDSZ > buf.qb2 + len) {
				error = ERR_PRINTING;
				break;
			}
			cp += n + QFIXEDSZ;
		}
		ancount = ntohs(buf.qb1.ancount);
		while (ancount-- > 0) {
			nmp = cp;
			n = dn_skipname(cp, buf.qb2 + len);
			if (n <= 0) {
				error = ERR_PRINTING;
				break;
			}
			cp += n;
			if (cp + INT16SZ > buf.qb2 + len) {
				error = ERR_PRINTING;
				break;
			}
			type = ns_get16(cp);
			cp += INT16SZ;
			if (type == ns_t_soa) {
				(void) dn_expand(buf.qb2, buf.qb2 + len, nmp,
						 dname[soacnt], sizeof dname[0]);
				if (soacnt) {
					if (ns_samename(dname[0], dname[1]) == 1)
						goto done;
				} else
					soacnt++;
			}
			if (cp + INT16SZ*2 + INT32SZ > buf.qb2 + len) {
				error = ERR_PRINTING;
				break;
			}
			cp += INT32SZ + INT16SZ;
			dlen = ns_get16(cp);
			cp += INT16SZ;
			if (cp + dlen > buf.qb2 + len) {
				error = ERR_PRINTING;
				break;
			}
			cp += dlen;
		}
		if (error != NO_ERRORS)
			break;
        }
 done:

	(void) close(sockFD);
	sockFD = -1;

	switch (error) {
	case NO_ERRORS:
		return (SUCCESS);

	case ERR_READING_LEN:
		return (ERROR);

	case ERR_PRINTING:
		fprintf(stderr,"*** Error during listing of %s: %s\n", 
				namePtr, DecodeError(result));
		return (result);

	case ERR_READING_MSG:
		headerPtr = (HEADER *) &buf;
		fprintf(stderr,"ListHosts: error receiving zone transfer:\n");
		fprintf(stderr,
	       "  result: %s, answers = %d, authority = %d, additional = %d\n",
			p_rcode(headerPtr->rcode), 
		    	ntohs(headerPtr->ancount), ntohs(headerPtr->nscount), 
			ntohs(headerPtr->arcount));
		return (ERROR);
	default:
		return (ERROR);
	}
}

static const char *
DecodeError(int result) {
	switch(result) {
	case NOERROR: 	return ("Success");
	case FORMERR:	return ("Format error");
	case SERVFAIL:	return ("Server failed");
	case NXDOMAIN:	return ("Non-existent domain");
	case NOTIMP:	return ("Not implemented");
	case REFUSED:	return ("Query refused");
	case NO_INFO: 	return ("No information");
	case ERROR: 	return ("Unspecified error");
	case TIME_OUT: 	return ("Timed out");
	case NONAUTH: 	return ("Non-authoritative answer");
	default:	return ("BAD ERROR VALUE");
	}
	/* NOTREACHED */
}
