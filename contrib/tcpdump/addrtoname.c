/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  Internet, ethernet, port, and protocol string to address
 *  and address to string conversion routines
 *
 * $FreeBSD$
 */
#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/addrtoname.c,v 1.69.2.1 2001/01/17 18:29:58 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

struct mbuf;
struct rtentry;
#include <net/if.h>

#include <netinet/in.h>
#ifdef HAVE_NETINET_IF_ETHER_H
#include <netinet/if_ether.h>
#endif

#include <arpa/inet.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <pcap-namedb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "interface.h"
#include "addrtoname.h"
#include "llc.h"
#include "savestr.h"
#include "setsignal.h"

/* Forwards */
static RETSIGTYPE nohostname(int);

/*
 * hash tables for whatever-to-name translations
 */

#define HASHNAMESIZE 4096

struct hnamemem {
	u_int32_t addr;
	char *name;
	struct hnamemem *nxt;
};

struct hnamemem hnametable[HASHNAMESIZE];
struct hnamemem tporttable[HASHNAMESIZE];
struct hnamemem uporttable[HASHNAMESIZE];
struct hnamemem eprototable[HASHNAMESIZE];
struct hnamemem dnaddrtable[HASHNAMESIZE];
struct hnamemem llcsaptable[HASHNAMESIZE];

#ifdef INET6
struct h6namemem {
	struct in6_addr addr;
	char *name;
	struct h6namemem *nxt;
};

struct h6namemem h6nametable[HASHNAMESIZE];
#endif /* INET6 */

struct enamemem {
	u_short e_addr0;
	u_short e_addr1;
	u_short e_addr2;
	char *e_name;
	u_char *e_nsap;			/* used only for nsaptable[] */
	struct enamemem *e_nxt;
};

struct enamemem enametable[HASHNAMESIZE];
struct enamemem nsaptable[HASHNAMESIZE];

struct protoidmem {
	u_int32_t p_oui;
	u_short p_proto;
	char *p_name;
	struct protoidmem *p_nxt;
};

struct protoidmem protoidtable[HASHNAMESIZE];

/*
 * A faster replacement for inet_ntoa().
 */
char *
intoa(u_int32_t addr)
{
	register char *cp;
	register u_int byte;
	register int n;
	static char buf[sizeof(".xxx.xxx.xxx.xxx")];

	NTOHL(addr);
	cp = &buf[sizeof buf];
	*--cp = '\0';

	n = 4;
	do {
		byte = addr & 0xff;
		*--cp = byte % 10 + '0';
		byte /= 10;
		if (byte > 0) {
			*--cp = byte % 10 + '0';
			byte /= 10;
			if (byte > 0)
				*--cp = byte + '0';
		}
		*--cp = '.';
		addr >>= 8;
	} while (--n > 0);

	return cp + 1;
}

static u_int32_t f_netmask;
static u_int32_t f_localnet;
static u_int32_t netmask;

/*
 * "getname" is written in this atrocious way to make sure we don't
 * wait forever while trying to get hostnames from yp.
 */
#include <setjmp.h>

jmp_buf getname_env;

static RETSIGTYPE
nohostname(int signo)
{
	longjmp(getname_env, 1);
}

/*
 * Return a name for the IP address pointed to by ap.  This address
 * is assumed to be in network byte order.
 */
char *
getname(const u_char *ap)
{
	register struct hostent *hp;
	u_int32_t addr;
	static struct hnamemem *p;		/* static for longjmp() */

#ifndef LBL_ALIGN
	addr = *(const u_int32_t *)ap;
#else
	memcpy(&addr, ap, sizeof(addr));
#endif
	p = &hnametable[addr & (HASHNAMESIZE-1)];
	for (; p->nxt; p = p->nxt) {
		if (p->addr == addr)
			return (p->name);
	}
	p->addr = addr;
	p->nxt = newhnamemem();

	/*
	 * Only print names when:
	 *	(1) -n was not given.
	 *      (2) Address is foreign and -f was given. (If -f was not
	 *	    give, f_netmask and f_local are 0 and the test
	 *	    evaluates to true)
	 *      (3) -a was given or the host portion is not all ones
	 *          nor all zeros (i.e. not a network or broadcast address)
	 */
	if (!nflag &&
	    (addr & f_netmask) == f_localnet &&
	    (aflag ||
	    !((addr & ~netmask) == 0 || (addr | netmask) == 0xffffffff))) {
		if (!setjmp(getname_env)) {
			(void)setsignal(SIGALRM, nohostname);
			(void)alarm(20);
			hp = gethostbyaddr((char *)&addr, 4, AF_INET);
			(void)alarm(0);
			if (hp) {
				char *dotp;

				p->name = savestr(hp->h_name);
				if (Nflag) {
					/* Remove domain qualifications */
					dotp = strchr(p->name, '.');
					if (dotp)
						*dotp = '\0';
				}
				return (p->name);
			}
		}
	}
	p->name = savestr(intoa(addr));
	return (p->name);
}

