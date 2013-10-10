/*	$NetBSD: pdqreg.h,v 1.14 2001/06/13 10:46:03 wiz Exp $	*/

/*-
 * Copyright (c) 1995, 1996 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 * Id: pdqreg.h,v 1.11 1997/03/21 21:16:04 thomas Exp
 * $FreeBSD$
 *
 */

/*
 * DEC PDQ FDDI Controller; PDQ port driver definitions
 *
 */

#ifndef _PDQREG_H
#define	_PDQREG_H

#if !defined(KERNEL) && !defined(_KERNEL)
#include <stddef.h>
#elif !defined(offsetof)
#define	offsetof(t, m)	((char *) (&((t *)0L)->m) - (char *) 0L)
#endif
#if defined(PDQTEST) && !defined(PDQ_NDEBUG)
#include <assert.h>
#define	PDQ_ASSERT	assert
#else
#define	PDQ_ASSERT(x)	do { } while(0)
#endif

#define	PDQ_RING_SIZE(array)	((sizeof(array) / sizeof(array[0])))
#define	PDQ_ARRAY_SIZE(array)	((sizeof(array) / sizeof(array[0])))
#define	PDQ_RING_MASK(array)	(PDQ_RING_SIZE(array) - 1)
#define	PDQ_BITMASK(n)		(1L << (pdq_uint32_t) (n))

#define	PDQ_FDDI_MAX		4495
#define	PDQ_FDDI_LLC_MIN	20
#define	PDQ_FDDI_SMT_MIN	37

#define	PDQ_FDDI_SMT		0x40
#define	PDQ_FDDI_LLC_ASYNC	0x50
#define	PDQ_FDDI_LLC_SYNC	0xD0
#define	PDQ_FDDI_IMP_ASYNC	0x60
#define	PDQ_FDDI_IMP_SYNC	0xE0

#define	PDQ_FDDIFC_C		0x80
#define	PDQ_FDDIFC_L		0x40
#define	PDQ_FDDIFC_F		0x30
#define	PDQ_FDDIFC_Z		0x0F

#define	PDQ_FDDI_PH0		0x20
#define	PDQ_FDDI_PH1		0x38
#define	PDQ_FDDI_PH2		0x00

typedef pdq_uint32_t pdq_physaddr_t;

struct _pdq_lanaddr_t {
    pdq_uint8_t lanaddr_bytes[8];
};

typedef struct {
    pdq_uint8_t fwrev_bytes[4];
} pdq_fwrev_t;

enum _pdq_state_t {
    PDQS_RESET=0,
    PDQS_UPGRADE=1,
    PDQS_DMA_UNAVAILABLE=2,
    PDQS_DMA_AVAILABLE=3,
    PDQS_LINK_AVAILABLE=4,
    PDQS_LINK_UNAVAILABLE=5,
    PDQS_HALTED=6,
    PDQS_RING_MEMBER=7
};

struct _pdq_csrs_t {
    pdq_bus_memoffset_t csr_port_reset;			/* 0x00 [RW] */
    pdq_bus_memoffset_t csr_host_data;			/* 0x04 [R]  */
    pdq_bus_memoffset_t csr_port_control;		/* 0x08 [RW] */
    pdq_bus_memoffset_t csr_port_data_a;		/* 0x0C [RW] */
    pdq_bus_memoffset_t csr_port_data_b;		/* 0x10 [RW] */
    pdq_bus_memoffset_t csr_port_status;		/* 0x14 [R]  */
    pdq_bus_memoffset_t csr_host_int_type_0;		/* 0x18 [RW] */
    pdq_bus_memoffset_t csr_host_int_enable;		/* 0x1C [RW] */
    pdq_bus_memoffset_t csr_type_2_producer;		/* 0x20 [RW] */
    pdq_bus_memoffset_t csr_cmd_response_producer;	/* 0x28 [RW] */
    pdq_bus_memoffset_t csr_cmd_request_producer;	/* 0x2C [RW] */
    pdq_bus_memoffset_t csr_host_smt_producer;		/* 0x30 [RW] */
    pdq_bus_memoffset_t csr_unsolicited_producer;	/* 0x34 [RW] */
    pdq_bus_t csr_bus;
    pdq_bus_memaddr_t csr_base;
};

struct _pdq_pci_csrs_t {
    pdq_bus_memoffset_t csr_pfi_mode_control;		/* 0x40 [RW] */
    pdq_bus_memoffset_t csr_pfi_status;			/* 0x44 [RW] */
    pdq_bus_memoffset_t csr_fifo_write;			/* 0x48 [RW] */
    pdq_bus_memoffset_t csr_fifo_read;			/* 0x4C [RW] */
    pdq_bus_t csr_bus;
    pdq_bus_memaddr_t csr_base;
};

#define PDQ_PFI_MODE_DMA_ENABLE		0x01	/* DMA Enable */
#define PDQ_PFI_MODE_PFI_PCI_INTR	0x02	/* PFI-to-PCI Int Enable */
#define PDQ_PFI_MODE_PDQ_PCI_INTR	0x04	/* PDQ-to-PCI Int Enable */

#define PDQ_PFI_STATUS_PDQ_INTR		0x10	/* PDQ Int received */
#define PDQ_PFI_STATUS_DMA_ABORT	0x08	/* PDQ DMA Abort asserted */

