/*
 * options.c - handles option processing for PPP.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <termios.h>
#include <syslog.h>
#include <string.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef PPP_FILTER
#include <pcap.h>
#include <pcap-int.h>	/* XXX: To get struct pcap */
#endif

#include "pppd.h"
#include "pathnames.h"
#include "patchlevel.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "upap.h"
#include "chap.h"
#include "ccp.h"
#ifdef CBCP_SUPPORT
#include "cbcp.h"
#endif

#ifdef IPX_CHANGE
#include "ipxcp.h"
#endif /* IPX_CHANGE */

#include <net/ppp_comp.h>

#define FALSE	0
#define TRUE	1

#if defined(ultrix) || defined(NeXT)
char *strdup __P((char *));
#endif

#ifndef GIDSET_TYPE
#define GIDSET_TYPE	gid_t
#endif

/*
 * Option variables and default values.
 */
#ifdef PPP_FILTER
int	dflag = 0;		/* Tell libpcap we want debugging */
#endif
int	debug = 0;		/* Debug flag */
int	kdebugflag = 0;		/* Tell kernel to print debug messages */
int	default_device = 1;	/* Using /dev/tty or equivalent */
char	devnam[MAXPATHLEN] = _PATH_TTY;	/* Device name */
int	crtscts = 0;		/* Use hardware flow control */
int	modem = 1;		/* Use modem control lines */
int	inspeed = 0;		/* Input/Output speed requested */
u_int32_t netmask = 0;		/* IP netmask to set on interface */
int	lockflag = 0;		/* Create lock file to lock the serial dev */
int	nodetach = 0;		/* Don't detach from controlling tty */
char	*connector = NULL;	/* Script to establish physical link */
char	*disconnector = NULL;	/* Script to disestablish physical link */
char	*welcomer = NULL;	/* Script to run after phys link estab. */
int	max_con_attempts = 0;	/* Maximum connect tries in non-demand mode */
int	maxconnect = 0;		/* Maximum connect time */
char	user[MAXNAMELEN];	/* Username for PAP */
char	passwd[MAXSECRETLEN];	/* Password for PAP */
int	auth_required = 0;	/* Peer is required to authenticate */
int	defaultroute = 0;	/* assign default route through interface */
int	proxyarp = 0;		/* Set up proxy ARP entry for peer */
int	persist = 0;		/* Reopen link after it goes down */
int	uselogin = 0;		/* Use /etc/passwd for checking PAP */
int	lcp_echo_interval = 0; 	/* Interval between LCP echo-requests */
int	lcp_echo_fails = 0;	/* Tolerance to unanswered echo-requests */
char	our_name[MAXNAMELEN];	/* Our name for authentication purposes */
char	remote_name[MAXNAMELEN]; /* Peer's name for authentication */
int	explicit_remote = 0;	/* User specified explicit remote name */
int	usehostname = 0;	/* Use hostname for our_name */
int	disable_defaultip = 0;	/* Don't use hostname for default IP adrs */
int	demand = 0;		/* do dial-on-demand */
char	*ipparam = NULL;	/* Extra parameter for ip up/down scripts */
int	cryptpap;		/* Passwords in pap-secrets are encrypted */
int	idle_time_limit = 0;	/* Disconnect if idle for this many seconds */
int	holdoff = 30;		/* # seconds to pause before reconnecting */
int	refuse_pap = 0;		/* Set to say we won't do PAP */
int	refuse_chap = 0;	/* Set to say we won't do CHAP */

#ifdef MSLANMAN
int	ms_lanman = 0;    	/* Nonzero if use LanMan password instead of NT */
			  	/* Has meaning only with MS-CHAP challenges */
#endif

struct option_info auth_req_info;
struct option_info connector_info;
struct option_info disconnector_info;
struct option_info welcomer_info;
struct option_info devnam_info;
#ifdef PPP_FILTER
struct	bpf_program pass_filter;/* Filter program for packets to pass */
struct	bpf_program active_filter; /* Filter program for link-active pkts */
pcap_t  pc;			/* Fake struct pcap so we can compile expr */
#endif

/*
 * Prototypes
 */
static int setdevname __P((char *, int));
static int setspeed __P((char *));
static int setdebug __P((char **));
static int setkdebug __P((char **));
static int setpassive __P((char **));
static int setsilent __P((char **));
static int noopt __P((char **));
static int setnovj __P((char **));
static int setnovjccomp __P((char **));
static int setvjslots __P((char **));
static int reqpap __P((char **));
static int nopap __P((char **));
#ifdef OLD_OPTIONS
static int setupapfile __P((char **));
#endif
static int nochap __P((char **));
static int reqchap __P((char **));
static int noaccomp __P((char **));
static int noasyncmap __P((char **));
static int noip __P((char **));
static int nomagicnumber __P((char **));
static int setasyncmap __P((char **));
static int setescape __P((char **));
static int setmru __P((char **));
static int setmtu __P((char **));
#ifdef CBCP_SUPPORT
static int setcbcp __P((char **));
#endif
static int nomru __P((char **));
static int nopcomp __P((char **));
static int setconnector __P((char **));
static int setdisconnector __P((char **));
static int setwelcomer __P((char **));
static int setmaxcon __P((char **));
static int setmaxconnect __P((char **));
static int setdomain __P((char **));
static int setnetmask __P((char **));
static int setcrtscts __P((char **));
static int setnocrtscts __P((char **));
static int setxonxoff __P((char **));
static int setnodetach __P((char **));
static int setupdetach __P((char **));
static int setmodem __P((char **));
static int setlocal __P((char **));
static int setlock __P((char **));
static int setname __P((char **));
static int setuser __P((char **));
static int setremote __P((char **));
static int setauth __P((char **));
static int setnoauth __P((char **));
static int readfile __P((char **));
static int callfile __P((char **));
static int setdefaultroute __P((char **));
static int setnodefaultroute __P((char **));
static int setproxyarp __P((char **));
static int setnoproxyarp __P((char **));
static int setpersist __P((char **));
static int setnopersist __P((char **));
static int setdologin __P((char **));
static int setusehostname __P((char **));
static int setnoipdflt __P((char **));
static int setlcptimeout __P((char **));
static int setlcpterm __P((char **));
static int setlcpconf __P((char **));
static int setlcpfails __P((char **));
static int setipcptimeout __P((char **));
static int setipcpterm __P((char **));
static int setipcpconf __P((char **));
static int setipcpfails __P((char **));
static int setpaptimeout __P((char **));
static int setpapreqs __P((char **));
static int setpapreqtime __P((char **));
static int setchaptimeout __P((char **));
static int setchapchal __P((char **));
static int setchapintv __P((char **));
static int setipcpaccl __P((char **));
static int setipcpaccr __P((char **));
static int setlcpechointv __P((char **));
static int setlcpechofails __P((char **));
static int noccp __P((char **));
static int setbsdcomp __P((char **));
static int setnobsdcomp __P((char **));
static int setdeflate __P((char **));
static int setnodeflate __P((char **));
static int setnodeflatedraft __P((char **));
static int setdemand __P((char **));
static int setpred1comp __P((char **));
static int setnopred1comp __P((char **));
static int setipparam __P((char **));
static int setpapcrypt __P((char **));
static int setidle __P((char **));
static int setholdoff __P((char **));
static int setdnsaddr __P((char **));
static int resetipxproto __P((char **));
static int setwinsaddr __P((char **));
static int showversion __P((char **));
static int showhelp __P((char **));

#ifdef PPP_FILTER
static int setpdebug __P((char **));
static int setpassfilter __P((char **));
static int setactivefilter __P((char **));
#endif

#ifdef IPX_CHANGE
static int setipxproto __P((char **));
static int setipxanet __P((char **));
static int setipxalcl __P((char **));
static int setipxarmt __P((char **));
static int setipxnetwork __P((char **));
static int setipxnode __P((char **));
static int setipxrouter __P((char **));
static int setipxname __P((char **));
static int setipxcptimeout __P((char **));
static int setipxcpterm __P((char **));
static int setipxcpconf __P((char **));
static int setipxcpfails __P((char **));
#endif /* IPX_CHANGE */

#ifdef MSLANMAN
static int setmslanman __P((char **));
#endif

static int number_option __P((char *, u_int32_t *, int));
static int int_option __P((char *, int *));
static int readable __P((int fd));

/*
 * Valid arguments.
 */
