/* $FreeBSD: src/contrib/ipfilter/ipmon.c,v 1.5.2.2 2000/07/19 23:00:44 darrenr Exp $ */
/*
 * Copyright (C) 1993-2000 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ipmon.c	1.21 6/5/96 (C)1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipmon.c,v 2.12.2.2 2000/07/15 14:50:06 darrenr Exp $";
#endif

#ifndef SOLARIS
#define SOLARIS (defined(__SVR4) || defined(__svr4__)) && defined(sun)
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#if !defined(__SVR4) && !defined(__svr4__)
# if (__FreeBSD_version >= 300000)
#  include <sys/dirent.h>
# else
#  include <sys/dir.h>
# endif
#else
# include <sys/filio.h>
# include <sys/byteorder.h>
#endif
#include <strings.h>
#include <signal.h>
#include <stdlib.h>
#include <stddef.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/tcp_fsm.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <sys/uio.h>
#ifndef linux
# include <sys/protosw.h>
# include <netinet/ip_var.h>
#endif

#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>

#include <ctype.h>
#include <syslog.h>

#include "netinet/ip_compat.h"
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_state.h"


#if	defined(sun) && !defined(SOLARIS2)
#define	STRERROR(x)	sys_errlist[x]
extern	char	*sys_errlist[];
#else
#define	STRERROR(x)	strerror(x)
#endif


struct	flags {
	int	value;
	char	flag;
};

struct	flags	tcpfl[] = {
	{ TH_ACK, 'A' },
	{ TH_RST, 'R' },
	{ TH_SYN, 'S' },
	{ TH_FIN, 'F' },
	{ TH_URG, 'U' },
	{ TH_PUSH,'P' },
	{ 0, '\0' }
};

#if SOLARIS
static	char	*pidfile = "/etc/opt/ipf/ipmon.pid";
#else
# if BSD >= 199306
static	char	*pidfile = "/var/run/ipmon.pid";
# else
static	char	*pidfile = "/etc/ipmon.pid";
# endif
#endif

static	char	line[2048];
static	int	opts = 0;
static	FILE	*newlog = NULL;
static	char	*logfile = NULL;
static	int	donehup = 0;
static	void	usage __P((char *));
static	void	handlehup __P((int));
static	void	flushlogs __P((char *, FILE *));
static	void	print_log __P((int, FILE *, char *, int));
static	void	print_ipflog __P((FILE *, char *, int));
static	void	print_natlog __P((FILE *, char *, int));
static	void	print_statelog __P((FILE *, char *, int));
static	void	dumphex __P((FILE *, u_char *, int));
static	int	read_log __P((int, int *, char *, int));
static	void	write_pid __P((char *));

char	*hostname __P((int, int, u_32_t *));
char	*portname __P((int, char *, u_int));
int	main __P((int, char *[]));

static	void	logopts __P((int, char *));
static	void	init_tabs __P((void));
static	char	*getproto __P((u_int));

static	char	**protocols = NULL;
static	char	**udp_ports = NULL;
static	char	**tcp_ports = NULL;


#define	OPT_SYSLOG	0x001
#define	OPT_RESOLVE	0x002
#define	OPT_HEXBODY	0x004
#define	OPT_VERBOSE	0x008
#define	OPT_HEXHDR	0x010
#define	OPT_TAIL	0x020
#define	OPT_NAT		0x080
#define	OPT_STATE	0x100
#define	OPT_FILTER	0x200
#define	OPT_PORTNUM	0x400
#define	OPT_LOGALL	(OPT_NAT|OPT_STATE|OPT_FILTER)

#define	HOSTNAME_V4(a,b)	hostname((a), 4, (u_32_t *)&(b))

#ifndef	LOGFAC
#define	LOGFAC	LOG_LOCAL0
#endif


void handlehup(sig)
int sig;
{
	FILE	*fp;

	signal(SIGHUP, handlehup);
	if (logfile && (fp = fopen(logfile, "a")))
		newlog = fp;
	init_tabs();
	donehup = 1;
}


static void init_tabs()
{
	struct	protoent	*p;
	struct	servent	*s;
	char	*name, **tab;
	int	port;

	if (protocols != NULL) {
		free(protocols);
		protocols = NULL;
	}
	protocols = (char **)malloc(256 * sizeof(*protocols));
	if (protocols != NULL) {
		bzero((char *)protocols, 256 * sizeof(*protocols));

		setprotoent(1);
		while ((p = getprotoent()) != NULL)
			if (p->p_proto >= 0 && p->p_proto <= 255 &&
			    p->p_name != NULL)
				protocols[p->p_proto] = strdup(p->p_name);
		endprotoent();
	}

	if (udp_ports != NULL) {
		free(udp_ports);
		udp_ports = NULL;
	}
	udp_ports = (char **)malloc(65536 * sizeof(*udp_ports));
	if (udp_ports != NULL)
		bzero((char *)udp_ports, 65536 * sizeof(*udp_ports));

	if (tcp_ports != NULL) {
		free(tcp_ports);
		tcp_ports = NULL;
	}
	tcp_ports = (char **)malloc(65536 * sizeof(*tcp_ports));
	if (tcp_ports != NULL)
		bzero((char *)tcp_ports, 65536 * sizeof(*tcp_ports));

	setservent(1);
	while ((s = getservent()) != NULL) {
		if (s->s_proto == NULL)
			continue;
		else if (!strcmp(s->s_proto, "tcp")) {
			port = ntohs(s->s_port);
			name = s->s_name;
			tab = tcp_ports;
		} else if (!strcmp(s->s_proto, "udp")) {
			port = ntohs(s->s_port);
			name = s->s_name;
			tab = udp_ports;
		} else
			continue;
		if ((port < 0 || port > 65535) || (name == NULL))
			continue;
		tab[port] = strdup(name);
	}
	endservent();
}


static char *getproto(p)
u_int p;
{
	static char pnum[4];
	char *s;

	p &= 0xff;
	s = protocols ? protocols[p] : NULL;
	if (s == NULL) {
		sprintf(pnum, "%u", p);
		s = pnum;
	}
	return s;
}


static int read_log(fd, lenp, buf, bufsize)
int fd, bufsize, *lenp;
char *buf;
{
	int	nr;

	nr = read(fd, buf, bufsize);
	if (!nr)
		return 2;
	if ((nr < 0) && (errno != EINTR))
		return -1;
	*lenp = nr;
	return 0;
}


char	*hostname(res, v, ip)
int	res, v;
u_32_t	*ip;
{
#ifdef	USE_INET6
	static char hostbuf[MAXHOSTNAMELEN+1];
#endif
	struct hostent *hp;
	struct in_addr ipa;

	if (v == 4) {
		ipa.s_addr = *ip;
		if (!res)
			return inet_ntoa(ipa);
		hp = gethostbyaddr((char *)ip, sizeof(ip), AF_INET);
		if (!hp)
			return inet_ntoa(ipa);
		return hp->h_name;

	}
#ifdef	USE_INET6
	(void) inet_ntop(AF_INET6, ip, hostbuf, sizeof(hostbuf) - 1);
	hostbuf[MAXHOSTNAMELEN] = '\0';
	return hostbuf;
#else
	return "IPv6";
#endif
}


char	*portname(res, proto, port)
int	res;
char	*proto;
u_int	port;
{
	static	char	pname[8];
	char	*s;

	port = ntohs(port);
	port &= 0xffff;
	(void) sprintf(pname, "%u", port);
	if (!res || (opts & OPT_PORTNUM))
		return pname;
	s = NULL;
	if (!strcmp(proto, "tcp"))
		s = tcp_ports[port];
	else if (!strcmp(proto, "udp"))
		s = udp_ports[port];
	if (s == NULL)
		s = pname;
	return s;
}


static	void	dumphex(log, buf, len)
FILE	*log;
u_char	*buf;
int	len;
{
	char	line[80];
	int	i, j, k;
	u_char	*s = buf, *t = (u_char *)line;

	for (i = len, j = 0; i; i--, j++, s++) {
		if (j && !(j & 0xf)) {
			*t++ = '\n';
			*t = '\0';
			if (!(opts & OPT_SYSLOG))
				fputs(line, log);
			else
				syslog(LOG_INFO, "%s", line);
			t = (u_char *)line;
			*t = '\0';
		}
		sprintf((char *)t, "%02x", *s & 0xff);
		t += 2;
		if (!((j + 1) & 0xf)) {
			s -= 15;
			sprintf((char *)t, "        ");
			t += 8;
			for (k = 16; k; k--, s++)
				*t++ = (isprint(*s) ? *s : '.');
			s--;
		}
			
		if ((j + 1) & 0xf)
			*t++ = ' ';;
	}

	if (j & 0xf) {
		for (k = 16 - (j & 0xf); k; k--) {
			*t++ = ' ';
			*t++ = ' ';
			*t++ = ' ';
		}
		sprintf((char *)t, "       ");
		t += 7;
		s -= j & 0xf;
		for (k = j & 0xf; k; k--, s++)
			*t++ = (isprint(*s) ? *s : '.');
		*t++ = '\n';
		*t = '\0';
	}
	if (!(opts & OPT_SYSLOG)) {
		fputs(line, log);
		fflush(log);
	} else
		syslog(LOG_INFO, "%s", line);
}

static	void	print_natlog(log, buf, blen)
FILE	*log;
char	*buf;
int	blen;
{
	struct	natlog	*nl;
	iplog_t	*ipl = (iplog_t *)buf;
	char	*t = line;
	struct	tm	*tm;
	int	res, i, len;
	char	*proto;

	nl = (struct natlog *)((char *)ipl + sizeof(*ipl));
	res = (opts & OPT_RESOLVE) ? 1 : 0;
	tm = localtime((time_t *)&ipl->ipl_sec);
	len = sizeof(line);
	if (!(opts & OPT_SYSLOG)) {
		(void) strftime(t, len, "%d/%m/%Y ", tm);
		i = strlen(t);
		len -= i;
		t += i;
	}
	(void) strftime(t, len, "%T", tm);
	t += strlen(t);
	(void) sprintf(t, ".%-.6ld @%hd ", ipl->ipl_usec, nl->nl_rule + 1);
	t += strlen(t);

	if (nl->nl_type == NL_NEWMAP)
		strcpy(t, "NAT:MAP ");
	else if (nl->nl_type == NL_NEWRDR)
		strcpy(t, "NAT:RDR ");
	else if (nl->nl_type == NL_EXPIRE)
		strcpy(t, "NAT:EXPIRE ");
	else if (nl->nl_type == NL_NEWBIMAP)
		strcpy(t, "NAT:BIMAP ");
	else if (nl->nl_type == NL_NEWBLOCK)
		strcpy(t, "NAT:MAPBLOCK ");
	else
		sprintf(t, "Type: %d ", nl->nl_type);
	t += strlen(t);

	proto = getproto(nl->nl_p);

	(void) sprintf(t, "%s,%s <- -> ", HOSTNAME_V4(res, nl->nl_inip),
		portname(res, proto, (u_int)nl->nl_inport));
	t += strlen(t);
	(void) sprintf(t, "%s,%s ", HOSTNAME_V4(res, nl->nl_outip),
		portname(res, proto, (u_int)nl->nl_outport));
	t += strlen(t);
	(void) sprintf(t, "[%s,%s]", HOSTNAME_V4(res, nl->nl_origip),
		portname(res, proto, (u_int)nl->nl_origport));
	t += strlen(t);
	if (nl->nl_type == NL_EXPIRE) {
#ifdef	USE_QUAD_T
		(void) sprintf(t, " Pkts %qd Bytes %qd",
				(long long)nl->nl_pkts,
				(long long)nl->nl_bytes);
#else
		(void) sprintf(t, " Pkts %ld Bytes %ld",
				nl->nl_pkts, nl->nl_bytes);
#endif
		t += strlen(t);
	}

	*t++ = '\n';
	*t++ = '\0';
	if (opts & OPT_SYSLOG)
		syslog(LOG_INFO, "%s", line);
	else
		(void) fprintf(log, "%s", line);
}


static	void	print_statelog(log, buf, blen)
FILE	*log;
char	*buf;
int	blen;
{
	struct	ipslog *sl;
	iplog_t	*ipl = (iplog_t *)buf;
	char	*t = line, *proto;
	struct	tm	*tm;
	int	res, i, len;

	sl = (struct ipslog *)((char *)ipl + sizeof(*ipl));
	res = (opts & OPT_RESOLVE) ? 1 : 0;
	tm = localtime((time_t *)&ipl->ipl_sec);
	len = sizeof(line);
	if (!(opts & OPT_SYSLOG)) {
		(void) strftime(t, len, "%d/%m/%Y ", tm);
		i = strlen(t);
		len -= i;
		t += i;
	}
	(void) strftime(t, len, "%T", tm);
	t += strlen(t);
	(void) sprintf(t, ".%-.6ld ", ipl->ipl_usec);
	t += strlen(t);

	if (sl->isl_type == ISL_NEW)
		strcpy(t, "STATE:NEW ");
	else if (sl->isl_type == ISL_EXPIRE) {
		if ((sl->isl_p == IPPROTO_TCP) &&
		    (sl->isl_state[0] > TCPS_ESTABLISHED ||
		     sl->isl_state[1] > TCPS_ESTABLISHED))
			strcpy(t, "STATE:CLOSE ");
		else
			strcpy(t, "STATE:EXPIRE ");
	} else if (sl->isl_type == ISL_FLUSH)
		strcpy(t, "STATE:FLUSH ");
	else if (sl->isl_type == ISL_REMOVE)
		strcpy(t, "STATE:REMOVE ");
	else
		sprintf(t, "Type: %d ", sl->isl_type);
	t += strlen(t);

	proto = getproto(sl->isl_p);

	if (sl->isl_p == IPPROTO_TCP || sl->isl_p == IPPROTO_UDP) {
		(void) sprintf(t, "%s,%s -> ",
			hostname(res, sl->isl_v, (u_32_t *)&sl->isl_src),
			portname(res, proto, (u_int)sl->isl_sport));
		t += strlen(t);
		(void) sprintf(t, "%s,%s PR %s",
			hostname(res, sl->isl_v, (u_32_t *)&sl->isl_dst),
			portname(res, proto, (u_int)sl->isl_dport), proto);
	} else if (sl->isl_p == IPPROTO_ICMP) {
		(void) sprintf(t, "%s -> ", hostname(res, sl->isl_v,
						     (u_32_t *)&sl->isl_src));
		t += strlen(t);
		(void) sprintf(t, "%s PR icmp %d",
			hostname(res, sl->isl_v, (u_32_t *)&sl->isl_dst),
			sl->isl_itype);
	}
	t += strlen(t);
	if (sl->isl_type != ISL_NEW) {
#ifdef	USE_QUAD_T
		(void) sprintf(t, " Pkts %qd Bytes %qd",
				(long long)sl->isl_pkts,
				(long long)sl->isl_bytes);
#else
		(void) sprintf(t, " Pkts %ld Bytes %ld",
				sl->isl_pkts, sl->isl_bytes);
#endif
		t += strlen(t);
	}

	*t++ = '\n';
	*t++ = '\0';
	if (opts & OPT_SYSLOG)
		syslog(LOG_INFO, "%s", line);
	else
		(void) fprintf(log, "%s", line);
}


static	void	print_log(logtype, log, buf, blen)
FILE	*log;
char	*buf;
int	logtype, blen;
{
	iplog_t	*ipl;
	char *bp = NULL, *bpo = NULL;
	int psize;

	while (blen > 0) {
		ipl = (iplog_t *)buf;
		if ((u_long)ipl & (sizeof(long)-1)) {
			if (bp)
				bpo = bp;
			bp = (char *)malloc(blen);
			bcopy((char *)ipl, bp, blen);
			if (bpo) {
				free(bpo);
				bpo = NULL;
			}
			buf = bp;
			continue;
		}
		if (ipl->ipl_magic != IPL_MAGIC) {
			/* invalid data or out of sync */
			break;
		}
		psize = ipl->ipl_dsize;
		switch (logtype)
		{
		case IPL_LOGIPF :
			print_ipflog(log, buf, psize);
			break;
		case IPL_LOGNAT :
			print_natlog(log, buf, psize);
			break;
		case IPL_LOGSTATE :
			print_statelog(log, buf, psize);
			break;
		}

		blen -= psize;
		buf += psize;
	}
	if (bp)
		free(bp);
	return;
}


