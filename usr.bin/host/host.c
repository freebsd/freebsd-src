/*
 * ++Copyright++ 1986
 * -
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
 * -
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
 * -
 * --Copyright--
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1986 Regents of the University of California.\n\
 portions Copyright (c) 1993 Digital Equipment Corporation\n\
 All rights reserved.\n";
#endif /* not lint */

/*
 * Actually, this program is from Rutgers University, however it is
 * based on nslookup and other pieces of named tools, so it needs
 * that copyright notice.
 */

#ifndef lint
static char rcsid[] = "$Id: host.c,v 1.6 1996/01/07 06:21:56 peter Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <stdio.h>
#include <netdb.h>
#include <resolv.h>
#include <ctype.h>

#include <string.h>

extern int h_errno;

#define NUMMX 50

#define  SUCCESS		0
#define  TIME_OUT		-1
#define  NO_INFO 		-2
#define  ERROR 			-3
#define  NONAUTH 		-4

#define NAME_LEN 256

#ifndef T_TXT
#define T_TXT 16
#endif
#ifndef NO_DATA
#define NO_DATA NO_ADDRESS
#endif
#ifndef C_HS
#define C_HS 4
#endif

int sockFD;
FILE *filePtr;
char *DecodeError();

static struct __res_state orig;
extern struct __res_state _res;
static char *cname = NULL;
int getclass = C_IN;
int gettype;
int verbose = 0;
int list = 0;
int server_specified = 0;

u_char *pr_cdname();
char *pr_class(), *pr_rr(), *pr_type();
extern char *hostalias();

main(c, v)
	int c;
	char **v;
{
	unsigned addr;
	register struct hostent *hp;
	register char *s;
	register inverse = 0;
	register waitmode = 0;
	char *oldcname;
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
			gettype = T_ANY;
			v++;
			c--;
		}
        }
	if (c > 2) {
		s = v[2];
		server_specified++;

		if (!inet_aton(s, (struct in_addr *)&addr)) {
		  hp = gethostbyname(s);
		  if (hp == NULL) {
		    fprintf(stderr,"Error in looking up server name:\n");
		    hperror(h_errno);
		    exit(1);
		  }
		  _res.nsaddr.sin_addr = *(struct in_addr *)hp->h_addr;
		  printf("Using domain server:\n");
		  printanswer(hp);
		}
		else {
		  _res.nsaddr.sin_family = AF_INET;
		  _res.nsaddr.sin_addr.s_addr = addr;
		  _res.nsaddr.sin_port = htons(NAMESERVER_PORT);
		  printf("Using domain server %s:\n",
			 inet_ntoa(_res.nsaddr.sin_addr));
		}
		_res.nscount = 1;
		_res.retry = 2;
	      }
	if (c > 1 && (strcmp (v[1], ".") == 0 ||
	    !inet_aton(v[1], (struct in_addr *)&addr)))
		addr = -1;
	hp = NULL;
	h_errno = TRY_AGAIN;
