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
 * IP/ATM service interface definitions
 *
 */

#ifndef _IPATM_IPATM_SERV_H
#define _IPATM_IPATM_SERV_H


/*
 * Structures specifying VCC parameters and pointers to all of the IP 
 * services offered by an external IP interface service provider.
 */
struct ip_vccparm {
	Aal_t		ivc_aal;		/* AAL type */
	Encaps_t	ivc_encaps;		/* VCC encapsulation */
};

#define	IPATM_VCCPARMS	4			/* Number of parameter lists */

struct ip_serv {
/* Interfaces to IP/ATM interface services */
	int		(*is_ifact)		/* Interface activation */
				(struct ip_nif *);
	int		(*is_ifdact)		/* Interface deactivation */
				(struct ip_nif *);
	int		(*is_ioctl)		/* Interface ioctl */
				(int, caddr_t, caddr_t);

/* Interfaces to IP/ATM ARP services */
	int		(*is_arp_pvcopen)	/* IP creating dynamic PVC */
				(struct ipvcc *);
	int		(*is_arp_svcout)	/* IP creating outgoing SVC */
				(struct ipvcc *, struct in_addr *);
	int		(*is_arp_svcin)		/* IP creating incoming SVC */
				(struct ipvcc *, Atm_addr *, Atm_addr *);
	int		(*is_arp_svcact)	/* IP SVC is active */
				(struct ipvcc *);
	void		(*is_arp_close)		/* IP closing VCC */
				(struct ipvcc *);

/* Interfaces to IP/ATM broadcast services */
	int		(*is_bcast_output)	/* IP broadcast packet output */
				(struct ip_nif *, KBuffer *);

/* Interfaces to IP/ATM multicast services */

/* Ordered list of parameters to try for IP/ATM VCC connections */
	struct ip_vccparm is_vccparm[IPATM_VCCPARMS];	/* List of vcc params */
};


/*
 * ARP Interface
 * ----------------
 */

/*
 * Common header for IP/ATM ARP mappings.  For each IP VCC created, the
 * appropriate IP/ATM ARP server must assign one of these structures to 
 * indicate the address mapping.  This is the only IP-visible ARP structure.
 * The servers may embed this structure at the beginning of their 
 * module-specific mappings.
 */
struct arpmap {
	struct in_addr	am_dstip;	/* Destination IP address */
	Atm_addr	am_dstatm;	/* Destination ATM address */
	Atm_addr	am_dstatmsub;	/* Destination ATM subaddress */
};


/*
 * is_arp_[ps]open() return codes and ipatm_arpnotify() event types
 */
#define	MAP_PROCEEDING	1		/* Lookup is proceeding (open only) */
#define	MAP_VALID	2		/* Mapping is valid */
#define	MAP_INVALID	3		/* Mapping is invalid */
#define	MAP_CHANGED	4		/* Mapping has changed */
#define	MAP_FAILED	5		/* Mapping request has failed */


#endif	/* _IPATM_IPATM_SERV_H */
