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
 *	@(#) $FreeBSD: src/sys/dev/hfa/fore_aali.h,v 1.5 2005/01/06 01:42:43 imp Exp $
 *
 */

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * ATM Adaptation Layer Interface (AALI) definitions
 *
 */

#ifndef _FORE_AALI_H
#define _FORE_AALI_H

/*
 * This file contains the definitions required by the FORE ATM Adaptation
 * Layer Interface (AALI) specification.
 */


/*
 * Addressing/Pointer definitions
 *
 * The CP memory only supports 32-bit word accesses (read and write) - thus,
 * all memory must be defined and accessed as 32-bit words.  Also, since the
 * data transfers are word-sized, we must take care of byte-swapping issues
 * from/to little-endian hosts (the CP is an i960 processor, ie big-endian).
 *
 * All pointers to CP memory areas are actually offsets from the start of 
 * the adapter RAM address space.
 *
 * All CP-resident data structures are declared volatile.
 */
typedef void *		H_addr;		/* Host-resident address */
typedef unsigned long	H_dma;		/* Host-resident DMA address */
typedef unsigned long	CP_word;	/* CP-resident word */
typedef unsigned long	CP_addr;	/* CP-resident CP memory offset */
typedef unsigned long	CP_dma;		/* CP-resident DMA address */
 

/*
 * Structure defining the CP's shared memory interface to the mon960 program
 */
struct mon960 {
	CP_word		mon_xmitmon;	/* Uart - host to mon960 (see below) */
	CP_word		mon_xmithost;	/* Uart - mon960 to host (see below) */
	CP_word		mon_bstat;	/* Boot status word (see below) */
	CP_addr		mon_appl;	/* Pointer to application memory area */
	CP_word		mon_ver;	/* Mon960 firmware version */
};
typedef volatile struct mon960	Mon960;

/*
 * Pseudo-UART usage
 */
#define	UART_READY	0x00000000	/* UART is ready for more data */
#define	UART_VALID	0x01000000	/* UART character is valid */
#define	UART_DATAMASK	0x000000ff	/* UART character data mask */

/*
 * Boot Status Word
 */
#define	BOOT_COLDSTART	0xc01dc01d	/* CP is performing cold start */
#define	BOOT_MONREADY	0x02201958	/* Monitor is waiting for commands */
#define	BOOT_FAILTEST	0xadbadbad	/* Monitor failed self-test */
#define	BOOT_RUNNING	0xce11feed	/* Microcode downloaded and running */

#define	BOOT_LOOPS	20		/* Loops to wait for CP to boot */
#define	BOOT_DELAY	100000		/* Delay (us) for each boot loop */


/*
 * Supported AALs
 */
enum fore_aal {
	FORE_AAL_0 = 0,			/* Cell Service */
	FORE_AAL_4 = 4,			/* AAL 3/4 */
	FORE_AAL_5 = 5			/* AAL 5 */
};
typedef enum fore_aal Fore_aal;


/*
 * Buffer strategy definition
 */
struct buf_strategy {
	CP_word		bfs_quelen;	/* Buffer supply queue entries */
	CP_word		bfs_bufsize;	/* Buffer size */
	CP_word		bfs_cppool;	/* Buffers in CP-resident pool */
	CP_word		bfs_entsize;	/* Buffers in each supply queue entry */
};
typedef volatile struct buf_strategy	Buf_strategy;

/*
 * Buffer strategy id
 */
#define	BUF_STRAT_1	0		/* Buffer strategy one */
#define	BUF_STRAT_2	1		/* Buffer strategy two */



#ifdef _KERNEL
/*
 * Common Queue Element
 *
 * Used for Transmit, Receive and Buffer Supply Queues
 */
struct com_queue {
	CP_dma		cq_descr;	/* Pointer to element descriptor */
	CP_dma		cq_status;	/* Pointer to element status word */
};
typedef volatile struct com_queue	Com_queue;


/*
 * Queue element status word
 */
typedef volatile unsigned long	Q_status;

#define	QSTAT_PENDING	0x01		/* Operation is pending */
#define	QSTAT_COMPLETED	0x02		/* Operation successfully completed */
#define	QSTAT_FREE	0x04		/* Queue element is free/unused */
#define	QSTAT_ERROR	0x08		/* Operation encountered an error */

#define	QSTAT_ALIGN	4


/*
 * PDU Transmit Queue
 */

/*
 * PDU Transmit Queue Element
 */
typedef volatile struct com_queue	Xmit_queue;


/*
 * PDU Transmit buffer segment descriptor
 */
struct xmit_seg_descr {
	H_dma		xsd_buffer;	/* Buffer's DMA address */
	u_int		xsd_len;	/* Data length in buffer */
};
typedef struct xmit_seg_descr	Xmit_seg_descr;

#define	XMIT_SEG_ALIGN	4


/*
 * PDU Transmit descriptor header
 */
struct xmit_descr_hdr {
	u_long		xdh_cell_hdr;	/* Cell header (minus HEC) */
	u_long		xdh_spec;	/* Transmit specification (see below) */
	u_long		xdh_rate;	/* Rate control (data/idle cell ratio)*/
	u_long		xdh_pad;	/* Pad to quad-word boundary */
};
typedef struct xmit_descr_hdr	Xmit_descr_hdr;


#define	XMIT_BLK_BITS		5		/* Bits to encode block size */
#define	XMIT_MAX_BLK_BITS	4		/* Max bits we can use */
#define	XMIT_BLK_SIZE		(1 << XMIT_BLK_BITS)
#define	XMIT_SEGS_TO_BLKS(nseg) \
		((((nseg) * sizeof(Xmit_seg_descr)) \
		+ sizeof(Xmit_descr_hdr) + (XMIT_BLK_SIZE - 1)) \
		>> XMIT_BLK_BITS)
#define	XMIT_MAX_BLKS		((1 << XMIT_MAX_BLK_BITS) - 1)
#define	XMIT_HDR_SEGS 		((XMIT_BLK_SIZE - sizeof(Xmit_descr_hdr)) \
					/ sizeof(Xmit_seg_descr))
