/* ntptrace.c,v 3.1 1993/07/06 01:09:38 jbj Exp
 * ntptrace - show the chain from an NTP host leading back to
 *	its source of time
 *
 *	Jeffrey Mogul	DECWRL	13 January 1993
 *
 *	Inspired by a script written by Glenn Trewitt
 *
 *	Large portions stolen from ntpdate.c
 */
#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>


#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#if defined(SYS_HPUX)
#include <utmp.h>
#endif

#include "ntp_select.h"
#include "ntp_fp.h"
#include "ntp.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntptrace.h"
#include "ntp_string.h"
#include "ntp_stdlib.h"
#include "ntp_syslog.h"

/*
 * Debugging flag
 */
int debug = 0;

int nonames = 0;			/* if set, don't print hostnames */

/*
 * Program name.
 */
char *progname;

/*
 * Systemwide parameters and flags
 */
int sys_retries = 5;			/* # of retry attempts per server */
U_LONG sys_timeout = 2;			/* timeout time, in seconds */
struct server **sys_servers;		/* the server list */
int sys_numservers = 0;			/* number of servers to poll */
int sys_maxservers = NTP_MAXSTRATUM+1;	/* max number of servers to deal with */
int sys_version = NTP_OLDVERSION;	/* version to poll with */

/*
 * recvbuf lists
 */
struct recvbuf *freelist;	/* free buffers */
struct recvbuf *fulllist;	/* buffers with data */

int full_recvbufs;	/* number of full ones */
int free_recvbufs;

/*
 * File descriptor masks etc. for call to select
 */
int fd;
fd_set fdmask;

/*
 * Miscellaneous flags
 */
int verbose = 0;
int always_step = 0;

extern int errno;

static	void	DoTrace		P((struct server *));
static	void	DoTransmit	P((struct server *));
static	int	DoReceive	P((struct server *));
static	int	ReceiveBuf	P((struct server *, struct recvbuf *));
static	struct server *addserver	P((struct in_addr *));
static	struct server *addservbyname	P((char *));
static	void	setup_io	P((void));
static	void	freerecvbuf	P((struct recvbuf *));
static	void	sendpkt	P((struct sockaddr_in *, struct pkt *, int));
static	int	getipaddr	P((char *, U_LONG *));
static	int	decodeipaddr	P((char *, U_LONG *));
static	void	printserver	P((struct server *, FILE *));
static	void	printrefid	P((FILE *, struct server *));

/*
 * Main program.  Initialize us and loop waiting for I/O and/or
 * timer expiries.
 */
void
main(argc, argv)
	int argc;
	char *argv[];
{
	struct server *firstserver;
	int errflg;
	int c;
	extern char *ntp_optarg;
	extern int ntp_optind;
	extern char *Version;

	errflg = 0;
	progname = argv[0];

	/*
	 * Decode argument list
	 */
	while ((c = ntp_getopt(argc, argv, "do:nr:t:v")) != EOF)
		switch (c) {
		case 'd':
			++debug;
			break;
		case 'n':
			nonames = 1;
			break;
		case 'o':
			sys_version = atoi(ntp_optarg);
			break;
		case 'r':
			sys_retries = atoi(ntp_optarg);
			if (sys_retries < 1) {
			    (void)fprintf(stderr,
					"%s: retries (%d) too small\n",
					progname, sys_retries);
			    errflg++;
			}
			break;
		case 't':
			sys_timeout = atoi(ntp_optarg);
			if (sys_timeout < 1) {
			    (void)fprintf(stderr,
					"%s: timeout (%d) too short\n",
					progname, sys_timeout);
			    errflg++;
			}
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
			++errflg;
			break;
		default:
			break;
		}
	
	if (errflg || (argc - ntp_optind) > 1) {
		(void) fprintf(stderr,
			"usage: %s [-vnd] [-r retries] [-t timeout] [server]\n",
			    progname);
		exit(2);
	}

	sys_servers = (struct server **)
	    emalloc(sys_maxservers * sizeof(struct server *));

	if (debug) {
#ifdef NTP_POSIX_SOURCE
                static char buf[BUFSIZ];
                setvbuf(stdout, buf, _IOLBF, BUFSIZ);
#else
		setlinebuf(stdout);
#endif
        }

	if (debug || verbose)
		syslog(LOG_NOTICE, "%s", Version);

	if ((argc - ntp_optind) == 1)
		firstserver = addservbyname(argv[ntp_optind]);
	else
		firstserver = addservbyname("localhost");
		
	if (firstserver == NULL) {
		/* a message has already been printed */
		exit(2);
	}

	/*
	 * Initialize the time of day routines and the I/O subsystem
	 */
	setup_io();
	
	DoTrace(firstserver);

	exit(0);
}

