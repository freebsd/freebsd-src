/* $Id: ispreg.h,v 1.3 1998/04/14 17:51:32 mjacob Exp $ */
/*
 * Machine Independent (well, as best as possible) register
 * definitions for Qlogic ISP SCSI adapters.
 *
 *---------------------------------------
 * Copyright (c) 1997 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
 *---------------------------------------
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef	_ISPREG_H
#define	_ISPREG_H

/*
 * Hardware definitions for the Qlogic ISP  registers.
 */

/*
 * This defines types of access to various registers.
 *
 *  	R:		Read Only
 *	W:		Write Only
 *	RW:		Read/Write
 *
 *	R*, W*, RW*:	Read Only, Write Only, Read/Write, but only
 *			if RISC processor in ISP is paused.
 */

/*
 * Offsets for various register blocks.
 *
 * Sad but true, different architectures have different offsets.
 */

#define	BIU_REGS_OFF		0x00

#define	 PCI_MBOX_REGS_OFF		0x70
#define	 PCI_MBOX_REGS2100_OFF		0x10
#define	SBUS_MBOX_REGS_OFF		0x80

#define	 PCI_SXP_REGS_OFF		0x80
#define	SBUS_SXP_REGS_OFF		0x200

#define	 PCI_RISC_REGS_OFF		0x80
#define	SBUS_RISC_REGS_OFF		0x400

/*
 * NB:	The *_BLOCK definitions have no specific hardware meaning.
 *	They serve simply to note to the MD layer which block of
 *	registers offsets are being accessed.
 */

/*
 * Bus Interface Block Register Offsets
 */
#define	BIU_BLOCK	0x0100
#define	BIU_ID_LO	BIU_BLOCK+0x0	/* R  : Bus ID, Low */
#define		BIU2100_FLASH_ADDR	BIU_BLOCK+0x0
#define	BIU_ID_HI	BIU_BLOCK+0x2	/* R  : Bus ID, High */
#define		BIU2100_FLASH_DATA	BIU_BLOCK+0x2
#define	BIU_CONF0	BIU_BLOCK+0x4	/* R  : Bus Configuration #0 */
#define	BIU_CONF1	BIU_BLOCK+0x6	/* R  : Bus Configuration #1 */
#define		BIU2100_CSR		BIU_BLOCK+0x6
#define	BIU_ICR		BIU_BLOCK+0x8	/* RW : Bus Interface Ctrl */
#define	BIU_ISR		BIU_BLOCK+0xA	/* R  : Bus Interface Status */
#define	BIU_SEMA	BIU_BLOCK+0xC	/* RW : Bus Semaphore */
#define	BIU_NVRAM	BIU_BLOCK+0xE	/* RW : Bus NVRAM */
#define	CDMA_CONF	BIU_BLOCK+0x20	/* RW*: DMA Configuration */
#define		CDMA2100_CONTROL	CDMA_CONF
#define	CDMA_CONTROL	BIU_BLOCK+0x22	/* RW*: DMA Control */
#define	CDMA_STATUS 	BIU_BLOCK+0x24	/* R  : DMA Status */
#define	CDMA_FIFO_STS	BIU_BLOCK+0x26	/* R  : DMA FIFO Status */
#define	CDMA_COUNT	BIU_BLOCK+0x28	/* RW*: DMA Transfer Count */
#define	CDMA_ADDR0	BIU_BLOCK+0x2C	/* RW*: DMA Address, Word 0 */
#define	CDMA_ADDR1	BIU_BLOCK+0x2E	/* RW*: DMA Address, Word 1 */
/* these are for the 1040A cards */
#define	CDMA_ADDR2	BIU_BLOCK+0x30	/* RW*: DMA Address, Word 2 */
#define	CDMA_ADDR3	BIU_BLOCK+0x32	/* RW*: DMA Address, Word 3 */

#define	DDMA_CONF	BIU_BLOCK+0x40	/* RW*: DMA Configuration */
#define		TDMA2100_CONTROL	DDMA_CONF
#define	DDMA_CONTROL	BIU_BLOCK+0x42	/* RW*: DMA Control */
#define	DDMA_STATUS	BIU_BLOCK+0x44	/* R  : DMA Status */
#define	DDMA_FIFO_STS	BIU_BLOCK+0x46	/* R  : DMA FIFO Status */
#define	DDMA_COUNT_LO	BIU_BLOCK+0x48	/* RW*: DMA Xfer Count, Low */
#define	DDMA_COUNT_HI	BIU_BLOCK+0x4A	/* RW*: DMA Xfer Count, High */
#define	DDMA_ADDR0	BIU_BLOCK+0x4C	/* RW*: DMA Address, Word 0 */
#define	DDMA_ADDR1	BIU_BLOCK+0x4E	/* RW*: DMA Address, Word 1 */
/* these are for the 1040A cards */
#define	DDMA_ADDR2	BIU_BLOCK+0x50	/* RW*: DMA Address, Word 2 */
#define	DDMA_ADDR3	BIU_BLOCK+0x52	/* RW*: DMA Address, Word 3 */

