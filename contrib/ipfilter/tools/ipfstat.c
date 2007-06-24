/*	$FreeBSD$	*/

/*
 * Copyright (C) 2002-2006 by Darren Reed.
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
#include <sys/ioctl.h>
#include <fcntl.h>
#ifdef linux
# include <linux/a.out.h>
#else
# include <nlist.h>
#endif
#include <ctype.h>
#if defined(sun) && (defined(__svr4__) || defined(__SVR4))
# include <stddef.h>
#endif
#include "ipf.h"
#include "netinet/ipl.h"
#if defined(STATETOP)
# if defined(_BSDI_VERSION)
#  undef STATETOP
# endif
# if defined(__FreeBSD__) && \
     (!defined(__FreeBSD_version) || (__FreeBSD_version < 430000))
#  undef STATETOP
# endif
# if defined(__NetBSD_Version__) && (__NetBSD_Version__ < 105000000)
#  undef STATETOP
# endif
# if defined(sun)
#  if defined(__svr4__) || defined(__SVR4)
#   include <sys/select.h>
#  else
#   undef STATETOP	/* NOT supported on SunOS4 */
#  endif
# endif
#endif
#if defined(STATETOP) && !defined(linux)
# include <netinet/ip_var.h>
# include <netinet/tcp_fsm.h>
#endif
#ifdef STATETOP
# include <ctype.h>
# include <signal.h>
# include <time.h>
# if SOLARIS || defined(__NetBSD__) || defined(_BSDI_VERSION) || \
     defined(__sgi)
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
static const char rcsid[] = "@(#)$Id: ipfstat.c,v 1.44.2.23 2007/05/31 13:13:02 darrenr Exp $";
#endif

#ifdef __hpux
# define	nlist	nlist64
#endif

extern	char	*optarg;
extern	int	optind;
extern	int	opterr;

#define	PRINTF	(void)printf
#define	FPRINTF	(void)fprintf
static	char	*filters[4] = { "ipfilter(in)", "ipfilter(out)",
				"ipacct(in)", "ipacct(out)" };
static	int	state_logging = -1;

int	opts = 0;
int	use_inet6 = 0;
int	live_kernel = 1;
int	state_fd = -1;
int	ipf_fd = -1;
int	auth_fd = -1;
int	nat_fd = -1;
frgroup_t *grtop = NULL;
frgroup_t *grtail = NULL;

#ifdef STATETOP
#define	STSTRSIZE 	80
#define	STGROWSIZE	16
#define	HOSTNMLEN	40

#define	STSORT_PR	0
#define	STSORT_PKTS	1
#define	STSORT_BYTES	2
#define	STSORT_TTL	3
#define	STSORT_SRCIP	4
#define	STSORT_SRCPT	5
#define	STSORT_DSTIP	6
#define	STSORT_DSTPT	7
#define	STSORT_MAX	STSORT_DSTPT
#define	STSORT_DEFAULT	STSORT_BYTES


typedef struct statetop {
	i6addr_t	st_src;
	i6addr_t	st_dst;
	u_short		st_sport;
	u_short 	st_dport;
	u_char		st_p;
	u_char		st_v;
	u_char		st_state[2];
	U_QUAD_T	st_pkts;
	U_QUAD_T	st_bytes;
	u_long		st_age;
} statetop_t;
#endif

int		main __P((int, char *[]));

static	int	fetchfrag __P((int, int, ipfr_t *));
static	void	showstats __P((friostat_t *, u_32_t));
static	void	showfrstates __P((ipfrstat_t *, u_long));
static	void	showlist __P((friostat_t *));
static	void	showipstates __P((ips_stat_t *));
static	void	showauthstates __P((fr_authstat_t *));
static	void	showgroups __P((friostat_t *));
static	void	usage __P((char *));
static	void	showtqtable_live __P((int));
static	void	printlivelist __P((int, int, frentry_t *, char *, char *));
static	void	printdeadlist __P((int, int, frentry_t *, char *, char *));
static	void	parse_ipportstr __P((const char *, i6addr_t *, int *));
static	void	ipfstate_live __P((char *, friostat_t **, ips_stat_t **,
				   ipfrstat_t **, fr_authstat_t **, u_32_t *));
static	void	ipfstate_dead __P((char *, friostat_t **, ips_stat_t **,
				   ipfrstat_t **, fr_authstat_t **, u_32_t *));
static	ipstate_t *fetchstate __P((ipstate_t *, ipstate_t *));
#ifdef STATETOP
static	void	topipstates __P((i6addr_t, i6addr_t, int, int, int,
				 int, int, int));
static	void	sig_break __P((int));
static	void	sig_resize __P((int));
static	char	*getip __P((int, i6addr_t *));
static	char	*ttl_to_string __P((long));
static	int	sort_p __P((const void *, const void *));
static	int	sort_pkts __P((const void *, const void *));
static	int	sort_bytes __P((const void *, const void *));
static	int	sort_ttl __P((const void *, const void *));
static	int	sort_srcip __P((const void *, const void *));
static	int	sort_srcpt __P((const void *, const void *));
static	int	sort_dstip __P((const void *, const void *));
static	int	sort_dstpt __P((const void *, const void *));
#endif


static void usage(name)
char *name;
{
#ifdef  USE_INET6
	fprintf(stderr, "Usage: %s [-6aAdfghIilnoRsv]\n", name);
#else
	fprintf(stderr, "Usage: %s [-aAdfghIilnoRsv]\n", name);
#endif
	fprintf(stderr, "       %s [-M corefile] [-N symbol-list]\n", name);
#ifdef	USE_INET6
	fprintf(stderr, "       %s -t [-6C] ", name);
#else
	fprintf(stderr, "       %s -t [-C] ", name);
#endif
	fprintf(stderr, "[-D destination address] [-P protocol] [-S source address] [-T refresh time]\n");
	exit(1);
}


