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
static char rcsid[] = "$Id: options.c,v 1.1 1994/03/30 09:38:16 jkh Exp $";
#endif

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <syslog.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ppp.h"
#include "pppd.h"
#include "pathnames.h"
#include "patchlevel.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "upap.h"
#include "chap.h"

#define FALSE	0
#define TRUE	1


/*
 * Prototypes
 */
static int setdebug __ARGS((void));
static int setpassive __ARGS((void));
static int setsilent __ARGS((void));
static int noopt __ARGS((void));
static int setnovj __ARGS((void));
static int reqpap __ARGS((void));
static int nopap __ARGS((void));
static int setupapfile __ARGS((char **));
static int nochap __ARGS((void));
static int reqchap __ARGS((void));
static int setspeed __ARGS((char *));
static int noaccomp __ARGS((void));
static int noasyncmap __ARGS((void));
static int noipaddr __ARGS((void));
static int nomagicnumber __ARGS((void));
static int setasyncmap __ARGS((char **));
static int setmru __ARGS((char **));
static int nomru __ARGS((void));
static int nopcomp __ARGS((void));
static int setconnector __ARGS((char **));
static int setdomain __ARGS((char **));
static int setnetmask __ARGS((char **));
static int setcrtscts __ARGS((void));
static int setnodetach __ARGS((void));
static int setmodem __ARGS((void));
static int setlocal __ARGS((void));
static int setname __ARGS((char **));
static int setuser __ARGS((char **));
static int setremote __ARGS((char **));
static int setauth __ARGS((void));
static int readfile __ARGS((char **));
static int setdefaultroute __ARGS((void));
static int setproxyarp __ARGS((void));
static int setpersist __ARGS((void));
static int setdologin __ARGS((void));
static int setusehostname __ARGS((void));
static int setnoipdflt __ARGS((void));
static int setlcptimeout __ARGS((char **));
static int setlcpterm __ARGS((char **));
static int setlcpconf __ARGS((char **));
static int setlcpfails __ARGS((char **));
static int setipcptimeout __ARGS((char **));
static int setipcpterm __ARGS((char **));
static int setipcpconf __ARGS((char **));
static int setipcpfails __ARGS((char **));
static int setpaptimeout __ARGS((char **));
static int setpapreqs __ARGS((char **));
static int setchaptimeout __ARGS((char **));
static int setchapchal __ARGS((char **));
static int setchapintv __ARGS((char **));
static int setipcpaccl __ARGS((void));
static int setipcpaccr __ARGS((void));

static int number_option __ARGS((char *, long *, int));


/*
 * Option variables
 */
extern char *progname;
extern int debug;
extern int modem;
extern int crtscts;
extern int nodetach;
extern char *connector;
extern int inspeed;
extern char devname[];
extern int default_device;
extern u_long netmask;
extern int detach;
extern char user[];
extern char passwd[];
extern int auth_required;
extern int proxyarp;
extern int persist;
extern int uselogin;
extern char our_name[];
extern char remote_name[];
int usehostname;
int disable_defaultip;

/*
 * Valid arguments.
 */
