/*
 * Rdisc (this program) was developed by Sun Microsystems, Inc. and is
 * provided for unrestricted use provided that this legend is included on
 * all tape media and as a part of the software program in whole or part.
 * Users may copy or modify Rdisc without charge, and they may freely
 * distribute it.
 *
 * RDISC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Rdisc is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY RDISC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/cdefs.h>

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * TBD
 *	Use 255.255.255.255 for broadcasts - not the interface broadcast
 *	address.
 */

#ifdef lint
#define ALLIGN(ptr)	(ptr ? 0 : 0)
#else
#define ALLIGN(ptr)	(ptr)
#endif


#define ICMP_ROUTER_ADVERTISEMENT	9
#define ICMP_ROUTER_SOLICITATION	10

#define ALL_HOSTS_ADDRESS		"224.0.0.1"
#define ALL_ROUTERS_ADDRESS		"224.0.0.2"

#define MAXIFS 32

/* Router constants */
#define	MAX_INITIAL_ADVERT_INTERVAL	16
#define	MAX_INITIAL_ADVERTISEMENTS  	3
#define	MAX_RESPONSE_DELAY		2	/* Not used */

/* Host constants */
#define MAX_SOLICITATIONS 		3
#define SOLICITATION_INTERVAL 		3
#define MAX_SOLICITATION_DELAY		1	/* Not used */

#define IGNORE_PREFERENCE	0x80000000	/* Maximum negative */

#define MAX_ADV_INT 600

/* Statics */
int num_interfaces;
struct interface {
	struct in_addr 	address;	/* Used to identify the interface */
	struct in_addr	localaddr;	/* Actual address if the interface */
	int 		preference;
	int		flags;
	struct in_addr	bcastaddr;
	struct in_addr	remoteaddr;
	struct in_addr	netmask;
};
struct interface *interfaces;
int interfaces_size;			/* Number of elements in interfaces */


#define	MAXPACKET	4096	/* max packet size */
u_char	packet[MAXPACKET];

char usage[] =
"Usage:	rdisc [-s] [-v] [-f] [-a] [send_address] [receive_address]\n\
        rdisc -r [-v] [-p <preference>] [-T <secs>] \n\
		[send_address] [receive_address]\n";


int s;			/* Socket file descriptor */
struct sockaddr_in whereto;/* Address to send to */

/* Common variables */
int verbose = 0;
int debug = 0;
int trace = 0;
int solicit = 0;
int responder;
int ntransmitted = 0;
int nreceived = 0;
int forever = 0;	/* Never give up on host. If 0 defer fork until
			 * first response.
			 */

/* Router variables */
int max_adv_int = MAX_ADV_INT;
int min_adv_int;
int lifetime;
int initial_advert_interval = MAX_INITIAL_ADVERT_INTERVAL;
int initial_advertisements = MAX_INITIAL_ADVERTISEMENTS;
int preference = 0;		/* Setable with -p option */

/* Host variables */
int max_solicitations = MAX_SOLICITATIONS;
unsigned int solicitation_interval = SOLICITATION_INTERVAL;
int best_preference = 1;  	/* Set to record only the router(s) with the
				   best preference in the kernel. Not set
				   puts all routes in the kernel. */

/* Prototypes */
void	do_fork		__P((void));
int	main		__P((int, char **));
void	timer		__P((int));
void	solicitor	__P((struct sockaddr_in *));
void	advertise	__P((struct sockaddr_in *));
char *	pr_type		__P((int));
char *	pr_name		__P((struct in_addr));
void	pr_pack		__P((char *, int, struct sockaddr_in *));
int	in_cksum	__P((u_short *, int));
void	finish		__P((int));
int	isbroadcast	__P((struct sockaddr_in *));
int	ismulticast	__P((struct sockaddr_in *));
int	sendbcast	__P((int, char *, int));
int	sendbcastif	__P((int, char *, int, struct interface *));
int	sendmcast	__P((int, char *, int, struct sockaddr_in *));
int	sendmcastif	__P((int, char *, int, struct sockaddr_in *, struct interface *));
void	init		__P((void));
void	initifs		__P((int));
int	support_multicast	__P((void));
int	is_directly_connected	__P((struct in_addr));
struct table * find_router	__P((struct in_addr));
int	max_preference	__P((void));
void	age_table	__P((int));
void	record_router	__P((struct in_addr, long, int));
void	add_route	__P((struct in_addr));
void	del_route	__P((struct in_addr));
void	rtioctl		__P((struct in_addr, int));
void	initlog		__P((void));
void	logerr		__P((char *fmt, ...));
void	logtrace	__P((char *fmt, ...));
void	logdebug	__P((char *fmt, ...));
void	logperror	__P((char *str));
void	prusage		__P((void));
int	join		__P((int sock, struct sockaddr_in *sin));


void
prusage()
{
	(void) fprintf(stderr, usage);
	exit(1);
}

void
do_fork()
{
	int t;
	long pid;

	if (trace)
		return;

	if ((pid = fork()))
		exit(0);

	for (t = 0; t < 20; t++)
		if (t != s)
			(void) close(t);

	(void) setsid ();

	initlog();
}

/*
 * 			M A I N
 */