#ifdef INET6
/*
 * Return a name for the IP6 address pointed to by ap.  This address
 * is assumed to be in network byte order.
 */
char *
getname6(const u_char *ap)
{
	register struct hostent *hp;
	struct in6_addr addr;
	static struct h6namemem *p;		/* static for longjmp() */
	register char *cp;
	char ntop_buf[INET6_ADDRSTRLEN];

	memcpy(&addr, ap, sizeof(addr));
	p = &h6nametable[*(u_int16_t *)&addr.s6_addr[14] & (HASHNAMESIZE-1)];
	for (; p->nxt; p = p->nxt) {
		if (memcmp(&p->addr, &addr, sizeof(addr)) == 0)
			return (p->name);
	}
	p->addr = addr;
	p->nxt = newh6namemem();

	/*
	 * Only print names when:
	 *	(1) -n was not given.
	 *      (2) Address is foreign and -f was given. (If -f was not
	 *	    give, f_netmask and f_local are 0 and the test
	 *	    evaluates to true)
	 *      (3) -a was given or the host portion is not all ones
	 *          nor all zeros (i.e. not a network or broadcast address)
	 */
	if (!nflag
#if 0
	&&
	    (addr & f_netmask) == f_localnet &&
	    (aflag ||
	    !((addr & ~netmask) == 0 || (addr | netmask) == 0xffffffff))
#endif
	    ) {
		if (!setjmp(getname_env)) {
			(void)setsignal(SIGALRM, nohostname);
			(void)alarm(20);
			hp = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET6);
			(void)alarm(0);
			if (hp) {
				char *dotp;

				p->name = savestr(hp->h_name);
				if (Nflag) {
					/* Remove domain qualifications */
					dotp = strchr(p->name, '.');
					if (dotp)
						*dotp = '\0';
				}
				return (p->name);
			}
		}
	}
	cp = (char *)inet_ntop(AF_INET6, &addr, ntop_buf, sizeof(ntop_buf));
	p->name = savestr(cp);
	return (p->name);
}
#endif /* INET6 */

static char hex[] = "0123456789abcdef";


/* Find the hash node that corresponds the ether address 'ep' */

static inline struct enamemem *
lookup_emem(const u_char *ep)
{
	register u_int i, j, k;
	struct enamemem *tp;

	k = (ep[0] << 8) | ep[1];
	j = (ep[2] << 8) | ep[3];
	i = (ep[4] << 8) | ep[5];

	tp = &enametable[(i ^ j) & (HASHNAMESIZE-1)];
	while (tp->e_nxt)
		if (tp->e_addr0 == i &&
		    tp->e_addr1 == j &&
		    tp->e_addr2 == k)
			return tp;
		else
			tp = tp->e_nxt;
	tp->e_addr0 = i;
	tp->e_addr1 = j;
	tp->e_addr2 = k;
	tp->e_nxt = (struct enamemem *)calloc(1, sizeof(*tp));
	if (tp->e_nxt == NULL)
		error("lookup_emem: calloc");

	return tp;
}

/* Find the hash node that corresponds the NSAP 'nsap' */

static inline struct enamemem *
lookup_nsap(register const u_char *nsap)
{
	register u_int i, j, k;
	int nlen = *nsap;
	struct enamemem *tp;
	const u_char *ensap = nsap + nlen - 6;

	if (nlen > 6) {
		k = (ensap[0] << 8) | ensap[1];
		j = (ensap[2] << 8) | ensap[3];
		i = (ensap[4] << 8) | ensap[5];
	}
	else
		i = j = k = 0;

	tp = &nsaptable[(i ^ j) & (HASHNAMESIZE-1)];
	while (tp->e_nxt)
		if (tp->e_addr0 == i &&
		    tp->e_addr1 == j &&
		    tp->e_addr2 == k &&
		    tp->e_nsap[0] == nlen &&
		    memcmp((char *)&(nsap[1]),
			(char *)&(tp->e_nsap[1]), nlen) == 0)
			return tp;
		else
			tp = tp->e_nxt;
	tp->e_addr0 = i;
	tp->e_addr1 = j;
	tp->e_addr2 = k;
	tp->e_nsap = (u_char *)malloc(nlen + 1);
	if (tp->e_nsap == NULL)
		error("lookup_nsap: malloc");
	memcpy((char *)tp->e_nsap, (char *)nsap, nlen + 1);
	tp->e_nxt = (struct enamemem *)calloc(1, sizeof(*tp));
	if (tp->e_nxt == NULL)
		error("lookup_nsap: calloc");

	return tp;
}