static struct cmd {
    char *cmd_name;
    int num_args;
    int (*cmd_func)();
} cmds[] = {
    "-all", 0, noopt,		/* Don't request/allow any options */
    "-ac", 0, noaccomp,		/* Disable Address/Control compress */
    "-am", 0, noasyncmap,	/* Disable asyncmap negotiation */
    "-as", 1, setasyncmap,	/* set the desired async map */
    "-d", 0, setdebug,		/* Increase debugging level */
    "-detach", 0, setnodetach,	/* don't fork */
    "-ip", 0, noipaddr,		/* Disable IP address negotiation */
    "-mn", 0, nomagicnumber,	/* Disable magic number negotiation */
    "-mru", 0, nomru,		/* Disable mru negotiation */
    "-p", 0, setpassive,	/* Set passive mode */
    "-pc", 0, nopcomp,		/* Disable protocol field compress */
    "+ua", 1, setupapfile,	/* Get PAP user and password from file */
    "+pap", 0, reqpap,		/* Require PAP auth from peer */
    "-pap", 0, nopap,		/* Don't allow UPAP authentication with peer */
    "+chap", 0, reqchap,	/* Require CHAP authentication from peer */
    "-chap", 0, nochap,		/* Don't allow CHAP authentication with peer */
    "-vj", 0, setnovj,		/* disable VJ compression */
    "asyncmap", 1, setasyncmap,	/* set the desired async map */
    "connect", 1, setconnector,	/* A program to set up a connection */
    "crtscts", 0, setcrtscts,	/* set h/w flow control */
    "debug", 0, setdebug,	/* Increase debugging level */
    "domain", 1, setdomain,	/* Add given domain name to hostname*/
    "mru", 1, setmru,		/* Set MRU value for negotiation */
    "netmask", 1, setnetmask,	/* set netmask */
    "passive", 0, setpassive,	/* Set passive mode */
    "silent", 0, setsilent,	/* Set silent mode */
    "modem", 0, setmodem,	/* Use modem control lines */
    "local", 0, setlocal,	/* Don't use modem control lines */
    "name", 1, setname,		/* Set local name for authentication */
    "user", 1, setuser,		/* Set username for PAP auth with peer */
    "usehostname", 0, setusehostname,	/* Must use hostname for auth. */
    "remotename", 1, setremote,	/* Set remote name for authentication */
    "auth", 0, setauth,		/* Require authentication from peer */
    "file", 1, readfile,	/* Take options from a file */
    "defaultroute", 0, setdefaultroute,	/* Add default route */
    "proxyarp", 0, setproxyarp,	/* Add proxy ARP entry */
    "persist", 0, setpersist,	/* Keep on reopening connection after close */
    "login", 0, setdologin,	/* Use system password database for UPAP */
    "noipdefault", 0, setnoipdflt, /* Don't use name for default IP adrs */
    "lcp-restart", 1, setlcptimeout,	/* Set timeout for LCP */
    "lcp-max-terminate", 1, setlcpterm,	/* Set max #xmits for term-reqs */
    "lcp-max-configure", 1, setlcpconf,	/* Set max #xmits for conf-reqs */
    "lcp-max-failure", 1, setlcpfails,	/* Set max #conf-naks for LCP */
    "ipcp-restart", 1, setipcptimeout,	/* Set timeout for IPCP */
    "ipcp-max-terminate", 1, setipcpterm, /* Set max #xmits for term-reqs */
    "ipcp-max-configure", 1, setipcpconf, /* Set max #xmits for conf-reqs */
    "ipcp-max-failure", 1, setipcpfails,  /* Set max #conf-naks for IPCP */
    "pap-restart", 1, setpaptimeout,	/* Set timeout for UPAP */
    "pap-max-authreq", 1, setpapreqs,	/* Set max #xmits for auth-reqs */
    "chap-restart", 1, setchaptimeout,	/* Set timeout for CHAP */
    "chap-max-challenge", 1, setchapchal, /* Set max #xmits for challenge */
    "chap-interval", 1, setchapintv,	/* Set interval for rechallenge */
    "ipcp-accept-local", 0, setipcpaccl, /* Accept peer's address for us */
    "ipcp-accept-remote", 0, setipcpaccr, /* Accept peer's address for it */
    NULL
};


static char *usage_string = "\
pppd version %s patch level %d\n\
Usage: %s [ arguments ], where arguments are:\n\
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
	netmask <n>	Set interface netmask to <n>\n\
See pppd(8) for more options.\n\
";

/*
Options omitted:
	-all		Don't request/allow any options\n\
	-ac		Disable Address/Control compression\n\
	-am		Disable asyncmap negotiation\n\
	-as <n>		Set the desired async map to hex <n>\n\
	-d		Increase debugging level\n\
	-detach		Don't fork to background\n\
	-ip		Disable IP address negotiation\n\
	-mn		Disable magic number negotiation\n\
	-mru		Disable mru negotiation\n\
	-p		Set passive mode\n\
	-pc		Disable protocol field compression\n\
	+ua <f>		Get username and password for authenticating\n\
			with peer using PAP from file <f>\n\
	+pap		Require PAP authentication from peer\n\
	-pap		Don't agree to authenticating with peer using PAP\n\
	+chap		Require CHAP authentication from peer\n\
	-chap		Don't agree to authenticating with peer using CHAP\n\
        -vj             disable VJ compression\n\
	-auth		Don't agree to authenticate with peer\n\
	debug		Increase debugging level\n\
        domain <d>      Append domain name <d> to hostname for authentication\n\
	passive		Set passive mode\n\
	local		Don't use modem control lines\n\
	proxyarp	Add proxy ARP entry\n\
*/


