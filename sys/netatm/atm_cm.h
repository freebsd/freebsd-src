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
 * Core ATM Services
 * -----------------
 *
 * ATM Connection Management definitions
 *
 */

#ifndef _NETATM_ATM_CM_H
#define _NETATM_ATM_CM_H

/*
 * Forward declaration
 */
struct atm_connection;
struct atm_attributes;


#ifdef _KERNEL
/*
 * Structure used to define a kernel ATM endpoint service module and its 
 * associated entry points.  An endpoint service is defined as a kernel 
 * entity which will serve as the endpoint of an ATM connection, i.e. it is
 * responsible for issuing/receiving requests to/from the connection manager.
 */
struct atm_endpoint {
	struct atm_endpoint *ep_next;	/* Next in registry list */
	u_int		ep_id;		/* Endpoint ID (see below) */

/* Exported functions: Miscellaneous */
	int		(*ep_ioctl)	/* Ioctl */
				(int, caddr_t, caddr_t);
	caddr_t		(*ep_getname)	/* Get application/owner name */
				(void *);

/* Exported functions: Connection Manager Control API */
	void		(*ep_connected)	/* Call connected */
				(void *);
	void		(*ep_cleared)	/* Call cleared */
				(void *, struct t_atm_cause *);
	int		(*ep_incoming)	/* Incoming call */
				(void *, struct atm_connection *,
					struct atm_attributes *, void **);
	int		(*ep_addparty)	/* Add Party notification */
				(void *, int, int);
	int		(*ep_dropparty)	/* Drop Party notification */
				(void *, int, int);

/* Exported functions: Connection Manager Data API: CPCS */ 
	void		(*ep_cpcs_ctl)	/* Control operation */
				(int, void *, void *);
	void		(*ep_cpcs_data)	/* Received data */
				(void *, KBuffer *);

/* Exported functions: Connection Manager Data API: SAAL */ 
	void		(*ep_saal_ctl)	/* Control operation */
				(int, void *, void *);
	void		(*ep_saal_data)	/* Received data */
				(void *, KBuffer *);

/* Exported functions: Connection Manager Data API: SSCOP */ 
	void		(*ep_sscop_ctl)	/* Control operation */
				(int, void *, void *, void *);
	void		(*ep_sscop_data)	/* Received data */
				(void *, KBuffer *, u_int);
};
typedef struct atm_endpoint	Atm_endpoint;
#endif	/* _KERNEL */

/*
 * Endpoint IDs
 */
#define	ENDPT_UNKNOWN		0	/* Unknown */
#define	ENDPT_IP		1	/* IP over ATM */
#define	ENDPT_ATMARP		2	/* ATMARP */
#define	ENDPT_SPANS_SIG		3	/* SPANS Signalling */
#define	ENDPT_SPANS_CLS		4	/* SPANS CLS */
#define	ENDPT_UNI_SIG		5	/* UNI Signalling */
#define	ENDPT_SOCK_AAL5		6	/* Socket - AAL5 */
#define	ENDPT_SOCK_SSCOP	7	/* Socket - SSCOP */
#define	ENDPT_MAX		7


/*
 * ATM Connection Attributes
 *
 * Note: Attribute tag values are the same as the SVE_tag values.
 *       Unless otherwise specified, attribute field values are the same 
 *       as the corresponding socket option values.
 *       The above values are all defined in netatm/atm.h.  
 */

/* AAL Attributes */
struct t_atm_aal4 {
	int32_t		forward_max_SDU_size;
	int32_t		backward_max_SDU_size;
	int32_t		SSCS_type;
	int32_t		mid_low;
	int32_t		mid_high;
};

struct attr_aal {
	int		tag;		/* Attribute tag */
	Aal_t		type;		/* AAL type (discriminator) */
	union {
		struct t_atm_aal4 aal4;
		struct t_atm_aal5 aal5;
	} v;				/* Attribute value */
};

/* Traffic Descriptor Attributes */
struct attr_traffic {
	int		tag;		/* Attribute tag */
	struct t_atm_traffic	v;	/* Attribute value */
};