#define	DFIFO_COMMAND	BIU_BLOCK+0x60	/* RW : Command FIFO Port */
#define		RDMA2100_CONTROL	DFIFO_COMMAND
#define	DFIFO_DATA	BIU_BLOCK+0x62	/* RW : Data FIFO Port */

/*
 * Bus Interface Block Register Definitions
 */
/* BUS CONFIGURATION REGISTER #0 */
#define	BIU_CONF0_HW_MASK		0x000F	/* Hardware revision mask */
/* BUS CONFIGURATION REGISTER #1 */

#define	BIU_SBUS_CONF1_PARITY		0x0100 	/* Enable parity checking */
#define	BIU_SBUS_CONF1_FCODE_MASK	0x00F0	/* Fcode cycle mask */

#define	BIU_PCI_CONF1_FIFO_128		0x0040	/* 128 bytes FIFO threshold */
#define	BIU_PCI_CONF1_FIFO_64		0x0030	/* 64 bytes FIFO threshold */
#define	BIU_PCI_CONF1_FIFO_32		0x0020	/* 32 bytes FIFO threshold */
#define	BIU_PCI_CONF1_FIFO_16		0x0010	/* 16 bytes FIFO threshold */
#define	BIU_BURST_ENABLE		0x0004	/* Global enable Bus bursts */
#define	BIU_SBUS_CONF1_FIFO_64		0x0003	/* 64 bytes FIFO threshold */
#define	BIU_SBUS_CONF1_FIFO_32		0x0002	/* 32 bytes FIFO threshold */
#define	BIU_SBUS_CONF1_FIFO_16		0x0001	/* 16 bytes FIFO threshold */
#define	BIU_SBUS_CONF1_FIFO_8		0x0000	/* 8 bytes FIFO threshold */
#define	BIU_SBUS_CONF1_BURST8		0x0008 	/* Enable 8-byte  bursts */
#define	BIU_PCI_CONF1_SXP		0x0008	/* SXP register select */

 /* ISP2100 Bus Control/Status Register */

#define	BIU2100_ICSR_REGBSEL		0x30	/* RW: register bank select */
#define		BIU2100_RISC_REGS	(0 << 4)	/* RISC Regs */
#define		BIU2100_FB_REGS		(1 << 4)	/* FrameBuffer Regs */
#define		BIU2100_FPM0_REGS	(2 << 4)	/* FPM 0 Regs */
#define		BIU2100_FPM1_REGS	(3 << 4)	/* FPM 1 Regs */
#define	BIU2100_PCI64			0x04	/*  R: 64 Bit PCI slot */
#define	BIU2100_FLASH_ENABLE		0x02	/* RW: Enable Flash RAM */
#define	BIU2100_SOFT_RESET		0x01
/* SOFT RESET FOR ISP2100 is same bit, but in this register, not ICR */


/* BUS CONTROL REGISTER */
#define	BIU_ICR_ENABLE_DMA_INT		0x0020	/* Enable DMA interrupts */
#define	BIU_ICR_ENABLE_CDMA_INT		0x0010	/* Enable CDMA interrupts */
#define	BIU_ICR_ENABLE_SXP_INT		0x0008	/* Enable SXP interrupts */
#define	BIU_ICR_ENABLE_RISC_INT		0x0004	/* Enable Risc interrupts */
#define	BIU_ICR_ENABLE_ALL_INTS		0x0002	/* Global enable all inter */
#define	BIU_ICR_SOFT_RESET		0x0001	/* Soft Reset of ISP */

#define	BIU2100_ICR_ENABLE_ALL_INTS	0x8000
#define	BIU2100_ICR_ENA_FPM_INT		0x0020
#define	BIU2100_ICR_ENA_FB_INT		0x0010
#define	BIU2100_ICR_ENA_RISC_INT	0x0008
#define	BIU2100_ICR_ENA_CDMA_INT	0x0004
#define	BIU2100_ICR_ENABLE_RXDMA_INT	0x0002
#define	BIU2100_ICR_ENABLE_TXDMA_INT	0x0001
#define	BIU2100_ICR_DISABLE_ALL_INTS	0x0000

#define	ENABLE_INTS(isp)	(isp->isp_type & ISP_HA_SCSI)?  \
 ISP_WRITE(isp, BIU_ICR, BIU_ICR_ENABLE_RISC_INT | BIU_ICR_ENABLE_ALL_INTS) : \
 ISP_WRITE(isp, BIU_ICR, BIU2100_ICR_ENA_RISC_INT | BIU2100_ICR_ENABLE_ALL_INTS)

#define	DISABLE_INTS(isp)	ISP_WRITE(isp, BIU_ICR, 0)