static struct cmd {
    char *cmd_name;
    int num_args;
    int (*cmd_func) __P((char **));
} cmds[] = {
    {"-all", 0, noopt},		/* Don't request/allow any options (useless) */
    {"noaccomp", 0, noaccomp},	/* Disable Address/Control compression */
    {"-ac", 0, noaccomp},	/* Disable Address/Control compress */
    {"default-asyncmap", 0, noasyncmap}, /* Disable asyncmap negoatiation */
    {"-am", 0, noasyncmap},	/* Disable asyncmap negotiation */
    {"-as", 1, setasyncmap},	/* set the desired async map */
    {"-d", 0, setdebug},	/* Increase debugging level */
    {"nodetach", 0, setnodetach}, /* Don't detach from controlling tty */
    {"-detach", 0, setnodetach}, /* don't fork */
    {"updetach", 0, setupdetach}, /* Detach once an NP has come up */
    {"noip", 0, noip},		/* Disable IP and IPCP */
    {"-ip", 0, noip},		/* Disable IP and IPCP */
    {"nomagic", 0, nomagicnumber}, /* Disable magic number negotiation */
    {"-mn", 0, nomagicnumber},	/* Disable magic number negotiation */
    {"default-mru", 0, nomru},	/* Disable MRU negotiation */
    {"-mru", 0, nomru},		/* Disable mru negotiation */
    {"-p", 0, setpassive},	/* Set passive mode */
    {"nopcomp", 0, nopcomp},	/* Disable protocol field compression */
    {"-pc", 0, nopcomp},	/* Disable protocol field compress */
#if OLD_OPTIONS
    {"+ua", 1, setupapfile},	/* Get PAP user and password from file */
#endif
    {"require-pap", 0, reqpap},	/* Require PAP authentication from peer */
    {"+pap", 0, reqpap},	/* Require PAP auth from peer */
    {"refuse-pap", 0, nopap},	/* Don't agree to auth to peer with PAP */
    {"-pap", 0, nopap},		/* Don't allow UPAP authentication with peer */
    {"require-chap", 0, reqchap}, /* Require CHAP authentication from peer */
    {"+chap", 0, reqchap},	/* Require CHAP authentication from peer */
    {"refuse-chap", 0, nochap},	/* Don't agree to auth to peer with CHAP */
    {"-chap", 0, nochap},	/* Don't allow CHAP authentication with peer */
    {"novj", 0, setnovj},	/* Disable VJ compression */
    {"-vj", 0, setnovj},	/* disable VJ compression */
    {"novjccomp", 0, setnovjccomp}, /* disable VJ connection-ID compression */
    {"-vjccomp", 0, setnovjccomp}, /* disable VJ connection-ID compression */
    {"vj-max-slots", 1, setvjslots}, /* Set maximum VJ header slots */
    {"asyncmap", 1, setasyncmap}, /* set the desired async map */
    {"escape", 1, setescape},	/* set chars to escape on transmission */
    {"connect", 1, setconnector}, /* A program to set up a connection */
    {"disconnect", 1, setdisconnector},	/* program to disconnect serial dev. */
    {"welcome", 1, setwelcomer},/* Script to welcome client */
    {"connect-max-attempts", 1, setmaxcon},  /* maximum # connect attempts */
    {"maxconnect", 1, setmaxconnect},  /* specify a maximum connect time */
    {"crtscts", 0, setcrtscts},	/* set h/w flow control */
    {"nocrtscts", 0, setnocrtscts}, /* clear h/w flow control */
    {"-crtscts", 0, setnocrtscts}, /* clear h/w flow control */
    {"xonxoff", 0, setxonxoff},	/* set s/w flow control */
    {"debug", 0, setdebug},	/* Increase debugging level */
    {"kdebug", 1, setkdebug},	/* Enable kernel-level debugging */
    {"domain", 1, setdomain},	/* Add given domain name to hostname*/
    {"mru", 1, setmru},		/* Set MRU value for negotiation */
    {"mtu", 1, setmtu},		/* Set our MTU */
#ifdef CBCP_SUPPORT
    {"callback", 1, setcbcp},	/* Ask for callback */
#endif
    {"netmask", 1, setnetmask},	/* set netmask */
    {"passive", 0, setpassive},	/* Set passive mode */
    {"silent", 0, setsilent},	/* Set silent mode */
    {"modem", 0, setmodem},	/* Use modem control lines */
    {"local", 0, setlocal},	/* Don't use modem control lines */
    {"lock", 0, setlock},	/* Lock serial device (with lock file) */
    {"name", 1, setname},	/* Set local name for authentication */
    {"user", 1, setuser},	/* Set name for auth with peer */
    {"usehostname", 0, setusehostname},	/* Must use hostname for auth. */
    {"remotename", 1, setremote}, /* Set remote name for authentication */
    {"auth", 0, setauth},	/* Require authentication from peer */
    {"noauth", 0, setnoauth},	/* Don't require peer to authenticate */
    {"file", 1, readfile},	/* Take options from a file */
    {"call", 1, callfile},	/* Take options from a privileged file */
    {"defaultroute", 0, setdefaultroute}, /* Add default route */
    {"nodefaultroute", 0, setnodefaultroute}, /* disable defaultroute option */
    {"-defaultroute", 0, setnodefaultroute}, /* disable defaultroute option */
    {"proxyarp", 0, setproxyarp}, /* Add proxy ARP entry */
    {"noproxyarp", 0, setnoproxyarp}, /* disable proxyarp option */
    {"-proxyarp", 0, setnoproxyarp}, /* disable proxyarp option */
    {"persist", 0, setpersist},	/* Keep on reopening connection after close */
    {"nopersist", 0, setnopersist},  /* Turn off persist option */
    {"demand", 0, setdemand},	/* Dial on demand */
    {"login", 0, setdologin},	/* Use system password database for UPAP */
    {"noipdefault", 0, setnoipdflt}, /* Don't use name for default IP adrs */
    {"lcp-echo-failure", 1, setlcpechofails}, /* consecutive echo failures */
    {"lcp-echo-interval", 1, setlcpechointv}, /* time for lcp echo events */
    {"lcp-restart", 1, setlcptimeout}, /* Set timeout for LCP */
    {"lcp-max-terminate", 1, setlcpterm}, /* Set max #xmits for term-reqs */
    {"lcp-max-configure", 1, setlcpconf}, /* Set max #xmits for conf-reqs */
    {"lcp-max-failure", 1, setlcpfails}, /* Set max #conf-naks for LCP */
    {"ipcp-restart", 1, setipcptimeout}, /* Set timeout for IPCP */
    {"ipcp-max-terminate", 1, setipcpterm}, /* Set max #xmits for term-reqs */
    {"ipcp-max-configure", 1, setipcpconf}, /* Set max #xmits for conf-reqs */
    {"ipcp-max-failure", 1, setipcpfails}, /* Set max #conf-naks for IPCP */
    {"pap-restart", 1, setpaptimeout},	/* Set retransmit timeout for PAP */
    {"pap-max-authreq", 1, setpapreqs}, /* Set max #xmits for auth-reqs */
    {"pap-timeout", 1, setpapreqtime},	/* Set time limit for peer PAP auth. */
    {"chap-restart", 1, setchaptimeout}, /* Set timeout for CHAP */
    {"chap-max-challenge", 1, setchapchal}, /* Set max #xmits for challenge */
    {"chap-interval", 1, setchapintv}, /* Set interval for rechallenge */
    {"ipcp-accept-local", 0, setipcpaccl}, /* Accept peer's address for us */
    {"ipcp-accept-remote", 0, setipcpaccr}, /* Accept peer's address for it */
    {"noccp", 0, noccp},		/* Disable CCP negotiation */
    {"-ccp", 0, noccp},			/* Disable CCP negotiation */
    {"bsdcomp", 1, setbsdcomp},		/* request BSD-Compress */
    {"nobsdcomp", 0, setnobsdcomp},	/* don't allow BSD-Compress */
    {"-bsdcomp", 0, setnobsdcomp},	/* don't allow BSD-Compress */
    {"deflate", 1, setdeflate},		/* request Deflate compression */
    {"nodeflate", 0, setnodeflate},	/* don't allow Deflate compression */
    {"-deflate", 0, setnodeflate},	/* don't allow Deflate compression */
    {"nodeflatedraft", 0, setnodeflatedraft}, /* don't use draft deflate # */
    {"predictor1", 0, setpred1comp},	/* request Predictor-1 */
    {"nopredictor1", 0, setnopred1comp},/* don't allow Predictor-1 */
    {"-predictor1", 0, setnopred1comp},	/* don't allow Predictor-1 */
    {"ipparam", 1, setipparam},		/* set ip script parameter */
    {"papcrypt", 0, setpapcrypt},	/* PAP passwords encrypted */
    {"idle", 1, setidle},		/* idle time limit (seconds) */
    {"holdoff", 1, setholdoff},		/* set holdoff time (seconds) */
/* backwards compat hack */
    {"dns1", 1, setdnsaddr},		/* DNS address for the peer's use */
    {"dns2", 1, setdnsaddr},		/* DNS address for the peer's use */
/* end compat hack */
    {"ms-dns", 1, setdnsaddr},		/* DNS address for the peer's use */
    {"ms-wins", 1, setwinsaddr},	/* Nameserver for SMB over TCP/IP for peer */
    {"noipx",  0, resetipxproto},	/* Disable IPXCP (and IPX) */
    {"-ipx",   0, resetipxproto},	/* Disable IPXCP (and IPX) */
    {"--version", 0, showversion},	/* Show version number */
    {"--help", 0, showhelp},		/* Show brief listing of options */
    {"-h", 0, showhelp},		/* ditto */

#ifdef PPP_FILTER
    {"pdebug", 1, setpdebug},		/* libpcap debugging */
    {"pass-filter", 1, setpassfilter},	/* set filter for packets to pass */
    {"active-filter", 1, setactivefilter}, /* set filter for active pkts */
#endif

#ifdef IPX_CHANGE
    {"ipx-network",          1, setipxnetwork}, /* IPX network number */
    {"ipxcp-accept-network", 0, setipxanet},    /* Accept peer netowrk */
    {"ipx-node",             1, setipxnode},    /* IPX node number */
    {"ipxcp-accept-local",   0, setipxalcl},    /* Accept our address */
    {"ipxcp-accept-remote",  0, setipxarmt},    /* Accept peer's address */
    {"ipx-routing",          1, setipxrouter},  /* IPX routing proto number */
    {"ipx-router-name",      1, setipxname},    /* IPX router name */
    {"ipxcp-restart",        1, setipxcptimeout}, /* Set timeout for IPXCP */
    {"ipxcp-max-terminate",  1, setipxcpterm},  /* max #xmits for term-reqs */
    {"ipxcp-max-configure",  1, setipxcpconf},  /* max #xmits for conf-reqs */
    {"ipxcp-max-failure",    1, setipxcpfails}, /* max #conf-naks for IPXCP */
#if 0
    {"ipx-compression", 1, setipxcompression}, /* IPX compression number */
#endif
    {"ipx",		     0, setipxproto},	/* Enable IPXCP (and IPX) */
    {"+ipx",		     0, setipxproto},	/* Enable IPXCP (and IPX) */
#endif /* IPX_CHANGE */

#ifdef MSLANMAN
    {"ms-lanman", 0, setmslanman},	/* Use LanMan psswd when using MS-CHAP */
#endif

    {NULL, 0, NULL}
};


