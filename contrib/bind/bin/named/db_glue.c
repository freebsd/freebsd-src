#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)db_glue.c	4.4 (Berkeley) 6/1/90";
static char rcsid[] = "$Id: db_glue.c,v 8.27 1998/02/14 00:41:39 halley Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986, 1988
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
 * Portions Copyright (c) 1996, 1997 by Internet Software Consortium.
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
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "named.h"

struct valuelist {
	struct valuelist *	next;
	struct valuelist *	prev;
	char *			name;
	char *			proto;
	int			port;
};
static struct valuelist *servicelist, *protolist;

void
buildservicelist() {
	struct servent *sp;
	struct valuelist *slp;

#ifdef MAYBE_HESIOD
	setservent(0);
#else
	setservent(1);
#endif
	while ((sp = getservent()) != NULL) {
		slp = (struct valuelist *)memget(sizeof(struct valuelist));
		if (!slp)
			panic("memget(servent)", NULL);
		slp->name = savestr(sp->s_name, 1);
		slp->proto = savestr(sp->s_proto, 1);
		slp->port = ntohs((u_int16_t)sp->s_port);  /* host byt order */
		slp->next = servicelist;
		slp->prev = NULL;
		if (servicelist)
			servicelist->prev = slp;
		servicelist = slp;
	}
	endservent();
}

void
destroyservicelist() {
	struct valuelist *slp, *slp_next;

	for (slp = servicelist; slp != NULL; slp = slp_next) {
		slp_next = slp->next;
		freestr(slp->name);
		freestr(slp->proto);
		memput(slp, sizeof *slp);
	}
}

void
buildprotolist() {
	struct protoent *pp;
	struct valuelist *slp;

#ifdef MAYBE_HESIOD
	setprotoent(0);
#else
	setprotoent(1);
#endif
	while ((pp = getprotoent()) != NULL) {
		slp = (struct valuelist *)memget(sizeof(struct valuelist));
		if (!slp)
			panic("memget(protoent)", NULL);
		slp->name = savestr(pp->p_name, 1);
		slp->port = pp->p_proto;	/* host byte order */
		slp->next = protolist;
		slp->prev = NULL;
		if (protolist)
			protolist->prev = slp;
		protolist = slp;
	}
	endprotoent();
}

void
destroyprotolist() {
	struct valuelist *plp, *plp_next;

	for (plp = protolist; plp != NULL; plp = plp_next) {
		plp_next = plp->next;
		freestr(plp->name);
		memput(plp, sizeof *plp);
	}
}

static int
findservice(const char *s, struct valuelist **list) {
	struct valuelist *lp = *list;
	int n;

	for (; lp != NULL; lp = lp->next)
		if (strcasecmp(lp->name, s) == 0) {
			if (lp != *list) {
				lp->prev->next = lp->next;
				if (lp->next)
					lp->next->prev = lp->prev;
				(*list)->prev = lp;
				lp->next = *list;
				*list = lp;
			}
			return (lp->port);	/* host byte order */
		}
	if (sscanf(s, "%d", &n) != 1 || n <= 0)
		n = -1;
	return (n);
}

/*
 * Convert service name or (ascii) number to int.
 */
int
servicenumber(const char *p) {
	return (findservice(p, &servicelist));
}

/*
 * Convert protocol name or (ascii) number to int.
 */
int
protocolnumber(const char *p) {
	return (findservice(p, &protolist));
}

static struct servent *
cgetservbyport(u_int16_t port, const char *proto) {	/* Host byte order. */
	struct valuelist **list = &servicelist;
	struct valuelist *lp = *list;
	static struct servent serv;

	port = ntohs(port);
	for (; lp != NULL; lp = lp->next) {
		if (port != (u_int16_t)lp->port)	/* Host byte order. */
			continue;
		if (strcasecmp(lp->proto, proto) == 0) {
			if (lp != *list) {
				lp->prev->next = lp->next;
				if (lp->next)
					lp->next->prev = lp->prev;
				(*list)->prev = lp;
				lp->next = *list;
				*list = lp;
			}
			serv.s_name = lp->name;
			serv.s_port = htons((u_int16_t)lp->port);
			serv.s_proto = lp->proto;
			return (&serv);
		}
	}
	return (0);
}

static struct protoent *
cgetprotobynumber(int proto) {				/* Host byte order. */
	struct valuelist **list = &protolist;
	struct valuelist *lp = *list;
	static struct protoent prot;

	for (; lp != NULL; lp = lp->next)
		if (lp->port == proto) {		/* Host byte order. */
			if (lp != *list) {
				lp->prev->next = lp->next;
				if (lp->next)
					lp->next->prev = lp->prev;
				(*list)->prev = lp;
				lp->next = *list;
				*list = lp;
			}
			prot.p_name = lp->name;
			prot.p_proto = lp->port;	/* Host byte order. */
			return (&prot);
		}
	return (0);
}