int main(argc,argv)
int argc;
char *argv[];
{
	fr_authstat_t	frauthst;
	fr_authstat_t	*frauthstp = &frauthst;
	friostat_t fio;
	friostat_t *fiop = &fio;
	ips_stat_t ipsst;
	ips_stat_t *ipsstp = &ipsst;
	ipfrstat_t ifrst;
	ipfrstat_t *ifrstp = &ifrst;
	char	*memf = NULL;
	char	*options, *kern = NULL;
	int	c, myoptind;

	int protocol = -1;		/* -1 = wild card for any protocol */
	int refreshtime = 1; 		/* default update time */
	int sport = -1;			/* -1 = wild card for any source port */
	int dport = -1;			/* -1 = wild card for any dest port */
	int topclosed = 0;		/* do not show closed tcp sessions */
	i6addr_t saddr, daddr;
	u_32_t frf;

#ifdef	USE_INET6
	options = "6aACdfghIilnostvD:M:N:P:RS:T:";
#else
	options = "aACdfghIilnostvD:M:N:P:RS:T:";
#endif

	saddr.in4.s_addr = INADDR_ANY; 	/* default any v4 source addr */
	daddr.in4.s_addr = INADDR_ANY; 	/* default any v4 dest addr */
#ifdef	USE_INET6
	saddr.in6 = in6addr_any;	/* default any v6 source addr */
	daddr.in6 = in6addr_any;	/* default any v6 dest addr */
#endif

	/* Don't warn about invalid flags when we run getopt for the 1st time */
	opterr = 0;

	/*
	 * Parse these two arguments now lest there be any buffer overflows
	 * in the parsing of the rest.
	 */
	myoptind = optind;
	while ((c = getopt(argc, argv, options)) != -1) {
		switch (c)
		{
		case 'M' :
			memf = optarg;
			live_kernel = 0;
			break;
		case 'N' :
			kern = optarg;
			live_kernel = 0;
			break;
		}
	}
	optind = myoptind;

	if (live_kernel == 1) {
		if ((state_fd = open(IPSTATE_NAME, O_RDONLY)) == -1) {
			perror("open(IPSTATE_NAME)");
			exit(-1);
		}
		if ((auth_fd = open(IPAUTH_NAME, O_RDONLY)) == -1) {
			perror("open(IPAUTH_NAME)");
			exit(-1);
		}
		if ((nat_fd = open(IPNAT_NAME, O_RDONLY)) == -1) {
			perror("open(IPAUTH_NAME)");
			exit(-1);
		}
		if ((ipf_fd = open(IPL_NAME, O_RDONLY)) == -1) {
			fprintf(stderr, "open(%s)", IPL_NAME);
			perror("");
			exit(-1);
		}
	}

	if (kern != NULL || memf != NULL) {
		(void)setgid(getgid());
		(void)setuid(getuid());
	}

	if (live_kernel == 1) {
		(void) checkrev(IPL_NAME);
	} else {
		if (openkmem(kern, memf) == -1)
			exit(-1);
	}

	(void)setgid(getgid());
	(void)setuid(getuid());

	opterr = 1;

	while ((c = getopt(argc, argv, options)) != -1)
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
			break;
		case 'A' :
			opts |= OPT_AUTHSTATS;
			break;
		case 'C' :
			topclosed = 1;
			break;
		case 'd' :
			opts |= OPT_DEBUG;
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
			protocol = getproto(optarg);
			if (protocol == -1) {
				fprintf(stderr, "%s: Invalid protocol: %s\n",
					argv[0], optarg);
				exit(-2);
			}
			break;
		case 'R' :
			opts |= OPT_NORESOLVE;
			break;
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
				"%s: state top facility not compiled in\n",
				argv[0]);
			exit(-2);
#endif
		case 'T' :
			if (!sscanf(optarg, "%d", &refreshtime) ||
				    (refreshtime <= 0)) {
				fprintf(stderr,
					"%s: Invalid refreshtime < 1 : %s\n",
					argv[0], optarg);
				exit(-2);
			}
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		default :
			usage(argv[0]);
			break;
		}
	}

	if (live_kernel == 1) {
		bzero((char *)&fio, sizeof(fio));
		bzero((char *)&ipsst, sizeof(ipsst));
		bzero((char *)&ifrst, sizeof(ifrst));

		ipfstate_live(IPL_NAME, &fiop, &ipsstp, &ifrstp,
			      &frauthstp, &frf);
	} else
		ipfstate_dead(kern, &fiop, &ipsstp, &ifrstp, &frauthstp, &frf);

	if (opts & OPT_IPSTATES) {
		showipstates(ipsstp);
	} else if (opts & OPT_SHOWLIST) {
		showlist(fiop);
		if ((opts & OPT_OUTQUE) && (opts & OPT_INQUE)){
			opts &= ~OPT_OUTQUE;
			showlist(fiop);
		}
	} else if (opts & OPT_FRSTATES)
		showfrstates(ifrstp, fiop->f_ticks);
#ifdef STATETOP
	else if (opts & OPT_STATETOP)
		topipstates(saddr, daddr, sport, dport, protocol,
			    use_inet6 ? 6 : 4, refreshtime, topclosed);
#endif
	else if (opts & OPT_AUTHSTATS)
		showauthstates(frauthstp);
	else if (opts & OPT_GROUPS)
		showgroups(fiop);
	else
		showstats(fiop, frf);

	return 0;
}


/*
 * Fill in the stats structures from the live kernel, using a combination
 * of ioctl's and copying directly from kernel memory.
 */
static void ipfstate_live(device, fiopp, ipsstpp, ifrstpp, frauthstpp, frfp)
char *device;
friostat_t **fiopp;
ips_stat_t **ipsstpp;
ipfrstat_t **ifrstpp;
fr_authstat_t **frauthstpp;
u_32_t *frfp;
{
	ipfobj_t ipfo;

	if (checkrev(device) == -1) {
		fprintf(stderr, "User/kernel version check failed\n");
		exit(1);
	}

	if ((opts & OPT_AUTHSTATS) == 0) {
		bzero((caddr_t)&ipfo, sizeof(ipfo));
		ipfo.ipfo_rev = IPFILTER_VERSION;
		ipfo.ipfo_type = IPFOBJ_IPFSTAT;
		ipfo.ipfo_size = sizeof(friostat_t);
		ipfo.ipfo_ptr = (void *)*fiopp;

		if (ioctl(ipf_fd, SIOCGETFS, &ipfo) == -1) {
			perror("ioctl(ipf:SIOCGETFS)");
			exit(-1);
		}

		if (ioctl(ipf_fd, SIOCGETFF, frfp) == -1)
			perror("ioctl(SIOCGETFF)");
	}

	if ((opts & OPT_IPSTATES) != 0) {

		bzero((caddr_t)&ipfo, sizeof(ipfo));
		ipfo.ipfo_rev = IPFILTER_VERSION;
		ipfo.ipfo_type = IPFOBJ_STATESTAT;
		ipfo.ipfo_size = sizeof(ips_stat_t);
		ipfo.ipfo_ptr = (void *)*ipsstpp;

		if ((ioctl(state_fd, SIOCGETFS, &ipfo) == -1)) {
			perror("ioctl(state:SIOCGETFS)");
			exit(-1);
		}
		if (ioctl(state_fd, SIOCGETLG, &state_logging) == -1) {
			perror("ioctl(state:SIOCGETLG)");
			exit(-1);
		}
	}

	if ((opts & OPT_FRSTATES) != 0) {
		bzero((caddr_t)&ipfo, sizeof(ipfo));
		ipfo.ipfo_rev = IPFILTER_VERSION;
		ipfo.ipfo_type = IPFOBJ_FRAGSTAT;
		ipfo.ipfo_size = sizeof(ipfrstat_t);
		ipfo.ipfo_ptr = (void *)*ifrstpp;
	
		if (ioctl(ipf_fd, SIOCGFRST, &ipfo) == -1) {
			perror("ioctl(SIOCGFRST)");
			exit(-1);
		}
	}

	if (opts & OPT_DEBUG)
		PRINTF("opts %#x name %s\n", opts, device);

	if ((opts & OPT_AUTHSTATS) != 0) {
		bzero((caddr_t)&ipfo, sizeof(ipfo));
		ipfo.ipfo_rev = IPFILTER_VERSION;
		ipfo.ipfo_type = IPFOBJ_AUTHSTAT;
		ipfo.ipfo_size = sizeof(fr_authstat_t);
		ipfo.ipfo_ptr = (void *)*frauthstpp;

	    	if (ioctl(auth_fd, SIOCATHST, &ipfo) == -1) {
			perror("ioctl(SIOCATHST)");
			exit(-1);
		}
	}
}