/*
 * parse_args - parse a string of arguments, from the command
 * line or from a file.
 */
int
parse_args(argc, argv)
    int argc;
    char **argv;
{
    char *arg, *val;
    struct cmd *cmdp;

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
		fprintf(stderr, "Too few parameters for command %s\n", arg);
		return 0;
	    }
	    if (!(*cmdp->cmd_func)(argv))
		return 0;
	    argc -= cmdp->num_args;
	    argv += cmdp->num_args;

	} else {
	    /*
	     * Maybe a tty name, speed or IP address?
	     */
	    if (!setdevname(arg) && !setspeed(arg) && !setipaddr(arg)) {
		fprintf(stderr, "%s: unrecognized command\n", arg);
		usage();
		return 0;
	    }
	}
    }
    return 1;
}

/*
 * usage - print out a message telling how to use the program.
 */
usage()
{
    fprintf(stderr, usage_string, VERSION, PATCHLEVEL, progname);
}

/*
 * options_from_file - Read a string of options from a file,
 * and interpret them.
 */
int
options_from_file(filename, must_exist)
    char *filename;
    int must_exist;
{
    FILE *f;
    int i, newline;
    struct cmd *cmdp;
    char *argv[MAXARGS];
    char args[MAXARGS][MAXWORDLEN];
    char cmd[MAXWORDLEN];

    if ((f = fopen(filename, "r")) == NULL) {
	if (!must_exist && errno == ENOENT)
	    return 1;
	perror(filename);
	exit(1);
    }
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
		    fprintf(stderr,
			    "In file %s: too few parameters for command %s\n",
			    filename, cmd);
		    fclose(f);
		    return 0;
		}
		argv[i] = args[i];
	    }
	    if (!(*cmdp->cmd_func)(argv)) {
		fclose(f);
		return 0;
	    }

	} else {
	    /*
	     * Maybe a tty name, speed or IP address?
	     */
	    if (!setdevname(cmd) && !setspeed(cmd) && !setipaddr(cmd)) {
		fprintf(stderr, "In file %s: unrecognized command %s\n",
			filename, cmd);
		fclose(f);
		return 0;
	    }
	}
    }
    return 1;
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

    if ((user = getenv("HOME")) == NULL)
	return;
    file = "/.ppprc";
    path = malloc(strlen(user) + strlen(file) + 1);
    if (path == NULL)
	novm("init file name");
    strcpy(path, user);
    strcat(path, file);
    ret = options_from_file(path, 0);
    free(path);
    return ret;
}