char    *sendaddress, *recvaddress;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct sigaction sa;
	struct sockaddr_in from;
	struct sockaddr_in *to = &whereto;
	struct sockaddr_in joinaddr;
	int val;
	int c;

	min_adv_int =( max_adv_int * 3 / 4);
	lifetime = (3*max_adv_int);

	while ((c = getopt(argc, argv, "dtvsrabfT:p:")) != EOF) {
		switch(c) {
		case 'd':
			debug = 1;
			break;
		case 't':
			trace = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 's':
			solicit = 1;
			break;
		case 'r':
			responder = 1;
			break;
		case 'a':
			best_preference = 0;
			break;
		case 'b':
			best_preference = 1;
			break;
		case 'f':
			forever = 1;
			break;
		case 'T':
			val = strtol(optarg, (char **)NULL, 0);
			if (val < 4 || val > 1800) {
				(void) fprintf(stderr,
			          "Bad Max Advertizement Interval\n");
				exit(1);
			}
			max_adv_int = val;
			min_adv_int =( max_adv_int * 3 / 4);
			lifetime = (3*max_adv_int);
			break;
		case 'p':
			val = strtol(optarg, (char **)NULL, 0);
			preference = val;
			break;
		default:
			prusage();
			/* NOTREACHED*/
		}
	}
	argc -= optind;
	argv += optind;

	if( argc < 1)  {
		if (support_multicast()) {
			if (responder)
				sendaddress = ALL_HOSTS_ADDRESS;
			else
				sendaddress = ALL_ROUTERS_ADDRESS;
		} else
			sendaddress = "255.255.255.255";
	} else {
		sendaddress = argv[0];
		argc--; argv++;
	}

	if (argc < 1) {
		if (support_multicast()) {
			if (responder)
				recvaddress = ALL_ROUTERS_ADDRESS;
			else
				recvaddress = ALL_HOSTS_ADDRESS;
		} else
			recvaddress = "255.255.255.255";
	} else {
		recvaddress = argv[0];
		argc--; argv++;
	}
	if (argc != 0) {
		(void) fprintf(stderr, "Extra paramaters\n");
		prusage();
		/* NOTREACHED */
	}

	if (solicit && responder) {
		prusage();
		/* NOTREACHED */
	}

	if (!(solicit && !forever)) {
		do_fork();
/*
 * Added the next line to stop forking a second time
 * Fraser Gardiner - Sun Microsystems Australia
 */
		forever = 1;
	}

	bzero( (char *)&whereto, sizeof(struct sockaddr_in) );
	to->sin_family = AF_INET;
	to->sin_addr.s_addr = inet_addr(sendaddress);

	bzero( (char *)&joinaddr, sizeof(struct sockaddr_in) );
	joinaddr.sin_family = AF_INET;
	joinaddr.sin_addr.s_addr = inet_addr(recvaddress);

	if (responder) {
		/* TBD fix this to be more random */
		srandom((int)gethostid()+(int)time((time_t *)NULL));
	}

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		logperror("socket");
		exit(5);
	}

	setvbuf( stdout, NULL, _IOLBF, 0 );

	(void) signal( SIGINT, finish );
	(void) signal( SIGTERM, finish );
	(void) signal( SIGHUP, initifs );

	init();
	if (join(s, &joinaddr) < 0) {
		logerr("Failed joining addresses\n");
		exit (2);
	}


	/*
	 * Make sure that this signal actually interrupts (rather than
	 * restarts) the recvfrom call below.
	 */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = timer;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;	/* note: no SA_RESTART */
	(void) sigaction(SIGALRM, &sa, (struct sigaction *)NULL);
	timer(0);	/* start things going */

	for (;;) {
		int len = sizeof (packet);
		int fromlen = sizeof (from);
		int cc;

		if ( (cc=recvfrom(s, (char *)packet, len, 0,
				  (struct sockaddr *)&from, &fromlen)) < 0) {
			if( errno == EINTR )
				continue;
			logperror("recvfrom");
			continue;
		}
		pr_pack( (char *)packet, cc, &from );
	}
	/*NOTREACHED*/
}

#define TIMER_INTERVAL 	3
#define GETIFCONF_TIMER	30

int left_until_advertise;

/* Called every TIMER_INTERVAL */
void timer(sig)
     int sig;
{
	static int left_until_getifconf;
	static int left_until_solicit;

	left_until_getifconf -= TIMER_INTERVAL;
	left_until_advertise -= TIMER_INTERVAL;
	left_until_solicit -= TIMER_INTERVAL;

	if (left_until_getifconf < 0) {
		initifs(0);
		left_until_getifconf = GETIFCONF_TIMER;
	}
	if (responder && left_until_advertise <= 0) {
		ntransmitted++;
		advertise(&whereto);
		if (ntransmitted < initial_advertisements)
			left_until_advertise = initial_advert_interval;
		else
			left_until_advertise = min_adv_int +
				((max_adv_int - min_adv_int) *
				 (random() % 1000)/1000);
	} else if (solicit && left_until_solicit <= 0) {
		ntransmitted++;
		solicitor(&whereto);
		if (ntransmitted < max_solicitations)
			left_until_solicit = solicitation_interval;
		else {
			solicit = 0;
			if (!forever && nreceived == 0)
				exit(5);
		}
	}
	age_table(TIMER_INTERVAL);
	(void) alarm(TIMER_INTERVAL);
}

