/*
 * Copyright (C) 1993-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ipf.h	1.12 6/5/96
 * $Id: ipf.h,v 2.1.2.1 1999/10/05 12:59:25 darrenr Exp $
 */

#ifndef	__IPF_H__
#define	__IPF_H__

#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif
#define	OPT_REMOVE	0x000001
#define	OPT_DEBUG	0x000002
#define	OPT_OUTQUE	FR_OUTQUE	/* 0x00004 */
#define	OPT_INQUE	FR_INQUE	/* 0x00008 */
#define	OPT_LOG		FR_LOG		/* 0x00010 */
#define	OPT_SHOWLIST	0x000020
#define	OPT_VERBOSE	0x000040
#define	OPT_DONOTHING	0x000080
#define	OPT_HITS	0x000100
#define	OPT_BRIEF	0x000200
#define OPT_ACCNT	FR_ACCOUNT	/* 0x0400 */
#define	OPT_FRSTATES	FR_KEEPFRAG	/* 0x0800 */
#define	OPT_IPSTATES	FR_KEEPSTATE	/* 0x1000 */
#define	OPT_INACTIVE	FR_INACTIVE	/* 0x2000 */
#define	OPT_SHOWLINENO	0x004000
#define	OPT_PRINTFR	0x008000
#define	OPT_ZERORULEST	0x010000
#define	OPT_SAVEOUT	0x020000
#define	OPT_AUTHSTATS	0x040000
#define	OPT_RAW		0x080000
#define	OPT_NAT		0x100000
#define	OPT_GROUPS	0x200000

#ifndef __P
# ifdef	__STDC__
#  define	__P(x)	x
# else
#  define	__P(x)	()
# endif
#endif

#ifdef	ultrix
extern	char	*strdup __P((char *));
#endif

extern	struct	frentry	*parse __P((char *, int));

extern	void	printfr __P((struct frentry *));
extern	void	binprint __P((struct frentry *)), initparse __P((void));
extern	int	portnum __P((char *, u_short *, int));


struct	ipopt_names	{
	int	on_value;
	int	on_bit;
	int	on_siz;
	char	*on_name;
};


extern	u_32_t	buildopts __P((char *, char *, int));
extern	u_32_t	hostnum __P((char *, int *, int));
extern	u_32_t	optname __P((char ***, u_short *, int));
extern	void	printpacket __P((ip_t *));
#if SOLARIS
extern	int	inet_aton __P((const char *, struct in_addr *));
extern	int	gethostname __P((char *, int ));
extern	void	sync __P((void));
#endif

#if defined(sun) && !SOLARIS
# define	STRERROR(x)	sys_errlist[x]
extern	char	*sys_errlist[];
#else
# define	STRERROR(x)	strerror(x)
#endif

#ifndef	MIN
#define	MIN(a,b)	((a) > (b) ? (b) : (a))
#endif

#endif /* __IPF_H__ */
