/*
 * Copyright (C) 1993-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */

/*
 * Written to comply with the recent RFC 1761 from Sun.
 */
#include <stdio.h>
#include <string.h>
#if !defined(__SVR4) && !defined(__GNUC__)
#include <strings.h>
#endif
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifndef	linux
#include <netinet/ip_var.h>
#endif
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include "ip_compat.h"
#include <netinet/tcpip.h>
#include "ipf.h"
#include "ipt.h"
#include "snoop.h"

#if !defined(lint)
static const char rcsid[] = "@(#)$Id: ipft_sn.c,v 2.0.2.6.2.1 1997/11/12 10:56:09 darrenr Exp $";
#endif

struct	llc	{
	int	lc_sz;	/* LLC header length */
	int	lc_to;	/* LLC Type offset */
	int	lc_tl;	/* LLC Type length */
};

/*
 * While many of these maybe the same, some do have different header formats
 * which make this useful.
 */
static	struct	llc	llcs[SDL_MAX+1] = {
	{ 0, 0, 0 },	/* SDL_8023 */
	{ 0, 0, 0 },	/* SDL_8024 */
	{ 0, 0, 0 },	/* SDL_8025 */
	{ 0, 0, 0 },	/* SDL_8026 */
	{ 14, 12, 2 },	/* SDL_ETHER */
	{ 0, 0, 0 },	/* SDL_HDLC */
	{ 0, 0, 0 },	/* SDL_CHSYNC */
	{ 0, 0, 0 },	/* SDL_IBMCC */
	{ 0, 0, 0 },	/* SDL_FDDI */
	{ 0, 0, 0 },	/* SDL_OTHER */
};

static	int	snoop_open __P((char *));
static	int	snoop_close __P((void));
static	int	snoop_readip __P((char *, int, char **, int *));

static	int	sfd = -1, s_type = -1;
static	int	snoop_read_rec __P((struct snooppkt *));

struct	ipread	snoop = { snoop_open, snoop_close, snoop_readip };


static	int	snoop_open(fname)
char	*fname;
{
	struct	snoophdr sh;
	int	fd;

	if (sfd != -1)
		return sfd;

	if (!strcmp(fname, "-"))
		fd = 0;
	else if ((fd = open(fname, O_RDONLY)) == -1)
		return -1;

	if (read(fd, (char *)&sh, sizeof(sh)) != sizeof(sh))
		return -2;

	if (sh.s_v != SNOOP_VERSION ||
	    sh.s_type < 0 || sh.s_type > SDL_MAX) {
		(void) close(fd);
		return -2;
	}

	sfd = fd;
	s_type = sh.s_type;
	printf("opened snoop file %s:\n", fname);
	printf("\tid: %8.8s version: %d type: %d\n", sh.s_id, sh.s_v, s_type);

	return fd;
}


static	int	snoop_close()
{
	return close(sfd);
}


/*
 * read in the header (and validate) which should be the first record
 * in a snoop file.
 */
static	int	snoop_read_rec(rec)
struct	snooppkt *rec;
{
	int	n, p;

	if (read(sfd, (char *)rec, sizeof(*rec)) != sizeof(*rec))
		return -2;

	if (rec->sp_ilen > rec->sp_plen || rec->sp_plen < sizeof(*rec))
		return -2;

	p = rec->sp_plen - sizeof(*rec);
	n = MIN(p, rec->sp_ilen);
	if (!n || n < 0)
		return -3;

	return p;
}


#ifdef	notyet
/*
 * read an entire snoop packet record.  only the data part is copied into
 * the available buffer, with the number of bytes copied returned.
 */
static	int	snoop_read(buf, cnt)
char	*buf;
int	cnt;
{
	struct	snooppkt rec;
	static	char	*bufp = NULL;
	int	i, n;

	if ((i = snoop_read_rec(&rec)) <= 0)
		return i;

	if (!bufp)
		bufp = malloc(i);
	else
		bufp = realloc(bufp, i);

	if (read(sfd, bufp, i) != i)
		return -2;

	n = MIN(i, cnt);
	bcopy(bufp, buf, n);
	return n;
}
#endif


/*
 * return only an IP packet read into buf
 */
static	int	snoop_readip(buf, cnt, ifn, dir)
char	*buf, **ifn;
int	cnt, *dir;
{
	static	char	*bufp = NULL;
	struct	snooppkt rec;
	struct	llc	*l;
	char	ty[4], *s;
	int	i, n;

	do {
		if ((i = snoop_read_rec(&rec)) <= 0)
			return i;

		if (!bufp)
			bufp = malloc(i);
		else
			bufp = realloc(bufp, i);
		s = bufp;

		if (read(sfd, s, i) != i)
			return -2;

		l = &llcs[s_type];
		i -= l->lc_to;
		s += l->lc_to;
		/*
		 * XXX - bogus assumption here on the part of the time field
		 * that it won't be greater than 4 bytes and the 1st two will
		 * have the values 8 and 0 for IP.  Should be a table of
		 * these too somewhere.  Really only works for SDL_ETHER.
		 */
		bcopy(s, ty, l->lc_tl);
	} while (ty[0] != 0x8 && ty[1] != 0);

	i -= l->lc_tl;
	s += l->lc_tl;
	n = MIN(i, cnt);
	bcopy(s, buf, n);

	return n;
}