/*
 * Build up the stats structures from data held in the "core" memory.
 * This is mainly useful when looking at data in crash dumps and ioctl's
 * just won't work any more.
 */
static void ipfstate_dead(kernel, fiopp, ipsstpp, ifrstpp, frauthstpp, frfp)
char *kernel;
friostat_t **fiopp;
ips_stat_t **ipsstpp;
ipfrstat_t **ifrstpp;
fr_authstat_t **frauthstpp;
u_32_t *frfp;
{
	static fr_authstat_t frauthst, *frauthstp;
	static ips_stat_t ipsst, *ipsstp;
	static ipfrstat_t ifrst, *ifrstp;
	static friostat_t fio, *fiop;
	static ipftq_t ipssttab[IPF_TCP_NSTATES];
	int temp;

	void *rules[2][2];
	struct nlist deadlist[44] = {
		{ "fr_authstats" },		/* 0 */
		{ "fae_list" },
		{ "ipauth" },
		{ "fr_authlist" },
		{ "fr_authstart" },
		{ "fr_authend" },		/* 5 */
		{ "fr_authnext" },
		{ "fr_auth" },
		{ "fr_authused" },
		{ "fr_authsize" },
		{ "fr_defaultauthage" },	/* 10 */
		{ "fr_authpkts" },
		{ "fr_auth_lock" },
		{ "frstats" },
		{ "ips_stats" },
		{ "ips_num" },			/* 15 */
		{ "ips_wild" },
		{ "ips_list" },
		{ "ips_table" },
		{ "fr_statemax" },
		{ "fr_statesize" },		/* 20 */
		{ "fr_state_doflush" },
		{ "fr_state_lock" },
		{ "ipfr_heads" },
		{ "ipfr_nattab" },
		{ "ipfr_stats" },		/* 25 */
		{ "ipfr_inuse" },
		{ "fr_ipfrttl" },
		{ "fr_frag_lock" },
		{ "ipfr_timer_id" },
		{ "fr_nat_lock" },		/* 30 */
		{ "ipfilter" },
		{ "ipfilter6" },
		{ "ipacct" },
		{ "ipacct6" },
		{ "ipl_frouteok" },		/* 35 */
		{ "fr_running" },
		{ "ipfgroups" },
		{ "fr_active" },
		{ "fr_pass" },
		{ "fr_flags" },			/* 40 */
		{ "ipstate_logging" },
		{ "ips_tqtqb" },
		{ NULL }
	};


	frauthstp = &frauthst;
	ipsstp = &ipsst;
	ifrstp = &ifrst;
	fiop = &fio;

	*frfp = 0;
	*fiopp = fiop;
	*ipsstpp = ipsstp;
	*ifrstpp = ifrstp;
	*frauthstpp = frauthstp;

	bzero((char *)fiop, sizeof(*fiop));
	bzero((char *)ipsstp, sizeof(*ipsstp));
	bzero((char *)ifrstp, sizeof(*ifrstp));
	bzero((char *)frauthstp, sizeof(*frauthstp));

	if (nlist(kernel, deadlist) == -1) {
		fprintf(stderr, "nlist error\n");
		return;
	}

	/*
	 * This is for SIOCGETFF.
	 */
	kmemcpy((char *)frfp, (u_long)deadlist[40].n_value, sizeof(*frfp));

	/*
	 * f_locks is a combination of the lock variable from each part of
	 * ipfilter (state, auth, nat, fragments).
	 */
	kmemcpy((char *)fiop, (u_long)deadlist[13].n_value, sizeof(*fiop));
	kmemcpy((char *)&fiop->f_locks[0], (u_long)deadlist[22].n_value,
		sizeof(fiop->f_locks[0]));
	kmemcpy((char *)&fiop->f_locks[0], (u_long)deadlist[30].n_value,
		sizeof(fiop->f_locks[1]));
	kmemcpy((char *)&fiop->f_locks[2], (u_long)deadlist[28].n_value,
		sizeof(fiop->f_locks[2]));
	kmemcpy((char *)&fiop->f_locks[3], (u_long)deadlist[12].n_value,
		sizeof(fiop->f_locks[3]));

	/*
	 * Get pointers to each list of rules (active, inactive, in, out)
	 */
	kmemcpy((char *)&rules, (u_long)deadlist[31].n_value, sizeof(rules));
	fiop->f_fin[0] = rules[0][0];
	fiop->f_fin[1] = rules[0][1];
	fiop->f_fout[0] = rules[1][0];
	fiop->f_fout[1] = rules[1][1];

	/*
	 * Same for IPv6, except make them null if support for it is not
	 * being compiled in.
	 */
#ifdef	USE_INET6
	kmemcpy((char *)&rules, (u_long)deadlist[32].n_value, sizeof(rules));
	fiop->f_fin6[0] = rules[0][0];
	fiop->f_fin6[1] = rules[0][1];
	fiop->f_fout6[0] = rules[1][0];
	fiop->f_fout6[1] = rules[1][1];
#else
	fiop->f_fin6[0] = NULL;
	fiop->f_fin6[1] = NULL;
	fiop->f_fout6[0] = NULL;
	fiop->f_fout6[1] = NULL;
#endif

	/*
	 * Now get accounting rules pointers.
	 */
	kmemcpy((char *)&rules, (u_long)deadlist[33].n_value, sizeof(rules));
	fiop->f_acctin[0] = rules[0][0];
	fiop->f_acctin[1] = rules[0][1];
	fiop->f_acctout[0] = rules[1][0];
	fiop->f_acctout[1] = rules[1][1];

#ifdef	USE_INET6
	kmemcpy((char *)&rules, (u_long)deadlist[34].n_value, sizeof(rules));
	fiop->f_acctin6[0] = rules[0][0];
	fiop->f_acctin6[1] = rules[0][1];
	fiop->f_acctout6[0] = rules[1][0];
	fiop->f_acctout6[1] = rules[1][1];
#else
	fiop->f_acctin6[0] = NULL;
	fiop->f_acctin6[1] = NULL;
	fiop->f_acctout6[0] = NULL;
	fiop->f_acctout6[1] = NULL;
#endif

	/*
	 * A collection of "global" variables used inside the kernel which
	 * are all collected in friostat_t via ioctl.
	 */
	kmemcpy((char *)&fiop->f_froute, (u_long)deadlist[35].n_value,
		sizeof(fiop->f_froute));
	kmemcpy((char *)&fiop->f_running, (u_long)deadlist[36].n_value,
		sizeof(fiop->f_running));
	kmemcpy((char *)&fiop->f_groups, (u_long)deadlist[37].n_value,
		sizeof(fiop->f_groups));
	kmemcpy((char *)&fiop->f_active, (u_long)deadlist[38].n_value,
		sizeof(fiop->f_active));
	kmemcpy((char *)&fiop->f_defpass, (u_long)deadlist[39].n_value,
		sizeof(fiop->f_defpass));

	/*
	 * Build up the state information stats structure.
	 */
	kmemcpy((char *)ipsstp, (u_long)deadlist[14].n_value, sizeof(*ipsstp));
	kmemcpy((char *)&temp, (u_long)deadlist[15].n_value, sizeof(temp));
	kmemcpy((char *)ipssttab, (u_long)deadlist[42].n_value,
		sizeof(ipssttab));
	ipsstp->iss_active = temp;
	ipsstp->iss_table = (void *)deadlist[18].n_value;
	ipsstp->iss_list = (void *)deadlist[17].n_value;
	ipsstp->iss_tcptab = ipssttab;

	/*
	 * Build up the authentiation information stats structure.
	 */
	kmemcpy((char *)frauthstp, (u_long)deadlist[0].n_value,
		sizeof(*frauthstp));
	frauthstp->fas_faelist = (void *)deadlist[1].n_value;

	/*
	 * Build up the fragment information stats structure.
	 */
	kmemcpy((char *)ifrstp, (u_long)deadlist[25].n_value,
		sizeof(*ifrstp));
	ifrstp->ifs_table = (void *)deadlist[23].n_value;
	ifrstp->ifs_nattab = (void *)deadlist[24].n_value;
	kmemcpy((char *)&ifrstp->ifs_inuse, (u_long)deadlist[26].n_value,
		sizeof(ifrstp->ifs_inuse));

	/*
	 * Get logging on/off switches
	 */
	kmemcpy((char *)&state_logging, (u_long)deadlist[41].n_value,
		sizeof(state_logging));
}


