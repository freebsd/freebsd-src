/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#ifdef __sgi
# include <sys/ptimers.h>
#endif
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <ctype.h>
#include <syslog.h>
#include "ip_compat.h"
#include "ip_fil.h"
#include "ipf.h"
#include "facpri.h"

#if !defined(lint)
static const char sccsid[] = "@(#)parse.c	1.44 6/5/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$IPFilter: parse.c,v 2.8 1999/12/28 10:49:46 darrenr Exp $";
#endif

extern	struct	ipopt_names	ionames[], secclass[];
extern	int	opts;
extern	int	use_inet6;


char	*proto = NULL;
char	flagset[] = "FSRPAUEC";
u_char	flags[] = { TH_FIN, TH_SYN, TH_RST, TH_PUSH, TH_ACK, TH_URG,
		    TH_ECN, TH_CWR };

void fill6bits __P((int, u_32_t *));
int count6bits __P((u_32_t *));

static	char	thishost[MAXHOSTNAMELEN];


void initparse()
{
	gethostname(thishost, sizeof(thishost));
	thishost[sizeof(thishost) - 1] = '\0';
}


int genmask(msk, mskp)
char *msk;
u_32_t *mskp;
{
	char *endptr = NULL;
#ifdef	USE_INET6
	u_32_t addr;
#endif
	int bits;

	if (index(msk, '.') || index(msk, 'x') || index(msk, ':')) {
		/* possibly of the form xxx.xxx.xxx.xxx
		 * or 0xYYYYYYYY */
#ifdef	USE_INET6
		if (use_inet6) {
			if (inet_pton(AF_INET6, msk, &addr) != 1)
				return -1;
		} else
#endif
		if (inet_aton(msk, (struct in_addr *)mskp) == 0)
			return -1;
	} else {
		/*
		 * set x most significant bits
		 */
		bits = (int)strtol(msk, &endptr, 0);
		if ((*endptr != '\0') ||
		    ((bits > 32) && !use_inet6) || (bits < 0) ||
		    ((bits > 128) && use_inet6))
			return -1;
		if (use_inet6)
			fill6bits(bits, mskp);
		else {
			if (bits == 0)
				*mskp = 0;
			else
				*mskp = htonl(0xffffffff << (32 - bits));
		}
	}
	return 0;
}



void fill6bits(bits, msk)
int bits;
u_32_t *msk;
{
	int i;
	
	for (i = 0; bits >= 32 && i < 4 ; ++i, bits -= 32)
		msk[i] = 0xffffffff;
	
	if (bits > 0 && i < 4)
		msk[i++] = htonl(0xffffffff << (32 - bits));

	while (i < 4)
		msk[i++] = 0;
}


/*
 * returns -1 if neither "hostmask/num" or "hostmask mask addr" are
 * found in the line segments, there is an error processing this information,
 * or there is an error processing ports information.
 */
int	hostmask(seg, sa, msk, pp, cp, tp, linenum)
char	***seg;
u_32_t	*sa, *msk;
u_short	*pp, *tp;
int	*cp;
int	linenum;
{
	struct in_addr maskaddr;
	char *s;

	/*
	 * is it possibly hostname/num ?
	 */
	if ((s = index(**seg, '/')) ||
	    ((s = index(**seg, ':')) && !index(s + 1, ':'))) {
		*s++ = '\0';
		if (genmask(s, msk) == -1) {
			fprintf(stderr, "%d: bad mask (%s)\n", linenum, s);
			return -1;
		}
		if (hostnum(sa, **seg, linenum) == -1) {
			fprintf(stderr, "%d: bad host (%s)\n", linenum, **seg);
			return -1;
		}
		*sa &= *msk;
		(*seg)++;
		return ports(seg, pp, cp, tp, linenum);
	}

	/*
	 * look for extra segments if "mask" found in right spot
	 */
	if (*(*seg+1) && *(*seg+2) && !strcasecmp(*(*seg+1), "mask")) {
		if (hostnum(sa, **seg, linenum) == -1) {
			fprintf(stderr, "%d: bad host (%s)\n", linenum, **seg);
			return -1;
		}
		(*seg)++;
		(*seg)++;
		if (inet_aton(**seg, &maskaddr) == 0) {
			fprintf(stderr, "%d: bad mask (%s)\n", linenum, **seg);
			return -1;
		}
		*msk = maskaddr.s_addr;
		(*seg)++;
		*sa &= *msk;
		return ports(seg, pp, cp, tp, linenum);
	}

	if (**seg) {
		if (hostnum(sa, **seg, linenum) == -1) {
			fprintf(stderr, "%d: bad host (%s)\n", linenum, **seg);
			return -1;
		}
		(*seg)++;
		if (use_inet6) {
			u_32_t k = 0;
			if (sa[0] || sa[1] || sa[2] || sa[3])
				k = 0xffffffff;
			msk[0] = msk[1] = msk[2] = msk[3] = k;
		}
		else
			*msk = *sa ? 0xffffffff : 0;
		return ports(seg, pp, cp, tp, linenum);
	}
	fprintf(stderr, "%d: bad host (%s)\n", linenum, **seg);
	return -1;
}

