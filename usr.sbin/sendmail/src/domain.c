/*
 * Copyright (c) 1986, 1995-1997 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#include "sendmail.h"

#ifndef lint
#if NAMED_BIND
static char sccsid[] = "@(#)domain.c	8.68 (Berkeley) 8/2/97 (with name server)";
#else
static char sccsid[] = "@(#)domain.c	8.68 (Berkeley) 8/2/97 (without name server)";
#endif
#endif /* not lint */

#if NAMED_BIND

#include <errno.h>
#include <resolv.h>
#include <arpa/inet.h>

/*
**  The standard udp packet size PACKETSZ (512) is not sufficient for some
**  nameserver answers containing very many resource records. The resolver
**  may switch to tcp and retry if it detects udp packet overflow.
**  Also note that the resolver routines res_query and res_search return
**  the size of the *un*truncated answer in case the supplied answer buffer
**  it not big enough to accommodate the entire answer.
*/

#ifndef MAXPACKET
# define MAXPACKET 8192		/* max packet size used internally by BIND */
#endif

typedef union
{
	HEADER	qb1;
	u_char	qb2[MAXPACKET];
} querybuf;

#ifndef MXHOSTBUFSIZE
# define MXHOSTBUFSIZE	(128 * MAXMXHOSTS)
#endif

static char	MXHostBuf[MXHOSTBUFSIZE];

#ifndef MAXDNSRCH
# define MAXDNSRCH	6	/* number of possible domains to search */
#endif

#ifndef MAX
# define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

#ifndef NO_DATA
# define NO_DATA	NO_ADDRESS
#endif

#ifndef HFIXEDSZ
# define HFIXEDSZ	12	/* sizeof(HEADER) */
#endif

#define MAXCNAMEDEPTH	10	/* maximum depth of CNAME recursion */

#if defined(__RES) && (__RES >= 19940415)
# define RES_UNC_T	char *
#else
# define RES_UNC_T	u_char *
#endif
/*
**  GETMXRR -- get MX resource records for a domain
**
**	Parameters:
**		host -- the name of the host to MX.
**		mxhosts -- a pointer to a return buffer of MX records.
**		droplocalhost -- If TRUE, all MX records less preferred
**			than the local host (as determined by $=w) will
**			be discarded.
**		rcode -- a pointer to an EX_ status code.
**
**	Returns:
**		The number of MX records found.
**		-1 if there is an internal failure.
**		If no MX records are found, mxhosts[0] is set to host
**			and 1 is returned.
*/