/*
 * Display the kernel stats for packets blocked and passed and other
 * associated running totals which are kept.
 */
static	void	showstats(fp, frf)
struct	friostat	*fp;
u_32_t frf;
{

	PRINTF("bad packets:\t\tin %lu\tout %lu\n",
			fp->f_st[0].fr_bad, fp->f_st[1].fr_bad);
#ifdef	USE_INET6
	PRINTF(" IPv6 packets:\t\tin %lu out %lu\n",
			fp->f_st[0].fr_ipv6, fp->f_st[1].fr_ipv6);
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
	PRINTF("fragment state(in):\tkept %lu\tlost %lu\tnot fragmented %lu\n",
			fp->f_st[0].fr_nfr, fp->f_st[0].fr_bnfr,
			fp->f_st[0].fr_cfr);
	PRINTF("fragment state(out):\tkept %lu\tlost %lu\tnot fragmented %lu\n",
			fp->f_st[1].fr_nfr, fp->f_st[1].fr_bnfr,
			fp->f_st[0].fr_cfr);
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
	PRINTF("IPF Ticks:\t%lu\n", fp->f_ticks);

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


/*
 * Print out a list of rules from the kernel, starting at the one passed.
 */
static void printlivelist(out, set, fp, group, comment)
int out, set;
frentry_t *fp;
char *group, *comment;
{
	struct	frentry	fb;
	ipfruleiter_t rule;
	frentry_t zero;
	frgroup_t *g;
	ipfobj_t obj;
	int n;

	if (use_inet6 == 1)
		fb.fr_v = 6;
	else
		fb.fr_v = 4;
	fb.fr_next = fp;
	n = 0;

	rule.iri_inout = out;
	rule.iri_active = set;
	rule.iri_rule = &fb;
	rule.iri_nrules = 1;
	rule.iri_v = use_inet6 ? 6 : 4;
	if (group != NULL)
		strncpy(rule.iri_group, group, FR_GROUPLEN);
	else
		rule.iri_group[0] = '\0';

	bzero((char *)&zero, sizeof(zero));

	bzero((char *)&obj, sizeof(obj));
	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_IPFITER;
	obj.ipfo_size = sizeof(rule);
	obj.ipfo_ptr = &rule;

	do {
		u_long array[1000];

		memset(array, 0xff, sizeof(array));
		fp = (frentry_t *)array;
		rule.iri_rule = fp;
		if (ioctl(ipf_fd, SIOCIPFITER, &obj) == -1) {
			perror("ioctl(SIOCIPFITER)");
			n = IPFGENITER_IPF;
			ioctl(ipf_fd, SIOCIPFDELTOK, &n);
			return;
		}
		if (bcmp(fp, &zero, sizeof(zero)) == 0)
			break;
		if (fp->fr_data != NULL)
			fp->fr_data = (char *)fp + sizeof(*fp);

		n++;

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

		printfr(fp, ioctl);
		if (opts & OPT_DEBUG) {
			binprint(fp, sizeof(*fp));
			if (fp->fr_data != NULL && fp->fr_dsize > 0)
				binprint(fp->fr_data, fp->fr_dsize);
		}
		if (fp->fr_grhead[0] != '\0') {
			for (g = grtop; g != NULL; g = g->fg_next) {
				if (!strncmp(fp->fr_grhead, g->fg_name,
					     FR_GROUPLEN))
					break;
			}
			if (g == NULL) {
				g = calloc(1, sizeof(*g));

				if (g != NULL) {
					strncpy(g->fg_name, fp->fr_grhead,
						FR_GROUPLEN);
					if (grtop == NULL) {
						grtop = g;
						grtail = g;
					} else {
						grtail->fg_next = g;
						grtail = g;
					}
				}
			}
		}
		if (fp->fr_type == FR_T_CALLFUNC) {
			printlivelist(out, set, fp->fr_data, group,
				      "# callfunc: ");
		}
	} while (fp->fr_next != NULL);

	n = IPFGENITER_IPF;
	ioctl(ipf_fd, SIOCIPFDELTOK, &n);

	if (group == NULL) {
		while ((g = grtop) != NULL) {
			printf("# Group %s\n", g->fg_name);
			printlivelist(out, set, NULL, g->fg_name, comment);
			grtop = g->fg_next;
			free(g);
		}
	}
}


static void printdeadlist(out, set, fp, group, comment)
int out, set;
frentry_t *fp;
char *group, *comment;
{
	frgroup_t *grtop, *grtail, *g;
	struct	frentry	fb;
	char	*data;
	u_32_t	type;
	int	n;

	fb.fr_next = fp;
	n = 0;
	grtop = NULL;
	grtail = NULL;

	do {
		fp = fb.fr_next;
		if (kmemcpy((char *)&fb, (u_long)fb.fr_next,
			    sizeof(fb)) == -1) {
			perror("kmemcpy");
			return;
		}

		data = NULL;
		type = fb.fr_type & ~FR_T_BUILTIN;
		if (type == FR_T_IPF || type == FR_T_BPFOPC) {
			if (fb.fr_dsize) {
				data = malloc(fb.fr_dsize);

				if (kmemcpy(data, (u_long)fb.fr_data,
					    fb.fr_dsize) == -1) {
					perror("kmemcpy");
					return;
				}
				fb.fr_data = data;
			}
		}

		n++;

		if (opts & (OPT_HITS|OPT_VERBOSE))
#ifdef	USE_QUAD_T
			PRINTF("%qu ", (unsigned long long) fb.fr_hits);
#else
			PRINTF("%lu ", fb.fr_hits);
#endif
		if (opts & (OPT_ACCNT|OPT_VERBOSE))
#ifdef	USE_QUAD_T
			PRINTF("%qu ", (unsigned long long) fb.fr_bytes);
#else
			PRINTF("%lu ", fb.fr_bytes);
#endif
		if (opts & OPT_SHOWLINENO)
			PRINTF("@%d ", n);

		printfr(fp, ioctl);
		if (opts & OPT_DEBUG) {
			binprint(fp, sizeof(*fp));
			if (fb.fr_data != NULL && fb.fr_dsize > 0)
				binprint(fb.fr_data, fb.fr_dsize);
		}
		if (data != NULL)
			free(data);
		if (fb.fr_grhead[0] != '\0') {
			g = calloc(1, sizeof(*g));

			if (g != NULL) {
				strncpy(g->fg_name, fb.fr_grhead,
					FR_GROUPLEN);
				if (grtop == NULL) {
					grtop = g;
					grtail = g;
				} else {
					grtail->fg_next = g;
					grtail = g;
				}
			}
		}
		if (type == FR_T_CALLFUNC) {
			printdeadlist(out, set, fb.fr_data, group,
				      "# callfunc: ");
		}
	} while (fb.fr_next != NULL);

	while ((g = grtop) != NULL) {
		printdeadlist(out, set, NULL, g->fg_name, comment);
		grtop = g->fg_next;
		free(g);
	}
}

/*
 * print out all of the asked for rule sets, using the stats struct as
 * the base from which to get the pointers.
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
#ifdef USE_INET6
		if ((use_inet6) && (opts & OPT_OUTQUE)) {
			i = F_ACOUT;
			fp = (struct frentry *)fiop->f_acctout6[set];
		} else if ((use_inet6) && (opts & OPT_INQUE)) {
			i = F_ACIN;
			fp = (struct frentry *)fiop->f_acctin6[set];
		} else
#endif
		if (opts & OPT_OUTQUE) {
			i = F_ACOUT;
			fp = (struct frentry *)fiop->f_acctout[set];
		} else if (opts & OPT_INQUE) {
			i = F_ACIN;
			fp = (struct frentry *)fiop->f_acctin[set];
		} else {
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
	if (opts & OPT_DEBUG)
		FPRINTF(stderr, "showlist:opts %#x i %d\n", opts, i);

	if (opts & OPT_DEBUG)
		PRINTF("fp %p set %d\n", fp, set);
	if (!fp) {
		FPRINTF(stderr, "empty list for %s%s\n",
			(opts & OPT_INACTIVE) ? "inactive " : "", filters[i]);
		return;
	}
	if (live_kernel == 1)
		printlivelist(i, set, fp, NULL, NULL);
	else
		printdeadlist(i, set, fp, NULL, NULL);
}


/*
 * Display ipfilter stateful filtering information
 */
static void showipstates(ipsp)
ips_stat_t *ipsp;
{
	u_long minlen, maxlen, totallen, *buckets;
	ipftable_t table;
	ipfobj_t obj;
	int i, sz;

	/*
	 * If a list of states hasn't been asked for, only print out stats
	 */
	if (!(opts & OPT_SHOWLIST)) {

		sz = sizeof(*buckets) * ipsp->iss_statesize;
		buckets = (u_long *)malloc(sz);

		obj.ipfo_rev = IPFILTER_VERSION;
		obj.ipfo_type = IPFOBJ_GTABLE;
		obj.ipfo_size = sizeof(table);
		obj.ipfo_ptr = &table;

		table.ita_type = IPFTABLE_BUCKETS;
		table.ita_table = buckets;

		if (live_kernel == 1) {
			if (ioctl(state_fd, SIOCGTABL, &obj) != 0) {
				free(buckets);
				return;
			}
		} else {
			if (kmemcpy((char *)buckets,
				    (u_long)ipsp->iss_bucketlen, sz)) {
				free(buckets);
				return;
			}
		}

		PRINTF("IP states added:\n\t%lu TCP\n\t%lu UDP\n\t%lu ICMP\n",
			ipsp->iss_tcp, ipsp->iss_udp, ipsp->iss_icmp);
		PRINTF("\t%lu hits\n\t%lu misses\n", ipsp->iss_hits,
			ipsp->iss_miss);
		PRINTF("\t%lu bucket full\n", ipsp->iss_bucketfull);
		PRINTF("\t%lu maximum rule references\n", ipsp->iss_maxref);
		PRINTF("\t%lu maximum\n\t%lu no memory\n\t%lu bkts in use\n",
			ipsp->iss_max, ipsp->iss_nomem, ipsp->iss_inuse);
		PRINTF("\t%lu active\n\t%lu expired\n\t%lu closed\n",
			ipsp->iss_active, ipsp->iss_expire, ipsp->iss_fin);

		PRINTF("State logging %sabled\n",
			state_logging ? "en" : "dis");

		PRINTF("\nState table bucket statistics:\n");
		PRINTF("\t%lu in use\t\n", ipsp->iss_inuse);
		PRINTF("\t%u%% hash efficiency\n", ipsp->iss_active ?
			(u_int)(ipsp->iss_inuse * 100 / ipsp->iss_active) : 0);

		minlen = ipsp->iss_max;
		totallen = 0;
		maxlen = 0;

		for (i = 0; i < ipsp->iss_statesize; i++) {
			if (buckets[i] > maxlen)
				maxlen = buckets[i];
			if (buckets[i] < minlen)
					minlen = buckets[i];
			totallen += buckets[i];
		}

		PRINTF("\t%2.2f%% bucket usage\n\t%lu minimal length\n",
			((float)ipsp->iss_inuse / ipsp->iss_statesize) * 100.0,
			minlen);
		PRINTF("\t%lu maximal length\n\t%.3f average length\n",
			maxlen,
			ipsp->iss_inuse ? (float) totallen/ ipsp->iss_inuse :
					  0.0);

#define ENTRIES_PER_LINE 5

		if (opts & OPT_VERBOSE) {
			PRINTF("\nCurrent bucket sizes :\n");
			for (i = 0; i < ipsp->iss_statesize; i++) {
				if ((i % ENTRIES_PER_LINE) == 0)
					PRINTF("\t");
				PRINTF("%4d -> %4lu", i, buckets[i]);
				if ((i % ENTRIES_PER_LINE) ==
				    (ENTRIES_PER_LINE - 1))
					PRINTF("\n");
				else
					PRINTF("  ");
			}
			PRINTF("\n");
		}
		PRINTF("\n");

		free(buckets);

		if (live_kernel == 1) {
			showtqtable_live(state_fd);
		} else {
			printtqtable(ipsp->iss_tcptab);
		}

		return;

	}

	/*
	 * Print out all the state information currently held in the kernel.
	 */
	while (ipsp->iss_list != NULL) {
		ipstate_t ips;

		ipsp->iss_list = fetchstate(ipsp->iss_list, &ips);

		if (ipsp->iss_list != NULL) {
			ipsp->iss_list = ips.is_next;
			printstate(&ips, opts, ipsp->iss_ticks);
		}
	}
}


#ifdef STATETOP
static int handle_resize = 0, handle_break = 0;

static void topipstates(saddr, daddr, sport, dport, protocol, ver,
		        refreshtime, topclosed)
i6addr_t saddr;
i6addr_t daddr;
int sport;
int dport;
int protocol;
int ver;
int refreshtime;
int topclosed;
{
	char str1[STSTRSIZE], str2[STSTRSIZE], str3[STSTRSIZE], str4[STSTRSIZE];
	int maxtsentries = 0, reverse = 0, sorting = STSORT_DEFAULT;
	int i, j, winy, tsentry, maxx, maxy, redraw = 0, ret = 0;
	int len, srclen, dstlen, forward = 1, c = 0;
	ips_stat_t ipsst, *ipsstp = &ipsst;
	statetop_t *tstable = NULL, *tp;
	const char *errstr = "";
	ipstate_t ips;
	ipfobj_t ipfo;
	struct timeval selecttimeout;
	char hostnm[HOSTNMLEN];
	struct protoent *proto;
	fd_set readfd;
	time_t t;

	/* install signal handlers */
	signal(SIGINT, sig_break);
	signal(SIGQUIT, sig_break);
	signal(SIGTERM, sig_break);
	signal(SIGWINCH, sig_resize);

	/* init ncurses stuff */
  	initscr();
  	cbreak();
  	noecho();
	curs_set(0);
	timeout(0);
	getmaxyx(stdscr, maxy, maxx);

	/* init hostname */
	gethostname(hostnm, sizeof(hostnm) - 1);
	hostnm[sizeof(hostnm) - 1] = '\0';

	/* init ipfobj_t stuff */
	bzero((caddr_t)&ipfo, sizeof(ipfo));
	ipfo.ipfo_rev = IPFILTER_VERSION;
	ipfo.ipfo_type = IPFOBJ_STATESTAT;
	ipfo.ipfo_size = sizeof(*ipsstp);
	ipfo.ipfo_ptr = (void *)ipsstp;

	/* repeat until user aborts */
	while ( 1 ) {

		/* get state table */
		bzero((char *)&ipsst, sizeof(ipsst));
		if ((ioctl(state_fd, SIOCGETFS, &ipfo) == -1)) {
			errstr = "ioctl(SIOCGETFS)";
			ret = -1;
			goto out;
		}

		/* clear the history */
		tsentry = -1;

		/* reset max str len */
		srclen = dstlen = 0;

		/* read the state table and store in tstable */
		for (; ipsstp->iss_list; ipsstp->iss_list = ips.is_next) {

			ipsstp->iss_list = fetchstate(ipsstp->iss_list, &ips);
			if (ipsstp->iss_list == NULL)
				break;

			if (ips.is_v != ver)
				continue;

			/* check v4 src/dest addresses */
			if (ips.is_v == 4) {
				if ((saddr.in4.s_addr != INADDR_ANY &&
				     saddr.in4.s_addr != ips.is_saddr) ||
				    (daddr.in4.s_addr != INADDR_ANY &&
				     daddr.in4.s_addr != ips.is_daddr))
					continue;
			}
#ifdef	USE_INET6
			/* check v6 src/dest addresses */
			if (ips.is_v == 6) {
				if ((IP6_NEQ(&saddr, &in6addr_any) &&
				     IP6_NEQ(&saddr, &ips.is_src)) ||
				    (IP6_NEQ(&daddr, &in6addr_any) &&
				     IP6_NEQ(&daddr, &ips.is_dst)))
					continue;
			}
#endif
			/* check protocol */
			if (protocol > 0 && protocol != ips.is_p)
				continue;

			/* check ports if protocol is TCP or UDP */
			if (((ips.is_p == IPPROTO_TCP) ||
			     (ips.is_p == IPPROTO_UDP)) &&
			   (((sport > 0) && (htons(sport) != ips.is_sport)) ||
			    ((dport > 0) && (htons(dport) != ips.is_dport))))
				continue;

			/* show closed TCP sessions ? */
			if ((topclosed == 0) && (ips.is_p == IPPROTO_TCP) &&
			    (ips.is_state[0] >= IPF_TCPS_LAST_ACK) &&
			    (ips.is_state[1] >= IPF_TCPS_LAST_ACK))
				continue;

			/*
			 * if necessary make room for this state
			 * entry
			 */
			tsentry++;
			if (!maxtsentries || tsentry == maxtsentries) {
				maxtsentries += STGROWSIZE;
				tstable = realloc(tstable,
				    maxtsentries * sizeof(statetop_t));
				if (tstable == NULL) {
					perror("realloc");
					exit(-1);
				}
			}

			/* get max src/dest address string length */
			len = strlen(getip(ips.is_v, &ips.is_src));
			if (srclen < len)
				srclen = len;
			len = strlen(getip(ips.is_v, &ips.is_dst));
			if (dstlen < len)
				dstlen = len;

			/* fill structure */
			tp = tstable + tsentry;
			tp->st_src = ips.is_src;
			tp->st_dst = ips.is_dst;
			tp->st_p = ips.is_p;
			tp->st_v = ips.is_v;
			tp->st_state[0] = ips.is_state[0];
			tp->st_state[1] = ips.is_state[1];
			if (forward) {
				tp->st_pkts = ips.is_pkts[0]+ips.is_pkts[1];
				tp->st_bytes = ips.is_bytes[0]+ips.is_bytes[1];
			} else {
				tp->st_pkts = ips.is_pkts[2]+ips.is_pkts[3];
				tp->st_bytes = ips.is_bytes[2]+ips.is_bytes[3];
			}
			tp->st_age = ips.is_die - ipsstp->iss_ticks;
			if ((ips.is_p == IPPROTO_TCP) ||
			    (ips.is_p == IPPROTO_UDP)) {
				tp->st_sport = ips.is_sport;
				tp->st_dport = ips.is_dport;
			}
		}


		/* sort the array */
		if (tsentry != -1) {
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
			case STSORT_SRCIP:
				qsort(tstable, tsentry + 1,
				      sizeof(statetop_t), sort_srcip);
				break;
			case STSORT_SRCPT:
				qsort(tstable, tsentry +1,
					sizeof(statetop_t), sort_srcpt);
				break;
			case STSORT_DSTIP:
				qsort(tstable, tsentry + 1,
				      sizeof(statetop_t), sort_dstip);
				break;
			case STSORT_DSTPT:
				qsort(tstable, tsentry + 1,
				      sizeof(statetop_t), sort_dstpt);
				break;
			default:
				break;
			}
		}

		/* handle window resizes */
		if (handle_resize) {
			endwin();
			initscr();
			cbreak();
			noecho();
			curs_set(0);
			timeout(0);
			getmaxyx(stdscr, maxy, maxx);
			redraw = 1;
			handle_resize = 0;
                }

		/* stop program? */
		if (handle_break)
			break;

		/* print title */
		erase();
		attron(A_BOLD);
		winy = 0;
		move(winy,0);
		sprintf(str1, "%s - %s - state top", hostnm, IPL_VERSION);
		for (j = 0 ; j < (maxx - 8 - strlen(str1)) / 2; j++)
			printw(" ");
		printw("%s", str1);
		attroff(A_BOLD);

		/* just for fun add a clock */
		move(winy, maxx - 8);
		t = time(NULL);
		strftime(str1, 80, "%T", localtime(&t));
		printw("%s\n", str1);

		/*
		 * print the display filters, this is placed in the loop,
		 * because someday I might add code for changing these
		 * while the programming is running :-)
		 */
		if (sport >= 0)
			sprintf(str1, "%s,%d", getip(ver, &saddr), sport);
		else
			sprintf(str1, "%s", getip(ver, &saddr));

		if (dport >= 0)
			sprintf(str2, "%s,%d", getip(ver, &daddr), dport);
		else
			sprintf(str2, "%s", getip(ver, &daddr));

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
		case STSORT_SRCIP:
			sprintf(str4, "src ip");
			break;
		case STSORT_SRCPT:
			sprintf(str4, "src port");
			break;
		case STSORT_DSTIP:
			sprintf(str4, "dest ip");
			break;
		case STSORT_DSTPT:
			sprintf(str4, "dest port");
			break;
		default:
			sprintf(str4, "unknown");
			break;
		}

		if (reverse)
			strcat(str4, " (reverse)");

		winy += 2;
		move(winy,0);
		printw("Src: %s, Dest: %s, Proto: %s, Sorted by: %s\n\n",
		       str1, str2, str3, str4);

		/* 
		 * For an IPv4 IP address we need at most 15 characters,
		 * 4 tuples of 3 digits, separated by 3 dots. Enforce this
		 * length, so the colums do not change positions based
		 * on the size of the IP address. This length makes the
		 * output fit in a 80 column terminal. 
		 * We are lacking a good solution for IPv6 addresses (that
		 * can be longer that 15 characters), so we do not enforce 
		 * a maximum on the IP field size.
		 */
		if (srclen < 15)
			srclen = 15;
		if (dstlen < 15)
			dstlen = 15;

		/* print column description */
		winy += 2;
		move(winy,0);
		attron(A_BOLD);
		printw("%-*s %-*s %3s %4s %7s %9s %9s\n",
		       srclen + 6, "Source IP", dstlen + 6, "Destination IP",
		       "ST", "PR", "#pkts", "#bytes", "ttl");
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
					getip(tp->st_v, &tp->st_src),
					ntohs(tp->st_sport));
				sprintf(str2, "%s,%hu",
					getip(tp->st_v, &tp->st_dst),
					ntohs(tp->st_dport));
			} else {
				sprintf(str1, "%s", getip(tp->st_v,
				    &tp->st_src));
				sprintf(str2, "%s", getip(tp->st_v,
				    &tp->st_dst));
			}
			winy++;
			move(winy, 0);
			printw("%-*s %-*s", srclen + 6, str1, dstlen + 6, str2);

			/* print state */
			sprintf(str1, "%X/%X", tp->st_state[0],
				tp->st_state[1]);
			printw(" %3s", str1);

			/* print protocol */
			proto = getprotobynumber(tp->st_p);
			if (proto) {
				strncpy(str1, proto->p_name, 4);
				str1[4] = '\0';
			} else {
				sprintf(str1, "%d", tp->st_p);
			}
			/* just print icmp for IPv6-ICMP */
			if (tp->st_p == IPPROTO_ICMPV6)
				strcpy(str1, "icmp");
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

		if (refresh() == ERR)
			break;
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

			if (ISALPHA(c) && ISUPPER(c))
				c = TOLOWER(c);
			if (c == 'l') {
				redraw = 1;
			} else if (c == 'q') {
				break;
			} else if (c == 'r') {
				reverse = !reverse;
			} else if (c == 'b') {
				forward = 0;
			} else if (c == 'f') {
				forward = 1;
			} else if (c == 's') {
				if (++sorting > STSORT_MAX)
					sorting = 0;
			}
		}
	} /* while */