/* Find the hash node that corresponds the protoid 'pi'. */

static inline struct protoidmem *
lookup_protoid(const u_char *pi)
{
	register u_int i, j;
	struct protoidmem *tp;

	/* 5 octets won't be aligned */
	i = (((pi[0] << 8) + pi[1]) << 8) + pi[2];
	j =   (pi[3] << 8) + pi[4];
	/* XXX should be endian-insensitive, but do big-endian testing  XXX */

	tp = &protoidtable[(i ^ j) & (HASHNAMESIZE-1)];
	while (tp->p_nxt)
		if (tp->p_oui == i && tp->p_proto == j)
			return tp;
		else
			tp = tp->p_nxt;
	tp->p_oui = i;
	tp->p_proto = j;
	tp->p_nxt = (struct protoidmem *)calloc(1, sizeof(*tp));
	if (tp->p_nxt == NULL)
		error("lookup_protoid: calloc");

	return tp;
}

char *
etheraddr_string(register const u_char *ep)
{
	register u_int i, j;
	register char *cp;
	register struct enamemem *tp;
	char buf[sizeof("00:00:00:00:00:00")];

	tp = lookup_emem(ep);
	if (tp->e_name)
		return (tp->e_name);
#ifdef HAVE_ETHER_NTOHOST
	if (!nflag) {
		char buf[128];
		if (ether_ntohost(buf, (struct ether_addr *)ep) == 0) {
			tp->e_name = savestr(buf);
			return (tp->e_name);
		}
	}
#endif
	cp = buf;
	if ((j = *ep >> 4) != 0)
		*cp++ = hex[j];
	*cp++ = hex[*ep++ & 0xf];
	for (i = 5; (int)--i >= 0;) {
		*cp++ = ':';
		if ((j = *ep >> 4) != 0)
			*cp++ = hex[j];
		*cp++ = hex[*ep++ & 0xf];
	}
	*cp = '\0';
	tp->e_name = savestr(buf);
	return (tp->e_name);
}

char *
etherproto_string(u_short port)
{
	register char *cp;
	register struct hnamemem *tp;
	register u_int32_t i = port;
	char buf[sizeof("0000")];

	for (tp = &eprototable[i & (HASHNAMESIZE-1)]; tp->nxt; tp = tp->nxt)
		if (tp->addr == i)
			return (tp->name);

	tp->addr = i;
	tp->nxt = newhnamemem();

	cp = buf;
	NTOHS(port);
	*cp++ = hex[port >> 12 & 0xf];
	*cp++ = hex[port >> 8 & 0xf];
	*cp++ = hex[port >> 4 & 0xf];
	*cp++ = hex[port & 0xf];
	*cp++ = '\0';
	tp->name = savestr(buf);
	return (tp->name);
}

char *
protoid_string(register const u_char *pi)
{
	register u_int i, j;
	register char *cp;
	register struct protoidmem *tp;
	char buf[sizeof("00:00:00:00:00")];

	tp = lookup_protoid(pi);
	if (tp->p_name)
		return tp->p_name;

	cp = buf;
	if ((j = *pi >> 4) != 0)
		*cp++ = hex[j];
	*cp++ = hex[*pi++ & 0xf];
	for (i = 4; (int)--i >= 0;) {
		*cp++ = ':';
		if ((j = *pi >> 4) != 0)
			*cp++ = hex[j];
		*cp++ = hex[*pi++ & 0xf];
	}
	*cp = '\0';
	tp->p_name = savestr(buf);
	return (tp->p_name);
}

char *
llcsap_string(u_char sap)
{
	register struct hnamemem *tp;
	register u_int32_t i = sap;
	char buf[sizeof("sap 00")];

	for (tp = &llcsaptable[i & (HASHNAMESIZE-1)]; tp->nxt; tp = tp->nxt)
		if (tp->addr == i)
			return (tp->name);

	tp->addr = i;
	tp->nxt = newhnamemem();

	snprintf(buf, sizeof(buf), "sap %02x", sap & 0xff);
	tp->name = savestr(buf);
	return (tp->name);
}