/*
 * 			S O L I C I T O R
 *
 * Compose and transmit an ICMP ROUTER SOLICITATION REQUEST packet.
 * The IP packet will be added on by the kernel.
 */
void
solicitor(sin)
	struct sockaddr_in *sin;
{
	static u_char outpack[MAXPACKET];
	register struct icmp *icp = (struct icmp *) ALLIGN(outpack);
	int packetlen, i;

	if (verbose) {
		logtrace("Sending solicitation to %s\n",
			 pr_name(sin->sin_addr));
	}
	icp->icmp_type = ICMP_ROUTER_SOLICITATION;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_void = 0; /* Reserved */
	packetlen = 8;

	/* Compute ICMP checksum here */
	icp->icmp_cksum = in_cksum( (u_short *)icp, packetlen );

	if (isbroadcast(sin))
		i = sendbcast(s, (char *)outpack, packetlen);
	else if (ismulticast(sin))
		i = sendmcast( s, (char *)outpack, packetlen, sin);
	else
		i = sendto( s, (char *)outpack, packetlen, 0,
			   (struct sockaddr *)sin, sizeof(struct sockaddr));

	if( i < 0 || i != packetlen )  {
		if( i<0 ) {
		    logperror("sendto");
		}
		logerr("wrote %s %d chars, ret=%d\n",
			sendaddress, packetlen, i );
	}
}

/*
 * 			A V E R T I S E
 *
 * Compose and transmit an ICMP ROUTER ADVERTISEMENT packet.
 * The IP packet will be added on by the kernel.
 */
void
advertise(sin)
	struct sockaddr_in *sin;
{
	static u_char outpack[MAXPACKET];
	register struct icmp *rap = (struct icmp *) ALLIGN(outpack);
	struct icmp_ra_addr *ap;
	int packetlen, i, cc;

	if (verbose) {
		logtrace("Sending advertisement to %s\n",
			 pr_name(sin->sin_addr));
	}

	for (i = 0; i < num_interfaces; i++) {
		rap->icmp_type = ICMP_ROUTER_ADVERTISEMENT;
		rap->icmp_code = 0;
		rap->icmp_cksum = 0;
		rap->icmp_num_addrs = 0;
		rap->icmp_wpa = 2;
		rap->icmp_lifetime = lifetime;
		packetlen = 8;

		/*
		 * TODO handle multiple logical interfaces per
		 * physical interface. (increment with rap->icmp_wpa * 4 for
		 * each address.)
		 */
		ap = (struct icmp_ra_addr *)ALLIGN(outpack + ICMP_MINLEN);
		ap->ira_addr = interfaces[i].localaddr.s_addr;
		ap->ira_preference = interfaces[i].preference;
		packetlen += rap->icmp_wpa * 4;
		rap->icmp_num_addrs++;


		/* Compute ICMP checksum here */
		rap->icmp_cksum = in_cksum( (u_short *)rap, packetlen );

		if (isbroadcast(sin))
			cc = sendbcastif(s, (char *)outpack, packetlen,
					&interfaces[i]);
		else if (ismulticast(sin))
			cc = sendmcastif( s, (char *)outpack, packetlen, sin,
					&interfaces[i]);
		else {
			struct interface *ifp = &interfaces[i];
			/*
			 * Verify that the interface matches the destination
			 * address.
			 */
			if ((sin->sin_addr.s_addr & ifp->netmask.s_addr) ==
			    (ifp->address.s_addr & ifp->netmask.s_addr)) {
				if (debug) {
					logdebug("Unicast to %s ",
						 pr_name(sin->sin_addr));
					logdebug("on interface %s\n",
						 pr_name(ifp->address));
				}
				cc = sendto( s, (char *)outpack, packetlen, 0,
					    (struct sockaddr *)sin,
					    sizeof(struct sockaddr));
			} else
				cc = packetlen;
		}
		if( cc < 0 || cc != packetlen )  {
			if (cc < 0) {
				logperror("sendto");
			} else {
				logerr("wrote %s %d chars, ret=%d\n",
				       sendaddress, packetlen, cc );
			}
		}
	}
}

/*
 * 			P R _ T Y P E
 *
 * Convert an ICMP "type" field to a printable string.
 */
char *
pr_type( t )
register int t;
{
	static char *ttab[] = {
		"Echo Reply",
		"ICMP 1",
		"ICMP 2",
		"Dest Unreachable",
		"Source Quench",
		"Redirect",
		"ICMP 6",
		"ICMP 7",
		"Echo",
		"Router Advertise",
		"Router Solicitation",
		"Time Exceeded",
		"Parameter Problem",
		"Timestamp",
		"Timestamp Reply",
		"Info Request",
		"Info Reply",
		"Netmask Request",
		"Netmask Reply"
	};

	if ( t < 0 || t > 16 )
		return("OUT-OF-RANGE");

	return(ttab[t]);
}

/*
 *			P R _ N A M E
 *
 * Return a string name for the given IP address.
 */
char *pr_name(addr)
    struct in_addr addr;
{
	char *inet_ntoa();
	struct hostent *phe;
	static char buf[256];