/*
 * returns an ip address as a long var as a result of either a DNS lookup or
 * straight inet_addr() call
 */
int	hostnum(ipa, host, linenum)
u_32_t	*ipa;
char	*host;
int     linenum;
{
	struct	hostent	*hp;
	struct	netent	*np;
	struct	in_addr	ip;

	if (!strcasecmp("any", host))
		return 0;
#ifdef	USE_INET6
	if (use_inet6) {
		if (inet_pton(AF_INET6, host, ipa) == 1)
			return 0;
		else
			return -1;
	}
#endif
	if (isdigit(*host) && inet_aton(host, &ip)) {
		*ipa = ip.s_addr;
		return 0;
	}

	if (!strcasecmp("<thishost>", host))
		host = thishost;

	if (!(hp = gethostbyname(host))) {
		if (!(np = getnetbyname(host))) {
			fprintf(stderr, "%d: can't resolve hostname: %s\n",
				linenum, host);
			return -1;
		}
		*ipa = htonl(np->n_net);
		return 0;
	}
	*ipa = *(u_32_t *)hp->h_addr;
	return 0;
}


/*
 * check for possible presence of the port fields in the line
 */
int	ports(seg, pp, cp, tp, linenum)
char	***seg;
u_short	*pp, *tp;
int	*cp;
int     linenum;
{
	int	comp = -1;

	if (!*seg || !**seg || !***seg)
		return 0;
	if (!strcasecmp(**seg, "port") && *(*seg + 1) && *(*seg + 2)) {
		(*seg)++;
		if (isalnum(***seg) && *(*seg + 2)) {
			if (portnum(**seg, pp, linenum) == 0)
				return -1;
			(*seg)++;
			if (!strcmp(**seg, "<>"))
				comp = FR_OUTRANGE;
			else if (!strcmp(**seg, "><"))
				comp = FR_INRANGE;
			else {
				fprintf(stderr,
					"%d: unknown range operator (%s)\n",
					linenum, **seg);
				return -1;
			}
			(*seg)++;
			if (**seg == NULL) {
				fprintf(stderr, "%d: missing 2nd port value\n",
					linenum);
				return -1;
			}
			if (portnum(**seg, tp, linenum) == 0)
				return -1;
		} else if (!strcmp(**seg, "=") || !strcasecmp(**seg, "eq"))
			comp = FR_EQUAL;
		else if (!strcmp(**seg, "!=") || !strcasecmp(**seg, "ne"))
			comp = FR_NEQUAL;
		else if (!strcmp(**seg, "<") || !strcasecmp(**seg, "lt"))
			comp = FR_LESST;
		else if (!strcmp(**seg, ">") || !strcasecmp(**seg, "gt"))
			comp = FR_GREATERT;
		else if (!strcmp(**seg, "<=") || !strcasecmp(**seg, "le"))
			comp = FR_LESSTE;
		else if (!strcmp(**seg, ">=") || !strcasecmp(**seg, "ge"))
			comp = FR_GREATERTE;
		else {
			fprintf(stderr, "%d: unknown comparator (%s)\n",
					linenum, **seg);
			return -1;
		}
		if (comp != FR_OUTRANGE && comp != FR_INRANGE) {
			(*seg)++;
			if (portnum(**seg, pp, linenum) == 0)
				return -1;
		}
		*cp = comp;
		(*seg)++;
	}
	return 0;
}


