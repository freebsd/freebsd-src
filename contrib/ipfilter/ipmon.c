/*
 * (C)opyright 1993-1996 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#include <sys/dir.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <sys/uio.h>
#include <sys/protosw.h>
#include <sys/user.h>

#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>

#include <ctype.h>
#include <syslog.h>

#include "ip_compat.h"
#include "ip_fil.h"
#include "ip_nat.h"
#include "ip_state.h"

#if !defined(lint) && defined(LIBC_SCCS)
static	char	sccsid[] = "@(#)ipmon.c	1.21 6/5/96 (C)1993-1996 Darren Reed";
static	char	rcsid[] = "$Id: ipmon.c,v 2.0.2.6 1997/04/02 12:23:27 darrenr Exp $";
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


static	char	line[2048];
static	int	opts = 0;
static	void	usage __P((char *));
static	void	print_ipflog __P((FILE *, char *, int));
static	void	print_natlog __P((FILE *, char *, int));
static	void	print_statelog __P((FILE *, char *, int));
static	void	dumphex __P((FILE *, u_char *, int));
static	void	printiplci __P((struct ipl_ci *));
static	void	resynclog __P((int, struct ipl_ci *, FILE *));
static	int	read_ipflog __P((int, int *, char *, int, FILE *));
static	int	read_natlog __P((int, int *, char *, int, FILE *));
static	int	read_statelog __P((int, int *, char *, int, FILE *));
char	*hostname __P((int, struct in_addr));
char	*portname __P((int, char *, u_short));
int	main __P((int, char *[]));

static	int	(*readfunc[3]) __P((int, int *, char *, int, FILE *)) =
		{ read_ipflog, read_natlog, read_statelog };
static	void	(*printfunc[3]) __P((FILE *, char *, int)) =
		{ print_ipflog, print_natlog, print_statelog };


#define	OPT_SYSLOG	0x001
#define	OPT_RESOLVE	0x002
#define	OPT_HEXBODY	0x004
#define	OPT_VERBOSE	0x008
#define	OPT_HEXHDR	0x010
#define	OPT_TAIL	0x020
#define	OPT_ALL		0x040
#define	OPT_NAT		0x080
#define	OPT_STATE	0x100

#ifndef	LOGFAC
#define	LOGFAC	LOG_LOCAL0
#endif

static void printiplci(icp)
struct ipl_ci *icp;
{
	printf("sec %ld usec %ld hlen %d plen %d\n", icp->sec, icp->usec,
		icp->hlen, icp->plen);
}


void resynclog(fd, iplcp, log)
int fd;
struct ipl_ci *iplcp;
FILE *log;
{
	time_t	now;
	char	*s = NULL;
	int	len, nr = 0;

	do {
		if (s) {
			s = (char *)&iplcp->sec;
			if (opts & OPT_SYSLOG) {
				syslog(LOG_INFO, "Sync bytes:");
				syslog(LOG_INFO, " %02x %02x %02x %02x",
					*s, *(s+1), *(s+2), *(s+3));
				syslog(LOG_INFO, " %02x %02x %02x %02x\n",
					*(s+4), *(s+5), *(s+6), *(s+7));
			} else {
				fprintf(log, "Sync bytes:");
				fprintf(log, " %02x %02x %02x %02x",
					*s, *(s+1), *(s+2), *(s+3));
				fprintf(log, " %02x %02x %02x %02x\n",
					*(s+4), *(s+5), *(s+6), *(s+7));
			}
		}
		do {
			s = (char *)&iplcp->sec;
			len = sizeof(iplcp->sec);
			while (len) {
				switch ((nr = read(fd, s, len)))
				{
				case -1:
				case 0:
					return;
				default :
					s += nr;
					len -= nr;
					now = time(NULL);
					break;
				}
			}
		} while ((now < iplcp->sec) ||
			 ((iplcp->sec - now) > (86400*5)));

		len = sizeof(iplcp->usec);
		while (len) {
			switch ((nr = read(fd, s, len)))
			{
			case -1:
			case 0:
				return;
			default :
				s += nr;
				len -= nr;
				break;
			}
		}
	} while (iplcp->usec > 1000000);

	len = sizeof(*iplcp) - sizeof(iplcp->sec) - sizeof(iplcp->usec);
	while (len) {
		switch ((nr = read(fd, s, len)))
		{
		case -1:
		case 0:
			return;
		default :
			s += nr;
			len -= nr;
			break;
		}
	}
}


static int read_natlog(fd, lenp, buf, bufsize, log)
int fd, bufsize, *lenp;
char *buf;
FILE *log;
{
	int	len, avail = 0, want = sizeof(struct natlog);

	*lenp = 0;

	if (ioctl(fd, FIONREAD, &avail) == -1) {
		perror("ioctl(FIONREAD");
		return 1;
	}

	if (avail < want)
		return 2;

	while (want) {
		len = read(fd, buf, want);
		if (len > 0)
			want -= len;
		else
			break;
	}

	if (!want) {
		*lenp = sizeof(struct natlog);
		return 0;
	}
	return !len ? 2 : -1;
}


static int read_statelog(fd, lenp, buf, bufsize, log)
int fd, bufsize, *lenp;
char *buf;
FILE *log;
{
	int	len, avail = 0, want = sizeof(struct ipslog);

	*lenp = 0;

	if (ioctl(fd, FIONREAD, &avail) == -1) {
		perror("ioctl(FIONREAD");
		return 1;
	}

	if (avail < want)
		return 2;

	while (want) {
		len = read(fd, buf, want);
		if (len > 0)
			want -= len;
		else
			break;
	}

	if (!want) {
		*lenp = sizeof(struct ipslog);
		return 0;
	}
	return !len ? 2 : -1;
}


static int read_ipflog(fd, lenp, buf, bufsize, log)
int fd, bufsize, *lenp;
char *buf;
FILE *log;
{
	struct	ipl_ci	*icp = (struct ipl_ci *)buf;
	time_t	now;
	char	*s;
	int	len, n = bufsize, tr = sizeof(struct ipl_ci), nr;

	if (bufsize < tr)
		return 1;
	for (s = buf; (n > 0) && (tr > 0); s += nr, n -= nr) {
		nr = read(fd, s, tr);
		if (nr > 0)
			tr -= nr;
		else
			return -1;
	}

	now = time(NULL);
	if ((icp->hlen > 92) || (now < icp->sec) ||
	    ((now - icp->sec) > (86400*5))) {
		if (opts & OPT_SYSLOG)
			syslog(LOG_INFO, "Out of sync! (1,%lx)\n", now);
		else
			fprintf(log, "Out of sync! (1,%lx)\n", now);
		dumphex(log, buf, sizeof(struct ipl_ci));
		resynclog(fd, icp, log);
	}


	len = (int)((u_int)icp->plen);
	if (len > 128 || len < 0) {
		if (opts & OPT_SYSLOG)
			syslog(LOG_INFO, "Out of sync! (2,%d)\n", len);
		else
			fprintf(log, "Out of sync! (2,%d)\n", len);
		dumphex(log, buf, sizeof(struct ipl_ci));
		resynclog(fd, icp, log);
	}


	tr = icp->hlen + icp->plen;
	if (n < tr)
		return 1;

	for (; (n > 0) && (tr > 0); s += nr, n-= nr) {
		nr = read(fd, s, tr);
		if (nr > 0)
			tr -= nr;
		else
			return -1;
	}
	*lenp = s - buf;
	return 0;
}


char	*hostname(res, ip)
int	res;
struct	in_addr	ip;
{
	struct hostent *hp;

	if (!res)
		return inet_ntoa(ip);
	hp = gethostbyaddr((char *)&ip, sizeof(ip), AF_INET);
	if (!hp)
		return inet_ntoa(ip);
	return hp->h_name;
}


char	*portname(res, proto, port)
int	res;
char	*proto;
u_short	port;
{
	static	char	pname[8];
	struct	servent	*serv;

	(void) sprintf(pname, "%hu", htons(port));
	if (!res)
		return pname;
	serv = getservbyport((int)port, proto);
	if (!serv)
		return pname;
	return serv->s_name;
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
			fputs(line, stdout);
			t = (u_char *)line;
			*t = '\0';
		}
		sprintf(t, "%02x", *s & 0xff);
		t += 2;
		if (!((j + 1) & 0xf)) {
			s -= 15;
			sprintf(t, "        ");
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
		sprintf(t, "       ");
		t += 7;
		s -= j & 0xf;
		for (k = j & 0xf; k; k--, s++)
			*t++ = (isprint(*s) ? *s : '.');
		*t++ = '\n';
		*t = '\0';
	}
	fputs(line, stdout);
	fflush(stdout);
}


static	void	print_natlog(log, buf, blen)
FILE	*log;
char	*buf;
int	blen;
{
	struct	natlog	*nl = (struct natlog *)buf;
	char	*t = line;
	struct	tm	*tm;
	int	res;

	res = (opts & OPT_RESOLVE) ? 1 : 0;
	tm = localtime((time_t *)&nl->nl_tv.tv_sec);
	if (!(opts & OPT_SYSLOG)) {
		(void) sprintf(t, "%2d/%02d/%4d ",
			tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
		t += strlen(t);
	}
	(void) sprintf(t, "%02d:%02d:%02d.%-.6ld @%hd ",
		tm->tm_hour, tm->tm_min, tm->tm_sec, nl->nl_tv.tv_usec,
		nl->nl_rule);
	t += strlen(t);

	if (nl->nl_type == NL_NEWMAP)
		strcpy(t, "NAT:MAP ");
	else if (nl->nl_type == NL_NEWRDR)
		strcpy(t, "NAT:RDR ");
	else if (nl->nl_type == ISL_EXPIRE)
		strcpy(t, "NAT:EXPIRE ");
	else
		sprintf(t, "Type: %d ", nl->nl_type);
	t += strlen(t);

	(void) sprintf(t, "%s,%s <- -> ", hostname(res, nl->nl_inip),
		portname(res, NULL, nl->nl_inport));
	t += strlen(t);
	(void) sprintf(t, "%s,%s ", hostname(res, nl->nl_outip),
		portname(res, NULL, nl->nl_outport));
	t += strlen(t);
	(void) sprintf(t, "[%s,%s]", hostname(res, nl->nl_origip),
		portname(res, NULL, nl->nl_origport));
	t += strlen(t);

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
	struct	ipslog *sl = (struct ipslog *)buf;
	struct	protoent *pr;
	char	*t = line, *proto, pname[6];
	struct	tm	*tm;
	int	res;

	res = (opts & OPT_RESOLVE) ? 1 : 0;
	tm = localtime((time_t *)&sl->isl_tv.tv_sec);
	if (!(opts & OPT_SYSLOG)) {
		(void) sprintf(t, "%2d/%02d/%4d ",
			tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
		t += strlen(t);
	}
	(void) sprintf(t, "%02d:%02d:%02d.%-.6ld ",
		tm->tm_hour, tm->tm_min, tm->tm_sec, sl->isl_tv.tv_usec);
	t += strlen(t);

	if (sl->isl_type == ISL_NEW)
		strcpy(t, "STATE:NEW ");
	else if (sl->isl_type == ISL_EXPIRE)
		strcpy(t, "STATE:EXPIRE ");
	else
		sprintf(t, "Type: %d ", sl->isl_type);
	t += strlen(t);

	pr = getprotobynumber((int)sl->isl_p);
	if (!pr) {
		proto = pname;
		sprintf(proto, "%d", (u_int)sl->isl_p);
	} else
		proto = pr->p_name;

	if (sl->isl_p == IPPROTO_TCP || sl->isl_p == IPPROTO_UDP) {
		(void) sprintf(t, "%s,%s -> ",
			hostname(res, sl->isl_src),
			portname(res, proto, sl->isl_sport));
		t += strlen(t);
		(void) sprintf(t, "%s,%s PR %s ",
			hostname(res, sl->isl_dst),
			portname(res, proto, sl->isl_dport), proto);
	} else if (sl->isl_p == IPPROTO_ICMP) {
		(void) sprintf(t, "%s -> ", hostname(res, sl->isl_src));
		t += strlen(t);
		(void) sprintf(t, "%s PR icmp %d ",
			hostname(res, sl->isl_dst), sl->isl_itype);
	}
	t += strlen(t);
	if (sl->isl_type != ISL_NEW) {
#ifdef	USE_QUAD_T
		(void) sprintf(t, "Pkts %qd Bytes %qd",
#else
		(void) sprintf(t, "Pkts %ld Bytes %ld",
#endif
				sl->isl_pkts, sl->isl_bytes);
		t += strlen(t);
	}

	*t++ = '\n';
	*t++ = '\0';
	if (opts & OPT_SYSLOG)
		syslog(LOG_INFO, "%s", line);
	else
		(void) fprintf(log, "%s", line);
}


static	void	print_ipflog(log, buf, blen)
FILE	*log;
char	*buf;
int	blen;
{
	struct	protoent *pr;
	struct	tcphdr	*tp;
	struct	icmp	*ic;
	struct	ip	*ipc;
	struct	tm	*tm;
	char	c[3], pname[8], *t, *proto;
	u_short	hl, p;
	int	i, lvl, res;
#if !SOLARIS && !(defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199603))
	int	len;
#endif
	struct	ip	*ip;
	struct	ipl_ci	*lp;

	lp = (struct ipl_ci *)buf;
	ip = (struct ip *)(buf + sizeof(*lp));
	res = (opts & OPT_RESOLVE) ? 1 : 0;
	t = line;
	*t = '\0';
	hl = (ip->ip_hl << 2);
	p = (u_short)ip->ip_p;
	tm = localtime((time_t *)&lp->sec);
	if (!(opts & OPT_SYSLOG)) {
		(void) sprintf(t, "%2d/%02d/%4d ",
			tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
		t += strlen(t);
	}
#if SOLARIS || (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199603))
	(void) sprintf(t, "%02d:%02d:%02d.%-.6ld %.*s @%hd ",
		tm->tm_hour, tm->tm_min, tm->tm_sec, lp->usec,
		(int)sizeof(lp->ifname), lp->ifname, lp->rule);
#else
	for (len = 0; len < 3; len++)
		if (!lp->ifname[len])
			break;
	if (lp->ifname[len])
		len++;
	(void) sprintf(t, "%02d:%02d:%02d.%-.6ld %*.*s%ld @%hd ",
		tm->tm_hour, tm->tm_min, tm->tm_sec, lp->usec,
		len, len, lp->ifname, lp->unit, lp->rule);
#endif
	pr = getprotobynumber((int)p);
	if (!pr) {
		proto = pname;
		sprintf(proto, "%d", (u_int)p);
	} else
		proto = pr->p_name;

 	if (lp->flags & (FI_SHORT << 20)) {
		c[0] = 'S';
		lvl = LOG_ERR;
	} else if (lp->flags & FR_PASS) {
		if (lp->flags & FR_LOGP)
			c[0] = 'p';
		else
			c[0] = 'P';
		lvl = LOG_NOTICE;
	} else if (lp->flags & FR_BLOCK) {
		if (lp->flags & FR_LOGB)
			c[0] = 'b';
		else
			c[0] = 'B';
		lvl = LOG_WARNING;
	} else if (lp->flags & FF_LOGNOMATCH) {
		c[0] = 'n';
		lvl = LOG_NOTICE;
	} else {
		c[0] = 'L';
		lvl = LOG_INFO;
	}
	c[1] = ' ';
	c[2] = '\0';
	(void) strcat(line, c);
	t = line + strlen(line);

	if ((p == IPPROTO_TCP || p == IPPROTO_UDP) && !(ip->ip_off & 0x1fff)) {
		tp = (struct tcphdr *)((char *)ip + hl);
		if (!(lp->flags & (FI_SHORT << 16))) {
			(void) sprintf(t, "%s,%s -> ",
				hostname(res, ip->ip_src),
				portname(res, proto, tp->th_sport));
			t += strlen(t);
			(void) sprintf(t, "%s,%s PR %s len %hu %hu ",
				hostname(res, ip->ip_dst),
				portname(res, proto, tp->th_dport),
				proto, hl, ip->ip_len);
			t += strlen(t);

			if (p == IPPROTO_TCP) {
				*t++ = '-';
				for (i = 0; tcpfl[i].value; i++)
					if (tp->th_flags & tcpfl[i].value)
						*t++ = tcpfl[i].flag;
			}
			if (opts & OPT_VERBOSE) {
				(void) sprintf(t, " %lu %lu %hu",
					(u_long)tp->th_seq,
					(u_long)tp->th_ack, tp->th_win);
				t += strlen(t);
			}
			*t = '\0';
		} else {
			(void) sprintf(t, "%s -> ", hostname(res, ip->ip_src));
			t += strlen(t);
			(void) sprintf(t, "%s PR %s len %hu %hu",
				hostname(res, ip->ip_dst), proto,
				hl, ip->ip_len);
		}
	} else if (p == IPPROTO_ICMP) {
		ic = (struct icmp *)((char *)ip + hl);
		(void) sprintf(t, "%s -> ", hostname(res, ip->ip_src));
		t += strlen(t);
		(void) sprintf(t, "%s PR icmp len %hu (%hu) icmp %d/%d",
			hostname(res, ip->ip_dst), hl,
			ip->ip_len, ic->icmp_type, ic->icmp_code);
		if (ic->icmp_type == ICMP_UNREACH ||
		    ic->icmp_type == ICMP_SOURCEQUENCH ||
		    ic->icmp_type == ICMP_PARAMPROB ||
		    ic->icmp_type == ICMP_REDIRECT ||
		    ic->icmp_type == ICMP_TIMXCEED) {
			ipc = &ic->icmp_ip;
			tp = (struct tcphdr *)((char *)ipc + hl);

			p = (u_short)ipc->ip_p;
			pr = getprotobynumber((int)p);
			if (!pr) {
				proto = pname;
				(void) sprintf(proto, "%d", (int)p);
			} else
				proto = pr->p_name;

			t += strlen(t);
			(void) sprintf(t, " for %s,%s -",
				hostname(res, ipc->ip_src),
				portname(res, proto, tp->th_sport));
			t += strlen(t);
			(void) sprintf(t, " %s,%s PR %s len %hu %hu",
				hostname(res, ipc->ip_dst),
				portname(res, proto, tp->th_dport),
				proto, ipc->ip_hl << 2, ipc->ip_len);
		}
	} else {
		(void) sprintf(t, "%s -> ", hostname(res, ip->ip_src));
		t += strlen(t);
		(void) sprintf(t, "%s PR %s len %hu (%hu)",
			hostname(res, ip->ip_dst), proto, hl, ip->ip_len);
		t += strlen(t);
		if (ip->ip_off & 0x1fff)
			(void) sprintf(t, " frag %s%s%hu@%hu",
				ip->ip_off & IP_MF ? "+" : "",
				ip->ip_off & IP_DF ? "-" : "",
				ip->ip_len - hl, (ip->ip_off & 0x1fff) << 3);
	}
	t += strlen(t);

	if (lp->flags & FR_KEEPSTATE) {
		(void) strcpy(t, " K-S");
		t += strlen(t);
	}

	if (lp->flags & FR_KEEPFRAG) {
		(void) strcpy(t, " K-F");
		t += strlen(t);
	}

	*t++ = '\n';
	*t++ = '\0';
	if (opts & OPT_SYSLOG)
		syslog(lvl, "%s", line);
	else
		(void) fprintf(log, "%s", line);
	if (opts & OPT_HEXHDR)
		dumphex(log, buf, sizeof(struct ipl_ci));
	if (opts & OPT_HEXBODY)
		dumphex(log, (u_char *)ip, lp->plen + lp->hlen);
}


void static usage(prog)
char *prog;
{
	fprintf(stderr, "%s: [-NFhstvxX] [-f <logfile>]\n", prog);
	exit(1);
}


void flushlogs(file, log)
char *file;
FILE *log;
{
	int	fd, flushed = 0;

	if ((fd = open(file, O_RDWR)) == -1) {
		(void) fprintf(stderr, "%s: ", file);
		perror("open");
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
		else
			fprintf(log, "%d bytes flushed from log\n", flushed);
	}
}


int main(argc, argv)
int argc;
char *argv[];
{
	struct	stat	stat;
	FILE	*log = NULL;
	int	fd[3] = {-1, -1, -1}, flushed = 0, doread, n, i, nfd = 1;
	int	tr, nr, regular;
	int	fdt[3] = {IPL_LOGIPF, IPL_LOGNAT, IPL_LOGSTATE};
	char	buf[512], c, *iplfile = IPL_NAME;
	extern	int	optind;
	extern	char	*optarg;

	while ((c = getopt(argc, argv, "?af:FhnNsStvxX")) != -1)
		switch (c)
		{
		case 'a' :
			opts |= OPT_ALL;
			nfd = 3;
			break;
		case 'f' :
			iplfile = optarg;
			break;
		case 'F' :
			if (!(opts & OPT_ALL))
				flushlogs(iplfile, log);
			else {
				flushlogs(IPL_NAME, log);
				flushlogs(IPL_NAT, log);
				flushlogs(IPL_STATE, log);
			}
			break;
		case 'n' :
			opts |= OPT_RESOLVE;
			break;
		case 'N' :
			opts |= OPT_NAT;
			fdt[0] = IPL_LOGNAT;
			readfunc[0] = read_natlog;
			printfunc[0] = print_natlog;
			break;
		case 's' :
			openlog(argv[0], LOG_NDELAY|LOG_PID, LOGFAC);
			opts |= OPT_SYSLOG;
			break;
		case 'S' :
			opts |= OPT_STATE;
			fdt[0] = IPL_LOGSTATE;
			readfunc[0] = read_statelog;
			printfunc[0] = print_statelog;
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

	if ((fd[0] == -1) && (fd[0] = open(iplfile, O_RDONLY)) == -1) {
		(void) fprintf(stderr, "%s: ", iplfile);
		perror("open");
		exit(-1);
	}

	if ((opts & OPT_ALL)) {
		if ((fd[1] = open(IPL_NAT, O_RDONLY)) == -1) {
			(void) fprintf(stderr, "%s: ", IPL_NAT);
			perror("open");
			exit(-1);
		}
		if ((fd[2] = open(IPL_STATE, O_RDONLY)) == -1) {
			(void) fprintf(stderr, "%s: ", IPL_STATE);
			perror("open");
			exit(-1);
		}
	}

	if (!(opts & OPT_SYSLOG)) {
		log = argv[optind] ? fopen(argv[optind], "a") : stdout;
		setvbuf(log, NULL, _IONBF, 0);
	}

	if (fstat(fd[0], &stat) == -1) {
		fprintf(stderr, "%s :", iplfile);
		perror("fstat");
		exit(-1);
	}

	regular = !S_ISCHR(stat.st_mode);

	for (doread = 1; doread; ) {
		nr = 0;

		for (i = 0; i < nfd; i++) {
			tr = 0;
			if (!regular) {
				if (ioctl(fd[i], FIONREAD, &tr) == -1) {
					perror("ioctl(FIONREAD)");
					exit(-1);
				}
			} else {
				tr = (lseek(fd[i], 0, SEEK_CUR) <
				      stat.st_size);
				if (!tr && !(opts & OPT_TAIL))
					doread = 0;
			}
			if (!tr)
				continue;
			nr += tr;

			tr = (*readfunc[i])(fd[i], &n, buf, sizeof(buf), log);
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
					(*printfunc[i])(log, buf, n);
					if (!(opts & OPT_SYSLOG))
						fflush(log);
				}
				break;
			}
		}
		if (!nr && regular && (opts & OPT_TAIL))
			sleep(1);
	}
	exit(0);
	/* NOTREACHED */
}