	phe = gethostbyaddr((char *)&addr.s_addr, 4, AF_INET);
	if (phe == NULL)
		return( inet_ntoa(addr));
	(void) sprintf(buf, "%s (%s)", phe->h_name, inet_ntoa(addr));
	return(buf);
}

/*
 *			P R _ P A C K
 *
 * Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
void
pr_pack( buf, cc, from )
char *buf;
int cc;
struct sockaddr_in *from;
{
	struct ip *ip;
	register struct icmp *icp;
	register int i;
	int hlen;

	ip = (struct ip *) ALLIGN(buf);
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (verbose)
			logtrace("packet too short (%d bytes) from %s\n", cc,
				 pr_name(from->sin_addr));
		return;
	}
	cc -= hlen;
	icp = (struct icmp *)ALLIGN(buf + hlen);
	if (ip->ip_p == 0) {
		/*
		 * Assume that we are running on a pre-4.3BSD system
		 * such as SunOS before 4.0
		 */
		 icp = (struct icmp *)ALLIGN(buf);
	}
	switch (icp->icmp_type) {
	case ICMP_ROUTER_ADVERTISEMENT: {
		struct icmp *rap = (struct icmp *)ALLIGN(icp);
		struct icmp_ra_addr *ap;

		if (responder)
			break;

		/* TBD verify that the link is multicast or broadcast */
		/* XXX Find out the link it came in over? */
#ifdef notdef
		if (debug) {
			logdebug("ROUTER_ADVERTISEMENT: \n");
			pr_hex(buf+hlen, cc);
		}
#endif notdef
		if (in_cksum((u_short *)ALLIGN(buf+hlen), cc)) {
			if (verbose)
				logtrace("ICMP %s from %s: Bad checksum\n",
					 pr_type((int)rap->icmp_type),
					 pr_name(from->sin_addr));
			return;
		}
		if (rap->icmp_code != 0) {
			if (verbose)
				logtrace("ICMP %s from %s: Code = %d\n",
					 pr_type((int)rap->icmp_type),
					 pr_name(from->sin_addr),
					 rap->icmp_code);
			return;
		}
		if (rap->icmp_num_addrs < 1) {
			if (verbose)
				logtrace("ICMP %s from %s: No addresses\n",
					 pr_type((int)rap->icmp_type),
					 pr_name(from->sin_addr));
			return;
		}
		if (rap->icmp_wpa < 2) {
			if (verbose)
				logtrace("ICMP %s from %s: Words/addr = %d\n",
					 pr_type((int)rap->icmp_type),
					 pr_name(from->sin_addr),
					 rap->icmp_wpa);
			return;
		}
		if ((unsigned)cc <
		    ICMP_MINLEN + rap->icmp_num_addrs * rap->icmp_wpa * 4) {
			if (verbose)
				logtrace("ICMP %s from %s: Too short %d, %d\n",
					      pr_type((int)rap->icmp_type),
					      pr_name(from->sin_addr),
					      cc,
					      ICMP_MINLEN +
					      rap->icmp_num_addrs * rap->icmp_wpa * 4);
			return;
		}
		rap->icmp_lifetime = ntohs(rap->icmp_lifetime);
		if (rap->icmp_lifetime < 4) {
			if (verbose)
				logtrace("ICMP %s from %s: Lifetime = %d\n",
					      pr_type((int)rap->icmp_type),
					      pr_name(from->sin_addr),
					      rap->icmp_lifetime);
			return;
		}
		if (verbose)
			logtrace("ICMP %s from %s, lifetime %d\n",
				      pr_type((int)rap->icmp_type),
				      pr_name(from->sin_addr),
				      rap->icmp_lifetime);

		/* Check that at least one router address is a neighboor
		 * on the arriving link.
		 */
		for (i = 0; (unsigned)i < rap->icmp_num_addrs; i++) {
			struct in_addr ina;
			ap = (struct icmp_ra_addr *)
				ALLIGN(buf + hlen + ICMP_MINLEN +
				       i * rap->icmp_wpa * 4);
			ina.s_addr = ap->ira_addr;
			if (verbose)
				logtrace("\taddress %s, preference 0x%x\n",
					      pr_name(ina),
					      ntohl(ap->ira_preference));
			if (!responder) {
				if (is_directly_connected(ina))
					record_router(ina,
						      (long)ntohl(ap->ira_preference),
						      rap->icmp_lifetime);
			}
		}
		nreceived++;
		if (!forever) {
			do_fork();
			forever = 1;
/*
 * The next line was added so that the alarm is set for the new procces
 * Fraser Gardiner Sun Microsystems Australia
 */
			(void) alarm(TIMER_INTERVAL);
		}
		break;
	}

	case ICMP_ROUTER_SOLICITATION: {
		struct sockaddr_in sin;

		if (!responder)
			break;

		/* TBD verify that the link is multicast or broadcast */
		/* XXX Find out the link it came in over? */
#ifdef notdef
		if (debug) {
			logdebug("ROUTER_SOLICITATION: \n");
			pr_hex(buf+hlen, cc);
		}
#endif notdef
		if (in_cksum((u_short *)ALLIGN(buf+hlen), cc)) {
			if (verbose)
				logtrace("ICMP %s from %s: Bad checksum\n",
					      pr_type((int)icp->icmp_type),
					      pr_name(from->sin_addr));
			return;
		}
		if (icp->icmp_code != 0) {
			if (verbose)
				logtrace("ICMP %s from %s: Code = %d\n",
					      pr_type((int)icp->icmp_type),
					      pr_name(from->sin_addr),
					      icp->icmp_code);
			return;
		}

		if (cc < ICMP_MINLEN) {
			if (verbose)
				logtrace("ICMP %s from %s: Too short %d, %d\n",
					      pr_type((int)icp->icmp_type),
					      pr_name(from->sin_addr),
					      cc,
					      ICMP_MINLEN);
			return;
		}

		if (verbose)
			logtrace("ICMP %s from %s\n",
				      pr_type((int)icp->icmp_type),
				      pr_name(from->sin_addr));

 		if (!responder)
			break;

		/* Check that ip_src is either a neighboor
		 * on the arriving link or 0.
		 */
		sin.sin_family = AF_INET;
		if (ip->ip_src.s_addr == 0) {
			/* If it was sent to the broadcast address we respond
			 * to the broadcast address.
			 */
			if (IN_CLASSD(ip->ip_dst.s_addr))
				sin.sin_addr.s_addr = INADDR_ALLHOSTS_GROUP;
			else
				sin.sin_addr.s_addr = INADDR_BROADCAST;
			/* Restart the timer when we broadcast */
			left_until_advertise = min_adv_int +
				((max_adv_int - min_adv_int)
				 * (random() % 1000)/1000);
		}
		else {
			if (!is_directly_connected(ip->ip_src)) {
				if (verbose)
					logtrace("ICMP %s from %s: source not directly connected\n",
						      pr_type((int)icp->icmp_type),
						      pr_name(from->sin_addr));
				break;
			}
			sin.sin_addr.s_addr = ip->ip_src.s_addr;
		}
		nreceived++;
		ntransmitted++;
		advertise(&sin);
		break;
	}
	} /* end switch */
}