#define	PDQ_EISA_BURST_HOLDOFF			0x0040
#define	PDQ_EISA_SLOT_ID			0x0C80
#define	PDQ_EISA_SLOT_CTRL			0x0C84
#define	PDQ_EISA_MEM_ADD_CMP_0			0x0C85
#define	PDQ_EISA_MEM_ADD_CMP_1			0x0C86
#define	PDQ_EISA_MEM_ADD_CMP_2			0x0C87
#define	PDQ_EISA_MEM_ADD_HI_CMP_0		0x0C88
#define	PDQ_EISA_MEM_ADD_HI_CMP_1		0x0C89
#define	PDQ_EISA_MEM_ADD_HI_CMP_2		0x0C8A
#define	PDQ_EISA_MEM_ADD_MASK_0			0x0C8B
#define	PDQ_EISA_MEM_ADD_MASK_1			0x0C8C
#define	PDQ_EISA_MEM_ADD_MASK_2			0x0C8D
#define	PDQ_EISA_MEM_ADD_LO_CMP_0		0x0C8E
#define	PDQ_EISA_MEM_ADD_LO_CMP_1		0x0C8F
#define	PDQ_EISA_MEM_ADD_LO_CMP_2		0x0C90
#define	PDQ_EISA_IO_CMP_0_0			0x0C91
#define	PDQ_EISA_IO_CMP_0_1			0x0C92
#define	PDQ_EISA_IO_CMP_1_0			0x0C93
#define	PDQ_EISA_IO_CMP_1_1			0x0C94
#define	PDQ_EISA_IO_CMP_2_0			0x0C95
#define	PDQ_EISA_IO_CMP_2_1			0x0C96
#define	PDQ_EISA_IO_CMP_3_0			0x0C97
#define	PDQ_EISA_IO_CMP_3_1			0x0C98
#define	PDQ_EISA_IO_ADD_MASK_0_0		0x0C99
#define	PDQ_EISA_IO_ADD_MASK_0_1		0x0C9A
#define	PDQ_EISA_IO_ADD_MASK_1_0		0x0C9B
#define	PDQ_EISA_IO_ADD_MASK_1_1		0x0C9C
#define	PDQ_EISA_IO_ADD_MASK_2_0		0x0C9D
#define	PDQ_EISA_IO_ADD_MASK_2_1		0x0C9E
#define	PDQ_EISA_IO_ADD_MASK_3_0		0x0C9F
#define	PDQ_EISA_IO_ADD_MASK_3_1		0x0CA0
#define	PDQ_EISA_MOD_CONFIG_1			0x0CA1
#define	PDQ_EISA_MOD_CONFIG_2			0x0CA2
#define	PDQ_EISA_MOD_CONFIG_3			0x0CA3
#define	PDQ_EISA_MOD_CONFIG_4			0x0CA4
#define	PDQ_EISA_MOD_CONFIG_5			0x0CA5
#define	PDQ_EISA_MOD_CONFIG_6			0x0CA6
#define	PDQ_EISA_MOD_CONFIG_7			0x0CA7
#define	PDQ_EISA_DIP_SWITCH			0x0CA8
#define	PDQ_EISA_IO_CONFIG_STAT_0		0x0CA9
#define	PDQ_EISA_IO_CONFIG_STAT_1		0x0CAA
#define	PDQ_EISA_DMA_CONFIG			0x0CAB
#define	PDQ_EISA_INPUT_PORT			0x0CAC
#define	PDQ_EISA_OUTPUT_PORT			0x0CAD
#define	PDQ_EISA_FUNCTION_CTRL			0x0CAE

#define	PDQ_TC_CSR_OFFSET			0x00100000
#define	PDQ_TC_CSR_SPACE			0x0040
#define	PDQ_FBUS_CSR_OFFSET			0x00200000
#define	PDQ_FBUS_CSR_SPACE			0x0080

/*
 * Port Reset Data A Definitions
 */
#define	PDQ_PRESET_SKIP_SELFTEST	0x0004
#define	PDQ_PRESET_SOFT_RESET		0x0002
#define	PDQ_PRESET_UPGRADE		0x0001
/*
 * Port Control Register Definitions
 */
#define	PDQ_PCTL_CMD_ERROR		0x8000
#define	PDQ_PCTL_FLASH_BLAST		0x4000
#define	PDQ_PCTL_HALT			0x2000
#define	PDQ_PCTL_COPY_DATA		0x1000
#define	PDQ_PCTL_ERROR_LOG_START	0x0800
#define	PDQ_PCTL_ERROR_LOG_READ		0x0400
#define	PDQ_PCTL_XMT_DATA_FLUSH_DONE	0x0200
#define	PDQ_PCTL_DMA_INIT		0x0100
#define	PDQ_DMA_INIT_LW_BSWAP_DATA	0x02
#define	PDQ_DMA_INIT_LW_BSWAP_LITERAL	0x01
#define	PDQ_PCTL_INIT_START		0x0080
#define	PDQ_PCTL_CONSUMER_BLOCK		0x0040
#define	PDQ_PCTL_DMA_UNINIT		0x0020
#define	PDQ_PCTL_RING_MEMBER		0x0010
#define	PDQ_PCTL_MLA_READ		0x0008
#define	PDQ_PCTL_FW_REV_READ		0x0004
#define	PDQ_PCTL_DEVICE_SPECIFIC	0x0002
#define	PDQ_PCTL_SUB_CMD		0x0001

typedef enum {
    PDQ_SUB_CMD_LINK_UNINIT=1,
    PDQ_SUB_CMD_DMA_BURST_SIZE_SET=2,
    PDQ_SUB_CMD_PDQ_REV_GET=4
} pdq_sub_cmd_t;

typedef enum {
    PDQ_DMA_BURST_4LW=0,
    PDQ_DMA_BURST_8LW=1,
    PDQ_DMA_BURST_16LW=2,
    PDQ_DMA_BURST_32LW=3
} pdq_dma_burst_size_t;

typedef enum {
    PDQ_CHIP_REV_A_B_OR_C=0,
    PDQ_CHIP_REV_D=2,
    PDQ_CHIP_REV_E=4
} pdq_chip_rev_t;
/*
 * Port Status Register Definitions
 */
#define	PDQ_PSTS_RCV_DATA_PENDING	0x80000000ul
#define	PDQ_PSTS_XMT_DATA_PENDING	0x40000000ul
#define	PDQ_PSTS_HOST_SMT_PENDING	0x20000000ul
#define	PDQ_PSTS_UNSOL_PENDING		0x10000000ul
#define	PDQ_PSTS_CMD_RSP_PENDING	0x08000000ul
#define	PDQ_PSTS_CMD_REQ_PENDING	0x04000000ul
#define	PDQ_PSTS_TYPE_0_PENDING		0x02000000ul
#define	PDQ_PSTS_INTR_PENDING		0xFE000000ul
#define	PDQ_PSTS_ADAPTER_STATE(sts)	((pdq_state_t) (((sts) >> 8) & 0x07))
#define	PDQ_PSTS_HALT_ID(sts)		((pdq_halt_code_t) ((sts) & 0xFF))
/*
 * Host Interrupt Register Definitions
 */
#define	PDQ_HOST_INT_TX_ENABLE			0x80000000ul
#define	PDQ_HOST_INT_RX_ENABLE			0x40000000ul
#define	PDQ_HOST_INT_UNSOL_ENABLE		0x20000000ul
#define	PDQ_HOST_INT_HOST_SMT_ENABLE		0x10000000ul
#define	PDQ_HOST_INT_CMD_RSP_ENABLE		0x08000000ul
#define	PDQ_HOST_INT_CMD_RQST_ENABLE		0x04000000ul

#define	PDQ_HOST_INT_1MS			0x80
#define	PDQ_HOST_INT_20MS			0x40
#define	PDQ_HOST_INT_CSR_CMD_DONE		0x20
#define	PDQ_HOST_INT_STATE_CHANGE		0x10
#define	PDQ_HOST_INT_XMT_DATA_FLUSH		0x08
#define	PDQ_HOST_INT_NXM			0x04
#define	PDQ_HOST_INT_PM_PARITY_ERROR		0x02
#define	PDQ_HOST_INT_HOST_BUS_PARITY_ERROR	0x01
#define	PDQ_HOST_INT_FATAL_ERROR		0x07

