/* $FreeBSD$ */
/* $NetBSD: tlsbreg.h,v 1.5 2000/01/27 22:27:50 mjacob Exp $ */

/*
 * Copyright (c) 1997, 2000 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Based in part upon a prototype version by Jason Thorpe
 * Copyright (c) 1996 by Jason Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * Definitions for the TurboLaser System Bus found on
 * AlphaServer 8200/8400 systems.
 */

/*
 * There are 9 TurboLaser nodes, 0 though 8.  Their uses are defined as
 * follows:
 *
 *	Node	Module
 *	----    ------
 *	0	CPU, Memory
 *	1	CPU, Memory
 *	2	CPU, Memory
 *	3	CPU, Memory
 *	4	CPU, Memory, I/O
 *	5	CPU, Memory, I/O
 *	6	CPU, Memory, I/O
 *	7	CPU, Memory, I/O
 *	8	I/O
 *
 * A node occurs every 0x00400000 bytes.
 *
 * Note, the AlphaServer 8200 only has nodes 4 though 8.
 */

#define TLSB_NODE_BASE		0x000000ff88000000	/* Dense */
#define TLSB_NODE_SIZE		0x00400000
#define TLSB_NODE_MAX		8	/* inclusive */

/* Translate a node number to an address. */
#define TLSB_NODE_ADDR(_node)					\
	(long)(TLSB_NODE_BASE + ((_node) * TLSB_NODE_SIZE))

#define TLSB_NODE_REG_ADDR(_node, _reg)				\
	KV((long)TLSB_NODE_ADDR((_node)) + (_reg))

/* Access the specified register on the specified node. */
#define TLSB_GET_NODEREG(_node, _reg)				\
	*(volatile u_int32_t *)(TLSB_NODE_REG_ADDR((_node), (_reg)))
#define TLSB_PUT_NODEREG(_node, _reg, _val)			\
	*(volatile u_int32_t *)(TLSB_NODE_REG_ADDR((_node), (_reg))) = (_val)

/*
 * Some registers are shared by all TurboLaser nodes, and appear in
 * the TurboLaser Broadcast space.
 */
#define TLSB_BCAST_BASE		0x000000ff8e000000	/* Dense */

#define TLSB_BCAST_REG_ADDR(_reg)	KV((long)(TLSB_BCASE_BASE + (_reg)))

/* Access the specified register in the broadcast space. */
#define TLSB_GET_BCASTREG(_reg)					\
	*(volatile u_int32_t *)(TLSB_BCAST_REG_ADDR + (_reg))
#define TLSB_PUT_BCASTREG(_reg, _val)				\
	*(volatile u_int32_t *)(TLSB_BCAST_REG_ADDR + (_reg)) = (_val)

/*
 * Location of the Gbus, the per-CPU bus containing the clock and
 * console hardware.
 */
#define TLSB_GBUS_BASE		0x000000ff90000000	/* Dense */

/*
 * Note that not every module type supports each TurboLaser register.
 * The following defines the keys used to denote module support for
 * a given register:
 *
 *	C	Supported by CPU module
 *	M	Supported by Memory module
 *	I	Supported by I/O module
 */

/*
 * Per-node TurboLaser System Bus registers, offsets from the
 * base of the node.
 */
#define TLDEV		0x0000		/* CMI: Device Register */
#define TLBER		0x0040		/* CMI: Bus Error Register */
#define TLCNR		0x0080		/* CMI: Congfiguration Register */
#define TLVID		0x00c0		/* CM: Virtual ID Register */
#define TLMMR0		0x0200		/* CM: Memory Mapping Register 0 */
#define TLMMR1		0x0240		/* CM: Memory Mapping Register 1 */
#define TLMMR2		0x0280		/* CM: Memory Mapping Register 2 */
#define TLMMR3		0x02c0		/* CM: Memory Mapping Register 3 */
#define TLMMR4		0x0300		/* CM: Memory Mapping Register 4 */
#define TLMMR5		0x0340		/* CM: Memory Mapping Register 5 */
#define TLMMR6		0x0380		/* CM: Memory Mapping Register 6 */
#define TLMMR7		0x03c0		/* CM: Memory Mapping Register 7 */
#define TLFADR0		0x0600		/* MI: Failing Address Register 0 */
#define TLFADR1		0x0640		/* MI: Failing Address Register 1 */
#define TLESR0		0x0680		/* CMI: Error Syndrome Register 0 */
#define TLESR1		0x06c0		/* CMI: Error Syndrome Register 1 */
#define TLESR2		0x0700		/* CMI: Error Syndrome Register 2 */
#define TLESR3		0x0740		/* CMI: Error Syndrome Register 3 */
#define TLILID0		0x0a00		/* I: Int. Level 0 IDENT Register */
#define TLILID1		0x0a40		/* I: Int. Level 1 IDENT Register */
#define TLILID2		0x0a80		/* I: Int. Level 2 IDENT Register */
#define TLILID3		0x0ac0		/* I: Int. Level 3 IDENT Register */
#define TLCPUMASK	0x0b00		/* I: CPU Interrupt Mask Register */
#define TLMBPTR		0x0c00		/* I: Mailbox Pointer Register */
#define	TLINTRMASK0	0x1100		/* C: Interrupt Mask Register CPU 0 */
#define	TLINTRMASK1	0x1140		/* C: Interrupt Mask Register CPU 1 */
#define	TLINTRSUM0	0x1180		/* C: Interrupt Sum Register CPU 0 */
#define	TLINTRSUM1	0x11C0		/* C: Interrupt Sum Register CPU 1 */
#define	TLEPAERR	0x1500		/* C: ADG error register */
#define	TLEPDERR	0x1540		/* C: DIGA error register */
#define	TLEPMERR	0x1580		/* C: MMG error register */
#define	TLDMCMD		0x1600		/* C: Data Mover Command */
#define	TLDMADRA	0x1680		/* C: Data Mover Source */
#define	TLDMADRB	0x16C0		/* C: Data Mover Destination */

