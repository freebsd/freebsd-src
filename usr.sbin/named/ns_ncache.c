/**************************************************************************
 * ns_ncache.c
 * author: anant kumar
 * last modification: March 17, 1993
 *
 * implements negative caching
 */

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

void
cache_n_resp(msg, msglen)
	u_char *msg;
	int msglen;
{
	register struct databuf *dp;
	HEADER *hp;
	u_char *cp;
	char dname[MAXDNAME];
	int n;
	int type, class;
	int Vcode;
	int flags;

	nameserIncr(from_addr.sin_addr, nssRcvdNXD);

	hp = (HEADER *)msg;
	cp = msg+HFIXEDSZ;
  
	n = dn_expand(msg, msg + msglen, cp, dname, sizeof dname);
	if (n < 0) {
		dprintf(1, (ddt, "Query expand name failed:cache_n_resp\n"));
		hp->rcode = FORMERR;
		return;
	}
	cp += n;
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
	if (hp->rcode==NXDOMAIN) {
	    u_int32_t ttl;
	    u_int16_t atype;
	    u_char * tp = cp;
	    u_char * cp1;
	    u_char data[BUFSIZ+MAXDNAME];
	    int len = sizeof(data);

	    /* store ther SOA record */
	    if (!hp->nscount) {
		dprintf(3, (ddt, "ncache: nscount == 0\n"));
		return;
	    }
	    n = dn_skipname(tp, msg + msglen);
	    if (n < 0) {
		dprintf(3, (ddt, "ncache: form error\n"));
		return;
	    }
	    tp += n;
	    GETSHORT(atype,tp);		/* type */
	    if (atype != T_SOA) {
		dprintf(3, (ddt, "ncache: type (%d) != T_SOA\n",atype));
		return;
	    }
	    tp += sizeof(u_int16_t);	/* class */
	    GETLONG(ttl,tp);		/* ttl */
	    tp += sizeof(u_int16_t);	/* dlen */

	    if ((n = dn_expand(msg, msg + msglen, tp, data, len))
			< 0 ) {
		dprintf(3, (ddt, "ncache: form error 2\n"));
		return;
	    }	/* origin */
	    tp += n;
	    cp1 = data + (n = strlen(data) + 1);
	    len -= n;
	    if ((n = dn_expand(msg, msg + msglen, tp, cp1, len)) < 0 ) {
		dprintf(3, (ddt, "ncache: form error 2\n"));
		return;
	    }	/* mail */
	    tp += n;
	    n = strlen(cp1) + 1;
	    cp1 +=  n;
	    len -= n;
	    bcopy(tp, cp1, n = 5 * sizeof(u_int32_t));
	    /* serial, refresh, retry, expire, min */
	    cp1 += n;
	    len -= n;
	    /* store the zone of the soa record */
	    if ((n = dn_expand(msg, msg + msglen, cp, cp1, len)) < 0 ) {
		dprintf(3, (ddt, "ncache: form error 2\n"));
		return;
	    }
	    n = strlen(cp1) + 1;
	    cp1 += n;

	    dp = savedata(class, T_SOA, MIN(ttl,NTTL)+tt.tv_sec, data,
			  cp1 - data);
	} else {
#endif
	dp = savedata(class, type, NTTL+tt.tv_sec, NULL, 0);
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

	if ((n = db_update(dname,dp,dp,flags,hashtab)) != OK) {
		dprintf(1, (ddt,
			  "db_update failed return value:%d, cache_n_resp()\n",
			    n));
		free((char *)dp);
		return;
	}
	dprintf(4, (ddt,
		    "ncache succeeded: [%s %s %s] rcode:%d ttl:%l\n",
		    dname, p_type(type), p_class(class),
		    dp->d_rcode, (long)(dp->d_ttl-tt.tv_sec)));
	return;
}

#endif /*NCACHE*/
