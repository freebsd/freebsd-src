/*	$FreeBSD$	*/

/*
 * (C)opyright 1995-1998 Darren Reed. (from tcplog)
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 */
#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#ifdef __NetBSD__
# include <paths.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#if BSD < 199103
#include <sys/fcntlcom.h>
#endif
#include <sys/dir.h>
#include <net/bpf.h>

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
static	char	sbpf[] = "@(#)sbpf.c	1.2 12/3/95 (C)1995 Darren Reed";
#endif

/*
(000) ldh      [12]
(001) jeq      #0x800	   jt 2	jf 5
(002) ldb      [23]
(003) jeq      #0x6	     jt 4	jf 5
(004) ret      #68
(005) ret      #0
*/
struct	bpf_insn filter[] = {
/* 0. */	{ BPF_LD|BPF_H|BPF_ABS,		0, 0, 12 },
/* 1. */	{ BPF_JMP|BPF_JEQ,		0, 3, 0x0800 },
/* 2. */	{ BPF_LD|BPF_B|BPF_ABS,		0, 0, 23 },
/* 3. */	{ BPF_JMP|BPF_JEQ,		0, 1, 0x06 },
/* 4. */	{ BPF_RET,			0, 0, 68 },
/* 5. */	{ BPF_RET,			0, 0, 0 }
};
/*
 * the code herein is dervied from libpcap.
 */
static	u_char	*buf = NULL;
static	u_int	bufsize = 32768, timeout = 1;


int	ack_recv(ep)
char	*ep;
{
	struct	tcpiphdr	tip;
	tcphdr_t	*tcp;
	ip_t	*ip;

	ip = (ip_t *)&tip;
	tcp = (tcphdr_t *)(ip + 1);
	bcopy(ep + 14, (char *)ip, sizeof(*ip));
	bcopy(ep + 14 + (ip->ip_hl << 2), (char *)tcp, sizeof(*tcp));
	if (ip->ip_p != IPPROTO_TCP && ip->ip_p != IPPROTO_UDP)
		return -1;
	if (ip->ip_p & 0x1fff != 0)
		return 0;
	if (0 == detect(ip, tcp))
		return 1;
	return 0;
}


int	readloop(fd, port, dst)
int 	fd, port;
struct	in_addr dst;
{
	register u_char	*bp, *cp, *bufend;
	register struct	bpf_hdr	*bh;
	register int	cc;
	time_t	in = time(NULL);
	int	done = 0;

	while ((cc = read(fd, buf, bufsize)) >= 0) {
		if (!cc && (time(NULL) - in) > timeout)
			return done;
		bp = buf;
		bufend = buf + cc;
		/*
		 * loop through each snapshot in the chunk
		 */
		while (bp < bufend) {
			bh = (struct bpf_hdr *)bp;
			cp = bp + bh->bh_hdrlen;
			done += ack_recv(cp);
			bp += BPF_WORDALIGN(bh->bh_caplen + bh->bh_hdrlen);
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
	struct	bpf_program prog;
	struct	bpf_version bv;
	struct	timeval to;
	struct	ifreq ifr;
#ifdef _PATH_BPF
	char 	*bpfname = _PATH_BPF;
	int	fd;

	if ((fd = open(bpfname, O_RDWR)) < 0)
	    {
		fprintf(stderr, "no bpf devices available as /dev/bpfxx\n");
		return -1;
	    }
#else
	char	bpfname[16];
	int	fd = -1, i;

	for (i = 0; i < 16; i++)
	    {
		(void) sprintf(bpfname, "/dev/bpf%d", i);
		if ((fd = open(bpfname, O_RDWR)) >= 0)
			break;
	    }
	if (i == 16)
	    {
		fprintf(stderr, "no bpf devices available as /dev/bpfxx\n");
		return -1;
	    }
#endif

	if (ioctl(fd, BIOCVERSION, (caddr_t)&bv) < 0)
	    {
		perror("BIOCVERSION");
		return -1;
	    }
	if (bv.bv_major != BPF_MAJOR_VERSION ||
	    bv.bv_minor < BPF_MINOR_VERSION)
	    {
		fprintf(stderr, "kernel bpf (v%d.%d) filter out of date:\n",
			bv.bv_major, bv.bv_minor);
		fprintf(stderr, "current version: %d.%d\n",
			BPF_MAJOR_VERSION, BPF_MINOR_VERSION);
		return -1;
	    }

	(void) strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
	if (ioctl(fd, BIOCSETIF, &ifr) == -1)
	    {
		fprintf(stderr, "%s(%d):", ifr.ifr_name, fd);
		perror("BIOCSETIF");
		exit(1);
	    }
	/*
	 * set the timeout
	 */
	timeout = tout;
	to.tv_sec = 1;
	to.tv_usec = 0;
	if (ioctl(fd, BIOCSRTIMEOUT, (caddr_t)&to) == -1)
	    {
		perror("BIOCSRTIMEOUT");
		exit(-1);
	    }
	/*
	 * get kernel buffer size
	 */
	if (ioctl(fd, BIOCSBLEN, &bufsize) == -1)
		perror("BIOCSBLEN");
	if (ioctl(fd, BIOCGBLEN, &bufsize) == -1)
	    {
		perror("BIOCGBLEN");
		exit(-1);
	    }
	printf("BPF buffer size: %d\n", bufsize);
	buf = (u_char*)malloc(bufsize);

	prog.bf_len = sizeof(filter) / sizeof(struct bpf_insn);
	prog.bf_insns = filter;
	if (ioctl(fd, BIOCSETF, (caddr_t)&prog) == -1)
	    {
		perror("BIOCSETF");
		exit(-1);
	    }
	(void) ioctl(fd, BIOCFLUSH, 0);
	return fd;
}
