/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_debug.h - i4b debug header file
 *	-----------------------------------
 *
 *	$Id: i4b_debug.h,v 1.32 2000/07/24 12:22:08 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Wed Oct 18 09:48:16 2000]
 *
 *---------------------------------------------------------------------------*/

#if !defined DO_I4B_DEBUG
# define DO_I4B_DEBUG 1		/* default = include debugging code */
#elif DO_I4B_DEBUG != 0
# undef DO_I4B_DEBUG
# define DO_I4B_DEBUG 1
#endif

#undef DO_I4B_MAXDEBUG		/* default = disable ALL debug messages */

#if DO_I4B_DEBUG

extern unsigned int i4b_l1_debug;
extern unsigned int i4b_l2_debug;
extern unsigned int i4b_l3_debug;
extern unsigned int i4b_l4_debug;

#define NDBGL1(bits, fmt, args...)				 	\
	if(bits & i4b_l1_debug)						\
	{ printf("i4b-L1 %s: " fmt "\n", __FUNCTION__ , ##args ); }

#define NDBGL2(bits, fmt, args...)				 	\
	if(bits & i4b_l2_debug)						\
	{ printf("i4b-L2 %s: " fmt "\n", __FUNCTION__ , ##args ); }
	
#define NDBGL3(bits, fmt, args...)				 	\
	if(bits & i4b_l3_debug)						\
	{ printf("i4b-L3 %s: " fmt "\n", __FUNCTION__ , ##args ); }
	
#define NDBGL4(bits, fmt, args...)				 	\
	if(bits & i4b_l4_debug)						\
	{ printf("i4b-L4 %s: " fmt "\n", __FUNCTION__ , ##args ); }
	
#else /* !DO_I4B_DEBUG */

#define NDBGL1(bits, fmt, args...);
#define NDBGL2(bits, fmt, args...);
#define NDBGL3(bits, fmt, args...);
#define NDBGL4(bits, fmt, args...);

#endif /* DO_I4B_DEBUG */

/* Layer 1 */

#define L1_ERROR	0x000001	/* general error message*/
#define L1_PRIM		0x000002	/* interlayer primitives*/
#define L1_BCHAN	0x000004	/* B channel action	*/
#define L1_H_ERR	0x000008	/* HSCX errors		*/
#define L1_H_IRQ	0x000010	/* HSCX IRQ messages	*/
#define L1_I_ERR	0x000020	/* ISAC errors		*/
#define L1_I_MSG	0x000040	/* ISAC messages	*/
#define L1_I_SETUP	0x000080	/* ISAC setup messages	*/
#define L1_F_MSG	0x000100	/* FSM messages		*/
#define L1_F_ERR	0x000200	/* FSM error messages	*/
#define L1_T_MSG	0x000400	/* Timer messages	*/
#define L1_T_ERR	0x000800	/* Timer error messages	*/
#define L1_H_XFRERR	0x001000	/* HSCX data xfer error */
#define L1_I_CICO	0x002000	/* ISAC command in/out	*/
#define L1_S_MSG	0x004000	/* silent messages (soft-HDLC)	*/
#define L1_S_ERR	0x008000	/* error messages (soft-HDLC)	*/
#define L1_HFC_DBG	0x010000	/* HFC-S PCI messages */
#define L1_DEBUG_MAX	0x01ffef	/* all messages on except IRQ!	*/
#define L1_DEBUG_ERR (L1_S_ERR | L1_H_ERR | L1_I_ERR | L1_F_ERR | L1_T_ERR | L1_ERROR)

#ifndef L1_DEBUG_DEFAULT
#ifdef DO_I4B_MAXDEBUG
#define L1_DEBUG_DEFAULT L1_DEBUG_MAX
#else
#define L1_DEBUG_DEFAULT L1_DEBUG_ERR
#endif
#endif

/* Layer 2 */

#define L2_ERROR	0x0001		/* general error message	*/
#define L2_PRIM		0x0002		/* interlayer primitives	*/
#define L2_U_MSG	0x0004		/* U frame messages		*/
#define L2_U_ERR	0x0008		/* U frame error messages	*/
#define L2_S_MSG	0x0010		/* S frame messages		*/
#define L2_S_ERR	0x0020		/* S frame error messages	*/
#define L2_I_MSG	0x0040		/* I frame messages		*/
#define L2_I_ERR	0x0080		/* I frame error messages	*/
#define L2_F_MSG	0x0100		/* FSM messages			*/
#define L2_F_ERR	0x0200		/* FSM error messages		*/
#define L2_T_MSG	0x0400		/* timer messages		*/
#define L2_T_ERR	0x0800		/* timer error messages		*/
#define L2_TEI_MSG	0x1000		/* TEI messages			*/
#define L2_TEI_ERR	0x2000		/* TEI error messages		*/

#define L2_DEBUG_MAX	0x3fff		/* all messages on		*/
#define L2_DEBUG_ERR (L2_ERROR | L2_I_ERR | L2_F_ERR | L2_T_ERR | L2_S_ERR | L2_TEI_ERR | L2_U_ERR )

#ifndef L2_DEBUG_DEFAULT
#ifdef DO_I4B_MAXDEBUG
#define L2_DEBUG_DEFAULT L2_DEBUG_MAX
#else
#define L2_DEBUG_DEFAULT L2_DEBUG_ERR
#endif
#endif

/* Layer 3 */

#define L3_ERR		0x0001		/* general error message	*/
#define L3_MSG		0x0002		/* general message		*/
#define L3_F_MSG	0x0004		/* FSM messages			*/
#define L3_F_ERR	0x0008		/* FSM error messages		*/
#define L3_T_MSG	0x0010		/* timer messages		*/
#define L3_T_ERR	0x0020		/* timer error messages		*/
#define L3_P_MSG	0x0040		/* protocol messages		*/
#define L3_P_ERR	0x0080		/* protocol error messages	*/
#define L3_A_MSG	0x0100		/* AOC messages			*/
#define L3_A_ERR	0x0200		/* AOC error messages		*/
#define L3_PRIM		0x0400		/* messages exchanged		*/

#define L3_DEBUG_MAX	0x07ff		/* all messages on	*/
#define L3_DEBUG_ERR	(L3_ERR | L3_F_ERR | L3_T_ERR | L3_P_ERR | L3_A_ERR)

#ifndef L3_DEBUG_DEFAULT
#ifdef DO_I4B_MAXDEBUG
#define L3_DEBUG_DEFAULT L3_DEBUG_MAX
#else
#define L3_DEBUG_DEFAULT L3_DEBUG_ERR
#endif
#endif

/* Layer 4 */

#define L4_ERR		0x0001		/* general error message	*/
#define L4_MSG		0x0002		/* general message		*/
#define L4_TIMO		0x0004		/* b channel idle timeout msgs	*/
#define L4_DIALST	0x0008		/* network driver dial states	*/
#define L4_IPRDBG	0x0010		/* ipr driver debug messages	*/
#define L4_RBCHDBG	0x0020		/* rbch driver debug messages	*/
#define L4_ISPDBG	0x0040		/* isp driver debug messages	*/
#define L4_TELDBG	0x0080		/* tel driver debug messages	*/
#define L4_TINADBG	0x0100		/* tina driver debug messages	*/
#define L4_TINAMSG	0x0200		/* tina driver messages		*/
#define L4_TINAERR	0x0400		/* tina driver error messages	*/
#define L4_INGDBG	0x0800		/* ing driver debug messages	*/

#define L4_DEBUG_MAX	0x0fff		/* all messages on	*/
#define L4_DEBUG_ERR	(L4_ERR | L4_TINADBG |  L4_TINAMSG | L4_TINAERR)

#ifndef L4_DEBUG_DEFAULT
#ifdef DO_I4B_MAXDEBUG
#define L4_DEBUG_DEFAULT L4_DEBUG_MAX
#else
#define L4_DEBUG_DEFAULT L4_DEBUG_ERR
#endif
#endif

/*---------------------------------------------------------------------------*
 * 	ioctl via /dev/i4bctl:
 *	get/set current debug bits settings
 *---------------------------------------------------------------------------*/

typedef struct {
	unsigned int	l1;
	unsigned int	l2;
	unsigned int	l3;
	unsigned int	l4;	
} ctl_debug_t;
	
#define	I4B_CTL_GET_DEBUG	_IOR('C', 0, ctl_debug_t)

#define	I4B_CTL_SET_DEBUG	_IOW('C', 1, ctl_debug_t)

/*---------------------------------------------------------------------------*
 *	generic get chipset statistics
 *---------------------------------------------------------------------------*/

/* for the ihfc-driver: structure for HFC-1/S/SP statistics */

typedef struct {
	int txframes;
	int rxframes;
	int xdu;
	int rdo;
	int crc;
	int rab;
} hfcstat_t;


/* for the isic-driver: structure for HSCX statistics */

typedef struct {
	int unit;	/* controller number */
	int chan;	/* channel number */
	int vfr;
	int rdo;
	int crc;
	int rab;
	int xdu;
	int rfo;
} hscxstat_t;

/* generic statistics structure */

struct chipstat {
	int driver_type;		/* type, L1DRVR_XXXX		*/
	int driver_unit;		/* the unit number		*/
	int driver_bchannel;		/* the B-channel		*/
	union stats {			/* union for all drivers	*/
		hfcstat_t  hfcstat;	/* for ihfc driver, L1DRVR_IHFC	*/
		hscxstat_t hscxstat;	/* for isic driver, L1DRVR_ISIC	*/
	} stats;
};

/* get statistics */

#define	I4B_CTL_GET_CHIPSTAT	_IOWR('C', 2, struct chipstat)

/* clear statistics */

#define	I4B_CTL_CLR_CHIPSTAT	_IOW('C', 3, struct chipstat)

/*---------------------------------------------------------------------------*
 *	get LAPD/Q.921 statistics
 *---------------------------------------------------------------------------*/
typedef struct {

	/* transmit */

	u_long	tx_i;		/* I 	*/
	u_long	tx_rr;		/* RR	*/
	u_long	tx_rnr;		/* RNR 	*/
	u_long	tx_rej;		/* REJ	*/
	u_long	tx_sabme;	/* SABME*/
	u_long	tx_dm;		/* DM	*/
	u_long	tx_disc;	/* DISC */
	u_long	tx_ua;		/* UA	*/
	u_long	tx_frmr;	/* FRMR	*/	
	u_long	tx_tei;		/* TEI	*/

	/* receive */

	u_long	rx_i;		/* I    */
	u_long	rx_rr;		/* RR   */
	u_long	rx_rnr;		/* RNR  */
	u_long	rx_rej;		/* REJ  */
	u_long	rx_sabme;	/* SABME*/
	u_long	rx_tei;		/* TEI	*/
	u_long	rx_ui;		/* UI	*/
	u_long	rx_disc;	/* DISC */
	u_long	rx_xid;		/* XID	*/
	u_long	rx_dm;		/* DM	*/
	u_long	rx_ua;		/* UA	*/
	u_long	rx_frmr;	/* FRMR	*/	
	
	/* errors */

	u_long	err_rx_len;	/* incorrect length */
	u_long	err_rx_badf;	/* bad frame type */
	u_long	err_rx_bads;	/* bad s frame */
	u_long	err_rx_badu;	/* bad u frame */
	u_long	err_rx_badui;	/* bad ui frame */
} lapdstat_t;

typedef struct {
	int unit;
	lapdstat_t lapdstat;
} l2stat_t;

#define	I4B_CTL_GET_LAPDSTAT	_IOWR('C', 4, l2stat_t)

#define	I4B_CTL_CLR_LAPDSTAT	_IOW('C', 5, int)

/* EOF */
