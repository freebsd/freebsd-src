/*	$FreeBSD$	*/

/*
 * (C)opyright 1995-1998 Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 */
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <malloc.h>
#include <netdb.h>
#include <string.h>
#include <sys/dir.h>
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
static const char sccsid[] = "@(#)ipsdr.c	1.3 12/3/95 (C)1995 Darren Reed";
static const char rcsid[] = "@(#)Id: ipsdr.c,v 2.2 2001/06/09 17:09:25 darrenr Exp";
#endif

extern	char	*optarg;
extern	int	optind;

#define	NPORTS	21

u_short	defports[NPORTS] = {
		7,   9,  20,  21,  23,  25,  53,  69,  79, 111,
		123, 161, 162, 512, 513, 513, 515, 520, 540, 6000, 0
	};
u_short	pweights[NPORTS] = {
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
	};

ipsd_t	*iphits[NPORTS];
int	pkts;


int	ipcmp(sh1, sh2)
sdhit_t	*sh1, *sh2;
{
	return sh1->sh_ip.s_addr - sh2->sh_ip.s_addr;
}


int	ssipcmp(sh1, sh2)
ipss_t	*sh1, *sh2;
{
	return sh1->ss_ip.s_addr - sh2->ss_ip.s_addr;
}


int countpbits(num)
u_long	num;
{
	int	i, j;

	for (i = 1, j = 0; i; i <<= 1)
		if (num & i)
			j++;
	return j;
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
int	detect(srcip, dport, date)
struct	in_addr	srcip;
u_short	dport;
time_t	date;
{
	ipsd_t	*ihp;
	sdhit_t	*sh;
	int	i, j, k;

	for (i = 10, j = 4; j >= 0; j--) {
		k = dport - defports[i];
		if (!k) {
			ihp = iphits[i];
			if (findhit(ihp, srcip, dport))
				return 0;
			sh = ihp->sd_hit + ihp->sd_cnt;
			sh->sh_date = date;
			sh->sh_ip = srcip;
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
 * Write statistics out to a file
 */
addfile(file)
char	*file;
{
	ipsd_t	ipsd, *ips = &ipsd;
	sdhit_t	hit, *hp;
	char	fname[32];
	int	i, fd, sz;

	if ((fd = open(file, O_RDONLY)) == -1) {
		perror("open");
		return;
	}

	printf("opened %s\n", file);
	do {
		if (read(fd, ips, sizeof(*ips)) != sizeof(*ips))
			break;
		sz = ips->sd_sz * sizeof(*hp);
		hp = (sdhit_t *)malloc(sz);
		if (read(fd, hp, sz) != sz)
			break;
		for (i = 0; i < ips->sd_cnt; i++)
			detect(hp[i].sh_ip, ips->sd_port, hp[i].sh_date);
	} while (1);
	(void) close(fd);
}


readfiles(dir)
char *dir;
{
	struct	direct	**d;
	int	i, j;

	d = NULL;
	i = scandir(dir, &d, NULL, NULL);

	for (j = 0; j < i; j++) {
		if (strncmp(d[j]->d_name, "ipsd-hits.", 10))
			continue;
		addfile(d[j]->d_name);
	}
}


void printreport(ss, num)
ipss_t	*ss;
int	num;
{
	struct	in_addr	ip;
	ipss_t	*sp;
	int	i, j, mask;
	u_long	ports;

	printf("Hosts detected: %d\n", num);
	if (!num)
		return;
	for (i = 0; i < num; i++)
		printf("%s %d %d\n", inet_ntoa(ss[i].ss_ip), ss[i].ss_hits,
			countpbits(ss[i].ss_ports));

	printf("--------------------------\n");
	for (mask = 0xfffffffe, j = 32; j; j--, mask <<= 1) {
		ip.s_addr = ss[0].ss_ip.s_addr & mask;
		ports = ss[0].ss_ports;
		for (i = 1; i < num; i++) {
			sp = ss + i;
			if (ip.s_addr != (sp->ss_ip.s_addr & mask)) {
				printf("Netmask: 0x%08x\n", mask);
				printf("%s %d\n", inet_ntoa(ip),
					countpbits(ports));
				ip.s_addr = sp->ss_ip.s_addr & mask;
				ports = 0;
			}
			ports |= sp->ss_ports;
		}
		if (ports) {
			printf("Netmask: 0x%08x\n", mask);
			printf("%s %d\n", inet_ntoa(ip), countpbits(ports));
		}
	}
}


collectips()
{
	ipsd_t	*ips;
	ipss_t	*ss;
	int	i, num, nip, in, j, k;

	for (i = 0; i < NPORTS; i++)
		nip += iphits[i]->sd_cnt;

	ss = (ipss_t *)malloc(sizeof(ipss_t) * nip);

	for (in = 0, i = 0, num = 0; i < NPORTS; i++) {
		ips = iphits[i];
		for (j = 0; j < ips->sd_cnt; j++) {
			for (k = 0; k < num; k++)
				if (!bcmp(&ss[k].ss_ip, &ips->sd_hit[j].sh_ip,
					  sizeof(struct in_addr))) {
					ss[k].ss_hits += pweights[i];
					ss[k].ss_ports |= (1 << i);
					break;
				}
			if (k == num) {
				ss[num].ss_ip = ips->sd_hit[j].sh_ip;
				ss[num].ss_hits = pweights[i];
				ss[k].ss_ports |= (1 << i);
				num++;
			}
		}
	}

	qsort(ss, num, sizeof(*ss), ssipcmp);

	printreport(ss, num);
}


main(argc, argv)
int	argc;
char	*argv[];
{
	char	c, *name =  argv[0], *dir = NULL;
	int	fd;

	setuphits();
	dir = dir ? dir : ".";
	readfiles(dir);
	collectips();
}
