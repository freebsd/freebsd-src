/*
 * (C)opyright 1992-1998 Darren Reed. (from tcplog)
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 */

#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stropts.h>

#include <sys/pfmod.h>
#include <sys/bufmod.h>
#include <sys/dlpi.h>

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

#include "ip_compat.h"

#ifndef	lint
static	char	snitid[] = "%W% %G% (C)1995 Darren Reed";
#endif

#define BUFSPACE	32768

static	int	solfd;

/*
 * Be careful to only include those defined in the flags option for the
 * interface are included in the header size.
 */
static	int	timeout;


void	nullbell()
{
	return 0;
}


int	ack_recv(ep)
char	*ep;
{
	struct	tcpiphdr	tip;
	tcphdr_t	*tcp;
	ip_t	*ip;

	ip = (ip_t *)&tip;
	tcp = (tcphdr_t *)(ip + 1);
	bcopy(ep, (char *)ip, sizeof(*ip));
	bcopy(ep + (ip->ip_hl << 2), (char *)tcp, sizeof(*tcp));

	if (ip->ip_off & 0x1fff != 0)
		return 0;
	if (0 == detect(ip, tcp))
		return 1;
	return 0;
}


int	readloop(fd, port, dst)
int 	fd, port;
struct	in_addr dst;
{
	static	u_char	buf[BUFSPACE];
	register u_char	*bp, *cp, *bufend;
	register struct	sb_hdr	*hp;
	register int	cc;
	struct	strbuf	dbuf;
	ether_header_t	eh;
	time_t	now = time(NULL);
	int	flags = 0, i, done = 0;

	fd = solfd;
	dbuf.len = 0;
	dbuf.buf = buf;
	dbuf.maxlen = sizeof(buf);
	/*
	 * no control data buffer...
	 */
	while (1) {
		(void) signal(SIGALRM, nullbell);
		alarm(1);
		i = getmsg(fd, NULL, &dbuf, &flags);
		alarm(0);
		(void) signal(SIGALRM, nullbell);

		cc = dbuf.len;
		if ((time(NULL) - now) > timeout)
			return done;
		if (i == -1)
			if (errno == EINTR)
				continue;
			else
				break;
		bp = buf;
		bufend = buf + cc;
		/*
		 * loop through each snapshot in the chunk
		 */
		while (bp < bufend) {
			/*
			 * get past bufmod header
			 */
			hp = (struct sb_hdr *)bp;
			cp = (u_char *)((char *)bp + sizeof(*hp));
			bcopy(cp, (char *)&eh, sizeof(eh));
			/*
			 * next snapshot
			 */
			bp += hp->sbh_totlen;
			cc -= hp->sbh_totlen;

			if (eh.ether_type != ETHERTYPE_IP)
				continue;

			cp += sizeof(eh);
			done += ack_recv(cp);
		}
		alarm(1);
	}
	perror("getmsg");
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
	char	devname[16], *s, buf[256];
	int	i, offset, fd, snaplen= 58, chunksize = BUFSPACE;

	(void) sprintf(devname, "/dev/%s", device);

	s = devname + 5;
	while (*s && !isdigit(*s))
		s++;
	if (!*s)
	    {
		fprintf(stderr, "bad device name %s\n", devname);
		exit(-1);
	    }
	i = atoi(s);
	*s = '\0';
	/*
	 * For reading
	 */
	if ((fd = open(devname, O_RDWR)) < 0)
	    {
		fprintf(stderr, "O_RDWR(0) ");
		perror(devname);
		exit(-1);
	    }
	if (dlattachreq(fd, i) == -1 || dlokack(fd, buf) == -1)
	    {
		fprintf(stderr, "DLPI error\n");
		exit(-1);
	    }
	dlbindreq(fd, ETHERTYPE_IP, 0, DL_CLDLS, 0, 0);
	dlbindack(fd, buf);
	/*
	 * read full headers
	 */
	if (strioctl(fd, DLIOCRAW, -1, 0, NULL) == -1)
	    {
		fprintf(stderr, "DLIOCRAW error\n");
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
	pfil.Pf_FilterLen = (fwp - &pfil.Pf_Filter[0]);
	/*
	 * put filter in place.
	 */

	if (ioctl(fd, I_PUSH, "pfmod") == -1)
	    {
		perror("ioctl: I_PUSH pf");
		exit(1);
	    }
	if (strioctl(fd, PFIOCSETF, -1, sizeof(pfil), (char *)&pfil) == -1)
	    {
		perror("ioctl: PFIOCSETF");
		exit(1);
	    }

	/*
	 * arrange to get messages from the NIT STREAM and use NIT_BUF option
	 */
	if (ioctl(fd, I_PUSH, "bufmod") == -1)
	    {
		perror("ioctl: I_PUSH bufmod");
		exit(1);
	    }
	i = 128;
	strioctl(fd, SBIOCSSNAP, -1, sizeof(i), (char *)&i);
	/*
	 * set the timeout
	 */
	to.tv_sec = 1;
	to.tv_usec = 0;
	if (strioctl(fd, SBIOCSTIME, -1, sizeof(to), (char *)&to) == -1)
	    {
		perror("strioctl(SBIOCSTIME)");
		exit(-1);
	    }
	/*
	 * flush read queue
	 */
	if (ioctl(fd, I_FLUSH, FLUSHR) == -1)
	    {
		perror("I_FLUSHR");
		exit(-1);
	    }
	timeout = tout;
	solfd = fd;
	return fd;
}
