/*
 * (C)opyright 1993, 1994, 1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * @(#)ip_compat.h	1.8 1/14/96
 * $Id: ip_compat.h,v 2.0.2.6 1997/04/02 12:23:17 darrenr Exp $
 */

#ifndef	__IP_COMPAT_H_
#define	__IP_COMPAT_H__

#ifndef	__P
# ifdef	__STDC__
#  define	__P(x)  x
# else
#  define	__P(x)  ()
# endif
#endif

#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#if	SOLARIS
# define	MTYPE(m)	((m)->b_datap->db_type)
# include	<sys/ioccom.h>
# include	<sys/sysmacros.h>
/*
 * because Solaris 2 defines these in two places :-/
 */
# undef	IPOPT_EOL
# undef	IPOPT_NOP
# undef	IPOPT_LSRR
# undef	IPOPT_RR
# undef	IPOPT_SSRR
# ifndef	_KERNEL
#  define	_KERNEL
#  undef	RES_INIT
#  include <inet/common.h>
#  include <inet/ip.h>
#  include <inet/ip_ire.h>
#  undef	_KERNEL
# else
#  include <inet/common.h>
#  include <inet/ip.h>
#  include <inet/ip_ire.h>
# endif
#endif
#define	IPMINLEN(i, h)	((i)->ip_len >= ((i)->ip_hl * 4 + sizeof(struct h)))

#ifndef	IP_OFFMASK
#define	IP_OFFMASK	0x1fff
#endif

#if	BSD > 199306
# define	USE_QUAD_T
# define	U_QUAD_T	u_quad_t
#else
# define	U_QUAD_T	u_long
#endif

#ifndef	MAX
#define	MAX(a,b)	(((a) > (b)) ? (a) : (b))
#endif

/*
 * Security Options for Intenet Protocol (IPSO) as defined in RFC 1108.
 *
 * Basic Option
 *
 * 00000001   -   (Reserved 4)
 * 00111101   -   Top Secret
 * 01011010   -   Secret
 * 10010110   -   Confidential
 * 01100110   -   (Reserved 3)
 * 11001100   -   (Reserved 2)
 * 10101011   -   Unclassified
 * 11110001   -   (Reserved 1)
 */
#define	IPSO_CLASS_RES4		0x01
#define	IPSO_CLASS_TOPS		0x3d
#define	IPSO_CLASS_SECR		0x5a
#define	IPSO_CLASS_CONF		0x96
#define	IPSO_CLASS_RES3		0x66
#define	IPSO_CLASS_RES2		0xcc
#define	IPSO_CLASS_UNCL		0xab
#define	IPSO_CLASS_RES1		0xf1

#define	IPSO_AUTH_GENSER	0x80
#define	IPSO_AUTH_ESI		0x40
#define	IPSO_AUTH_SCI		0x20
#define	IPSO_AUTH_NSA		0x10
#define	IPSO_AUTH_DOE		0x08
#define	IPSO_AUTH_UN		0x06
#define	IPSO_AUTH_FTE		0x01

/*
 * IP option #defines
 */
/*#define	IPOPT_RR	7 */
#define	IPOPT_ZSU	10	/* ZSU */
#define	IPOPT_MTUP	11	/* MTUP */
#define	IPOPT_MTUR	12	/* MTUR */
#define	IPOPT_ENCODE	15	/* ENCODE */
/*#define	IPOPT_TS	68 */
#define	IPOPT_TR	82	/* TR */
/*#define	IPOPT_SECURITY	130 */
/*#define	IPOPT_LSRR	131 */
#define	IPOPT_E_SEC	133	/* E-SEC */
#define	IPOPT_CIPSO	134	/* CIPSO */
/*#define	IPOPT_SATID	136 */
#ifndef	IPOPT_SID
# define	IPOPT_SID	IPOPT_SATID
#endif
/*#define	IPOPT_SSRR	137 */
#define	IPOPT_ADDEXT	147	/* ADDEXT */
#define	IPOPT_VISA	142	/* VISA */
#define	IPOPT_IMITD	144	/* IMITD */
#define	IPOPT_EIP	145	/* EIP */
#define	IPOPT_FINN	205	/* FINN */


#ifdef	__FreeBSD__
# include <machine/spl.h>
# if defined(IPFILTER_LKM) && !defined(ACTUALLY_LKM_NOT_KERNEL)
#  define	ACTUALLY_LKM_NOT_KERNEL
# endif
#endif

/*
 * Build some macros and #defines to enable the same code to compile anywhere
 * Well, that's the idea, anyway :-)
 */
