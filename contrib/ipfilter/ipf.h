/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ipf.h	1.12 6/5/96
 * $Id: ipf.h,v 2.9.2.6 2002/01/03 08:00:12 darrenr Exp $
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
#define	OPT_STATETOP	0x400000
#define	OPT_FLUSH	0x800000
#define	OPT_CLEAR	0x1000000
#define	OPT_HEX		0x2000000
#define	OPT_NODO	0x80000000

#define	OPT_STAT	OPT_FRSTATES
#define	OPT_LIST	OPT_SHOWLIST


#ifndef __P
# ifdef	__STDC__
#  define	__P(x)	x
# else
#  define	__P(x)	()
# endif
#endif

struct ipstate;
struct frpcmp;
struct ipnat;
struct nat;

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


extern	char	*proto;
extern	char	flagset[];
extern	u_char	flags[];

extern	u_char	tcp_flags __P((char *, u_char *, int));
extern	int	countbits __P((u_32_t));
extern	int	ratoi __P((char *, int *, int, int));
extern	int	ratoui __P((char *, u_int *, u_int, u_int));
extern	int	hostmask __P((char ***, u_32_t *, u_32_t *, u_short *, int *,
			      u_short *, int));
extern	int	ports __P((char ***, u_short *, int *, u_short *, int));
extern	char	*portname __P((int, int));
extern	u_32_t	buildopts __P((char *, char *, int));
extern	int	genmask __P((char *, u_32_t *));
extern	int	hostnum __P((u_32_t *, char *, int));
extern	u_32_t	optname __P((char ***, u_short *, int));
extern	void	printpacket __P((ip_t *));
extern	void	printpacket6 __P((ip_t *));
extern	void	printportcmp __P((int, struct frpcmp *));
extern	void	printhostmask __P((int, u_32_t *, u_32_t *));
extern	void	printbuf __P((char *, int, int));
extern	char	*hostname __P((int, void *));
extern	struct ipstate *printstate __P((struct ipstate *, int));
extern	void	printnat __P((struct ipnat *, int));
extern	void	printactivenat __P((struct nat *, int));

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
