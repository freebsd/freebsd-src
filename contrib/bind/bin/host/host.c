#ifndef lint
static char rcsid[] = "$Id: host.c,v 8.21 1998/03/19 19:31:25 halley Exp $";
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
 * Portions Copyright (c) 1996 by Internet Software Consortium
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
 Portions Copyright (c) 1996 Internet Software Consortium.\n\
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
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "port_after.h"

extern int h_errno;
extern char *_res_resultcodes[];

/* Global. */

#define NUMNS		8
#define NUMNSADDR	16
#define NUMMX		50

#define SUCCESS		0
#define TIME_OUT	-1
#define NO_INFO 	-2
#define ERROR 		-3
#define NONAUTH 	-4

typedef union {
	HEADER	qb1;
	u_char	qb2[NS_PACKETSZ];
} querybuf;

static char		cnamebuf[NS_MAXDNAME];
static u_char		hostbuf[NS_MAXDNAME];

static int		sockFD;
static FILE		*filePtr;

static struct __res_state  orig;
static char		*cname = NULL;
static int		getclass = ns_c_in, verbose = 0, list = 0;
static int		server_specified = 0;
static int		gettype;

/* Forward. */

static int		parsetype(const char *s);
static int		parseclass(const char *s);
static void		printanswer(const struct hostent *hp);
static void		hperror(int errnum);
static int		getaddrinfo(struct in_addr addr);
static int		gethostinfo(char *name);
static int		getdomaininfo(const char *name, const char *domain);
static int		getinfo(const char *name, const char *domain,
				int type);
static int		printinfo(const querybuf *answer, const u_char *eom,
				  int filter, int isls);
static const u_char *	pr_rr(const u_char *cp, const u_char *msg, FILE *file,
			      int filter);
static const char *	pr_type(int type);
static const char *	pr_class(int class);
static const u_char *	pr_cdname(const u_char *cp, const u_char *msg,
				  char *name, int namelen);
static int		ListHosts(char *namePtr, int queryType);
static const char *	DecodeError(int result);

/* Public. */

