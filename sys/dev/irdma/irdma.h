/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2017 - 2022 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef IRDMA_H
#define IRDMA_H

#define RDMA_BIT2(type, a) ((u##type) 1UL << a)
#define RDMA_MASK3(type, mask, shift)	((u##type) mask << shift)
#define MAKEMASK(m, s) ((m) << (s))

#define IRDMA_WQEALLOC_WQE_DESC_INDEX_S 20
#define IRDMA_WQEALLOC_WQE_DESC_INDEX GENMASK(31, 20)

#define IRDMA_CQPTAIL_WQTAIL_S 0
#define IRDMA_CQPTAIL_WQTAIL GENMASK(10, 0)
#define IRDMA_CQPTAIL_CQP_OP_ERR_S 31
#define IRDMA_CQPTAIL_CQP_OP_ERR BIT(31)

#define IRDMA_CQPERRCODES_CQP_MINOR_CODE_S 0
#define IRDMA_CQPERRCODES_CQP_MINOR_CODE GENMASK(15, 0)
#define IRDMA_CQPERRCODES_CQP_MAJOR_CODE_S 16
#define IRDMA_CQPERRCODES_CQP_MAJOR_CODE GENMASK(31, 16)
#define IRDMA_GLPCI_LBARCTRL_PE_DB_SIZE_S 4
#define IRDMA_GLPCI_LBARCTRL_PE_DB_SIZE GENMASK(5, 4)
#define IRDMA_GLINT_RATE_INTERVAL_S 0
#define IRDMA_GLINT_RATE_INTERVAL GENMASK(4, 0)
#define IRDMA_GLINT_RATE_INTRL_ENA_S 6
#define IRDMA_GLINT_RATE_INTRL_ENA_M BIT(6)
#define IRDMA_GLINT_RATE_INTRL_ENA BIT(6)

#define IRDMA_GLINT_DYN_CTL_INTENA_S 0
#define IRDMA_GLINT_DYN_CTL_INTENA BIT(0)
#define IRDMA_GLINT_DYN_CTL_CLEARPBA_S 1
#define IRDMA_GLINT_DYN_CTL_CLEARPBA BIT(1)
#define IRDMA_GLINT_DYN_CTL_ITR_INDX_S 3
#define IRDMA_GLINT_DYN_CTL_ITR_INDX GENMASK(4, 3)
#define IRDMA_GLINT_DYN_CTL_INTERVAL_S 5
#define IRDMA_GLINT_DYN_CTL_INTERVAL GENMASK(16, 5)
#define IRDMA_GLINT_CEQCTL_ITR_INDX_S 11
#define IRDMA_GLINT_CEQCTL_ITR_INDX GENMASK(12, 11)
#define IRDMA_GLINT_CEQCTL_CAUSE_ENA_S 30
#define IRDMA_GLINT_CEQCTL_CAUSE_ENA BIT(30)
#define IRDMA_GLINT_CEQCTL_MSIX_INDX_S 0
#define IRDMA_GLINT_CEQCTL_MSIX_INDX GENMASK(10, 0)
#define IRDMA_PFINT_AEQCTL_MSIX_INDX_S 0
#define IRDMA_PFINT_AEQCTL_MSIX_INDX GENMASK(10, 0)
#define IRDMA_PFINT_AEQCTL_ITR_INDX_S 11
#define IRDMA_PFINT_AEQCTL_ITR_INDX GENMASK(12, 11)
#define IRDMA_PFINT_AEQCTL_CAUSE_ENA_S 30
#define IRDMA_PFINT_AEQCTL_CAUSE_ENA BIT(30)
#define IRDMA_PFHMC_PDINV_PMSDIDX_S 0
#define IRDMA_PFHMC_PDINV_PMSDIDX GENMASK(11, 0)
#define IRDMA_PFHMC_PDINV_PMSDPARTSEL_S 15
#define IRDMA_PFHMC_PDINV_PMSDPARTSEL BIT(15)
#define IRDMA_PFHMC_PDINV_PMPDIDX_S 16
#define IRDMA_PFHMC_PDINV_PMPDIDX GENMASK(24, 16)
#define IRDMA_PFHMC_SDDATALOW_PMSDVALID_S 0
#define IRDMA_PFHMC_SDDATALOW_PMSDVALID BIT(0)
#define IRDMA_PFHMC_SDDATALOW_PMSDTYPE_S 1
#define IRDMA_PFHMC_SDDATALOW_PMSDTYPE BIT(1)
#define IRDMA_PFHMC_SDDATALOW_PMSDBPCOUNT_S 2
#define IRDMA_PFHMC_SDDATALOW_PMSDBPCOUNT GENMASK(11, 2)
#define IRDMA_PFHMC_SDDATALOW_PMSDDATALOW_S 12
#define IRDMA_PFHMC_SDDATALOW_PMSDDATALOW GENMASK(31, 12)
#define IRDMA_PFHMC_SDCMD_PMSDWR_S 31
#define IRDMA_PFHMC_SDCMD_PMSDWR BIT(31)
#define IRDMA_PFHMC_SDCMD_PMSDPARTSEL_S 15
#define IRDMA_PFHMC_SDCMD_PMSDPARTSEL BIT(15)

#define IRDMA_INVALID_CQ_IDX 0xffffffff