int
getmxrr(host, mxhosts, droplocalhost, rcode)
	char *host;
	char **mxhosts;
	bool droplocalhost;
	int *rcode;
{
	register u_char *eom, *cp;
	register int i, j, n;
	int nmx = 0;
	register char *bp;
	HEADER *hp;
	querybuf answer;
	int ancount, qdcount, buflen;
	bool seenlocal = FALSE;
	u_short pref, type;
	u_short localpref = 256;
	char *fallbackMX = FallBackMX;
	bool trycanon = FALSE;
	int (*resfunc)();
	extern int res_query(), res_search();
	u_short prefer[MAXMXHOSTS];
	int weight[MAXMXHOSTS];
	extern int mxrand __P((char *));

	if (tTd(8, 2))
		printf("getmxrr(%s, droplocalhost=%d)\n", host, droplocalhost);

	if (fallbackMX != NULL && droplocalhost &&
	    wordinclass(fallbackMX, 'w'))
	{
		/* don't use fallback for this pass */
		fallbackMX = NULL;
	}

	*rcode = EX_OK;

	/* efficiency hack -- numeric or non-MX lookups */
	if (host[0] == '[')
		goto punt;

	/*
	**  If we don't have MX records in our host switch, don't
	**  try for MX records.  Note that this really isn't "right",
	**  since we might be set up to try NIS first and then DNS;
	**  if the host is found in NIS we really shouldn't be doing
	**  MX lookups.  However, that should be a degenerate case.
	*/

	if (!UseNameServer)
		goto punt;
	if (HasWildcardMX && ConfigLevel >= 6)
		resfunc = res_query;
	else
		resfunc = res_search;

	errno = 0;
	n = (*resfunc)(host, C_IN, T_MX, (u_char *) &answer, sizeof(answer));
	if (n < 0)
	{
		if (tTd(8, 1))
			printf("getmxrr: res_search(%s) failed (errno=%d, h_errno=%d)\n",
			    (host == NULL) ? "<NULL>" : host, errno, h_errno);
		switch (h_errno)
		{
		  case NO_DATA:
			trycanon = TRUE;
			/* fall through */

		  case NO_RECOVERY:
			/* no MX data on this host */
			goto punt;

		  case HOST_NOT_FOUND:
#if BROKEN_RES_SEARCH
		  case 0:	/* Ultrix resolver retns failure w/ h_errno=0 */
#endif
			/* host doesn't exist in DNS; might be in /etc/hosts */
			trycanon = TRUE;
			*rcode = EX_NOHOST;
			goto punt;

		  case TRY_AGAIN:
		  case -1:
			/* couldn't connect to the name server */
			if (fallbackMX != NULL)
			{
				/* name server is hosed -- push to fallback */
				mxhosts[nmx++] = fallbackMX;
				return nmx;
			}
			/* it might come up later; better queue it up */
			*rcode = EX_TEMPFAIL;
			break;

		  default:
			syserr("getmxrr: res_search (%s) failed with impossible h_errno (%d)\n",
				host, h_errno);
			*rcode = EX_OSERR;
			break;
		}

		/* irreconcilable differences */
		return (-1);
	}

	/* avoid problems after truncation in tcp packets */
	if (n > sizeof(answer))
		n = sizeof(answer);

	/* find first satisfactory answer */
	hp = (HEADER *)&answer;
	cp = (u_char *)&answer + HFIXEDSZ;
	eom = (u_char *)&answer + n;
	for (qdcount = ntohs(hp->qdcount); qdcount--; cp += n + QFIXEDSZ)
		if ((n = dn_skipname(cp, eom)) < 0)
			goto punt;
	buflen = sizeof(MXHostBuf) - 1;
	bp = MXHostBuf;
	ancount = ntohs(hp->ancount);
	while (--ancount >= 0 && cp < eom && nmx < MAXMXHOSTS - 1)
	{
		if ((n = dn_expand((u_char *)&answer,
		    eom, cp, (RES_UNC_T) bp, buflen)) < 0)
			break;
		cp += n;
		GETSHORT(type, cp);
 		cp += INT16SZ + INT32SZ;
		GETSHORT(n, cp);
		if (type != T_MX)
		{
			if (tTd(8, 8) || _res.options & RES_DEBUG)
				printf("unexpected answer type %d, size %d\n",
				    type, n);
			cp += n;
			continue;
		}
		GETSHORT(pref, cp);
		if ((n = dn_expand((u_char *)&answer, eom, cp,
				   (RES_UNC_T) bp, buflen)) < 0)
			break;
		cp += n;
		if (wordinclass(bp, 'w'))
		{
			if (tTd(8, 3))
				printf("found localhost (%s) in MX list, pref=%d\n",
					bp, pref);
			if (droplocalhost)
			{
				if (!seenlocal || pref < localpref)
					localpref = pref;
				seenlocal = TRUE;
				continue;
			}
			weight[nmx] = 0;
		}
		else
			weight[nmx] = mxrand(bp);
		prefer[nmx] = pref;
		mxhosts[nmx++] = bp;
		n = strlen(bp);
		bp += n;
		if (bp[-1] != '.')
		{
			*bp++ = '.';
			n++;
		}
		*bp++ = '\0';
		buflen -= n + 1;
	}

	/* sort the records */
	for (i = 0; i < nmx; i++)
	{
		for (j = i + 1; j < nmx; j++)
		{
			if (prefer[i] > prefer[j] ||
			    (prefer[i] == prefer[j] && weight[i] > weight[j]))
			{
				register int temp;
				register char *temp1;

				temp = prefer[i];
				prefer[i] = prefer[j];
				prefer[j] = temp;
				temp1 = mxhosts[i];
				mxhosts[i] = mxhosts[j];
				mxhosts[j] = temp1;
				temp = weight[i];
				weight[i] = weight[j];
				weight[j] = temp;
			}
		}
		if (seenlocal && prefer[i] >= localpref)
		{
			/* truncate higher preference part of list */
			nmx = i;
		}
	}

	/* delete duplicates from list (yes, some bozos have duplicates) */
	for (i = 0; i < nmx - 1; )
	{
		if (strcasecmp(mxhosts[i], mxhosts[i + 1]) != 0)
			i++;
		else
		{
			/* compress out duplicate */
			for (j = i + 1; j < nmx; j++)
				mxhosts[j] = mxhosts[j + 1];
			nmx--;
		}
	}

	if (nmx == 0)
	{
punt:
		if (seenlocal &&
		    (!TryNullMXList || sm_gethostbyname(host) == NULL))
		{
			/*
			**  If we have deleted all MX entries, this is
			**  an error -- we should NEVER send to a host that
			**  has an MX, and this should have been caught
			**  earlier in the config file.
			**
			**  Some sites prefer to go ahead and try the
			**  A record anyway; that case is handled by
			**  setting TryNullMXList.  I believe this is a
			**  bad idea, but it's up to you....
			*/

			*rcode = EX_CONFIG;
			syserr("MX list for %s points back to %s",
				host, MyHostName);
			return -1;
		}
		if (strlen(host) >= (SIZE_T) sizeof MXHostBuf)
		{
			*rcode = EX_CONFIG;
			syserr("Host name %s too long", shortenstring(host, 203));
			return -1;
		}
		snprintf(MXHostBuf, sizeof MXHostBuf, "%s", host);
		mxhosts[0] = MXHostBuf;
		if (host[0] == '[')
		{
			register char *p;

			/* this may be an MX suppression-style address */
			p = strchr(MXHostBuf, ']');
			if (p != NULL)
			{
				*p = '\0';
				if (inet_addr(&MXHostBuf[1]) != INADDR_NONE)
				{
					nmx++;
					*p = ']';
				}
				else
				{
					trycanon = TRUE;
					mxhosts[0]++;
				}
			}
		}
		if (trycanon &&
		    getcanonname(mxhosts[0], sizeof MXHostBuf - 2, FALSE))
		{
			bp = &MXHostBuf[strlen(MXHostBuf)];
			if (bp[-1] != '.')
			{
				*bp++ = '.';
				*bp = '\0';
			}
			nmx = 1;
		}
	}

	/* if we have a default lowest preference, include that */
	if (fallbackMX != NULL && !seenlocal)
		mxhosts[nmx++] = fallbackMX;

	return (nmx);
}
/*
**  MXRAND -- create a randomizer for equal MX preferences
**
**	If two MX hosts have equal preferences we want to randomize
**	the selection.  But in order for signatures to be the same,
**	we need to randomize the same way each time.  This function
**	computes a pseudo-random hash function from the host name.
**
**	Parameters:
**		host -- the name of the host.
**
**	Returns:
**		A random but repeatable value based on the host name.
**
**	Side Effects:
**		none.
*/

