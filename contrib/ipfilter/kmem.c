/*
 * Copyright (C) 1993-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
/*
 * kmemcpy() - copies n bytes from kernel memory into user buffer.
 * returns 0 on success, -1 on error.
 */

#ifdef __sgi
# include <sys/ptimers.h>
#endif
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/file.h>
#ifndef __sgi
#include <kvm.h>
#endif
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif

#include "kmem.h"
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "ipf.h"


#ifndef __STDC__
# define	const
#endif

#if !defined(lint)
static const char sccsid[] = "@(#)kmem.c	1.4 1/12/96 (C) 1992 Darren Reed";
static const char rcsid[] = "@(#)$Id: kmem.c,v 2.2.2.12 2002/03/06 09:44:16 darrenr Exp $";
#endif

#ifndef	__sgi

static	kvm_t	*kvm_f = NULL;

#else

typedef	int	kvm_t;

static	kvm_t	kvm_f = -1;
static	char	*kvm_errstr;

kvm_t kvm_open(kernel, core, swap, mode, errstr)
char *kernel, *core, *swap;
int mode;
char *errstr;
{
	kvm_t fd;

	kvm_errstr = errstr;

	fd = open(core, mode);
	return fd;
}

int kvm_read(kvm, pos, buffer, size)
kvm_t kvm;
u_long pos;
char *buffer;
size_t size;
{
	int r, left;
	char *bufp;

	if (lseek(kvm, pos, 0) == -1) {
		fprintf(stderr, "%s", kvm_errstr);
		perror("lseek");
		return -1;
	}

	for (bufp = buffer, left = size; left > 0; bufp += r, left -= r) {
		r = read(kvm, bufp, 1);
		if (r <= 0)
			return -1;
	}
	return 0;
}
#endif


int	openkmem(kern, core)
char	*kern, *core;
{
	kvm_f = kvm_open(kern, core, NULL, O_RDONLY, "");
	if (kvm_f == NULL)
	    {
		perror("openkmem:open");
		return -1;
	    }
	return (int)kvm_f;
}

int	kmemcpy(buf, pos, n)
register char	*buf;
long	pos;
register int	n;
{
	register int	r;

	if (!n)
		return 0;

	if (kvm_f == NULL)
		if (openkmem(NULL, NULL) == -1)
			return -1;

	while ((r = kvm_read(kvm_f, pos, buf, n)) < n)
		if (r <= 0)
		    {
			fprintf(stderr, "pos=0x%x ", (u_int)pos);
			perror("kmemcpy:read");
			return -1;
		    }
		else
		    {
			buf += r;
			pos += r;
			n -= r;
		    }
	return 0;
}

int	kstrncpy(buf, pos, n)
register char	*buf;
long	pos;
register int	n;
{
	register int	r;

	if (!n)
		return 0;

	if (kvm_f == NULL)
		if (openkmem(NULL, NULL) == -1)
			return -1;

	while (n > 0)
	    {
		r = kvm_read(kvm_f, pos, buf, 1);
		if (r <= 0)
		    {
			fprintf(stderr, "pos=0x%x ", (u_int)pos);
			perror("kstrncpy:read");
			return -1;
		    }
		else
		    {
			if (*buf == '\0')
				break;
			buf++;
			pos++;
			n--;
		    }
	    }
	return 0;
}


/*
 * Given a pointer to an interface in the kernel, return a pointer to a
 * string which is the interface name.
 */
char *getifname(ptr)
void *ptr;
{
#if SOLARIS
	char *ifname;
	ill_t ill;

	if (ptr == (void *)-1)
		return "!";
	if (ptr == NULL)
		return "-";

	if (kmemcpy((char *)&ill, (u_long)ptr, sizeof(ill)) == -1)
		return "X";
	ifname = malloc(ill.ill_name_length + 1);
	if (kmemcpy(ifname, (u_long)ill.ill_name,
		    ill.ill_name_length) == -1)
		return "X";
	return ifname;
#else
# if defined(NetBSD) && (NetBSD >= 199905) && (NetBSD < 1991011) || \
    defined(__OpenBSD__)
#else
	char buf[32];
	int len;
# endif
	struct ifnet netif;

	if (ptr == (void *)-1)
		return "!";
	if (ptr == NULL)
		return "-";

	if (kmemcpy((char *)&netif, (u_long)ptr, sizeof(netif)) == -1)
		return "X";
# if defined(NetBSD) && (NetBSD >= 199905) && (NetBSD < 1991011) || \
    defined(__OpenBSD__)
	return strdup(netif.if_xname);
# else
	if (kstrncpy(buf, (u_long)netif.if_name, sizeof(buf)) == -1)
		return "X";
	if (netif.if_unit < 10)
		len = 2;
	else if (netif.if_unit < 1000)
		len = 3;
	else if (netif.if_unit < 10000)
		len = 4;
	else
		len = 5;
	buf[sizeof(buf) - len] = '\0';
	sprintf(buf + strlen(buf), "%d", netif.if_unit % 10000);
	return strdup(buf);
# endif
#endif
}
