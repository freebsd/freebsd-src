/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
/* $Id: name6.c,v 1.9 1999/10/29 03:04:26 itojun Exp $ */
/*
 *	Atsushi Onoe <onoe@sm.sony.co.jp>
 */

/*
 * TODO for thread safe
 *	use mutex for _hostconf, _hostconf_init.
 *	rewrite resolvers to be thread safe
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <netinet/in.h>

#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef _PATH_HOSTS
#define	_PATH_HOSTS	"/etc/hosts"
#endif

#ifndef MAXALIASES
#define	MAXALIASES	10
#endif
#ifndef	MAXADDRS
#define	MAXADDRS	20
#endif
#ifndef MAXDNAME
#define	MAXDNAME	1025
#endif

#ifdef INET6
#define	ADDRLEN(af)	((af) == AF_INET6 ? sizeof(struct in6_addr) : \
					    sizeof(struct in_addr))
#else
#define	ADDRLEN(af)	sizeof(struct in_addr)
#endif

#define	MAPADDR(ab, ina) \
do {									\
	memcpy(&(ab)->map_inaddr, ina, sizeof(struct in_addr));		\
	memset((ab)->map_zero, 0, sizeof((ab)->map_zero));		\
	memset((ab)->map_one, 0xff, sizeof((ab)->map_one));		\
} while (0)
#define	MAPADDRENABLED(flags) \
	(((flags) & AI_V4MAPPED) || \
	 (((flags) & AI_V4MAPPED_CFG) && _mapped_addr_enabled()))

union inx_addr {
	struct in_addr	in_addr;
#ifdef INET6
	struct in6_addr	in6_addr;
#endif
	struct {
		u_char	mau_zero[10];
		u_char	mau_one[2];
		struct in_addr mau_inaddr;
	}		map_addr_un;
#define	map_zero	map_addr_un.mau_zero
#define	map_one		map_addr_un.mau_one
#define	map_inaddr	map_addr_un.mau_inaddr
};

static struct	 hostent *_hpcopy(struct hostent *hp, int *errp);
static struct	 hostent *_hpaddr(int af, const char *name, void *addr, int *errp);
static struct	 hostent *_hpmerge(struct hostent *hp1, struct hostent *hp2, int *errp);
#ifdef INET6
static struct	 hostent *_hpmapv6(struct hostent *hp, int *errp);
#endif
static struct	 hostent *_hpsort(struct hostent *hp);
static struct	 hostent *_ghbyname(const char *name, int af, int flags, int *errp);
static char	*_hgetword(char **pp);
static int	 _mapped_addr_enabled(void);

static FILE	*_files_open(int *errp);
static struct	 hostent *_files_ghbyname(const char *name, int af, int *errp);
static struct	 hostent *_files_ghbyaddr(const void *addr, int addrlen, int af, int *errp);
static void	 _files_shent(int stayopen);
static void	 _files_ehent(void);
static struct	 hostent *_nis_ghbyname(const char *name, int af, int *errp);
static struct	 hostent *_nis_ghbyaddr(const void *addr, int addrlen, int af, int *errp);
static struct	 hostent *_dns_ghbyname(const char *name, int af, int *errp);
static struct	 hostent *_dns_ghbyaddr(const void *addr, int addrlen, int af, int *errp);
static void	 _dns_shent(int stayopen);
static void	 _dns_ehent(void);
#ifdef ICMPNL
static struct	 hostent *_icmp_ghbyaddr(const void *addr, int addrlen, int af, int *errp);
#endif /* ICMPNL */

/*
 * Select order host function.
 */
#define	MAXHOSTCONF	4

#ifndef HOSTCONF
#  define	HOSTCONF	"/etc/host.conf"
#endif /* !HOSTCONF */

struct _hostconf {
	struct hostent *(*byname)(const char *name, int af, int *errp);
	struct hostent *(*byaddr)(const void *addr, int addrlen, int af, int *errp);
};

/* default order */
static struct _hostconf _hostconf[MAXHOSTCONF] = {
	{ _dns_ghbyname,	_dns_ghbyaddr },
	{ _files_ghbyname,	_files_ghbyaddr },
#ifdef ICMPNL
	{ NULL,			_icmp_ghbyaddr },
#endif /* ICMPNL */
};

static int	_hostconf_init_done;
static void	_hostconf_init(void);

/*
 * Initialize hostconf structure.
 */

static void
_hostconf_init(void)
{
	FILE *fp;
	int n;
	char *p, *line;
	char buf[BUFSIZ];

	_hostconf_init_done = 1;
	n = 0;
	p = HOSTCONF;
	if ((fp = fopen(p, "r")) == NULL)
		return;
	while (n < MAXHOSTCONF && fgets(buf, sizeof(buf), fp)) {
		line = buf;
		if ((p = _hgetword(&line)) == NULL)
			continue;
		do {
			if (strcmp(p, "hosts") == 0
			||  strcmp(p, "local") == 0
			||  strcmp(p, "file") == 0
			||  strcmp(p, "files") == 0) {
				_hostconf[n].byname = _files_ghbyname;
				_hostconf[n].byaddr = _files_ghbyaddr;
				n++;
			}
			else if (strcmp(p, "dns") == 0
			     ||  strcmp(p, "bind") == 0) {
				_hostconf[n].byname = _dns_ghbyname;
				_hostconf[n].byaddr = _dns_ghbyaddr;
				n++;
			}
			else if (strcmp(p, "nis") == 0) {
				_hostconf[n].byname = _nis_ghbyname;
				_hostconf[n].byaddr = _nis_ghbyaddr;
				n++;
			}
#ifdef ICMPNL
			else if (strcmp(p, "icmp") == 0) {
				_hostconf[n].byname = NULL;
				_hostconf[n].byaddr = _icmp_ghbyaddr;
				n++;
			}
#endif /* ICMPNL */
		} while ((p = _hgetword(&line)) != NULL);
	}
	fclose(fp);
	if (n < 0) {
		/* no keyword found. do not change default configuration */
		return;
	}
	for (; n < MAXHOSTCONF; n++) {
		_hostconf[n].byname = NULL;
		_hostconf[n].byaddr = NULL;
	}
}

