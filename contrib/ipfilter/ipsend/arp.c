/*
 * arp.c (C) 1995 Darren Reed
 *
 * The author provides this program as-is, with no gaurantee for its
 * suitability for any specific purpose.  The author takes no responsibility
 * for the misuse/abuse of this program and provides it for the sole purpose
 * of testing packet filter policies.  This file maybe distributed freely
 * providing it is not modified and that this notice remains in tact.
 */
#if !defined(lint) && defined(LIBC_SCCS)
static	char	sccsid[] = "@(#)arp.c	1.4 1/11/96 (C)1995 Darren Reed";
#endif
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include "ipsend.h"

#if defined(__SVR4) || defined(__svr4__)
#define	bcopy(a,b,c)	memmove(b,a,c)
#define	bzero(a,c)	memset(a,0,c)
#define	bcmp(a,b,c)	memcmp(a,b,c)
#endif

/*
 * lookup host and return
 * its IP address in address
 * (4 bytes)
 */
int	resolve(host, address) 
char	*host, *address;
{
        struct	hostent	*hp;
        u_long	add;

	add = inet_addr(host);
	if (add == -1)
	    {
		if (!(hp = gethostbyname(host)))
		    {
			fprintf(stderr, "unknown host: %s\n", host);
			return -1;
		    }
		bcopy((char *)hp->h_addr, (char *)address, 4);
		return 0;
	}
	bcopy((char*)&add, address, 4);
	return 0;
}

/*
 * ARP for the MAC address corresponding
 * to the IP address.  This taken from
 * some BSD program, I cant remember which.
 */
int	arp(ip, ether)
char	*ip;
char	*ether;
{
	static	int	sfd = -1;
	static	char	ethersave[6], ipsave[4];
	struct	arpreq	ar;
	struct	sockaddr_in	*sin, san;
	struct	hostent	*hp;
	int	fd;

	if (!bcmp(ipsave, ip, 4)) {
		bcopy(ethersave, ether, 6);
		return 0;
	}
	fd = -1;
	bzero((char *)&ar, sizeof(ar));
	sin = (struct sockaddr_in *)&ar.arp_pa;
	sin->sin_family = AF_INET;
	bcopy(ip, (char *)&sin->sin_addr.s_addr, 4);
	if ((hp = gethostbyaddr(ip, 4, AF_INET)))
		if (!(ether_hostton(hp->h_name, ether)))
			goto savearp;

	if (sfd == -1)
		if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		    {
			perror("arp: socket");
			return -1;
		    }
tryagain:
	if (ioctl(sfd, SIOCGARP, (caddr_t)&ar) == -1)
	    {
		if (fd == -1)
		    {
			bzero((char *)&san, sizeof(san));
			san.sin_family = AF_INET;
			san.sin_port = htons(1);
			bcopy(ip, &san.sin_addr.s_addr, 4);
			fd = socket(AF_INET, SOCK_DGRAM, 0);
			(void) sendto(fd, ip, 4, 0,
				      (struct sockaddr *)&san, sizeof(san));
			sleep(1);
			(void) close(fd);
			goto tryagain;
		    }
		fprintf(stderr, "(%s):", inet_ntoa(sin->sin_addr));
		if (errno != ENXIO)
			perror("SIOCGARP");
		return -1;
	    }

	bcopy(ar.arp_ha.sa_data, ether, 6);
savearp:
	bcopy(ether, ethersave, 6);
	bcopy(ip, ipsave, 4);
	return 0;
}