typedef enum {
    PDQH_SELFTEST_TIMEOUT=0,
    PDQH_HOST_BUS_PARITY_ERROR=1,
    PDQH_HOST_DIRECTED_HALT=2,
    PDQH_SOFTWARE_FAULT=3,
    PDQH_HARDWARE_FAULT=4,
    PDQH_PC_TRACE_PATH_TEST=5,
    PDQH_DMA_ERROR=6,
    PDQH_IMAGE_CRC_ERROR=7,
    PDQH_ADAPTER_PROCESSOR_ERROR=8,
    PDQH_MAX=9
} pdq_halt_code_t;

typedef struct {
    pdq_uint16_t pdqcb_receives;
    pdq_uint16_t pdqcb_transmits;
    pdq_uint32_t pdqcb__filler1;
    pdq_uint32_t pdqcb_host_smt;
    pdq_uint32_t pdqcb__filler2;
    pdq_uint32_t pdqcb_unsolicited_event;
    pdq_uint32_t pdqcb__filler3;
    pdq_uint32_t pdqcb_command_response;
    pdq_uint32_t pdqcb__filler4;
    pdq_uint32_t pdqcb_command_request;
    pdq_uint32_t pdqcb__filler5[7];
} pdq_consumer_block_t;

#if defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN
#define	PDQ_BITFIELD2(a, b)		         b, a
#define	PDQ_BITFIELD3(a, b, c)		      c, b, a
#define	PDQ_BITFIELD4(a, b, c, d)	   d, c, b, a
#define	PDQ_BITFIELD5(a, b, c, d, e)	e, d, c, b, a
#define	PDQ_BITFIELD12(a, b, c, d, e, f, g, h, i, j, k, l)	\
					l, k, j, i, h, g, f, e, d, c, b, a
#else
#define	PDQ_BITFIELD2(a, b)		a, b
#define	PDQ_BITFIELD3(a, b, c)		a, b, c
#define	PDQ_BITFIELD4(a, b, c, d)	a, b, c, d
#define	PDQ_BITFIELD5(a, b, c, d, e)	a, b, c, d, e
#define	PDQ_BITFIELD12(a, b, c, d, e, f, g, h, i, j, k, l)	\
					a, b, c, d, e, f, g, h, i, j, k, l
#endif

typedef struct {
    pdq_uint32_t PDQ_BITFIELD5(rxd_pa_hi : 16,
			       rxd_seg_cnt : 4,
			       rxd_seg_len_hi : 9,
			       rxd_seg_len_lo : 2,
			       rxd_sop : 1);
    pdq_uint32_t rxd_pa_lo;
} pdq_rxdesc_t;

typedef union {
    pdq_uint32_t rxs_status;
    struct {
	pdq_uint32_t PDQ_BITFIELD12(st_len : 13,
				    st_rcc_ss : 2,
				    st_rcc_dd : 2,
				    st_rcc_reason : 3,
				    st_rcc_badcrc : 1,
				    st_rcc_badpdu : 1,
				    st_fsb__reserved : 2,
				    st_fsb_c : 1,
				    st_fsb_a : 1,
				    st_fsb_e : 1,
				    st_fsc : 3,
				    st__reserved : 2);
    } rxs_st;
} pdq_rxstatus_t;
#define rxs_len			rxs_st.st_len
#define rxs_rcc_ss		rxs_st.st_rcc_ss
#define rxs_rcc_dd		rxs_st.st_rcc_dd
#define rxs_rcc_reason		rxs_st.st_rcc_reason
#define rxs_rcc_badcrc		rxs_st.st_rcc_badcrc
#define rxs_rcc_badpdu		rxs_st.st_rcc_badpdu
#define rxs_fsb_c		rxs_st.st_fsb_c
#define rxs_fsb_a		rxs_st.st_fsb_a
#define rxs_fsb_e		rxs_st.st_fsb_e
#define rxs_fsc			rxs_st.st_fsc

#define	PDQ_RXS_RCC_DD_NO_MATCH		0x00
#define	PDQ_RXS_RCC_DD_PROMISC_MATCH	0x01
#define	PDQ_RXS_RCC_DD_CAM_MATCH	0x02
#define	PDQ_RXS_RCC_DD_MLA_MATCH	0x03

typedef struct {
    pdq_uint32_t PDQ_BITFIELD5(txd_pa_hi : 16,
			       txd_seg_len : 13,
			       txd_mbz : 1,
			       txd_eop : 1,
			       txd_sop : 1);
    pdq_uint32_t txd_pa_lo;
} pdq_txdesc_t;

typedef struct {
    pdq_rxdesc_t pdqdb_receives[256];		/* 2048;	0x0000..0x07FF */
    pdq_txdesc_t pdqdb_transmits[256];		/* 2048;	0x0800..0x0FFF */
    pdq_rxdesc_t pdqdb_host_smt[64];		/*  512;	0x1000..0x11FF */
    pdq_rxdesc_t pdqdb_unsolicited_events[16];	/*  128;	0x1200..0x127F */
    pdq_rxdesc_t pdqdb_command_responses[16];	/*  128;	0x1280..0x12FF */
    pdq_txdesc_t pdqdb_command_requests[16];	/*  128;	0x1300..0x137F */
    /*
     * The rest of the descriptor block is unused.
     * As such we could use it for other things.
     */
    pdq_uint32_t pdqdb__filler1[16];		/*   64;	0x1380..0x13BF */
    pdq_consumer_block_t pdqdb_consumer;	/*   64;	0x13C0..0x13FF */
    /*
     * The maximum command size is 512 so as long as thes
     * command is at least that long all will be fine.
     */
    pdq_uint32_t pdqdb__filler2[64];		/*  256;	0x1400..0x14FF */
    pdq_uint8_t pdqdb_cmd_request_buf[1024];	/* 1024;	0x1500..0x18FF */
    pdq_uint8_t pdqdb_cmd_response_buf[1024];	/* 1024;	0x1900..0x1CFF */
    pdq_uint32_t pdqdb__filler3[128];		/*  512;	0x1D00..0x1EFF */
    pdq_uint8_t pdqdb_tx_hdr[4];		/*    4;	0x1F00..0x1F03 */
    pdq_uint32_t pdqdb__filler4[63];		/*  252;	0x1F04..0x1FFF */
} pdq_descriptor_block_t;

#define	PDQ_SIZE_COMMAND_RESPONSE	512

typedef enum {
    PDQC_START=0,
    PDQC_FILTER_SET=1,
    PDQC_FILTER_GET=2,
    PDQC_CHARS_SET=3,
    PDQC_STATUS_CHARS_GET=4,
    PDQC_COUNTERS_GET=5,
    PDQC_COUNTERS_SET=6,
    PDQC_ADDR_FILTER_SET=7,
    PDQC_ADDR_FILTER_GET=8,
    PDQC_ERROR_LOG_CLEAR=9,
    PDQC_ERROR_LOG_GET=10,
    PDQC_FDDI_MIB_GET=11,
    PDQC_DEC_EXT_MIB_GET=12,
    PDQC_DEV_SPECIFIC_GET=13,
    PDQC_SNMP_SET=14,
    PDQC_SMT_MIB_GET=16,
    PDQC_SMT_MIB_SET=17,
    PDQC_BOGUS_CMD=18
} pdq_cmd_code_t;

