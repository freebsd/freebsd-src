/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/sys/netatm/uni/uniip_var.h,v 1.2 1999/08/28 00:49:04 peter Exp $
 *
 */

/*
 * ATM Forum UNI Support
 * ---------------------
 *
 * IP interface control blocks
 *
 */

#ifndef _UNI_UNIIP_VAR_H
#define _UNI_UNIIP_VAR_H

#ifdef ATM_KERNEL
/*
 * UNI IP network interface structure.  There will be one such structure for 
 * each IP network interface attached via a UNI signalling instance.
 */
struct uniip {
	struct uniip	*uip_next;	/* Next attached IP interface */
	struct ip_nif	*uip_ipnif;	/* IP network interface */
	u_char		uip_flags;	/* Interface flags (see below) */

	/* ATMARP (RFC1577) */
	u_char		uip_arpstate;	/* ARP interface state (see below) */
	struct arpmap	uip_arpsvrmap;	/* ATMARP server map info */
	struct ipvcc	*uip_arpsvrvcc;	/* ATMARP server's VCC */
	int		uip_nprefix;	/* Count of IP prefixes (server only) */
	struct uniarp_prf *uip_prefix;	/* Array of IP prefixes (server only) */
	struct atm_time	uip_arptime;	/* ARP timer controls */
};
#define	uip_arpsvrip	uip_arpsvrmap.am_dstip
#define	uip_arpsvratm	uip_arpsvrmap.am_dstatm
#define	uip_arpsvrsub	uip_arpsvrmap.am_dstatmsub
#endif	/* ATM_KERNEL */

/*
 * UNI Interface Flags
 */
#define	UIF_IFADDR	0x01		/* Interface address is set */

/*
 * UNI ARP Interface States
 */
#define	UIAS_NOTCONF		1	/* Not configured */
#define	UIAS_SERVER_ACTIVE	2	/* Server - active */
#define	UIAS_CLIENT_PADDR	3	/* Client - pending ATM address */
#define	UIAS_CLIENT_POPEN	4	/* Client - pending server vcc open */
#define	UIAS_CLIENT_REGISTER	5	/* Client - registering with server */
#define	UIAS_CLIENT_ACTIVE	6	/* Client - active */


#ifdef ATM_KERNEL
/*
 * Structure for allowable IP prefixes for ATMARP server registration
 */
struct uniarp_prf {
	struct in_addr	upf_addr;	/* Prefix address */
	struct in_addr	upf_mask;	/* Prefix mask */
};


/*
 * UNI ARP protocol constants
 */
#define	UNIARP_AGING		(60 * ATM_HZ)	/* Aging timer tick */
#define	UNIARP_HASHSIZ		19	/* Hash table size */
#define	UNIARP_REGIS_REFRESH	(15 * 60 * ATM_HZ)
					/* Client registration refresh timer */
#define	UNIARP_REGIS_RETRY	(60 * ATM_HZ)
					/* Client registration retry timer */
#define	UNIARP_ARP_RETRY	(3 * ATM_HZ)	/* ARP command retry timer */
#define	UNIARP_CLIENT_AGE	12	/* Client validation timeout */
#define	UNIARP_CLIENT_RETRY	3	/* Client validation retrys */
#define	UNIARP_SERVER_AGE	17	/* Server validation timeout */
#define	UNIARP_SERVER_RETRY	3	/* Server validation retrys */
#define	UNIARP_RETRY_AGE	1	/* Retry timeout */
#define	UNIARP_REVALID_AGE	2	/* Revalidation timeout */
#define	UNIARP_MIN_REFRESH	10	/* Minimum entry refresh time */


/*
 * Structure for ATMARP mappings.  Each of these structures will contain
 * IP address to ATM hardware address mappings.  There will be one such
 * structure for each IP address and for each unresolved ATM address
 * currently in use.
 */
