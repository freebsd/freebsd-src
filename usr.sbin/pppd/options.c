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
static char rcsid[] = "$Id: options.c,v 1.5 1995/09/06 16:33:40 pst Exp $";
#endif

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#define devname STDLIB_devname
#include <stdlib.h>
#undef devname
#include <termios.h>
#include <syslog.h>
#include <string.h>
#include <netdb.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

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

#ifdef ultrix
char *strdup __ARGS((char *));
#endif

#ifndef GIDSET_TYPE
#define GIDSET_TYPE	gid_t
#endif

/*
 * Prototypes
 */
static int setdebug __ARGS((void));
static int setkdebug __ARGS((char **));
static int setpassive __ARGS((void));
static int setsilent __ARGS((void));
static int noopt __ARGS((void));
static int setnovj __ARGS((void));
static int setnovjccomp __ARGS((void));
static int setvjslots __ARGS((char **));
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
static int setescape __ARGS((char **));
static int setmru __ARGS((char **));
static int setmtu __ARGS((char **));
static int nomru __ARGS((void));
static int nopcomp __ARGS((void));
static int setconnector __ARGS((char **));
static int setdisconnector __ARGS((char **));
static int setdomain __ARGS((char **));
static int setnetmask __ARGS((char **));
static int setcrtscts __ARGS((void));
static int setxonxoff __ARGS((void));
static int setnodetach __ARGS((void));
static int setmodem __ARGS((void));
static int setlocal __ARGS((void));
static int setlock __ARGS((void));
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
static int setlcpechointv __ARGS((char **));
static int setlcpechofails __ARGS((char **));

static int number_option __ARGS((char *, long *, int));
static int readable __ARGS((int fd));

/*
 * Option variables
 */
extern char *progname;
extern int debug;
extern int kdebugflag;
extern int modem;
extern int lockflag;
extern int crtscts;
extern int nodetach;
extern char *connector;
extern char *disconnector;
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
extern u_long lcp_echo_interval;
extern u_long lcp_echo_fails;
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
    {"-all", 0, noopt},		/* Don't request/allow any options */
    {"-ac", 0, noaccomp},	/* Disable Address/Control compress */
    {"-am", 0, noasyncmap},	/* Disable asyncmap negotiation */
    {"-as", 1, setasyncmap},	/* set the desired async map */
    {"-d", 0, setdebug},	/* Increase debugging level */
    {"-detach", 0, setnodetach}, /* don't fork */
    {"-ip", 0, noipaddr},	/* Disable IP address negotiation */
    {"-mn", 0, nomagicnumber},	/* Disable magic number negotiation */
    {"-mru", 0, nomru},		/* Disable mru negotiation */
    {"-p", 0, setpassive},	/* Set passive mode */
    {"-pc", 0, nopcomp},	/* Disable protocol field compress */
    {"+ua", 1, setupapfile},	/* Get PAP user and password from file */
    {"+pap", 0, reqpap},	/* Require PAP auth from peer */
    {"-pap", 0, nopap},		/* Don't allow UPAP authentication with peer */
    {"+chap", 0, reqchap},	/* Require CHAP authentication from peer */
    {"-chap", 0, nochap},	/* Don't allow CHAP authentication with peer */
    {"-vj", 0, setnovj},	/* disable VJ compression */
    {"-vjccomp", 0, setnovjccomp}, /* disable VJ connection-ID compression */
    {"vj-max-slots", 1, setvjslots}, /* Set maximum VJ header slots */
    {"asyncmap", 1, setasyncmap}, /* set the desired async map */
    {"escape", 1, setescape},	/* set chars to escape on transmission */
    {"connect", 1, setconnector}, /* A program to set up a connection */
    {"disconnect", 1, setdisconnector},	/* program to disconnect serial dev. */
    {"crtscts", 0, setcrtscts},	/* set h/w flow control */
    {"xonxoff", 0, setxonxoff},	/* set s/w flow control */
    {"-crtscts", 0, setxonxoff}, /* another name for xonxoff */
    {"debug", 0, setdebug},	/* Increase debugging level */
    {"kdebug", 1, setkdebug},	/* Enable kernel-level debugging */
    {"domain", 1, setdomain},	/* Add given domain name to hostname*/
    {"mru", 1, setmru},		/* Set MRU value for negotiation */
    {"mtu", 1, setmtu},		/* Set our MTU */
    {"netmask", 1, setnetmask},	/* set netmask */
    {"passive", 0, setpassive},	/* Set passive mode */
    {"silent", 0, setsilent},	/* Set silent mode */
    {"modem", 0, setmodem},	/* Use modem control lines */
    {"local", 0, setlocal},	/* Don't use modem control lines */
    {"lock", 0, setlock},	/* Lock serial device (with lock file) */
    {"name", 1, setname},	/* Set local name for authentication */
    {"user", 1, setuser},	/* Set username for PAP auth with peer */
    {"usehostname", 0, setusehostname},	/* Must use hostname for auth. */
    {"remotename", 1, setremote}, /* Set remote name for authentication */
    {"auth", 0, setauth},	/* Require authentication from peer */
    {"file", 1, readfile},	/* Take options from a file */
    {"defaultroute", 0, setdefaultroute}, /* Add default route */
    {"proxyarp", 0, setproxyarp}, /* Add proxy ARP entry */
    {"persist", 0, setpersist},	/* Keep on reopening connection after close */
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
    {"pap-restart", 1, setpaptimeout}, /* Set timeout for UPAP */
    {"pap-max-authreq", 1, setpapreqs}, /* Set max #xmits for auth-reqs */
    {"chap-restart", 1, setchaptimeout}, /* Set timeout for CHAP */
    {"chap-max-challenge", 1, setchapchal}, /* Set max #xmits for challenge */
    {"chap-interval", 1, setchapintv}, /* Set interval for rechallenge */
    {"ipcp-accept-local", 0, setipcpaccl}, /* Accept peer's address for us */
    {"ipcp-accept-remote", 0, setipcpaccr}, /* Accept peer's address for it */
    {NULL, 0, NULL}
};


