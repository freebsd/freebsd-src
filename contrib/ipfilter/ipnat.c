/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Added redirect stuff and a variety of bug fixes. (mcn@EnGarde.com)
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/time.h>
#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#if defined(sun) && (defined(__svr4__) || defined(__SVR4))
# include <sys/ioccom.h>
# include <sys/sysmacros.h>
#endif
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <ctype.h>
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_nat.h"
#include "ipf.h"
#include "kmem.h"

#if	defined(sun) && !SOLARIS2
# define	STRERROR(x)	sys_errlist[x]
extern	char	*sys_errlist[];
#else
# define	STRERROR(x)	strerror(x)
#endif

#if !defined(lint)
static const char sccsid[] ="@(#)ipnat.c	1.9 6/5/96 (C) 1993 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipnat.c,v 2.16.2.9 2001/07/18 15:06:33 darrenr Exp $";
#endif


#if	SOLARIS
#define	bzero(a,b)	memset(a,0,b)
#endif
#ifdef	USE_INET6
int	use_inet6 = 0;
#endif

static	char	thishost[MAXHOSTNAMELEN];


extern	char	*optarg;
extern	ipnat_t	*natparse __P((char *, int));
extern	void	natparsefile __P((int, char *, int));
extern	void	printnat __P((ipnat_t *, int, void *));

void	dostats __P((int, int)), flushtable __P((int, int));
void	usage __P((char *));
int	countbits __P((u_32_t));
char	*getnattype __P((ipnat_t *));
int	main __P((int, char*[]));
void	printaps __P((ap_session_t *, int));
char	*getsumd __P((u_32_t));


void usage(name)
char *name;
{
	fprintf(stderr, "%s: [-CFhlnrsv] [-f filename]\n", name);
	exit(1);
}


char *getsumd(sum)
u_32_t sum;
{
	static char sumdbuf[17];

	if (sum & NAT_HW_CKSUM)
		sprintf(sumdbuf, "hw(%#0x)", sum & 0xffff);
	else
		sprintf(sumdbuf, "%#0x", sum);
	return sumdbuf;
}


int main(argc, argv)
int argc;
char *argv[];
{
	int	fd = -1, opts = 0, c, mode = O_RDWR;
	char	*file = NULL, *core = NULL;

	while ((c = getopt(argc, argv, "CdFf:hlM:nrsv")) != -1)
		switch (c)
		{
		case 'C' :
			opts |= OPT_CLEAR;
			break;
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'f' :
			file = optarg;
			break;
		case 'F' :
			opts |= OPT_FLUSH;
			break;
		case 'h' :
			opts |=OPT_HITS;
			break;
		case 'l' :
			opts |= OPT_LIST;
			mode = O_RDONLY;
			break;
		case 'M' :
			core = optarg;
			break;
		case 'n' :
			opts |= OPT_NODO;
			mode = O_RDONLY;
			break;
		case 'r' :
			opts |= OPT_REMOVE;
			break;
		case 's' :
			opts |= OPT_STAT;
			mode = O_RDONLY;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		default :
			usage(argv[0]);
		}

	if (core != NULL) {
		if (openkmem(core) == -1)
			exit(1);
		(void) setgid(getgid());
		(void) setuid(getuid());
	}

	gethostname(thishost, sizeof(thishost));
	thishost[sizeof(thishost) - 1] = '\0';

	if (!(opts & OPT_NODO) && ((fd = open(IPL_NAT, mode)) == -1) &&
	    ((fd = open(IPL_NAT, O_RDONLY)) == -1)) {
		(void) fprintf(stderr, "%s: open: %s\n", IPL_NAT,
			STRERROR(errno));
		exit(-1);
	}

	if (opts & (OPT_FLUSH|OPT_CLEAR))
		flushtable(fd, opts);
	if (file)
		natparsefile(fd, file, opts);
	if (opts & (OPT_LIST|OPT_STAT))
		dostats(fd, opts);
	return 0;
}


