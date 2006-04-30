/*
 * arlib.c (C)opyright 1993 Darren Reed. All rights reserved.
 * This file may not be distributed without the author's permission in any
 * shape or form. The author takes no responsibility for any damage or loss
 * of property which results from the use of this software.
 */
#ifndef lint
static	char	sccsid[] = "@(#)arlib.c	1.9 6/5/93 (C)opyright 1992 Darren \
Reed. ASYNC DNS";
#endif

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "netdb.h"
#include "arpa/nameser.h"
#include <resolv.h>
#include "arlib.h"
#include "arplib.h"

extern	int	errno, h_errno;
static	char	ar_hostbuf[65], ar_domainname[65];
static	char	ar_dot[] = ".";
static	int	ar_resfd = -1, ar_vc = 0;
static	struct	reslist	*ar_last, *ar_first;

/*
 * Statistics structure.
 */
static	struct	resstats {
	int	re_errors;
	int	re_nu_look;
	int	re_na_look;
	int	re_replies;
	int	re_requests;
	int	re_resends;
	int	re_sent;
	int	re_timeouts;
} ar_reinfo;

static int do_query_name(/* struct resinfo *, char *, struct reslist * */);
static int do_query_number(/* struct resinfo *, char *, struct reslist * */);
static int ar_resend_query(/* struct reslist * */);

/*
 * ar_init
 *
 * Initializes the various ARLIB internal varilables and related DNS
 * options for res_init().
 *
 * Returns 0 or the socket opened for use with talking to name servers
 * if 0 is passed or ARES_INITSOCK is set.
 */
int	ar_init(op)
int	op;
{
	int	ret = 0;

	if (op & ARES_INITLIST)
	    {
		bzero(&ar_reinfo, sizeof(ar_reinfo));
		ar_first = ar_last = NULL;
	    }

	if (op & ARES_CALLINIT && !(_res.options & RES_INIT))
	    {
		ret = res_init();
		(void)strcpy(ar_domainname, ar_dot);
		(void)strncat(ar_domainname, _res.defdname,
				sizeof(ar_domainname)-2);
	    }

	if (op & ARES_INITSOCK)
		ret = ar_resfd = ar_open();

	if (op & ARES_INITDEBG)
		_res.options |= RES_DEBUG;

	if (op == 0)
		ret = ar_resfd;

	return ret;
}


/*
 * ar_open
 *
 * Open a socket to talk to a name server with.
 * Check _res.options to see if we use a TCP or UDP socket.
 */
int	ar_open()
{
	if (ar_resfd == -1)
	    {
		if (_res.options & RES_USEVC)
		    {
			struct	sockaddr_in	*sip;
			int	i;

			sip = _res.NS_ADDR_LIST;	/* was _res.nsaddr_list */
			ar_vc = 1;
			ar_resfd = socket(AF_INET, SOCK_STREAM, 0);

			/*
			 * Try each name server listed in sequence until we
			 * succeed or run out.
			 */
			while (connect(ar_resfd, (struct sockaddr *)sip++,
					sizeof(struct sockaddr)))
			    {
				(void)close(ar_resfd);
				ar_resfd = -1;
				if (i >= _res.nscount)
					break;
				ar_resfd = socket(AF_INET, SOCK_STREAM, 0);
			    }
		    }
		else
			ar_resfd = socket(AF_INET, SOCK_DGRAM, 0);
	    }
	if (ar_resfd >= 0)
	    {	/* Need one of these two here - and it MUST work!! */
		int flags;

		if ((flags = fcntl(ar_resfd, F_GETFL, 0)) != -1)
#ifdef	O_NONBLOCK
			 if (fcntl(ar_resfd, F_SETFL, flags|O_NONBLOCK) == -1)
#else
# ifdef	O_NDELAY
			 if (fcntl(ar_resfd, F_SETFL, flags|O_NDELAY) == -1)
# else
#  ifdef	FNDELAY
			 if (fcntl(ar_resfd, F_SETFL, flags|FNDELAY) == -1)
#  endif
# endif
#endif
		    {
			(void)close(ar_resfd);
			ar_resfd = -1;
		    }
	    }
	return ar_resfd;
}


/*
 * ar_close
 *
 * Closes and flags the ARLIB socket as closed.
 */
