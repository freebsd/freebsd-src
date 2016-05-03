/*-
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * $FreeBSD$
 *
 * Fore PCA200E hardware definitions.
 */

/*
 * Fore implements some additional PCI registers. One of them is the
 * master control register. One of the bits allow to automatically byte
 * swap accesses to the on-board RAM.
 */
#define	FATM_PCIR_MCTL	0x41
#define	FATM_PCIM_SWAB	0x100

/*
 * Operations codes for commands.
 */
enum {
	FATM_OP_INITIALIZE	= 0x01,	/* Initialize the card */
	FATM_OP_ACTIVATE_VCIN	= 0x02,	/* Start reassembly on a channel */
	FATM_OP_ACTIVATE_VCOUT	= 0x03,	/* (not used) */
	FATM_OP_DEACTIVATE_VCIN	= 0x04,	/* Stop reassembly on a channel */
	FATM_OP_DEACTIVATE_VCOUT= 0x05,	/* (not used) */
	FATM_OP_REQUEST_STATS	= 0x06,	/* Get statistics */
	FATM_OP_OC3_SET_REG	= 0x07,	/* Set OC3 chip register */
	FATM_OP_OC3_GET_REG	= 0x08,	/* Get OC3 chip registers */
	FATM_OP_ZERO_STATS	= 0x09,	/* Zero out statistics */
	FATM_OP_GET_PROM_DATA	= 0x0a,	/* Return expansion ROM data */
	FATM_OP_SETVPI_BITS	= 0x0b,	/* (not used, not implemented) */

	FATM_OP_INTERRUPT_SEL	= 0x80,	/* Request interrupt on completion */
};

/*
 * Status word definitions. Before initiating an operation the host sets the
 * status word to PENDING. The card sets it to COMPLETE upon completion of
 * the transmit/receive or command. An unused queue entry contains FREE.
 * The ERROR can be ored into the COMPLETE. Note, that there are circumstances
 * when ERROR is set without COMPLETE being set (when you try to activate
 * a bad VCI like, for example, VCI 0).
 */
enum {
	FATM_STAT_PENDING	= 0x01,
	FATM_STAT_COMPLETE	= 0x02,
	FATM_STAT_FREE		= 0x04,
	FATM_STAT_ERROR		= 0x08,
};

/*
 * On board queue offsets. There are two fundamentally different queue types:
 * the command queue and all other queues. The command queue has 32 byte
 * entries on the card which contain the operation code, parameters and the
 * DMA pointer to the status word. All other queues have 8 byte entries, which
 * contain a DMA pointer to the i/o block, that contains the parameters, and
 * a DMA pointer to the status word.
 */
#define	FATMOC_OP		0	/* cmd queue: offset to op code */
#define	FATMOC_PARAM		4	/* cmd queue: offset to parameters */
#define	FATMOC_STATP		16	/* cmd queue: offset to status ptr */
#define	FATMOC_END		32	/* cmd queue: element size */

#define	FATMOC_ACTIN_VPVC	(FATMOC_PARAM + 0)
#define	FATMOC_ACTIN_MTU	(FATMOC_PARAM + 4)
#define	FATMOC_DEACTIN_VPVC	(FATMOC_PARAM + 0)
#define	FATMOC_GETOC3_BUF	(FATMOC_PARAM + 0)
#define	FATMOC_GSTAT_BUF	(FATMOC_PARAM + 0)
#define	FATMOC_GPROM_BUF	(FATMOC_PARAM + 0)

#define	FATMOS_IOBLK		0	/* other queues: offset to ioblk ptr */
#define	FATMOS_STATP		4	/* other queues: offset to status ptr */

#define	FATM_MAKE_SETOC3(REG,VAL,MASK)					\
    (FATM_OP_OC3_SET_REG | (((REG) & 0xff) << 8) | 			\
     (((VAL) & 0xff) << 16) | (((MASK) & 0xff) << 24))
#define	FATM_NREGS	128


/*
 * On board memory layout.
 *
 * The card contains up to 2MByte memory that is mapped at virtual offset 0.
 * It is followed by three registers. The memory contains two areas at
 * fixed addresses: the mon960 area that is used for communication with
 * the card's operating system and the common block that is used by the
 * firmware to communicate with the driver.
 */
