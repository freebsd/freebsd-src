/*
 * Copyright (C) 1993-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */

#include <stdio.h>
#include <string.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/file.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <nlist.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netinet/tcp.h>
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "ipf.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_auth.h"
#include "kmem.h"
#if defined(__NetBSD__) || (__OpenBSD__)
#include <paths.h>
#endif

#if !defined(lint)
static const char sccsid[] = "@(#)fils.c	1.21 4/20/96 (C) 1993-1996 Darren Reed";
static const char rcsid[] = "@(#)$Id: fils.c,v 2.0.2.25.2.1 1997/11/06 21:21:19 darrenr Exp $";
#endif
#ifdef	_PATH_UNIX
#define	VMUNIX	_PATH_UNIX
#else
#define	VMUNIX	"/vmunix"
#endif

extern	char	*optarg;

#define	PRINTF	(void)printf
#define	FPRINTF	(void)fprintf
#define	F_IN	0
#define	F_OUT	1
#define	F_AC	2
static	char	*filters[4] = { "ipfilter(in)", "ipfilter(out)",
				"ipacct(in)", "ipacct(out)" };

int	opts = 0;

extern	int	main __P((int, char *[]));
static	void	showstats __P((int, friostat_t *));
static	void	showfrstates __P((int, ipfrstat_t *));
static	void	showlist __P((friostat_t *));
static	void	showipstates __P((int, ips_stat_t *));
static	void	showauthstates __P((int, fr_authstat_t *));
static	void	Usage __P((char *));
static	void	printlist __P((frentry_t *));


static void Usage(name)
char *name;
{
	fprintf(stderr, "Usage: %s [-aAfhIinosv] [-d <device>]\n", name);
	exit(1);
}


int main(argc,argv)
int argc;
char *argv[];
{
	fr_authstat_t	frauthst;
	friostat_t fio;
	ips_stat_t ipsst;
	ipfrstat_t ifrst;
	char	*name = NULL, *device = IPL_NAME;
	int	c, fd;

	if (openkmem() == -1)
		exit(-1);

	(void)setuid(getuid());
	(void)setgid(getgid());

	while ((c = getopt(argc, argv, "aAfhIinosvd:")) != -1)
	{
		switch (c)
		{
		case 'a' :
			opts |= OPT_ACCNT|OPT_SHOWLIST;
			break;
		case 'A' :
			opts |= OPT_AUTHSTATS;
			break;
		case 'd' :
			device = optarg;
			break;
		case 'f' :
			opts |= OPT_FRSTATES;
			break;
		case 'h' :
			opts |= OPT_HITS;
			break;
		case 'i' :
			opts |= OPT_INQUE|OPT_SHOWLIST;
			break;
		case 'I' :
			opts |= OPT_INACTIVE;
			break;
		case 'n' :
			opts |= OPT_SHOWLINENO;
			break;
		case 'o' :
			opts |= OPT_OUTQUE|OPT_SHOWLIST;
			break;
		case 's' :
			opts |= OPT_IPSTATES;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		default :
			Usage(argv[0]);
			break;
		}
	}

	if ((fd = open(device, O_RDONLY)) < 0) {
		perror("open");
		exit(-1);
	}

	bzero((char *)&fio, sizeof(fio));
	bzero((char *)&ipsst, sizeof(ipsst));
	bzero((char *)&ifrst, sizeof(ifrst));

	if (ioctl(fd, SIOCGETFS, &fio) == -1) {
		perror("ioctl(SIOCGETFS)");
		exit(-1);
	}
	if ((opts & OPT_IPSTATES)) {
		int	sfd = open(IPL_STATE, O_RDONLY);

		if (sfd == -1) {
			perror("open");
			exit(-1);
		}
		if ((ioctl(sfd, SIOCGIPST, &ipsst) == -1)) {
			perror("ioctl(SIOCGIPST)");
			exit(-1);
		}
		close(sfd);
	}
	if ((opts & OPT_FRSTATES) && (ioctl(fd, SIOCGFRST, &ifrst) == -1)) {
		perror("ioctl(SIOCGFRST)");
		exit(-1);
	}

	if (opts & OPT_VERBOSE)
		PRINTF("opts %#x name %s\n", opts, name ? name : "<>");

	if ((opts & OPT_AUTHSTATS) &&
	    (ioctl(fd, SIOCATHST, &frauthst) == -1)) {
		perror("ioctl(SIOCATHST)");
		exit(-1);
	}

	if (opts & OPT_SHOWLIST) {
		showlist(&fio);
		if ((opts & OPT_OUTQUE) && (opts & OPT_INQUE)){
			opts &= ~OPT_OUTQUE;
			showlist(&fio);
		}
	} else {
		if (opts & OPT_IPSTATES)
			showipstates(fd, &ipsst);
		else if (opts & OPT_FRSTATES)
			showfrstates(fd, &ifrst);
		else if (opts & OPT_AUTHSTATS)
			showauthstates(fd, &frauthst);
		else
			showstats(fd, &fio);
	}
	return 0;
}


