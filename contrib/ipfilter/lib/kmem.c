/*	$NetBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
/*
 * kmemcpy() - copies n bytes from kernel memory into user buffer.
 * returns 0 on success, -1 on error.
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>
#if !defined(__sgi) && !defined(__hpux) && !defined(__osf__) && !defined(linux)
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
#if defined(linux) || defined(__osf__) || defined(__sgi) || defined(__hpux)
# include <stdlib.h>
#endif

#include "kmem.h"

#ifndef __STDC__
# define	const
#endif

#if !defined(lint)
static const char sccsid[] = "@(#)kmem.c	1.4 1/12/96 (C) 1992 Darren Reed";
static const char rcsid[] = "@(#)Id: kmem.c,v 1.16.2.1 2004/06/20 10:25:58 darrenr Exp";
#endif



#if !defined(__sgi) && !defined(__hpux) && !defined(__osf__) && !defined(linux)
/*
 * For all platforms where there is a libkvm and a kvm_t, we use that...
 */
static	kvm_t	*kvm_f = NULL;

#else
/*
 *...and for the others (HP-UX, IRIX, Tru64), we have to provide our own.
 */

typedef	int *	kvm_t;

static	kvm_t	kvm_f = NULL;
static	char	*kvm_errstr = NULL;

kvm_t kvm_open __P((char *, char *, char *, int, char *));
int kvm_read __P((kvm_t, u_long, char *, size_t));

kvm_t kvm_open(kernel, core, swap, mode, errstr)
char *kernel, *core, *swap;
int mode;
char *errstr;
{
	kvm_t k;
	int fd;

	kvm_errstr = errstr;

	if (core == NULL)
		core = "/dev/kmem";

	fd = open(core, mode);
	if (fd == -1)
		return NULL;
	k = malloc(sizeof(*k));
	if (k == NULL)
		return NULL;
	*k = fd;
	return k;
}

int kvm_read(kvm, pos, buffer, size)
kvm_t kvm;
u_long pos;
char *buffer;
size_t size;
{
	int r = 0, left;
	char *bufp;

	if (lseek(*kvm, pos, 0) == -1) {
		if (kvm_errstr != NULL) {
			fprintf(stderr, "%s", kvm_errstr);
			perror("lseek");
		}
		return -1;
	}

	for (bufp = buffer, left = size; left > 0; bufp += r, left -= r) {
		r = read(*kvm, bufp, left);
#ifdef	__osf__
		/*
		 * Tru64 returns "0" for successful operation, not the number
		 * of bytes read.
		 */
		if (r == 0)
			r = left;
#endif
		if (r <= 0)
			return -1;
	}
	return r;
}
#endif /* !defined(__sgi) && !defined(__hpux) && !defined(__osf__) */

int	openkmem(kern, core)
char	*kern, *core;
{
	kvm_f = kvm_open(kern, core, NULL, O_RDONLY, NULL);
	if (kvm_f == NULL)
	    {
		perror("openkmem:open");
		return -1;
	    }
	return kvm_f != NULL;
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
			fprintf(stderr, "pos=0x%lx ", (u_long)pos);
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
			fprintf(stderr, "pos=0x%lx ", (u_long)pos);
			perror("kmemcpy:read");
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
