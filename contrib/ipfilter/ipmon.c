/*
 * (C)opyright 1993-1996 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/uio.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <sys/dir.h>
#include <sys/mbuf.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/user.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>

#if !defined(lint) && defined(LIBC_SCCS)
static	char	rcsid[] = "$Id: ipmon.c,v 2.0.1.2 1997/02/04 14:49:19 darrenr Exp $";
#endif

#include "ip_fil.h"


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
static	void	printpacket(), dumphex();
static	int	opts = 0;

#define	OPT_SYSLOG	0x01
#define	OPT_RESOLVE	0x02
#define	OPT_HEXBODY	0x04
#define	OPT_VERBOSE	0x08
#define	OPT_HEXHDR	0x10

#ifndef	LOGFAC
#define	LOGFAC	LOG_LOCAL0
#endif

void printiplci(icp)
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


int readlogentry(fd, lenp, buf, bufsize, log)
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
			syslog(LOG_INFO, "Out of sync! (1,%x)\n", now);
		else
			fprintf(log, "Out of sync! (1,%x)\n", now);
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


static	void	printpacket(log, buf, blen)
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
		dumphex(log, ip, lp->plen + lp->hlen);
	fflush(log);
}

int main(argc, argv)
int argc;
char *argv[];
{
	FILE	*log = NULL;
	int	fd = -1, flushed = 0, doread, n;
	char	buf[512], c, *iplfile = IPL_NAME;
	extern	int	optind;
	extern	char	*optarg;

	while ((c = getopt(argc, argv, "Nf:FsvxX")) != -1)
		switch (c)
		{
		case 'f' :
			iplfile = optarg;
			break;
		case 'F' :
			if ((fd == -1) &&
			    (fd = open(iplfile, O_RDWR)) == -1) {
				(void) fprintf(stderr, "%s: ", IPL_NAME);
				perror("open");
				exit(-1);
			}
			if (ioctl(fd, SIOCIPFFB, &flushed) == 0) {
				printf("%d bytes flushed from log buffer\n",
					flushed);
				fflush(stdout);
			} else
				perror("SIOCIPFFB");
			break;
		case 'N' :
			opts |= OPT_RESOLVE;
			break;
		case 's' :
			openlog(argv[0], LOG_NDELAY|LOG_PID, LOGFAC);
			opts |= OPT_SYSLOG;
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
		}

	if ((fd == -1) && (fd = open(iplfile, O_RDONLY)) == -1) {
		(void) fprintf(stderr, "%s: ", IPL_NAME);
		perror("open");
		exit(-1);
	}

	if (!(opts & OPT_SYSLOG)) {
		log = argv[optind] ? fopen(argv[optind], "a") : stdout;
		setvbuf(log, NULL, _IONBF, 0);
	}

	if (flushed) {
		if (opts & OPT_SYSLOG)
			syslog(LOG_INFO, "%d bytes flushed from log\n",
				flushed);
		else
			fprintf(log, "%d bytes flushed from log\n", flushed);
	}

	for (doread = 1; doread; )
		switch (readlogentry(fd, &n, buf, sizeof(buf), log))
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
			printpacket(log, buf, n, opts);
			break;
		}
	exit(0);
	/* NOTREACHED */
}