/* BUS STATUS REGISTER */
#define	BIU_ISR_DMA_INT			0x0020	/* DMA interrupt pending */
#define	BIU_ISR_CDMA_INT		0x0010	/* CDMA interrupt pending */
#define	BIU_ISR_SXP_INT			0x0008	/* SXP interrupt pending */
#define	BIU_ISR_RISC_INT		0x0004	/* Risc interrupt pending */
#define	BIU_ISR_IPEND			0x0002	/* Global interrupt pending */

#define	BIU2100_ISR_INT_PENDING		0x8000	/* Global interrupt pending */
#define	BIU2100_ISR_FPM_INT		0x0020	/* FPM interrupt pending */
#define	BIU2100_ISR_FB_INT		0x0010	/* FB interrupt pending */
#define	BIU2100_ISR_RISC_INT		0x0008	/* Risc interrupt pending */
#define	BIU2100_ISR_CDMA_INT		0x0004	/* CDMA interrupt pending */
#define	BIU2100_ISR_RXDMA_INT_PENDING	0x0002	/* Global interrupt pending */
#define	BIU2100_ISR_TXDMA_INT_PENDING	0x0001	/* Global interrupt pending */


/* BUS SEMAPHORE REGISTER */
#define	BIU_SEMA_STATUS		0x0002	/* Semaphore Status Bit */
#define	BIU_SEMA_LOCK  		0x0001	/* Semaphore Lock Bit */


/* COMNMAND && DATA DMA CONFIGURATION REGISTER */
#define	DMA_ENABLE_SXP_DMA		0x0008	/* Enable SXP to DMA Data */
#define	DMA_ENABLE_INTS			0x0004	/* Enable interrupts to RISC */
#define	DMA_ENABLE_BURST		0x0002	/* Enable Bus burst trans */
#define	DMA_DMA_DIRECTION		0x0001	/*
						 * Set DMA direction:
						 *	0 - DMA FIFO to host
						 *	1 - Host to DMA FIFO
						 */

/* COMMAND && DATA DMA CONTROL REGISTER */
#define	DMA_CNTRL_SUSPEND_CHAN		0x0010	/* Suspend DMA transfer */
#define	DMA_CNTRL_CLEAR_CHAN		0x0008	/*
						 * Clear FIFO and DMA Channel,
						 * reset DMA registers
						 */
#define	DMA_CNTRL_CLEAR_FIFO		0x0004	/* Clear DMA FIFO */
#define	DMA_CNTRL_RESET_INT		0x0002	/* Clear DMA interrupt */
#define	DMA_CNTRL_STROBE		0x0001	/* Start DMA transfer */

/*
 * Variants of same for 2100
 */
#define	DMA_CNTRL2100_CLEAR_CHAN	0x0004
#define	DMA_CNTRL2100_RESET_INT		0x0002



/* DMA STATUS REGISTER */
#define	DMA_SBUS_STATUS_PIPE_MASK	0x00C0	/* DMA Pipeline status mask */
#define	DMA_SBUS_STATUS_CHAN_MASK	0x0030	/* Channel status mask */
#define	DMA_SBUS_STATUS_BUS_PARITY	0x0008	/* Parity Error on bus */
#define	DMA_SBUS_STATUS_BUS_ERR		0x0004	/* Error Detected on bus */
#define	DMA_SBUS_STATUS_TERM_COUNT	0x0002	/* DMA Transfer Completed */
#define	DMA_SBUS_STATUS_INTERRUPT	0x0001	/* Enable DMA channel inter */

#define	DMA_PCI_STATUS_INTERRUPT	0x8000	/* Enable DMA channel inter */
#define	DMA_PCI_STATUS_RETRY_STAT	0x4000	/* Retry status */
#define	DMA_PCI_STATUS_CHAN_MASK	0x3000	/* Channel status mask */
#define	DMA_PCI_STATUS_FIFO_OVR		0x0100	/* DMA FIFO overrun cond */
#define	DMA_PCI_STATUS_FIFO_UDR		0x0080	/* DMA FIFO underrun cond */
#define	DMA_PCI_STATUS_BUS_ERR		0x0040	/* Error Detected on bus */
#define	DMA_PCI_STATUS_BUS_PARITY	0x0020	/* Parity Error on bus */
#define	DMA_PCI_STATUS_CLR_PEND		0x0010	/* DMA clear pending */
#define	DMA_PCI_STATUS_TERM_COUNT	0x0008	/* DMA Transfer Completed */
#define	DMA_PCI_STATUS_DMA_SUSP		0x0004	/* DMA suspended */
#define	DMA_PCI_STATUS_PIPE_MASK	0x0003	/* DMA Pipeline status mask */

/* DMA Status Register, pipeline status bits */
#define	DMA_SBUS_PIPE_FULL		0x00C0	/* Both pipeline stages full */
#define	DMA_SBUS_PIPE_OVERRUN		0x0080	/* Pipeline overrun */
#define	DMA_SBUS_PIPE_STAGE1		0x0040	/*
						 * Pipeline stage 1 Loaded,
						 * stage 2 empty
						 */