out:
	printw("\n");
	curs_set(1);
	/* nocbreak(); XXX - endwin() should make this redundant */
	endwin();

	free(tstable);
	if (ret != 0)
		perror(errstr);
}
#endif


/*
 * Show fragment cache information that's held in the kernel.
 */
static void showfrstates(ifsp, ticks)
ipfrstat_t *ifsp;
u_long ticks;
{
	struct ipfr *ipfrtab[IPFT_SIZE], ifr;
	int i;

	/*
	 * print out the numeric statistics
	 */
	PRINTF("IP fragment states:\n\t%lu new\n\t%lu expired\n\t%lu hits\n",
		ifsp->ifs_new, ifsp->ifs_expire, ifsp->ifs_hits);
	PRINTF("\t%lu retrans\n\t%lu too short\n",
		ifsp->ifs_retrans0, ifsp->ifs_short);
	PRINTF("\t%lu no memory\n\t%lu already exist\n",
		ifsp->ifs_nomem, ifsp->ifs_exists);
	PRINTF("\t%lu inuse\n", ifsp->ifs_inuse);
	PRINTF("\n");

	if (live_kernel == 0) {
		if (kmemcpy((char *)ipfrtab, (u_long)ifsp->ifs_table,
			    sizeof(ipfrtab)))
			return;
	}

	/*
	 * Print out the contents (if any) of the fragment cache table.
	 */
	if (live_kernel == 1) {
		do {
			if (fetchfrag(ipf_fd, IPFGENITER_FRAG, &ifr) != 0)
				break;
			if (ifr.ipfr_ifp == NULL)
				break;
			ifr.ipfr_ttl -= ticks;
			printfraginfo("", &ifr);
		} while (1);
	} else {
		for (i = 0; i < IPFT_SIZE; i++)
			while (ipfrtab[i] != NULL) {
				if (kmemcpy((char *)&ifr, (u_long)ipfrtab[i],
					    sizeof(ifr)) == -1)
					break;
				printfraginfo("", &ifr);
				ipfrtab[i] = ifr.ipfr_next;
			}
	}
	/*
	 * Print out the contents (if any) of the NAT fragment cache table.
	 */

	if (live_kernel == 0) {
		if (kmemcpy((char *)ipfrtab, (u_long)ifsp->ifs_nattab,
			    sizeof(ipfrtab)))
			return;
	}

	if (live_kernel == 1) {
		do {
			if (fetchfrag(nat_fd, IPFGENITER_NATFRAG, &ifr) != 0)
				break;
			if (ifr.ipfr_ifp == NULL)
				break;
			ifr.ipfr_ttl -= ticks;
			printfraginfo("NAT: ", &ifr);
		} while (1);
	} else {
		for (i = 0; i < IPFT_SIZE; i++)
			while (ipfrtab[i] != NULL) {
				if (kmemcpy((char *)&ifr, (u_long)ipfrtab[i],
					    sizeof(ifr)) == -1)
					break;
				printfraginfo("NAT: ", &ifr);
				ipfrtab[i] = ifr.ipfr_next;
			}
	}
}


