/*-
 * Copyright (c) 1985, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_init.c	6.15 (Berkeley) 2/24/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Resolver state default settings
 */

struct state _res = {
	RES_TIMEOUT,               	/* retransmition time interval */
	4,                         	/* number of times to retransmit */
	RES_DEFAULT,			/* options flags */
	1,                         	/* number of name servers */
};

/*
 * Set up default settings.  If the configuration file exist, the values
 * there will have precedence.  Otherwise, the server address is set to
 * INADDR_ANY and the default domain name comes from the gethostname().
 *
 * The configuration file should only be used if you want to redefine your
 * domain or run without a server on your machine.
 *
 * Return 0 if completes successfully, -1 on error
 */
res_init()
{
	register FILE *fp;
	register char *cp, **pp;
	register int n;
	char buf[BUFSIZ];
	int nserv = 0;    /* number of nameserver records read from file */
	int haveenv = 0;
	int havesearch = 0;

	_res.nsaddr.sin_addr.s_addr = INADDR_ANY;
	_res.nsaddr.sin_family = AF_INET;
	_res.nsaddr.sin_port = htons(NAMESERVER_PORT);
	_res.nscount = 1;

	/* Allow user to override the local domain definition */
	if ((cp = getenv("LOCALDOMAIN")) != NULL) {
		(void)strncpy(_res.defdname, cp, sizeof(_res.defdname));
		haveenv++;
	}

	if ((fp = fopen(_PATH_RESCONF, "r")) != NULL) {
	    /* read the config file */
	    while (fgets(buf, sizeof(buf), fp) != NULL) {
		/* read default domain name */
		if (!strncmp(buf, "domain", sizeof("domain") - 1)) {
		    if (haveenv)	/* skip if have from environ */
			    continue;
		    cp = buf + sizeof("domain") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    (void)strncpy(_res.defdname, cp, sizeof(_res.defdname) - 1);
		    if ((cp = index(_res.defdname, '\n')) != NULL)
			    *cp = '\0';
		    havesearch = 0;
		    continue;
		}
		/* set search list */
		if (!strncmp(buf, "search", sizeof("search") - 1)) {
		    if (haveenv)	/* skip if have from environ */
			    continue;
		    cp = buf + sizeof("search") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    (void)strncpy(_res.defdname, cp, sizeof(_res.defdname) - 1);
		    if ((cp = index(_res.defdname, '\n')) != NULL)
			    *cp = '\0';
		    /*
		     * Set search list to be blank-separated strings
		     * on rest of line.
		     */
		    cp = _res.defdname;
		    pp = _res.dnsrch;
		    *pp++ = cp;
		    for (n = 0; *cp && pp < _res.dnsrch + MAXDNSRCH; cp++) {
			    if (*cp == ' ' || *cp == '\t') {
				    *cp = 0;
				    n = 1;
			    } else if (n) {
				    *pp++ = cp;
				    n = 0;
			    }
		    }
		    /* null terminate last domain if there are excess */
		    while (*cp != '\0' && *cp != ' ' && *cp != '\t')
			    cp++;
		    *cp = '\0';
		    *pp++ = 0;
		    havesearch = 1;
		    continue;
		}
		/* read nameservers to query */
		if (!strncmp(buf, "nameserver", sizeof("nameserver") - 1) &&
		   nserv < MAXNS) {
		    cp = buf + sizeof("nameserver") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    if ((_res.nsaddr_list[nserv].sin_addr.s_addr =
			inet_addr(cp)) == (unsigned)-1) {
			    _res.nsaddr_list[nserv].sin_addr.s_addr
				= INADDR_ANY;
			    continue;
		    }
		    _res.nsaddr_list[nserv].sin_family = AF_INET;
		    _res.nsaddr_list[nserv].sin_port = htons(NAMESERVER_PORT);
		    nserv++;
		    continue;
		}
	    }
	    if (nserv > 1) 
		_res.nscount = nserv;
	    (void) fclose(fp);
	}
	if (_res.defdname[0] == 0) {
		if (gethostname(buf, sizeof(_res.defdname)) == 0 &&
		   (cp = index(buf, '.')))
			(void)strcpy(_res.defdname, cp + 1);
	}

	/* find components of local domain that might be searched */
	if (havesearch == 0) {
		pp = _res.dnsrch;
		*pp++ = _res.defdname;
		for (cp = _res.defdname, n = 0; *cp; cp++)
			if (*cp == '.')
				n++;
		cp = _res.defdname;
		for (; n >= LOCALDOMAINPARTS && pp < _res.dnsrch + MAXDFLSRCH;
		    n--) {
			cp = index(cp, '.');
			*pp++ = ++cp;
		}
		*pp++ = 0;
	}
	_res.options |= RES_INIT;
	return (0);
}