/*
 * Check if kernel supports mapped address.
 *	implementation dependent
 */
#ifdef __KAME__
#include <sys/sysctl.h>
#endif /* __KAME__ */

static int
_mapped_addr_enabled(void)
{
	/* implementation dependent check */
#if defined(__KAME__) && defined(IPV6CTL_MAPPED_ADDR)
	int mib[4];
	size_t len;
	int val;

	mib[0] = CTL_NET;
	mib[1] = PF_INET6;
	mib[2] = IPPROTO_IPV6;
	mib[3] = IPV6CTL_MAPPED_ADDR;
	len = sizeof(val);
	if (sysctl(mib, 4, &val, &len, 0, 0) == 0 && val != 0)
		return 1;
#endif /* __KAME__ && IPV6CTL_MAPPED_ADDR */
	return 0;
}

/*
 * Functions defined in RFC2553
 *	getipnodebyname, getipnodebyadr, freehostent
 */

static struct hostent *
_ghbyname(const char *name, int af, int flags, int *errp)
{
	struct hostent *hp;
	int i;

	if (flags & AI_ADDRCONFIG) {
		int s;

		/*
		 * TODO:
		 * Note that implementation dependent test for address
		 * configuration should be done everytime called
		 * (or apropriate interval),
		 * because addresses will be dynamically assigned or deleted.
		 */
		if (af == AF_UNSPEC) {
			if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
				af = AF_INET;
			else {
				_close(s);
				if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
					af = AF_INET6;
				else
				_close(s);
			}

		}
		if (af != AF_UNSPEC) {
			if ((s = socket(af, SOCK_DGRAM, 0)) < 0)
				return NULL;
			_close(s);
		}
	}

	for (i = 0; i < MAXHOSTCONF; i++) {
		if (_hostconf[i].byname
		&&  (hp = (*_hostconf[i].byname)(name, af, errp))
		    != NULL)
			return hp;
	}

	return NULL;
}

/* getipnodebyname() internal routine for multiple query(PF_UNSPEC) support. */
struct hostent *
_getipnodebyname_multi(const char *name, int af, int flags, int *errp)
{
	struct hostent *hp;
	union inx_addr addrbuf;

	/* XXX: PF_UNSPEC is only supposed to be passed from getaddrinfo() */
	if (af != AF_INET
#ifdef INET6
	    && af != AF_INET6
#endif
	    && af != PF_UNSPEC
		)
	{
		*errp = NO_RECOVERY;
		return NULL;
	}

#ifdef INET6
	/* special case for literal address */
	if (inet_pton(AF_INET6, name, &addrbuf) == 1) {
		if (af != AF_INET6) {
			*errp = HOST_NOT_FOUND;
			return NULL;
		}
		return _hpaddr(af, name, &addrbuf, errp);
	}
#endif
	if (inet_aton(name, (struct in_addr *)&addrbuf) == 1) {
		if (af != AF_INET) {
			if (MAPADDRENABLED(flags)) {
				MAPADDR(&addrbuf, &addrbuf.in_addr);
			} else {
				*errp = HOST_NOT_FOUND;
				return NULL;
			}
		}
		return _hpaddr(af, name, &addrbuf, errp);
	}

	if (!_hostconf_init_done)
		_hostconf_init();

	*errp = HOST_NOT_FOUND;
	hp = _ghbyname(name, af, flags, errp);

#ifdef INET6
	if (af == AF_INET6
	&&  ((flags & AI_ALL) || hp == NULL)
	&&  (MAPADDRENABLED(flags))) {
		struct hostent *hp2 = _ghbyname(name, AF_INET, flags, errp);
		if (hp == NULL)
			hp = _hpmapv6(hp2, errp);
		else {
			if (hp2 && strcmp(hp->h_name, hp2->h_name) != 0) {
				freehostent(hp2);
				hp2 = NULL;
			}
			hp = _hpmerge(hp, hp2, errp);
		}
	}
#endif
	return _hpsort(hp);
}

struct hostent *
getipnodebyname(const char *name, int af, int flags, int *errp)
{
	if (af != AF_INET
#ifdef INET6
	    && af != AF_INET6
#endif
		)
	{
		*errp = NO_RECOVERY;
		return NULL;
	}
	return(_getipnodebyname_multi(name, af ,flags, errp));
}

struct hostent *
getipnodebyaddr(const void *src, size_t len, int af, int *errp)
{
	struct hostent *hp;
	int i;
#ifdef INET6
	struct in6_addr addrbuf;
#else
	struct in_addr addrbuf;
#endif

	*errp = HOST_NOT_FOUND;

	switch (af) {
	case AF_INET:
		if (len != sizeof(struct in_addr)) {
			*errp = NO_RECOVERY;
			return NULL;
		}
		if ((long)src & ~(sizeof(struct in_addr) - 1)) {
			memcpy(&addrbuf, src, len);
			src = &addrbuf;
		}
		if (((struct in_addr *)src)->s_addr == 0)
			return NULL;
		break;
#ifdef INET6
	case AF_INET6:
		if (len != sizeof(struct in6_addr)) {
			*errp = NO_RECOVERY;
			return NULL;
		}
		if ((long)src & ~(sizeof(struct in6_addr) / 2 - 1)) {	/*XXX*/
			memcpy(&addrbuf, src, len);
			src = &addrbuf;
		}
		if (IN6_IS_ADDR_UNSPECIFIED((struct in6_addr *)src))
			return NULL;
		if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)src)
		||  IN6_IS_ADDR_V4COMPAT((struct in6_addr *)src)) {
			src = (char *)src +
			    (sizeof(struct in6_addr) - sizeof(struct in_addr));
			af = AF_INET;
			len = sizeof(struct in_addr);
		}
		break;