/*
 * we handle default domains ourselves, thank you
 */
	_res.options &= ~RES_DEFNAMES;

        if (list)
	  exit(ListHosts(v[1], gettype ? gettype : T_A));
	oldcname = NULL;
	ncnames = 5;
	while (hp == NULL && h_errno == TRY_AGAIN) {
	  if (addr == -1) {
	      cname = NULL;
	      if (oldcname == NULL)
		hp = (struct hostent *)gethostinfo(v[1]);
	      else
		hp = (struct hostent *)gethostinfo(oldcname);
	      if (cname) {
		if (ncnames-- == 0) {
		  printf("Too many cnames.  Possible loop.\n");
		  exit(1);
		}
		strcat(cname, ".");
		oldcname = cname;
		hp = NULL;
		h_errno = TRY_AGAIN;
		continue;
	      }
	  }
	  else {
	    hp = gethostbyaddr((char*)&addr, 4, AF_INET);
	    if (hp)
	      printanswer(hp);
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

parsetype(s)
	char *s;
{
if (strcmp(s,"a") == 0)
  return(T_A);
if (strcmp(s,"ns") == 0)
  return(T_NS);
if (strcmp(s,"md") == 0)
  return(T_MD);
if (strcmp(s,"mf") == 0)
  return(T_MF);
if (strcmp(s,"cname") == 0)
  return(T_CNAME);
if (strcmp(s,"soa") == 0)
  return(T_SOA);
if (strcmp(s,"mb") == 0)
  return(T_MB);
if (strcmp(s,"mg") == 0)
  return(T_MG);
if (strcmp(s,"mr") == 0)
  return(T_MR);
if (strcmp(s,"null") == 0)
  return(T_NULL);
if (strcmp(s,"wks") == 0)
  return(T_WKS);
if (strcmp(s,"ptr") == 0)
  return(T_PTR);
if (strcmp(s,"hinfo") == 0)
  return(T_HINFO);
if (strcmp(s,"minfo") == 0)
  return(T_MINFO);
if (strcmp(s,"mx") == 0)
  return(T_MX);
if (strcmp(s,"txt") == 0)
  return(T_TXT);
if (strcmp(s,"rp") == 0)
  return(T_RP);
if (strcmp(s,"afsdb") == 0)
  return(T_AFSDB);
if (strcmp(s,"x25") == 0)
  return(T_X25);
if (strcmp(s,"isdn") == 0)
  return(T_ISDN);
if (strcmp(s,"rt") == 0)
  return(T_RT);
if (strcmp(s,"uinfo") == 0)
  return(T_UINFO);
if (strcmp(s,"uid") == 0)
  return(T_UID);
if (strcmp(s,"gid") == 0)
  return(T_GID);
if (strcmp(s,"unspec") == 0)
  return(T_UNSPEC);
if (strcmp(s,"any") == 0)
  return(T_ANY);
if (strcmp(s,"*") == 0)
  return(T_ANY);
if (atoi(s))
  return(atoi(s));
fprintf(stderr, "Invalid query type: %s\n", s);
exit(2);
}

parseclass(s)
	char *s;
{
if (strcmp(s,"in") == 0)
  return(C_IN);
if (strcmp(s,"hs") == 0)
  return(C_HS);
if (strcmp(s,"any") == 0)
  return(C_ANY);
if (atoi(s))
  return(atoi(s));
fprintf(stderr, "Invalid query class: %s\n", s);
exit(2);
}

printanswer(hp)
	register struct hostent *hp;
{
	register char **cp;
	register long **hptr;

	printf("Name: %s\n", hp->h_name);
	printf("Address:");
	for (hptr = (long **)hp->h_addr_list; *hptr; hptr++)
	  printf(" %s", inet_ntoa(*(struct in_addr *)*hptr));
	printf("\nAliases:");
	for (cp = hp->h_aliases; cp && *cp && **cp; cp++)
		printf(" %s", *cp);
	printf("\n\n");
}

hperror(errnum) 
int errnum;
{
switch(errnum) {
	case HOST_NOT_FOUND:
		fprintf(stderr,"Host not found.\n");
		break;
	case TRY_AGAIN:
		fprintf(stderr,"Host not found, try again.\n");
		break;
	case NO_RECOVERY:
		fprintf(stderr,"No recovery, Host not found.\n");
		break;
	case NO_ADDRESS:
		fprintf(stderr,"There is an entry for this host, but it doesn't have ");
		switch (gettype) {
			case T_A:
				fprintf(stderr,"an Internet address.\n");
				break;
			case T_NS:
				fprintf(stderr,"a Name Server.\n");
				break;
			case T_MD:
				fprintf(stderr,"a Mail Destination.\n");
				break;
			case T_MF:
				fprintf(stderr,"a Mail Forwarder.\n");
				break;
			case T_CNAME:
				fprintf(stderr,"a Canonical Name.\n");
				break;
			case T_SOA:
				fprintf(stderr,"a Start of Authority record.\n");
				break;
			case T_MB:
				fprintf(stderr,"a Mailbox Domain Name.\n");
				break;
			case T_MG:
				fprintf(stderr,"a Mail Group Member.\n");
				break;
			case T_MR:
				fprintf(stderr,"a Mail Rename Name.\n");
				break;
			case T_NULL:
				fprintf(stderr,"a Null Resource record.\n");
				break;
			case T_WKS:
				fprintf(stderr,"any Well Known Service information.\n");
				break;
			case T_PTR:
				fprintf(stderr,"a Pointer record.\n");
				break;
			case T_HINFO:
				fprintf(stderr,"any Host Information.\n");
				break;
			case T_MINFO:
				fprintf(stderr,"any Mailbox Information.\n");
				break;
			case T_MX:
				fprintf(stderr,"a Mail Exchanger record.\n");
				break;
			case T_TXT:
				fprintf(stderr,"a Text record.\n");
				break;
			case T_RP:
				fprintf(stderr,"a Responsible Person.\n");
				break;
			case T_UINFO:
				fprintf(stderr,"any User Information.\n");
				break;
			case T_UID:
				fprintf(stderr,"a User ID.\n");
				break;
			case T_GID:
				fprintf(stderr,"a Group ID.\n");
				break;
			case T_UNSPEC:
				fprintf(stderr,"any Unspecified Format data.\n");
				break;
			default:
				fprintf(stderr,"the information you requested.\n");
				break;
			}
		break;
	}
}


typedef union {
	HEADER qb1;
	u_char qb2[PACKETSZ];
} querybuf;

static u_char hostbuf[BUFSIZ+1];

gethostinfo(name)
	char *name;
{
	register char *cp, **domain;
	u_int n;
	int hp;
	int nDomain;
	int asis = 0;

	if (strcmp(name, ".") == 0)
		return(getdomaininfo(name, NULL));
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
	if (n == 0 && (cp = hostalias(name))) {
	        if (verbose)
		    printf("Aliased to \"%s\"\n", cp);
		_res.options |= RES_DEFNAMES;
		return (getdomaininfo(cp, (char *)NULL));
	}
	if (n >= _res.ndots) {
		asis = 1;
		if (verbose)
		    printf("Trying null domain\n");
		if(hp = getdomaininfo(name, (char*)NULL))
			return(hp);
	}
#ifdef MAXDS
	for (nDomain = 0;
	     _res.defdname_list[nDomain][0] != 0;
	     nDomain++) {
	    for (domain = _res.dnsrch_list[nDomain]; *domain; domain++) {
	        if (verbose)
		    printf("Trying domain \"%s\"\n", *domain);
		hp = getdomaininfo(name, *domain);
		if (hp)
			return (hp);
	    }
	}
#else
	for (domain = _res.dnsrch; *domain; domain++) {
	  if (verbose)
	    printf("Trying domain \"%s\"\n", *domain);
	  hp = getdomaininfo(name, *domain);
	  if (hp)
	    return (hp);
	}
#endif
	if (h_errno != HOST_NOT_FOUND ||
	   (_res.options & RES_DNSRCH) == 0)
		return (0);
	if (!asis)
		return (0);
	if (verbose)
	    printf("Trying null domain\n");
	return (getdomaininfo(name, (char *)NULL));
}

getdomaininfo(name, domain)
	char *name, *domain;
{
  int val1, val2;

  if (gettype)
    return getinfo(name, domain, gettype);
  else {
    val1 = getinfo(name, domain, T_A);
    if (cname || verbose)
      return val1;
    val2 = getinfo(name, domain, T_MX);
    return val1 || val2;
  }
}

getinfo(name, domain, type)
	char *name, *domain;
	int type;
{

	HEADER *hp;
	char *eom, *bp, *cp;
	querybuf buf, answer;
	int n, n1, i, j, nmx, ancount, nscount, arcount, qdcount, buflen;
	u_short pref, class;
	char host[2*MAXDNAME+2];

	if (domain == NULL)
		(void)sprintf(host, "%.*s", MAXDNAME, name);
	else
		(void)sprintf(host, "%.*s.%.*s", MAXDNAME, name, MAXDNAME, domain);

	n = res_mkquery(QUERY, host, getclass, type, NULL, 0, NULL,
			buf.qb2, sizeof(buf));
	if (n < 0) {
		if (_res.options & RES_DEBUG)
			printf("res_mkquery failed\n");
		h_errno = NO_RECOVERY;
		return(0);
	}
	n = res_send(buf.qb2, n, answer.qb2, sizeof answer);
	if (n < 0) {
		if (_res.options & RES_DEBUG)
			printf("res_send failed\n");
		h_errno = TRY_AGAIN;
		return (0);
	}
	eom = (char *)&answer + n;
	return(printinfo(&answer, eom, T_ANY, 0));
      }

printinfo(answer, eom, filter, isls)
	querybuf *answer;
	u_char *eom;
        int filter;
        int isls;
{
	HEADER *hp;
	u_char *bp, *cp;
	int n, n1, i, j, nmx, ancount, nscount, arcount, qdcount, buflen;
	u_short pref, class;

	/*
	 * find first satisfactory answer
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
				return(0);
			case SERVFAIL:
				h_errno = TRY_AGAIN;
				return(0);
#ifdef OLDJEEVES
			/*
			 * Jeeves (TOPS-20 server) still does not
			 * support MX records.  For the time being,
			 * we must accept FORMERRs as the same as
			 * NOERROR.
			 */
			case FORMERR:
#endif /*OLDJEEVES*/
			case NOERROR:
/* TpB - set a return error for this case. NO_DATA */
				h_errno = NO_DATA;
				return(0); /* was 1,but now indicates exception */
#ifndef OLDJEEVES
			case FORMERR:
#endif /*OLDJEEVES*/
			case NOTIMP:
			case REFUSED:
				h_errno = NO_RECOVERY;
				return(0);
		}
		return (0);
	}
	bp = hostbuf;
	nmx = 0;
	buflen = sizeof(hostbuf);
	cp = (u_char *)answer + HFIXEDSZ;
	if (qdcount) {
		cp += dn_skipname(cp, eom) + QFIXEDSZ;
		while (--qdcount > 0)
			cp += dn_skipname(cp, eom) + QFIXEDSZ;
	}
	if (ancount) {
	  if (!hp->aa)
	    if (verbose && isls == 0)
	      printf("The following answer is not authoritative:\n");
	  while (--ancount >= 0 && cp && cp < eom) {
	    cp = (u_char *)pr_rr(cp, answer, stdout, filter);
/*
 * When we ask for address and there is a CNAME, it seems to return
 * both the CNAME and the address.  Since we trace down the CNAME
 * chain ourselves, we don't really want to print the address at
 * this point.
 */
	    if (cname && ! verbose)
	      return (1);
	  }
	}
	if (! verbose)
	  return (1);
	if (nscount) {
	  printf("For authoritative answers, see:\n");
	  while (--nscount >= 0 && cp && cp < eom) {
	    cp = (u_char *)pr_rr(cp, answer, stdout, filter);
	  }
	}
	if (arcount) {
	  printf("Additional information:\n");
	  while (--arcount >= 0 && cp && cp < eom) {
	    cp = (u_char *)pr_rr(cp, answer, stdout, filter);
	  }
	}
	return(1);
 }