void	ar_close()
{
	(void)close(ar_resfd);
	ar_resfd = -1;
	return;
}


/*
 * ar_add_request
 *
 * Add a new DNS query to the end of the query list.
 */
static	int	ar_add_request(new)
struct	reslist *new;
{
	if (!new)
		return -1;
	if (!ar_first)
		ar_first = ar_last = new;
	else {
		ar_last->re_next = new;
		ar_last = new;
	}
	new->re_next = NULL;
	ar_reinfo.re_requests++;
	return 0;
}


/*
 * ar_remrequest
 *
 * Remove a request from the list. This must also free any memory that has
 * been allocated for temporary storage of DNS results.
 *
 * Returns -1 if there are anyy problems removing the requested structure
 * or 0 if the remove is successful.
 */
static	int	ar_remrequest(old)
struct	reslist *old;
{
	register struct	reslist	*rptr, *r2ptr;
	register char	**s;

	if (!old)
		return -1;
	for (rptr = ar_first, r2ptr = NULL; rptr; rptr = rptr->re_next)
	    {
		if (rptr == old)
			break;
		r2ptr = rptr;
	    }

	if (!rptr)
		return -1;
	if (rptr == ar_first)
		ar_first = ar_first->re_next;
	else if (rptr == ar_last)
	    {
		if (ar_last = r2ptr)
			ar_last->re_next = NULL;
	    }
	else
		r2ptr->re_next = rptr->re_next;

	if (!ar_first)
		ar_last = ar_first;

#ifdef	ARLIB_DEBUG
	ar_dump_hostent("ar_remrequest:", rptr->re_he);
#endif

	if (rptr->re_he.h_name)
		(void)free(rptr->re_he.h_name);
	if (s = rptr->re_he.h_aliases)
		for (; *s; s++)
			(void)free(*s);
	if (rptr->re_rinfo.ri_ptr)
		(void)free(rptr->re_rinfo.ri_ptr);
	(void)free(rptr);

	return 0;
}


/*
 * ar_make_request
 *
 * Create a DNS query recorded for the request being made and place it on the
 * current list awaiting replies.  Initialization of the record with set
 * values should also be done.
 */
static	struct	reslist	*ar_make_request(resi)
register struct	resinfo	*resi;
{
	register struct	reslist	*rptr;
	register struct resinfo *rp;

	rptr = (struct reslist *)calloc(1, sizeof(struct reslist));
	rp = &rptr->re_rinfo;

	rptr->re_next    = NULL; /* where NULL is non-zero ;) */
	rptr->re_sentat  = time(NULL);
	rptr->re_retries = _res.retry;
	rptr->re_sends = 1;
	rptr->re_resend  = 1;
	rptr->re_timeout = rptr->re_sentat + _res.retrans;
	rptr->re_he.h_name = NULL;
	rptr->re_he.h_addrtype   = AF_INET;
	rptr->re_he.h_aliases[0] = NULL;
	rp->ri_ptr = resi->ri_ptr;
	rp->ri_size = resi->ri_size;

	(void)ar_add_request(rptr);

	return rptr;
}


/*
 * ar_timeout
 *
 * Remove queries from the list which have been there too long without
 * being resolved.
 */
long	ar_timeout(now, info, size)
time_t	now;
char	*info;
int	size;
{
	register struct	reslist	*rptr, *r2ptr;
	register long	next = 0;

	for (rptr = ar_first, r2ptr = NULL; rptr; rptr = r2ptr)
	    {
		r2ptr = rptr->re_next;
		if (now >= rptr->re_timeout)
		    {
			/*
			 * If the timeout for the query has been exceeded,
			 * then resend the query if we still have some
			 * 'retry credit' and reset the timeout. If we have
			 * used it all up, then remove the request.
			 */
			if (--rptr->re_retries <= 0)
			    {
				ar_reinfo.re_timeouts++;
				if (info && rptr->re_rinfo.ri_ptr)
					bcopy(rptr->re_rinfo.ri_ptr, info,
						MIN(rptr->re_rinfo.ri_size,
						    size));
				(void)ar_remrequest(rptr);
				return now;
			    }
			else
			    {
				rptr->re_sends++;
				rptr->re_sentat = now;
				rptr->re_timeout = now + _res.retrans;
				(void)ar_resend_query(rptr);
			    }
		    }
		if (!next || rptr->re_timeout < next)
			next = rptr->re_timeout;
	    }
	return next;
}