const char *
protocolname(int num) {
	static char number[8];
	struct protoent *pp;

	pp = cgetprotobynumber(num);
	if (pp == 0)  {
		(void) sprintf(number, "%d", num);
		return (number);
	}
	return (pp->p_name);
}

const char *
servicename(u_int16_t port, const char *proto) {	/* Host byte order. */
	static char number[8];
	struct servent *ss;

	ss = cgetservbyport(htons(port), proto);
	if (ss == 0)  {
		(void) sprintf(number, "%d", port);
		return (number);
	}
	return (ss->s_name);
}

static struct map map_class[] = {
	{ "in",		C_IN },
	{ "chaos",	C_CHAOS },
	{ "hs",		C_HS },
	{ NULL,		0 }
};

int
get_class(const char *class) {
	const struct map *mp;

	if (isdigit(*class))
		return (atoi(class));
	for (mp = map_class; mp->token != NULL; mp++)
		if (strcasecmp(class, mp->token) == 0)
			return (mp->val);
	return (C_IN);
}

/* rm_datum(dp, np, pdp, savedpp)
 *	remove datum 'dp' from name 'np'.  pdp is previous data pointer.
 *	if savedpp is not NULL, and compiled with BIND_UPDATE, save 
 *      datum dp there rather than freeing the memory (caller will take
 *      care of freeing it)
 * return value:
 *	"next" field from removed datum, suitable for relinking
 */
struct databuf *
rm_datum(struct databuf *dp, struct namebuf *np, struct databuf *pdp,
	 struct databuf **savedpp) {
	struct databuf *ndp = dp->d_next;

	ns_debug(ns_log_db, 3, "rm_datum(%lx, %lx, %lx, %lx) -> %lx",
		 (u_long)dp, (u_long)np->n_data, (u_long)pdp, 
		 (u_long)savedpp, (u_long)ndp);
	if ((dp->d_flags & DB_F_ACTIVE) == 0)
		panic("rm_datum: DB_F_ACTIVE not set", NULL);
	if (pdp == NULL)
		np->n_data = ndp;
	else
		pdp->d_next = ndp;
#ifdef BIND_UPDATE
	if (savedpp != NULL) {
		/* mark deleted or pending deletion */
		dp->d_mark |= D_MARK_DELETED; 
		dp->d_next = *savedpp;
		*savedpp = dp;
	} else
		dp->d_next = NULL;
#else
	dp->d_next = NULL;
#endif
	dp->d_flags &= ~DB_F_ACTIVE;
	DRCNTDEC(dp);
	if (dp->d_rcnt) {
#ifdef DEBUG
		int32_t ii;
#endif

		switch(dp->d_type) {
		case T_NS:
			ns_debug(ns_log_db, 3, "rm_datum: %s rcnt = %d",
				 dp->d_data, dp->d_rcnt);
			break;
#ifdef DEBUG
		case T_A:
			memcpy(&ii, dp->d_data, sizeof ii);
			ns_debug(ns_log_db, 3,
				 "rm_datum: %08.8X rcnt = %d",
				 ii, dp->d_rcnt);
			break;
#endif
		default:
			ns_debug(ns_log_db, 3,
				 "rm_datum: rcnt = %d", dp->d_rcnt);
		}
	} else
#ifdef BIND_UPDATE
		if (savedpp == NULL)
#endif
		        db_freedata(dp);
	return (ndp);
}

/* rm_name(np, he, pnp)
 *	remove name 'np' from parent 'pp'.  pnp is previous name pointer.
 * return value:
 *	"next" field from removed name, suitable for relinking.
 */
struct namebuf *
rm_name(struct namebuf *np, struct namebuf **pp, struct namebuf *pnp) {
	struct namebuf *nnp = np->n_next;
	const char *msg;

	/* verify */
	if ( (np->n_data && (msg = "data"))
	  || (np->n_hash && (msg = "hash"))
	    ) {
		ns_panic(ns_log_db, 1, "rm_name(%#x(%s)): non-nil %s pointer",
			 np, NAME(*np), msg);
	}

	/* unlink */
	if (pnp)
		pnp->n_next = nnp;
	else
		*pp = nnp;

	/* deallocate */
	memput(np, NAMESIZE(NAMELEN(*np)));

	/* done */
	return (nnp);
}

void
rm_hash(struct hashbuf *htp) {
	REQUIRE(htp != NULL);
	REQUIRE(htp->h_cnt == 0);

	memput(htp, HASHSIZE(htp->h_size));
}

/*
 * Get the domain name of 'np' and put in 'buf'.  Bounds checking is done.
 */