int
main(int c, char **v) {
	struct in_addr addr;
	struct hostent *hp;
	char *s, *oldcname;
	int inverse = 0, waitmode = 0;
	int ncnames;

	res_init();
	_res.retrans = 5;

	if (c < 2) {
		fprintf(stderr, "Usage: host [-w] [-v] [-r] [-d] [-t querytype] [-c class] [-a] host [server]\n  -w to wait forever until reply\n  -v for verbose output\n  -r to disable recursive processing\n  -d to turn on debugging output\n  -t querytype to look for a specific type of information\n  -c class to look for non-Internet data\n  -a is equivalent to '-v -t *'\n");
		exit(1);
	}
	while (c > 2 && v[1][0] == '-') {
		if (strcmp (v[1], "-w") == 0) {
			_res.retry = 1;
			_res.retrans = 15;
			waitmode = 1;
			v++;
			c--;
		}
		else if (strcmp (v[1], "-r") == 0) {
			_res.options &= ~RES_RECURSE;
			v++;
			c--;
		}
		else if (strcmp (v[1], "-d") == 0) {
			_res.options |= RES_DEBUG;
			v++;
			c--;
		}
		else if (strcmp (v[1], "-v") == 0) {
			verbose = 1;
			v++;
			c--;
		}
		else if (strcmp (v[1], "-l") == 0) {
			list = 1;
			v++;
			c--;
		}
		else if (strncmp (v[1], "-t", 2) == 0) {
			v++;
			c--;
			gettype = parsetype(v[1]);
			v++;
			c--;
		}
		else if (strncmp (v[1], "-c", 2) == 0) {
			v++;
			c--;
			getclass = parseclass(v[1]);
			v++;
			c--;
		}
		else if (strcmp (v[1], "-a") == 0) {
			verbose = 1;
			gettype = ns_t_any;
			v++;
			c--;
		}		
        }
	if (c > 2) {
		s = v[2];
		server_specified++;
		
		if (!inet_aton(s, &addr)) {
			hp = gethostbyname(s);
			if (hp == NULL) {
				fprintf(stderr,
					"Error in looking up server name:\n");
				hperror(h_errno);
				exit(1);
			}
			memcpy(&_res.nsaddr.sin_addr, hp->h_addr, NS_INADDRSZ);
			printf("Using domain server:\n");
			printanswer(hp);
		} else {
			_res.nsaddr.sin_family = AF_INET;
			_res.nsaddr.sin_addr = addr;
			_res.nsaddr.sin_port = htons(NAMESERVER_PORT);
			printf("Using domain server %s:\n",
			       inet_ntoa(_res.nsaddr.sin_addr));
		}
		_res.nscount = 1;
		_res.retry = 2;
	}
	if (strcmp(v[1], ".") == 0 || !inet_aton(v[1], &addr))
		addr.s_addr = INADDR_NONE;
	hp = NULL;
	h_errno = TRY_AGAIN;
/*
 * We handle default domains ourselves, thank you.
 */
	_res.options &= ~RES_DEFNAMES;

        if (list)
		exit(ListHosts(v[1], gettype ? gettype : ns_t_a));
	oldcname = NULL;
	ncnames = 5;
	while (hp == NULL && h_errno == TRY_AGAIN) {
		if (addr.s_addr == INADDR_NONE) {
			cname = NULL;
			if (oldcname == NULL)
				hp = (struct hostent *)gethostinfo(v[1]);
			else
				hp = (struct hostent *)gethostinfo(oldcname);
			if (cname) {
				if (ncnames-- == 0) {
					printf("Too many cnames.  Loop?\n");
					exit(1);
				}
				strcat(cname, ".");
				oldcname = cname;
				hp = NULL;
				h_errno = TRY_AGAIN;
				continue;
			}
		} else {
			if (getaddrinfo(addr) == 0)
				hp = NULL;
			else
				hp = (struct hostent *)1;	/* XXX */
		}
		if (!waitmode)
			break;
	}

	if (hp == NULL) {
		hperror(h_errno);
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
}

static void
printanswer(const struct hostent *hp) {
	struct in_addr **hptr;
	char **cp;

	printf("Name: %s\n", hp->h_name);
	printf("Address:");
	for (hptr = (struct in_addr **)hp->h_addr_list; *hptr; hptr++)
		printf(" %s", inet_ntoa(**hptr));
	printf("\nAliases:");
	for (cp = hp->h_aliases; cp && *cp && **cp; cp++)
		printf(" %s", *cp);
	printf("\n\n");
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
getaddrinfo(struct in_addr addr) {
	u_int32_t ha = ntohl(addr.s_addr);
	char name[NS_MAXDNAME];

	sprintf(name, "%u.%u.%u.%u.IN-ADDR.ARPA.",
		(ha) & 0xff,
		(ha >> 8) & 0xff,
		(ha >> 16) & 0xff,
		(ha >> 24) & 0xff);
	return (getinfo(name, NULL, ns_t_ptr));
}

static int
gethostinfo(char *name) {
	char *cp, **domain;
	const char *tp;
	int hp, nDomain;
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
	if (n == 0 && (tp = hostalias(name))) {
	        if (verbose)
		    printf("Aliased to \"%s\"\n", tp);
		_res.options |= RES_DEFNAMES;	  
		return (getdomaininfo(tp, (char *)NULL));
	}
	if (n >= _res.ndots) {
		asis = 1;
		if (verbose)
		    printf("Trying null domain\n");
		hp = getdomaininfo(name, (char*)NULL);
		if (hp)
			return (hp);
	}
	for (domain = _res.dnsrch; *domain; domain++) {
		if (verbose)
			printf("Trying domain \"%s\"\n", *domain);
		hp = getdomaininfo(name, *domain);
		if (hp)
			return (hp);
	}
	if (h_errno != HOST_NOT_FOUND || (_res.options & RES_DNSRCH) == 0)
		return (0);
	if (!asis)
		return (0);
	if (verbose)
		printf("Trying null domain\n");
	return (getdomaininfo(name, (char *)NULL));
}

static int
getdomaininfo(const char *name, const char *domain) {
	int val1, val2;

	if (gettype)
		return (getinfo(name, domain, gettype));
	else {
		val1 = getinfo(name, domain, ns_t_a);
		if (cname || verbose)
			return (val1);
		val2 = getinfo(name, domain, ns_t_mx);
		return (val1 || val2);
	}
}

static int
getinfo(const char *name, const char *domain, int type) {
	HEADER *hp;
	u_char *eom, *bp, *cp;
	querybuf buf, answer;
	int n, n1, i, j, nmx, ancount, nscount, arcount, qdcount, buflen;
	u_short pref, class;
	char host[NS_MAXDNAME];

	if (domain == NULL)
		sprintf(host, "%.*s", NS_MAXDNAME, name);
	else
		sprintf(host, "%.*s.%.*s",
			NS_MAXDNAME, name, NS_MAXDNAME, domain);

	n = res_mkquery(QUERY, host, getclass, type, NULL, 0, NULL,
			buf.qb2, sizeof buf);
	if (n < 0) {
		if (_res.options & RES_DEBUG)
			printf("res_mkquery failed\n");
		h_errno = NO_RECOVERY;
		return (0);
	}
	n = res_send(buf.qb2, n, answer.qb2, sizeof answer);
	if (n < 0) {
		if (_res.options & RES_DEBUG)
			printf("res_send failed\n");
		h_errno = TRY_AGAIN;
		return (0);
	}
	eom = answer.qb2 + n;
	return (printinfo(&answer, eom, ns_t_any, 0));
}

static int
printinfo(const querybuf *answer, const u_char *eom, int filter, int isls) {
	int n, n1, i, j, nmx, ancount, nscount, arcount, qdcount, buflen;
	u_short pref, class;
	const u_char *bp, *cp;
	const HEADER *hp;

	/*
	 * Find first satisfactory answer.
	 */
	hp = (HEADER *) answer;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	nscount = ntohs(hp->nscount);
	arcount = ntohs(hp->arcount);
	if (_res.options & RES_DEBUG || (verbose && isls == 0))
		printf("rcode = %d (%s), ancount=%d\n", 
		       hp->rcode, DecodeError(hp->rcode), ancount);
	if (hp->rcode != NOERROR || (ancount+nscount+arcount) == 0) {
		switch (hp->rcode) {
		case NXDOMAIN:
			h_errno = HOST_NOT_FOUND;
			return (0);
		case SERVFAIL:
			h_errno = TRY_AGAIN;
			return (0);
			case NOERROR:
				h_errno = NO_DATA;
				return (0);
			case FORMERR:
			case NOTIMP:
			case REFUSED:
				h_errno = NO_RECOVERY;
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
		while (--ancount >= 0 && cp && cp < eom) {
			cp = pr_rr(cp, answer->qb2, stdout, filter);
			/*
			 * When we ask for address and there is a CNAME, it
			 * seems to return both the CNAME and the address.
			 * Since we trace down the CNAME chain ourselves, we
			 * don't really want to print the address at this
			 * point.
			 */
			if (cname && ! verbose)
				return (1);
		}
	}
	if (!verbose)
		return (1);
	if (nscount) {
		printf("For authoritative answers, see:\n");
		while (--nscount >= 0 && cp && cp < eom)
			cp = (u_char *)pr_rr(cp, answer->qb2, stdout, filter);
	}
	if (arcount) {
		printf("Additional information:\n");
		while (--arcount >= 0 && cp && cp < eom)
			cp = (u_char *)pr_rr(cp, answer->qb2, stdout, filter);
	}
	return (1);
}

/*
 * Print resource record fields in human readable form.
 */
static const u_char *
pr_rr(const u_char *cp, const u_char *msg, FILE *file, int filter) {
	int type, class, dlen, n, c, proto, ttl;
	struct in_addr inaddr;
	u_char in6addr[NS_IN6ADDRSZ];
	const u_char *cp1;
	struct protoent *protop;
	struct servent *servp;
	char punc = ' ';
	int doprint;
	char name[NS_MAXDNAME];
	char tmpbuf[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];

	if ((cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name))) == NULL)
		return (NULL);			/* compression error */

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
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
			fprintf(file,"%c%s",punc, name);
		break;

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
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
			fprintf(file, "\t%s", name);
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
			fprintf(file, " %s", name);
		if (doprint)
			fprintf(file, "(\n\t\t\t%ld\t;serial (version)",
				ns_get32(cp));
		cp += INT32SZ;
		if (doprint)
			fprintf(file, "\n\t\t\t%ld\t;refresh period",
				ns_get32(cp));
		cp += INT32SZ;
		if (doprint)
			fprintf(file,
				"\n\t\t\t%ld\t;retry refresh this often",
				ns_get32(cp));
		cp += INT32SZ;
		if (doprint)
			fprintf(file, "\n\t\t\t%ld\t;expiration period",
				ns_get32(cp));
		cp += INT32SZ;
		if (doprint)
			fprintf(file, "\n\t\t\t%ld\t;minimum TTL\n\t\t\t)",
				ns_get32(cp));
		cp += INT32SZ;
		break;

	case ns_t_mx:
	case ns_t_afsdb:
	case ns_t_rt:
		if (doprint) {
			if (type == ns_t_mx && !verbose)
				fprintf(file," (pri=%d) by ", ns_get16(cp));
			else if (verbose)
				fprintf(file,"\t%d ", ns_get16(cp));
			else
				fprintf(file," ");
		}
		cp += sizeof(u_short);
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
			fprintf(file, "%s", name);
		break;

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
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
			fprintf(file,"%s",name);
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
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
			fprintf(file, "%s", name);
		break;

	case ns_t_minfo:
	case ns_t_rp:
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof name);
		if (doprint) {
			if (type == ns_t_rp) {
				char *p;

				p = strchr(name, '.');
				if (p != NULL)
					*p = '@';
			}
			fprintf(file, "%c%s", punc, name);
		}
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
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

	default:
		if (doprint)
			fprintf(file, "%c???", punc);
		cp += dlen;
		break;
	}
	if (cp != cp1 + dlen)
		fprintf(file, "packet size error (%p != %p)\n",
			cp, cp1 + dlen);
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
		return ("has address");
	case ns_t_cname:
		return ("is a nickname for");
	case ns_t_mx:
		return ("mail is handled");
	case ns_t_txt:
		return ("descriptive text");
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
	int n = dn_expand(msg, msg + 512, cp, name, namelen - 2);

	if (n < 0)
		return (NULL);
	if (name[0] == '\0') {
		name[0] = '.';
		name[1] = '\0';
	}
	return (cp + n);
}

