/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#ifdef __FreeBSD__
# ifndef __FreeBSD_cc_version
#  include <osreldate.h>
# else
#  if __FreeBSD_cc_version < 430000
#   include <osreldate.h>
#  endif
# endif
#endif
#include <stdio.h>
#include <string.h>
#if !defined(__SVR4) && !defined(__svr4__)
# include <strings.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/file.h>
#if defined(STATETOP) && defined(sun) && !defined(__svr4__) && !defined(__SVR4)
#include <sys/select.h>
#endif
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
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netinet/tcp.h>
#if defined(STATETOP) && !defined(linux)
# include <netinet/ip_var.h>
# include <netinet/tcp_fsm.h>
#endif
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "ipf.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_auth.h"
#ifdef STATETOP
# include "netinet/ipl.h"
# include <ctype.h>
# if SOLARIS
#  ifdef ERR
#   undef ERR
#  endif
#  include <curses.h>
# else /* SOLARIS */
#  include <ncurses.h>
# endif /* SOLARIS */
#endif /* STATETOP */
#include "kmem.h"
#if defined(__NetBSD__) || (__OpenBSD__)
# include <paths.h>
#endif

#if !defined(lint)
static const char sccsid[] = "@(#)fils.c	1.21 4/20/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id: fils.c,v 2.21.2.17 2001/07/19 12:24:09 darrenr Exp $";
#endif

extern	char	*optarg;
extern	int	optind;

#define	PRINTF	(void)printf
#define	FPRINTF	(void)fprintf
#define	F_IN	0
#define	F_OUT	1
#define	F_AC	2
static	char	*filters[4] = { "ipfilter(in)", "ipfilter(out)",
				"ipacct(in)", "ipacct(out)" };

int	opts = 0;
#ifdef	USE_INET6
int	use_inet6 = 0;
#endif

#ifdef STATETOP
#define	STSTRSIZE 	80
#define	STGROWSIZE	16
#define	HOSTNMLEN	40

#define	STSORT_PR	0
#define	STSORT_PKTS	1
#define	STSORT_BYTES	2
#define	STSORT_TTL	3
#define	STSORT_MAX	STSORT_TTL
#define	STSORT_DEFAULT	STSORT_BYTES


typedef struct statetop {
	union i6addr	st_src;
	union i6addr	st_dst;
	u_short		st_sport;
	u_short 	st_dport;
	u_char		st_p;
	u_char		st_state[2];
	U_QUAD_T	st_pkts;
	U_QUAD_T	st_bytes;
	u_long		st_age;
} statetop_t;
#endif

extern	int	main __P((int, char *[]));
static	void	showstats __P((int, friostat_t *));
static	void	showfrstates __P((int, ipfrstat_t *));
static	void	showlist __P((friostat_t *));
static	void	showipstates __P((int, ips_stat_t *));
static	void	showauthstates __P((int, fr_authstat_t *));
static	void	showgroups __P((friostat_t *));
static	void	Usage __P((char *));
static	void	printlist __P((frentry_t *));
static	char	*get_ifname __P((void *));
static	char	*hostname __P((int, void *));
static	void	parse_ipportstr __P((const char *, struct in_addr *, int *));
#ifdef STATETOP
static	void	topipstates __P((int, struct in_addr, struct in_addr, int, int, int, int, int));
static	char	*ttl_to_string __P((long));
static	int	sort_p __P((const void *, const void *));
static	int	sort_pkts __P((const void *, const void *));
static	int	sort_bytes __P((const void *, const void *));
static	int	sort_ttl __P((const void *, const void *));
#endif
#if SOLARIS
void showqiflist __P((char *));
#endif

