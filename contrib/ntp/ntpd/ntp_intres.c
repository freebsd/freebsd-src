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

#include "ntp_machine.h"
#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_request.h"
#include "ntp_stdlib.h"
#include "ntp_syslog.h"

#include <stdio.h>
#include <ctype.h>
#include <signal.h>

/**/
#include <netinet/in.h>
#include <arpa/inet.h>
/**/
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>		/* MAXHOSTNAMELEN (often) */
#endif

#include <isc/net.h>
#include <isc/result.h>

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

/*
 * Each item we are to resolve and configure gets one of these
 * structures defined for it.
 */
struct conf_entry {
	struct conf_entry *ce_next;
	char *ce_name;			/* name we are trying to resolve */
	struct conf_peer ce_config;	/* configuration info for peer */
	struct sockaddr_storage peer_store; /* address info for both fams */
};
#define	ce_peeraddr	ce_config.peeraddr
#define	ce_peeraddr6	ce_config.peeraddr6
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
static	SOCKET sockfd = INVALID_SOCKET;	/* NT uses SOCKET */

/* stuff to be filled in by caller */

keyid_t req_keyid;	/* request keyid */
char *req_file;		/* name of the file with configuration info */

/* end stuff to be filled in */


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


static void	resolver_exit P((int));

/*
 * Call here instead of just exiting
 */

static void resolver_exit (int code)
{
#ifdef SYS_WINNT
	CloseHandle(ResolverEventHandle);
	ResolverEventHandle = NULL;
	ExitThread(code);	/* Just to kill the thread not the process */
#else
	exit(code);		/* kill the forked process */
#endif
}

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
	struct timeval tv;
	fd_set fdset;
#ifdef SYS_WINNT
	DWORD rc;
#else
	int rc;
#endif

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
			resolver_exit(1);
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
		resolver_exit(1);
	}
	readconf(in, req_file);
	(void) fclose(in);

#ifdef DEBUG
	if (!debug )
#endif
		(void) unlink(req_file);

	/*
	 * Set up the timers to do first shot immediately.
	 */
	resolve_timer = 0;
	resolve_value = MINRESOLVE;
	config_timer = CONFIG_TIME;

	for (;;) {
		checkparent();

		if (resolve_timer == 0) {
			/*
			 * Sleep a little to make sure the network is completely up
			 */
			sleep(SLEEPTIME);
			doconfigure(1);

			/* prepare retry, in case there's more work to do */
			resolve_timer = resolve_value;
#ifdef DEBUG
			if (debug > 2)
				msyslog(LOG_INFO, "resolve_timer: 0->%d", resolve_timer);
#endif
			if (resolve_value < MAXRESOLVE)
				resolve_value <<= 1;

			config_timer = CONFIG_TIME;
		} else if (config_timer == 0) {  /* MB: in which case would this be required ? */
			doconfigure(0);
			/* MB: should we check now if we could exit, similar to the code above? */
			config_timer = CONFIG_TIME;
#ifdef DEBUG
			if (debug > 2)
				msyslog(LOG_INFO, "config_timer: 0->%d", config_timer);
#endif
		}

		if (confentries == NULL)
			resolver_exit(0);   /* done */

#ifdef SYS_WINNT
		rc = WaitForSingleObject(ResolverEventHandle, 1000 * ALARM_TIME);  /* in milliseconds */

		if ( rc == WAIT_OBJECT_0 ) { /* signaled by the main thread */
			resolve_timer = 0;         /* retry resolving immediately */
			continue;
		}

		if ( rc != WAIT_TIMEOUT ) /* not timeout: error */
			resolver_exit(1);

#else  /* not SYS_WINNT */
		tv.tv_sec = ALARM_TIME;
		tv.tv_usec = 0;
		FD_ZERO(&fdset);
		FD_SET(resolver_pipe_fd[0], &fdset);
		rc = select(resolver_pipe_fd[0] + 1, &fdset, (fd_set *)0, (fd_set *)0, &tv);

		if (rc > 0) {  /* parent process has written to the pipe */
			read(resolver_pipe_fd[0], (char *)&rc, sizeof(rc));  /* make pipe empty */
			resolve_timer = 0;   /* retry resolving immediately */
			continue;
		}

		if ( rc < 0 )  /* select() returned error */
			resolver_exit(1);
#endif

		/* normal timeout, keep on waiting */
		if (config_timer > 0)
			config_timer--;
		if (resolve_timer > 0)
			resolve_timer--;
	}
}



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
		resolver_exit(0);
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
#ifdef ISC_PLATFORM_HAVEIPV6
	ce->ce_peeraddr6 = in6addr_any;
