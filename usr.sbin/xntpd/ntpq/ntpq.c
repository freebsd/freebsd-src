/* ntpq.c,v 3.1 1993/07/06 01:09:29 jbj Exp
 * ntpq - query an NTP server using mode 6 commands
 */
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>

#include "ntp_select.h"
#include "ntpq.h"
#include "ntp_unixtime.h"
#include "ntp_calendar.h"
#include "ntp_io.h"
#include "ntp_stdlib.h"

/*
 * Because we potentially understand a lot of commands we will run
 * interactive if connected to a terminal.
 */
int interactive = 0;		/* set to 1 when we should prompt */
char *prompt = "ntpq> ";	/* prompt to ask him about */


/*
 * Keyid used for authenticated requests.  Obtained on the fly.
 */
U_LONG info_auth_keyid = -1;

/*
 * Type of key md5 or des
 */
#define	KEY_TYPE_DES	3
#define	KEY_TYPE_MD5	4

int info_auth_keytype = KEY_TYPE_DES;	/* DES */

/*
 * Flag which indicates we should always send authenticated requests
 */
int always_auth = 0;

/*
 * Flag which indicates raw mode output.
 */
int rawmode = 0;

/*
 * Packet version number we use
 */
u_char pktversion = NTP_VERSION;

/*
 * Don't jump if no set jmp.
 */
int jump = 0;

/*
 * Format values
 */
#define	PADDING	0
#define	TS	1	/* time stamp */
#define	FL	2	/* l_fp type value */
#define	FU	3	/* u_fp type value */
#define	FS	4	/* s_fp type value */
#define	UI	5	/* unsigned integer value */
#define	IN	6	/* signed integer value */
#define	HA	7	/* host address */
#define	NA	8	/* network address */
#define	ST	9	/* string value */
#define	RF	10	/* refid (sometimes string, sometimes not) */
#define	LP	11	/* leap (print in binary) */
#define	OC	12	/* integer, print in octal */
#define	MD	13	/* mode */
#define	AR	14	/* array of times */
#define TST	15	/* test flags */
#define	EOV	255	/* end of table */


/*
 * System variable values.  The array can be indexed by
 * the variable index to find the textual name.
 */
struct ctl_var sys_var[] = {
	{ 0,		PADDING, "" },		/* 0 */
	{ CS_LEAP,	LP,	"leap" },	/* 1 */
	{ CS_STRATUM,	UI,	"stratum" },	/* 2 */
	{ CS_PRECISION,	IN,	"precision" },	/* 3 */
	{ CS_ROOTDELAY,	FU,	"rootdelay" },	/* 4 */
	{ CS_ROOTDISPERSION, FU, "rootdispersion" }, /* 5 */
	{ CS_REFID,	RF,	"refid" },	/* 6 */
	{ CS_REFTIME,	TS,	"reftime" },	/* 7 */
	{ CS_POLL,	UI,	"poll" },	/* 8 */
	{ CS_PEERID,	UI,	"peer" },	/* 9 */
	{ CS_OFFSET,	FL,	"phase" },	/* 10 */
	{ CS_DRIFT,	FS,	"freq" },	/* 11 */
	{ CS_COMPLIANCE, UI,	"compliance" },	/* 12 */
	{ CS_CLOCK,	TS,	"clock" },	/* 13 */
	{ CS_LEAPIND,	LP,	"leapindicator" },	/* 14 */
	{ CS_LEAPWARNING, LP,	"leapwarning" },	/* 15 */
	{ CS_PROCESSOR,	ST,	"processor" },	/* 16 */
	{ CS_SYSTEM,	ST,	"system" },	/* 17 */
	{ CS_KEYID,	UI,	"keyid" },	/* 18 */
	{ CS_REFSKEW,	FL,	"refskew" },	/* 19 */
	{ 0,		EOV,	""	}
};


/*
 * Peer variable list
 */
struct ctl_var peer_var[] = {
	{ 0,		PADDING, "" },		/* 0 */
	{ CP_CONFIG,	UI,	"config" },	/* 1 */
	{ CP_AUTHENABLE, UI,	"authenable" },	/* 2 */
	{ CP_AUTHENTIC,	UI,	"authentic" },	/* 3 */
	{ CP_SRCADR,	HA,	"srcadr" },	/* 4 */
	{ CP_SRCPORT,	UI,	"srcport" },	/* 5 */
	{ CP_DSTADR,	NA,	"dstadr" },	/* 6 */
	{ CP_DSTPORT,	UI,	"dstport" },	/* 7 */
	{ CP_LEAP,	LP,	"leap" },	/* 8 */
	{ CP_HMODE,	MD,	"hmode" },	/* 9 */
	{ CP_STRATUM,	UI,	"stratum" },	/* 10 */
	{ CP_PPOLL,	UI,	"ppoll" },	/* 11 */
	{ CP_HPOLL,	UI,	"hpoll" },	/* 12 */
	{ CP_PRECISION,	IN,	"precision" },	/* 13 */
	{ CP_ROOTDELAY,	FS,	"rootdelay" },	/* 14 */
	{ CP_ROOTDISPERSION, FU, "rootdispersion" }, /* 15 */
	{ CP_REFID,	RF,	"refid" },	/* 16 */
	{ CP_REFTIME,	TS,	"reftime" },	/* 17 */
	{ CP_ORG,	TS,	"org" },	/* 18 */
	{ CP_REC,	TS,	"rec" },	/* 19 */
	{ CP_XMT,	TS,	"xmt" },	/* 20 */
	{ CP_REACH,	OC,	"reach" },	/* 21 */
	{ CP_VALID,	UI,	"valid" },	/* 22 */
	{ CP_TIMER,	UI,	"timer" },	/* 23 */
	{ CP_DELAY,	AR,	"delay" },	/* 24 */
	{ CP_OFFSET,	AR,	"offset" },	/* 25 */
	{ CP_DISPERSION, FU,	"dispersion" },	/* 26 */
	{ CP_KEYID,	UI,	"keyid" },	/* 27 */
	{ CP_FILTDELAY,	AR,	"filtdelay" },	/* 28 */
	{ CP_FILTOFFSET, AR,	"filtoffset" },	/* 29 */
	{ CP_PMODE,	ST,	"pmode" },	/* 30 */
	{ CP_RECEIVED,	UI,	"received" },	/* 31 */
	{ CP_SENT,	UI,	"sent" },	/* 32 */
	{ CP_FILTERROR,	AR,	"filterror" },	/* 33 */
	{ CP_FLASH,     TST,	"flash"},	/* 34 */ 
	{ CP_DISP,      AR,	"disp" },	/* 35 */
	/*
	 * These are duplicate entires so that we can
	 * process deviant version of the xntp protocal.
	 */
	{ CP_SRCADR,	HA,	"peeraddr" },	/* 4 */
	{ CP_SRCPORT,	UI,	"peerport" },	/* 5 */
	{ CP_PPOLL,	UI,	"peerpoll" },	/* 11 */
	{ CP_HPOLL,	UI,	"hostpoll" },	/* 12 */
	{ 0,		EOV,	""	}
};


/*
 * Clock variable list
 */
struct ctl_var clock_var[] = {
	{ 0,		PADDING, "" },		/* 0 */
	{ CC_TYPE,	UI,	"type" },	/* 1 */
	{ CC_TIMECODE,	ST,	"timecode" },	/* 2 */
	{ CC_POLL,	UI,	"poll" },	/* 3 */
	{ CC_NOREPLY,	UI,	"noreply" },	/* 4 */
	{ CC_BADFORMAT,	UI,	"badformat" },	/* 5 */
	{ CC_BADDATA,	UI,	"baddata" },	/* 6 */
	{ CC_FUDGETIME1, FL,	"fudgetime1" },	/* 7 */
	{ CC_FUDGETIME2, FL,	"fudgetime2" },	/* 8 */
	{ CC_FUDGEVAL1,	IN,	"fudgeval1" },	/* 9 */
	{ CC_FUDGEVAL2,	IN,	"fudgeval2" },	/* 10 */
	{ CC_FLAGS,	UI,	"flags" },	/* 11 */
	{ CC_DEVICE,	ST,	"device" },	/* 12 */
	{ 0,		EOV,	""	}
};


/*
 * Structure for turning various constants into a readable string.
 */
struct codestring {
	int code;
	char *string;
};

/*
 * flasher bits
 */
static char *tstflagnames[] = {
      "DUPLICATE PKT",
      "BOGUS PKT",
      "PROTO UNSYNC",
      "PEER BOUNDS",
      "BAD AUTH",
      "PEER CLOCK UNSYNC",
      "BAD STRATUM",
      "ROOT BOUNDS"
};

/*
 * Leap values
 */
struct codestring leap_codes[] = {
	{ 0,	"leap_none" },
	{ 1,	"leap_add_sec" },
	{ 2,	"leap_del_sec" },
	{ 3,	"sync_alarm" },
	{ -1,	"leap" }
};


/*
 * Clock source
 */
struct codestring sync_codes[] = {
	{ CTL_SST_TS_UNSPEC,	"sync_unspec" },
	{ CTL_SST_TS_ATOM,	"sync_atomic" },
	{ CTL_SST_TS_LF,	"sync_lf_clock" },
	{ CTL_SST_TS_HF,	"sync_hf_clock" },
	{ CTL_SST_TS_UHF,	"sync_uhf_clock" },
	{ CTL_SST_TS_LOCAL,	"sync_local_proto" },
	{ CTL_SST_TS_NTP,	"sync_ntp" },
	{ CTL_SST_TS_UDPTIME,	"sync_udp/time" },
	{ CTL_SST_TS_WRSTWTCH,	"sync_wristwatch" },
	{ CTL_SST_TS_TELEPHONE,	"sync_telephone" },
	{ -1,			"sync" }
};