static	void	print_ipflog(log, buf, blen)
FILE	*log;
char	*buf;
int	blen;
{
	tcphdr_t	*tp;
	struct	icmp	*ic;
	struct	tm	*tm;
	char	*t, *proto;
	int	i, v, lvl, res, len, off, plen, ipoff;
	ip_t	*ipc, *ip;
	u_short	hl, p;
	ipflog_t *ipf;
	iplog_t	*ipl;
	u_32_t	*s, *d;
#ifdef	USE_INET6
	ip6_t *ip6;
#endif

	ipl = (iplog_t *)buf;
	ipf = (ipflog_t *)((char *)buf + sizeof(*ipl));
	ip = (ip_t *)((char *)ipf + sizeof(*ipf));
	v = ip->ip_v;
	res = (opts & OPT_RESOLVE) ? 1 : 0;
	t = line;
	*t = '\0';
	tm = localtime((time_t *)&ipl->ipl_sec);
#ifdef	linux
	if (v == 4)
		ip->ip_len = ntohs(ip->ip_len);
#endif

	len = sizeof(line);
	if (!(opts & OPT_SYSLOG)) {
		(void) strftime(t, len, "%d/%m/%Y ", tm);
		i = strlen(t);
		len -= i;
		t += i;
	}
	(void) strftime(t, len, "%T", tm);
	t += strlen(t);
	(void) sprintf(t, ".%-.6ld ", ipl->ipl_usec);
	t += strlen(t);
	if (ipl->ipl_count > 1) {
		(void) sprintf(t, "%dx ", ipl->ipl_count);
		t += strlen(t);
	}
#if (SOLARIS || \
	(defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199603)) || \
	(defined(OpenBSD) && (OpenBSD >= 199603))) || defined(linux)
	len = (int)sizeof(ipf->fl_ifname);
	(void) sprintf(t, "%*.*s", len, len, ipf->fl_ifname);
	t += strlen(t);
