#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: res_findzonecut.c,v 1.14.2.1 2001/05/17 20:47:35 mellon Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1999 by Internet Software Consortium.
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

/* Import. */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <isc-dhcp/list.h>

#include "minires/minires.h"
#include "arpa/nameser.h"

/* Data structures. */

typedef struct rr_a {
	ISC_LINK(struct rr_a)	link;
	struct in_addr		addr;
} rr_a;
typedef ISC_LIST(rr_a) rrset_a;

typedef struct rr_ns {
	ISC_LINK(struct rr_ns) link;
	char *name;
	rrset_a addrs;
} rr_ns;
typedef ISC_LIST(rr_ns) rrset_ns;

/* Forward. */

static int	satisfy(res_state,
			const char *, rrset_ns *, struct in_addr *, int);
static int	add_addrs(res_state, rr_ns *, struct in_addr *, int);
static ns_rcode	get_soa(res_state, const char *, ns_class,
			char *, size_t, char *, size_t,
			rrset_ns *);
static isc_result_t get_ns(res_state, const char *, ns_class, rrset_ns *);
static isc_result_t get_glue(res_state, ns_class, rrset_ns *);
static isc_result_t save_ns(res_state, ns_msg *, ns_sect,
			    const char *, ns_class, rrset_ns *);
static isc_result_t save_a(res_state, ns_msg *, ns_sect,
			   const char *, ns_class, rrset_a *);
static void	free_nsrrset(rrset_ns *);
static void	free_nsrr(rrset_ns *, rr_ns *);
static rr_ns *	find_ns(rrset_ns *, const char *);
static isc_result_t do_query(res_state, const char *, ns_class, ns_type,
			     double *, ns_msg *, int *);

/* Public. */

/*
 * int
 * res_findzonecut(res, dname, class, zname, zsize, addrs, naddrs)
 *	find enclosing zone for a <dname,class>, and some server addresses
 * parameters:
 *	res - resolver context to work within (is modified)
 *	dname - domain name whose enclosing zone is desired
 *	class - class of dname (and its enclosing zone)
 *	zname - found zone name
 *	zsize - allocated size of zname
 *	addrs - found server addresses
 *	naddrs - max number of addrs
 * return values:
 *	< 0 - an error occurred (check errno)
 *	= 0 - zname is now valid, but addrs[] wasn't changed
 *	> 0 - zname is now valid, and return value is number of addrs[] found
 * notes:
 *	this function calls res_nsend() which means it depends on correctly
 *	functioning recursive nameservers (usually defined in /etc/resolv.conf
 *	or its local equivilent).
 *
 *	we start by asking for an SOA<dname,class>.  if we get one as an
 *	answer, that just means <dname,class> is a zone top, which is fine.
 *	more than likely we'll be told to go pound sand, in the form of a
 *	negative answer.
 *
 *	note that we are not prepared to deal with referrals since that would
 *	only come from authority servers and our correctly functioning local
 *	recursive server would have followed the referral and got us something
 *	more definite.
 *
 *	if the authority section contains an SOA, this SOA should also be the
 *	closest enclosing zone, since any intermediary zone cuts would've been
 *	returned as referrals and dealt with by our correctly functioning local
 *	recursive name server.  but an SOA in the authority section should NOT
 *	match our dname (since that would have been returned in the answer
 *	section).  an authority section SOA has to be "above" our dname.
 *
 *	we cannot fail to find an SOA in this way.  ultimately we'll return
 *	a zname indicating the root zone if that's the closest enclosing zone.
 *	however, since authority section SOA's were once optional, it's
 *	possible that we'll have to go hunting for the enclosing SOA by
 *	ripping labels off the front of our dname -- this is known as "doing
 *	it the hard way."
 *
 *	ultimately we want some server addresses, which are ideally the ones
 *	pertaining to the SOA.MNAME, but only if there is a matching NS RR.
 *	so the second phase (after we find an SOA) is to go looking for the
 *	NS RRset for that SOA's zone.
 *
 *	no answer section processed by this code is allowed to contain CNAME
 *	or DNAME RR's.  for the SOA query this means we strip a label and
 *	keep going.  for the NS and A queries this means we just give up.
 */

