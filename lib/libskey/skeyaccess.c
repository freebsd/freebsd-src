 /*
  * Figure out if UNIX passwords are permitted for any combination of user
  * name, group member, terminal port, host_name or network:
  * 
  * Programmatic interface: skeyaccess(char *user, char *port, char *host)
  * 
  * Specify a null character pointer where information is not available.
  * 
  * When compiled with -DPERMIT_CONSOLE always permits UNIX passwords with
  * console logins, no matter what the configuration file says.
  * 
  * To build a stand-alone test version, compile with -DTEST and run it off an
  * skey.access file in the current directory:
  * 
  * Command-line interface: ./skeyaccess user port [host]
  * 
  * Author: Wietse Venema, Eindhoven University of Technology.
  */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <grp.h>
#include <ctype.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>

#include "pathnames.h"

 /*
  * Token input with one-deep pushback.
  */
static char *prev_token = 0;		/* push-back buffer */
static char *first_token();
static int line_number;
static void unget_token();
static char *get_token();
static char *need_token();
static char *need_internet_addr();

 /*
  * Various forms of token matching.
  */
#define match_host_name(l)	match_token((l)->host_name)
#define match_port(l)		match_token((l)->port)
#define match_user(l)		match_token((l)->user)
static int match_internet_addr();
static int match_group();
static int match_token();
static int is_internet_addr();

#define MAX_ADDR	32
#define PERMIT		1
#define DENY		0

struct login_info {
    char   *host_name;			/* host name */
    struct in_addr *internet_addr;	/* null terminated list */
    char   *user;			/* user name */
    char   *port;			/* login port */
};

/* skeyaccess - find out if UNIX passwords are permitted */

int     skeyaccess(user, port, host)
char   *user;
char   *port;
char   *host;
{
    struct hostent *hp;
    struct login_info login_info;
    struct in_addr internet_addr[MAX_ADDR + 1];
    char    hostname_buf[MAXHOSTNAMELEN + 1];
    int     i;

    login_info.user = user;
    login_info.port = port;
    login_info.host_name = 0;
    login_info.internet_addr = 0;

    if (host) {
	if (is_internet_addr(host)) {		/* not DECnet */
	    internet_addr[0].s_addr = inet_addr(host);
	    internet_addr[1].s_addr = 0;
	    login_info.internet_addr = internet_addr;
	} else {
	    if ((hp = gethostbyname(host)) != 0 && hp->h_addrtype == AF_INET) {
		for (i = 0; i < MAX_ADDR && hp->h_addr_list[i]; i++)
		    memcpy((char *) &internet_addr[i],
			   hp->h_addr_list[i], hp->h_length);
		internet_addr[i].s_addr = 0;
		login_info.internet_addr = internet_addr;
		host = hp->h_name;
	    }
	    strncpy(hostname_buf, host, MAXHOSTNAMELEN);
	    hostname_buf[MAXHOSTNAMELEN] = 0;
	    login_info.host_name = hostname_buf;
	}
    }

    /*
     * Print what we think the user wants us to do.
     */
#ifdef TEST
    printf("port: %s\n", login_info.port);
    printf("user: %s\n", login_info.user);
    printf("host: %s\n", login_info.host_name ? login_info.host_name : "none");
    printf("addr: ");
    if (login_info.internet_addr == 0) {
	printf("none\n");
    } else {
	for (i = 0; login_info.internet_addr[i].s_addr; i++)
	    printf("%s%s", inet_ntoa(login_info.internet_addr[i]),
		   login_info.internet_addr[i + 1].s_addr ? " " : "\n");
    }
#endif
    return (_skeyaccess(&login_info));
}

/* _skeyaccess - find out if UNIX passwords are permitted */

int     _skeyaccess(login_info)
struct login_info *login_info;
{
    FILE   *fp;
    char    buf[BUFSIZ];
    char   *tok;
    int     match;
    int     permission;

#ifdef PERMIT_CONSOLE
    if (login_info->port != 0 && strcasecmp(login_info->port, "console") == 0)
	return (1);
#endif

    /*
     * Assume no restriction on the use of UNIX passwords when the s/key
     * acces table does not exist.
     */
    if ((fp = fopen(_PATH_SKEYACCESS, "r")) == 0)
	return (PERMIT);

    /*
     * Scan the s/key access table until we find an entry that matches. If no
     * match is found, assume that UNIX passwords are disallowed.
     */
    match = 0;
    while (match == 0 && (tok = first_token(buf, sizeof(buf), fp))) {
	if (strncasecmp(tok, "permit", 4) == 0) {
	    permission = PERMIT;
	} else if (strncasecmp(tok, "deny", 4) == 0) {
	    permission = DENY;
	} else {
	    syslog(LOG_ERR, "%s: line %d: bad permission: %s",
		   _PATH_SKEYACCESS, line_number, tok);
	    continue;				/* error */
	}

	/*
	 * Process all conditions in this entry until we find one that fails.
	 */
	match = 1;
	while (match != 0 && (tok = get_token())) {
	    if (strcasecmp(tok, "hostname") == 0) {
		match = match_host_name(login_info);
	    } else if (strcasecmp(tok, "port") == 0) {
		match = match_port(login_info);
	    } else if (strcasecmp(tok, "user") == 0) {
		match = match_user(login_info);
	    } else if (strcasecmp(tok, "group") == 0) {
		match = match_group(login_info);
	    } else if (strcasecmp(tok, "internet") == 0) {
		match = match_internet_addr(login_info);
	    } else if (is_internet_addr(tok)) {
		unget_token(tok);
		match = match_internet_addr(login_info);
	    } else {
		syslog(LOG_ERR, "%s: line %d: bad condition: %s",
		       _PATH_SKEYACCESS, line_number, tok);
		match = 0;
	    }
	}
    }
    fclose(fp);
    return (match ? permission : DENY);
}

