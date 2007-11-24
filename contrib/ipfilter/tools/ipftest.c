/*	$FreeBSD$	*/

/*
 * Copyright (C) 2002-2006 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#include "ipf.h"
#include "ipt.h"
#include <sys/ioctl.h>
#include <sys/file.h>

#if !defined(lint)
static const char sccsid[] = "@(#)ipt.c	1.19 6/3/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipftest.c,v 1.44.2.13 2006/12/12 16:13:01 darrenr Exp $";
#endif

extern	char	*optarg;
extern	struct frentry	*ipfilter[2][2];
extern	struct ipread	snoop, etherf, tcpd, pcap, iptext, iphex;
extern	struct ifnet	*get_unit __P((char *, int));
extern	void	init_ifp __P((void));
extern	ipnat_t	*natparse __P((char *, int));
extern	int	fr_running;
extern	hostmap_t **ipf_hm_maptable;
extern	hostmap_t *ipf_hm_maplist;

ipfmutex_t	ipl_mutex, ipf_authmx, ipf_rw, ipf_stinsert;
ipfmutex_t	ipf_nat_new, ipf_natio, ipf_timeoutlock;
ipfrwlock_t	ipf_mutex, ipf_global, ipf_ipidfrag, ip_poolrw, ipf_frcache;
ipfrwlock_t	ipf_frag, ipf_state, ipf_nat, ipf_natfrag, ipf_auth, ipf_tokens;
int	opts = OPT_DONOTHING;
int	use_inet6 = 0;
int	docksum = 0;
int	pfil_delayed_copy = 0;
int	main __P((int, char *[]));
int	loadrules __P((char *, int));
int	kmemcpy __P((char *, long, int));
int     kstrncpy __P((char *, long, int n));
void	dumpnat __P((void));
void	dumpstate __P((void));
void	dumplookups __P((void));
void	dumpgroups __P((void));
void	drain_log __P((char *));
void	fixv4sums __P((mb_t *, ip_t *));

#if defined(__NetBSD__) || defined(__OpenBSD__) || SOLARIS || \
	(_BSDI_VERSION >= 199701) || (__FreeBSD_version >= 300000) || \
	defined(__osf__) || defined(linux)
int ipftestioctl __P((int, ioctlcmd_t, ...));
int ipnattestioctl __P((int, ioctlcmd_t, ...));
int ipstatetestioctl __P((int, ioctlcmd_t, ...));
int ipauthtestioctl __P((int, ioctlcmd_t, ...));
int ipscantestioctl __P((int, ioctlcmd_t, ...));
int ipsynctestioctl __P((int, ioctlcmd_t, ...));
int ipooltestioctl __P((int, ioctlcmd_t, ...));
#else
int ipftestioctl __P((dev_t, ioctlcmd_t, void *));
int ipnattestioctl __P((dev_t, ioctlcmd_t, void *));
int ipstatetestioctl __P((dev_t, ioctlcmd_t, void *));
int ipauthtestioctl __P((dev_t, ioctlcmd_t, void *));
int ipsynctestioctl __P((dev_t, ioctlcmd_t, void *));
int ipscantestioctl __P((dev_t, ioctlcmd_t, void *));
int ipooltestioctl __P((dev_t, ioctlcmd_t, void *));
#endif

static	ioctlfunc_t	iocfunctions[IPL_LOGSIZE] = { ipftestioctl,
						      ipnattestioctl,
						      ipstatetestioctl,
						      ipauthtestioctl,
						      ipsynctestioctl,
						      ipscantestioctl,
						      ipooltestioctl,
						      NULL };


int main(argc,argv)
int argc;
char *argv[];
{
	char	*datain, *iface, *ifname, *logout;
	int	fd, i, dir, c, loaded, dump, hlen;
	struct	in_addr	sip;
	struct	ifnet	*ifp;
	struct	ipread	*r;
	mb_t	mb, *m;
	ip_t	*ip;

	m = &mb;
	dir = 0;
	dump = 0;
	hlen = 0;
	loaded = 0;
	r = &iptext;
	iface = NULL;
	logout = NULL;
	datain = NULL;
	sip.s_addr = 0;
	ifname = "anon0";

	MUTEX_INIT(&ipf_rw, "ipf rw mutex");
	MUTEX_INIT(&ipf_timeoutlock, "ipf timeout lock");
	RWLOCK_INIT(&ipf_global, "ipf filter load/unload mutex");
	RWLOCK_INIT(&ipf_mutex, "ipf filter rwlock");
	RWLOCK_INIT(&ipf_ipidfrag, "ipf IP NAT-Frag rwlock");
	RWLOCK_INIT(&ipf_frcache, "ipf filter cache");
	RWLOCK_INIT(&ipf_tokens, "ipf token rwlock");

	initparse();
	if (fr_initialise() == -1)
		abort();
	fr_running = 1;

	while ((c = getopt(argc, argv, "6bCdDF:i:I:l:N:P:or:RS:T:vxX")) != -1)
		switch (c)
		{
		case '6' :
#ifdef	USE_INET6
			use_inet6 = 1;
#else
			fprintf(stderr, "IPv6 not supported\n");
			exit(1);
#endif
			break;
		case 'b' :
			opts |= OPT_BRIEF;
			break;
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'C' :
			docksum = 1;
			break;
		case 'D' :
			dump = 1;
			break;
		case 'F' :
			if (strcasecmp(optarg, "pcap") == 0)
				r = &pcap;
			else if (strcasecmp(optarg, "etherfind") == 0)
				r = &etherf;
			else if (strcasecmp(optarg, "snoop") == 0)
				r = &snoop;
			else if (strcasecmp(optarg, "tcpdump") == 0)
				r = &tcpd;
			else if (strcasecmp(optarg, "hex") == 0)
				r = &iphex;
			else if (strcasecmp(optarg, "text") == 0)
				r = &iptext;
			break;
		case 'i' :
			datain = optarg;
			break;
		case 'I' :
			ifname = optarg;
			break;
		case 'l' :
			logout = optarg;
			break;
		case 'N' :
			if (ipnat_parsefile(-1, ipnat_addrule, ipnattestioctl,
					    optarg) == -1)
				return -1;
			loaded = 1;
			opts |= OPT_NAT;
			break;
		case 'o' :
			opts |= OPT_SAVEOUT;
			break;
		case 'P' :
			if (ippool_parsefile(-1, optarg, ipooltestioctl) == -1)
				return -1;
			loaded = 1;
			break;
		case 'r' :
			if (ipf_parsefile(-1, ipf_addrule, iocfunctions,
					  optarg) == -1)
				return -1;
			loaded = 1;
			break;
		case 'S' :
			sip.s_addr = inet_addr(optarg);
			break;
		case 'R' :
			opts |= OPT_NORESOLVE;
			break;
		case 'T' :
			ipf_dotuning(-1, optarg, ipftestioctl);
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		case 'x' :
			opts |= OPT_HEX;
			break;
		}

	if (loaded == 0) {
		(void)fprintf(stderr,"no rules loaded\n");
		exit(-1);
	}

	if (opts & OPT_SAVEOUT)
		init_ifp();

	if (datain)
		fd = (*r->r_open)(datain);
	else
		fd = (*r->r_open)("-");

	if (fd < 0)
		exit(-1);

	ip = MTOD(m, ip_t *);
	while ((i = (*r->r_readip)(MTOD(m, char *), sizeof(m->mb_buf),
				    &iface, &dir)) > 0) {
		if ((iface == NULL) || (*iface == '\0'))
			iface = ifname;
		ifp = get_unit(iface, IP_V(ip));
		if (!use_inet6) {
			ip->ip_off = ntohs(ip->ip_off);
			ip->ip_len = ntohs(ip->ip_len);
			if ((r->r_flags & R_DO_CKSUM) || docksum)
				fixv4sums(m, ip);
			hlen = IP_HL(ip) << 2;
			if (sip.s_addr)
				dir = !(sip.s_addr == ip->ip_src.s_addr);
		}
#ifdef	USE_INET6
		else
			hlen = sizeof(ip6_t);
#endif
		/* ipfr_slowtimer(); */
		m = &mb;
		m->mb_len = i;
		i = fr_check(ip, hlen, ifp, dir, &m);
		if ((opts & OPT_NAT) == 0)
			switch (i)
			{
			case -4 :
				(void)printf("preauth");
				break;
			case -3 :
				(void)printf("account");
				break;
			case -2 :
				(void)printf("auth");
				break;
			case -1 :
				(void)printf("block");
				break;
			case 0 :
				(void)printf("pass");
				break;
			case 1 :
				if (m == NULL)
					(void)printf("bad-packet");
				else
					(void)printf("nomatch");
				break;
			case 3 :
				(void)printf("block return-rst");
				break;
			case 4 :
				(void)printf("block return-icmp");
				break;
			case 5 :
				(void)printf("block return-icmp-as-dest");
				break;
			default :
				(void)printf("recognised return %#x\n", i);
				break;
			}
		if (!use_inet6) {
			ip->ip_off = htons(ip->ip_off);
			ip->ip_len = htons(ip->ip_len);
		}

		if (!(opts & OPT_BRIEF)) {
			putchar(' ');
			printpacket(ip);
			printf("--------------");
		} else if ((opts & (OPT_BRIEF|OPT_NAT)) == (OPT_NAT|OPT_BRIEF))
			printpacket(ip);
		if (dir && (ifp != NULL) && IP_V(ip) && (m != NULL))
