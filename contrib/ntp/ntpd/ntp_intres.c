/*
 * ripped off from ../ntpres/ntpres.c by Greg Troxel 4/2/92
 * routine callable from ntpd, rather than separate program
 * also, key info passed in via a global, so no key file needed.
 */

/*
 * ntpres - process configuration entries which require use of the resolver
 *
 * This is meant to be run by ntpd on the fly.  It is not guaranteed
 * to work properly if run by hand.  This is actually a quick hack to
 * stave off violence from people who hate using numbers in the
 * configuration file (at least I hope the rest of the daemon is
 * better than this).  Also might provide some ideas about how one
 * might go about autoconfiguring an NTP distribution network.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_request.h"
#include "ntp_stdlib.h"
#include "ntp_syslog.h"

#include <stdio.h>
#include <ctype.h>
#include <netdb.h>
#include <signal.h>

/**/
#include <netinet/in.h>
#include <arpa/inet.h>
/**/
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>		/* MAXHOSTNAMELEN (often) */
#endif

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
#define ce_ttl		ce_config.ttl
#define	ce_keyid	ce_config.keyid
#define ce_keystr	ce_config.keystr

/*
 * confentries is a pointer to the list of configuration entries
 * we have left to do.
 */
static struct conf_entry *confentries = NULL;

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

static	volatile int config_timer = 0;
static	volatile int resolve_timer = 0;

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
#define TOK_TTL		6
#define	TOK_KEYID	7
#define TOK_KEYSTR	8
#define	NUMTOK		9

#define	MAXLINESIZE	512


/*
 * File descriptor for ntp request code.
 */
static	int sockfd = -1;


/* stuff to be filled in by caller */

keyid_t req_keyid;	/* request keyid */
char *req_file;		/* name of the file with configuration info */

/* end stuff to be filled in */


static	RETSIGTYPE bong		P((int));
static	void	checkparent	P((void));
static	void	removeentry	P((struct conf_entry *));
static	void	addentry	P((char *, int, int, int, int, u_int,
				   int, keyid_t, char *));
static	int	findhostaddr	P((struct conf_entry *));
static	void	openntp		P((void));
static	int	request		P((struct conf_peer *));
static	char *	nexttoken	P((char **));
static	void	readconf	P((FILE *, char *));
static	void	doconfigure	P((int));

struct ntp_res_t_pkt {		/* Tagged packet: */
	void *tag;		/* For the caller */
	u_int32 paddr;		/* IP to look up, or 0 */
	char name[MAXHOSTNAMELEN]; /* Name to look up (if 1st byte is not 0) */
};

struct ntp_res_c_pkt {		/* Control packet: */
	char name[MAXHOSTNAMELEN];
	u_int32 paddr;
	int mode;
	int version;
	int minpoll;
	int maxpoll;
	u_int flags;
	int ttl;
	keyid_t keyid;
	u_char keystr[MAXFILENAME];
};


/*
 * ntp_res_recv: Process an answer from the resolver
 */

void
ntp_res_recv(void)
{
	/*
	  We have data ready on our descriptor.
	  It may be an EOF, meaning the resolver process went away.
	  Otherwise, it will be an "answer".
	*/
}


/*
 * ntp_intres needs;
 *
 *	req_key(???), req_keyid, req_file valid
 *	syslog still open
 */

