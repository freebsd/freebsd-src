#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)db_glue.c	4.4 (Berkeley) 6/1/90";
static const char rcsid[] = "$Id: db_glue.c,v 8.47 2002/05/18 01:02:54 marka Exp $";
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
 * Portions Copyright (c) 1996-2000 by Internet Software Consortium.
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
#include <sys/socket.h>
#include <sys/un.h>

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
		slp->name = freestr(slp->name);
		slp->proto = freestr(slp->proto);
		memput(slp, sizeof *slp);
	}
	servicelist = NULL;
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
		plp->name = freestr(plp->name);
		memput(plp, sizeof *plp);
	}
	protolist = NULL;
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
	if (savedpp != NULL) {
		/* mark deleted or pending deletion */
		dp->d_mark |= D_MARK_DELETED; 
		dp->d_next = *savedpp;
		DRCNTINC(dp);
		*savedpp = dp;
	} else
		dp->d_next = NULL;
	dp->d_flags &= ~DB_F_ACTIVE;
	db_detach(&dp);
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
	if ( (np->n_data != NULL && (msg = "data") != NULL)
	  || (np->n_hash != NULL && (msg = "hash") != NULL)
	    ) {
		ns_panic(ns_log_db, 1, "rm_name(%p(%s)): non-nil %s pointer",
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

/* u_int
 * nhash(name)
 *	compute hash for this name and return it; ignore case differences
 * note:
 *	this logic is intended to produce the same result as nlookup()'s.
 */
u_int
nhash(const char *name) {
	u_char ch;
	u_int hval;

	hval = 0;
	while ((ch = (u_char)*name++) != (u_char)'\0')
		HASHIMILATE(hval, ch);
	return (hval);
}

static void
db_freedata(struct databuf *dp) {
	int bytes = BIND_DATASIZE(dp->d_size);

	if (dp->d_rcnt != 0)
		panic("db_freedata: d_rcnt != 0", NULL);
	if ((dp->d_flags & (DB_F_ACTIVE|DB_F_FREE)) != 0)
		panic("db_freedata: %s set",
		      (dp->d_flags & DB_F_FREE) != 0 ? "DB_F_FREE" :
		      "DB_F_ACTIVE");
	if (dp->d_next != NULL)
		panic("db_free: d_next != NULL", NULL);
	dp->d_flags |= DB_F_FREE;
#ifdef CHECK_MAGIC
	dp->d_magic = 0;
#endif
	memput(dp, bytes);
}

void
db_detach(struct databuf **dpp) {
	struct databuf *dp;

	INSIST(dpp != NULL && *dpp != NULL);
	dp = *dpp;
#ifdef CHECK_MAGIC
	INSIST(dp->d_magic == DATABUF_MAGIC);
#endif

	DRCNTDEC(dp);
	if (dp->d_rcnt == 0)
		db_freedata(dp);
	*dpp = NULL;
}

struct lame_hash {
	struct lame_hash	*next;
	char			*zone;
	char			*server;
	time_t			when;
	unsigned int		hval;
} **lame_hash = NULL;

static int lame_hash_size = 0;
static int lame_hash_cnt = 0;

void
db_lame_add(char *zone, char *server, time_t when) {
	unsigned int hval = nhash(zone);
	struct lame_hash *last, *this;
	struct lame_hash **new;
	int n;
	int newsize;

	db_lame_clean();

	/* grow / initalise hash table */
	if (lame_hash_cnt >= lame_hash_size) {
		if (lame_hash_size == 0)
			newsize = hashsizes[0];
		else {
			for (n = 0; (newsize = hashsizes[n++]) != 0; (void)NULL)
				if (lame_hash_size == newsize) {
					newsize = hashsizes[n];
					break;
				}
			if (newsize == 0)
				newsize = lame_hash_size * 2 + 1;
		}
		new = memget(newsize * sizeof this);
		if (new == NULL)
			return;
		memset(new, 0, newsize * sizeof this);
		for (n = 0 ; n < lame_hash_size; n++) {
			this = lame_hash[n];
			while (this) {
				last = this;
				this = this->next;
				last->next = new[hval%newsize];
				new[hval%newsize] = last;
			}
		}
		if (lame_hash != NULL)
			memput(lame_hash, lame_hash_size * sizeof this);
		lame_hash = new;
		lame_hash_size = newsize;
	}

	last = NULL;
	this = lame_hash[hval%lame_hash_size];
	while (this) {
		if ((ns_samename(this->server, server) == 1) &&
		    (ns_samename(this->zone, zone) == 1)) {
			this->when = when;
			return;
		}
		last = this;
		this = this->next;
	}
	this = memget(sizeof *this);
	if (this == NULL)
		return;
	this->server = savestr(server, 0);
	this->zone = savestr(zone, 0);
	if (this->server == NULL || this->zone == NULL) {
		if (this->server != NULL)
			this->server = freestr(this->server);
		if (this->zone != NULL)
			this->zone = freestr(this->zone);
		memput(this, sizeof *this);
		return;
	}
	this->when = when;
	this->hval = hval;
	this->next = NULL;
	if (last != NULL)
		last->next = this;
	else
		lame_hash[hval%lame_hash_size] = this;
	lame_hash_cnt++;
}

time_t 
db_lame_find(char *zone, struct databuf *dp) {
	unsigned int hval = nhash(zone);
	struct lame_hash *this;

	if (lame_hash_size == 0) {
		/* db_lame_destroy() must have been called. */
		dp->d_flags &= ~DB_F_LAME;
		return (0);
	}

	db_lame_clean();	/* Remove expired record so that we can
			 	 * clear DB_F_LAME when there are no
				 * additions. */

	this = lame_hash[hval % lame_hash_size];
	while (this) {
		if ((ns_samename(this->server, (char*)dp->d_data) == 1) &&
		    (ns_samename(this->zone, zone) == 1))
			return (this->when);
		this = this->next;
	}
	dp->d_flags &= ~DB_F_LAME;
	return (0);
}

void
db_lame_clean(void) {
	int i;
	struct lame_hash *last, *this;

	for (i = 0 ; i < lame_hash_size; i++) {
		last = NULL;
		this = lame_hash[i];
		while (this != NULL) {
			if (this->when < tt.tv_sec) {
				this->zone = freestr(this->zone);
				this->server = freestr(this->server);
				if (last != NULL) {
					last->next = this->next;
					memput(this, sizeof *this);
					this = last->next;
				} else {
					lame_hash[i] = this->next;
					memput(this, sizeof *this);
					this = lame_hash[i];
				}
				lame_hash_cnt--;
			} else {
				last = this;
				this = this->next;
			}
		}
	}
}

void
db_lame_destroy(void) {
	int i;
	struct lame_hash *last, *this;

	if (lame_hash_size == 0)
		return;

	for (i = 0 ; i < lame_hash_size; i++) {
		this = lame_hash[i];
		while (this != NULL) {
			last = this;
			this = this->next;
			last->zone = freestr(last->zone);
			last->server = freestr(last->server);
			memput(last, sizeof *this);
		}
	}
	memput(lame_hash, lame_hash_size * sizeof this);
	lame_hash_cnt = 0;
	lame_hash_size = 0;
	lame_hash = NULL;
}