void
getname(struct namebuf *np, char *buf, int buflen) {
	char *cp;
	int i;

	cp = buf;
	while (np != NULL) {
		i = (int) NAMELEN(*np);
		if (i + 1 >= buflen) {
			*cp = '\0';
			ns_info(ns_log_db, 
				"domain name too long: %s...", buf);
			strcpy(buf, "Name_Too_Long");
			return;
		}
		if (cp != buf)
			*cp++ = '.';
		memcpy(cp, NAME(*np), i);
		cp += i;
		buflen -= i + 1;
		np = np->n_parent;
	}
	*cp = '\0';
}

/*
 * Compute hash value from data.
 */
u_int
dhash(const u_char *dp, int dlen) {
	u_char *cp;
	u_int hval;
	int n;

	n = dlen;
	if (n > 8)
		n = 8;
	hval = 0;
	while (--n >= 0) {
		hval <<= 1;
		hval += *dp++;
	}
	return (hval % INVHASHSZ);
}

/* u_int
 * nhash(name)
 *	compute hash for this name and return it; ignore case differences
 */
u_int
nhash(const char *name) {
	u_char ch;
	u_int hval;

	hval = 0;
	while ((ch = (u_char)*name++) != (u_char)'\0') {
		if (isascii(ch) && isupper(ch))
			ch = tolower(ch);
		hval <<= 1;
		hval += ch;
	}
	return (hval % INVHASHSZ);
}

/*
** SAMEDOMAIN -- Check whether a name belongs to a domain
** ------------------------------------------------------
**
**	Returns:
**		TRUE if the given name lies in the domain.
**		FALSE otherwise.
**
**	Trailing dots are first removed from name and domain.
**	Always compare complete subdomains, not only whether the
**	domain name is the trailing string of the given name.
**
**	"host.foobar.top" lies in "foobar.top" and in "top" and in ""
**	but NOT in "bar.top"
*/

int
samedomain(const char *a, const char *b) {
	size_t la, lb;
	int diff, i, escaped;
	const char *cp;

	la = strlen(a);
	lb = strlen(b);

	/* ignore a trailing label separator (i.e. an unescaped dot) in 'a' */
	if (la && a[la-1] == '.') {
		escaped = 0;
		/* note this loop doesn't get executed if la==1 */
		for (i = la - 2; i >= 0; i--)
			if (a[i] == '\\') {
				if (escaped)
					escaped = 0;
				else
					escaped = 1;
			} else {
				break;
			}
		if (!escaped)
			la--;
	}
	/* ignore a trailing label separator (i.e. an unescaped dot) in 'b' */
	if (lb && b[lb-1] == '.') {
		escaped = 0;
		/* note this loop doesn't get executed if lb==1 */
		for (i = lb - 2; i >= 0; i--)
			if (b[i] == '\\') {
				if (escaped)
					escaped = 0;
				else
					escaped = 1;
			} else {
				break;
			}
		if (!escaped)
			lb--;
	}

	/* lb==0 means 'b' is the root domain, so 'a' must be in 'b'. */
	if (lb == 0)
		return (1);

	/* 'b' longer than 'a' means 'a' can't be in 'b'. */
	if (lb > la)
		return (0);

	/* We use strncasecmp because we might be trying to
	 * ignore a trailing dot. */
	if (lb == la)
		return (strncasecmp(a, b, lb) == 0);

	/* Ok, we know la > lb. */

	diff = la - lb;

	/* If 'a' is only 1 character longer than 'b', then it can't be
	   a subdomain of 'b' (because of the need for the '.' label
	   separator). */
	if (diff < 2)
		return (0);

	/* If the character before the last 'lb' characters of 'b'
	   isn't '.', then it can't be a match (this lets us avoid
	   having "foobar.com" match "bar.com"). */
	if (a[diff-1] != '.')
		return (0);

	/* We're not sure about that '.', however.  It could be escaped
           and thus not a really a label separator. */
	escaped=0;
	for (i = diff-2; i >= 0; i--)
		if (a[i] == '\\') {
			if (escaped)
				escaped = 0;
			else
				escaped = 1;
		}
		else
			break;
	if (escaped)
		return (0);
	  
	/* We use strncasecmp because we might be trying to
	 * ignore trailing dots. */
	cp = a + diff;
	return (strncasecmp(cp, b, lb) == 0);
}

void
db_freedata(struct databuf *dp) {
	int bytes = (dp->d_type == T_NS) ?
		DATASIZE(dp->d_size)+INT32SZ : DATASIZE(dp->d_size);

	if (dp->d_rcnt != 0)
		panic("db_freedata: d_rcnt != 0", NULL);
	if ((dp->d_flags & (DB_F_ACTIVE|DB_F_FREE)) != 0)
		panic("db_freedata: %s set",
		      (dp->d_flags & DB_F_FREE) != 0 ? "DB_F_FREE" :
		      "DB_F_ACTIVE");
	if (dp->d_next != NULL)
		panic("db_free: d_next != NULL", NULL);
	dp->d_flags |= DB_F_FREE;
	memput(dp, bytes);
}