char *
isonsap_string(const u_char *nsap)
{
	register u_int i, nlen = nsap[0];
	register char *cp;
	register struct enamemem *tp;

	tp = lookup_nsap(nsap);
	if (tp->e_name)
		return tp->e_name;

	tp->e_name = cp = (char *)malloc(nlen * 2 + 2 + (nlen>>1));
	if (cp == NULL)
		error("isonsap_string: malloc");

	nsap++;
	for (i = 0; i < nlen; i++) {
		*cp++ = hex[*nsap >> 4];
		*cp++ = hex[*nsap++ & 0xf];
		if (((i & 1) == 0) && (i + 1 < nlen))
			*cp++ = '.';
	}
	*cp = '\0';
	return (tp->e_name);
}

char *
tcpport_string(u_short port)
{
	register struct hnamemem *tp;
	register u_int32_t i = port;
	char buf[sizeof("00000")];

	for (tp = &tporttable[i & (HASHNAMESIZE-1)]; tp->nxt; tp = tp->nxt)
		if (tp->addr == i)
			return (tp->name);

	tp->addr = i;
	tp->nxt = newhnamemem();

	(void)snprintf(buf, sizeof(buf), "%u", i);
	tp->name = savestr(buf);
	return (tp->name);
}

char *
udpport_string(register u_short port)
{
	register struct hnamemem *tp;
	register u_int32_t i = port;
	char buf[sizeof("00000")];

	for (tp = &uporttable[i & (HASHNAMESIZE-1)]; tp->nxt; tp = tp->nxt)
		if (tp->addr == i)
			return (tp->name);

	tp->addr = i;
	tp->nxt = newhnamemem();

	(void)snprintf(buf, sizeof(buf), "%u", i);
	tp->name = savestr(buf);
	return (tp->name);
}

static void
init_servarray(void)
{
	struct servent *sv;
	register struct hnamemem *table;
	register int i;
	char buf[sizeof("0000000000")];

	while ((sv = getservent()) != NULL) {
		int port = ntohs(sv->s_port);
		i = port & (HASHNAMESIZE-1);
		if (strcmp(sv->s_proto, "tcp") == 0)
			table = &tporttable[i];
		else if (strcmp(sv->s_proto, "udp") == 0)
			table = &uporttable[i];
		else
			continue;

		while (table->name)
			table = table->nxt;
		if (nflag) {
			(void)snprintf(buf, sizeof(buf), "%d", port);
			table->name = savestr(buf);
		} else
			table->name = savestr(sv->s_name);
		table->addr = port;
		table->nxt = newhnamemem();
	}
	endservent();
}

/*XXX from libbpfc.a */
extern struct eproto {
	char *s;
	u_short p;
} eproto_db[];

static void
init_eprotoarray(void)
{
	register int i;
	register struct hnamemem *table;

	for (i = 0; eproto_db[i].s; i++) {
		int j = ntohs(eproto_db[i].p) & (HASHNAMESIZE-1);
		table = &eprototable[j];
		while (table->name)
			table = table->nxt;
		table->name = eproto_db[i].s;
		table->addr = ntohs(eproto_db[i].p);
		table->nxt = newhnamemem();
	}
}

/*
 * SNAP proto IDs with org code 0:0:0 are actually encapsulated Ethernet
 * types.
 */
static void
init_protoidarray(void)
{
	register int i;
	register struct protoidmem *tp;
	u_char protoid[5];

	protoid[0] = 0;
	protoid[1] = 0;
	protoid[2] = 0;
	for (i = 0; eproto_db[i].s; i++) {
		u_short etype = htons(eproto_db[i].p);

		memcpy((char *)&protoid[3], (char *)&etype, 2);
		tp = lookup_protoid(protoid);
		tp->p_name = savestr(eproto_db[i].s);
	}
}

