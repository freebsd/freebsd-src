/*
 * Copyright (c) 1993, 1994, 1995
 *	Rodney W. Grimes, Milwaukie, Oregon  97222.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Rodney W. Grimes.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RODNEY W. GRIMES ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL RODNEY W. GRIMES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: if_ixreg.h,v 1.7 1996/01/30 22:55:52 mpp Exp $
 */

/*
 * These really belong some place else, but I can't find them right
 * now.  I'll look again latter
 */

#define	IX_IO_PORTS	16	/* Number of I/O ports used, note
				 * this is not true, due to shadow
				 * ports at 400X,800X and C00X
				 */

#define	dxreg		0x00	/* Data transfer register	Word R/W */
#define	wrptr		0x02	/* Write address pointer	Word R/W */
#define	rdptr		0x04	/* Read address pointer		Word R/W */
#define	ca_ctrl		0x06	/* Channel attention control	Byte R/W */
#define	sel_irq		0x07	/* IRQ select			Byte R/W */
#define	  IRQ_ENABLE	  0x08	/* Enable board interrupts */
#define	smb_ptr		0x08	/* Shadow memory bank pointer	Word R/W */
#define	memdec		0x0A	/* Memory address decode	Byte   W */
#define	memctrl		0x0B	/* Memory mapped control	Byte R/W */
#define	  MEMCTRL_UNUSED  0x83	/* Unused bits */
#define	  MEMCTRL_MEMMEG  0x60	/* Which megabyte of memory, 0, E or F */
#define	  MEMCTRL_FMCS16  0x10	/* MEMCS16- for F000 */
#define	  MEMCTRL_MEMADJ  0xC0	/* memory adjust value */
#define	mempc		0x0C	/* MEMCS16- page control	Byte R/W */
#define	config		0x0D	/* Configuration, test		Byte R/W */
#define	 BART_LINK	  0x01	/* link integrity active, TPE */
#define	 BART_LOOPBACK	  0x02	/* Loopback, 0=none, 1=loopback */
#define	 SLOT_WIDTH	  0x04	/* 0 = 8bit, 1 = 16bit */
#define	 BART_USEWIDTH	  0x08	/* use SLOT_WIDTH for bus size */
#define	 BART_IOCHRDY_LATE 0x10 /* iochrdy late control bit */
#define	 BART_IO_TEST_EN  0x20	/* enable iochrdy timing test */
#define	 BART_IO_RESULT	  0x40	/* result of the iochrdy test */
#define	 BART_MCS16_TEST  0x80	/* enable memcs16 select test */
#define	ee_ctrl		0x0E	/* EEPROM control, reset	Byte R/W */
#define	  EENORMAL	  0x00	/* normal state of ee_ctrl */
#define	  EESK		  0x01	/* EEPROM clock bit */
#define	  EECS		  0x02	/* EEPROM chip select */
#define	  EEDI		  0x04	/* EEPROM data in bit (write EEPROM) */
#define	  EEDO		  0x08	/* EEPROM data out bit (read EEPROM) */
#define	  EEUNUSED	  0x30	/* unused bits in ee_ctrl */
#define	  GA_RESET	  0x40	/* BART ASIC chip reset pin */
#define	  I586_RESET	  0x80  /* 82586 chip reset pin */
#define	memectrl	0x0F	/* Memory control, E000h seg	Byte   W */
#define	autoid		0x0F	/* Auto ID register		Byte R   */

#define	BOARDID		0xBABA	/* Intel PCED board ID for EtherExpress */

#define	eeprom_opsize1		0x03	/* Size of opcodes for r/w/e */
#define	eeprom_read_op		0x06	/* EEPROM read op code */
#define	eeprom_write_op		0x05	/* EEPROM write op code */
#define	eeprom_erase_op		0x07	/* EEPROM erase op code */
#define	eeprom_opsize2		0x05	/* Size of opcodes for we/wdr */
#define	eeprom_wenable_op	0x13	/* EEPROM write enable op code */
#define	eeprom_wdisable_op	0x10	/* EEPROM write disable op code */

#define	eeprom_addr_size	0x06	/* Size of EEPROM address */