#ifndef IMPLEMENTATION
#define IMPLEMENTATION ""
#endif

static const char usage_string[] = "\
pppd version %s patch level %d%s\n\
Usage: %s [ options ], where options are:\n\
	<device>	Communicate over the named device\n\
	<speed>		Set the baud rate to <speed>\n\
	<loc>:<rem>	Set the local and/or remote interface IP\n\
			addresses.  Either one may be omitted.\n\
	asyncmap <n>	Set the desired async map to hex <n>\n\
	auth		Require authentication from peer\n\
        connect <p>     Invoke shell command <p> to set up the serial line\n\
	crtscts		Use hardware RTS/CTS flow control\n\
	defaultroute	Add default route through interface\n\
	file <f>	Take options from file <f>\n\
	modem		Use modem control lines\n\
	mru <n>		Set MRU value to <n> for negotiation\n\
See pppd(8) for more options.\n\
";

static char *current_option;	/* the name of the option being parsed */
static int privileged_option;	/* set iff the current option came from root */
static char *option_source;	/* string saying where the option came from */

/*
 * parse_args - parse a string of arguments from the command line.
 */
int
parse_args(argc, argv)
    int argc;
    char **argv;
{
    char *arg;
    struct cmd *cmdp;
    int ret;

    privileged_option = privileged;
    option_source = "command line";
    while (argc > 0) {
	arg = *argv++;
	--argc;

	/*
	 * First see if it's a command.
	 */
	for (cmdp = cmds; cmdp->cmd_name; cmdp++)
	    if (!strcmp(arg, cmdp->cmd_name))
		break;

	if (cmdp->cmd_name != NULL) {
	    if (argc < cmdp->num_args) {
		option_error("too few parameters for option %s", arg);
		return 0;
	    }
	    current_option = arg;
	    if (!(*cmdp->cmd_func)(argv))
		return 0;
	    argc -= cmdp->num_args;
	    argv += cmdp->num_args;

	} else {
	    /*
	     * Maybe a tty name, speed or IP address?
	     */
	    if ((ret = setdevname(arg, 0)) == 0
		&& (ret = setspeed(arg)) == 0
		&& (ret = setipaddr(arg)) == 0) {
		option_error("unrecognized option '%s'", arg);
		usage();
		return 0;
	    }
	    if (ret < 0)	/* error */
		return 0;
	}
    }
    return 1;
}

/*
 * scan_args - scan the command line arguments to get the tty name,
 * if specified.
 */
void
scan_args(argc, argv)
    int argc;
    char **argv;
{
    char *arg;
    struct cmd *cmdp;

    while (argc > 0) {
	arg = *argv++;
	--argc;

	/* Skip options and their arguments */
	for (cmdp = cmds; cmdp->cmd_name; cmdp++)
	    if (!strcmp(arg, cmdp->cmd_name))
		break;

	if (cmdp->cmd_name != NULL) {
	    argc -= cmdp->num_args;
	    argv += cmdp->num_args;
	    continue;
	}

	/* Check if it's a tty name and copy it if so */
	(void) setdevname(arg, 1);
    }
}

/*
 * usage - print out a message telling how to use the program.
 */
void
usage()
{
    if (phase == PHASE_INITIALIZE)
	fprintf(stderr, usage_string, VERSION, PATCHLEVEL, IMPLEMENTATION,
		progname);
}

/*
 * showhelp - print out usage message and exit.
 */
static int
showhelp(argv)
    char **argv;
{
    if (phase == PHASE_INITIALIZE) {
	usage();
	exit(0);
    }
    return 0;
}

/*
 * showversion - print out the version number and exit.
 */
static int
showversion(argv)
    char **argv;
{
    if (phase == PHASE_INITIALIZE) {
	fprintf(stderr, "pppd version %s patch level %d%s\n",
		VERSION, PATCHLEVEL, IMPLEMENTATION);
	exit(0);
    }
    return 0;
}

/*
 * options_from_file - Read a string of options from a file,
 * and interpret them.
 */
int
options_from_file(filename, must_exist, check_prot, priv)
    char *filename;
    int must_exist;
    int check_prot;
    int priv;
{
    FILE *f;
    int i, newline, ret;
    struct cmd *cmdp;
    int oldpriv;
    char *argv[MAXARGS];
    char args[MAXARGS][MAXWORDLEN];
    char cmd[MAXWORDLEN];

    if ((f = fopen(filename, "r")) == NULL) {
	if (!must_exist && errno == ENOENT)
	    return 1;
	option_error("Can't open options file %s: %m", filename);
	return 0;
    }
    if (check_prot && !readable(fileno(f))) {
	option_error("Can't open options file %s: access denied", filename);
	fclose(f);
	return 0;
    }

    oldpriv = privileged_option;
    privileged_option = priv;
    ret = 0;
    while (getword(f, cmd, &newline, filename)) {
	/*
	 * First see if it's a command.
	 */
	for (cmdp = cmds; cmdp->cmd_name; cmdp++)
	    if (!strcmp(cmd, cmdp->cmd_name))
		break;

	if (cmdp->cmd_name != NULL) {
	    for (i = 0; i < cmdp->num_args; ++i) {
		if (!getword(f, args[i], &newline, filename)) {
		    option_error(
			"In file %s: too few parameters for option '%s'",
			filename, cmd);
		    goto err;
		}
		argv[i] = args[i];
	    }
	    current_option = cmd;
	    if (!(*cmdp->cmd_func)(argv))
		goto err;

	} else {
	    /*
	     * Maybe a tty name, speed or IP address?
	     */
	    if ((i = setdevname(cmd, 0)) == 0
		&& (i = setspeed(cmd)) == 0
		&& (i = setipaddr(cmd)) == 0) {
		option_error("In file %s: unrecognized option '%s'",
			     filename, cmd);
		goto err;
	    }
	    if (i < 0)		/* error */
		goto err;
	}
    }
    ret = 1;

err:
    fclose(f);
    privileged_option = oldpriv;
    return ret;
}

