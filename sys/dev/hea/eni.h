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
 * Efficient ENI Adapter Support
 *
 * Protocol and implementation definitions
 *
 */

#ifndef	_ENI_ENI_H
#define	_ENI_ENI_H

#include <pci/pcireg.h>
#include <pci/pcivar.h>

/*
 * Physical device name - used to configure HARP devices
 */
#ifndef	ENI_DEV_NAME
#define	ENI_DEV_NAME	"hea"		/* HARP Efficient ATM */
#endif

#define	ENI_MAX_UNITS	4

#define	ENI_IFF_MTU	9188
#define	ENI_MAX_VCI	1023		/* 0 - 1023 */
#define	ENI_MAX_VPI	0

#define	ENI_IFQ_MAXLEN	1000		/* rx/tx queue lengths */

#ifdef	BSD
/*
 * Size of small and large receive buffers
 */
#define	ENI_SMALL_BSIZE		64
#define	ENI_LARGE_BSIZE		MCLBYTES
#endif	/* BSD */

/*
 * ENI memory map offsets IN WORDS, not bytes
 *
 * The Efficient Adapter implements a 4 MB address space. The lower
 * 2 MB are used by bootprom (E)EPROM and by chipset registers such
 * as the MIDWAY and SUNI chips. The (upto) upper 2 MB is used for
 * RAM. Of the RAM, the lower 28 KB is used for fixed tables - the
 * VCI table, the RX and TX DMA queues, and the Service List queue.
 * Memory above the 28 KB range is available for RX and TX buffers.
 *
 * NOTE: Access to anything other then the (E)EPROM MUST be as a 32 bit
 * access. Also note that Efficient uses both byte addresses and word
 * addresses when describing offsets. BE CAREFUL or you'll get confused!
 */
/*
 * Size of memory space reserved for registers and expansion (e)eprom.
 */
#define	ENI_REG_SIZE	0x200000	/* Two megabytes */

#define	SUNI_OFFSET	0x008000	/* SUNI chip registers */
#define	MIDWAY_OFFSET	0x010000	/* MIDWAY chip registers */
#define	RAM_OFFSET	0x080000	/* Adapter RAM */
#define	VCITBL_OFFSET	0x080000	/* VCI Table offset */
#define	RXQUEUE_OFFSET	0x081000	/* RX DMA Queue offset */
#define	TXQUEUE_OFFSET	0x081400	/* TX DMA Queue offset */
#define	SVCLIST_OFFSET	0x081800	/* SVC List Queue offset */

#define	SEGBUF_BASE	0x007000	/* Base from start of RAM */

#define	DMA_LIST_SIZE	512		/* 1024 words / 2 words per entry */
#define	SVC_LIST_SIZE	1024		/* 1024 words / 1 word  per entry */

/*
 * Values for testing size of RAM on adapter
 *
 * Efficient has (at least) two different memory sizes available. One
 * is a client card which has either 128 KB or 512 KB RAM, the other
 * is a server card which has 2 MB RAM. The driver will size and test
 * the memory to correctly determine what's available.
 */
#define	MAX_ENI_MEM	0x200000	/* 2 MB - max. mem supported */
#define	TEST_STEP	0x000400	/* Look at 1 KB steps */
#define	TEST_PAT	0xA5A5A5A5	/* Test pattern */

/*
 * Values for memory allocator
 */
#define	ENI_BUF_PGSZ	1024		/* Allocation unit of buffers */
#define	ENI_BUF_NBIT	8		/* Number of bits to get from */
					/* min buffer (1KB) to max (128KB) */

/*
 * Values for allocating TX buffers
 */
#define	MAX_CLIENT_RAM	512		/* Most RAM a client card will have */
#define	TX_SMALL_BSIZE	32		/* Small buffer - 32KB */
#define	TX_LARGE_BSIZE	128		/* Large buffer - 128KB */

/*
 * Values for allocating RX buffers
 */
#define	RX_SIG_BSIZE	4		/* Signalling buffer - 4KB */
#define	RX_CLIENT_BSIZE	16		/* Client buffer - 16KB */
#define	RX_SERVER_BSIZE	32		/* Server buffer - 32KB */

