/*-
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
 *	@(#) $FreeBSD: src/sys/netatm/atm_stack.h,v 1.6 2005/01/07 01:45:36 imp Exp $
 *
 */

/*
 * Core ATM Services
 * -----------------
 *
 * ATM Stack definitions
 *
 */

#ifndef _NETATM_ATM_STACK_H
#define _NETATM_ATM_STACK_H

#ifdef _KERNEL
/*
 * Structure used to define a kernel-provided ATM stack service and its
 * associated entry points.  Each stack service provider must register
 * themselves before they will be used.  ATM stack service providers include 
 * kernel modules (both linked and loaded) and device drivers, which must list
 * (via its atm_pif) any of its available hardware-supplied stack services 
 * (such as on-card AAL processing).
 */
struct stack_defn {
	struct stack_defn *sd_next;	/* Next in registry list */
	Sap_t		sd_sap;		/* Stack instance SAP */
	u_char		sd_flag;	/* Flags (see below) */
/* Exported functions */
	int		(*sd_inst)	/* Stack instantiation */
				(struct stack_defn **, Atm_connvc *);
	void		(*sd_lower)	/* Lower (from above) command handler */
				(int, void *, intptr_t, intptr_t);
	void		(*sd_upper)	/* Upper (from below) command handler */
				(int, void *, intptr_t, intptr_t);
/* Variables used during stack instantiation */
	void		*sd_toku;	/* Stack service instance token */
};

/*
 * Stack Service Flags
 */
#define	SDF_TERM	0x01		/* Terminal (to lowest layer) service */


/*
 * Stack Specification List  
 *
 * The list names the stack services and their layering relationships in
 * order to construct a stack to provide the protocol services defined
 * by the list.  The list is ordered starting from the stack service 
 * interfacing with the user "down" to the ATM cell service.
 */
#define	STACK_CNT	8		/* Max services in a stack list */
struct stack_list {
	Sap_t		sl_sap[STACK_CNT];	/* Stack service SAP list */
};


/*
 * Structure used during the construction and instantiation of a stack 
 * instance from a supplied stack list.  It contains pointers to the stack 
 * service definitions which will be used to implement the stack.  The first 
 * element in the array is reserved for the user's "stack service".
 */
struct stack_inst {
	struct stack_defn *si_srvc[STACK_CNT+1];	/* Assigned services */
};


/*
 * Macros to update buffer headroom values during stack instantiation.
 *
 * These values are advisory, i.e. every service must verify the amount
 * of available space in input/output messages and allocate new buffers
 * if needed.
 *
 * The 'maximum' and 'minimum' values used below may be chosen by a 
 * service to reflect the typical, expected message traffic pattern 
 * for a specific connection.
 * 
 * The macro arguments are:
 *	cvp = pointer to connection vcc;
 *	hi = maximum amount of buffer headroom required by the current
 *	     service during input message processing;
 *	si = minimum amount of buffer data stripped off the front 
 *	     of an input message by the current service;
 *	ho = maximum amount of buffer headroom required by the current
 *	     service during output message processing;
 *	ao = maximum amount of buffer data added to the front 
 *	     of an output message by the current service;
 */
#define HEADIN(cvp, hi, si)					\
{								\
	short	t = (cvp)->cvc_attr.headin - (si);		\
	t = (t >= (hi)) ? t : (hi);				\
	(cvp)->cvc_attr.headin = roundup(t, sizeof(long));	\
}

#define HEADOUT(cvp, ho, ao)					\
{								\
	short	t = (cvp)->cvc_attr.headout + (ao);		\
	t = (t >= (ho)) ? t : (ho);				\
	(cvp)->cvc_attr.headout = roundup(t, sizeof(long));	\
}


/*
 * Stack command codes - All stack command codes are specific to the 
 * defined stack SAP across which the command is used.  Command values 0-15 
 * are reserved for any common codes, which all stack SAPs must support.
 */
#define	STKCMD(s, d, v)	(((s) << 16) | (d) | (v))
#define	STKCMD_DOWN	0
#define	STKCMD_UP	0x00008000
#define	STKCMD_SAP_MASK	0xffff0000
#define	STKCMD_VAL_MASK	0x00007fff

/* Common command values (0-15) */
#define	CCV_INIT	1		/* DOWN */
#define	CCV_TERM	2		/* DOWN */

/* SAP_ATM */
#define	ATM_INIT		STKCMD(SAP_ATM, STKCMD_DOWN, CCV_INIT)
#define	ATM_TERM		STKCMD(SAP_ATM, STKCMD_DOWN, CCV_TERM)
#define	ATM_DATA_REQ		STKCMD(SAP_ATM, STKCMD_DOWN, 16)
#define	ATM_DATA_IND		STKCMD(SAP_ATM, STKCMD_UP, 17)