/* These are the locations in the EEPROM */
#define	eeprom_config1		0x00	/* Configuration register 1 */
#define	  CONNECT_BNCTPE	0x1000	/* 0 = AUI, 1 = BNC/TPE */
#define	  IRQ			  0xE000	/* Encoded IRQ */
#define	  IRQ_SHIFT		  13		/* To shift IRQ to lower bits */
#define	eeprom_lock_address	0x01	/* contains the lock bit */
#define	  EEPROM_LOCKED		 0x01	/* means that it is locked */
#define	eeprom_enetaddr_low	0x02	/* Ethernet address, low word */
#define	eeprom_enetaddr_mid	0x03	/* Ethernet address, middle word */
#define	eeprom_enetaddr_high	0x04	/* Ethernet address, high word */
#define	eeprom_config2		0x05	/* Configuration register 2 */
#define	  CONNECT_TPE		  0x0001	/* 0 = BNC, 1 = TPE */

/* this converts a kernal virtual address to a board offset */
#define	KVTOBOARD(addr)	((int)addr - (int)sc->maddr)
#define BOARDTOKV(addr) ((int)addr + (int)sc->maddr)

/* XXX This belongs is ic/i825x6.h, but is here for editing for now */

#define	INTEL586NULL	0xFFFF		/* NULL pointer for 82586 */
#define	INTEL596NULL	0xFFFFFFFF	/* NULL pointer for 82596 */

/*
 * Layout of memory for the 825x6 chip:
 * Low:		Control Blocks
 *		Transmit Frame Descriptor(s)
 *		Transmit Frame Buffer(s)
 *		Receive Frame Descriptors
 *		Receive Frames
 *		SCB_ADDR	System Control Block
 *		ISCP_ADDR	Intermediate System Configuration Pointer
 * High:	SCP_ADDR	System Configuration Pointer
 */
#define	SCP_ADDR	(sc->msize - sizeof(scp_t))
#define	ISCP_ADDR	(SCP_ADDR - sizeof(iscp_t))
#define	SCB_ADDR	(ISCP_ADDR - sizeof(scb_t))

#define	TB_COUNT	3	/* How many transfer buffers in the TFA */
#define TB_SIZE		(ETHER_MAX_LEN)	/* size of transmit buffer */
#define	TFA_START	0x0000	/* Start of the TFA */
#define	TFA_SIZE	(TB_COUNT * \
			(sizeof(cb_transmit_t) + sizeof(tbd_t) + TB_SIZE))

#define	RFA_START	(TFA_SIZE)
#define	RFA_SIZE	(SCP_ADDR - RFA_START)
#define	RB_SIZE		(ETHER_MAX_LEN)	/* size of receive buffer */

typedef struct /* System Configuration Pointer */
	{
	u_short	unused1;	/* should be zeros for 82596 compatibility */
	u_short	sysbus;		/* width of the 82586 data bus 0=16, 1=8 */
	u_short	unused2;	/* should be zeros for 82596 compatibility */
	u_short	unused3;	/* should be zeros for 82596 compatibility */
	u_long	iscp;		/* iscp address (24bit 586, 32bit 596) */
	}	scp_t;

typedef struct /* Intermediate System Configuration Pointer */
	{
	volatile
	u_short	busy;		/* Set to 1 by host before its first CA,
				   cleared by 82586 after reading */
#define	ISCP_BUSY	0x01	/* 82586 is busy reading the iscp */
	u_short	scb_offset;	/* Address of System Control Block */
	u_long	scb_base;	/* scb base address (24bit 586, 32bit 596) */
	}	iscp_t;