/*
 * Adapter bases all addresses off of some power from 1KB. Thus, it
 * only needs to store the most sigificant bits and can drop the lower
 * 10 bits.
 */
#define	ENI_LOC_PREDIV	10		/* Bits location is shifted */
					/* Location is prescaled by 1KB */
					/* before use in various places */

#define	MIDWAY_DELAY	10		/* Time to wait for Midway finish */

/*
 * Define the MIDWAY register offsets and any interesting bits within
 * the register
 */
#define	MIDWAY_ID		0x00		/* ID/Reset register */
	#define	MIDWAY_RESET	0		/* iWrite of any value */
	#define	ID_SHIFT	27		/* Midway ID version */
	#define	ID_MASK		0x1F		/* ID mask */
	#define	MID_SHIFT	7		/* Mother board ID */
	#define	MID_MASK	0x7		/* MID mask */
	#define	DID_SHIFT	0		/* Daughter board ID */
	#define	DID_MASK	0x1F		/* DID mask */
	/*
	 * Efficient defines the following IDs for their adapters:
	 * 0x420/0x620 - SONET MMF, client memory size
	 * 0x430/0x630 - SONET MMF, server memory size
	 * 0x424/0x624 - UTP-5, client memory size
	 * 0x434/0x634 - UTP-5, server memory size
	 */
	#define	MEDIA_MASK	0x04		/* Mask off UTP-5/MMF media */

#define	MIDWAY_ISA		0x01		/* Interrupt Status Ack. */
						/* Reading this register */
						/* also acknowledges the */
						/* posted interrupt(s) */

#define	MIDWAY_IS		0x02		/* Interrupt Status */
						/* Reading this register */
						/* does NOT acknowledge the */
						/* posted interrupt(s) */
	/* Interrupt names */
	#define	ENI_INT_STAT		0x00000001
	#define	ENI_INT_SUNI		0x00000002
	#define	ENI_INT_SERVICE		0x00000004
	#define	ENI_INT_TX_DMA		0x00000008
	#define	ENI_INT_RX_DMA		0x00000010
	#define	ENI_INT_DMA_ERR		0x00000020
	#define	ENI_INT_DMA_LERR	0x00000040
	#define	ENI_INT_IDEN		0x00000080
	#define	ENI_INT_DMA_OVFL	0x00000100
	#define	ENI_INT_TX_MASK		0x0001FE00

#define	MIDWAY_IE		0x03		/* Interrupt Enable register */
	/* Interrupt enable bits are the same as the Interrupt names */

#define	MIDWAY_MASTER		0x04		/* Master Control */
	/* Master control bits */
	#define	ENI_M_WAIT500	0x00000001	/* Disable interrupts .5 ms */
	#define	ENI_M_WAIT1	0x00000002	/* Disable interrupts 1 ms */
	#define	ENI_M_RXENABLE	0x00000004	/* Enable RX engine */
	#define	ENI_M_TXENABLE	0x00000008	/* Enable TX engine */
	#define	ENI_M_DMAENABLE	0x00000010	/* Enable DMA */
	#define	ENI_M_TXLOCK	0x00000020	/* 0: Streaming, 1: Lock */
	#define	ENI_M_INTSEL	0x000001C0	/* Int Select mask */
	#define	ENI_ISEL_SHIFT	6		/* Bits to shift ISEL value */

#define	MIDWAY_STAT		0x05		/* Statistics register */

#define	MIDWAY_SVCWR		0x06		/* Svc List write pointer */
	#define	SVC_SIZE_MASK	0x3FF		/* Valid bits in svc pointer */

#define	MIDWAY_DMAADDR		0x07		/* Current virtual DMA addr */

#define	MIDWAY_RX_WR		0x08		/* Write ptr to RX DMA queue */

#define	MIDWAY_RX_RD		0x09		/* Read ptr to RX DMA queue */

#define	MIDWAY_TX_WR		0x0A		/* Write ptr to TX DMA queue */