/*
 * Peer selection
 */
struct codestring select_codes[] = {
	{ CTL_PST_SEL_REJECT,	"sel_reject" },
	{ CTL_PST_SEL_SANE,	"sel_sane" },
	{ CTL_PST_SEL_CORRECT,	"sel_correct" },
	{ CTL_PST_SEL_SELCAND,	"sel_candidate" },
	{ CTL_PST_SEL_SYNCCAND,	"sel_sync" },
	{ CTL_PST_SEL_DISTSYSPEER, "sel_sys.peer, hi_dist" },
	{ CTL_PST_SEL_SYSPEER,	"sel_sys.peer" },
	{ -1,			"sel" }
};


/*
 * Clock status
 */
struct codestring clock_codes[] = {
	{ CTL_CLK_OKAY,		"clk_okay" },
	{ CTL_CLK_NOREPLY,	"clk_noreply" },
	{ CTL_CLK_BADFORMAT,	"clk_badformat" },
	{ CTL_CLK_FAULT,	"clk_fault" },
	{ CTL_CLK_PROPAGATION,	"clk_propagation" },
	{ CTL_CLK_BADDATE,	"clk_baddate" },
	{ CTL_CLK_BADTIME,	"clk_badtime" },
	{ -1,			"clk" }
};


/*
 * System Events
 */
struct codestring sys_codes[] = {
	{ EVNT_UNSPEC,		"event_unspec" },
	{ EVNT_SYSRESTART,	"event_restart" },
	{ EVNT_SYSFAULT,	"event_fault" },
	{ EVNT_SYNCCHG,		"event_sync_chg" },
	{ EVNT_PEERSTCHG,	"event_peer/strat_chg" },
	{ EVNT_CLOCKRESET,	"event_clock_reset" },
	{ EVNT_BADDATETIM,	"event_bad_date" },
	{ EVNT_CLOCKEXCPT,	"event_clock_excptn" },
	{ -1,			"event" }
};

/*
 * Peer Events
 */
struct codestring peer_codes[] = {
	{ EVNT_UNSPEC,			"event_unspec" },
	{ EVNT_PEERIPERR & ~PEER_EVENT,	"event_ip_err" },
	{ EVNT_PEERAUTH & ~PEER_EVENT,	"event_authen" },
	{ EVNT_UNREACH & ~PEER_EVENT,	"event_unreach" },
	{ EVNT_REACH & ~PEER_EVENT,	"event_reach" },
#if 0
	{ EVNT_PEERSTRAT & ~PEER_EVENT,	"event_stratum_chg" },
#endif
	{ -1,				"event" }
};


/*
 * Built in command handler declarations
 */
static	int	openhost	P((char *));
static	int	sendpkt		P((char *, int));
static	int	getresponse	P((int, int, u_short *, int *, char **, int));
static	int	sendrequest	P((int, int, int, int, char *));
static	void	getcmds		P((void));
static	RETSIGTYPE abortcmd	P((int));
static	void	docmd		P((char *));
static	void	tokenize	P((char *, char **, int *));
static	int	findcmd		P((char *, struct xcmd *, struct xcmd *, struct xcmd **));
static	int	getarg		P((char *, int, arg_v *));
static	int	rtdatetolfp	P((char *, l_fp *));

#ifdef	UNUSED
static	int	decodereach	P((char *, U_LONG *));
#endif	/* UNUSED */

static	int	decodearr	P((char *, int *, l_fp *));
static	char *	getcode		P((int, struct codestring *));
static	void	help		P((struct parse *, FILE *));
#if defined(sgi) || defined(SYS_BSDI) || defined(__STDC__)
static	int	helpsort	P((const void *, const void *));
#else
static	int	helpsort	P((char **, char **));
#endif /* sgi */
static	void	printusage	P((struct xcmd *, FILE *));
static	void	timeout		P((struct parse *, FILE *));
static	void	delay		P((struct parse *, FILE *));
static	void	host		P((struct parse *, FILE *));
static	void	ntp_poll	P((struct parse *, FILE *));
static	void	keyid		P((struct parse *, FILE *));
static	void	keytype		P((struct parse *, FILE *));
static	void	passwd		P((struct parse *, FILE *));
static	void	hostnames	P((struct parse *, FILE *));
static	void	setdebug	P((struct parse *, FILE *));
static	void	quit		P((struct parse *, FILE *));
static	void	version		P((struct parse *, FILE *));
static	void	raw		P((struct parse *, FILE *));
static	void	cooked		P((struct parse *, FILE *));
static	void	authenticate	P((struct parse *, FILE *));
static	void	ntpversion	P((struct parse *, FILE *));
static	void	warning		P((char *, char *, char *));
static	void	error		P((char *, char *, char *));
static	U_LONG	getkeyid	P((char *));
static	void	atoascii	P((int, char *, char *));
static	void	makeascii	P((int, char *, FILE *));
static	char *	getevents	P((int));
static	char *	statustoa	P((int, int));
static	void	rawprint	P((int, int, char *, int, FILE *));
static	void	startoutput	P((void));
static	void	output		P((FILE *, char *, char *));
static	void	endoutput	P((FILE *));
static	void	outputarr	P((FILE *, char *, int, l_fp *));
static	void	cookedprint	P((int, int, char *, int, FILE *));
#if defined(sgi) || defined(SYS_BSDI) || defined(__STDC__)
static	int	assoccmp	P((const void *, const void *));
#else
static	int	assoccmp	P((struct association *, struct association *));
#endif /* sgi || bsdi */


/*
 * Built-in commands we understand
 */
