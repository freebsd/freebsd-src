/*
 * Copyright (C) 1993-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ipf.h	1.12 6/5/96
 * $Id: ipf.h,v 2.0.2.12 1997/09/28 07:11:50 darrenr Exp $
 */

#ifndef	__IPF_H__
#define	__IPF_H__

#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif
#define	OPT_REMOVE	0x00001
#define	OPT_DEBUG	0x00002
#define	OPT_OUTQUE	FR_OUTQUE	/* 0x0004 */
#define	OPT_INQUE	FR_INQUE	/* 0x0008 */
#define	OPT_LOG		FR_LOG		/* 0x0010 */
#define	OPT_SHOWLIST	0x00020
#define	OPT_VERBOSE	0x00040
#define	OPT_DONOTHING	0x00080
#define	OPT_HITS	0x00100
#define	OPT_BRIEF	0x00200
#define OPT_ACCNT	FR_ACCOUNT	/* 0x0400 */
#define	OPT_FRSTATES	FR_KEEPFRAG	/* 0x0800 */
#define	OPT_IPSTATES	FR_KEEPSTATE	/* 0x1000 */
#define	OPT_INACTIVE	FR_INACTIVE	/* 0x2000 */
#define	OPT_SHOWLINENO	0x04000
#define	OPT_PRINTFR	0x08000
#define	OPT_ZERORULEST	0x10000
#define	OPT_SAVEOUT	0x20000
#define	OPT_AUTHSTATS	0x40000
#define	OPT_RAW		0x80000

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

extern	struct	frentry	*parse __P((char *));

extern	void	printfr __P((struct frentry *));
extern	void	binprint __P((struct frentry *)), initparse __P((void));
extern	u_short	portnum __P((char *));


struct	ipopt_names	{
	int	on_value;
	int	on_bit;
	int	on_siz;
	char	*on_name;
};


extern	u_32_t	buildopts __P((char *, char *, int));
extern	u_32_t	hostnum __P((char *, int *));
extern	u_32_t	optname __P((char ***, u_short *));
extern	void	printpacket __P((ip_t *));
#if SOLARIS
extern	int	inet_aton __P((const char *, struct in_addr *));
#endif

#ifdef	sun
#define	STRERROR(x)	sys_errlist[x]
extern	char	*sys_errlist[];
#else
#define	STRERROR(x)	strerror(x)
#endif

#ifndef	MIN
#define	MIN(a,b)	((a) > (b) ? (b) : (a))
#endif

#endif /* __IPF_H__ */