#if  defined(__sgi) && (IRIX < 60500)
			(*ifp->if_output)(ifp, (void *)m, NULL);
#else
# if TRU64 >= 1885
			(*ifp->if_output)(ifp, (void *)m, NULL, 0, 0);
# else
			(*ifp->if_output)(ifp, (void *)m, NULL, 0);
# endif
#endif
		if ((opts & (OPT_BRIEF|OPT_NAT)) != (OPT_NAT|OPT_BRIEF))
			putchar('\n');
		dir = 0;
		if (iface != ifname) {
			free(iface);
			iface = ifname;
		}
		m = &mb;
	}

	if (i != 0)
		fprintf(stderr, "readip failed: %d\n", i);
	(*r->r_close)();

	if (logout != NULL) {
		drain_log(logout);
	}

	if (dump == 1)  {
		dumpnat();
		dumpstate();
		dumplookups();
		dumpgroups();
	}

	fr_deinitialise();

	return 0;
}


#if defined(__NetBSD__) || defined(__OpenBSD__) || SOLARIS || \
	(_BSDI_VERSION >= 199701) || (__FreeBSD_version >= 300000) || \
	defined(__osf__) || defined(linux)
int ipftestioctl(int dev, ioctlcmd_t cmd, ...)
{
	caddr_t data;
	va_list ap;
	int i;

	va_start(ap, cmd);
	data = va_arg(ap, caddr_t);
	va_end(ap);

	i = iplioctl(IPL_LOGIPF, cmd, data, FWRITE|FREAD);
	if (opts & OPT_DEBUG)
		fprintf(stderr, "iplioctl(IPF,%#x,%p) = %d\n",
			(u_int)cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipnattestioctl(int dev, ioctlcmd_t cmd, ...)
{
	caddr_t data;
	va_list ap;
	int i;

	va_start(ap, cmd);
	data = va_arg(ap, caddr_t);
	va_end(ap);

	i = iplioctl(IPL_LOGNAT, cmd, data, FWRITE|FREAD);
	if (opts & OPT_DEBUG)
		fprintf(stderr, "iplioctl(NAT,%#x,%p) = %d\n",
			(u_int)cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipstatetestioctl(int dev, ioctlcmd_t cmd, ...)
{
	caddr_t data;
	va_list ap;
	int i;

	va_start(ap, cmd);
	data = va_arg(ap, caddr_t);
	va_end(ap);

	i = iplioctl(IPL_LOGSTATE, cmd, data, FWRITE|FREAD);
	if ((opts & OPT_DEBUG) || (i != 0))
		fprintf(stderr, "iplioctl(STATE,%#x,%p) = %d\n",
			(u_int)cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipauthtestioctl(int dev, ioctlcmd_t cmd, ...)
{
	caddr_t data;
	va_list ap;
	int i;

	va_start(ap, cmd);
	data = va_arg(ap, caddr_t);
	va_end(ap);

	i = iplioctl(IPL_LOGAUTH, cmd, data, FWRITE|FREAD);
	if ((opts & OPT_DEBUG) || (i != 0))
		fprintf(stderr, "iplioctl(AUTH,%#x,%p) = %d\n",
			(u_int)cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipscantestioctl(int dev, ioctlcmd_t cmd, ...)
{
	caddr_t data;
	va_list ap;
	int i;

	va_start(ap, cmd);
	data = va_arg(ap, caddr_t);
	va_end(ap);

	i = iplioctl(IPL_LOGSCAN, cmd, data, FWRITE|FREAD);
	if ((opts & OPT_DEBUG) || (i != 0))
		fprintf(stderr, "iplioctl(SCAN,%#x,%p) = %d\n",
			(u_int)cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipsynctestioctl(int dev, ioctlcmd_t cmd, ...)
{
	caddr_t data;
	va_list ap;
	int i;

	va_start(ap, cmd);
	data = va_arg(ap, caddr_t);
	va_end(ap);

	i = iplioctl(IPL_LOGSYNC, cmd, data, FWRITE|FREAD);
	if ((opts & OPT_DEBUG) || (i != 0))
		fprintf(stderr, "iplioctl(SYNC,%#x,%p) = %d\n",
			(u_int)cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipooltestioctl(int dev, ioctlcmd_t cmd, ...)
{
	caddr_t data;
	va_list ap;
	int i;

	va_start(ap, cmd);
	data = va_arg(ap, caddr_t);
	va_end(ap);

	i = iplioctl(IPL_LOGLOOKUP, cmd, data, FWRITE|FREAD);
	if ((opts & OPT_DEBUG) || (i != 0))
		fprintf(stderr, "iplioctl(POOL,%#x,%p) = %d\n",
			(u_int)cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}
#else
int ipftestioctl(dev, cmd, data)
dev_t dev;
ioctlcmd_t cmd;
void *data;
{
	int i;

	i = iplioctl(IPL_LOGIPF, cmd, data, FWRITE|FREAD);
	if ((opts & OPT_DEBUG) || (i != 0))
		fprintf(stderr, "iplioctl(IPF,%#x,%p) = %d\n", cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipnattestioctl(dev, cmd, data)
dev_t dev;
ioctlcmd_t cmd;
void *data;
{
	int i;

	i = iplioctl(IPL_LOGNAT, cmd, data, FWRITE|FREAD);
	if ((opts & OPT_DEBUG) || (i != 0))
		fprintf(stderr, "iplioctl(NAT,%#x,%p) = %d\n", cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipstatetestioctl(dev, cmd, data)
dev_t dev;
ioctlcmd_t cmd;
void *data;
{
	int i;

	i = iplioctl(IPL_LOGSTATE, cmd, data, FWRITE|FREAD);
	if ((opts & OPT_DEBUG) || (i != 0))
		fprintf(stderr, "iplioctl(STATE,%#x,%p) = %d\n", cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipauthtestioctl(dev, cmd, data)
dev_t dev;
ioctlcmd_t cmd;
void *data;
{
	int i;

	i = iplioctl(IPL_LOGAUTH, cmd, data, FWRITE|FREAD);
	if ((opts & OPT_DEBUG) || (i != 0))
		fprintf(stderr, "iplioctl(AUTH,%#x,%p) = %d\n", cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipsynctestioctl(dev, cmd, data)
dev_t dev;
ioctlcmd_t cmd;
void *data;
{
	int i;

	i = iplioctl(IPL_LOGSYNC, cmd, data, FWRITE|FREAD);
	if ((opts & OPT_DEBUG) || (i != 0))
		fprintf(stderr, "iplioctl(SYNC,%#x,%p) = %d\n", cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipscantestioctl(dev, cmd, data)
dev_t dev;
ioctlcmd_t cmd;
void *data;
{
	int i;

	i = iplioctl(IPL_LOGSCAN, cmd, data, FWRITE|FREAD);
	if ((opts & OPT_DEBUG) || (i != 0))
		fprintf(stderr, "iplioctl(SCAN,%#x,%p) = %d\n", cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}


int ipooltestioctl(dev, cmd, data)
dev_t dev;
ioctlcmd_t cmd;
void *data;
{
	int i;

	i = iplioctl(IPL_LOGLOOKUP, cmd, data, FWRITE|FREAD);
	if (opts & OPT_DEBUG)
		fprintf(stderr, "iplioctl(POOL,%#x,%p) = %d\n", cmd, data, i);
	if (i != 0) {
		errno = i;
		return -1;
	}
	return 0;
}
#endif


int kmemcpy(addr, offset, size)
char *addr;
long offset;
int size;
{
	bcopy((char *)offset, addr, size);
	return 0;
}


int kstrncpy(buf, pos, n)
char *buf;
long pos;
int n;
{
	char *ptr;

	ptr = (char *)pos;

	while ((n > 0) && (*buf++ = *ptr++))
		;
	return 0;
}


/*
 * Display the built up NAT table rules and mapping entries.
 */
void dumpnat()
{
	hostmap_t *hm;
	ipnat_t	*ipn;
	nat_t *nat;

	printf("List of active MAP/Redirect filters:\n");
	for (ipn = nat_list; ipn != NULL; ipn = ipn->in_next)
		printnat(ipn, opts & (OPT_DEBUG|OPT_VERBOSE));
	printf("\nList of active sessions:\n");
	for (nat = nat_instances; nat; nat = nat->nat_next) {
		printactivenat(nat, opts, 0, 0);
		if (nat->nat_aps)
			printaps(nat->nat_aps, opts);
	}

	printf("\nHostmap table:\n");
	for (hm = ipf_hm_maplist; hm != NULL; hm = hm->hm_next)
		printhostmap(hm, 0);
}


/*
 * Display the built up state table rules and mapping entries.
 */
void dumpstate()
{
	ipstate_t *ips;

	printf("List of active state sessions:\n");
	for (ips = ips_list; ips != NULL; )
		ips = printstate(ips, opts & (OPT_DEBUG|OPT_VERBOSE),
				 fr_ticks);
}


void dumplookups()
{
	iphtable_t *iph;
	ip_pool_t *ipl;
	int i;

	printf("List of configured pools\n");
	for (i = 0; i < IPL_LOGSIZE; i++)
		for (ipl = ip_pool_list[i]; ipl != NULL; ipl = ipl->ipo_next)
			printpool(ipl, bcopywrap, NULL, opts);

	printf("List of configured hash tables\n");
	for (i = 0; i < IPL_LOGSIZE; i++)
		for (iph = ipf_htables[i]; iph != NULL; iph = iph->iph_next)
			printhash(iph, bcopywrap, NULL, opts);
}


void dumpgroups()
{
	frgroup_t *fg;
	frentry_t *fr;
	int i;

	printf("List of groups configured (set 0)\n");
	for (i = 0; i < IPL_LOGSIZE; i++)
		for (fg =  ipfgroups[i][0]; fg != NULL; fg = fg->fg_next) {
			printf("Dev.%d. Group %s Ref %d Flags %#x\n",
				i, fg->fg_name, fg->fg_ref, fg->fg_flags);
			for (fr = fg->fg_start; fr != NULL; fr = fr->fr_next) {
#ifdef	USE_QUAD_T
				printf("%qu ",(unsigned long long)fr->fr_hits);
#else
				printf("%ld ", fr->fr_hits);
#endif
				printfr(fr, ipftestioctl);
			}
		}

	printf("List of groups configured (set 1)\n");
	for (i = 0; i < IPL_LOGSIZE; i++)
		for (fg =  ipfgroups[i][1]; fg != NULL; fg = fg->fg_next) {
			printf("Dev.%d. Group %s Ref %d Flags %#x\n",
				i, fg->fg_name, fg->fg_ref, fg->fg_flags);
			for (fr = fg->fg_start; fr != NULL; fr = fr->fr_next) {
#ifdef	USE_QUAD_T
				printf("%qu ",(unsigned long long)fr->fr_hits);
#else
				printf("%ld ", fr->fr_hits);
#endif
				printfr(fr, ipftestioctl);
			}
		}
}


void drain_log(filename)
char *filename;
{
	char buffer[DEFAULT_IPFLOGSIZE];
	struct iovec iov;
	struct uio uio;
	size_t resid;
	int fd, i;

	fd = open(filename, O_CREAT|O_TRUNC|O_WRONLY, 0644);
	if (fd == -1) {
		perror("drain_log:open");
		return;
	}

	for (i = 0; i <= IPL_LOGMAX; i++)
		while (1) {
			bzero((char *)&iov, sizeof(iov));
			iov.iov_base = buffer;
			iov.iov_len = sizeof(buffer);

			bzero((char *)&uio, sizeof(uio));
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_resid = iov.iov_len;
			resid = uio.uio_resid;

			if (ipflog_read(i, &uio) == 0) {
				/*
				 * If nothing was read then break out.
				 */
				if (uio.uio_resid == resid)
					break;
				write(fd, buffer, resid - uio.uio_resid);
			} else
				break;
	}

	close(fd);
}


void fixv4sums(m, ip)
mb_t *m;
ip_t *ip;
{
	u_char *csump, *hdr;

	ip->ip_sum = 0;
	ip->ip_sum = ipf_cksum((u_short *)ip, IP_HL(ip) << 2);

	csump = (u_char *)ip;
	csump += IP_HL(ip) << 2;

	switch (ip->ip_p)
	{
	case IPPROTO_TCP :
		hdr = csump;
		csump += offsetof(tcphdr_t, th_sum);
		break;
	case IPPROTO_UDP :
		hdr = csump;
		csump += offsetof(udphdr_t, uh_sum);
		break;
	case IPPROTO_ICMP :
		hdr = csump;
		csump += offsetof(icmphdr_t, icmp_cksum);
		break;
	default :
		csump = NULL;
		hdr = NULL;
		break;
	}
	if (hdr != NULL) {
		*csump = 0;
		*(u_short *)csump = fr_cksum(m, ip, ip->ip_p, hdr, ip->ip_len);
	}
}