static char cnamebuf[MAXDNAME];

/*
 * Print resource record fields in human readable form.
 */
char *
pr_rr(cp, msg, file, filter)
	u_char *cp, *msg;
	FILE *file;
        int filter;
{
	int type, class, dlen, n, c, proto, ttl;
	struct in_addr inaddr;
	u_char *cp1;
	struct protoent *protop;
	struct servent *servp;
	char punc;
	int doprint;
	char name[MAXDNAME];

	if ((cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name))) == NULL)
		return (NULL);			/* compression error */

	type = _getshort(cp);
	cp += INT16SZ;

	class = _getshort(cp);
	cp += INT16SZ;

	ttl = _getlong(cp);
	cp += INT32SZ;

	if (filter == type || filter == T_ANY ||
	    (filter == T_A && (type == T_PTR || type == T_NS)))
	  doprint = 1;
	else
	  doprint = 0;

	if (doprint)
	  if (verbose)
	    fprintf(file,"%s\t%d%s\t%s",
		    name, ttl, pr_class(class), pr_type(type));
	  else
	    fprintf(file,"%s%s %s",name, pr_class(class), pr_type(type));
	if (verbose)
	  punc = '\t';
	else
	  punc = ' ';

	dlen = _getshort(cp);
	cp += INT16SZ;
	cp1 = cp;
	/*
	 * Print type specific data, if appropriate
	 */
	switch (type) {
	case T_A:
		switch (class) {
		case C_IN:
			bcopy(cp, (char *)&inaddr, INADDRSZ);
			if (dlen == 4) {
			        if (doprint)
				  fprintf(file,"%c%s", punc,
					inet_ntoa(inaddr));
				cp += dlen;
			} else if (dlen == 7) {
			        if (doprint) {
				  fprintf(file,"%c%s", punc,
					  inet_ntoa(inaddr));
				  fprintf(file,", protocol = %d", cp[4]);
				  fprintf(file,", port = %d",
					  (cp[5] << 8) + cp[6]);
				}
				cp += dlen;
			}
			break;
		}
		break;
	case T_CNAME:
		if (dn_expand(msg, msg + 512, cp, cnamebuf,
			      sizeof(cnamebuf)) >= 0)
			cname = cnamebuf;
	case T_MB:
#ifdef OLDRR
	case T_MD:
	case T_MF:
#endif /* OLDRR */
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
		  fprintf(file,"%c%s",punc, name);
		break;

	case T_HINFO:
	case T_ISDN:
		{
			u_char *cp2 = cp + dlen;
			if (n = *cp++) {
				if (doprint)
				  fprintf(file,"%c%.*s", punc, n, cp);
				cp += n;
			}
			if ((cp < cp2) && (n = *cp++)) {
				if (doprint)
				  fprintf(file,"%c%.*s", punc, n, cp);
				cp += n;
			} else if (type == T_HINFO)
				if (doprint)
				  fprintf(file,"\n; *** Warning *** OS-type missing");
		}
		break;

	case T_SOA:
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
		  fprintf(file,"\t%s", name);
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
		  fprintf(file," %s", name);
		if (doprint)
		  fprintf(file,"(\n\t\t\t%ld\t;serial (version)", _getlong(cp));
		cp += INT32SZ;
		if (doprint)
		  fprintf(file,"\n\t\t\t%ld\t;refresh period", _getlong(cp));
		cp += INT32SZ;
		if (doprint)
		  fprintf(file,"\n\t\t\t%ld\t;retry refresh this often", _getlong(cp));
		cp += INT32SZ;
		if (doprint)
		  fprintf(file,"\n\t\t\t%ld\t;expiration period", _getlong(cp));
		cp += INT32SZ;
		if (doprint)
		  fprintf(file,"\n\t\t\t%ld\t;minimum TTL\n\t\t\t)", _getlong(cp));
		cp += INT32SZ;
		break;

	case T_MX:
	case T_AFSDB:
	case T_RT:
		if (doprint)
		  if (type == T_MX && !verbose)
		    fprintf(file," (pri=%d) by ", _getshort(cp));
		  else
		    if (verbose)
		      fprintf(file,"\t%d ", _getshort(cp));
		    else
		      fprintf(file," ");
		cp += sizeof(u_short);
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
		  fprintf(file, "%s", name);
		break;

	case T_MINFO:
	case T_RP:
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint){
                  if (type == T_RP) {
		    char * p;
		    if (p = strchr(name, '.')) *p = '@';
                  }

		  fprintf(file, "%c%s", punc, name);
		}
		cp = (u_char *)pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
			fprintf(file, " %s", name);
		break;

	case T_TXT:
	case T_X25:
		{
		  int n,j;
		  u_char * end = cp + dlen;

		  if (doprint)
		    (void) fputs(" \"", file);
		  while (cp < end) {
		    if (n = *cp++) {
		      for (j = n; j > 0 && cp < end ; j --)
			if ((*cp == '\n') || (*cp == '"')) {
			  if (doprint){
			    (void) putc('\\', file);
			    (void) putc(*cp++, file);
			  }
			} else
			  if (doprint)
			    (void) putc(*cp++, file);
		    }
		  }
		  if (doprint)
		    (void) fputs("\"", file);
		  break;
		}
	case T_UINFO:
		if (doprint)
		  fprintf(file,"%c%s", punc, cp);
		cp += dlen;
		break;

	case T_UID:
	case T_GID:
		if (dlen == 4) {
			if (doprint)
			  fprintf(file,"%c%ld", punc, _getlong(cp));
			cp += INT32SZ;
		}
		break;

	case T_WKS:
		if (dlen < INT32SZ + 1)
			break;
		bcopy(cp, (char *)&inaddr, INADDRSZ);
		cp += INT32SZ;
		proto = *cp++;
		protop = getprotobynumber(proto);
		if (doprint)
		  if (protop)
		    fprintf(file,"%c%s %s", punc,
			    inet_ntoa(inaddr), protop->p_name);
		  else
		    fprintf(file,"%c%s %d", punc,
			    inet_ntoa(inaddr), proto);

		n = 0;
		while (cp < cp1 + dlen) {
			c = *cp++;
			do {
 				if (c & 0200) {
				  servp = NULL;
				  if (protop)
				    servp = getservbyport (htons(n),
							   protop->p_name);
				  if (doprint)
				    if (servp)
				      fprintf(file, " %s", servp->s_name);
				    else
				      fprintf(file, " %d", n);
				}
 				c <<= 1;
			} while (++n & 07);
		}
		break;

	default:
		if (doprint)
		  fprintf(file,"%c???", punc);
		cp += dlen;
	}
	if (cp != cp1 + dlen)
		fprintf(file,"packet size error (%#x != %#x)\n", cp, cp1+dlen);
	if (doprint)
	  fprintf(file,"\n");
	return (char *)cp;
}

