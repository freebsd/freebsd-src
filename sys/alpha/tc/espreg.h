/* $FreeBSD$ */
/*	$NetBSD: espreg.h,v 1.2.4.1 1996/09/10 17:28:17 cgd Exp $	*/

/*
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Peter Galbavy.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Register addresses, relative to some base address
 */

#define	ESP_TCL		0x00		/* RW - Transfer Count Low	*/
#define	ESP_TCM		0x01		/* RW - Transfer Count Mid	*/
#define	ESP_TCH		0x0e		/* RW - Transfer Count High	*/
					/*	NOT on 53C90		*/

#define	ESP_FIFO	0x02		/* RW - FIFO data		*/

#define	ESP_CMD		0x03		/* RW - Command (2 deep)	*/
#define  ESPCMD_DMA	0x80		/*	DMA Bit			*/
#define  ESPCMD_NOP	0x00		/*	No Operation		*/
#define  ESPCMD_FLUSH	0x01		/*	Flush FIFO		*/
#define  ESPCMD_RSTCHIP	0x02		/*	Reset Chip		*/
#define  ESPCMD_RSTSCSI	0x03		/*	Reset SCSI Bus		*/
#define  ESPCMD_RESEL	0x40		/*	Reselect Sequence	*/
#define  ESPCMD_SELNATN	0x41		/*	Select without ATN	*/
#define  ESPCMD_SELATN	0x42		/*	Select with ATN		*/
#define  ESPCMD_SELATNS	0x43		/*	Select with ATN & Stop	*/
#define  ESPCMD_ENSEL	0x44		/*	Enable (Re)Selection	*/
#define  ESPCMD_DISSEL	0x45		/*	Disable (Re)Selection	*/
#define  ESPCMD_SELATN3	0x46		/*	Select with ATN3	*/
#define  ESPCMD_RESEL3	0x47		/*	Reselect3 Sequence	*/
#define  ESPCMD_SNDMSG	0x20		/*	Send Message		*/
#define  ESPCMD_SNDSTAT	0x21		/*	Send Status		*/
#define  ESPCMD_SNDDATA	0x22		/*	Send Data		*/
#define  ESPCMD_DISCSEQ	0x23		/*	Disconnect Sequence	*/
#define  ESPCMD_TERMSEQ	0x24		/*	Terminate Sequence	*/
#define  ESPCMD_TCCS	0x25		/*	Target Command Comp Seq	*/
#define  ESPCMD_DISC	0x27		/*	Disconnect		*/
#define  ESPCMD_RECMSG	0x28		/*	Receive Message		*/
#define  ESPCMD_RECCMD	0x29		/*	Receive Command 	*/
#define  ESPCMD_RECDATA	0x2a		/*	Receive Data		*/
#define  ESPCMD_RECCSEQ	0x2b		/*	Receive Command Sequence*/
#define  ESPCMD_ABORT	0x04		/*	Target Abort DMA	*/
#define  ESPCMD_TRANS	0x10		/*	Transfer Information	*/
#define  ESPCMD_ICCS	0x11		/*	Initiator Cmd Comp Seq 	*/
#define  ESPCMD_MSGOK	0x12		/*	Message Accepted	*/
#define  ESPCMD_TRPAD	0x18		/*	Transfer Pad		*/
#define  ESPCMD_SETATN	0x1a		/*	Set ATN			*/
#define  ESPCMD_RSTATN	0x1b		/*	Reset ATN		*/

#define	ESP_STAT	0x04		/* RO - Status			*/
#define  ESPSTAT_INT	0x80		/*	Interrupt		*/
#define  ESPSTAT_GE	0x40		/*	Gross Error		*/
#define  ESPSTAT_PE	0x20		/*	Parity Error		*/
#define  ESPSTAT_TC	0x10		/*	Terminal Count		*/
#define  ESPSTAT_VGC	0x08		/*	Valid Group Code	*/
#define  ESPSTAT_PHASE	0x07		/*	Phase bits		*/

#define	ESP_SELID	0x04		/* WO - Select/Reselect Bus ID	*/

