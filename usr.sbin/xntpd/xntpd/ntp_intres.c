/* ntp_intres.c,v 3.1 1993/07/06 01:11:16 jbj Exp
 * ripped off from ../xnptres/xntpres.c by Greg Troxel 4/2/92
 * routine callable from xntpd, rather than separate program
 * also, key info passed in via a global, so no key file needed.
 */

/*
 * xntpres - process configuration entries which require use of the resolver
 *
 * This is meant to be run by xntpd on the fly.  It is not guaranteed
 * to work properly if run by hand.  This is actually a quick hack to
 * stave off violence from people who hate using numbers in the
 * configuration file (at least I hope the rest of the daemon is
 * better than this).  Also might provide some ideas about how one
 * might go about autoconfiguring an NTP distribution network.
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

#include "ntpd.h"
#include "ntp_select.h"
#include "ntp_io.h"
#include "ntp_request.h"
#include "ntp_stdlib.h"
#include "ntp_syslog.h"

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

/*
 * Each item we are to resolve and configure gets one of these
 * structures defined for it.
 */
struct conf_entry {
	struct conf_entry *ce_next;
	char *ce_name;			/* name we are trying to resolve */
	struct conf_peer ce_config;	/* configuration info for peer */
};
#define	ce_peeraddr	ce_config.peeraddr
#define	ce_hmode	ce_config.hmode
#define	ce_version	ce_config.version
#define ce_minpoll	ce_config.minpoll
#define ce_maxpoll	ce_config.maxpoll
#define	ce_flags	ce_config.flags
#define	ce_keyid	ce_config.keyid

/*
 * confentries is a pointer to the list of configuration entries
 * we have left to do.
 */
struct conf_entry *confentries = NULL;

/*
 * We take an interrupt every thirty seconds, at which time we decrement
 * config_timer and resolve_timer.  The former is set to 2, so we retry
 * unsucessful reconfigurations every minute.  The latter is set to
 * an exponentially increasing value which starts at 2 and increases to
 * 32.  When this expires we retry failed name resolutions.
 *
 * We sleep SLEEPTIME seconds before doing anything, to give the server
 * time to arrange itself.
 */
#define	MINRESOLVE	2
#define	MAXRESOLVE	32
#define	CONFIG_TIME	2
#define	ALARM_TIME	30

#define	SLEEPTIME	2

static	int config_timer = 0;
static	int resolve_timer = 0;

static	int resolve_value;	/* next value of resolve timer */

/*
 * Big hack attack
 */
#define	LOCALHOST	0x7f000001	/* 127.0.0.1, in hex, of course */
#define	SKEWTIME	0x08000000	/* 0.03125 seconds as a l_fp fraction */

/*
 * Select time out.  Set to 2 seconds.  The server is on the local machine,
 * after all.
 */
#define	TIMEOUT_SEC	2
#define	TIMEOUT_USEC	0


/*
 * Input processing.  The data on each line in the configuration file
 * is supposed to consist of entries in the following order
 */
#define	TOK_HOSTNAME	0
#define	TOK_HMODE	1
#define	TOK_VERSION	2
#define TOK_MINPOLL	3
#define TOK_MAXPOLL	4
#define	TOK_FLAGS	5
#define	TOK_KEYID	6
#define	NUMTOK		7

#define	MAXLINESIZE	512


/*
 * File descriptor for ntp request code.
 */
static	int sockfd = -1;


/* stuff to be filled in by caller */

U_LONG req_keyid;	/* request keyid */
char *req_file;		/* name of the file with configuration info */

/* end stuff to be filled in */


extern int debug;		/* use global debug flag */
extern int errno;

static	RETSIGTYPE bong		P((int));
static	void	checkparent	P((void));
static	void	removeentry	P((struct conf_entry *));
static	void	addentry	P((char *, int, int, int, int, int, U_LONG));
static	int	findhostaddr	P((struct conf_entry *));
static	void	openntp		P((void));
static	int	request	P((struct conf_peer *));
static	char *	nexttoken	P((char **));
static	void	readconf	P((FILE *, char *));
static	void	doconfigure	P((int));

/*
 * assumes:  req_key, req_keyid, conffile valid
 *  syslog still open
 */