#endif
	ANYSOCK(&ce->peer_store);
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
	static int eai_again_seen = 0;
	struct addrinfo *addr;
	struct addrinfo hints;
	int again;
	int error;

	checkparent();		/* make sure our guy is still running */

	if (entry->ce_name != NULL && !SOCKNUL(&entry->peer_store)) {
		/* HMS: Squawk? */
		msyslog(LOG_ERR, "findhostaddr: both ce_name and ce_peeraddr are defined...");
		return 1;
	}

	if (entry->ce_name == NULL && SOCKNUL(&entry->peer_store)) {
		msyslog(LOG_ERR, "findhostaddr: both ce_name and ce_peeraddr are undefined!");
		return 0;
	}

	if (entry->ce_name) {
		DPRINTF(2, ("findhostaddr: Resolving <%s>\n",
			entry->ce_name));

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		/*
		 * If the IPv6 stack is not available look only for IPv4 addresses
		 */
		if (isc_net_probeipv6() != ISC_R_SUCCESS)
			hints.ai_family = AF_INET;

		error = getaddrinfo(entry->ce_name, NULL, &hints, &addr);
		if (error == 0) {
			entry->peer_store = *((struct sockaddr_storage*)(addr->ai_addr));
			if (entry->peer_store.ss_family == AF_INET) {
				entry->ce_peeraddr =
				    GET_INADDR(entry->peer_store);
				entry->ce_config.v6_flag = 0;
			} else {
				entry->ce_peeraddr6 =
				    GET_INADDR6(entry->peer_store);
				entry->ce_config.v6_flag = 1;
			}
		}
	} else {
		DPRINTF(2, ("findhostaddr: Resolving <%s>\n",
			stoa(&entry->peer_store)));

		entry->ce_name = emalloc(MAXHOSTNAMELEN);
		error = getnameinfo((const struct sockaddr *)&entry->peer_store,
				   SOCKLEN(&entry->peer_store),
				   (char *)&entry->ce_name, MAXHOSTNAMELEN,
				   NULL, 0, 0);
	}

	if (0 == error) {

		/* again is our return value, for success it is 1 */
		again = 1;

		DPRINTF(2, ("findhostaddr: %s resolved.\n", 
			(entry->ce_name) ? "name" : "address"));
	} else {
		/*
		 * If the resolver failed, see if the failure is
		 * temporary. If so, return success.
		 */
		again = 0;

		switch (error) {

		case EAI_FAIL:
			again = 1;
			break;

		case EAI_AGAIN:
			again = 1;
			eai_again_seen = 1;
			break;

		case EAI_NONAME:
#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
		case EAI_NODATA:
#endif
			msyslog(LOG_ERR, "host name not found%s%s: %s",
				(EAI_NONAME == error) ? "" : " EAI_NODATA",
				(eai_again_seen) ? " (permanent)" : "",
				entry->ce_name);
			again = !eai_again_seen;
			break;

#ifdef EAI_SYSTEM
		case EAI_SYSTEM:
			/* 
			 * EAI_SYSTEM means the real error is in errno.  We should be more
			 * discriminating about which errno values require retrying, but
			 * this matches existing behavior.
			 */
			again = 1;
			DPRINTF(1, ("intres: EAI_SYSTEM errno %d (%s) means try again, right?\n",
				errno, strerror(errno)));
			break;
#endif
		}

		/* do this here to avoid perturbing errno earlier */
		DPRINTF(2, ("intres: got error status of: %d\n", error));
	}

	return again;
}


/*
 * openntp - open a socket to the ntp server
 */