/*
 * Read a word from a file.
 * Words are delimited by white-space or by quotes (").
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
    int quoted;

    *newlinep = 0;
    len = 0;
    escape = 0;
    quoted = 0;

    /*
     * First skip white-space and comments
     */
    while ((c = getc(f)) != EOF) {
	if (c == '\\') {
	    /*
	     * \<newline> is ignored; \ followed by anything else
	     * starts a word.
	     */
	    if ((c = getc(f)) == '\n')
		continue;
	    word[len++] = '\\';
	    escape = 1;
	    break;
	}
	if (c == '\n')
	    *newlinep = 1;	/* next word starts a line */
	else if (c == '#') {
	    /* comment - ignore until EOF or \n */
	    while ((c = getc(f)) != EOF && c != '\n')
		;
	    if (c == EOF)
		break;
	    *newlinep = 1;
	} else if (!isspace(c))
	    break;
    }

    /*
     * End of file or error - fail
     */
    if (c == EOF) {
	if (ferror(f)) {
	    perror(filename);
	    die(1);
	}
	return 0;
    }

    for (;;) {
	/*
	 * Is this character escaped by \ ?
	 */
	if (escape) {
	    if (c == '\n')
		--len;			/* ignore \<newline> */
	    else if (c == '"' || isspace(c) || c == '\\')
		word[len-1] = c;	/* put special char in word */
	    else {
		if (len < MAXWORDLEN-1)
		    word[len] = c;
		++len;
	    }
	    escape = 0;
	} else if (c == '"') {
	    quoted = !quoted;
	} else if (!quoted && (isspace(c) || c == '#')) {
	    ungetc(c, f);
	    break;
	} else {
	    if (len < MAXWORDLEN-1)
		word[len] = c;
	    ++len;
	    if (c == '\\')
		escape = 1;
	}
	if ((c = getc(f)) == EOF)
	    break;
    }

    if (ferror(f)) {
	perror(filename);
	die(1);
    }

    if (len >= MAXWORDLEN) {
	word[MAXWORDLEN-1] = 0;
	fprintf(stderr, "%s: warning: word in file %s too long (%.20s...)\n",
		progname, filename, word);
    } else
	word[len] = 0;

    return 1;
}

/*
 * number_option - parse a numeric parameter for an option
 */
static int
number_option(str, valp, base)
    char *str;
    long *valp;
    int base;
{
    char *ptr;

    *valp = strtol(str, &ptr, base);
    if (ptr == str) {
	fprintf(stderr, "%s: invalid number: %s\n", progname, str);
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
    long v;

    if (!number_option(str, &v, 0))
	return 0;
    *valp = (int) v;
    return 1;
}


/*
 * The following procedures execute commands.
 */

/*
 * readfile - take commands from a file.
 */
static int
readfile(argv)
    char **argv;
{
    return options_from_file(*argv, 1);
}

/*
 * setdebug - Set debug (command line argument).
 */
static int
setdebug()
{
    debug++;
    setlogmask(LOG_UPTO(LOG_DEBUG));
    return (1);
}

/*
 * noopt - Disable all options.
 */
static int
noopt()
{
    BZERO((char *) &lcp_wantoptions[0], sizeof (struct lcp_options));
    BZERO((char *) &lcp_allowoptions[0], sizeof (struct lcp_options));
    BZERO((char *) &ipcp_wantoptions[0], sizeof (struct ipcp_options));
    BZERO((char *) &ipcp_allowoptions[0], sizeof (struct ipcp_options));
    return (1);
}

/*
 * noaccomp - Disable Address/Control field compression negotiation.
 */
static int
noaccomp()
{
    lcp_wantoptions[0].neg_accompression = 0;
    lcp_allowoptions[0].neg_accompression = 0;
    return (1);
}


/*
 * noasyncmap - Disable async map negotiation.
 */
static int
noasyncmap()
{
    lcp_wantoptions[0].neg_asyncmap = 0;
    lcp_allowoptions[0].neg_asyncmap = 0;
    return (1);
}


/*
 * noipaddr - Disable IP address negotiation.
 */
static int
noipaddr()
{
    ipcp_wantoptions[0].neg_addr = 0;
    ipcp_allowoptions[0].neg_addr = 0;
    return (1);
}


/*
 * nomagicnumber - Disable magic number negotiation.
 */
static int
nomagicnumber()
{
    lcp_wantoptions[0].neg_magicnumber = 0;
    lcp_allowoptions[0].neg_magicnumber = 0;
    return (1);
}


/*
 * nomru - Disable mru negotiation.
 */
static int
nomru()
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
    long mru;

    if (!number_option(*argv, &mru, 0))
	return 0;
    lcp_wantoptions[0].mru = mru;
    lcp_wantoptions[0].neg_mru = 1;
    return (1);
}


/*
 * nopcomp - Disable Protocol field compression negotiation.
 */
static int
nopcomp()
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
setpassive()
{
    lcp_wantoptions[0].passive = 1;
    return (1);
}


/*
 * setsilent - Set silent mode (don't start sending LCP configure-requests
 * until we get one from the peer).
 */
static int
setsilent()
{
    lcp_wantoptions[0].silent = 1;
    return 1;
}


