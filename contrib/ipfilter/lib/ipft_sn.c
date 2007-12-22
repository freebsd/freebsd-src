/*	$FreeBSD$	*/

/*
 * Copyright (C) 2000-2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: ipft_sn.c,v 1.7.4.1 2006/06/16 17:21:03 darrenr Exp $
 */

/*
 * Written to comply with the recent RFC 1761 from Sun.
 */
#include "ipf.h"
#include "snoop.h"
#include "ipt.h"

#if !defined(lint)
static const char rcsid[] = "@(#)$Id: ipft_sn.c,v 1.7.4.1 2006/06/16 17:21:03 darrenr Exp $";
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

struct	ipread	snoop = { snoop_open, snoop_close, snoop_readip, 0 };


static	int	snoop_open(fname)
char	*fname;
{
	struct	snoophdr sh;
	int	fd;
	int s_v;

	if (sfd != -1)
		return sfd;

	if (!strcmp(fname, "-"))
		fd = 0;
	else if ((fd = open(fname, O_RDONLY)) == -1)
		return -1;

	if (read(fd, (char *)&sh, sizeof(sh)) != sizeof(sh))
		return -2;

	s_v = (int)ntohl(sh.s_v);
	s_type = (int)ntohl(sh.s_type);

	if (s_v != SNOOP_VERSION ||
	    s_type < 0 || s_type > SDL_MAX) {
		(void) close(fd);
		return -2;
	}

	sfd = fd;
	printf("opened snoop file %s:\n", fname);
	printf("\tid: %8.8s version: %d type: %d\n", sh.s_id, s_v, s_type);

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
	int	n, plen, ilen;

	if (read(sfd, (char *)rec, sizeof(*rec)) != sizeof(*rec))
		return -2;

	ilen = (int)ntohl(rec->sp_ilen);
	plen = (int)ntohl(rec->sp_plen);
	if (ilen > plen || plen < sizeof(*rec))
		return -2;

	plen -= sizeof(*rec);
	n = MIN(plen, ilen);
	if (!n || n < 0)
		return -3;

	return plen;
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