/*
 * options_from_user - See if the use has a ~/.ppprc file,
 * and if so, interpret options from it.
 */
int
options_from_user()
{
    char *user, *path, *file;
    int ret;
    struct passwd *pw;

    pw = getpwuid(getuid());
    if (pw == NULL || (user = pw->pw_dir) == NULL || user[0] == 0)
	return 1;
    file = _PATH_USEROPT;
    path = malloc(strlen(user) + strlen(file) + 2);
    if (path == NULL)
	novm("init file name");
    strcpy(path, user);
    strcat(path, "/");
    strcat(path, file);
    ret = options_from_file(path, 0, 1, privileged);
    free(path);
    return ret;
}

/*
 * options_for_tty - See if an options file exists for the serial
 * device, and if so, interpret options from it.
 */
int
options_for_tty()
{
    char *dev, *path, *p;
    int ret;

    dev = devnam;
    if (strncmp(dev, _PATH_DEV, sizeof _PATH_DEV - 1) == 0)
	dev += 5;
    if (strcmp(dev, "tty") == 0)
	return 1;		/* don't look for /etc/ppp/options.tty */
    path = malloc(strlen(_PATH_TTYOPT) + strlen(dev) + 1);
    if (path == NULL)
	novm("tty init file name");
    strcpy(path, _PATH_TTYOPT);
    /* Turn slashes into dots, for Solaris case (e.g. /dev/term/a) */
    for (p = path + strlen(path); *dev != 0; ++dev)
	*p++ = (*dev == '/'? '.': *dev);
    *p = 0;
    ret = options_from_file(path, 0, 0, 1);
    free(path);
    return ret;
}

/*
 * option_error - print a message about an error in an option.
 * The message is logged, and also sent to
 * stderr if phase == PHASE_INITIALIZE.
 */
void
option_error __V((char *fmt, ...))
{
    va_list args;
    char buf[256];

#if __STDC__
    va_start(args, fmt);
#else
    char *fmt;
    va_start(args);
    fmt = va_arg(args, char *);
#endif
    vfmtmsg(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (phase == PHASE_INITIALIZE)
	fprintf(stderr, "%s: %s\n", progname, buf);
    syslog(LOG_ERR, "%s", buf);
}

/*
 * readable - check if a file is readable by the real user.
 */
static int
readable(fd)
    int fd;
{
    uid_t uid;
    int ngroups, i;
    struct stat sbuf;
    GIDSET_TYPE groups[NGROUPS_MAX];

    uid = getuid();
    if (uid == 0)
	return 1;
    if (fstat(fd, &sbuf) != 0)
	return 0;
    if (sbuf.st_uid == uid)
	return sbuf.st_mode & S_IRUSR;
    if (sbuf.st_gid == getgid())
	return sbuf.st_mode & S_IRGRP;
    ngroups = getgroups(NGROUPS_MAX, groups);
    for (i = 0; i < ngroups; ++i)
	if (sbuf.st_gid == groups[i])
	    return sbuf.st_mode & S_IRGRP;
    return sbuf.st_mode & S_IROTH;
}

/*
 * Read a word from a file.
 * Words are delimited by white-space or by quotes (" or ').
 * Quotes, white-space and \ may be escaped with \.
 * \<newline> is ignored.
 */
int
getword(f, word, newlinep, filename)
    FILE *f;
    char *word;
    int *newlinep;
    char *filename;
{
    int c, len, escape;
    int quoted, comment;
    int value, digit, got, n;

#define isoctal(c) ((c) >= '0' && (c) < '8')

    *newlinep = 0;
    len = 0;
    escape = 0;
    comment = 0;

    /*
     * First skip white-space and comments.
     */
    for (;;) {
	c = getc(f);
	if (c == EOF)
	    break;

	/*
	 * A newline means the end of a comment; backslash-newline
	 * is ignored.  Note that we cannot have escape && comment.
	 */
	if (c == '\n') {
	    if (!escape) {
		*newlinep = 1;
		comment = 0;
	    } else
		escape = 0;
	    continue;
	}

	/*
	 * Ignore characters other than newline in a comment.
	 */
	if (comment)
	    continue;

	/*
	 * If this character is escaped, we have a word start.
	 */
	if (escape)
	    break;

	/*
	 * If this is the escape character, look at the next character.
	 */
	if (c == '\\') {
	    escape = 1;
	    continue;
	}

	/*
	 * If this is the start of a comment, ignore the rest of the line.
	 */
	if (c == '#') {
	    comment = 1;
	    continue;
	}

	/*
	 * A non-whitespace character is the start of a word.
	 */
	if (!isspace(c))
	    break;
    }

    /*
     * Save the delimiter for quoted strings.
     */
    if (!escape && (c == '"' || c == '\'')) {
        quoted = c;
	c = getc(f);
    } else
        quoted = 0;

    /*
     * Process characters until the end of the word.
     */
    while (c != EOF) {
	if (escape) {
	    /*
	     * This character is escaped: backslash-newline is ignored,
	     * various other characters indicate particular values
	     * as for C backslash-escapes.
	     */
	    escape = 0;
	    if (c == '\n') {
	        c = getc(f);
		continue;
	    }

	    got = 0;
	    switch (c) {
	    case 'a':
		value = '\a';
		break;
	    case 'b':
		value = '\b';
		break;
	    case 'f':
		value = '\f';
		break;
	    case 'n':
		value = '\n';
		break;
	    case 'r':
		value = '\r';
		break;
	    case 's':
		value = ' ';
		break;
	    case 't':
		value = '\t';
		break;

	    default:
		if (isoctal(c)) {
		    /*
		     * \ddd octal sequence
		     */
		    value = 0;
		    for (n = 0; n < 3 && isoctal(c); ++n) {
			value = (value << 3) + (c & 07);
			c = getc(f);
		    }
		    got = 1;
		    break;
		}

		if (c == 'x') {
		    /*
		     * \x<hex_string> sequence
		     */
		    value = 0;
		    c = getc(f);
		    for (n = 0; n < 2 && isxdigit(c); ++n) {
			digit = toupper(c) - '0';
			if (digit > 10)
			    digit += '0' + 10 - 'A';
			value = (value << 4) + digit;
			c = getc (f);
		    }
		    got = 1;
		    break;
		}

		/*
		 * Otherwise the character stands for itself.
		 */
		value = c;
		break;
	    }

	    /*
	     * Store the resulting character for the escape sequence.
	     */
	    if (len < MAXWORDLEN-1)
		word[len] = value;
	    ++len;

	    if (!got)
		c = getc(f);
	    continue;

	}

	/*
	 * Not escaped: see if we've reached the end of the word.
	 */
	if (quoted) {
	    if (c == quoted)
		break;
	} else {
	    if (isspace(c) || c == '#') {
		ungetc (c, f);
		break;
	    }
	}

	/*
	 * Backslash starts an escape sequence.
	 */
	if (c == '\\') {
	    escape = 1;
	    c = getc(f);
	    continue;
	}

	/*
	 * An ordinary character: store it in the word and get another.
	 */
	if (len < MAXWORDLEN-1)
	    word[len] = c;
	++len;

	c = getc(f);
    }

    /*
     * End of the word: check for errors.
     */
    if (c == EOF) {
	if (ferror(f)) {
	    if (errno == 0)
		errno = EIO;
	    option_error("Error reading %s: %m", filename);
	    die(1);
	}
	/*
	 * If len is zero, then we didn't find a word before the
	 * end of the file.
	 */
	if (len == 0)
	    return 0;
    }

    /*
     * Warn if the word was too long, and append a terminating null.
     */
    if (len >= MAXWORDLEN) {
	option_error("warning: word in file %s too long (%.20s...)",
		     filename, word);
	len = MAXWORDLEN - 1;
    }
    word[len] = 0;

    return 1;

#undef isoctal

}

/*
 * number_option - parse an unsigned numeric parameter for an option.
 */
static int
number_option(str, valp, base)
    char *str;
    u_int32_t *valp;
    int base;
{
    char *ptr;

    *valp = strtoul(str, &ptr, base);
    if (ptr == str) {
	option_error("invalid numeric parameter '%s' for %s option",
		     str, current_option);
	return 0;
    }
    return 1;
}


/*
 * int_option - like number_option, but valp is int *,
 * the base is assumed to be 0, and *valp is not changed
 * if there is an error.
 */
static int
int_option(str, valp)
    char *str;
    int *valp;
{
    u_int32_t v;

    if (!number_option(str, &v, 0))
	return 0;
    *valp = (int) v;
    return 1;
}


/*
 * The following procedures parse options.
 */

/*
 * readfile - take commands from a file.
 */
static int
readfile(argv)
    char **argv;
{
    return options_from_file(*argv, 1, 1, privileged_option);
}

/*
 * callfile - take commands from /etc/ppp/peers/<name>.
 * Name may not contain /../, start with / or ../, or end in /..
 */
static int
callfile(argv)
    char **argv;
{
    char *fname, *arg, *p;
    int l, ok;

    arg = *argv;
    ok = 1;
    if (arg[0] == '/' || arg[0] == 0)
	ok = 0;
    else {
	for (p = arg; *p != 0; ) {
	    if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == 0)) {
		ok = 0;
		break;
	    }
	    while (*p != '/' && *p != 0)
		++p;
	    if (*p == '/')
		++p;
	}
    }
    if (!ok) {
	option_error("call option value may not contain .. or start with /");
	return 0;
    }

    l = strlen(arg) + strlen(_PATH_PEERFILES) + 1;
    if ((fname = (char *) malloc(l)) == NULL)
	novm("call file name");
    strcpy(fname, _PATH_PEERFILES);
    strcat(fname, arg);

    ok = options_from_file(fname, 1, 1, 1);

    free(fname);
    return ok;
}