#define	FATM_RAM_SIZE		(256 * 1024)	/* normal RAM size */

#define	FATMO_RAM		(0x0)		/* virtual RAM start */
#define	FATMO_MON960		(0x400)		/* mon960 communication area */
#define	FATMO_COMMON_ORIGIN	(0x4d40)	/* firmware comm. area */

#define	FATMO_HCR		(0x100000)	/* host control registers */
#define	FATMO_HIMR		(0x100004)	/* host interrupt mask */
#define	FATMO_PSR		(0x100008)	/* PCI control register */

#define	FATMO_END		(0x200000)	/* end of mapped area */

/*
 * The mon960 area contains two cells that are used as a virtual serial
 * interface, a status word, the base for loading the application (i.e.
 * firmware) and a version number.
 */
#define	FATMO_UART_TO_960	(FATMO_MON960 + 0)
#define	FATMO_UART_TO_HOST	(FATMO_MON960 + 4)
#define	FATMO_BOOT_STATUS	(FATMO_MON960 + 8)
#define	FATMO_APP_BASE		(FATMO_MON960 + 12)
#define	FATMO_VERSION		(FATMO_MON960 + 16)


/*
 * The host control register allows to hold the i960 or send it interrupts.
 * The bits have different meaning on read and write.
 */
#define	FATM_HCR_RESET		0x01	/* (W) reset the card */
#define	FATM_HCR_LOCK_HOLD	0x02	/* (W) hold the i960 */
#define	FATM_HCR_I960FAIL	0x04	/* (R) internal self-test failed */
#define	FATM_HCR_INTR2		0x04	/* (W) assert i960 interrupt 2 */
#define	FATM_HCR_HOLDA		0x08	/* (R) hold ack from i960 */
#define	FATM_HCR_INTR1		0x08	/* (W) assert i960 interrupt 1 */
#define	FATM_HCR_OFIFO		0x10	/* (R) DMA request FIFO full */
#define	FATM_HCR_CLRIRQ		0x10	/* (W) clear interrupt request */
#define	FATM_HCR_ESP_HOLD	0x20	/* (R) SAR chip holds i960 */
#define	FATM_HCR_IFIFO		0x40	/* (R) input FIFO full */
#define	FATM_HCR_TESTMODE	0x80	/* (R) board is in test mode */

/*
 * The mon960 area contains a virtual UART and a status word.
 * The UART uses a simple protocol: a zero means, that there is no
 * character available from the i960 or that one can write the next
 * character to the i960. This character has to be ored with 0x1000000
 * to signal to the i960 that there is a new character.
 * The cold_start values must be written to the status word, the others
 * denote certain stages of initializing.
 */
#define	XMIT_READY	0
#define	CHAR_AVAIL	0x1000000

#define	COLD_START	0xc01dc01d
#define	SELF_TEST_OK	0x02201958
#define	SELF_TEST_FAIL	0xadbadbad
#define	CP_RUNNING	0xce11feed
#define	MON906_TOO_BIG	0x10aded00

/*
 * The firmware communication area contains a big structure most of which
 * is used only during initialisation.
 */
/*
 * These are the offsets to the onboard queues that are valid after the
 * initialisation command has completed.
 */
#define	FATMO_COMMAND_QUEUE	(FATMO_COMMON_ORIGIN + 0)
#define	FATMO_TRANSMIT_QUEUE	(FATMO_COMMON_ORIGIN + 4)
#define	FATMO_RECEIVE_QUEUE	(FATMO_COMMON_ORIGIN + 8)
#define	FATMO_SMALL_B1_QUEUE	(FATMO_COMMON_ORIGIN + 12)
#define	FATMO_LARGE_B1_QUEUE	(FATMO_COMMON_ORIGIN + 16)
#define	FATMO_SMALL_B2_QUEUE	(FATMO_COMMON_ORIGIN + 20)
#define	FATMO_LARGE_B2_QUEUE	(FATMO_COMMON_ORIGIN + 24)

/*
 * If the interrupt mask is set to 1, interrupts to the host are queued, but
 * inhbited. The istat variable is set, when this card has posted an interrupt.
 */
#define	FATMO_IMASK		(FATMO_COMMON_ORIGIN + 28)
#define	FATMO_ISTAT		(FATMO_COMMON_ORIGIN + 32)