#define	MIDWAY_TX_RD		0x0B		/* Read ptr to TX DMA queue */

/*
 * Registers 0x0C - 0x0F are unused
 */

/*
 * MIDWAY supports 8 transmit channels. Each channel has 3 registers
 * to control operation. Each new channel starts on N * 4 set. Thus,
 * channel 0 uses register 0x10 - 0x13, channel 1 uses 0x14 - 0x17, etc.
 * Register 0x13 + N * 4 is unused.
 */

#define	MIDWAY_TXPLACE		0x10		/* Channel N TX location */
	#define	TXSIZE_SHIFT	11		/* Bits to shift size by */
	#define	TX_PLACE_MASK	0x7FF		/* Valid bits in TXPLACE */

#define	MIDWAY_RDPTR		0x11		/* Channel N Read ptr */

#define	MIDWAY_DESCR		0x12		/* Channel N Descr ptr */

/*
 * Register 0x30 on up are unused
 */

/*
 * Part of PCI configuration registers but not defined in <pci/pcireg.h>
 */
#define	PCI_CONTROL_REG		0x60
#define	ENDIAN_SWAP_DMA		0x80		/* Enable endian swaps on DMA */

/*
 * The Efficient adapter references adapter RAM through the use of
 * location and size values. Eight sizes are defined. When allocating
 * buffers, there size must be rounded up to the next size which will
 * hold the requested size. Buffers are allocated on 'SIZE' boundaries.
 * See eni_buffer.c for more info.
 */

/*
 * Buffer SIZE definitions - in words, so from 1 KB to 128 KB
 */
#define	SIZE_256	0x00
#define	SIZE_512	0x01
#define	SIZE_1K		0x02
#define	SIZE_2K		0x03
#define	SIZE_4K		0x04
#define	SIZE_8K		0x05
#define	SIZE_16K	0x06
#define	SIZE_32K	0x07

/*
 * Define values for DMA type - DMA descriptors include a type field and a
 * count field except in the special case of JK (just-kidding). With type JK,
 * the count field should be set to the address which will be loaded
 * into the pointer, ie. where the pointer should next point to, since
 * JK doesn't have a "size" associated with it. JK DMA is used to skip
 * over descriptor words, and to strip off padding of AAL5 PDUs. The 
 * DMA_nWORDM types will do a n word DMA burst, but the count field
 * does not have to equal n. Any difference results in garbage filling
 * the remaining words of the DMA. These types could be used where a
 * particular burst size yields better DMA performance.
 */
#define	DMA_WORD	0x00
#define	DMA_BYTE	0x01
#define	DMA_HWORD	0x02
#define	DMA_JK		0x03
#define	DMA_4WORD	0x04
#define	DMA_8WORD	0x05
#define	DMA_16WORD	0x06
#define	DMA_2WORD	0x07
#define	DMA_4WORDM	0x0C
#define	DMA_8WORDM	0x0D
#define	DMA_16WORDM	0x0E
#define	DMA_2WORDM	0x0F

/*
 * Define the size of the local DMA list we'll build before
 * giving up on the PDU.
 */
#define	TEMP_DMA_SIZE	120		/* Enough for 58/59 buffers */

#define	DMA_COUNT_SHIFT	16		/* Number of bits to shift count */
					/* in DMA descriptor word */
#define	DMA_VCC_SHIFT	6		/* Number of bits to shift RX VCC or */
					/* TX channel in DMA descriptor word */
#define	DMA_END_BIT	0x20		/* Signal end of DMA list */

/*
 * Defines for VCI table
 *
 * The VCI table is a 1K by 4 word table allowing up to 1024 (0-1023)
 * VCIs. Entries into the table use the VCI number as the index.
 */
struct vci_table {
	u_long	vci_control;		/* Control word */
	u_long	vci_descr;		/* Descr/ReadPtr */
	u_long	vci_write;		/* WritePtr/State/Cell count */
	u_long	vci_crc;		/* ongoing CRC calculation */
};
typedef volatile struct vci_table VCI_Table;