/*
 * setdebug - Set debug (command line argument).
 */
static int
setdebug(argv)
    char **argv;
{
    debug++;
    return (1);
}

/*
 * setkdebug - Set kernel debugging level.
 */
static int
setkdebug(argv)
    char **argv;
{
    return int_option(*argv, &kdebugflag);
}

#ifdef PPP_FILTER
/*
 * setpdebug - Set libpcap debugging level.
 */
static int
setpdebug(argv)
    char **argv;
{
    return int_option(*argv, &dflag);
}

/*
 * setpassfilter - Set the pass filter for packets
 */
static int
setpassfilter(argv)
    char **argv;
{
    pc.linktype = DLT_PPP;
    pc.snapshot = PPP_HDRLEN;
 
    if (pcap_compile(&pc, &pass_filter, *argv, 1, netmask) == 0)
	return 1;
    option_error("error in pass-filter expression: %s\n", pcap_geterr(&pc));
    return 0;
}

/*
 * setactivefilter - Set the active filter for packets
 */
static int
setactivefilter(argv)
    char **argv;
{
    pc.linktype = DLT_PPP;
    pc.snapshot = PPP_HDRLEN;
 
    if (pcap_compile(&pc, &active_filter, *argv, 1, netmask) == 0)
	return 1;
    option_error("error in active-filter expression: %s\n", pcap_geterr(&pc));
    return 0;
}
#endif

/*
 * noopt - Disable all options.
 */
static int
noopt(argv)
    char **argv;
{
    BZERO((char *) &lcp_wantoptions[0], sizeof (struct lcp_options));
    BZERO((char *) &lcp_allowoptions[0], sizeof (struct lcp_options));
    BZERO((char *) &ipcp_wantoptions[0], sizeof (struct ipcp_options));
    BZERO((char *) &ipcp_allowoptions[0], sizeof (struct ipcp_options));

#ifdef IPX_CHANGE
    BZERO((char *) &ipxcp_wantoptions[0], sizeof (struct ipxcp_options));
    BZERO((char *) &ipxcp_allowoptions[0], sizeof (struct ipxcp_options));
#endif /* IPX_CHANGE */

    return (1);
}

/*
 * noaccomp - Disable Address/Control field compression negotiation.
 */
static int
noaccomp(argv)
    char **argv;
{
    lcp_wantoptions[0].neg_accompression = 0;
    lcp_allowoptions[0].neg_accompression = 0;
    return (1);
}


/*
 * noasyncmap - Disable async map negotiation.
 */
static int
noasyncmap(argv)
    char **argv;
{
    lcp_wantoptions[0].neg_asyncmap = 0;
    lcp_allowoptions[0].neg_asyncmap = 0;
    return (1);
}


/*
 * noip - Disable IP and IPCP.
 */
static int
noip(argv)
    char **argv;
{
    ipcp_protent.enabled_flag = 0;
    return (1);
}


/*
 * nomagicnumber - Disable magic number negotiation.
 */
static int
nomagicnumber(argv)
    char **argv;
{
    lcp_wantoptions[0].neg_magicnumber = 0;
    lcp_allowoptions[0].neg_magicnumber = 0;
    return (1);
}


/*
 * nomru - Disable mru negotiation.
 */
static int
nomru(argv)
    char **argv;
{
    lcp_wantoptions[0].neg_mru = 0;
    lcp_allowoptions[0].neg_mru = 0;
    return (1);
}


/*
 * setmru - Set MRU for negotiation.
 */
static int
setmru(argv)
    char **argv;
{
    u_int32_t mru;

    if (!number_option(*argv, &mru, 0))
	return 0;
    lcp_wantoptions[0].mru = mru;
    lcp_wantoptions[0].neg_mru = 1;
    return (1);
}


/*
 * setmru - Set the largest MTU we'll use.
 */
static int
setmtu(argv)
    char **argv;
{
    u_int32_t mtu;

    if (!number_option(*argv, &mtu, 0))
	return 0;
    if (mtu < MINMRU || mtu > MAXMRU) {
	option_error("mtu option value of %u is too %s", mtu,
		     (mtu < MINMRU? "small": "large"));
	return 0;
    }
    lcp_allowoptions[0].mru = mtu;
    return (1);
}

#ifdef CBCP_SUPPORT
static int
setcbcp(argv)
    char **argv;
{
    lcp_wantoptions[0].neg_cbcp = 1;
    cbcp_protent.enabled_flag = 1;
    cbcp[0].us_number = strdup(*argv);
    if (cbcp[0].us_number == 0)
	novm("callback number");
    cbcp[0].us_type |= (1 << CB_CONF_USER);
    cbcp[0].us_type |= (1 << CB_CONF_ADMIN);
    return (1);
}
#endif

/*
 * nopcomp - Disable Protocol field compression negotiation.
 */
static int
nopcomp(argv)
    char **argv;
{
    lcp_wantoptions[0].neg_pcompression = 0;
    lcp_allowoptions[0].neg_pcompression = 0;
    return (1);
}


/*
 * setpassive - Set passive mode (don't give up if we time out sending
 * LCP configure-requests).
 */
static int
setpassive(argv)
    char **argv;
{
    lcp_wantoptions[0].passive = 1;
    return (1);
}


/*
 * setsilent - Set silent mode (don't start sending LCP configure-requests
 * until we get one from the peer).
 */
static int
setsilent(argv)
    char **argv;
{
    lcp_wantoptions[0].silent = 1;
    return 1;
}


/*
 * nopap - Disable PAP authentication with peer.
 */
static int
nopap(argv)
    char **argv;
{
    refuse_pap = 1;
    return (1);
}


/*
 * reqpap - Require PAP authentication from peer.
 */
static int
reqpap(argv)
    char **argv;
{
    lcp_wantoptions[0].neg_upap = 1;
    setauth(NULL);
    return 1;
}

#if OLD_OPTIONS
/*
 * setupapfile - specifies UPAP info for authenticating with peer.
 */