typedef struct {
    /*
     * These value manage the available space in command/response
     * buffer area.
     */
    pdq_physaddr_t ci_pa_request_bufstart;
    pdq_uint8_t *ci_request_bufstart;
    pdq_physaddr_t ci_pa_response_bufstart;
    pdq_uint8_t *ci_response_bufstart;
    /*
     * Bitmask of commands to sent to the PDQ
     */
    pdq_uint32_t ci_pending_commands;
    /*
     * Variables to maintain the PDQ queues.
     */
    pdq_uint32_t ci_command_active;
    pdq_uint32_t ci_request_producer;
    pdq_uint32_t ci_response_producer;
    pdq_uint32_t ci_request_completion;
    pdq_uint32_t ci_response_completion;
    /*
     *
     */
    pdq_physaddr_t ci_pa_request_descriptors;
    pdq_physaddr_t ci_pa_response_descriptors;

    pdq_cmd_code_t ci_queued_commands[16];
} pdq_command_info_t;

#define	PDQ_SIZE_UNSOLICITED_EVENT	512
#define	PDQ_NUM_UNSOLICITED_EVENTS	(PDQ_OS_PAGESIZE / PDQ_SIZE_UNSOLICITED_EVENT)

typedef struct _pdq_unsolicited_event_t pdq_unsolicited_event_t;

typedef struct {
    pdq_physaddr_t ui_pa_bufstart;
    pdq_physaddr_t ui_pa_descriptors;
    pdq_unsolicited_event_t *ui_events;

    pdq_uint32_t ui_free;
    pdq_uint32_t ui_producer;
    pdq_uint32_t ui_completion;
} pdq_unsolicited_info_t;

#define	PDQ_RX_FC_OFFSET	(sizeof(pdq_rxstatus_t) + 3)
#define	PDQ_RX_SEGCNT		((PDQ_FDDI_MAX + PDQ_OS_DATABUF_SIZE - 1) / PDQ_OS_DATABUF_SIZE)
#define	PDQ_DO_TYPE2_PRODUCER(pdq) \
    PDQ_CSR_WRITE(&(pdq)->pdq_csrs, csr_type_2_producer, \
	  ((pdq)->pdq_rx_info.rx_producer << 0) \
	| ((pdq)->pdq_tx_info.tx_producer << 8) \
	| ((pdq)->pdq_rx_info.rx_completion << 16) \
	| ((pdq)->pdq_tx_info.tx_completion << 24))

#define	PDQ_DO_HOST_SMT_PRODUCER(pdq) \
    PDQ_CSR_WRITE(&(pdq)->pdq_csrs, csr_host_smt_producer, \
	  ((pdq)->pdq_host_smt_info.rx_producer   << 0) \
	| ((pdq)->pdq_host_smt_info.rx_completion << 8))\

#define	PDQ_ADVANCE(n, a, m)	((n) = ((n) + (a)) & (m))

typedef struct {
    void *q_head;
    void *q_tail;
} pdq_databuf_queue_t;

typedef struct {
    void *rx_buffers;
    pdq_physaddr_t rx_pa_descriptors;

    pdq_uint32_t rx_target;
    pdq_uint32_t rx_free;
    pdq_uint32_t rx_producer;
    pdq_uint32_t rx_completion;
} pdq_rx_info_t;

typedef struct {
    pdq_databuf_queue_t tx_txq;
    pdq_txdesc_t tx_hdrdesc;
    pdq_uint8_t tx_descriptor_count[256];
    pdq_physaddr_t tx_pa_descriptors;

    pdq_uint32_t tx_free;
    pdq_uint32_t tx_producer;
    pdq_uint32_t tx_completion;
} pdq_tx_info_t;

typedef struct _pdq_os_ctx_t pdq_os_ctx_t;
struct _pdq_t {
    pdq_csrs_t pdq_csrs;
    pdq_pci_csrs_t pdq_pci_csrs;
    pdq_type_t pdq_type;
    pdq_chip_rev_t pdq_chip_rev;
    pdq_lanaddr_t pdq_hwaddr;
    pdq_fwrev_t pdq_fwrev;
    pdq_descriptor_block_t *pdq_dbp;
    volatile pdq_consumer_block_t *pdq_cbp;
    pdq_uint32_t pdq_intrmask;
    pdq_uint32_t pdq_flags;
#define	PDQ_PROMISC	0x0001
#define	PDQ_ALLMULTI	0x0002
#define	PDQ_PASS_SMT	0x0004
#define	PDQ_RUNNING	0x0008
#define	PDQ_PRINTCHARS	0x0010
#define	PDQ_TXOK	0x0020
#define	PDQ_WANT_FDX	0x0040
#define	PDQ_IS_FDX	0x0080
#define	PDQ_IS_ONRING	0x0100
    const char *pdq_os_name;
    pdq_os_ctx_t *pdq_os_ctx;
    pdq_uint32_t pdq_unit;
    pdq_command_info_t pdq_command_info;
    pdq_unsolicited_info_t pdq_unsolicited_info;
    pdq_tx_info_t pdq_tx_info;
    pdq_rx_info_t pdq_rx_info;
    pdq_rx_info_t pdq_host_smt_info;
    void *pdq_receive_buffers[256];
    void *pdq_host_smt_buffers[64];
    pdq_physaddr_t pdq_pa_consumer_block;
    pdq_physaddr_t pdq_pa_descriptor_block;
};

#define	PDQ_DB_BUSPA(pdq, m) \
	((pdq)->pdq_pa_descriptor_block + \
		((volatile u_int8_t *) (m) - (u_int8_t *) (pdq)->pdq_dbp))


