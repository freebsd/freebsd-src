 /*
  * Figure out if UNIX passwords are permitted for any combination of user
  * name, group member, terminal port, host_name or network:
  *
  * Programmatic interface: skeyaccess(user, port, host, addr)
  *
  * All arguments are null-terminated strings. Specify a null character pointer
  * where information is not available.
  *
  * When no address information is given this code performs the host (internet)
  * address lookup itself. It rejects addresses that appear to belong to
  * someone else.
  *
  * When compiled with -DPERMIT_CONSOLE always permits UNIX passwords with
  * console logins, no matter what the configuration file says.
  *
  * To build a stand-alone test version, compile with -DTEST and run it off an
  * skey.access file in the current directory:
  *
  * Command-line interface: ./skeyaccess user port [host_or_ip_addr]
  *
  * Errors are reported via syslogd.
  *
  * Author: Wietse Venema, Eindhoven University of Technology.
  *
  * $FreeBSD$
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
static char *line_pointer = NULL;
static char *first_token __P((char *, int, FILE *));
static int line_number;
static void unget_token __P((char *));
static char *get_token __P((void));
static char *need_token __P((void));
static char *need_internet_addr __P((void));

 /*
  * Various forms of token matching.
  */
#define match_host_name(l)	match_token((l)->host_name)
#define match_port(l)		match_token((l)->port)
#define match_user(l)		match_token((l)->user)
struct login_info;
static int match_internet_addr __P((struct login_info *));
static int match_group __P((struct login_info *));
static int match_token __P((char *));
static int is_internet_addr __P((char *));
static struct addrinfo *convert_internet_addr __P((char *));
static struct addrinfo *lookup_internet_addr __P((char *));

#define MAX_ADDR	32
#define PERMIT		1
#define DENY		0

#ifndef CONSOLE
#define CONSOLE		"console"
#endif
#ifndef VTY_PREFIX
#define VTY_PREFIX      "ttyv"
#endif

struct login_info {
    char   *host_name;			/* host name */
    struct addrinfo *internet_addr;	/* addrinfo chain */
    char   *user;			/* user name */
    char   *port;			/* login port */
};

static int _skeyaccess __P((FILE *, struct login_info *));
int skeyaccess __P((char *, char *, char *, char *));

/* skeyaccess - find out if UNIX passwords are permitted */

int     skeyaccess(user, port, host, addr)
char   *user;
char   *port;
char   *host;
char   *addr;
{
    FILE   *fp;
    struct login_info login_info;
    int     result;

    /*
     * Assume no restriction on the use of UNIX passwords when the s/key
     * acces table does not exist.
     */
    if ((fp = fopen(_PATH_SKEYACCESS, "r")) == 0) {
#ifdef TEST
	fprintf(stderr, "No file %s, thus no access control\n", _PATH_SKEYACCESS);
#endif
	return (PERMIT);
    }

    /*
     * Bundle up the arguments in a structure so we won't have to drag around
     * boring long argument lists.
     *
     * Look up the host address when only the name is given. We try to reject
     * addresses that belong to someone else.
     */
    login_info.user = user;
    login_info.port = port;

    if (host != NULL && !is_internet_addr(host)) {
	login_info.host_name = host;
    } else {
	login_info.host_name = NULL;
    }

    if (addr != NULL && is_internet_addr(addr)) {
	login_info.internet_addr = convert_internet_addr(addr);
    } else if (host != NULL) {
	if (is_internet_addr(host)) {
	    login_info.internet_addr = convert_internet_addr(host);
	} else {
	    login_info.internet_addr = lookup_internet_addr(host);
	}
    } else {
	login_info.internet_addr = NULL;
    }

    /*
     * Print what we think the user wants us to do.
     */
#ifdef TEST
    printf("port: %s\n", login_info.port);
    printf("user: %s\n", login_info.user);
    printf("host: %s\n", login_info.host_name ? login_info.host_name : "none");
    printf("addr: ");
    if (login_info.internet_addr == NULL) {
	printf("none\n");
    } else {
	struct addrinfo *res;
	char haddr[NI_MAXHOST];

	for (res = login_info.internet_addr; res; res = res->ai_next) {
	    getnameinfo(res->ai_addr, res->ai_addrlen, haddr, sizeof(haddr),
			NULL, 0, NI_NUMERICHOST | NI_WITHSCOPEID);
	    printf("%s%s", haddr, res->ai_next ? " " : "\n");
	}
    }
#endif
    result = _skeyaccess(fp, &login_info);
    fclose(fp);
    if (login_info.internet_addr)
	freeaddrinfo(login_info.internet_addr);
    return (result);
}

