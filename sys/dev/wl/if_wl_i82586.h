/*-
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 * $FreeBSD: src/sys/dev/wl/if_wl_i82586.h,v 1.1 2005/05/09 04:47:57 nyan Exp $
 */
/*
  Copyright 1988, 1989 by Olivetti Advanced Technology Center, Inc.,
Cupertino, California.

		All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Olivetti
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

  OLIVETTI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL OLIVETTI BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUR OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * Defines for managing the status word of the 82586 cpu.  For details see
 * the Intel LAN Component User's Manual starting at p. 2-14.
 *
 */

#define SCB_SW_INT	0xf000
#define SCB_SW_CX	0x8000		/* CU finished w/ int. bit set */
#define SCB_SW_FR	0x4000		/* RU finished receiving a frame */
#define SCB_SW_CNA	0x2000		/* CU left active state */
#define SCB_SW_RNR	0x1000		/* RU left ready state */

/* 
 * Defines for managing the Command Unit Status portion of the 82586
 * System Control Block.
 *
 */

#define SCB_CUS_IDLE	0x0000
#define SCB_CUS_SUSPND	0x0100
#define SCB_CUS_ACTV	0x0200

/* 
 * Defines for managing the Receive Unit Status portion of the System
 * Control Block.
 *
 */

#define SCB_RUS_IDLE	0x0000
#define SCB_RUS_SUSPND	0x0010
#define SCB_RUS_NORESRC 0x0020
#define SCB_RUS_READY	0x0040

/*
 * Defines that manage portions of the Command Word in the System Control
 * Block of the 82586.  Below are the Interrupt Acknowledge Bits and their
 * appropriate masks.
 *
 */

#define SCB_ACK_CX	0x8000
#define SCB_ACK_FR	0x4000
#define SCB_ACK_CNA	0x2000
#define SCB_ACK_RNR	0x1000

/* 
 * Defines for managing the Command Unit Control word, and the Receive
 * Unit Control word.  The software RESET bit is also defined.
 *
 */

#define SCB_CU_STRT	0x0100
#define SCB_CU_RSUM	0x0200
#define SCB_CU_SUSPND	0x0300
#define SCB_CU_ABRT	0x0400

#define SCB_RESET	0x0080

#define SCB_RU_STRT	0x0010
#define SCB_RU_RSUM	0x0020
#define SCB_RU_SUSPND	0x0030
#define SCB_RU_ABRT	0x0040


/*
 * The following define Action Commands for the 82586 chip.
 *
 */

#define	AC_NOP		0x00
#define AC_IASETUP	0x01
#define AC_CONFIGURE	0x02
#define AC_MCSETUP	0x03
#define AC_TRANSMIT	0x04
#define AC_TDR		0x05
#define AC_DUMP		0x06
#define AC_DIAGNOSE	0x07


/*
 * Defines for General Format for Action Commands, both Status Words, and
 * Command Words.
 *
 */

#define AC_SW_C		0x8000
#define AC_SW_B		0x4000
#define AC_SW_OK	0x2000
#define AC_SW_A		0x1000
#define TC_CARRIER	0x0400
#define TC_CLS		0x0200
#define TC_DMA		0x0100
#define TC_DEFER	0x0080
#define TC_SQE		0x0040
#define TC_COLLISION	0x0020
#define	AC_CW_EL	0x8000
#define AC_CW_S		0x4000
#define AC_CW_I		0x2000

/*
 * Specific defines for the transmit action command.
 *
 */

#define TBD_SW_EOF	0x8000
#define TBD_SW_COUNT	0x3fff

/*
 * Specific defines for the receive frame actions.
 *
 */

#define RBD_SW_EOF	0x8000
#define RBD_SW_COUNT	0x3fff

#define RFD_DONE	0x8000
#define RFD_BUSY	0x4000
#define RFD_OK		0x2000
#define RFD_CRC		0x0800
#define RFD_ALN		0x0400
#define RFD_RSC		0x0200
#define RFD_DMA		0x0100
#define RFD_SHORT	0x0080
#define RFD_EOF		0x0040
#define RFD_EL		0x8000
#define RFD_SUSP	0x4000
/*
 * 82586 chip specific structure definitions.  For details, see the Intel
 * LAN Components manual.
 *
 */


typedef	struct	{
	u_short	scp_sysbus;
	u_short	scp_unused[2];
	u_short	scp_iscp;
	u_short	scp_iscp_base;
} scp_t;


typedef	struct	{
	u_short	iscp_busy;
	u_short	iscp_scb_offset;
	u_short	iscp_scb;
	u_short	iscp_scb_base;
} iscp_t;


typedef struct	{
	u_short	scb_status;
	u_short	scb_command;
	u_short	scb_cbl_offset;
	u_short	scb_rfa_offset;
	u_short	scb_crcerrs;
	u_short	scb_alnerrs;
	u_short	scb_rscerrs;
	u_short	scb_ovrnerrs;
} scb_t;


typedef	struct	{
	u_short	tbd_offset;
	u_char	dest_addr[6];
	u_short	length;
} transmit_t;


typedef	struct	{
	u_short	fifolim_bytecnt;
	u_short	addrlen_mode;
	u_short	linprio_interframe;
	u_short	slot_time;
	u_short	hardware;
	u_short	min_frame_len;
} configure_t;


typedef	struct	{
	u_short	ac_status;
	u_short	ac_command;
	u_short	ac_link_offset;
	union	{
		transmit_t	transmit;
		configure_t	configure;
		u_char		iasetup[6];
	} cmd;
} ac_t;
	

typedef	struct	{
	u_short	act_count;
	u_short	next_tbd_offset;
	u_short	buffer_addr;
	u_short	buffer_base;
} tbd_t;


typedef	struct	{
	u_short	status;
	u_short	command;
	u_short	link_offset;
	u_short	rbd_offset;
	u_char	destination[6];
	u_char	source[6];
	u_short	length;
} fd_t;


typedef	struct	{
	u_short	status;
	u_short	next_rbd_offset;
	u_short	buffer_addr;
	u_short	buffer_base;
	u_short	size;
} rbd_t;