void
ntp_intres(void)
{
	FILE *in;
#ifdef HAVE_SIGSUSPEND
	sigset_t set;

	sigemptyset(&set);
#endif /* HAVE_SIGSUSPEND */

#ifdef DEBUG
	if (debug > 1) {
		msyslog(LOG_INFO, "NTP_INTRES running");
	}
#endif

	/* check out auth stuff */
	if (sys_authenticate) {
		if (!authistrusted(req_keyid)) {
			msyslog(LOG_ERR, "invalid request keyid %08x",
			    req_keyid );
			exit(1);
		}
	}

	/*
	 * Read the configuration info
	 * {this is bogus, since we are forked, but it is easier
	 * to keep this code - gdt}
	 */
	if ((in = fopen(req_file, "r")) == NULL) {
		msyslog(LOG_ERR, "can't open configuration file %s: %m",
			req_file);
		exit(1);
	}
	readconf(in, req_file);
	(void) fclose(in);

	if (!debug )
		(void) unlink(req_file);

	/*
	 * Sleep a little to make sure the server is completely up
	 */

	sleep(SLEEPTIME);

	/*
	 * Make a first cut at resolving the bunch
	 */
	doconfigure(1);
	if (confentries == NULL) {
#if defined SYS_WINNT
		ExitThread(0);	/* Don't want to kill whole NT process */
#else
		exit(0);	/* done that quick */
#endif
	}
	
	/*
	 * Here we've got some problem children.  Set up the timer
	 * and wait for it.
	 */
	resolve_value = resolve_timer = MINRESOLVE;
	config_timer = CONFIG_TIME;
#ifndef SYS_WINNT
	(void) signal_no_reset(SIGALRM, bong);
	alarm(ALARM_TIME);
#endif /* SYS_WINNT */

	for (;;) {
		if (confentries == NULL)
		    exit(0);

		checkparent();

		if (resolve_timer == 0) {
			if (resolve_value < MAXRESOLVE)
			    resolve_value <<= 1;
			resolve_timer = resolve_value;
#ifdef DEBUG
			if (debug > 2)
				msyslog(LOG_INFO, "resolve_timer: 0->%d", resolve_timer);
#endif
			config_timer = CONFIG_TIME;
			doconfigure(1);
			continue;
		} else if (config_timer == 0) {
			config_timer = CONFIG_TIME;
#ifdef DEBUG
			if (debug > 2)
				msyslog(LOG_INFO, "config_timer: 0->%d", config_timer);
#endif
			doconfigure(0);
			continue;
		}
#ifndef SYS_WINNT
		/*
		 * There is a race in here.  Is okay, though, since
		 * all it does is delay things by 30 seconds.
		 */
# ifdef HAVE_SIGSUSPEND
		sigsuspend(&set);
# else
		sigpause(0);
# endif /* HAVE_SIGSUSPEND */
#else
		if (config_timer > 0)
		    config_timer--;
		if (resolve_timer > 0)
		    resolve_timer--;
		sleep(ALARM_TIME);
#endif /* SYS_WINNT */
	}
}


#ifndef SYS_WINNT
/*
 * bong - service and reschedule an alarm() interrupt
 */
static RETSIGTYPE
bong(
	int sig
	)
{
	if (config_timer > 0)
	    config_timer--;
	if (resolve_timer > 0)
	    resolve_timer--;
	alarm(ALARM_TIME);
}
#endif /* SYS_WINNT */

/*
 * checkparent - see if our parent process is still running
 *
 * No need to worry in the Windows NT environment whether the
 * main thread is still running, because if it goes
 * down it takes the whole process down with it (in
 * which case we won't be running this thread either)
 * Turn function into NOP;
 */

static void
checkparent(void)
{
#if !defined (SYS_WINNT) && !defined (SYS_VXWORKS)

	/*
	 * If our parent (the server) has died we will have been
	 * inherited by init.  If so, exit.
	 */
	if (getppid() == 1) {
		msyslog(LOG_INFO, "parent died before we finished, exiting");
		exit(0);
	}
#endif /* SYS_WINNT && SYS_VXWORKS*/
}



/*
 * removeentry - we are done with an entry, remove it from the list
 */