/*
 * This is the offset and the size of the queue area. Could be used to
 * dynamically compute queue sizes.
 */
#define	FATMO_HEAP_BASE		(FATMO_COMMON_ORIGIN + 36)
#define	FATMO_HEAP_SIZE		(FATMO_COMMON_ORIGIN + 40)

#define	FATMO_HLOGGER		(FATMO_COMMON_ORIGIN + 44)

/*
 * The heartbeat variable is incremented in each loop of the normal processing.
 * If it is stuck this means, that the card had a fatal error. In this case
 * it may set the word to a number of values of the form 0xdeadXXXX where
 * XXXX is an error code.
 */
#define	FATMO_HEARTBEAT		(FATMO_COMMON_ORIGIN + 48)

#define	FATMO_FIRMWARE_RELEASE	(FATMO_COMMON_ORIGIN + 52)
#define	FATMO_MON960_RELEASE	(FATMO_COMMON_ORIGIN + 56)
#define	FATMO_TQ_PLEN		(FATMO_COMMON_ORIGIN + 60)

/*
 * At this offset the init command block is located. The init command cannot
 * use the normal queue mechanism because it is used to initialize the
 * queues. For this reason it is located at this fixed offset.
 */
#define	FATMO_INIT		(FATMO_COMMON_ORIGIN + 64)

/*
 * physical media type
 */
#define	FATMO_MEDIA_TYPE	(FATMO_COMMON_ORIGIN + 176)
#define	FATMO_OC3_REVISION	(FATMO_COMMON_ORIGIN + 180)

/*
 * End of the common block
 */
#define	FATMO_COMMON_END	(FATMO_COMMON_ORIGIN + 184)

/*
 * The INITIALIZE command block. This is embedded into the above common
 * block. The offsets are from the beginning of the command block.
 */
#define	FATMOI_OP		0	/* operation code */
#define	FATMOI_STATUS		4	/* status word */
#define	FATMOI_RECEIVE_TRESHOLD	8	/* when to start interrupting */
#define	FATMOI_NUM_CONNECT	12	/* max number of VCIs */
#define	FATMOI_CQUEUE_LEN	16	/* length of command queue */
#define	FATMOI_TQUEUE_LEN	20	/* length of transmit queue */
#define	FATMOI_RQUEUE_LEN	24	/* length of receive queue */
#define	FATMOI_RPD_EXTENSION	28	/* additional 32 byte blocks */
#define	FATMOI_TPD_EXTENSION	32	/* additional 32 byte blocks */
#define	FATMOI_CONLESS_VPVC	36	/* (not used) */
#define	FATMOI_SMALL_B1		48	/* small buffer 1 pool */
#define	FATMOI_LARGE_B1		64	/* small buffer 2 pool */
#define	FATMOI_SMALL_B2		80	/* large buffer 1 pool */
#define	FATMOI_LARGE_B2		96	/* large buffer 2 pool */
#define	FATMOI_END		112	/* size of init block */

/*
 * Each of the four buffer schemes is initialized with a block that
 * contains four words:
 */
#define	FATMOB_QUEUE_LENGTH	0	/* supply queue length */
#define	FATMOB_BUFFER_SIZE	4	/* size of each buffer */
#define	FATMOB_POOL_SIZE	8	/* size of on-board pool */
#define	FATMOB_SUPPLY_BLKSIZE	12	/* number of buffers/supply */

/*
 * The fore firmware is a binary file, that starts with a header. The
 * header contains the offset to where the file must be loaded and the
 * entry for execution. The header must also be loaded onto the card!
 */
struct firmware {
	uint32_t	id;		/* "FORE" */
	uint32_t	version;	/* firmware version */
	uint32_t	offset;		/* load offset */
	uint32_t	entry;		/* entry point */
};
#define	FATM_FWID	0x65726f66	/* "FORE" */
#define	FATM_FWVERSION	0x100		/* supported version */