void
ntp_intres()
{
	FILE *in;

	if ( debug )
	  syslog(LOG_INFO, "ntp_intres running");


	/* check out auth stuff */
	if (!authhavekey(req_keyid)) {
		syslog(LOG_ERR, "request keyid %lu not found",
		    req_keyid );
		exit(1);
	}

	/*
	 * Read the configuration info
	 * {this is bogus, since we are forked, but it is easier
	 * to keep this code - gdt}
	 */
	if ((in = fopen(req_file, "r")) == NULL) {
		syslog(LOG_ERR, "can't open configuration file %s: %m",
		    req_file);
		exit(1);
	}
	readconf(in, req_file);
	(void) fclose(in);

	if ( ! debug )
		(void) unlink(req_file);

	/*
	 * Sleep a little to make sure the server is completely up
	 */
	sleep(SLEEPTIME);

	/*
	 * Make a first cut at resolving the bunch
	 */
	doconfigure(1);
	if (confentries == NULL)
		exit(0);		/* done that quick */
	
	/*
	 * Here we've got some problem children.  Set up the timer
	 * and wait for it.
	 */
	resolve_value = resolve_timer = MINRESOLVE;
	config_timer = CONFIG_TIME;
	(void) signal_no_reset(SIGALRM, bong);
	alarm(ALARM_TIME);

	for (;;) {
		if (confentries == NULL)
			exit(0);
		checkparent();
		if (resolve_timer == 0) {
			if (resolve_value < MAXRESOLVE)
				resolve_value <<= 1;
			resolve_timer = resolve_value;
			config_timer = CONFIG_TIME;
			doconfigure(1);
			continue;
		} else if (config_timer == 0) {
			config_timer = CONFIG_TIME;
			doconfigure(0);
			continue;
		}
		/*
		 * There is a race in here.  Is okay, though, since
		 * all it does is delay things by 30 seconds.
		 */
		(void) pause();
	}
}


/*
 * bong - service and reschedule an alarm() interrupt
 */
static RETSIGTYPE
bong(sig)
int sig;
{
	if (config_timer > 0)
		config_timer--;
	if (resolve_timer > 0)
		resolve_timer--;
	alarm(ALARM_TIME);
}


/*
 * checkparent - see if our parent process is still running
 */
static void
checkparent()
{
	/*
	 * If our parent (the server) has died we will have been
	 * inherited by init.  If so, exit.
	 */
	if (getppid() == 1) {
		syslog(LOG_INFO, "parent died before we finished, exiting");
		exit(0);
	}
}


/*
 * removeentry - we are done with an entry, remove it from the list
 */
static void
removeentry(entry)
	struct conf_entry *entry;
{
	register struct conf_entry *ce;

	ce = confentries;
	if (ce == entry) {
		confentries = ce->ce_next;
		return;
	}

	while (ce != NULL) {
		if (ce->ce_next == entry) {
			ce->ce_next = entry->ce_next;
			return;
		}
		ce = ce->ce_next;
	}
}


/*
 * addentry - add an entry to the configuration list
 */
static void
addentry(name, mode, version, minpoll, maxpoll, flags, keyid)
	char *name;
	int mode;
	int version;
	int minpoll;
	int maxpoll;
	int flags;
	U_LONG keyid;
{
	register char *cp;
	register struct conf_entry *ce;
	int len;

	len = strlen(name) + 1;
	cp = emalloc((unsigned)len);
	bcopy(name, cp, len);

	ce = (struct conf_entry *)emalloc(sizeof(struct conf_entry));
	ce->ce_name = cp;
	ce->ce_peeraddr = 0;
	ce->ce_hmode = (u_char)mode;
	ce->ce_version = (u_char)version;
	ce->ce_minpoll = (u_char)minpoll;
	ce->ce_maxpoll = (u_char)maxpoll;
	ce->ce_flags = (u_char)flags;
	ce->ce_keyid = htonl(keyid);
	ce->ce_next = NULL;

	if (confentries == NULL) {
		confentries = ce;
	} else {
		register struct conf_entry *cep;

		for (cep = confentries; cep->ce_next != NULL;
		    cep = cep->ce_next)
			/* nothing */;
		cep->ce_next = ce;
	}
}


/*
 * findhostaddr - resolve a host name into an address
 *
 * The routine sticks the address into the entry's ce_peeraddr if it
 * gets one.  It returns 1 for "success" and 0 for an uncorrectable
 * failure.  Note that "success" includes try again errors.  You can
 * tell that you got a try again since ce_peeraddr will still be zero.
 */
static int
findhostaddr(entry)
	struct conf_entry *entry;
{
	struct hostent *hp;

	checkparent();		/* make sure our guy is still running */

	hp = gethostbyname(entry->ce_name);

