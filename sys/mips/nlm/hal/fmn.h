/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 * NETLOGIC_BSD */

#ifndef __NLM_FMNV2_H__
#define __NLM_FMNV2_H__

/**
* @file_name fmn.h
* @author Netlogic Microsystems
* @brief HAL for Fast message network V2
*/

/* FMN configuration registers */
#define XLP_CMS_OUTPUTQ_CONFIG_REG(i)	((i)*2)
#define XLP_CMS_MAX_OUTPUTQ		1024
#define XLP_CMS_OUTPUTQ_CREDIT_CFG_REG	(0x2000/4)
#define XLP_CMS_MSG_CONFIG_REG		(0x2008/4)
#define XLP_CMS_MSG_ERR_REG		(0x2010/4)
#define XLP_CMS_TRACE_CONFIG_REG	(0x2018/4)
#define XLP_CMS_TRACE_BASE_ADDR_REG	(0x2020/4)
#define XLP_CMS_TRACE_LIMIT_ADDR_REG	(0x2028/4)
#define XLP_CMS_TRACE_CURRENT_ADDR_REG	(0x2030/4)
#define XLP_CMS_MSG_ENDIAN_SWAP_REG	(0x2038/4)

#define XLP_CMS_CPU_PUSHQ(node, core, thread, vc)	\
		(((node)<<10) | ((core)<<4) | ((thread)<<2) | ((vc)<<0))
#define XLP_CMS_POPQ(node, queue)	(((node)<<10) | (queue))
#define XLP_CMS_IO_PUSHQ(node, queue)	(((node)<<10) | (queue))

#define XLP_CMS_POPQ_QID(i)	(128+(i))
#define XLP_CMS_POPQ_MAXQID	255
#define XLP_CMS_PCIE0_QID(i)	(256+(i))
#define XLP_CMS_PCIE0_MAXQID	257
#define XLP_CMS_PCIE1_QID(i)	(258+(i))
#define XLP_CMS_PCIE1_MAXQID	259
#define XLP_CMS_PCIE2_QID(i)	(260+(i))
#define XLP_CMS_PCIE2_MAXQID	261
#define XLP_CMS_PCIE3_QID(i)	(262+(i))
#define XLP_CMS_PCIE3_MAXQID	263
#define XLP_CMS_DTE_QID(i)	(264+(i))
#define XLP_CMS_DTE_MAXQID	267
#define XLP_CMS_RSA_ECC_QID(i)	(272+(i))
#define XLP_CMS_RSA_ECC_MAXQID	280
#define XLP_CMS_CRYPTO_QID(i)	(281+(i))
#define XLP_CMS_CRYPTO_MAXQID	296
/* TODO PCI header register 0x3C says CMP starts at 297(0x129) VERIFY */
#define XLP_CMS_CMP_QID(i)	(298+(i))
#define XLP_CMS_CMP_MAXQID	305
#define XLP_CMS_POE_QID(i)	(384+(i))
#define XLP_CMS_POE_MAXQID	391
#define XLP_CMS_NAE_QID(i)	(476+(i))
#define XLP_CMS_NAE_MAXQID	1023

#define XLP_CMS_NAE_TX_VC_BASE	476
#define XLP_CMS_NAE_TX_VC_LIMIT	999
#define XLP_CMS_NAE_RX_VC_BASE	1000
#define XLP_CMS_NAE_RX_VC_LIMIT	1019

#define XLP_MAX_CMS_QUEUES	1024

/* FMN Level Interrupt Type */
#define XLP_CMS_LVL_INTR_DISABLE	0
#define XLP_CMS_LVL_LOW_WATERMARK	1
#define XLP_CMS_LVL_HI_WATERMARK	2

/* FMN Level interrupt trigger values */
#define XLP_CMS_QUEUE_NON_EMPTY			0
#define XLP_CMS_QUEUE_QUARTER_FULL		1
#define XLP_CMS_QUEUE_HALF_FULL			2
#define XLP_CMS_QUEUE_THREE_QUARTER_FULL	3
#define XLP_CMS_QUEUE_FULL			4

/* FMN Timer Interrupt Type */
#define XLP_CMS_TIMER_INTR_DISABLE	0
#define XLP_CMS_TIMER_CONSUMER		1
#define XLP_CMS_TIMER_PRODUCER		1

/* FMN timer interrupt trigger values */
#define XLP_CMS_TWO_POW_EIGHT_CYCLES		0
#define XLP_CMS_TWO_POW_TEN_CYCLES		1
#define XLP_CMS_TWO_POW_TWELVE_CYCLES		2
#define XLP_CMS_TWO_POW_FOURTEEN_CYCLES		3
#define XLP_CMS_TWO_POW_SIXTEEN_CYCLES		4
#define XLP_CMS_TWO_POW_EIGHTTEEN_CYCLES	5
#define XLP_CMS_TWO_POW_TWENTY_CYCLES		6
#define XLP_CMS_TWO_POW_TWENTYTWO_CYCLES	7