/*
 * Registers shared between TurboLaser nodes, offsets from the
 * TurboLaser Broadcast Base.
 */
#define TLPRIVATE	0x0000		/* CMI: private "global" space */
#define TLIPINTR	0x0040		/* C: Interprocessor Int. Register */
#define TLIOINTR4	0x0100		/* C: I/O Interrupt Register 4 */
#define TLIOINTR5	0x0140		/* C: I/O Interrupt Register 5 */
#define TLIOINTR6	0x0180		/* C: I/O Interrupt Register 6 */
#define TLIOINTR7	0x01c0		/* C: I/O Interrupt Register 7 */
#define TLIOINTR8	0x0200		/* C: I/O Interrupt Register 8 */
#define TLWSDQR4	0x0400		/* C: Win Spc Dcr Que Ctr Reg 4 */
#define TLWSDQR5	0x0440		/* C: Win Spc Dcr Que Ctr Reg 5 */
#define TLWSDQR6	0x0480		/* C: Win Spc Dcr Que Ctr Reg 6 */
#define TLWSDQR7	0x04c0		/* C: Win Spc Dcr Que Ctr Reg 7 */
#define TLWSDQR8	0x0500		/* C: Win Spc Dcr Que Ctr Reg 8 */
#define TLRMDQRX	0x0600		/* C: Mem Chan Dcr Que Ctr Reg X */
#define TLRMDQR8	0x0640		/* C: Mem Chan Dcr Que Ctr Reg 8 */
#define TLRDRD		0x0800		/* C: CSR Read Data Rtn Data Reg */
#define TLRDRE		0x0840		/* C: CSR Read Data Rtn Error Reg */
#define TLMCR		0x1880		/* M: Memory Control Register */

/*
 * TLDEV - Device Register
 *
 * Access: R/W
 *
 * Notes:
 *	Register is loaded during initialization with information
 *	that identifies a node.  A zero value indicates a non-initialized
 *	(slot empty) node.
 *
 *	Bits 0-15 contain the hardware device type, bits 16-23
 *	the board's software revision, and bits 24-31 the board's
 *	hardware revision.
 *
 *	The device type portion is laid out as follows:
 *
 *		Bit 15: identifies a CPU
 *		Bit 14: identifies a memory board
 *		Bit 13: identifies an I/O board
 *		Bits 0-7: specify the ID of a node type
 */
#define TLDEV_DTYPE_MASK	0x0000ffff
#define TLDEV_DTYPE_KFTHA	0x2000		/* KFTHA board, I/O */
#define TLDEV_DTYPE_KFTIA	0x2020		/* KFTIA board, I/O */
#define TLDEV_DTYPE_MS7CC	0x5000		/* Memory board */
#define TLDEV_DTYPE_SCPU4	0x8011		/* 1 CPU, 4mb cache */
#define TLDEV_DTYPE_SCPU16	0x8012		/* 1 CPU, 16mb cache */
#define TLDEV_DTYPE_DCPU4	0x8014		/* 2 CPU, 4mb cache */
#define TLDEV_DTYPE_DCPU16	0x8015		/* 2 CPU, 16mb cache */