int
mxrand(host)
	register char *host;
{
	int hfunc;
	static unsigned int seed;

	if (seed == 0)
	{
		seed = (int) curtime() & 0xffff;
		if (seed == 0)
			seed++;
	}

	if (tTd(17, 9))
		printf("mxrand(%s)", host);

	hfunc = seed;
	while (*host != '\0')
	{
		int c = *host++;

		if (isascii(c) && isupper(c))
			c = tolower(c);
		hfunc = ((hfunc << 1) ^ c) % 2003;
	}

	hfunc &= 0xff;
	hfunc++;

	if (tTd(17, 9))
		printf(" = %d\n", hfunc);
	return hfunc;
}
/*
**  BESTMX -- find the best MX for a name
**
**	This is really a hack, but I don't see any obvious way
**	to generalize it at the moment.
*/

char *
bestmx_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	int nmx;
	auto int rcode;
	int saveopts = _res.options;
	char *mxhosts[MAXMXHOSTS + 1];

	_res.options &= ~(RES_DNSRCH|RES_DEFNAMES);
	nmx = getmxrr(name, mxhosts, FALSE, &rcode);
	_res.options = saveopts;
	if (nmx <= 0)
		return NULL;
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
		return map_rewrite(map, mxhosts[0], strlen(mxhosts[0]), av);
}
/*
**  DNS_GETCANONNAME -- get the canonical name for named host using DNS
**
**	This algorithm tries to be smart about wildcard MX records.
**	This is hard to do because DNS doesn't tell is if we matched
**	against a wildcard or a specific MX.
**	
**	We always prefer A & CNAME records, since these are presumed
**	to be specific.
**
**	If we match an MX in one pass and lose it in the next, we use
**	the old one.  For example, consider an MX matching *.FOO.BAR.COM.
**	A hostname bletch.foo.bar.com will match against this MX, but
**	will stop matching when we try bletch.bar.com -- so we know
**	that bletch.foo.bar.com must have been right.  This fails if
**	there was also an MX record matching *.BAR.COM, but there are
**	some things that just can't be fixed.
**
**	Parameters:
**		host -- a buffer containing the name of the host.
**			This is a value-result parameter.
**		hbsize -- the size of the host buffer.
**		trymx -- if set, try MX records as well as A and CNAME.
**		statp -- pointer to place to store status.
**
**	Returns:
**		TRUE -- if the host matched.
**		FALSE -- otherwise.
*/