/*
 *			I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
int
in_cksum(addr, len)
u_short *addr;
int len;
{
	register int nleft = len;
	register u_short *w = addr;
	register u_short answer;
	u_short odd_byte = 0;
	register int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while( nleft > 1 )  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if( nleft == 1 ) {
		*(u_char *)(&odd_byte) = *(u_char *)w;
		sum += odd_byte;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}

/*
 *			F I N I S H
 *
 * Print out statistics, and give up.
 * Heavily buffered STDIO is used here, so that all the statistics
 * will be written with 1 sys-write call.  This is nice when more
 * than one copy of the program is running on a terminal;  it prevents
 * the statistics output from becomming intermingled.
 */
void
finish(sig)
	int sig;
{
	if (responder) {
		int i;

		/* Send out a packet with a preference so that all
		 * hosts will know that we are dead.
		 */
		logerr("terminated\n");
		for (i = 0; i < num_interfaces; i++)
			interfaces[i].preference = IGNORE_PREFERENCE;
		ntransmitted++;
		advertise(&whereto);
	}
	logtrace("\n----%s rdisc Statistics----\n", sendaddress );
	logtrace("%d packets transmitted, ", ntransmitted );
	logtrace("%d packets received, ", nreceived );
	logtrace("\n");
	(void) fflush(stdout);
	exit(0);
}

#include <ctype.h>

#ifdef notdef
pr_hex(data, len)
int len;
unsigned char *data;
{
	FILE *out;

	out = stdout;

	while (len) {
		register int i;
		char charstring[17];

		(void)strcpy(charstring,"                "); /* 16 spaces */
		for (i = 0; i < 16; i++) {
			/* output the bytes one at a time,
			 * not going pas "len" bytes
			 */
			if (len) {
				char ch = *data & 0x7f; /* strip parity */
				if (!isprint((u_char)ch))
					ch = ' '; /* ensure printable */
				charstring[i] = ch;
				(void) fprintf(out,"%02x ",*data++);
				len--;
			} else
				(void) fprintf(out,"   ");
			if (i==7)
				(void) fprintf(out,"   ");
		}

		(void) fprintf(out,"    *%s*\n",charstring);
	}
}
#endif notdef

int
isbroadcast(sin)
	struct sockaddr_in *sin;
{
	return (sin->sin_addr.s_addr == INADDR_BROADCAST);
}

int
ismulticast(sin)
	struct sockaddr_in *sin;
{
	return (IN_CLASSD(ntohl(sin->sin_addr.s_addr)));
}

/* From libc/rpc/pmap_rmt.c */

int
sendbcast(s, packet, packetlen)
	int s;
	char *packet;
	int packetlen;
{
	int i, cc;

	for (i = 0; i < num_interfaces; i++) {
		if ((interfaces[i].flags & IFF_BROADCAST) == 0)
			continue;
		cc = sendbcastif(s, packet, packetlen, &interfaces[i]);
		if (cc!= packetlen) {
			return (cc);
		}
	}
	return (packetlen);
}

int
sendbcastif(s, packet, packetlen, ifp)
	int s;
	char *packet;
	int packetlen;
	struct interface *ifp;
{
	int cc;
	struct sockaddr_in baddr;

	baddr.sin_family = AF_INET;

	if ((ifp->flags & IFF_BROADCAST) == 0)
		return (packetlen);

