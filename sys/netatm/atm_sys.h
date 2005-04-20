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
 *	@(#) $FreeBSD$
 *
 */

/*
 * Core ATM Services
 * -----------------
 *
 * General system definitions
 *
 */

#ifndef _NETATM_ATM_SYS_H
#define _NETATM_ATM_SYS_H

/*
 * Software Version
 */
#define	ATM_VERSION	0x00030000	/* Version 3.0 */
#define	ATM_VERS_MAJ(v)	((v) >> 16)
#define	ATM_VERS_MIN(v)	((v) & 0xffff)


/*
 * Misc system defines
 */
#define	ATM_CALLQ_MAX	100		/* Maximum length of call queue */
#define	ATM_INTRQ_MAX	1000		/* Maximum length of interrupt queue */


/*
 * ATM address manipulation macros
 */
#define	ATM_ADDR_EQUAL(a1, a2)						\
	(((a1)->address_format == (a2)->address_format) && 		\
	 ((a1)->address_length == (a2)->address_length) && 		\
	 (bcmp((caddr_t)(a1)->address, (caddr_t)(a2)->address,	 	\
		(a1)->address_length) == 0))

#define	ATM_ADDR_SEL_EQUAL(a1, s1, a2)					\
	(((a1)->address_format == (a2)->address_format) && 		\
	 ((a1)->address_length == (a2)->address_length) && 		\
	 (((((a1)->address_format == T_ATM_ENDSYS_ADDR) ||		\
	    ((a1)->address_format == T_ATM_NSAP_ADDR)) &&		\
	   (bcmp((caddr_t)(a1)->address, (caddr_t)(a2)->address, 	\
		(a1)->address_length - 1) == 0) &&			\
	   ((s1) == ((struct atm_addr_nsap *)(a2)->address)->aan_sel)) || \
	  (((a1)->address_format != T_ATM_ENDSYS_ADDR) &&		\
	   ((a1)->address_format != T_ATM_NSAP_ADDR) &&			\
	   (bcmp((caddr_t)(a1)->address, (caddr_t)(a2)->address, 	\
		(a1)->address_length) == 0))))

#define	ATM_ADDR_COPY(a1, a2)						\
{									\
	(a2)->address_format = (a1)->address_format;			\
	(a2)->address_length = (a1)->address_length;			\
	bcopy((caddr_t)(a1)->address, (caddr_t)(a2)->address,		\
		(a1)->address_length);					\
}

#define	ATM_ADDR_SEL_COPY(a1, s1, a2)					\
{									\
	(a2)->address_format = (a1)->address_format;			\
	(a2)->address_length = (a1)->address_length;			\
	if (((a1)->address_format == T_ATM_ENDSYS_ADDR) || 		\
	    ((a1)->address_format == T_ATM_NSAP_ADDR)) {		\
		bcopy((caddr_t)(a1)->address, (caddr_t)(a2)->address,	\
			(a1)->address_length - 1);			\
		((struct atm_addr_nsap *)(a2)->address)->aan_sel = (s1);\
	} else {							\
		bcopy((caddr_t)(a1)->address, (caddr_t)(a2)->address,	\
			(a1)->address_length);				\
	}								\
}


/*
 * ATM Cell Header definitions
 */

/*
 * These macros assume that the cell header (minus the HEC)
 * is contained in the least-significant 32-bits of a word
 */
#define	ATM_HDR_SET_VPI(vpi)	(((vpi) & 0xff) << 20)
#define	ATM_HDR_SET_VCI(vci)	(((vci) & 0xffff) << 4)
#define	ATM_HDR_SET_PT(pt)	(((pt) & 0x7) << 1)
#define	ATM_HDR_SET_CLP(clp)	((clp) & 0x1)
#define	ATM_HDR_SET(vpi,vci,pt,clp)	(ATM_HDR_SET_VPI(vpi) | \
					ATM_HDR_SET_VCI(vci) | \
					ATM_HDR_SET_PT(pt) | \
					ATM_HDR_SET_CLP(clp))
#define	ATM_HDR_GET_VPI(hdr)	(((hdr) >> 20) & 0xff)
#define	ATM_HDR_GET_VCI(hdr)	(((hdr) >> 4) & 0xffff)
#define	ATM_HDR_GET_PT(hdr)	(((hdr) >> 1) & 0x7)
#define	ATM_HDR_GET_CLP(hdr)	((hdr) & 0x1)

/*
 * Payload Type Identifier (3 bits)
 */