static void
openntp(void)
{
	const char	*localhost = "127.0.0.1";	/* Use IPv4 loopback */
	struct addrinfo	hints;
	struct addrinfo	*addr;
	u_long		on;
	int		err;

	if (sockfd != INVALID_SOCKET)
		return;

	memset(&hints, 0, sizeof(hints));

	/*
	 * For now only bother with IPv4
	 */
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	err = getaddrinfo(localhost, "ntp", &hints, &addr);

	if (err) {
#ifdef EAI_SYSTEM
		if (EAI_SYSTEM == err)
			msyslog(LOG_ERR, "getaddrinfo(%s) failed: %m",
				localhost);
		else
#endif
			msyslog(LOG_ERR, "getaddrinfo(%s) failed: %s",
				localhost, gai_strerror(err));
		resolver_exit(1);
	}

	sockfd = socket(addr->ai_family, addr->ai_socktype, 0);

	if (INVALID_SOCKET == sockfd) {
		msyslog(LOG_ERR, "socket() failed: %m");
		resolver_exit(1);
	}

#ifndef SYS_WINNT
	/*
	 * On Windows only the count of sockets must be less than
	 * FD_SETSIZE. On Unix each descriptor's value must be less
	 * than FD_SETSIZE, as fd_set is a bit array.
	 */
	if (sockfd >= FD_SETSIZE) {
		msyslog(LOG_ERR, "socket fd %d too large, FD_SETSIZE %d",
			(int)sockfd, FD_SETSIZE);
		resolver_exit(1);
	}

	/*
	 * Make the socket non-blocking.  We'll wait with select()
	 * Unix: fcntl(O_NONBLOCK) or fcntl(FNDELAY)
	 */
# ifdef O_NONBLOCK
	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
		msyslog(LOG_ERR, "fcntl(O_NONBLOCK) failed: %m");
		resolver_exit(1);
	}
# else
#  ifdef FNDELAY
	if (fcntl(sockfd, F_SETFL, FNDELAY) == -1) {
		msyslog(LOG_ERR, "fcntl(FNDELAY) failed: %m");
		resolver_exit(1);
	}
#  else
#   include "Bletch: NEED NON BLOCKING IO"
#  endif	/* FNDDELAY */
# endif	/* O_NONBLOCK */
	(void)on;	/* quiet unused warning */
#else	/* !SYS_WINNT above */
	/*
	 * Make the socket non-blocking.  We'll wait with select()
	 * Windows: ioctlsocket(FIONBIO)
	 */
	on = 1;
	err = ioctlsocket(sockfd, FIONBIO, &on);
	if (SOCKET_ERROR == err) {
		msyslog(LOG_ERR, "ioctlsocket(FIONBIO) fails: %m");
		resolver_exit(1);
	}