typedef enum {
    PDQR_SUCCESS=0,
    PDQR_FAILURE=1,
    PDQR_WARNING=2,
    PDQR_LOOP_MODE_BAD=3,
    PDQR_ITEM_CODE_BAD=4,
    PDQR_TVX_BAD=5,
    PDQR_TREQ_BAD=6,
    PDQR_RESTRICTED_TOKEN_BAD=7,
    PDQR_NO_EOL=12,
    PDQR_FILTER_STATE_BAD=13,
    PDQR_CMD_TYPE_BAD=14,
    PDQR_ADAPTER_STATE_BAD=15,
    PDQR_RING_PURGER_BAD=16,
    PDQR_LEM_THRESHOLD_BAD=17,
    PDQR_LOOP_NOT_SUPPORTED=18,
    PDQR_FLUSH_TIME_BAD=19,
    PDQR_NOT_YET_IMPLEMENTED=20,
    PDQR_CONFIG_POLICY_BAD=21,
    PDQR_STATION_ACTION_BAD=22,
    PDQR_MAC_ACTION_BAD=23,
    PDQR_CON_POLICIES_BAD=24,
    PDQR_MAC_LOOP_TIME_BAD=25,
    PDQR_TB_MAX_BAD=26,
    PDQR_LER_CUTOFF_BAD=27,
    PDQR_LER_ALARM_BAD=28,
    PDQR_MAC_PATHS_REQ_BAD=29,
    PDQR_MAC_T_REQ_BAD=30,
    PDQR_EMAC_RING_PURGER_BAD=31,
    PDQR_EMAC_RTOKEN_TIMOUT_AD=32,
    PDQR_NO_SUCH_ENTRY=33,
    PDQR_T_NOTIFY_BAD=34,
    PDQR_TR_MAX_EXP_BAD=35,
    PDQR_FRAME_ERR_THRESHOLD_BAD=36,
    PDQR_MAX_TREQ_BAD=37,
    PDQR_FULL_DUPLEX_ENABLE_BAD=38,
    PDQR_ITEM_INDEX_BAD=39
} pdq_response_code_t;

typedef enum {
    PDQI_EOL=0,
    PDQI_T_REQ=1,
    PDQI_TVX=2,
    PDQI_RESTRICTED_TOKEN=3,
    PDQI_LEM_THRESHOLD=4,
    PDQI_RING_PURGER=5,
    PDQI_COUNTER_INTERVAL=6,
    PDQI_IND_GROUP_PROM=7,
    PDQI_GROUP_PROM=8,
    PDQI_BROADCAST=9,
    PDQI_SMT_PROM=10,
    PDQI_SMT_USER=11,
    PDQI_RESERVED=12,
    PDQI_IMPLEMENTOR=13,
    PDQI_LOOPBACK_MODE=14,
    PDQI_SMT_CONFIG_POLICY=16,
    PDQI_SMT_CONNECTION_POLICY=17,
    PDQI_SMT_T_NOTIFY=18,
    PDQI_SMT_STATION_ACTION=19,
    PDQI_MAC_PATHS_REQUESTED=21,
    PDQI_MAC_ACTION=23,
    PDQI_PORT_CONNECTION_POLICIES=24,
    PDQI_PORT_PATHS_REQUESTED=25,
    PDQI_PORT_MAC_LOOP_TIME=26,
    PDQI_PORT_TB_MAX=27,
    PDQI_PORT_LER_CUTOFF=28,
    PDQI_PORT_LER_ALARM=29,
    PDQI_PORT_ACTION=30,
    PDQI_FLUSH_TIME=32,
    PDQI_SMT_USER_DATA=33,
    PDQI_SMT_STATUS_REPORT_POLICY=34,
    PDQI_SMT_TRACE_MAX_EXPIRATION=35,
    PDQI_MAC_FRAME_ERR_THRESHOLD=36,
    PDQI_MAC_UNIT_DATA_ENABLE=37,
    PDQI_PATH_TVX_LOWER_BOUND=38,
    PDQI_PATH_TMAX_LOWER_BOUND=39,
    PDQI_PATH_MAX_TREQ=40,
    PDQI_MAC_TREQ=41,
    PDQI_EMAC_RING_PURGER=42,
    PDQI_EMAC_RTOKEN_TIMEOUT=43,
    PDQI_FULL_DUPLEX_ENABLE=44
} pdq_item_code_t;

typedef enum {
    PDQSNMP_EOL=0,
    PDQSNMP_FULL_DUPLEX_ENABLE=0x2F11
} pdq_snmp_item_code_t;

enum _pdq_boolean_t {
    PDQ_FALSE=0,
    PDQ_TRUE=1
};

typedef enum {
    PDQ_FILTER_BLOCK=0,
    PDQ_FILTER_PASS=1
} pdq_filter_state_t;

typedef enum {
    PDQ_STATION_TYPE_SAS=0,
    PDQ_STATION_TYPE_DAC=1,
    PDQ_STATION_TYPE_SAC=2,
    PDQ_STATION_TYPE_NAC=3,
    PDQ_STATION_TYPE_DAS=4
} pdq_station_type_t;

typedef enum {
    PDQ_STATION_STATE_OFF=0,
    PDQ_STATION_STATE_ON=1,
    PDQ_STATION_STATE_LOOPBACK=2
} pdq_station_state_t;

typedef enum {
    PDQ_LINK_STATE_OFF_READY=1,
    PDQ_LINK_STATE_OFF_FAULT_RECOVERY=2,
    PDQ_LINK_STATE_ON_RING_INIT=3,
    PDQ_LINK_STATE_ON_RING_RUN=4,
    PDQ_LINK_STATE_BROKEN=5
} pdq_link_state_t;

typedef enum {
    PDQ_DA_TEST_STATE_UNKNOWN=0,
    PDQ_DA_TEST_STATE_SUCCESS=1,
    PDQ_DA_TEST_STATE_DUPLICATE=2
} pdq_da_test_state_t;

typedef enum {
    PDQ_RING_PURGER_STATE_OFF=0,
    PDQ_RING_PURGER_STATE_CANDIDATE=1,
    PDQ_RING_PURGER_STATE_NON_PURGER=2,
    PDQ_RING_PURGER_STATE_PURGER=3
} pdq_ring_purger_state_t;

typedef enum {
    PDQ_FRAME_STRING_MODE_SA_MATCH=0,
    PDQ_FRAME_STRING_MODE_FCI_STRIP=1
} pdq_frame_strip_mode_t;

typedef enum {
    PDQ_RING_ERROR_REASON_NO_ERROR=0,
    PDQ_RING_ERROR_REASON_RING_INIT_INITIATED=5,
    PDQ_RING_ERROR_REASON_RING_INIT_RECEIVED=6,
    PDQ_RING_ERROR_REASON_RING_BEACONING_INITIATED=7,
    PDQ_RING_ERROR_REASON_DUPLICATE_ADDRESS_DETECTED=8,
    PDQ_RING_ERROR_REASON_DUPLICATE_TOKEN_DETECTED=9,
    PDQ_RING_ERROR_REASON_RING_PURGER_ERROR=10,
    PDQ_RING_ERROR_REASON_FCI_STRIP_ERROR=11,
    PDQ_RING_ERROR_REASON_RING_OP_OSCILLATION=12,
    PDQ_RING_ERROR_REASON_DIRECTED_BEACON_RECEVIED=13,
    PDQ_RING_ERROR_REASON_PC_TRACE_INITIATED=14,
    PDQ_RING_ERROR_REASON_PC_TRACE_RECEVIED=15
} pdq_ring_error_reason_t;

typedef enum {
    PDQ_STATION_MODE_NORMAL=0,
    PDQ_STATION_MODE_INTERNAL_LOOPBACK=1
} pdq_station_mode_t;