#define	ATM_PT_USER_SDU0	0x0	/* User, no congestion, sdu type 0 */
#define	ATM_PT_USER_SDU1	0x1	/* User, no congestion, sdu type 1 */
#define	ATM_PT_USER_CONG_SDU0	0x2	/* User, congestion, sdu type 0 */
#define	ATM_PT_USER_CONG_SDU1	0x3	/* User, congestion, sdu type 1 */
#define	ATM_PT_NONUSER		0x4	/* User/non-user differentiator */
#define	ATM_PT_OAMF5_SEG	0x4	/* OAM F5 segment flow */
#define	ATM_PT_OAMF5_E2E	0x5	/* OAM F5 end-to-end flow */


/*
 * AAL (ATM Adaptation Layer) codes
 */
typedef u_char	Aal_t;
#define	ATM_AAL0	0		/* AAL0 - Cell service */
#define	ATM_AAL1	1		/* AAL1 */
#define	ATM_AAL2	2		/* AAL2 */
#define	ATM_AAL3_4	3		/* AAL3/4 */
#define	ATM_AAL5	5		/* AAL5 */


/*
 * VCC Encapsulation codes
 */
typedef u_char	Encaps_t;
#define	ATM_ENC_NULL	1		/* Null encapsulation */
#define	ATM_ENC_LLC	2		/* LLC encapsulation */


#ifdef _KERNEL
/*
 * ATM timer control block.  Used to schedule a timeout via atm_timeout().
 * This control block will typically be embedded in a processing-specific
 * control block.
 */
struct atm_time {
	u_short		ti_ticks;	/* Delta of ticks until timeout */
	u_char		ti_flag;	/* Timer flag bits (see below) */
	void 		(*ti_func)	/* Call at timeout expiration */
				(struct atm_time *);
	struct atm_time	*ti_next;	/* Next on queue */
};

/*
 * Timer Flags
 */
#define	TIF_QUEUED	0x01		/* Control block on timer queue */

#define	ATM_HZ		2		/* Time ticks per second */

/*
 * Debugging
 */
#ifdef DIAGNOSTIC
#define ATM_TIME							\
	struct timeval now, delta;					\
	KT_TIME(now);							\
	delta.tv_sec = now.tv_sec - atm_debugtime.tv_sec;		\
	delta.tv_usec = now.tv_usec - atm_debugtime.tv_usec;		\
	atm_debugtime = now;						\
	if (delta.tv_usec < 0) {					\
		delta.tv_sec--;						\
		delta.tv_usec += 1000000;				\
	}								\
	printf("%3ld.%6ld: ", delta.tv_sec, delta.tv_usec);

#define	ATM_DEBUG0(f)		if (atm_debug) {ATM_TIME; printf(f);}
#define	ATM_DEBUGN0(f)		if (atm_debug) {printf(f);}
#define	ATM_DEBUG1(f,a1)	if (atm_debug) {ATM_TIME; printf(f, a1);}
#define	ATM_DEBUGN1(f,a1)	if (atm_debug) {printf(f, a1);}
#define	ATM_DEBUG2(f,a1,a2)	if (atm_debug) {ATM_TIME; printf(f, a1, a2);}
#define	ATM_DEBUGN2(f,a1,a2)	if (atm_debug) {printf(f, a1, a2);}
#define	ATM_DEBUG3(f,a1,a2,a3)	if (atm_debug) {ATM_TIME; printf(f, a1, a2, a3);}
#define	ATM_DEBUGN3(f,a1,a2,a3)	if (atm_debug) {printf(f, a1, a2, a3);}
#define	ATM_DEBUG4(f,a1,a2,a3,a4)	if (atm_debug) {ATM_TIME; printf(f, a1, a2, a3, a4);}
#define	ATM_DEBUGN4(f,a1,a2,a3,a4)	if (atm_debug) {printf(f, a1, a2, a3, a4);}
#define	ATM_DEBUG5(f,a1,a2,a3,a4,a5)	if (atm_debug) {ATM_TIME; printf(f, a1, a2, a3, a4, a5);}
#define	ATM_DEBUGN5(f,a1,a2,a3,a4,a5)	if (atm_debug) {printf(f, a1, a2, a3, a4, a5);}
#else
#define	ATM_DEBUG0(f)
#define	ATM_DEBUGN0(f)
#define	ATM_DEBUG1(f,a1)
#define	ATM_DEBUGN1(f,a1)
#define	ATM_DEBUG2(f,a1,a2)
#define	ATM_DEBUGN2(f,a1,a2)
#define	ATM_DEBUG3(f,a1,a2,a3)
#define	ATM_DEBUGN3(f,a1,a2,a3)
#define	ATM_DEBUG4(f,a1,a2,a3,a4)
#define	ATM_DEBUGN4(f,a1,a2,a3,a4)
#define	ATM_DEBUG5(f,a1,a2,a3,a4,a5)
#define	ATM_DEBUGN5(f,a1,a2,a3,a4,a5)
#endif

#endif	/* _KERNEL */

#endif	/* _NETATM_ATM_SYS_H */