static void
DoTrace(server)
register struct server *server;
{
	int retries = sys_retries;

	if (!verbose) {
	    if (nonames)
		printf("%s: ", ntoa(&server->srcadr));
	    else
		printf("%s: ", ntohost(&server->srcadr));
	    fflush(stdout);
	}
	while (retries-- > 0) {
	    DoTransmit(server);
	    if (DoReceive(server))
		return;
	}
	if (verbose) {
	    if (nonames)
		printf("%s:\t*Timeout*\n", ntoa(&server->srcadr));
	    else
		printf("%s:\t*Timeout*\n", ntohost(&server->srcadr));
	}
	else
	    printf("\t*Timeout*\n");
}

/*
 * Dotransmit - transmit a packet to the given server
 */
static void
DoTransmit(server)
register struct server *server;
{
	struct pkt xpkt;

	if (debug)
		printf("DoTransmit(%s)\n", ntoa(&server->srcadr));

	/*
	 * Fill in the packet and let 'er rip.
	 */
	xpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOTINSYNC,
					sys_version, MODE_CLIENT);
	xpkt.stratum = STRATUM_TO_PKT(STRATUM_UNSPEC);
	xpkt.ppoll = NTP_MINPOLL;
	xpkt.precision = NTPTRACE_PRECISION;
	xpkt.rootdelay = htonl(NTPTRACE_DISTANCE);
	xpkt.rootdispersion = htonl(NTPTRACE_DISP);
	xpkt.refid = htonl(NTPTRACE_REFID);
	xpkt.reftime.l_ui = xpkt.reftime.l_uf = 0;
	xpkt.org.l_ui = xpkt.org.l_uf = 0;
	xpkt.rec.l_ui = xpkt.rec.l_uf = 0;

	/*
	 * just timestamp packet and send it away.
	 */
	get_systime(&(server->xmt));
	HTONL_FP(&server->xmt, &xpkt.xmt);
	sendpkt(&(server->srcadr), &xpkt, LEN_PKT_NOMAC);

	if (debug)
		printf("DoTransmit to %s\n", ntoa(&(server->srcadr)));
}

/*
 * DoReceive - attempt to receive a packet from a specific server
 */
static int
DoReceive(server)
register struct server *server;
{
	register int n;
	fd_set fds;
	struct timeval timeout;
	l_fp ts;
	register struct recvbuf *rb;
	int fromlen;
	int status;

	/*
	 * Loop until we see the packet we want or until we time out
	 */
	for (;;) {
	    fds = fdmask;
	    timeout.tv_sec = sys_timeout;
	    timeout.tv_usec = 0;
	    n = select(fd+1, &fds, (fd_set *)0, (fd_set *)0, &timeout);
	    
	    if (n == 0) {	/* timed out */
		if (debug)
			printf("timeout\n");
		return(0);
	    }
	    else if (n == -1) {
		syslog(LOG_ERR, "select() error: %m");
		return(0);
	    }
	    get_systime(&ts);
	    
	    if (free_recvbufs == 0) {
		syslog(LOG_ERR, "no buffers");
		exit(1);
	    }

	    rb = freelist;
	    freelist = rb->next;
	    free_recvbufs--;

	    fromlen = sizeof(struct sockaddr_in);
	    rb->recv_length = recvfrom(fd, (char *)&rb->recv_pkt,
		sizeof(rb->recv_pkt), 0,
		(struct sockaddr *)&rb->srcadr, &fromlen);
	    if (rb->recv_length == -1) {
		    rb->next = freelist;
		    freelist = rb;
		    free_recvbufs++;
		    continue;
	    }

	    /*
	     * Got one.  Mark how and when it got here,
	     * put it on the full list.
	     */
	    rb->recv_time = ts;
	    rb->next = fulllist;
	    fulllist = rb;
	    full_recvbufs++;

	    status = ReceiveBuf(server, rb);

	    freerecvbuf(rb);
	    
	    return(status);
	}
}