static struct etherlist {
	u_char addr[6];
	char *name;
} etherlist[] = {
	{{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, "Broadcast" },
	{{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, NULL }
};

/*
 * Initialize the ethers hash table.  We take two different approaches
 * depending on whether or not the system provides the ethers name
 * service.  If it does, we just wire in a few names at startup,
 * and etheraddr_string() fills in the table on demand.  If it doesn't,
 * then we suck in the entire /etc/ethers file at startup.  The idea
 * is that parsing the local file will be fast, but spinning through
 * all the ethers entries via NIS & next_etherent might be very slow.
 *
 * XXX pcap_next_etherent doesn't belong in the pcap interface, but
 * since the pcap module already does name-to-address translation,
 * it's already does most of the work for the ethernet address-to-name
 * translation, so we just pcap_next_etherent as a convenience.
 */
static void
init_etherarray(void)
{
	register struct etherlist *el;
	register struct enamemem *tp;
#ifdef HAVE_ETHER_NTOHOST
	char name[256];
#else
	register struct pcap_etherent *ep;
	register FILE *fp;

	/* Suck in entire ethers file */
	fp = fopen(PCAP_ETHERS_FILE, "r");
	if (fp != NULL) {
		while ((ep = pcap_next_etherent(fp)) != NULL) {
			tp = lookup_emem(ep->addr);
			tp->e_name = savestr(ep->name);
		}
		(void)fclose(fp);
	}
#endif

	/* Hardwire some ethernet names */
	for (el = etherlist; el->name != NULL; ++el) {
		tp = lookup_emem(el->addr);
		/* Don't override existing name */
		if (tp->e_name != NULL)
			continue;

#ifdef HAVE_ETHER_NTOHOST
                /* Use yp/nis version of name if available */
                if (ether_ntohost(name, (struct ether_addr *)el->addr) == 0) {
                        tp->e_name = savestr(name);
			continue;
		}
#endif
		tp->e_name = el->name;
	}
}

static struct tok llcsap_db[] = {
	{ LLCSAP_NULL,		"null" },
	{ LLCSAP_8021B_I,	"802.1b-gsap" },
	{ LLCSAP_8021B_G,	"802.1b-isap" },
	{ LLCSAP_IP,		"ip-sap" },
	{ LLCSAP_PROWAYNM,	"proway-nm" },
	{ LLCSAP_8021D,		"802.1d" },
	{ LLCSAP_RS511,		"eia-rs511" },
	{ LLCSAP_ISO8208,	"x.25/llc2" },
	{ LLCSAP_PROWAY,	"proway" },
	{ LLCSAP_ISONS,		"iso-clns" },
	{ LLCSAP_GLOBAL,	"global" },
	{ 0,			NULL }
};

static void
init_llcsaparray(void)
{
	register int i;
	register struct hnamemem *table;

	for (i = 0; llcsap_db[i].s != NULL; i++) {
		table = &llcsaptable[llcsap_db[i].v];
		while (table->name)
			table = table->nxt;
		table->name = llcsap_db[i].s;
		table->addr = llcsap_db[i].v;
		table->nxt = newhnamemem();
	}
}

/*
 * Initialize the address to name translation machinery.  We map all
 * non-local IP addresses to numeric addresses if fflag is true (i.e.,
 * to prevent blocking on the nameserver).  localnet is the IP address
 * of the local network.  mask is its subnet mask.
 */
void
init_addrtoname(u_int32_t localnet, u_int32_t mask)
{
	netmask = mask;
	if (fflag) {
		f_localnet = localnet;
		f_netmask = mask;
	}
	if (nflag)
		/*
		 * Simplest way to suppress names.
		 */
		return;

	init_etherarray();
	init_servarray();
	init_eprotoarray();
	init_llcsaparray();
	init_protoidarray();
}

char *
dnaddr_string(u_short dnaddr)
{
	register struct hnamemem *tp;

	for (tp = &dnaddrtable[dnaddr & (HASHNAMESIZE-1)]; tp->nxt != 0;
	     tp = tp->nxt)
		if (tp->addr == dnaddr)
			return (tp->name);

	tp->addr = dnaddr;
	tp->nxt = newhnamemem();
	if (nflag)
		tp->name = dnnum_string(dnaddr);
	else
		tp->name = dnname_string(dnaddr);

	return(tp->name);
}

/* Return a zero'ed hnamemem struct and cuts down on calloc() overhead */
struct hnamemem *
newhnamemem(void)
{
	register struct hnamemem *p;
	static struct hnamemem *ptr = NULL;
	static u_int num = 0;

	if (num  <= 0) {
		num = 64;
		ptr = (struct hnamemem *)calloc(num, sizeof (*ptr));
		if (ptr == NULL)
			error("newhnamemem: calloc");
	}
	--num;
	p = ptr++;
	return (p);
}

#ifdef INET6
/* Return a zero'ed h6namemem struct and cuts down on calloc() overhead */
struct h6namemem *
newh6namemem(void)
{
	register struct h6namemem *p;
	static struct h6namemem *ptr = NULL;
	static u_int num = 0;

	if (num  <= 0) {
		num = 64;
		ptr = (struct h6namemem *)calloc(num, sizeof (*ptr));
		if (ptr == NULL)
			error("newh6namemem: calloc");
	}
	--num;
	p = ptr++;
	return (p);
}
#endif /* INET6 */
