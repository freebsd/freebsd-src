/* ntpdate.c,v 3.1 1993/07/06 01:09:22 jbj Exp
 * ntpdate - set the time of day by polling one or more NTP servers
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

#ifdef SYS_LINUX
#include <sys/timex.h>
#endif

#ifndef SYSLOG_FILE
#define SYSLOG_FILE	/* we want to go through the syslog/printf/file code */
#endif

#include "ntp_select.h"
#include "ntp_fp.h"
#include "ntp.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntpdate.h"
#include "ntp_string.h"
#include "ntp_stdlib.h"
#include "ntp_syslog.h"

/*
 * Scheduling priority we run at
 */
#define	NTPDATE_PRIO	(-12)

/*
 * Compatibility stuff for Version 2
 */
#define NTP_MAXSKW      0x28f   /* 0.01 sec in fp format */
#define NTP_MINDIST     0x51f   /* 0.02 sec in fp format */
#define PEER_MAXDISP    (64*FP_SECOND)  /* maximum dispersion (fp 64) */
#define NTP_INFIN       15      /* max stratum, infinity a la Bellman-Ford */
#define NTP_MAXWGT      (8*FP_SECOND)   /* maximum select weight 8 seconds */
#define NTP_MAXLIST     5       /* maximum select list size */
#define PEER_SHIFT      8       /* 8 suitable for crystal time base */

/*
 * Debugging flag
 */
int debug = 0;

/*
 * File descriptor masks etc. for call to select
 */
int fd;
fd_set fdmask;

/*
 * Initializing flag.  All async routines watch this and only do their
 * thing when it is clear.
 */
int initializing = 1;

/*
 * Alarm flag.  Set when an alarm occurs
 */
int alarm_flag = 0;

/*
 * Simple query flag.
 */
int simple_query = 0;

/*
 * Program name.
 */
char *progname;

/*
 * Systemwide parameters and flags
 */
int sys_samples = DEFSAMPLES;		/* number of samples/server */
U_LONG sys_timeout = DEFTIMEOUT;	/* timeout time, in TIMER_HZ units */
struct server **sys_servers;		/* the server list */
int sys_numservers = 0;			/* number of servers to poll */
int sys_maxservers = 0;			/* max number of servers to deal with */
int sys_authenticate = 0;		/* true when authenticating */
U_LONG sys_authkey = 0;			/* set to authentication key in use */
U_LONG sys_authdelay = 0;		/* authentication delay */
int sys_version = NTP_VERSION;		/* version to poll with */

/*
 * The current internal time
 */
U_LONG current_time = 0;

/*
 * Counter for keeping track of completed servers
 */
int complete_servers = 0;

/*
 * File of encryption keys
 */
#ifndef KEYFILE
#define	KEYFILE		"/etc/ntp.keys"
#endif	/* KEYFILE */

char *key_file = KEYFILE;

/*
 * Miscellaneous flags
 */
extern	int syslogit;
int verbose = 0;
int always_step = 0;

extern int errno;

static	void	transmit	P((struct server *));
static	void	receive		P((struct recvbuf *));
static	void	server_data	P((struct server *, s_fp, l_fp *, u_fp));
static	void	clock_filter	P((struct server *));
static	struct server *clock_select P((void));
static	int	clock_adjust	P((void));
static	void	addserver	P((char *));
static	struct server *findserver P((struct sockaddr_in *));
static	void	timer		P((void));
static	void	init_alarm	P((void));
static	RETSIGTYPE alarming	P((int));
static	void	init_io		P((void));
static	struct recvbuf *getrecvbufs P((void));
static	void	freerecvbuf	P((struct recvbuf *));
static	void	sendpkt		P((struct sockaddr_in *, struct pkt *, int));
static  void    input_handler   P((void));

static	int	l_adj_systime	P((l_fp *));
static  int	l_step_systime  P((l_fp *));

static	int	getnetnum	P((char *, U_LONG *));
static	void	printserver	P((struct server *, FILE *));

/*
 * Main program.  Initialize us and loop waiting for I/O and/or
 * timer expiries.
 */
