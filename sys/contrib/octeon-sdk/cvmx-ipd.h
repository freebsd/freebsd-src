/***********************license start***************
 *  Copyright (c) 2003-2008 Cavium Networks (support@cavium.com). All rights
 *  reserved.
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Cavium Networks nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 *  TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *  AND WITH ALL FAULTS AND CAVIUM NETWORKS MAKES NO PROMISES, REPRESENTATIONS
 *  OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 *  RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
 *  REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
 *  DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
 *  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
 *  PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
 *  POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
 *  OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 *
 *
 *  For any questions regarding licensing please contact marketing@caviumnetworks.com
 *
 ***********************license end**************************************/






/**
 * @file
 *
 * Interface to the hardware Input Packet Data unit.
 *
 * <hr>$Revision: 41586 $<hr>
 */


#ifndef __CVMX_IPD_H__
#define __CVMX_IPD_H__

#ifndef CVMX_DONT_INCLUDE_CONFIG
#include "executive-config.h"
#ifdef CVMX_ENABLE_PKO_FUNCTIONS
#include "cvmx-config.h"
#endif
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef CVMX_ENABLE_LEN_M8_FIX
#define CVMX_ENABLE_LEN_M8_FIX 0
#endif

/* CSR typedefs have been moved to cvmx-csr-*.h */

typedef cvmx_ipd_mbuff_first_skip_t cvmx_ipd_mbuff_not_first_skip_t;
typedef cvmx_ipd_first_next_ptr_back_t cvmx_ipd_second_next_ptr_back_t;


/**
 * Configure IPD
 *
 * @param mbuff_size Packets buffer size in 8 byte words
 * @param first_mbuff_skip
 *                   Number of 8 byte words to skip in the first buffer
 * @param not_first_mbuff_skip
 *                   Number of 8 byte words to skip in each following buffer
 * @param first_back Must be same as first_mbuff_skip / 128
 * @param second_back
 *                   Must be same as not_first_mbuff_skip / 128
 * @param wqe_fpa_pool
 *                   FPA pool to get work entries from
 * @param cache_mode
 * @param back_pres_enable_flag
 *                   Enable or disable port back pressure
 */
static inline void cvmx_ipd_config(uint64_t mbuff_size,
                                   uint64_t first_mbuff_skip,
                                   uint64_t not_first_mbuff_skip,
                                   uint64_t first_back,
                                   uint64_t second_back,
                                   uint64_t wqe_fpa_pool,
                                   cvmx_ipd_mode_t cache_mode,
                                   uint64_t back_pres_enable_flag
                                  )
{
    cvmx_ipd_mbuff_first_skip_t first_skip;
    cvmx_ipd_mbuff_not_first_skip_t not_first_skip;
    cvmx_ipd_mbuff_size_t size;
    cvmx_ipd_first_next_ptr_back_t first_back_struct;
    cvmx_ipd_second_next_ptr_back_t second_back_struct;
    cvmx_ipd_wqe_fpa_pool_t wqe_pool;
    cvmx_ipd_ctl_status_t ipd_ctl_reg;

    first_skip.u64 = 0;
    first_skip.s.skip_sz = first_mbuff_skip;
    cvmx_write_csr(CVMX_IPD_1ST_MBUFF_SKIP, first_skip.u64);

    not_first_skip.u64 = 0;
    not_first_skip.s.skip_sz = not_first_mbuff_skip;
    cvmx_write_csr(CVMX_IPD_NOT_1ST_MBUFF_SKIP, not_first_skip.u64);

    size.u64 = 0;
    size.s.mb_size = mbuff_size;
    cvmx_write_csr(CVMX_IPD_PACKET_MBUFF_SIZE, size.u64);

    first_back_struct.u64 = 0;
    first_back_struct.s.back = first_back;
    cvmx_write_csr(CVMX_IPD_1st_NEXT_PTR_BACK, first_back_struct.u64);

    second_back_struct.u64 = 0;
    second_back_struct.s.back = second_back;
    cvmx_write_csr(CVMX_IPD_2nd_NEXT_PTR_BACK,second_back_struct.u64);

    wqe_pool.u64 = 0;
    wqe_pool.s.wqe_pool = wqe_fpa_pool;
    cvmx_write_csr(CVMX_IPD_WQE_FPA_QUEUE, wqe_pool.u64);

    ipd_ctl_reg.u64 = cvmx_read_csr(CVMX_IPD_CTL_STATUS);
    ipd_ctl_reg.s.opc_mode = cache_mode;
    ipd_ctl_reg.s.pbp_en = back_pres_enable_flag;
    cvmx_write_csr(CVMX_IPD_CTL_STATUS, ipd_ctl_reg.u64);

    /* Note: the example RED code that used to be here has been moved to
        cvmx_helper_setup_red */
}