isc_result_t
res_findzonecut(res_state statp, const char *dname, ns_class class, int opts,
		char *zname, size_t zsize, struct in_addr *addrs, int naddrs,
		int *count, void *zcookie)
{
	char mname[NS_MAXDNAME];
	u_long save_pfcode;
	rrset_ns nsrrs;
	int n = 0;
	isc_result_t rcode;

	DPRINTF(("START dname='%s' class=%s, zsize=%ld, naddrs=%d",
		 dname, p_class(class), (long)zsize, naddrs));
	save_pfcode = statp->pfcode;
	statp->pfcode |= RES_PRF_HEAD2 | RES_PRF_HEAD1 | RES_PRF_HEADX |
			 RES_PRF_QUES | RES_PRF_ANS |
			 RES_PRF_AUTH | RES_PRF_ADD;
	ISC_LIST_INIT(nsrrs);

	DPRINTF (("look for a predefined zone statement"));
	rcode = find_cached_zone (dname, class, zname, zsize,
				  addrs, naddrs, &n, zcookie);
	if (rcode == ISC_R_SUCCESS)
		goto done;

	DPRINTF(("get the soa, and see if it has enough glue"));
	if ((rcode = get_soa(statp, dname, class, zname, zsize,
			     mname, sizeof mname, &nsrrs)) != ISC_R_SUCCESS ||
	    ((opts & RES_EXHAUSTIVE) == 0 &&
	     (n = satisfy(statp, mname, &nsrrs, addrs, naddrs)) > 0))
		goto done;

	DPRINTF(("get the ns rrset and see if it has enough glue"));
	if ((rcode = get_ns(statp, zname, class, &nsrrs)) != ISC_R_SUCCESS ||
	    ((opts & RES_EXHAUSTIVE) == 0 &&
	     (n = satisfy(statp, mname, &nsrrs, addrs, naddrs)) > 0))
		goto done;

	DPRINTF(("get the missing glue and see if it's finally enough"));
	if ((rcode = get_glue(statp, class, &nsrrs)) == ISC_R_SUCCESS)
		n = satisfy(statp, mname, &nsrrs, addrs, naddrs);

	/* If we found the zone, cache it. */
	if (n > 0)
		cache_found_zone (class, zname, addrs, n);
 done:
	DPRINTF(("FINISH n=%d (%s)", n, (n < 0) ? strerror(errno) : "OK"));
	free_nsrrset(&nsrrs);
	statp->pfcode = save_pfcode;
	if (count)
		*count = n;
	return rcode;
}

/* Private. */

static int
satisfy(res_state statp,
	const char *mname, rrset_ns *nsrrsp, struct in_addr *addrs, int naddrs)
{
	rr_ns *nsrr;
	int n, x;

	n = 0;
	nsrr = find_ns(nsrrsp, mname);
	if (nsrr != NULL) {
		x = add_addrs(statp, nsrr, addrs, naddrs);
		addrs += x;
		naddrs -= x;
		n += x;
	}
	for (nsrr = ISC_LIST_HEAD(*nsrrsp);
	     nsrr != NULL && naddrs > 0;
	     nsrr = ISC_LIST_NEXT(nsrr, link))
		if (ns_samename(nsrr->name, mname) != 1) {
			x = add_addrs(statp, nsrr, addrs, naddrs);
			addrs += x;
			naddrs -= x;
			n += x;
		}
	DPRINTF(("satisfy(%s): %d", mname, n));
	return (n);
}

static int
add_addrs(res_state statp, rr_ns *nsrr, struct in_addr *addrs, int naddrs) {
	rr_a *arr;
	int n = 0;

	for (arr = ISC_LIST_HEAD(nsrr->addrs);
	     arr != NULL; arr = ISC_LIST_NEXT(arr, link)) {
		if (naddrs <= 0)
			return (0);
		*addrs++ = arr->addr;
		naddrs--;
		n++;
	}
	DPRINTF(("add_addrs: %d", n));
	return (n);
}