/*
 * ar_send_res_msg
 *
 * When sending queries to nameservers listed in the resolv.conf file,
 * don't send a query to every one, but increase the number sent linearly
 * to match the number of resends. This increase only occurs if there are
 * multiple nameserver entries in the resolv.conf file.
 * The return value is the number of messages successfully sent to 
 * nameservers or -1 if no successful sends.
 */
static	int	ar_send_res_msg(msg, len, rcount)
char	*msg;
int	len, rcount;
{
	register int	i;
	int	sent = 0;

	if (!msg)
		return -1;

	rcount = (_res.nscount > rcount) ? rcount : _res.nscount;
	if (_res.options & RES_PRIMARY)
		rcount = 1;

	if (ar_vc)
	    {
		ar_reinfo.re_sent++;
		sent++;
		if (write(ar_resfd, msg, len) == -1)
		    {
			int errtmp = errno;
			(void)close(ar_resfd);
			errno = errtmp;
			ar_resfd = -1;
		    }
	    }
	else
		for (i = 0; i < rcount; i++)
		    {
			if (sendto(ar_resfd, msg, len, 0,
				   (struct sockaddr *)&(_res.NS_ADDR_LIST[i]),
				sizeof(struct sockaddr_in)) == len)
			    {
				ar_reinfo.re_sent++;
				sent++;
			    }
		    }
	return (sent) ? sent : -1;
}


/*
 * ar_find_id
 *
 * find a dns query record by the id (id is determined by dn_mkquery)
 */
static	struct	reslist	*ar_find_id(id)
int	id;
{
	register struct	reslist	*rptr;

	for (rptr = ar_first; rptr; rptr = rptr->re_next)
		if (rptr->re_id == id)
			return rptr;
	return NULL;
}


/*
 * ar_delete
 *
 * Delete a request from the waiting list if it has a data pointer which
 * matches the one passed.
 */
int	ar_delete(ptr, size)
char	*ptr;
int	size;
{
	register struct	reslist	*rptr;
	register struct	reslist	*r2ptr;
	int	removed = 0;

	for (rptr = ar_first; rptr; rptr = r2ptr)
	    {
		r2ptr = rptr->re_next;
		if (rptr->re_rinfo.ri_ptr && ptr && size &&
		    bcmp(rptr->re_rinfo.ri_ptr, ptr, size) == 0)
		    {
			(void)ar_remrequest(rptr);
			removed++;
		    }
	    }
	return removed;
}


/*
 * ar_query_name
 *
 * generate a query based on class, type and name.
 */
static	int	ar_query_name(name, class, type, rptr)
char	*name;
int	class, type;
struct	reslist	*rptr;
{
	static	char buf[MAXPACKET];
	int	r,s,a;
	HEADER	*hptr;

	bzero(buf, sizeof(buf));
	r = res_mkquery(QUERY, name, class, type, NULL, 0, NULL,
			buf, sizeof(buf));
	if (r <= 0)
	    {
		h_errno = NO_RECOVERY;
		return r;
	    }
	hptr = (HEADER *)buf;
	rptr->re_id = ntohs(hptr->id);

	s = ar_send_res_msg(buf, r, rptr->re_sends);

	if (s == -1)
	    {
		h_errno = TRY_AGAIN;
		return -1;
	    }
	else
		rptr->re_sent += s;
	return 0;
}


/*
 * ar_gethostbyname
 *
 * Replacement library function call to gethostbyname().  This one, however,
 * doesn't return the record being looked up but just places the query in the
 * queue to await answers.
 */
int	ar_gethostbyname(name, info, size)
char	*name;
char	*info;
int	size;
{
	char	host[65];
	struct	resinfo	resi;
	register struct resinfo *rp = &resi;

	if (size && info)
	    {
		rp->ri_ptr = (char *)malloc(size);
		bcopy(info, rp->ri_ptr, size);
		rp->ri_size = size;
	    }
	else
		bzero((char *)rp, sizeof(resi));
	ar_reinfo.re_na_look++;
	(void)strncpy(host, name, 64);
	host[64] = '\0';

	return (do_query_name(rp, host, NULL));
}