	if (hp == NULL) {
#ifndef NODNS
		/*
		 * If the resolver is in use, see if the failure is
		 * temporary.  If so, return success.
		 */
		extern int h_errno;

		if (h_errno == TRY_AGAIN)
			return 1;
#endif
		return 0;
	}

	/*
	 * Use the first address.  We don't have any way to
	 * tell preferences and older gethostbyname() implementations
	 * only return one.
	 */
	(void) bcopy(hp->h_addr, (char *)&(entry->ce_peeraddr),
	    sizeof(struct in_addr));
	return 1;
}


/*
 * openntp - open a socket to the ntp server
 */
static void
openntp()
{
	struct sockaddr_in saddr;

	if (sockfd >= 0)
		return;
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1) {
		syslog(LOG_ERR, "socket() failed: %m");
		exit(1);
	}

	bzero((char *)&saddr, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(NTP_PORT);		/* trash */
	saddr.sin_addr.s_addr = htonl(LOCALHOST);	/* garbage */

        /*
         * Make the socket non-blocking.  We'll wait with select()
         */
#if defined(O_NONBLOCK)
        if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
                syslog(LOG_ERR, "fcntl(O_NONBLOCK) failed: %m");
                exit(1);
        }
#else
#if defined(FNDELAY)
        if (fcntl(sockfd, F_SETFL, FNDELAY) == -1) {
                syslog(LOG_ERR, "fcntl(FNDELAY) failed: %m");
                exit(1);
        }
#else
NEED NON BLOCKING IO
#endif
#endif


	if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
		syslog(LOG_ERR, "connect() failed: %m");
		exit(1);
	}
}


/*
 * request - send a configuration request to the server, wait for a response
 */
static int
request(conf)
	struct conf_peer *conf;
{
	fd_set fdset;
	struct timeval tvout;
	struct req_pkt reqpkt;
	l_fp ts;
	int n;

	checkparent();		/* make sure our guy is still running */

	if (sockfd < 0)
		openntp();
	
	/*
	 * Try to clear out any previously received traffic so it
	 * doesn't fool us.  Note the socket is nonblocking.
	 */
	tvout.tv_sec =  0;
	tvout.tv_usec = 0;
	FD_ZERO(&fdset);
	FD_SET(sockfd, &fdset);
	while (select(sockfd + 1, &fdset, (fd_set *)0, (fd_set *)0, &tvout) >
	       0) {
	       read(sockfd, (char *)&reqpkt, REQ_LEN_MAC);
	       FD_ZERO(&fdset);
	       FD_SET(sockfd, &fdset);
	}

	/*
	 * Make up a request packet with the configuration info
	 */
	bzero((char *)&reqpkt, sizeof(reqpkt));

	reqpkt.rm_vn_mode = RM_VN_MODE(0, 0);
	reqpkt.auth_seq = AUTH_SEQ(1, 0);	/* authenticated, no seq */
	reqpkt.implementation = IMPL_XNTPD;	/* local implementation */
	reqpkt.request = REQ_CONFIG;		/* configure a new peer */
	reqpkt.err_nitems = ERR_NITEMS(0, 1);	/* one item */
	reqpkt.mbz_itemsize = MBZ_ITEMSIZE(sizeof(struct conf_peer));
	bcopy((char *)conf, reqpkt.data, sizeof(struct conf_peer));
	reqpkt.keyid = htonl(req_keyid);

	auth1crypt(req_keyid, (U_LONG *)&reqpkt, REQ_LEN_NOMAC);
	gettstamp(&ts);
	M_ADDUF(ts.l_ui, ts.l_uf, SKEWTIME);
	HTONL_FP(&ts, &reqpkt.tstamp);
	n = auth2crypt(req_keyid, (U_LONG *)&reqpkt, REQ_LEN_NOMAC);

	/*
	 * Done.  Send it.
	 */
	n = write(sockfd, (char *)&reqpkt, REQ_LEN_NOMAC + n);
	if (n < 0) {
		syslog(LOG_ERR, "send to NTP server failed: %m");
		return 0;	/* maybe should exit */
	}

