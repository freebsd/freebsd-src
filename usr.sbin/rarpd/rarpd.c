/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1990, 1991, 1992, 1993, 1996\n\
The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /home/ncvs/src/usr.sbin/rarpd/rarpd.c,v 1.7.2.2 1996/11/28 08:28:15 phk Exp $ (LBL)";
#endif

/*
 * rarpd - Reverse ARP Daemon
 *
 * Usage:	rarpd -a [ -fsv ] [ hostname ]
 *		rarpd [ -fsv ] interface [ hostname ]
 *
 * 'hostname' is optional solely for backwards compatibility with Sun's rarpd.
 * Currently, the argument is ignored.
 */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/bpf.h>
#include <net/if.h>

#if BSD >= 199100
#include <net/if_types.h>
#include <net/if_dl.h>
#if BSD >= 199200
#include <net/route.h>
#endif
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#if BSD >= 199200
#include <stdlib.h>
#include <unistd.h>
#else

extern char *optarg;
extern int optind, opterr;

extern int errno;
#endif

#if defined(SUNOS4) || defined(__FreeBSD__) /* XXX */
#define HAVE_DIRENT_H
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#else
#include <sys/dir.h>
#endif

/* Cast a struct sockaddr to a structaddr_in */
#define SATOSIN(sa) ((struct sockaddr_in *)(sa))

#ifndef TFTP_DIR
#define TFTP_DIR "/tftpboot"
#endif

#if BSD >= 199200
#define ARPSECS (20 * 60)		/* as per code in netinet/if_ether.c */
#define REVARP_REQUEST ARPOP_REVREQUEST
#define REVARP_REPLY ARPOP_REVREPLY
#endif

#ifndef ETHERTYPE_REVARP
#define ETHERTYPE_REVARP 0x8035
#define REVARP_REQUEST 3
#define REVARP_REPLY 4
#endif

/*
 * Map field names in ether_arp struct.  What a pain in the neck.
 */
#ifdef SUNOS3
#undef arp_sha
#undef arp_spa
#undef arp_tha
#undef arp_tpa
#define arp_sha arp_xsha
#define arp_spa arp_xspa
#define arp_tha arp_xtha
#define arp_tpa arp_xtpa
#endif

#ifndef __GNUC__
#define inline
#endif

/*
 * The structure for each interface.
 */
struct if_info {
	struct	if_info *ii_next;
	int	ii_fd;		/* BPF file descriptor */
	u_long	ii_ipaddr;	/* IP address of this interface */
	u_long	ii_netmask;	/* subnet or net mask */
	u_char	ii_eaddr[6];	/* Ethernet address of this interface */
	char ii_ifname[sizeof(((struct ifreq *)0)->ifr_name) + 1];
};

/*
 * The list of all interfaces that are being listened to.  rarp_loop()
 * "selects" on the descriptors in this list.
 */
struct if_info *iflist;

int verbose;			/* verbose messages */
int s;				/* inet datagram socket */
char *tftp_dir = TFTP_DIR;	/* tftp directory */

#ifndef __P
#define __P(protos) ()
#endif

#if BSD < 199200
extern	char *malloc();
extern	void exit();
#endif
extern	int ether_ntohost();

void	init __P((char *));
void	init_one __P((struct ifreq *, char *));
char	*intoa __P((u_long));
u_long	ipaddrtonetmask __P((u_long));
char	*eatoa __P((u_char *));
int	rarp_bootable __P((u_long));
void	rarp_loop __P((void));
int	rarp_open __P((char *));
void	rarp_process __P((struct if_info *, u_char *, u_int));
void	rarp_reply __P((struct if_info *, struct ether_header *, u_long, u_int));
void	update_arptab __P((u_char *, u_long));
void	usage __P((void));

static	u_char zero[6];

int sflag = 0;			/* ignore /tftpboot */