typedef enum {
    PDQ_PHY_TYPE_A=0,
    PDQ_PHY_TYPE_B=1,
    PDQ_PHY_TYPE_S=2,
    PDQ_PHY_TYPE_M=3,
    PDQ_PHY_TYPE_UNKNOWN=4
} pdq_phy_type_t;

typedef enum {
    PDQ_PMD_TYPE_ANSI_MUTLI_MODE=0,
    PDQ_PMD_TYPE_ANSI_SINGLE_MODE_TYPE_1=1,
    PDQ_PMD_TYPE_ANSI_SIGNLE_MODE_TYPE_2=2,
    PDQ_PMD_TYPE_ANSI_SONET=3,
    PDQ_PMD_TYPE_LOW_POWER=100,
    PDQ_PMD_TYPE_THINWIRE=101,
    PDQ_PMD_TYPE_SHIELDED_TWISTED_PAIR=102,
    PDQ_PMD_TYPE_UNSHIELDED_TWISTED_PAIR=103
} pdq_pmd_type_t;

typedef enum {
    PDQ_PMD_CLASS_ANSI_MULTI_MODE=0,
    PDQ_PMD_CLASS_SINGLE_MODE_TYPE_1=1,
    PDQ_PMD_CLASS_SINGLE_MODE_TYPE_2=2,
    PDQ_PMD_CLASS_SONET=3,
    PDQ_PMD_CLASS_LOW_COST_POWER_FIBER=4,
    PDQ_PMD_CLASS_TWISTED_PAIR=5,
    PDQ_PMD_CLASS_UNKNOWN=6,
    PDQ_PMD_CLASS_UNSPECIFIED=7
} pdq_pmd_class_t;

typedef enum {
    PDQ_PHY_STATE_INTERNAL_LOOPBACK=0,
    PDQ_PHY_STATE_BROKEN=1,
    PDQ_PHY_STATE_OFF_READY=2,
    PDQ_PHY_STATE_WAITING=3,
    PDQ_PHY_STATE_STARTING=4,
    PDQ_PHY_STATE_FAILED=5,
    PDQ_PHY_STATE_WATCH=6,
    PDQ_PHY_STATE_INUSE=7
} pdq_phy_state_t;

typedef enum {
    PDQ_REJECT_REASON_NONE=0,
    PDQ_REJECT_REASON_LOCAL_LCT=1,
    PDQ_REJECT_REASON_REMOTE_LCT=2,
    PDQ_REJECT_REASON_LCT_BOTH_SIDES=3,
    PDQ_REJECT_REASON_LEM_REJECT=4,
    PDQ_REJECT_REASON_TOPOLOGY_ERROR=5,
    PDQ_REJECT_REASON_NOISE_REJECT=6,
    PDQ_REJECT_REASON_REMOTE_REJECT=7,
    PDQ_REJECT_REASON_TRACE_IN_PROGRESS=8,
    PDQ_REJECT_REASON_TRACE_RECEIVED_DISABLED=9,
    PDQ_REJECT_REASON_STANDBY=10,
    PDQ_REJECT_REASON_LCT_PROTOCOL_ERROR=11
} pdq_reject_reason_t;

typedef enum {
    PDQ_BROKEN_REASON_NONE=0
} pdq_broken_reason_t;

typedef enum {
    PDQ_RI_REASON_TVX_EXPIRED=0,
    PDQ_RI_REASON_TRT_EXPIRED=1,
    PDQ_RI_REASON_RING_PURGER_ELECTION_ATTEMPT_LIMIT_EXCEEDED=2,
    PDQ_RI_REASON_PURGE_ERROR_LIMIT_EXCEEDED=3,
    PDQ_RI_REASON_RESTRICTED_TOKEN_TIMEOUT=4
} pdq_ri_reason_t;

typedef enum {
    PDQ_LCT_DIRECTION_LOCAL_LCT=0,
    PDQ_LCT_DIRECTION_REMOTE_LCT=1,
    PDQ_LCT_DIRECTION_LCT_BOTH_SIDES=2
} pdq_lct_direction_t;

typedef enum {
    PDQ_PORT_A=0,
    PDQ_PORT_B=1
} pdq_port_type_t;

typedef struct {
    pdq_uint8_t station_id_bytes[8];
} pdq_station_id_t;

typedef pdq_uint32_t pdq_fdditimer_t;
/*
 * Command format for Start, Filter_Get, ... commands
 */
typedef struct {
    pdq_cmd_code_t generic_op;
} pdq_cmd_generic_t;

/*
 * Response format for Start, Filter_Set, ... commands
 */
typedef struct {
    pdq_uint32_t generic_reserved;
    pdq_cmd_code_t generic_op;
    pdq_response_code_t generic_status;
} pdq_response_generic_t;

/*
 * Command format for Filter_Set command
 */
typedef struct {
    pdq_cmd_code_t filter_set_op;
    struct {
	pdq_item_code_t item_code;
	pdq_filter_state_t filter_state;
    } filter_set_items[7];
    pdq_item_code_t filter_set_eol_item_code;
} pdq_cmd_filter_set_t;

/*
 * Response format for Filter_Get command.
 */
typedef struct {
    pdq_uint32_t filter_get_reserved;
    pdq_cmd_code_t filter_get_op;
    pdq_response_code_t filter_get_status;
    pdq_filter_state_t filter_get_ind_group_prom;
    pdq_filter_state_t filter_get_group_prom;
    pdq_filter_state_t filter_get_broadcast_all;
    pdq_filter_state_t filter_get_smt_prom;
    pdq_filter_state_t filter_get_smt_user;
    pdq_filter_state_t filter_get_reserved_all;
    pdq_filter_state_t filter_get_implementor_all;
} pdq_response_filter_get_t;

#define	PDQ_SIZE_RESPONSE_FILTER_GET	0x28

typedef struct {
    pdq_cmd_code_t chars_set_op;
    struct {
	pdq_item_code_t item_code;
	pdq_uint32_t item_value;
	pdq_port_type_t item_port;
    } chars_set_items[1];
    pdq_item_code_t chars_set_eol_item_code;
} pdq_cmd_chars_set_t;

typedef struct {
    pdq_cmd_code_t addr_filter_set_op;
    pdq_lanaddr_t addr_filter_set_addresses[62];
} pdq_cmd_addr_filter_set_t;

#define	PDQ_SIZE_CMD_ADDR_FILTER_SET	0x1F4

typedef struct {
    pdq_uint32_t addr_filter_get_reserved;
    pdq_cmd_code_t addr_filter_get_op;
    pdq_response_code_t addr_filter_get_status;
    pdq_lanaddr_t addr_filter_get_addresses[62];
} pdq_response_addr_filter_get_t;

#define	PDQ_SIZE_RESPONSE_ADDR_FILTER_GET	0x1FC