/*
 * find the port number given by the name, either from getservbyname() or
 * straight atoi(). Return 1 on success, 0 on failure
 */
int	portnum(name, port, linenum)
char	*name;
u_short	*port;
int     linenum;
{
	struct	servent	*sp, *sp2;
	u_short	p1 = 0;
	int i;

	if (isdigit(*name)) {
		if (ratoi(name, &i, 0, USHRT_MAX)) {
			*port = (u_short)i;
			return 1;
		}
		fprintf(stderr, "%d: unknown port \"%s\"\n", linenum, name);
		return 0;
	}
	if (proto != NULL && strcasecmp(proto, "tcp/udp") != 0) {
		sp = getservbyname(name, proto);
		if (sp) {
			*port = ntohs(sp->s_port);
			return 1;
		}
		fprintf(stderr, "%d: unknown service \"%s\".\n", linenum, name);
		return 0;
	}
	sp = getservbyname(name, "tcp");
	if (sp)
		p1 = sp->s_port;
	sp2 = getservbyname(name, "udp");
	if (!sp || !sp2) {
		fprintf(stderr, "%d: unknown tcp/udp service \"%s\".\n",
			linenum, name);
		return 0;
	}
	if (p1 != sp2->s_port) {
		fprintf(stderr, "%d: %s %d/tcp is a different port to ",
			linenum, name, p1);
		fprintf(stderr, "%d: %s %d/udp\n", linenum, name, sp->s_port);
		return 0;
	}
	*port = ntohs(p1);
	return 1;
}


u_char tcp_flags(flgs, mask, linenum)
char *flgs;
u_char *mask;
int    linenum;
{
	u_char tcpf = 0, tcpfm = 0, *fp = &tcpf;
	char *s, *t;

	if (*flgs == '0') {
		s = strchr(flgs, '/');
		if (s)
			*s++ = '\0';
		tcpf = strtol(flgs, NULL, 0);
		fp = &tcpfm;
	} else
		s = flgs;

	for (; *s; s++) {
		if (*s == '/' && fp == &tcpf) {
			fp = &tcpfm;
			if (*(s + 1) == '0')
				break;
			continue;
		}
		if (!(t = index(flagset, *s))) {
			fprintf(stderr, "%d: unknown flag (%c)\n", linenum, *s);
			return 0;
		}
		*fp |= flags[t - flagset];
	}

	if (s && *s == '0')
		tcpfm = strtol(s, NULL, 0);

	if (!tcpfm) {
		if (tcpf == TH_SYN)
			tcpfm = 0xff & ~(TH_ECN|TH_CWR);
		else
			tcpfm = 0xff & ~(TH_ECN);
	}
	*mask = tcpfm;
	return tcpf;
}


/*
 * count consecutive 1's in bit mask.  If the mask generated by counting
 * consecutive 1's is different to that passed, return -1, else return #
 * of bits.
 */
int	countbits(ip)
u_32_t	ip;
{
	u_32_t	ipn;
	int	cnt = 0, i, j;

	ip = ipn = ntohl(ip);
	for (i = 32; i; i--, ipn *= 2)
		if (ipn & 0x80000000)
			cnt++;
		else
			break;
	ipn = 0;
	for (i = 32, j = cnt; i; i--, j--) {
		ipn *= 2;
		if (j > 0)
			ipn++;
	}
	if (ipn == ip)
		return cnt;
	return -1;
}


int count6bits(msk)
u_32_t *msk;
{
	int i = 0, k;
	u_32_t j;

	for (k = 3; k >= 0; k--)
		if (msk[k] == 0xffffffff)
			i += 32;
		else {
			for (j = msk[k]; j; j <<= 1)
				if (j & 0x80000000)
					i++;
		}
	return i;
}


