 /*
  * Routines for testing only. Not really industrial strength.
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  *
  * $FreeBSD$
  */

#ifndef lint
static char sccs_id[] = "@(#) scaffold.c 1.6 97/03/21 19:27:24";
#endif

/* System libraries. */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <syslog.h>
#include <setjmp.h>
#include <string.h>
#include <resolv.h>

#ifndef INADDR_NONE
#define	INADDR_NONE	(-1)		/* XXX should be 0xffffffff */
#endif

extern char *malloc();

/* Application-specific. */

#include "tcpd.h"
#include "scaffold.h"

 /*
  * These are referenced by the options module and by rfc931.c.
  */
int     allow_severity = SEVERITY;
int     deny_severity = LOG_WARNING;
int     rfc931_timeout = RFC931_TIMEOUT;

/* dup_hostent - create hostent in one memory block */

static struct hostent *dup_hostent(hp)
struct hostent *hp;
{
    struct hostent_block {
	struct hostent host;
	char   *addr_list[1];
    };
    struct hostent_block *hb;
    int     count;
    char   *data;
    char   *addr;

    for (count = 0; hp->h_addr_list[count] != 0; count++)
	 /* void */ ;

    if ((hb = (struct hostent_block *) malloc(sizeof(struct hostent_block)
#ifdef INET6
			 + strlen(hp->h_name) + 1
#endif
			 + (hp->h_length + sizeof(char *)) * count)) == 0) {
	fprintf(stderr, "Sorry, out of memory\n");
	exit(1);
    }
    memset((char *) &hb->host, 0, sizeof(hb->host));
    hb->host.h_length = hp->h_length;
    hb->host.h_addr_list = hb->addr_list;
    hb->host.h_addr_list[count] = 0;
    data = (char *) (hb->host.h_addr_list + count + 1);
#ifdef INET6
    hb->host.h_name = data + hp->h_length * count;
    strcpy(hb->host.h_name, hp->h_name);
    hb->host.h_addrtype = hp->h_addrtype;
#endif

    for (count = 0; (addr = hp->h_addr_list[count]) != 0; count++) {
	hb->host.h_addr_list[count] = data + hp->h_length * count;
	memcpy(hb->host.h_addr_list[count], addr, hp->h_length);
    }
    return (&hb->host);
}

#if defined(INET6) && !defined(USE_GETIPNODEBY)
/* merge_hostent - merge hostent in one memory block */

static struct hostent *merge_hostent(hp1, hp2)
struct hostent *hp1, *hp2;
{
    struct hostent_block {
	struct hostent host;
	char   *addr_list[1];
    };
    struct hostent_block *hb;
    int     count, count2;
    char   *data;
    char   *addr;

    for (count = 0; hp1->h_addr_list[count] != 0; count++)
	 /* void */ ;
    for (count2 = 0; hp2->h_addr_list[count2] != 0; count2++)
	 /* void */ ;
    count += count2;

    if ((hb = (struct hostent_block *) malloc(sizeof(struct hostent_block)
			 + strlen(hp1->h_name) + 1
			 + (hp1->h_length + sizeof(char *)) * count)) == 0) {
	fprintf(stderr, "Sorry, out of memory\n");
	exit(1);
    }
    memset((char *) &hb->host, 0, sizeof(hb->host));
    hb->host.h_length = hp1->h_length;
    hb->host.h_addr_list = hb->addr_list;
    hb->host.h_addr_list[count] = 0;
    data = (char *) (hb->host.h_addr_list + count + 1);
    hb->host.h_name = data + hp1->h_length * count;
    strcpy(hb->host.h_name, hp1->h_name);
    hb->host.h_addrtype = hp1->h_addrtype;

    for (count = 0; (addr = hp1->h_addr_list[count]) != 0; count++) {
	hb->host.h_addr_list[count] = data + hp1->h_length * count;
	memcpy(hb->host.h_addr_list[count], addr, hp1->h_length);
    }
    for (count2 = 0; (addr = hp2->h_addr_list[count2]) != 0; count2++) {
	hb->host.h_addr_list[count] = data + hp1->h_length * count;
	memcpy(hb->host.h_addr_list[count], addr, hp1->h_length);
	++count;
    }
    return (&hb->host);
}
#endif

static struct hostent *gethostbyname64(host)
char *host;
{
    struct hostent *hp = NULL, *hp2 = NULL;
#ifdef USE_GETIPNODEBY
    int h_error;

    if ((hp = getipnodebyname(host, AF_INET6,
			      AI_V4MAPPED | AI_ADDRCONFIG | AI_ALL,
			      &h_error)) != 0) {
	hp2 = dup_hostent(hp);
	freehostent(hp);
	return (hp2);
    }
#else
    struct hostent *hp1;
    u_long res_options;

    if ((_res.options & RES_INIT) == 0) {
	if (res_init() < 0) {
	    tcpd_warn("%s: res_init() failed", host);
	    return (NULL);
	}
    }
    res_options = _res.options;
#ifdef INET6
    _res.options |= RES_USE_INET6;
    if ((hp1 = gethostbyname2(host, AF_INET6)) != NULL)
	hp1 = dup_hostent(hp1);
#endif
    if ((hp2 = gethostbyname2(host, AF_INET)) != NULL)
	hp2 = dup_hostent(hp2);
    _res.options = res_options;
#ifdef INET6
    if (hp1 && hp2) {
	hp = merge_hostent(hp1, hp2);
	free((char *) hp1);
	free((char *) hp2);
	return (hp);
    }
    if (hp1)
	return (hp1);
#endif
    if (hp2)
	return (hp2);
#endif
    return (NULL);
}

