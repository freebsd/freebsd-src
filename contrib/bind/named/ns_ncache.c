/**************************************************************************
 * ns_ncache.c
 * author: anant kumar
 * last modification: March 17, 1993
 *
 * implements negative caching
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <resolv.h>

#include "named.h"

#ifdef NCACHE

#define BOUNDS_CHECK(ptr, count) \
	do { \
		if ((ptr) + (count) > eom) { \
			return; \
		} \
	} while (0)

void
cache_n_resp(msg, msglen)
	u_char *msg;
	int msglen;
{
	register struct databuf *dp;
	HEADER *hp;
	u_char *cp, *eom, *rdatap;
	char dname[MAXDNAME];
	int n;
	int type, class;
#ifdef VALIDATE
	int Vcode;
#endif
	int flags;
	u_int dlen;

	nameserIncr(from_addr.sin_addr, nssRcvdNXD);

	hp = (HEADER *)msg;
	cp = msg+HFIXEDSZ;
	eom = msg + msglen;
  
	n = dn_expand(msg, eom, cp, dname, sizeof dname);
	if (n < 0) {
		dprintf(1, (ddt, "Query expand name failed:cache_n_resp\n"));
		hp->rcode = FORMERR;
		return;
	}
	cp += n;
	BOUNDS_CHECK(cp, 2 * INT16SZ);
	GETSHORT(type, cp);
	GETSHORT(class, cp);
	dprintf(1, (ddt,
		    "ncache: dname %s, type %d, class %d\n",
		    dname, type, class));

#ifdef VALIDATE
	Vcode = validate(dname, dname, &from_addr, type, class, NULL, 0,
			 hp->rcode == NXDOMAIN ? NXDOMAIN : NOERROR_NODATA);
	if (Vcode == INVALID || Vcode == VALID_NO_CACHE) {
		/*Valid_no_cache should never occur but doesn't hurt to check*/
		return;
	}
#endif
#ifdef RETURNSOA
	if (hp->nscount) {
		u_int32_t ttl;
		u_int16_t atype;
		u_char *tp = cp;
		u_char *cp1;
		u_char data[MAXDNAME*2 + INT32SZ*5];
		size_t len = sizeof data;

		/* we store NXDOMAIN as T_SOA regardless of the query type */
		if (hp->rcode == NXDOMAIN)
			type = T_SOA;

		/* store their SOA record */
		n = dn_skipname(tp, eom);
		if (n < 0) {
			dprintf(3, (ddt, "ncache: form error\n"));
			return;
		}
		tp += n;
		BOUNDS_CHECK(tp, 3 * INT16SZ + INT32SZ);
		GETSHORT(atype, tp);		/* type */
		if (atype != T_SOA) {
			dprintf(3, (ddt,
				    "ncache: type (%d) != T_SOA\n",atype));
			goto no_soa;
		}
		tp += INT16SZ;		/* class */
		GETLONG(ttl, tp);	/* ttl */
		GETSHORT(dlen, tp);	/* dlen */
		BOUNDS_CHECK(tp, dlen);
		rdatap = tp;

		/* origin */
		n = dn_expand(msg, eom, tp, (char*)data, len);
		if (n < 0) {
			dprintf(3, (ddt, "ncache: form error 2\n"));
			return;
		}
		tp += n;
		n = strlen((char*)data) + 1;
		cp1 = data + n;
		len -= n;
		/* mail */
		n = dn_expand(msg, msg + msglen, tp, (char*)cp1, len);
		if (n < 0) {
			dprintf(3, (ddt, "ncache: form error 2\n"));
			return;
		}
		tp += n;
		n = strlen((char*)cp1) + 1;
		cp1 += n;
		len -= n;
		n = 5 * INT32SZ;
		BOUNDS_CHECK(tp, n);
		bcopy(tp, cp1, n);
		/* serial, refresh, retry, expire, min */
		cp1 += n;
		len -= n;
		tp += n;
		if (tp != rdatap + dlen) {
			dprintf(3, (ddt, "ncache: form error 2\n"));
			return;
		}
		/* store the zone of the soa record */
		n = dn_expand(msg, msg + msglen, cp, (char*)cp1, len);
		if (n < 0) {
			dprintf(3, (ddt, "ncache: form error 2\n"));
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

	if ((n = db_update(dname, dp, dp, flags, hashtab)) != OK) {
		dprintf(1, (ddt,
			  "db_update failed return value:%d, cache_n_resp()\n",
			    n));
		db_free(dp);
		return;
	}
	dprintf(4, (ddt,
		    "ncache succeeded: [%s %s %s] rcode:%d ttl:%ld\n",
		    dname, p_type(type), p_class(class),
		    dp->d_rcode, (long)(dp->d_ttl-tt.tv_sec)));
	return;
}

#endif /*NCACHE*/
