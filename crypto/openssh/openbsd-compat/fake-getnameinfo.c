/*
 * fake library for ssh
 *
 * This file includes getnameinfo().
 * These funtions are defined in rfc2133.
 *
 * But these functions are not implemented correctly. The minimum subset
 * is implemented for ssh use only. For exapmle, this routine assumes
 * that ai_family is AF_INET. Don't use it for another purpose.
 */

#include "includes.h"
#include "ssh.h"

RCSID("$Id: fake-getnameinfo.c,v 1.2 2001/02/09 01:55:36 djm Exp $");

#ifndef HAVE_GETNAMEINFO
int getnameinfo(const struct sockaddr *sa, size_t salen, char *host, 
                size_t hostlen, char *serv, size_t servlen, int flags)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)sa;
	struct hostent *hp;
	char tmpserv[16];

	if (serv) {
		snprintf(tmpserv, sizeof(tmpserv), "%d", ntohs(sin->sin_port));
		if (strlen(tmpserv) >= servlen)
			return EAI_MEMORY;
		else
			strcpy(serv, tmpserv);
	}

	if (host) {
		if (flags & NI_NUMERICHOST) {
			if (strlen(inet_ntoa(sin->sin_addr)) >= hostlen)
				return EAI_MEMORY;

			strcpy(host, inet_ntoa(sin->sin_addr));
			return 0;
		} else {
			hp = gethostbyaddr((char *)&sin->sin_addr, 
				sizeof(struct in_addr), AF_INET);
			if (hp == NULL)
				return EAI_NODATA;
			
			if (strlen(hp->h_name) >= hostlen)
				return EAI_MEMORY;

			strcpy(host, hp->h_name);
			return 0;
		}
	}
	return 0;
}
#endif /* !HAVE_GETNAMEINFO */
