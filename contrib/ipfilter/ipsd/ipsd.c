/*
 * (C)opyright 1995-1998 Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 *   The author of this software makes no garuntee about the
 * performance of this package or its suitability to fulfill any purpose.
 *
 */
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#ifndef	linux
#include <netinet/ip_var.h>
#include <netinet/tcpip.h>
#endif
#include "ip_compat.h"
#ifdef	linux
#include <linux/sockios.h>
#include "tcpip.h"
#endif
#include "ipsd.h"

#ifndef	lint
static const char sccsid[] = "@(#)ipsd.c	1.3 12/3/95 (C)1995 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipsd.c,v 2.1.4.1 2001/06/26 10:43:21 darrenr Exp $";
#endif

extern	char	*optarg;
extern	int	optind;

#ifdef	linux
char	default_device[] = "eth0";
#else
# ifdef	sun
char	default_device[] = "le0";
# else
#  ifdef	ultrix
char	default_device[] = "ln0";
#  else
char	default_device[] = "lan0";
#  endif
# endif
#endif

#define	NPORTS	21

u_short	defports[NPORTS] = {
		  7,   9,  20,  21,  23,  25,  53,  69,  79, 111,
		123, 161, 162, 512, 513, 514, 515, 520, 540, 6000, 0
	};

ipsd_t	*iphits[NPORTS];
int	writes = 0;


int	ipcmp(sh1, sh2)
sdhit_t	*sh1, *sh2;
{
	return sh1->sh_ip.s_addr - sh2->sh_ip.s_addr;
}


/*
 * Check to see if we've already received a packet from this host for this
 * port.
 */
int	findhit(ihp, src, dport)
ipsd_t	*ihp;
struct	in_addr	src;
u_short	dport;
{
	int	i, j, k;
	sdhit_t	*sh;

	sh = NULL;

	if (ihp->sd_sz == 4) {
		for (i = 0, sh = ihp->sd_hit; i < ihp->sd_cnt; i++, sh++)
			if (src.s_addr == sh->sh_ip.s_addr)
				return 1;
	} else {
		for (i = ihp->sd_cnt / 2, j = (i / 2) - 1; j >= 0; j--) {
			k = ihp->sd_hit[i].sh_ip.s_addr - src.s_addr;
			if (!k)
				return 1;
			else if (k < 0)
				i -= j;
			else
				i += j;
		}
	}
	return 0;
}


/*
 * Search for port number amongst the sorted array of targets we're
 * interested in.
 */
int	detect(ip, tcp)
ip_t	*ip;
tcphdr_t	*tcp;
{
	ipsd_t	*ihp;
	sdhit_t	*sh;
	int	i, j, k;

	for (i = 10, j = 4; j >= 0; j--) {
		k = tcp->th_dport - defports[i];
		if (!k) {
			ihp = iphits[i];
			if (findhit(ihp, ip->ip_src, tcp->th_dport))
				return 0;
			sh = ihp->sd_hit + ihp->sd_cnt;
			sh->sh_date = time(NULL);
			sh->sh_ip.s_addr = ip->ip_src.s_addr;
			if (++ihp->sd_cnt == ihp->sd_sz)
			{
				ihp->sd_sz += 8;
				sh = realloc(sh, ihp->sd_sz * sizeof(*sh));
				ihp->sd_hit = sh;
			}
			qsort(sh, ihp->sd_cnt, sizeof(*sh), ipcmp);
			return 0;
		}
		if (k < 0)
			i -= j;
		else
			i += j;
	}
	return -1;
}


/*
 * Allocate initial storage for hosts
 */
setuphits()
{
	int	i;

	for (i = 0; i < NPORTS; i++) {
		if (iphits[i]) {
			if (iphits[i]->sd_hit)
				free(iphits[i]->sd_hit);
			free(iphits[i]);
		}
		iphits[i] = (ipsd_t *)malloc(sizeof(ipsd_t));
		iphits[i]->sd_port = defports[i];
		iphits[i]->sd_cnt = 0;
		iphits[i]->sd_sz = 4;
		iphits[i]->sd_hit = (sdhit_t *)malloc(sizeof(sdhit_t) * 4);
	}
}


/*
 * cleanup exits
 */
waiter()
{
	wait(0);
}


/*
 * Write statistics out to a file
 */
writestats(nwrites)
int	nwrites;
{
	ipsd_t	**ipsd, *ips;
	char	fname[32];
	int	i, fd;

	(void) sprintf(fname, "/var/log/ipsd/ipsd-hits.%d", nwrites);
	fd = open(fname, O_RDWR|O_CREAT|O_TRUNC|O_EXCL, 0644);
	for (i = 0, ipsd = iphits; i < NPORTS; i++, ipsd++) {
		ips = *ipsd;
		if (ips->sd_cnt) {
			write(fd, ips, sizeof(ipsd_t));
			write(fd, ips->sd_hit, sizeof(sdhit_t) * ips->sd_sz);
		}
	}
	(void) close(fd);
	exit(0);
}


void writenow()
{
	signal(SIGCHLD, waiter);
	switch (fork())
	{
	case 0 :
		writestats(writes);
		exit(0);
	case -1 :
		perror("vfork");
		break;
	default :
		writes++;
		setuphits();
		break;
	}
}


void	usage(prog)
char	*prog;
{
	fprintf(stderr, "Usage: %s [-d device]\n", prog);
	exit(1);
}


void detecthits(fd, writecount)
int fd, writecount;
{
	struct	in_addr	ip;
	int	hits = 0;

	while (1) {
		hits += readloop(fd, ip);
		if (hits > writecount) {
			writenow();
			hits = 0;
		}
	}
}


main(argc, argv)
int	argc;
char	*argv[];
{
	char	*name =  argv[0], *dev = NULL;
	int	fd, writeafter = 10000, angelic = 0, c;

	while ((c = getopt(argc, argv, "ad:n:")) != -1)
		switch (c)
		{
		case 'a' :
			angelic = 1;
			break;
		case 'd' :
			dev = optarg;
			break;
		case 'n' :
			writeafter = atoi(optarg);
			break;
		default :
			fprintf(stderr, "Unknown option \"%c\"\n", c);
			usage(name);
		}

	bzero(iphits, sizeof(iphits));
	setuphits();

	if (!dev)
		dev = default_device;
	printf("Device:  %s\n", dev);
	fd = initdevice(dev, 60);

	if (!angelic) {
		switch (fork())
		{
		case 0 :
			(void) close(0);
			(void) close(1);
			(void) close(2);
			(void) setpgrp(0, getpgrp());
			(void) setsid();
			break;
		case -1:
			perror("fork");
			exit(-1);
		default:
			exit(0);
		}
	}
	signal(SIGUSR1, writenow);
	detecthits(fd, writeafter);
}