#define	XMIT_BLK_SEGS		(XMIT_BLK_SIZE / sizeof(Xmit_seg_descr))
#define	XMIT_EXTRA_SEGS		((XMIT_MAX_BLKS - 1) * XMIT_BLK_SEGS)
#define	XMIT_MAX_SEGS		(XMIT_EXTRA_SEGS + XMIT_HDR_SEGS)


/*
 * PDU Transmit descriptor
 */
struct xmit_descr {
	Xmit_descr_hdr	xd_hdr;		/* Descriptor header */
	Xmit_seg_descr	xd_seg[XMIT_MAX_SEGS];	/* PDU segments */
};
typedef struct xmit_descr	Xmit_descr;

#define	xd_cell_hdr	xd_hdr.xdh_cell_hdr
#define	xd_spec		xd_hdr.xdh_spec
#define	xd_rate		xd_hdr.xdh_rate

/*
 * Transmit specification
 *
 *	Bits  0-15 - Total PDU length
 *	Bits 16-23 - Number of transmit segments
 *	Bits 24-27 - AAL type
 *	Bits 28-31 - Interrupt flag
 */
#define	XDS_SET_SPEC(i,a,n,l)	(((i) << 28) | ((a) << 24) | ((n) << 16) | (l))
#define	XDS_GET_LEN(s)		((s) & 0xffff)
#define	XDS_GET_SEGS(s)		(((s) >> 16) & 0xff)
#define	XDS_GET_AAL(s)		(((s) >> 24) & 0xf)
#define	XDS_GET_INTR(s)		(((s) >> 28) & 0xf)

#define	XMIT_MAX_PDULEN		65535
#define	XMIT_DESCR_ALIGN	32



/*
 * PDU Receive Queue
 */

/*
 * PDU Receive Queue Element
 */
typedef volatile struct com_queue	Recv_queue;