#endif
	default:
		*errp = NO_RECOVERY;
		return NULL;
	}

	if (!_hostconf_init_done)
		_hostconf_init();
	for (i = 0; i < MAXHOSTCONF; i++) {
		if (_hostconf[i].byaddr
		&&  (hp = (*_hostconf[i].byaddr)(src, len, af, errp)) != NULL)
			return hp;
	}

	return NULL;
}

void
freehostent(struct hostent *ptr)
{
	free(ptr);
}

#if 0

/* XXX: should be deprecated */
struct hostent *
getnodebyname(const char *name, int af, int flags)
{
	return getipnodebyname(name, af, flags, &h_errno);
}

#ifdef __warn_references
__warn_references(getnodebyname,
	"warning: getnodebyname() deprecated, "
	"should use getaddrinfo() or getipnodebyname()");
#endif

struct hostent *
getnodebyaddr(const void *src, size_t len, int af)
{
	return getipnodebyaddr(src, len, af, &h_errno);
}

#ifdef __warn_references
__warn_references(getnodebyaddr,
	"warning: getnodebyaddr() deprecated, "
	"should use getnameinfo() or getipnodebyaddr()");
#endif

#endif

/*
 * Private utility functions
 */

/*
 * _hpcopy: allocate and copy hostent structure
 */
static struct hostent *
_hpcopy(struct hostent *hp, int *errp)
{
	struct hostent *nhp;
	char *cp, **pp;
	int size, addrsize;
	int nalias = 0, naddr = 0;
	int al_off;
	int i;

	if (hp == NULL)
		return hp;

	/* count size to be allocated */
	size = sizeof(struct hostent);
	if (hp->h_name != NULL && *hp->h_name != '\0')
		size += strlen(hp->h_name) + 1;
	if ((pp = hp->h_aliases) != NULL) {
		for (i = 0; *pp != NULL; i++, pp++) {
			if (**pp != '\0') {
				size += strlen(*pp) + 1;
				nalias++;
			}
		}
	}
	/* adjust alignment */
	size = ALIGN(size);
	al_off = size;
	size += sizeof(char *) * (nalias + 1);
	addrsize = ALIGN(hp->h_length);
	if ((pp = hp->h_addr_list) != NULL) {
		while (*pp++ != NULL)
			naddr++;
	}
	size += addrsize * naddr;
	size += sizeof(char *) * (naddr + 1);

	/* copy */
	if ((nhp = (struct hostent *)malloc(size)) == NULL) {
		*errp = TRY_AGAIN;
		return NULL;
	}
	cp = (char *)&nhp[1];
	if (hp->h_name != NULL && *hp->h_name != '\0') {
		nhp->h_name = cp;
		strcpy(cp, hp->h_name);
		cp += strlen(cp) + 1;
	} else
		nhp->h_name = NULL;
	nhp->h_aliases = (char **)((char *)nhp + al_off);
	if ((pp = hp->h_aliases) != NULL) {
		for (i = 0; *pp != NULL; pp++) {
			if (**pp != '\0') {
				nhp->h_aliases[i++] = cp;
				strcpy(cp, *pp);
				cp += strlen(cp) + 1;
			}
		}
	}
	nhp->h_aliases[nalias] = NULL;
	cp = (char *)&nhp->h_aliases[nalias + 1];
	nhp->h_addrtype = hp->h_addrtype;
	nhp->h_length = hp->h_length;
	nhp->h_addr_list = (char **)cp;
	if ((pp = hp->h_addr_list) != NULL) {
		cp = (char *)&nhp->h_addr_list[naddr + 1];
		for (i = 0; *pp != NULL; pp++) {
			nhp->h_addr_list[i++] = cp;
			memcpy(cp, *pp, hp->h_length);
			cp += addrsize;
		}
	}
	nhp->h_addr_list[naddr] = NULL;
	return nhp;
}

/*
 * _hpaddr: construct hostent structure with one address
 */
static struct hostent *
_hpaddr(int af, const char *name, void *addr, int *errp)
{
	struct hostent *hp, hpbuf;
	char *addrs[2];

	hp = &hpbuf;
	hp->h_name = (char *)name;
	hp->h_aliases = NULL;
	hp->h_addrtype = af;
	hp->h_length = ADDRLEN(af);
	hp->h_addr_list = addrs;
	addrs[0] = (char *)addr;
	addrs[1] = NULL;
	return _hpcopy(hp, errp);
}

/*
 * _hpmerge: merge 2 hostent structure, arguments will be freed
 */
static struct hostent *
_hpmerge(struct hostent *hp1, struct hostent *hp2, int *errp)
{
	int i, j;
	int naddr, nalias;
	char **pp;
	struct hostent *hp, hpbuf;
	char *aliases[MAXALIASES + 1], *addrs[MAXADDRS + 1];
	union inx_addr addrbuf[MAXADDRS];

	if (hp1 == NULL)
		return hp2;
	if (hp2 == NULL)
		return hp1;

#define	HP(i)	(i == 1 ? hp1 : hp2)
	hp = &hpbuf;
	hp->h_name = (hp1->h_name != NULL ? hp1->h_name : hp2->h_name);
	hp->h_aliases = aliases;
	nalias = 0;
	for (i = 1; i <= 2; i++) {
		if ((pp = HP(i)->h_aliases) == NULL)
			continue;
		for (; nalias < MAXALIASES && *pp != NULL; pp++) {
			/* check duplicates */
			for (j = 0; j < nalias; j++)
				if (strcasecmp(*pp, aliases[j]) == 0)
					break;
			if (j == nalias)
				aliases[nalias++] = *pp;
		}
	}
	aliases[nalias] = NULL;
#ifdef INET6
	if (hp1->h_length != hp2->h_length) {
		hp->h_addrtype = AF_INET6;
		hp->h_length = sizeof(struct in6_addr);
	} else {
#endif
		hp->h_addrtype = hp1->h_addrtype;
		hp->h_length = hp1->h_length;
#ifdef INET6
	}
#endif
	hp->h_addr_list = addrs;
	naddr = 0;
	for (i = 1; i <= 2; i++) {
		if ((pp = HP(i)->h_addr_list) == NULL)
			continue;
		if (HP(i)->h_length == hp->h_length) {
			while (naddr < MAXADDRS && *pp != NULL)
				addrs[naddr++] = *pp++;
		} else {
			/* copy IPv4 addr as mapped IPv6 addr */
			while (naddr < MAXADDRS && *pp != NULL) {
				MAPADDR(&addrbuf[naddr], *pp++);
				addrs[naddr] = (char *)&addrbuf[naddr];
				naddr++;
			}
		}
	}
	addrs[naddr] = NULL;
	hp = _hpcopy(hp, errp);
	freehostent(hp1);
	freehostent(hp2);
	return hp;
}