static int
setupapfile(argv)
    char **argv;
{
    FILE * ufile;
    int l;

    lcp_allowoptions[0].neg_upap = 1;

    /* open user info file */
    if ((ufile = fopen(*argv, "r")) == NULL) {
	option_error("unable to open user login data file %s", *argv);
	return 0;
    }
    if (!readable(fileno(ufile))) {
	option_error("%s: access denied", *argv);
	return 0;
    }
    check_access(ufile, *argv);

    /* get username */
    if (fgets(user, MAXNAMELEN - 1, ufile) == NULL
	|| fgets(passwd, MAXSECRETLEN - 1, ufile) == NULL){
	option_error("unable to read user login data file %s", *argv);
	return 0;
    }
    fclose(ufile);

    /* get rid of newlines */
    l = strlen(user);
    if (l > 0 && user[l-1] == '\n')
	user[l-1] = 0;
    l = strlen(passwd);
    if (l > 0 && passwd[l-1] == '\n')
	passwd[l-1] = 0;

    return (1);
}
#endif

/*
 * nochap - Disable CHAP authentication with peer.
 */
static int
nochap(argv)
    char **argv;
{
    refuse_chap = 1;
    return (1);
}


/*
 * reqchap - Require CHAP authentication from peer.
 */
static int
reqchap(argv)
    char **argv;
{
    lcp_wantoptions[0].neg_chap = 1;
    setauth(NULL);
    return (1);
}


/*
 * setnovj - disable vj compression
 */
static int
setnovj(argv)
    char **argv;
{
    ipcp_wantoptions[0].neg_vj = 0;
    ipcp_allowoptions[0].neg_vj = 0;
    return (1);
}


/*
 * setnovjccomp - disable VJ connection-ID compression
 */
static int
setnovjccomp(argv)
    char **argv;
{
    ipcp_wantoptions[0].cflag = 0;
    ipcp_allowoptions[0].cflag = 0;
    return 1;
}


/*
 * setvjslots - set maximum number of connection slots for VJ compression
 */
static int
setvjslots(argv)
    char **argv;
{
    int value;

    if (!int_option(*argv, &value))
	return 0;
    if (value < 2 || value > 16) {
	option_error("vj-max-slots value must be between 2 and 16");
	return 0;
    }
    ipcp_wantoptions [0].maxslotindex =
        ipcp_allowoptions[0].maxslotindex = value - 1;
    return 1;
}


/*
 * setconnector - Set a program to connect to a serial line
 */
static int
setconnector(argv)
    char **argv;
{
    connector = strdup(*argv);
    if (connector == NULL)
	novm("connect script");
    connector_info.priv = privileged_option;
    connector_info.source = option_source;

    return (1);
}

/*
 * setdisconnector - Set a program to disconnect from the serial line
 */
static int
setdisconnector(argv)
    char **argv;
{
    disconnector = strdup(*argv);
    if (disconnector == NULL)
	novm("disconnect script");
    disconnector_info.priv = privileged_option;
    disconnector_info.source = option_source;
  
    return (1);
}

/*
 * setwelcomer - Set a program to welcome a client after connection
 */
static int
setwelcomer(argv)
    char **argv;
{
    welcomer = strdup(*argv);
    if (welcomer == NULL)
	novm("welcome script");
    welcomer_info.priv = privileged_option;
    welcomer_info.source = option_source;

    return (1);
}

static int
setmaxcon(argv)
    char **argv;
{
    return int_option(*argv, &max_con_attempts);
}

/*
 * setmaxconnect - Set the maximum connect time
 */
static int
setmaxconnect(argv)
    char **argv;
{
    int value;

    if (!int_option(*argv, &value))
	return 0;
    if (value < 0) {
	option_error("maxconnect time must be positive");
	return 0;
    }
    if (maxconnect > 0 && (value == 0 || value > maxconnect)) {
	option_error("maxconnect time cannot be increased");
	return 0;
    }
    maxconnect = value;
    return 1;
}

/*
 * setdomain - Set domain name to append to hostname 
 */
static int
setdomain(argv)
    char **argv;
{
    if (!privileged_option) {
	option_error("using the domain option requires root privilege");
	return 0;
    }
    gethostname(hostname, MAXNAMELEN);
    if (**argv != 0) {
	if (**argv != '.')
	    strncat(hostname, ".", MAXNAMELEN - strlen(hostname));
	strncat(hostname, *argv, MAXNAMELEN - strlen(hostname));
    }
    hostname[MAXNAMELEN-1] = 0;
    return (1);
}


/*
 * setasyncmap - add bits to asyncmap (what we request peer to escape).
 */
static int
setasyncmap(argv)
    char **argv;
{
    u_int32_t asyncmap;

    if (!number_option(*argv, &asyncmap, 16))
	return 0;
    lcp_wantoptions[0].asyncmap |= asyncmap;
    lcp_wantoptions[0].neg_asyncmap = 1;
    return(1);
}


/*
 * setescape - add chars to the set we escape on transmission.
 */
static int
setescape(argv)
    char **argv;
{
    int n, ret;
    char *p, *endp;

    p = *argv;
    ret = 1;
    while (*p) {
	n = strtol(p, &endp, 16);
	if (p == endp) {
	    option_error("escape parameter contains invalid hex number '%s'",
			 p);
	    return 0;
	}
	p = endp;
	if (n < 0 || (0x20 <= n && n <= 0x3F) || n == 0x5E || n > 0xFF) {
	    option_error("can't escape character 0x%x", n);
	    ret = 0;
	} else
	    xmit_accm[0][n >> 5] |= 1 << (n & 0x1F);
	while (*p == ',' || *p == ' ')
	    ++p;
    }
    return ret;
}


/*
 * setspeed - Set the speed.
 */
static int
setspeed(arg)
    char *arg;
{
    char *ptr;
    int spd;

    spd = strtol(arg, &ptr, 0);
    if (ptr == arg || *ptr != 0 || spd == 0)
	return 0;
    inspeed = spd;
    return 1;
}


/*
 * setdevname - Set the device name.
 */
static int
setdevname(cp, quiet)
    char *cp;
    int quiet;
{
    struct stat statbuf;
    char dev[MAXPATHLEN];

    if (*cp == 0)
	return 0;

    if (strncmp(_PATH_DEV, cp, sizeof _PATH_DEV - 1) != 0) {
	strcpy(dev, _PATH_DEV);
	strncat(dev, cp, MAXPATHLEN - sizeof _PATH_DEV - 1);
	dev[MAXPATHLEN-1] = 0;
	cp = dev;
    }

    /*
     * Check if there is a device by this name.
     */
    if (stat(cp, &statbuf) < 0) {
	if (errno == ENOENT || quiet)
	    return 0;
	option_error("Couldn't stat %s: %m", cp);
	return -1;
    }

    (void) strncpy(devnam, cp, MAXPATHLEN);
    devnam[MAXPATHLEN-1] = 0;
    default_device = FALSE;
    devnam_info.priv = privileged_option;
    devnam_info.source = option_source;
  
    return 1;
}


/*
 * setipaddr - Set the IP address
 */
int
setipaddr(arg)
    char *arg;
{
    struct hostent *hp;
    char *colon;
    u_int32_t local, remote;
    ipcp_options *wo = &ipcp_wantoptions[0];
  
    /*
     * IP address pair separated by ":".
     */
    if ((colon = strchr(arg, ':')) == NULL)
	return 0;
  
    /*
     * If colon first character, then no local addr.
     */
    if (colon != arg) {
	*colon = '\0';
	if ((local = inet_addr(arg)) == -1) {
	    if ((hp = gethostbyname(arg)) == NULL) {
		option_error("unknown host: %s", arg);
		return -1;
	    } else {
		local = *(u_int32_t *)hp->h_addr;
	    }
	}
	if (bad_ip_adrs(local)) {
	    option_error("bad local IP address %s", ip_ntoa(local));
	    return -1;
	}
	if (local != 0)
	    wo->ouraddr = local;
	*colon = ':';
    }
  
    /*
     * If colon last character, then no remote addr.
     */
    if (*++colon != '\0') {
	if ((remote = inet_addr(colon)) == -1) {
	    if ((hp = gethostbyname(colon)) == NULL) {
		option_error("unknown host: %s", colon);
		return -1;
	    } else {
		remote = *(u_int32_t *)hp->h_addr;
		if (remote_name[0] == 0) {
		    strncpy(remote_name, colon, MAXNAMELEN);
		    remote_name[MAXNAMELEN-1] = 0;
		}
	    }
	}
	if (bad_ip_adrs(remote)) {
	    option_error("bad remote IP address %s", ip_ntoa(remote));
	    return -1;
	}
	if (remote != 0)
	    wo->hisaddr = remote;
    }

    return 1;
}


/*
 * setnoipdflt - disable setipdefault()
 */