void
main(argc, argv)
	int argc;
	char *argv[];
{
	int was_alarmed;
	struct recvbuf *rbuflist;
	struct recvbuf *rbuf;
	l_fp tmp;
	int errflg;
	int c;
	extern char *optarg;
	extern int optind;
	extern char *Version;

	errflg = 0;
	progname = argv[0];
	syslogit = 0;

	/*
	 * Decode argument list
	 */
	while ((c = getopt_l(argc, argv, "a:bde:k:o:p:qst:v")) != EOF)
		switch (c) {
		case 'a':
			c = atoi(optarg);
			sys_authenticate = 1;
			sys_authkey = (U_LONG)c;
			break;
		case 'b':
			always_step++;
			break;
		case 'd':
			++debug;
			break;
		case 'e':
			if (!atolfp(optarg, &tmp)
			    || tmp.l_ui != 0) {
				(void) fprintf(stderr,
				    "%s: encryption delay %s is unlikely\n",
				    progname, optarg);
				errflg++;
			} else {
				sys_authdelay = tmp.l_uf;
			}
			break;
		case 'k':
			key_file = optarg;
			break;
		case 'o':
			sys_version = atoi(optarg);
			break;
		case 'p':
			c = atoi(optarg);
			if (c <= 0 || c > NTP_SHIFT) {
				(void) fprintf(stderr,
				    "%s: number of samples (%d) is invalid\n",
				    progname, c);
				errflg++;
			} else {
				sys_samples = c;
			}
			break;
		case 'q':
			simple_query = 1;
			break;
		case 's':
			syslogit = 1;
			break;
		case 't':
			if (!atolfp(optarg, &tmp)) {
				(void) fprintf(stderr,
				    "%s: timeout %s is undecodeable\n",
				    progname, optarg);
				errflg++;
			} else {
				sys_timeout = ((LFPTOFP(&tmp) * TIMER_HZ)
				    + 0x8000) >> 16;
				if (sys_timeout == 0)
					sys_timeout = 1;
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
	
	sys_maxservers = argc - optind;
	if (errflg || sys_maxservers == 0) {
		(void) fprintf(stderr,
"usage: %s [-bqs] [-a key#] [-k file] [-p samples] [-t timeo] server ...\n",
		    progname);
		exit(2);
	}

	sys_servers = (struct server **)
	    emalloc(sys_maxservers * sizeof(struct server *));

	if (debug || simple_query) {
#ifdef NTP_POSIX_SOURCE
                static char buf[BUFSIZ];
                setvbuf(stdout, buf, _IOLBF, BUFSIZ);
#else
		setlinebuf(stdout);
#endif
        }

	/*
	 * Logging.  Open the syslog if we have to
	 */
	if (syslogit) {
#ifndef	LOG_DAEMON
		openlog("ntpdate", LOG_PID);
#else

#ifndef	LOG_NTP
#define	LOG_NTP	LOG_DAEMON
#endif
		openlog("ntpdate", LOG_PID | LOG_NDELAY, LOG_NTP);
		if (debug)
			setlogmask(LOG_UPTO(LOG_DEBUG));
		else
			setlogmask(LOG_UPTO(LOG_INFO));
#endif	/* LOG_DAEMON */
	}

	if (debug || verbose)
		syslog(LOG_NOTICE, "%s", Version);

	/*
	 * Add servers we are going to be polling
	 */
	for ( ; optind < argc; optind++)
		addserver(argv[optind]);

	if (sys_numservers == 0) {
		syslog(LOG_ERR, "no servers can be used, exiting");
		exit(1);
	}

	/*
	 * Initialize the time of day routines and the I/O subsystem
	 */
	if (sys_authenticate) {
		init_auth();
		if (!authreadkeys(key_file)) {
			syslog(LOG_ERR, "no key file, exitting");
			exit(1);
		}
		if (!authhavekey(sys_authkey)) {
			char buf[10];

			(void) sprintf(buf, "%u", sys_authkey);
			syslog(LOG_ERR, "authentication key %s unknown", buf);
			exit(1);
		}
	}
	init_io();
	init_alarm();

	/*
	 * Set the priority.
	 */
#if defined(HAVE_ATT_NICE)
	nice (NTPDATE_PRIO);
#endif
#if defined(HAVE_BSD_NICE)
	(void) setpriority(PRIO_PROCESS, 0, NTPDATE_PRIO);
#endif

	initializing = 0;

	was_alarmed = 0;
	rbuflist = (struct recvbuf *)0;
	while (complete_servers < sys_numservers) {
		fd_set rdfdes;
		int nfound;

		if (alarm_flag) {		/* alarmed? */
			was_alarmed = 1;
			alarm_flag = 0;
		}
		rbuflist = getrecvbufs();	/* get received buffers */

		if (!was_alarmed && rbuflist == (struct recvbuf *)0) {
                       /*
                         * Nothing to do.  Wait for something.
                         */
                        rdfdes = fdmask;
                        nfound = select(fd+1, &rdfdes, (fd_set *)0,
                                        (fd_set *)0, (struct timeval *)0);
                        if (nfound > 0)
                                input_handler();

                        else if (nfound == -1 && errno != EINTR) {
                                syslog(LOG_ERR, "select() error: %m");
                        }
                        if (alarm_flag) {               /* alarmed? */
                                was_alarmed = 1;
                                alarm_flag = 0;
                        }
                        rbuflist = getrecvbufs();  /* get received buffers */

		}

		/*
		 * Out here, signals are unblocked.  Call receive
		 * procedure for each incoming packet.
		 */
		while (rbuflist != (struct recvbuf *)0) {
			rbuf = rbuflist;
			rbuflist = rbuf->next;
			receive(rbuf);
			freerecvbuf(rbuf);
		}

		/*
		 * Call timer to process any timeouts
		 */
		if (was_alarmed) {
			timer();
			was_alarmed = 0;
		}

		/*
		 * Go around again
		 */
	}

	/*
	 * When we get here we've completed the polling of all servers.
	 * Adjust the clock, then exit.
	 */
	exit(clock_adjust());
}


/*
 * transmit - transmit a packet to the given server, or mark it completed.
 *	      This is called by the timeout routine and by the receive
 *	      procedure.
 */
static void
transmit(server)
	register struct server *server;
{
	struct pkt xpkt;

	if (debug)
		printf("transmit(%s)\n", ntoa(&server->srcadr));

	if (server->filter_nextpt < server->xmtcnt) {
		l_fp ts;
		/*
		 * Last message to this server timed out.  Shift
		 * zeros into the filter.
		 */
		ts.l_ui = ts.l_uf = 0;
		server_data(server, 0, &ts, 0);
	}

	if ((int)server->filter_nextpt >= sys_samples) {
		/*
		 * Got all the data we need.  Mark this guy
		 * completed and return.
		 */
		server->event_time = 0;
		complete_servers++;
		return;
	}

	/*
	 * If we're here, send another message to the server.  Fill in
	 * the packet and let 'er rip.
	 */
	xpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOTINSYNC,
	    sys_version, MODE_CLIENT);
	xpkt.stratum = STRATUM_TO_PKT(STRATUM_UNSPEC);
	xpkt.ppoll = NTP_MINPOLL;
	xpkt.precision = NTPDATE_PRECISION;
	xpkt.rootdelay = htonl(NTPDATE_DISTANCE);
	xpkt.rootdispersion = htonl(NTPDATE_DISP);
	xpkt.refid = htonl(NTPDATE_REFID);
	xpkt.reftime.l_ui = xpkt.reftime.l_uf = 0;
	xpkt.org.l_ui = xpkt.org.l_uf = 0;
	xpkt.rec.l_ui = xpkt.rec.l_uf = 0;

	/*
	 * Determine whether to authenticate or not.  If so,
	 * fill in the extended part of the packet and do it.
	 * If not, just timestamp it and send it away.
	 */
	if (sys_authenticate) {
		int len;

		xpkt.keyid = htonl(sys_authkey);
		auth1crypt(sys_authkey, (U_LONG *)&xpkt, LEN_PKT_NOMAC);
		get_systime(&server->xmt);
		L_ADDUF(&server->xmt, sys_authdelay);
		HTONL_FP(&server->xmt, &xpkt.xmt);
		len = auth2crypt(sys_authkey, (U_LONG *)&xpkt, LEN_PKT_NOMAC);
		sendpkt(&(server->srcadr), &xpkt, LEN_PKT_NOMAC + len);

		if (debug > 1)
			printf("transmit auth to %s\n",
			    ntoa(&(server->srcadr)));
	} else {
		get_systime(&(server->xmt));
		HTONL_FP(&server->xmt, &xpkt.xmt);
		sendpkt(&(server->srcadr), &xpkt, LEN_PKT_NOMAC);

		if (debug > 1)
			printf("transmit to %s\n", ntoa(&(server->srcadr)));
	}

	/*
	 * Update the server timeout and transmit count
	 */
	server->event_time = current_time + sys_timeout;
	server->xmtcnt++;
}


/*
 * receive - receive and process an incoming frame
 */
static void
receive(rbufp)
	struct recvbuf *rbufp;
{
	register struct pkt *rpkt;
	register struct server *server;
	register s_fp di;
	register U_LONG t10_ui, t10_uf;
	register U_LONG t23_ui, t23_uf;
	l_fp org;
	l_fp rec;
	l_fp ci;
	int has_mac;
	int is_authentic;

	if (debug)
		printf("receive(%s)\n", ntoa(&rbufp->srcadr));
	/*
	 * Check to see if the packet basically looks like something
	 * intended for us.
	 */
	if (rbufp->recv_length == LEN_PKT_NOMAC)
		has_mac = 0;
	else if (rbufp->recv_length >= LEN_PKT_NOMAC)
		has_mac = 1;
	else {
		if (debug)
			printf("receive: packet length %d\n",
			    rbufp->recv_length);
		return;		/* funny length packet */
	}

	rpkt = &(rbufp->recv_pkt);
	if (PKT_VERSION(rpkt->li_vn_mode) == NTP_OLDVERSION) {
#ifdef notdef
		/*
		 * Fuzzballs do encryption but still claim
		 * to be version 1.
		 */
		if (has_mac)
			return;
#endif
	} else if (PKT_VERSION(rpkt->li_vn_mode) != NTP_VERSION) {
		return;
	}

	if ((PKT_MODE(rpkt->li_vn_mode) != MODE_SERVER
	    && PKT_MODE(rpkt->li_vn_mode) != MODE_PASSIVE)
	    || rpkt->stratum > NTP_MAXSTRATUM) {
		if (debug)
			printf("receive: mode %d stratum %d\n",
			    PKT_MODE(rpkt->li_vn_mode), rpkt->stratum);
		return;
	}
	
	/*
	 * So far, so good.  See if this is from a server we know.
	 */
	server = findserver(&(rbufp->srcadr));
	if (server == NULL) {
		if (debug)
			printf("receive: server not found\n");
		return;
	}

	/*
	 * Decode the org timestamp and make sure we're getting a response
	 * to our last request.
	 */
	NTOHL_FP(&rpkt->org, &org);
	if (!L_ISEQU(&org, &server->xmt)) {
		if (debug)
			printf("receive: pkt.org and peer.xmt differ\n");
		return;
	}
	
	/*
	 * Check out the authenticity if we're doing that.
	 */
	if (!sys_authenticate)
		is_authentic = 1;
	else {
		is_authentic = 0;

		if (debug > 3)
		    printf("receive: rpkt keyid=%d sys_authkey=%d decrypt=%d\n",
			   ntohl(rpkt->keyid), sys_authkey, 
			   authdecrypt(sys_authkey, (U_LONG *)rpkt,
				       LEN_PKT_NOMAC));

		if (has_mac && ntohl(rpkt->keyid) == sys_authkey &&
		    authdecrypt(sys_authkey, (U_LONG *)rpkt, LEN_PKT_NOMAC))
				is_authentic = 1;
		if (debug)
		    printf("receive: authentication %s\n",
			   is_authentic ? "passed" : "failed");
	}
	server->trust <<= 1;
	if (!is_authentic)
		server->trust |= 1;
	
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
		transmit(server);
		return;
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

	if (debug > 3)
		printf("offset: %s, delay %s\n", lfptoa(&ci, 9), fptoa(di, 4));

	di += (FP_SECOND >> (-(int)NTPDATE_PRECISION))
	    + (FP_SECOND >> (-(int)server->precision)) + NTP_MAXSKW;

	if (di <= 0) {		/* value still too raunchy to use? */
		ci.l_ui = ci.l_uf = 0;
		di = 0;
	} else {
		di = max(di, NTP_MINDIST);
	}

	/*
	 * Shift this data in, then transmit again.
	 */
	server_data(server, (u_fp) di, &ci, 0);
	transmit(server);
}


/*
 * server_data - add a sample to the server's filter registers
 */
static void
server_data(server, d, c, e)
	register struct server *server;
	s_fp d;
	l_fp *c;
	u_fp e;
{
	register int i;

	i = server->filter_nextpt;
	if (i < NTP_SHIFT) {
		server->filter_delay[i] = d;
		server->filter_offset[i] = *c;
		server->filter_soffset[i] = MFPTOFP(c->l_ui, c->l_uf);
		server->filter_error[i] = e;
		server->filter_nextpt = i + 1;
	}
}


/*
 * clock_filter - determine a server's delay, dispersion and offset
 */
static void
clock_filter(server)
	register struct server *server;
{
	register int i, j;
	int ord[NTP_SHIFT];

	/*
	 * Sort indices into increasing delay order
	 */
	for (i = 0; i < sys_samples; i++)
		ord[i] = i;
	
	for (i = 0; i < (sys_samples-1); i++) {
		for (j = i+1; j < sys_samples; j++) {
			if (server->filter_delay[ord[j]] == 0)
				continue;
			if (server->filter_delay[ord[i]] == 0
			    || (server->filter_delay[ord[i]]
			    > server->filter_delay[ord[j]])) {
				register int tmp;

				tmp = ord[i];
				ord[i] = ord[j];
				ord[j] = tmp;
			}
		}
	}

	/*
	 * Now compute the dispersion, and assign values to delay and
	 * offset.  If there are no samples in the register, delay and
	 * offset go to zero and dispersion is set to the maximum.
	 */
	if (server->filter_delay[ord[0]] == 0) {
		server->delay = 0;
		server->offset.l_ui = server->offset.l_uf = 0;
		server->soffset = 0;
		server->dispersion = PEER_MAXDISP;
	} else {
		register s_fp d;

		server->delay = server->filter_delay[ord[0]];
		server->offset = server->filter_offset[ord[0]];
		server->soffset = LFPTOFP(&server->offset);
		server->dispersion = 0;
		for (i = 1; i < sys_samples; i++) {
			if (server->filter_delay[ord[i]] == 0)
				d = PEER_MAXDISP;
			else {
				d = server->filter_soffset[ord[i]]
				    - server->filter_soffset[ord[0]];
				if (d < 0)
					d = -d;
				if (d > PEER_MAXDISP)
					d = PEER_MAXDISP;
			}
			/*
			 * XXX This *knows* PEER_FILTER is 1/2
			 */
			server->dispersion += (u_fp)(d) >> i;
		}
	}
	/*
	 * We're done
	 */
}


/*
 * clock_select - select the pick-of-the-litter clock from the samples
 *		  we've got.
 */
static struct server *
clock_select()
{
	register struct server *server;
	register int i;
	register int nlist;
	register s_fp d;
	register int j;
	register int n;
	s_fp local_threshold;
	struct server *server_list[NTP_MAXCLOCK];
	u_fp server_badness[NTP_MAXCLOCK];
	struct server *sys_server;

	/*
	 * This first chunk of code is supposed to go through all
	 * servers we know about to find the NTP_MAXLIST servers which
	 * are most likely to succeed.  We run through the list
	 * doing the sanity checks and trying to insert anyone who
	 * looks okay.  We are at all times aware that we should
	 * only keep samples from the top two strata and we only need
	 * NTP_MAXLIST of them.
	 */
	nlist = 0;	/* none yet */
	for (n = 0; n < sys_numservers; n++) {
		server = sys_servers[n];
		if (server->delay == 0)
			continue;	/* no data */
		if (server->stratum > NTP_INFIN)
			continue;	/* stratum no good */
		if (server->delay > NTP_MAXWGT) {
			continue;	/* too far away */
		}
		if (server->leap == LEAP_NOTINSYNC)
			continue;	/* he's in trouble */
		if (server->org.l_ui < server->reftime.l_ui) {
			continue;	/* very broken host */
		}
		if ((server->org.l_ui - server->reftime.l_ui)
		    >= NTP_MAXAGE) {
			continue;	/* too LONG without sync */
		}
		if (server->trust != 0) {
			continue;
		}

		/*
		 * This one seems sane.  Find where he belongs
		 * on the list.
		 */
		d = server->dispersion + server->dispersion;
		for (i = 0; i < nlist; i++)
			if (server->stratum <= server_list[i]->stratum)
				break;
		for ( ; i < nlist; i++) {
			if (server->stratum < server_list[i]->stratum)
				break;
			if (d < server_badness[i])
				break;
		}

		/*
		 * If i points past the end of the list, this
		 * guy is a loser, else stick him in.
		 */
		if (i >= NTP_MAXLIST)
			continue;
		for (j = nlist; j > i; j--)
			if (j < NTP_MAXLIST) {
				server_list[j] = server_list[j-1];
				server_badness[j]
				    = server_badness[j-1];
			}

		server_list[i] = server;
		server_badness[i] = d;
		if (nlist < NTP_MAXLIST)
			nlist++;
	}

	/*
	 * Got the five-or-less best.  Cut the list where the number of
	 * strata exceeds two.
	 */
	j = 0;
	for (i = 1; i < nlist; i++)
		if (server_list[i]->stratum > server_list[i-1]->stratum)
			if (++j == 2) {
				nlist = i;
				break;
			}

	/*
	 * Whew!  What we should have by now is 0 to 5 candidates for
	 * the job of syncing us.  If we have none, we're out of luck.
	 * If we have one, he's a winner.  If we have more, do falseticker
	 * detection.
	 */

	if (nlist == 0)
		sys_server = 0;
	else if (nlist == 1) {
		sys_server = server_list[0];
	} else {
		/*
		 * Re-sort by stratum, bdelay estimate quality and
		 * server.delay.
		 */
		for (i = 0; i < nlist-1; i++)
			for (j = i+1; j < nlist; j++) {
				if (server_list[i]->stratum
				    < server_list[j]->stratum)
					break;	/* already sorted by stratum */
				if (server_list[i]->delay
				    < server_list[j]->delay)
					continue;
				server = server_list[i];
				server_list[i] = server_list[j];
				server_list[j] = server;
			}
		
		/*
		 * Calculate the fixed part of the dispersion limit
		 */
		local_threshold = (FP_SECOND >> (-(int)NTPDATE_PRECISION))
		    + NTP_MAXSKW;

		/*
		 * Now drop samples until we're down to one.
		 */
		while (nlist > 1) {
			for (n = 0; n < nlist; n++) {
				server_badness[n] = 0;
				for (j = 0; j < nlist; j++) {
					if (j == n)	/* with self? */
						continue;
					d = server_list[j]->soffset
					    - server_list[n]->soffset;
					if (d < 0)	/* absolute value */
						d = -d;
					/*
					 * XXX This code *knows* that
					 * NTP_SELECT is 3/4
					 */
					for (i = 0; i < j; i++)
						d = (d>>1) + (d>>2);
					server_badness[n] += d;
				}
			}

			/*
			 * We now have an array of nlist badness
			 * coefficients.  Find the badest.  Find
			 * the minimum precision while we're at
			 * it.
			 */
			i = 0;
			n = server_list[0]->precision;;
			for (j = 1; j < nlist; j++) {
				if (server_badness[j] >= server_badness[i])
					i = j;
				if (n > server_list[j]->precision)
					n = server_list[j]->precision;
			}
			
			/*
			 * i is the index of the server with the worst
			 * dispersion.  If his dispersion is less than
			 * the threshold, stop now, else delete him and
			 * continue around again.
			 */
			if (server_badness[i] < (local_threshold
			    + (FP_SECOND >> (-n))))
				break;
			for (j = i + 1; j < nlist; j++)
				server_list[j-1] = server_list[j];
			nlist--;
		}

		/*
		 * What remains is a list of less than 5 servers.  Take
		 * the best.
		 */
		sys_server = server_list[0];
	}

	/*
	 * That's it.  Return our server.
	 */
	return sys_server;
}


/*
 * clock_adjust - process what we've received, and adjust the time
 *	         if we got anything decent.
 */
static int
clock_adjust()
{
	register int i;
	register struct server *server;
	s_fp absoffset;
	int dostep;

	for (i = 0; i < sys_numservers; i++)
		clock_filter(sys_servers[i]);
	server = clock_select();

	if (debug || simple_query) {
		for (i = 0; i < sys_numservers; i++)
			printserver(sys_servers[i], stdout);
	}

	if (server == 0) {
		syslog(LOG_ERR,
		    "no server suitable for synchronization found");
		return(1);
	}
	
	dostep = 1;
	if (!always_step) {
		absoffset = server->soffset;
		if (absoffset < 0)
			absoffset = -absoffset;
		if (absoffset < NTPDATE_THRESHOLD)
			dostep = 0;
	}

	if (dostep) {
		if (simple_query || l_step_systime(&server->offset)) {
			syslog(LOG_NOTICE, "step time server %s offset %s",
			    ntoa(&server->srcadr),
			    lfptoa(&server->offset, 7));
		}
	} else {
		if (simple_query || l_adj_systime(&server->offset)) {
			syslog(LOG_NOTICE, "adjust time server %s offset %s",
			    ntoa(&server->srcadr),
			    lfptoa(&server->offset, 7));
		}
	}
	return(0);
}


/* XXX ELIMINATE: merge BIG slew into adj_systime in lib/systime.c */
/*
 * addserver - determine a server's address and allocate a new structure
 *	       for it.
 */
static void
addserver(serv)
	char *serv;
{
	register struct server *server;
	U_LONG netnum;
	static int toomany = 0;

	if (sys_numservers >= sys_maxservers) {
		if (!toomany) {
			/*
			 * This is actually a `can't happen' now.  Leave
			 * the error message in anyway, though
			 */
			toomany = 1;
			syslog(LOG_ERR,
		"too many servers (> %d) specified, remainder not used",
			    sys_maxservers);
		}
		return;
	}

	if (!getnetnum(serv, &netnum)) {
		syslog(LOG_ERR, "can't find host %s\n", serv);
		return;
	}

	server = (struct server *)emalloc(sizeof(struct server));
	bzero((char *)server, sizeof(struct server));

	server->srcadr.sin_family = AF_INET;
	server->srcadr.sin_addr.s_addr = netnum;
	server->srcadr.sin_port = htons(NTP_PORT);

	sys_servers[sys_numservers++] = server;
	server->event_time = (U_LONG)sys_numservers;
}


/*
 * findserver - find a server in the list given its address
 */
static struct server *
findserver(addr)
	struct sockaddr_in *addr;
{
	register int i;
	register U_LONG netnum;

	if (htons(addr->sin_port) != NTP_PORT)
		return 0;
	netnum = addr->sin_addr.s_addr;

	for (i = 0; i < sys_numservers; i++) {
		if (netnum == sys_servers[i]->srcadr.sin_addr.s_addr)
			return sys_servers[i];
	}
	return 0;
}


/*
 * timer - process a timer interrupt
 */
static void
timer()
{
	register int i;

	/*
	 * Bump the current idea of the time
	 */
	current_time++;

	/*
	 * Search through the server list looking for guys
	 * who's event timers have expired.  Give these to
	 * the transmit routine.
	 */
	for (i = 0; i < sys_numservers; i++) {
		if (sys_servers[i]->event_time != 0
		    && sys_servers[i]->event_time <= current_time)
			transmit(sys_servers[i]);
	}
}



/*
 * init_alarm - set up the timer interrupt
 */
static void
init_alarm()
{
	struct itimerval itimer;

	alarm_flag = 0;

	/*
	 * Set up the alarm interrupt.  The first comes 1/(2*TIMER_HZ)
	 * seconds from now and they continue on every 1/TIMER_HZ seconds.
	 */
	(void) signal_no_reset(SIGALRM, alarming);
	itimer.it_interval.tv_sec = itimer.it_value.tv_sec = 0;
	itimer.it_interval.tv_usec = 1000000/TIMER_HZ;
	itimer.it_value.tv_usec = 1000000/(TIMER_HZ<<1);
	setitimer(ITIMER_REAL, &itimer, (struct itimerval *)0);
}


/*
 * alarming - record the occurance of an alarm interrupt
 */
static RETSIGTYPE
alarming(sig)
int sig;
{
	alarm_flag++;
}


/*
 * We do asynchronous input using the SIGIO facility.  A number of
 * recvbuf buffers are preallocated for input.  In the signal
 * handler we poll to see if the socket is ready and read the
 * packets from it into the recvbuf's along with a time stamp and
 * an indication of the source host and the interface it was received
 * through.  This allows us to get as accurate receive time stamps
 * as possible independent of other processing going on.
 *
 * We allocate a number of recvbufs equal to the number of servers
 * plus 2.  This should be plenty.
 */

/*
 * recvbuf lists
 */
struct recvbuf *freelist;	/* free buffers */
struct recvbuf *fulllist;	/* buffers with data */

int full_recvbufs;	/* number of full ones */
int free_recvbufs;


/*
 * init_io - initialize I/O data and open socket
 */
static void
init_io()
{
	register int i;
	register struct recvbuf *rb;

	/*
	 * Init buffer free list and stat counters
	 */
	rb = (struct recvbuf *)
	    emalloc((sys_numservers + 2) * sizeof(struct recvbuf));
	freelist = 0;
	for (i = sys_numservers + 2; i > 0; i--) {
		rb->next = freelist;
		freelist = rb;
		rb++;
	}

	fulllist = 0;
	full_recvbufs = 0;
	free_recvbufs = sys_numservers + 2;

	/*
	 * Open the socket
	 */

	/* create a datagram (UDP) socket */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "socket() failed: %m");
		exit(1);
		/*NOTREACHED*/
	}

	/*
	 * bind the socket to the NTP port
	 */
	if (!debug && !simple_query) {
		struct sockaddr_in addr;

		bzero((char *)&addr, sizeof addr);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(NTP_PORT);
		addr.sin_addr.s_addr = INADDR_ANY;
		if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			if (errno == EADDRINUSE)
				syslog(LOG_ERR,
				    "the NTP socket is in use, exiting");
			else
				syslog(LOG_ERR, "bind() fails: %m");
			exit(1);
		}
	}

	FD_ZERO(&fdmask);
	FD_SET(fd, &fdmask);

        /*
         * set non-blocking,
         */