/*
 * _hpmapv6: convert IPv4 hostent into IPv4-mapped IPv6 addresses
 */
#ifdef INET6
static struct hostent *
_hpmapv6(struct hostent *hp, int *errp)
{
	struct hostent *hp6;

	if (hp == NULL)
		return NULL;
	if (hp->h_addrtype == AF_INET6)
		return hp;

	/* make dummy hostent to convert IPv6 address */
	if ((hp6 = (struct hostent *)malloc(sizeof(struct hostent))) == NULL) {
		*errp = TRY_AGAIN;
		return NULL;
	}
	hp6->h_name = NULL;
	hp6->h_aliases = NULL;
	hp6->h_addrtype = AF_INET6;
	hp6->h_length = sizeof(struct in6_addr);
	hp6->h_addr_list = NULL;
	return _hpmerge(hp6, hp, errp);
}
#endif

/*
 * _hpsort: sort address by sortlist
 */
static struct hostent *
_hpsort(struct hostent *hp)
{
	int i, j, n;
	u_char *ap, *sp, *mp, **pp;
	char t;
	char order[MAXADDRS];
	int nsort = _res.nsort;

	if (hp == NULL || hp->h_addr_list[1] == NULL || nsort == 0)
		return hp;
	for (i = 0; (ap = (u_char *)hp->h_addr_list[i]); i++) {
		for (j = 0; j < nsort; j++) {
#ifdef INET6
			if (_res_ext.sort_list[j].af != hp->h_addrtype)
				continue;
			sp = (u_char *)&_res_ext.sort_list[j].addr;
			mp = (u_char *)&_res_ext.sort_list[j].mask;
#else
			sp = (u_char *)&_res.sort_list[j].addr;
			mp = (u_char *)&_res.sort_list[j].mask;
#endif
			for (n = 0; n < hp->h_length; n++) {
				if ((ap[n] & mp[n]) != sp[n])
					break;
			}
			if (n == hp->h_length)
				break;
		}
		order[i] = j;
	}
	n = i;
	pp = (u_char **)hp->h_addr_list;
	for (i = 0; i < n - 1; i++) {
		for (j = i + 1; j < n; j++) {
			if (order[i] > order[j]) {
				ap = pp[i];
				pp[i] = pp[j];
				pp[j] = ap;
				t = order[i];
				order[i] = order[j];
				order[j] = t;
			}
		}
	}
	return hp;
}

static char *
_hgetword(char **pp)
{
	char c, *p, *ret;
	const char *sp;
	static const char sep[] = "# \t\n";

	ret = NULL;
	for (p = *pp; (c = *p) != '\0'; p++) {
		for (sp = sep; *sp != '\0'; sp++) {
			if (c == *sp)
				break;
		}
		if (c == '#')
			p[1] = '\0';	/* ignore rest of line */
		if (ret == NULL) {
			if (*sp == '\0')
				ret = p;
		} else {
			if (*sp != '\0') {
				*p++ = '\0';
				break;
			}
		}
	}
	*pp = p;
	if (ret == NULL || *ret == '\0')
		return NULL;
	return ret;
}

/*
 * FILES (/etc/hosts)
 */

static FILE *
_files_open(int *errp)
{
	FILE *fp;
	fp = fopen(_PATH_HOSTS, "r");
	if (fp == NULL)
		*errp = NO_RECOVERY;
	return fp;
}

static struct hostent *
_files_ghbyname(const char *name, int af, int *errp)
{
	int match, nalias;
	char *p, *line, *addrstr, *cname;
	FILE *fp;
	struct hostent *rethp, *hp, hpbuf;
	char *aliases[MAXALIASES + 1], *addrs[2];
	union inx_addr addrbuf;
	char buf[BUFSIZ];
	int af0 = af;

	if ((fp = _files_open(errp)) == NULL)
		return NULL;
	rethp = hp = NULL;

	while (fgets(buf, sizeof(buf), fp)) {
		line = buf;
		if ((addrstr = _hgetword(&line)) == NULL
		||  (cname = _hgetword(&line)) == NULL)
			continue;
		match = (strcasecmp(cname, name) == 0);
		nalias = 0;
		while ((p = _hgetword(&line)) != NULL) {
			if (!match)
				match = (strcasecmp(p, name) == 0);
			if (nalias < MAXALIASES)
				aliases[nalias++] = p;
		}
		if (!match)
			continue;
		switch (af0) {
		case AF_INET:
			if (inet_aton(addrstr, (struct in_addr *)&addrbuf)
			    != 1) {
				*errp = NO_DATA;	/* name found */
				continue;
			}
			af = af0;
			break;
#ifdef INET6
		case AF_INET6:
			if (inet_pton(af, addrstr, &addrbuf) != 1) {
				*errp = NO_DATA;	/* name found */
				continue;
			}
			af = af0;
			break;
#endif
		case AF_UNSPEC:
			if (inet_aton(addrstr, (struct in_addr *)&addrbuf)
			    == 1) {
				af = AF_INET;
				break;
			}
#ifdef INET6
			if (inet_pton(AF_INET6, addrstr, &addrbuf) == 1) {
				af = AF_INET6;
				break; 
			}
#endif
			*errp = NO_DATA;	/* name found */
			continue;
			/* NOTREACHED */
		}
		hp = &hpbuf;
		hp->h_name = cname;
		hp->h_aliases = aliases;
		aliases[nalias] = NULL;
		hp->h_addrtype = af;
		hp->h_length = ADDRLEN(af);
		hp->h_addr_list = addrs;
		addrs[0] = (char *)&addrbuf;
		addrs[1] = NULL;
		hp = _hpcopy(hp, errp);
		rethp = _hpmerge(rethp, hp, errp);
	}
	fclose(fp);
	return rethp;
}