void
main(argc, argv)
	int argc;
	char **argv;
{
	int op;
	char *ifname, *hostname, *name;

	int aflag = 0;		/* listen on "all" interfaces  */
	int fflag = 0;		/* don't fork */

	if ((name = strrchr(argv[0], '/')) != NULL)
		++name;
	else
		name = argv[0];
	if (*name == '-')
		++name;

	/*
	 * All error reporting is done through syslogs.
	 */
	openlog(name, LOG_PID | LOG_CONS, LOG_DAEMON);

	opterr = 0;
	while ((op = getopt(argc, argv, "afsv")) !=  -1) {
		switch (op) {
		case 'a':
			++aflag;
			break;

		case 'f':
			++fflag;
			break;

		case 's':
			++sflag;
			break;

		case 'v':
			++verbose;
			break;

		default:
			usage();
			/* NOTREACHED */
		}
	}
	ifname = argv[optind++];
	hostname =  ifname ? argv[optind] : NULL;
	if ((aflag && ifname) || (!aflag && ifname == NULL))
		usage();

	if (aflag)
		init(NULL);
	else
		init(ifname);

	if (!fflag) {
		if (daemon(0,0)) {
			syslog(LOG_ERR, "cannot fork");
			exit(1);
		}
	}
	rarp_loop();
}

/*
 * Add to the interface list.
 */
void
init_one(ifrp, target)
	register struct ifreq *ifrp;
	register char *target;
{
	register struct if_info *ii;
	register struct sockaddr_dl *ll;
	int family;
	struct ifreq ifr;

	family = ifrp->ifr_addr.sa_family;
	switch (family) {

	case AF_INET:
#if BSD >= 199100
	case AF_LINK:
#endif
		(void)strncpy(ifr.ifr_name, ifrp->ifr_name,
		    sizeof(ifrp->ifr_name));
		if (ioctl(s, SIOCGIFFLAGS, (char *)&ifr) < 0) {
			syslog(LOG_ERR,
			    "SIOCGIFFLAGS: %.*s: %m",
				sizeof(ifrp->ifr_name), ifrp->ifr_name);
			exit(1);
		}
		if ((ifr.ifr_flags & IFF_UP) == 0 ||
		    (ifr.ifr_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) != 0)
			return;
		break;


	default:
		return;
	}

	/* Don't bother going any further if not the target interface */
	if (target != NULL &&
	    strncmp(ifrp->ifr_name, target, sizeof(ifrp->ifr_name)) != 0)
		return;

	/* Look for interface in list */
	for (ii = iflist; ii != NULL; ii = ii->ii_next)
		if (strncmp(ifrp->ifr_name, ii->ii_ifname,
		    sizeof(ifrp->ifr_name)) == 0)
			break;

	/* Allocate a new one if not found */
	if (ii == NULL) {
		ii = (struct if_info *)malloc(sizeof(*ii));
		if (ii == NULL) {
			syslog(LOG_ERR, "malloc: %m");
			exit(1);
		}
		bzero(ii, sizeof(*ii));
		ii->ii_fd = -1;
		(void)strncpy(ii->ii_ifname, ifrp->ifr_name,
		    sizeof(ifrp->ifr_name));
		ii->ii_ifname[sizeof(ii->ii_ifname) - 1] = '\0';
		ii->ii_next = iflist;
		iflist = ii;
	}

	switch (family) {

	case AF_INET:
		if (ioctl(s, SIOCGIFADDR, (char *)&ifr) < 0) {
			syslog(LOG_ERR, "ipaddr SIOCGIFADDR: %s: %m",
			    ii->ii_ifname);
			exit(1);
		}
		ii->ii_ipaddr = SATOSIN(&ifr.ifr_addr)->sin_addr.s_addr;
		if (ioctl(s, SIOCGIFNETMASK, (char *)&ifr) < 0) {
			syslog(LOG_ERR, "SIOCGIFNETMASK: %m");
			exit(1);
		}
		ii->ii_netmask = SATOSIN(&ifr.ifr_addr)->sin_addr.s_addr;
		if (ii->ii_netmask == 0)
			ii->ii_netmask = ipaddrtonetmask(ii->ii_ipaddr);
		if (ii->ii_fd < 0) {
			ii->ii_fd = rarp_open(ii->ii_ifname);
#if BSD < 199100
			/* Use BPF descriptor to get ethernet address. */
			if (ioctl(ii->ii_fd, SIOCGIFADDR, (char *)&ifr) < 0) {
				syslog(LOG_ERR, "eaddr SIOCGIFADDR: %s: %m",
				    ii->ii_ifname);
				exit(1);
			}
			bcopy(&ifr.ifr_addr.sa_data[0], ii->ii_eaddr, 6);
#endif
		}
		break;

#if BSD >= 199100
		case AF_LINK:
			ll = (struct sockaddr_dl *)&ifrp->ifr_addr;
			if (ll->sdl_type == IFT_ETHER)
				bcopy(LLADDR(ll), ii->ii_eaddr, 6);
			break;
#endif
		}
}
/*
 * Initialize all "candidate" interfaces that are in the system
 * configuration list.  A "candidate" is up, not loopback and not
 * point to point.
 */