/*
 * PDUs to be transmitted are described by Transmit PDU Descriptors.
 * These descriptors are held in host memory, but referenced from the ioblk
 * member of the queue structure on the card. The card DMAs the descriptor
 * and than gather-DMAs the PDU transmitting it on-the-fly. Tpds are variable
 * length in blocks of 32 byte (8 words). The minimum length is one block,
 * maximum 15. The number of blocks beyond 1 is configured during the
 * initialisation command (tpd_extension).
 * Each gather-DMA segment is described by a segment descriptor. The buffer
 * address and the length must be a multiple of four.
 * Tpd must also be 4 byte aligned.
 * Because of the minimum length of 32 byte, the first blocks contains already
 * 2 segement descriptors. Each extension block holds four descriptors.
 */
#define	TXD_FIXED	2
#define	SEGS_PER_BLOCK	4	/* segment descriptors per extension block */
struct txseg {
	uint32_t	buffer;		/* DMA buffer address */
	uint32_t	length;		/* and length */
};
struct tpd {
	uint32_t	atm_header;	/* header for the transmitted cells */
	uint32_t	spec;		/* PDU description */
	uint32_t	stream;		/* traffic shaping word */
	uint32_t	pad[1];
	struct txseg	segment[TXD_FIXED];
};

#define	TDX_MKSPEC(INTR,AAL,NSEG,LEN) \
	(((INTR) << 28) | ((AAL) << 24) | ((NSEG) << 16) | (LEN))
#define	TDX_MKSTR(DATA,IDLE) \
	(((DATA) << 16) | (IDLE))
#define	TDX_MKHDR(VPI,VCI,PT,CLP) \
	(((VPI) << 20) | ((VCI) << 4) | ((PT) << 1) | (CLP))
#define	TDX_SEGS2BLKS(SEGS) \
	(1 + ((SEGS)-TXD_FIXED+SEGS_PER_BLOCK-1)/SEGS_PER_BLOCK)

/*
 * We want probably support scatter transmission, so we use the maximum
 * transmit descriptor extension that is possible. Because the size of the
 * Tpd is encoded in 32-byte blocks in a 4-bit field, the maximum extension
 * is 14 such blocks. The value for the init command is the number of 
 * additional descriptor entries NOT the number of 32 byte blocks.
 */
#define	TPD_EXTENSION_BLOCKS	14
#define	TPD_EXTENSIONS		(TPD_EXTENSION_BLOCKS * 4)
#define	TPD_SIZE		((size_t)((TPD_EXTENSION_BLOCKS+1) * 32))

/*
 * Received PDUs are handed from the card to the host by means of Receive
 * PDU descriptors. Each segment describes on part of the PDU. The buffer
 * handle is a 32 bit value that is supplied by the host and passed
 * transparently back to the host by the card. It is used to locate the buffer.
 * The length field is the number of actual bytes in that buffer.
 */
#define	RXD_FIXED	3
struct rxseg {
	uint32_t	handle;		/* buffer handle */
	uint32_t	length;		/* number of bytes */
};
struct rpd {
	uint32_t	atm_header;
	uint32_t	nseg;
	struct rxseg	segment[RXD_FIXED];
};

/*
 * PDUs received are stored in buffers supplied to the card. We use only
 * buffer scheme 1: small buffers are normal mbuf's which can hold three
 * cells in their default size (256 byte) and mbuf clusters which can
 * hold 42 cells (2 kbyte).
 * The number of receive segments can be computed from these sizes:
 */
#define	FATM_MAXPDU		65535
#define	MAXPDU_CELLS		((FATM_MAXPDU+47)/48)

#define	SMALL_BUFFER_CELLS	(MHLEN/48)
#define	LARGE_BUFFER_CELLS	(MCLBYTES/48)

#define	SMALL_BUFFER_LEN	(SMALL_BUFFER_CELLS * 48)
#define	LARGE_BUFFER_LEN	(LARGE_BUFFER_CELLS * 48)

/*
 * The card first alloctes a small buffer and the switches to large
 * buffers. So the number of large buffers needed to store the maximum
 * PDU is:
 */
#define	MAX_LARGE_BUFFERS	((MAXPDU_CELLS - SMALL_BUFFER_CELLS	\
				  + LARGE_BUFFER_CELLS - 1)		\
				 / LARGE_BUFFER_CELLS)			\

/*
 * From this we get the number of extension blocks for the Rpds as:
 */
#define	RPD_EXTENSION_BLOCKS	((MAX_LARGE_BUFFERS + 1 - RXD_FIXED	\
				  + SEGS_PER_BLOCK - 1)			\
				 / SEGS_PER_BLOCK)