	baddr.sin_addr = ifp->bcastaddr;
	if (debug)
		logdebug("Broadcast to %s\n",
			 pr_name(baddr.sin_addr));
	cc = sendto(s, packet, packetlen, 0,
		    (struct sockaddr *)&baddr, sizeof (struct sockaddr));
	if (cc!= packetlen) {
		logperror("sendbcast: sendto");
		logerr("Cannot send broadcast packet to %s\n",
		       pr_name(baddr.sin_addr));
	}
	return (cc);
}

int
sendmcast(s, packet, packetlen, sin)
	int s;
	char *packet;
	int packetlen;
	struct sockaddr_in *sin;
{
	int i, cc;

	for (i = 0; i < num_interfaces; i++) {
		if ((interfaces[i].flags & IFF_MULTICAST) == 0)
			continue;
		cc = sendmcastif(s, packet, packetlen, sin, &interfaces[i]);
		if (cc!= packetlen) {
			return (cc);
		}
	}
	return (packetlen);
}

int
sendmcastif(s, packet, packetlen, sin, ifp)
	int s;
	char *packet;
	int packetlen;
	struct sockaddr_in *sin;
	struct interface *ifp;
{
	int cc;
	struct sockaddr_in ifaddr;

	ifaddr.sin_family = AF_INET;

	if ((ifp->flags & IFF_MULTICAST) == 0)
		return (packetlen);

	ifaddr.sin_addr = ifp->address;
	if (debug)
		logdebug("Multicast to interface %s\n",
			 pr_name(ifaddr.sin_addr));
	if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF,
		       (char *)&ifaddr.sin_addr,
		       sizeof(ifaddr.sin_addr)) < 0) {
		logperror("setsockopt (IP_MULTICAST_IF)");
		logerr("Cannot send multicast packet over interface %s\n",
		       pr_name(ifaddr.sin_addr));
		return (-1);
	}
	cc = sendto(s, packet, packetlen, 0,
		    (struct sockaddr *)sin, sizeof (struct sockaddr));
	if (cc!= packetlen) {
		logperror("sendmcast: sendto");
		logerr("Cannot send multicast packet over interface %s\n",
		       pr_name(ifaddr.sin_addr));
	}
	return (cc);
}

void
init()
{
	int i;

	initifs(0);
	for (i = 0; i < interfaces_size; i++)
		interfaces[i].preference = preference;
}

void
initifs(sig)
	int sig;
{
	int	sock;
	struct ifconf ifc;
	struct ifreq ifreq, *ifrp, *ifend;
	struct sockaddr_in *sin;
	int n, i;
	char *buf;
	int numifs;
	unsigned bufsize;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		logperror("initifs: socket");
		return;
	}
#ifdef SIOCGIFNUM
	if (ioctl(sock, SIOCGIFNUM, (char *)&numifs) < 0) {
		numifs = MAXIFS;
	}
#else
	numifs = MAXIFS;
#endif
	bufsize = numifs * sizeof(struct ifreq);
	buf = (char *)malloc(bufsize);
	if (buf == NULL) {
		logerr("out of memory\n");
		(void) close(sock);
		return;
	}
	bzero(buf, bufsize);
	if (interfaces)
		interfaces = (struct interface *)ALLIGN(realloc((char *)interfaces,
					 numifs * sizeof(struct interface)));
	else
		interfaces = (struct interface *)ALLIGN(malloc(numifs *
						sizeof(struct interface)));
	if (interfaces == NULL) {
		logerr("out of memory\n");
		(void) close(sock);
		(void) free(buf);
		return;
	}
	interfaces_size = numifs;

	ifc.ifc_len = bufsize;
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) < 0) {
		logperror("initifs: ioctl (get interface configuration)");
		(void) close(sock);
		(void) free(buf);
		return;
	}
	ifrp = (struct ifreq *)buf;
	ifend = (struct ifreq *)(buf + ifc.ifc_len);
	for (i = 0; ifrp < ifend; ifrp = (struct ifreq *)((char *)ifrp + n)) {
		ifreq = *ifrp;
		if (ioctl(sock, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			logperror("initifs: ioctl (get interface flags)");
			n = 0;
			continue;
		}
		n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
		if (n < sizeof(*ifrp))
		    n = sizeof(*ifrp);
		if (ifrp->ifr_addr.sa_family != AF_INET)
			continue;
		if ((ifreq.ifr_flags & IFF_UP) == 0)
			continue;
		if (ifreq.ifr_flags & IFF_LOOPBACK)
			continue;
		if ((ifreq.ifr_flags & (IFF_MULTICAST | IFF_BROADCAST)) == 0)
			continue;
		sin = (struct sockaddr_in *)ALLIGN(&ifrp->ifr_addr);
		interfaces[i].localaddr = sin->sin_addr;
		interfaces[i].flags = ifreq.ifr_flags;
		interfaces[i].netmask.s_addr = (unsigned long)0xffffffff;
		if (ifreq.ifr_flags & IFF_POINTOPOINT) {
			if (ioctl(sock, SIOCGIFDSTADDR, (char *)&ifreq) < 0) {
				logperror("initifs: ioctl (get destination addr)");
				continue;
			}
			sin = (struct sockaddr_in *)ALLIGN(&ifreq.ifr_addr);
			/* A pt-pt link is identified by the remote address */
			interfaces[i].address = sin->sin_addr;
			interfaces[i].remoteaddr = sin->sin_addr;
			/* Simulate broadcast for pt-pt */
			interfaces[i].bcastaddr = sin->sin_addr;
			interfaces[i].flags |= IFF_BROADCAST;
		} else {
			/* Non pt-pt links are identified by the local address */
			interfaces[i].address = interfaces[i].localaddr;
			interfaces[i].remoteaddr = interfaces[i].address;
			if (ioctl(sock, SIOCGIFNETMASK, (char *)&ifreq) < 0) {
				logperror("initifs: ioctl (get netmask)");
				continue;
			}
			sin = (struct sockaddr_in *)ALLIGN(&ifreq.ifr_addr);
			interfaces[i].netmask = sin->sin_addr;
			if (ifreq.ifr_flags & IFF_BROADCAST) {
				if (ioctl(sock, SIOCGIFBRDADDR, (char *)&ifreq) < 0) {
					logperror("initifs: ioctl (get broadcast address)");
					continue;
				}
				sin = (struct sockaddr_in *)ALLIGN(&ifreq.ifr_addr);
				interfaces[i].bcastaddr = sin->sin_addr;
			}
		}
#ifdef notdef
		if (debug)
			logdebug("Found interface %s, flags 0x%x\n",
				 pr_name(interfaces[i].localaddr),
				 interfaces[i].flags);
#endif
		i++;
	}
	num_interfaces = i;
#ifdef notdef
	if (debug)
		logdebug("Found %d interfaces\n", num_interfaces);
#endif
	(void) close(sock);
	(void) free(buf);
}

