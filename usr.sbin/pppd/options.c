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
static char rcsid[] = "$Id: options.c,v 1.7 1995/10/31 21:29:25 peter Exp $";
#endif

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <termios.h>
#include <syslog.h>
#include <string.h>
#include <netdb.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include "pppd.h"
#include "pathnames.h"
#include "patchlevel.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "upap.h"
#include "chap.h"
#include "ccp.h"

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
int	debug = 0;		/* Debug flag */
int	kdebugflag = 0;		/* Tell kernel to print debug messages */
int	default_device = 1;	/* Using /dev/tty or equivalent */
char	devnam[MAXPATHLEN] = "/dev/tty";	/* Device name */
int	crtscts = 0;		/* Use hardware flow control */
int	modem = 1;		/* Use modem control lines */
int	inspeed = 0;		/* Input/Output speed requested */
u_int32_t netmask = 0;		/* IP netmask to set on interface */
int	lockflag = 0;		/* Create lock file to lock the serial dev */
int	nodetach = 0;		/* Don't detach from controlling tty */
char	*connector = NULL;	/* Script to establish physical link */
char	*disconnector = NULL;	/* Script to disestablish physical link */
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
int	usehostname = 0;	/* Use hostname for our_name */
int	disable_defaultip = 0;	/* Don't use hostname for default IP adrs */
char	*ipparam = NULL;	/* Extra parameter for ip up/down scripts */
int	cryptpap;		/* Passwords in pap-secrets are encrypted */

#ifdef _linux_
int idle_time_limit = 0;
static int setidle __P((char **));
#endif

/*
 * Prototypes
 */
static int setdebug __P((void));
static int setkdebug __P((char **));
static int setpassive __P((void));
static int setsilent __P((void));
static int noopt __P((void));
static int setnovj __P((void));
static int setnovjccomp __P((void));
static int setvjslots __P((char **));
static int reqpap __P((void));
static int nopap __P((void));
static int setupapfile __P((char **));
static int nochap __P((void));
static int reqchap __P((void));
static int setspeed __P((char *));
static int noaccomp __P((void));
static int noasyncmap __P((void));
static int noipaddr __P((void));
static int nomagicnumber __P((void));
static int setasyncmap __P((char **));
static int setescape __P((char **));
static int setmru __P((char **));
static int setmtu __P((char **));
static int nomru __P((void));
static int nopcomp __P((void));
static int setconnector __P((char **));
static int setdisconnector __P((char **));
static int setdomain __P((char **));
static int setnetmask __P((char **));
static int setcrtscts __P((void));
static int setnocrtscts __P((void));
static int setxonxoff __P((void));
static int setnodetach __P((void));
static int setmodem __P((void));
static int setlocal __P((void));
static int setlock __P((void));
static int setname __P((char **));
static int setuser __P((char **));
static int setremote __P((char **));
static int setauth __P((void));
static int readfile __P((char **));
static int setdefaultroute __P((void));
static int setnodefaultroute __P((void));
static int setproxyarp __P((void));
static int setnoproxyarp __P((void));
static int setpersist __P((void));
static int setdologin __P((void));
static int setusehostname __P((void));
static int setnoipdflt __P((void));
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
static int setipcpaccl __P((void));
static int setipcpaccr __P((void));
static int setlcpechointv __P((char **));
static int setlcpechofails __P((char **));
static int setbsdcomp __P((char **));
static int setnobsdcomp __P((void));
static int setipparam __P((char **));
static int setpapcrypt __P((void));

static int number_option __P((char *, u_int32_t *, int));
static int readable __P((int fd));