#define	DMA_PCI_PIPE_FULL		0x0003	/* Both pipeline stages full */
#define	DMA_PCI_PIPE_OVERRUN		0x0002	/* Pipeline overrun */
#define	DMA_PCI_PIPE_STAGE1		0x0001	/*
						 * Pipeline stage 1 Loaded,
						 * stage 2 empty
						 */
#define	DMA_PIPE_EMPTY			0x0000	/* All pipeline stages empty */

/* DMA Status Register, channel status bits */
#define	DMA_SBUS_CHAN_SUSPEND	0x0030	/* Channel error or suspended */
#define	DMA_SBUS_CHAN_TRANSFER	0x0020	/* Chan transfer in progress */
#define	DMA_SBUS_CHAN_ACTIVE	0x0010	/* Chan trans to host active */
#define	DMA_PCI_CHAN_TRANSFER	0x3000	/* Chan transfer in progress */
#define	DMA_PCI_CHAN_SUSPEND	0x2000	/* Channel error or suspended */
#define	DMA_PCI_CHAN_ACTIVE	0x1000	/* Chan trans to host active */
#define	ISP_DMA_CHAN_IDLE	0x0000	/* Chan idle (normal comp) */


/* DMA FIFO STATUS REGISTER */
#define	DMA_FIFO_STATUS_OVERRUN		0x0200	/* FIFO Overrun Condition */
#define	DMA_FIFO_STATUS_UNDERRUN	0x0100	/* FIFO Underrun Condition */
#define	DMA_FIFO_SBUS_COUNT_MASK	0x007F	/* FIFO Byte count mask */
#define	DMA_FIFO_PCI_COUNT_MASK		0x00FF	/* FIFO Byte count mask */

/*
 * Mailbox Block Register Offsets
 */

#define	MBOX_BLOCK	0x0200
#define	INMAILBOX0	MBOX_BLOCK+0x0
#define	INMAILBOX1	MBOX_BLOCK+0x2
#define	INMAILBOX2	MBOX_BLOCK+0x4
#define	INMAILBOX3	MBOX_BLOCK+0x6
#define	INMAILBOX4	MBOX_BLOCK+0x8
#define	INMAILBOX5	MBOX_BLOCK+0xA
#define	INMAILBOX6	MBOX_BLOCK+0xC
#define	INMAILBOX7	MBOX_BLOCK+0xE

#define	OUTMAILBOX0	MBOX_BLOCK+0x0
#define	OUTMAILBOX1	MBOX_BLOCK+0x2
#define	OUTMAILBOX2	MBOX_BLOCK+0x4
#define	OUTMAILBOX3	MBOX_BLOCK+0x6
#define	OUTMAILBOX4	MBOX_BLOCK+0x8
#define	OUTMAILBOX5	MBOX_BLOCK+0xA
#define	OUTMAILBOX6	MBOX_BLOCK+0xC
#define	OUTMAILBOX7	MBOX_BLOCK+0xE

#define	OMBOX_OFFN(n)	(MBOX_BLOCK + (n * 2))
#define	NMBOX(isp)	\
	(((((isp)->isp_type & ISP_HA_SCSI) >= ISP_HA_SCSI_1040A) || \
	 ((isp)->isp_type & ISP_HA_FC))? 8 : 6)
/*
 * Mailbox Command Complete Status Codes
 */
#define	MBOX_COMMAND_COMPLETE		0x4000
#define	MBOX_INVALID_COMMAND		0x4001
#define	MBOX_HOST_INTERFACE_ERROR	0x4002
#define	MBOX_TEST_FAILED		0x4003
#define	MBOX_COMMAND_ERROR		0x4005
#define	MBOX_COMMAND_PARAM_ERROR	0x4006

/*
 * Asynchronous event status codes
 */
#define	ASYNC_BUS_RESET			0x8001
#define	ASYNC_SYSTEM_ERROR		0x8002
#define	ASYNC_RQS_XFER_ERR		0x8003
#define	ASYNC_RSP_XFER_ERR		0x8004
#define	ASYNC_QWAKEUP			0x8005
#define	ASYNC_TIMEOUT_RESET		0x8006

/* for ISP2100 only */
#define	ASYNC_LIP_OCCURRED		0x8010
#define	ASYNC_LOOP_UP			0x8011
#define	ASYNC_LOOP_DOWN			0x8012
#define	ASYNC_LOOP_RESET		0x8013

/*
 * SXP Block Register Offsets
 */