	/*
	 * Wait for a response.  A weakness of the mode 7 protocol used
	 * is that there is no way to associate a response with a
	 * particular request, i.e. the response to this configuration
	 * request is indistinguishable from that to any other.  I should
	 * fix this some day.  In any event, the time out is fairly
	 * pessimistic to make sure that if an answer is coming back
	 * at all, we get it.
	 */
	for (;;) {
		FD_ZERO(&fdset);
		FD_SET(sockfd, &fdset);
		tvout.tv_sec = TIMEOUT_SEC;
		tvout.tv_usec = TIMEOUT_USEC;

		n = select(sockfd + 1, &fdset, (fd_set *)0,
		    (fd_set *)0, &tvout);

		if (n <= 0) {
			if (n < 0)
				syslog(LOG_ERR, "select() fails: %m");
			return 0;
		}

		n = read(sockfd, (char *)&reqpkt, REQ_LEN_MAC);
		if (n <= 0) {
			if (n < 0) {
				syslog(LOG_ERR, "read() fails: %m");
				return 0;
			}
			continue;
		}

		/*
		 * Got one.  Check through to make sure it is what
		 * we expect.
		 */
		if (n < RESP_HEADER_SIZE) {
			syslog(LOG_ERR, "received runt response (%d octets)",
			    n);
			continue;
		}

		if (!ISRESPONSE(reqpkt.rm_vn_mode)) {
#ifdef DEBUG
			if (debug > 1)
				printf("received non-response packet\n");
#endif
			continue;
		}

		if (ISMORE(reqpkt.rm_vn_mode)) {
#ifdef DEBUG
			if (debug > 1)
				printf("received fragmented packet\n");
#endif
			continue;
		}

		if (INFO_VERSION(reqpkt.rm_vn_mode) != NTP_VERSION
		    || INFO_MODE(reqpkt.rm_vn_mode) != MODE_PRIVATE) {
#ifdef DEBUG
			if (debug > 1)
				printf("version (%d) or mode (%d) incorrect\n",
				    INFO_VERSION(reqpkt.rm_vn_mode),
				    INFO_MODE(reqpkt.rm_vn_mode));
#endif
			continue;
		}

		if (INFO_SEQ(reqpkt.auth_seq) != 0) {
#ifdef DEBUG
			if (debug > 1)
				printf("nonzero sequence number (%d)\n",
				    INFO_SEQ(reqpkt.auth_seq));
#endif
			continue;
		}

		if (reqpkt.implementation != IMPL_XNTPD ||
		    reqpkt.request != REQ_CONFIG) {
#ifdef DEBUG
			if (debug > 1)
				printf(
			    "implementation (%d) or request (%d) incorrect\n",
				    reqpkt.implementation, reqpkt.request);
#endif
			continue;
		}

		if (INFO_NITEMS(reqpkt.err_nitems) != 0 ||
		    INFO_MBZ(reqpkt.mbz_itemsize) != 0 ||
		    INFO_ITEMSIZE(reqpkt.mbz_itemsize != 0)) {
#ifdef DEBUG
			if (debug > 1)
				printf(
			    "nitems (%d) mbz (%d) or itemsize (%d) nonzero\n",
				    INFO_NITEMS(reqpkt.err_nitems),
				    INFO_MBZ(reqpkt.mbz_itemsize),
				    INFO_ITEMSIZE(reqpkt.mbz_itemsize));
#endif
			continue;
		}

		n = INFO_ERR(reqpkt.err_nitems);
		switch (n) {
		case INFO_OKAY:
			/* success */
			return 1;
		
		case INFO_ERR_IMPL:
			syslog(LOG_ERR,
			    "server reports implementation mismatch!!");
			return 0;
		
		case INFO_ERR_REQ:
			syslog(LOG_ERR,
			    "server claims configuration request is unknown");
			return 0;
		
		case INFO_ERR_FMT:
			syslog(LOG_ERR,
			    "server indicates a format error occured(!!)");
			return 0;

		case INFO_ERR_NODATA:
			syslog(LOG_ERR,
		"server indicates no data available (shouldn't happen)");
			return 0;
		
		case INFO_ERR_AUTH:
			syslog(LOG_ERR,
			    "server returns a permission denied error");
			return 0;

		default:
			syslog(LOG_ERR,
			    "server returns unknown error code %d", n);
			return 0;
		}
	}
}


/*
 * nexttoken - return the next token from a line
 */
static char *
nexttoken(lptr)
	char **lptr;
{
	register char *cp;
	register char *tstart;

	cp = *lptr;

	/*
	 * Skip leading white space
	 */
	while (*cp == ' ' || *cp == '\t')
		cp++;
	
	/*
	 * If this is the end of the line, return nothing.
	 */
	if (*cp == '\n' || *cp == '\0') {
		*lptr = cp;
		return NULL;
	}
	
	/*
	 * Must be the start of a token.  Record the pointer and look
	 * for the end.
	 */
	tstart = cp++;
	while (*cp != ' ' && *cp != '\t' && *cp != '\n' && *cp != '\0')
		cp++;
	
	/*
	 * Terminate the token with a \0.  If this isn't the end of the
	 * line, space to the next character.
	 */
	if (*cp == '\n' || *cp == '\0')
		*cp = '\0';
	else
		*cp++ = '\0';

	*lptr = cp;
	return tstart;
}