/* SAP_SAR */
#define	SAR_INIT		STKCMD(SAP_SAR, STKCMD_DOWN, CCV_INIT)
#define	SAR_TERM		STKCMD(SAP_SAR, STKCMD_DOWN, CCV_TERM)
#define	SAR_UNITDATA_INV	STKCMD(SAP_SAR, STKCMD_DOWN, 16)
#define	SAR_UNITDATA_SIG	STKCMD(SAP_SAR, STKCMD_UP, 17)
#define	SAR_UABORT_INV		STKCMD(SAP_SAR, STKCMD_DOWN, 18)
#define	SAR_UABORT_SIG		STKCMD(SAP_SAR, STKCMD_UP, 19)
#define	SAR_PABORT_SIG		STKCMD(SAP_SAR, STKCMD_UP, 20)

/* SAP_CPCS */
#define	CPCS_INIT		STKCMD(SAP_CPCS, STKCMD_DOWN, CCV_INIT)
#define	CPCS_TERM		STKCMD(SAP_CPCS, STKCMD_DOWN, CCV_TERM)
#define	CPCS_UNITDATA_INV	STKCMD(SAP_CPCS, STKCMD_DOWN, 16)
#define	CPCS_UNITDATA_SIG	STKCMD(SAP_CPCS, STKCMD_UP, 17)
#define	CPCS_UABORT_INV		STKCMD(SAP_CPCS, STKCMD_DOWN, 18)
#define	CPCS_UABORT_SIG		STKCMD(SAP_CPCS, STKCMD_UP, 19)
#define	CPCS_PABORT_SIG		STKCMD(SAP_CPCS, STKCMD_UP, 20)

/* SAP_SSCOP */
#define	SSCOP_INIT		STKCMD(SAP_SSCOP, STKCMD_DOWN, CCV_INIT)
#define	SSCOP_TERM		STKCMD(SAP_SSCOP, STKCMD_DOWN, CCV_TERM)
#define	SSCOP_ESTABLISH_REQ	STKCMD(SAP_SSCOP, STKCMD_DOWN, 16)
#define	SSCOP_ESTABLISH_IND	STKCMD(SAP_SSCOP, STKCMD_UP, 17)
#define	SSCOP_ESTABLISH_RSP	STKCMD(SAP_SSCOP, STKCMD_DOWN, 18)
#define	SSCOP_ESTABLISH_CNF	STKCMD(SAP_SSCOP, STKCMD_UP, 19)
#define	SSCOP_RELEASE_REQ	STKCMD(SAP_SSCOP, STKCMD_DOWN, 20)
#define	SSCOP_RELEASE_IND	STKCMD(SAP_SSCOP, STKCMD_UP, 21)
#define	SSCOP_RELEASE_CNF	STKCMD(SAP_SSCOP, STKCMD_UP, 22)
#define	SSCOP_DATA_REQ		STKCMD(SAP_SSCOP, STKCMD_DOWN, 23)
#define	SSCOP_DATA_IND		STKCMD(SAP_SSCOP, STKCMD_UP, 24)
#define	SSCOP_RESYNC_REQ	STKCMD(SAP_SSCOP, STKCMD_DOWN, 25)
#define	SSCOP_RESYNC_IND	STKCMD(SAP_SSCOP, STKCMD_UP, 26)
#define	SSCOP_RESYNC_RSP	STKCMD(SAP_SSCOP, STKCMD_DOWN, 27)
#define	SSCOP_RESYNC_CNF	STKCMD(SAP_SSCOP, STKCMD_UP, 28)
#define	SSCOP_RECOVER_IND	STKCMD(SAP_SSCOP, STKCMD_UP, 29)
#define	SSCOP_RECOVER_RSP	STKCMD(SAP_SSCOP, STKCMD_DOWN, 30)
#define	SSCOP_UNITDATA_REQ	STKCMD(SAP_SSCOP, STKCMD_DOWN, 31)
#define	SSCOP_UNITDATA_IND	STKCMD(SAP_SSCOP, STKCMD_UP, 32)
#define	SSCOP_RETRIEVE_REQ	STKCMD(SAP_SSCOP, STKCMD_DOWN, 33)
#define	SSCOP_RETRIEVE_IND	STKCMD(SAP_SSCOP, STKCMD_UP, 34)
#define	SSCOP_RETRIEVECMP_IND	STKCMD(SAP_SSCOP, STKCMD_UP, 35)

