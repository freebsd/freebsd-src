/*	$FreeBSD$	*/
/*	$NecBSD: ncr53c500reg.h,v 1.5 1998/12/26 11:50:01 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	Kouichi Matsuda. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_NCR53C500REG_H_
#define	_NCR53C500REG_H_

/* Control Register Set 0 */
#define	NCVIOSZ		0x10

#define	cr0_tclsb	0x00	/* RW - Transfer Count Low	*/
#define	cr0_tcmsb	0x01	/* RW - Transfer Count Mid	*/
#define	cr0_sfifo	0x02	/* RW - FIFO data		*/
#define	cr0_cmd		0x03	/* RW - Command (2 deep)	*/
#define	cr0_stat	0x04	/* RO - Status			*/
#define	cr0_dstid	0x04	/* WO - Select/Reselect Bus ID	*/
#define	cr0_istat	0x05	/* RO - Interrupt		*/
#define	cr0_srtout	0x05	/* WO - Select/Reselect Timeout	*/
#define	cr0_seq		0x06	/* RO - Sequence Step		*/
#define	cr0_period	0x06	/* WO - Synch Transfer Period	*/
#define	cr0_sffl	0x07	/* RO - FIFO FLags		*/
#define	cr0_offs	0x07	/* WO - Synch Ofset		*/
#define	cr0_cfg1 	0x08	/* RW - Configuration #1	*/
#define	cr0_clk		0x09	/* WO - Clock Conversion Factor	*/
#define	cr0_tst		0x0a	/* WO - Test (Chip Test Only)	*/
#define	cr0_cfg2	0x0b	/* RW - Configuration #2	*/
#define	cr0_cfg3	0x0c	/* RW - Configuration #3	*/
#define	cr0_cfg4	0x0d	/* RW - Configuration #4	*/
#define	cr0_tchsb	0x0e	/* RW - Transfer Count High	*/
#define	cr0_fifo_bottom	0x0f	/* WO - FIFO bottom		*/

/* Control Register Set 1 */
#define	cr1_jumper	0x00	/* RW - Jumper Sense Port	*/
#define	cr1_sram_ptr	0x01	/* RW - SRAM Address Pointer	*/
#define	cr1_sram_data	0x02	/* RW - SRAM Data		*/
#define	cr1_fdata	0x04	/* RW - PIO FIFO		*/
#define	cr1_fstat	0x08	/* RW - PIO Status		*/
#define	cr1_atacmd	0x09	/* RW - ATA Command/Status	*/
#define	cr1_ataerr	0x0a	/* RW - ATA Features/Error	*/
#define	cr1_pflag	0x0b	/* RW - PIO Flag Interrupt Enable	*/
#define	cr1_cfg5	0x0d	/* RW - Configuration #5	*/
#define	cr1_sig		0x0e	/* RO - Signature		*/
#define	cr1_cfg6	0x0f	/* RW - Configuration #6	*/

/* atacmd (MPS110 ONLY) */
#define	ATACMD_POWDOWN	0x2d
#define	ATACMD_ENGAGE	0x24

/* cr0_sffl regster */
#define	CR0_SFFLR_BMASK	0x1f	/* scsi fifo byte mask */

/* cfg4 */
#define	C4_ANE		0x04

/* cfg3 */
#define	C3_NULL		0x00
#define	C3_FCLK		0x08	/* Fast SCSI		*/
#define	C3_FSCSI	0x10	/* Fast Clock (>25Mhz)	*/

/* cfg2 */
#define	C2_SCSI2	0x08	/* SCSI-2 Enable	*/
#define	C2_FE		0x40	/* Features Enable	*/

/* cfg1 */
#define	C1_SLOW		0x80	/* Slow Cable Mode	*/
#define	C1_SRR		0x40	/* SCSI Reset Rep Int Dis	*/
#define	C1_PARENB	0x10	/* Enable Parity Check	*/

/* clk factor */
#define	CLK_40M_F	0x00
#define	CLK_25M_F	0x05
#define	CLK_30M_F	0x06
#define	CLK_35M_F	0x07