/*
 * Show stats on how auth within IPFilter has been used
 */
static void showauthstates(asp)
fr_authstat_t *asp;
{
	frauthent_t *frap, fra;
	ipfgeniter_t auth;
	ipfobj_t obj;

	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_GENITER;
	obj.ipfo_size = sizeof(auth);
	obj.ipfo_ptr = &auth;

	auth.igi_type = IPFGENITER_AUTH;
	auth.igi_nitems = 1;
	auth.igi_data = &fra;

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
		if (live_kernel == 1) {
			if (ioctl(auth_fd, SIOCGENITER, &obj))
				break;
		} else {
			if (kmemcpy((char *)&fra, (u_long)frap,
				    sizeof(fra)) == -1)
				break;
		}
		printf("age %ld\t", fra.fae_age);
		printfr(&fra.fae_fr, ioctl);
		frap = fra.fae_next;
	}
}


/*
 * Display groups used for each of filter rules, accounting rules and
 * authentication, separately.
 */
static void showgroups(fiop)
struct friostat	*fiop;
{
	static char *gnames[3] = { "Filter", "Accounting", "Authentication" };
	static int gnums[3] = { IPL_LOGIPF, IPL_LOGCOUNT, IPL_LOGAUTH };
	frgroup_t *fp, grp;
	int on, off, i;