struct uniarp {
	struct arpmap	ua_arpmap;	/* Common entry header */
	struct uniip	*ua_intf;	/* Interface where we learned answer */
	struct uniarp	*ua_next;	/* Hash chain link */
	u_char		ua_flags;	/* Flags (see below) */
	u_char		ua_origin;	/* Source of mapping (see below) */
	u_char		ua_retry;	/* Retry counter */
	u_char		ua_aging;	/* Aging timeout value (minutes) */
	struct ipvcc	*ua_ivp;	/* Head of IP VCC chain */
	struct atm_time	ua_time;	/* Timer controls */
};
#define	ua_dstip	ua_arpmap.am_dstip
#define	ua_dstatm	ua_arpmap.am_dstatm
#define	ua_dstatmsub	ua_arpmap.am_dstatmsub
#endif	/* ATM_KERNEL */

/*
 * UNIARP Entry Flags
 */
#define	UAF_VALID	ARPF_VALID	/* Entry is valid */
#define	UAF_REFRESH	ARPF_REFRESH	/* Entry has been refreshed */
#define	UAF_LOCKED	0x04		/* Entry is locked */
#define	UAF_USED	0x08		/* Entry has been used recently */

/*
 * UNIARP Entry Origin
 *
 * The origin values are ranked according to the source precedence.  
 * Larger values are more preferred.
 */
#define	UAO_LOCAL	100		/* Local address */
#define	UAO_PERM	ARP_ORIG_PERM	/* Permanently installed */
#define	UAO_REGISTER	40		/* Learned via client registration */
#define	UAO_SCSP	30		/* Learned via SCSP */
#define	UAO_LOOKUP	20		/* Learned via server lookup */
#define	UAO_PEER_RSP	15		/* Learned from peer - inarp rsp */
#define	UAO_PEER_REQ	10		/* Learned from peer - inarp req */

/*
 * ATMARP/InATMARP Packet Format
 */
struct atmarp_hdr {
	u_short		ah_hrd;		/* Hardware type (see below) */
	u_short		ah_pro;		/* Protocol type */
	u_char		ah_shtl;	/* Type/len of source ATM address */
	u_char		ah_sstl;	/* Type/len of source ATM subaddress */
	u_short		ah_op;		/* Operation code (see below) */
	u_char		ah_spln;	/* Length of source protocol address */
	u_char		ah_thtl;	/* Type/len of target ATM address */
	u_char		ah_tstl;	/* Type/len of target ATM subaddress */
	u_char		ah_tpln;	/* Length of target protocol address */
#ifdef notdef
	/* Variable size fields */
	u_char		ah_sha[];	/* Source ATM address */
	u_char		ah_ssa[];	/* Source ATM subaddress */
	u_char		ah_spa[];	/* Source protocol address */
	u_char		ah_tha[];	/* Target ATM subaddress */
	u_char		ah_tsa[];	/* Target ATM address */
	u_char		ah_tpa[];	/* Target protocol subaddress */
#endif
};

/*
 * Hardware types
 */
#define	ARP_ATMFORUM	19

/*
 * Operation types
 */
#define	ARP_REQUEST	1		/* ATMARP request */
#define	ARP_REPLY	2		/* ATMARP response */
#define	INARP_REQUEST	8		/* InATMARP request */
#define	INARP_REPLY	9		/* InATMARP response */
#define	ARP_NAK		10		/* ATMARP negative ack */

/*
 * Type/length fields
 */
#define ARP_TL_TMASK	0x40		/* Type mask */
#define ARP_TL_NSAPA	0x00		/* Type = ATM Forum NSAPA */
#define ARP_TL_E164	0x40		/* Type = E.164 */
#define ARP_TL_LMASK	0x3f		/* Length mask */


#ifdef ATM_KERNEL
/*
 * Timer macros
 */
#define	UNIIP_ARP_TIMER(s, t)	atm_timeout(&(s)->uip_arptime, (t), uniarp_iftimeout)
#define	UNIIP_ARP_CANCEL(s)	atm_untimeout(&(s)->uip_arptime)
#define	UNIARP_TIMER(s, t)	atm_timeout(&(s)->ua_time, (t), uniarp_timeout)
#define	UNIARP_CANCEL(s)	atm_untimeout(&(s)->ua_time)


/*
 * Macros for manipulating UNIARP tables and entries
 */
#define UNIARP_HASH(ip)	((u_long)(ip) % UNIARP_HASHSIZ)

