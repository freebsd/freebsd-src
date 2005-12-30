/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: ipt.h,v 2.6 2003/02/16 02:33:09 darrenr Exp $
 */

#ifndef	__IPT_H__
#define	__IPT_H__

#ifndef	__P
# define P_DEF
# ifdef	__STDC__
#  define	__P(x) x
# else
#  define	__P(x) ()
# endif
#endif

#include <fcntl.h>


struct	ipread	{
	int	(*r_open) __P((char *));
	int	(*r_close) __P((void));
	int	(*r_readip) __P((char *, int, char **, int *));
	int	r_flags;
};

#define	R_DO_CKSUM	0x01

extern	void	debug __P((char *, ...));
extern	void	verbose __P((char *, ...));

#ifdef P_DEF
# undef	__P
# undef	P_DEF
#endif

#endif /* __IPT_H__ */