/* _skeyaccess - find out if UNIX passwords are permitted */

static int _skeyaccess(fp, login_info)
FILE   *fp;
struct login_info *login_info;
{
    char    buf[BUFSIZ];
    char   *tok;
    int     match;
    int     permission=DENY;

#ifdef PERMIT_CONSOLE
    if (login_info->port != 0 &&
	(strcmp(login_info->port, CONSOLE) == 0 ||
	 strncmp(login_info->port, VTY_PREFIX, sizeof(VTY_PREFIX) - 1) == 0
	)
       )
	return (1);
#endif

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
    return (match ? permission : DENY);
}

/* translate IPv4 mapped IPv6 address to IPv4 address */

static void
ai_unmapped(struct addrinfo *ai)
{
    struct sockaddr_in6 *sin6;
    struct sockaddr_in *sin4;
    u_int32_t addr;
    int port;

    if (ai->ai_family != AF_INET6)
	return;
    sin6 = (struct sockaddr_in6 *)ai->ai_addr;
    if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
	return;
    sin4 = (struct sockaddr_in *)ai->ai_addr;
    addr = *(u_int32_t *)&sin6->sin6_addr.s6_addr[12];
    port = sin6->sin6_port;
    memset(sin4, 0, sizeof(struct sockaddr_in));
    sin4->sin_addr.s_addr = addr;
    sin4->sin_port = port;
    sin4->sin_family = AF_INET;
    sin4->sin_len = sizeof(struct sockaddr_in);
    ai->ai_family = AF_INET;
    ai->ai_addrlen = sizeof(struct sockaddr_in);
}

/* match_internet_addr - match internet network address */

static int match_internet_addr(login_info)
struct login_info *login_info;
{
    char *tok;
    struct addrinfo *res;
    struct sockaddr_storage pattern, mask;
    struct sockaddr_in *addr4, *pattern4, *mask4;
    struct sockaddr_in6 *addr6, *pattern6, *mask6;
    int i, match;