static struct hostent *
_files_ghbyaddr(const void *addr, int addrlen, int af, int *errp)
{
	int nalias;
	char *p, *line;
	FILE *fp;
	struct hostent *hp, hpbuf;
	char *aliases[MAXALIASES + 1], *addrs[2];
	union inx_addr addrbuf;
	char buf[BUFSIZ];

	if ((fp = _files_open(errp)) == NULL)
		return NULL;
	hp = NULL;
	while (fgets(buf, sizeof(buf), fp)) {
		line = buf;
		if ((p = _hgetword(&line)) == NULL
		||  (af == AF_INET
		     ? inet_aton(p, (struct in_addr *)&addrbuf)
		     : inet_pton(af, p, &addrbuf)) != 1
		||  memcmp(addr, &addrbuf, addrlen) != 0
		||  (p = _hgetword(&line)) == NULL)
			continue;
		hp = &hpbuf;
		hp->h_name = p;
		hp->h_aliases = aliases;
		nalias = 0;
		while ((p = _hgetword(&line)) != NULL) {
			if (nalias < MAXALIASES)
				aliases[nalias++] = p;
		}
		aliases[nalias] = NULL;
		hp->h_addrtype = af;
		hp->h_length = addrlen;
		hp->h_addr_list = addrs;
		addrs[0] = (char *)&addrbuf;
		addrs[1] = NULL;
		hp = _hpcopy(hp, errp);
		break;
	}
	fclose(fp);
	return hp;
}

/*
 * NIS
 *
 * XXX actually a hack, these are INET4 specific.
 */
static struct hostent *
_nis_ghbyname(const char *name, int af, int *errp)
{
	struct hostent *hp = NULL;

	if (af == AF_UNSPEC)
		af = AF_INET;
	if (af == AF_INET) {
		hp = _gethostbynisname(name, af);
		if (hp != NULL)
			hp = _hpcopy(hp, errp);
	}
	return (hp);
	
}

static struct hostent *
_nis_ghbyaddr(const void *addr, int addrlen, int af, int *errp)
{
	struct hostent *hp = NULL;

	if (af == AF_INET) {
		hp = _gethostbynisaddr(addr, addrlen, af);
		if (hp != NULL)
			hp = _hpcopy(hp, errp);
	}
	return (hp);
}

#ifdef DEBUG
#define	DNS_ASSERT(X)	if (!(X)) { fprintf(stderr, "ASSFAIL: %s %d: %s\n", __FILE__, __LINE__, #X); goto badanswer; }
#else
#define	DNS_ASSERT(X)	if (!(X)) { goto badanswer; }
#endif

struct __res_type_list {
        SLIST_ENTRY(__res_type_list) rtl_entry;
        int     rtl_type;
};

static struct hostent *
_gethpbyanswer(answer, anslen, qtype, errp)
	const u_char *answer;
	int anslen;
	int qtype;
	int *errp;
{
	int n;
	char tbuf[MAXDNAME+1];
	HEADER *hp;
	const u_char *cp, *eom;
	int type, class, ancount, qdcount;
	u_long ttl;
	char hostbuf[BUFSIZ];
	char *bp;
	char *alist[MAXALIASES];
	char *hlist[MAXADDRS];
	struct hostent hbuf;
	int buflen;
	int na, nh;

	hbuf.h_aliases = alist;
	hbuf.h_addrtype =
#ifdef INET6
		(qtype == T_AAAA) ? AF_INET6 :
#endif
		AF_INET;
	hbuf.h_length = ADDRLEN(hbuf.h_addrtype);
	hbuf.h_addr_list = hlist;
	na = nh = 0;
	hp = (HEADER *)answer;
	eom = answer + anslen;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	DNS_ASSERT(qdcount == 1);
	cp = answer + sizeof(HEADER);
	bp = hostbuf;
	buflen = sizeof(hostbuf);

	n = dn_expand(answer, eom, cp, bp, buflen);
	DNS_ASSERT(n >= 0);
	cp += n + QFIXEDSZ;
	hbuf.h_name = bp;
	n = strlen(bp) + 1;
	bp += n;
	buflen -= n;
	while (ancount-- > 0 && cp < eom) {
		n = dn_expand(answer, eom, cp, bp, buflen);
		DNS_ASSERT(n >= 0);
		cp += n;		/* name */
		type = _getshort(cp);
		cp += 2;		/* type */
		class = _getshort(cp);
		cp += 2;		/* class */
		ttl = _getlong(cp);
		cp += 4;		/* ttl */
		n = _getshort(cp);
		cp += 2;		/* len */
		DNS_ASSERT(class == C_IN);
		switch (type) {
		case T_CNAME:
			if (na >= MAXALIASES-1) {
				cp += n;
				break;
			}
			n = dn_expand(answer, eom, cp, tbuf, sizeof(tbuf));
			DNS_ASSERT(n >= 0);
			cp += n;
			/* alias */
			alist[na++] = bp;
			n = strlen(bp) + 1;
			bp += n;
			buflen -= n;
			/* canon */
			n = strlen(tbuf) + 1;
			DNS_ASSERT(n < buflen);
			strcpy(bp, tbuf);
			hbuf.h_name = bp;
			bp += n;
			buflen -= n;
			break;
		case T_A:
#ifdef INET6
		case T_AAAA:
#endif
			DNS_ASSERT(type == qtype);
			bp = (char *)ALIGN(bp);
			DNS_ASSERT(n == hbuf.h_length);
			DNS_ASSERT(n < buflen);
			if (nh < MAXADDRS-1) {
				hlist[nh++] = bp;
				memcpy(bp, cp, n);
				bp += n;
				buflen -= n;
			}
			cp += n;
			break;
		default:
			DNS_ASSERT(0);
			cp += n;
			break;
		}
	}
	if (nh == 0) {
  badanswer:
		*errp = NO_RECOVERY;
		return NULL;
	}
	alist[na] = NULL;
	hlist[nh] = NULL;
	return _hpcopy(&hbuf, errp);
}