/*
 * Receive PDU buffer segment description
 */
struct recv_seg_descr {
	H_addr		rsd_handle;	/* Buffer handle (from supply) */
	u_int		rsd_len;	/* Data length in buffer */
};
typedef struct recv_seg_descr	Recv_seg_descr;


/*
 * PDU Receive descriptor header
 */
struct recv_descr_hdr {
	u_long		rdh_cell_hdr;	/* Cell header (minus HEC) */
	u_long		rdh_nsegs;	/* Number of receive segments */
};
typedef struct recv_descr_hdr	Recv_descr_hdr;


#define	RECV_BLK_SIZE		32
#define	RECV_HDR_SEGS 		((RECV_BLK_SIZE - sizeof(Recv_descr_hdr)) \
					/ sizeof(Recv_seg_descr))
#define	RECV_BLK_SEGS		(RECV_BLK_SIZE / sizeof(Recv_seg_descr))
#define	RECV_MAX_LG_SEGS	((FORE_IFF_MTU - BUF1_SM_SIZE \
					+ (BUF1_LG_SIZE - 1)) / BUF1_LG_SIZE)
#define	RECV_EXTRA_BLKS		(((RECV_MAX_LG_SEGS + 1 - RECV_HDR_SEGS) \
					+ (RECV_BLK_SEGS - 1)) / RECV_BLK_SEGS)
#define RECV_EXTRA_SEGS		(RECV_EXTRA_BLKS * RECV_BLK_SEGS)
#define	RECV_MAX_SEGS		(RECV_EXTRA_SEGS + RECV_HDR_SEGS)


/*
 * PDU Receive descriptor
 */
struct recv_descr {
	Recv_descr_hdr	rd_hdr;		/* Descriptor header */
	Recv_seg_descr	rd_seg[RECV_MAX_SEGS];	/* PDU segments */
};
typedef struct recv_descr	Recv_descr;

#define	rd_cell_hdr	rd_hdr.rdh_cell_hdr
#define	rd_nsegs	rd_hdr.rdh_nsegs

#define	RECV_DESCR_ALIGN	32



/*
 * Buffer Supply Queue
 */

/*
 * Buffer Supply Queue Element
 */
typedef volatile struct com_queue	Buf_queue;


/*
 * Buffer supply descriptor for supplying receive buffers
 */
struct buf_descr {
	H_addr		bsd_handle;	/* Host-specific buffer handle */
	H_dma		bsd_buffer;	/* Buffer DMA address */
};
typedef struct buf_descr	Buf_descr;

#define	BUF_DESCR_ALIGN		32



/*
 * Command Queue
 */

/*
 * Command Codes
 */
typedef volatile unsigned long	Cmd_code;

#define	CMD_INIT	0x01		/* Initialize microcode */
#define	CMD_ACT_VCCIN	0x02		/* Activate incoming VCC */
#define	CMD_ACT_VCCOUT	0x03		/* Activate outgoing VCC */
#define	CMD_DACT_VCCIN	0x04		/* Deactivate incoming VCC */
#define	CMD_DACT_VCCOUT	0x05		/* Deactivate outgoing VCC */
#define	CMD_GET_STATS	0x06		/* Get adapter statistics */
#define	CMD_SET_OC3_REG	0x07		/* Set SUNI OC3 registers */
#define	CMD_GET_OC3_REG	0x08		/* Get SUNI OC3 registers */
#define	CMD_GET_PROM	0x09		/* Get PROM data */
#define	CMD_ZERO_STATS4	0x09		/* FT 4 Zero stats (unimpl) */
#define	CMD_GET_PROM4	0x0a		/* FT 4 Get PROM data */
#define	CMD_INTR_REQ	0x80		/* Request host interrupt */

#endif	/* _KERNEL */


/*
 * Structure defining the parameters for the Initialize command
 */