#if defined(O_NONBLOCK)
        if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
                syslog(LOG_ERR, "fcntl(FNDELAY|FASYNC) fails: %m");
                exit(1);
                /*NOTREACHED*/
        }
#else /* O_NONBLOCK */
#if defined(FNDELAY)
        if (fcntl(fd, F_SETFL, FNDELAY) < 0) {
                syslog(LOG_ERR, "fcntl(FNDELAY|FASYNC) fails: %m");
                exit(1);
                /*NOTREACHED*/
        }
#else /* FNDELAY */
Need non blocking I/O
#endif /* FNDELAY */
#endif /* O_NONBLOCK */
}


/* XXX ELIMINATE getrecvbufs (almost) identical to ntpdate.c, ntptrace.c, ntp_io.c */
/*
 * getrecvbufs - get receive buffers which have data in them
 *
 * ***N.B. must be called with SIGIO blocked***
 */
static struct recvbuf *
getrecvbufs()
{
	struct recvbuf *rb;

	if (full_recvbufs == 0) {
		return (struct recvbuf *)0;	/* nothing has arrived */
	}
	
	/*
	 * Get the fulllist chain and mark it empty
	 */
	rb = fulllist;
	fulllist = 0;
	full_recvbufs = 0;

	/*
	 * Return the chain
	 */
	return rb;
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
 * input_handler - receive packets asynchronously
 */
static void
input_handler()
{
	register int n;
	register struct recvbuf *rb;
	struct timeval tvzero;
	int fromlen;
	l_fp ts;
	fd_set fds;

	/*
	 * Do a poll to see if we have data
	 */
	for (;;) {
		fds = fdmask;
		tvzero.tv_sec = tvzero.tv_usec = 0;
		n = select(fd+1, &fds, (fd_set *)0, (fd_set *)0, &tvzero);

		/*
		 * If nothing to do, just return.  If an error occurred,
		 * complain and return.  If we've got some, freeze a
		 * timestamp.
		 */
		if (n == 0)
			return;
		else if (n == -1) {
			syslog(LOG_ERR, "select() error: %m");
			return;
		}
		get_systime(&ts);

		/*
		 * Get a buffer and read the frame.  If we
		 * haven't got a buffer, or this is received
		 * on the wild card socket, just dump the packet.
		 */
		if (initializing || free_recvbufs == 0) {
			char buf[100];

			(void) read(fd, buf, sizeof buf);
			continue;
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
	}
}


/*
 * adj_systime - do a big LONG slew of the system time
 */
static int
l_adj_systime(ts)
	l_fp *ts;
{
	struct timeval adjtv, oadjtv;
	int isneg = 0;
	l_fp offset;
	l_fp overshoot;

	/*
	 * Take the absolute value of the offset
	 */
	offset = *ts;
	if (L_ISNEG(&offset)) {
		isneg = 1;
		L_NEG(&offset);
	}

#ifndef STEP_SLEW
	/*
	 * Calculate the overshoot.  XXX N.B. This code *knows*
	 * ADJ_OVERSHOOT is 1/2.
	 */
	overshoot = offset;
	L_RSHIFTU(&overshoot);
	if (overshoot.l_ui != 0 || (overshoot.l_uf > ADJ_MAXOVERSHOOT)) {
		overshoot.l_ui = 0;
		overshoot.l_uf = ADJ_MAXOVERSHOOT;
	}
	L_ADD(&offset, &overshoot);
#endif
	TSTOTV(&offset, &adjtv);

	if (isneg) {
		adjtv.tv_sec = -adjtv.tv_sec;
		adjtv.tv_usec = -adjtv.tv_usec;
	}

	if (adjtv.tv_usec != 0 && !debug) {
		if (adjtime(&adjtv, &oadjtv) < 0) {
			syslog(LOG_ERR, "Can't adjust the time of day: %m");
			return 0;
		}
	}
	return 1;
}


/*
 * This fuction is not the same as lib/systime step_systime!!!
 */
static int
l_step_systime(ts)
	l_fp *ts;
{
#ifdef SLEWALWAYS 
#ifdef STEP_SLEW
	register U_LONG tmp_ui;
	register U_LONG tmp_uf;
	int isneg;
	int n;

	if (debug) return 1;
	/*
	 * Take the absolute value of the offset
	 */
	tmp_ui = ts->l_ui;
	tmp_uf = ts->l_uf;
	if (M_ISNEG(tmp_ui, tmp_uf)) {
		M_NEG(tmp_ui, tmp_uf);
		isneg = 1;
	} else
		isneg = 0;

	if (tmp_ui >= 3) {		/* Step it and slew - we might win */
             n = step_systime_real(ts);
	     if (!n) return n;
	     if (isneg) 
		ts->l_ui = ~0;
	     else
		ts->l_ui = ~0;
	}
        /*
         * Just add adjustment into the current offset.  The update
         * routine will take care of bringing the system clock into
         * line.
         */
#endif
	if (debug) return 1;
#ifdef FORCE_NTPDATE_STEP
        return step_systime_real(ts);
#else
	l_adj_systime(ts);
	return 1;
#endif
#else /* SLEWALWAYS  */
	if (debug) return 1;
        return step_systime_real(ts);
#endif	/* SLEWALWAYS */
}

/*
 * getnetnum - given a host name, return its net number
 */
static int
getnetnum(host, num)
	char *host;
	U_LONG *num;
{
	struct hostent *hp;

	if (decodenetnum(host, num)) {
		return 1;
	} else if ((hp = gethostbyname(host)) != 0) {
		bcopy(hp->h_addr, (char *)num, sizeof(U_LONG));
		return 1;
	}
	return 0;
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
	register int i;
	char junk[5];
	char *str;

	if (!debug) {
	    (void) fprintf(fp, "server %s, stratum %d, offset %s, delay %s\n",
			   ntoa(&pp->srcadr), pp->stratum,
			   lfptoa(&pp->offset, 7), ufptoa(pp->delay, 4));
	    return;
	}

	(void) fprintf(fp, "server %s, port %d\n",
	    ntoa(&pp->srcadr), ntohs(pp->srcadr.sin_port));

	(void) fprintf(fp, "stratum %d, precision %d, leap %c%c, trust %03o\n",
	    pp->stratum, pp->precision,
	    pp->leap & 0x2 ? '1' : '0',
	    pp->leap & 0x1 ? '1' : '0',
	    pp->trust);
	
	if (pp->stratum == 1) {
		junk[4] = 0;
		bcopy((char *)&pp->refid, junk, 4);
		str = junk;
	} else {
		str = numtoa(pp->refid);
	}
	(void) fprintf(fp,
	    "refid [%s], delay %s, dispersion %s\n",
	    str, fptoa(pp->delay, 4),
	    ufptoa(pp->dispersion, 4));
	
	(void) fprintf(fp, "transmitted %d, in filter %d\n",
	    pp->xmtcnt, pp->filter_nextpt);

	(void) fprintf(fp, "reference time:      %s\n",
	    prettydate(&pp->reftime));
	(void) fprintf(fp, "originate timestamp: %s\n",
	    prettydate(&pp->org));
	(void) fprintf(fp, "transmit timestamp:  %s\n",
	    prettydate(&pp->xmt));
	
	(void) fprintf(fp, "filter delay: ");
	for (i = 0; i < NTP_SHIFT; i++) {
		(void) fprintf(fp, " %-8.8s", ufptoa(pp->filter_delay[i],4));
		if (i == (NTP_SHIFT>>1)-1)
			(void) fprintf(fp, "\n              ");
	}
	(void) fprintf(fp, "\n");

	(void) fprintf(fp, "filter offset:");
	for (i = 0; i < PEER_SHIFT; i++) {
		(void) fprintf(fp, " %-8.8s", lfptoa(&pp->filter_offset[i], 5));
		if (i == (PEER_SHIFT>>1)-1)
			(void) fprintf(fp, "\n              ");
	}
	(void) fprintf(fp, "\n");

	(void) fprintf(fp, "delay %s, dispersion %s\n",
	    ufptoa(pp->delay, 4), ufptoa(pp->dispersion, 4));

	(void) fprintf(fp, "offset %s\n\n",
	    lfptoa(&pp->offset, 7));
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