/* res_search() variant with multiple query support. */
static struct hostent *
_res_search_multi(name, rtl, errp)
	const char *name;	/* domain name */
	struct	__res_type_list *rtl; /* list of query types */
	int *errp;
{
	u_char answer[BUFSIZ];	/* buffer to put answer */
	const char *cp, * const *domain;
	struct hostent *hp0 = NULL, *hp;
	u_int dots;
	int trailing_dot, ret, saved_herrno;
	int got_nodata = 0, got_servfail = 0, tried_as_is = 0;
	struct __res_type_list *rtl0 = rtl;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		*errp = NETDB_INTERNAL;
		return (NULL);
	}
	dots = 0;
	for (cp = name; *cp; cp++)
		dots += (*cp == '.');
	trailing_dot = 0;
	if (cp > name && *--cp == '.')
		trailing_dot++;

	/* If there aren't any dots, it could be a user-level alias */
	if (!dots && (cp = hostalias(name)) != NULL) {
		for(rtl = rtl0; rtl != NULL;
		    rtl = SLIST_NEXT(rtl, rtl_entry)) {
			ret = res_query(cp, C_IN, rtl->rtl_type, answer,
					     sizeof(answer));
			if (ret > 0) {
				hp = _gethpbyanswer(answer, ret, rtl->rtl_type,
						    errp);
				hp0 = _hpmerge(hp0, hp, errp);
			}
		}
		return (hp0);
	}

	/*
	 * If there are dots in the name already, let's just give it a try
	 * 'as is'.  The threshold can be set with the "ndots" option.
	 */
	saved_herrno = -1;
	if (dots >= _res.ndots) {
		for(rtl = rtl0; rtl != NULL;
		    rtl = SLIST_NEXT(rtl, rtl_entry)) {
			ret = res_querydomain(name, NULL, C_IN, rtl->rtl_type,
					      answer, sizeof(answer));
			if (ret > 0) {
				hp = _gethpbyanswer(answer, ret, rtl->rtl_type,
						    errp);
				hp0 = _hpmerge(hp0, hp, errp);
			}
		}
		if (hp0 != NULL)
			return (hp0);
		saved_herrno = *errp;
		tried_as_is++;
	}

	/*
	 * We do at least one level of search if
	 *	- there is no dot and RES_DEFNAME is set, or
	 *	- there is at least one dot, there is no trailing dot,
	 *	  and RES_DNSRCH is set.
	 */
	if ((!dots && (_res.options & RES_DEFNAMES)) ||
	    (dots && !trailing_dot && (_res.options & RES_DNSRCH))) {
		int done = 0;

		for (domain = (const char * const *)_res.dnsrch;
		     *domain && !done;
		     domain++) {

			for(rtl = rtl0; rtl != NULL;
			    rtl = SLIST_NEXT(rtl, rtl_entry)) {
				ret = res_querydomain(name, *domain, C_IN,
						      rtl->rtl_type,
						      answer, sizeof(answer));
				if (ret > 0) {
					hp = _gethpbyanswer(answer, ret,
							      rtl->rtl_type,
							      errp);
					hp0 = _hpmerge(hp0, hp, errp);
				}
			}
			if (hp0 != NULL)
				return (hp0);

			/*
			 * If no server present, give up.
			 * If name isn't found in this domain,
			 * keep trying higher domains in the search list
			 * (if that's enabled).
			 * On a NO_DATA error, keep trying, otherwise
			 * a wildcard entry of another type could keep us
			 * from finding this entry higher in the domain.
			 * If we get some other error (negative answer or
			 * server failure), then stop searching up,
			 * but try the input name below in case it's
			 * fully-qualified.
			 */
			if (errno == ECONNREFUSED) {
				*errp = TRY_AGAIN;
				return (NULL);
			}

			switch (*errp) {
			case NO_DATA:
				got_nodata++;
				/* FALLTHROUGH */
			case HOST_NOT_FOUND:
				/* keep trying */
				break;
			case TRY_AGAIN:
				if (((HEADER *)answer)->rcode == SERVFAIL) {
					/* try next search element, if any */
					got_servfail++;
					break;
				}
				/* FALLTHROUGH */
			default:
				/* anything else implies that we're done */
				done++;
			}

			/* if we got here for some reason other than DNSRCH,
			 * we only wanted one iteration of the loop, so stop.
			 */
			if (!(_res.options & RES_DNSRCH))
				done++;
		}
	}

	/*
	 * If we have not already tried the name "as is", do that now.
	 * note that we do this regardless of how many dots were in the
	 * name or whether it ends with a dot unless NOTLDQUERY is set.
	 */
	if (!tried_as_is && (dots || !(_res.options & RES_NOTLDQUERY))) {
		for(rtl = rtl0; rtl != NULL;
		    rtl = SLIST_NEXT(rtl, rtl_entry)) {
			ret = res_querydomain(name, NULL, C_IN, rtl->rtl_type,
					      answer, sizeof(answer));
			if (ret > 0) {
				hp = _gethpbyanswer(answer, ret, rtl->rtl_type,
						    errp);
				hp0 = _hpmerge(hp0, hp, errp);
			}
		}
		if (hp0 != NULL)
			return (hp0);
	}

	/* if we got here, we didn't satisfy the search.
	 * if we did an initial full query, return that query's h_errno
	 * (note that we wouldn't be here if that query had succeeded).
	 * else if we ever got a nodata, send that back as the reason.
	 * else send back meaningless h_errno, that being the one from
	 * the last DNSRCH we did.
	 */
	if (saved_herrno != -1)
		*errp = saved_herrno;
	else if (got_nodata)
		*errp = NO_DATA;
	else if (got_servfail)
		*errp = TRY_AGAIN;
	return (NULL);
}