/*
 * read the kernel stats for packets blocked and passed
 */
static	void	showstats(fd, fp)
int	fd;
struct	friostat	*fp;
{
	int	frf = 0;

	if (ioctl(fd, SIOCGETFF, &frf) == -1)
		perror("ioctl(SIOCGETFF)");

#if SOLARIS
	PRINTF("dropped packets:\tin %lu\tout %lu\n",
			fp->f_st[0].fr_drop, fp->f_st[1].fr_drop);
	PRINTF("non-ip packets:\t\tin %lu\tout %lu\n",
			fp->f_st[0].fr_notip, fp->f_st[1].fr_notip);
	PRINTF("   bad packets:\t\tin %lu\tout %lu\n",
			fp->f_st[0].fr_bad, fp->f_st[1].fr_bad);
#endif
	PRINTF(" input packets:\t\tblocked %lu passed %lu nomatch %lu",
			fp->f_st[0].fr_block, fp->f_st[0].fr_pass,
			fp->f_st[0].fr_nom);
	PRINTF(" counted %lu\n", fp->f_st[0].fr_acct);
	PRINTF("output packets:\t\tblocked %lu passed %lu nomatch %lu",
			fp->f_st[1].fr_block, fp->f_st[1].fr_pass,
			fp->f_st[1].fr_nom);
	PRINTF(" counted %lu\n", fp->f_st[0].fr_acct);
	PRINTF(" input packets logged:\tblocked %lu passed %lu\n",
			fp->f_st[0].fr_bpkl, fp->f_st[0].fr_ppkl);
	PRINTF("output packets logged:\tblocked %lu passed %lu\n",
			fp->f_st[1].fr_bpkl, fp->f_st[1].fr_ppkl);
	PRINTF(" packets logged:\tinput %lu output %lu\n",
			fp->f_st[0].fr_pkl, fp->f_st[1].fr_pkl);
	PRINTF(" log failures:\t\tinput %lu output %lu\n",
			fp->f_st[0].fr_skip, fp->f_st[1].fr_skip);
	PRINTF("fragment state(in):\tkept %lu\tlost %lu\n",
			fp->f_st[0].fr_nfr, fp->f_st[0].fr_bnfr);
	PRINTF("fragment state(out):\tkept %lu\tlost %lu\n",
			fp->f_st[1].fr_nfr, fp->f_st[1].fr_bnfr);
	PRINTF("packet state(in):\tkept %lu\tlost %lu\n",
			fp->f_st[0].fr_ads, fp->f_st[0].fr_bads);
	PRINTF("packet state(out):\tkept %lu\tlost %lu\n",
			fp->f_st[1].fr_ads, fp->f_st[1].fr_bads);
	PRINTF("ICMP replies:\t%lu\tTCP RSTs sent:\t%lu\n",
			fp->f_st[0].fr_ret, fp->f_st[1].fr_ret);
	PRINTF("Result cache hits(in):\t%lu\t(out):\t%lu\n",
			fp->f_st[0].fr_chit, fp->f_st[1].fr_chit);
	PRINTF("IN Pullups succeeded:\t%lu\tfailed:\t%lu\n",
			fp->f_st[0].fr_pull[0], fp->f_st[0].fr_pull[1]);
	PRINTF("OUT Pullups succeeded:\t%lu\tfailed:\t%lu\n",
			fp->f_st[1].fr_pull[0], fp->f_st[1].fr_pull[1]);
	PRINTF("Fastroute successes:\t%lu\tfailures:\t%lu\n",
			fp->f_froute[0], fp->f_froute[1]);
	PRINTF("TCP cksum fails in:\t%lu\tout%lu\n",
			fp->f_st[0].fr_tcpbad, fp->f_st[1].fr_tcpbad);

	PRINTF("Packet log flags set: (%#x)\n", frf);
	if (frf & FF_LOGPASS)
		PRINTF("\tpackets passed through filter\n");
	if (frf & FF_LOGBLOCK)
		PRINTF("\tpackets blocked by filter\n");
	if (frf & FF_LOGNOMATCH)
		PRINTF("\tpackets not matched by filter\n");
	if (!frf)
		PRINTF("\tnone\n");
}