/* SAP_SSCF_UNI */
#define	SSCF_UNI_INIT		STKCMD(SAP_SSCF_UNI, STKCMD_DOWN, CCV_INIT)
#define	SSCF_UNI_TERM		STKCMD(SAP_SSCF_UNI, STKCMD_DOWN, CCV_TERM)
#define	SSCF_UNI_ESTABLISH_REQ	STKCMD(SAP_SSCF_UNI, STKCMD_DOWN, 16)
#define	SSCF_UNI_ESTABLISH_IND	STKCMD(SAP_SSCF_UNI, STKCMD_UP, 17)
#define	SSCF_UNI_ESTABLISH_CNF	STKCMD(SAP_SSCF_UNI, STKCMD_UP, 18)
#define	SSCF_UNI_RELEASE_REQ	STKCMD(SAP_SSCF_UNI, STKCMD_DOWN, 19)
#define	SSCF_UNI_RELEASE_IND	STKCMD(SAP_SSCF_UNI, STKCMD_UP, 20)
#define	SSCF_UNI_RELEASE_CNF	STKCMD(SAP_SSCF_UNI, STKCMD_UP, 21)
#define	SSCF_UNI_DATA_REQ	STKCMD(SAP_SSCF_UNI, STKCMD_DOWN, 22)
#define	SSCF_UNI_DATA_IND	STKCMD(SAP_SSCF_UNI, STKCMD_UP, 23)
#define	SSCF_UNI_UNITDATA_REQ	STKCMD(SAP_SSCF_UNI, STKCMD_DOWN, 24)
#define	SSCF_UNI_UNITDATA_IND	STKCMD(SAP_SSCF_UNI, STKCMD_UP, 25)


/*
 * The STACK_CALL macro must be used for all stack calls between adjacent
 * entities.  In order to avoid the problem with recursive stack calls 
 * modifying protocol state, this macro will only allow calls to proceed if 
 * they are not "against the flow" of any currently pending calls for a
 * stack instance.  If the requested call can't be processed now, it will 
 * be deferred and queued until a later, safe time (but before control is 
 * returned back to the kernel scheduler) when it will be dispatched.
 *
 * The STACK_CALL macro arguments are:
 *	cmd = command code;
 *	fn  = Destination entity processing function
 * 	tok = Destination layer's session token;
 *	cvp = Connection VCC address;
 *	a1  = command specific argument;
 *	a2  = command specific argument;
 *	ret = call result value (0 => success)
 *
 * The receiving entity command processing function prototype is:
 *
 * 	void (fn)(int cmd, int tok, int arg1, int arg2)
 *
 */
#define	STACK_CALL(cmd, fn, tok, cvp, a1, a2, ret)			\
{									\
	if ((cmd) & STKCMD_UP) {					\
		if ((cvp)->cvc_downcnt) {				\
			(ret) = atm_stack_enq((cmd), (fn), (tok), 	\
						(cvp), (a1), (a2));	\
		} else {						\
			(cvp)->cvc_upcnt++;				\
			(*fn)(cmd, tok, a1, a2);			\
			(cvp)->cvc_upcnt--;				\
			(ret) = 0;					\
		}							\
	} else {							\
		if ((cvp)->cvc_upcnt) {					\
			(ret) = atm_stack_enq((cmd), (fn), (tok), 	\
						(cvp), (a1), (a2));	\
		} else {						\
			(cvp)->cvc_downcnt++;				\
			(*fn)(cmd, tok, a1, a2);			\
			(cvp)->cvc_downcnt--;				\
			(ret) = 0;					\
		}							\
	}								\
}


/*
 * Stack queue entry - The stack queue will contain stack calls which have 
 * been deferred in order to avoid recursive calls to a single protocol 
 * control block.  The queue entries are allocated from its own storage pool.
 */
struct stackq_entry {
	struct stackq_entry *sq_next;	/* Next entry in queue */
	int		sq_cmd;		/* Stack command */
	void		(*sq_func)	/* Destination function */
				(int, void *, intptr_t, intptr_t);
	void		*sq_token;	/* Destination token */
	intptr_t	sq_arg1;	/* Command-specific argument */
	intptr_t	sq_arg2;	/* Command-specific argument */
	Atm_connvc	*sq_connvc;	/* Connection VCC */
};


/*
 * Macro to avoid unnecessary function call when draining the stack queue.
 */
#define	STACK_DRAIN()							\
{									\
	if (atm_stackq_head)						\
		atm_stack_drain();					\
}
#endif	/* _KERNEL */

#endif	/* _NETATM_ATM_STACK_H */