static struct hostent *
_dns_ghbyname(const char *name, int af, int *errp)
{
	struct __res_type_list *rtl, rtl4;
#ifdef INET6
	struct __res_type_list rtl6;
#endif

#ifdef INET6
	switch (af) {
	case AF_UNSPEC:
		SLIST_NEXT(&rtl4, rtl_entry) = NULL; rtl4.rtl_type = T_A;
		SLIST_NEXT(&rtl6, rtl_entry) = &rtl4; rtl6.rtl_type = T_AAAA;
		rtl = &rtl6;
		break;
	case AF_INET6:
		SLIST_NEXT(&rtl6, rtl_entry) = NULL; rtl6.rtl_type = T_AAAA;
		rtl = &rtl6;
		break;
	case AF_INET:
		SLIST_NEXT(&rtl4, rtl_entry) = NULL; rtl4.rtl_type = T_A;
		rtl = &rtl4;
		break;
	}
#else
	SLIST_NEXT(&rtl4, rtl_entry) = NULL; rtl4.rtl_type = T_A;
	rtl = &rtl4;
#endif
	return(_res_search_multi(name, rtl, errp));
}

static struct hostent *
_dns_ghbyaddr(const void *addr, int addrlen, int af, int *errp)
{
	int n;
	u_char answer[BUFSIZ];
	HEADER *hp;
	u_char c, *cp, *eom;
	int type, class, ancount, qdcount;
	u_long ttl;
	char hostbuf[BUFSIZ];
	char *bp;
	char *alist[MAXALIASES];
	char *hlist[2];
	struct hostent hbuf;
	int buflen;
	int na;
#ifdef INET6
	static const char hex[] = "0123456789abcdef";
#endif

#ifdef INET6
	/* XXX */
	if (af == AF_INET6 && IN6_IS_ADDR_LINKLOCAL((struct in6_addr *)addr))
		return NULL;
#endif

	if ((_res.options & RES_INIT) == 0) {
		if (res_init() < 0) {
			*errp = h_errno;
			return NULL;
		}
	}
	hbuf.h_name = NULL;
	hbuf.h_aliases = alist;
	hbuf.h_addrtype = af;
	hbuf.h_length = addrlen;
	hbuf.h_addr_list = hlist;
	hlist[0] = (char *)addr;
	hlist[1] = NULL;
	na = 0;

	n = 0;
	bp = hostbuf;
	cp = (u_char *)addr+addrlen-1;
	switch (af) {
#ifdef INET6
	case AF_INET6:
		for (; n < addrlen; n++, cp--) {
			c = *cp;
			*bp++ = hex[c & 0xf];
			*bp++ = '.';
			*bp++ = hex[c >> 4];
			*bp++ = '.';
		}
		strcpy(bp, "ip6.int");
		break;
#endif
	default:
		for (; n < addrlen; n++, cp--) {
			c = *cp;
			if (c >= 100)
				*bp++ = '0' + c / 100;
			if (c >= 10)
				*bp++ = '0' + (c % 100) / 10;
			*bp++ = '0' + c % 10;
			*bp++ = '.';
		}
		strcpy(bp, "in-addr.arpa");
		break;
	}

	n = res_query(hostbuf, C_IN, T_PTR, answer, sizeof(answer));
	if (n < 0) {
		*errp = h_errno;
		return NULL;
	}
	hp = (HEADER *)answer;
	eom = answer + n;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	DNS_ASSERT(qdcount == 1);
	cp = answer + sizeof(HEADER);
	bp = hostbuf;
	buflen = sizeof(hostbuf);

	n = dn_expand(answer, eom, cp, bp, buflen);
	DNS_ASSERT(n >= 0);
	cp += n + QFIXEDSZ;
	while (ancount-- > 0 && cp < eom) {
		n = dn_expand(answer, eom, cp, bp, buflen);
		DNS_ASSERT(n >= 0);
		cp += n;		/* name */
		type = _getshort(cp);
		cp += 2;		/* type */
		class = _getshort(cp);
		cp += 2;		/* class */
		ttl = _getlong(cp);
		cp += 4;		/* ttl */
		n = _getshort(cp);
		cp += 2;		/* len */
		DNS_ASSERT(class == C_IN);
		switch (type) {
		case T_PTR:
			n = dn_expand(answer, eom, cp, bp, buflen);
			DNS_ASSERT(n >= 0);
			cp += n;
			if (na >= MAXALIASES-1)
				break;
			if (hbuf.h_name == NULL)
				hbuf.h_name = bp;
			else
				alist[na++] = bp;
			n = strlen(bp) + 1;
			bp += n;
			buflen -= n;
			break;
		case T_CNAME:
			cp += n;
			break;
		default:
  badanswer:
			*errp = NO_RECOVERY;
			return NULL;
		}
	}
	if (hbuf.h_name == NULL) {
		*errp = h_errno;
		return NULL;
	}
	alist[na] = NULL;
	return _hpcopy(&hbuf, errp);
}