static	char nbuf[20];

/*
 * Return a string for the type
 */
char *
pr_type(type)
	int type;
{
	switch (type) {
	case T_A:
		return(verbose? "A" : "has address");
	case T_NS:		/* authoritative server */
		return("NS");
#ifdef OLDRR
	case T_MD:		/* mail destination */
		return("MD");
	case T_MF:		/* mail forwarder */
		return("MF");
#endif /* OLDRR */
	case T_CNAME:		/* connonical name */
		return(verbose? "CNAME" : "is a nickname for");
	case T_SOA:		/* start of authority zone */
		return("SOA");
	case T_MB:		/* mailbox domain name */
		return("MB");
	case T_MG:		/* mail group member */
		return("MG");
	case T_MX:		/* mail routing info */
		return(verbose? "MX" : "mail is handled");
	case T_TXT:		/* TXT - descriptive info */
		return(verbose? "TXT" : "descriptive text");
	case T_AFSDB:		/* AFS/DCE info */
		return(verbose? "AFSDB" : "DCE or AFS service from");
	case T_X25:		/* X25 */
		return(verbose? "X25" : "X25 address");
	case T_ISDN:		/* ISDN */
		return(verbose? "ISDN" : "ISDN address");
	case T_RT:		/* Router */
		return(verbose? "RT" : "router");
	case T_MR:		/* mail rename name */
		return("MR");
	case T_NULL:		/* null resource record */
		return("NULL");
	case T_WKS:		/* well known service */
		return("WKS");
	case T_PTR:		/* domain name pointer */
		return("PTR");
	case T_HINFO:		/* host information */
		return("HINFO");
	case T_MINFO:		/* mailbox information */
		return("MINFO");
        case T_RP:              /* responsible person */
		return(verbose?"RP":"responsible person");
	case T_AXFR:		/* zone transfer */
		return("AXFR");
	case T_MAILB:		/* mail box */
		return("MAILB");
	case T_MAILA:		/* mail address */
		return("MAILA");
	case T_ANY:		/* matches any type */
		return("ANY");
	case T_UINFO:
		return("UINFO");
	case T_UID:
		return("UID");
	case T_GID:
		return("GID");
	default:
		sprintf(nbuf, "%d", type);
		return nbuf;
	}
}