/* match_internet_addr - match internet network address */

static int match_internet_addr(login_info)
struct login_info *login_info;
{
    char   *tok;
    long    pattern;
    long    mask;
    struct in_addr *addrp;
    struct hostent *hp;

    if (login_info->internet_addr == 0)
	return (0);
    if ((tok = need_internet_addr()) == 0)
	return (0);
    pattern = inet_addr(tok);
    if ((tok = need_internet_addr()) == 0)
	return (0);
    mask = inet_addr(tok);

    /*
     * See if any of the addresses matches a pattern in the control file.
     * Report and skip the address if it does not belong to the remote host.
     * Assume localhost == localhost.domain.
     */

#define NEQ(x,y) (strcasecmp((x),(y)) != 0)

    for (addrp = login_info->internet_addr; addrp->s_addr; addrp++) {
	if ((addrp->s_addr & mask) == pattern) {
	    if (login_info->host_name != 0 &&
		((hp = gethostbyaddr((char *) addrp, sizeof(*addrp), AF_INET)) == 0
		 || (NEQ(login_info->host_name, hp->h_name)
		     && NEQ(login_info->host_name, "localhost")))) {
		syslog(LOG_ERR, "address %s not registered for host %s",
		       inet_ntoa(*addrp), login_info->host_name);
		continue;
	    }
	    return (1);
	}
    }
    return (0);
}

/* match_group - match username against group */

static int match_group(login_info)
struct login_info *login_info;
{
    struct group *group;
    char   *tok;
    char  **memp;

    if ((tok = need_token()) && (group = getgrnam(tok))) {
	for (memp = group->gr_mem; *memp; memp++)
	    if (strcmp(login_info->user, *memp) == 0)
		return (1);
    }
    return (0);					/* XXX endgrent() */
}

/* match_token - get and match token */

static int match_token(str)
char   *str;
{
    char   *tok;

    return (str && (tok = need_token()) && strcasecmp(str, tok) == 0);
}

/* first_token - read line and return first token */

static char *first_token(buf, len, fp)
char   *buf;
int     len;
FILE   *fp;
{
    char   *cp;

    prev_token = 0;
    for (;;) {
	if (fgets(buf, len, fp) == 0)
	    return (0);
	line_number++;
	buf[strcspn(buf, "\r\n#")] = 0;
#ifdef TEST
	if (buf[0])
	    printf("rule: %s\n", buf);
#endif
	if (cp = strtok(buf, " \t"))
	    return (cp);
    }
}

/* unget_token - push back last token */

static void unget_token(cp)
char   *cp;
{
    prev_token = cp;
}

/* get_token - retrieve next token from buffer */

static char *get_token()
{
    char   *cp;

    if (cp = prev_token) {
	prev_token = 0;
    } else {
	cp = strtok((char *) 0, " \t");
    }
    return (cp);
}

/* need_token - complain if next token is not available */

static char *need_token()
{
    char   *cp;

    if ((cp = get_token()) == 0)
	syslog(LOG_ERR, "%s: line %d: premature end of rule",
	       _PATH_SKEYACCESS, line_number);
    return (cp);
}

/* need_internet_addr - complain if next token is not an internet address */

static char *need_internet_addr()
{
    char   *cp;

    if ((cp = get_token()) == 0) {
	syslog(LOG_ERR, "%s: line %d: internet address expected",
	       _PATH_SKEYACCESS, line_number);
	return (0);
    } else if (!is_internet_addr(cp)) {
	syslog(LOG_ERR, "%s: line %d: bad internet address: %s",
	       _PATH_SKEYACCESS, line_number, cp);
	return (0);
    } else {
	return (cp);
    }
}

/* is_internet_addr - determine if string is a dotted quad decimal address */

static int is_internet_addr(str)
char   *str;
{
    int     in_run = 0;
    int     runs = 0;

    /* Count the number of runs of characters between the dots. */

    while (*str) {
	if (*str == '.') {
	    in_run = 0;
	} else {
	    if (!isdigit(*str))
		return (0);
	    if (in_run == 0) {
		in_run = 1;
		runs++;
	    }
	}
	str++;
    }
    return (runs == 4);
}

#ifdef TEST

main(argc, argv)
int     argc;
char  **argv;
{
    if (argc != 3 && argc != 4) {
	fprintf(stderr, "usage: %s user port [host_or_ip_address]\n", argv[0]);
	exit(0);
    }
    openlog("login", LOG_PID, LOG_AUTH);
    printf("%s\n", skeyaccess(argv[1], argv[2], argv[3]) ? "YES" : "NO");
    return (0);
}

#endif