/*
 * readconf - read the configuration information out of the file we
 *	      were passed.  Note that since the file is supposed to be
 *	      machine generated, we bail out at the first sign of trouble.
 */
static void
readconf(fp, name)
	FILE *fp;
	char *name;
{
	register int i;
	char *token[NUMTOK];
	U_LONG intval[NUMTOK];
	int flags;
	char buf[MAXLINESIZE];
	char *bp;

	while (fgets(buf, MAXLINESIZE, fp) != NULL) {

		bp = buf;
		for (i = 0; i < NUMTOK; i++) {
			if ((token[i] = nexttoken(&bp)) == NULL) {
				syslog(LOG_ERR,
				    "tokenizing error in file `%s', quitting",
				    name);
				exit(1);
			}
		}

		for (i = 1; i < NUMTOK; i++) {
			if (!atouint(token[i], &intval[i])) {
				syslog(LOG_ERR,
		 "format error for integer token `%s', file `%s', quitting",
				    token[i], name);
				exit(1);
			}
		}

		if (intval[TOK_HMODE] != MODE_ACTIVE &&
		    intval[TOK_HMODE] != MODE_CLIENT &&
		    intval[TOK_HMODE] != MODE_BROADCAST) {
			syslog(LOG_ERR, "invalid mode (%d) in file %s",
			    intval[TOK_HMODE], name);
			exit(1);
		}

		if (intval[TOK_VERSION] > NTP_VERSION ||
		    intval[TOK_VERSION] < NTP_OLDVERSION) {
			syslog(LOG_ERR, "invalid version (%d) in file %s",
			    intval[TOK_VERSION], name);
			exit(1);
		}
		if (intval[TOK_MINPOLL] < NTP_MINPOLL ||
		    intval[TOK_MINPOLL] > NTP_MAXPOLL) {
			syslog(LOG_ERR, "invalid MINPOLL value (%d) in file %s",
			       intval[TOK_MINPOLL], name);
			exit(1);
		}

		if (intval[TOK_MAXPOLL] < NTP_MINPOLL ||
		    intval[TOK_MAXPOLL] > NTP_MAXPOLL) {
			syslog(LOG_ERR, "invalid MAXPOLL value (%d) in file %s",
			       intval[TOK_MAXPOLL], name);
			exit(1);
		}

		if ((intval[TOK_FLAGS] & ~(FLAG_AUTHENABLE|FLAG_PREFER))
		    != 0) {
			syslog(LOG_ERR, "invalid flags (%d) in file %s",
			    intval[TOK_FLAGS], name);
			exit(1);
		}

		flags = 0;
		if (intval[TOK_FLAGS] & FLAG_AUTHENABLE)
			flags |= CONF_FLAG_AUTHENABLE;
		if (intval[TOK_FLAGS] & FLAG_PREFER)
			flags |= CONF_FLAG_PREFER;
		
		/*
		 * This is as good as we can check it.  Add it in.
		 */
  		addentry(token[TOK_HOSTNAME], (int)intval[TOK_HMODE],
		    (int)intval[TOK_VERSION], (int)intval[TOK_MINPOLL],
		    (int)intval[TOK_MAXPOLL], flags, intval[TOK_KEYID]);
	}
}


/*
 * doconfigure - attempt to resolve names and configure the server
 */
static void
doconfigure(dores)
	int dores;
{
	register struct conf_entry *ce;
	register struct conf_entry *ceremove;

	ce = confentries;
	while (ce != NULL) {
		if (dores && ce->ce_peeraddr == 0) {
			if (!findhostaddr(ce)) {
				syslog(LOG_ERR,
				    "couldn't resolve `%s', giving up on it",
				    ce->ce_name);
				ceremove = ce;
				ce = ceremove->ce_next;
				removeentry(ceremove);
				continue;
			}
		}

		if (ce->ce_peeraddr != 0) {
			if (request(&ce->ce_config)) {
				ceremove = ce;
				ce = ceremove->ce_next;
				removeentry(ceremove);
				continue;
			}
		}
		ce = ce->ce_next;
	}
}