enum irdma_dyn_idx_t {
	IRDMA_IDX_ITR0 = 0,
	IRDMA_IDX_ITR1 = 1,
	IRDMA_IDX_ITR2 = 2,
	IRDMA_IDX_NOITR = 3,
};

enum irdma_registers {
	IRDMA_CQPTAIL,
	IRDMA_CQPDB,
	IRDMA_CCQPSTATUS,
	IRDMA_CCQPHIGH,
	IRDMA_CCQPLOW,
	IRDMA_CQARM,
	IRDMA_CQACK,
	IRDMA_AEQALLOC,
	IRDMA_CQPERRCODES,
	IRDMA_WQEALLOC,
	IRDMA_GLINT_DYN_CTL,
	IRDMA_DB_ADDR_OFFSET,
	IRDMA_GLPCI_LBARCTRL,
	IRDMA_GLPE_CPUSTATUS0,
	IRDMA_GLPE_CPUSTATUS1,
	IRDMA_GLPE_CPUSTATUS2,
	IRDMA_PFINT_AEQCTL,
	IRDMA_GLINT_CEQCTL,
	IRDMA_VSIQF_PE_CTL1,
	IRDMA_PFHMC_PDINV,
	IRDMA_GLHMC_VFPDINV,
	IRDMA_GLPE_CRITERR,
	IRDMA_GLINT_RATE,
	IRDMA_MAX_REGS, /* Must be last entry */
};

enum irdma_shifts {
	IRDMA_CCQPSTATUS_CCQP_DONE_S,
	IRDMA_CCQPSTATUS_CCQP_ERR_S,
	IRDMA_CQPSQ_STAG_PDID_S,
	IRDMA_CQPSQ_CQ_CEQID_S,
	IRDMA_CQPSQ_CQ_CQID_S,
	IRDMA_COMMIT_FPM_CQCNT_S,
	IRDMA_CQPSQ_UPESD_HMCFNID_S,
	IRDMA_MAX_SHIFTS,
};

enum irdma_masks {
	IRDMA_CCQPSTATUS_CCQP_DONE_M,
	IRDMA_CCQPSTATUS_CCQP_ERR_M,
	IRDMA_CQPSQ_STAG_PDID_M,
	IRDMA_CQPSQ_CQ_CEQID_M,
	IRDMA_CQPSQ_CQ_CQID_M,
	IRDMA_COMMIT_FPM_CQCNT_M,
	IRDMA_CQPSQ_UPESD_HMCFNID_M,
	IRDMA_MAX_MASKS, /* Must be last entry */
};

#define IRDMA_MAX_MGS_PER_CTX	8

struct irdma_mcast_grp_ctx_entry_info {
	u32 qp_id;
	bool valid_entry;
	u16 dest_port;
	u32 use_cnt;
};

struct irdma_mcast_grp_info {
	u8 dest_mac_addr[ETHER_ADDR_LEN];
	u16 vlan_id;
	u16 hmc_fcn_id;
	bool ipv4_valid:1;
	bool vlan_valid:1;
	u16 mg_id;
	u32 no_of_mgs;
	u32 dest_ip_addr[4];
	u16 qs_handle;
	struct irdma_dma_mem dma_mem_mc;
	struct irdma_mcast_grp_ctx_entry_info mg_ctx_info[IRDMA_MAX_MGS_PER_CTX];
};

enum irdma_vers {
	IRDMA_GEN_RSVD = 0,
	IRDMA_GEN_1 = 1,
	IRDMA_GEN_2 = 2,
	IRDMA_GEN_MAX = IRDMA_GEN_2,
};

struct irdma_uk_attrs {
	u64 feature_flags;
	u32 max_hw_wq_frags;
	u32 max_hw_read_sges;
	u32 max_hw_inline;
	u32 max_hw_rq_quanta;
	u32 max_hw_wq_quanta;
	u32 min_hw_cq_size;
	u32 max_hw_cq_size;
	u16 max_hw_sq_chunk;
	u16 min_hw_wq_size;
	u8 hw_rev;
};

struct irdma_hw_attrs {
	struct irdma_uk_attrs uk_attrs;
	u64 max_hw_outbound_msg_size;
	u64 max_hw_inbound_msg_size;
	u64 max_mr_size;
	u64 page_size_cap;
	u32 min_hw_qp_id;
	u32 min_hw_aeq_size;
	u32 max_hw_aeq_size;
	u32 min_hw_ceq_size;
	u32 max_hw_ceq_size;
	u32 max_hw_device_pages;
	u32 max_hw_vf_fpm_id;
	u32 first_hw_vf_fpm_id;
	u32 max_hw_ird;
	u32 max_hw_ord;
	u32 max_hw_wqes;
	u32 max_hw_pds;
	u32 max_hw_ena_vf_count;
	u32 max_qp_wr;
	u32 max_pe_ready_count;
	u32 max_done_count;
	u32 max_sleep_count;
	u32 max_cqp_compl_wait_time_ms;
	u16 max_stat_inst;
	u16 max_stat_idx;
};

void icrdma_init_hw(struct irdma_sc_dev *dev);
void irdma_check_fc_for_qp(struct irdma_sc_vsi *vsi, struct irdma_sc_qp *sc_qp);
#endif /* IRDMA_H*/
