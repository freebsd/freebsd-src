/*
 * ntpdc - control and monitor your ntpd daemon
 */

#include <stdio.h>

#include "ntpdc.h"
#include "ntp_select.h"
#include "ntp_io.h"
#include "ntp_stdlib.h"
/* Don't include ISC's version of IPv6 variables and structures */
#define ISC_IPV6_H 1
#include "isc/net.h"
#include "isc/result.h"

#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <netdb.h>

#ifdef SYS_WINNT
# include <io.h>
#else
# define closesocket close
#endif /* SYS_WINNT */

#if defined(HAVE_LIBREADLINE) || defined (HAVE_LIBEDIT)
# include <readline/readline.h>
# include <readline/history.h>
#endif /* HAVE_LIBREADLINE || HAVE_LIBEDIT */

#ifdef SYS_VXWORKS
/* vxWorks needs mode flag -casey*/
#define open(name, flags)   open(name, flags, 0777)
#define SERVER_PORT_NUM     123
#endif

/*
 * Because we now potentially understand a lot of commands (and
 * it requires a lot of commands to talk to ntpd) we will run
 * interactive if connected to a terminal.
 */
static	int	interactive = 0;	/* set to 1 when we should prompt */
static	const char *	prompt = "ntpdc> ";	/* prompt to ask him about */

/*
 * Keyid used for authenticated requests.  Obtained on the fly.
 */
static	u_long	info_auth_keyid;

/*
 * Type of key md5
 */
#define	KEY_TYPE_MD5	4

static	int info_auth_keytype = KEY_TYPE_MD5;	/* MD5 */
u_long	current_time;		/* needed by authkeys; not used */

int		ntpdcmain	P((int,	char **));
/*
 * Built in command handler declarations
 */
static	int	openhost	P((const char *));
static	int	sendpkt		P((char *, int));
static	void	growpktdata	P((void));
static	int	getresponse	P((int, int, int *, int *, char **, int));
static	int	sendrequest	P((int, int, int, int, int, char *));
static	void	getcmds		P((void));
static	RETSIGTYPE abortcmd	P((int));
static	void	docmd		P((const char *));
static	void	tokenize	P((const char *, char **, int *));
static	int	findcmd		P((char *, struct xcmd *, struct xcmd *, struct xcmd **));
static	int	getarg		P((char *, int, arg_v *));
static	int	getnetnum	P((const char *, struct sockaddr_storage *, char *, int));
static	void	help		P((struct parse *, FILE *));
#ifdef QSORT_USES_VOID_P
static	int	helpsort	P((const void *, const void *));
#else
static	int	helpsort	P((char **, char **));
#endif
static	void	printusage	P((struct xcmd *, FILE *));
static	void	timeout		P((struct parse *, FILE *));
static	void	my_delay	P((struct parse *, FILE *));
static	void	host		P((struct parse *, FILE *));
static	void	keyid		P((struct parse *, FILE *));
static	void	keytype		P((struct parse *, FILE *));
static	void	passwd		P((struct parse *, FILE *));
static	void	hostnames	P((struct parse *, FILE *));
static	void	setdebug	P((struct parse *, FILE *));
static	void	quit		P((struct parse *, FILE *));
static	void	version		P((struct parse *, FILE *));
static	void	warning		P((const char *, const char *, const char *));
static	void	error		P((const char *, const char *, const char *));
static	u_long	getkeyid	P((const char *));



/*
 * Built-in commands we understand
 */