typedef struct /* System Control Block */
	{
	volatile
	u_short	status;		/* status bits */
#define	SCB_RUS_MASK	0x0070	/* receive unit status mask */
#define	SCB_RUS_IDLE	0x0000	/* receive unit status idle */
#define	SCB_RUS_SUSP	0x0010	/* receive unit status suspended */
#define	SCB_RUS_NRSC	0x0020	/* receive unit status no resources */
#define	SCB_RUS_READY	0x0040	/* receive unit status ready */
#define	SCB_CUS_MASK	0x0700	/* command unit status mask */
#define	SCB_CUS_IDLE	0x0000	/* command unit status idle */
#define	SCB_CUS_SUSP	0x0100	/* command unit status suspended */
#define	SCB_CUS_ACT	0x0200	/* command unit status active */
#define	SCB_STAT_MASK	0xF000	/* command unit status mask */
#define	SCB_STAT_RNR	0x1000	/* receive unit left the ready state */
#define	SCB_STAT_CNA	0x2000	/* command unit left the active state */
#define	SCB_STAT_FR	0x4000	/* the ru finished receiving a frame */
#define	SCB_STAT_CX	0x8000	/* the cu finished executing a command
				   with its I (interrupt) bit set */
#define	SCB_STAT_NULL	0x0000	/* used to clear the status work */
	u_short	command;	/* command bits */
#define	SCB_RUC_MASK	0x0070	/* receive unit command mask */
#define	SCB_RUC_NOP	0x0000	/* receive unit command nop */
#define	SCB_RUC_START	0x0010	/* receive unit command start */
#define	SCB_RUC_RESUME	0x0020	/* receive unit command resume */
#define	SCB_RUC_SUSP	0x0030	/* receive unit command suspend */
#define SCB_RUC_ABORT	0x0040	/* receive unit command abort */
#define SCB_RESET	0x0080	/* reset the chip, same as hardware reset */
#define	SCB_CUC_MASK	0x0700	/* command unit command mask */
#define	SCB_CUC_NOP	0x0000	/* command unit command nop */
#define	SCB_CUC_START	0x0100	/* start execution of the first command */
#define	SCB_CUC_RESUME	0x0200	/* resume execution of the next command */
#define	SCB_CUC_SUSP	0x0300	/* suspend execution after the current command */
#define	SCB_CUC_ABORT	0x0400	/* abort execution of the current command */
#define	SCB_ACK_MASK	0xF000	/* command unit acknowledge mask */
#define	SCB_ACK_RNR	0x1000	/* ack receive unit left the ready state */
#define	SCB_ACK_CNA	0x2000	/* ack command unit left the active state */
#define	SCB_ACK_FR	0x4000	/* ack the ru finished receiving a frame */
#define	SCB_ACK_CX	0x8000	/* ack the cu finished executing a command
				   with its I (interrupt) bit set */
	u_short	cbl_offset;	/* first command block on the cbl */
	u_short	rfa_offset;	/* receive frame area */
	volatile
	u_short	crc_errors;	/* frame was aligned, but bad crc */
	volatile
	u_short	aln_errors;	/* frame was not aligned, and had bad crc */
	volatile
	u_short	rsc_errors;	/* did not have resources to receive */
	volatile
	u_short	ovr_errors;	/* system bus was not available to receive */
	}	scb_t;

typedef struct /* command block - nop (also the common part of cb's */
	{
	volatile
	u_short	status;		/* status bits */
#define	CB_COLLISIONS	0x000F	/* the number of collisions that occured */
#define	CB_BIT4		0x0010	/* reserved by intel */
#define	CB_EXCESSCOLL	0x0020	/* the number of collisions > MAX allowed */
#define	CB_HEARTBEAT	0x0040	/* */
#define	CB_DEFER	0x0080	/* had to defer due to trafic */
#define	CB_DMAUNDER	0x0100	/* dma underrun */
#define	CB_NOCTS	0x0200	/* lost clear to send */
#define	CB_NOCS		0x0400	/* lost carrier sense */
#define	CB_LATECOLL	0x0800	/* late collision occured (82596 only) */
#define	CB_ABORT	0x1000	/* command was aborted by CUC abort command */
#define	CB_OK		0x2000	/* command executed without error */
#define	CB_BUSY		0x4000	/* command is being executed */
#define	CB_COMPLETE	0x8000	/* command completed */
	u_short	command;	/* command bits */
#define	CB_CMD_MASK	0x0007	/* command mask */
#define	CB_CMD_NOP	0x0000	/* nop command */
#define	CB_CMD_IAS	0x0001	/* individual address setup command */
#define	CB_CMD_CONF	0x0002	/* configure command */
#define	CB_CMD_MCAS	0x0003	/* multicast address setup command */
#define	CB_CMD_TRANSMIT	0x0004	/* transmit command */
#define	CB_CMD_TDR	0x0005	/* time domain reflectometry command */
#define	CB_CMD_DUMP	0x0006	/* dump command */
#define	CB_CMD_DIAGNOSE	0x0007	/* diagnose command */
#define	CB_CMD_INT	0x2000	/* interrupt when command completed */
#define	CB_CMD_SUSP	0x4000	/* suspend CU when command completed */
#define	CB_CMD_EL	0x8000	/* end of the command block list */
	u_short	next;		/* pointer to the next cb */
	}	cb_t;

typedef	struct /* command block - individual address setup command */
	{
	cb_t	common;		/* common part of all command blocks */
	u_char	source[ETHER_ADDR_LEN];
				/* ethernet hardware address */
	}	cb_ias_t;

typedef	struct /* command block - configure command */
	{
	cb_t	common;		/* common part of all command blocks */
	u_char	byte[12];	/* ZZZ this is ugly, but it works */
	}	cb_configure_t;