/**
 * Enable IPD
 */
static inline void cvmx_ipd_enable(void)
{
    cvmx_ipd_ctl_status_t ipd_reg;
    ipd_reg.u64 = cvmx_read_csr(CVMX_IPD_CTL_STATUS);
    if (ipd_reg.s.ipd_en)
    {
        cvmx_dprintf("Warning: Enabling IPD when IPD already enabled.\n");
    }
    ipd_reg.s.ipd_en = TRUE;
    #if  CVMX_ENABLE_LEN_M8_FIX
    if(!OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2)) {
        ipd_reg.s.len_m8 = TRUE;
    }
    #endif
    cvmx_write_csr(CVMX_IPD_CTL_STATUS, ipd_reg.u64);
}


/**
 * Disable IPD
 */
static inline void cvmx_ipd_disable(void)
{
    cvmx_ipd_ctl_status_t ipd_reg;
    ipd_reg.u64 = cvmx_read_csr(CVMX_IPD_CTL_STATUS);
    ipd_reg.s.ipd_en = FALSE;
    cvmx_write_csr(CVMX_IPD_CTL_STATUS, ipd_reg.u64);
}

#ifdef CVMX_ENABLE_PKO_FUNCTIONS
/**
 * Supportive function for cvmx_fpa_shutdown_pool.
 */