bool
dns_getcanonname(host, hbsize, trymx, statp)
	char *host;
	int hbsize;
	bool trymx;
	int *statp;
{
	register u_char *eom, *ap;
	register char *cp;
	register int n; 
	HEADER *hp;
	querybuf answer;
	int ancount, qdcount;
	int ret;
	char **domain;
	int type;
	char **dp;
	char *mxmatch;
	bool amatch;
	bool gotmx = FALSE;
	int qtype;
	int loopcnt;
	char *xp;
	char nbuf[MAX(MAXPACKET, MAXDNAME*2+2)];
	char *searchlist[MAXDNSRCH+2];
	extern char *gethostalias();

	if (tTd(8, 2))
		printf("dns_getcanonname(%s, trymx=%d)\n", host, trymx);

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}

	/*
	**  Initialize domain search list.  If there is at least one
	**  dot in the name, search the unmodified name first so we
	**  find "vse.CS" in Czechoslovakia instead of in the local
	**  domain (e.g., vse.CS.Berkeley.EDU).
	**
	**  Older versions of the resolver could create this
	**  list by tearing apart the host name.
	*/

	loopcnt = 0;
cnameloop:
	/* Check for dots in the name */
	for (cp = host, n = 0; *cp != '\0'; cp++)
		if (*cp == '.')
			n++;

	/*
	**  If this is a simple name, determine whether it matches an 
	**  alias in the file defined by the environment variable HOSTALIASES.
	*/
	if (n == 0 && (xp = gethostalias(host)) != NULL)
	{
		if (loopcnt++ > MAXCNAMEDEPTH)
		{
			syserr("loop in ${HOSTALIASES} file");
		}
		else
		{
			strncpy(host, xp, hbsize);
			host[hbsize - 1] = '\0';
			goto cnameloop;
		}
	}

	/*
	**  Build the search list.  
	**	If there is at least one dot in name, start with a null
	**	domain to search the unmodified name first.
	**	If name does not end with a dot and search up local domain
	**	tree desired, append each local domain component to the
	**	search list; if name contains no dots and default domain
	**	name is desired, append default domain name to search list;
	**	else if name ends in a dot, remove that dot.
	*/

	dp = searchlist;
	if (n > 0)
		*dp++ = "";
	if (n >= 0 && *--cp != '.' && bitset(RES_DNSRCH, _res.options))
	{
		for (domain = _res.dnsrch; *domain != NULL; )
			*dp++ = *domain++;
	}
	else if (n == 0 && bitset(RES_DEFNAMES, _res.options))
	{
		*dp++ = _res.defdname;
	}
	else if (*cp == '.')
	{
		*cp = '\0';
	}
	*dp = NULL;

	/*
	**  Now loop through the search list, appending each domain in turn
	**  name and searching for a match.
	*/

	mxmatch = NULL;
	qtype = T_ANY;

	for (dp = searchlist; *dp != NULL; )
	{
		if (qtype == T_ANY)
			gotmx = FALSE;
		if (tTd(8, 5))
			printf("dns_getcanonname: trying %s.%s (%s)\n",
				host, *dp,
				qtype == T_ANY ? "ANY" : qtype == T_A ? "A" :
				qtype == T_MX ? "MX" : "???");
		ret = res_querydomain(host, *dp, C_IN, qtype,
				      answer.qb2, sizeof(answer.qb2));
		if (ret <= 0)
		{
			if (tTd(8, 7))
				printf("\tNO: errno=%d, h_errno=%d\n",
					errno, h_errno);

			if (errno == ECONNREFUSED || h_errno == TRY_AGAIN)
			{
				/* the name server seems to be down */
				h_errno = TRY_AGAIN;
				*statp = EX_TEMPFAIL;
				return FALSE;
			}

			if (h_errno != HOST_NOT_FOUND)
			{
				/* might have another type of interest */
				if (qtype == T_ANY)
				{
					qtype = T_A;
					continue;
				}
				else if (qtype == T_A && !gotmx && trymx)
				{
					qtype = T_MX;
					continue;
				}
			}

			/* definite no -- try the next domain */
			dp++;
			qtype = T_ANY;
			continue;
		}
		else if (tTd(8, 7))
			printf("\tYES\n");

		/* avoid problems after truncation in tcp packets */
		if (ret > sizeof(answer))
			ret = sizeof(answer);

		/*
		**  Appear to have a match.  Confirm it by searching for A or
		**  CNAME records.  If we don't have a local domain
		**  wild card MX record, we will accept MX as well.
		*/

		hp = (HEADER *) &answer;
		ap = (u_char *) &answer + HFIXEDSZ;
		eom = (u_char *) &answer + ret;

		/* skip question part of response -- we know what we asked */
		for (qdcount = ntohs(hp->qdcount); qdcount--; ap += ret + QFIXEDSZ)
		{
			if ((ret = dn_skipname(ap, eom)) < 0)
			{
				if (tTd(8, 20))
					printf("qdcount failure (%d)\n",
						ntohs(hp->qdcount));
				*statp = EX_SOFTWARE;
				return FALSE;		/* ???XXX??? */
			}
		}

		amatch = FALSE;
		for (ancount = ntohs(hp->ancount); --ancount >= 0 && ap < eom;
									ap += n)
		{
			n = dn_expand((u_char *) &answer, eom, ap,
				      (RES_UNC_T) nbuf, sizeof nbuf);
			if (n < 0)
				break;
			ap += n;
			GETSHORT(type, ap);
			ap += INT16SZ + INT32SZ;
			GETSHORT(n, ap);
			switch (type)
			{
			  case T_MX:
				gotmx = TRUE;
				if (**dp != '\0' && HasWildcardMX)
				{
					/*
					**  If we are using MX matches and have
					**  not yet gotten one, save this one
					**  but keep searching for an A or 
					**  CNAME match.
					*/

					if (trymx && mxmatch == NULL)
						mxmatch = *dp;
					continue;
				}

				/*
				**  If we did not append a domain name, this
				**  must have been a canonical name to start
				**  with.  Even if we did append a domain name,
				**  in the absence of a wildcard MX this must
				**  still be a real MX match.
				**  Such MX matches are as good as an A match,
				**  fall through.
				*/

			  case T_A:
				/* Flag that a good match was found */
				amatch = TRUE;

				/* continue in case a CNAME also exists */
				continue;

			  case T_CNAME:
				if (DontExpandCnames)
				{
					/* got CNAME -- guaranteed canonical */
					amatch = TRUE;
					break;
				}

				if (loopcnt++ > MAXCNAMEDEPTH)
				{
					/*XXX should notify postmaster XXX*/
					message("DNS failure: CNAME loop for %s",
						host);
					if (CurEnv->e_message == NULL)
					{
						char ebuf[MAXLINE];

						snprintf(ebuf, sizeof ebuf,
							"Deferred: DNS failure: CNAME loop for %.100s",
							host);
						CurEnv->e_message = newstr(ebuf);
					}
					h_errno = NO_RECOVERY;
					*statp = EX_CONFIG;
					return FALSE;
				}

				/* value points at name */
				if ((ret = dn_expand((u_char *)&answer,
				    eom, ap, (RES_UNC_T) nbuf, sizeof(nbuf))) < 0)
					break;
				(void)strncpy(host, nbuf, hbsize); /* XXX */
				host[hbsize - 1] = '\0';

				/*
				**  RFC 1034 section 3.6 specifies that CNAME
				**  should point at the canonical name -- but
				**  urges software to try again anyway.
				*/

				goto cnameloop;

			  default:
				/* not a record of interest */
				continue;
			}
		}

		if (amatch)
		{
			/* 
			**  Got a good match -- either an A, CNAME, or an
			**  exact MX record.  Save it and get out of here.
			*/

			mxmatch = *dp;
			break;
		}

		/*
		**  Nothing definitive yet.
		**	If this was a T_ANY query, we don't really know what
		**		was returned -- it might have been a T_NS,
		**		for example.  Try T_A to be more specific
		**		during the next pass.
		**	If this was a T_A query and we haven't yet found a MX
		**		match, try T_MX if allowed to do so.
		**	Otherwise, try the next domain.
		*/

		if (qtype == T_ANY)
			qtype = T_A;
		else if (qtype == T_A && !gotmx && trymx)
			qtype = T_MX;
		else
		{
			qtype = T_ANY;
			dp++;
		}
	}

	/* if nothing was found, we are done */
	if (mxmatch == NULL)
	{
		*statp = EX_NOHOST;
		return FALSE;
	}

	/*
	**  Create canonical name and return.
	**  If saved domain name is null, name was already canonical.
	**  Otherwise append the saved domain name.
	*/

	(void) snprintf(nbuf, sizeof nbuf, "%.*s%s%.*s", MAXDNAME, host,
			*mxmatch == '\0' ? "" : ".",
			MAXDNAME, mxmatch);
	strncpy(host, nbuf, hbsize);
	host[hbsize - 1] = '\0';
	if (tTd(8, 5))
		printf("dns_getcanonname: %s\n", host);
	*statp = EX_OK;
	return TRUE;
}



