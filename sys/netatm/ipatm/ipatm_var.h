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
 *	@(#) $FreeBSD$
 *
 */

/*
 * IP Over ATM Support
 * -------------------
 *
 * Protocol control blocks
 *
 */

#ifndef _IPATM_IPATM_VAR_H
#define _IPATM_IPATM_VAR_H

#ifdef ATM_KERNEL
/*
 * Structure containing information for each VCC, both SVC and PVC, which
 * supports IP traffic.
 */
struct ipvcc {
	Qelem_t		iv_elem;	/* ip_nif queueing links */
	u_short		iv_flags;	/* VCC flags (see below) */
	u_char		iv_state;	/* VCC state (see below) */
	Atm_connection	*iv_conn;	/* Connection manager token */
	struct in_addr	iv_dst;		/* Peer's IP address */
	struct ip_nif	*iv_ipnif;	/* IP network interface */
	struct atm_time	iv_time;	/* Timer controls */
	short		iv_idle;	/* VCC idle timer */
	u_char		iv_parmx;	/* Index into provider's vcc params */ 
	KBuffer		*iv_queue;	/* Packet waiting for VCC */
	struct arpmap	*iv_arpent;	/* ARP entry for VCC */
	struct ipvcc	*iv_arpnext;	/* ARP link field */
	Atm_connection	*iv_arpconn;	/* ARP connection manager token */
};
#define iv_forw		iv_elem.q_forw
#define iv_back		iv_elem.q_back
#endif	/* ATM_KERNEL */

/*
 * VCC Flags
 */
#define	IVF_PVC		0x0001		/* PVC */
#define	IVF_SVC		0x0002		/* SVC */
#define	IVF_LLC		0x0004		/* VCC uses LLC/SNAP encapsulation */
#define	IVF_MAPOK	0x0008		/* VCC ARP mapping is valid */
#define	IVF_NOIDLE	0x0010		/* Do not idle-timeout this VCC */

/*
 * VCC States
 */
#define	IPVCC_FREE	0		/* No VCC associated with entry */
#define	IPVCC_PMAP	1		/* SVC waiting for ARP mapping */
#define	IPVCC_POPEN	2		/* Pending SVC open completion */
#define	IPVCC_PACCEPT	3		/* Pending SVC accept completion */
#define	IPVCC_ACTPENT	4		/* PVC open - waiting for ARP entry */
#define	IPVCC_ACTIVE	5		/* VCC open - available */
#define	IPVCC_CLOSED	6		/* VCC has been closed */


#ifdef ATM_KERNEL
/*
 * Structure containing IP-specific information for each ATM network
 * interface in the system.
 */
struct ip_nif {
	struct ip_nif	*inf_next;	/* Next on interface chain */
	struct atm_nif	*inf_nif;	/* ATM network interface */
	u_short		inf_state;	/* Interface state (see below) */
	struct in_ifaddr *inf_addr;	/* Interface's IP address */
	Queue_t		inf_vcq;	/* VCC connections queue */
	struct ip_serv	*inf_serv;	/* Interface service provider */

/* For use by IP interface service provider (ie signalling manager) */
	caddr_t		inf_isintf;	/* Interface control block */

/* IP/ATM provided interface services */
	void		(*inf_arpnotify)/* ARP event notification */
				__P((struct ipvcc *, int));
	int		(*inf_ipinput)	/* IP packet input */
				__P((struct ip_nif *, KBuffer *));
	int		(*inf_createsvc)/* Create an IP SVC */
				__P((struct ifnet *, u_short, caddr_t,
				     struct ipvcc **));
};

/*
 * Network Interface States
 */
#define	IPNIF_ADDR	1		/* Waiting for i/f address */
#define	IPNIF_SIGMGR	2		/* Waiting for sigmgr attach */
#define	IPNIF_ACTIVE	3		/* Active */


/*
 * Global IP/ATM Statistics
 */
struct ipatm_stat {
	u_long		ias_rcvstate;	/* Packets received, bad vcc state */
	u_long		ias_rcvnobuf;	/* Packets received, no buf avail */
};


/*
 * Structure to pass parameters for ipatm_openpvc()
 */
struct ipatmpvc {
	struct ip_nif	*ipp_ipnif;	/* PVC's IP network interface */
	u_short		ipp_vpi;	/* VPI value */
	u_short		ipp_vci;	/* VCI value */
	Aal_t		ipp_aal;	/* AAL type */
	Encaps_t	ipp_encaps;	/* VCC encapsulation */
	struct sockaddr_in ipp_dst;	/* Destination's IP address */
};


/*
 * Timer macros
 */
#define	IPVCC_TIMER(s, t)	atm_timeout(&(s)->iv_time, (t), ipatm_timeout)
#define	IPVCC_CANCEL(s)		atm_untimeout(&(s)->iv_time)

/*
 * Misc useful macros
 */
#define SATOSIN(sa)	((struct sockaddr_in *)(sa))


/*
 * Global function declarations
 */
	/* ipatm_event.c */
void		ipatm_timeout __P((struct atm_time *));
void		ipatm_connected __P((void *));
void		ipatm_cleared __P((void *, struct t_atm_cause *));
void		ipatm_arpnotify __P((struct ipvcc *, int));
void		ipatm_itimeout __P((struct atm_time *));

	/* ipatm_if.c */
int		ipatm_nifstat __P((int, struct atm_nif *, int));

	/* ipatm_input.c */
void		ipatm_cpcs_data __P((void *, KBuffer *));
int		ipatm_ipinput __P((struct ip_nif *, KBuffer *));

	/* ipatm_load.c */

	/* ipatm_output.c */
int		ipatm_ifoutput __P((struct ifnet *, KBuffer *,
			struct sockaddr *));

	/* ipatm_usrreq.c */
int		ipatm_ioctl __P((int, caddr_t, caddr_t));
caddr_t		ipatm_getname __P((void *));

	/* ipatm_vcm.c */
int		ipatm_openpvc __P((struct ipatmpvc *, struct ipvcc **));
int		ipatm_createsvc __P((struct ifnet *, u_short, caddr_t,
			struct ipvcc **));
int		ipatm_opensvc __P((struct ipvcc *));
int		ipatm_retrysvc __P((struct ipvcc *));
void		ipatm_activate __P((struct ipvcc *));
int		ipatm_incoming __P((void *, Atm_connection *, Atm_attributes *,
			void **));
int		ipatm_closevc __P((struct ipvcc *, int));
int		ipatm_chknif __P((struct in_addr, struct ip_nif *));
struct ipvcc	*ipatm_iptovc __P((struct sockaddr_in *, struct atm_nif *));
 

/*
 * External variables
 */
extern int 		ipatm_vccnt;
extern int 		ipatm_vcidle;
extern int 		ipatm_print;
extern u_long		last_map_ipdst;
extern struct ipvcc	*last_map_ipvcc;
extern struct ip_nif	*ipatm_nif_head;
extern struct sp_info	ipatm_vcpool;
extern struct sp_info	ipatm_nifpool;
extern struct ipatm_stat	ipatm_stat;
extern struct atm_time	ipatm_itimer;
extern Atm_endpoint	ipatm_endpt;
extern Atm_attributes	ipatm_aal5llc;
extern Atm_attributes	ipatm_aal5null;
extern Atm_attributes	ipatm_aal4null;

#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_harp_ip);
#endif

#endif	/* ATM_KERNEL */

#endif	/* _IPATM_IPATM_VAR_H */