/*
 * Return a mnemonic for class
 */
char *
pr_class(class)
	int class;
{

	switch (class) {
	case C_IN:		/* internet class */
		return(verbose? " IN" : "");
	case C_HS:		/* internet class */
		return(verbose? " HS" : "");
	case C_ANY:		/* matches any class */
		return(" ANY");
	default:
		(void) sprintf(nbuf," %d", class);
		return nbuf;
	}
}

u_char *
pr_cdname(cp, msg, name, namelen)
	u_char *cp, *msg;
	char *name;
        int namelen;
{
	int n;

	if ((n = dn_expand(msg, msg + 512, cp, name, namelen - 2)) < 0)
		return (NULL);
	if (name[0] == '\0') {
		name[0] = '.';
		name[1] = '\0';
	}
	return (cp + n);
}

char *resultcodes[] = {
	"NOERROR",
	"FORMERR",
	"SERVFAIL",
	"NXDOMAIN",
	"NOTIMP",
	"REFUSED",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13",
	"14",
	"NOCHANGE",
};



/*
 ******************************************************************************
 *
 *  ListHosts --
 *
 *	Requests the name server to do a zone transfer so we
 *	find out what hosts it knows about.
 *
 *  Results:
 *	SUCCESS		the listing was successful.
 *	ERROR		the server could not be contacted because
 *			a socket could not be obtained or an error
 *			occured while receiving, or the output file
 *			could not be opened.
 *
 ******************************************************************************
 */