#ifdef ICMPNL

/*
 * experimental:
 *	draft-ietf-ipngwg-icmp-namelookups-02.txt
 *	ifindex is assumed to be encoded in addr.
 */
#include <sys/uio.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

struct _icmp_host_cache {
	struct _icmp_host_cache *hc_next;
	int hc_ifindex;
	struct in6_addr hc_addr;
	char *hc_name;
};

static char *
_icmp_fqdn_query(const struct in6_addr *addr, int ifindex)
{
	int s;
	struct icmp6_filter filter;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct in6_pktinfo *pkt;
	char cbuf[256];
	char buf[1024];
	int cc;
	struct icmp6_fqdn_query *fq;
	struct icmp6_fqdn_reply *fr;
	struct _icmp_host_cache *hc;
	struct sockaddr_in6 sin6;
	struct iovec iov;
	fd_set s_fds, fds;
	struct timeval tout;
	int len;
	char *name;
	static int pid;
	static struct _icmp_host_cache *hc_head;

	for (hc = hc_head; hc; hc = hc->hc_next) {
		if (hc->hc_ifindex == ifindex
		&&  IN6_ARE_ADDR_EQUAL(&hc->hc_addr, addr))
			return hc->hc_name;
	}

	if (pid == 0)
		pid = getpid();

	ICMP6_FILTER_SETBLOCKALL(&filter);
	ICMP6_FILTER_SETPASS(ICMP6_FQDN_REPLY, &filter);

	FD_ZERO(&s_fds);
	tout.tv_sec = 0;
	tout.tv_usec = 200000;	/*XXX: 200ms*/

	fq = (struct icmp6_fqdn_query *)buf;
	fq->icmp6_fqdn_type = ICMP6_FQDN_QUERY;
	fq->icmp6_fqdn_code = 0;
	fq->icmp6_fqdn_cksum = 0;
	fq->icmp6_fqdn_id = (u_short)pid;
	fq->icmp6_fqdn_unused = 0;
	fq->icmp6_fqdn_cookie[0] = 0;
	fq->icmp6_fqdn_cookie[1] = 0;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = *addr;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (caddr_t)&sin6;
	msg.msg_namelen = sizeof(sin6);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	iov.iov_base = (caddr_t)buf;
	iov.iov_len = sizeof(struct icmp6_fqdn_query);

	if (ifindex) {
		msg.msg_control = cbuf;
		msg.msg_controllen = sizeof(cbuf);
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
		cmsg->cmsg_level = IPPROTO_IPV6;
		cmsg->cmsg_type = IPV6_PKTINFO;
		pkt = (struct in6_pktinfo *)&cmsg[1];
		memset(&pkt->ipi6_addr, 0, sizeof(struct in6_addr));
		pkt->ipi6_ifindex = ifindex;
		cmsg = CMSG_NXTHDR(&msg, cmsg);
		msg.msg_controllen = (char *)cmsg - cbuf;
	}

	if ((s = socket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0)
		return NULL;
	(void)setsockopt(s, IPPROTO_ICMPV6, ICMP6_FILTER,
			 (char *)&filter, sizeof(filter));
	cc = sendmsg(s, &msg, 0);
	if (cc < 0) {
		_close(s);
		return NULL;
	}
	FD_SET(s, &s_fds);
	for (;;) {
		fds = s_fds;
		if (select(s + 1, &fds, NULL, NULL, &tout) <= 0) {
			_close(s);
			return NULL;
		}
		len = sizeof(sin6);
		cc = recvfrom(s, buf, sizeof(buf), 0,
			      (struct sockaddr *)&sin6, &len);
		if (cc <= 0) {
			_close(s);
			return NULL;
		}
		if (cc < sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr))
			continue;
		if (!IN6_ARE_ADDR_EQUAL(addr, &sin6.sin6_addr))
			continue;
		fr = (struct icmp6_fqdn_reply *)(buf + sizeof(struct ip6_hdr));
		if (fr->icmp6_fqdn_type == ICMP6_FQDN_REPLY)
			break;
	}
	_close(s);
	if (fr->icmp6_fqdn_cookie[1] != 0) {
		/* rfc1788 type */
		name = buf + sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr) + 4;
		len = (buf + cc) - name;
	} else {
		len = fr->icmp6_fqdn_namelen;
		name = fr->icmp6_fqdn_name;
	}
	if (len <= 0)
		return NULL;
	name[len] = 0;

	if ((hc = (struct _icmp_host_cache *)malloc(sizeof(*hc))) == NULL)
		return NULL;
	/* XXX: limit number of cached entries */
	hc->hc_ifindex = ifindex;
	hc->hc_addr = *addr;
	hc->hc_name = strdup(name);
	hc->hc_next = hc_head;
	hc_head = hc;
	return hc->hc_name;
}

static struct hostent *
_icmp_ghbyaddr(const void *addr, int addrlen, int af, int *errp)
{
	char *hname;
	int ifindex;
	struct in6_addr addr6;

	if (af != AF_INET6) {
		/*
		 * Note: rfc1788 defines Who Are You for IPv4,
		 * but no one implements it.
		 */
		return NULL;
	}

	memcpy(&addr6, addr, addrlen);
	ifindex = (addr6.s6_addr[2] << 8) | addr6.s6_addr[3];
	addr6.s6_addr[2] = addr6.s6_addr[3] = 0;

	if (!IN6_IS_ADDR_LINKLOCAL(&addr6))
		return NULL;	/*XXX*/

	if ((hname = _icmp_fqdn_query(&addr6, ifindex)) == NULL)
		return NULL;
	return _hpaddr(af, hname, &addr6, errp);
}
#endif /* ICMPNL */