/*
 * nopap - Disable PAP authentication with peer.
 */
static int
nopap()
{
    lcp_allowoptions[0].neg_upap = 0;
    return (1);
}


/*
 * reqpap - Require PAP authentication from peer.
 */
static int
reqpap()
{
    lcp_wantoptions[0].neg_upap = 1;
    auth_required = 1;
}


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
	fprintf(stderr, "unable to open user login data file %s\n", *argv);
	exit(1);
    }
    check_access(ufile, *argv);

    /* get username */
    if (fgets(user, MAXNAMELEN - 1, ufile) == NULL
	|| fgets(passwd, MAXSECRETLEN - 1, ufile) == NULL){
	fprintf(stderr, "Unable to read user login data file %s.\n", *argv);
	exit(2);
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


/*
 * nochap - Disable CHAP authentication with peer.
 */
static int
nochap()
{
    lcp_allowoptions[0].neg_chap = 0;
    return (1);
}


/*
 * reqchap - Require CHAP authentication from peer.
 */
static int
reqchap()
{
    lcp_wantoptions[0].neg_chap = 1;
    auth_required = 1;
    return (1);
}


/*
 * setnovj - diable vj compression
 */
static int
setnovj()
{
    ipcp_wantoptions[0].neg_vj = 0;
    ipcp_allowoptions[0].neg_vj = 0;
    return (1);
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
	novm("connector string");
  
    return (1);
}


/*
 * setdomain - Set domain name to append to hostname 
 */
static int
setdomain(argv)
    char **argv;
{
    strncat(hostname, *argv, MAXNAMELEN - strlen(hostname));
    hostname[MAXNAMELEN-1] = 0;
    return (1);
}

static int
setasyncmap(argv)
    char **argv;
{
    long asyncmap;

    if (!number_option(*argv, &asyncmap, 16))
	return 0;
    lcp_wantoptions[0].asyncmap |= asyncmap;
    lcp_wantoptions[0].neg_asyncmap = 1;
    return(1);
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
int
setdevname(cp)
    char *cp;
{
    struct stat statbuf;
    char *tty, *ttyname();
    char dev[MAXPATHLEN];
  
    if (strncmp("/dev/", cp, 5) != 0) {
	strcpy(dev, "/dev/");
	strncat(dev, cp, MAXPATHLEN - 5);
	dev[MAXPATHLEN-1] = 0;
	cp = dev;
    }

    /*
     * Check if there is a device by this name.
     */
    if (stat(cp, &statbuf) < 0) {
	if (errno == ENOENT)
	    return (0);
	syslog(LOG_ERR, cp);
	exit(1);
    }
  
    (void) strncpy(devname, cp, MAXPATHLEN);
    devname[MAXPATHLEN-1] = 0;
    default_device = FALSE;
  
    return (1);
}


/*
 * setipaddr - Set the IP address
 */
int
setipaddr(arg)
    char *arg;
{
    struct hostent *hp;
    char *colon, *index();
    u_long local, remote;
    ipcp_options *wo = &ipcp_wantoptions[0];
  
    /*
     * IP address pair separated by ":".
     */
    if ((colon = index(arg, ':')) == NULL)
	return (0);
  
    /*
     * If colon first character, then no local addr.
     */
    if (colon != arg) {
	*colon = '\0';
	if ((local = inet_addr(arg)) == -1) {
	    if ((hp = gethostbyname(arg)) == NULL) {
		fprintf(stderr, "unknown host: %s", arg);
		local = 0;
	    } else {
		local = *(long *)hp->h_addr;
		if (our_name[0] == 0) {
		    strncpy(our_name, arg, MAXNAMELEN);
		    our_name[MAXNAMELEN-1] = 0;
		}
	    }
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
		fprintf(stderr, "unknown host: %s", colon);
		remote = 0;
	    } else {
		remote = *(long *)hp->h_addr;
		if (remote_name[0] == 0) {
		    strncpy(remote_name, colon, MAXNAMELEN);
		    remote_name[MAXNAMELEN-1] = 0;
		}
	    }
	}
	if (remote != 0)
	    wo->hisaddr = remote;
    }

    return (1);
}


/*
 * setnoipdflt - disable setipdefault()
 */
static int
setnoipdflt()
{
    disable_defaultip = 1;
    return 1;
}


/*
 * setipcpaccl - accept peer's idea of our address
 */
static int
setipcpaccl()
{
    ipcp_wantoptions[0].accept_local = 1;
    return 1;
}


/*
 * setipcpaccr - accept peer's idea of its address
 */
static int
setipcpaccr()
{
    ipcp_wantoptions[0].accept_remote = 1;
    return 1;
}


/*
 * setipdefault - default our local IP address based on our hostname.
 */
void
setipdefault()
{
    struct hostent *hp;
    u_long local;
    ipcp_options *wo = &ipcp_wantoptions[0];

    /*
     * If local IP address already given, don't bother.
     */
    if (wo->ouraddr != 0 || disable_defaultip)
	return;

    /*
     * Look up our hostname (possibly with domain name appended)
     * and take the first IP address as our local IP address.
     * If there isn't an IP address for our hostname, too bad.
     */
    wo->accept_local = 1;	/* don't insist on this default value */
    if ((hp = gethostbyname(hostname)) == NULL)
	return;
    local = *(long *)hp->h_addr;
    if (local != 0)
	wo->ouraddr = local;
}


/*
 * setnetmask - set the netmask to be used on the interface.
 */
static int
setnetmask(argv)
    char **argv;
{
    u_long mask;
	
    if ((mask = inet_addr(*argv)) == -1) {
	fprintf(stderr, "Invalid netmask %s\n", *argv);
	exit(1);
    }

    netmask = mask;
    return (1);
}

static int
setcrtscts()
{
    crtscts = 1;
    return (1);
}

static int
setnodetach()
{
    nodetach = 1;
    return (1);
}

static int
setmodem()
{
    modem = 1;
    return 1;
}

static int
setlocal()
{
    modem = 0;
    return 1;
}

static int
setusehostname()
{
    usehostname = 1;
    return 1;
}

static int
setname(argv)
    char **argv;
{
    if (our_name[0] == 0) {
	strncpy(our_name, argv[0], MAXNAMELEN);
	our_name[MAXNAMELEN-1] = 0;
    }
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
setauth()
{
    auth_required = 1;
    return 1;
}

static int
setdefaultroute()
{
    ipcp_wantoptions[0].default_route = 1;
    return 1;
}

static int
setproxyarp()
{
    ipcp_wantoptions[0].proxy_arp = 1;
    return 1;
}

static int
setpersist()
{
    persist = 1;
    return 1;
}

static int
setdologin()
{
    uselogin = 1;
    return 1;
}

/*
 * Functions to set timeouts, max transmits, etc.
 */
static int
setlcptimeout(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].timeouttime, 0);
}