static ns_rcode
get_soa(res_state statp, const char *dname, ns_class class,
	char *zname, size_t zsize, char *mname, size_t msize,
	rrset_ns *nsrrsp)
{
	char tname[NS_MAXDNAME];
	double resp[NS_PACKETSZ / sizeof (double)];
	int n, i, ancount, nscount;
	ns_sect sect;
	ns_msg msg;
	u_int rcode;
	isc_result_t status;

	/*
	 * Find closest enclosing SOA, even if it's for the root zone.
	 */

	/* First canonicalize dname (exactly one unescaped trailing "."). */
	status = ns_makecanon(dname, tname, sizeof tname);
	if (status != ISC_R_SUCCESS)
		return status;
	dname = tname;

	/* Now grovel the subdomains, hunting for an SOA answer or auth. */
	for (;;) {
		/* Leading or inter-label '.' are skipped here. */
		while (*dname == '.')
			dname++;

		/* Is there an SOA? */
		rcode = do_query(statp, dname, class, ns_t_soa,
				 resp, &msg, &n);
		if (rcode != ISC_R_SUCCESS) {
			DPRINTF(("get_soa: do_query('%s', %s) failed (%d)",
				 dname, p_class(class), n));
			return rcode;
		}
		if (n > 0) {
			DPRINTF(("get_soa: CNAME or DNAME found"));
			sect = ns_s_max, n = 0;
		} else {
			ancount = ns_msg_count(msg, ns_s_an);
			nscount = ns_msg_count(msg, ns_s_ns);
			if (ancount > 0 && rcode == ISC_R_SUCCESS)
				sect = ns_s_an, n = ancount;
			else if (nscount > 0)
				sect = ns_s_ns, n = nscount;
			else
				sect = ns_s_max, n = 0;
		}
		for (i = 0; i < n; i++) {
			const char *t;
			const u_char *rdata;
			int rdlen;
			ns_rr rr;

			rcode = ns_parserr(&msg, sect, i, &rr) < 0;
			if (rcode != ISC_R_SUCCESS) {
				DPRINTF(("get_soa: ns_parserr(%s, %d) failed",
					 p_section(sect, ns_o_query), i));
				return rcode;
			}
			if (ns_rr_type(rr) == ns_t_cname ||
			    ns_rr_type(rr) == ns_t_dname)
				break;
			if (ns_rr_type(rr) != ns_t_soa ||
			    ns_rr_class(rr) != class)
				continue;
			t = ns_rr_name(rr);
			switch (sect) {
			case ns_s_an:
				if (ns_samedomain(dname, t) == 0) {
					DPRINTF(("get_soa: %s'%s', '%s') == 0",
						 "ns_samedomain(", dname, t));
					return ISC_R_NOTZONE;
				}
				break;
			case ns_s_ns:
				if (ns_samename(dname, t) == 1 ||
				    ns_samedomain(dname, t) == 0) {
					DPRINTF(("get_soa: %smain('%s', '%s')",
						 "ns_samename() || !ns_samedo",
						 dname, t));
					return ISC_R_NOTZONE;
				}
				break;
			default:
				abort();
			}
			if (strlen(t) + 1 > zsize) {
				DPRINTF(("get_soa: zname(%d) too small (%d)",
					 zsize, strlen(t) + 1));
				return ISC_R_NOSPACE;
			}
			strcpy(zname, t);
			rdata = ns_rr_rdata(rr);
			rdlen = ns_rr_rdlen(rr);
			if (ns_name_uncompress((u_char *)resp,
					       ns_msg_end(msg), rdata,
					       mname, msize) < 0) {
				DPRINTF(("get_soa: %s failed",
					 "ns_name_uncompress"));
				return ISC_R_NOMEMORY;
			}
			rcode = save_ns(statp, &msg,
					ns_s_ns, zname, class, nsrrsp);
			if (rcode != ISC_R_SUCCESS) {
				DPRINTF(("get_soa: save_ns failed"));
				return rcode;
			}
			return ISC_R_SUCCESS;
		}

		/* If we're out of labels, then not even "." has an SOA! */
		if (*dname == '\0')
			break;

		/* Find label-terminating "."; top of loop will skip it. */
		while (*dname != '.') {
			if (*dname == '\\')
				if (*++dname == '\0') {
					ISC_R_NOSPACE;
				}
			dname++;
		}
	}
	DPRINTF(("get_soa: out of labels"));
	return ISC_R_DESTADDRREQ;
}