/*
 * receive - receive and process an incoming frame
 *	Return 1 on success, 0 on failure
 */
static int
ReceiveBuf(server, rbufp)
	struct server *server;
	struct recvbuf *rbufp;
{
	register struct pkt *rpkt;
	register s_fp di;
	register U_LONG t10_ui, t10_uf;
	register U_LONG t23_ui, t23_uf;
	l_fp org;
	l_fp rec;
	l_fp ci;
	struct server *nextserver;
	struct in_addr nextia;
	

	if (debug) {
		printf("ReceiveBuf(%s, ", ntoa(&server->srcadr));
		printf("%s)\n", ntoa(&rbufp->srcadr));
	}

	/*
	 * Check to see if the packet basically looks like something
	 * intended for us.
	 */
	if (rbufp->recv_length < LEN_PKT_NOMAC) {
		if (debug)
			printf("receive: packet length %d\n",
			    rbufp->recv_length);
		return(0);		/* funny length packet */
	}
	if (rbufp->srcadr.sin_addr.s_addr != server->srcadr.sin_addr.s_addr) {
		if (debug)
			printf("receive: wrong server\n");
		return(0);		/* funny length packet */
	}

	rpkt = &(rbufp->recv_pkt);

	if (PKT_VERSION(rpkt->li_vn_mode) < NTP_OLDVERSION) {
	    if (debug)
		printf("receive: version %d\n", PKT_VERSION(rpkt->li_vn_mode));
	    return(0);
	}
	if (PKT_VERSION(rpkt->li_vn_mode) > NTP_VERSION) {
	    if (debug)
		printf("receive: version %d\n", PKT_VERSION(rpkt->li_vn_mode));
	    return(0);
	}

	if ((PKT_MODE(rpkt->li_vn_mode) != MODE_SERVER
	    && PKT_MODE(rpkt->li_vn_mode) != MODE_PASSIVE)
	    || rpkt->stratum > NTP_MAXSTRATUM) {
		if (debug)
			printf("receive: mode %d stratum %d\n",
			    PKT_MODE(rpkt->li_vn_mode), rpkt->stratum);
		return(0);
	}
	
	/*
	 * Decode the org timestamp and make sure we're getting a response
	 * to our last request.
	 */
	NTOHL_FP(&rpkt->org, &org);
	if (!L_ISEQU(&org, &server->xmt)) {
		if (debug)
			printf("receive: pkt.org and peer.xmt differ\n");
		return(0);
	}
	
	/*
	 * Looks good.  Record info from the packet.
	 */
	server->leap = PKT_LEAP(rpkt->li_vn_mode);
	server->stratum = PKT_TO_STRATUM(rpkt->stratum);
	server->precision = rpkt->precision;
	server->rootdelay = ntohl(rpkt->rootdelay);
	server->rootdispersion = ntohl(rpkt->rootdispersion);
	server->refid = rpkt->refid;
	NTOHL_FP(&rpkt->reftime, &server->reftime);
	NTOHL_FP(&rpkt->rec, &rec);
	NTOHL_FP(&rpkt->xmt, &server->org);

	/*
	 * Make sure the server is at least somewhat sane.  If not, try
	 * again.
	 */
	if ((rec.l_ui == 0 && rec.l_uf == 0) || !L_ISHIS(&server->org, &rec)) {
		return(0);
	}

	/*
	 * Calculate the round trip delay (di) and the clock offset (ci).
	 * We use the equations (reordered from those in the spec):
	 *
	 * d = (t2 - t3) - (t1 - t0)
	 * c = ((t2 - t3) + (t1 - t0)) / 2
	 */
	t10_ui = server->org.l_ui;	/* pkt.xmt == t1 */
	t10_uf = server->org.l_uf;
	M_SUB(t10_ui, t10_uf, rbufp->recv_time.l_ui,
	    rbufp->recv_time.l_uf);	/* recv_time == t0*/

	t23_ui = rec.l_ui;	/* pkt.rec == t2 */
	t23_uf = rec.l_uf;
	M_SUB(t23_ui, t23_uf, org.l_ui, org.l_uf);	/* pkt->org == t3 */

	/* now have (t2 - t3) and (t0 - t1).  Calculate (ci) and (di) */
	ci.l_ui = t10_ui;
	ci.l_uf = t10_uf;
	M_ADD(ci.l_ui, ci.l_uf, t23_ui, t23_uf);
	M_RSHIFT(ci.l_i, ci.l_uf);