char	*portname(pr, port)
int	pr, port;
{
	static	char	buf[32];
	struct	protoent	*p = NULL;
	struct	servent	*sv = NULL, *sv1 = NULL;

	if (pr == -1) {
		if ((sv = getservbyport(htons(port), "tcp"))) {
			strncpy(buf, sv->s_name, sizeof(buf)-1);
			buf[sizeof(buf)-1] = '\0';
			sv1 = getservbyport(htons(port), "udp");
			sv = strncasecmp(buf, sv->s_name, strlen(buf)) ?
			     NULL : sv1;
		}
		if (sv)
			return buf;
	} else if (pr && (p = getprotobynumber(pr))) {
		if ((sv = getservbyport(htons(port), p->p_name))) {
			strncpy(buf, sv->s_name, sizeof(buf)-1);
			buf[sizeof(buf)-1] = '\0';
			return buf;
		}
	}

	(void) sprintf(buf, "%d", port);
	return buf;
}


int	ratoi(ps, pi, min, max)
char 	*ps;
int	*pi, min, max;
{
	int i;
	char *pe;

	i = (int)strtol(ps, &pe, 0);
	if (*pe != '\0' || i < min || i > max)
		return 0;
	*pi = i;
	return 1;
}


int	ratoui(ps, pi, min, max)
char 	*ps;
u_int	*pi, min, max;
{
	u_int i;
	char *pe;

	i = (u_int)strtol(ps, &pe, 0);
	if (*pe != '\0' || i < min || i > max)
		return 0;
	*pi = i;
	return 1;
}


void	printhostmask(v, addr, mask)
int	v;
u_32_t	*addr, *mask;
{
	struct in_addr ipa;
	int ones;

#ifdef USE_INET6
	if (v == 6) {
		ones = count6bits(mask);
		if (ones == 0 && !addr[0] && !addr[1] && !addr[2] && !addr[3])
			printf("any");
		else {
			char ipbuf[64];
			printf("%s/%d",
			       inet_ntop(AF_INET6, addr, ipbuf, sizeof(ipbuf)),
			       ones);
		}
	}
	else
#endif
	if (!*addr && !*mask)
		printf("any");
	else {
		ipa.s_addr = *addr;
		printf("%s", inet_ntoa(ipa));
		if ((ones = countbits(*mask)) == -1) {
			ipa.s_addr = *mask;
			printf("/%s", inet_ntoa(ipa));
		} else
			printf("/%d", ones);
	}
}


void	printportcmp(pr, frp)
int	pr;
frpcmp_t	*frp;
{
	static char *pcmp1[] = { "*", "=", "!=", "<", ">", "<=", ">=",
				 "<>", "><"};

	if (frp->frp_cmp == FR_INRANGE || frp->frp_cmp == FR_OUTRANGE)
		printf(" port %d %s %d", frp->frp_port,
			     pcmp1[frp->frp_cmp], frp->frp_top);
	else
		printf(" port %s %s", pcmp1[frp->frp_cmp],
			     portname(pr, frp->frp_port));
}


void printbuf(buf, len, zend)
char *buf;
int len, zend;
{
	char *s, c;
	int i;

	for (s = buf, i = len; i; i--) {
		c = *s++;
		if (isprint(c))
			putchar(c);
		else
			printf("\\%03o", c);
		if ((c == '\0') && zend)
			break;
	}
}



char *hostname(v, ip)
int v;
void *ip;
{
#ifdef  USE_INET6
	static char hostbuf[MAXHOSTNAMELEN+1];
#endif
	struct in_addr ipa;

	if (v == 4) {
		ipa.s_addr = *(u_32_t *)ip;
		return inet_ntoa(ipa);
	}
#ifdef  USE_INET6
	(void) inet_ntop(AF_INET6, ip, hostbuf, sizeof(hostbuf) - 1);
	hostbuf[MAXHOSTNAMELEN] = '\0';
	return hostbuf;
#else
	return "IPv6";
#endif
}