void
init(target)
	char *target;
{
	register int n;
	register struct ifreq *ifrp, *ifend;
	register struct if_info *ii, *nii, *lii;
	struct ifconf ifc;
	struct ifreq ibuf[16];

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "socket: %m");
		exit(1);
	}
	ifc.ifc_len = sizeof ibuf;
	ifc.ifc_buf = (caddr_t)ibuf;
	if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0 ||
	    (u_int)ifc.ifc_len < sizeof(struct ifreq)) {
		syslog(LOG_ERR, "SIOCGIFCONF: %m");
		exit(1);
	}
	ifrp = ibuf;
	ifend = (struct ifreq *)((char *)ibuf + ifc.ifc_len);
	while (ifrp < ifend) {
		init_one(ifrp, target);

#if BSD >= 199100
		n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
		if (n < sizeof(*ifrp))
			n = sizeof(*ifrp);
		ifrp = (struct ifreq *)((char *)ifrp + n);
#else
		++ifrp;
#endif
	}

	/* Throw away incomplete interfaces */
	lii = NULL;
	for (ii = iflist; ii != NULL; ii = nii) {
		nii = ii->ii_next;
		if (ii->ii_ipaddr == 0 ||
		    bcmp(ii->ii_eaddr, zero, 6) == 0) {
			if (lii == NULL)
				iflist = nii;
			else
				lii->ii_next = nii;
			if (ii->ii_fd >= 0)
				close(ii->ii_fd);
			free(ii);
			continue;
		}
		lii = ii;
	}

	/* Verbose stuff */
	if (verbose)
		for (ii = iflist; ii != NULL; ii = ii->ii_next)
			syslog(LOG_DEBUG, "%s %s 0x%08x %s",
			    ii->ii_ifname, intoa(ntohl(ii->ii_ipaddr)),
			    ntohl(ii->ii_netmask), eatoa(ii->ii_eaddr));
}

void
usage()
{
	(void)fprintf(stderr, "usage: rarpd [ -afnv ] [ interface ]\n");
	exit(1);
}

static int
bpf_open()
{
	int fd;
	int n = 0;
	char device[sizeof "/dev/bpf000"];

	/*
	 * Go through all the minors and find one that isn't in use.
	 */
	do {
		(void)sprintf(device, "/dev/bpf%d", n++);
		fd = open(device, O_RDWR);
	} while (fd < 0 && errno == EBUSY);

	if (fd < 0) {
		syslog(LOG_ERR, "%s: %m", device);
		exit(1);
	}
	return fd;
}

/*
 * Open a BPF file and attach it to the interface named 'device'.
 * Set immediate mode, and set a filter that accepts only RARP requests.
 */
int
rarp_open(device)
	char *device;
{
	int fd;
	struct ifreq ifr;
	u_int dlt;
	int immediate;

	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 12),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, ETHERTYPE_REVARP, 0, 3),
		BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 20),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, REVARP_REQUEST, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, sizeof(struct ether_arp) +
			 sizeof(struct ether_header)),
		BPF_STMT(BPF_RET|BPF_K, 0),
	};
	static struct bpf_program filter = {
		sizeof insns / sizeof(insns[0]),
		insns
	};

	fd = bpf_open();
	/*
	 * Set immediate mode so packets are processed as they arrive.
	 */
	immediate = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &immediate) < 0) {
		syslog(LOG_ERR, "BIOCIMMEDIATE: %m");
		exit(1);
	}
	(void)strncpy(ifr.ifr_name, device, sizeof ifr.ifr_name);
	if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) < 0) {
		syslog(LOG_ERR, "BIOCSETIF: %m");
		exit(1);
	}
	/*
	 * Check that the data link layer is an Ethernet; this code won't
	 * work with anything else.
	 */
	if (ioctl(fd, BIOCGDLT, (caddr_t)&dlt) < 0) {
		syslog(LOG_ERR, "BIOCGDLT: %m");
		exit(1);
	}
	if (dlt != DLT_EN10MB) {
		syslog(LOG_ERR, "%s is not an ethernet", device);
		exit(1);
	}
	/*
	 * Set filter program.
	 */
	if (ioctl(fd, BIOCSETF, (caddr_t)&filter) < 0) {
		syslog(LOG_ERR, "BIOCSETF: %m");
		exit(1);
	}
	return fd;
}

