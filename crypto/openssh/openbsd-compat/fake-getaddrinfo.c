/*
 * fake library for ssh
 *
 * This file includes getaddrinfo(), freeaddrinfo() and gai_strerror().
 * These funtions are defined in rfc2133.
 *
 * But these functions are not implemented correctly. The minimum subset
 * is implemented for ssh use only. For exapmle, this routine assumes
 * that ai_family is AF_INET. Don't use it for another purpose.
 */

#include "includes.h"
#include "ssh.h"

RCSID("$Id: fake-getaddrinfo.c,v 1.2 2001/02/09 01:55:36 djm Exp $");

#ifndef HAVE_GAI_STRERROR
char *gai_strerror(int ecode)
{
	switch (ecode) {
		case EAI_NODATA:
			return "no address associated with hostname.";
		case EAI_MEMORY:
			return "memory allocation failure.";
		default:
			return "unknown error.";
	}
}    
#endif /* !HAVE_GAI_STRERROR */

#ifndef HAVE_FREEADDRINFO
void freeaddrinfo(struct addrinfo *ai)
{
	struct addrinfo *next;

	do {
		next = ai->ai_next;
		free(ai);
	} while (NULL != (ai = next));
}
#endif /* !HAVE_FREEADDRINFO */

#ifndef HAVE_GETADDRINFO
static struct addrinfo *malloc_ai(int port, u_long addr)
{
	struct addrinfo *ai;

	ai = malloc(sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
	if (ai == NULL)
		return(NULL);
	
	memset(ai, 0, sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
	
	ai->ai_addr = (struct sockaddr *)(ai + 1);
	/* XXX -- ssh doesn't use sa_len */
	ai->ai_addrlen = sizeof(struct sockaddr_in);
	ai->ai_addr->sa_family = ai->ai_family = AF_INET;

	((struct sockaddr_in *)(ai)->ai_addr)->sin_port = port;
	((struct sockaddr_in *)(ai)->ai_addr)->sin_addr.s_addr = addr;
	
	return(ai);
}

int getaddrinfo(const char *hostname, const char *servname, 
                const struct addrinfo *hints, struct addrinfo **res)
{
	struct addrinfo *cur, *prev = NULL;
	struct hostent *hp;
	struct in_addr in;
	int i, port;

	if (servname)
		port = htons(atoi(servname));
	else
		port = 0;

	if (hints && hints->ai_flags & AI_PASSIVE) {
		if (NULL != (*res = malloc_ai(port, htonl(0x00000000))))
			return 0;
		else
			return EAI_MEMORY;
	}
		
	if (!hostname) {
		if (NULL != (*res = malloc_ai(port, htonl(0x7f000001))))
			return 0;
		else
			return EAI_MEMORY;
	}
	
	if (inet_aton(hostname, &in)) {
		if (NULL != (*res = malloc_ai(port, in.s_addr)))
			return 0;
		else
			return EAI_MEMORY;
	}
	
	hp = gethostbyname(hostname);
	if (hp && hp->h_name && hp->h_name[0] && hp->h_addr_list[0]) {
		for (i = 0; hp->h_addr_list[i]; i++) {
			cur = malloc_ai(port, ((struct in_addr *)hp->h_addr_list[i])->s_addr);
			if (cur == NULL) {
				if (*res)
					freeaddrinfo(*res);
				return EAI_MEMORY;
			}
			
			if (prev)
				prev->ai_next = cur;
			else
				*res = cur;

			prev = cur;
		}
		return 0;
	}
	
	return EAI_NODATA;
}
#endif /* !HAVE_GETADDRINFO */