static char *hostname(v, ip)
int v;
void *ip;
{
#ifdef	USE_INET6
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


static void Usage(name)
char *name;
{
#ifdef  USE_INET6
	fprintf(stderr, "Usage: %s [-6aAfhIinosv] [-d <device>]\n", name);
#else
	fprintf(stderr, "Usage: %s [-aAfhIinosv] [-d <device>]\n", name);
#endif
	fprintf(stderr, "\t\t[-M corefile]");
#if	SOLARIS
	fprintf(stderr, " [-N symbol-list]");
#endif
	fprintf(stderr, "\n       %s -t [-S source address] [-D destination address] [-P protocol] [-T refreshtime] [-C] [-d <device>]\n", name);
	exit(1);
}


int main(argc,argv)
int argc;
char *argv[];
{
	fr_authstat_t	frauthst;
	fr_authstat_t	*frauthstp = &frauthst;
	friostat_t fio;
	friostat_t *fiop=&fio;
	ips_stat_t ipsst;
	ips_stat_t *ipsstp = &ipsst;
	ipfrstat_t ifrst;
	ipfrstat_t *ifrstp = &ifrst;
	char	*name = NULL, *device = IPL_NAME, *memf = NULL;
#if SOLARIS
	char	*kern = NULL;
#endif
	int	c, fd, myoptind;
	struct protoent *proto;

	int protocol = -1;		/* -1 = wild card for any protocol */
	int refreshtime = 1; 		/* default update time */
	int sport = -1;			/* -1 = wild card for any source port */
	int dport = -1;			/* -1 = wild card for any dest port */
	int topclosed = 0;		/* do not show closed tcp sessions */
	struct in_addr saddr, daddr;
	saddr.s_addr = INADDR_ANY; 	/* default any source addr */ 
	daddr.s_addr = INADDR_ANY; 	/* default any dest addr */

	/*
	 * Parse these two arguments now lest there be any buffer overflows
	 * in the parsing of the rest.
	 */
	myoptind = optind;
#if SOLARIS
	while ((c = getopt(argc, argv, "6aACfghIilnoqstvd:D:M:N:P:S:T:")) != -1)
#else
	while ((c = getopt(argc, argv, "6aACfghIilnoqstvd:D:M:P:S:T:")) != -1)
#endif
		switch (c)
		{
		case 'M' :
			memf = optarg;
			break;
#if SOLARIS
		case 'N' :
			kern = optarg;
			break;
#endif
		}
	optind = myoptind;

#if SOLARIS
	if (kern != NULL || memf != NULL)
#else
	if (memf != NULL)
#endif
	{
		(void)setuid(getuid());
		(void)setgid(getgid());
	}

	if (openkmem(memf) == -1)
		exit(-1);

	(void)setuid(getuid());
	(void)setgid(getgid());

#if SOLARIS
	while ((c = getopt(argc, argv, "6aACfghIilnoqstvd:D:M:N:P:S:T:")) != -1)
#else
	while ((c = getopt(argc, argv, "6aACfghIilnostvd:D:M:P:S:T:")) != -1)
#endif
	{
		switch (c)
		{
#ifdef	USE_INET6
		case '6' :
			use_inet6 = 1;
			break;
#endif
		case 'a' :
			opts |= OPT_ACCNT|OPT_SHOWLIST;
			break; case 'A' :
			device = IPAUTH_NAME;
			opts |= OPT_AUTHSTATS;
			break;
		case 'C' :
			topclosed = 1;
			break;
		case 'd' :
			device = optarg;
			break;
		case 'D' :
			parse_ipportstr(optarg, &daddr, &dport);
			break;
		case 'f' :
			opts |= OPT_FRSTATES;
			break;
		case 'g' :
			opts |= OPT_GROUPS;
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
		case 'l' :
			opts |= OPT_SHOWLIST;
			break;
		case 'M' :
			break;
		case 'N' :
			break;
		case 'n' :
			opts |= OPT_SHOWLINENO;
			break;
		case 'o' :
			opts |= OPT_OUTQUE|OPT_SHOWLIST;
			break;
		case 'P' :
			if ((proto = getprotobyname(optarg)) != NULL) {
				protocol = proto->p_proto;
			} else if (!sscanf(optarg, "%ud", &protocol) ||
					   (protocol < 0)) {
				fprintf(stderr, "%s : Invalid protocol: %s\n",
					argv[0], optarg);
				exit(-2);
			}
			break;
#if	SOLARIS
		case 'q' :
			showqiflist(kern);
			exit(0);
			break;
#endif
		case 's' :
			opts |= OPT_IPSTATES;
			break;
		case 'S' :
			parse_ipportstr(optarg, &saddr, &sport);
			break;
		case 't' :
#ifdef STATETOP
			opts |= OPT_STATETOP;
			break;
#else
			fprintf(stderr,
				"%s : state top facility not compiled in\n",
				argv[0]);
			exit(-2);
#endif
		case 'T' :
			if (!sscanf(optarg, "%d", &refreshtime) ||
				    (refreshtime <= 0)) {
				fprintf(stderr,
					"%s : Invalid refreshtime < 1 : %s\n",
					argv[0], optarg);
				exit(-2);
			}
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

	if (!(opts & OPT_AUTHSTATS) && ioctl(fd, SIOCGETFS, &fiop) == -1) {
		perror("ioctl(ipf:SIOCGETFS)");
		exit(-1);
	}
	if ((opts & OPT_IPSTATES)) {
		int	sfd = open(IPL_STATE, O_RDONLY);

		if (sfd == -1) {
			perror("open");
			exit(-1);
		}
		if ((ioctl(sfd, SIOCGETFS, &ipsstp) == -1)) {
			perror("ioctl(state:SIOCGETFS)");
			exit(-1);
		}
		close(sfd);
	}
	if ((opts & OPT_FRSTATES) && (ioctl(fd, SIOCGFRST, &ifrstp) == -1)) {
		perror("ioctl(SIOCGFRST)");
		exit(-1);
	}

	if (opts & OPT_VERBOSE)
		PRINTF("opts %#x name %s\n", opts, name ? name : "<>");

	if ((opts & OPT_AUTHSTATS) &&
	    (ioctl(fd, SIOCATHST, &frauthstp) == -1)) {
		perror("ioctl(SIOCATHST)");
		exit(-1);
	}

	if (opts & OPT_IPSTATES) {
		showipstates(fd, ipsstp);
	} else if (opts & OPT_SHOWLIST) {
		showlist(&fio);
		if ((opts & OPT_OUTQUE) && (opts & OPT_INQUE)){
			opts &= ~OPT_OUTQUE;
			showlist(&fio);
		}
	} else {
		if (opts & OPT_FRSTATES)
			showfrstates(fd, ifrstp);
#ifdef STATETOP
		else if (opts & OPT_STATETOP)
			topipstates(fd, saddr, daddr, sport, dport,
				    protocol, refreshtime, topclosed);
#endif
		else if (opts & OPT_AUTHSTATS)
			showauthstates(fd, frauthstp);
		else if (opts & OPT_GROUPS)
			showgroups(&fio);
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
	u_32_t	frf = 0;

	if (ioctl(fd, SIOCGETFF, &frf) == -1)
		perror("ioctl(SIOCGETFF)");

#if SOLARIS
	PRINTF("dropped packets:\tin %lu\tout %lu\n",
			fp->f_st[0].fr_drop, fp->f_st[1].fr_drop);
	PRINTF("non-data packets:\tin %lu\tout %lu\n",
			fp->f_st[0].fr_notdata, fp->f_st[1].fr_notdata);
	PRINTF("no-data packets:\tin %lu\tout %lu\n",
			fp->f_st[0].fr_nodata, fp->f_st[1].fr_nodata);
	PRINTF("non-ip packets:\t\tin %lu\tout %lu\n",
			fp->f_st[0].fr_notip, fp->f_st[1].fr_notip);
	PRINTF("   bad packets:\t\tin %lu\tout %lu\n",
			fp->f_st[0].fr_bad, fp->f_st[1].fr_bad);
	PRINTF("copied messages:\tin %lu\tout %lu\n",
			fp->f_st[0].fr_copy, fp->f_st[1].fr_copy);
#endif
#ifdef	USE_INET6
	PRINTF(" IPv6 packets:\t\tin %lu out %lu\n",
			fp->f_st[0].fr_ipv6[0], fp->f_st[0].fr_ipv6[1]);
#endif
	PRINTF(" input packets:\t\tblocked %lu passed %lu nomatch %lu",
			fp->f_st[0].fr_block, fp->f_st[0].fr_pass,
			fp->f_st[0].fr_nom);
	PRINTF(" counted %lu short %lu\n", 
			fp->f_st[0].fr_acct, fp->f_st[0].fr_short);
	PRINTF("output packets:\t\tblocked %lu passed %lu nomatch %lu",
			fp->f_st[1].fr_block, fp->f_st[1].fr_pass,
			fp->f_st[1].fr_nom);
	PRINTF(" counted %lu short %lu\n", 
			fp->f_st[1].fr_acct, fp->f_st[1].fr_short);
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
	PRINTF("Invalid source(in):\t%lu\n", fp->f_st[0].fr_badsrc);
	PRINTF("Result cache hits(in):\t%lu\t(out):\t%lu\n",
			fp->f_st[0].fr_chit, fp->f_st[1].fr_chit);
	PRINTF("IN Pullups succeeded:\t%lu\tfailed:\t%lu\n",
			fp->f_st[0].fr_pull[0], fp->f_st[0].fr_pull[1]);
	PRINTF("OUT Pullups succeeded:\t%lu\tfailed:\t%lu\n",
			fp->f_st[1].fr_pull[0], fp->f_st[1].fr_pull[1]);
	PRINTF("Fastroute successes:\t%lu\tfailures:\t%lu\n",
			fp->f_froute[0], fp->f_froute[1]);
	PRINTF("TCP cksum fails(in):\t%lu\t(out):\t%lu\n",
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
			PRINTF("%qu ", (unsigned long long) fp->fr_hits);
#else
			PRINTF("%lu ", fp->fr_hits);
#endif
		if (opts & (OPT_ACCNT|OPT_VERBOSE))
#ifdef	USE_QUAD_T
			PRINTF("%qu ", (unsigned long long) fp->fr_bytes);
#else
			PRINTF("%lu ", fp->fr_bytes);
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
	} else {
#ifdef	USE_INET6
		if ((use_inet6) && (opts & OPT_OUTQUE)) {
			i = F_OUT;
			fp = (struct frentry *)fiop->f_fout6[set];
		} else if ((use_inet6) && (opts & OPT_INQUE)) {
			i = F_IN;
			fp = (struct frentry *)fiop->f_fin6[set];
		} else
#endif
		if (opts & OPT_OUTQUE) {
			i = F_OUT;
			fp = (struct frentry *)fiop->f_fout[set];
		} else if (opts & OPT_INQUE) {
			i = F_IN;
			fp = (struct frentry *)fiop->f_fin[set];
		} else
			return;
	}
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

	if (!(opts & OPT_SHOWLIST)) {
		PRINTF("IP states added:\n\t%lu TCP\n\t%lu UDP\n\t%lu ICMP\n",
			ipsp->iss_tcp, ipsp->iss_udp, ipsp->iss_icmp);
		PRINTF("\t%lu hits\n\t%lu misses\n", ipsp->iss_hits,
			ipsp->iss_miss);
		PRINTF("\t%lu maximum\n\t%lu no memory\n\t%lu bkts in use\n",
			ipsp->iss_max, ipsp->iss_nomem, ipsp->iss_inuse);
		PRINTF("\t%lu active\n\t%lu expired\n\t%lu closed\n",
			ipsp->iss_active, ipsp->iss_expire, ipsp->iss_fin);
		return;
	}

	if (kmemcpy((char *)istab, (u_long)ipsp->iss_table, sizeof(istab)))
		return;

	while (ipsp->iss_list) {
		if (kmemcpy((char *)&ips, (u_long)ipsp->iss_list, sizeof(ips)))
			break;
		ipsp->iss_list = ips.is_next;
		PRINTF("%s -> ", hostname(ips.is_v, &ips.is_src.in4));
		PRINTF("%s ttl %ld pass %#x pr %d state %d/%d\n",
			hostname(ips.is_v, &ips.is_dst.in4),
			ips.is_age, ips.is_pass, ips.is_p,
			ips.is_state[0], ips.is_state[1]);
#ifdef	USE_QUAD_T
		PRINTF("\tpkts %qu bytes %qu",
			(unsigned long long) ips.is_pkts,
			(unsigned long long) ips.is_bytes);
#else
		PRINTF("\tpkts %ld bytes %ld", ips.is_pkts, ips.is_bytes);
#endif
		if (ips.is_p == IPPROTO_TCP)
#if defined(NetBSD) && (NetBSD >= 199905) && (NetBSD < 1991011) || \
    (__FreeBSD_version >= 220000) || defined(__OpenBSD__)
			PRINTF("\t%hu -> %hu %x:%x %hu:%hu",
				ntohs(ips.is_sport), ntohs(ips.is_dport),
				ips.is_send, ips.is_dend,
				ips.is_maxswin, ips.is_maxdwin);
#else
			PRINTF("\t%hu -> %hu %x:%x %hu:%hu",
				ntohs(ips.is_sport), ntohs(ips.is_dport),
				ips.is_send, ips.is_dend,
				ips.is_maxswin, ips.is_maxdwin);
#endif
		else if (ips.is_p == IPPROTO_UDP)
			PRINTF(" %hu -> %hu", ntohs(ips.is_sport),
				ntohs(ips.is_dport));
		else if (ips.is_p == IPPROTO_ICMP
#ifdef	USE_INET6
			 || ips.is_p == IPPROTO_ICMPV6
#endif
			)
			PRINTF(" %hu %hu %d", ips.is_icmp.ics_id,
				ips.is_icmp.ics_seq, ips.is_icmp.ics_type);

		PRINTF("\n\t");

		if (ips.is_pass & FR_PASS) {
			PRINTF("pass");
		} else if (ips.is_pass & FR_BLOCK) {
			PRINTF("block");
			switch (ips.is_pass & FR_RETMASK)
			{
			case FR_RETICMP :
				PRINTF(" return-icmp");
				break;
			case FR_FAKEICMP :
				PRINTF(" return-icmp-as-dest");
				break;
			case FR_RETRST :
				PRINTF(" return-rst");
				break;
			default :
				break;
			}
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

		if ((ips.is_pass & FR_LOG) != 0) {
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
		PRINTF("\tIPv%d", ips.is_v);
		PRINTF("\n");

		PRINTF("\tpkt_flags & %x(%x) = %x,\t",
			ips.is_flags & 0xf, ips.is_flags,
			ips.is_flags >> 4);
		PRINTF("\tpkt_options & %x = %x\n", ips.is_optmsk,
			ips.is_opt);
		PRINTF("\tpkt_security & %x = %x, pkt_auth & %x = %x\n",
			ips.is_secmsk, ips.is_sec, ips.is_authmsk,
			ips.is_auth);
		PRINTF("\tinterfaces: in %s[%p] ",
		       get_ifname(ips.is_ifpin), ips.is_ifpin);
		PRINTF("out %s[%p]\n",
		       get_ifname(ips.is_ifpout), ips.is_ifpout);
	}
}


#if SOLARIS
void showqiflist(kern)
char *kern;
{
	struct nlist qifnlist[2] = {
		{ "qif_head" },
		{ NULL }
	};
	qif_t qif, *qf;

	if (kern == NULL)
		kern = "/dev/ksyms";

	if (nlist(kern, qifnlist) == -1) {
		fprintf(stderr, "nlist error\n");
		return;
	}

	printf("List of interfaces bound by IPFilter:\n");
	if (kmemcpy((char *)&qf, (u_long)qifnlist[0].n_value, sizeof(qf)))
		return;
	while (qf) {
		if (kmemcpy((char *)&qif, (u_long)qf, sizeof(qif)))
			break;
		printf("\tName: %-8s Header Length: %2d SAP: %s (%04x)\n",
			qif.qf_name, qif.qf_hl,
#ifdef	IP6_DL_SAP
			(qif.qf_sap == IP6_DL_SAP) ? "IPv6" : "IPv4"
#else
			"IPv4"
#endif
			, qif.qf_sap);
		qf = qif.qf_next;
	}
}
#endif


#ifdef STATETOP
static void topipstates(fd, saddr, daddr, sport, dport, protocol,
		        refreshtime, topclosed)
int fd;
struct in_addr saddr;
struct in_addr daddr;
int sport;
int dport;
int protocol;
int refreshtime;
int topclosed;
{
	char str1[STSTRSIZE], str2[STSTRSIZE], str3[STSTRSIZE], str4[STSTRSIZE];
	int maxtsentries = 0, reverse = 0, sorting = STSORT_DEFAULT;
	int i, j, sfd, winx, tsentry, maxx, maxy, redraw = 0;
	ipstate_t *istab[IPSTATE_SIZE], ips;
	ips_stat_t ipsst, *ipsstp = &ipsst;
	statetop_t *tstable = NULL, *tp;
	struct timeval selecttimeout; 
	char hostnm[HOSTNMLEN];
	struct protoent *proto;
	fd_set readfd;
	int c = 0;
	time_t t;

	/* open state device */
	if ((sfd = open(IPL_STATE, O_RDONLY)) == -1) {
		perror("open");
		exit(-1);
	}

	/* init ncurses stuff */
  	initscr();
  	cbreak();
  	noecho();

	/* init hostname */
	gethostname(hostnm, sizeof(hostnm) - 1);
	hostnm[sizeof(hostnm) - 1] = '\0';

	/* repeat until user aborts */
	while ( 1 ) {

		/* get state table */
		bzero((char *)&ipsst, sizeof(&ipsst));
		if ((ioctl(sfd, SIOCGETFS, &ipsstp) == -1)) {
			perror("ioctl(SIOCGETFS)");
			exit(-1);
		}
		if (kmemcpy((char *)istab, (u_long)ipsstp->iss_table,
			    sizeof(ips)))
			return;

		/* clear the history */
		tsentry = -1;

		/* read the state table and store in tstable */
		while (ipsstp->iss_list) {
			if (kmemcpy((char *)&ips, (u_long)ipsstp->iss_list,
				    sizeof(ips)))
				break;
			ipsstp->iss_list = ips.is_next;

			if (((saddr.s_addr == INADDR_ANY) ||
			     (saddr.s_addr == ips.is_saddr)) &&
			    ((daddr.s_addr == INADDR_ANY) ||
			     (daddr.s_addr == ips.is_daddr)) &&
			    ((protocol < 0) || (protocol == ips.is_p)) &&
			    (((ips.is_p != IPPROTO_TCP) &&
			     (ips.is_p != IPPROTO_UDP)) || 
			     (((sport < 0) ||
			       (htons(sport) == ips.is_sport)) &&
			      ((dport < 0) ||
			       (htons(dport) == ips.is_dport)))) &&
			     (topclosed || (ips.is_p != IPPROTO_TCP) ||
			     (ips.is_state[0] < TCPS_CLOSE_WAIT) ||
			     (ips.is_state[1] < TCPS_CLOSE_WAIT))) { 
				/*
				 * if necessary make room for this state
				 * entry
				 */
				tsentry++;
				if (!maxtsentries ||
				    (tsentry == maxtsentries)) {

					maxtsentries += STGROWSIZE;
					tstable = realloc(tstable, maxtsentries * sizeof(statetop_t));
					if (!tstable) {
						perror("malloc");
						exit(-1);
					}
				}

				/* fill structure */
				tp = tstable + tsentry;
				tp->st_src = ips.is_src;
				tp->st_dst = ips.is_dst;
				tp->st_p = ips.is_p;
				tp->st_state[0] = ips.is_state[0];
				tp->st_state[1] = ips.is_state[1];
				tp->st_pkts = ips.is_pkts;
				tp->st_bytes = ips.is_bytes;
				tp->st_age = ips.is_age;
				if ((ips.is_p == IPPROTO_TCP) ||
				    (ips.is_p == IPPROTO_UDP)) {
					tp->st_sport = ips.is_sport;
					tp->st_dport = ips.is_dport;
				}

			}
		}


		/* sort the array */
		if (tsentry != -1)
			switch (sorting)
			{
			case STSORT_PR:
				qsort(tstable, tsentry + 1,
				      sizeof(statetop_t), sort_p);
				break;
			case STSORT_PKTS:
				qsort(tstable, tsentry + 1,
				      sizeof(statetop_t), sort_pkts);
				break;
			case STSORT_BYTES:
				qsort(tstable, tsentry + 1,
				      sizeof(statetop_t), sort_bytes);
				break;
			case STSORT_TTL:
				qsort(tstable, tsentry + 1,
				      sizeof(statetop_t), sort_ttl);
				break;
			default:
				break;
			}

		/* print title */
		erase();
		getmaxyx(stdscr, maxy, maxx);
		attron(A_BOLD);
		winx = 0;
		move(winx,0);
		sprintf(str1, "%s - %s - state top", hostnm, IPL_VERSION);
		for (j = 0 ; j < (maxx - 8 - strlen(str1)) / 2; j++)
			printw(" ");
		printw("%s", str1);
		attroff(A_BOLD);

		/* just for fun add a clock */
		move(winx, maxx - 8);
		t = time(NULL);
		strftime(str1, 80, "%T", localtime(&t));
		printw("%s\n", str1);

		/*
		 * print the display filters, this is placed in the loop, 
		 * because someday I might add code for changing these
		 * while the programming is running :-)
		 */
		if (sport >= 0)
			sprintf(str1, "%s,%d", inet_ntoa(saddr), sport);
		else
			sprintf(str1, "%s", inet_ntoa(saddr));

		if (dport >= 0)
			sprintf(str2, "%s,%d", inet_ntoa(daddr), dport);
		else
			sprintf(str2, "%s", inet_ntoa(daddr));

		if (protocol < 0)
			strcpy(str3, "any");
		else if ((proto = getprotobynumber(protocol)) != NULL)
			sprintf(str3, "%s", proto->p_name); 
		else
			sprintf(str3, "%d", protocol);

		switch (sorting)
		{
		case STSORT_PR:
			sprintf(str4, "proto");
			break;
		case STSORT_PKTS:
			sprintf(str4, "# pkts");
			break;
		case STSORT_BYTES:
			sprintf(str4, "# bytes");
			break;
		case STSORT_TTL:
			sprintf(str4, "ttl");
			break;
		default:
			sprintf(str4, "unknown");
			break;
		}

		if (reverse)
			strcat(str4, " (reverse)");

		winx += 2;
		move(winx,0);
		printw("Src = %s  Dest = %s  Proto = %s  Sorted by = %s\n\n",
		       str1, str2, str3, str4);

		/* print column description */
		winx += 2;
		move(winx,0);
		attron(A_BOLD);
		printw("%-21s %-21s %3s %4s %7s %9s %9s\n", "Source IP",
		       "Destination IP", "ST", "PR", "#pkts", "#bytes", "ttl");
		attroff(A_BOLD);

		/* print all the entries */
		tp = tstable;
		if (reverse)
			tp += tsentry;

		if (tsentry > maxy - 6)
			tsentry = maxy - 6;
		for (i = 0; i <= tsentry; i++) {
			/* print src/dest and port */
			if ((tp->st_p == IPPROTO_TCP) ||
			    (tp->st_p == IPPROTO_UDP)) {
				sprintf(str1, "%s,%hu",
					inet_ntoa(tp->st_src.in4),
					ntohs(tp->st_sport));
				sprintf(str2, "%s,%hu",
					inet_ntoa(tp->st_dst.in4),
					ntohs(tp->st_dport));
			} else {
				sprintf(str1, "%s", inet_ntoa(tp->st_src.in4));
				sprintf(str2, "%s", inet_ntoa(tp->st_dst.in4));
			}
			winx++;
			move(winx, 0);
			printw("%-21s %-21s", str1, str2);

			/* print state */
			sprintf(str1, "%X/%X", tp->st_state[0],
				tp->st_state[1]);
			printw(" %3s", str1);

			/* print proto */
			proto = getprotobynumber(tp->st_p);
			if (proto) {
				strncpy(str1, proto->p_name, 4);
				str1[4] = '\0';
			} else {
				sprintf(str1, "%d", tp->st_p);
			}
			printw(" %4s", str1);
				/* print #pkt/#bytes */
#ifdef	USE_QUAD_T
			printw(" %7qu %9qu", (unsigned long long) tp->st_pkts,
				(unsigned long long) tp->st_bytes);
#else
			printw(" %7lu %9lu", tp->st_pkts, tp->st_bytes);
#endif
			printw(" %9s", ttl_to_string(tp->st_age));

			if (reverse)
				tp--;
			else
				tp++;
		}

		/* screen data structure is filled, now update the screen */
		if (redraw)
			clearok(stdscr,1);

		refresh();
		if (redraw) {
			clearok(stdscr,0);
			redraw = 0;
		}

		/* wait for key press or a 1 second time out period */
		selecttimeout.tv_sec = refreshtime;
		selecttimeout.tv_usec = 0;
		FD_ZERO(&readfd);
		FD_SET(0, &readfd);
		select(1, &readfd, NULL, NULL, &selecttimeout);

		/* if key pressed, read all waiting keys */
		if (FD_ISSET(0, &readfd)) {
			c = wgetch(stdscr);
			if (c == ERR)
				continue;

			if (tolower(c) == 'l') {
				redraw = 1;
			} else if (tolower(c) == 'q') {
				nocbreak();
				endwin();
				exit(0);
			} else if (tolower(c) == 'r') {
				reverse = !reverse;
			} else if (tolower(c) == 's') {
				sorting++;
				if (sorting > STSORT_MAX)
					sorting = 0;
			}
		}
	} /* while */

	close(sfd);

	printw("\n");
	nocbreak();
	endwin();
}
#endif

static void showfrstates(fd, ifsp)
int fd;
ipfrstat_t *ifsp;
{
	struct ipfr *ipfrtab[IPFT_SIZE], ifr;
	frentry_t fr;
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
			PRINTF("%s -> ", hostname(4, &ifr.ipfr_src));
			if (kmemcpy((char *)&fr, (u_long)ifr.ipfr_rule,
				    sizeof(fr)) == -1)
				break;
			PRINTF("%s %d %d %d %#02x = %#x\n",
				hostname(4, &ifr.ipfr_dst), ifr.ipfr_id,
				ifr.ipfr_ttl, ifr.ipfr_p, ifr.ipfr_tos,
				fr.fr_flags);
			ipfrtab[i] = ifr.ipfr_next;
		}
	if (kmemcpy((char *)ipfrtab, (u_long)ifsp->ifs_nattab,sizeof(ipfrtab)))
		return;
	for (i = 0; i < IPFT_SIZE; i++)
		while (ipfrtab[i]) {
			if (kmemcpy((char *)&ifr, (u_long)ipfrtab[i],
				    sizeof(ifr)) == -1)
				break;
			PRINTF("NAT: %s -> ", hostname(4, &ifr.ipfr_src));
			if (kmemcpy((char *)&fr, (u_long)ifr.ipfr_rule,
				    sizeof(fr)) == -1)
				break;
			PRINTF("%s %d %d %d %#02x = %#x\n",
				hostname(4, &ifr.ipfr_dst), ifr.ipfr_id,
				ifr.ipfr_ttl, ifr.ipfr_p, ifr.ipfr_tos,
				fr.fr_flags);
			ipfrtab[i] = ifr.ipfr_next;
		}
}


static void showauthstates(fd, asp)
int fd;
fr_authstat_t *asp;
{
	frauthent_t *frap, fra;

#ifdef	USE_QUAD_T
	printf("Authorisation hits: %qu\tmisses %qu\n",
		(unsigned long long) asp->fas_hits,
		(unsigned long long) asp->fas_miss);
#else
	printf("Authorisation hits: %ld\tmisses %ld\n", asp->fas_hits,
		asp->fas_miss);
#endif
	printf("nospace %ld\nadded %ld\nsendfail %ld\nsendok %ld\n",
		asp->fas_nospace, asp->fas_added, asp->fas_sendfail,
		asp->fas_sendok);
	printf("queok %ld\nquefail %ld\nexpire %ld\n",
		asp->fas_queok, asp->fas_quefail, asp->fas_expire);

	frap = asp->fas_faelist;
	while (frap) {
		if (kmemcpy((char *)&fra, (u_long)frap, sizeof(fra)) == -1)
			break;

		printf("age %ld\t", fra.fae_age);
		printfr(&fra.fae_fr);
		frap = fra.fae_next;
	}
}


static char *get_ifname(ptr)
void *ptr;
{
#if SOLARIS
	char *ifname;
	ill_t ill;

	if (ptr == (void *)-1)
		return "!";
	if (ptr == NULL)
		return "-";

	if (kmemcpy((char *)&ill, (u_long)ptr, sizeof(ill)) == -1)
		return "X";
	ifname = malloc(ill.ill_name_length + 1);
	if (kmemcpy(ifname, (u_long)ill.ill_name,
		    ill.ill_name_length) == -1)
		return "X";
	return ifname;
#else
# if defined(NetBSD) && (NetBSD >= 199905) && (NetBSD < 1991011) || \
    defined(__OpenBSD__)
#else
	char buf[32];
	int len;
# endif
	struct ifnet netif;

	if (ptr == (void *)-1)
		return "!";
	if (ptr == NULL)
		return "-";

	if (kmemcpy((char *)&netif, (u_long)ptr, sizeof(netif)) == -1)
		return "X";
# if defined(NetBSD) && (NetBSD >= 199905) && (NetBSD < 1991011) || \
    defined(__OpenBSD__)
	return strdup(netif.if_xname);
# else
	if (kstrncpy(buf, (u_long)netif.if_name, sizeof(buf)) == -1)
		return "X";
	if (netif.if_unit < 10)
		len = 2;
	else if (netif.if_unit < 1000)
		len = 3;
	else if (netif.if_unit < 10000)
		len = 4;
	else
		len = 5;
	buf[sizeof(buf) - len] = '\0';
	sprintf(buf + strlen(buf), "%d", netif.if_unit % 10000);
	return strdup(buf);
# endif
#endif
}


static void showgroups(fiop)
struct friostat	*fiop;
{
	static char *gnames[3] = { "Filter", "Accounting", "Authentication" };
	frgroup_t *fp, grp;
	int on, off, i;

	on = fiop->f_active;
	off = 1 - on;

	for (i = 0; i < 3; i++) {
		printf("%s groups (active):\n", gnames[i]);
		for (fp = fiop->f_groups[i][on]; fp; fp = grp.fg_next)
			if (kmemcpy((char *)&grp, (u_long)fp, sizeof(grp)))
				break;
			else
				printf("%hu\n", grp.fg_num);
		printf("%s groups (inactive):\n", gnames[i]);
		for (fp = fiop->f_groups[i][off]; fp; fp = grp.fg_next)
			if (kmemcpy((char *)&grp, (u_long)fp, sizeof(grp)))
				break;
			else
				printf("%hu\n", grp.fg_num);
	}
}

static void parse_ipportstr(argument, ip, port)
const char *argument;
struct in_addr *ip;
int *port;
{

	char *s, *comma;

	/* make working copy of argument, Theoretically you must be able
	 * to write to optarg, but that seems very ugly to me....
	 */
	if ((s = malloc(strlen(argument) + 1)) == NULL)
		perror("malloc");
	strcpy(s, argument);

	/* get port */
	if ((comma = strchr(s, ',')) != NULL) {
		if (!strcasecmp(s, "any")) {
			*port = -1;
		} else if (!sscanf(comma + 1, "%d", port) ||
			   (*port < 0) || (*port > 65535)) {
			fprintf(stderr, "Invalid port specfication in %s\n",
				argument);
			exit(-2);
		}
		*comma = '\0';
	}


	/* get ip address */
	if (!strcasecmp(s, "any")) {
		ip->s_addr = INADDR_ANY;
	} else	if (!inet_aton(s, ip)) {
		fprintf(stderr, "Invalid IP address: %s\n", s);
		exit(-2);
	}

	/* free allocated memory */
	free(s);
}


#ifdef STATETOP
static char ttlbuf[STSTRSIZE];

static char *ttl_to_string(ttl)
long int ttl;
{

	int hours, minutes, seconds;

	/* ttl is in half seconds */
	ttl /= 2;

	hours = ttl / 3600;
	ttl = ttl % 3600;
	minutes = ttl / 60;
	seconds = ttl % 60;

	if (hours > 0 )
		sprintf(ttlbuf, "%2d:%02d:%02d", hours, minutes, seconds);
	else
		sprintf(ttlbuf, "%2d:%02d", minutes, seconds);
	return ttlbuf;
}


static int sort_pkts(a, b)
const void *a;
const void *b;
{

	register const statetop_t *ap = a;
	register const statetop_t *bp = b;

	if (ap->st_pkts == bp->st_pkts)
		return 0;
	else if (ap->st_pkts < bp->st_pkts)
		return 1;
	return -1;
}


static int sort_bytes(a, b)
const void *a;
const void *b;
{
	register const statetop_t *ap = a;
	register const statetop_t *bp = b;

	if (ap->st_bytes == bp->st_bytes)
		return 0;
	else if (ap->st_bytes < bp->st_bytes)
		return 1;
	return -1;
}


static int sort_p(a, b)
const void *a;
const void *b;
{
	register const statetop_t *ap = a;
	register const statetop_t *bp = b;

	if (ap->st_p == bp->st_p)
		return 0;
	else if (ap->st_p < bp->st_p)
		return 1;
	return -1;
}


static int sort_ttl(a, b)
const void *a;
const void *b;
{
	register const statetop_t *ap = a;
	register const statetop_t *bp = b;

	if (ap->st_age == bp->st_age)
		return 0;
	else if (ap->st_age < bp->st_age)
		return 1;
	return -1;
}
#endif