#define	VCI_MODE_SHIFT	30		/* Shift to get MODE field */
#define	VCI_MODE_MASK	0x3FFFFFFF	/* Bits to strip MODE off */
#define	VCI_PTI_SHIFT	29		/* Shift to get PTI mode field */
#define	VCI_LOC_SHIFT	18		/* Shift to get location field */
#define	VCI_LOC_MASK	0x7FF		/* Valid bits in location field */
#define	VCI_SIZE_SHIFT	15		/* Shift to get size field */
#define	VCI_SIZE_MASK	7		/* Valid bits in size field */
#define	VCI_IN_SERVICE	1		/* Mask for IN_SERVICE field */

/*
 * Defines for VC mode
 */
#define	VCI_MODE_TRASH	0x00		/* Trash all cells for this VC */
#define	VCI_MODE_AAL0	0x01		/* Reassemble as AAL_0 PDU */
#define	VCI_MODE_AAL5	0x02		/* Reassemble as AAL_5 PDU */
/*
 * Defines for handling cells with PTI(2) set to 1.
 */
#define	PTI_MODE_TRASH	0x00		/* Trash cell */
#define	PTI_MODE_PRESV	0x01		/* Send cell to OAM channel */
/*
 * Current state of VC
 */
#define	VCI_STATE_IDLE	0x00		/* VC is idle */
#define	VCI_STATE_REASM	0x01		/* VC is reassembling PDU */
#define	VCI_STATE_TRASH	0x03		/* VC is trashing cells */

/*
 * RX Descriptor word values
 */
#define	DESCR_TRASH_BIT		0x1000	/* VCI was trashing cells */
#define	DESCR_CRC_ERR		0x0800	/* PDU has CRC error */
#define	DESCR_CELL_COUNT	0x07FF	/* Mask to get cell count */
/*
 * TX Descriptor word values
 */
#define	TX_IDEN_SHIFT	28		/* Unique identifier location */
#define	TX_MODE_SHIFT	27		/* AAL5 or AAL0 */
#define	TX_VCI_SHIFT	4		/* Bits to shift VCI value */

/*
 * When setting up descriptor words (at head of segmentation queues), there
 * is a unique identifier used to help detect sync problems.
 */
#define	MIDWAY_UNQ_ID	0x0B

/*
 * Defines for cell sizes
 */
#define	BYTES_PER_CELL	48		/* Number of data bytes per cell */
#define	WORDS_PER_CELL	12		/* Number of data words per cell */

/*
 * Access to Serial EEPROM [as opposed to expansion (E)PROM].
 *
 * This is an ATMEL AT24C01 serial EEPROM part.
 * See http://www.atmel.com/atmel/products/prod162.htm for timimg diagrams
 * for START/STOP/ACK/READ cycles.
 */
#define	SEEPROM		PCI_CONTROL_REG	/* Serial EEPROM is accessed thru */
					/* PCI control register 	  */
#define	SEPROM_DATA	0x02		/* SEEPROM DATA line */
#define	SEPROM_CLK	0x01		/* SEEPROM CLK line */
#define	SEPROM_SIZE	128		/* Size of Serial EEPROM */
#define	SEPROM_MAC_OFF	64		/* Offset to MAC address */
#define	SEPROM_SN_OFF	112		/* Offset to serial number */
#define	SEPROM_DELAY	10		/* Delay when strobing CLK/DATA lines */

/*
 * Host protocol control blocks
 *
 */

/*
 * Device VCC Entry
 *
 * Contains the common and ENI-specific information for each VCC
 * which is opened through an ENI device.
 */
struct eni_vcc {
	struct cmn_vcc	ev_cmn;		/* Common VCC stuff */
	caddr_t		ev_rxbuf;	/* Receive buffer */
	u_long		ev_rxpos;	/* Adapter buffer read pointer */
};
typedef	struct eni_vcc Eni_vcc;

#define	ev_next		ev_cmn.cv_next
#define	ev_toku		ev_cmn.cv_toku
#define	ev_upper	ev_cmn.cv_upper
#define	ev_connvc	ev_cmn.cv_connvc
#define	ev_state	ev_cmn.cv_state

