#if !defined(lint) && !defined(SABER)
static char rcsid[] = "$Id: ns_ncache.c,v 8.17 1998/03/20 01:12:01 halley Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996, 1997 by Internet Software Consortium.
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
cache_n_resp(u_char *msg, int msglen, struct sockaddr_in from) {
	struct databuf *dp;
	HEADER *hp;
	u_char *cp, *eom, *rdatap;
	char dname[MAXDNAME];
	int n;
	int type, class;
	int Vcode;
	int flags;
	u_int16_t ancount;
	u_int dlen;

	nameserIncr(from.sin_addr, nssRcvdNXD);

	hp = (HEADER *)msg;
	cp = msg+HFIXEDSZ;
	eom = msg + msglen;
  
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
	ns_debug(ns_log_ncache, 1, "ncache: dname %s, type %d, class %d",
		 dname, type, class);

	ancount = ntohs(hp->ancount);

	while (ancount--) {
		u_int32_t ttl;
		u_int16_t atype;
		u_int16_t aclass;
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
			ns_debug(ns_log_ncache, 3, "ncache: form error");
			return;
		}
		GETLONG(ttl, cp);
		GETSHORT(dlen, cp);
		BOUNDS_CHECK(cp, dlen);
		rdatap = cp;
		n = dn_expand(msg, msg + msglen, cp, dname, sizeof dname);
		if (n < 0) {
			ns_debug(ns_log_ncache, 3, "ncache: form error");
			return;
		}
		cp += n;
		if (cp != rdatap + dlen) {
			ns_debug(ns_log_ncache, 3, "ncache: form error");
			return;
		}
	}

#ifdef RETURNSOA
	if (hp->nscount) {
		u_int32_t ttl;
		u_int16_t atype;
		u_char *tp = cp;
		u_char *cp1;
		u_char data[MAXDATA];
		size_t len = sizeof data;

		/* we store NXDOMAIN as T_SOA regardless of the query type */
		if (hp->rcode == NXDOMAIN)
			type = T_SOA;

		/* store ther SOA record */
		n = dn_skipname(tp, msg + msglen);
		if (n < 0) {
			ns_debug(ns_log_ncache, 3, "ncache: form error");
			return;
		}
		tp += n;
		
		BOUNDS_CHECK(tp, 3 * INT16SZ + INT32SZ);
		GETSHORT(atype, tp);		/* type */
		if (atype != T_SOA) {
			ns_debug(ns_log_ncache, 3,
				 "ncache: type (%d) != T_SOA", atype);
			goto no_soa;
		}
		tp += INT16SZ;			/* class */
		GETLONG(ttl, tp);		/* ttl */
		GETSHORT(dlen, tp);		/* dlen */
		BOUNDS_CHECK(tp, dlen);
		rdatap = tp;

		/* origin */
		n = dn_expand(msg, msg + msglen, tp, (char*)data, len);
		if (n < 0) {
			ns_debug(ns_log_ncache, 3,
				 "ncache: origin form error");
			return;
		}
		tp += n;
		n = strlen((char*)data) + 1;
		cp1 = data + n;
		len -= n;
		/* mail */
		n = dn_expand(msg, msg + msglen, tp, (char*)cp1, len);
		if (n < 0) {
			ns_debug(ns_log_ncache, 3, "ncache: mail form error");
			return;
		}
		tp += n;
		n = strlen((char*)cp1) + 1;
		cp1 += n;
		len -= n;
		n = 5 * INT32SZ;
		BOUNDS_CHECK(tp, n);
		memcpy(cp1, tp, n);
		/* serial, refresh, retry, expire, min */
		cp1 += n;
		len -= n;
		tp += n;
		if (tp != rdatap + dlen) {
			ns_debug(ns_log_ncache, 3, "ncache: form error");
			return;
		}
		/* store the zone of the soa record */
		n = dn_expand(msg, msg + msglen, cp, (char*)cp1, len);
		if (n < 0) {
			ns_debug(ns_log_ncache, 3, "ncache: form error 2");
			return;
		}
		n = strlen((char*)cp1) + 1;
		cp1 += n;

		dp = savedata(class, type, MIN(ttl, NTTL) + tt.tv_sec, data,
			      cp1 - data);
	} else {
 no_soa:
#endif
        dp = savedata(class, type, NTTL + tt.tv_sec, NULL, 0);
#ifdef RETURNSOA
	}
#endif
	dp->d_zone = DB_Z_CACHE;
	dp->d_cred = hp->aa ? DB_C_AUTH : DB_C_ANSWER;
	dp->d_clev = 0;
	if(hp->rcode == NXDOMAIN) {
		dp->d_rcode = NXDOMAIN;
		flags = DB_NODATA|DB_NOTAUTH|DB_NOHINTS;
	} else {
		dp->d_rcode = NOERROR_NODATA;
		flags = DB_NOTAUTH|DB_NOHINTS;
	}

	if ((n = db_update(dname, dp, dp, NULL, flags, hashtab, from)) != OK) {
		ns_debug(ns_log_ncache, 1,
			 "db_update failed (%d), cache_n_resp()", n);
		db_freedata(dp);
		return;
	}
	ns_debug(ns_log_ncache, 4,
		 "ncache succeeded: [%s %s %s] rcode:%d ttl:%ld",
		    dname, p_type(type), p_class(class),
		    dp->d_rcode, (long)(dp->d_ttl - tt.tv_sec));
}