static int setlcpterm(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxtermtransmits, 0);
}

static int setlcpconf(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxconfreqtransmits, 0);
}

static int setlcpfails(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxnakloops, 0);
}

static int setipcptimeout(argv)
    char **argv;
{
    return int_option(*argv, &ipcp_fsm[0].timeouttime, 0);
}

static int setipcpterm(argv)
    char **argv;
{
    return int_option(*argv, &ipcp_fsm[0].maxtermtransmits, 0);
}

static int setipcpconf(argv)
    char **argv;
{
    return int_option(*argv, &ipcp_fsm[0].maxconfreqtransmits, 0);
}

static int setipcpfails(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxnakloops, 0);
}

static int setpaptimeout(argv)
    char **argv;
{
    return int_option(*argv, &upap[0].us_timeouttime, 0);
}

static int setpapreqs(argv)
    char **argv;
{
    return int_option(*argv, &upap[0].us_maxtransmits, 0);
}

static int setchaptimeout(argv)
    char **argv;
{
    return int_option(*argv, &chap[0].timeouttime, 0);
}

static int setchapchal(argv)
    char **argv;
{
    return int_option(*argv, &chap[0].max_transmits, 0);
}

static int setchapintv(argv)
    char **argv;
{
    return int_option(*argv, &chap[0].chal_interval, 0);
}