static inline void cvmx_ipd_free_ptr(void)
{
    /* Only CN38XXp{1,2} cannot read pointer out of the IPD */
    if (!OCTEON_IS_MODEL(OCTEON_CN38XX_PASS1) && !OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2)) {
	int no_wptr = 0;
	cvmx_ipd_ptr_count_t ipd_ptr_count;
	ipd_ptr_count.u64 = cvmx_read_csr(CVMX_IPD_PTR_COUNT);

	/* Handle Work Queue Entry in cn56xx and cn52xx */
	if (octeon_has_feature(OCTEON_FEATURE_NO_WPTR)) {
	    cvmx_ipd_ctl_status_t ipd_ctl_status;
	    ipd_ctl_status.u64 = cvmx_read_csr(CVMX_IPD_CTL_STATUS);
	    if (ipd_ctl_status.s.no_wptr) 
		no_wptr = 1;
	}

	/* Free the prefetched WQE */
	if (ipd_ptr_count.s.wqev_cnt) {
	    cvmx_ipd_wqe_ptr_valid_t ipd_wqe_ptr_valid;
	    ipd_wqe_ptr_valid.u64 = cvmx_read_csr(CVMX_IPD_WQE_PTR_VALID);
	    if (no_wptr)
	        cvmx_fpa_free(cvmx_phys_to_ptr((uint64_t)ipd_wqe_ptr_valid.s.ptr<<7), CVMX_FPA_PACKET_POOL, 0);
	    else
	        cvmx_fpa_free(cvmx_phys_to_ptr((uint64_t)ipd_wqe_ptr_valid.s.ptr<<7), CVMX_FPA_WQE_POOL, 0);
	}

	/* Free all WQE in the fifo */
	if (ipd_ptr_count.s.wqe_pcnt) {
	    int i;
	    cvmx_ipd_pwp_ptr_fifo_ctl_t ipd_pwp_ptr_fifo_ctl;
	    ipd_pwp_ptr_fifo_ctl.u64 = cvmx_read_csr(CVMX_IPD_PWP_PTR_FIFO_CTL);
	    for (i = 0; i < ipd_ptr_count.s.wqe_pcnt; i++) {
		ipd_pwp_ptr_fifo_ctl.s.cena = 0;
		ipd_pwp_ptr_fifo_ctl.s.raddr = ipd_pwp_ptr_fifo_ctl.s.max_cnts + (ipd_pwp_ptr_fifo_ctl.s.wraddr+i) % ipd_pwp_ptr_fifo_ctl.s.max_cnts;
		cvmx_write_csr(CVMX_IPD_PWP_PTR_FIFO_CTL, ipd_pwp_ptr_fifo_ctl.u64);
		ipd_pwp_ptr_fifo_ctl.u64 = cvmx_read_csr(CVMX_IPD_PWP_PTR_FIFO_CTL);
		if (no_wptr)
		    cvmx_fpa_free(cvmx_phys_to_ptr((uint64_t)ipd_pwp_ptr_fifo_ctl.s.ptr<<7), CVMX_FPA_PACKET_POOL, 0);
		else
		    cvmx_fpa_free(cvmx_phys_to_ptr((uint64_t)ipd_pwp_ptr_fifo_ctl.s.ptr<<7), CVMX_FPA_WQE_POOL, 0);
	    }
	    ipd_pwp_ptr_fifo_ctl.s.cena = 1;
	    cvmx_write_csr(CVMX_IPD_PWP_PTR_FIFO_CTL, ipd_pwp_ptr_fifo_ctl.u64);
	}

	/* Free the prefetched packet */
	if (ipd_ptr_count.s.pktv_cnt) {
	    cvmx_ipd_pkt_ptr_valid_t ipd_pkt_ptr_valid;
	    ipd_pkt_ptr_valid.u64 = cvmx_read_csr(CVMX_IPD_PKT_PTR_VALID);
	    cvmx_fpa_free(cvmx_phys_to_ptr(ipd_pkt_ptr_valid.s.ptr<<7), CVMX_FPA_PACKET_POOL, 0);
	}

	/* Free the per port prefetched packets */
	if (1) {
	    int i;
	    cvmx_ipd_prc_port_ptr_fifo_ctl_t ipd_prc_port_ptr_fifo_ctl;
	    ipd_prc_port_ptr_fifo_ctl.u64 = cvmx_read_csr(CVMX_IPD_PRC_PORT_PTR_FIFO_CTL);

	    for (i = 0; i < ipd_prc_port_ptr_fifo_ctl.s.max_pkt; i++) {
		ipd_prc_port_ptr_fifo_ctl.s.cena = 0;
		ipd_prc_port_ptr_fifo_ctl.s.raddr = i % ipd_prc_port_ptr_fifo_ctl.s.max_pkt;
		cvmx_write_csr(CVMX_IPD_PRC_PORT_PTR_FIFO_CTL, ipd_prc_port_ptr_fifo_ctl.u64);
		ipd_prc_port_ptr_fifo_ctl.u64 = cvmx_read_csr(CVMX_IPD_PRC_PORT_PTR_FIFO_CTL);
		cvmx_fpa_free(cvmx_phys_to_ptr((uint64_t)ipd_prc_port_ptr_fifo_ctl.s.ptr<<7), CVMX_FPA_PACKET_POOL, 0);
	    }
	    ipd_prc_port_ptr_fifo_ctl.s.cena = 1;
	    cvmx_write_csr(CVMX_IPD_PRC_PORT_PTR_FIFO_CTL, ipd_prc_port_ptr_fifo_ctl.u64);
	}

	/* Free all packets in the holding fifo */
	if (ipd_ptr_count.s.pfif_cnt) {
	    int i;
	    cvmx_ipd_prc_hold_ptr_fifo_ctl_t ipd_prc_hold_ptr_fifo_ctl;

	    ipd_prc_hold_ptr_fifo_ctl.u64 = cvmx_read_csr(CVMX_IPD_PRC_HOLD_PTR_FIFO_CTL);

	    for (i = 0; i < ipd_ptr_count.s.pfif_cnt; i++) {
		ipd_prc_hold_ptr_fifo_ctl.s.cena = 0;
		ipd_prc_hold_ptr_fifo_ctl.s.raddr = (ipd_prc_hold_ptr_fifo_ctl.s.praddr + i) % ipd_prc_hold_ptr_fifo_ctl.s.max_pkt;
		cvmx_write_csr(CVMX_IPD_PRC_HOLD_PTR_FIFO_CTL, ipd_prc_hold_ptr_fifo_ctl.u64);
		ipd_prc_hold_ptr_fifo_ctl.u64 = cvmx_read_csr(CVMX_IPD_PRC_HOLD_PTR_FIFO_CTL);
		cvmx_fpa_free(cvmx_phys_to_ptr((uint64_t)ipd_prc_hold_ptr_fifo_ctl.s.ptr<<7), CVMX_FPA_PACKET_POOL, 0);
	    }
	    ipd_prc_hold_ptr_fifo_ctl.s.cena = 1;
	    cvmx_write_csr(CVMX_IPD_PRC_HOLD_PTR_FIFO_CTL, ipd_prc_hold_ptr_fifo_ctl.u64);
	}

	/* Free all packets in the fifo */
	if (ipd_ptr_count.s.pkt_pcnt) {
	    int i;
	    cvmx_ipd_pwp_ptr_fifo_ctl_t ipd_pwp_ptr_fifo_ctl;
	    ipd_pwp_ptr_fifo_ctl.u64 = cvmx_read_csr(CVMX_IPD_PWP_PTR_FIFO_CTL);

	    for (i = 0; i < ipd_ptr_count.s.pkt_pcnt; i++) {
		ipd_pwp_ptr_fifo_ctl.s.cena = 0;
		ipd_pwp_ptr_fifo_ctl.s.raddr = (ipd_pwp_ptr_fifo_ctl.s.praddr+i) % ipd_pwp_ptr_fifo_ctl.s.max_cnts;
		cvmx_write_csr(CVMX_IPD_PWP_PTR_FIFO_CTL, ipd_pwp_ptr_fifo_ctl.u64);
		ipd_pwp_ptr_fifo_ctl.u64 = cvmx_read_csr(CVMX_IPD_PWP_PTR_FIFO_CTL);
		cvmx_fpa_free(cvmx_phys_to_ptr((uint64_t)ipd_pwp_ptr_fifo_ctl.s.ptr<<7), CVMX_FPA_PACKET_POOL, 0);
	    }
	    ipd_pwp_ptr_fifo_ctl.s.cena = 1;
	    cvmx_write_csr(CVMX_IPD_PWP_PTR_FIFO_CTL, ipd_pwp_ptr_fifo_ctl.u64);
	}

	/* Reset the IPD to get all buffers out of it */
	{
	    cvmx_ipd_ctl_status_t ipd_ctl_status;
	    ipd_ctl_status.u64 = cvmx_read_csr(CVMX_IPD_CTL_STATUS);
	    ipd_ctl_status.s.reset = 1;
	    cvmx_write_csr(CVMX_IPD_CTL_STATUS, ipd_ctl_status.u64);
	}

	/* Reset the PIP */
	{
	    cvmx_pip_sft_rst_t pip_sft_rst;
	    pip_sft_rst.u64 = cvmx_read_csr(CVMX_PIP_SFT_RST);
	    pip_sft_rst.s.rst = 1;
	    cvmx_write_csr(CVMX_PIP_SFT_RST, pip_sft_rst.u64);
	}
    }
}
#endif

#ifdef	__cplusplus
}
#endif

#endif  /*  __CVMX_IPD_H__ */