static void printlist(fp)
frentry_t *fp;
{
	struct	frentry	fb;
	int	n;

	for (n = 1; fp; n++) {
		if (kmemcpy((char *)&fb, (u_long)fp, sizeof(fb)) == -1) {
			perror("kmemcpy");
			return;
		}
		fp = &fb;
		if (opts & OPT_OUTQUE)
			fp->fr_flags |= FR_OUTQUE;
		if (opts & (OPT_HITS|OPT_VERBOSE))
#ifdef	USE_QUAD_T
			PRINTF("%qd ", fp->fr_hits);
#else
			PRINTF("%ld ", fp->fr_hits);
#endif
		if (opts & (OPT_ACCNT|OPT_VERBOSE))
#ifdef	USE_QUAD_T
			PRINTF("%qd ", fp->fr_bytes);
#else
			PRINTF("%ld ", fp->fr_bytes);
#endif
		if (opts & OPT_SHOWLINENO)
			PRINTF("@%d ", n);
		printfr(fp);
		if (opts & OPT_VERBOSE)
			binprint(fp);
		if (fp->fr_grp)
			printlist(fp->fr_grp);
		fp = fp->fr_next;
	}
}

/*
 * print out filter rule list
 */
static	void	showlist(fiop)
struct	friostat	*fiop;
{
	struct	frentry	*fp = NULL;
	int	i, set;

	set = fiop->f_active;
	if (opts & OPT_INACTIVE)
		set = 1 - set;
	if (opts & OPT_ACCNT) {
		i = F_AC;
		if (opts & OPT_OUTQUE) {
			fp = (struct frentry *)fiop->f_acctout[set];
			i++;
		} else if (opts & OPT_INQUE)
			fp = (struct frentry *)fiop->f_acctin[set];
		else {
			FPRINTF(stderr, "No -i or -o given with -a\n");
			return;
		}
	} else if (opts & OPT_OUTQUE) {
		i = F_OUT;
		fp = (struct frentry *)fiop->f_fout[set];
	} else if (opts & OPT_INQUE) {
		i = F_IN;
		fp = (struct frentry *)fiop->f_fin[set];
	} else
		return;
	if (opts & OPT_VERBOSE)
		FPRINTF(stderr, "showlist:opts %#x i %d\n", opts, i);

	if (opts & OPT_VERBOSE)
		PRINTF("fp %p set %d\n", fp, set);
	if (!fp) {
		FPRINTF(stderr, "empty list for %s%s\n",
			(opts & OPT_INACTIVE) ? "inactive " : "", filters[i]);
		return;
	}
	printlist(fp);
}