#define	SXP_BLOCK	0x0400
#define	SXP_PART_ID		SXP_BLOCK+0x0	/* R  : Part ID Code */
#define	SXP_CONFIG1		SXP_BLOCK+0x2	/* RW*: Configuration Reg #1 */
#define	SXP_CONFIG2		SXP_BLOCK+0x4	/* RW*: Configuration Reg #2 */
#define	SXP_CONFIG3		SXP_BLOCK+0x6	/* RW*: Configuration Reg #2 */
#define	SXP_INSTRUCTION		SXP_BLOCK+0xC	/* RW*: Instruction Pointer */
#define	SXP_RETURN_ADDR		SXP_BLOCK+0x10	/* RW*: Return Address */
#define	SXP_COMMAND		SXP_BLOCK+0x14	/* RW*: Command */
#define	SXP_INTERRUPT		SXP_BLOCK+0x18	/* R  : Interrupt */
#define	SXP_SEQUENCE		SXP_BLOCK+0x1C	/* RW*: Sequence */
#define	SXP_GROSS_ERR		SXP_BLOCK+0x1E	/* R  : Gross Error */
#define	SXP_EXCEPTION		SXP_BLOCK+0x20	/* RW*: Exception Enable */
#define	SXP_OVERRIDE		SXP_BLOCK+0x24	/* RW*: Override */
#define	SXP_LITERAL_BASE	SXP_BLOCK+0x28	/* RW*: Literal Base */
#define	SXP_USER_FLAGS		SXP_BLOCK+0x2C	/* RW*: User Flags */
#define	SXP_USER_EXCEPT		SXP_BLOCK+0x30	/* RW*: User Exception */
#define	SXP_BREAKPOINT		SXP_BLOCK+0x34	/* RW*: Breakpoint */
#define	SXP_SCSI_ID		SXP_BLOCK+0x40	/* RW*: SCSI ID */
#define	SXP_DEV_CONFIG1		SXP_BLOCK+0x42	/* RW*: Device Config Reg #1 */
#define	SXP_DEV_CONFIG2		SXP_BLOCK+0x44	/* RW*: Device Config Reg #2 */
#define	SXP_PHASE_POINTER	SXP_BLOCK+0x48	/* RW*: SCSI Phase Pointer */
#define	SXP_BUF_POINTER		SXP_BLOCK+0x4C	/* RW*: SCSI Buffer Pointer */
#define	SXP_BUF_COUNTER		SXP_BLOCK+0x50	/* RW*: SCSI Buffer Counter */
#define	SXP_BUFFER		SXP_BLOCK+0x52	/* RW*: SCSI Buffer */
#define	SXP_BUF_BYTE		SXP_BLOCK+0x54	/* RW*: SCSI Buffer Byte */
#define	SXP_BUF_WORD		SXP_BLOCK+0x56	/* RW*: SCSI Buffer Word */
#define	SXP_BUF_WORD_TRAN	SXP_BLOCK+0x58	/* RW*: SCSI Buffer Wd xlate */
#define	SXP_FIFO		SXP_BLOCK+0x5A	/* RW*: SCSI FIFO */
#define	SXP_FIFO_STATUS		SXP_BLOCK+0x5C	/* RW*: SCSI FIFO Status */
#define	SXP_FIFO_TOP		SXP_BLOCK+0x5E	/* RW*: SCSI FIFO Top Resid */
#define	SXP_FIFO_BOTTOM		SXP_BLOCK+0x60	/* RW*: SCSI FIFO Bot Resid */
#define	SXP_TRAN_REG		SXP_BLOCK+0x64	/* RW*: SCSI Transferr Reg */
#define	SXP_TRAN_COUNT_LO	SXP_BLOCK+0x68	/* RW*: SCSI Trans Count */
#define	SXP_TRAN_COUNT_HI	SXP_BLOCK+0x6A	/* RW*: SCSI Trans Count */
#define	SXP_TRAN_COUNTER_LO	SXP_BLOCK+0x6C	/* RW*: SCSI Trans Counter */
#define	SXP_TRAN_COUNTER_HI	SXP_BLOCK+0x6E	/* RW*: SCSI Trans Counter */
#define	SXP_ARB_DATA		SXP_BLOCK+0x70	/* R  : SCSI Arb Data */
#define	SXP_PINS_CONTROL	SXP_BLOCK+0x72	/* RW*: SCSI Control Pins */
#define	SXP_PINS_DATA		SXP_BLOCK+0x74	/* RW*: SCSI Data Pins */
#define	SXP_PINS_DIFF		SXP_BLOCK+0x76	/* RW*: SCSI Diff Pins */


/* SXP CONF1 REGISTER */
#define	SXP_CONF1_ASYNCH_SETUP		0xF000	/* Asynchronous setup time */
#define	SXP_CONF1_SELECTION_UNIT	0x0000	/* Selection time unit */
#define	SXP_CONF1_SELECTION_TIMEOUT	0x0600	/* Selection timeout */
#define	SXP_CONF1_CLOCK_FACTOR		0x00E0	/* Clock factor */
#define	SXP_CONF1_SCSI_ID		0x000F	/* SCSI id */

/* SXP CONF2 REGISTER */
#define	SXP_CONF2_DISABLE_FILTER	0x0040	/* Disable SCSI rec filters */
#define	SXP_CONF2_REQ_ACK_PULLUPS	0x0020	/* Enable req/ack pullups */
#define	SXP_CONF2_DATA_PULLUPS		0x0010	/* Enable data pullups */
#define	SXP_CONF2_CONFIG_AUTOLOAD	0x0008	/* Enable dev conf auto-load */
#define	SXP_CONF2_RESELECT		0x0002	/* Enable reselection */
#define	SXP_CONF2_SELECT		0x0001	/* Enable selection */