int
join(sock, sin)
	int sock;
	struct sockaddr_in *sin;
{
	int i;
	struct ip_mreq mreq;

	if (isbroadcast(sin))
		return (0);

	mreq.imr_multiaddr = sin->sin_addr;
	for (i = 0; i < num_interfaces; i++) {
		mreq.imr_interface = interfaces[i].address;

		if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			       (char *)&mreq, sizeof(mreq)) < 0) {
			logperror("setsockopt (IP_ADD_MEMBERSHIP)");
			return (-1);
		}
	}
	return (0);
}

int support_multicast()
{
	int sock;
	u_char ttl = 1;

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		logperror("support_multicast: socket");
		return (0);
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
		       (char *)&ttl, sizeof(ttl)) < 0) {
		(void) close(sock);
		return (0);
	}
	(void) close(sock);
	return (1);
}

int
is_directly_connected(in)
	struct in_addr in;
{
	int i;

	for (i = 0; i < num_interfaces; i++) {
		/* Check that the subnetwork numbers match */

		if ((in.s_addr & interfaces[i].netmask.s_addr ) ==
		    (interfaces[i].remoteaddr.s_addr & interfaces[i].netmask.s_addr))
			return (1);
	}
	return (0);
}


/*
 * TABLES
 */
struct table {
	struct in_addr	router;
	int		preference;
	int		remaining_time;
	int		in_kernel;
	struct table	*next;
};

struct table *table;

struct table *
find_router(addr)
	struct in_addr addr;
{
	struct table *tp;

	tp = table;
	while (tp) {
		if (tp->router.s_addr == addr.s_addr)
			return (tp);
		tp = tp->next;
	}
	return (NULL);
}

int max_preference()
{
	struct table *tp;
	int max = (int)IGNORE_PREFERENCE;

	tp = table;
	while (tp) {
		if (tp->preference > max)
			max = tp->preference;
		tp = tp->next;
	}
	return (max);
}


/* Note: this might leave the kernel with no default route for a short time. */
void
age_table(time)
	int time;
{
	struct table **tpp, *tp;
	int recalculate_max = 0;
	int max = max_preference();

	tpp = &table;
	while (*tpp != NULL) {
		tp = *tpp;
		tp->remaining_time -= time;
		if (tp->remaining_time <= 0) {
			*tpp = tp->next;
			if (tp->in_kernel)
				del_route(tp->router);
			if (best_preference &&
			    tp->preference == max)
				recalculate_max++;
			free((char *)tp);
		} else {
			tpp = &tp->next;
		}
	}
	if (recalculate_max) {
		int max = max_preference();

		if (max != IGNORE_PREFERENCE) {
			tp = table;
			while (tp) {
				if (tp->preference == max && !tp->in_kernel) {
					add_route(tp->router);
					tp->in_kernel++;
				}
				tp = tp->next;
			}
		}
	}
}