#define	ESP_INTR	0x05		/* RO - Interrupt		*/
#define  ESPINTR_SBR	0x80		/*	SCSI Bus Reset		*/
#define  ESPINTR_ILL	0x40		/*	Illegal Command		*/
#define  ESPINTR_DIS	0x20		/*	Disconnect		*/
#define  ESPINTR_BS	0x10		/*	Bus Service		*/
#define  ESPINTR_FC	0x08		/*	Function Complete	*/
#define  ESPINTR_RESEL	0x04		/*	Reselected		*/
#define  ESPINTR_SELATN	0x02		/*	Select with ATN		*/
#define  ESPINTR_SEL	0x01		/*	Selected		*/

#define	ESP_TIMEOUT	0x05		/* WO - Select/Reselect Timeout */

#define	ESP_STEP	0x06		/* RO - Sequence Step		*/
#define  ESPSTEP_MASK	0x07		/*	the last 3 bits		*/
#define  ESPSTEP_DONE	0x04		/*	command went out	*/

#define	ESP_SYNCTP	0x06		/* WO - Synch Transfer Period	*/
					/*	Default 5 (53C9X)	*/

#define	ESP_FFLAG	0x07		/* RO - FIFO Flags		*/
#define  ESPFIFO_SS	0xe0		/*	Sequence Step (Dup)	*/
#define  ESPFIFO_FF	0x1f		/*	Bytes in FIFO		*/

#define	ESP_SYNCOFF	0x07		/* WO - Synch Offset		*/
					/*	0 = ASYNC		*/
					/*	1 - 15 = SYNC bytes	*/

#define	ESP_CFG1	0x08		/* RW - Configuration #1	*/
#define  ESPCFG1_SLOW	0x80		/*	Slow Cable Mode		*/
#define  ESPCFG1_SRR	0x40		/*	SCSI Reset Rep Int Dis	*/
#define  ESPCFG1_PTEST	0x20		/*	Parity Test Mod		*/
#define  ESPCFG1_PARENB	0x10		/*	Enable Parity Check	*/
#define  ESPCFG1_CTEST	0x08		/*	Enable Chip Test	*/
#define  ESPCFG1_BUSID	0x07		/*	Bus ID			*/

#define	ESP_CCF		0x09		/* WO -	Clock Conversion Factor	*/
					/*	0 = 35.01 - 40Mhz	*/
					/*	NEVER SET TO 1		*/
					/*	2 = 10Mhz		*/
					/*	3 = 10.01 - 15Mhz	*/
					/*	4 = 15.01 - 20Mhz	*/
					/*	5 = 20.01 - 25Mhz	*/
					/*	6 = 25.01 - 30Mhz	*/
					/*	7 = 30.01 - 35Mhz	*/

#define	ESP_TEST	0x0a		/* WO - Test (Chip Test Only)	*/

#define	ESP_CFG2	0x0b		/* RW - Configuration #2	*/
#define	 ESPCFG2_RSVD	0xa0		/*	reserved		*/
#define  ESPCFG2_FE	0x40		/* 	Features Enable		*/
#define  ESPCFG2_DREQ	0x10		/* 	DREQ High Impedance	*/
#define  ESPCFG2_SCSI2	0x08		/* 	SCSI-2 Enable		*/
#define  ESPCFG2_BPA	0x04		/* 	Target Bad Parity Abort	*/
#define  ESPCFG2_RPE	0x02		/* 	Register Parity Error	*/
#define  ESPCFG2_DPE	0x01		/* 	DMA Parity Error	*/

/* Config #3 only on 53C9X */
#define	ESP_CFG3	0x0c		/* RW - Configuration #3	*/
#define	 ESPCFG3_RSVD	0xe0		/*	reserved		*/
#define  ESPCFG3_IDM	0x10		/*	ID Message Res Check	*/
#define  ESPCFG3_QTE	0x08		/*	Queue Tag Enable	*/
#define  ESPCFG3_CDB	0x04		/*	CDB 10-bytes OK		*/
#define  ESPCFG3_FSCSI	0x02		/*	Fast SCSI		*/
#define  ESPCFG3_FCLK	0x01		/*	Fast Clock (>25Mhz)	*/
