 /* Portions taken from the skey distribution on Oct 21 1993 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <pwd.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <skey.h>

#if (MAXHOSTNAMELEN < 64)		/* AIX weirdness */
#undef MAXHOSTNAMELEN
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 255
#endif

#include "pathnames.h"

static int isaddr();
static int rdnets();

#define MAXADDR	16			/* how many addresses can a machine
					 * have? */

 /*
  * Turn host into an IP address and then look it up in the authorization
  * database to determine if ordinary password logins are OK
  */
int     authfile(host)
char   *host;
{
    char   *addr[MAXADDR];
    char  **ap;
    long    n;
    struct hostent *hp;
    char  **lp;
    struct hostent *xp;
    int     addr_length;

    if (strlen(host) == 0) {
	/* Local login, okay */
	return 1;
    }
    if (isaddr(host)) {
	return rdnets(inet_addr(host));
    } else {

	/*
	 * Stash away a copy of the host address list because it will be
	 * clobbered by other gethostbyXXX() calls.
	 */

	hp = gethostbyname(host);
	if (hp == NULL) {
	    syslog(LOG_ERR, "unknown host: %s", host);
	    return 0;
	}
	if (hp->h_addrtype != AF_INET) {
	    syslog(LOG_ERR, "unknown network family: %d", hp->h_addrtype);
	    return 0;
	}
	for (lp = hp->h_addr_list, ap = addr; ap < addr + MAXADDR; lp++, ap++) {
	    if (*lp == NULL) {
		*ap = 0;
		break;
	    } else {
		if ((*ap = malloc(hp->h_length)) == 0) {
		    syslog(LOG_ERR, "out of memory");
		    return 0;
		}
		memcpy(*ap, *lp, hp->h_length);
	    }
	}
	addr_length = hp->h_length;

	/*
	 * See if any of the addresses matches a pattern in the control file.
	 * Report and skip the address if it does not belong to the remote
	 * host. Assume localhost == localhost.domain.
	 */

#define NEQ(x,y) (strcasecmp((x),(y)) != 0)

	while (ap-- > addr) {
	    memcpy((char *) &n, *ap, addr_length);
	    if (rdnets(n)) {
		if ((hp = gethostbyaddr(*ap, addr_length, AF_INET)) == 0
		    || (NEQ(host, hp->h_name) && NEQ(host, "localhost"))) {
		    syslog(LOG_ERR, "IP address %s not registered for host %s",
			   inet_ntoa(*(struct in_addr *) * ap), host);
		    continue;
		}
		return 1;
	    }
	}
	return 0;
    }
}
static int rdnets(host)
unsigned long host;
{
    FILE   *fp;
    char    buf[128],
           *cp;
    long    pattern,
            mask;
    char   *strtok();
    int     permit_it = 0;

    /*
     * If auth file not found, be backwards compatible with standard login
     * and allow hard coded passwords in from anywhere.  Some may consider
     * this a security hole,  but backwards compatibility is more desirable
     * than others.  If you don't like it, change the return value to be zero.
     */
    if ((fp = fopen(_PATH_SKEYACCESS, "r")) == NULL)
	return 1;

    while (fgets(buf, sizeof(buf), fp), !feof(fp)) {
	if (buf[0] == '#')
	    continue;				/* Comment */
	cp = strtok(buf, " \t");
	if (cp == NULL)
	    continue;
	/* two choices permit or deny */
	if (strncasecmp(cp, "permit", 4) == 0) {
	    permit_it = 1;
	} else {
	    if (strncasecmp(cp, "deny", 4) == 0) {
		permit_it = 0;
	    } else {
		continue;			/* ignore this it is not
						 * permit/deny */
	    }
	}
	cp = strtok(NULL, " \t");
	if (cp == NULL)
	    continue;				/* Invalid line */
	pattern = inet_addr(cp);
	cp = strtok(NULL, " \t");
	if (cp == NULL)
	    continue;				/* Invalid line */
	mask = inet_addr(cp);
	if ((host & mask) == pattern) {
	    fclose(fp);
	    return permit_it;
	}
    }
    fclose(fp);
    return 0;
}

 /*
  * Return TRUE if string appears to be an IP address in dotted decimal;
  * return FALSE otherwise (i.e., if string is a domain name)
  */
static int isaddr(s)
register char *s;
{
    char    c;

    if (s == NULL)
	return 1;				/* Can't happen */

    while ((c = *s++) != '\0') {
	if (c != '[' && c != ']' && !isdigit(c) && c != '.')
	    return 0;
    }
    return 1;
}