/* interrupt status register */
#define	INTR_SBR	0x80	/* SCSI Bus Reset	*/
#define	INTR_ILL	0x40	/* Illegal Command	*/
#define	INTR_DIS	0x20	/* Disconnect		*/
#define	INTR_BS		0x10	/* Bus Service		*/
#define	INTR_FC		0x08	/* Function Complete	*/
#define	INTR_RESEL	0x04	/* Reselected		*/
#define	INTR_SELATN	0x02	/* Select with ATN	*/
#define	INTR_SEL	0x01	/* Selected		*/
#define	INTR_RESELECT	(INTR_RESEL | INTR_FC)

/* status register */
#define	STAT_INT	0x80	/* Interrupt		*/
#define	STAT_GE		0x40	/* Gross Error		*/
#define	STAT_PE		0x20	/* Parity Error		*/
#define	STAT_TC		0x10	/* Terminal Count	*/

/* phase bits */
#define	IOI			0x01
#define	CDI			0x02
#define	MSGI			0x04

/* Information transfer phases */
#define	DATA_OUT_PHASE		(0)
#define	DATA_IN_PHASE		(IOI)
#define	COMMAND_PHASE		(CDI)
#define	STATUS_PHASE		(CDI|IOI)
#define	MESSAGE_OUT_PHASE	(MSGI|CDI)
#define	MESSAGE_IN_PHASE	(MSGI|CDI|IOI)

#define	PHASE_MASK		(MSGI|CDI|IOI)

/* fifo status register */
#define	FIFO_SMASK	0x1e
#define	FIFO_E		0x10		/* fifo empty */
#define	FIFO_B		0x00		/* there exists any */
#define	FIFO_1		0x08		/* 1/3 <= bytes < 2/3 */
#define	FIFO_2		0x04		/* 2/3 <= bytes < full */
#define	FIFO_F		0x02		/* full */
#define	FIFO_EN		0x01		/* fifo direction */
#define	FIFO_BRK	0x40		/* phase miss */

#define	FIFO_F_SZ	128
#define	FIFO_1_SZ	44
#define	FIFO_2_SZ	84

/* pflags */
#define	PFR_WRITE	0x01

/* Commands */
#define	CMD_DMA		0x80	/* DMA Bit			*/
#define	CMD_NOP		0x00	/* No Operation			*/
#define	CMD_FLUSH	0x01	/* Flush FIFO			*/
#define	CMD_RSTCHIP	0x02	/* Reset Chip			*/
#define	CMD_RSTSCSI	0x03	/* Reset SCSI Bus		*/
#define	CMD_RESEL	0x40	/* Reselect Sequence		*/
#define	CMD_SELNATN	0x41	/* Select without ATN		*/
#define	CMD_SELATN	0x42	/* Select with ATN		*/
#define	CMD_SELATNS	0x43	/* Select with ATN & Stop	*/
#define	CMD_ENSEL	0x44	/* Enable (Re)Selection		*/
#define	CMD_DISSEL	0x45	/* Disable (Re)Selection	*/
#define	CMD_SELATN3	0x46	/* Select with ATN3		*/
#define	CMD_RESEL3	0x47	/* Reselect3 Sequence		*/
#define	CMD_SNDMSG	0x20	/* Send Message			*/
#define	CMD_SNDSTAT	0x21	/* Send Status			*/
#define	CMD_SNDDATA	0x22	/* Send Data			*/
#define	CMD_DISCSEQ	0x23	/* Disconnect Sequence		*/
#define	CMD_TERMSEQ	0x24	/* Terminate Sequence		*/
#define	CMD_TCCS	0x25	/* Target Command Comp Seq	*/
#define	CMD_DISC	0x27	/* Disconnect			*/
#define	CMD_RECMSG	0x28	/* Receive Message		*/
#define	CMD_RECCMD	0x29	/* Receive Command		*/
#define	CMD_RECDATA	0x2a	/* Receive Data			*/
#define	CMD_RECCSEQ	0x2b	/* Receive Command Sequence	*/
#define	CMD_ABORT	0x04	/* Target Abort DMA		*/
#define	CMD_TRANS	0x10	/* Transfer Information		*/
#define	CMD_ICCS	0x11	/* Initiator Cmd Comp Seq	*/
#define	CMD_MSGOK	0x12	/* Message Accepted		*/
#define	CMD_TRPAD	0x18	/* Transfer Pad			*/
#define	CMD_SETATN	0x1a	/* Set ATN			*/
#define	CMD_RSTATN	0x1b	/* Reset ATN			*/

/* Default timeout */
#define	SEL_TOUT 	0xa3
#endif	/* !_NCR53C500REG_H_ */