struct init_parms {
	CP_word		init_cmd;	/* Command code */
	CP_word		init_status;	/* Completion status */
	CP_word		init_indisc;	/* Not used */
	CP_word		init_numvcc;	/* Number of VCC's supported */
	CP_word		init_cmd_elem;	/* # of command queue elements */
	CP_word		init_xmit_elem;	/* # of transmit queue elements */
	CP_word		init_recv_elem;	/* # of receive queue elements */
	CP_word		init_recv_ext;	/* # of extra receive descr SEGMENTS */
	CP_word		init_xmit_ext;	/* # of extra transmit descr SEGMENTS */
	CP_word		init_cls_vcc;	/* Not used */
	CP_word		init_pad[2];	/* Pad to quad-word boundary */
	Buf_strategy	init_buf1s;	/* Buffer strategy - 1 small */
	Buf_strategy	init_buf1l;	/* Buffer strategy - 1 large */
	Buf_strategy	init_buf2s;	/* Buffer strategy - 2 small */
	Buf_strategy	init_buf2l;	/* Buffer strategy - 2 large */
};
typedef volatile struct init_parms	Init_parms;


#ifdef _KERNEL
/*
 * Structure defining the parameters for the Activate commands
 */
struct activate_parms {
	CP_word		act_spec;	/* Command specification (see below) */
	CP_word		act_vccid;	/* VCC id (VPI=0,VCI=id) */
	CP_word		act_batch;	/* # cells in batch (AAL=NULL) */
	CP_word		act_pad;	/* Pad to quad-word boundary */
};
typedef volatile struct activate_parms	Activate_parms;

/*
 * Activate command specification
 *
 *	Bits  0-7  - command code
 *	Bits  8-15 - AAL type
 *	Bits 16-23 - buffer strategy
 *	Bits 24-31 - reserved
 */
#define	ACT_SET_SPEC(b,a,c)	(((b) << 16) | ((a) << 8) | (c))
#define	ACT_GET_CMD(s)		((s) & 0xff)
#define	ACT_GET_AAL(s)		(((s) >> 8) & 0xff)
#define	ACT_GET_STRAT(s)	(((s) >> 16) & 0xff)


/*
 * Structure defining the parameters for the Deactivate commands
 */
struct dactivate_parms {
	CP_word		dact_cmd;	/* Command code */
	CP_word		dact_vccid;	/* VCC id (VPI=0,VCI=id) */
	CP_word		dact_pad[2];	/* Pad to quad-word boundary */
};
typedef volatile struct dactivate_parms	Dactivate_parms;


/*
 * Structure defining the parameters for the Get Statistics command
 */
struct stats_parms {
	CP_word		stats_cmd;	/* Command code */
	CP_dma		stats_buffer;	/* DMA address of host stats buffer */
	CP_word		stats_pad[2];	/* Pad to quad-word boundary */
};
typedef volatile struct stats_parms	Stats_parms;


/*
 * Structure defining the parameters for the SUNI OC3 commands
 */
struct suni_parms {
	CP_word		suni_spec;	/* Command specification (see below) */
	CP_dma		suni_buffer;	/* DMA address of host SUNI buffer */
	CP_word		suni_pad[2];	/* Pad to quad-word boundary */
};
typedef volatile struct suni_parms	Suni_parms;

/*
 * SUNI OC3 command specification
 *
 *	Bits  0-7  - command code
 *	Bits  8-15 - SUNI register number
 *	Bits 16-23 - Value(s) to set in register
 *	Bits 24-31 - Mask selecting value bits
 */
#define	SUNI_SET_SPEC(m,v,r,c)	(((m) << 24) | ((v) << 16) | ((r) << 8) | (c))
#define	SUNI_GET_CMD(s)		((s) & 0xff)
#define	SUNI_GET_REG(s)		(((s) >> 8) & 0xff)
#define	SUNI_GET_VALUE(s)	(((s) >> 16) & 0xff)
#define	SUNI_GET_MASK(s)	(((s) >> 24) & 0xff)


/*
 * Structure defining the parameters for the Get Prom command
 */