static void
removeentry(
	struct conf_entry *entry
	)
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
addentry(
	char *name,
	int mode,
	int version,
	int minpoll,
	int maxpoll,
	u_int flags,
	int ttl,
	keyid_t keyid,
	char *keystr
	)
{
	register char *cp;
	register struct conf_entry *ce;
	unsigned int len;

#ifdef DEBUG
	if (debug > 1)
		msyslog(LOG_INFO, 
		    "intres: <%s> %d %d %d %d %x %d %x %s\n", name,
		    mode, version, minpoll, maxpoll, flags, ttl, keyid,
		    keystr);
#endif
	len = strlen(name) + 1;
	cp = (char *)emalloc(len);
	memmove(cp, name, len);

	ce = (struct conf_entry *)emalloc(sizeof(struct conf_entry));
	ce->ce_name = cp;
	ce->ce_peeraddr = 0;
	ce->ce_hmode = (u_char)mode;
	ce->ce_version = (u_char)version;
	ce->ce_minpoll = (u_char)minpoll;
	ce->ce_maxpoll = (u_char)maxpoll;
	ce->ce_flags = (u_char)flags;
	ce->ce_ttl = (u_char)ttl;
	ce->ce_keyid = keyid;
	strncpy((char *)ce->ce_keystr, keystr, MAXFILENAME);
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
 * findhostaddr - resolve a host name into an address (Or vice-versa)
 *
 * Given one of {ce_peeraddr,ce_name}, find the other one.
 * It returns 1 for "success" and 0 for an uncorrectable failure.
 * Note that "success" includes try again errors.  You can tell that you
 *  got a "try again" since {ce_peeraddr,ce_name} will still be zero.
 */
static int
findhostaddr(
	struct conf_entry *entry
	)
{
	struct hostent *hp;
	struct in_addr in;

	checkparent();		/* make sure our guy is still running */

	if (entry->ce_name && entry->ce_peeraddr) {
		/* HMS: Squawk? */
		msyslog(LOG_ERR, "findhostaddr: both ce_name and ce_peeraddr are defined...");
		return 1;
	}

	if (!entry->ce_name && !entry->ce_peeraddr) {
		msyslog(LOG_ERR, "findhostaddr: both ce_name and ce_peeraddr are undefined!");
		return 0;
	}

	if (entry->ce_name) {
#ifdef DEBUG
		if (debug > 2)
			msyslog(LOG_INFO, "findhostaddr: Resolving <%s>",
				entry->ce_name);
#endif /* DEBUG */
		hp = gethostbyname(entry->ce_name);
	} else {
#ifdef DEBUG
		if (debug > 2)
			msyslog(LOG_INFO, "findhostaddr: Resolving %x>",
				entry->ce_peeraddr);
#endif
		in.s_addr = entry->ce_peeraddr;
		hp = gethostbyaddr((const char *)&in,
				   sizeof entry->ce_peeraddr,
				   AF_INET);
	}

	if (hp == NULL) {
		/*
		 * If the resolver is in use, see if the failure is
		 * temporary.  If so, return success.
		 */
		if (h_errno == TRY_AGAIN)
		    return (1);
		return (0);
	}

	if (entry->ce_name) {
#ifdef DEBUG
		if (debug > 2)
			msyslog(LOG_INFO, "findhostaddr: name resolved.");
#endif
		/*
		 * Use the first address.  We don't have any way to tell
		 * preferences and older gethostbyname() implementations
		 * only return one.
		 */
		memmove((char *)&(entry->ce_peeraddr),
			(char *)hp->h_addr,
			sizeof(struct in_addr));
		if (entry->ce_keystr[0] == '*')
			strncpy((char *)&(entry->ce_keystr), hp->h_name,
				MAXFILENAME);
	} else {
		char *cp;
		size_t s;

#ifdef DEBUG
		if (debug > 2)
			msyslog(LOG_INFO, "findhostaddr: address resolved.");
#endif
		s = strlen(hp->h_name) + 1;
		cp = emalloc(s);
		strcpy(cp, hp->h_name);
		entry->ce_name = cp;
	}
		   
	return (1);
}


/*
 * openntp - open a socket to the ntp server
 */
static void
openntp(void)
{
	struct sockaddr_in saddr;

	if (sockfd >= 0)
	    return;
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1) {
		msyslog(LOG_ERR, "socket() failed: %m");
		exit(1);
	}

	memset((char *)&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(NTP_PORT);		/* trash */
	saddr.sin_addr.s_addr = htonl(LOCALHOST);	/* garbage */

	/*
	 * Make the socket non-blocking.  We'll wait with select()
	 */
#ifndef SYS_WINNT
#if defined(O_NONBLOCK)
	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
		msyslog(LOG_ERR, "fcntl(O_NONBLOCK) failed: %m");
		exit(1);
	}
#else
#if defined(FNDELAY)
	if (fcntl(sockfd, F_SETFL, FNDELAY) == -1) {
		msyslog(LOG_ERR, "fcntl(FNDELAY) failed: %m");
		exit(1);
	}
#else
# include "Bletch: NEED NON BLOCKING IO"
#endif /* FNDDELAY */
#endif /* O_NONBLOCK */
#else  /* SYS_WINNT */
	{
		int on = 1;
		if (ioctlsocket(sockfd,FIONBIO,(u_long *) &on) == SOCKET_ERROR) {
			msyslog(LOG_ERR, "ioctlsocket(FIONBIO) fails: %m");
			exit(1); /* Windows NT - set socket in non-blocking mode */
		}
	}
#endif /* SYS_WINNT */


	if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1) {
		msyslog(LOG_ERR, "openntp: connect() failed: %m");
		exit(1);
	}
}


/*
 * request - send a configuration request to the server, wait for a response
 */