/*
 * Perform various sanity checks on the RARP request packet.  Return
 * false on failure and log the reason.
 */
static int
rarp_check(p, len)
	u_char *p;
	u_int len;
{
	struct ether_header *ep = (struct ether_header *)p;
	struct ether_arp *ap = (struct ether_arp *)(p + sizeof(*ep));

	if (len < sizeof(*ep) + sizeof(*ap)) {
		syslog(LOG_ERR, "truncated request, got %d, expected %d",
				len, sizeof(*ep) + sizeof(*ap));
		return 0;
	}
	/*
	 * XXX This test might be better off broken out...
	 */
	if (ntohs(ep->ether_type) != ETHERTYPE_REVARP ||
	    ntohs(ap->arp_hrd) != ARPHRD_ETHER ||
	    ntohs(ap->arp_op) != REVARP_REQUEST ||
	    ntohs(ap->arp_pro) != ETHERTYPE_IP ||
	    ap->arp_hln != 6 || ap->arp_pln != 4) {
		syslog(LOG_DEBUG, "request fails sanity check");
		return 0;
	}
	if (bcmp((char *)&ep->ether_shost, (char *)&ap->arp_sha, 6) != 0) {
		syslog(LOG_DEBUG, "ether/arp sender address mismatch");
		return 0;
	}
	if (bcmp((char *)&ap->arp_sha, (char *)&ap->arp_tha, 6) != 0) {
		syslog(LOG_DEBUG, "ether/arp target address mismatch");
		return 0;
	}
	return 1;
}

#ifndef FD_SETSIZE
#define FD_SET(n, fdp) ((fdp)->fds_bits[0] |= (1 << (n)))
#define FD_ISSET(n, fdp) ((fdp)->fds_bits[0] & (1 << (n)))
#define FD_ZERO(fdp) ((fdp)->fds_bits[0] = 0)
#endif

/*
 * Loop indefinitely listening for RARP requests on the
 * interfaces in 'iflist'.
 */
void
rarp_loop()
{
	u_char *buf, *bp, *ep;
	int cc, fd;
	fd_set fds, listeners;
	int bufsize, maxfd = 0;
	struct if_info *ii;

	if (iflist == NULL) {
		syslog(LOG_ERR, "no interfaces");
		exit(1);
	}
	if (ioctl(iflist->ii_fd, BIOCGBLEN, (caddr_t)&bufsize) < 0) {
		syslog(LOG_ERR, "BIOCGBLEN: %m");
		exit(1);
	}
	buf = (u_char *)malloc((unsigned)bufsize);
	if (buf == NULL) {
		syslog(LOG_ERR, "malloc: %m");
		exit(1);
	}

	while (1) {
		/*
		 * Find the highest numbered file descriptor for select().
		 * Initialize the set of descriptors to listen to.
		 */
		FD_ZERO(&fds);
		for (ii = iflist; ii != NULL; ii = ii->ii_next) {
			FD_SET(ii->ii_fd, &fds);
			if (ii->ii_fd > maxfd)
				maxfd = ii->ii_fd;
		}
		listeners = fds;
		if (select(maxfd + 1, &listeners, NULL, NULL, NULL) < 0) {
			/* Don't choke when we get ptraced */
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "select: %m");
			exit(1);
		}
		for (ii = iflist; ii != NULL; ii = ii->ii_next) {
			fd = ii->ii_fd;
			if (!FD_ISSET(fd, &listeners))
				continue;
		again:
			cc = read(fd, (char *)buf, bufsize);
			/* Don't choke when we get ptraced */
			if (cc < 0 && errno == EINTR)
				goto again;
#if defined(SUNOS3) || defined(SUNOS4)
			/*
			 * Due to a SunOS bug, after 2^31 bytes, the
			 * file offset overflows and read fails with
			 * EINVAL.  The lseek() to 0 will fix things.
			 */
			if (cc < 0) {
				if (errno == EINVAL &&
				    (long)(tell(fd) + bufsize) < 0) {
					(void)lseek(fd, 0, 0);
					goto again;
				}
				syslog(LOG_ERR, "read: %m");
				exit(1);
			}
#endif

			/* Loop through the packet(s) */
#define bhp ((struct bpf_hdr *)bp)
			bp = buf;
			ep = bp + cc;
			while (bp < ep) {
				register u_int caplen, hdrlen;

				caplen = bhp->bh_caplen;
				hdrlen = bhp->bh_hdrlen;
				if (rarp_check(bp + hdrlen, caplen))
					rarp_process(ii, bp + hdrlen, caplen);
				bp += BPF_WORDALIGN(hdrlen + caplen);
			}
		}
	}