#define TLDEV_DTYPE(_val)	((_val) & TLDEV_DTYPE_MASK)
#	define	TLDEV_ISCPU(_val)	(TLDEV_DTYPE(_val) & 0x8000)
#	define	TLDEV_ISMEM(_val)	(TLDEV_DTYPE(_val) & 0x4000)
#	define	TLDEV_ISIOPORT(_val)	(TLDEV_DTYPE(_val) & 0x2000)
#define TLDEV_SWREV(_val)	(((_val) >> 16) & 0xff)
#define TLDEV_HWREV(_val)	(((_val) >> 24) & 0xff)

/*
 * TLBER - Bus Error Register
 *
 * Access: R/W
 *
 * Notes:
 *	This register contains information about TLSB errors detected by
 *	nodes on the TLSB.  The register will become locked when:
 *
 *		* Any error occurs and the "lock on first error"
 *		  bit of the Configuration Register is set.
 *
 *		* Any bit other than 20-23 (DS0-DS3) becomes set.
 *
 *	and will remain locked until either:
 *
 *		* All bits in the TLBER are cleared.
 *
 *		* The "lock on first error" bit is cleared.
 *
 *	TLBER locking is intended for diagnosic purposes only, and
 *	not for general use.
 */
#define TLBER_ATCE	0x00000001	/* Addr Transmit Ck Error */
#define TLBER_APE	0x00000002	/* Addr Parity Error */
#define TLBER_BAE	0x00000004	/* Bank Avail Violation Error */
#define TLBER_LKTO	0x00000008	/* Bank Lock Timeout */
#define TLBER_NAE	0x00000010	/* No Ack Error */
#define TLBER_RTCE	0x00000020	/* Read Transmit Ck Error */
#define TLBER_ACKTCE	0x00000040	/* Ack Transmit Ck Error */
#define TLBER_MMRE	0x00000080	/* Mem Mapping Register Error */
#define TLBER_FNAE	0x00000100	/* Fatal No Ack Error */
#define TLBER_REQDE	0x00000200	/* Request Deassertion Error */
#define TLBER_ATDE	0x00000400	/* Addredd Transmitter During Error */
#define TLBER_UDE	0x00010000	/* Uncorrectable Data Error */
#define TLBER_CWDE	0x00020000	/* Correctable Write Data Error */
#define TLBER_CRDE	0x00040000	/* Correctable Read Data Error */
#define TLBER_CRDE2	0x00080000	/* ...ditto... */
#define TLBER_DS0	0x00100000	/* Data Synd 0 */
#define TLBER_DS1	0x00200000	/* Data Synd 1 */
#define TLBER_DS2	0x00400000	/* Data Synd 2 */
#define TLBER_DS3	0x00800000	/* Data Synd 3 */
#define TLBER_DTDE	0x01000000	/* Data Transmitter During Error */
#define TLBER_FDTCE	0x02000000	/* Fatal Data Transmit Ck Error */
#define TLBER_UACKE	0x04000000	/* Unexpected Ack Error */
#define TLBER_ABTCE	0x08000000	/* Addr Bus Transmit Error */
#define TLBER_DCTCE	0x10000000	/* Data Control Transmit Ck Error */
#define TLBER_SEQE	0x20000000	/* Sequence Error */
#define TLBER_DSE	0x40000000	/* Data Status Error */
#define TLBER_DTO	0x80000000	/* Data Timeout Error */

/*
 * TLCNR - Configuration Register
 *
 * Access: R/W
 */
#define TLCNR_CWDD	0x00000001	/* Corr Write Data Err INTR Dis */
#define TLCNR_CRDD	0x00000002	/* Corr Read Data Err INTR Dis */
#define TLCNR_LKTOD	0x00000004	/* Bank Lock Timeout Disable */
#define TLCNR_DTOD	0x00000008	/* Data Timeout Disable */
#define TLCNR_STF_A	0x00001000	/* Self-Test Fail A */
#define TLCNR_STF_B	0x00002000	/* Self-Test Fail B */
#define TLCNR_HALT_A	0x00100000	/* Halt A */
#define TLCNR_HALT_B	0x00200000	/* Halt B */
#define TLCNR_RSTSTAT	0x10000000	/* Reset Status */
#define TLCNR_NRST	0x40000000	/* Node Reset */
#define TLCNR_LOFE	0x80000000	/* Lock On First Error */

#define TLCNR_NODE_MASK	0x000000f0	/* Node ID mask */
#define TLCNR_NODE_SHIFT	 4

#define TLCNR_VCNT_MASK	0x00000f00	/* VCNT mask */
#define TLCNR_VCNT_SHIFT	 8