	/*
	 * Calculate di in t23 in full precision, then truncate
	 * to an s_fp.
	 */
	M_SUB(t23_ui, t23_uf, t10_ui, t10_uf);
	di = MFPTOFP(t23_ui, t23_uf);

	server->offset = ci;
	server->delay = di;

	printserver(server, stdout);

	/* End of recursion if we reach stratum 1 */
	if (server->stratum <= 1)
		return(1);

	nextia.s_addr = server->refid;
	nextserver = addserver(&nextia);
	DoTrace(nextserver);
	return(1);
}

/* XXX ELIMINATE addserver (almost) identical to ntpdate.c, ntptrace.c */
/*
 * addserver - Allocate a new structure for server.
 *		Returns a pointer to that structure.
 */
static struct server *
addserver(iap)
struct in_addr *iap;
{
	register struct server *server;
	static int toomany = 0;

	if (sys_numservers >= sys_maxservers) {
		if (!toomany) {
			toomany = 1;
			syslog(LOG_ERR,
		"too many servers (> %d) specified, remainder not used",
			    sys_maxservers);
		}
		return(NULL);
	}

	server = (struct server *)emalloc(sizeof(struct server));
	memset((char *)server, 0, sizeof(struct server));

	server->srcadr.sin_family = AF_INET;
	server->srcadr.sin_addr = *iap;
	server->srcadr.sin_port = htons(NTP_PORT);

	sys_servers[sys_numservers++] = server;
	
	return(server);
}
/*
 * addservbyname - determine a server's address and allocate a new structure
 *	       for it.  Returns a pointer to that structure.
 */
static struct server *
addservbyname(serv)
	char *serv;
{
	U_LONG ipaddr;
	struct in_addr ia;

	if (!getipaddr(serv, &ipaddr)) {
		syslog(LOG_ERR, "can't find host %s\n", serv);
		return(NULL);
	}

	ia.s_addr = ipaddr;
	return(addserver(&ia));
}

/* XXX ELIMINATE getrecvbufs (almost) identical to ntpdate.c, ntptrace.c, ntp_io.c */
/*
 * setup_io - initialize I/O data and open socket
 */
static void
setup_io()
{
	register int i;
	register struct recvbuf *rb;

	/*
	 * Init buffer free list and stat counters
	 */
	rb = (struct recvbuf *)
	    emalloc((sys_maxservers + 2) * sizeof(struct recvbuf));
	freelist = 0;
	for (i = sys_maxservers + 2; i > 0; i--) {
		rb->next = freelist;
		freelist = rb;
		rb++;
	}

	fulllist = 0;
	full_recvbufs = 0;
	free_recvbufs = sys_maxservers + 2;

	/* create a datagram (UDP) socket */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "socket() failed: %m");
		exit(1);
		/*NOTREACHED*/
	}

	FD_ZERO(&fdmask);
	FD_SET(fd, &fdmask);
}

/* XXX ELIMINATE freerecvbuf (almost) identical to ntpdate.c, ntptrace.c, ntp_io.c */
/*
 * freerecvbuf - make a single recvbuf available for reuse
 */
static void
freerecvbuf(rb)
	struct recvbuf *rb;
{
	rb->next = freelist;
	freelist = rb;
	free_recvbufs++;
}


/* XXX ELIMINATE sendpkt similar in ntpq.c, ntpdc.c, ntp_io.c, ntptrace.c */
/*
 * sendpkt - send a packet to the specified destination
 */
static void
sendpkt(dest, pkt, len)
	struct sockaddr_in *dest;
	struct pkt *pkt;
	int len;
{
	int cc;

	cc = sendto(fd, (char *)pkt, len, 0, (struct sockaddr *)dest,
	    sizeof(struct sockaddr_in));
	if (cc == -1) {
		if (errno != EWOULDBLOCK && errno != ENOBUFS)
			syslog(LOG_ERR, "sendto(%s): %m", ntoa(dest));
	}
}

/*
 * getipaddr - given a host name, return its host address
 */
static int
getipaddr(host, num)
	char *host;
	U_LONG *num;
{
	struct hostent *hp;

	if (decodeipaddr(host, num)) {
		return 1;
	} else if ((hp = gethostbyname(host)) != 0) {
		memmove((char *)num, hp->h_addr, sizeof(U_LONG));
		return 1;
	}
	return 0;
}