static void showipstates(fd, ipsp)
int fd;
ips_stat_t *ipsp;
{
	ipstate_t *istab[IPSTATE_SIZE], ips;
	int i;

	PRINTF("IP states added:\n\t%lu TCP\n\t%lu UDP\n\t%lu ICMP\n",
		ipsp->iss_tcp, ipsp->iss_udp, ipsp->iss_icmp);
	PRINTF("\t%lu hits\n\t%lu misses\n", ipsp->iss_hits, ipsp->iss_miss);
	PRINTF("\t%lu maximum\n\t%lu no memory\n",
		ipsp->iss_max, ipsp->iss_nomem);
	PRINTF("\t%lu active\n\t%lu expired\n\t%lu closed\n",
		ipsp->iss_active, ipsp->iss_expire, ipsp->iss_fin);
	if (kmemcpy((char *)istab, (u_long)ipsp->iss_table, sizeof(istab)))
		return;
	for (i = 0; i < IPSTATE_SIZE; i++)
		while (istab[i]) {
			if (kmemcpy((char *)&ips, (u_long)istab[i],
				    sizeof(ips)) == -1)
				break;
			PRINTF("%s -> ", inet_ntoa(ips.is_src));
			PRINTF("%s ttl %ld pass %d pr %d state %d/%d\n",
				inet_ntoa(ips.is_dst), ips.is_age,
				ips.is_pass, ips.is_p, ips.is_state[0],
				ips.is_state[1]);
#ifdef	USE_QUAD_T
			PRINTF("\tpkts %qd bytes %qd",
				ips.is_pkts, ips.is_bytes);
#else
			PRINTF("\tpkts %ld bytes %ld",
				ips.is_pkts, ips.is_bytes);
#endif
			if (ips.is_p == IPPROTO_TCP)
				PRINTF("\t%hu -> %hu %lu:%lu %hu:%hu\n",
					ntohs(ips.is_sport),
					ntohs(ips.is_dport),
					ips.is_seq, ips.is_ack,
					ips.is_swin, ips.is_dwin);
			else if (ips.is_p == IPPROTO_UDP)
				PRINTF(" %hu -> %hu\n", ntohs(ips.is_sport),
					ntohs(ips.is_dport));
			else if (ips.is_p == IPPROTO_ICMP)
				PRINTF(" %hu %hu %d\n", ips.is_icmp.ics_id,
					ips.is_icmp.ics_seq,
					ips.is_icmp.ics_type);

			/* phil@ultimate.com ... */
			PRINTF("\t");
			/* from "printfr()" */
			if (ips.is_pass & FR_PASS) {
				PRINTF("pass");
			} else if (ips.is_pass & FR_BLOCK) {
				PRINTF("block");
				if (ips.is_pass & FR_RETICMP)
					PRINTF(" return-icmp");
				if (ips.is_pass & FR_RETRST)
					PRINTF(" return-rst");
			} else if ((ips.is_pass & FR_LOGMASK) == FR_LOG) {
					PRINTF("log");
				if (ips.is_pass & FR_LOGBODY)
					PRINTF(" body");
				if (ips.is_pass & FR_LOGFIRST)
					PRINTF(" first");
			} else if (ips.is_pass & FR_ACCOUNT)
				PRINTF("count");

			if (ips.is_pass & FR_OUTQUE)
				PRINTF(" out");
			else
				PRINTF(" in");

			if ((ips.is_pass & (FR_LOGB|FR_LOGP)) != 0) {
				PRINTF(" log");
				if (ips.is_pass & FR_LOGBODY)
					PRINTF(" body");
				if (ips.is_pass & FR_LOGFIRST)
					PRINTF(" first");
				if (ips.is_pass & FR_LOGORBLOCK)
					PRINTF(" or-block");
			}
			if (ips.is_pass & FR_QUICK)
				PRINTF(" quick");
			if (ips.is_pass & FR_KEEPFRAG)
				PRINTF(" keep frags");
			/* a given; no? */
			if (ips.is_pass & FR_KEEPSTATE)
				PRINTF(" keep state");
			PRINTF("\n");
			/* ... phil@ultimate.com */

			istab[i] = ips.is_next;
		}
}


static void showfrstates(fd, ifsp)
int fd;
ipfrstat_t *ifsp;
{
	struct ipfr *ipfrtab[IPFT_SIZE], ifr;
	int i;

	PRINTF("IP fragment states:\n\t%lu new\n\t%lu expired\n\t%lu hits\n",
		ifsp->ifs_new, ifsp->ifs_expire, ifsp->ifs_hits);
	PRINTF("\t%lu no memory\n\t%lu already exist\n",
		ifsp->ifs_nomem, ifsp->ifs_exists);
	PRINTF("\t%lu inuse\n", ifsp->ifs_inuse);
	if (kmemcpy((char *)ipfrtab, (u_long)ifsp->ifs_table, sizeof(ipfrtab)))
		return;
	for (i = 0; i < IPFT_SIZE; i++)
		while (ipfrtab[i]) {
			if (kmemcpy((char *)&ifr, (u_long)ipfrtab[i],
				    sizeof(ifr)) == -1)
				break;
			PRINTF("%s -> ", inet_ntoa(ifr.ipfr_src));
			PRINTF("%s %d %d %d %#02x = %#x\n",
				inet_ntoa(ifr.ipfr_dst), ifr.ipfr_id,
				ifr.ipfr_ttl, ifr.ipfr_p, ifr.ipfr_tos,
				ifr.ipfr_pass);
			ipfrtab[i] = ifr.ipfr_next;
		}
}


static void showauthstates(fd, asp)
int fd;
fr_authstat_t *asp;
{
#ifdef	USE_QUAD_T
	printf("Authorisation hits: %qd\tmisses %qd\n", asp->fas_hits,
		asp->fas_miss);
#else
	printf("Authorisation hits: %ld\tmisses %ld\n", asp->fas_hits,
		asp->fas_miss);
#endif
	printf("nospace %ld\nadded %ld\nsendfail %ld\nsendok %ld\n",
		asp->fas_nospace, asp->fas_added, asp->fas_sendfail,
		asp->fas_sendok);
	printf("queok %ld\nquefail %ld\nexpire %ld\n",
		asp->fas_queok, asp->fas_quefail, asp->fas_expire);
}