#undef bhp
}

/*
 * True if this server can boot the host whose IP address is 'addr'.
 * This check is made by looking in the tftp directory for the
 * configuration file.
 */
int
rarp_bootable(addr)
	u_long addr;
{
#ifdef HAVE_DIRENT_H
	register struct dirent *dent;
#else
	register struct direct *dent;
#endif
	register DIR *d;
	char ipname[9];
	static DIR *dd = NULL;

	(void)sprintf(ipname, "%08X", (unsigned int )ntohl(addr));

	/*
	 * If directory is already open, rewind it.  Otherwise, open it.
	 */
	if ((d = dd) != NULL)
		rewinddir(d);
	else {
		if (chdir(tftp_dir) == -1) {
			syslog(LOG_ERR, "chdir: %s: %m", tftp_dir);
			exit(1);
		}
		d = opendir(".");
		if (d == NULL) {
			syslog(LOG_ERR, "opendir: %m");
			exit(1);
		}
		dd = d;
	}
	while ((dent = readdir(d)) != NULL)
		if (strncmp(dent->d_name, ipname, 8) == 0)
			return 1;
	return 0;
}

/*
 * Given a list of IP addresses, 'alist', return the first address that
 * is on network 'net'; 'netmask' is a mask indicating the network portion
 * of the address.
 */
u_long
choose_ipaddr(alist, net, netmask)
	u_long **alist;
	u_long net;
	u_long netmask;
{
	for (; *alist; ++alist)
		if ((**alist & netmask) == net)
			return **alist;
	return 0;
}

/*
 * Answer the RARP request in 'pkt', on the interface 'ii'.  'pkt' has
 * already been checked for validity.  The reply is overlaid on the request.
 */
void
rarp_process(ii, pkt, len)
	struct if_info *ii;
	u_char *pkt;
	u_int len;
{
	struct ether_header *ep;
	struct hostent *hp;
	u_long target_ipaddr;
	char ename[256];

	ep = (struct ether_header *)pkt;
	/* should this be arp_tha? */
	if (ether_ntohost(ename, &ep->ether_shost) != 0) {
		syslog(LOG_ERR, "cannot map %s to name",
			eatoa(ep->ether_shost));
		return;
	}

	if ((hp = gethostbyname(ename)) == NULL) {
		syslog(LOG_ERR, "cannot map %s to IP address", ename);
		return;
	}

	/*
	 * Choose correct address from list.
	 */
	if (hp->h_addrtype != AF_INET) {
		syslog(LOG_ERR, "cannot handle non IP addresses for %s",
								ename);
		return;
	}
	target_ipaddr = choose_ipaddr((u_long **)hp->h_addr_list,
				      ii->ii_ipaddr & ii->ii_netmask,
				      ii->ii_netmask);
	if (target_ipaddr == 0) {
		syslog(LOG_ERR, "cannot find %s on net %s",
		       ename, intoa(ntohl(ii->ii_ipaddr & ii->ii_netmask)));
		return;
	}
	if (sflag || rarp_bootable(target_ipaddr))
		rarp_reply(ii, ep, target_ipaddr, len);
	else if (verbose > 1)
		syslog(LOG_INFO, "%s %s at %s DENIED (not bootable)",
		    ii->ii_ifname,
		    eatoa(ep->ether_shost),
		    intoa(ntohl(target_ipaddr)));
}

/*
 * Poke the kernel arp tables with the ethernet/ip address combinataion
 * given.  When processing a reply, we must do this so that the booting
 * host (i.e. the guy running rarpd), won't try to ARP for the hardware
 * address of the guy being booted (he cannot answer the ARP).
 */
#if BSD >= 199200
static struct sockaddr_inarp sin_inarp = {
	sizeof(struct sockaddr_inarp), AF_INET
};
static struct sockaddr_dl sin_dl = {
	sizeof(struct sockaddr_dl), AF_LINK, 0, IFT_ETHER, 0, 6
};
static struct {
	struct rt_msghdr rthdr;
	char rtspace[512];
} rtmsg;