    if (login_info->internet_addr == NULL)
	return (0);
    if ((tok = need_internet_addr()) == 0)
	return (0);
    if ((res = convert_internet_addr(tok)) == NULL)
	return (0);
    memcpy(&pattern, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if ((tok = need_internet_addr()) == 0)
	return (0);
    if ((res = convert_internet_addr(tok)) == NULL)
	return (0);
    memcpy(&mask, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (pattern.ss_family != mask.ss_family)
	return (0);
    mask4 = (struct sockaddr_in *)&mask;
    pattern4 = (struct sockaddr_in *)&pattern;
    mask6 = (struct sockaddr_in6 *)&mask;
    pattern6 = (struct sockaddr_in6 *)&pattern;

    /*
     * See if any of the addresses matches a pattern in the control file. We
     * have already tried to drop addresses that belong to someone else.
     */

    for (res = login_info->internet_addr; res; res = res->ai_next) {
	ai_unmapped(res);
	if (res->ai_family != pattern.ss_family)
	    continue;
	switch (res->ai_family) {
	case AF_INET:
	    addr4 = (struct sockaddr_in *)res->ai_addr;
	    if (addr4->sin_addr.s_addr != INADDR_NONE &&
		(addr4->sin_addr.s_addr & mask4->sin_addr.s_addr) == pattern4->sin_addr.s_addr)
		return (1);
	    break;
	case AF_INET6:
	    addr6 = (struct sockaddr_in6 *)res->ai_addr;
	    if (pattern6->sin6_scope_id != 0 &&
		addr6->sin6_scope_id != pattern6->sin6_scope_id)
		break;
	    match = 1;
	    for (i = 0; i < 16; ++i) {
		if ((addr6->sin6_addr.s6_addr[i] & mask6->sin6_addr.s6_addr[i]) != pattern6->sin6_addr.s6_addr[i]) {
		    match = 0;
		    break;
		}
	    }
	    if (match)
		return (1);
	    break;
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
	line_pointer = buf;
	while ((cp = strsep(&line_pointer, " \t")) != NULL && *cp == '\0')
		;
	if (cp != NULL)
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

    if ( (cp = prev_token) ) {
	prev_token = 0;
    } else {
	while ((cp = strsep(&line_pointer, " \t")) != NULL && *cp == '\0')
		;
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
    struct addrinfo *res;

    if ((res = convert_internet_addr(str)) != NULL)
	freeaddrinfo(res);
    return (res != NULL);
}

/*
 * Nuke addrinfo entry from list.
 * XXX: Depending on the implementation of KAME's getaddrinfo(3).
 */
static void nuke_ai(aip)
struct addrinfo **aip;
{
    struct addrinfo *ai;

    ai = *aip;
    *aip = ai->ai_next;
    if (ai->ai_canonname) {
	if (ai->ai_next && !ai->ai_next->ai_canonname)
	    ai->ai_next->ai_canonname = ai->ai_canonname;
	else
	    free(ai->ai_canonname);
    }
    free(ai);
}

/* lookup_internet_addr - look up internet addresses with extreme prejudice */

static struct addrinfo *lookup_internet_addr(host)
char   *host;
{
    struct addrinfo hints, *res0, *res, **resp;
    char hname[NI_MAXHOST], haddr[NI_MAXHOST];
    int error;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_CANONNAME;
    if (getaddrinfo(host, NULL, &hints, &res0) != 0)
	return (NULL);
    if (res0->ai_canonname == NULL) {
	freeaddrinfo(res0);
	return (NULL);
    }

    /*
     * Wipe addresses that appear to belong to someone else. We will get
     * false alarms when when the hostname comes from DNS, while its
     * addresses are listed under different names in local databases.
     */
#define NEQ(x,y)	(strcasecmp((x),(y)) != 0)
#define NEQ3(x,y,n)	(strncasecmp((x),(y), (n)) != 0)

    resp = &res0;
    while ((res = *resp) != NULL) {
	if (res->ai_family != AF_INET && res->ai_family != AF_INET6) {
	    nuke_ai(resp);
	    continue;
	}
	error = getnameinfo(res->ai_addr, res->ai_addrlen,
			    hname, sizeof(hname),
			    NULL, 0, NI_NAMEREQD | NI_WITHSCOPEID);
	if (error) {
	    getnameinfo(res->ai_addr, res->ai_addrlen, haddr, sizeof(haddr),
			NULL, 0, NI_NUMERICHOST | NI_WITHSCOPEID);
	    syslog(LOG_ERR, "address %s not registered for host %s",
		   haddr, res0->ai_canonname);
	    nuke_ai(resp);
	    continue;
	}
	if (NEQ(res0->ai_canonname, hname) &&
	    NEQ3(res0->ai_canonname, "localhost.", 10)) {
	    getnameinfo(res->ai_addr, res->ai_addrlen, haddr, sizeof(haddr),
			NULL, 0, NI_NUMERICHOST | NI_WITHSCOPEID);
	    syslog(LOG_ERR, "address %s registered for host %s and %s",
		   haddr, hname, res0->ai_canonname);
	    nuke_ai(resp);
	    continue;
	}
	resp = &res->ai_next;
    }
    return (res0);
}

/* convert_internet_addr - convert string to internet address */

static struct addrinfo *convert_internet_addr(string)
char   *string;
{
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
    if (getaddrinfo(string, NULL, &hints, &res) != 0)
	return (NULL);
    return (res);
}

#ifdef TEST

main(argc, argv)
int     argc;
char  **argv;
{
    struct addrinfo hints, *res;
    char    host[MAXHOSTNAMELEN + 1];
    int     verdict;
    char   *user;
    char   *port;

    if (argc != 3 && argc != 4) {
	fprintf(stderr, "usage: %s user port [host_or_ip_address]\n", argv[0]);
	exit(0);
    }
    if (_PATH_SKEYACCESS[0] != '/')
	printf("Warning: this program uses control file: %s\n", _PATH_SKEYACCESS);
    openlog("login", LOG_PID, LOG_AUTH);

    user = argv[1];
    port = argv[2];
    if (argv[3]) {
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_CANONNAME;
	if (getaddrinfo(argv[3], NULL, &hints, &res) == 0) {
	    if (res->ai_canonname == NULL)
		strncpy(host, argv[3], MAXHOSTNAMELEN);
	    else
		strncpy(host, res->ai_canonname, MAXHOSTNAMELEN);
	    freeaddrinfo(res);
	} else
	    strncpy(host, argv[3], MAXHOSTNAMELEN);
	host[MAXHOSTNAMELEN] = 0;
    }
    verdict = skeyaccess(user, port, argv[3] ? host : (char *) 0, (char *) 0);
    printf("UNIX passwords %spermitted\n", verdict ? "" : "NOT ");
    return (0);
}

#endif