static isc_result_t
get_ns(res_state statp, const char *zname, ns_class class, rrset_ns *nsrrsp) {
	double resp[NS_PACKETSZ / sizeof (double)];
	ns_msg msg;
	int n;
	isc_result_t rcode;

	/* Go and get the NS RRs for this zone. */
	rcode = do_query(statp, zname, class, ns_t_ns, resp, &msg, &n);
	if (rcode != ISC_R_SUCCESS) {
		DPRINTF(("get_ns: do_query('zname', %s) failed (%d)",
			 zname, p_class(class), rcode));
		return rcode;
	}

	/* Remember the NS RRs and associated A RRs that came back. */
	rcode = save_ns(statp, &msg, ns_s_an, zname, class, nsrrsp);
	if (rcode != ISC_R_SUCCESS) {
		DPRINTF(("get_ns save_ns('%s', %s) failed",
			 zname, p_class(class)));
		return rcode;
	}

	return ISC_R_SUCCESS;
}

static isc_result_t
get_glue(res_state statp, ns_class class, rrset_ns *nsrrsp) {
	rr_ns *nsrr, *nsrr_n;

	/* Go and get the A RRs for each empty NS RR on our list. */
	for (nsrr = ISC_LIST_HEAD(*nsrrsp); nsrr != NULL; nsrr = nsrr_n) {
		double resp[NS_PACKETSZ / sizeof (double)];
		ns_msg msg;
		int n;
		isc_result_t rcode;

		nsrr_n = ISC_LIST_NEXT(nsrr, link);

		if (ISC_LIST_EMPTY(nsrr->addrs)) {
			rcode = do_query(statp, nsrr->name, class, ns_t_a,
					 resp, &msg, &n);
			if (rcode != ISC_R_SUCCESS) {
			    DPRINTF(("get_glue: do_query('%s', %s') failed",
				     nsrr->name, p_class(class)));
			    return rcode;
			}
			if (n > 0) {
				DPRINTF((
			"get_glue: do_query('%s', %s') CNAME or DNAME found",
					 nsrr->name, p_class(class)));
			}
			rcode = save_a(statp, &msg, ns_s_an, nsrr->name, class,
				       &nsrr->addrs);
			if (rcode != ISC_R_SUCCESS) {
				DPRINTF(("get_glue: save_r('%s', %s) failed",
					 nsrr->name, p_class(class)));
				return rcode;
			}
			/* If it's still empty, it's just chaff. */
			if (ISC_LIST_EMPTY(nsrr->addrs)) {
				DPRINTF(("get_glue: removing empty '%s' NS",
					 nsrr->name));
				free_nsrr(nsrrsp, nsrr);
			}
		}
	}
	return ISC_R_SUCCESS;
}

static isc_result_t
save_ns(res_state statp, ns_msg *msg, ns_sect sect,
	const char *owner, ns_class class,
	rrset_ns *nsrrsp)
{
	int i;
	isc_result_t rcode;

	for (i = 0; i < ns_msg_count(*msg, sect); i++) {
		char tname[MAXDNAME];
		const u_char *rdata;
		rr_ns *nsrr;
		ns_rr rr;
		int rdlen;

		rcode = ns_parserr(msg, sect, i, &rr);
		if (rcode != ISC_R_SUCCESS) {
			DPRINTF(("save_ns: ns_parserr(%s, %d) failed",
				 p_section(sect, ns_o_query), i));
			return rcode;
		}
		if (ns_rr_type(rr) != ns_t_ns ||
		    ns_rr_class(rr) != class ||
		    ns_samename(ns_rr_name(rr), owner) != 1)
			continue;
		nsrr = find_ns(nsrrsp, ns_rr_name(rr));
		if (nsrr == NULL) {
			nsrr = malloc(sizeof *nsrr);
			if (nsrr == NULL) {
				DPRINTF(("save_ns: malloc failed"));
				return ISC_R_NOMEMORY;
			}
			rdata = ns_rr_rdata(rr);
			rdlen = ns_rr_rdlen(rr);
			if (ns_name_uncompress(ns_msg_base(*msg),
					       ns_msg_end(*msg), rdata,
					       tname, sizeof tname) < 0) {
				DPRINTF(("save_ns: ns_name_uncompress failed"));
				free(nsrr);
				return ISC_R_NOMEMORY;
			}
			nsrr->name = strdup(tname);
			if (nsrr->name == NULL) {
				DPRINTF(("save_ns: strdup failed"));
				free(nsrr);
				return ISC_R_NOMEMORY;
			}
			ISC_LIST_INIT(nsrr->addrs);
			ISC_LIST_APPEND(*nsrrsp, nsrr, link);
		}
		rcode = save_a(statp, msg, ns_s_ar,
			       nsrr->name, class, &nsrr->addrs);
		if (rcode != ISC_R_SUCCESS) {
			DPRINTF(("save_ns: save_r('%s', %s) failed",
				 nsrr->name, p_class(class)));
			return rcode;
		}
	}
	return ISC_R_SUCCESS;
}