char *
gethostalias(host)
	char *host;
{
	char *fname;
	FILE *fp;
	register char *p = NULL;
	int sff = SFF_REGONLY;
	char buf[MAXLINE];
	static char hbuf[MAXDNAME];

	if (DontLockReadFiles)
		sff |= SFF_NOLOCK;
	fname = getenv("HOSTALIASES");
	if (fname == NULL ||
	    (fp = safefopen(fname, O_RDONLY, 0, sff)) == NULL)
		return NULL;
	while (fgets(buf, sizeof buf, fp) != NULL)
	{
		for (p = buf; p != '\0' && !(isascii(*p) && isspace(*p)); p++)
			continue;
		if (*p == 0)
		{
			/* syntax error */
			continue;
		}
		*p++ = '\0';
		if (strcasecmp(buf, host) == 0)
			break;
	}

	if (feof(fp))
	{
		/* no match */
		fclose(fp);
		return NULL;
	}
	fclose(fp);

	/* got a match; extract the equivalent name */
	while (*p != '\0' && isascii(*p) && isspace(*p))
		p++;
	host = p;
	while (*p != '\0' && !(isascii(*p) && isspace(*p)))
		p++;
	*p = '\0';
	strncpy(hbuf, host, sizeof hbuf - 1);
	hbuf[sizeof hbuf - 1] = '\0';
	return hbuf;
}

#endif /* NAMED_BIND */
