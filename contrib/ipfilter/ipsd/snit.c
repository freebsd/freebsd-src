/*
 * (C)opyright 1992-1998 Darren Reed. (from tcplog)
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 *   The author of this software makes no garuntee about the
 * performance of this package or its suitability to fulfill any purpose.
 *
 */

#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <net/nit.h>
#include <sys/fcntlcom.h>
#include <sys/dir.h>
#include <net/nit_if.h>
#include <net/nit_pf.h>
#include <net/nit_buf.h>
#include <net/packetfilt.h>
#include <sys/stropts.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#ifndef	lint
static	char	snitid[] = "@(#)snit.c	1.2 12/3/95 (C)1995 Darren Reed";
#endif

#define BUFSPACE	32768

/*
 * Be careful to only include those defined in the flags option for the
 * interface are included in the header size.
 */
#define BUFHDR_SIZE  (sizeof(struct nit_bufhdr))
#define NIT_HDRSIZE  (BUFHDR_SIZE)

static	int	timeout;


int	ack_recv(ep)
char	*ep;
{
	struct	tcpiphdr	tip;
	struct	tcphdr	*tcp;
	struct	ip	*ip;

	ip = (struct ip *)&tip;
	tcp = (struct tcphdr *)(ip + 1);
	bcopy(ep + 14, (char *)ip, sizeof(*ip));
	bcopy(ep + 14 + (ip->ip_hl << 2), (char *)tcp, sizeof(*tcp));
	if (ip->ip_off & 0x1fff != 0)
		return 0;
	if (0 == detect(ip, tcp))
		return 1;
	return 0;
}


int	readloop(fd, dst)
int 	fd;
struct	in_addr dst;
{
	static	u_char	buf[BUFSPACE];
	register u_char	*bp, *cp, *bufend;
	register struct	nit_bufhdr	*hp;
	register int	cc;
	time_t	now = time(NULL);
	int	done = 0;

	while ((cc = read(fd, buf, BUFSPACE-1)) >= 0) {
		if (!cc)
			if ((time(NULL) - now) > timeout)
				return done;
			else
				continue;
		bp = buf;
		bufend = buf + cc;
		/*
		 * loop through each snapshot in the chunk
		 */
		while (bp < bufend) {
			cp = (u_char *)((char *)bp + NIT_HDRSIZE);
			/*
			 * get past NIT buffer
			 */
			hp = (struct nit_bufhdr *)bp;
			/*
			 * next snapshot
			 */
			bp += hp->nhb_totlen;
			done += ack_recv(cp);
		}
		return done;
	}
	perror("read");
	exit(-1);
}

int	initdevice(device, tout)
char	*device;
int	tout;
{
	struct	strioctl si;
	struct	timeval to;
	struct	ifreq ifr;
	struct	packetfilt pfil;
	u_long	if_flags;
	u_short	*fwp = pfil.Pf_Filter;
	int	ret, offset, fd, snaplen= 76, chunksize = BUFSPACE;

	if ((fd = open("/dev/nit", O_RDWR)) < 0)
	    {
		perror("/dev/nit");
		exit(-1);
	    }

	/*
	 * Create some filter rules for our TCP watcher. We only want ethernet
	 * pacets which are IP protocol and only the TCP packets from IP.
	 */
	offset = 6;
	*fwp++ = ENF_PUSHWORD + offset;
	*fwp++ = ENF_PUSHLIT | ENF_CAND;
	*fwp++ = htons(ETHERTYPE_IP);
	*fwp++ = ENF_PUSHWORD + sizeof(struct ether_header)/sizeof(short)+4;
	*fwp++ = ENF_PUSHLIT | ENF_AND;
	*fwp++ = htons(0x00ff);
	*fwp++ = ENF_PUSHLIT | ENF_COR;
	*fwp++ = htons(IPPROTO_TCP);
	*fwp++ = ENF_PUSHWORD + sizeof(struct ether_header)/sizeof(short)+4;
	*fwp++ = ENF_PUSHLIT | ENF_AND;
	*fwp++ = htons(0x00ff);
	*fwp++ = ENF_PUSHLIT | ENF_CAND;
	*fwp++ = htons(IPPROTO_UDP);
	pfil.Pf_FilterLen = fwp - &pfil.Pf_Filter[0];
	/*
	 * put filter in place.
	 */
	if (ioctl(fd, I_PUSH, "pf") == -1)
	    {
		perror("ioctl: I_PUSH pf");
		exit(1);
	    }
	if (ioctl(fd, NIOCSETF, &pfil) == -1)
	    {
		perror("ioctl: NIOCSETF");
		exit(1);
	    }
	/*
	 * arrange to get messages from the NIT STREAM and use NIT_BUF option
	 */
	ioctl(fd, I_SRDOPT, (char*)RMSGD);
	ioctl(fd, I_PUSH, "nbuf");
	/*
	 * set the timeout
	 */
	timeout = tout;
	si.ic_timout = 1;
	to.tv_sec = 1;
	to.tv_usec = 0;
	si.ic_cmd = NIOCSTIME;
	si.ic_len = sizeof(to);
	si.ic_dp = (char*)&to;
	if (ioctl(fd, I_STR, (char*)&si) == -1)
	    {
		perror("ioctl: NIT timeout");
		exit(-1);
	    }
	/*
	 * set the chunksize
	 */
	si.ic_cmd = NIOCSCHUNK;
	si.ic_len = sizeof(chunksize);
	si.ic_dp = (char*)&chunksize;
	if (ioctl(fd, I_STR, (char*)&si) == -1)
		perror("ioctl: NIT chunksize");
	if (ioctl(fd, NIOCGCHUNK, (char*)&chunksize) == -1)
	    {
		perror("ioctl: NIT chunksize");
		exit(-1);
	    }
	printf("NIT buffer size: %d\n", chunksize);

	/*
	 * request the interface
	 */
	strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
	ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = ' ';
	si.ic_cmd = NIOCBIND;
	si.ic_len = sizeof(ifr);
	si.ic_dp = (char*)&ifr;
	if (ioctl(fd, I_STR, (char*)&si) == -1)
	    {
		perror(ifr.ifr_name);
		exit(1);
	    }

	/*
	 * set the snapshot length
	 */
	si.ic_cmd = NIOCSSNAP;
	si.ic_len = sizeof(snaplen);
	si.ic_dp = (char*)&snaplen;
	if (ioctl(fd, I_STR, (char*)&si) == -1)
	    {
		perror("ioctl: NIT snaplen");
		exit(1);
	    }
	(void) ioctl(fd, I_FLUSH, (char*)FLUSHR);
	return fd;
}