/* SXP INTERRUPT REGISTER */
#define	SXP_INT_PARITY_ERR		0x8000	/* Parity error detected */
#define	SXP_INT_GROSS_ERR		0x4000	/* Gross error detected */
#define	SXP_INT_FUNCTION_ABORT		0x2000	/* Last cmd aborted */
#define	SXP_INT_CONDITION_FAILED	0x1000	/* Last cond failed test */
#define	SXP_INT_FIFO_EMPTY		0x0800	/* SCSI FIFO is empty */
#define	SXP_INT_BUF_COUNTER_ZERO	0x0400	/* SCSI buf count == zero */
#define	SXP_INT_XFER_ZERO		0x0200	/* SCSI trans count == zero */
#define	SXP_INT_INT_PENDING		0x0080	/* SXP interrupt pending */
#define	SXP_INT_CMD_RUNNING		0x0040	/* SXP is running a command */
#define	SXP_INT_INT_RETURN_CODE		0x000F	/* Interrupt return code */


/* SXP GROSS ERROR REGISTER */
#define	SXP_GROSS_OFFSET_RESID		0x0040	/* Req/Ack offset not zero */
#define	SXP_GROSS_OFFSET_UNDERFLOW	0x0020	/* Req/Ack offset underflow */
#define	SXP_GROSS_OFFSET_OVERFLOW	0x0010	/* Req/Ack offset overflow */
#define	SXP_GROSS_FIFO_UNDERFLOW	0x0008	/* SCSI FIFO underflow */
#define	SXP_GROSS_FIFO_OVERFLOW		0x0004	/* SCSI FIFO overflow */
#define	SXP_GROSS_WRITE_ERR		0x0002	/* SXP and RISC wrote to reg */
#define	SXP_GROSS_ILLEGAL_INST		0x0001	/* Bad inst loaded into SXP */

/* SXP EXCEPTION REGISTER */
#define	SXP_EXCEPT_USER_0		0x8000	/* Enable user exception #0 */
#define	SXP_EXCEPT_USER_1		0x4000	/* Enable user exception #1 */
#define	PCI_SXP_EXCEPT_SCAM		0x0400	/* SCAM Selection enable */
#define	SXP_EXCEPT_BUS_FREE		0x0200	/* Enable Bus Free det */
#define	SXP_EXCEPT_TARGET_ATN		0x0100	/* Enable TGT mode atten det */
#define	SXP_EXCEPT_RESELECTED		0x0080	/* Enable ReSEL exc handling */
#define	SXP_EXCEPT_SELECTED		0x0040	/* Enable SEL exc handling */
#define	SXP_EXCEPT_ARBITRATION		0x0020	/* Enable ARB exc handling */
#define	SXP_EXCEPT_GROSS_ERR		0x0010	/* Enable gross error except */
#define	SXP_EXCEPT_BUS_RESET		0x0008	/* Enable Bus Reset except */

	/* SXP OVERRIDE REGISTER */
#define	SXP_ORIDE_EXT_TRIGGER		0x8000	/* Enable external trigger */
#define	SXP_ORIDE_STEP			0x4000	/* Enable single step mode */
#define	SXP_ORIDE_BREAKPOINT		0x2000	/* Enable breakpoint reg */
#define	SXP_ORIDE_PIN_WRITE		0x1000	/* Enable write to SCSI pins */
#define	SXP_ORIDE_FORCE_OUTPUTS		0x0800	/* Force SCSI outputs on */
#define	SXP_ORIDE_LOOPBACK		0x0400	/* Enable SCSI loopback mode */
#define	SXP_ORIDE_PARITY_TEST		0x0200	/* Enable parity test mode */
#define	SXP_ORIDE_TRISTATE_ENA_PINS	0x0100	/* Tristate SCSI enable pins */
#define	SXP_ORIDE_TRISTATE_PINS		0x0080	/* Tristate SCSI pins */
#define	SXP_ORIDE_FIFO_RESET		0x0008	/* Reset SCSI FIFO */
#define	SXP_ORIDE_CMD_TERMINATE		0x0004	/* Terminate cur SXP com */
#define	SXP_ORIDE_RESET_REG		0x0002	/* Reset SXP registers */
#define	SXP_ORIDE_RESET_MODULE		0x0001	/* Reset SXP module */

/* SXP COMMANDS */
#define	SXP_RESET_BUS_CMD		0x300b

/* SXP SCSI ID REGISTER */
#define	SXP_SELECTING_ID		0x0F00	/* (Re)Selecting id */
#define	SXP_SELECT_ID			0x000F	/* Select id */

/* SXP DEV CONFIG1 REGISTER */
#define	SXP_DCONF1_SYNC_HOLD		0x7000	/* Synchronous data hold */
#define	SXP_DCONF1_SYNC_SETUP		0x0F00	/* Synchronous data setup */
#define	SXP_DCONF1_SYNC_OFFSET		0x000F	/* Synchronous data offset */