struct xcmd builtins[] = {
	{ "?",		help,		{  OPT|STR, NO, NO, NO },
					{ "command", "", "", "" },
			"tell the use and syntax of commands" },
	{ "help",	help,		{  OPT|STR, NO, NO, NO },
					{ "command", "", "", "" },
			"tell the use and syntax of commands" },
	{ "timeout",	timeout,	{ OPT|UINT, NO, NO, NO },
					{ "msec", "", "", "" },
			"set the primary receive time out" },
	{ "delay",	delay,		{ OPT|INT, NO, NO, NO },
					{ "msec", "", "", "" },
			"set the delay added to encryption time stamps" },
	{ "host",	host,		{ OPT|STR, NO, NO, NO },
					{ "hostname", "", "", "" },
			"specify the host whose NTP server we talk to" },
	{ "poll",	ntp_poll,	{ OPT|UINT, OPT|STR, NO, NO },
					{ "n", "verbose", "", "" },
			"poll an NTP server in client mode `n' times" },
	{ "passwd",	passwd,		{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"specify a password to use for authenticated requests"},
	{ "hostnames",	hostnames,	{ OPT|STR, NO, NO, NO },
					{ "yes|no", "", "", "" },
			"specify whether hostnames or net numbers are printed"},
	{ "debug",	setdebug,	{ OPT|STR, NO, NO, NO },
					{ "no|more|less", "", "", "" },
			"set/change debugging level" },
	{ "quit",	quit,		{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"exit ntpq" },
	{ "keyid",	keyid,		{ OPT|UINT, NO, NO, NO },
					{ "key#", "", "", "" },
			"set keyid to use for authenticated requests" },
	{ "version",	version,	{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"print version number" },
	{ "raw",	raw,		{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"do raw mode variable output" },
	{ "cooked",	cooked,		{ NO, NO, NO, NO },
					{ "", "", "", "" },
			"do cooked mode variable output" },
	{ "authenticate", authenticate,	{ OPT|STR, NO, NO, NO },
					{ "yes|no", "", "", "" },
			"always authenticate requests to this server" },
	{ "ntpversion",	ntpversion,	{ OPT|UINT, NO, NO, NO },
					{ "version number", "", "", "" },
			"set the NTP version number to use for requests" },
	{ "keytype",	keytype,	{ STR, NO, NO, NO },
					{ "key type (md5|des)", "", "", "" },
			"set key type to use for authenticated requests (des|md5)" },
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
#define	LENHOSTNAME	256		/* host name is 256 characters LONG */
#define	MAXCMDS		100		/* maximum commands on cmd line */
#define	MAXHOSTS	100		/* maximum hosts on cmd line */
#define	MAXLINE		512		/* maximum line length */
#define	MAXTOKENS	(1+MAXARGS+2)	/* maximum number of usable tokens */
#define	MAXVARLEN	256		/* maximum length of a variable name */
#define	MAXVALLEN	256		/* maximum length of a variable value */
#define	MAXOUTLINE	72		/* maximum length of an output line */

/*
 * Some variables used and manipulated locally
 */
struct timeval tvout = { DEFTIMEOUT, 0 };	/* time out for reads */
struct timeval tvsout = { DEFSTIMEOUT, 0 };	/* secondary time out */
l_fp delay_time;				/* delay time */
char currenthost[LENHOSTNAME];			/* current host name */
struct sockaddr_in hostaddr = { 0 };		/* host address */
int showhostnames = 1;				/* show host names by default */

int sockfd;					/* fd socket is openned on */
int havehost = 0;				/* set to 1 when host open */
struct servent *server_entry = NULL;		/* server entry for ntp */

/*
 * Sequence number used for requests.  It is incremented before
 * it is used.
 */
u_short sequence;

/*
 * Holds data returned from queries.  Declare buffer LONG to be sure of
 * alignment.
 */
#define	MAXFRAGS	24		/* maximum number of fragments */
#define	DATASIZE	(MAXFRAGS*480)	/* maximum amount of data */
LONG pktdata[DATASIZE/sizeof(LONG)];

/*
 * Holds association data for use with the &n operator.
 */
struct association assoc_cache[MAXASSOC];
int numassoc = 0;		/* number of cached associations */

/*
 * For commands typed on the command line (with the -c option)
 */
int numcmds = 0;
char *ccmds[MAXCMDS];
#define	ADDCMD(cp)	if (numcmds < MAXCMDS) ccmds[numcmds++] = (cp)

/*
 * When multiple hosts are specified.
 */
int numhosts = 0;
char *chosts[MAXHOSTS];
#define	ADDHOST(cp)	if (numhosts < MAXHOSTS) chosts[numhosts++] = (cp)

/*
 * Error codes for internal use
 */
#define	ERR_UNSPEC		256
#define	ERR_INCOMPLETE		257
#define	ERR_TIMEOUT		258
#define	ERR_TOOMUCH		259

/*
 * Macro definitions we use
 */
#define	ISSPACE(c)	((c) == ' ' || (c) == '\t')
#define	ISEOL(c)	((c) == '\n' || (c) == '\r' || (c) == '\0')
#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

/*
 * Jump buffer for longjumping back to the command level
 */
jmp_buf interrupt_buf;

/*
 * Points at file being currently printed into
 */
FILE *current_output;

/*
 * Command table imported from ntpdc_ops.c
 */
extern struct xcmd opcmds[];

char *progname;
int debug;

/*
 * main - parse arguments and handle options
 */
void
main(argc, argv)
int argc;
char *argv[];
{
	int c;
	int errflg = 0;
	extern int ntp_optind;
	extern char *ntp_optarg;

	delay_time.l_ui = 0;
	delay_time.l_uf = DEFDELAY;

	progname = argv[0];
	while ((c = ntp_getopt(argc, argv, "c:dinp")) != EOF)
		switch (c) {
		case 'c':
			ADDCMD(ntp_optarg);
			break;
		case 'd':
			++debug;
			break;
		case 'i':
			interactive = 1;
			break;
		case 'n':
			showhostnames = 0;
			break;
		case 'p':
			ADDCMD("peers");
			break;
		default:
			errflg++;
			break;
		}
	if (errflg) {
		(void) fprintf(stderr,
		    "usage: %s [-dinp] [-c cmd] host ...\n",
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

	if (interactive)
		(void) signal_no_reset(SIGINT, abortcmd);

	if (numcmds == 0) {
		(void) openhost(chosts[0]);
		getcmds();
	} else {
		int ihost;
		int icmd;

		for (ihost = 0; ihost < numhosts; ihost++) {
			if (openhost(chosts[ihost]))
				for (icmd = 0; icmd < numcmds; icmd++)
					docmd(ccmds[icmd]);
		}
	}
	exit(0);
}


/*
 * openhost - open a socket to a host
 */
static int
openhost(hname)
	char *hname;
{
	U_LONG netnum;
	char temphost[LENHOSTNAME];

	if (server_entry == NULL) {
		server_entry = getservbyname("ntp", "udp");
		if (server_entry == NULL) {
			(void) fprintf(stderr, "%s: ntp/udp: unknown service\n",
			    progname);
			exit(1);
		}
		if (debug > 2)
			printf("Got ntp/udp service entry\n");
	}

	if (!getnetnum(hname, &netnum, temphost))
		return 0;
	
	if (debug > 2)
		printf("Opening host %s\n", temphost);

	if (havehost == 1) {
		if (debug > 2)
			printf("Closing old host %s\n", currenthost);
		(void) close(sockfd);
		havehost = 0;
	}
	(void) strcpy(currenthost, temphost);

	hostaddr.sin_family = AF_INET;
	hostaddr.sin_port = server_entry->s_port;
	hostaddr.sin_addr.s_addr = netnum;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1)
		error("socket", "", "");
	
#if defined(SYS_HPUX) && (SYS_HPUX < 8)
#ifdef SO_RCVBUF
	{ int rbufsize = DATASIZE + 2048;	/* 2K for slop */
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
	    &rbufsize, sizeof(int)) == -1)
		error("setsockopt", "", "");
	}
#endif
#endif

	if (connect(sockfd, (struct sockaddr *)&hostaddr,
		    sizeof(hostaddr)) == -1)
		error("connect", "", "");
	
	havehost = 1;
	return 1;
}


/* XXX ELIMINATE sendpkt similar in ntpq.c, ntpdc.c, ntp_io.c, ntptrace.c */
/*
 * sendpkt - send a packet to the remote host
 */
static int
sendpkt(xdata, xdatalen)
	char *xdata;
	int xdatalen;
{
	if (debug >= 3)
		printf("Sending %d octets\n", xdatalen);

	if (write(sockfd, xdata, xdatalen) == -1) {
		warning("write to %s failed", currenthost, "");
		return -1;
	}

	if (debug >= 4) {
		int first = 8;
		printf("Packet data:\n");
		while (xdatalen-- > 0) {
			if (first-- == 0) {
				printf("\n");
				first = 7;
			}
			printf(" %02x", *xdata++ & 0xff);
		}
		printf("\n");
	}
	return 0;
}



/*
 * getresponse - get a (series of) response packet(s) and return the data
 */
static int
getresponse(opcode, associd, rstatus, rsize, rdata, timeo)
	int opcode;
	int associd;
	u_short *rstatus;
	int *rsize;
	char **rdata;
	int timeo;
{
	struct ntp_control rpkt;
	struct timeval tvo;
	u_short offsets[MAXFRAGS+1];
	u_short counts[MAXFRAGS+1];
	u_short offset;
	u_short count;
	int numfrags;
	int seenlastfrag;
	fd_set fds;
	int n;

	/*
	 * This is pretty tricky.  We may get between 1 and MAXFRAG packets
	 * back in response to the request.  We peel the data out of
	 * each packet and collect it in one LONG block.  When the last
	 * packet in the sequence is received we'll know how much data we
	 * should have had.  Note we use one LONG time out, should reconsider.
	 */
	*rsize = 0;
	if (rstatus)
		*rstatus = 0;
	*rdata = (char *)pktdata;

	numfrags = 0;
	seenlastfrag = 0;

	FD_ZERO(&fds);

again:
	if (numfrags == 0)
		tvo = tvout;
	else
		tvo = tvsout;
	
	FD_SET(sockfd, &fds);
	n = select(sockfd+1, &fds, (fd_set *)0, (fd_set *)0, &tvo);

	if (debug >= 1)
		printf("select() returns %d\n", n);

	if (n == -1) {
		warning("select fails", "", "");
		return -1;
	}
	if (n == 0) {
		/*
		 * Timed out.  Return what we have
		 */
		if (numfrags == 0) {
			if (timeo)
				(void) fprintf(stderr,
				    "%s: timed out, nothing received\n",
				    currenthost);
			return ERR_TIMEOUT;
		} else {
			if (timeo)
				(void) fprintf(stderr,
				    "%s: timed out with incomplete data\n",
				    currenthost);
			if (debug) {
				printf("Received fragments:\n");
				for (n = 0; n < numfrags; n++)
					printf("%4d %d\n", offsets[n],
					    counts[n]);
				if (seenlastfrag)
					printf("last fragment received\n");
				else
					printf("last fragment not received\n");
			}
			return ERR_INCOMPLETE;
		}
	}

	n = read(sockfd, (char *)&rpkt, sizeof(rpkt));
	if (n == -1) {
		warning("read", "", "");
		return -1;
	}

	if (debug >= 4) {
	    int len = n, first = 8;
	    char *data = (char *)&rpkt;

	    printf("Packet data:\n");
	    while (len-- > 0) {
		if (first-- == 0) {
		    printf("\n");
		    first = 7;
		}
		printf(" %02x", *data++ & 0xff);
	    }
	    printf("\n");
	}

	/*
	 * Check for format errors.  Bug proofing.
	 */
	if (n < CTL_HEADER_LEN) {
		if (debug)
			printf("Short (%d byte) packet received\n", n);
		goto again;
	}
	if (PKT_VERSION(rpkt.li_vn_mode) > NTP_VERSION
	    || PKT_VERSION(rpkt.li_vn_mode) <= NTP_OLDVERSION) {
		if (debug)
			printf("Packet received with version %d\n",
			    PKT_VERSION(rpkt.li_vn_mode));
		goto again;
	}
	if (PKT_MODE(rpkt.li_vn_mode) != MODE_CONTROL) {
		if (debug)
			printf("Packet received with mode %d\n",
			    PKT_MODE(rpkt.li_vn_mode));
		goto again;
	}
	if (!CTL_ISRESPONSE(rpkt.r_m_e_op)) {
		if (debug)
			printf("Received request packet, wanted response\n");
		goto again;
	}

	/*
	 * Check opcode and sequence number for a match.
	 * Could be old data getting to us.
	 */
	if (ntohs(rpkt.sequence) != sequence) {
		if (debug)
			printf(
			    "Received sequnce number %d, wanted %d\n",
			    ntohs(rpkt.sequence), sequence);
		goto again;
	}
	if (CTL_OP(rpkt.r_m_e_op) != opcode) {
		if (debug)
			printf(
		"Received opcode %d, wanted %d (sequence number okay)\n",
			 CTL_OP(rpkt.r_m_e_op), opcode);
		goto again;
	}

	/*
	 * Check the error code.  If non-zero, return it.
	 */
	if (CTL_ISERROR(rpkt.r_m_e_op)) {
		int errcode;

		errcode = (ntohs(rpkt.status) >> 8) & 0xff;
		if (debug && CTL_ISMORE(rpkt.r_m_e_op)) {
			printf("Error code %d received on not-final packet\n",
			    errcode);
		}
		if (errcode == CERR_UNSPEC)
			return ERR_UNSPEC;
		return errcode;
	}

	/*
	 * Check the association ID to make sure it matches what
	 * we sent.
	 */
	if (ntohs(rpkt.associd) != associd) {
		if (debug)
			printf("Association ID %d doesn't match expected %d\n",
			    ntohs(rpkt.associd), associd);
	/*
	 * Hack for silly fuzzballs which, at the time of writing,
	 * return an assID of sys.peer when queried for system variables.
	 */
#ifdef notdef
		goto again;
#endif
	}

	/*
	 * Collect offset and count.  Make sure they make sense.
	 */
	offset = ntohs(rpkt.offset);
	count = ntohs(rpkt.count);

	if (debug >= 3) {
		int shouldbesize;
		U_LONG key;
		U_LONG *lpkt;
		int maclen;

		/*
		 * Usually we ignore authentication, but for debugging purposes
		 * we watch it here.
		 */
		shouldbesize = CTL_HEADER_LEN + count;

		/* round to 8 octet boundary */
		shouldbesize = (shouldbesize + 7) & ~7;

		if (n & 0x3) {
			printf("Packet not padded, size = %d\n", n);
		} if ((maclen = n - shouldbesize) >= MIN_MAC_LEN) {
			printf(
"Packet shows signs of authentication (total %d, data %d, mac %d)\n",
			    n, shouldbesize, maclen);
			lpkt = (U_LONG *)&rpkt;
			printf("%08lx %08lx %08lx %08lx %08lx %08lx\n",
			    ntohl(lpkt[(n - maclen)/sizeof(U_LONG) - 3]),
			    ntohl(lpkt[(n - maclen)/sizeof(U_LONG) - 2]),
			    ntohl(lpkt[(n - maclen)/sizeof(U_LONG) - 1]),
			    ntohl(lpkt[(n - maclen)/sizeof(U_LONG)]),
			    ntohl(lpkt[(n - maclen)/sizeof(U_LONG) + 1]),
			    ntohl(lpkt[(n - maclen)/sizeof(U_LONG) + 2]));
			key = ntohl(lpkt[(n - maclen) / sizeof(U_LONG)]);
			printf("Authenticated with keyid %lu\n", key);
			if (key != 0 && key != info_auth_keyid) {
				printf("We don't know that key\n");
			} else {
				if (authdecrypt(key, (U_LONG *)&rpkt,
						(n - maclen))) {
					printf("Auth okay!\n");
				} else {
					printf("Auth failed!\n");
				}
			}
		}
	}

	if (debug >= 2)
		printf("Got packet, size = %d\n", n);
	if (count > (u_short)(n-CTL_HEADER_LEN)) {
		if (debug)
			printf(
			  "Received count of %d octets, data in packet is %d\n",
			    count, n-CTL_HEADER_LEN);
		goto again;
	}
	if (count == 0 && CTL_ISMORE(rpkt.r_m_e_op)) {
		if (debug)
			printf("Received count of 0 in non-final fragment\n");
		goto again;
	}
	if (offset + count > sizeof(pktdata)) {
		if (debug)
			printf("Offset %d, count %d, too big for buffer\n",
				offset, count);
		return ERR_TOOMUCH;
	}
	if (seenlastfrag && !CTL_ISMORE(rpkt.r_m_e_op)) {
		if (debug)
			printf("Received second last fragment packet\n");
		goto again;
	}

	/*
	 * So far, so good.  Record this fragment, making sure it doesn't
	 * overlap anything.
	 */
	if (debug >= 2)
		printf("Packet okay\n");;

	if (numfrags == MAXFRAGS) {
		if (debug)
			printf("Number of fragments exceeds maximum\n");
		return ERR_TOOMUCH;
	}
	
	for (n = 0; n < numfrags; n++) {
		if (offset == offsets[n])
			goto again;	/* duplicate */
		if (offset < offsets[n])
			break;
	}
	
	if ((u_short)(n > 0 && offsets[n-1] + counts[n-1]) > offset)
		goto overlap;
	if (n < numfrags && (u_short)(offset + count) > offsets[n])
		goto overlap;
	
	{
		register int i;
		
		for (i = numfrags; i > n; i--) {
			offsets[i] = offsets[i-1];
			counts[i] = counts[i-1];
		}
	}
	offsets[n] = offset;
	counts[n] = count;
	numfrags++;

	/*
	 * Got that stuffed in right.  Figure out if this was the last.
	 * Record status info out of the last packet.
	 */
	if (!CTL_ISMORE(rpkt.r_m_e_op)) {
		seenlastfrag = 1;
		if (rstatus != 0)
			*rstatus = ntohs(rpkt.status);
	}

	/*
	 * Copy the data into the data buffer.
	 */
	memmove((char *)pktdata + offset, (char *)rpkt.data, count);

	/*
	 * If we've seen the last fragment, look for holes in the sequence.
	 * If there aren't any, we're done.
	 */
	if (seenlastfrag && offsets[0] == 0) {
		for (n = 1; n < numfrags; n++) {
			if (offsets[n-1] + counts[n-1] != offsets[n])
				break;
		}
		if (n == numfrags) {
			*rsize = offsets[numfrags-1] + counts[numfrags-1];
			return 0;
		}
	}
	goto again;

overlap:
	/*
	 * Print debugging message about overlapping fragments
	 */
	if (debug)
		printf("Overlapping fragments returned in response\n");
	goto again;
}


/*
 * sendrequest - format and send a request packet
 */
static int
sendrequest(opcode, associd, auth, qsize, qdata)
	int opcode;
	int associd;
	int auth;
	int qsize;
	char *qdata;
{
	struct ntp_control qpkt;
	int pktsize;

	/*
	 * Check to make sure the data will fit in one packet
	 */
	if (qsize > CTL_MAX_DATA_LEN) {
		(void) fprintf(stderr,
		    "***Internal error!  qsize (%d) too large\n",
		    qsize);
		return 1;
	}

	/*
	 * Fill in the packet
	 */
	qpkt.li_vn_mode = PKT_LI_VN_MODE(0, pktversion, MODE_CONTROL);
	qpkt.r_m_e_op = (u_char)opcode & CTL_OP_MASK;
	qpkt.sequence = htons(sequence);
	qpkt.status = 0;
	qpkt.associd = htons((u_short)associd);
	qpkt.offset = 0;
	qpkt.count = htons((u_short)qsize);

	/*
	 * If we have data, copy it in and pad it out to a 64
	 * bit boundary.
	 */
	if (qsize > 0) {
		memmove((char *)qpkt.data, qdata, qsize);
		pktsize = qsize + CTL_HEADER_LEN;
		while (pktsize & (sizeof(U_LONG)-1)) {
			qpkt.data[qsize++] = 0;
			pktsize++;
		}
	} else {
		pktsize = CTL_HEADER_LEN;
	}

	/*
	 * If it isn't authenticated we can just send it.  Otherwise
	 * we're going to have to think about it a little.
	 */
	if (!auth && !always_auth) {
		return sendpkt((char *)&qpkt, pktsize);
	} else {
		char *pass;

		/*
		 * Pad out packet to a multiple of 8 octets to be sure
		 * receiver can handle it.
		 */
		while (pktsize & 7) {
			qpkt.data[qsize++] = 0;
			pktsize++;
		}

		/*
		 * Get the keyid and the password if we don't have one.
		 */
		if (info_auth_keyid == -1) {
			info_auth_keyid = getkeyid("Keyid: ");
			if (info_auth_keyid == -1) {
				(void) fprintf(stderr,
				   "Keyid must be defined, request not sent\n");
				return 1;
			}
		}
		if (!auth_havekey(info_auth_keyid)) {
			pass = getpass("Password: ");
			if (*pass != '\0')
				authusekey(info_auth_keyid,
					   info_auth_keytype, pass);
		}
		if (auth_havekey(info_auth_keyid)) {
			int maclen;

			/*
			 * Stick the keyid in the packet where
			 * cp currently points.  Cp should be aligned
			 * properly.  Then do the encryptions.
			 */
			*(U_LONG *)(&qpkt.data[qsize]) = htonl(info_auth_keyid);
			maclen = authencrypt(info_auth_keyid, (U_LONG *)&qpkt,
					     pktsize);
			return sendpkt((char *)&qpkt, pktsize + maclen);
		} else {
			(void) fprintf(stderr,
			    "No password, request not sent\n");
			return 1;
		}
	}
	/*NOTREACHED*/
}


/*
 * doquery - send a request and process the response
 */
int
doquery(opcode, associd, auth, qsize, qdata, rstatus, rsize, rdata)
	int opcode;
	int associd;
	int auth;
	int qsize;
	char *qdata;
	u_short *rstatus;
	int *rsize;
	char **rdata;
{
	int res;
	int done;

	/*
	 * Check to make sure host is open
	 */
	if (!havehost) {
		(void) fprintf(stderr, "***No host open, use `host' command\n");
		return -1;
	}

	done = 0;
	sequence++;

again:
	/*
	 * send a request
	 */
	res = sendrequest(opcode, associd, auth, qsize, qdata);
	if (res != 0)
		return res;
	
	/*
	 * Get the response.  If we got a standard error, print a message
	 */
	res = getresponse(opcode, associd, rstatus, rsize, rdata, done);

	if (res > 0) {
		if (!done && (res == ERR_TIMEOUT || res == ERR_INCOMPLETE)) {
			if (res == ERR_INCOMPLETE) {
				/*
				 * better bump the sequence so we don't
				 * get confused about differing fragments.
				 */
				sequence++;
			}
			done = 1;
			goto again;
		}
		switch(res) {
		case CERR_BADFMT:
			(void) fprintf(stderr,
		  	   "***Server reports a bad format request packet\n");
			break;
		case CERR_PERMISSION:
			(void) fprintf(stderr,
			    "***Server disallowed request (authentication?)\n");
			break;
		case CERR_BADOP:
			(void) fprintf(stderr,
			    "***Server reports a bad opcode in request\n");
			break;
		case CERR_BADASSOC:
			(void) fprintf(stderr,
			    "***Association ID %d unknown to server\n",associd);
			break;
		case CERR_UNKNOWNVAR:
			(void) fprintf(stderr,
			   "***A request variable was unknown to the server\n");
			break;
		case CERR_BADVALUE:
			(void) fprintf(stderr,
			    "***Server indicates a request variable was bad\n");
			break;
		case ERR_UNSPEC:
			(void) fprintf(stderr,
			    "***Server returned an unspecified error\n");
			break;
		case ERR_TIMEOUT:
			(void) fprintf(stderr, "***Request timed out\n");
			break;
		case ERR_INCOMPLETE:
			(void) fprintf(stderr,
			    "***Response from server was incomplete\n");
			break;
		case ERR_TOOMUCH:
			(void) fprintf(stderr,
			    "***Buffer size exceeded for returned data\n");
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
getcmds()
{
	char line[MAXLINE];

	for (;;) {
		if (interactive) {
			(void) fputs(prompt, stderr);
			(void) fflush(stderr);
		}

		if (fgets(line, sizeof line, stdin) == NULL)
			return;

		docmd(line);
	}
}


/*
 * abortcmd - catch interrupts and abort the current command
 */
static RETSIGTYPE
abortcmd(sig)
int sig;
{
	if (current_output == stdout)
		(void) fflush(stdout);
	putc('\n', stderr);
	(void) fflush(stderr);
	if (jump) longjmp(interrupt_buf, 1);
}


/*
 * docmd - decode the command line and execute a command
 */
static void
docmd(cmdline)
	char *cmdline;
{
	char *tokens[1+MAXARGS+2];
	struct parse pcmd;
	int ntok;
	static int i;
	struct xcmd *xcmd;

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
	for (i = 0; i < MAXARGS && xcmd->arg[i] != NO; i++) {
		if ((i+1) >= ntok) {
			if (!(xcmd->arg[i] & OPT)) {
				printusage(xcmd, stderr);
				return;
			}
			break;
		}
		if ((xcmd->arg[i] & OPT) && (*tokens[i+1] == '>'))
			break;
		if (!getarg(tokens[i+1], (int)xcmd->arg[i], &pcmd.argval[i]))
			return;
		pcmd.nargs++;
	}

	i++;
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
		i = 1;		/* flag we need a close */
	} else {
		current_output = stdout;
		i = 0;		/* flag no close */
	}

	if (interactive && setjmp(interrupt_buf)) {
		return;
	} else {
		jump++;
		(xcmd->handler)(&pcmd, current_output);
		if (i) (void) fclose(current_output);
	}
}


/*
 * tokenize - turn a command line into tokens
 */
static void
tokenize(line, tokens, ntok)
	char *line;
	char **tokens;
	int *ntok;
{
	register char *cp;
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
findcmd(str, clist1, clist2, cmd)
	register char *str;
	struct xcmd *clist1;
	struct xcmd *clist2;
	struct xcmd **cmd;
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
		if (strncmp(str, cl->keyword, clen) == 0) {
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
 */
static int
getarg(str, code, argp)
	char *str;
	int code;
	arg_v *argp;
{
	int isneg;
	char *cp, *np;
	static char *digits = "0123456789";

	switch (code & ~OPT) {
	case STR:
		argp->string = str;
		break;
	case ADD:
		if (!getnetnum(str, &(argp->netnum), (char *)0)) {
			return 0;
		}
		break;
	case INT:
	case UINT:
		isneg = 0;
		np = str;
		if (*np == '&') {
			np++;
			isneg = atoi(np);
			if (isneg <= 0) {
				(void) fprintf(stderr,
			"***Association value `%s' invalid/undecodable\n", str);
				return 0;
			}
			if (isneg > numassoc) {
				(void) fprintf(stderr,
			"***Association for `%s' unknown (max &%d)\n",
				    str, numassoc);
				return 0;
			}
			argp->uval = assoc_cache[isneg-1].assid;
			break;
		}

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
	}

	return 1;
}


/*
 * getnetnum - given a host name, return its net number
 *	       and (optional) full name
 */
int
getnetnum(host, num, fullhost)
	char *host;
	U_LONG *num;
	char *fullhost;
{
	struct hostent *hp;

	if (decodenetnum(host, num)) {
		if (fullhost != 0) {
			(void) sprintf(fullhost,
			    "%d.%d.%d.%d", ((htonl(*num)>>24)&0xff),
			    ((htonl(*num)>>16)&0xff), ((htonl(*num)>>8)&0xff),
			    (htonl(*num)&0xff));
		}
		return 1;
	} else if ((hp = gethostbyname(host)) != 0) {
		memmove((char *)num, hp->h_addr, sizeof(U_LONG));
		if (fullhost != 0)
			(void) strcpy(fullhost, hp->h_name);
		return 1;
	} else {
		(void) fprintf(stderr, "***Can't find host %s\n", host);
		return 0;
	}
	/*NOTREACHED*/
}

/*
 * nntohost - convert network number to host name.  This routine enforces
 *	       the showhostnames setting.
 */
char *
nntohost(netnum)
	U_LONG netnum;
{
	if (!showhostnames)
		return numtoa(netnum);
	if ((ntohl(netnum) & REFCLOCK_MASK) == REFCLOCK_ADDR)
	        return refnumtoa(netnum);
	return numtohost(netnum);
}


/*
 * rtdatetolfp - decode an RT-11 date into an l_fp
 */
static int
rtdatetolfp(str, lfp)
	char *str;
	l_fp *lfp;
{
	register char *cp;
	register int i;
	struct calendar cal;
	char buf[4];
	static char *months[12] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	cal.yearday = 0;

	/*
	 * An RT-11 date looks like:
	 *
	 * d[d]-Mth-y[y] hh:mm:ss
	 */
	cp = str;
	if (!isdigit(*cp)) {
		if (*cp == '-') {
			/*
			 * Catch special case
			 */
			lfp->l_ui = lfp->l_uf = 0;
			return 1;
		}
		return 0;
	}

	cal.monthday = *cp++ - '0';	/* ascii dependent */
	if (isdigit(*cp)) {
		cal.monthday = (cal.monthday << 3) + (cal.monthday << 1);
		cal.monthday += *cp++ - '0';
	}

	if (*cp++ != '-')
		return 0;
	
	for (i = 0; i < 3; i++)
		buf[i] = *cp++;
	buf[3] = '\0';

	for (i = 0; i < 12; i++)
		if (STREQ(buf, months[i]))
			break;
	if (i == 12)
		return 0;
	cal.month = i + 1;

	if (*cp++ != '-')
		return 0;
	
	if (!isdigit(*cp))
		return 0;
	cal.year = *cp++ - '0';
	if (isdigit(*cp)) {
		cal.year = (cal.year << 3) + (cal.year << 1);
		cal.year += *cp++ - '0';
	}

	/*
	 * Catch special case.  If cal.year == 0 this is a zero timestamp.
	 */
	if (cal.year == 0) {
		lfp->l_ui = lfp->l_uf = 0;
		return 1;
	}

	if (*cp++ != ' ' || !isdigit(*cp))
		return 0;
	cal.hour = *cp++ - '0';
	if (isdigit(*cp)) {
		cal.hour = (cal.hour << 3) + (cal.hour << 1);
		cal.hour += *cp++ - '0';
	}

	if (*cp++ != ':' || !isdigit(*cp))
		return 0;
	cal.minute = *cp++ - '0';
	if (isdigit(*cp)) {
		cal.minute = (cal.minute << 3) + (cal.minute << 1);
		cal.minute += *cp++ - '0';
	}

	if (*cp++ != ':' || !isdigit(*cp))
		return 0;
	cal.second = *cp++ - '0';
	if (isdigit(*cp)) {
		cal.second = (cal.second << 3) + (cal.second << 1);
		cal.second += *cp++ - '0';
	}

	cal.year += 1900;
	lfp->l_ui = caltontp(&cal);
	lfp->l_uf = 0;
	return 1;
}


/*
 * decodets - decode a timestamp into an l_fp format number, with
 *	      consideration of fuzzball formats.
 */
int
decodets(str, lfp)
	char *str;
	l_fp *lfp;
{
	/*
	 * If it starts with a 0x, decode as hex.
	 */
	if (*str == '0' && (*(str+1) == 'x' || *(str+1) == 'X'))
		return hextolfp(str+2, lfp);

	/*
	 * If it starts with a '"', try it as an RT-11 date.
	 */
	if (*str == '"') {
		register char *cp = str+1;
		register char *bp;
		char buf[30];

		bp = buf;
		while (*cp != '"' && *cp != '\0' && bp < &buf[29])
			*bp++ = *cp++;
		*bp = '\0';
		return rtdatetolfp(buf, lfp);
	}

	/*
	 * Might still be hex.  Check out the first character.  Talk
	 * about heuristics!
	 */
	if ((*str >= 'A' && *str <= 'F') || (*str >= 'a' && *str <= 'f'))
		return hextolfp(str, lfp);

	/*
	 * Try it as a decimal.  If this fails, try as an unquoted
	 * RT-11 date.  This code should go away eventually.
	 */
	if (atolfp(str, lfp))
		return 1;
	return rtdatetolfp(str, lfp);
}


/*
 * decodetime - decode a time value.  It should be in milliseconds
 */
int
decodetime(str, lfp)
	char *str;
	l_fp *lfp;
{
	return mstolfp(str, lfp);
}


#ifdef	UNUSED
/*
 * decodereach - decode a (possibly octal or hex, damn fuzzballs) reachability
 */
static int
decodereach(str, uval)
	char *str;
	U_LONG *uval;
{
	U_LONG u;

	if (*str == '0') {
		/*
		 * Could be octal or hex
		 */
		if (*(str+1) == 'x' || *(str+1) == 'X')
			return hextoint(str+2, uval);
		return octtoint(str, uval);
	}

	if (!atouint(str, &u))
		return 0;
	
	if (u > 255)
		return octtoint(str, uval);
	*uval = u;
	return 1;
}
#endif	/* UNUSED */


/*
 * decodeint - decode an integer
 */
int
decodeint(str, val)
	char *str;
	LONG *val;
{
	if (*str == '0') {
		if (*(str+1) == 'x' || *(str+1) == 'X')
			return hextoint(str+2, (U_LONG *)val);
		return octtoint(str, (U_LONG *)val);
	}
	return atoint(str, val);
}


/*
 * decodeuint - decode an unsigned integer
 */
int
decodeuint(str, val)
	char *str;
	U_LONG *val;
{
	if (*str == '0') {
		if (*(str+1) == 'x' || *(str+1) == 'X')
			return hextoint(str+2, val);
		return octtoint(str, val);
	}
	return atouint(str, val);
}


/*
 * decodearr - decode an array of time values
 */
static int
decodearr(str, narr, lfparr)
	char *str;
	int *narr;
	l_fp *lfparr;
{
	register char *cp, *bp;
	register l_fp *lfp;
	char buf[60];

	lfp = lfparr;
	cp = str;
	*narr = 0;

	while (*narr < 8) {
		while (isspace(*cp))
			cp++;
		if (*cp == '\0')
			break;

		bp = buf;
		while (!isspace(*cp) && *cp != '\0')
			*bp++ = *cp++;
		*bp++ = '\0';

		if (!decodetime(buf, lfp))
			return 0;
		(*narr)++;
		lfp++;
	}
	return 1;
}




/*
 * getcode - return string corresponding to code
 */
static char *
getcode(code, codetab)
	int code;
	struct codestring *codetab;
{
	static char buf[30];

	while (codetab->code != -1) {
		if (codetab->code == code)
			return codetab->string;
		codetab++;
	}
	(void) sprintf(buf, "%s_%d", codetab->string, code);
	return buf;
}


/*
 * Finally, the built in command handlers
 */

/*
 * help - tell about commands, or details of a particular command
 */
static void
help(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	int i;
	int n;
	struct xcmd *xcp;
	char *cmd;
	char *cmdsort[100];
	int length[100];
	int maxlength;
	int numperline;
	static char *spaces = "                    ";	/* 20 spaces */

	if (pcmd->nargs == 0) {
		n = 0;
		for (xcp = builtins; xcp->keyword != 0; xcp++) {
			if (*(xcp->keyword) != '?')
				cmdsort[n++] = xcp->keyword;
		}
		for (xcp = opcmds; xcp->keyword != 0; xcp++)
			cmdsort[n++] = xcp->keyword;

#if defined(sgi) || defined(SYS_BSDI) || defined(__STDC__)
		qsort((void *)cmdsort, n, sizeof(char *), helpsort);
#else
		qsort((char *)cmdsort, n, sizeof(char *), helpsort);
#endif /* sgi || bsdi */

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
static int
#if defined(sgi) || defined(SYS_BSDI) || defined(__STDC__)
helpsort(t1, t2)
	const void *t1;
	const void *t2;
{
	const char **name1 = (const char **)t1;
	const char **name2 = (const char **)t2;
#else
helpsort(name1, name2)
	char **name1;
	char **name2;
{
#endif /* sgi || bsdi */
	return strcmp(*name1, *name2);
}


/*
 * printusage - print usage information for a command
 */
static void
printusage(xcp, fp)
	struct xcmd *xcp;
	FILE *fp;
{
	register int i;

	(void) fprintf(fp, "usage: %s", xcp->keyword);
	for (i = 0; i < MAXARGS && xcp->arg[i] != NO; i++) {
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
timeout(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
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
 * delay - set delay for auth requests
 */
static void
delay(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	int isneg;
	U_LONG val;

	if (pcmd->nargs == 0) {
		val = delay_time.l_ui * 1000 + delay_time.l_uf / 4294967;
		(void) fprintf(fp, "delay %d ms\n", val);
	} else {
		if (pcmd->argval[0].ival < 0) {
			isneg = 1;
			val = (U_LONG)(-pcmd->argval[0].ival);
		} else {
			isneg = 0;
			val = (U_LONG)pcmd->argval[0].ival;
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
host(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	if (pcmd->nargs == 0) {
		if (havehost)
			(void) fprintf(fp, "current host is %s\n", currenthost);
		else
			(void) fprintf(fp, "no current host\n");
	} else if (openhost(pcmd->argval[0].string)) {
		(void) fprintf(fp, "current host set to %s\n", currenthost);
		numassoc = 0;
	} else {
		if (havehost)
			(void) fprintf(fp,
			    "current host remains %s\n", currenthost);
		else
			(void) fprintf(fp, "still no current host\n");
	}
}


/*
 * poll - do one (or more) polls of the host via NTP
 */
/*ARGSUSED*/
static void
ntp_poll(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	(void) fprintf(fp, "poll not implemented yet\n");
}


/*
 * keyid - get a keyid to use for authenticating requests
 */
static void
keyid(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	if (pcmd->nargs == 0) {
		if (info_auth_keyid == -1)
			(void) fprintf(fp, "no keyid defined\n");
		else
			(void) fprintf(fp, "keyid is %u\n", info_auth_keyid);
	} else {
		info_auth_keyid = pcmd->argval[0].uval;
	}
}

/*
 * keytype - get type of key to use for authenticating requests
 */
static void
keytype(pcmd, fp)
    struct parse *pcmd;
    FILE *fp;
{
    if (pcmd->nargs == 0)
	fprintf(fp, "keytype is %s",
		(info_auth_keytype == KEY_TYPE_MD5) ? "md5" : "des");
    else
	switch (*(pcmd->argval[0].string)) {
	case 'm':
	case 'M':
	    info_auth_keytype = KEY_TYPE_MD5;
	    break;

	case 'd':
	case 'D':
	    info_auth_keytype = KEY_TYPE_DES;
	    break;

	default:
	    fprintf(fp, "keytype must be 'md5' or 'des'\n");
	}
}



/*
 * passwd - get an authentication key
 */
/*ARGSUSED*/
static void
passwd(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	char *pass;

	if (info_auth_keyid == -1) {
		info_auth_keyid = getkeyid("Keyid: ");
		if (info_auth_keyid == -1) {
			(void)fprintf(fp, "Keyid must be defined\n");
			return;
		}
	}
	pass = getpass("Password: ");
	if (*pass == '\0')
		(void) fprintf(fp, "Password unchanged\n");
	else
		authusekey(info_auth_keyid, info_auth_keytype, pass);
}


/*
 * hostnames - set the showhostnames flag
 */
static void
hostnames(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
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
setdebug(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
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
quit(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	if (havehost)
		(void) close(sockfd);	/* cleanliness next to godliness */
	exit(0);
}


/*
 * version - print the current version number
 */
/*ARGSUSED*/
static void
version(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	extern char *Version;

	(void) fprintf(fp, "%s\n", Version);
}


/*
 * raw - set raw mode output
 */
/*ARGSUSED*/
static void
raw(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	rawmode = 1;
	(void) fprintf(fp, "Output set to raw\n");
}


/*
 * cooked - set cooked mode output
 */
/*ARGSUSED*/
static void
cooked(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	rawmode = 0;
	(void) fprintf(fp, "Output set to cooked\n");
	return;
}


/*
 * authenticate - always authenticate requests to this host
 */
static void
authenticate(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	if (pcmd->nargs == 0) {
		if (always_auth) {
			(void) fprintf(fp,
			    "authenticated requests being sent\n");
		} else
			(void) fprintf(fp,
			    "unauthenticated requests being sent\n");
	} else {
		if (STREQ(pcmd->argval[0].string, "yes")) {
			always_auth = 1;
		} else if (STREQ(pcmd->argval[0].string, "no")) {
			always_auth = 0;
		} else
			(void)fprintf(stderr, "What?\n");
	}
}


/*
 * ntpversion - choose the NTP version to use
 */
static void
ntpversion(pcmd, fp)
	struct parse *pcmd;
	FILE *fp;
{
	if (pcmd->nargs == 0) {
		(void) fprintf(fp,
		    "NTP version being claimed is %d\n", pktversion);
	} else {
		if (pcmd->argval[0].uval <= NTP_OLDVERSION
		    || pcmd->argval[0].uval > NTP_VERSION) {
			(void) fprintf(stderr, "versions %d to %d, please\n",
			    NTP_OLDVERSION+1, NTP_VERSION);
		} else {
			pktversion = pcmd->argval[0].uval;
		}
	}
}


/*
 * warning - print a warning message
 */
static void
warning(fmt, st1, st2)
	char *fmt;
	char *st1;
	char *st2;
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
error(fmt, st1, st2)
	char *fmt;
	char *st1;
	char *st2;
{
	warning(fmt, st1, st2);
	exit(1);
}

/*
 * getkeyid - prompt the user for a keyid to use
 */
static U_LONG
getkeyid(prompt)
char *prompt;
{
	register char *p;
	register c;
	FILE *fi;
	char pbuf[20];

	if ((fi = fdopen(open("/dev/tty", 2), "r")) == NULL)
		fi = stdin;
	else
		setbuf(fi, (char *)NULL);
	fprintf(stderr, "%s", prompt); fflush(stderr);
	for (p=pbuf; (c = getc(fi))!='\n' && c!=EOF;) {
		if (p < &pbuf[18])
			*p++ = c;
	}
	*p = '\0';
	if (fi != stdin)
		fclose(fi);
	if (strcmp(pbuf, "0") == 0)
	    return 0;

	return (U_LONG) atoi(pbuf);
}


/*
 * atoascii - printable-ize possibly ascii data using the character
 *	      transformations cat -v uses.
 */
static void
atoascii(length, data, outdata)
	int length;
	char *data;
	char *outdata;
{
	register u_char *cp;
	register u_char *ocp;
	register u_char c;

	if (!data)
	  {
	    *outdata = '\0';
	    return;
	  }

	ocp = (u_char *)outdata;
	for (cp = (u_char *)data; cp < (u_char *)data + length; cp++) {
		c = *cp;
		if (c == '\0')
			break;
		if (c == '\0')
			break;
		if (c > 0177) {
			*ocp++ = 'M';
			*ocp++ = '-';
			c &= 0177;
		}

		if (c < ' ') {
			*ocp++ = '^';
			*ocp++ = c + '@';
		} else if (c == 0177) {
			*ocp++ = '^';
			*ocp++ = '?';
		} else {
			*ocp++ = c;
		}
		if (ocp >= ((u_char *)outdata + length - 4))
			break;
	}
	*ocp++ = '\0';
}



/*
 * makeascii - print possibly ascii data using the character
 *	       transformations that cat -v uses.
 */
static void
makeascii(length, data, fp)
	int length;
	char *data;
	FILE *fp;
{
	register u_char *cp;
	register int c;

	for (cp = (u_char *)data; cp < (u_char *)data + length; cp++) {
		c = (int)*cp;
		if (c > 0177) {
			putc('M', fp);
			putc('-', fp);
			c &= 0177;
		}

		if (c < ' ') {
			putc('^', fp);
			putc(c+'@', fp);
		} else if (c == 0177) {
			putc('^', fp);
			putc('?', fp);
		} else {
			putc(c, fp);
		}
	}
}


/*
 * asciize - same thing as makeascii except add a newline
 */
void
asciize(length, data, fp)
	int length;
	char *data;
	FILE *fp;
{
	makeascii(length, data, fp);
	putc('\n', fp);
}


/*
 * Some circular buffer space
 */
#define	CBLEN	80
#define	NUMCB	6

char circ_buf[NUMCB][CBLEN];
int nextcb = 0;


/*
 * getevents - return a descriptive string for the event count
 */
static char *
getevents(cnt)
	int cnt;
{
	static char buf[20];

	if (cnt == 0)
		return "no events";
	(void) sprintf(buf, "%d event%s", cnt, (cnt==1) ? "" : "s");
	return buf;
}


/*
 * statustoa - return a descriptive string for a peer status
 */
static char *
statustoa(type, st)
	int type;
	int st;
{
	char *cb;
	u_char pst;

	cb = &circ_buf[nextcb][0];
	if (++nextcb >= NUMCB)
		nextcb = 0;

	switch (type) {
	case TYPE_SYS:
		(void)strcpy(cb, getcode(CTL_SYS_LI(st), leap_codes));
		(void)strcat(cb, ", ");
		(void)strcat(cb, getcode(CTL_SYS_SOURCE(st), sync_codes));
		(void)strcat(cb, ", ");
		(void)strcat(cb, getevents(CTL_SYS_NEVNT(st)));
		(void)strcat(cb, ", ");
		(void)strcat(cb, getcode(CTL_SYS_EVENT(st), sys_codes));
		break;
	
	case TYPE_PEER:
		/*
		 * Handcraft the bits
		 */
		pst = CTL_PEER_STATVAL(st);
		if (!(pst & CTL_PST_REACH)) {
			(void)strcpy(cb, "unreach");
		} else {
			(void)strcpy(cb, "reach");
#if 0
			if (!(pst & CTL_PST_DISP)) {
				(void)strcat(cb, ", hi_disp");
			} else {
				if (pst & CTL_PST_SANE) {
					if ((pst & 0x3) == CTL_PST_SEL_REJECT)
						(void)strcat(cb, ", sane");
				} else {
					(void)strcat(cb, ", insane");
				}
			}
#endif
		}
		if (pst & CTL_PST_CONFIG)
			(void)strcat(cb, ", conf");
		if (pst & CTL_PST_AUTHENABLE) {
			if (!(pst & CTL_PST_REACH) || (pst & CTL_PST_AUTHENTIC))
				(void)strcat(cb, ", auth");
			else
				(void)strcat(cb, ", unauth");
		}

		/*
		 * Now the codes
		 */
		if ((pst & 0x7) != CTL_PST_SEL_REJECT) {
			(void)strcat(cb, ", ");
			(void)strcat(cb, getcode(pst & 0x7, select_codes));
		}
		(void)strcat(cb, ", ");
		(void)strcat(cb, getevents(CTL_PEER_NEVNT(st)));
		if (CTL_PEER_EVENT(st) != EVNT_UNSPEC) {
			(void)strcat(cb, ", ");
			(void)strcat(cb, getcode(CTL_PEER_EVENT(st),
			    peer_codes));
		}
		break;
	
	case TYPE_CLOCK:
		(void)strcpy(cb, getcode(((st)>>8) & 0xff, clock_codes));
		(void)strcat(cb, ", last_");
		(void)strcat(cb, getcode((st) & 0xff, clock_codes));
		break;
	}
	return cb;
}


/*
 * nextvar - find the next variable in the buffer
 */
int
nextvar(datalen, datap, vname, vvalue)
	int *datalen;
	char **datap;
	char **vname;
	char **vvalue;
{
	register char *cp;
	register char *np;
	register char *cpend;
	int quoted = 0;
	static char name[MAXVARLEN];
	static char value[MAXVALLEN];

	cp = *datap;
	cpend = cp + *datalen;

	/*
	 * Space past commas and white space
	 */
	while (cp < cpend && (*cp == ',' || isspace(*cp)))
		cp++;
	if (cp == cpend)
		return 0;
	
	/*
	 * Copy name until we hit a ',', an '=', a '\r' or a '\n'.  Backspace
	 * over any white space and terminate it.
	 */
	np = name;
	while (cp < cpend && *cp != ',' && *cp != '='
	    && *cp != '\r' && *cp != '\n')
		*np++ = *cp++;
	while (isspace(*(np-1)))
		np--;
	*np = '\0';
	*vname = name;

	/*
	 * Check if we hit the end of the buffer or a ','.  If so we are done.
	 */
	if (cp == cpend || *cp == ',' || *cp == '\r' || *cp == '\n') {
		if (cp != cpend)
			cp++;
		*datap = cp;
		*datalen = cpend - cp;
		*vvalue = (char *)0;
		return 1;
	}

	/*
	 * So far, so good.  Copy out the value
	 */
	cp++;	/* past '=' */
	while (cp < cpend && (isspace(*cp) && *cp != '\r' && *cp != '\n'))
		cp++;
	np = value;
	while (cp < cpend && ((*cp != ',') || quoted))
	  {
	    quoted ^= ((*np++ = *cp++) == '"');
	  }

	while (np > value && isspace(*(np-1)))
		np--;
	*np = '\0';

	/*
	 * Return this.  All done.
	 */
	if (cp != cpend)
		cp++;
	*datap = cp;
	*datalen = cpend - cp;
	*vvalue = value;
	return 1;
}


/*
 * findvar - see if this variable is known to us
 */
int
findvar(varname, varlist)
	char *varname;
	struct ctl_var *varlist;
{
	register char *np;
	register struct ctl_var *vl;

	vl = varlist;
	np = varname;
	while (vl->fmt != EOV) {
		if (vl->fmt != PADDING && STREQ(np, vl->text))
			return vl->code;
		vl++;
	}
	return 0;
}



/*
 * printvars - print variables returned in response packet
 */
void
printvars(length, data, status, sttype, fp)
	int length;
	char *data;
	int status;
	int sttype;
	FILE *fp;
{
	if (rawmode)
		rawprint(sttype, length, data, status, fp);
	else
		cookedprint(sttype, length, data, status, fp);
}


/*
 * rawprint - do a printout of the data in raw mode
 */
static void
rawprint(datatype, length, data, status, fp)
	int datatype;
	int length;
	char *data;
	int status;
	FILE *fp;
{
	register char *cp;
	register char *cpend;

	/*
	 * Essentially print the data as is.  We reformat unprintables, though.
	 */
	cp = data;
	cpend = data + length;

	(void) fprintf(fp, "status=%04x %s\n", status,
	    statustoa(datatype, status));

	while (cp < cpend) {
		if (*cp == '\r') {
			/*
			 * If this is a \r and the next character is a
			 * \n, supress this, else pretty print it.  Otherwise
			 * just output the character.
			 */
			if (cp == (cpend-1) || *(cp+1) != '\n')
				makeascii(1, cp, fp);
		} else if (isspace(*cp) || isprint(*cp)) {
			putc(*cp, fp);
		} else {
			makeascii(1, cp, fp);
		}
		cp++;
	}
}


/*
 * Global data used by the cooked output routines
 */
int out_chars;		/* number of characters output */
int out_linecount;	/* number of characters output on this line */


/*
 * startoutput - get ready to do cooked output
 */
static void
startoutput()
{
	out_chars = 0;
	out_linecount = 0;
}


/*
 * output - output a variable=value combination
 */
static void
output(fp, name, value)
	FILE *fp;
	char *name;
	char *value;
{
	int lenname;
	int lenvalue;

	lenname = strlen(name);
	lenvalue = strlen(value);

	if (out_chars != 0) {
		putc(',', fp);
		out_chars++;
		out_linecount++;
		if ((out_linecount + lenname + lenvalue + 3) > MAXOUTLINE) {
			putc('\n', fp);
			out_chars++;
			out_linecount = 0;
		} else {
			putc(' ', fp);
			out_chars++;
			out_linecount++;
		}
	}

	fputs(name, fp);
	putc('=', fp);
	fputs(value, fp);
	out_chars += lenname + 1 + lenvalue;
	out_linecount += lenname + 1 + lenvalue;
}


/*
 * endoutput - terminate a block of cooked output
 */
static void
endoutput(fp)
	FILE *fp;
{
	if (out_chars != 0)
		putc('\n', fp);
}


/*
 * outputarr - output an array of values
 */
static void
outputarr(fp, name, narr, lfp)
	FILE *fp;
	char *name;
	int narr;
	l_fp *lfp;
{
	register char *bp;
	register char *cp;
	register int i;
	register int len;
	char buf[256];

	bp = buf;
	/*
	 * Hack to align delay and offset values
	 */
	if ((int)strlen(name) < 10)
		*bp++ = ' ';
	
	for (i = narr; i > 0; i--) {
		if (i != narr)
			*bp++ = ' ';
		cp = lfptoms(lfp, 2);
		len = strlen(cp);
		while (len < 7) {
			*bp++ = ' ';
			len++;
		}
		while (*cp != '\0')
			*bp++ = *cp++;
		lfp++;
	}
	*bp = '\0';
	output(fp, name, buf);
}

static char *
tstflags(val)
  	U_LONG val;
{
  	register char *cb, *s;
  	register int i;
  	register char *sep;

  	sep = "";
  	i = 0;
  	s = cb = &circ_buf[nextcb][0];
  	if (++nextcb >= NUMCB)
	  	nextcb = 0;

  	sprintf(cb, "0x%x", val);
  	cb += strlen(cb);
  	if (val <= ((1<<8)-1)) {
    		if (!val) {
      			strcat(cb, "<OK>");
      			cb += strlen(cb);
    		} else {
      			*cb++ = '<';
      			while (val) {
				if (val & 0x1) {
	  				sprintf(cb, "%s%s", sep, tstflagnames[i]);
	  				sep = ";";
	  				cb += strlen(cb);
				}
				i++;
				val >>= 1;
      			}
      			*cb++ = '>';
    		}
  	} else {
    		*cb++ = '?';
  	}
  	*cb = '\0';
  	return s;
}

/*
 * cookedprint - output variables in cooked mode
 */
static void
cookedprint(datatype, length, data, status, fp)
	int datatype;
	int length;
	char *data;
	int status;
	FILE *fp;
{
	register int varid;
	char *name;
	char *value;
	int output_raw;
	int fmt;
	struct ctl_var *varlist;
	l_fp lfp;
	LONG ival;
	U_LONG uval;
	l_fp lfparr[8];
	int narr;

	switch (datatype) {
	case TYPE_PEER:
		varlist = peer_var;
		break;
	case TYPE_SYS:
		varlist = sys_var;
		break;
	case TYPE_CLOCK:
		varlist = clock_var;
		break;
	default:
		(void) fprintf(stderr, "Unknown datatype(0x%x) in cookedprint\n", datatype);
		return;
		break;
	}

	(void) fprintf(fp, "status=%04x %s\n", status,
	    statustoa(datatype, status));

	startoutput();
	while (nextvar(&length, &data, &name, &value)) {
		varid = findvar(name, varlist);
		if (varid == 0) {
			output_raw = '*';
		} else {
			output_raw = 0;
			switch((fmt = varlist[varid].fmt)) {
			case TS:
				if (!decodets(value, &lfp))
					output_raw = '?';
				else
					output(fp, name, prettydate(&lfp));
				break;
			case FL:
			case FU:
			case FS:
				if (!decodetime(value, &lfp))
					output_raw = '?';
				else {
					switch (fmt) {
					case FL:
						output(fp, name,
						    lfptoms(&lfp, 3));
						break;
					case FU:
						output(fp, name,
						    ulfptoms(&lfp, 2));
						break;
					case FS:
						output(fp, name,
						    lfptoms(&lfp, 2));
						break;
					}
				}
				break;
			
			case UI:
				if (!decodeuint(value, &uval))
					output_raw = '?';
				else
					output(fp, name, uinttoa(uval));
				break;
			
			case IN:
				if (!decodeint(value, &ival))
					output_raw = '?';
				else
					output(fp, name, inttoa(ival));
				break;

			case HA:
			case NA:
				if (!decodenetnum(value, &uval))
					output_raw = '?';
				else if (fmt == HA)
					output(fp, name, nntohost(uval));
				else
					output(fp, name, numtoa(uval));
				break;
			
			case ST:
				output_raw = '*';
				break;
			
			case RF:
				if (decodenetnum(value, &uval))
					output(fp, name, nntohost(uval));
				else if ((int)strlen(value) <= 4)
					output(fp, name, value);
				else
					output_raw = '?';
				break;

			case LP:
				if (!decodeuint(value, &uval) || uval > 3)
					output_raw = '?';
				else {
					char b[3];
					b[0] = b[1] = '0';
					if (uval & 0x2)
						b[0] = '1';
					if (uval & 0x1)
						b[1] = '1';
					b[2] = '\0';
					output(fp, name, b);
				}
				break;

			case OC:
				if (!decodeuint(value, &uval))
					output_raw = '?';
				else {
					char b[10];

					(void) sprintf(b, "%03o", uval);
					output(fp, name, b);
				}
				break;
			
			case MD:
				if (!decodeuint(value, &uval))
					output_raw = '?';
				else
					output(fp, name, uinttoa(uval));
				break;
			
			case AR:
				if (!decodearr(value, &narr, lfparr))
					output_raw = '?';
				else
					outputarr(fp, name, narr, lfparr);
				break;

			case TST:
				if (!decodeuint(value, &uval))
					output_raw = '?';
				else
					output(fp, name, tstflags(uval));
				break;
			
			default:
				(void) fprintf(stderr,
			"Internal error in cookedprint, %s=%s, fmt %d\n",
				    name, value, fmt);
				break;
			}

		}
		if (output_raw != 0) {
			char bn[401];
			char bv[401];
			int len;

			atoascii(400, name, bn);
			atoascii(400, value, bv);
			if (output_raw != '*') {
				len = strlen(bv);
				bv[len] = output_raw;
				bv[len+1] = '\0';
			}
			output(fp, bn, bv);
		}
	}
	endoutput(fp);
}


/*
 * sortassoc - sort associations in the cache into ascending order
 */
void
sortassoc()
{
	if (numassoc > 1)
#if defined(sgi) || defined(SYS_BSDI) || defined(__STDC__)
		qsort((void *)assoc_cache, numassoc,
		    sizeof(struct association), assoccmp);
#else
		qsort((char *)assoc_cache, numassoc,
		    sizeof(struct association), assoccmp);
#endif /* sgi || bsdi */
}


/*
 * assoccmp - compare two associations
 */
static int
#if defined(sgi) || defined(SYS_BSDI) || defined(__STDC__)
assoccmp(t1, t2)
	const void *t1;
	const void *t2;
{
	const struct association *ass1 = (const struct association *)t1;
	const struct association *ass2 = (const struct association *)t2;
#else
assoccmp(ass1, ass2)
	struct association *ass1;
	struct association *ass2;
{
#endif /* sgi || bsdi */
	if (ass1->assid < ass2->assid)
		return -1;
	if (ass1->assid > ass2->assid)
		return 1;
	return 0;
}