typedef struct {
    pdq_uint32_t status_chars_get_reserved;
    pdq_cmd_code_t status_chars_get_op;
    pdq_response_code_t status_chars_get_status;
    struct {
	/* Station Characteristic Attributes */
	pdq_station_id_t station_id;
	pdq_station_type_t station_type;
	pdq_uint32_t smt_version_id;
	pdq_uint32_t smt_max_version_id;
	pdq_uint32_t smt_min_version_id;
	/* Station Status Attributes */
	pdq_station_state_t station_state;
	/* Link Characteristic Attributes */
	pdq_lanaddr_t link_address;
	pdq_fdditimer_t t_req;
	pdq_fdditimer_t tvx;
	pdq_fdditimer_t restricted_token_timeout;
	pdq_boolean_t ring_purger_enable;
	pdq_link_state_t link_state;
	pdq_fdditimer_t negotiated_trt;
	pdq_da_test_state_t dup_addr_flag;
	/* Link Status Attributes */
	pdq_lanaddr_t upstream_neighbor;
	pdq_lanaddr_t old_upstream_neighbor;
	pdq_boolean_t upstream_neighbor_dup_addr_flag;
	pdq_lanaddr_t downstream_neighbor;
	pdq_lanaddr_t old_downstream_neighbor;
	pdq_ring_purger_state_t ring_purger_state;
	pdq_frame_strip_mode_t frame_strip_mode;
	pdq_ring_error_reason_t ring_error_reason;
	pdq_boolean_t loopback;
	pdq_fdditimer_t ring_latency;
	pdq_lanaddr_t last_dir_beacon_sa;
	pdq_lanaddr_t last_dir_beacon_una;
	/* Phy Characteristic Attributes */
	pdq_phy_type_t phy_type[2];
	pdq_pmd_type_t pmd_type[2];
	pdq_uint32_t lem_threshold[2];
	/* Phy Status Attributes */
	pdq_phy_state_t phy_state[2];
	pdq_phy_type_t neighbor_phy_type[2];
	pdq_uint32_t link_error_estimate[2];
	pdq_broken_reason_t broken_reason[2];
	pdq_reject_reason_t reject_reason[2];
	/* Miscellaneous */
	pdq_uint32_t counter_interval;
	pdq_fwrev_t module_rev;
	pdq_fwrev_t firmware_rev;
	pdq_uint32_t mop_device_type;
	pdq_uint32_t fddi_led[2];
	pdq_uint32_t flush;
    } status_chars_get;
} pdq_response_status_chars_get_t;

#define	PDQ_SIZE_RESPONSE_STATUS_CHARS_GET	0xF0

typedef struct {
    pdq_uint32_t fddi_mib_get_reserved;
    pdq_cmd_code_t fddi_mib_get_op;
    pdq_response_code_t fddi_mib_get_status;
    struct {
	/* SMT Objects */
	pdq_station_id_t smt_station_id;
	pdq_uint32_t smt_op_version_id;
	pdq_uint32_t smt_hi_version_id;
	pdq_uint32_t smt_lo_version_id;
	pdq_uint32_t smt_mac_ct;
	pdq_uint32_t smt_non_master_ct;
	pdq_uint32_t smt_master_ct;
	pdq_uint32_t smt_paths_available;
	pdq_uint32_t smt_config_capabilities;
	pdq_uint32_t smt_config_policy;
	pdq_uint32_t smt_connection_policy;
	pdq_uint32_t smt_t_notify;
	pdq_uint32_t smt_status_reporting;
	pdq_uint32_t smt_ecm_state;
	pdq_uint32_t smt_cf_state;
	pdq_uint32_t smt_hold_state;
	pdq_uint32_t smt_remote_disconnect_flag;
	pdq_uint32_t smt_station_action;
	/* MAC Objects */
	pdq_uint32_t mac_frame_status_capabilities;
	pdq_uint32_t mac_t_max_greatest_lower_bound;
	pdq_uint32_t mac_tvx_greatest_lower_bound;
	pdq_uint32_t mac_paths_available;
	pdq_uint32_t mac_current_path;
	pdq_lanaddr_t mac_upstream_neighbor;
	pdq_lanaddr_t mac_old_upstream_neighbor;
	pdq_uint32_t mac_dup_addr_test;
	pdq_uint32_t mac_paths_requested;
	pdq_uint32_t mac_downstream_port_type;
	pdq_lanaddr_t mac_smt_address;
	pdq_uint32_t mac_t_req;
	pdq_uint32_t mac_t_neg;
	pdq_uint32_t mac_t_max;
	pdq_uint32_t mac_tvx_value;
	pdq_uint32_t mac_t_min;
	pdq_uint32_t mac_current_frame_status;
	pdq_uint32_t mac_frame_error_threshold;
	pdq_uint32_t mac_frame_error_ratio;
	pdq_uint32_t mac_rmt_state;
	pdq_uint32_t mac_da_flag;
	pdq_uint32_t mac_una_da_flag;
	pdq_uint32_t mac_frame_condition;
	pdq_uint32_t mac_chip_set;
	pdq_uint32_t mac_action;
	/* Port Objects */
	pdq_uint32_t port_pc_type[2];
	pdq_uint32_t port_pc_neighbor[2];
	pdq_uint32_t port_connection_policies[2];
	pdq_uint32_t port_remote_mac_indicated[2];
	pdq_uint32_t port_ce_state[2];
	pdq_uint32_t port_paths_requested[2];
	pdq_uint32_t port_mac_placement[2];
	pdq_uint32_t port_available_paths[2];
	pdq_uint32_t port_mac_loop_time[2];
	pdq_uint32_t port_tb_max[2];
	pdq_uint32_t port_bs_flag[2];
	pdq_uint32_t port_ler_estimate[2];
	pdq_uint32_t port_ler_cutoff[2];
	pdq_uint32_t port_ler_alarm[2];
	pdq_uint32_t port_connect_state[2];
	pdq_uint32_t port_pcm_state[2];
	pdq_uint32_t port_pc_withhold[2];
	pdq_uint32_t port_ler_condition[2];
	pdq_uint32_t port_chip_set[2];
	pdq_uint32_t port_action[2];
	/* Attachment Objects */
	pdq_uint32_t attachment_class;
	pdq_uint32_t attachment_optical_bypass_present;
	pdq_uint32_t attachment_imax_expiration;
	pdq_uint32_t attachment_inserted_status;
	pdq_uint32_t attachment_insert_policy;
    } fddi_mib_get;
} pdq_response_fddi_mib_get_t;

#define	PDQ_SIZE_RESPONSE_FDDI_MIB_GET	0x17C

typedef enum {
    PDQ_FDX_STATE_IDLE=0,
    PDQ_FDX_STATE_REQUEST=1,
    PDQ_FDX_STATE_CONFIRM=2,
    PDQ_FDX_STATE_OPERATION=3
} pdq_fdx_state_t;