struct	prom_parms {
	CP_word		prom_cmd;	/* Command code */
	CP_dma		prom_buffer;	/* DMA address of host prom buffer */
	CP_word		prom_pad[2];	/* Pad to quad-word boundary */
};
typedef volatile struct prom_parms	Prom_parms;


/*
 * Command Queue Element
 */
struct cmd_queue {
	union {				/* Command-specific parameters */
		Activate_parms	cmdqu_act;
		Dactivate_parms	cmdqu_dact;
		Stats_parms	cmdqu_stats;
		Suni_parms	cmdqu_suni;
		Prom_parms	cmdqu_prom;
	} cmdq_u;
	CP_dma		cmdq_status;	/* Pointer to element status word */
	CP_word		cmdq_pad[3];	/* Pad to quad-word boundary */
};
#define	cmdq_act	cmdq_u.cmdqu_act
#define	cmdq_dact	cmdq_u.cmdqu_dact
#define	cmdq_stats	cmdq_u.cmdqu_stats
#define	cmdq_suni	cmdq_u.cmdqu_suni
#define	cmdq_prom	cmdq_u.cmdqu_prom
typedef volatile struct cmd_queue	Cmd_queue;

#endif	/* _KERNEL */



/*
 * Structure defining the CP's shared memory interface to the 
 * AALI firmware program (downloaded microcode)
 */
struct aali {
	CP_addr		aali_cmd_q;	/* Pointer to command queue */
	CP_addr		aali_xmit_q;	/* Pointer to transmit queue */
	CP_addr		aali_recv_q;	/* Pointer to receive queue */
	CP_addr		aali_buf1s_q;	/* Pointer to strategy-1 small queue */
	CP_addr		aali_buf1l_q;	/* Pointer to strategy-1 large queue */
	CP_addr		aali_buf2s_q;	/* Pointer to strategy-2 small queue */
	CP_addr		aali_buf2l_q;	/* Pointer to strategy-2 large queue */
	CP_word		aali_intr_ena;	/* Enables interrupts if non-zero */
	CP_word		aali_intr_sent;	/* Interrupt issued if non-zero */
	CP_addr		aali_heap;	/* Pointer to application heap */
	CP_word		aali_heaplen;	/* Length of application heap */
	CP_word		aali_hostlog;	/* FORE internal use */
	CP_word		aali_heartbeat;	/* Monitor microcode health */
	CP_word		aali_ucode_ver;	/* Microcode firmware version */
	CP_word		aali_mon_ver;	/* Mon960 version */
	CP_word		aali_xmit_tput;	/* FORE internal use */

	/* This must be on a quad-word boundary */
	Init_parms	aali_init;	/* Initialize command parameters */
};
typedef volatile struct aali	Aali;


/*
 * CP maintained statistics - DMA'd to host with CMD_GET_STATS command
 */
struct stats_taxi {
	u_long		taxi_bad_crc;	/* Bad header CRC errors */
	u_long		taxi_framing;	/* Framing errors */
	u_long		taxi_pad[2];	/* Pad to quad-word boundary */
};
typedef struct stats_taxi	Stats_taxi;

struct stats_oc3 {
	u_long		oc3_sect_bip8;	/* Section 8-bit intrlv parity errors */
	u_long		oc3_path_bip8;	/* Path 8-bit intrlv parity errors */
	u_long		oc3_line_bip24;	/* Line 24-bit intrlv parity errors */
	u_long		oc3_line_febe;	/* Line far-end block errors */
	u_long		oc3_path_febe;	/* Path far-end block errors */
	u_long		oc3_hec_corr;	/* Correctible HEC errors */
	u_long		oc3_hec_uncorr;	/* Uncorrectible HEC errors */
	u_long		oc3_pad;	/* Pad to quad-word boundary */
};
typedef struct stats_oc3	Stats_oc3;

struct stats_atm {
	u_long		atm_xmit;	/* Cells transmitted */
	u_long		atm_rcvd;	/* Cells received */
	u_long		atm_vpi_range;	/* Cell drops - VPI out of range */
	u_long		atm_vpi_noconn;	/* Cell drops - no connect for VPI */
	u_long		atm_vci_range;	/* Cell drops - VCI out of range */
	u_long		atm_vci_noconn;	/* Cell drops - no connect for VCI */
	u_long		atm_pad[2];	/* Pad to quad-word boundary */
};
typedef struct stats_atm	Stats_atm;

