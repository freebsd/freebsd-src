#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: ns_ncache.c,v 8.29 2001/06/18 14:43:16 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996-2000 by Internet Software Consortium.
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

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>

#include "port_after.h"

#include "named.h"

#define BOUNDS_CHECK(ptr, count) \
	do { \
		if ((ptr) + (count) > eom) { \
			return; \
		} \
	} while (0)

void
cache_n_resp(u_char *msg, int msglen, struct sockaddr_in from,
	     const char *qname, int qclass, int qtype)
{
	struct databuf *dp;
	HEADER *hp;
	u_char *cp, *eom, *rdatap;
	char dname[MAXDNAME];
	int n, type, class, flags;
	u_int ancount, nscount, dlen;
#ifdef	RETURNSOA
	u_int32_t ttl;
	u_int16_t atype;
	u_char *sp, *cp1;
	u_char data[MAXDATA];
	size_t len = sizeof data;
#endif

	nameserIncr(from.sin_addr, nssRcvdNXD);

	hp = (HEADER *)msg;
	cp = msg + HFIXEDSZ;
	eom = msg + msglen;

	switch (ntohs(hp->qdcount)) {
	case 0:
		dname[sizeof dname - 1] = '\0';
		strncpy(dname, qname, sizeof dname);
		if (dname[sizeof dname - 1] != '\0') {
			ns_debug(ns_log_ncache, 1,
				 "qp->qname too long (%d)", strlen(qname));
			hp->rcode = FORMERR;
			return;
		}
		class = qclass;
		type = qtype;
		break;
	case 1:
		n = dn_expand(msg, eom, cp, dname, sizeof dname);
		if (n < 0) {
			ns_debug(ns_log_ncache, 1,	
				 "Query expand name failed: cache_n_resp");
			hp->rcode = FORMERR;
			return;
		}
		cp += n;
		BOUNDS_CHECK(cp, 2 * INT16SZ);
		GETSHORT(type, cp);
		GETSHORT(class, cp);
		if (class > CLASS_MAX) {
			ns_debug(ns_log_ncache, 1,
				 "bad class in cache_n_resp");
			hp->rcode = FORMERR;
			return;
		}
		break;
	default:
		ns_debug(ns_log_ncache, 1,
			 "QDCOUNT>1 (%d) in cache_n_resp", ntohs(hp->qdcount));
		hp->rcode = FORMERR;
		return;
	}
	ns_debug(ns_log_ncache, 1, "ncache: dname %s, type %d, class %d",
		 dname, type, class);

	ancount = ntohs(hp->ancount);
	nscount = ntohs(hp->nscount);

	while (ancount--) {
		u_int32_t ttl;
		int atype, aclass;

		n = dn_skipname(cp, eom);
		if (n < 0) {
			ns_debug(ns_log_ncache, 3, "ncache: form error");
			return;
		}
		cp += n;
		BOUNDS_CHECK(cp, 3 * INT16SZ + INT32SZ);
		GETSHORT(atype, cp);
		GETSHORT(aclass, cp);
		if (atype != T_CNAME || aclass != class) {
			ns_debug(ns_log_ncache, 3,
				 "ncache: not CNAME (%s) or wrong class (%s)",
				 p_type(atype), p_class(aclass));
			return;
		}
		GETLONG(ttl, cp);
		GETSHORT(dlen, cp);
		BOUNDS_CHECK(cp, dlen);
		rdatap = cp;
		n = dn_expand(msg, msg + msglen, cp, dname, sizeof dname);
		if (n < 0) {
			ns_debug(ns_log_ncache, 3, "ncache: bad cname target");
			return;
		}
		cp += n;
		if (cp != rdatap + dlen) {
			ns_debug(ns_log_ncache, 3, "ncache: bad cname rdata");
			return;
		}
	}

	dp = NULL;
#ifdef RETURNSOA
	while (nscount--) {
		sp = cp;

		/* we store NXDOMAIN as T_SOA regardless of the query type */
		if (hp->rcode == NXDOMAIN)
			type = T_SOA;

		/* store ther SOA record */
		n = dn_skipname(cp, msg + msglen);
		if (n < 0) {
			ns_debug(ns_log_ncache, 3, "ncache: form error");
			return;
		}
		cp += n;
		
		BOUNDS_CHECK(cp, 3 * INT16SZ + INT32SZ);
		GETSHORT(atype, cp);		/* type */
		cp += INT16SZ;			/* class */
		GETLONG(ttl, cp);		/* ttl */
		GETSHORT(dlen, cp);		/* dlen */
		BOUNDS_CHECK(cp, dlen);
		if (atype != T_SOA) {
			ns_debug(ns_log_ncache, 3,
				 "ncache: type (%d) != T_SOA", atype);
			cp += dlen;
			continue;
		}
		rdatap = cp;

		/* origin */
		n = dn_expand(msg, msg + msglen, cp, (char*)data, len);
		if (n < 0) {
			ns_debug(ns_log_ncache, 3,
				 "ncache: origin form error");
			return;
		}
		cp += n;
		n = strlen((char*)data) + 1;
		cp1 = data + n;
		len -= n;
		/* mail */
		n = dn_expand(msg, msg + msglen, cp, (char*)cp1, len);
		if (n < 0) {
			ns_debug(ns_log_ncache, 3, "ncache: mail form error");
			return;
		}
		cp += n;
		n = strlen((char*)cp1) + 1;
		cp1 += n;
		len -= n;
		n = 5 * INT32SZ;
		BOUNDS_CHECK(cp, n);
		memcpy(cp1, cp, n);
		/* serial, refresh, retry, expire, min */
		cp1 += n;
		len -= n;
		cp += n;
		if (cp != rdatap + dlen) {
			ns_debug(ns_log_ncache, 3, "ncache: form error");
			return;
		}
		/* store the zone of the soa record */
		n = dn_expand(msg, msg + msglen, sp, (char*)cp1, len);
		if (n < 0) {
			ns_debug(ns_log_ncache, 3, "ncache: form error 2");
			return;
		}
		n = strlen((char*)cp1) + 1;
		cp1 += n;

		/*
		 * we only want to store these long enough so that
		 * ns_resp can find it.
		 */
		if (qtype == T_SOA && hp->rcode == NXDOMAIN)
			ttl = 0;
		dp = savedata(class, type,
				MIN(ttl, server_options->max_ncache_ttl) +
					tt.tv_sec, data,
			      cp1 - data);
		break;
	}
#endif
	if (dp == NULL)
#ifdef STRICT_RFC2308
		dp = savedata(class, type, tt.tv_sec, NULL, 0);
#else
		dp = savedata(class, type, NTTL + tt.tv_sec, NULL, 0);
#endif
	dp->d_zone = DB_Z_CACHE;
	dp->d_cred = hp->aa ? DB_C_AUTH : DB_C_ANSWER;
	dp->d_secure = DB_S_INSECURE; /* BEW - should be UNCHECKED */
	dp->d_clev = 0;
	if(hp->rcode == NXDOMAIN) {
		dp->d_rcode = NXDOMAIN;
		flags = DB_NODATA|DB_NOTAUTH|DB_NOHINTS;
	} else {
		dp->d_rcode = NOERROR_NODATA;
		flags = DB_NOTAUTH|DB_NOHINTS;
	}

	n = db_update(dname, dp, dp, NULL, flags, hashtab, from);
	if (n != OK)
		ns_debug(ns_log_ncache, 1,
			 "db_update failed (%d), cache_n_resp()", n);
	else
		ns_debug(ns_log_ncache, 4,
			 "ncache succeeded: [%s %s %s] rcode:%d ttl:%ld",
			 dname, p_type(type), p_class(class),
			 dp->d_rcode, (long)(dp->d_ttl - tt.tv_sec));
	db_detach(&dp);
}