static int
setnoipdflt(argv)
    char **argv;
{
    disable_defaultip = 1;
    return 1;
}


/*
 * setipcpaccl - accept peer's idea of our address
 */
static int
setipcpaccl(argv)
    char **argv;
{
    ipcp_wantoptions[0].accept_local = 1;
    return 1;
}


/*
 * setipcpaccr - accept peer's idea of its address
 */
static int
setipcpaccr(argv)
    char **argv;
{
    ipcp_wantoptions[0].accept_remote = 1;
    return 1;
}


/*
 * setnetmask - set the netmask to be used on the interface.
 */
static int
setnetmask(argv)
    char **argv;
{
    struct in_addr mask;

    if (!inet_aton(*argv, &mask) || (netmask & ~mask.s_addr)) {
	fprintf(stderr, "Invalid netmask %s\n", *argv);
	return (0);
    }

    netmask = mask.s_addr;
    return (1);
}

static int
setcrtscts(argv)
    char **argv;
{
    crtscts = 1;
    return (1);
}

static int
setnocrtscts(argv)
    char **argv;
{
    crtscts = -1;
    return (1);
}

static int
setxonxoff(argv)
    char **argv;
{
    lcp_wantoptions[0].asyncmap |= 0x000A0000;	/* escape ^S and ^Q */
    lcp_wantoptions[0].neg_asyncmap = 1;

    crtscts = -2;
    return (1);
}

static int
setnodetach(argv)
    char **argv;
{
    nodetach = 1;
    return (1);
}

static int
setupdetach(argv)
    char **argv;
{
    nodetach = -1;
    return (1);
}

static int
setdemand(argv)
    char **argv;
{
    demand = 1;
    persist = 1;
    return 1;
}

static int
setmodem(argv)
    char **argv;
{
    modem = 1;
    return 1;
}

static int
setlocal(argv)
    char **argv;
{
    modem = 0;
    return 1;
}

static int
setlock(argv)
    char **argv;
{
    lockflag = 1;
    return 1;
}

static int
setusehostname(argv)
    char **argv;
{
    usehostname = 1;
    return 1;
}

static int
setname(argv)
    char **argv;
{
    if (!privileged_option) {
	option_error("using the name option requires root privilege");
	return 0;
    }
    strncpy(our_name, argv[0], MAXNAMELEN);
    our_name[MAXNAMELEN-1] = 0;
    return 1;
}

static int
setuser(argv)
    char **argv;
{
    strncpy(user, argv[0], MAXNAMELEN);
    user[MAXNAMELEN-1] = 0;
    return 1;
}

static int
setremote(argv)
    char **argv;
{
    strncpy(remote_name, argv[0], MAXNAMELEN);
    remote_name[MAXNAMELEN-1] = 0;
    return 1;
}

static int
setauth(argv)
    char **argv;
{
    auth_required = 1;
    if (privileged_option > auth_req_info.priv) {
	auth_req_info.priv = privileged_option;
	auth_req_info.source = option_source;
    }
    return 1;
}

static int
setnoauth(argv)
    char **argv;
{
    if (auth_required && privileged_option < auth_req_info.priv) {
	option_error("cannot override auth option set by %s",
		     auth_req_info.source);
	return 0;
    }
    auth_required = 0;
    return 1;
}

static int
setdefaultroute(argv)
    char **argv;
{
    if (!ipcp_allowoptions[0].default_route) {
	option_error("defaultroute option is disabled");
	return 0;
    }
    ipcp_wantoptions[0].default_route = 1;
    return 1;
}

static int
setnodefaultroute(argv)
    char **argv;
{
    ipcp_allowoptions[0].default_route = 0;
    ipcp_wantoptions[0].default_route = 0;
    return 1;
}

static int
setproxyarp(argv)
    char **argv;
{
    if (!ipcp_allowoptions[0].proxy_arp) {
	option_error("proxyarp option is disabled");
	return 0;
    }
    ipcp_wantoptions[0].proxy_arp = 1;
    return 1;
}

static int
setnoproxyarp(argv)
    char **argv;
{
    ipcp_wantoptions[0].proxy_arp = 0;
    ipcp_allowoptions[0].proxy_arp = 0;
    return 1;
}

static int
setpersist(argv)
    char **argv;
{
    persist = 1;
    return 1;
}

static int
setnopersist(argv)
    char **argv;
{
    persist = 0;
    return 1;
}

static int
setdologin(argv)
    char **argv;
{
    uselogin = 1;
    return 1;
}

/*
 * Functions to set the echo interval for modem-less monitors
 */

static int
setlcpechointv(argv)
    char **argv;
{
    return int_option(*argv, &lcp_echo_interval);
}

static int
setlcpechofails(argv)
    char **argv;
{
    return int_option(*argv, &lcp_echo_fails);
}

/*
 * Functions to set timeouts, max transmits, etc.
 */
static int
setlcptimeout(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].timeouttime);
}

static int
setlcpterm(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxtermtransmits);
}

static int
setlcpconf(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxconfreqtransmits);
}

static int
setlcpfails(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxnakloops);
}

static int
setipcptimeout(argv)
    char **argv;
{
    return int_option(*argv, &ipcp_fsm[0].timeouttime);
}

static int
setipcpterm(argv)
    char **argv;
{
    return int_option(*argv, &ipcp_fsm[0].maxtermtransmits);
}

static int
setipcpconf(argv)
    char **argv;
{
    return int_option(*argv, &ipcp_fsm[0].maxconfreqtransmits);
}

static int
setipcpfails(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxnakloops);
}

static int
setpaptimeout(argv)
    char **argv;
{
    return int_option(*argv, &upap[0].us_timeouttime);
}

static int
setpapreqtime(argv)
    char **argv;
{
    return int_option(*argv, &upap[0].us_reqtimeout);
}

static int
setpapreqs(argv)
    char **argv;
{
    return int_option(*argv, &upap[0].us_maxtransmits);
}

static int
setchaptimeout(argv)
    char **argv;
{
    return int_option(*argv, &chap[0].timeouttime);
}

static int
setchapchal(argv)
    char **argv;
{
    return int_option(*argv, &chap[0].max_transmits);
}

static int
setchapintv(argv)
    char **argv;
{
    return int_option(*argv, &chap[0].chal_interval);
}

static int
noccp(argv)
    char **argv;
{
    ccp_protent.enabled_flag = 0;
    return 1;
}

static int
setbsdcomp(argv)
    char **argv;
{
    int rbits, abits;
    char *str, *endp;

    str = *argv;
    abits = rbits = strtol(str, &endp, 0);
    if (endp != str && *endp == ',') {
	str = endp + 1;
	abits = strtol(str, &endp, 0);
    }
    if (*endp != 0 || endp == str) {
	option_error("invalid parameter '%s' for bsdcomp option", *argv);
	return 0;
    }
    if ((rbits != 0 && (rbits < BSD_MIN_BITS || rbits > BSD_MAX_BITS))
	|| (abits != 0 && (abits < BSD_MIN_BITS || abits > BSD_MAX_BITS))) {
	option_error("bsdcomp option values must be 0 or %d .. %d",
		     BSD_MIN_BITS, BSD_MAX_BITS);
	return 0;
    }
    if (rbits > 0) {
	ccp_wantoptions[0].bsd_compress = 1;
	ccp_wantoptions[0].bsd_bits = rbits;
    } else
	ccp_wantoptions[0].bsd_compress = 0;
    if (abits > 0) {
	ccp_allowoptions[0].bsd_compress = 1;
	ccp_allowoptions[0].bsd_bits = abits;
    } else
	ccp_allowoptions[0].bsd_compress = 0;
    return 1;
}

static int
setnobsdcomp(argv)
    char **argv;
{
    ccp_wantoptions[0].bsd_compress = 0;
    ccp_allowoptions[0].bsd_compress = 0;
    return 1;
}