void printaps(aps, opts)
ap_session_t *aps;
int opts;
{
	ap_session_t ap;
	ftpinfo_t ftp;
	aproxy_t apr;
	raudio_t ra;

	if (kmemcpy((char *)&ap, (long)aps, sizeof(ap)))
		return;
	if (kmemcpy((char *)&apr, (long)ap.aps_apr, sizeof(apr)))
		return;
	printf("\tproxy %s/%d use %d flags %x\n", apr.apr_label,
		apr.apr_p, apr.apr_ref, apr.apr_flags);
	printf("\t\tproto %d flags %#x bytes ", ap.aps_p, ap.aps_flags);
#ifdef	USE_QUAD_T
	printf("%qu pkts %qu", (unsigned long long)ap.aps_bytes,
		(unsigned long long)ap.aps_pkts);
#else
	printf("%lu pkts %lu", ap.aps_bytes, ap.aps_pkts);
#endif
	printf(" data %p psiz %d\n", ap.aps_data, ap.aps_psiz);
	if ((ap.aps_p == IPPROTO_TCP) && (opts & OPT_VERBOSE)) {
		printf("\t\tstate[%u,%u], sel[%d,%d]\n",
			ap.aps_state[0], ap.aps_state[1],
			ap.aps_sel[0], ap.aps_sel[1]);
#if (defined(NetBSD) && (NetBSD >= 199905) && (NetBSD < 1991011)) || \
    (__FreeBSD_version >= 300000) || defined(OpenBSD)
		printf("\t\tseq: off %hd/%hd min %x/%x\n",
			ap.aps_seqoff[0], ap.aps_seqoff[1],
			ap.aps_seqmin[0], ap.aps_seqmin[1]);
		printf("\t\tack: off %hd/%hd min %x/%x\n",
			ap.aps_ackoff[0], ap.aps_ackoff[1],
			ap.aps_ackmin[0], ap.aps_ackmin[1]);
#else
		printf("\t\tseq: off %hd/%hd min %lx/%lx\n",
			ap.aps_seqoff[0], ap.aps_seqoff[1],
			ap.aps_seqmin[0], ap.aps_seqmin[1]);
		printf("\t\tack: off %hd/%hd min %lx/%lx\n",
			ap.aps_ackoff[0], ap.aps_ackoff[1],
			ap.aps_ackmin[0], ap.aps_ackmin[1]);
#endif
	}

	if (!strcmp(apr.apr_label, "raudio") && ap.aps_psiz == sizeof(ra)) {
		if (kmemcpy((char *)&ra, (long)ap.aps_data, sizeof(ra)))
			return;
		printf("\tReal Audio Proxy:\n");
		printf("\t\tSeen PNA: %d\tVersion: %d\tEOS: %d\n",
			ra.rap_seenpna, ra.rap_version, ra.rap_eos);
		printf("\t\tMode: %#x\tSBF: %#x\n", ra.rap_mode, ra.rap_sbf);
		printf("\t\tPorts:pl %hu, pr %hu, sr %hu\n",
			ra.rap_plport, ra.rap_prport, ra.rap_srport);
	} else if (!strcmp(apr.apr_label, "ftp") &&
		   (ap.aps_psiz == sizeof(ftp))) {
		if (kmemcpy((char *)&ftp, (long)ap.aps_data, sizeof(ftp)))
			return;
		printf("\tFTP Proxy:\n");
		printf("\t\tpassok: %d\n", ftp.ftp_passok);
		ftp.ftp_side[0].ftps_buf[FTP_BUFSZ - 1] = '\0';
		ftp.ftp_side[1].ftps_buf[FTP_BUFSZ - 1] = '\0';
		printf("\tClient:\n");
		printf("\t\trptr %p wptr %p seq %x len %d junk %d\n",
			ftp.ftp_side[0].ftps_rptr, ftp.ftp_side[0].ftps_wptr,
			ftp.ftp_side[0].ftps_seq, ftp.ftp_side[0].ftps_len,
			ftp.ftp_side[0].ftps_junk);
		printf("\t\tbuf [");
		printbuf(ftp.ftp_side[0].ftps_buf, FTP_BUFSZ, 1);
		printf("]\n\tServer:\n");
		printf("\t\trptr %p wptr %p seq %x len %d junk %d\n",
			ftp.ftp_side[1].ftps_rptr, ftp.ftp_side[1].ftps_wptr,
			ftp.ftp_side[1].ftps_seq, ftp.ftp_side[1].ftps_len,
			ftp.ftp_side[1].ftps_junk);
		printf("\t\tbuf [");
		printbuf(ftp.ftp_side[1].ftps_buf, FTP_BUFSZ, 1);
		printf("]\n");
	}
}


/*
 * Get a nat filter type given its kernel address.
 */
char *getnattype(ipnat)
ipnat_t *ipnat;
{
	char *which;
	ipnat_t ipnatbuff;

	if (!ipnat || (ipnat && kmemcpy((char *)&ipnatbuff, (long)ipnat,
					sizeof(ipnatbuff))))
		return "???";

	switch (ipnatbuff.in_redir)
	{
	case NAT_MAP :
		which = "MAP";
		break;
	case NAT_MAPBLK :
		which = "MAP-BLOCK";
		break;
	case NAT_REDIRECT :
		which = "RDR";
		break;
	case NAT_BIMAP :
		which = "BIMAP";
		break;
	default :
		which = "unknown";
		break;
	}
	return which;
}


