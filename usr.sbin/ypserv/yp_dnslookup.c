/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>. All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: yp_dnslookup.c,v 1.2 1996/05/01 02:37:46 wpaul Exp $
 */

/*
 * Do standard and reverse DNS lookups using the resolver library.
 * Take care of all the dirty work here so the main program only has to
 * pass us a pointer to an array of characters.
 *
 * We have to use direct resolver calls here otherwise the YP server
 * could end up looping by calling itself over and over again until
 * it disappeared up its own belly button.
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "yp_extern.h"

#ifndef lint
static const char rcsid[] = "$Id: yp_dnslookup.c,v 1.2 1996/05/01 02:37:46 wpaul Exp $";
#endif

extern struct hostent *_gethostbydnsname __P(( char * ));
extern struct hostent *_gethostbydnsaddr __P(( const char *, int, int ));

static char *parse(hp)
	struct hostent *hp;
{
	static char result[MAXHOSTNAMELEN * 2];
	int len,i;
	struct in_addr addr;

	len = 16 + strlen(hp->h_name);
	for (i = 0; hp->h_aliases[i]; i++)
		len += strlen(hp->h_aliases[i]) + 1;

	bzero(result, sizeof(result));

	bcopy(hp->h_addr, &addr, sizeof(struct in_addr));
	snprintf(result, sizeof(result), "%s %s", inet_ntoa(addr), hp->h_name);

	for (i = 0; hp->h_aliases[i]; i++) {
		strcat(result, " ");
		strcat(result, hp->h_aliases[i]);
	}

	return ((char *)&result);
}

char *yp_dnsname(address)
	char *address;
{
	struct hostent *hp;

	if (strchr(address, '@'))
		return (NULL);
	if ((hp = (struct hostent *)_gethostbydnsname(address)) == NULL)
		return (NULL);

	return(parse(hp));
}

char *yp_dnsaddr(address)
	const char *address;
{
	struct hostent *hp;
	struct in_addr addr;

	if (strchr(address, '@'))
		return (NULL);
	if (!inet_aton(address, &addr))
		return (NULL);
	if ((hp = (struct hostent *)_gethostbydnsaddr((const char *)&addr,
	     sizeof(unsigned long), AF_INET)) == NULL)
		return (NULL);

	return(parse(hp));
}