static int
setdeflate(argv)
    char **argv;
{
    int rbits, abits;
    char *str, *endp;

    str = *argv;
    abits = rbits = strtol(str, &endp, 0);
    if (endp != str && *endp == ',') {
	str = endp + 1;
	abits = strtol(str, &endp, 0);
    }
    if (*endp != 0 || endp == str) {
	option_error("invalid parameter '%s' for deflate option", *argv);
	return 0;
    }
    if ((rbits != 0 && (rbits < DEFLATE_MIN_SIZE || rbits > DEFLATE_MAX_SIZE))
	|| (abits != 0 && (abits < DEFLATE_MIN_SIZE
			  || abits > DEFLATE_MAX_SIZE))) {
	option_error("deflate option values must be 0 or %d .. %d",
		     DEFLATE_MIN_SIZE, DEFLATE_MAX_SIZE);
	return 0;
    }
    if (rbits > 0) {
	ccp_wantoptions[0].deflate = 1;
	ccp_wantoptions[0].deflate_size = rbits;
    } else
	ccp_wantoptions[0].deflate = 0;
    if (abits > 0) {
	ccp_allowoptions[0].deflate = 1;
	ccp_allowoptions[0].deflate_size = abits;
    } else
	ccp_allowoptions[0].deflate = 0;

    /* XXX copy over settings for switch compatibility */
    ccp_wantoptions[0].baddeflate = ccp_wantoptions[0].deflate;
    ccp_wantoptions[0].baddeflate_size = ccp_wantoptions[0].deflate_size;
    ccp_allowoptions[0].baddeflate = ccp_allowoptions[0].deflate;
    ccp_allowoptions[0].baddeflate_size = ccp_allowoptions[0].deflate_size;

    return 1;
}

static int
setnodeflate(argv)
    char **argv;
{
    ccp_wantoptions[0].deflate = 0;
    ccp_allowoptions[0].deflate = 0;
    return 1;
}

static int
setnodeflatedraft(argv)
    char **argv;
{
    ccp_wantoptions[0].deflate_draft = 0;
    ccp_allowoptions[0].deflate_draft = 0;
    return 1;
}

static int
setpred1comp(argv)
    char **argv;
{
    ccp_wantoptions[0].predictor_1 = 1;
    ccp_allowoptions[0].predictor_1 = 1;
    return 1;
}

static int
setnopred1comp(argv)
    char **argv;
{
    ccp_wantoptions[0].predictor_1 = 0;
    ccp_allowoptions[0].predictor_1 = 0;
    return 1;
}

static int
setipparam(argv)
    char **argv;
{
    ipparam = strdup(*argv);
    if (ipparam == NULL)
	novm("ipparam string");

    return 1;
}

static int
setpapcrypt(argv)
    char **argv;
{
    cryptpap = 1;
    return 1;
}

static int
setidle(argv)
    char **argv;
{
    return int_option(*argv, &idle_time_limit);
}

static int
setholdoff(argv)
    char **argv;
{
    return int_option(*argv, &holdoff);
}

/*
 * setdnsaddr - set the dns address(es)
 */
static int
setdnsaddr(argv)
    char **argv;
{
    u_int32_t dns;
    struct hostent *hp;

    dns = inet_addr(*argv);
    if (dns == -1) {
	if ((hp = gethostbyname(*argv)) == NULL) {
	    option_error("invalid address parameter '%s' for ms-dns option",
			 *argv);
	    return 0;
	}
	dns = *(u_int32_t *)hp->h_addr;
    }

    /* if there is no primary then update it. */
    if (ipcp_allowoptions[0].dnsaddr[0] == 0)
	ipcp_allowoptions[0].dnsaddr[0] = dns;

    /* always set the secondary address value to the same value. */
    ipcp_allowoptions[0].dnsaddr[1] = dns;

    return (1);
}

/*
 * setwinsaddr - set the wins address(es)
 * This is primrarly used with the Samba package under UNIX or for pointing
 * the caller to the existing WINS server on a Windows NT platform.
 */
static int
setwinsaddr(argv)
    char **argv;
{
    u_int32_t wins;
    struct hostent *hp;

    wins = inet_addr(*argv);
    if (wins == -1) {
	if ((hp = gethostbyname(*argv)) == NULL) {
	    option_error("invalid address parameter '%s' for ms-wins option",
			 *argv);
	    return 0;
	}
	wins = *(u_int32_t *)hp->h_addr;
    }

    /* if there is no primary then update it. */
    if (ipcp_allowoptions[0].winsaddr[0] == 0)
	ipcp_allowoptions[0].winsaddr[0] = wins;

    /* always set the secondary address value to the same value. */
    ipcp_allowoptions[0].winsaddr[1] = wins;

    return (1);
}

#ifdef IPX_CHANGE
static int
setipxrouter (argv)
    char **argv;
{
    ipxcp_wantoptions[0].neg_router  = 1;
    ipxcp_allowoptions[0].neg_router = 1;
    return int_option(*argv, &ipxcp_wantoptions[0].router); 
}

static int
setipxname (argv)
    char **argv;
{
    char *dest = ipxcp_wantoptions[0].name;
    char *src  = *argv;
    int  count;
    char ch;

    ipxcp_wantoptions[0].neg_name  = 1;
    ipxcp_allowoptions[0].neg_name = 1;
    memset (dest, '\0', sizeof (ipxcp_wantoptions[0].name));

    count = 0;
    while (*src) {
        ch = *src++;
	if (! isalnum (ch) && ch != '_') {
	    option_error("IPX router name must be alphanumeric or _");
	    return 0;
	}

	if (count >= sizeof (ipxcp_wantoptions[0].name)) {
	    option_error("IPX router name is limited to %d characters",
			 sizeof (ipxcp_wantoptions[0].name) - 1);
	    return 0;
	}

	dest[count++] = toupper (ch);
    }

    return 1;
}

static int
setipxcptimeout (argv)
    char **argv;
{
    return int_option(*argv, &ipxcp_fsm[0].timeouttime);
}

static int
setipxcpterm (argv)
    char **argv;
{
    return int_option(*argv, &ipxcp_fsm[0].maxtermtransmits);
}

static int
setipxcpconf (argv)
    char **argv;
{
    return int_option(*argv, &ipxcp_fsm[0].maxconfreqtransmits);
}

static int
setipxcpfails (argv)
    char **argv;
{
    return int_option(*argv, &ipxcp_fsm[0].maxnakloops);
}

static int
setipxnetwork(argv)
    char **argv;
{
    u_int32_t v;

    if (!number_option(*argv, &v, 16))
	return 0;

    ipxcp_wantoptions[0].our_network = (int) v;
    ipxcp_wantoptions[0].neg_nn      = 1;
    return 1;
}

static int
setipxanet(argv)
    char **argv;
{
    ipxcp_wantoptions[0].accept_network = 1;
    ipxcp_allowoptions[0].accept_network = 1;
    return 1;
}

static int
setipxalcl(argv)
    char **argv;
{
    ipxcp_wantoptions[0].accept_local = 1;
    ipxcp_allowoptions[0].accept_local = 1;
    return 1;
}

static int
setipxarmt(argv)
    char **argv;
{
    ipxcp_wantoptions[0].accept_remote = 1;
    ipxcp_allowoptions[0].accept_remote = 1;
    return 1;
}

static u_char *
setipxnodevalue(src,dst)
u_char *src, *dst;
{
    int indx;
    int item;

    for (;;) {
        if (!isxdigit (*src))
	    break;
	
	for (indx = 0; indx < 5; ++indx) {
	    dst[indx] <<= 4;
	    dst[indx] |= (dst[indx + 1] >> 4) & 0x0F;
	}

	item = toupper (*src) - '0';
	if (item > 9)
	    item -= 7;

	dst[5] = (dst[5] << 4) | item;
	++src;
    }
    return src;
}

static int
setipxnode(argv)
    char **argv;
{
    char *end;

    memset (&ipxcp_wantoptions[0].our_node[0], 0, 6);
    memset (&ipxcp_wantoptions[0].his_node[0], 0, 6);

    end = setipxnodevalue (*argv, &ipxcp_wantoptions[0].our_node[0]);
    if (*end == ':')
	end = setipxnodevalue (++end, &ipxcp_wantoptions[0].his_node[0]);

    if (*end == '\0') {
        ipxcp_wantoptions[0].neg_node = 1;
        return 1;
    }

    option_error("invalid parameter '%s' for ipx-node option", *argv);
    return 0;
}

static int
setipxproto(argv)
    char **argv;
{
    ipxcp_protent.enabled_flag = 1;
    return 1;
}

static int
resetipxproto(argv)
    char **argv;
{
    ipxcp_protent.enabled_flag = 0;
    return 1;
}
#else

static int
resetipxproto(argv)
    char **argv;
{
    return 1;
}
#endif /* IPX_CHANGE */

#ifdef MSLANMAN
static int
setmslanman(argv)
    char **argv;
{
    ms_lanman = 1;
    return (1);
}
#endif