#if defined(_KERNEL) || defined(KERNEL)
# if SOLARIS
#  define	MUTEX_ENTER(x)	mutex_enter(x)
#  define	MUTEX_EXIT(x)	mutex_exit(x)
#  define	MTOD(m,t)	(t)((m)->b_rptr)
#  define	IRCOPY(a,b,c)	copyin((a), (b), (c))
#  define	IWCOPY(a,b,c)	copyout((a), (b), (c))
# else
#  define	MUTEX_ENTER(x)	;
#  define	MUTEX_EXIT(x)	;
#  ifndef linux
#   define	MTOD(m,t)	mtod(m,t)
#   define	IRCOPY(a,b,c)	bcopy((a), (b), (c))
#   define	IWCOPY(a,b,c)	bcopy((a), (b), (c))
#  endif
# endif /* SOLARIS */

# ifdef sun
#  if defined(__svr4__) || defined(__SVR4)
extern	ill_t	*get_unit __P((char *));
#   define	GETUNIT(n)	get_unit((n))
#  else
#   include	<sys/kmem_alloc.h>
#   define	GETUNIT(n)	ifunit((n), IFNAMSIZ)
#  endif
# else
#  define	GETUNIT(n)	ifunit((n))
# endif /* sun */

# if defined(sun) && !defined(linux)
#  define	UIOMOVE(a,b,c,d)	uiomove(a,b,c,d)
#  define	SLEEP(id, n)	sleep((id), PZERO+1)
#  define	KFREE(x)	kmem_free((char *)(x), sizeof(*(x)))
#  if SOLARIS
typedef	struct	qif	{
	struct	qif	*qf_next;
	ill_t	*qf_ill;
	kmutex_t	qf_lock;
	void	*qf_iptr;
	void	*qf_optr;
	queue_t	*qf_in;
	queue_t	*qf_out;
	void	*qf_wqinfo;
	void	*qf_rqinfo;
	int	(*qf_inp) __P((queue_t *, mblk_t *));
	int	(*qf_outp) __P((queue_t *, mblk_t *));
	mblk_t	*qf_m;
	int	qf_len;
	char	qf_name[8];
	/*
	 * in case the ILL has disappeared...
	 */
	int	qf_hl;	/* header length */
} qif_t;
#   define	SPLNET(x)	;
#   undef	SPLX
#   define	SPLX(x)		;
#   ifdef	sparc
#    define	ntohs(x)	(x)
#    define	ntohl(x)	(x)
#    define	htons(x)	(x)
#    define	htonl(x)	(x)
#   endif
#   define	KMALLOC(a,b,c)	(a) = (b)kmem_alloc((c), KM_NOSLEEP)
#   define	GET_MINOR(x)	getminor(x)
#  else
#   define	KMALLOC(a,b,c)	(a) = (b)new_kmem_alloc((c), KMEM_NOSLEEP)
#  endif /* __svr4__ */
# endif /* sun && !linux */
# ifndef	GET_MINOR
#  define	GET_MINOR(x)	minor(x)
# endif
# if BSD >= 199306 || defined(__FreeBSD__)
#  include <vm/vm.h>
#  if !defined(__FreeBSD__) || (defined (__FreeBSD__) && __FreeBSD__>=3)
#   include <vm/vm_extern.h>
#   include <sys/proc.h>
extern	vm_map_t	kmem_map;
#  else
#   include <vm/vm_kern.h>
#  endif /* __FreeBSD__ */
/*
#  define	KMALLOC(a,b,c)	(a) = (b)kmem_alloc(kmem_map, (c))
#  define	KFREE(x)	kmem_free(kmem_map, (vm_offset_t)(x), \
					  sizeof(*(x)))
*/
#  ifdef	M_PFIL
#   define	KMALLOC(a, b, c)	MALLOC((a), b, (c), M_PFIL, M_NOWAIT)
#   define	KFREE(x)	FREE((x), M_PFIL)
#  else
#   define	KMALLOC(a, b, c)	MALLOC((a), b, (c), M_TEMP, M_NOWAIT)
#   define	KFREE(x)	FREE((x), M_TEMP)
#  endif
#  define	UIOMOVE(a,b,c,d)	uiomove(a,b,d)
#  define	SLEEP(id, n)	tsleep((id), PPAUSE|PCATCH, n, 0)
# endif /* BSD */
# if defined(NetBSD1_0) && (NetBSD1_0 > 1)
#  define	SPLNET(x)	x = splsoftnet()
# else
#  if !SOLARIS
#   define	SPLNET(x)	x = splnet()
#   define	SPLX(x)		(void) splx(x)
#  endif
# endif
#else
# define	MUTEX_ENTER(x)	;
# define	MUTEX_EXIT(x)	;
# define	SPLNET(x)	;
# undef		SPLX
# define	SPLX(x)		;
# define	KMALLOC(a,b,c)	(a) = (b)malloc(c)
# define	KFREE(x)	free(x)
# define	GETUNIT(x)	get_unit(x)
# define	IRCOPY(a,b,c)	bcopy((a), (b), (c))
# define	IWCOPY(a,b,c)	bcopy((a), (b), (c))
#endif /* KERNEL */