void dostats(fd, opts)
int fd, opts;
{
	hostmap_t hm, *hmp, **maptable;
	natstat_t ns, *nsp = &ns;
	nat_t **nt[2], *np, nat;
	u_int hv, hv1, hv2;
	ipnat_t	ipn;

	bzero((char *)&ns, sizeof(ns));

	if (!(opts & OPT_NODO) && ioctl(fd, SIOCGNATS, &nsp) == -1) {
		perror("ioctl(SIOCGNATS)");
		return;
	}

	if (opts & OPT_STAT) {
		printf("mapped\tin\t%lu\tout\t%lu\n",
			ns.ns_mapped[0], ns.ns_mapped[1]);
		printf("added\t%lu\texpired\t%lu\n",
			ns.ns_added, ns.ns_expire);
		printf("no memory\t%lu\tbad nat\t%lu\n",
			ns.ns_memfail, ns.ns_badnat);
		printf("inuse\t%lu\nrules\t%lu\n", ns.ns_inuse, ns.ns_rules);
		printf("wilds\t%u\n", ns.ns_wilds);
		if (opts & OPT_VERBOSE)
			printf("table %p list %p\n", ns.ns_table, ns.ns_list);
	}
	if (opts & OPT_LIST) {
		printf("List of active MAP/Redirect filters:\n");
		while (ns.ns_list) {
			if (kmemcpy((char *)&ipn, (long)ns.ns_list,
				    sizeof(ipn))) {
				perror("kmemcpy");
				break;
			}
			if (opts & OPT_HITS)
				printf("%d ", ipn.in_hits);
			printnat(&ipn, opts & (OPT_DEBUG|OPT_VERBOSE),
				 (void *)ns.ns_list);
			ns.ns_list = ipn.in_next;
		}

		nt[0] = (nat_t **)malloc(sizeof(*nt) * NAT_SIZE);
		if (kmemcpy((char *)nt[0], (long)ns.ns_table[0],
			    sizeof(**nt) * NAT_SIZE)) {
			perror("kmemcpy");
			return;
		}

		printf("\nList of active sessions:\n");

		for (np = ns.ns_instances; np; np = nat.nat_next) {
			if (kmemcpy((char *)&nat, (long)np, sizeof(nat)))
				break;

			printf("%s %-15s %-5hu <- ->", getnattype(nat.nat_ptr),
			       inet_ntoa(nat.nat_inip), ntohs(nat.nat_inport));
			printf(" %-15s %-5hu", inet_ntoa(nat.nat_outip),
				ntohs(nat.nat_outport));
			printf(" [%s %hu]", inet_ntoa(nat.nat_oip),
				ntohs(nat.nat_oport));
			if (opts & OPT_VERBOSE) {
				printf("\n\tage %lu use %hu sumd %s/",
					nat.nat_age, nat.nat_use,
					getsumd(nat.nat_sumd[0]));
				hv1 = NAT_HASH_FN(nat.nat_inip.s_addr,
						  nat.nat_inport,
						  0xffffffff),
				hv1 = NAT_HASH_FN(nat.nat_oip.s_addr,
						  hv1 + nat.nat_oport,
						  NAT_TABLE_SZ),
				hv2 = NAT_HASH_FN(nat.nat_outip.s_addr,
						  nat.nat_outport,
						  0xffffffff),
				hv2 = NAT_HASH_FN(nat.nat_oip.s_addr,
						  hv2 + nat.nat_oport,
						  NAT_TABLE_SZ),
				printf("%s pr %u bkt %d/%d flags %x ",
					getsumd(nat.nat_sumd[1]), nat.nat_p,
					hv1, hv2, nat.nat_flags);
#ifdef	USE_QUAD_T
				printf("bytes %qu pkts %qu",
					(unsigned long long)nat.nat_bytes,
					(unsigned long long)nat.nat_pkts);
#else
				printf("bytes %lu pkts %lu",
					nat.nat_bytes, nat.nat_pkts);
#endif
#if SOLARIS
				printf(" %lx", nat.nat_ipsumd);
#endif
			}
			putchar('\n');
			if (nat.nat_aps)
				printaps(nat.nat_aps, opts);
		}

		if (opts & OPT_VERBOSE) {
			printf("\nList of active host mappings:\n");
		
			maptable = (hostmap_t **)malloc(sizeof(hostmap_t *) *
							ns.ns_hostmap_sz);
			if (kmemcpy((char *)maptable, (u_long)ns.ns_maptable,
				    sizeof(hostmap_t *) * ns.ns_hostmap_sz)) {
				perror("kmemcpy (maptable)");
				return;
			}

			for (hv = 0; hv < ns.ns_hostmap_sz; hv++) {
				hmp = maptable[hv];

				while(hmp) {

					if (kmemcpy((char *)&hm, (u_long)hmp,
						    sizeof(hostmap_t))) {
						perror("kmemcpy (hostmap)");
						return;
					}
	
					printf("%s -> ",
					       inet_ntoa(hm.hm_realip));
					printf("%s ", inet_ntoa(hm.hm_mapip));
					printf("(use = %d hv = %u)\n",
					       hm.hm_ref, hv);
					hmp = hm.hm_next;
				}
			}
			free(maptable);
		}
		free(nt[0]);
	}
}


void flushtable(fd, opts)
int fd, opts;
{
	int n = 0;

	if (opts & OPT_FLUSH) {
		n = 0;
		if (!(opts & OPT_NODO) && ioctl(fd, SIOCIPFFL, &n) == -1)
			perror("ioctl(SIOCFLNAT)");
		else
			printf("%d entries flushed from NAT table\n", n);
	}

	if (opts & OPT_CLEAR) {
		n = 1;
		if (!(opts & OPT_NODO) && ioctl(fd, SIOCIPFFL, &n) == -1)
			perror("ioctl(SIOCCNATL)");
		else
			printf("%d entries flushed from NAT list\n", n);
	}
}