# if SOLARIS
	if (isalpha(*(t - 1)))
		*t++ = '0' + ipf->fl_unit;
# endif
#else
	for (len = 0; len < 3; len++)
		if (ipf->fl_ifname[len] == '\0')
			break;
	if (ipf->fl_ifname[len])
		len++;
	(void) sprintf(t, "%*.*s%u", len, len, ipf->fl_ifname, ipf->fl_unit);
	t += strlen(t);
#endif
	(void) sprintf(t, " @%hu:%hu ", ipf->fl_group, ipf->fl_rule + 1);
	t += strlen(t);

 	if (ipf->fl_flags & FF_SHORT) {
		*t++ = 'S';
		lvl = LOG_ERR;
	} else if (ipf->fl_flags & FR_PASS) {
		if (ipf->fl_flags & FR_LOGP)
			*t++ = 'p';
		else
			*t++ = 'P';
		lvl = LOG_NOTICE;
	} else if (ipf->fl_flags & FR_BLOCK) {
		if (ipf->fl_flags & FR_LOGB)
			*t++ = 'b';
		else
			*t++ = 'B';
		lvl = LOG_WARNING;
	} else if (ipf->fl_flags & FF_LOGNOMATCH) {
		*t++ = 'n';
		lvl = LOG_NOTICE;
	} else {
		*t++ = 'L';
		lvl = LOG_INFO;
	}
	if (ipf->fl_loglevel != 0xffff)
		lvl = ipf->fl_loglevel;
	*t++ = ' ';
	*t = '\0';

	if (v == 6) {
#ifdef	USE_INET6
		off = 0;
		ipoff = 0;
		hl = sizeof(ip6_t);
		ip6 = (ip6_t *)ip;
		p = (u_short)ip6->ip6_nxt;
		s = (u_32_t *)&ip6->ip6_src;
		d = (u_32_t *)&ip6->ip6_dst;
		plen = ntohs(ip6->ip6_plen);
#else
		sprintf(t, "ipv6");
		goto printipflog;
#endif
	} else if (v == 4) {
		hl = (ip->ip_hl << 2);
		ipoff = ip->ip_off;
		off = ipoff & IP_OFFMASK;
		p = (u_short)ip->ip_p;
		s = (u_32_t *)&ip->ip_src;
		d = (u_32_t *)&ip->ip_dst;
		plen = ntohs(ip->ip_len);
	} else {
		goto printipflog;
	}
	proto = getproto(p);

	if ((p == IPPROTO_TCP || p == IPPROTO_UDP) && !off) {
		tp = (tcphdr_t *)((char *)ip + hl);
		if (!(ipf->fl_flags & (FI_SHORT << 16))) {
			(void) sprintf(t, "%s,%s -> ", hostname(res, v, s),
				portname(res, proto, (u_int)tp->th_sport));
			t += strlen(t);
			(void) sprintf(t, "%s,%s PR %s len %hu %hu ",
				hostname(res, v, d),
				portname(res, proto, (u_int)tp->th_dport),
				proto, hl, plen);
			t += strlen(t);

			if (p == IPPROTO_TCP) {
				*t++ = '-';
				for (i = 0; tcpfl[i].value; i++)
					if (tp->th_flags & tcpfl[i].value)
						*t++ = tcpfl[i].flag;
				if (opts & OPT_VERBOSE) {
					(void) sprintf(t, " %lu %lu %hu",
						(u_long)(ntohl(tp->th_seq)),
						(u_long)(ntohl(tp->th_ack)),
						ntohs(tp->th_win));
					t += strlen(t);
				}
			}
			*t = '\0';
		} else {
			(void) sprintf(t, "%s -> ", hostname(res, v, s));
			t += strlen(t);
			(void) sprintf(t, "%s PR %s len %hu %hu",
				hostname(res, v, d), proto, hl, plen);
		}
	} else if ((p == IPPROTO_ICMP) && !off && (v == 4)) {
		ic = (struct icmp *)((char *)ip + hl);
		(void) sprintf(t, "%s -> ", hostname(res, v, s));
		t += strlen(t);
		(void) sprintf(t, "%s PR icmp len %hu %hu icmp %d/%d",
			hostname(res, v, d), hl, plen,
			ic->icmp_type, ic->icmp_code);
		if (ic->icmp_type == ICMP_UNREACH ||
		    ic->icmp_type == ICMP_SOURCEQUENCH ||
		    ic->icmp_type == ICMP_PARAMPROB ||
		    ic->icmp_type == ICMP_REDIRECT ||
		    ic->icmp_type == ICMP_TIMXCEED) {
			ipc = &ic->icmp_ip;
			tp = (tcphdr_t *)((char *)ipc + hl);

			proto = getproto(ipc->ip_p);

			t += strlen(t);
			(void) sprintf(t, " for %s,%s -",
				HOSTNAME_V4(res, ipc->ip_src),
				portname(res, proto, (u_int)tp->th_sport));
			t += strlen(t);
			(void) sprintf(t, " %s,%s PR %s len %hu %hu",
				HOSTNAME_V4(res, ipc->ip_dst),
				portname(res, proto, (u_int)tp->th_dport),
				proto, ipc->ip_hl << 2, ipc->ip_len);
		}
	} else {
		(void) sprintf(t, "%s -> ", hostname(res, v, s));
		t += strlen(t);
		(void) sprintf(t, "%s PR %s len %hu (%hu)",
			hostname(res, v, d), proto, hl, plen);
		t += strlen(t);
		if (off & IP_OFFMASK)
			(void) sprintf(t, " frag %s%s%hu@%hu",
				ipoff & IP_MF ? "+" : "",
				ipoff & IP_DF ? "-" : "",
				plen - hl, (off & IP_OFFMASK) << 3);
	}
	t += strlen(t);

	if (ipf->fl_flags & FR_KEEPSTATE) {
		(void) strcpy(t, " K-S");
		t += strlen(t);
	}

	if (ipf->fl_flags & FR_KEEPFRAG) {
		(void) strcpy(t, " K-F");
		t += strlen(t);
	}

	if (ipf->fl_flags & FR_INQUE)
		strcpy(t, " IN");
	else if (ipf->fl_flags & FR_OUTQUE)
		strcpy(t, " OUT");
	t += strlen(t);