#endif /* SYS_WINNT */

	err = connect(sockfd, addr->ai_addr, addr->ai_addrlen);
	if (SOCKET_ERROR == err) {
		msyslog(LOG_ERR, "openntp: connect() failed: %m");
		resolver_exit(1);
	}

	freeaddrinfo(addr);
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

	if (sockfd == INVALID_SOCKET)
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
		resolver_exit(1);
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
			NULL, (LPOVERLAPPED)&overlap);
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
	if (!GetOverlappedResult((HANDLE)sockfd, (LPOVERLAPPED)&overlap,
				(LPDWORD)&NumberOfBytesWritten, FALSE)) {
		msyslog(LOG_ERR, "GetOverlappedResult for WriteFile fails: %m");
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
			if (errno != EINTR)
			    msyslog(LOG_ERR, "select() fails: %m");
			return 0;
		}
		else if (n == 0)
		{
#ifdef DEBUG
			if (debug)
			    msyslog(LOG_INFO, "select() returned 0.");
#endif
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
			       NULL, (LPOVERLAPPED)&overlap);
		if ((ret == FALSE) && (GetLastError() != ERROR_IO_PENDING)) {
			msyslog(LOG_ERR, "ReadFile() fails: %m");
			return 0;
		}
		dwWait = WaitForSingleObject(hReadWriteEvent, (DWORD) TIMEOUT_SEC * 1000);
		if ((dwWait == WAIT_FAILED) || (dwWait == WAIT_TIMEOUT)) {
			if (dwWait == WAIT_FAILED) {
				msyslog(LOG_ERR, "WaitForSingleObject for ReadFile fails: %m");
				return 0;
			}
			continue;
		}
		if (!GetOverlappedResult((HANDLE)sockfd, (LPOVERLAPPED)&overlap,
					(LPDWORD)&NumberOfBytesRead, FALSE)) {
			msyslog(LOG_ERR, "GetOverlappedResult fails: %m");
			return 0;
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
				"ntpd reports implementation mismatch!");
			return 0;
		
		    case INFO_ERR_REQ:
			msyslog(LOG_ERR,
				"ntpd says configuration request is unknown!");
			return 0;
		
		    case INFO_ERR_FMT:
			msyslog(LOG_ERR,
				"ntpd indicates a format error occurred!");
			return 0;

		    case INFO_ERR_NODATA:
			msyslog(LOG_ERR,
				"ntpd indicates no data available!");
			return 0;
		
		    case INFO_ERR_AUTH:
			msyslog(LOG_ERR,
				"ntpd returns a permission denied error!");
			return 0;

		    default:
			msyslog(LOG_ERR,
				"ntpd returns unknown error code %d!", n);
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
				resolver_exit(1);
			}
		}

		for (i = 1; i < NUMTOK - 1; i++) {
			if (!atouint(token[i], &intval[i])) {
				msyslog(LOG_ERR,
					"format error for integer token `%s', file `%s', quitting",
					token[i], name);
				resolver_exit(1);
			}
		}

		if (intval[TOK_HMODE] != MODE_ACTIVE &&
		    intval[TOK_HMODE] != MODE_CLIENT &&
		    intval[TOK_HMODE] != MODE_BROADCAST) {
			msyslog(LOG_ERR, "invalid mode (%ld) in file %s",
				intval[TOK_HMODE], name);
			resolver_exit(1);
		}

		if (intval[TOK_VERSION] > NTP_VERSION ||
		    intval[TOK_VERSION] < NTP_OLDVERSION) {
			msyslog(LOG_ERR, "invalid version (%ld) in file %s",
				intval[TOK_VERSION], name);
			resolver_exit(1);
		}
		if (intval[TOK_MINPOLL] < NTP_MINPOLL ||
		    intval[TOK_MINPOLL] > NTP_MAXPOLL) {
			msyslog(LOG_ERR, "invalid MINPOLL value (%ld) in file %s",
				intval[TOK_MINPOLL], name);
			resolver_exit(1);
		}

		if (intval[TOK_MAXPOLL] < NTP_MINPOLL ||
		    intval[TOK_MAXPOLL] > NTP_MAXPOLL) {
			msyslog(LOG_ERR, "invalid MAXPOLL value (%ld) in file %s",
				intval[TOK_MAXPOLL], name);
			resolver_exit(1);
		}

		if ((intval[TOK_FLAGS] & ~(FLAG_AUTHENABLE | FLAG_PREFER |
		    FLAG_NOSELECT | FLAG_BURST | FLAG_IBURST | FLAG_SKEY))
		    != 0) {
			msyslog(LOG_ERR, "invalid flags (%ld) in file %s",
				intval[TOK_FLAGS], name);
			resolver_exit(1);
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

#ifdef DEBUG
		if (debug > 1)
			msyslog(LOG_INFO, "Running doconfigure %s DNS",
			    dores ? "with" : "without" );
#endif

	ce = confentries;
	while (ce != NULL) {
#ifdef DEBUG
		if (debug > 1)
			msyslog(LOG_INFO,
			    "doconfigure: <%s> has peeraddr %s",
			    ce->ce_name, stoa(&ce->peer_store));
#endif
		if (dores && SOCKNUL(&(ce->peer_store))) {
			if (!findhostaddr(ce)) {
#ifndef IGNORE_DNS_ERRORS
				msyslog(LOG_ERR,
					"couldn't resolve `%s', giving up on it",
					ce->ce_name);
				ceremove = ce;
				ce = ceremove->ce_next;
				removeentry(ceremove);
				continue;
#endif
			}
		}

		if (!SOCKNUL(&ce->peer_store)) {
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