typedef struct {
    pdq_uint32_t dec_ext_mib_get_reserved;
    pdq_cmd_code_t dec_ext_mib_get_op;
    pdq_response_code_t dec_ext_mib_get_response;
    struct {
	/* SMT Objects */
	pdq_uint32_t esmt_station_type;
	/* MAC Objects */
	pdq_uint32_t emac_link_state;
	pdq_uint32_t emac_ring_purger_state;
	pdq_uint32_t emac_ring_purger_enable;
	pdq_uint32_t emac_frame_strip_mode;
	pdq_uint32_t emac_ring_error_reason;
	pdq_uint32_t emac_upstream_nbr_dupl_address_flag;
	pdq_uint32_t emac_restricted_token_timeout;
	/* Port Objects */
	pdq_uint32_t eport_pmd_type[2];
	pdq_uint32_t eport_phy_state[2];
	pdq_uint32_t eport_reject_reason[2];
	/* Full Duplex Objects */
	pdq_boolean_t fdx_enable;
	pdq_boolean_t fdx_operational;
	pdq_fdx_state_t fdx_state;
    } dec_ext_mib_get;
} pdq_response_dec_ext_mib_get_t;

#define	PDQ_SIZE_RESPONSE_DEC_EXT_MIB_GET	0x50

typedef struct {
    pdq_cmd_code_t snmp_set_op;
    struct {
	pdq_item_code_t item_code;
	pdq_uint32_t item_value;
	pdq_port_type_t item_port;
    } snmp_set_items[7];
    pdq_item_code_t snmp_set_eol_item_code;
} pdq_cmd_snmp_set_t;

typedef enum {
    PDQ_CALLER_ID_NONE=0,
    PDQ_CALLER_ID_SELFTEST=1,
    PDQ_CALLER_ID_MFG=2,
    PDQ_CALLER_ID_FIRMWARE=5,
    PDQ_CALLER_ID_CONSOLE=8
} pdq_caller_id_t;

typedef struct {
    pdq_uint32_t error_log_get__reserved;
    pdq_cmd_code_t error_log_get_op;
    pdq_response_code_t error_log_get_status;
    /* Error Header */
    pdq_uint32_t error_log_get_event_status;
    /* Event Information Block */
    pdq_caller_id_t error_log_get_caller_id;
    pdq_uint32_t error_log_get_timestamp[2];
    pdq_uint32_t error_log_get_write_count;
    /* Diagnostic Information */
    pdq_uint32_t error_log_get_fru_implication_mask;
    pdq_uint32_t error_log_get_test_id;
    pdq_uint32_t error_log_get_diag_reserved[6];
    /* Firmware Information */
    pdq_uint32_t error_log_get_fw_reserved[112];
} pdq_response_error_log_get_t;


/*
 * Definitions for the Unsolicited Event Queue.
 */
typedef enum {
    PDQ_UNSOLICITED_EVENT=0,
    PDQ_UNSOLICITED_COUNTERS=1
} pdq_event_t;

typedef enum {
    PDQ_ENTITY_STATION=0,
    PDQ_ENTITY_LINK=1,
    PDQ_ENTITY_PHY_PORT=2,
    PDQ_ENTITY_MAX=3
} pdq_entity_t;

typedef enum {
    PDQ_STATION_EVENT_TRACE_RECEIVED=1,
    PDQ_STATION_EVENT_MAX=2
} pdq_station_event_t;

typedef enum {
    PDQ_STATION_EVENT_ARGUMENT_REASON=0,	/* pdq_uint32_t */
    PDQ_STATION_EVENT_ARGUMENT_EOL=0xFF
} pdq_station_event_argument_t;

typedef enum {
    PDQ_LINK_EVENT_TRANSMIT_UNDERRUN=0,
    PDQ_LINK_EVENT_TRANSMIT_FAILED=1,
    PDQ_LINK_EVENT_BLOCK_CHECK_ERROR=2,
    PDQ_LINK_EVENT_FRAME_STATUS_ERROR=3,
    PDQ_LINK_EVENT_PDU_LENGTH_ERROR=4,
    PDQ_LINK_EVENT_RECEIVE_DATA_OVERRUN=7,
    PDQ_LINK_EVENT_NO_USER_BUFFER=9,
    PDQ_LINK_EVENT_RING_INITIALIZATION_INITIATED=10,
    PDQ_LINK_EVENT_RING_INITIALIZATION_RECEIVED=11,
    PDQ_LINK_EVENT_RING_BEACON_INITIATED=12,
    PDQ_LINK_EVENT_DUPLICATE_ADDRESS_FAILURE=13,
    PDQ_LINK_EVENT_DUPLICATE_TOKEN_DETECTED=14,
    PDQ_LINK_EVENT_RING_PURGE_ERROR=15,
    PDQ_LINK_EVENT_FCI_STRIP_ERROR=16,
    PDQ_LINK_EVENT_TRACE_INITIATED=17,
    PDQ_LINK_EVENT_DIRECTED_BEACON_RECEIVED=18,
    PDQ_LINK_EVENT_MAX=19
} pdq_link_event_t;

typedef enum {
    PDQ_LINK_EVENT_ARGUMENT_REASON=0,		/* pdq_rireason_t */
    PDQ_LINK_EVENT_ARGUMENT_DATA_LINK_HEADER=1,	/* pdq_dlhdr_t */
    PDQ_LINK_EVENT_ARGUMENT_SOURCE=2,		/* pdq_lanaddr_t */
    PDQ_LINK_EVENT_ARGUMENT_UPSTREAM_NEIGHBOR=3,/* pdq_lanaddr_t */	
    PDQ_LINK_EVENT_ARGUMENT_EOL=0xFF
} pdq_link_event_argument_t;

typedef enum {
    PDQ_PHY_EVENT_LEM_ERROR_MONITOR_REJECT=0,
    PDQ_PHY_EVENT_ELASTICITY_BUFFER_ERROR=1,
    PDQ_PHY_EVENT_LINK_CONFIDENCE_TEST_REJECT=2,
    PDQ_PHY_EVENT_MAX=3
} pdq_phy_event_t;

typedef enum {
    PDQ_PHY_EVENT_ARGUMENT_DIRECTION=0,		/* pdq_lct_direction_t */
    PDQ_PHY_EVENT_ARGUMENT_EOL=0xFF
} pdq_phy_event_arguments;

struct _pdq_unsolicited_event_t {
    pdq_uint32_t rvent_reserved;
    pdq_event_t event_type;
    pdq_entity_t event_entity;
    pdq_uint32_t event_index;
    union {
	pdq_station_event_t station_event;
	pdq_link_event_t link_event;
	pdq_phy_event_t phy_event;
	pdq_uint32_t value;
    } event_code;
    /*
     * The remainder of this event is an argument list.
     */
    pdq_uint32_t event__filler[123];
};

#endif /* _PDQREG_H */