/* find_inet_addr - find all addresses for this host, result to free() */

struct hostent *find_inet_addr(host)
char   *host;
{
    struct in_addr addr;
    struct hostent *hp;
    static struct hostent h;
    static char *addr_list[2];
    static char hnamebuf[BUFSIZ];

    /*
     * Host address: translate it to internal form.
     */
    if ((addr.s_addr = dot_quad_addr(host)) != INADDR_NONE) {
	h.h_addr_list = addr_list;
	h.h_addr_list[0] = (char *) &addr;
	h.h_length = sizeof(addr);
#ifdef INET6
	h.h_addrtype = AF_INET;
	h.h_name = hnamebuf;
	strcpy(h.h_name, host);
#endif
	return (dup_hostent(&h));
    }

    /*
     * Map host name to a series of addresses. Watch out for non-internet
     * forms or aliases. The NOT_INADDR() is here in case gethostbyname() has
     * been "enhanced" to accept numeric addresses. Make a copy of the
     * address list so that later gethostbyXXX() calls will not clobber it.
     */
    if (NOT_INADDR(host) == 0) {
	tcpd_warn("%s: not an internet address", host);
	return (0);
    }
#ifdef INET6
    if ((hp = gethostbyname64(host)) == 0) {
#else
    if ((hp = gethostbyname(host)) == 0) {
#endif
	tcpd_warn("%s: host not found", host);
	return (0);
    }
#ifdef INET6
    if (hp->h_addrtype != AF_INET6) {
	tcpd_warn("%d: not an internet host", hp->h_addrtype);
	free((char *) hp);
#else
    if (hp->h_addrtype != AF_INET) {
	tcpd_warn("%d: not an internet host", hp->h_addrtype);
#endif
	return (0);
    }
    if (STR_NE(host, hp->h_name)) {
	tcpd_warn("%s: hostname alias", host);
	tcpd_warn("(official name: %.*s)", STRING_LENGTH, hp->h_name);
    }
#ifdef INET6
    return (hp);
#else
    return (dup_hostent(hp));
#endif
}

/* check_dns - give each address thorough workout, return address count */

int     check_dns(host)
char   *host;
{
    struct request_info request;
#ifdef INET6
    struct sockaddr_storage sin;
    char *ap;
    int alen;
#else
    struct sockaddr_in sin;
#endif
    struct hostent *hp;
    int     count;
    char   *addr;

    if ((hp = find_inet_addr(host)) == 0)
	return (0);
    request_init(&request, RQ_CLIENT_SIN, &sin, 0);
    sock_methods(&request);
    memset((char *) &sin, 0, sizeof(sin));
#ifdef INET6
    sin.ss_family = hp->h_addrtype;
    switch (hp->h_addrtype) {
    case AF_INET:
	ap = (char *)&((struct sockaddr_in *)&sin)->sin_addr;
	alen = sizeof(struct sockaddr_in);
	break;
    case AF_INET6:
	ap = (char *)&((struct sockaddr_in6 *)&sin)->sin6_addr;
	alen = sizeof(struct sockaddr_in6);
	break;
    default:
	return (0);
    }
#else
    sin.sin_family = AF_INET;
#endif

    for (count = 0; (addr = hp->h_addr_list[count]) != 0; count++) {
#ifdef INET6
	memcpy(ap, addr, alen);
#else
	memcpy((char *) &sin.sin_addr, addr, sizeof(sin.sin_addr));
#endif

	/*
	 * Force host name and address conversions. Use the request structure
	 * as a cache. Detect hostname lookup problems. Any name/name or
	 * name/address conflicts will be reported while eval_hostname() does
	 * its job.
	 */
	request_set(&request, RQ_CLIENT_ADDR, "", RQ_CLIENT_NAME, "", 0);
	if (STR_EQ(eval_hostname(request.client), unknown))
	    tcpd_warn("host address %s->name lookup failed",
		      eval_hostaddr(request.client));
    }
    free((char *) hp);
    return (count);
}

/* dummy function to intercept the real shell_cmd() */

/* ARGSUSED */

void    shell_cmd(command)
char   *command;
{
    if (hosts_access_verbose)
	printf("command: %s", command);
}

/* dummy function  to intercept the real clean_exit() */

/* ARGSUSED */

void    clean_exit(request)
struct request_info *request;
{
    exit(0);
}

/* dummy function  to intercept the real rfc931() */

/* ARGSUSED */

void    rfc931(request)
struct request_info *request;
{
    strcpy(request->user, unknown);
}

/* check_path - examine accessibility */

int     check_path(path, st)
char   *path;
struct stat *st;
{
    struct stat stbuf;
    char    buf[BUFSIZ];

    if (stat(path, st) < 0)
	return (-1);
#ifdef notdef
    if (st->st_uid != 0)
	tcpd_warn("%s: not owned by root", path);
    if (st->st_mode & 020)
	tcpd_warn("%s: group writable", path);
#endif
    if (st->st_mode & 002)
	tcpd_warn("%s: world writable", path);
    if (path[0] == '/' && path[1] != 0) {
	strrchr(strcpy(buf, path), '/')[0] = 0;
	(void) check_path(buf[0] ? buf : "/", &stbuf);
    }
    return (0);
}