printipflog:
	*t++ = '\n';
	*t++ = '\0';
	if (opts & OPT_SYSLOG)
		syslog(lvl, "%s", line);
	else
		(void) fprintf(log, "%s", line);
	if (opts & OPT_HEXHDR)
		dumphex(log, (u_char *)buf, sizeof(iplog_t) + sizeof(*ipf));
	if (opts & OPT_HEXBODY)
		dumphex(log, (u_char *)ip, ipf->fl_plen + ipf->fl_hlen);
}


static void usage(prog)
char *prog;
{
	fprintf(stderr, "%s: [-NFhstvxX] [-f <logfile>]\n", prog);
	exit(1);
}


static void write_pid(file)
char *file;
{
	FILE *fp = NULL;
	int fd;

	if ((fd = open(file, O_CREAT|O_TRUNC|O_WRONLY, 0644)) >= 0)
		fp = fdopen(fd, "w");
	if (!fp) {
		close(fd);
		fprintf(stderr, "unable to open/create pid file: %s\n", file);
		return;
	}
	fprintf(fp, "%d", getpid());
	fclose(fp);
	close(fd);
}


static void flushlogs(file, log)
char *file;
FILE *log;
{
	int	fd, flushed = 0;

	if ((fd = open(file, O_RDWR)) == -1) {
		(void) fprintf(stderr, "%s: open: %s\n", file,STRERROR(errno));
		exit(-1);
	}

	if (ioctl(fd, SIOCIPFFB, &flushed) == 0) {
		printf("%d bytes flushed from log buffer\n",
			flushed);
		fflush(stdout);
	} else
		perror("SIOCIPFFB");
	(void) close(fd);

	if (flushed) {
		if (opts & OPT_SYSLOG)
			syslog(LOG_INFO, "%d bytes flushed from log\n",
				flushed);
		else if (log != stdout)
			fprintf(log, "%d bytes flushed from log\n", flushed);
	}
}