static	int	do_query_name(resi, name, rptr)
struct	resinfo	*resi;
char	*name;
register struct	reslist	*rptr;
{
	char	hname[65];
	int	len;

	len = strlen((char *)strncpy(hname, name, sizeof(hname)-1));

	if (rptr && (hname[len-1] != '.'))
	    {
		(void)strncat(hname, ar_dot, sizeof(hname)-len-1);
		/*
		 * NOTE: The logical relationship between DNSRCH and DEFNAMES
		 * is implies. ie no DEFNAES, no DNSRCH.
		 */
		if (_res.options & (RES_DEFNAMES|RES_DNSRCH) ==
		    (RES_DEFNAMES|RES_DNSRCH))
		    {
			if (_res.dnsrch[rptr->re_srch])
				(void)strncat(hname, _res.dnsrch[rptr->re_srch],
					sizeof(hname) - ++len -1);
		    }
		else if (_res.options & RES_DEFNAMES)
			(void)strncat(hname, ar_domainname, sizeof(hname) - len -1);
	    }

	/*
	 * Store the name passed as the one to lookup and generate other host
	 * names to pass onto the nameserver(s) for lookups.
	 */
	if (!rptr)
	    {
		rptr = ar_make_request(resi);
		rptr->re_type = T_A;
		(void)strncpy(rptr->re_name, name, sizeof(rptr->re_name)-1);
	    }
	return (ar_query_name(hname, C_IN, T_A, rptr));
}


/*
 * ar_gethostbyaddr
 *
 * Generates a query for a given IP address.
 */
int	ar_gethostbyaddr(addr, info, size)
char	*addr;
char	*info;
int	size;
{
	struct	resinfo	resi;
	register struct resinfo *rp = &resi;

	if (size && info)
	    {
		rp->ri_ptr = (char *)malloc(size);
		bcopy(info, rp->ri_ptr, size);
		rp->ri_size = size;
	    }
	else
		bzero((char *)rp, sizeof(resi));
	ar_reinfo.re_nu_look++;
	return (do_query_number(rp, addr, NULL));
}


/*
 * do_query_number
 *
 * Use this to do reverse IP# lookups.
 */
static	int	do_query_number(resi, numb, rptr)
struct	resinfo	*resi;
char	*numb;
register struct	reslist	*rptr;
{
	register unsigned char	*cp;
	static	char	ipbuf[32];

	/*
	 * Generate name in the "in-addr.arpa" domain.  No addings bits to this
	 * name to get more names to query!.
	 */
	cp = (unsigned char *)numb;
	(void)sprintf(ipbuf,"%u.%u.%u.%u.in-addr.arpa.",
			(unsigned int)(cp[3]), (unsigned int)(cp[2]),
			(unsigned int)(cp[1]), (unsigned int)(cp[0]));

	if (!rptr)
	    {
		rptr = ar_make_request(resi);
		rptr->re_type = T_PTR;
		rptr->re_he.h_length = sizeof(struct in_addr);
		bcopy(numb, (char *)&rptr->re_addr, rptr->re_he.h_length);
		bcopy(numb, (char *)&rptr->re_he.h_addr_list[0].s_addr,
			rptr->re_he.h_length);
	    }
	return (ar_query_name(ipbuf, C_IN, T_PTR, rptr));
}


/*
 * ar_resent_query
 *
 * resends a query.
 */
static	int	ar_resend_query(rptr)
struct	reslist	*rptr;
{
	if (!rptr->re_resend)
		return -1;

	switch(rptr->re_type)
	{
	case T_PTR:
		ar_reinfo.re_resends++;
		return do_query_number(NULL, &rptr->re_addr, rptr);
	case T_A:
		ar_reinfo.re_resends++;
		return do_query_name(NULL, rptr->re_name, rptr);
	default:
		break;
	}

	return -1;
}


/*
 * ar_procanswer
 *
 * process an answer received from a nameserver.
 */