/* SXP DEV CONFIG2 REGISTER */
#define	SXP_DCONF2_FLAGS_MASK		0xF000	/* Device flags */
#define	SXP_DCONF2_WIDE			0x0400	/* Enable wide SCSI */
#define	SXP_DCONF2_PARITY		0x0200	/* Enable parity checking */
#define	SXP_DCONF2_BLOCK_MODE		0x0100	/* Enable blk mode xfr count */
#define	SXP_DCONF2_ASSERTION_MASK	0x0007	/* Assersion period mask */


/* SXP PHASE POINTER REGISTER */
#define	SXP_PHASE_STATUS_PTR		0x1000	/* Status buffer offset */
#define	SXP_PHASE_MSG_IN_PTR		0x0700	/* Msg in buffer offset */
#define	SXP_PHASE_COM_PTR		0x00F0	/* Command buffer offset */
#define	SXP_PHASE_MSG_OUT_PTR		0x0007	/* Msg out buffer offset */


/* SXP FIFO STATUS REGISTER */
#define	SXP_FIFO_TOP_RESID		0x8000	/* Top residue reg full */
#define	SXP_FIFO_ACK_RESID		0x4000	/* Wide transfers odd resid */
#define	SXP_FIFO_COUNT_MASK		0x001C	/* Words in SXP FIFO */
#define	SXP_FIFO_BOTTOM_RESID		0x0001	/* Bottom residue reg full */


/* SXP CONTROL PINS REGISTER */
#define	SXP_PINS_CON_PHASE		0x8000	/* Scsi phase valid */
#define	SXP_PINS_CON_PARITY_HI		0x0400	/* Parity pin */
#define	SXP_PINS_CON_PARITY_LO		0x0200	/* Parity pin */
#define	SXP_PINS_CON_REQ		0x0100	/* SCSI bus REQUEST */
#define	SXP_PINS_CON_ACK		0x0080	/* SCSI bus ACKNOWLEDGE */
#define	SXP_PINS_CON_RST		0x0040	/* SCSI bus RESET */
#define	SXP_PINS_CON_BSY		0x0020	/* SCSI bus BUSY */
#define	SXP_PINS_CON_SEL		0x0010	/* SCSI bus SELECT */
#define	SXP_PINS_CON_ATN		0x0008	/* SCSI bus ATTENTION */
#define	SXP_PINS_CON_MSG		0x0004	/* SCSI bus MESSAGE */
#define	SXP_PINS_CON_CD 		0x0002	/* SCSI bus COMMAND */
#define	SXP_PINS_CON_IO 		0x0001	/* SCSI bus INPUT */

/*
 * Set the hold time for the SCSI Bus Reset to be 250 ms
 */
#define	SXP_SCSI_BUS_RESET_HOLD_TIME	250

/* SXP DIFF PINS REGISTER */
#define	SXP_PINS_DIFF_SENSE		0x0200	/* DIFFSENS sig on SCSI bus */
#define	SXP_PINS_DIFF_MODE		0x0100	/* DIFFM signal */
#define	SXP_PINS_DIFF_ENABLE_OUTPUT	0x0080	/* Enable SXP SCSI data drv */
#define	SXP_PINS_DIFF_PINS_MASK		0x007C	/* Differential control pins */
#define	SXP_PINS_DIFF_TARGET		0x0002	/* Enable SXP target mode */
#define	SXP_PINS_DIFF_INITIATOR		0x0001	/* Enable SXP initiator mode */

/*
 * RISC and Host Command and Control Block Register Offsets
 */
#define	RISC_BLOCK	0x0800

#define	RISC_ACC	RISC_BLOCK+0x0	/* RW*: Accumulator */
#define	RISC_R1		RISC_BLOCK+0x2	/* RW*: GP Reg R1  */
#define	RISC_R2		RISC_BLOCK+0x4	/* RW*: GP Reg R2  */
#define	RISC_R3		RISC_BLOCK+0x6	/* RW*: GP Reg R3  */
#define	RISC_R4		RISC_BLOCK+0x8	/* RW*: GP Reg R4  */
#define	RISC_R5		RISC_BLOCK+0xA	/* RW*: GP Reg R5  */
#define	RISC_R6		RISC_BLOCK+0xC	/* RW*: GP Reg R6  */
#define	RISC_R7		RISC_BLOCK+0xE	/* RW*: GP Reg R7  */
#define	RISC_R8		RISC_BLOCK+0x10	/* RW*: GP Reg R8  */
#define	RISC_R9		RISC_BLOCK+0x12	/* RW*: GP Reg R9  */
#define	RISC_R10	RISC_BLOCK+0x14	/* RW*: GP Reg R10 */
#define	RISC_R11	RISC_BLOCK+0x16	/* RW*: GP Reg R11 */
#define	RISC_R12	RISC_BLOCK+0x18	/* RW*: GP Reg R12 */
#define	RISC_R13	RISC_BLOCK+0x1a	/* RW*: GP Reg R13 */
#define	RISC_R14	RISC_BLOCK+0x1c	/* RW*: GP Reg R14 */
#define	RISC_R15	RISC_BLOCK+0x1e	/* RW*: GP Reg R15 */
#define	RISC_PSR	RISC_BLOCK+0x20	/* RW*: Processor Status */
#define	RISC_IVR	RISC_BLOCK+0x22	/* RW*: Interrupt Vector */
#define	RISC_PCR	RISC_BLOCK+0x24	/* RW*: Processor Ctrl */
#define	RISC_RAR0	RISC_BLOCK+0x26	/* RW*: Ram Address #0 */
#define	RISC_RAR1	RISC_BLOCK+0x28	/* RW*: Ram Address #1 */
#define	RISC_LCR	RISC_BLOCK+0x2a	/* RW*: Loop Counter */
#define	RISC_PC		RISC_BLOCK+0x2c	/* R  : Program Counter */
#define	RISC_MTR	RISC_BLOCK+0x2e	/* RW*: Memory Timing */
#define		RISC_MTR2100	RISC_BLOCK+0x30