static void logopts(turnon, options)
int turnon;
char *options;
{
	int flags = 0;
	char *s;

	for (s = options; *s; s++)
	{
		switch (*s)
		{
		case 'N' :
			flags |= OPT_NAT;
			break;
		case 'S' :
			flags |= OPT_STATE;
			break;
		case 'I' :
			flags |= OPT_FILTER;
			break;
		default :
			fprintf(stderr, "Unknown log option %c\n", *s);
			exit(1);
		}
	}

	if (turnon)
		opts |= flags;
	else
		opts &= ~(flags);
}


int main(argc, argv)
int argc;
char *argv[];
{
	struct	stat	sb;
	FILE	*log = stdout;
	int	fd[3], doread, n, i;
	int	tr, nr, regular[3], c;
	int	fdt[3], devices = 0, make_daemon = 0;
	char	buf[512], *iplfile[3], *s;
	extern	int	optind;
	extern	char	*optarg;

	fd[0] = fd[1] = fd[2] = -1;
	fdt[0] = fdt[1] = fdt[2] = -1;
	iplfile[0] = IPL_NAME;
	iplfile[1] = IPNAT_NAME;
	iplfile[2] = IPSTATE_NAME;

	while ((c = getopt(argc, argv, "?aDf:FhnN:o:O:pP:sS:tvxX")) != -1)
		switch (c)
		{
		case 'a' :
			opts |= OPT_LOGALL;
			fdt[0] = IPL_LOGIPF;
			fdt[1] = IPL_LOGNAT;
			fdt[2] = IPL_LOGSTATE;
			break;
		case 'D' :
			make_daemon = 1;
			break;
		case 'f' : case 'I' :
			opts |= OPT_FILTER;
			fdt[0] = IPL_LOGIPF;
			iplfile[0] = optarg;
			break;
		case 'F' :
			flushlogs(iplfile[0], log);
			flushlogs(iplfile[1], log);
			flushlogs(iplfile[2], log);
			break;
		case 'n' :
			opts |= OPT_RESOLVE;
			break;
		case 'N' :
			opts |= OPT_NAT;
			fdt[1] = IPL_LOGNAT;
			iplfile[1] = optarg;
			break;
		case 'o' : case 'O' :
			logopts(c == 'o', optarg);
			fdt[0] = fdt[1] = fdt[2] = -1;
			if (opts & OPT_FILTER)
				fdt[0] = IPL_LOGIPF;
			if (opts & OPT_NAT)
				fdt[1] = IPL_LOGNAT;
			if (opts & OPT_STATE)
				fdt[2] = IPL_LOGSTATE;
			break;
		case 'p' :
			opts |= OPT_PORTNUM;
			break;
		case 'P' :
			pidfile = optarg;
			break;
		case 's' :
			s = strrchr(argv[0], '/');
			if (s == NULL)
				s = argv[0];
			else
				s++;
			openlog(s, LOG_NDELAY|LOG_PID, LOGFAC);
			s = NULL;
			opts |= OPT_SYSLOG;
			break;
		case 'S' :
			opts |= OPT_STATE;
			fdt[2] = IPL_LOGSTATE;
			iplfile[2] = optarg;
			break;
		case 't' :
			opts |= OPT_TAIL;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		case 'x' :
			opts |= OPT_HEXBODY;
			break;
		case 'X' :
			opts |= OPT_HEXHDR;
			break;
		default :
		case 'h' :
		case '?' :
			usage(argv[0]);
		}

	init_tabs();

	/*
	 * Default action is to only open the filter log file.
	 */
	if ((fdt[0] == -1) && (fdt[1] == -1) && (fdt[2] == -1))
		fdt[0] = IPL_LOGIPF;

	for (i = 0; i < 3; i++) {
		if (fdt[i] == -1)
			continue;
		if (!strcmp(iplfile[i], "-"))
			fd[i] = 0;
		else {
			if ((fd[i] = open(iplfile[i], O_RDONLY)) == -1) {
				(void) fprintf(stderr,
					       "%s: open: %s\n", iplfile[i],
					       STRERROR(errno));
				exit(-1);
			}

			if (fstat(fd[i], &sb) == -1) {
				(void) fprintf(stderr, "%d: fstat: %s\n",fd[i],
					       STRERROR(errno));
				exit(-1);
			}
			if (!(regular[i] = !S_ISCHR(sb.st_mode)))
				devices++;
		}
	}

	if (!(opts & OPT_SYSLOG)) {
		logfile = argv[optind];
		log = logfile ? fopen(logfile, "a") : stdout;
		if (log == NULL) {
			
			(void) fprintf(stderr, "%s: fopen: %s\n", argv[optind],
				STRERROR(errno));
			exit(-1);
		}
		setvbuf(log, NULL, _IONBF, 0);
	} else
		log = NULL;

	if (make_daemon && ((log != stdout) || (opts & OPT_SYSLOG))) {
		if (fork() > 0)
			exit(0);
		write_pid(pidfile);
		close(0);
		close(1);
		close(2);
		setsid();
	} else
		write_pid(pidfile);

	signal(SIGHUP, handlehup);

	for (doread = 1; doread; ) {
		nr = 0;

		for (i = 0; i < 3; i++) {
			tr = 0;
			if (fdt[i] == -1)
				continue;
			if (!regular[i]) {
				if (ioctl(fd[i], FIONREAD, &tr) == -1) {
					perror("ioctl(FIONREAD)");
					exit(-1);
				}
			} else {
				tr = (lseek(fd[i], 0, SEEK_CUR) < sb.st_size);
				if (!tr && !(opts & OPT_TAIL))
					doread = 0;
			}
			if (!tr)
				continue;
			nr += tr;

			tr = read_log(fd[i], &n, buf, sizeof(buf));
			if (donehup) {
				donehup = 0;
				if (newlog) {
					fclose(log);
					log = newlog;
					newlog = NULL;
				}
			}

			switch (tr)
			{
			case -1 :
				if (opts & OPT_SYSLOG)
					syslog(LOG_ERR, "read: %m\n");
				else
					perror("read");
				doread = 0;
				break;
			case 1 :
				if (opts & OPT_SYSLOG)
					syslog(LOG_ERR, "aborting logging\n");
				else
					fprintf(log, "aborting logging\n");
				doread = 0;
				break;
			case 2 :
				break;
			case 0 :
				if (n > 0) {
					print_log(fdt[i], log, buf, n);
					if (!(opts & OPT_SYSLOG))
						fflush(log);
				}
				break;
			}
		}
		if (!nr && ((opts & OPT_TAIL) || devices))
			sleep(1);
	}
	exit(0);
	/* NOTREACHED */
}