	on = fiop->f_active;
	off = 1 - on;

	for (i = 0; i < 3; i++) {
		printf("%s groups (active):\n", gnames[i]);
		for (fp = fiop->f_groups[gnums[i]][on]; fp != NULL;
		     fp = grp.fg_next)
			if (kmemcpy((char *)&grp, (u_long)fp, sizeof(grp)))
				break;
			else
				printf("%s\n", grp.fg_name);
		printf("%s groups (inactive):\n", gnames[i]);
		for (fp = fiop->f_groups[gnums[i]][off]; fp != NULL;
		     fp = grp.fg_next)
			if (kmemcpy((char *)&grp, (u_long)fp, sizeof(grp)))
				break;
			else
				printf("%s\n", grp.fg_name);
	}
}

static void parse_ipportstr(argument, ip, port)
const char *argument;
i6addr_t *ip;
int *port;
{
	char *s, *comma;
	int ok = 0;

	/* make working copy of argument, Theoretically you must be able
	 * to write to optarg, but that seems very ugly to me....
	 */
	s = strdup(argument);
	if (s == NULL)
		return;

	/* get port */
	if ((comma = strchr(s, ',')) != NULL) {
		if (!strcasecmp(comma + 1, "any")) {
			*port = -1;
		} else if (!sscanf(comma + 1, "%d", port) ||
			   (*port < 0) || (*port > 65535)) {
			fprintf(stderr, "Invalid port specification in %s\n",
				argument);
			free(s);
			exit(-2);
		}
		*comma = '\0';
	}


	/* get ip address */
	if (!strcasecmp(s, "any")) {
		ip->in4.s_addr = INADDR_ANY;
		ok = 1;
#ifdef	USE_INET6
		ip->in6 = in6addr_any;
	} else if (use_inet6 && inet_pton(AF_INET6, s, &ip->in6)) {
		ok = 1;
#endif
	} else if (inet_aton(s, &ip->in4))
		ok = 1;

	if (ok == 0) {
		fprintf(stderr, "Invalid IP address: %s\n", s);
		free(s);
		exit(-2);
	}

	/* free allocated memory */
	free(s);
}