/*
 * decodeipaddr - return a host address (this is crude, but careful)
 */
static int
decodeipaddr(num, ipaddr)
	char *num;
	U_LONG *ipaddr;
{
	register char *cp;
	register char *bp;
	register int i;
	register int temp;
	char buf[80];		/* will core dump on really stupid stuff */

	cp = num;
	*ipaddr = 0;
	for (i = 0; i < 4; i++) {
		bp = buf;
		while (isdigit(*cp))
			*bp++ = *cp++;
		if (bp == buf)
			break;

		if (i < 3) {
			if (*cp++ != '.')
				break;
		} else if (*cp != '\0')
			break;

		*bp = '\0';
		temp = atoi(buf);
		if (temp > 255)
			break;
		*ipaddr <<= 8;
		*ipaddr += temp;
	}
	
	if (i < 4)
		return 0;
	*ipaddr = htonl(*ipaddr);
	return 1;
}


/* XXX ELIMINATE printserver similar in ntptrace.c, ntpdate.c */
/*
 * printserver - print detail information for a server
 */
static void
printserver(pp, fp)
	register struct server *pp;
	FILE *fp;
{
	u_fp synchdist;

	synchdist = pp->rootdispersion + (pp->rootdelay/2);

	if (!verbose) {
	    (void) fprintf(fp, "stratum %d, offset %s, synch distance %s",
			   pp->stratum,
			   lfptoa(&pp->offset, 7),
			   ufptoa(synchdist, 7));
	    if (pp->stratum == 1) {
		(void) fprintf(fp, ", refid ");
		printrefid(fp, pp);
	    }
	    (void) fprintf(fp, "\n");
	    return;
	}

	(void) fprintf(fp, "server %s, port %d\n",
	    ntoa(&pp->srcadr), ntohs(pp->srcadr.sin_port));

	(void) fprintf(fp, "stratum %d, precision %d, leap %c%c\n",
	    pp->stratum, pp->precision,
	    pp->leap & 0x2 ? '1' : '0',
	    pp->leap & 0x1 ? '1' : '0');
	
	(void) fprintf(fp, "refid ");
	printrefid(fp, pp);

	(void) fprintf(fp,
	    " delay %s, dispersion %s ",
	    fptoa(pp->delay, 7),
	    ufptoa(pp->dispersion, 4));
	(void) fprintf(fp, "offset %s\n",
	    lfptoa(&pp->offset, 7));
	(void) fprintf(fp, "rootdelay %s, rootdispersion %s",
		fptoa(pp->rootdelay, 7), ufptoa(pp->rootdispersion, 4));
	(void) fprintf(fp, ", synch dist %s\n",
		ufptoa(synchdist, 7));
	
	(void) fprintf(fp, "reference time:      %s\n",
	    prettydate(&pp->reftime));
	(void) fprintf(fp, "originate timestamp: %s\n",
	    prettydate(&pp->org));
	(void) fprintf(fp, "transmit timestamp:  %s\n",
	    prettydate(&pp->xmt));

	(void) fprintf(fp, "\n");

}

static void
printrefid(fp, pp)
FILE *fp;
struct server *pp;
{
	char junk[5];
	char *str;

	if (pp->stratum == 1) {
		junk[4] = 0;
		memmove(junk, (char *)&pp->refid, 4);
		str = junk;
		(void) fprintf(fp, "'%s'", str);
	} else {
	    if (nonames) {
		str = numtoa(pp->refid);
		(void) fprintf(fp, "[%s]", str);
	    }
	    else {
		str = numtohost(pp->refid);
		(void) fprintf(fp, "%s", str);
	    }
	}
}

#if defined(NEED_VSPRINTF)
/*
 * This nugget for pre-tahoe 4.3bsd systems
 */
#if !defined(__STDC__) || !__STDC__
#define const
#endif

int
vsprintf(str, fmt, ap)
	char *str;
	const char *fmt;
	va_list ap;
{
	FILE f;
	int len;

	f._flag = _IOWRT+_IOSTRG;
	f._ptr = str;
	f._cnt = 32767;
	len = _doprnt(fmt, ap, &f);
	*f._ptr = 0;
	return (len);
}
#endif