static int
request(
	struct conf_peer *conf
	)
{
	fd_set fdset;
	struct timeval tvout;
	struct req_pkt reqpkt;
	l_fp ts;
	int n;
#ifdef SYS_WINNT
	HANDLE hReadWriteEvent = NULL;
	BOOL ret;
	DWORD NumberOfBytesWritten, NumberOfBytesRead, dwWait;
	OVERLAPPED overlap;
#endif /* SYS_WINNT */

	checkparent();		/* make sure our guy is still running */

	if (sockfd < 0)
	    openntp();
	
#ifdef SYS_WINNT
	hReadWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif /* SYS_WINNT */

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
		recv(sockfd, (char *)&reqpkt, REQ_LEN_MAC, 0);
		FD_ZERO(&fdset);
		FD_SET(sockfd, &fdset);
	}

	/*
	 * Make up a request packet with the configuration info
	 */
	memset((char *)&reqpkt, 0, sizeof(reqpkt));

	reqpkt.rm_vn_mode = RM_VN_MODE(0, 0, 0);
	reqpkt.auth_seq = AUTH_SEQ(1, 0);	/* authenticated, no seq */
	reqpkt.implementation = IMPL_XNTPD;	/* local implementation */
	reqpkt.request = REQ_CONFIG;		/* configure a new peer */
	reqpkt.err_nitems = ERR_NITEMS(0, 1);	/* one item */
	reqpkt.mbz_itemsize = MBZ_ITEMSIZE(sizeof(struct conf_peer));
	/* Make sure mbz_itemsize <= sizeof reqpkt.data */
	if (sizeof(struct conf_peer) > sizeof (reqpkt.data)) {
		msyslog(LOG_ERR, "Bletch: conf_peer is too big for reqpkt.data!");
		exit(1);
	}
	memmove(reqpkt.data, (char *)conf, sizeof(struct conf_peer));
	reqpkt.keyid = htonl(req_keyid);

	get_systime(&ts);
	L_ADDUF(&ts, SKEWTIME);
	HTONL_FP(&ts, &reqpkt.tstamp);
	n = 0;
	if (sys_authenticate)
		n = authencrypt(req_keyid, (u_int32 *)&reqpkt, REQ_LEN_NOMAC);

	/*
	 * Done.  Send it.
	 */
#ifndef SYS_WINNT
	n = send(sockfd, (char *)&reqpkt, (unsigned)(REQ_LEN_NOMAC + n), 0);
	if (n < 0) {
		msyslog(LOG_ERR, "send to NTP server failed: %m");
		return 0;	/* maybe should exit */
	}
#else
	/* In the NT world, documentation seems to indicate that there
	 * exist _write and _read routines that can be used to do blocking
	 * I/O on sockets. Problem is these routines require a socket
	 * handle obtained through the _open_osf_handle C run-time API
	 * of which there is no explanation in the documentation. We need
	 * nonblocking write's and read's anyway for our purpose here.
	 * We're therefore forced to deviate a little bit from the Unix
	 * model here and use the ReadFile and WriteFile Win32 I/O API's
	 * on the socket
	 */
	overlap.Offset = overlap.OffsetHigh = (DWORD)0;
	overlap.hEvent = hReadWriteEvent;
	ret = WriteFile((HANDLE)sockfd, (char *)&reqpkt, REQ_LEN_NOMAC + n,
			(LPDWORD)&NumberOfBytesWritten, (LPOVERLAPPED)&overlap);
	if ((ret == FALSE) && (GetLastError() != ERROR_IO_PENDING)) {
		msyslog(LOG_ERR, "send to NTP server failed: %m");
		return 0;
	}
	dwWait = WaitForSingleObject(hReadWriteEvent, (DWORD) TIMEOUT_SEC * 1000);
	if ((dwWait == WAIT_FAILED) || (dwWait == WAIT_TIMEOUT)) {
		if (dwWait == WAIT_FAILED)
		    msyslog(LOG_ERR, "WaitForSingleObject failed: %m");
		return 0;
	}
#endif /* SYS_WINNT */
    

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

		if (n < 0)
		{
			msyslog(LOG_ERR, "select() fails: %m");
			return 0;
		}
		else if (n == 0)
		{
			if (debug)
			    msyslog(LOG_INFO, "select() returned 0.");
			return 0;
		}

#ifndef SYS_WINNT
		n = recv(sockfd, (char *)&reqpkt, REQ_LEN_MAC, 0);
		if (n <= 0) {
			if (n < 0) {
				msyslog(LOG_ERR, "recv() fails: %m");
				return 0;
			}
			continue;
		}