void
record_router(router, preference, ttl)
	struct in_addr 	router;
	long		preference;
	int		ttl;
{
	struct table *tp;
	int old_max = max_preference();
	int changed_up = 0;	/* max preference could have increased */
	int changed_down = 0;	/* max preference could have decreased */

	if (debug)
		logdebug("Recording %s, preference 0x%x\n",
			      pr_name(router),
			      preference);
	tp = find_router(router);
	if (tp) {
		if (tp->preference > preference &&
		    tp->preference == old_max)
			changed_down++;
		else if (preference > tp->preference)
			changed_up++;
		tp->preference = preference;
		tp->remaining_time = ttl;
	} else {
		if (preference > old_max)
			changed_up++;
		tp = (struct table *)ALLIGN(malloc(sizeof(struct table)));
		if (tp == NULL) {
			logerr("Out of memory\n");
			return;
		}
		tp->router = router;
		tp->preference = preference;
		tp->remaining_time = ttl;
		tp->in_kernel = 0;
		tp->next = table;
		table = tp;
	}
	if (!tp->in_kernel &&
	    (!best_preference || tp->preference == max_preference()) &&
	    tp->preference != IGNORE_PREFERENCE) {
		add_route(tp->router);
		tp->in_kernel++;
	}
	if (tp->preference == IGNORE_PREFERENCE && tp->in_kernel) {
		del_route(tp->router);
		tp->in_kernel = 0;
	}
	if (best_preference && changed_down) {
		/* Check if we should add routes */
		int new_max = max_preference();
		if (new_max != IGNORE_PREFERENCE) {
			tp = table;
			while (tp) {
				if (tp->preference == new_max &&
				    !tp->in_kernel) {
					add_route(tp->router);
					tp->in_kernel++;
				}
				tp = tp->next;
			}
		}
	}
	if (best_preference && (changed_up || changed_down)) {
		/* Check if we should remove routes already in the kernel */
		int new_max = max_preference();
		tp = table;
		while (tp) {
			if (tp->preference < new_max && tp->in_kernel) {
				del_route(tp->router);
				tp->in_kernel = 0;
			}
			tp = tp->next;
		}
	}
}


#include <net/route.h>

void
add_route(addr)
	struct in_addr addr;
{
	if (debug)
		logdebug("Add default route to %s\n", pr_name(addr));
	rtioctl(addr, RTM_ADD);
}

void
del_route(addr)
	struct in_addr addr;
{
	if (debug)
		logdebug("Delete default route to %s\n", pr_name(addr));
	rtioctl(addr, RTM_DELETE);
}

void
rtioctl(addr, op)
	struct in_addr addr;
	int	op;
{
	int sock;
	struct {
		struct rt_msghdr m_rtm;
		struct sockaddr_in m_dst;
		struct sockaddr_in m_gateway;
		struct sockaddr_in m_netmask;
	} m_rtmsg;
	static int seq = 0;

	bzero(&m_rtmsg, sizeof(m_rtmsg));

	m_rtmsg.m_rtm.rtm_type = op;
	m_rtmsg.m_rtm.rtm_flags = RTF_GATEWAY | RTF_UP;		/* XXX more? */
	m_rtmsg.m_rtm.rtm_version = RTM_VERSION;
	m_rtmsg.m_rtm.rtm_seq = ++seq;
	m_rtmsg.m_rtm.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;

	/* initialise route metrics to zero.. tcp may fill these in */
	bzero(&m_rtmsg.m_rtm.rtm_rmx, sizeof(m_rtmsg.m_rtm.rtm_rmx));

	m_rtmsg.m_rtm.rtm_inits = 0;
	m_rtmsg.m_rtm.rtm_msglen = sizeof(m_rtmsg);

	m_rtmsg.m_dst.sin_len = sizeof(struct sockaddr_in);
	m_rtmsg.m_dst.sin_family = AF_INET;
	m_rtmsg.m_dst.sin_addr.s_addr = 0;		/* default */

	/* XXX: is setting a zero length right? */
	m_rtmsg.m_netmask.sin_len = 0;
	m_rtmsg.m_netmask.sin_family = AF_INET;
	m_rtmsg.m_netmask.sin_addr.s_addr = 0;		/* default */

	m_rtmsg.m_gateway.sin_len = sizeof(struct sockaddr_in);
	m_rtmsg.m_gateway.sin_family = AF_INET;
	m_rtmsg.m_gateway.sin_addr = addr;		/* gateway */

	sock = socket(PF_ROUTE, SOCK_RAW, 0);
	if (sock < 0) {
		logperror("rtioctl: socket");
		return;
	}
	if (write(sock, &m_rtmsg, sizeof(m_rtmsg)) != sizeof(m_rtmsg)) {
		if (!(op == RTM_ADD && errno == EEXIST))
			logperror("rtioctl: write");
	}
	close(sock);
}



/*
 * LOGGER
 */


static int logging = 0;

void
initlog()
{
	logging++;
	openlog("rdisc", LOG_PID | LOG_CONS, LOG_DAEMON);
}

/* VARARGS1 */
void
#if __STDC__
logerr(char *fmt, ...)
#else
logerr(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	if (logging)
		vsyslog(LOG_ERR, fmt, ap);
	else
		(void) vfprintf(stderr, fmt, ap);

	va_end(ap);
}

/* VARARGS1 */
void
#if __STDC__
logtrace(char *fmt, ...)
#else
logtrace(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	if (logging)
		vsyslog(LOG_INFO, fmt, ap);
	else
		(void) vfprintf(stdout, fmt, ap);

	va_end(ap);
}

/* VARARGS1 */
void
#if __STDC__
logdebug(char *fmt, ...)
#else
logdebug(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	if (logging)
		vsyslog(LOG_DEBUG, fmt, ap);
	else
		(void) vfprintf(stdout, fmt, ap);

	va_end(ap);
}

void
logperror(str)
	char *str;
{
	if (logging)
		syslog(LOG_ERR, "%s: %m", str);
	else
		(void) warn("%s", str);
}