static int
ListHosts(char *namePtr, int queryType) {
	querybuf buf, answer;
	struct sockaddr_in sin;
	const HEADER *headerPtr;
	const struct hostent *hp;
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
	struct in_addr nsipaddr[NUMNSADDR];
	int numns, numnsaddr, thisns;

	/*
	 * Normalize to not have trailing dot.  We do string compares below
	 * of info from name server, and it won't have trailing dots.
	 */
	i = strlen(namePtr);
	if (namePtr[i-1] == '.')
		namePtr[i-1] = 0;

	if (server_specified) {
		memcpy(&nsipaddr[0], &_res.nsaddr.sin_addr, NS_INADDRSZ);
		numnsaddr = 1;
	} else {
		/*
		 * First we have to find out where to look.  This needs a NS
		 * query, possibly followed by looking up addresses for some
		 * of the names.
		 */
		msglen = res_mkquery(ns_o_query, namePtr, ns_c_in, ns_t_ns,
				     NULL, 0, NULL, buf.qb2, sizeof buf);
		if (msglen < 0) {
			printf("res_mkquery failed\n");
			return (ERROR);
		}

		msglen = res_send(buf.qb2, msglen, answer.qb2, sizeof answer);
		if (msglen < 0) {
			printf("Cannot find nameserver -- try again later\n");
			return (ERROR);
		}
		if (_res.options & RES_DEBUG || verbose)
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
		if (ntohs(answer.qb1.qdcount) > 0) {
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
					    strcasecmp((char *)domain, 
						       namePtr) == 0) {
						for (i = 0; i < numns; i++)
							if (strcasecmp(
								     nsname[i],
								   (char *)name
								       ) == 0)
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
			} else if (type == ns_t_a) {
				if (numnsaddr < NUMNSADDR)
					for (i = 0; i < numns; i++) {
						if (strcasecmp(nsname[i],
							       (char *)domain)
						    == 0) {
							nshaveaddr[i]++;
							memcpy(
							  &nsipaddr[numnsaddr],
							  cp, NS_INADDRSZ);
							numnsaddr++;
							break;
						}
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
			if (nshaveaddr[i] == 0) {
				struct in_addr **hptr;
				int numaddrs = 0;

				hp = gethostbyname(nsname[i]);
				if (hp) {
					for (hptr = (struct in_addr **)
					     	hp->h_addr_list;
					     *hptr != NULL;
					     hptr++)
						if (numnsaddr < NUMNSADDR) {
							memcpy(
							  &nsipaddr[numnsaddr],
							  *hptr, NS_INADDRSZ);
							numnsaddr++;
							numaddrs++;
						}
				}
				if (_res.options & RES_DEBUG || verbose)
					printf(
				  "Found %d addresses for %s by extra query\n",
					       numaddrs, nsname[i]);
			} else if (_res.options & RES_DEBUG || verbose)
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
	msglen = res_mkquery(QUERY, namePtr, getclass, ns_t_axfr, NULL,
			     0, NULL, buf.qb2, sizeof buf);
	if (msglen < 0) {
		if (_res.options & RES_DEBUG)
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
		if ((sockFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			perror("ListHosts");
			return (ERROR);
		}
		memcpy(&sin.sin_addr, &nsipaddr[thisns], NS_INADDRSZ);
		if (_res.options & RES_DEBUG || verbose)
			printf("Trying %s\n", inet_ntoa(sin.sin_addr));
		if (connect(sockFD, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
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
				if (_res.options & RES_DEBUG || verbose)
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

		result = printinfo(&buf, cp, queryType, 1);
		if (! result) {
			error = ERR_PRINTING;
			break;
		}
		numAnswers++;
		cp = buf.qb2 + HFIXEDSZ;
		if (ntohs(buf.qb1.qdcount) > 0) {
			n = dn_skipname(cp, buf.qb2 + len);
			if (n < 0) {
				error = ERR_PRINTING;
				break;
			}
			cp += n + QFIXEDSZ;
		}
		nmp = cp;
		n = dn_skipname(cp, buf.qb2 + len);
		if (n < 0) {
			error = ERR_PRINTING;
			break;
		}
		cp += n;
		if (cp + INT16SZ > buf.qb2 + len) {
			error = ERR_PRINTING;
			break;
		}
		if ((ns_get16(cp) == ns_t_soa)) {
			(void) dn_expand(buf.qb2, buf.qb2 + len, nmp,
					 dname[soacnt], sizeof dname[0]);
			if (soacnt) {
				if (strcmp(dname[0], dname[1]) == 0)
					break;
			} else
				soacnt++;
		}
        }

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
			_res_resultcodes[headerPtr->rcode], 
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
	case NOERROR: 	return ("Success"); break;
	case FORMERR:	return ("Format error"); break;
	case SERVFAIL:	return ("Server failed"); break;
	case NXDOMAIN:	return ("Non-existent domain"); break;
	case NOTIMP:	return ("Not implemented"); break;
	case REFUSED:	return ("Query refused"); break;
	case NO_INFO: 	return ("No information"); break;
	case ERROR: 	return ("Unspecified error"); break;
	case TIME_OUT: 	return ("Timed out"); break;
	case NONAUTH: 	return ("Non-authoritative answer"); break;
	default:	return ("BAD ERROR VALUE");
	}
	/* NOTREACHED */
}
