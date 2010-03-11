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

/*------------------------------------------------------------------
 * octeon_ipd.c      Input Packet Unit
 *
 *------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <mips/cavium/octeon_pcmap_regs.h>
#include "octeon_ipd.h"

/*
 * octeon_ipd_enable
 *
 * enable ipd
 */
void octeon_ipd_enable (void)
{
    octeon_ipd_ctl_status_t octeon_ipd_reg;

    octeon_ipd_reg.word64 = oct_read64(OCTEON_IPD_CTL_STATUS);
    octeon_ipd_reg.bits.ipd_en = 1;
    oct_write64(OCTEON_IPD_CTL_STATUS, octeon_ipd_reg.word64);
}


/*
 * octeon_ipd_disable
 *
 * disable ipd
 */
void octeon_ipd_disable (void)
{
    octeon_ipd_ctl_status_t octeon_ipd_reg;

    octeon_ipd_reg.word64 = oct_read64(OCTEON_IPD_CTL_STATUS);
    octeon_ipd_reg.bits.ipd_en = 0;
    oct_write64(OCTEON_IPD_CTL_STATUS, octeon_ipd_reg.word64);
}


/*
 * octeon_ipd_config
 *
 * Configure IPD
 *
 * mbuff_size Packets buffer size in 8 byte words
 * first_mbuff_skip
 *                   Number of 8 byte words to skip in the first buffer
 * not_first_mbuff_skip
 *                   Number of 8 byte words to skip in each following buffer
 * first_back Must be same as first_mbuff_skip / Cache_Line_size
 * second_back
 *                   Must be same as not_first_mbuff_skip / Cache_Line_Size
 * wqe_fpa_pool
 *                   FPA pool to get work entries from
 * cache_mode
 * back_pres_enable_flag
 *                   Enable or disable port back pressure
 */
void octeon_ipd_config (u_int mbuff_size,
                        u_int first_mbuff_skip,
                        u_int not_first_mbuff_skip,
                        u_int first_back,
                        u_int second_back,
                        u_int wqe_fpa_pool,
                        octeon_ipd_mode_t cache_mode,
                        u_int back_pres_enable_flag)
{
    octeon_ipd_mbuff_first_skip_t first_skip;
    octeon_ipd_mbuff_not_first_skip_t not_first_skip;
    octeon_ipd_mbuff_size_t size;
    octeon_ipd_first_next_ptr_back_t first_back_struct;
    octeon_ipd_second_next_ptr_back_t second_back_struct;
    octeon_ipd_wqe_fpa_pool_t wqe_pool;
    octeon_ipd_ctl_status_t octeon_ipd_ctl_reg;

    first_skip.word64 = 0;
    first_skip.bits.skip_sz = first_mbuff_skip;
    oct_write64(OCTEON_IPD_1ST_MBUFF_SKIP, first_skip.word64);

    not_first_skip.word64 = 0;
    not_first_skip.bits.skip_sz = not_first_mbuff_skip;
    oct_write64(OCTEON_IPD_NOT_1ST_MBUFF_SKIP, not_first_skip.word64);

    size.word64 = 0;
    size.bits.mb_size = mbuff_size;
    oct_write64(OCTEON_IPD_PACKET_MBUFF_SIZE, size.word64);

    first_back_struct.word64 = 0;
    first_back_struct.bits.back = first_back;
    oct_write64(OCTEON_IPD_1ST_NEXT_PTR_BACK, first_back_struct.word64);

    second_back_struct.word64 = 0;
    second_back_struct.bits.back = second_back;
    oct_write64(OCTEON_IPD_2ND_NEXT_PTR_BACK, second_back_struct.word64);

    wqe_pool.word64 = 0;
    wqe_pool.bits.wqe_pool = wqe_fpa_pool;
    oct_write64(OCTEON_IPD_WQE_FPA_QUEUE, wqe_pool.word64);

    octeon_ipd_ctl_reg.word64 = 0;
    octeon_ipd_ctl_reg.bits.opc_mode = cache_mode;
    octeon_ipd_ctl_reg.bits.pbp_en = back_pres_enable_flag;
    oct_write64(OCTEON_IPD_CTL_STATUS, octeon_ipd_ctl_reg.word64);
}