void
update_arptab(ep, ipaddr)
	u_char *ep;
	u_long ipaddr;
{
	register int cc;
	register struct sockaddr_inarp *ar, *ar2;
	register struct sockaddr_dl *ll, *ll2;
	register struct rt_msghdr *rt;
	register int xtype, xindex;
	static pid_t pid;
	static int r, seq;
	static init = 0;

	if (!init) {
		r = socket(PF_ROUTE, SOCK_RAW, 0);
		if (r < 0) {
			syslog(LOG_ERR, "raw route socket: %m");
			exit(1);
		}
		pid = getpid();
		++init;
	}

	ar = &sin_inarp;
	ar->sin_addr.s_addr = ipaddr;
	ll = &sin_dl;
	bcopy(ep, LLADDR(ll), 6);

	/* Get the type and interface index */
	rt = &rtmsg.rthdr;
	bzero(rt, sizeof(rtmsg));
	rt->rtm_version = RTM_VERSION;
	rt->rtm_addrs = RTA_DST;
	rt->rtm_type = RTM_GET;
	rt->rtm_seq = ++seq;
	ar2 = (struct sockaddr_inarp *)rtmsg.rtspace;
	bcopy(ar, ar2, sizeof(*ar));
	rt->rtm_msglen = sizeof(*rt) + sizeof(*ar);
	errno = 0;
	if (write(r, rt, rt->rtm_msglen) < 0 && errno != ESRCH) {
		syslog(LOG_ERR, "rtmsg get write: %m");
		return;
	}
	do {
		cc = read(r, rt, sizeof(rtmsg));
	} while (cc > 0 && (rt->rtm_seq != seq || rt->rtm_pid != pid));
	if (cc < 0) {
		syslog(LOG_ERR, "rtmsg get read: %m");
		return;
	}
	ll2 = (struct sockaddr_dl *)((u_char *)ar2 + ar2->sin_len);
	if (ll2->sdl_family != AF_LINK) {
		/*
		 * XXX I think this means the ip address is not on a
		 * directly connected network (the family is AF_INET in
		 * this case).
		 */
		syslog(LOG_ERR, "bogus link family (%d) wrong net for %08X?\n",
		    ll2->sdl_family, ipaddr);
		return;
	}
	xtype = ll2->sdl_type;
	xindex = ll2->sdl_index;

	/* Set the new arp entry */
	bzero(rt, sizeof(rtmsg));
	rt->rtm_version = RTM_VERSION;
	rt->rtm_addrs = RTA_DST | RTA_GATEWAY;
	rt->rtm_inits = RTV_EXPIRE;
	rt->rtm_rmx.rmx_expire = time(0) + ARPSECS;
	rt->rtm_flags = RTF_HOST | RTF_STATIC;
	rt->rtm_type = RTM_ADD;
	rt->rtm_seq = ++seq;

	bcopy(ar, ar2, sizeof(*ar));

	ll2 = (struct sockaddr_dl *)((u_char *)ar2 + sizeof(*ar2));
	bcopy(ll, ll2, sizeof(*ll));
	ll2->sdl_type = xtype;
	ll2->sdl_index = xindex;

	rt->rtm_msglen = sizeof(*rt) + sizeof(*ar2) + sizeof(*ll2);
	errno = 0;
	if (write(r, rt, rt->rtm_msglen) < 0 && errno != EEXIST) {
		syslog(LOG_ERR, "rtmsg add write: %m");
		return;
	}
	do {
		cc = read(r, rt, sizeof(rtmsg));
	} while (cc > 0 && (rt->rtm_seq != seq || rt->rtm_pid != pid));
	if (cc < 0) {
		syslog(LOG_ERR, "rtmsg add read: %m");
		return;
	}
}
#else
void
update_arptab(ep, ipaddr)
	u_char *ep;
	u_long ipaddr;
{
	struct arpreq request;
	struct sockaddr_in *sin;

	request.arp_flags = 0;
	sin = (struct sockaddr_in *)&request.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = ipaddr;
	request.arp_ha.sa_family = AF_UNSPEC;
	bcopy((char *)ep, (char *)request.arp_ha.sa_data, 6);

	if (ioctl(s, SIOCSARP, (caddr_t)&request) < 0)
		syslog(LOG_ERR, "SIOCSARP: %m");
}
#endif