#ifndef IMPLEMENTATION
#define IMPLEMENTATION ""
#endif

static char *usage_string = "\
pppd version %s patch level %d%s\n\
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
    int ret;

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
	    if ((ret = setdevname(arg)) == 0
		&& (ret = setspeed(arg)) == 0
		&& (ret = setipaddr(arg)) == 0) {
		fprintf(stderr, "%s: unrecognized command\n", arg);
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
 * usage - print out a message telling how to use the program.
 */
usage()
{
    fprintf(stderr, usage_string, VERSION, PATCHLEVEL, IMPLEMENTATION,
	    progname);
}

/*
 * options_from_file - Read a string of options from a file,
 * and interpret them.
 */
int
options_from_file(filename, must_exist, check_prot)
    char *filename;
    int must_exist;
    int check_prot;
{
    FILE *f;
    int i, newline, ret;
    struct cmd *cmdp;
    char *argv[MAXARGS];
    char args[MAXARGS][MAXWORDLEN];
    char cmd[MAXWORDLEN];

    if ((f = fopen(filename, "r")) == NULL) {
	if (!must_exist && errno == ENOENT)
	    return 1;
	perror(filename);
	return 0;
    }
    if (check_prot && !readable(fileno(f))) {
	fprintf(stderr, "%s: access denied\n", filename);
	fclose(f);
	return 0;
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
	    if ((ret = setdevname(cmd)) == 0
		&& (ret = setspeed(cmd)) == 0
		&& (ret = setipaddr(cmd)) == 0) {
		fprintf(stderr, "In file %s: unrecognized command %s\n",
			filename, cmd);
		fclose(f);
		return 0;
	    }
	    if (ret < 0)	/* error */
		return 0;
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
    ret = options_from_file(path, 0, 1);
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
    char *dev, *path;
    int ret;

    dev = strrchr(devname, '/');
    if (dev == NULL)
	dev = devname;
    else
	++dev;
    if (strcmp(dev, "tty") == 0)
	return 1;		/* don't look for /etc/ppp/options.tty */
    path = malloc(strlen(_PATH_TTYOPT) + strlen(dev) + 1);
    if (path == NULL)
	novm("tty init file name");
    strcpy(path, _PATH_TTYOPT);
    strcat(path, dev);
    ret = options_from_file(path, 0, 0);
    free(path);
    return ret;
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
    return options_from_file(*argv, 1, 1);
}

/*
 * setdebug - Set debug (command line argument).
 */
static int
setdebug()
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
 * setmru - Set the largest MTU we'll use.
 */
static int
setmtu(argv)
    char **argv;
{
    long mtu;

    if (!number_option(*argv, &mtu, 0))
	return 0;
    if (mtu < MINMRU || mtu > MAXMRU) {
	fprintf(stderr, "mtu option value of %d is too %s\n", mtu,
		(mtu < MINMRU? "small": "large"));
	return 0;
    }
    lcp_allowoptions[0].mru = mtu;
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
	return 0;
    }
    if (!readable(fileno(ufile))) {
	fprintf(stderr, "%s: access denied\n", *argv);
	return 0;
    }
    check_access(ufile, *argv);

    /* get username */
    if (fgets(user, MAXNAMELEN - 1, ufile) == NULL
	|| fgets(passwd, MAXSECRETLEN - 1, ufile) == NULL){
	fprintf(stderr, "Unable to read user login data file %s.\n", *argv);
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
 * setnovj - disable vj compression
 */
static int
setnovj()
{
    ipcp_wantoptions[0].neg_vj = 0;
    ipcp_allowoptions[0].neg_vj = 0;
    return (1);
}


/*
 * setnovjccomp - disable VJ connection-ID compression
 */
static int
setnovjccomp()
{
    ipcp_wantoptions[0].cflag = 0;
    ipcp_allowoptions[0].cflag = 0;
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
	fprintf(stderr, "pppd: vj-max-slots value must be between 2 and 16\n");
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
	novm("connector string");

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
	novm("disconnector string");

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


/*
 * setasyncmap - add bits to asyncmap (what we request peer to escape).
 */
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
	    fprintf(stderr, "%s: invalid hex number: %s\n", progname, p);
	    return 0;
	}
	p = endp;
	if (n < 0 || 0x20 <= n && n <= 0x3F || n == 0x5E || n > 0xFF) {
	    fprintf(stderr, "%s: can't escape character 0x%x\n", progname, n);
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
	    return 0;
	syslog(LOG_ERR, cp);
	return -1;
    }

    (void) strncpy(devname, cp, MAXPATHLEN);
    devname[MAXPATHLEN-1] = 0;
    default_device = FALSE;

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
    char *colon, *index();
    u_long local, remote;
    ipcp_options *wo = &ipcp_wantoptions[0];

    /*
     * IP address pair separated by ":".
     */
    if ((colon = index(arg, ':')) == NULL)
	return 0;

    /*
     * If colon first character, then no local addr.
     */
    if (colon != arg) {
	*colon = '\0';
	if ((local = inet_addr(arg)) == -1) {
	    if ((hp = gethostbyname(arg)) == NULL) {
		fprintf(stderr, "unknown host: %s\n", arg);
		return -1;
	    } else {
		local = *(long *)hp->h_addr;
		if (our_name[0] == 0) {
		    strncpy(our_name, arg, MAXNAMELEN);
		    our_name[MAXNAMELEN-1] = 0;
		}
	    }
	}
	if (bad_ip_adrs(local)) {
	    fprintf(stderr, "bad local IP address %s\n", ip_ntoa(local));
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
		fprintf(stderr, "unknown host: %s\n", colon);
		return -1;
	    } else {
		remote = *(long *)hp->h_addr;
		if (remote_name[0] == 0) {
		    strncpy(remote_name, colon, MAXNAMELEN);
		    remote_name[MAXNAMELEN-1] = 0;
		}
	    }
	}
	if (bad_ip_adrs(remote)) {
	    fprintf(stderr, "bad remote IP address %s\n", ip_ntoa(remote));
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
    if (local != 0 && !bad_ip_adrs(local))
	wo->ouraddr = local;
}


/*
 * setnetmask - set the netmask to be used on the interface.
 */
static int
setnetmask(argv)
    char **argv;
{
    struct in_addr mask;

    if ((inet_aton(*argv, &mask) < 0) || (netmask & ~mask.s_addr)) {
	fprintf(stderr, "Invalid netmask %s\n", *argv);
	return (0);
    }

    netmask = mask.s_addr;
    return (1);
}

/*
 * Return user specified netmask. A value of zero means no netmask has
 * been set.
 */
/* ARGSUSED */
u_long
GetMask(addr)
    u_long addr;
{
    return(netmask);
}


static int
setcrtscts()
{
    crtscts = 1;
    return (1);
}

static int
setxonxoff()
{
    crtscts = 2;
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
setlock()
{
    lockflag = 1;
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

static int setlcpterm(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxtermtransmits);
}

static int setlcpconf(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxconfreqtransmits);
}

static int setlcpfails(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxnakloops);
}

static int setipcptimeout(argv)
    char **argv;
{
    return int_option(*argv, &ipcp_fsm[0].timeouttime);
}

static int setipcpterm(argv)
    char **argv;
{
    return int_option(*argv, &ipcp_fsm[0].maxtermtransmits);
}

static int setipcpconf(argv)
    char **argv;
{
    return int_option(*argv, &ipcp_fsm[0].maxconfreqtransmits);
}

static int setipcpfails(argv)
    char **argv;
{
    return int_option(*argv, &lcp_fsm[0].maxnakloops);
}

static int setpaptimeout(argv)
    char **argv;
{
    return int_option(*argv, &upap[0].us_timeouttime);
}

static int setpapreqs(argv)
    char **argv;
{
    return int_option(*argv, &upap[0].us_maxtransmits);
}

static int setchaptimeout(argv)
    char **argv;
{
    return int_option(*argv, &chap[0].timeouttime);
}

static int setchapchal(argv)
    char **argv;
{
    return int_option(*argv, &chap[0].max_transmits);
}

static int setchapintv(argv)
    char **argv;
{
    return int_option(*argv, &chap[0].chal_interval);
}