#else /* Overlapped I/O used on non-blocking sockets on Windows NT */
		ret = ReadFile((HANDLE)sockfd, (char *)&reqpkt, (DWORD)REQ_LEN_MAC,
			       (LPDWORD)&NumberOfBytesRead, (LPOVERLAPPED)&overlap);
		if ((ret == FALSE) && (GetLastError() != ERROR_IO_PENDING)) {
			msyslog(LOG_ERR, "ReadFile() fails: %m");
			return 0;
		}
		dwWait = WaitForSingleObject(hReadWriteEvent, (DWORD) TIMEOUT_SEC * 1000);
		if ((dwWait == WAIT_FAILED) || (dwWait == WAIT_TIMEOUT)) {
			if (dwWait == WAIT_FAILED) {
				msyslog(LOG_ERR, "WaitForSingleObject fails: %m");
				return 0;
			}
			continue;
		}
		n = NumberOfBytesRead;
#endif /* SYS_WINNT */

		/*
		 * Got one.  Check through to make sure it is what
		 * we expect.
		 */
		if (n < RESP_HEADER_SIZE) {
			msyslog(LOG_ERR, "received runt response (%d octets)",
				n);
			continue;
		}

		if (!ISRESPONSE(reqpkt.rm_vn_mode)) {
#ifdef DEBUG
			if (debug > 1)
			    msyslog(LOG_INFO, "received non-response packet");
#endif
			continue;
		}

		if (ISMORE(reqpkt.rm_vn_mode)) {
#ifdef DEBUG
			if (debug > 1)
			    msyslog(LOG_INFO, "received fragmented packet");
#endif
			continue;
		}

		if ( ( (INFO_VERSION(reqpkt.rm_vn_mode) < 2)
		       || (INFO_VERSION(reqpkt.rm_vn_mode) > NTP_VERSION))
		     || INFO_MODE(reqpkt.rm_vn_mode) != MODE_PRIVATE) {
#ifdef DEBUG
			if (debug > 1)
			    msyslog(LOG_INFO,
				    "version (%d/%d) or mode (%d/%d) incorrect",
				    INFO_VERSION(reqpkt.rm_vn_mode),
				    NTP_VERSION,
				    INFO_MODE(reqpkt.rm_vn_mode),
				    MODE_PRIVATE);
#endif
			continue;
		}

		if (INFO_SEQ(reqpkt.auth_seq) != 0) {
#ifdef DEBUG
			if (debug > 1)
			    msyslog(LOG_INFO,
				    "nonzero sequence number (%d)",
				    INFO_SEQ(reqpkt.auth_seq));
#endif
			continue;
		}

		if (reqpkt.implementation != IMPL_XNTPD ||
		    reqpkt.request != REQ_CONFIG) {
#ifdef DEBUG
			if (debug > 1)
			    msyslog(LOG_INFO,
				    "implementation (%d) or request (%d) incorrect",
				    reqpkt.implementation, reqpkt.request);
#endif
			continue;
		}

		if (INFO_NITEMS(reqpkt.err_nitems) != 0 ||
		    INFO_MBZ(reqpkt.mbz_itemsize) != 0 ||
		    INFO_ITEMSIZE(reqpkt.mbz_itemsize) != 0) {
#ifdef DEBUG
			if (debug > 1)
			    msyslog(LOG_INFO,
				    "nitems (%d) mbz (%d) or itemsize (%d) nonzero",
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
			msyslog(LOG_ERR,
				"server reports implementation mismatch!!");
			return 0;
		
		    case INFO_ERR_REQ:
			msyslog(LOG_ERR,
				"server claims configuration request is unknown");
			return 0;
		
		    case INFO_ERR_FMT:
			msyslog(LOG_ERR,
				"server indicates a format error occurred(!!)");
			return 0;

		    case INFO_ERR_NODATA:
			msyslog(LOG_ERR,
				"server indicates no data available (shouldn't happen)");
			return 0;
		
		    case INFO_ERR_AUTH:
			msyslog(LOG_ERR,
				"server returns a permission denied error");
			return 0;

		    default:
			msyslog(LOG_ERR,
				"server returns unknown error code %d", n);
			return 0;
		}
	}
}


/*
 * nexttoken - return the next token from a line
 */