#define	UNIARP_ADD(ua)						\
{								\
	struct uniarp	**h;					\
	h = &uniarp_arptab[UNIARP_HASH((ua)->ua_dstip.s_addr)];	\
	LINK2TAIL((ua), struct uniarp, *h, ua_next);		\
}

#define	UNIARP_DELETE(ua)					\
{								\
	struct uniarp	**h;					\
	h = &uniarp_arptab[UNIARP_HASH((ua)->ua_dstip.s_addr)];	\
	UNLINK((ua), struct uniarp, *h, ua_next);		\
}

#define	UNIARP_LOOKUP(ip, ua)					\
{								\
	for ((ua) = uniarp_arptab[UNIARP_HASH(ip)];		\
				(ua); (ua) = (ua)->ua_next) {	\
		if ((ua)->ua_dstip.s_addr == (ip))		\
			break;					\
	}							\
}


/*
 * Global UNIARP Statistics
 */
struct uniarp_stat {
	u_long		uas_rcvdrop;	/* Input packets dropped */
};


/*
 * External variables
 */
extern struct uniip		*uniip_head;
extern struct ip_serv		uniip_ipserv;
extern struct uniarp		*uniarp_arptab[];
extern struct uniarp		*uniarp_nomaptab;
extern struct uniarp		*uniarp_pvctab;
extern struct sp_info		uniarp_pool;
extern struct atm_time		uniarp_timer;
extern int			uniarp_print;
extern Atm_endpoint		uniarp_endpt;
extern struct uniarp_stat	uniarp_stat;


/*
 * Global function declarations
 */
	/* uniarp.c */
int		uniarp_start __P((void));
void		uniarp_stop __P((void));
void		uniarp_ipact __P((struct uniip *));
void		uniarp_ipdact __P((struct uniip *));
void		uniarp_ifaddr __P((struct siginst *));
void		uniarp_iftimeout __P((struct atm_time *));
int		uniarp_ioctl __P((int, caddr_t, caddr_t));
caddr_t		uniarp_getname __P((void *));

	/* uniarp_cache.c */
int		uniarp_cache_svc __P((struct uniip *, struct in_addr *,
			Atm_addr *, Atm_addr *, u_int));
void		uniarp_cache_pvc __P((struct ipvcc *, struct in_addr *,
			Atm_addr *, Atm_addr *));
int		uniarp_validate_ip __P((struct uniip *, struct in_addr *,
			u_int));

	/* uniarp_input.c */
void		uniarp_cpcs_data __P((void *, KBuffer *));
void		uniarp_pdu_print __P((struct ipvcc *, KBuffer *, char *));

	/* uniarp_output.c */
int		uniarp_arp_req __P((struct uniip *, struct in_addr *));
int		uniarp_arp_rsp __P((struct uniip *, struct arpmap *,
			struct in_addr *, Atm_addr *,
			Atm_addr *, struct ipvcc *));
int		uniarp_arp_nak __P((struct uniip *, KBuffer *, struct ipvcc *));
int		uniarp_inarp_req __P((struct uniip *, Atm_addr *,
			Atm_addr *, struct ipvcc *));
int		uniarp_inarp_rsp __P((struct uniip *, struct in_addr *,
			Atm_addr *, Atm_addr *, struct ipvcc *));

	/* uniarp_timer.c */
void		uniarp_timeout __P((struct atm_time *));
void		uniarp_aging __P((struct atm_time *));

	/* uniarp_vcm.c */
int		uniarp_pvcopen __P((struct ipvcc *));
int		uniarp_svcout __P((struct ipvcc *, struct in_addr *));
int		uniarp_svcin __P((struct ipvcc *, Atm_addr *, Atm_addr *));
int		uniarp_svcactive __P((struct ipvcc *));
void		uniarp_vcclose __P((struct ipvcc *));
void		uniarp_connected __P((void *));
void		uniarp_cleared __P((void *, struct t_atm_cause *));

	/* uniip.c */
int		uniip_start __P((void));
int		uniip_stop __P((void));


#endif	/* ATM_KERNEL */

#endif	/* _UNI_UNIIP_VAR_H */