#define	RPD_EXTENSIONS		(RPD_EXTENSION_BLOCKS * 4)
#define	RPD_SIZE		((size_t)((RPD_EXTENSION_BLOCKS+1) * 32))

/*
 * Buffers are supplied to the card prior receiving by the supply queues.
 * We use two queues: scheme 1 small buffers and scheme 1 large buffers.
 * The queues and on-card pools are initialized by the initialize command.
 * Buffers are supplied in chunks. Each chunk can contain from 4 to 124
 * buffers in multiples of four. The chunk sizes are configured by the
 * initialize command. Each buffer in a chunk is described by a Receive
 * Buffer Descriptor that is held in host memory and given as the ioblk
 * to the card.
 */
#define	BSUP_BLK2SIZE(CHUNK)	(8 * (CHUNK))

struct rbd {
	uint32_t	handle;
	uint32_t	buffer;		/* DMA address for card */
};

/*
 * The PCA200E has an expansion ROM that contains version information and
 * the FORE-assigned MAC address. It can be read via the get_prom_data
 * operation.
 */
struct prom {
	uint32_t	version;
	uint32_t	serial;
	uint8_t		mac[8];
};

/*
 * The media type member of the firmware communication block contains a 
 * code that describes the physical medium and physical protocol.
 */
#define	FORE_MT_TAXI_100	0x04
#define	FORE_MT_TAXI_140	0x05
#define	FORE_MT_UTP_SONET	0x06
#define	FORE_MT_MM_OC3_ST	0x16
#define	FORE_MT_MM_OC3_SC	0x26
#define	FORE_MT_SM_OC3_ST	0x36
#define	FORE_MT_SM_OC3_SC	0x46

/*
 * Assorted constants
 */
#define	FORE_MAX_VCC	1024	/* max. number of VCIs supported */
#define	FORE_VCIBITS	10

#define	FATM_STATE_TIMEOUT	500	/* msec */

/*
 * Statistics as delivered by the FORE cards
 */
struct fatm_stats {
	struct {
		uint32_t	crc_header_errors;
		uint32_t	framing_errors;
		uint32_t	pad[2];
	}			phy_4b5b;

	struct {
		uint32_t	section_bip8_errors;
		uint32_t	path_bip8_errors;
		uint32_t	line_bip24_errors;
		uint32_t	line_febe_errors;
		uint32_t	path_febe_errors;
		uint32_t	corr_hcs_errors;
		uint32_t	ucorr_hcs_errors;
		uint32_t	pad[1];
	}			phy_oc3;

	struct {
		uint32_t	cells_transmitted;
		uint32_t	cells_received;
		uint32_t	vpi_bad_range;
		uint32_t	vpi_no_conn;
		uint32_t	vci_bad_range;
		uint32_t	vci_no_conn;
		uint32_t	pad[2];
	}			atm;

	struct {
		uint32_t	cells_transmitted;
		uint32_t	cells_received;
		uint32_t	cells_dropped;
		uint32_t	pad[1];
	}			aal0;

	struct {
		uint32_t	cells_transmitted;
		uint32_t	cells_received;
		uint32_t	cells_crc_errors;
		uint32_t	cels_protocol_errors;
		uint32_t	cells_dropped;
		uint32_t	cspdus_transmitted;
		uint32_t	cspdus_received;
		uint32_t	cspdus_protocol_errors;
		uint32_t	cspdus_dropped;
		uint32_t	pad[3];
	}			aal4;

	struct {
		uint32_t	cells_transmitted;
		uint32_t	cells_received;
		uint32_t	congestion_experienced;
		uint32_t	cells_dropped;
		uint32_t	cspdus_transmitted;
		uint32_t	cspdus_received;
		uint32_t	cspdus_crc_errors;
		uint32_t	cspdus_protocol_errors;
		uint32_t	cspdus_dropped;
		uint32_t	pad[3];
	}			aal5;

	struct {
		uint32_t	small_b1_failed;
		uint32_t	large_b1_failed;
		uint32_t	small_b2_failed;
		uint32_t	large_b2_failed;
		uint32_t	rpd_alloc_failed;
		uint32_t	receive_carrier;
		uint32_t	pad[2];
	}			aux;
};
#define	FATM_NSTATS	42