static char *
nexttoken(
	char **lptr
	)
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
readconf(
	FILE *fp,
	char *name
	)
{
	register int i;
	char *token[NUMTOK];
	u_long intval[NUMTOK];
	u_int flags;
	char buf[MAXLINESIZE];
	char *bp;

	while (fgets(buf, MAXLINESIZE, fp) != NULL) {

		bp = buf;
		for (i = 0; i < NUMTOK; i++) {
			if ((token[i] = nexttoken(&bp)) == NULL) {
				msyslog(LOG_ERR,
					"tokenizing error in file `%s', quitting",
					name);
				exit(1);
			}
		}

		for (i = 1; i < NUMTOK - 1; i++) {
			if (!atouint(token[i], &intval[i])) {
				msyslog(LOG_ERR,
					"format error for integer token `%s', file `%s', quitting",
					token[i], name);
				exit(1);
			}
		}

		if (intval[TOK_HMODE] != MODE_ACTIVE &&
		    intval[TOK_HMODE] != MODE_CLIENT &&
		    intval[TOK_HMODE] != MODE_BROADCAST) {
			msyslog(LOG_ERR, "invalid mode (%ld) in file %s",
				intval[TOK_HMODE], name);
			exit(1);
		}

		if (intval[TOK_VERSION] > NTP_VERSION ||
		    intval[TOK_VERSION] < NTP_OLDVERSION) {
			msyslog(LOG_ERR, "invalid version (%ld) in file %s",
				intval[TOK_VERSION], name);
			exit(1);
		}
		if (intval[TOK_MINPOLL] < NTP_MINPOLL ||
		    intval[TOK_MINPOLL] > NTP_MAXPOLL) {
			msyslog(LOG_ERR, "invalid MINPOLL value (%ld) in file %s",
				intval[TOK_MINPOLL], name);
			exit(1);
		}

		if (intval[TOK_MAXPOLL] < NTP_MINPOLL ||
		    intval[TOK_MAXPOLL] > NTP_MAXPOLL) {
			msyslog(LOG_ERR, "invalid MAXPOLL value (%ld) in file %s",
				intval[TOK_MAXPOLL], name);
			exit(1);
		}

		if ((intval[TOK_FLAGS] & ~(FLAG_AUTHENABLE | FLAG_PREFER |
		    FLAG_NOSELECT | FLAG_BURST | FLAG_IBURST | FLAG_SKEY))
		    != 0) {
			msyslog(LOG_ERR, "invalid flags (%ld) in file %s",
				intval[TOK_FLAGS], name);
			exit(1);
		}

		flags = 0;
		if (intval[TOK_FLAGS] & FLAG_AUTHENABLE)
		    flags |= CONF_FLAG_AUTHENABLE;
		if (intval[TOK_FLAGS] & FLAG_PREFER)
		    flags |= CONF_FLAG_PREFER;
		if (intval[TOK_FLAGS] & FLAG_NOSELECT)
		    flags |= CONF_FLAG_NOSELECT;
		if (intval[TOK_FLAGS] & FLAG_BURST)
		    flags |= CONF_FLAG_BURST;
		if (intval[TOK_FLAGS] & FLAG_IBURST)
		    flags |= CONF_FLAG_IBURST;
		if (intval[TOK_FLAGS] & FLAG_SKEY)
		    flags |= CONF_FLAG_SKEY;

		/*
		 * This is as good as we can check it.  Add it in.
		 */
		addentry(token[TOK_HOSTNAME], (int)intval[TOK_HMODE],
			 (int)intval[TOK_VERSION], (int)intval[TOK_MINPOLL],
			 (int)intval[TOK_MAXPOLL], flags, (int)intval[TOK_TTL],
			 intval[TOK_KEYID], token[TOK_KEYSTR]);
	}
}


/*
 * doconfigure - attempt to resolve names and configure the server
 */
static void
doconfigure(
	int dores
	)
{
	register struct conf_entry *ce;
	register struct conf_entry *ceremove;

	ce = confentries;
	while (ce != NULL) {
#ifdef DEBUG
		if (debug > 1)
			msyslog(LOG_INFO,
			    "doconfigure: <%s> has peeraddr %#x",
			    ce->ce_name, ce->ce_peeraddr);
#endif
		if (dores && ce->ce_peeraddr == 0) {
			if (!findhostaddr(ce)) {
				msyslog(LOG_ERR,
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
#ifdef DEBUG
			if (debug > 1) {
				msyslog(LOG_INFO,
				    "doconfigure: request() FAILED, maybe next time.");
			}
#endif
		}
		ce = ce->ce_next;
	}
}