#ifdef STATETOP
static void sig_resize(s)
int s;
{
	handle_resize = 1;
}

static void sig_break(s)
int s;
{
	handle_break = 1;
}

static char *getip(v, addr)
int v;
i6addr_t *addr;
{
#ifdef  USE_INET6
	static char hostbuf[MAXHOSTNAMELEN+1];
#endif

	if (v == 4)
		return inet_ntoa(addr->in4);

#ifdef  USE_INET6
	(void) inet_ntop(AF_INET6, &addr->in6, hostbuf, sizeof(hostbuf) - 1);
	hostbuf[MAXHOSTNAMELEN] = '\0';
	return hostbuf;
#else
	return "IPv6";
#endif
}


static char *ttl_to_string(ttl)
long int ttl;
{
	static char ttlbuf[STSTRSIZE];
	int hours, minutes, seconds;

	/* ttl is in half seconds */
	ttl /= 2;

	hours = ttl / 3600;
	ttl = ttl % 3600;
	minutes = ttl / 60;
	seconds = ttl % 60;

	if (hours > 0)
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

static int sort_srcip(a, b)
const void *a;
const void *b;
{
	register const statetop_t *ap = a;
	register const statetop_t *bp = b;

#ifdef USE_INET6
	if (use_inet6) {
		if (IP6_EQ(&ap->st_src, &bp->st_src))
			return 0;
		else if (IP6_GT(&ap->st_src, &bp->st_src))
			return 1;
	} else
#endif
	{
		if (ntohl(ap->st_src.in4.s_addr) ==
		    ntohl(bp->st_src.in4.s_addr))
			return 0;
		else if (ntohl(ap->st_src.in4.s_addr) >
		         ntohl(bp->st_src.in4.s_addr))
			return 1;
	}
	return -1;
}

static int sort_srcpt(a, b)
const void *a;
const void *b;
{
	register const statetop_t *ap = a;
	register const statetop_t *bp = b;

	if (htons(ap->st_sport) == htons(bp->st_sport))
		return 0;
	else if (htons(ap->st_sport) > htons(bp->st_sport))
		return 1;
	return -1;
}

static int sort_dstip(a, b)
const void *a;
const void *b;
{
	register const statetop_t *ap = a;
	register const statetop_t *bp = b;

#ifdef USE_INET6
	if (use_inet6) {
		if (IP6_EQ(&ap->st_dst, &bp->st_dst))
			return 0;
		else if (IP6_GT(&ap->st_dst, &bp->st_dst))
			return 1;
	} else
#endif
	{
		if (ntohl(ap->st_dst.in4.s_addr) ==
		    ntohl(bp->st_dst.in4.s_addr))
			return 0;
		else if (ntohl(ap->st_dst.in4.s_addr) >
		         ntohl(bp->st_dst.in4.s_addr))
			return 1;
	}
	return -1;
}

static int sort_dstpt(a, b)
const void *a;
const void *b;
{
	register const statetop_t *ap = a;
	register const statetop_t *bp = b;

	if (htons(ap->st_dport) == htons(bp->st_dport))
		return 0;
	else if (htons(ap->st_dport) > htons(bp->st_dport))
		return 1;
	return -1;
}

#endif


ipstate_t *fetchstate(src, dst)
ipstate_t *src, *dst;
{
	int i;

	if (live_kernel == 1) {
		ipfgeniter_t state;
		ipfobj_t obj;

		obj.ipfo_rev = IPFILTER_VERSION;
		obj.ipfo_type = IPFOBJ_GENITER;
		obj.ipfo_size = sizeof(state);
		obj.ipfo_ptr = &state;

		state.igi_type = IPFGENITER_STATE;
		state.igi_nitems = 1;
		state.igi_data = dst;

		if (ioctl(state_fd, SIOCGENITER, &obj) != 0)
			return NULL;
		if (dst->is_next == NULL) {
			i = IPFGENITER_STATE;
			ioctl(state_fd, SIOCIPFDELTOK, &i);
		}
	} else {
		if (kmemcpy((char *)dst, (u_long)src, sizeof(*dst)))
			return NULL;
	}
	return dst;
}


static int fetchfrag(fd, type, frp)
int fd, type;
ipfr_t *frp;
{
	ipfgeniter_t frag;
	ipfobj_t obj;

	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_GENITER;
	obj.ipfo_size = sizeof(frag);
	obj.ipfo_ptr = &frag;

	frag.igi_type = type;
	frag.igi_nitems = 1;
	frag.igi_data = frp;

	if (ioctl(fd, SIOCGENITER, &obj))
		return EFAULT;
	return 0;
}


static void showtqtable_live(fd)
int fd;
{
	ipftq_t table[IPF_TCP_NSTATES];
	ipfobj_t obj;

	bzero((char *)&obj, sizeof(obj));
	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_size = sizeof(table);
	obj.ipfo_ptr = (void *)table;
	obj.ipfo_type = IPFOBJ_STATETQTAB;

	if (ioctl(fd, SIOCGTQTAB, &obj) == 0) {
		printtqtable(table);
	}
}