#define XLP_CMS_QUEUE_ENA		1ULL
#define XLP_CMS_QUEUE_DIS		0
#define XLP_CMS_SPILL_ENA		1ULL
#define XLP_CMS_SPILL_DIS		0

#define XLP_CMS_MAX_VCPU_VC		4

/* Each XLP chip can hold upto 32K messages on the chip itself */
#define XLP_CMS_ON_CHIP_MESG_SPACE	(32*1024)
#define XLP_CMS_ON_CHIP_PER_QUEUE_SPACE	\
		((XLP_CMS_ON_CHIP_MESG_SPACE)/(XLP_MAX_CMS_QUEUES))
#define XLP_CMS_MAX_ONCHIP_SEGMENTS	1024
#define XLP_CMS_MAX_SPILL_SEGMENTS_PER_QUEUE 	64

/* FMN Network error */
#define XLP_CMS_ILLEGAL_DST_ERROR		0x100
#define XLP_CMS_BIU_TIMEOUT_ERROR		0x080
#define XLP_CMS_BIU_ERROR			0x040
#define XLP_CMS_SPILL_FILL_UNCORRECT_ECC_ERROR	0x020
#define XLP_CMS_SPILL_FILL_CORRECT_ECC_ERROR	0x010
#define XLP_CMS_SPILL_UNCORRECT_ECC_ERROR	0x008
#define XLP_CMS_SPILL_CORRECT_ECC_ERROR		0x004
#define XLP_CMS_OUTPUTQ_UNCORRECT_ECC_ERROR	0x002
#define XLP_CMS_OUTPUTQ_CORRECT_ECC_ERROR	0x001

/* worst case, a single entry message consists of a 4 byte header
 * and an 8-byte entry = 12 bytes in total
 */
#define XLP_CMS_SINGLE_ENTRY_MSG_SIZE	12
/* total spill memory needed for one FMN queue */
#define XLP_CMS_PER_QUEUE_SPILL_MEM(spilltotmsgs)		\
		((spilltotmsgs) * (XLP_CMS_SINGLE_ENTRY_MSG_SIZE))
/* total spill memory needed */
#define XLP_CMS_TOTAL_SPILL_MEM(spilltotmsgs)			\
		((XLP_CMS_PER_QUEUE_SPILL_MEM(spilltotmsgs)) *	\
		(XLP_MAX_CMS_QUEUES))
/* total number of FMN messages possible in a queue */
#define XLP_CMS_TOTAL_QUEUE_SIZE(spilltotmsgs)			\
		((spilltotmsgs) + (XLP_CMS_ON_CHIP_PER_QUEUE_SPACE))

/* FMN Src station id's */
#define XLP_CMS_CPU0_SRC_STID		(0 << 4)
#define XLP_CMS_CPU1_SRC_STID		(1 << 4)
#define XLP_CMS_CPU2_SRC_STID		(2 << 4)
#define XLP_CMS_CPU3_SRC_STID		(3 << 4)
#define XLP_CMS_CPU4_SRC_STID		(4 << 4)
#define XLP_CMS_CPU5_SRC_STID		(5 << 4)
#define XLP_CMS_CPU6_SRC_STID		(6 << 4)
#define XLP_CMS_CPU7_SRC_STID		(7 << 4)
#define XLP_CMS_PCIE0_SRC_STID		256
#define XLP_CMS_PCIE1_SRC_STID		258
#define XLP_CMS_PCIE2_SRC_STID		260
#define XLP_CMS_PCIE3_SRC_STID		262
#define XLP_CMS_DTE_SRC_STID		264
#define XLP_CMS_RSA_ECC_SRC_STID	272
#define XLP_CMS_CRYPTO_SRC_STID		281
#define XLP_CMS_CMP_SRC_STID		298
#define XLP_CMS_POE_SRC_STID		384
#define XLP_CMS_NAE_SRC_STID		476
#if 0
#define XLP_CMS_DEFAULT_CREDIT(cmstotstns,spilltotmsgs)		\
		((XLP_CMS_TOTAL_QUEUE_SIZE(spilltotmsgs)) /	\
		(cmstotstns))
#endif
#define XLP_CMS_DEFAULT_CREDIT(cmstotstns,spilltotmsgs) 8

/* POPQ related defines */
#define XLP_CMS_POPQID_START		128
#define XLP_CMS_POPQID_END		255

#define XLP_CMS_INT_RCVD		0x800000000000000ULL

#define	nlm_rdreg_cms(b, r)	nlm_read_reg64_xkseg(b,r)
#define	nlm_wreg_cms(b, r, v)	nlm_write_reg64_xkseg(b,r,v)
#define nlm_pcibase_cms(node)	nlm_pcicfg_base(XLP_IO_CMS_OFFSET(node))
#define nlm_regbase_cms(node)	nlm_pcibar0_base_xkphys(nlm_pcibase_cms(node))