/*
 * Build a reverse ARP packet and sent it out on the interface.
 * 'ep' points to a valid REVARP_REQUEST.  The REVARP_REPLY is built
 * on top of the request, then written to the network.
 *
 * RFC 903 defines the ether_arp fields as follows.  The following comments
 * are taken (more or less) straight from this document.
 *
 * REVARP_REQUEST
 *
 * arp_sha is the hardware address of the sender of the packet.
 * arp_spa is undefined.
 * arp_tha is the 'target' hardware address.
 *   In the case where the sender wishes to determine his own
 *   protocol address, this, like arp_sha, will be the hardware
 *   address of the sender.
 * arp_tpa is undefined.
 *
 * REVARP_REPLY
 *
 * arp_sha is the hardware address of the responder (the sender of the
 *   reply packet).
 * arp_spa is the protocol address of the responder (see the note below).
 * arp_tha is the hardware address of the target, and should be the same as
 *   that which was given in the request.
 * arp_tpa is the protocol address of the target, that is, the desired address.
 *
 * Note that the requirement that arp_spa be filled in with the responder's
 * protocol is purely for convenience.  For instance, if a system were to use
 * both ARP and RARP, then the inclusion of the valid protocol-hardware
 * address pair (arp_spa, arp_sha) may eliminate the need for a subsequent
 * ARP request.
 */
void
rarp_reply(ii, ep, ipaddr, len)
	struct if_info *ii;
	struct ether_header *ep;
	u_long ipaddr;
	u_int len;
{
	int n;
	struct ether_arp *ap = (struct ether_arp *)(ep + 1);

	update_arptab((u_char *)&ap->arp_sha, ipaddr);

	/*
	 * Build the rarp reply by modifying the rarp request in place.
	 */
	ap->arp_op = htons(REVARP_REPLY);

#ifdef BROKEN_BPF
	ep->ether_type = ETHERTYPE_REVARP;
#endif
	bcopy((char *)&ap->arp_sha, (char *)&ep->ether_dhost, 6);
	bcopy((char *)ii->ii_eaddr, (char *)&ep->ether_shost, 6);
	bcopy((char *)ii->ii_eaddr, (char *)&ap->arp_sha, 6);

	bcopy((char *)&ipaddr, (char *)ap->arp_tpa, 4);
	/* Target hardware is unchanged. */
	bcopy((char *)&ii->ii_ipaddr, (char *)ap->arp_spa, 4);

	/* Zero possible garbage after packet. */
	bzero((char *)ep + (sizeof(*ep) + sizeof(*ap)),
			len - (sizeof(*ep) + sizeof(*ap)));
	n = write(ii->ii_fd, (char *)ep, len);
	if (n != len)
		syslog(LOG_ERR, "write: only %d of %d bytes written", n, len);
	if (verbose)
		syslog(LOG_INFO, "%s %s at %s REPLIED", ii->ii_ifname,
		    eatoa(ap->arp_tha),
		    intoa(ntohl(ipaddr)));
}

/*
 * Get the netmask of an IP address.  This routine is used if
 * SIOCGIFNETMASK doesn't work.
 */
u_long
ipaddrtonetmask(addr)
	u_long addr;
{
	addr = ntohl(addr);
	if (IN_CLASSA(addr))
		return htonl(IN_CLASSA_NET);
	if (IN_CLASSB(addr))
		return htonl(IN_CLASSB_NET);
	if (IN_CLASSC(addr))
		return htonl(IN_CLASSC_NET);
	syslog(LOG_DEBUG, "unknown IP address class: %08X", addr);
	return htonl(0xffffffff);
}

/*
 * A faster replacement for inet_ntoa().
 */
char *
intoa(addr)
	u_long addr;
{
	register char *cp;
	register u_int byte;
	register int n;
	static char buf[sizeof(".xxx.xxx.xxx.xxx")];

	cp = &buf[sizeof buf];
	*--cp = '\0';

	n = 4;
	do {
		byte = addr & 0xff;
		*--cp = byte % 10 + '0';
		byte /= 10;
		if (byte > 0) {
			*--cp = byte % 10 + '0';
			byte /= 10;
			if (byte > 0)
				*--cp = byte + '0';
		}
		*--cp = '.';
		addr >>= 8;
	} while (--n > 0);

	return cp + 1;
}

char *
eatoa(ea)
	register u_char *ea;
{
	static char buf[sizeof("xx:xx:xx:xx:xx:xx")];

	(void)sprintf(buf, "%x:%x:%x:%x:%x:%x",
	    ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]);
	return (buf);
}