struct stats_aal0 {
	u_long		aal0_xmit;	/* Cells transmitted */
	u_long		aal0_rcvd;	/* Cells received */
	u_long		aal0_drops;	/* Cell drops */
	u_long		aal0_pad;	/* Pad to quad-word boundary */
};
typedef struct stats_aal0	Stats_aal0;

struct stats_aal4 {
	u_long		aal4_xmit;	/* Cells transmitted */
	u_long		aal4_rcvd;	/* Cells received */
	u_long		aal4_crc;	/* Cells with payload CRC errors */
	u_long		aal4_sar_cs;	/* Cells with SAR/CS errors */
	u_long		aal4_drops;	/* Cell drops */
	u_long		aal4_pdu_xmit;	/* CS PDUs transmitted */
	u_long		aal4_pdu_rcvd;	/* CS PDUs received */
	u_long		aal4_pdu_errs;	/* CS layer protocol errors */
	u_long		aal4_pdu_drops;	/* CS PDUs dropped */
	u_long		aal4_pad[3];	/* Pad to quad-word boundary */
};
typedef struct stats_aal4	Stats_aal4;

struct stats_aal5 {
	u_long		aal5_xmit;	/* Cells transmitted */
	u_long		aal5_rcvd;	/* Cells received */
	u_long		aal5_crc_len;	/* Cells with CRC/length errors */
	u_long		aal5_drops;	/* Cell drops */
	u_long		aal5_pdu_xmit;	/* CS PDUs transmitted */
	u_long		aal5_pdu_rcvd;	/* CS PDUs received */
	u_long		aal5_pdu_crc;	/* CS PDUs with CRC errors */
	u_long		aal5_pdu_errs;	/* CS layer protocol errors */
	u_long		aal5_pdu_drops;	/* CS PDUs dropped */
	u_long		aal5_pad[3];	/* Pad to quad-word boundary */
};
typedef struct stats_aal5	Stats_aal5;

struct stats_misc {
	u_long		buf1_sm_fail;	/* Alloc fail: buffer strat 1 small */
	u_long		buf1_lg_fail;	/* Alloc fail: buffer strat 1 large */
	u_long		buf2_sm_fail;	/* Alloc fail: buffer strat 2 small */
	u_long		buf2_lg_fail;	/* Alloc fail: buffer strat 2 large */
	u_long		rcvd_pdu_fail;	/* Received PDU allocation failure */
	u_long		carrier_status;	/* Carrier status */
	u_long		misc_pad[2];	/* Pad to quad-word boundary */
};
typedef struct stats_misc	Stats_misc;

struct fore_cp_stats {
	Stats_taxi	st_cp_taxi;	/* TAXI layer statistics */
	Stats_oc3	st_cp_oc3;	/* OC3 layer statistics */
	Stats_atm	st_cp_atm;	/* ATM layer statistics */
	Stats_aal0	st_cp_aal0;	/* AAL0 layer statistics */
	Stats_aal4	st_cp_aal4;	/* AAL3/4 layer statistics */
	Stats_aal5	st_cp_aal5;	/* AAL5 layer statistics */
	Stats_misc	st_cp_misc;	/* Miscellaneous statistics */
};
typedef struct fore_cp_stats	Fore_cp_stats;

#define	FORE_STATS_ALIGN	32

/*
 * CP PROM data - DMA'd to host with CMD_GET_PROM command
 */
struct fore_prom {
	u_long		pr_hwver;	/* Hardware version number */
	u_long		pr_serno;	/* Serial number */
	u_char		pr_mac[8];	/* MAC address */
};
typedef struct fore_prom	Fore_prom;

#define	FORE_PROM_ALIGN		32

#endif	/* _FORE_AALI_H */