static	int	ar_procanswer(rptr, hptr, buf, eob)
struct	reslist	*rptr;
char	*buf, *eob;
HEADER	*hptr;
{
	char	*cp, **alias, *s;
	int	class, type, dlen, len, ans = 0, n, i;
	u_int32_t ttl, dr, *adr;
	struct	hent	*hp;

	cp = buf + sizeof(HEADER);
	adr = (u_int32_t *)rptr->re_he.h_addr_list;

	while (*adr)
		adr++;

	alias = rptr->re_he.h_aliases;
	while (*alias)
		alias++;

	hp = &rptr->re_he;


	/*
	 * Skip over the original question.
	 */
	while (hptr->qdcount-- > 0)
		cp += dn_skipname(cp, eob) + QFIXEDSZ;
	/*
	 * proccess each answer sent to us. blech.
	 */
	while (hptr->ancount-- > 0 && cp < eob) {
		n = dn_expand(buf, eob, cp, ar_hostbuf, sizeof(ar_hostbuf));
		cp += n;
		if (n <= 0)
			return ans;

		ans++;
		/*
		 * 'skip' past the general dns crap (ttl, class, etc) to get
		 * the pointer to the right spot.  Some of thse are actually
		 * useful so its not a good idea to skip past in one big jump.
		 */
		type = (int)_getshort(cp);
		cp += sizeof(short);
		class = (int)_getshort(cp);
		cp += sizeof(short);
		ttl = (u_int32_t)_getlong(cp);
		cp += sizeof(u_int32_t);
		dlen =  (int)_getshort(cp);
		cp += sizeof(short);
		rptr->re_type = type;

		switch(type)
		{
		case T_A :
			rptr->re_he.h_length = dlen;
			if (ans == 1)
				rptr->re_he.h_addrtype=(class == C_IN) ?
							AF_INET : AF_UNSPEC;
			if (dlen != sizeof(dr))
			    {
				h_errno = TRY_AGAIN;
				continue;
			    }
			bcopy(cp, &dr, dlen);
			*adr++ = dr;
			*adr = 0;
			cp += dlen;
			len = strlen(ar_hostbuf);
			if (!rptr->re_he.h_name)
			    {
				rptr->re_he.h_name = (char *)malloc(len+1);
				if (!rptr->re_he.h_name)
					break;
				(void)strcpy(rptr->re_he.h_name, ar_hostbuf);
			    }
 			break;
		case T_PTR :
			if ((n = dn_expand(buf, eob, cp, ar_hostbuf,
					   sizeof(ar_hostbuf) )) < 0)
			    {
				cp += n;
				continue;
			    }
			cp += n;
			len = strlen(ar_hostbuf)+1;
			/*
			 * copy the returned hostname into the host name
			 * or alias field if there is a known hostname
			 * already.
			 */
			if (!rptr->re_he.h_name)
			    {
				rptr->re_he.h_name = (char *)malloc(len);
				if (!rptr->re_he.h_name)
					break;
				(void)strcpy(rptr->re_he.h_name, ar_hostbuf);
			    }
			else
			    {
				*alias = (char *)malloc(len);
				if (!*alias)
					return -1;
				(void)strcpy(*alias++, ar_hostbuf);
				*alias = NULL;
			    }
			break;
		case T_CNAME :
			cp += dlen;
			if (alias >= &(rptr->re_he.h_aliases[MAXALIASES-1]))
				continue;
			n = strlen(ar_hostbuf)+1;
			*alias = (char *)malloc(n);
			if (!*alias)
				return -1;
			(void)strcpy(*alias++, ar_hostbuf);
			*alias = NULL;
			break;
		default :
			break;
		}
	}

	return ans;
}


/*
 * ar_answer
 *
 * Get an answer from a DNS server and process it.  If a query is found to
 * which no answer has been given to yet, copy its 'info' structure back
 * to where "reip" points and return a pointer to the hostent structure.
 */