void usage();

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
    {"-crtscts", 0, setnocrtscts}, /* clear h/w flow control */
    {"xonxoff", 0, setxonxoff},	/* set s/w flow control */
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
    {"-defaultroute", 0, setnodefaultroute}, /* disable defaultroute option */
    {"proxyarp", 0, setproxyarp}, /* Add proxy ARP entry */
    {"-proxyarp", 0, setnoproxyarp}, /* disable proxyarp option */
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
    {"pap-restart", 1, setpaptimeout},	/* Set retransmit timeout for PAP */
    {"pap-max-authreq", 1, setpapreqs}, /* Set max #xmits for auth-reqs */
    {"pap-timeout", 1, setpapreqtime},	/* Set time limit for peer PAP auth. */
    {"chap-restart", 1, setchaptimeout}, /* Set timeout for CHAP */
    {"chap-max-challenge", 1, setchapchal}, /* Set max #xmits for challenge */
    {"chap-interval", 1, setchapintv}, /* Set interval for rechallenge */
    {"ipcp-accept-local", 0, setipcpaccl}, /* Accept peer's address for us */
    {"ipcp-accept-remote", 0, setipcpaccr}, /* Accept peer's address for it */
    {"bsdcomp", 1, setbsdcomp},		/* request BSD-Compress */
    {"-bsdcomp", 0, setnobsdcomp},	/* don't allow BSD-Compress */
    {"ipparam", 1, setipparam},		/* set ip script parameter */
    {"papcrypt", 0, setpapcrypt},	/* PAP passwords encrypted */
#ifdef _linux_
    {"idle-disconnect", 1, setidle}, /* seconds for disconnect of idle IP */
#endif
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
void
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

    dev = strrchr(devnam, '/');
    if (dev == NULL)
	dev = devnam;
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
	    perror(filename);
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
	fprintf(stderr, "%s: warning: word in file %s too long (%.20s...)\n",
		progname, filename, word);
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
    u_int32_t v;

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
	fprintf(stderr, "mtu option value of %ld is too %s\n", mtu,
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
    return 1;
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
  
    (void) strncpy(devnam, cp, MAXPATHLEN);
    devnam[MAXPATHLEN-1] = 0;
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
		fprintf(stderr, "unknown host: %s\n", arg);
		return -1;
	    } else {
		local = *(u_int32_t *)hp->h_addr;
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
		remote = *(u_int32_t *)hp->h_addr;
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
    u_int32_t local;
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
    local = *(u_int32_t *)hp->h_addr;
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

    if ((inet_aton(*argv, &mask)) == -1 || (netmask & ~mask.s_addr)) {
	fprintf(stderr, "Invalid netmask %s\n", *argv);
	return (0);
    }

    netmask = mask.s_addr;
    return (1);
}

static int
setcrtscts()
{
    crtscts = 1;
    return (1);
}

static int
setnocrtscts()
{
    crtscts = -1;
    return (1);
}

static int
setxonxoff()
{
    lcp_wantoptions[0].asyncmap |= 0x000A0000;	/* escape ^S and ^Q */
    lcp_wantoptions[0].neg_asyncmap = 1;

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
    if (!ipcp_allowoptions[0].default_route) {
	fprintf(stderr, "%s: defaultroute option is disabled\n", progname);
	return 0;
    }
    ipcp_wantoptions[0].default_route = 1;
    return 1;
}

static int
setnodefaultroute()
{
    ipcp_allowoptions[0].default_route = 0;
    ipcp_wantoptions[0].default_route = 0;
    return 1;
}

static int
setproxyarp()
{
    if (!ipcp_allowoptions[0].proxy_arp) {
	fprintf(stderr, "%s: proxyarp option is disabled\n", progname);
	return 0;
    }
    ipcp_wantoptions[0].proxy_arp = 1;
    return 1;
}

static int
setnoproxyarp()
{
    ipcp_wantoptions[0].proxy_arp = 0;
    ipcp_allowoptions[0].proxy_arp = 0;
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
	fprintf(stderr, "%s: invalid argument format for bsdcomp option\n",
		progname);
	return 0;
    }
    if (rbits != 0 && (rbits < BSD_MIN_BITS || rbits > BSD_MAX_BITS)
	|| abits != 0 && (abits < BSD_MIN_BITS || abits > BSD_MAX_BITS)) {
	fprintf(stderr, "%s: bsdcomp option values must be 0 or %d .. %d\n",
		progname, BSD_MIN_BITS, BSD_MAX_BITS);
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
setnobsdcomp()
{
    ccp_wantoptions[0].bsd_compress = 0;
    ccp_allowoptions[0].bsd_compress = 0;
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
setpapcrypt()
{
    cryptpap = 1;
    return 1;
}

#ifdef _linux_
static int setidle (argv)
    char **argv;
{
    return int_option(*argv, &idle_time_limit);
}
#endif