typedef	struct /* command block - multicast address setup command */
	{
	cb_t	common;		/* common part of all command blocks */
	}	cb_mcas_t;

typedef	struct /* command block - transmit command */
	{
	cb_t	common;		/* common part of all command blocks */
	u_short	tbd_offset;	/* transmit buffer descriptor offset */
	u_char	destination[ETHER_ADDR_LEN];
				/* ethernet destination address field */
	u_short	length;		/* ethernet length field */
	u_char	byte[16];	/* XXX stupid fill tell I fix the ixinit
				 * code for the special cb's */
	}	cb_transmit_t;

typedef	struct /* command block - tdr command */
	{
	cb_t	common;		/* common part of all command blocks */
	}	cb_tdr_t;

typedef	struct /* command block - dump command */
	{
	cb_t	common;		/* common part of all command blocks */
	}	cb_dump_t;

typedef	struct /* command block - diagnose command */
	{
	cb_t	common;		/* common part of all command blocks */
	}	cb_diagnose_t;

typedef struct /* Transmit Buffer Descriptor */
	{
	volatile
	u_short	act_count;	/* size of buffer actual count of valid bytes */
#define	TBD_STAT_EOF	0x8000	/* end of frame */
	u_short	next;		/* pointer to the next tbd */
	u_long	buffer;		/* transmit buffer address (24bit 586, 32bit 596) */
	}	tbd_t;

typedef	struct /* Receive Frame Descriptor */
	{
	volatile
	u_short	status;		/* status bits */
#define	RFD_BUSY	0x4000	/* frame is being received */
#define	RFD_COMPLETE	0x8000	/* this frame is complete */
	u_short	command;	/* command bits */
#define	RFD_CMD_SUSP	0x4000	/* suspend the ru after this rfd is used */
#define	RFD_CMD_EL	0x8000	/* end of the rfd list */
	u_short	next;		/* pointer to the next rfd */
	u_short	rbd_offset;	/* pointer to the first rbd for this frame */
	u_char	destination[6];	/* ethernet destination address */
	u_char	source[6];	/* ethernet source address */
	u_short	length;		/* ethernet length field */
	}	rfd_t;

typedef	struct /* Receive Buffer Descriptor */
	{
	volatile
	u_short	act_count;	/* Actual Count (size) and status bits */
#define	RBD_STAT_SIZE	0x3FFF	/* size mask */
#define	RBD_STAT_VALID	0x4000	/* act_count field is valid */
#define	RBD_STAT_EOF	0x8000	/* end of frame */
	u_short	next;		/* pointer to the next rbd */
	u_long	buffer;		/* receive buffer address */
	u_short	size;		/* size of buffer in bytes, must be even */
#define	RBD_SIZE_EL	0x8000	/* end of rbd list */
	}	rbd_t;

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * arpcom.ac_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
typedef struct
	{
	struct	arpcom arpcom;		/* Ethernet common part see net/if.h */
	int	iobase;			/* I/O base address for this interface */
	caddr_t	maddr;			/* Memory base address for this interface */
	int	msize;			/* Size of memory */
	int	flags;			/* Software state */
#define	IXF_NONE	0x00000000	/* Clear all flags */
#define	IXF_INITED	0x00000001	/* Device has been inited */
#define	IXF_BPFATTACHED	0x80000000	/* BPF has been attached */
	int	connector;		/* Type of connector used on board */
#define	AUI		0x00		/* Using AUI connector */
#define	BNC		0x01		/* Using BNC connector */
#define TPE		0x02		/* Using TPE connector */
	u_short	irq_encoded;		/* Encoded interrupt for use on bart */
	int	width;			/* Width of slot the board is in, these
					 * constants are defined to match what
					 * the 82586/596 wants in scp->sysbus */
#define	WIDTH_8		0x01		/* 8-bit slot */
#define WIDTH_16	0x00		/* 16-bit slot */
	cb_t	*cb_head;		/* head of cb list */
	cb_t	*cb_tail;		/* tail of cb list */
	tbd_t	*tbd_head;		/* head of the tbd list */
	tbd_t	*tbd_tail;		/* tail of the tbd list */
	rfd_t	*rfd_head;		/* head of the rfd list */
	rfd_t	*rfd_tail;		/* tail of the rfd list */
	rbd_t	*rbd_head;		/* head of the rbd list */
	rbd_t	*rbd_tail;		/* tail of the rbd list */
	struct kern_devconf	kdc;
	}	ix_softc_t;