int
ListHosts(namePtr, queryType)
    char *namePtr;
    int  queryType;  /* e.g. T_A */
{
	querybuf 		buf, answer;
	struct sockaddr_in 	sin;
	HEADER 			*headerPtr;

	int 			msglen;
	int 			amtToRead;
	int 			numRead;
	int 			i;
	int 			numAnswers = 0;
	int 			result;
	int 			soacnt = 0;
	u_short 		len;
	int			dlen;
	int			type;
	int			nscount;
	u_char 			*cp, *nmp;
	char 			name[NAME_LEN];
	char 			dname[2][NAME_LEN];
	char 			domain[NAME_LEN];
/* names and addresses of name servers to try */
#define NUMNS 8
	char			nsname[NUMNS][NAME_LEN];
	int			nshaveaddr[NUMNS];
#define IPADDRSIZE 4
#define NUMNSADDR 16
	char	 		nsipaddr[NUMNSADDR][IPADDRSIZE];
	int			numns;
	int			numnsaddr;
	int			thisns;
	struct hostent		*hp;
	enum {
	    NO_ERRORS,
	    ERR_READING_LEN,
	    ERR_READING_MSG,
	    ERR_PRINTING
	} error = NO_ERRORS;

/*
 * normalize to not have trailing dot.  We do string compares below
 * of info from name server, and it won't have trailing dots.
 */
	i = strlen(namePtr);
	if (namePtr[i-1] == '.')
	  namePtr[i-1] = 0;

	if (server_specified) {
	  bcopy(&_res.nsaddr.sin_addr, nsipaddr[0], IPADDRSIZE);
	  numnsaddr = 1;
	}
	else {

/*
 * First we have to find out where to look.  This needs a NS query,
 * possibly followed by looking up addresses for some of the names.
 */

	msglen = res_mkquery(QUERY, namePtr, C_IN, T_NS, NULL,
			     0, NULL, buf.qb2, sizeof buf);

	if (msglen < 0) {
		printf("res_mkquery failed\n");
		return (ERROR);
	}

	msglen = res_send(buf.qb2, msglen, answer.qb2, sizeof answer);

	if (msglen < 0) {
		printf("Unable to get to nameserver -- try again later\n");
		return (ERROR);
	}
	if (_res.options & RES_DEBUG || verbose)
		printf("rcode = %d (%s), ancount=%d\n",
		       answer.qb1.rcode, DecodeError(answer.qb1.rcode),
		       ntohs(answer.qb1.ancount));

/*
 * Analyze response to our NS lookup
 */

	nscount = ntohs(answer.qb1.ancount) + ntohs(answer.qb1.nscount) +
		  ntohs(answer.qb1.arcount);

	if (answer.qb1.rcode != NOERROR || nscount == 0) {
		switch (answer.qb1.rcode) {
			case NXDOMAIN:
				/* Check if it's an authoritive answer */
				if (answer.qb1.aa) {
					printf("No such domain\n");
				} else {
					printf("Unable to get information about domain -- try again later.\n");
				}
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

	cp = (u_char *)answer.qb2 + HFIXEDSZ;
	if (ntohs(answer.qb1.qdcount) > 0)
	  cp += dn_skipname(cp, answer.qb2 + msglen) + QFIXEDSZ;

	numns = 0;
	numnsaddr = 0;

/*
 * Look at response from NS lookup for NS and A records.
 */

	for (;nscount; nscount--) {
	  cp += dn_expand(answer.qb2, answer.qb2 + msglen, cp,
			  domain, sizeof(domain));
	  type = _getshort(cp);
	  cp += INT16SZ + INT16SZ + INT32SZ;
	  dlen = _getshort(cp);
	  cp += INT16SZ;
	  if (type == T_NS) {
	    if (dn_expand(answer.qb2, answer.qb2 + msglen, cp,
			  name, sizeof(name)) >= 0) {
	      if (numns < NUMNS && strcasecmp((char *)domain, namePtr) == 0) {
		for (i = 0; i < numns; i++)
		  if (strcasecmp(nsname[i], (char *)name) == 0)
		    break;  /* duplicate */
		if (i >= numns) {
		  strncpy(nsname[numns], (char *)name, sizeof(name));
		  nshaveaddr[numns] = 0;
		  numns++;
		}
	      }
	    }
	  }
	  else if (type == T_A) {
	    if (numnsaddr < NUMNSADDR)
	      for (i = 0; i < numns; i++) {
		if (strcasecmp(nsname[i], (char *)domain) == 0) {
		  nshaveaddr[i]++;
		  bcopy(cp, nsipaddr[numnsaddr],IPADDRSIZE);
		  numnsaddr++;
		  break;
		}
	      }
	  }
	  cp += dlen;
	}

/*
 * Usually we'll get addresses for all the servers in the additional
 * info section.  But in case we don't, look up their addresses.
 */

	for (i = 0; i < numns; i++) {
	  if (! nshaveaddr[i]) {
	    register long **hptr;
	    int numaddrs = 0;

	    hp = gethostbyname(nsname[i]);
	    if (hp) {
	      for (hptr = (long **)hp->h_addr_list; *hptr; hptr++)
		if (numnsaddr < NUMNSADDR) {
		  bcopy((char *)*hptr, nsipaddr[numnsaddr],IPADDRSIZE);
		  numnsaddr++;
		  numaddrs++;
		}
	    }
	    if (_res.options & RES_DEBUG || verbose)
	      printf("Found %d addresses for %s by extra query\n",
		     numaddrs, nsname[i]);
	  }
	  else
	    if (_res.options & RES_DEBUG || verbose)
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
	 *  Create a query packet for the requested domain name.
	 *
	 */
	msglen = res_mkquery(QUERY, namePtr, getclass, T_AXFR, NULL,
			     0, NULL, buf.qb2, sizeof buf);
	if (msglen < 0) {
	    if (_res.options & RES_DEBUG) {
		fprintf(stderr, "ListHosts: Res_mkquery failed\n");
	    }
	    return (ERROR);
	}

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family	= AF_INET;
	sin.sin_port	=  htons(NAMESERVER_PORT);

	/*
	 *  Set up a virtual circuit to the server.
	 */

	for (;thisns < numnsaddr; thisns++) {
	  if ((sockFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    perror("ListHosts");
	    return(ERROR);
	  }
	  bcopy(nsipaddr[thisns], &sin.sin_addr, IPADDRSIZE);
	  if (_res.options & RES_DEBUG || verbose)
	    printf("Trying %s\n", inet_ntoa(sin.sin_addr));
	  if (connect(sockFD, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
	    break;
	  if (verbose)
	    perror("Connection failed, trying next server");
	  (void) close(sockFD);
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

	__putshort(msglen, (u_char *)&len);

        if (write(sockFD, (char *)&len, INT16SZ) != INT16SZ ||
            write(sockFD, (char *) &buf, msglen) != msglen) {
		perror("ListHosts");
		(void) close(sockFD);
		sockFD = -1;
		return (ERROR);
	}

	filePtr = stdout;

	while (1) {

	    /*
	     * Read the length of the response.
	     */

	    cp = (u_char *) &buf;
	    amtToRead = INT16SZ;
	    while(amtToRead > 0 && (numRead = read(sockFD, cp, amtToRead)) > 0){
		cp 	  += numRead;
		amtToRead -= numRead;
	    }
	    if (numRead <= 0) {
		error = ERR_READING_LEN;
		break;
	    }

	    if ((len = _getshort((u_char*)&buf)) == 0) {
		break;	/* nothing left to read */
	    }

	    /*
	     * Read the response.
	     */

	    amtToRead = len;
	    cp = (u_char *) &buf;
	    while(amtToRead > 0 && (numRead = read(sockFD, cp, amtToRead)) > 0){
		cp += numRead;
		amtToRead -= numRead;
	    }
	    if (numRead <= 0) {
		error = ERR_READING_MSG;
		break;
	    }

	    i = buf.qb1.rcode;
	    if (i != NOERROR || ntohs(buf.qb1.ancount) == 0) {
	      if ((thisns+1) < numnsaddr &&
		  (i == SERVFAIL || i == NOTIMP || i == REFUSED)) {
		if (_res.options & RES_DEBUG || verbose)
		  printf("Server failed, trying next server: %s\n",
			 i != NOERROR ?
			 DecodeError(i) : "Premature end of data");
		(void) close(sockFD);
		sockFD = -1;
		thisns++;
		goto again;
	      }
	      printf("Server failed: %s\n",
		     i != NOERROR ? DecodeError(i) : "Premature end of data");
	      break;
	    }


	    result = printinfo(&buf, cp, queryType, 1);
	    if (! result) {
		error = ERR_PRINTING;
		break;
	    }
	    numAnswers++;
	    cp = buf.qb2 + HFIXEDSZ;
	    if (ntohs(buf.qb1.qdcount) > 0)
		cp += dn_skipname(cp, buf.qb2 + len) + QFIXEDSZ;

	    nmp = cp;
	    cp += dn_skipname(cp, (u_char *)&buf + len);
	    if ((_getshort(cp) == T_SOA)) {
		(void) dn_expand(buf.qb2, buf.qb2 + len, nmp,
				 dname[soacnt], sizeof dname[0]);
	        if (soacnt) {
		    if (strcmp((char *)dname[0], (char *)dname[1]) == 0)
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
		return(ERROR);

	    case ERR_PRINTING:
		fprintf(stderr,"*** Error during listing of %s: %s\n",
				namePtr, DecodeError(result));
		return(result);

	    case ERR_READING_MSG:
		headerPtr = (HEADER *) &buf;
		fprintf(stderr,"ListHosts: error receiving zone transfer:\n");
		fprintf(stderr,
	       "  result: %s, answers = %d, authority = %d, additional = %d\n",
		    	resultcodes[headerPtr->rcode],
		    	ntohs(headerPtr->ancount), ntohs(headerPtr->nscount),
			ntohs(headerPtr->arcount));
		return(ERROR);
	    default:
		return(ERROR);
	}
}

char *
DecodeError(result)
    int result;
{
	switch(result) {
	    case NOERROR: 	return("Success"); break;
	    case FORMERR:	return("Format error"); break;
	    case SERVFAIL:	return("Server failed"); break;
	    case NXDOMAIN:	return("Non-existent domain"); break;
	    case NOTIMP:	return("Not implemented"); break;
	    case REFUSED:	return("Query refused"); break;
#ifdef NOCHANGE
	    case NOCHANGE:	return("No change"); break;
#endif
	    case NO_INFO: 	return("No information"); break;
	    case ERROR: 	return("Unspecified error"); break;
	    case TIME_OUT: 	return("Timed out"); break;
	    case NONAUTH: 	return("Non-authoritative answer"); break;
	    default: 		break;
	}
	return("BAD ERROR VALUE");
}