struct	hostent	*ar_answer(reip, size)
char	*reip;
int	size;
{
	static	char	ar_rcvbuf[sizeof(HEADER) + MAXPACKET];
	static	struct	hostent	ar_host;

	register HEADER	*hptr;
	register struct	reslist	*rptr = NULL;
	register struct hostent *hp;
	register char **s;
	unsigned long	*adr;
	int	rc, i, n, a;

	rc = recv(ar_resfd, ar_rcvbuf, sizeof(ar_rcvbuf), 0);
	if (rc <= 0)
		goto getres_err;

	ar_reinfo.re_replies++;
	hptr = (HEADER *)ar_rcvbuf;
	/*
	 * convert things to be in the right order.
	 */
	hptr->id = ntohs(hptr->id);
	hptr->ancount = ntohs(hptr->ancount);
	hptr->arcount = ntohs(hptr->arcount);
	hptr->nscount = ntohs(hptr->nscount);
	hptr->qdcount = ntohs(hptr->qdcount);
	/*
	 * response for an id which we have already received an answer for
	 * just ignore this response.
	 */
	rptr = ar_find_id(hptr->id);
	if (!rptr)
		goto getres_err;

	if ((hptr->rcode != NOERROR) || (hptr->ancount == 0))
	    {
		switch (hptr->rcode)
		{
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
		ar_reinfo.re_errors++;
		/*
		** If a bad error was returned, we stop here and dont send
		** send any more (no retries granted).
		*/
		if (h_errno != TRY_AGAIN)
		    {
			rptr->re_resend = 0;
			rptr->re_retries = 0;
		    }
		goto getres_err;
	    }

	a = ar_procanswer(rptr, hptr, ar_rcvbuf, ar_rcvbuf+rc);

	if ((rptr->re_type == T_PTR) && (_res.options & RES_CHECKPTR))
	    {
		/*
		 * For reverse lookups on IP#'s, lookup the name that is given
		 * for the ip# and return with that as the official result.
		 * -avalon
		 */
		rptr->re_type = T_A;
		/*
		 * Clean out the list of addresses already set, even though
		 * there should only be one :)
		 */
		adr = (unsigned long *)rptr->re_he.h_addr_list;
		while (*adr)
			*adr++ = 0L;
		/*
		 * Lookup the name that we were given for the ip#
		 */
		ar_reinfo.re_na_look++;
		(void)strncpy(rptr->re_name, rptr->re_he.h_name,
			sizeof(rptr->re_name)-1);
		rptr->re_he.h_name = NULL;
		rptr->re_retries = _res.retry;
		rptr->re_sends = 1;
		rptr->re_resend = 1;
		rptr->re_he.h_name = NULL;
		ar_reinfo.re_na_look++;
		(void)ar_query_name(rptr->re_name, C_IN, T_A, rptr);
		return NULL;
	    }

	if (reip && rptr->re_rinfo.ri_ptr && size)
		bcopy(rptr->re_rinfo.ri_ptr, reip,
			MIN(rptr->re_rinfo.ri_size, size));
	/*
	 * Clean up structure from previous usage.
	 */
	hp = &ar_host;
#ifdef	ARLIB_DEBUG
	ar_dump_hostent("ar_answer: previous usage", hp);
#endif

	if (hp->h_name)
		(void)free(hp->h_name);
	if (s = hp->h_aliases)
	    {
		while (*s)
			(void)free(*s++);
		(void)free(hp->h_aliases);
	    }
	if (s = hp->h_addr_list)
	    {
		/*
		 * Only free once since we allocated space for
		 * address in one big chunk.
		 */
		(void)free(*s);
		(void)free(hp->h_addr_list);
	    }
	bzero((char *)hp, sizeof(*hp));

	/*
	 * Setup and copy details for the structure we return a pointer to.
	 */
	hp->h_addrtype = AF_INET;
	hp->h_length = sizeof(struct in_addr);
	if(rptr->re_he.h_name)
	    {
		hp->h_name = (char *)malloc(strlen(rptr->re_he.h_name)+1);
		if(!hp->h_name)
		    {
#ifdef	ARLIB_DEBUG
			fprintf(stderr, "no memory for hostname\n");
#endif
			h_errno = TRY_AGAIN;
			goto getres_err;
		    }
		(void)strcpy(hp->h_name, rptr->re_he.h_name);
	    }
#ifdef	ARLIB_DEBUG
	ar_dump_hostent("ar_answer: (snap) store name", hp);
#endif

	/*
	 * Count IP#'s.
	 */
	for (i = 0, n = 0; i < MAXADDRS; i++, n++)
		if (!rptr->re_he.h_addr_list[i].s_addr)
			break;
	s = hp->h_addr_list = (char **)malloc((n + 1) * sizeof(char *));
	if (n)
	    {
		*s = (char *)malloc(n * sizeof(struct in_addr));
		if(!*s)
		    {
#ifdef	ARLIB_DEBUG
			fprintf(stderr, "no memory for IP#'s (%d)\n", n);
#endif
			h_errno = TRY_AGAIN;
			goto getres_err;
		    }
		bcopy((char *)&rptr->re_he.h_addr_list[0].s_addr, *s,
			sizeof(struct in_addr));
		s++;
		for (i = 1; i < n; i++, s++)
		    {
			*s = hp->h_addr + i * sizeof(struct in_addr);
			bcopy((char *)&rptr->re_he.h_addr_list[i].s_addr, *s,
				sizeof(struct in_addr));
		    }
	    }
	*s = NULL;
#ifdef	ARLIB_DEBUG
	ar_dump_hostent("ar_answer: (snap) store IP#'s", hp);
#endif

	/*
	 * Count CNAMEs
	 */
	for (i = 0, n = 0; i < MAXADDRS; i++, n++)
		if (!rptr->re_he.h_aliases[i])
			break;
	s = hp->h_aliases = (char **)malloc((n + 1) * sizeof(char *));
	if (!s)
	    {
#ifdef	ARLIB_DEBUG
		fprintf(stderr, "no memory for aliases (%d)\n", n);
#endif
		h_errno = TRY_AGAIN;
		goto getres_err;
	    }
	for (i = 0; i < n; i++)
	    {
		*s++ = rptr->re_he.h_aliases[i];
		rptr->re_he.h_aliases[i] = NULL;
	    }
	*s = NULL;
#ifdef	ARLIB_DEBUG
	ar_dump_hostent("ar_answer: (snap) store CNAMEs", hp);
	ar_dump_hostent("ar_answer: new one", hp);
#endif

	if (a > 0)
		(void)ar_remrequest(rptr);
	else
		if (!rptr->re_sent)
			(void)ar_remrequest(rptr);
	return hp;

getres_err:
	if (rptr)
	    {
		if (reip && rptr->re_rinfo.ri_ptr && size)
			bcopy(rptr->re_rinfo.ri_ptr, reip,
				MIN(rptr->re_rinfo.ri_size, size));
		if ((h_errno != TRY_AGAIN) &&
		    (_res.options & (RES_DNSRCH|RES_DEFNAMES) ==
		     (RES_DNSRCH|RES_DEFNAMES) ))
			if (_res.dnsrch[rptr->re_srch])
			    {
				rptr->re_retries = _res.retry;
				rptr->re_sends = 1;
				rptr->re_resend = 1;
				(void)ar_resend_query(rptr);
				rptr->re_srch++;
			    }
		return NULL;
	    }
	return NULL;
}


#ifdef	ARLIB_DEBUG
void ar_dump_hostent(prefix, hp)
char *prefix;
struct hostent *hp;
{
	register char **s;

	fflush(stdout);

	fprintf(stderr, "%s\n", prefix);
	fprintf(stderr, "  hp %p\n", hp);
	fprintf(stderr, "    h_name %p '%s'\n",
	hp->h_name, hp->h_name);
	if (s = hp->h_aliases)
	    {
		fprintf(stderr, "    h_aliases %p\n",
		hp->h_aliases);
		while (*s)
		    {
			fprintf(stderr, "      element %p\n", *s);
			s++;
		    }
	    }
	if (s = hp->h_addr_list)
	    {
		fprintf(stderr, "    h_addr_list %p\n",
		hp->h_addr_list);
		while (*s)
		    {
			fprintf(stderr, "      element %p\n", *s);
			s++;
		    }
	    }

	fflush(stderr);
}


void ar_dump_reslist(FILE* fp)
{
	register struct reslist *rptr;
	int c;

	c = 0;
	for (rptr = ar_first; rptr; rptr = rptr->re_next)
	    {
		fprintf(fp, "%4d [%p] %4d [%p]: %s\n", rptr->re_id, rptr,
			*(rptr->re_rinfo.ri_ptr), rptr->re_rinfo.ri_ptr,
			rptr->re_name);
	    }
}
#endif