/*
 * TLVID - Virtual ID Register
 *
 * Access: R/W
 *
 * Notes:
 *	Virtual units can be CPUs or Memory boards.  The units are
 *	are addressed using virtual IDs.  These virtual IDs are assigned
 *	by writing to the TLVID register.  The upper 24 bits of this
 *	register are reserved and must be written as `0'.
 */
#define TLVID_VIDA_MASK	0x0000000f	/* Virtual ID for unit 0 */
#define TLVID_VIDA_SHIFT	 0

#define TLVID_VIDB_MASK	0x000000f0	/* Virtual ID for unit 1 */
#define TLVID_VIDB_SHIFT	 4

/*
 * TLMMRn - Memory Mapping Registers
 *
 * Access: W
 *
 * Notes:
 *	Contains mapping information for doing a bank-decode.
 */
#define TLMMR_INTMASK	0x00000003	/* Valid bits in Interleave */
#define TLMMR_ADRMASK	0x000000f0	/* Valid bits in Address */
#define TLMMR_SBANK	0x00000800	/* Single-bank indicator */
#define TLMMR_VALID	0x80000000	/* Indicated mapping is valid */

#define TLMMR_INTLV_MASK 0x00000700	/* Mask for interleave value */
#define TLMMR_INTLV_SHIFT	  8

#define TLMMR_ADDRESS_MASK 0x03fff000	/* Mask for address value */
#define TLMMR_ADDRESS_SHIFT	   12

/*
 * TLFADRn - Failing Address Registers
 *
 * Access: R/W
 *
 * Notes:
 *	These registers contain status information for a failed address.
 *	Not all nodes preserve this information.  The validation bits
 *	indicate the validity of a given field.
 */


/*
 * CPU Interrupt Mask Register
 *
 * The PAL code reads this register for each CPU on a TLSB CPU board
 * to see what is or isn't enabled.
 */
#define	TLINTRMASK_CONHALT	0x100	/* Enable ^P Halt */
#define	TLINTRMASK_HALT		0x080	/* Enable Halt */
#define	TLINTRMASK_CLOCK	0x040	/* Enable Clock Interrupts */
#define	TLINTRMASK_XCALL	0x020	/* Enable Interprocessor Interrupts */
#define	TLINTRMASK_IPL17	0x010	/* Enable IPL 17 Interrupts */
#define	TLINTRMASK_IPL16	0x008	/* Enable IPL 16 Interrupts */
#define	TLINTRMASK_IPL15	0x004	/* Enable IPL 15 Interrupts */
#define	TLINTRMASK_IPL14	0x002	/* Enable IPL 14 Interrupts */
#define	TLINTRMASK_DUART	0x001	/* Enable GBUS Duart0 Interrupts */

/*
 * CPU Interrupt Summary Register
 *
 * The PAL code reads this register at interrupt time to figure out
 * which interrupt line to assert to the CPU. Note that when the
 * interrupt is actually vectored through the PAL code, it arrives
 * here already presorted as to type (clock, halt, iointr).
 */
#define	TLINTRSUM_HALT		(1 << 28)	/* Halted via TLCNR register */
#define	TLINTRSUM_CONHALT	(1 << 27)	/* Halted via ^P (W1C) */
#define	TLINTRSUM_CLOCK		(1 << 6)	/* Clock Interrupt (W1C) */
#define	TLINTRSUM_XCALL		(1 << 5)	/* Interprocessor Int (W1C) */
#define	TLINTRSUM_IPL17		(1 << 4)	/* IPL 17 Interrupt Summary */
#define	TLINTRSUM_IPL16		(1 << 3)	/* IPL 16 Interrupt Summary */
#define	TLINTRSUM_IPL15		(1 << 2)	/* IPL 15 Interrupt Summary */
#define	TLINTRSUM_IPL14		(1 << 1)	/* IPL 14 Interrupt Summary */
#define	TLINTRSUM_DUART		(1 << 0)	/* Duart Int (W1C) */
/* after checking the summaries, you can get the source node for each level */
#define	TLINTRSUM_IPL17_SOURCE(x)	((x >> 22) & 0x1f)
#define	TLINTRSUM_IPL16_SOURCE(x)	((x >> 17) & 0x1f)
#define	TLINTRSUM_IPL15_SOURCE(x)	((x >> 12) & 0x1f)
#define	TLINTRSUM_IPL14_SOURCE(x)	((x >> 7) & 0x1f)

/*
 * (some of) TurboLaser CPU ADG error register defines.
 */
#define	TLEPAERR_IBOX_TMO	0x1800	/* window space read failed */
#define	TLEPAERR_WSPC_RD	0x0600	/* window space read failed */

/*
 * (some of) TurboLaser CPU DIGA error register defines.
 */
#define	TLEPDERR_GBTMO		0x4	/* GBus timeout */