static	struct xcmd builtins[] = {
	{ "?",		help,		{  OPT|NTP_STR, NO, NO, NO },
	  { "command", "", "", "" },
	  "tell the use and syntax of commands" },
	{ "help",	help,		{  OPT|NTP_STR, NO, NO, NO },
	  { "command", "", "", "" },
	  "tell the use and syntax of commands" },
	{ "timeout",	timeout,	{ OPT|UINT, NO, NO, NO },
	  { "msec", "", "", "" },
	  "set the primary receive time out" },
	{ "delay",	my_delay,	{ OPT|INT, NO, NO, NO },
	  { "msec", "", "", "" },
	  "set the delay added to encryption time stamps" },
	{ "host",	host,		{ OPT|NTP_STR, OPT|NTP_STR, NO, NO },
	  { "-4|-6", "hostname", "", "" },
	  "specify the host whose NTP server we talk to" },
	{ "passwd",	passwd,		{ OPT|NTP_STR, NO, NO, NO },
	  { "", "", "", "" },
	  "specify a password to use for authenticated requests"},
	{ "hostnames",	hostnames,	{ OPT|NTP_STR, NO, NO, NO },
	  { "yes|no", "", "", "" },
	  "specify whether hostnames or net numbers are printed"},
	{ "debug",	setdebug,	{ OPT|NTP_STR, NO, NO, NO },
	  { "no|more|less", "", "", "" },
	  "set/change debugging level" },
	{ "quit",	quit,		{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "exit ntpdc" },
	{ "exit",	quit,		{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "exit ntpdc" },
	{ "keyid",	keyid,		{ OPT|UINT, NO, NO, NO },
	  { "key#", "", "", "" },
	  "set/show keyid to use for authenticated requests" },
	{ "keytype",	keytype,	{ OPT|NTP_STR, NO, NO, NO },
	  { "(md5|des)", "", "", "" },
	  "set/show key authentication type for authenticated requests (des|md5)" },
	{ "version",	version,	{ NO, NO, NO, NO },
	  { "", "", "", "" },
	  "print version number" },
	{ 0,		0,		{ NO, NO, NO, NO },
	  { "", "", "", "" }, "" }
};


/*
 * Default values we use.
 */
#define	DEFTIMEOUT	(5)		/* 5 second time out */
#define	DEFSTIMEOUT	(2)		/* 2 second time out after first */
#define	DEFDELAY	0x51EB852	/* 20 milliseconds, l_fp fraction */
#define	DEFHOST		"localhost"	/* default host name */
#define	LENHOSTNAME	256		/* host name is 256 characters long */
#define	MAXCMDS		100		/* maximum commands on cmd line */
#define	MAXHOSTS	200		/* maximum hosts on cmd line */
#define	MAXLINE		512		/* maximum line length */
#define	MAXTOKENS	(1+1+MAXARGS+2)	/* maximum number of usable tokens */
					/* command + -4|-6 + MAXARGS + */
					/* redirection */

/*
 * Some variables used and manipulated locally
 */
static	struct timeval tvout = { DEFTIMEOUT, 0 };	/* time out for reads */
static	struct timeval tvsout = { DEFSTIMEOUT, 0 };	/* secondary time out */
static	l_fp delay_time;				/* delay time */
static	char currenthost[LENHOSTNAME];			/* current host name */
int showhostnames = 1;					/* show host names by default */

static	int ai_fam_templ;				/* address family */
static	int ai_fam_default;				/* default address family */
static	SOCKET sockfd;					/* fd socket is opened on */
static	int havehost = 0;				/* set to 1 when host open */
int s_port = 0;

#if defined (SYS_WINNT) || defined (SYS_VXWORKS)
char password[9];
#endif /* SYS_WINNT || SYS_VXWORKS */

#ifdef SYS_WINNT
DWORD NumberOfBytesWritten;

HANDLE	TimerThreadHandle = NULL;	/* 1998/06/03 - Used in ntplib/machines.c */
void timer(void)	{  ; };	/* 1998/06/03 - Used in ntplib/machines.c */

#endif /* SYS_WINNT */

/*
 * Holds data returned from queries.  We allocate INITDATASIZE
 * octets to begin with, increasing this as we need to.
 */
#define	INITDATASIZE	(sizeof(struct resp_pkt) * 16)
#define	INCDATASIZE	(sizeof(struct resp_pkt) * 8)

static	char *pktdata;
static	int pktdatasize;

/*
 * These are used to help the magic with old and new versions of ntpd.
 */
int impl_ver = IMPL_XNTPD;
static int req_pkt_size = REQ_LEN_NOMAC;

/*
 * For commands typed on the command line (with the -c option)
 */
static	int numcmds = 0;
static	const char *ccmds[MAXCMDS];
#define	ADDCMD(cp)	if (numcmds < MAXCMDS) ccmds[numcmds++] = (cp)

/*
 * When multiple hosts are specified.
 */
static	int numhosts = 0;
static	const char *chosts[MAXHOSTS];
#define	ADDHOST(cp)	if (numhosts < MAXHOSTS) chosts[numhosts++] = (cp)

/*
 * Error codes for internal use
 */
#define	ERR_INCOMPLETE		16
#define	ERR_TIMEOUT		17

/*
 * Macro definitions we use
 */
#define	ISSPACE(c)	((c) == ' ' || (c) == '\t')
#define	ISEOL(c)	((c) == '\n' || (c) == '\r' || (c) == '\0')
#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

/*
 * For converting time stamps to dates
 */
#define	JAN_1970	2208988800	/* 1970 - 1900 in seconds */

/*
 * Jump buffer for longjumping back to the command level
 */
static	jmp_buf interrupt_buf;
static  volatile int jump = 0;

/*
 * Pointer to current output unit
 */
static	FILE *current_output;

/*
 * Command table imported from ntpdc_ops.c
 */
extern struct xcmd opcmds[];

char *progname;
volatile int debug;

#ifdef NO_MAIN_ALLOWED
CALL(ntpdc,"ntpdc",ntpdcmain);
#else
int
main(
	int argc,
	char *argv[]
	)
{
	return ntpdcmain(argc, argv);
}
#endif

#ifdef SYS_VXWORKS
void clear_globals(void)
{
    extern int ntp_optind;
    extern char *ntp_optarg;
    showhostnames = 0;              /* show host names by default */
    ntp_optind = 0;
    ntp_optarg = 0;
    havehost = 0;                   /* set to 1 when host open */
    numcmds = 0;
    numhosts = 0;
}
#endif

/*
 * main - parse arguments and handle options
 */
int
ntpdcmain(
	int argc,
	char *argv[]
	)
{
	int c;
	int errflg = 0;
	extern int ntp_optind;
	extern char *ntp_optarg;

	delay_time.l_ui = 0;
	delay_time.l_uf = DEFDELAY;

#ifdef SYS_VXWORKS
	clear_globals();
	taskPrioritySet(taskIdSelf(), 100 );
#endif

#ifdef SYS_WINNT
	if (!Win32InitSockets())
	{
		fprintf(stderr, "No useable winsock.dll:");
		exit(1);
	}
#endif /* SYS_WINNT */

	/* Check to see if we have IPv6. Otherwise force the -4 flag */
	if (isc_net_probeipv6() != ISC_R_SUCCESS) {
		ai_fam_default = AF_INET;
	}

	progname = argv[0];
	ai_fam_templ = ai_fam_default;
	while ((c = ntp_getopt(argc, argv, "46c:dilnps")) != EOF)
	    switch (c) {
		case '4':
		    ai_fam_templ = AF_INET;
		    break;
		case '6':
		    ai_fam_templ = AF_INET6;
		    break;
		case 'c':
		    ADDCMD(ntp_optarg);
		    break;
		case 'd':
		    ++debug;
		    break;
		case 'i':
		    interactive = 1;
		    break;
		case 'l':
		    ADDCMD("listpeers");
		    break;
		case 'n':
		    showhostnames = 0;
		    break;
		case 'p':
		    ADDCMD("peers");
		    break;
		case 's':
		    ADDCMD("dmpeers");
		    break;
		default:
		    errflg++;
		    break;
	    }
	if (errflg) {
		(void) fprintf(stderr,
			       "usage: %s [-46dilnps] [-c cmd] host ...\n",
			       progname);
		exit(2);
	}
	if (ntp_optind == argc) {
		ADDHOST(DEFHOST);
	} else {
		for (; ntp_optind < argc; ntp_optind++)
		    ADDHOST(argv[ntp_optind]);
	}

	if (numcmds == 0 && interactive == 0
	    && isatty(fileno(stdin)) && isatty(fileno(stderr))) {
		interactive = 1;
	}

#ifndef SYS_WINNT /* Under NT cannot handle SIGINT, WIN32 spawns a handler */
	if (interactive)
	    (void) signal_no_reset(SIGINT, abortcmd);
#endif /* SYS_WINNT */

	/*
	 * Initialize the packet data buffer
	 */
	pktdata = (char *)malloc(INITDATASIZE);
	if (pktdata == NULL) {
		(void) fprintf(stderr, "%s: malloc() failed!\n", progname);
		exit(1);
	}
	pktdatasize = INITDATASIZE;

	if (numcmds == 0) {
		(void) openhost(chosts[0]);
		getcmds();
	} else {
		int ihost;
		int icmd;

		for (ihost = 0; ihost < numhosts; ihost++) {
			if (openhost(chosts[ihost]))
			    for (icmd = 0; icmd < numcmds; icmd++) {
				    if (numhosts > 1) 
					printf ("--- %s ---\n",chosts[ihost]);
				    docmd(ccmds[icmd]);
			    }
		}
	}
#ifdef SYS_WINNT
	WSACleanup();
#endif
	return(0);
} /* main end */


/*
 * openhost - open a socket to a host
 */
static int
openhost(
	const char *hname
	)
{
	char temphost[LENHOSTNAME];
	int a_info, i;
	struct addrinfo hints, *ai = NULL;
	register const char *cp;
	char name[LENHOSTNAME];
	char service[5];

	/*
	 * We need to get by the [] if they were entered 
	 */
	
	cp = hname;
	
	if (*cp == '[') {
		cp++;	
		for(i = 0; *cp != ']'; cp++, i++)
			name[i] = *cp;	
		name[i] = '\0';
		hname = name;
	}	

	/*
	 * First try to resolve it as an ip address and if that fails,
	 * do a fullblown (dns) lookup. That way we only use the dns
	 * when it is needed and work around some implementations that
	 * will return an "IPv4-mapped IPv6 address" address if you
	 * give it an IPv4 address to lookup.
	 */
	strcpy(service, "ntp");
	memset((char *)&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = ai_fam_templ;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICHOST;

	a_info = getaddrinfo(hname, service, &hints, &ai);
	if (a_info == EAI_NONAME || a_info == EAI_NODATA) {
		hints.ai_flags = AI_CANONNAME;
#ifdef AI_ADDRCONFIG
		hints.ai_flags |= AI_ADDRCONFIG;
#endif
		a_info = getaddrinfo(hname, service, &hints, &ai);	
	}
	/* Some older implementations don't like AI_ADDRCONFIG. */
	if (a_info == EAI_BADFLAGS) {
		hints.ai_flags = AI_CANONNAME;
		a_info = getaddrinfo(hname, service, &hints, &ai);	
	}
	if (a_info != 0) {
		(void) fprintf(stderr, "%s\n", gai_strerror(a_info));
		return 0;
	}

	if (ai->ai_canonname == NULL) {
		strncpy(temphost, stoa((struct sockaddr_storage *)ai->ai_addr),
		    LENHOSTNAME);
		temphost[LENHOSTNAME-1] = '\0';
	} else {
		strncpy(temphost, ai->ai_canonname, LENHOSTNAME);
		temphost[LENHOSTNAME-1] = '\0';
	}

	if (debug > 2)
	    printf("Opening host %s\n", temphost);

	if (havehost == 1) {
		if (debug > 2)
		    printf("Closing old host %s\n", currenthost);
		(void) closesocket(sockfd);
		havehost = 0;
	}
	(void) strcpy(currenthost, temphost);
	
	/* port maps to the same in both families */
	s_port = ((struct sockaddr_in6 *)ai->ai_addr)->sin6_port; 
#ifdef SYS_VXWORKS
	((struct sockaddr_in6 *)&hostaddr)->sin6_port = htons(SERVER_PORT_NUM);
	if (ai->ai_family == AF_INET)
		*(struct sockaddr_in *)&hostaddr= 
			*((struct sockaddr_in *)ai->ai_addr);
	else 
		*(struct sockaddr_in6 *)&hostaddr= 
			*((struct sockaddr_in6 *)ai->ai_addr);
#endif /* SYS_VXWORKS */

#ifdef SYS_WINNT
	{
		int optionValue = SO_SYNCHRONOUS_NONALERT;
		int err;
		err = setsockopt(INVALID_SOCKET, SOL_SOCKET, SO_OPENTYPE, (char *)&optionValue, sizeof(optionValue));
		if (err != NO_ERROR) {
			(void) fprintf(stderr, "cannot open nonoverlapped sockets\n");
			exit(1);
		}
	}

	sockfd = socket(ai->ai_family, SOCK_DGRAM, 0);
	if (sockfd == INVALID_SOCKET) {
		error("socket", "", "");
		exit(-1);
	}
#else
	sockfd = socket(ai->ai_family, SOCK_DGRAM, 0);
	if (sockfd == -1)
	    error("socket", "", "");
#endif /* SYS_WINNT */

	
#ifdef NEED_RCVBUF_SLOP
# ifdef SO_RCVBUF
	{
		int rbufsize = INITDATASIZE + 2048; /* 2K for slop */

		if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
			       &rbufsize, sizeof(int)) == -1)
		    error("setsockopt", "", "");
	}
# endif
#endif

#ifdef SYS_VXWORKS
	if (connect(sockfd, (struct sockaddr *)&hostaddr, 
		    sizeof(hostaddr)) == -1)
#else
	if (connect(sockfd, (struct sockaddr *)ai->ai_addr,
		    ai->ai_addrlen) == -1)
#endif /* SYS_VXWORKS */
	    error("connect", "", "");
	if (a_info)
		freeaddrinfo(ai);
	havehost = 1;
	req_pkt_size = REQ_LEN_NOMAC;
	impl_ver = IMPL_XNTPD;
	return 1;
}


/* XXX ELIMINATE sendpkt similar in ntpq.c, ntpdc.c, ntp_io.c, ntptrace.c */
/*
 * sendpkt - send a packet to the remote host
 */
static int
sendpkt(
	char *xdata,
	int xdatalen
	)
{
	if (send(sockfd, xdata, (size_t)xdatalen, 0) == -1) {
		warning("write to %s failed", currenthost, "");
		return -1;
	}

	return 0;
}


/*
 * growpktdata - grow the packet data area
 */
static void
growpktdata(void)
{
	pktdatasize += INCDATASIZE;
	pktdata = (char *)realloc(pktdata, (unsigned)pktdatasize);
	if (pktdata == 0) {
		(void) fprintf(stderr, "%s: realloc() failed!\n", progname);
		exit(1);
	}
}


/*
 * getresponse - get a (series of) response packet(s) and return the data
 */
static int
getresponse(
	int implcode,
	int reqcode,
	int *ritems,
	int *rsize,
	char **rdata,
	int esize
	)
{
	struct resp_pkt rpkt;
	struct timeval tvo;
	int items;
	int i;
	int size;
	int datasize;
	char *datap;
	char *tmp_data;
	char haveseq[MAXSEQ+1];
	int firstpkt;
	int lastseq;
	int numrecv;
	int seq;
	fd_set fds;
	int n;
	int pad;

	/*
	 * This is pretty tricky.  We may get between 1 and many packets
	 * back in response to the request.  We peel the data out of
	 * each packet and collect it in one long block.  When the last
	 * packet in the sequence is received we'll know how many we
	 * should have had.  Note we use one long time out, should reconsider.
	 */
	*ritems = 0;
	*rsize = 0;
	firstpkt = 1;
	numrecv = 0;
	*rdata = datap = pktdata;
	lastseq = 999;	/* too big to be a sequence number */
	memset(haveseq, 0, sizeof(haveseq));
	FD_ZERO(&fds);

    again:
	if (firstpkt)
	    tvo = tvout;
	else
	    tvo = tvsout;
	
	FD_SET(sockfd, &fds);
	n = select(sockfd+1, &fds, (fd_set *)0, (fd_set *)0, &tvo);

	if (n == -1) {
		warning("select fails", "", "");
		return -1;
	}
	if (n == 0) {
		/*
		 * Timed out.  Return what we have
		 */
		if (firstpkt) {
			(void) fprintf(stderr,
				       "%s: timed out, nothing received\n", currenthost);
			return ERR_TIMEOUT;
		} else {
			(void) fprintf(stderr,
				       "%s: timed out with incomplete data\n",
				       currenthost);
			if (debug) {
				printf("Received sequence numbers");
				for (n = 0; n <= MAXSEQ; n++)
				    if (haveseq[n])
					printf(" %d,", n);
				if (lastseq != 999)
				    printf(" last frame received\n");
				else
				    printf(" last frame not received\n");
			}
			return ERR_INCOMPLETE;
		}
	}

	n = recv(sockfd, (char *)&rpkt, sizeof(rpkt), 0);
	if (n == -1) {
		warning("read", "", "");
		return -1;
	}


	/*
	 * Check for format errors.  Bug proofing.
	 */
	if (n < RESP_HEADER_SIZE) {
		if (debug)
		    printf("Short (%d byte) packet received\n", n);
		goto again;
	}
	if (INFO_VERSION(rpkt.rm_vn_mode) > NTP_VERSION ||
	    INFO_VERSION(rpkt.rm_vn_mode) < NTP_OLDVERSION) {
		if (debug)
		    printf("Packet received with version %d\n",
			   INFO_VERSION(rpkt.rm_vn_mode));
		goto again;
	}
	if (INFO_MODE(rpkt.rm_vn_mode) != MODE_PRIVATE) {
		if (debug)
		    printf("Packet received with mode %d\n",
			   INFO_MODE(rpkt.rm_vn_mode));
		goto again;
	}
	if (INFO_IS_AUTH(rpkt.auth_seq)) {
		if (debug)
		    printf("Encrypted packet received\n");
		goto again;
	}
	if (!ISRESPONSE(rpkt.rm_vn_mode)) {
		if (debug)
		    printf("Received request packet, wanted response\n");
		goto again;
	}
	if (INFO_MBZ(rpkt.mbz_itemsize) != 0) {
		if (debug)
		    printf("Received packet with nonzero MBZ field!\n");
		goto again;
	}

	/*
	 * Check implementation/request.  Could be old data getting to us.
	 */
	if (rpkt.implementation != implcode || rpkt.request != reqcode) {
		if (debug)
		    printf(
			    "Received implementation/request of %d/%d, wanted %d/%d",
			    rpkt.implementation, rpkt.request,
			    implcode, reqcode);
		goto again;
	}

	/*
	 * Check the error code.  If non-zero, return it.
	 */
	if (INFO_ERR(rpkt.err_nitems) != INFO_OKAY) {
		if (debug && ISMORE(rpkt.rm_vn_mode)) {
			printf("Error code %d received on not-final packet\n",
			       INFO_ERR(rpkt.err_nitems));
		}
		return (int)INFO_ERR(rpkt.err_nitems);
	}

	/*
	 * Collect items and size.  Make sure they make sense.
	 */
	items = INFO_NITEMS(rpkt.err_nitems);
	size = INFO_ITEMSIZE(rpkt.mbz_itemsize);
	if (esize > size)
		pad = esize - size;
	else 
		pad = 0;
	if ((datasize = items*size) > (n-RESP_HEADER_SIZE)) {
		if (debug)
		    printf(
			    "Received items %d, size %d (total %d), data in packet is %d\n",
			    items, size, datasize, n-RESP_HEADER_SIZE);
		goto again;
	}

	/*
	 * If this isn't our first packet, make sure the size matches
	 * the other ones.
	 */
	if (!firstpkt && esize != *rsize) {
		if (debug)
		    printf("Received itemsize %d, previous %d\n",
			   size, *rsize);
		goto again;
	}
	/*
	 * If we've received this before, +toss it
	 */
	seq = INFO_SEQ(rpkt.auth_seq);
	if (haveseq[seq]) {
		if (debug)
		    printf("Received duplicate sequence number %d\n", seq);
		goto again;
	}
	haveseq[seq] = 1;

	/*
	 * If this is the last in the sequence, record that.
	 */
	if (!ISMORE(rpkt.rm_vn_mode)) {
		if (lastseq != 999) {
			printf("Received second end sequence packet\n");
			goto again;
		}
		lastseq = seq;
	}

	/*
	 * So far, so good.  Copy this data into the output array.
	 */
	if ((datap + datasize + (pad * items)) > (pktdata + pktdatasize)) {
		int offset = datap - pktdata;
		growpktdata();
	        *rdata = pktdata; /* might have been realloced ! */
		datap = pktdata + offset;
	}
	/* 
	 * We now move the pointer along according to size and number of
	 * items.  This is so we can play nice with older implementations
	 */

	tmp_data = (char *)rpkt.data;
	for(i = 0; i <items; i++){
		memmove(datap, tmp_data, (unsigned)size);
		tmp_data += size;
		memset(datap + size, 0, pad);
		datap += size + pad;
	}

	if (firstpkt) {
		firstpkt = 0;
		*rsize = size + pad;
	}
	*ritems += items;

	/*
	 * Finally, check the count of received packets.  If we've got them
	 * all, return
	 */
	++numrecv;
	if (numrecv <= lastseq)
	    goto again;
	return INFO_OKAY;
}


/*
 * sendrequest - format and send a request packet
 */
static int
sendrequest(
	int implcode,
	int reqcode,
	int auth,
	int qitems,
	int qsize,
	char *qdata
	)
{
	struct req_pkt qpkt;
	int datasize;

	memset((char *)&qpkt, 0, sizeof qpkt);

	qpkt.rm_vn_mode = RM_VN_MODE(0, 0, 0);
	qpkt.implementation = (u_char)implcode;
	qpkt.request = (u_char)reqcode;

	datasize = qitems * qsize;
	if (datasize != 0 && qdata != NULL) {
		memmove((char *)qpkt.data, qdata, (unsigned)datasize);
		qpkt.err_nitems = ERR_NITEMS(0, qitems);
		qpkt.mbz_itemsize = MBZ_ITEMSIZE(qsize);
	} else {
		qpkt.err_nitems = ERR_NITEMS(0, 0);
		qpkt.mbz_itemsize = MBZ_ITEMSIZE(0);
	}

	if (!auth) {
		qpkt.auth_seq = AUTH_SEQ(0, 0);
		return sendpkt((char *)&qpkt, req_pkt_size);
	} else {
		l_fp ts;
		int maclen = 0;
		const char *pass = "\0";
		struct req_pkt_tail *qpktail;

		qpktail = (struct req_pkt_tail *)((char *)&qpkt + req_pkt_size
		    + MAX_MAC_LEN - sizeof(struct req_pkt_tail));

		if (info_auth_keyid == 0) {
			maclen = getkeyid("Keyid: ");
			if (maclen == 0) {
				(void) fprintf(stderr,
				    "Invalid key identifier\n");
				return 1;
			}
			info_auth_keyid = maclen;
		}
		if (!authistrusted(info_auth_keyid)) {
			pass = getpass("MD5 Password: ");
			if (*pass == '\0') {
				(void) fprintf(stderr,
				    "Invalid password\n");
				return (1);
			}
		}
		authusekey(info_auth_keyid, info_auth_keytype, (const u_char *)pass);
		authtrust(info_auth_keyid, 1);
		qpkt.auth_seq = AUTH_SEQ(1, 0);
		qpktail->keyid = htonl(info_auth_keyid);
		get_systime(&ts);
		L_ADD(&ts, &delay_time);
		HTONL_FP(&ts, &qpktail->tstamp);
		maclen = authencrypt(info_auth_keyid, (u_int32 *)&qpkt,
		    req_pkt_size);
		if (maclen == 0) {  
			(void) fprintf(stderr, "Key not found\n");
			return (1);
		}
		return sendpkt((char *)&qpkt, (int)(req_pkt_size + maclen));
	}
	/*NOTREACHED*/
}


/*
 * doquery - send a request and process the response
 */
int
doquery(
	int implcode,
	int reqcode,
	int auth,
	int qitems,
	int qsize,
	char *qdata,
	int *ritems,
	int *rsize,
	char **rdata,
 	int quiet_mask,
	int esize
	)
{
	int res;
	char junk[512];
	fd_set fds;
	struct timeval tvzero;

	/*
	 * Check to make sure host is open
	 */
	if (!havehost) {
		(void) fprintf(stderr, "***No host open, use `host' command\n");
		return -1;
	}

	/*
	 * Poll the socket and clear out any pending data
	 */
again:
	do {
		tvzero.tv_sec = tvzero.tv_usec = 0;
		FD_ZERO(&fds);
		FD_SET(sockfd, &fds);
		res = select(sockfd+1, &fds, (fd_set *)0, (fd_set *)0, &tvzero);

		if (res == -1) {
			warning("polling select", "", "");
			return -1;
		} else if (res > 0)

		    (void) recv(sockfd, junk, sizeof junk, 0);
	} while (res > 0);


	/*
	 * send a request
	 */
	res = sendrequest(implcode, reqcode, auth, qitems, qsize, qdata);
	if (res != 0)
	    return res;
	
	/*
	 * Get the response.  If we got a standard error, print a message
	 */
	res = getresponse(implcode, reqcode, ritems, rsize, rdata, esize);

	/*
	 * Try to be compatible with older implementations of ntpd.
	 */
	if (res == INFO_ERR_FMT && req_pkt_size != 48) {
		int oldsize;

		oldsize = req_pkt_size;

		switch(req_pkt_size) {
		case REQ_LEN_NOMAC:
			req_pkt_size = 160;
			break;
		case 160:
			req_pkt_size = 48;
			break;
		}
		if (impl_ver == IMPL_XNTPD) {
			fprintf(stderr,
			    "***Warning changing to older implementation\n");
			return INFO_ERR_IMPL;
		}

		fprintf(stderr,
		    "***Warning changing the request packet size from %d to %d\n",
		    oldsize, req_pkt_size);
		goto again;
	}

 	/* log error message if not told to be quiet */
 	if ((res > 0) && (((1 << res) & quiet_mask) == 0)) {
		switch(res) {
		    case INFO_ERR_IMPL:
			/* Give us a chance to try the older implementation. */
			if (implcode == IMPL_XNTPD)
				break;
			(void) fprintf(stderr,
				       "***Server implementation incompatable with our own\n");
			break;
		    case INFO_ERR_REQ:
			(void) fprintf(stderr,
				       "***Server doesn't implement this request\n");
			break;
		    case INFO_ERR_FMT:
			(void) fprintf(stderr,
				       "***Server reports a format error in the received packet (shouldn't happen)\n");
			break;
		    case INFO_ERR_NODATA:
			(void) fprintf(stderr,
				       "***Server reports data not found\n");
			break;
		    case INFO_ERR_AUTH:
			(void) fprintf(stderr, "***Permission denied\n");
			break;
		    case ERR_TIMEOUT:
			(void) fprintf(stderr, "***Request timed out\n");
			break;
		    case ERR_INCOMPLETE:
			(void) fprintf(stderr,
				       "***Response from server was incomplete\n");
			break;
		    default:
			(void) fprintf(stderr,
				       "***Server returns unknown error code %d\n", res);
			break;
		}
	}
	return res;
}


/*
 * getcmds - read commands from the standard input and execute them
 */
static void
getcmds(void)
{
#if defined(HAVE_LIBREADLINE) || defined(HAVE_LIBEDIT)
	char *line;

	for (;;) {
		if ((line = readline(interactive?prompt:"")) == NULL) return;
		if (*line) add_history(line);
		docmd(line);
		free(line);
	}
#else /* not (HAVE_LIBREADLINE || HAVE_LIBEDIT) */
	char line[MAXLINE];

	for (;;) {
		if (interactive) {
#ifdef VMS	/* work around a problem with mixing stdout & stderr */
			fputs("",stdout);
#endif
			(void) fputs(prompt, stderr);
			(void) fflush(stderr);
		}

		if (fgets(line, sizeof line, stdin) == NULL)
		    return;

		docmd(line);
	}
#endif /* not HAVE_LIBREADLINE || HAVE_LIBEDIT */
}


#ifndef SYS_WINNT /* Under NT cannot handle SIGINT, WIN32 spawns a handler */
/*
 * abortcmd - catch interrupts and abort the current command
 */
static RETSIGTYPE
abortcmd(
	int sig
	)
{

	if (current_output == stdout)
	    (void) fflush(stdout);
	putc('\n', stderr);
	(void) fflush(stderr);
	if (jump) longjmp(interrupt_buf, 1);
}
#endif /* SYS_WINNT */

/*
 * docmd - decode the command line and execute a command
 */
static void
docmd(
	const char *cmdline
	)
{
	char *tokens[1+MAXARGS+2];
	struct parse pcmd;
	int ntok;
	int i, ti;
	int rval;
	struct xcmd *xcmd;

	ai_fam_templ = ai_fam_default;
	/*
	 * Tokenize the command line.  If nothing on it, return.
	 */
	tokenize(cmdline, tokens, &ntok);
	if (ntok == 0)
	    return;
	
	/*
	 * Find the appropriate command description.
	 */
	i = findcmd(tokens[0], builtins, opcmds, &xcmd);
	if (i == 0) {
		(void) fprintf(stderr, "***Command `%s' unknown\n",
			       tokens[0]);
		return;
	} else if (i >= 2) {
		(void) fprintf(stderr, "***Command `%s' ambiguous\n",
			       tokens[0]);
		return;
	}
	
	/*
	 * Save the keyword, then walk through the arguments, interpreting
	 * as we go.
	 */
	pcmd.keyword = tokens[0];
	pcmd.nargs = 0;
	ti = 1;
	for (i = 0; i < MAXARGS && xcmd->arg[i] != NO;) {
		if ((i+ti) >= ntok) {
			if (!(xcmd->arg[i] & OPT)) {
				printusage(xcmd, stderr);
				return;
			}
			break;
		}
		if ((xcmd->arg[i] & OPT) && (*tokens[i+ti] == '>'))
			break;
		rval = getarg(tokens[i+ti], (int)xcmd->arg[i], &pcmd.argval[i]);
		if (rval == -1) {
			ti++;
			continue;
		}
		if (rval == 0)
			return;
		pcmd.nargs++;
		i++;
	}

	i += ti;
	if (i < ntok && *tokens[i] == '>') {
		char *fname;

		if (*(tokens[i]+1) != '\0')
		    fname = tokens[i]+1;
		else if ((i+1) < ntok)
		    fname = tokens[i+1];
		else {
			(void) fprintf(stderr, "***No file for redirect\n");
			return;
		}

		current_output = fopen(fname, "w");
		if (current_output == NULL) {
			(void) fprintf(stderr, "***Error opening %s: ", fname);
			perror("");
			return;
		}
	} else {
		current_output = stdout;
	}

	if (interactive && setjmp(interrupt_buf)) {
		return;
	} else {
		jump = 1;
		(xcmd->handler)(&pcmd, current_output);
		jump = 0;
		if (current_output != stdout)
			(void) fclose(current_output);
		current_output = NULL;
	}
}


/*
 * tokenize - turn a command line into tokens
 */
static void
tokenize(
	const char *line,
	char **tokens,
	int *ntok
	)
{
	register const char *cp;
	register char *sp;
	static char tspace[MAXLINE];

	sp = tspace;
	cp = line;
	for (*ntok = 0; *ntok < MAXTOKENS; (*ntok)++) {
		tokens[*ntok] = sp;
		while (ISSPACE(*cp))
		    cp++;
		if (ISEOL(*cp))
		    break;
		do {
			*sp++ = *cp++;
		} while (!ISSPACE(*cp) && !ISEOL(*cp));

		*sp++ = '\0';
	}
}



/*
 * findcmd - find a command in a command description table
 */
static int
findcmd(
	register char *str,
	struct xcmd *clist1,
	struct xcmd *clist2,
	struct xcmd **cmd
	)
{
	register struct xcmd *cl;
	register int clen;
	int nmatch;
	struct xcmd *nearmatch = NULL;
	struct xcmd *clist;

	clen = strlen(str);
	nmatch = 0;
	if (clist1 != 0)
	    clist = clist1;
	else if (clist2 != 0)
	    clist = clist2;
	else
	    return 0;

    again:
	for (cl = clist; cl->keyword != 0; cl++) {
		/* do a first character check, for efficiency */
		if (*str != *(cl->keyword))
		    continue;
		if (strncmp(str, cl->keyword, (unsigned)clen) == 0) {
			/*
			 * Could be extact match, could be approximate.
			 * Is exact if the length of the keyword is the
			 * same as the str.
			 */
			if (*((cl->keyword) + clen) == '\0') {
				*cmd = cl;
				return 1;
			}
			nmatch++;
			nearmatch = cl;
		}
	}

				/*
				 * See if there is more to do.  If so, go again.  Sorry about the
				 * goto, too much looking at BSD sources...
				 */
	if (clist == clist1 && clist2 != 0) {
		clist = clist2;
		goto again;
	}

				/*
				 * If we got extactly 1 near match, use it, else return number
				 * of matches.
				 */
	if (nmatch == 1) {
		*cmd = nearmatch;
		return 1;
	}
	return nmatch;
}


/*
 * getarg - interpret an argument token
 *
 * return:	 0 - failure
 *		 1 - success
 *		-1 - skip to next token
 */
static int
getarg(
	char *str,
	int code,
	arg_v *argp
	)
{
	int isneg;
	char *cp, *np;
	static const char *digits = "0123456789";

	switch (code & ~OPT) {
	    case NTP_STR:
		argp->string = str;
		break;
	    case ADD:
		if (!strcmp("-6", str)) {
			ai_fam_templ = AF_INET6;
			return -1;
		} else if (!strcmp("-4", str)) {
			ai_fam_templ = AF_INET;
			return -1;
		}
		if (!getnetnum(str, &(argp->netnum), (char *)0, 0)) {
			return 0;
		}
		break;
	    case INT:
	    case UINT:
		isneg = 0;
		np = str;
		if (*np == '-') {
			np++;
			isneg = 1;
		}

		argp->uval = 0;
		do {
			cp = strchr(digits, *np);
			if (cp == NULL) {
				(void) fprintf(stderr,
					       "***Illegal integer value %s\n", str);
				return 0;
			}
			argp->uval *= 10;
			argp->uval += (cp - digits);
		} while (*(++np) != '\0');

		if (isneg) {
			if ((code & ~OPT) == UINT) {
				(void) fprintf(stderr,
					       "***Value %s should be unsigned\n", str);
				return 0;
			}
			argp->ival = -argp->ival;
		}
		break;
	    case IP_VERSION:
		if (!strcmp("-6", str))
			argp->ival = 6 ;
		else if (!strcmp("-4", str))
			argp->ival = 4 ;
		else {
			(void) fprintf(stderr,
			    "***Version must be either 4 or 6\n");
			return 0;
		}
		break;
	}

	return 1;
}


/*
 * getnetnum - given a host name, return its net number
 *	       and (optional) full name
 */
static int
getnetnum(
	const char *hname,
	struct sockaddr_storage *num,
	char *fullhost,
	int af
	)
{
	int err;
	int sockaddr_len;
	struct addrinfo hints, *ai = NULL;

	sockaddr_len = (af == AF_INET)
			   ? sizeof(struct sockaddr_in)
			   : sizeof(struct sockaddr_in6);
	memset((char *)&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_CANONNAME;
#ifdef AI_ADDRCONFIG
	hints.ai_flags |= AI_ADDRCONFIG;
#endif
	
	/* decodenetnum only works with addresses */
	if (decodenetnum(hname, num)) {
		if (fullhost != 0) {
			getnameinfo((struct sockaddr *)num, sockaddr_len, 
				    fullhost, sizeof(fullhost), NULL, 0, 
				    NI_NUMERICHOST); 
		}
		return 1;
	} else if ((err = getaddrinfo(hname, "ntp", &hints, &ai)) == 0) {
		memmove((char *)num, ai->ai_addr, ai->ai_addrlen);
		if (fullhost != 0)
			(void) strcpy(fullhost, ai->ai_canonname);
		return 1;
	} else {
		(void) fprintf(stderr, "***Can't find host %s\n", hname);
		return 0;
	}
	/*NOTREACHED*/
}

/*
 * nntohost - convert network number to host name.  This routine enforces
 *	       the showhostnames setting.
 */
char *
nntohost(
	struct sockaddr_storage *netnum
	)
{
	if (!showhostnames)
	    return stoa(netnum);

	if ((netnum->ss_family == AF_INET) && ISREFCLOCKADR(netnum))
		return refnumtoa(netnum);
	return socktohost(netnum);
}


/*
 * Finally, the built in command handlers
 */

/*
 * help - tell about commands, or details of a particular command
 */
static void
help(
	struct parse *pcmd,
	FILE *fp
	)
{
	int i;
	int n;
	struct xcmd *xcp;
	char *cmd;
	const char *cmdsort[100];
	int length[100];
	int maxlength;
	int numperline;
	static const char *spaces = "                    ";	/* 20 spaces */

	if (pcmd->nargs == 0) {
		n = 0;
		for (xcp = builtins; xcp->keyword != 0; xcp++) {
			if (*(xcp->keyword) != '?')
			    cmdsort[n++] = xcp->keyword;
		}
		for (xcp = opcmds; xcp->keyword != 0; xcp++)
		    cmdsort[n++] = xcp->keyword;

#ifdef QSORT_USES_VOID_P
		qsort(cmdsort, (size_t)n, sizeof(char *), helpsort);
#else
		qsort((char *)cmdsort, (size_t)n, sizeof(char *), helpsort);
#endif

		maxlength = 0;
		for (i = 0; i < n; i++) {
			length[i] = strlen(cmdsort[i]);
			if (length[i] > maxlength)
			    maxlength = length[i];
		}
		maxlength++;
		numperline = 76 / maxlength;

		(void) fprintf(fp, "Commands available:\n");
		for (i = 0; i < n; i++) {
			if ((i % numperline) == (numperline-1)
			    || i == (n-1))
			    (void) fprintf(fp, "%s\n", cmdsort[i]);
			else
			    (void) fprintf(fp, "%s%s", cmdsort[i],
					   spaces+20-maxlength+length[i]);
		}
	} else {
		cmd = pcmd->argval[0].string;
		n = findcmd(cmd, builtins, opcmds, &xcp);
		if (n == 0) {
			(void) fprintf(stderr,
				       "Command `%s' is unknown\n", cmd);
			return;
		} else if (n >= 2) {
			(void) fprintf(stderr,
				       "Command `%s' is ambiguous\n", cmd);
			return;
		}
		(void) fprintf(fp, "function: %s\n", xcp->comment);
		printusage(xcp, fp);
	}
}


/*
 * helpsort - do hostname qsort comparisons
 */
#ifdef QSORT_USES_VOID_P
static int
helpsort(
	const void *t1,
	const void *t2
	)
{
	char const * const * name1 = (char const * const *)t1;
	char const * const * name2 = (char const * const *)t2;

	return strcmp(*name1, *name2);
}
#else
static int
helpsort(
	char **name1,
	char **name2
	)
{
	return strcmp(*name1, *name2);
}
#endif


/*
 * printusage - print usage information for a command
 */
static void
printusage(
	struct xcmd *xcp,
	FILE *fp
	)
{
	int i, opt46;

	opt46 = 0;
	(void) fprintf(fp, "usage: %s", xcp->keyword);
	for (i = 0; i < MAXARGS && xcp->arg[i] != NO; i++) {
		if (opt46 == 0 && (xcp->arg[i] & ~OPT) == ADD) {
			(void) fprintf(fp, " [ -4|-6 ]");
			opt46 = 1;
		}
		if (xcp->arg[i] & OPT)
		    (void) fprintf(fp, " [ %s ]", xcp->desc[i]);
		else
		    (void) fprintf(fp, " %s", xcp->desc[i]);
	}
	(void) fprintf(fp, "\n");
}


/*
 * timeout - set time out time
 */
static void
timeout(
	struct parse *pcmd,
	FILE *fp
	)
{
	int val;

	if (pcmd->nargs == 0) {
		val = tvout.tv_sec * 1000 + tvout.tv_usec / 1000;
		(void) fprintf(fp, "primary timeout %d ms\n", val);
	} else {
		tvout.tv_sec = pcmd->argval[0].uval / 1000;
		tvout.tv_usec = (pcmd->argval[0].uval - (tvout.tv_sec * 1000))
			* 1000;
	}
}


/*
 * my_delay - set delay for auth requests
 */
static void
my_delay(
	struct parse *pcmd,
	FILE *fp
	)
{
	int isneg;
	u_long val;

	if (pcmd->nargs == 0) {
		val = delay_time.l_ui * 1000 + delay_time.l_uf / 4294967;
		(void) fprintf(fp, "delay %lu ms\n", val);
	} else {
		if (pcmd->argval[0].ival < 0) {
			isneg = 1;
			val = (u_long)(-pcmd->argval[0].ival);
		} else {
			isneg = 0;
			val = (u_long)pcmd->argval[0].ival;
		}

		delay_time.l_ui = val / 1000;
		val %= 1000;
		delay_time.l_uf = val * 4294967;	/* 2**32/1000 */

		if (isneg)
		    L_NEG(&delay_time);
	}
}


/*
 * host - set the host we are dealing with.
 */
static void
host(
	struct parse *pcmd,
	FILE *fp
	)
{
	int i;

	if (pcmd->nargs == 0) {
		if (havehost)
		    (void) fprintf(fp, "current host is %s\n", currenthost);
		else
		    (void) fprintf(fp, "no current host\n");
		return;
	}

	i = 0;
	if (pcmd->nargs == 2) {
		if (!strcmp("-4", pcmd->argval[i].string))
			ai_fam_templ = AF_INET;
		else if (!strcmp("-6", pcmd->argval[i].string))
			ai_fam_templ = AF_INET6;
		else {
			if (havehost)
				(void) fprintf(fp,
				    "current host remains %s\n", currenthost);
			else
				(void) fprintf(fp, "still no current host\n");
			return;
		}
		i = 1;
	}
	if (openhost(pcmd->argval[i].string)) {
		(void) fprintf(fp, "current host set to %s\n", currenthost);
	} else {
		if (havehost)
		    (void) fprintf(fp,
				   "current host remains %s\n", currenthost);
		else
		    (void) fprintf(fp, "still no current host\n");
	}
}


/*
 * keyid - get a keyid to use for authenticating requests
 */
static void
keyid(
	struct parse *pcmd,
	FILE *fp
	)
{
	if (pcmd->nargs == 0) {
		if (info_auth_keyid == 0)
		    (void) fprintf(fp, "no keyid defined\n");
		else
		    (void) fprintf(fp, "keyid is %lu\n", (u_long)info_auth_keyid);
	} else {
		info_auth_keyid = pcmd->argval[0].uval;
	}
}


/*
 * keytype - get type of key to use for authenticating requests
 */
static void
keytype(
	struct parse *pcmd,
	FILE *fp
	)
{
	if (pcmd->nargs == 0)
	    fprintf(fp, "keytype is %s\n",
		    (info_auth_keytype == KEY_TYPE_MD5) ? "MD5" : "???");
	else
	    switch (*(pcmd->argval[0].string)) {
		case 'm':
		case 'M':
		    info_auth_keytype = KEY_TYPE_MD5;
		    break;

		default:
		    fprintf(fp, "keytype must be 'md5'\n");
	    }
}



/*
 * passwd - get an authentication key
 */
/*ARGSUSED*/
static void
passwd(
	struct parse *pcmd,
	FILE *fp
	)
{
	char *pass;

	if (info_auth_keyid == 0) {
		info_auth_keyid = getkeyid("Keyid: ");
		if (info_auth_keyid == 0) {
			(void)fprintf(fp, "Keyid must be defined\n");
			return;
		}
	}
	if (!interactive) {
		authusekey(info_auth_keyid, info_auth_keytype,
			   (u_char *)pcmd->argval[0].string);
		authtrust(info_auth_keyid, 1);
	} else {
		pass = getpass("MD5 Password: ");
		if (*pass == '\0')
		    (void) fprintf(fp, "Password unchanged\n");
		else {
		    authusekey(info_auth_keyid, info_auth_keytype,
			       (u_char *)pass);
		    authtrust(info_auth_keyid, 1);
		}
	}
}


/*
 * hostnames - set the showhostnames flag
 */
static void
hostnames(
	struct parse *pcmd,
	FILE *fp
	)
{
	if (pcmd->nargs == 0) {
		if (showhostnames)
		    (void) fprintf(fp, "hostnames being shown\n");
		else
		    (void) fprintf(fp, "hostnames not being shown\n");
	} else {
		if (STREQ(pcmd->argval[0].string, "yes"))
		    showhostnames = 1;
		else if (STREQ(pcmd->argval[0].string, "no"))
		    showhostnames = 0;
		else
		    (void)fprintf(stderr, "What?\n");
	}
}


/*
 * setdebug - set/change debugging level
 */
static void
setdebug(
	struct parse *pcmd,
	FILE *fp
	)
{
	if (pcmd->nargs == 0) {
		(void) fprintf(fp, "debug level is %d\n", debug);
		return;
	} else if (STREQ(pcmd->argval[0].string, "no")) {
		debug = 0;
	} else if (STREQ(pcmd->argval[0].string, "more")) {
		debug++;
	} else if (STREQ(pcmd->argval[0].string, "less")) {
		debug--;
	} else {
		(void) fprintf(fp, "What?\n");
		return;
	}
	(void) fprintf(fp, "debug level set to %d\n", debug);
}


/*
 * quit - stop this nonsense
 */
/*ARGSUSED*/
static void
quit(
	struct parse *pcmd,
	FILE *fp
	)
{
	if (havehost)
	    closesocket(sockfd);
	exit(0);
}


/*
 * version - print the current version number
 */
/*ARGSUSED*/
static void
version(
	struct parse *pcmd,
	FILE *fp
	)
{

	(void) fprintf(fp, "%s\n", Version);
	return;
}


/*
 * warning - print a warning message
 */
static void
warning(
	const char *fmt,
	const char *st1,
	const char *st2
	)
{
	(void) fprintf(stderr, "%s: ", progname);
	(void) fprintf(stderr, fmt, st1, st2);
	(void) fprintf(stderr, ": ");
	perror("");
}


/*
 * error - print a message and exit
 */
static void
error(
	const char *fmt,
	const char *st1,
	const char *st2
	)
{
	warning(fmt, st1, st2);
	exit(1);
}

/*
 * getkeyid - prompt the user for a keyid to use
 */
static u_long
getkeyid(
	const char *keyprompt
	)
{
	register char *p;
	register int c;
	FILE *fi;
	char pbuf[20];

#ifndef SYS_WINNT
	if ((fi = fdopen(open("/dev/tty", 2), "r")) == NULL)
#else
	    if ((fi = _fdopen((int)GetStdHandle(STD_INPUT_HANDLE), "r")) == NULL)
#endif /* SYS_WINNT */
		fi = stdin;
	    else
		setbuf(fi, (char *)NULL);
	fprintf(stderr, "%s", keyprompt); fflush(stderr);
	for (p=pbuf; (c = getc(fi))!='\n' && c!=EOF;) {
		if (p < &pbuf[18])
		    *p++ = (char) c;
	}
	*p = '\0';
	if (fi != stdin)
	    fclose(fi);
	return (u_int32)atoi(pbuf);
}