typedef	volatile unsigned long *	Eni_mem;

/*
 * Define the ID's we'll look for in the PCI config
 * register when deciding if we'll support this device.
 * The DEV_ID will need to be turned into an array of
 * ID's in order to support multiple adapters with
 * the same driver.
 */
#define	EFF_VENDOR_ID	0x111A
#define	EFF_DEV_ID	0x0002

/*
 * Memory allocator defines and buffer descriptors
 */
#define	MEM_FREE	0
#define	MEM_INUSE	1

typedef struct mbd Mbd;
struct mbd {
	Mbd	*prev;
	Mbd	*next;
	caddr_t	base;			/* Adapter base address */
	int	size;			/* Size of buffer */
	int	state;			/* INUSE or FREE */
};

/*
 * We use a hack to allocate a smaller RX buffer for signalling
 * channels as they tend to have small MTU lengths.
 */
#define	UNI_SIG_VCI	5

/*
 * Device Unit Structure
 *
 * Contains all the information for a single device (adapter).
 */
struct eni_unit {
	Cmn_unit	eu_cmn;		/* Common unit stuff */
	void *		eu_pcitag;	/* PCI tag */
	Eni_mem 	eu_base;	/* Adapter memory base */
	Eni_mem 	eu_ram;		/* Adapter RAM */
	u_long		eu_ramsize;

	Eni_mem		eu_suni;	/* SUNI registers */

	Eni_mem		eu_midway;	/* MIDWAY registers */

	VCI_Table	*eu_vcitbl;	/* VCI Table */
	Eni_mem		eu_rxdma;	/* Receive DMA queue */
	Eni_mem		eu_txdma;	/* Transmit DMA queue */
	Eni_mem		eu_svclist;	/* Service list */
	u_long		eu_servread;	/* Read pointer into Service list */

	caddr_t		eu_txbuf;	/* One large TX buff for everything */
	u_long		eu_txsize;	/* Size of TX buffer */
	u_long		eu_txpos;	/* Current word being stored in RAM */
	u_long		eu_txfirst;	/* First word of unack'ed data */

	u_long		eu_trash;
	u_long		eu_ovfl;

	struct ifqueue	eu_txqueue;
	u_long		eu_txdmawr;
	struct ifqueue	eu_rxqueue;
	u_long		eu_rxdmawr;	/* DMA list write pointer */

	u_char		eu_seeprom[SEPROM_SIZE]; /* Serial EEPROM contents */
	u_int		eu_sevar;	/* Unique (per unit) seeprom var. */

	Mbd		*eu_memmap;	/* Adapter RAM memory allocator map */
	int		eu_memclicks[ENI_BUF_NBIT];/* Count of INUSE buffers */

	Eni_stats	eu_stats;	/* Statistics */

	int		eu_type;
#define	TYPE_UNKNOWN	0
#define	TYPE_ENI	1
#define	TYPE_ADP	2
};
typedef	struct eni_unit		Eni_unit;

#define	eu_pif		eu_cmn.cu_pif
#define	eu_unit		eu_cmn.cu_unit
#define	eu_flags	eu_cmn.cu_flags
#define	eu_mtu		eu_cmn.cu_mtu
#define	eu_open_vcc	eu_cmn.cu_open_vcc
#define	eu_vcc		eu_cmn.cu_vcc
#define	eu_vcc_zone	eu_cmn.cu_vcc_zone
#define	eu_nif_zone	eu_cmn.cu_nif_zone
#define	eu_ioctl	eu_cmn.cu_ioctl
#define	eu_instvcc	eu_cmn.cu_instvcc
#define	eu_openvcc	eu_cmn.cu_openvcc
#define	eu_closevcc	eu_cmn.cu_closevcc
#define	eu_output	eu_cmn.cu_output
#define	eu_config	eu_cmn.cu_config
#define	eu_softc	eu_cmn.cu_softc

#endif	/* _ENI_ENI_H */