#ifdef linux
# define	ICMP_UNREACH	ICMP_DEST_UNREACH
# define	ICMP_SOURCEQUENCH	ICMP_SOURCE_QUENCH
# define	ICMP_TIMXCEED	ICMP_TIME_EXCEEDED
# define	ICMP_PARAMPROB	ICMP_PARAMETERPROB

# define	TH_FIN	0x01
# define	TH_SYN	0x02
# define	TH_RST	0x04
# define	TH_PUSH	0x08
# define	TH_ACK	0x10
# define	TH_URG	0x20

typedef	struct	{
	__u16	th_sport;
	__u16	th_dport;
	__u32	th_seq;
	__u32	th_ack;
	__u8	th_x;
	__u8	th_flags;
	__u16	th_win;
	__u16	th_sum;
	__u16	th_urp;
} tcphdr_t;

typedef	struct	{
	__u16	uh_sport;
	__u16	uh_dport;
	__u16	uh_ulen;
	__u16	uh_sun;
} udphdr_t;

typedef	struct	{
# if defined(__i386__) || defined(__MIPSEL__) || defined(__alpha__) ||\
    defined(vax)
	__u8	ip_hl:4;
	__u8	ip_v:4;
# else
	__u8	ip_hl:4;
	__u8	ip_v:4;
# endif
	__u8	ip_tos;
	__u16	ip_len;
	__u16	ip_id;
	__u16	ip_off;
	__u8	ip_ttl;
	__u8	ip_p;
	__u16	ip_sum;
	struct	in_addr	ip_src;
	struct	in_addr	ip_dst;
} ip_t;

/*
 * Structure of an icmp header.
 */
struct icmp {
	u_char	icmp_type;		/* type of message, see below */
	u_char	icmp_code;		/* type sub code */
	u_short	icmp_cksum;		/* ones complement cksum of struct */
	union {
		u_char ih_pptr;			/* ICMP_PARAMPROB */
		struct in_addr ih_gwaddr;	/* ICMP_REDIRECT */
		struct ih_idseq {
			n_short	icd_id;
			n_short	icd_seq;
		} ih_idseq;
		int ih_void;
	} icmp_hun;
# define	icmp_pptr	icmp_hun.ih_pptr
# define	icmp_gwaddr	icmp_hun.ih_gwaddr
# define	icmp_id		icmp_hun.ih_idseq.icd_id
# define	icmp_seq	icmp_hun.ih_idseq.icd_seq
# define	icmp_void	icmp_hun.ih_void
	union {
		struct id_ts {
			n_time its_otime;
			n_time its_rtime;
			n_time its_ttime;
		} id_ts;
		struct id_ip  {
			ip_t idi_ip;
			/* options and then 64 bits of data */
		} id_ip;
		u_long	id_mask;
		char	id_data[1];
	} icmp_dun;
# define	icmp_otime	icmp_dun.id_ts.its_otime
# define	icmp_rtime	icmp_dun.id_ts.its_rtime
# define	icmp_ttime	icmp_dun.id_ts.its_ttime
# define	icmp_ip		icmp_dun.id_ip.idi_ip
# define	icmp_mask	icmp_dun.id_mask
# define	icmp_data	icmp_dun.id_data
};

struct ipovly {
	caddr_t	ih_next, ih_prev;	/* for protocol sequence q's */
	u_char	ih_x1;			/* (unused) */
	u_char	ih_pr;			/* protocol */
	short	ih_len;			/* protocol length */
	struct	in_addr ih_src;		/* source internet address */
	struct	in_addr ih_dst;		/* destination internet address */
};

# define	SPLX(x)		(void)
# define	SPLNET(x)	(void)

# define	bcopy(a,b,c)	memmove(b,a,c)
# define	bcmp(a,b,c)	memcmp(a,b,c)

# define	UNITNAME(n)	dev_get((n))
# define	ifnet	device

# define	KMALLOC(a,b,c)	(a) = (b)kmalloc((c), GFP_ATOMIC)
# define	KFREE(x)	kfree_s((x), sizeof(*(x)))
# define	IRCOPY(a,b,c)	{ \
				 error = verify_area(VERIFY_READ, \
						     (b) ,sizeof((b))); \
				 if (!error) \
					memcpy_fromfs((b), (a), (c)); \
				}
# define	IWCOPY(a,b,c)	{ \
				 error = verify_area(VERIFY_WRITE, \
						     (b) ,sizeof((b))); \
				 if (!error) \
					memcpy_tofs((b), (a), (c)); \
				}
#else
typedef	struct	tcphdr	tcphdr_t;
typedef	struct	udphdr	udphdr_t;
typedef	struct	icmp	icmphdr_t;
typedef	struct	ip	ip_t;
#endif /* linux */

#endif	/* __IP_COMPAT_H__ */