enum fmn_swcode {
	FMN_SWCODE_CPU0=1,
	FMN_SWCODE_CPU1,
	FMN_SWCODE_CPU2,
	FMN_SWCODE_CPU3,
	FMN_SWCODE_CPU4,
	FMN_SWCODE_CPU5,
	FMN_SWCODE_CPU6,
	FMN_SWCODE_CPU7,
	FMN_SWCODE_CPU8,
	FMN_SWCODE_CPU9,
	FMN_SWCODE_CPU10,
	FMN_SWCODE_CPU11,
	FMN_SWCODE_CPU12,
	FMN_SWCODE_CPU13,
	FMN_SWCODE_CPU14,
	FMN_SWCODE_CPU15,
	FMN_SWCODE_CPU16,
	FMN_SWCODE_CPU17,
	FMN_SWCODE_CPU18,
	FMN_SWCODE_CPU19,
	FMN_SWCODE_CPU20,
	FMN_SWCODE_CPU21,
	FMN_SWCODE_CPU22,
	FMN_SWCODE_CPU23,
	FMN_SWCODE_CPU24,
	FMN_SWCODE_CPU25,
	FMN_SWCODE_CPU26,
	FMN_SWCODE_CPU27,
	FMN_SWCODE_CPU28,
	FMN_SWCODE_CPU29,
	FMN_SWCODE_CPU30,
	FMN_SWCODE_CPU31,
	FMN_SWCODE_CPU32,
	FMN_SWCODE_PCIE0,
	FMN_SWCODE_PCIE1,
	FMN_SWCODE_PCIE2,
	FMN_SWCODE_PCIE3,
	FMN_SWCODE_DTE,
	FMN_SWCODE_CRYPTO,
	FMN_SWCODE_RSA,
	FMN_SWCODE_CMP,
	FMN_SWCODE_POE,
	FMN_SWCODE_NAE,
};

extern uint64_t nlm_cms_spill_total_messages;
extern uint32_t nlm_cms_total_stations;
extern uint32_t cms_onchip_seg_availability[XLP_CMS_ON_CHIP_PER_QUEUE_SPACE];

extern uint64_t cms_base_addr(int node);
extern int nlm_cms_verify_credit_config (int spill_en, int tot_credit);
extern int nlm_cms_get_oc_space(int qsize, int max_queues, int qid, int *ocbase, int *ocstart, int *ocend);
extern void nlm_cms_setup_credits (uint64_t base, int destid, int srcid, int credit);
extern int nlm_cms_config_onchip_queue (uint64_t base, uint64_t cms_spill_base, int qid, int spill_en);
extern void nlm_cms_default_setup(int node, uint64_t spill_base, int spill_en, int popq_en);
extern uint64_t nlm_cms_get_onchip_queue (uint64_t base, int qid);
extern void nlm_cms_set_onchip_queue (uint64_t base, int qid, uint64_t val);
extern void nlm_cms_per_queue_level_intr(uint64_t base, int qid, int sub_type, int intr_val);
extern void nlm_cms_level_intr(int node, int sub_type, int intr_val);
extern void nlm_cms_per_queue_timer_intr(uint64_t base, int qid, int sub_type, int intr_val);
extern void nlm_cms_timer_intr(int node, int en, int sub_type, int intr_val);
extern int nlm_cms_outputq_intr_check(uint64_t base, int qid);
extern void nlm_cms_outputq_clr_intr(uint64_t base, int qid);
extern void nlm_cms_illegal_dst_error_intr(uint64_t base, int en);
extern void nlm_cms_timeout_error_intr(uint64_t base, int en);
extern void nlm_cms_biu_error_resp_intr(uint64_t base, int en);
extern void nlm_cms_spill_uncorrectable_ecc_error_intr(uint64_t base, int en);
extern void nlm_cms_spill_correctable_ecc_error_intr(uint64_t base, int en);
extern void nlm_cms_outputq_uncorrectable_ecc_error_intr(uint64_t base, int en);
extern void nlm_cms_outputq_correctable_ecc_error_intr(uint64_t base, int en);
extern uint64_t nlm_cms_network_error_status(uint64_t base);
extern int nlm_cms_get_net_error_code(uint64_t err);
extern int nlm_cms_get_net_error_syndrome(uint64_t err);
extern int nlm_cms_get_net_error_ramindex(uint64_t err);
extern int nlm_cms_get_net_error_outputq(uint64_t err);
extern void nlm_cms_trace_setup(uint64_t base, int en, uint64_t trace_base, uint64_t trace_limit, int match_dstid_en, int dst_id, int match_srcid_en, int src_id, int wrap);
extern void nlm_cms_endian_byte_swap (uint64_t base, int en);
extern uint8_t xlp_msg_send(uint8_t vc, uint8_t size);
extern int nlm_cms_alloc_spill_q(uint64_t base, int qid, uint64_t spill_base,
	int nsegs);
extern int nlm_cms_alloc_onchip_q(uint64_t base, int qid, int nsegs);

#endif