#define	RISC_EMB	RISC_BLOCK+0x30	/* RW*: Ext Mem Boundary */
#define	RISC_SP		RISC_BLOCK+0x32	/* RW*: Stack Pointer */
#define	RISC_HRL	RISC_BLOCK+0x3e	/* R *: Hardware Rev Level */
#define	HCCR		RISC_BLOCK+0x40	/* RW : Host Command & Ctrl */
#define	BP0		RISC_BLOCK+0x42	/* RW : Processor Brkpt #0 */
#define	BP1		RISC_BLOCK+0x44	/* RW : Processor Brkpt #1 */
#define	TCR		RISC_BLOCK+0x46	/*  W : Test Control */
#define	TMR		RISC_BLOCK+0x48	/*  W : Test Mode */


/* PROCESSOR STATUS REGISTER */
#define	RISC_PSR_FORCE_TRUE		0x8000
#define	RISC_PSR_LOOP_COUNT_DONE	0x4000
#define	RISC_PSR_RISC_INT		0x2000
#define	RISC_PSR_TIMER_ROLLOVER		0x1000
#define	RISC_PSR_ALU_OVERFLOW		0x0800
#define	RISC_PSR_ALU_MSB		0x0400
#define	RISC_PSR_ALU_CARRY		0x0200
#define	RISC_PSR_ALU_ZERO		0x0100
#define	RISC_PSR_DMA_INT		0x0010
#define	RISC_PSR_SXP_INT		0x0008
#define	RISC_PSR_HOST_INT		0x0004
#define	RISC_PSR_INT_PENDING		0x0002
#define	RISC_PSR_FORCE_FALSE  		0x0001


/* Host Command and Control */
#define	HCCR_CMD_NOP			0x0000	/* NOP */
#define	HCCR_CMD_RESET			0x1000	/* Reset RISC */
#define	HCCR_CMD_PAUSE			0x2000	/* Pause RISC */
#define	HCCR_CMD_RELEASE		0x3000	/* Release Paused RISC */
#define	HCCR_CMD_STEP			0x4000	/* Single Step RISC */
#define	HCCR_CMD_SET_HOST_INT		0x5000	/* Set Host Interrupt */
#define	HCCR_CMD_CLEAR_HOST_INT		0x6000	/* Clear Host Interrupt */
#define	HCCR_CMD_CLEAR_RISC_INT		0x7000	/* Clear RISC interrupt */
#define	HCCR_CMD_BREAKPOINT		0x8000	/* Change breakpoint enables */
#define	PCI_HCCR_CMD_BIOS		0x9000	/* Write BIOS (disable) */
#define	PCI_HCCR_CMD_PARITY		0xA000	/* Write parity enable */
#define	PCI_HCCR_CMD_PARITY_ERR		0xE000	/* Generate parity error */
#define	HCCR_CMD_TEST_MODE		0xF000	/* Set Test Mode */

#define	ISP2100_HCCR_PARITY_ENABLE_2	0x0400
#define	ISP2100_HCCR_PARITY_ENABLE_1	0x0200
#define	ISP2100_HCCR_PARITY_ENABLE_0	0x0100
#define	ISP2100_HCCR_PARITY		0x0001

#define	PCI_HCCR_PARITY			0x0400	/* Parity error flag */
#define	PCI_HCCR_PARITY_ENABLE_1	0x0200	/* Parity enable bank 1 */
#define	PCI_HCCR_PARITY_ENABLE_0	0x0100	/* Parity enable bank 0 */

#define	HCCR_HOST_INT			0x0080	/* R  : Host interrupt set */
#define	HCCR_RESET			0x0040	/* R  : reset in progress */
#define	HCCR_PAUSE			0x0020	/* R  : RISC paused */

#define	PCI_HCCR_BIOS			0x0001	/*  W : BIOS enable */
#endif	/* _ISPREG_H */