static isc_result_t
save_a(res_state statp, ns_msg *msg, ns_sect sect,
       const char *owner, ns_class class,
       rrset_a *arrsp)
{
	int i;
	isc_result_t rcode;

	for (i = 0; i < ns_msg_count(*msg, sect); i++) {
		ns_rr rr;
		rr_a *arr;

		rcode = ns_parserr(msg, sect, i, &rr);
		if (rcode != ISC_R_SUCCESS) {
			DPRINTF(("save_a: ns_parserr(%s, %d) failed",
				 p_section(sect, ns_o_query), i));
			return rcode;
		}
		if (ns_rr_type(rr) != ns_t_a ||
		    ns_rr_class(rr) != class ||
		    ns_samename(ns_rr_name(rr), owner) != 1 ||
		    ns_rr_rdlen(rr) != NS_INADDRSZ)
			continue;
		arr = malloc(sizeof *arr);
		if (arr == NULL) {
			DPRINTF(("save_a: malloc failed"));
			return ISC_R_NOMEMORY;
		}
		memcpy(&arr->addr, ns_rr_rdata(rr), NS_INADDRSZ);
		ISC_LIST_APPEND(*arrsp, arr, link);
	}
	return ISC_R_SUCCESS;
}

static void
free_nsrrset(rrset_ns *nsrrsp) {
	rr_ns *nsrr;

	while ((nsrr = ISC_LIST_HEAD(*nsrrsp)) != NULL)
		free_nsrr(nsrrsp, nsrr);
}

static void
free_nsrr(rrset_ns *nsrrsp, rr_ns *nsrr) {
	rr_a *arr;

	while ((arr = ISC_LIST_HEAD(nsrr->addrs)) != NULL) {
		ISC_LIST_UNLINK(nsrr->addrs, arr, link);
		free(arr);
	}
	free((char *)nsrr->name);
	ISC_LIST_UNLINK(*nsrrsp, nsrr, link);
	free(nsrr);
}

static rr_ns *
find_ns(rrset_ns *nsrrsp, const char *dname) {
	rr_ns *nsrr;

	for (nsrr = ISC_LIST_HEAD(*nsrrsp);
	     nsrr != NULL; nsrr = ISC_LIST_NEXT(nsrr, link))
		if (ns_samename(nsrr->name, dname) == 1)
			return (nsrr);
	return (NULL);
}

static isc_result_t
do_query(res_state statp, const char *dname, ns_class class, ns_type qtype,
	 double *resp, ns_msg *msg, int *alias_count)
{
	double req[NS_PACKETSZ / sizeof (double)];
	int i;
	unsigned n;
	isc_result_t status;

	status = res_nmkquery(statp, ns_o_query, dname, class, qtype,
			      NULL, 0, NULL, req, NS_PACKETSZ, &n);
	if (status != ISC_R_SUCCESS) {
		DPRINTF(("do_query: res_nmkquery failed"));
		return status;
	}
	status = res_nsend(statp, req, n, resp, NS_PACKETSZ, &n);
	if (status != ISC_R_SUCCESS) {
		DPRINTF(("do_query: res_nsend failed"));
		return status;
	}
	if (n == 0) {
		DPRINTF(("do_query: res_nsend returned 0"));
		return ISC_R_NOTFOUND;
	}
	if (ns_initparse((u_char *)resp, n, msg) < 0) {
		DPRINTF(("do_query: ns_initparse failed"));
		return ISC_R_NOSPACE;
	}
	n = 0;
	for (i = 0; i < ns_msg_count(*msg, ns_s_an); i++) {
		ns_rr rr;

		status = ns_parserr(msg, ns_s_an, i, &rr);
		if (status != ISC_R_SUCCESS) {
			DPRINTF(("do_query: ns_parserr failed"));
			return status;
		}
		n += (ns_rr_class(rr) == class &&
		      (ns_rr_type(rr) == ns_t_cname ||
		       ns_rr_type(rr) == ns_t_dname));
	}
	if (alias_count)
		*alias_count = n;
	return ISC_R_SUCCESS;
}