/* Broadband Bearer Attributes */
struct attr_bearer {
	int		tag;		/* Attribute tag */
	struct t_atm_bearer	v;	/* Attribute value */
};

/* Broadband High Layer Information Attributes */
struct attr_bhli {
	int		tag;		/* Attribute tag */
	struct t_atm_bhli	v;	/* Attribute value */
};

/* Broadband Low Layer Information Attributes */
struct attr_blli {
	int		tag_l2;		/* Layer 2 attribute tag */
	int		tag_l3;		/* Layer 3 attribute tag */
	struct t_atm_blli	v;	/* Attribute value */
};

/* Logical Link Control Attributes (multiplexing use only, not signalled) */
struct attr_llc {
	int		tag;		/* Attribute tag */
	struct t_atm_llc	v;	/* Attribute value */
};

/* Called Party Attributes */
struct attr_called {
	int		tag;		/* Attribute tag */
	Atm_addr	addr;		/* Called party address */
	Atm_addr	subaddr;	/* Called party subaddress */
};

/* Calling Party Attributes */
struct attr_calling {
	int		tag;		/* Attribute tag */
	Atm_addr	addr;		/* Calling party address */
	Atm_addr	subaddr;	/* Calling party subaddress */
	struct t_atm_caller_id	cid;	/* Caller ID */
};

/* Quality of Service Attributes */
struct attr_qos {
	int		tag;		/* Attribute tag */
	struct t_atm_qos	v;	/* Attribute value */
};

/* Transit Network Attributes */
struct attr_transit {
	int		tag;		/* Attribute tag */
	struct t_atm_transit	v;	/* Attribute value */
};

/* Cause Attributes */
struct attr_cause {
	int		tag;		/* Attribute tag */
	struct t_atm_cause	v;	/* Attribute value */
};


struct atm_attributes {
	struct atm_nif		*nif;	/* Network interface */
	u_int			api;	/* Connect Mgr Data API (see below) */
	int			api_init;/* API initialization parameter */
	u_short			headin;	/* Input buffer headroom */
	u_short			headout;/* Output buffer headroom */
	struct attr_aal		aal;	/* AAL attributes */
	struct attr_traffic	traffic;/* Traffic descriptor attributes */
	struct attr_bearer	bearer;	/* Broadband bearer attributes */
	struct attr_bhli	bhli;	/* Broadband high layer attributes */
	struct attr_blli	blli;	/* Broadband low layer attributes */
	struct attr_llc		llc;	/* Logical link control attributes */
	struct attr_called	called;	/* Called party attributes */
	struct attr_calling	calling;/* Calling party attributes */
	struct attr_qos		qos;	/* Quality of service attributes */
	struct attr_transit	transit;/* Transit network attributes */
	struct attr_cause	cause;	/* Cause attributes */
};
typedef struct atm_attributes	Atm_attributes;

/*
 * Connection Manager Data APIs
 */
#define	CMAPI_CPCS	0		/* AAL CPCS */
#define	CMAPI_SAAL	1		/* Signalling AAL */
#define	CMAPI_SSCOP	2		/* Reliable data (SSCOP) */


#ifdef _KERNEL
/*
 * ATM Connection Instance
 *
 * There will be one connection block for each endpoint <-> Connection Manager 
 * API instance.  Note that with connection multiplexors (e.g. LLC), there 
 * may be multiple connections per VCC.
 */
struct atm_connection {
	struct atm_connection *co_next;	/* Multiplexor/listen queue link */
	struct atm_connection *co_mxh;	/* Connection multiplexor head */
	u_char		co_flags;	/* Connection flags (see below) */
	u_char		co_state;	/* User <-> CM state (see below) */
	Encaps_t	co_mpx;		/* Multiplexor type */
	void		*co_toku;	/* Endpoint's session token */
	Atm_endpoint	*co_endpt;	/* Endpoint service */
	struct atm_connvc *co_connvc;	/* Connection VCC */
	struct attr_llc	co_llc;		/* Connection LLC header */
	Atm_attributes	*co_lattr;	/* Listening attributes */
};
typedef struct atm_connection	Atm_connection;

/*
 * Connection Flags
 */
#define	COF_P2P		0x01		/* Point-to-point */
#define	COF_P2MP	0x02		/* Point-to-multipoint */

/*
 * Endpoint <-> Connection Manager States
 */
#define	COS_FREE	0		/* Not allocated */
#define	COS_OUTCONN	1		/* Outgoing connection pending */
#define	COS_LISTEN	2		/* Listening for connection */
#define	COS_INCONN	3		/* Incoming connection pending */
#define	COS_INACCEPT	4		/* Incoming connection accepted */
#define	COS_ACTIVE	5		/* Connection active */
#define	COS_CLEAR	6		/* Connection is clearing */


/*
 * ATM Connection VCC Instance
 *
 * There will be one connection-vcc block for each VCC created by the
 * Connection Manager. For multiplexed connections, there may be multiple
 * connection blocks associated with each connection-vcc.  This block is
 * used to control the Connection Manager <-> VCC interface, including the 
 * interfaces to stack management and the signalling manager.
 */
struct atm_connvc {
	Qelem_t		cvc_q;		/* Queueing links */
	Atm_connection	*cvc_conn;	/* Connection head */
	struct vccb	*cvc_vcc;	/* VCC for connection */
	struct sigmgr	*cvc_sigmgr;	/* VCC signalling manager */
	u_char		cvc_flags;	/* Connection flags (see below) */
	u_char		cvc_state;	/* CM - VCC state (see below) */
	void		*cvc_tokl;	/* Stack lower layer token */
	void		(*cvc_lower)	/* Stack lower layer handler */
				(int, void *, int, int);
	u_short		cvc_upcnt;	/* Up stack calls in progress */
	u_short		cvc_downcnt;	/* Down stack calls in progress */
	KBuffer		*cvc_rcvq;	/* Packet receive queue */
	int		cvc_rcvqlen;	/* Receive queue length */
	Atm_attributes	cvc_attr;	/* VCC attributes */
	struct atm_time	cvc_time;	/* Timer controls */
};
typedef struct atm_connvc	Atm_connvc;

/*
 * Connection Flags
 */
#define	CVCF_ABORTING	0x01		/* VCC abort is pending */
#define	CVCF_INCOMQ	0x02		/* VCC is on incoming queue */
#define	CVCF_CONNQ	0x04		/* VCC is on connection queue */
#define	CVCF_CALLER	0x08		/* We are the call originator */

/*
 * Connection Manager <-> VCC States
 */
#define	CVCS_FREE	0		/* Not allocated */
#define	CVCS_SETUP	1		/* Call setup pending */
#define	CVCS_INIT	2		/* Stack INIT pending */
#define	CVCS_INCOMING	3		/* Incoming call present */
#define	CVCS_ACCEPT	4		/* Incoming call accepted */
#define	CVCS_REJECT	5		/* Incoming call rejected */
#define	CVCS_ACTIVE	6		/* Stack active */
#define	CVCS_RELEASE	7		/* Connection release pending */
#define	CVCS_CLEAR	8		/* Call has been cleared */
#define	CVCS_TERM	9		/* Stack TERM pending */


/*
 * Connection VCC variables
 */
#define	CVC_RCVQ_MAX	3		/* Max length of receive queue */


/*
 * Timer macros
 */
#define	CVC_TIMER(s, t)		atm_timeout(&(s)->cvc_time, (t), atm_cm_timeout)
#define	CVC_CANCEL(s)		atm_untimeout(&(s)->cvc_time)


/*
 * Connection Manager Statistics
 */
struct atm_cm_stat {
	u_long		cms_llcdrop;	/* Packets dropped by llc demux'ing */
	u_long		cms_llcid;	/* Packets with unknown llc id */
	u_long		cms_rcvconn;	/* Packets dropped, bad conn state */
	u_long		cms_rcvconnvc;	/* Packets dropped, bad connvc state */
};
#endif	/* _KERNEL */

#endif	/* _NETATM_ATM_CM_H */
