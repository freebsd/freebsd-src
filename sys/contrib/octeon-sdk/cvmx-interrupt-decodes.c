/***********************license start***************
 *  Copyright (c) 2003-2009 Cavium Networks (support@cavium.com). All rights
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
 * Automatically generated functions useful for enabling
 * and decoding RSL_INT_BLOCKS interrupts.
 *
 * This file is auto generated. Do not edit.
 *
 * <hr>$Revision$<hr>
 *
 */

#include "cvmx.h"
#include "cvmx-interrupt.h"
#include "cvmx-pcie.h"

#ifndef PRINT_ERROR
#define PRINT_ERROR(format, ...) cvmx_safe_printf("ERROR " format, ##__VA_ARGS__)
#endif

void __cvmx_interrupt_pci_int_enb2_enable(void);
void __cvmx_interrupt_pci_int_sum2_decode(void);
void __cvmx_interrupt_pescx_dbg_info_en_enable(int index);
void __cvmx_interrupt_pescx_dbg_info_decode(int index);

/**
 * __cvmx_interrupt_agl_gmx_rxx_int_en_enable enables all interrupt bits in cvmx_agl_gmx_rxx_int_en_t
 */
void __cvmx_interrupt_agl_gmx_rxx_int_en_enable(int index)
{
    cvmx_agl_gmx_rxx_int_en_t agl_gmx_rx_int_en;
    cvmx_write_csr(CVMX_AGL_GMX_RXX_INT_REG(index), cvmx_read_csr(CVMX_AGL_GMX_RXX_INT_REG(index)));
    agl_gmx_rx_int_en.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping agl_gmx_rx_int_en.s.reserved_20_63
        agl_gmx_rx_int_en.s.pause_drp = 1;
        // Skipping agl_gmx_rx_int_en.s.reserved_16_18
        agl_gmx_rx_int_en.s.ifgerr = 1;
        //agl_gmx_rx_int_en.s.coldet = 1; // Collsion detect
        //agl_gmx_rx_int_en.s.falerr = 1; // False carrier error or extend error after slottime
        //agl_gmx_rx_int_en.s.rsverr = 1; // RGMII reserved opcodes
        //agl_gmx_rx_int_en.s.pcterr = 1; // Bad Preamble / Protocol
        agl_gmx_rx_int_en.s.ovrerr = 1;
        // Skipping agl_gmx_rx_int_en.s.reserved_9_9
        agl_gmx_rx_int_en.s.skperr = 1;
        agl_gmx_rx_int_en.s.rcverr = 1;
        agl_gmx_rx_int_en.s.lenerr = 1;
        agl_gmx_rx_int_en.s.alnerr = 1;
        agl_gmx_rx_int_en.s.fcserr = 1;
        agl_gmx_rx_int_en.s.jabber = 1;
        agl_gmx_rx_int_en.s.maxerr = 1;
        // Skipping agl_gmx_rx_int_en.s.reserved_1_1
        agl_gmx_rx_int_en.s.minerr = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping agl_gmx_rx_int_en.s.reserved_20_63
        agl_gmx_rx_int_en.s.pause_drp = 1;
        // Skipping agl_gmx_rx_int_en.s.reserved_16_18
        agl_gmx_rx_int_en.s.ifgerr = 1;
        //agl_gmx_rx_int_en.s.coldet = 1; // Collsion detect
        //agl_gmx_rx_int_en.s.falerr = 1; // False carrier error or extend error after slottime
        //agl_gmx_rx_int_en.s.rsverr = 1; // RGMII reserved opcodes
        //agl_gmx_rx_int_en.s.pcterr = 1; // Bad Preamble / Protocol
        agl_gmx_rx_int_en.s.ovrerr = 1;
        // Skipping agl_gmx_rx_int_en.s.reserved_9_9
        agl_gmx_rx_int_en.s.skperr = 1;
        agl_gmx_rx_int_en.s.rcverr = 1;
        agl_gmx_rx_int_en.s.lenerr = 1;
        agl_gmx_rx_int_en.s.alnerr = 1;
        agl_gmx_rx_int_en.s.fcserr = 1;
        agl_gmx_rx_int_en.s.jabber = 1;
        agl_gmx_rx_int_en.s.maxerr = 1;
        // Skipping agl_gmx_rx_int_en.s.reserved_1_1
        agl_gmx_rx_int_en.s.minerr = 1;
    }
    cvmx_write_csr(CVMX_AGL_GMX_RXX_INT_EN(index), agl_gmx_rx_int_en.u64);
}


/**
 * __cvmx_interrupt_agl_gmx_rxx_int_reg_decode decodes all interrupt bits in cvmx_agl_gmx_rxx_int_reg_t
 */
void __cvmx_interrupt_agl_gmx_rxx_int_reg_decode(int index)
{
    cvmx_agl_gmx_rxx_int_reg_t agl_gmx_rx_int_reg;
    agl_gmx_rx_int_reg.u64 = cvmx_read_csr(CVMX_AGL_GMX_RXX_INT_REG(index));
    agl_gmx_rx_int_reg.u64 &= cvmx_read_csr(CVMX_AGL_GMX_RXX_INT_EN(index));
    cvmx_write_csr(CVMX_AGL_GMX_RXX_INT_REG(index), agl_gmx_rx_int_reg.u64);
    // Skipping agl_gmx_rx_int_reg.s.reserved_20_63
    if (agl_gmx_rx_int_reg.s.pause_drp)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[PAUSE_DRP]: Pause packet was dropped due to full GMX RX FIFO\n", index);
    // Skipping agl_gmx_rx_int_reg.s.reserved_16_18
    if (agl_gmx_rx_int_reg.s.ifgerr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[IFGERR]: Interframe Gap Violation\n"
                    "    Does not necessarily indicate a failure\n", index);
    if (agl_gmx_rx_int_reg.s.coldet)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[COLDET]: Collision Detection\n", index);
    if (agl_gmx_rx_int_reg.s.falerr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[FALERR]: False carrier error or extend error after slottime\n", index);
    if (agl_gmx_rx_int_reg.s.rsverr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[RSVERR]: MII reserved opcodes\n", index);
    if (agl_gmx_rx_int_reg.s.pcterr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[PCTERR]: Bad Preamble / Protocol\n", index);
    if (agl_gmx_rx_int_reg.s.ovrerr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[OVRERR]: Internal Data Aggregation Overflow\n"
                    "    This interrupt should never assert\n", index);
    // Skipping agl_gmx_rx_int_reg.s.reserved_9_9
    if (agl_gmx_rx_int_reg.s.skperr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[SKPERR]: Skipper error\n", index);
    if (agl_gmx_rx_int_reg.s.rcverr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[RCVERR]: Frame was received with MII Data reception error\n", index);
    if (agl_gmx_rx_int_reg.s.lenerr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[LENERR]: Frame was received with length error\n", index);
    if (agl_gmx_rx_int_reg.s.alnerr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[ALNERR]: Frame was received with an alignment error\n", index);
    if (agl_gmx_rx_int_reg.s.fcserr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[FCSERR]: Frame was received with FCS/CRC error\n", index);
    if (agl_gmx_rx_int_reg.s.jabber)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[JABBER]: Frame was received with length > sys_length\n", index);
    if (agl_gmx_rx_int_reg.s.maxerr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[MAXERR]: Frame was received with length > max_length\n", index);
    // Skipping agl_gmx_rx_int_reg.s.reserved_1_1
    if (agl_gmx_rx_int_reg.s.minerr)
        PRINT_ERROR("AGL_GMX_RX%d_INT_REG[MINERR]: Frame was received with length < min_length\n", index);
}


/**
 * __cvmx_interrupt_fpa_int_enb_enable enables all interrupt bits in cvmx_fpa_int_enb_t
 */
void __cvmx_interrupt_fpa_int_enb_enable(void)
{
    cvmx_fpa_int_enb_t fpa_int_enb;
    cvmx_write_csr(CVMX_FPA_INT_SUM, cvmx_read_csr(CVMX_FPA_INT_SUM));
    fpa_int_enb.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping fpa_int_enb.s.reserved_28_63
        fpa_int_enb.s.q7_perr = 1;
        fpa_int_enb.s.q7_coff = 1;
        fpa_int_enb.s.q7_und = 1;
        fpa_int_enb.s.q6_perr = 1;
        fpa_int_enb.s.q6_coff = 1;
        fpa_int_enb.s.q6_und = 1;
        fpa_int_enb.s.q5_perr = 1;
        fpa_int_enb.s.q5_coff = 1;
        fpa_int_enb.s.q5_und = 1;
        fpa_int_enb.s.q4_perr = 1;
        fpa_int_enb.s.q4_coff = 1;
        fpa_int_enb.s.q4_und = 1;
        fpa_int_enb.s.q3_perr = 1;
        fpa_int_enb.s.q3_coff = 1;
        fpa_int_enb.s.q3_und = 1;
        fpa_int_enb.s.q2_perr = 1;
        fpa_int_enb.s.q2_coff = 1;
        fpa_int_enb.s.q2_und = 1;
        fpa_int_enb.s.q1_perr = 1;
        fpa_int_enb.s.q1_coff = 1;
        fpa_int_enb.s.q1_und = 1;
        fpa_int_enb.s.q0_perr = 1;
        fpa_int_enb.s.q0_coff = 1;
        fpa_int_enb.s.q0_und = 1;
        fpa_int_enb.s.fed1_dbe = 1;
        fpa_int_enb.s.fed1_sbe = 1;
        fpa_int_enb.s.fed0_dbe = 1;
        fpa_int_enb.s.fed0_sbe = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        // Skipping fpa_int_enb.s.reserved_28_63
        fpa_int_enb.s.q7_perr = 1;
        fpa_int_enb.s.q7_coff = 1;
        fpa_int_enb.s.q7_und = 1;
        fpa_int_enb.s.q6_perr = 1;
        fpa_int_enb.s.q6_coff = 1;
        fpa_int_enb.s.q6_und = 1;
        fpa_int_enb.s.q5_perr = 1;
        fpa_int_enb.s.q5_coff = 1;
        fpa_int_enb.s.q5_und = 1;
        fpa_int_enb.s.q4_perr = 1;
        fpa_int_enb.s.q4_coff = 1;
        fpa_int_enb.s.q4_und = 1;
        fpa_int_enb.s.q3_perr = 1;
        fpa_int_enb.s.q3_coff = 1;
        fpa_int_enb.s.q3_und = 1;
        fpa_int_enb.s.q2_perr = 1;
        fpa_int_enb.s.q2_coff = 1;
        fpa_int_enb.s.q2_und = 1;
        fpa_int_enb.s.q1_perr = 1;
        fpa_int_enb.s.q1_coff = 1;
        fpa_int_enb.s.q1_und = 1;
        fpa_int_enb.s.q0_perr = 1;
        fpa_int_enb.s.q0_coff = 1;
        fpa_int_enb.s.q0_und = 1;
        fpa_int_enb.s.fed1_dbe = 1;
        fpa_int_enb.s.fed1_sbe = 1;
        fpa_int_enb.s.fed0_dbe = 1;
        fpa_int_enb.s.fed0_sbe = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        // Skipping fpa_int_enb.s.reserved_28_63
        fpa_int_enb.s.q7_perr = 1;
        fpa_int_enb.s.q7_coff = 1;
        fpa_int_enb.s.q7_und = 1;
        fpa_int_enb.s.q6_perr = 1;
        fpa_int_enb.s.q6_coff = 1;
        fpa_int_enb.s.q6_und = 1;
        fpa_int_enb.s.q5_perr = 1;
        fpa_int_enb.s.q5_coff = 1;
        fpa_int_enb.s.q5_und = 1;
        fpa_int_enb.s.q4_perr = 1;
        fpa_int_enb.s.q4_coff = 1;
        fpa_int_enb.s.q4_und = 1;
        fpa_int_enb.s.q3_perr = 1;
        fpa_int_enb.s.q3_coff = 1;
        fpa_int_enb.s.q3_und = 1;
        fpa_int_enb.s.q2_perr = 1;
        fpa_int_enb.s.q2_coff = 1;
        fpa_int_enb.s.q2_und = 1;
        fpa_int_enb.s.q1_perr = 1;
        fpa_int_enb.s.q1_coff = 1;
        fpa_int_enb.s.q1_und = 1;
        fpa_int_enb.s.q0_perr = 1;
        fpa_int_enb.s.q0_coff = 1;
        fpa_int_enb.s.q0_und = 1;
        fpa_int_enb.s.fed1_dbe = 1;
        fpa_int_enb.s.fed1_sbe = 1;
        fpa_int_enb.s.fed0_dbe = 1;
        fpa_int_enb.s.fed0_sbe = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping fpa_int_enb.s.reserved_28_63
        fpa_int_enb.s.q7_perr = 1;
        fpa_int_enb.s.q7_coff = 1;
        fpa_int_enb.s.q7_und = 1;
        fpa_int_enb.s.q6_perr = 1;
        fpa_int_enb.s.q6_coff = 1;
        fpa_int_enb.s.q6_und = 1;
        fpa_int_enb.s.q5_perr = 1;
        fpa_int_enb.s.q5_coff = 1;
        fpa_int_enb.s.q5_und = 1;
        fpa_int_enb.s.q4_perr = 1;
        fpa_int_enb.s.q4_coff = 1;
        fpa_int_enb.s.q4_und = 1;
        fpa_int_enb.s.q3_perr = 1;
        fpa_int_enb.s.q3_coff = 1;
        fpa_int_enb.s.q3_und = 1;
        fpa_int_enb.s.q2_perr = 1;
        fpa_int_enb.s.q2_coff = 1;
        fpa_int_enb.s.q2_und = 1;
        fpa_int_enb.s.q1_perr = 1;
        fpa_int_enb.s.q1_coff = 1;
        fpa_int_enb.s.q1_und = 1;
        fpa_int_enb.s.q0_perr = 1;
        fpa_int_enb.s.q0_coff = 1;
        fpa_int_enb.s.q0_und = 1;
        fpa_int_enb.s.fed1_dbe = 1;
        fpa_int_enb.s.fed1_sbe = 1;
        fpa_int_enb.s.fed0_dbe = 1;
        fpa_int_enb.s.fed0_sbe = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        // Skipping fpa_int_enb.s.reserved_28_63
        fpa_int_enb.s.q7_perr = 1;
        fpa_int_enb.s.q7_coff = 1;
        fpa_int_enb.s.q7_und = 1;
        fpa_int_enb.s.q6_perr = 1;
        fpa_int_enb.s.q6_coff = 1;
        fpa_int_enb.s.q6_und = 1;
        fpa_int_enb.s.q5_perr = 1;
        fpa_int_enb.s.q5_coff = 1;
        fpa_int_enb.s.q5_und = 1;
        fpa_int_enb.s.q4_perr = 1;
        fpa_int_enb.s.q4_coff = 1;
        fpa_int_enb.s.q4_und = 1;
        fpa_int_enb.s.q3_perr = 1;
        fpa_int_enb.s.q3_coff = 1;
        fpa_int_enb.s.q3_und = 1;
        fpa_int_enb.s.q2_perr = 1;
        fpa_int_enb.s.q2_coff = 1;
        fpa_int_enb.s.q2_und = 1;
        fpa_int_enb.s.q1_perr = 1;
        fpa_int_enb.s.q1_coff = 1;
        fpa_int_enb.s.q1_und = 1;
        fpa_int_enb.s.q0_perr = 1;
        fpa_int_enb.s.q0_coff = 1;
        fpa_int_enb.s.q0_und = 1;
        fpa_int_enb.s.fed1_dbe = 1;
        fpa_int_enb.s.fed1_sbe = 1;
        fpa_int_enb.s.fed0_dbe = 1;
        fpa_int_enb.s.fed0_sbe = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping fpa_int_enb.s.reserved_28_63
        fpa_int_enb.s.q7_perr = 1;
        fpa_int_enb.s.q7_coff = 1;
        fpa_int_enb.s.q7_und = 1;
        fpa_int_enb.s.q6_perr = 1;
        fpa_int_enb.s.q6_coff = 1;
        fpa_int_enb.s.q6_und = 1;
        fpa_int_enb.s.q5_perr = 1;
        fpa_int_enb.s.q5_coff = 1;
        fpa_int_enb.s.q5_und = 1;
        fpa_int_enb.s.q4_perr = 1;
        fpa_int_enb.s.q4_coff = 1;
        fpa_int_enb.s.q4_und = 1;
        fpa_int_enb.s.q3_perr = 1;
        fpa_int_enb.s.q3_coff = 1;
        fpa_int_enb.s.q3_und = 1;
        fpa_int_enb.s.q2_perr = 1;
        fpa_int_enb.s.q2_coff = 1;
        fpa_int_enb.s.q2_und = 1;
        fpa_int_enb.s.q1_perr = 1;
        fpa_int_enb.s.q1_coff = 1;
        fpa_int_enb.s.q1_und = 1;
        fpa_int_enb.s.q0_perr = 1;
        fpa_int_enb.s.q0_coff = 1;
        fpa_int_enb.s.q0_und = 1;
        fpa_int_enb.s.fed1_dbe = 1;
        fpa_int_enb.s.fed1_sbe = 1;
        fpa_int_enb.s.fed0_dbe = 1;
        fpa_int_enb.s.fed0_sbe = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping fpa_int_enb.s.reserved_28_63
        fpa_int_enb.s.q7_perr = 1;
        fpa_int_enb.s.q7_coff = 1;
        fpa_int_enb.s.q7_und = 1;
        fpa_int_enb.s.q6_perr = 1;
        fpa_int_enb.s.q6_coff = 1;
        fpa_int_enb.s.q6_und = 1;
        fpa_int_enb.s.q5_perr = 1;
        fpa_int_enb.s.q5_coff = 1;
        fpa_int_enb.s.q5_und = 1;
        fpa_int_enb.s.q4_perr = 1;
        fpa_int_enb.s.q4_coff = 1;
        fpa_int_enb.s.q4_und = 1;
        fpa_int_enb.s.q3_perr = 1;
        fpa_int_enb.s.q3_coff = 1;
        fpa_int_enb.s.q3_und = 1;
        fpa_int_enb.s.q2_perr = 1;
        fpa_int_enb.s.q2_coff = 1;
        fpa_int_enb.s.q2_und = 1;
        fpa_int_enb.s.q1_perr = 1;
        fpa_int_enb.s.q1_coff = 1;
        fpa_int_enb.s.q1_und = 1;
        fpa_int_enb.s.q0_perr = 1;
        fpa_int_enb.s.q0_coff = 1;
        fpa_int_enb.s.q0_und = 1;
        fpa_int_enb.s.fed1_dbe = 1;
        fpa_int_enb.s.fed1_sbe = 1;
        fpa_int_enb.s.fed0_dbe = 1;
        fpa_int_enb.s.fed0_sbe = 1;
    }
    cvmx_write_csr(CVMX_FPA_INT_ENB, fpa_int_enb.u64);
}


/**
 * __cvmx_interrupt_fpa_int_sum_decode decodes all interrupt bits in cvmx_fpa_int_sum_t
 */
void __cvmx_interrupt_fpa_int_sum_decode(void)
{
    cvmx_fpa_int_sum_t fpa_int_sum;
    fpa_int_sum.u64 = cvmx_read_csr(CVMX_FPA_INT_SUM);
    fpa_int_sum.u64 &= cvmx_read_csr(CVMX_FPA_INT_ENB);
    cvmx_write_csr(CVMX_FPA_INT_SUM, fpa_int_sum.u64);
    // Skipping fpa_int_sum.s.reserved_28_63
    if (fpa_int_sum.s.q7_perr)
        PRINT_ERROR("FPA_INT_SUM[Q7_PERR]: Set when a Queue0 pointer read from the stack in\n"
                    "    the L2C does not have the FPA owner ship bit set.\n");
    if (fpa_int_sum.s.q7_coff)
        PRINT_ERROR("FPA_INT_SUM[Q7_COFF]: Set when a Queue0 stack end tag is present and\n"
                    "    the count available is greater than than pointers\n"
                    "    present in the FPA.\n");
    if (fpa_int_sum.s.q7_und)
        PRINT_ERROR("FPA_INT_SUM[Q7_UND]: Set when a Queue0 page count available goes\n"
                    "    negative.\n");
    if (fpa_int_sum.s.q6_perr)
        PRINT_ERROR("FPA_INT_SUM[Q6_PERR]: Set when a Queue0 pointer read from the stack in\n"
                    "    the L2C does not have the FPA owner ship bit set.\n");
    if (fpa_int_sum.s.q6_coff)
        PRINT_ERROR("FPA_INT_SUM[Q6_COFF]: Set when a Queue0 stack end tag is present and\n"
                    "    the count available is greater than than pointers\n"
                    "    present in the FPA.\n");
    if (fpa_int_sum.s.q6_und)
        PRINT_ERROR("FPA_INT_SUM[Q6_UND]: Set when a Queue0 page count available goes\n"
                    "    negative.\n");
    if (fpa_int_sum.s.q5_perr)
        PRINT_ERROR("FPA_INT_SUM[Q5_PERR]: Set when a Queue0 pointer read from the stack in\n"
                    "    the L2C does not have the FPA owner ship bit set.\n");
    if (fpa_int_sum.s.q5_coff)
        PRINT_ERROR("FPA_INT_SUM[Q5_COFF]: Set when a Queue0 stack end tag is present and\n"
                    "    the count available is greater than than pointers\n"
                    "    present in the FPA.\n");
    if (fpa_int_sum.s.q5_und)
        PRINT_ERROR("FPA_INT_SUM[Q5_UND]: Set when a Queue0 page count available goes\n"
                    "    negative.\n");
    if (fpa_int_sum.s.q4_perr)
        PRINT_ERROR("FPA_INT_SUM[Q4_PERR]: Set when a Queue0 pointer read from the stack in\n"
                    "    the L2C does not have the FPA owner ship bit set.\n");
    if (fpa_int_sum.s.q4_coff)
        PRINT_ERROR("FPA_INT_SUM[Q4_COFF]: Set when a Queue0 stack end tag is present and\n"
                    "    the count available is greater than than pointers\n"
                    "    present in the FPA.\n");
    if (fpa_int_sum.s.q4_und)
        PRINT_ERROR("FPA_INT_SUM[Q4_UND]: Set when a Queue0 page count available goes\n"
                    "    negative.\n");
    if (fpa_int_sum.s.q3_perr)
        PRINT_ERROR("FPA_INT_SUM[Q3_PERR]: Set when a Queue0 pointer read from the stack in\n"
                    "    the L2C does not have the FPA owner ship bit set.\n");
    if (fpa_int_sum.s.q3_coff)
        PRINT_ERROR("FPA_INT_SUM[Q3_COFF]: Set when a Queue0 stack end tag is present and\n"
                    "    the count available is greater than than pointers\n"
                    "    present in the FPA.\n");
    if (fpa_int_sum.s.q3_und)
        PRINT_ERROR("FPA_INT_SUM[Q3_UND]: Set when a Queue0 page count available goes\n"
                    "    negative.\n");
    if (fpa_int_sum.s.q2_perr)
        PRINT_ERROR("FPA_INT_SUM[Q2_PERR]: Set when a Queue0 pointer read from the stack in\n"
                    "    the L2C does not have the FPA owner ship bit set.\n");
    if (fpa_int_sum.s.q2_coff)
        PRINT_ERROR("FPA_INT_SUM[Q2_COFF]: Set when a Queue0 stack end tag is present and\n"
                    "    the count available is greater than than pointers\n"
                    "    present in the FPA.\n");
    if (fpa_int_sum.s.q2_und)
        PRINT_ERROR("FPA_INT_SUM[Q2_UND]: Set when a Queue0 page count available goes\n"
                    "    negative.\n");
    if (fpa_int_sum.s.q1_perr)
        PRINT_ERROR("FPA_INT_SUM[Q1_PERR]: Set when a Queue0 pointer read from the stack in\n"
                    "    the L2C does not have the FPA owner ship bit set.\n");
    if (fpa_int_sum.s.q1_coff)
        PRINT_ERROR("FPA_INT_SUM[Q1_COFF]: Set when a Queue0 stack end tag is present and\n"
                    "    the count available is greater than pointers\n"
                    "    present in the FPA.\n");
    if (fpa_int_sum.s.q1_und)
        PRINT_ERROR("FPA_INT_SUM[Q1_UND]: Set when a Queue0 page count available goes\n"
                    "    negative.\n");
    if (fpa_int_sum.s.q0_perr)
        PRINT_ERROR("FPA_INT_SUM[Q0_PERR]: Set when a Queue0 pointer read from the stack in\n"
                    "    the L2C does not have the FPA owner ship bit set.\n");
    if (fpa_int_sum.s.q0_coff)
        PRINT_ERROR("FPA_INT_SUM[Q0_COFF]: Set when a Queue0 stack end tag is present and\n"
                    "    the count available is greater than pointers\n"
                    "    present in the FPA.\n");
    if (fpa_int_sum.s.q0_und)
        PRINT_ERROR("FPA_INT_SUM[Q0_UND]: Set when a Queue0 page count available goes\n"
                    "    negative.\n");
    if (fpa_int_sum.s.fed1_dbe)
        PRINT_ERROR("FPA_INT_SUM[FED1_DBE]: Set when a Double Bit Error is detected in FPF1.\n");
    if (fpa_int_sum.s.fed1_sbe)
        PRINT_ERROR("FPA_INT_SUM[FED1_SBE]: Set when a Single Bit Error is detected in FPF1.\n");
    if (fpa_int_sum.s.fed0_dbe)
        PRINT_ERROR("FPA_INT_SUM[FED0_DBE]: Set when a Double Bit Error is detected in FPF0.\n");
    if (fpa_int_sum.s.fed0_sbe)
        PRINT_ERROR("FPA_INT_SUM[FED0_SBE]: Set when a Single Bit Error is detected in FPF0.\n");
}


/**
 * __cvmx_interrupt_gmxx_rxx_int_en_enable enables all interrupt bits in cvmx_gmxx_rxx_int_en_t
 */
void __cvmx_interrupt_gmxx_rxx_int_en_enable(int index, int block)
{
    cvmx_gmxx_rxx_int_en_t gmx_rx_int_en;
    cvmx_write_csr(CVMX_GMXX_RXX_INT_REG(index, block), cvmx_read_csr(CVMX_GMXX_RXX_INT_REG(index, block)));
    gmx_rx_int_en.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping gmx_rx_int_en.s.reserved_29_63
        gmx_rx_int_en.s.hg2cc = 1;
        gmx_rx_int_en.s.hg2fld = 1;
        gmx_rx_int_en.s.undat = 1;
        gmx_rx_int_en.s.uneop = 1;
        gmx_rx_int_en.s.unsop = 1;
        gmx_rx_int_en.s.bad_term = 1;
        gmx_rx_int_en.s.bad_seq = 1;
        gmx_rx_int_en.s.rem_fault = 1;
        gmx_rx_int_en.s.loc_fault = 1;
        gmx_rx_int_en.s.pause_drp = 1;
        // Skipping gmx_rx_int_en.s.reserved_16_18
        //gmx_rx_int_en.s.ifgerr = 1;
        //gmx_rx_int_en.s.coldet = 1; // Collsion detect
        //gmx_rx_int_en.s.falerr = 1; // False carrier error or extend error after slottime
        //gmx_rx_int_en.s.rsverr = 1; // RGMII reserved opcodes
        //gmx_rx_int_en.s.pcterr = 1; // Bad Preamble / Protocol
        gmx_rx_int_en.s.ovrerr = 1;
        // Skipping gmx_rx_int_en.s.reserved_9_9
        gmx_rx_int_en.s.skperr = 1;
        gmx_rx_int_en.s.rcverr = 1;
        // Skipping gmx_rx_int_en.s.reserved_5_6
        //gmx_rx_int_en.s.fcserr = 1; // FCS errors are handled when we get work
        gmx_rx_int_en.s.jabber = 1;
        // Skipping gmx_rx_int_en.s.reserved_2_2
        gmx_rx_int_en.s.carext = 1;
        // Skipping gmx_rx_int_en.s.reserved_0_0
    }
    if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        // Skipping gmx_rx_int_en.s.reserved_19_63
        //gmx_rx_int_en.s.phy_dupx = 1;
        //gmx_rx_int_en.s.phy_spd = 1;
        //gmx_rx_int_en.s.phy_link = 1;
        //gmx_rx_int_en.s.ifgerr = 1;
        //gmx_rx_int_en.s.coldet = 1; // Collsion detect
        //gmx_rx_int_en.s.falerr = 1; // False carrier error or extend error after slottime
        //gmx_rx_int_en.s.rsverr = 1; // RGMII reserved opcodes
        //gmx_rx_int_en.s.pcterr = 1; // Bad Preamble / Protocol
        gmx_rx_int_en.s.ovrerr = 1;
        gmx_rx_int_en.s.niberr = 1;
        gmx_rx_int_en.s.skperr = 1;
        gmx_rx_int_en.s.rcverr = 1;
        //gmx_rx_int_en.s.lenerr = 1; // Length errors are handled when we get work
        gmx_rx_int_en.s.alnerr = 1;
        //gmx_rx_int_en.s.fcserr = 1; // FCS errors are handled when we get work
        gmx_rx_int_en.s.jabber = 1;
        gmx_rx_int_en.s.maxerr = 1;
        gmx_rx_int_en.s.carext = 1;
        gmx_rx_int_en.s.minerr = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        // Skipping gmx_rx_int_en.s.reserved_20_63
        gmx_rx_int_en.s.pause_drp = 1;
        //gmx_rx_int_en.s.phy_dupx = 1;
        //gmx_rx_int_en.s.phy_spd = 1;
        //gmx_rx_int_en.s.phy_link = 1;
        //gmx_rx_int_en.s.ifgerr = 1;
        //gmx_rx_int_en.s.coldet = 1; // Collsion detect
        //gmx_rx_int_en.s.falerr = 1; // False carrier error or extend error after slottime
        //gmx_rx_int_en.s.rsverr = 1; // RGMII reserved opcodes
        //gmx_rx_int_en.s.pcterr = 1; // Bad Preamble / Protocol
        gmx_rx_int_en.s.ovrerr = 1;
        gmx_rx_int_en.s.niberr = 1;
        gmx_rx_int_en.s.skperr = 1;
        gmx_rx_int_en.s.rcverr = 1;
        // Skipping gmx_rx_int_en.s.reserved_6_6
        gmx_rx_int_en.s.alnerr = 1;
        //gmx_rx_int_en.s.fcserr = 1; // FCS errors are handled when we get work
        gmx_rx_int_en.s.jabber = 1;
        // Skipping gmx_rx_int_en.s.reserved_2_2
        gmx_rx_int_en.s.carext = 1;
        // Skipping gmx_rx_int_en.s.reserved_0_0
    }
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping gmx_rx_int_en.s.reserved_19_63
        //gmx_rx_int_en.s.phy_dupx = 1;
        //gmx_rx_int_en.s.phy_spd = 1;
        //gmx_rx_int_en.s.phy_link = 1;
        //gmx_rx_int_en.s.ifgerr = 1;
        //gmx_rx_int_en.s.coldet = 1; // Collsion detect
        //gmx_rx_int_en.s.falerr = 1; // False carrier error or extend error after slottime
        //gmx_rx_int_en.s.rsverr = 1; // RGMII reserved opcodes
        //gmx_rx_int_en.s.pcterr = 1; // Bad Preamble / Protocol
        gmx_rx_int_en.s.ovrerr = 1;
        gmx_rx_int_en.s.niberr = 1;
        gmx_rx_int_en.s.skperr = 1;
        gmx_rx_int_en.s.rcverr = 1;
        //gmx_rx_int_en.s.lenerr = 1; // Length errors are handled when we get work
        gmx_rx_int_en.s.alnerr = 1;
        //gmx_rx_int_en.s.fcserr = 1; // FCS errors are handled when we get work
        gmx_rx_int_en.s.jabber = 1;
        gmx_rx_int_en.s.maxerr = 1;
        gmx_rx_int_en.s.carext = 1;
        gmx_rx_int_en.s.minerr = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        // Skipping gmx_rx_int_en.s.reserved_19_63
        //gmx_rx_int_en.s.phy_dupx = 1;
        //gmx_rx_int_en.s.phy_spd = 1;
        //gmx_rx_int_en.s.phy_link = 1;
        //gmx_rx_int_en.s.ifgerr = 1;
        //gmx_rx_int_en.s.coldet = 1; // Collsion detect
        //gmx_rx_int_en.s.falerr = 1; // False carrier error or extend error after slottime
        //gmx_rx_int_en.s.rsverr = 1; // RGMII reserved opcodes
        //gmx_rx_int_en.s.pcterr = 1; // Bad Preamble / Protocol
        gmx_rx_int_en.s.ovrerr = 1;
        gmx_rx_int_en.s.niberr = 1;
        gmx_rx_int_en.s.skperr = 1;
        gmx_rx_int_en.s.rcverr = 1;
        //gmx_rx_int_en.s.lenerr = 1; // Length errors are handled when we get work
        gmx_rx_int_en.s.alnerr = 1;
        //gmx_rx_int_en.s.fcserr = 1; // FCS errors are handled when we get work
        gmx_rx_int_en.s.jabber = 1;
        gmx_rx_int_en.s.maxerr = 1;
        gmx_rx_int_en.s.carext = 1;
        gmx_rx_int_en.s.minerr = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping gmx_rx_int_en.s.reserved_20_63
        gmx_rx_int_en.s.pause_drp = 1;
        //gmx_rx_int_en.s.phy_dupx = 1;
        //gmx_rx_int_en.s.phy_spd = 1;
        //gmx_rx_int_en.s.phy_link = 1;
        //gmx_rx_int_en.s.ifgerr = 1;
        //gmx_rx_int_en.s.coldet = 1; // Collsion detect
        //gmx_rx_int_en.s.falerr = 1; // False carrier error or extend error after slottime
        //gmx_rx_int_en.s.rsverr = 1; // RGMII reserved opcodes
        //gmx_rx_int_en.s.pcterr = 1; // Bad Preamble / Protocol
        gmx_rx_int_en.s.ovrerr = 1;
        gmx_rx_int_en.s.niberr = 1;
        gmx_rx_int_en.s.skperr = 1;
        gmx_rx_int_en.s.rcverr = 1;
        //gmx_rx_int_en.s.lenerr = 1; // Length errors are handled when we get work
        gmx_rx_int_en.s.alnerr = 1;
        //gmx_rx_int_en.s.fcserr = 1; // FCS errors are handled when we get work
        gmx_rx_int_en.s.jabber = 1;
        gmx_rx_int_en.s.maxerr = 1;
        gmx_rx_int_en.s.carext = 1;
        gmx_rx_int_en.s.minerr = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping gmx_rx_int_en.s.reserved_29_63
        gmx_rx_int_en.s.hg2cc = 1;
        gmx_rx_int_en.s.hg2fld = 1;
        gmx_rx_int_en.s.undat = 1;
        gmx_rx_int_en.s.uneop = 1;
        gmx_rx_int_en.s.unsop = 1;
        gmx_rx_int_en.s.bad_term = 1;
        gmx_rx_int_en.s.bad_seq = 0;
        gmx_rx_int_en.s.rem_fault = 1;
        gmx_rx_int_en.s.loc_fault = 0;
        gmx_rx_int_en.s.pause_drp = 1;
        // Skipping gmx_rx_int_en.s.reserved_16_18
        //gmx_rx_int_en.s.ifgerr = 1;
        //gmx_rx_int_en.s.coldet = 1; // Collsion detect
        //gmx_rx_int_en.s.falerr = 1; // False carrier error or extend error after slottime
        //gmx_rx_int_en.s.rsverr = 1; // RGMII reserved opcodes
        //gmx_rx_int_en.s.pcterr = 1; // Bad Preamble / Protocol
        gmx_rx_int_en.s.ovrerr = 1;
        // Skipping gmx_rx_int_en.s.reserved_9_9
        gmx_rx_int_en.s.skperr = 1;
        gmx_rx_int_en.s.rcverr = 1;
        // Skipping gmx_rx_int_en.s.reserved_5_6
        //gmx_rx_int_en.s.fcserr = 1; // FCS errors are handled when we get work
        gmx_rx_int_en.s.jabber = 1;
        // Skipping gmx_rx_int_en.s.reserved_2_2
        gmx_rx_int_en.s.carext = 1;
        // Skipping gmx_rx_int_en.s.reserved_0_0
    }
    cvmx_write_csr(CVMX_GMXX_RXX_INT_EN(index, block), gmx_rx_int_en.u64);
}


/**
 * __cvmx_interrupt_gmxx_rxx_int_reg_decode decodes all interrupt bits in cvmx_gmxx_rxx_int_reg_t
 */
void __cvmx_interrupt_gmxx_rxx_int_reg_decode(int index, int block)
{
    cvmx_gmxx_rxx_int_reg_t gmx_rx_int_reg;
    gmx_rx_int_reg.u64 = cvmx_read_csr(CVMX_GMXX_RXX_INT_REG(index, block));
    /* Don't clear inband status bits so someone else can use them */
    gmx_rx_int_reg.s.phy_dupx = 0;
    gmx_rx_int_reg.s.phy_spd = 0;
    gmx_rx_int_reg.s.phy_link = 0;
    gmx_rx_int_reg.u64 &= cvmx_read_csr(CVMX_GMXX_RXX_INT_EN(index, block));
    cvmx_write_csr(CVMX_GMXX_RXX_INT_REG(index, block), gmx_rx_int_reg.u64);
    // Skipping gmx_rx_int_reg.s.reserved_29_63
    if (gmx_rx_int_reg.s.hg2cc)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[HG2CC]: HiGig2 received message CRC or Control char  error\n"
                    "    Set when either CRC8 error detected or when\n"
                    "    a Control Character is found in the message\n"
                    "    bytes after the K.SOM\n"
                    "    NOTE: HG2CC has higher priority than HG2FLD\n"
                    "          i.e. a HiGig2 message that results in HG2CC\n"
                    "          getting set, will never set HG2FLD.\n", block, index);
    if (gmx_rx_int_reg.s.hg2fld)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[HG2FLD]: HiGig2 received message field error, as below\n"
                    "    1) MSG_TYPE field not 6'b00_0000\n"
                    "       i.e. it is not a FLOW CONTROL message, which\n"
                    "       is the only defined type for HiGig2\n"
                    "    2) FWD_TYPE field not 2'b00 i.e. Link Level msg\n"
                    "       which is the only defined type for HiGig2\n"
                    "    3) FC_OBJECT field is neither 4'b0000 for\n"
                    "       Physical Link nor 4'b0010 for Logical Link.\n"
                    "       Those are the only two defined types in HiGig2\n", block, index);
    if (gmx_rx_int_reg.s.undat)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[UNDAT]: Unexpected Data\n"
                    "    (XAUI Mode only)\n", block, index);
    if (gmx_rx_int_reg.s.uneop)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[UNEOP]: Unexpected EOP\n"
                    "    (XAUI Mode only)\n", block, index);
    if (gmx_rx_int_reg.s.unsop)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[UNSOP]: Unexpected SOP\n"
                    "    (XAUI Mode only)\n", block, index);
    if (gmx_rx_int_reg.s.bad_term)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[BAD_TERM]: Frame is terminated by control character other\n"
                    "    than /T/.  The error propagation control\n"
                    "    character /E/ will be included as part of the\n"
                    "    frame and does not cause a frame termination.\n"
                    "    (XAUI Mode only)\n", block, index);
    if (gmx_rx_int_reg.s.bad_seq)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[BAD_SEQ]: Reserved Sequence Deteted\n"
                    "    (XAUI Mode only)\n", block, index);
    if (gmx_rx_int_reg.s.rem_fault)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[REM_FAULT]: Remote Fault Sequence Deteted\n"
                    "    (XAUI Mode only)\n", block, index);
    if (gmx_rx_int_reg.s.loc_fault)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[LOC_FAULT]: Local Fault Sequence Deteted\n"
                    "    (XAUI Mode only)\n", block, index);
    if (gmx_rx_int_reg.s.pause_drp)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[PAUSE_DRP]: Pause packet was dropped due to full GMX RX FIFO\n", block, index);
#if 0
    if (gmx_rx_int_reg.s.phy_dupx)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[PHY_DUPX]: Change in the RMGII inbound LinkDuplex\n", block, index);
    if (gmx_rx_int_reg.s.phy_spd)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[PHY_SPD]: Change in the RMGII inbound LinkSpeed\n", block, index);
    if (gmx_rx_int_reg.s.phy_link)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[PHY_LINK]: Change in the RMGII inbound LinkStatus\n", block, index);
#endif
    if (gmx_rx_int_reg.s.ifgerr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[IFGERR]: Interframe Gap Violation\n"
                    "    Does not necessarily indicate a failure\n", block, index);
    if (gmx_rx_int_reg.s.coldet)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[COLDET]: Collision Detection\n", block, index);
    if (gmx_rx_int_reg.s.falerr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[FALERR]: False carrier error or extend error after slottime\n", block, index);
    if (gmx_rx_int_reg.s.rsverr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[RSVERR]: RGMII reserved opcodes\n", block, index);
    if (gmx_rx_int_reg.s.pcterr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[PCTERR]: Bad Preamble / Protocol\n", block, index);
    if (gmx_rx_int_reg.s.ovrerr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[OVRERR]: Internal Data Aggregation Overflow\n"
                    "    This interrupt should never assert\n", block, index);
    if (gmx_rx_int_reg.s.niberr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[NIBERR]: Nibble error (hi_nibble != lo_nibble)\n", block, index);
    if (gmx_rx_int_reg.s.skperr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[SKPERR]: Skipper error\n", block, index);
    if (gmx_rx_int_reg.s.rcverr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[RCVERR]: Frame was received with RMGII Data reception error\n", block, index);
    if (gmx_rx_int_reg.s.lenerr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[LENERR]: Frame was received with length error\n", block, index);
    if (gmx_rx_int_reg.s.alnerr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[ALNERR]: Frame was received with an alignment error\n", block, index);
    if (gmx_rx_int_reg.s.fcserr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[FCSERR]: Frame was received with FCS/CRC error\n", block, index);
    if (gmx_rx_int_reg.s.jabber)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[JABBER]: Frame was received with length > sys_length\n", block, index);
    if (gmx_rx_int_reg.s.maxerr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[MAXERR]: Frame was received with length > max_length\n", block, index);
    if (gmx_rx_int_reg.s.carext)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[CAREXT]: RGMII carrier extend error\n", block, index);
    if (gmx_rx_int_reg.s.minerr)
        PRINT_ERROR("GMX%d_RX%d_INT_REG[MINERR]: Frame was received with length < min_length\n", block, index);
}


/**
 * __cvmx_interrupt_iob_int_enb_enable enables all interrupt bits in cvmx_iob_int_enb_t
 */
void __cvmx_interrupt_iob_int_enb_enable(void)
{
    cvmx_iob_int_enb_t iob_int_enb;
    cvmx_write_csr(CVMX_IOB_INT_SUM, cvmx_read_csr(CVMX_IOB_INT_SUM));
    iob_int_enb.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping iob_int_enb.s.reserved_6_63
        iob_int_enb.s.p_dat = 1;
        iob_int_enb.s.p_eop = 1;
        iob_int_enb.s.p_sop = 1;
        /* These interrupts are disabled on CN56XXp2.X due to errata IOB-800 */
        if (!OCTEON_IS_MODEL(OCTEON_CN56XX_PASS2_X))
        {
            iob_int_enb.s.np_dat = 1;
            iob_int_enb.s.np_eop = 1;
            iob_int_enb.s.np_sop = 1;
        }
    }
    if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        // Skipping iob_int_enb.s.reserved_4_63
        iob_int_enb.s.p_eop = 1;
        iob_int_enb.s.p_sop = 1;
        iob_int_enb.s.np_eop = 1;
        iob_int_enb.s.np_sop = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        // Skipping iob_int_enb.s.reserved_6_63
        iob_int_enb.s.p_dat = 1;
        iob_int_enb.s.np_dat = 1;
        iob_int_enb.s.p_eop = 1;
        iob_int_enb.s.p_sop = 1;
        iob_int_enb.s.np_eop = 1;
        iob_int_enb.s.np_sop = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping iob_int_enb.s.reserved_4_63
        iob_int_enb.s.p_eop = 1;
        iob_int_enb.s.p_sop = 1;
        iob_int_enb.s.np_eop = 1;
        iob_int_enb.s.np_sop = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        // Skipping iob_int_enb.s.reserved_4_63
        iob_int_enb.s.p_eop = 1;
        iob_int_enb.s.p_sop = 1;
        iob_int_enb.s.np_eop = 1;
        iob_int_enb.s.np_sop = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping iob_int_enb.s.reserved_6_63
        iob_int_enb.s.p_dat = 1;
        iob_int_enb.s.np_dat = 1;
        iob_int_enb.s.p_eop = 1;
        iob_int_enb.s.p_sop = 1;
        iob_int_enb.s.np_eop = 1;
        iob_int_enb.s.np_sop = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping iob_int_enb.s.reserved_6_63
        iob_int_enb.s.p_dat = 1;
        iob_int_enb.s.p_eop = 1;
        iob_int_enb.s.p_sop = 1;
        /* These interrupts are disabled on CN52XXp2.X due to errata IOB-800 */
        if (!OCTEON_IS_MODEL(OCTEON_CN52XX_PASS2_X))
        {
            iob_int_enb.s.np_dat = 1;
            iob_int_enb.s.np_eop = 1;
            iob_int_enb.s.np_sop = 1;
        }
    }
    cvmx_write_csr(CVMX_IOB_INT_ENB, iob_int_enb.u64);
}


/**
 * __cvmx_interrupt_iob_int_sum_decode decodes all interrupt bits in cvmx_iob_int_sum_t
 */
void __cvmx_interrupt_iob_int_sum_decode(void)
{
    cvmx_iob_int_sum_t iob_int_sum;
    iob_int_sum.u64 = cvmx_read_csr(CVMX_IOB_INT_SUM);
    iob_int_sum.u64 &= cvmx_read_csr(CVMX_IOB_INT_ENB);
    cvmx_write_csr(CVMX_IOB_INT_SUM, iob_int_sum.u64);
    // Skipping iob_int_sum.s.reserved_6_63
    if (iob_int_sum.s.p_dat)
        PRINT_ERROR("IOB_INT_SUM[P_DAT]: Set when a data arrives before a SOP for the same\n"
                    "    port for a passthrough packet.\n"
                    "    The first detected error associated with bits [5:0]\n"
                    "    of this register will only be set here. A new bit\n"
                    "    can be set when the previous reported bit is cleared.\n");
    if (iob_int_sum.s.np_dat)
        PRINT_ERROR("IOB_INT_SUM[NP_DAT]: Set when a data arrives before a SOP for the same\n"
                    "    port for a non-passthrough packet.\n"
                    "    The first detected error associated with bits [5:0]\n"
                    "    of this register will only be set here. A new bit\n"
                    "    can be set when the previous reported bit is cleared.\n");
    if (iob_int_sum.s.p_eop)
        PRINT_ERROR("IOB_INT_SUM[P_EOP]: Set when a EOP is followed by an EOP for the same\n"
                    "    port for a passthrough packet.\n"
                    "    The first detected error associated with bits [5:0]\n"
                    "    of this register will only be set here. A new bit\n"
                    "    can be set when the previous reported bit is cleared.\n");
    if (iob_int_sum.s.p_sop)
        PRINT_ERROR("IOB_INT_SUM[P_SOP]: Set when a SOP is followed by an SOP for the same\n"
                    "    port for a passthrough packet.\n"
                    "    The first detected error associated with bits [5:0]\n"
                    "    of this register will only be set here. A new bit\n"
                    "    can be set when the previous reported bit is cleared.\n");
    if (iob_int_sum.s.np_eop)
        PRINT_ERROR("IOB_INT_SUM[NP_EOP]: Set when a EOP is followed by an EOP for the same\n"
                    "    port for a non-passthrough packet.\n"
                    "    The first detected error associated with bits [5:0]\n"
                    "    of this register will only be set here. A new bit\n"
                    "    can be set when the previous reported bit is cleared.\n");
    if (iob_int_sum.s.np_sop)
        PRINT_ERROR("IOB_INT_SUM[NP_SOP]: Set when a SOP is followed by an SOP for the same\n"
                    "    port for a non-passthrough packet.\n"
                    "    The first detected error associated with bits [5:0]\n"
                    "    of this register will only be set here. A new bit\n"
                    "    can be set when the previous reported bit is cleared.\n");
}


/**
 * __cvmx_interrupt_ipd_int_enb_enable enables all interrupt bits in cvmx_ipd_int_enb_t
 */
void __cvmx_interrupt_ipd_int_enb_enable(void)
{
    cvmx_ipd_int_enb_t ipd_int_enb;
    cvmx_write_csr(CVMX_IPD_INT_SUM, cvmx_read_csr(CVMX_IPD_INT_SUM));
    ipd_int_enb.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping ipd_int_enb.s.reserved_12_63
        //ipd_int_enb.s.pq_sub = 1; // Disable per port backpressure overflow checking since it happens when not in use
        //ipd_int_enb.s.pq_add = 1; // Disable per port backpressure overflow checking since it happens when not in use
        ipd_int_enb.s.bc_ovr = 1;
        ipd_int_enb.s.d_coll = 1;
        ipd_int_enb.s.c_coll = 1;
        ipd_int_enb.s.cc_ovr = 1;
        ipd_int_enb.s.dc_ovr = 1;
        ipd_int_enb.s.bp_sub = 1;
        ipd_int_enb.s.prc_par3 = 1;
        ipd_int_enb.s.prc_par2 = 1;
        ipd_int_enb.s.prc_par1 = 1;
        ipd_int_enb.s.prc_par0 = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        // Skipping ipd_int_enb.s.reserved_5_63
        ipd_int_enb.s.bp_sub = 1;
        ipd_int_enb.s.prc_par3 = 1;
        ipd_int_enb.s.prc_par2 = 1;
        ipd_int_enb.s.prc_par1 = 1;
        ipd_int_enb.s.prc_par0 = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        // Skipping ipd_int_enb.s.reserved_10_63
        ipd_int_enb.s.bc_ovr = 1;
        ipd_int_enb.s.d_coll = 1;
        ipd_int_enb.s.c_coll = 1;
        ipd_int_enb.s.cc_ovr = 1;
        ipd_int_enb.s.dc_ovr = 1;
        ipd_int_enb.s.bp_sub = 1;
        ipd_int_enb.s.prc_par3 = 1;
        ipd_int_enb.s.prc_par2 = 1;
        ipd_int_enb.s.prc_par1 = 1;
        ipd_int_enb.s.prc_par0 = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping ipd_int_enb.s.reserved_10_63
        if (!OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2))
        {
            ipd_int_enb.s.bc_ovr = 1;
            ipd_int_enb.s.d_coll = 1;
            ipd_int_enb.s.c_coll = 1;
            ipd_int_enb.s.cc_ovr = 1;
            ipd_int_enb.s.dc_ovr = 1;
        }
        ipd_int_enb.s.bp_sub = 1;
        ipd_int_enb.s.prc_par3 = 1;
        ipd_int_enb.s.prc_par2 = 1;
        ipd_int_enb.s.prc_par1 = 1;
        ipd_int_enb.s.prc_par0 = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        // Skipping ipd_int_enb.s.reserved_5_63
        ipd_int_enb.s.bp_sub = 1;
        ipd_int_enb.s.prc_par3 = 1;
        ipd_int_enb.s.prc_par2 = 1;
        ipd_int_enb.s.prc_par1 = 1;
        ipd_int_enb.s.prc_par0 = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping ipd_int_enb.s.reserved_10_63
        ipd_int_enb.s.bc_ovr = 1;
        ipd_int_enb.s.d_coll = 1;
        ipd_int_enb.s.c_coll = 1;
        ipd_int_enb.s.cc_ovr = 1;
        ipd_int_enb.s.dc_ovr = 1;
        ipd_int_enb.s.bp_sub = 1;
        ipd_int_enb.s.prc_par3 = 1;
        ipd_int_enb.s.prc_par2 = 1;
        ipd_int_enb.s.prc_par1 = 1;
        ipd_int_enb.s.prc_par0 = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping ipd_int_enb.s.reserved_12_63
        //ipd_int_enb.s.pq_sub = 1; // Disable per port backpressure overflow checking since it happens when not in use
        //ipd_int_enb.s.pq_add = 1; // Disable per port backpressure overflow checking since it happens when not in use
        ipd_int_enb.s.bc_ovr = 1;
        ipd_int_enb.s.d_coll = 1;
        ipd_int_enb.s.c_coll = 1;
        ipd_int_enb.s.cc_ovr = 1;
        ipd_int_enb.s.dc_ovr = 1;
        ipd_int_enb.s.bp_sub = 1;
        ipd_int_enb.s.prc_par3 = 1;
        ipd_int_enb.s.prc_par2 = 1;
        ipd_int_enb.s.prc_par1 = 1;
        ipd_int_enb.s.prc_par0 = 1;
    }
    cvmx_write_csr(CVMX_IPD_INT_ENB, ipd_int_enb.u64);
}


/**
 * __cvmx_interrupt_ipd_int_sum_decode decodes all interrupt bits in cvmx_ipd_int_sum_t
 */
void __cvmx_interrupt_ipd_int_sum_decode(void)
{
    cvmx_ipd_int_sum_t ipd_int_sum;
    ipd_int_sum.u64 = cvmx_read_csr(CVMX_IPD_INT_SUM);
    ipd_int_sum.u64 &= cvmx_read_csr(CVMX_IPD_INT_ENB);
    cvmx_write_csr(CVMX_IPD_INT_SUM, ipd_int_sum.u64);
    // Skipping ipd_int_sum.s.reserved_12_63
    if (ipd_int_sum.s.pq_sub)
        PRINT_ERROR("IPD_INT_SUM[PQ_SUB]: Set when a port-qos does an sub to the count\n"
                    "    that causes the counter to wrap.\n");
    if (ipd_int_sum.s.pq_add)
        PRINT_ERROR("IPD_INT_SUM[PQ_ADD]: Set when a port-qos does an add to the count\n"
                    "    that causes the counter to wrap.\n");
    if (ipd_int_sum.s.bc_ovr)
        PRINT_ERROR("IPD_INT_SUM[BC_OVR]: Set when the byte-count to send to IOB overflows.\n"
                    "    This is a PASS-3 Field.\n");
    if (ipd_int_sum.s.d_coll)
        PRINT_ERROR("IPD_INT_SUM[D_COLL]: Set when the packet/WQE data to be sent to IOB\n"
                    "    collides.\n"
                    "    This is a PASS-3 Field.\n");
    if (ipd_int_sum.s.c_coll)
        PRINT_ERROR("IPD_INT_SUM[C_COLL]: Set when the packet/WQE commands to be sent to IOB\n"
                    "    collides.\n"
                    "    This is a PASS-3 Field.\n");
    if (ipd_int_sum.s.cc_ovr)
        PRINT_ERROR("IPD_INT_SUM[CC_OVR]: Set when the command credits to the IOB overflow.\n"
                    "    This is a PASS-3 Field.\n");
    if (ipd_int_sum.s.dc_ovr)
        PRINT_ERROR("IPD_INT_SUM[DC_OVR]: Set when the data credits to the IOB overflow.\n"
                    "    This is a PASS-3 Field.\n");
    if (ipd_int_sum.s.bp_sub)
        PRINT_ERROR("IPD_INT_SUM[BP_SUB]: Set when a backpressure subtract is done with a\n"
                    "    supplied illegal value.\n");
    if (ipd_int_sum.s.prc_par3)
        PRINT_ERROR("IPD_INT_SUM[PRC_PAR3]: Set when a parity error is dected for bits\n"
                    "    [127:96] of the PBM memory.\n");
    if (ipd_int_sum.s.prc_par2)
        PRINT_ERROR("IPD_INT_SUM[PRC_PAR2]: Set when a parity error is dected for bits\n"
                    "    [95:64] of the PBM memory.\n");
    if (ipd_int_sum.s.prc_par1)
        PRINT_ERROR("IPD_INT_SUM[PRC_PAR1]: Set when a parity error is dected for bits\n"
                    "    [63:32] of the PBM memory.\n");
    if (ipd_int_sum.s.prc_par0)
        PRINT_ERROR("IPD_INT_SUM[PRC_PAR0]: Set when a parity error is dected for bits\n"
                    "    [31:0] of the PBM memory.\n");
}


/**
 * __cvmx_interrupt_key_int_enb_enable enables all interrupt bits in cvmx_key_int_enb_t
 */
void __cvmx_interrupt_key_int_enb_enable(void)
{
    cvmx_key_int_enb_t key_int_enb;
    cvmx_write_csr(CVMX_KEY_INT_SUM, cvmx_read_csr(CVMX_KEY_INT_SUM));
    key_int_enb.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping key_int_enb.s.reserved_4_63
        key_int_enb.s.ked1_dbe = 1;
        key_int_enb.s.ked1_sbe = 1;
        key_int_enb.s.ked0_dbe = 1;
        key_int_enb.s.ked0_sbe = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping key_int_enb.s.reserved_4_63
        key_int_enb.s.ked1_dbe = 1;
        key_int_enb.s.ked1_sbe = 1;
        key_int_enb.s.ked0_dbe = 1;
        key_int_enb.s.ked0_sbe = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping key_int_enb.s.reserved_4_63
        key_int_enb.s.ked1_dbe = 1;
        key_int_enb.s.ked1_sbe = 1;
        key_int_enb.s.ked0_dbe = 1;
        key_int_enb.s.ked0_sbe = 1;
    }
    cvmx_write_csr(CVMX_KEY_INT_ENB, key_int_enb.u64);
}


/**
 * __cvmx_interrupt_key_int_sum_decode decodes all interrupt bits in cvmx_key_int_sum_t
 */
void __cvmx_interrupt_key_int_sum_decode(void)
{
    cvmx_key_int_sum_t key_int_sum;
    key_int_sum.u64 = cvmx_read_csr(CVMX_KEY_INT_SUM);
    key_int_sum.u64 &= cvmx_read_csr(CVMX_KEY_INT_ENB);
    cvmx_write_csr(CVMX_KEY_INT_SUM, key_int_sum.u64);
    // Skipping key_int_sum.s.reserved_4_63
    if (key_int_sum.s.ked1_dbe)
        PRINT_ERROR("KEY_INT_SUM[KED1_DBE]: Error bit\n");
    if (key_int_sum.s.ked1_sbe)
        PRINT_ERROR("KEY_INT_SUM[KED1_SBE]: Error bit\n");
    if (key_int_sum.s.ked0_dbe)
        PRINT_ERROR("KEY_INT_SUM[KED0_DBE]: Error bit\n");
    if (key_int_sum.s.ked0_sbe)
        PRINT_ERROR("KEY_INT_SUM[KED0_SBE]: Error bit\n");
}


/**
 * __cvmx_interrupt_mio_boot_int_enable enables all interrupt bits in cvmx_mio_boot_int_t
 */
void __cvmx_interrupt_mio_boot_int_enable(void)
{
    cvmx_mio_boot_int_t mio_boot_int;
    cvmx_write_csr(CVMX_MIO_BOOT_ERR, cvmx_read_csr(CVMX_MIO_BOOT_ERR));
    mio_boot_int.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping mio_boot_int.s.reserved_2_63
        mio_boot_int.s.wait_int = 1;
        mio_boot_int.s.adr_int = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        // Skipping mio_boot_int.s.reserved_2_63
        mio_boot_int.s.wait_int = 1;
        mio_boot_int.s.adr_int = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        // Skipping mio_boot_int.s.reserved_2_63
        mio_boot_int.s.wait_int = 1;
        mio_boot_int.s.adr_int = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping mio_boot_int.s.reserved_2_63
        mio_boot_int.s.wait_int = 1;
        mio_boot_int.s.adr_int = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        // Skipping mio_boot_int.s.reserved_2_63
        mio_boot_int.s.wait_int = 1;
        mio_boot_int.s.adr_int = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping mio_boot_int.s.reserved_2_63
        mio_boot_int.s.wait_int = 1;
        mio_boot_int.s.adr_int = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping mio_boot_int.s.reserved_2_63
        mio_boot_int.s.wait_int = 1;
        mio_boot_int.s.adr_int = 1;
    }
    cvmx_write_csr(CVMX_MIO_BOOT_INT, mio_boot_int.u64);
}


/**
 * __cvmx_interrupt_mio_boot_err_decode decodes all interrupt bits in cvmx_mio_boot_err_t
 */
void __cvmx_interrupt_mio_boot_err_decode(void)
{
    cvmx_mio_boot_err_t mio_boot_err;
    mio_boot_err.u64 = cvmx_read_csr(CVMX_MIO_BOOT_ERR);
    mio_boot_err.u64 &= cvmx_read_csr(CVMX_MIO_BOOT_INT);
    cvmx_write_csr(CVMX_MIO_BOOT_ERR, mio_boot_err.u64);
    // Skipping mio_boot_err.s.reserved_2_63
    if (mio_boot_err.s.wait_err)
        PRINT_ERROR("MIO_BOOT_ERR[WAIT_ERR]: Wait mode error\n");
    if (mio_boot_err.s.adr_err)
        PRINT_ERROR("MIO_BOOT_ERR[ADR_ERR]: Address decode error\n");
}


/**
 * __cvmx_interrupt_npei_int_sum_decode decodes all interrupt bits in cvmx_npei_int_sum_t
 */
void __cvmx_interrupt_npei_int_sum_decode(void)
{
    cvmx_npei_int_sum_t npei_int_sum;
    npei_int_sum.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_INT_SUM);
    /* Note that NPEI_INT_ENB2 controls the internal RSL interrupts.
        NPEI_INT_ENB controls external forwarding which is not what we
        want. It is a little strange that we are using NPEI_INT_SUM with
        NPEI_INT_ENB2, but we need the R/W version of NPEI_INT_SUM2 and
        internal RSL interrupts */
    npei_int_sum.u64 &= cvmx_read_csr(CVMX_PEXP_NPEI_INT_ENB2);
    cvmx_write_csr(CVMX_PEXP_NPEI_INT_SUM, npei_int_sum.u64);
    if (npei_int_sum.s.mio_inta)
        PRINT_ERROR("NPEI_INT_SUM[MIO_INTA]: Interrupt from MIO.\n");
    // Skipping npei_int_sum.s.reserved_62_62
    if (npei_int_sum.s.int_a)
        PRINT_ERROR("NPEI_INT_SUM[INT_A]: Set when a bit in the NPEI_INT_A_SUM register and\n"
                    "    the cooresponding bit in the NPEI_INT_A_ENB\n"
                    "    register is set.\n");
    if (npei_int_sum.s.c1_ldwn)
    {
        cvmx_ciu_soft_prst_t ciu_soft_prst;
        PRINT_ERROR("NPEI_INT_SUM[C1_LDWN]: Reset request due to link1 down status.\n");
        ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
        if (!ciu_soft_prst.s.soft_prst)
        {
            /* Attempt to automatically bring the link back up */
            cvmx_pcie_rc_shutdown(1);
            cvmx_pcie_rc_initialize(1);
        }
        cvmx_write_csr(CVMX_PEXP_NPEI_INT_SUM, cvmx_read_csr(CVMX_PEXP_NPEI_INT_SUM));
    }
    if (npei_int_sum.s.c0_ldwn)
    {
        cvmx_ciu_soft_prst_t ciu_soft_prst;
        PRINT_ERROR("NPEI_INT_SUM[C0_LDWN]: Reset request due to link0 down status.\n");
        ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
        if (!ciu_soft_prst.s.soft_prst)
        {
            /* Attempt to automatically bring the link back up */
            cvmx_pcie_rc_shutdown(0);
            cvmx_pcie_rc_initialize(0);
        }
        cvmx_write_csr(CVMX_PEXP_NPEI_INT_SUM, cvmx_read_csr(CVMX_PEXP_NPEI_INT_SUM));
    }
    if (npei_int_sum.s.c1_exc)
    {
#if 0
        PRINT_ERROR("NPEI_INT_SUM[C1_EXC]: Set when the PESC1_DBG_INFO register has a bit\n"
                    "    set and its cooresponding PESC1_DBG_INFO_EN bit\n"
                    "    is set.\n");
#endif
        __cvmx_interrupt_pescx_dbg_info_decode(1);
    }
    if (npei_int_sum.s.c0_exc)
    {
#if 0
        PRINT_ERROR("NPEI_INT_SUM[C0_EXC]: Set when the PESC0_DBG_INFO register has a bit\n"
                    "    set and its cooresponding PESC0_DBG_INFO_EN bit\n"
                    "    is set.\n");
#endif
        __cvmx_interrupt_pescx_dbg_info_decode(0);
    }
    if (npei_int_sum.s.c1_up_wf)
        PRINT_ERROR("NPEI_INT_SUM[C1_UP_WF]: Received Unsupported P-TLP for filtered window\n"
                    "    register. Core1.\n");
    if (npei_int_sum.s.c0_up_wf)
        PRINT_ERROR("NPEI_INT_SUM[C0_UP_WF]: Received Unsupported P-TLP for filtered window\n"
                    "    register. Core0.\n");
    if (npei_int_sum.s.c1_un_wf)
        PRINT_ERROR("NPEI_INT_SUM[C1_UN_WF]: Received Unsupported N-TLP for filtered window\n"
                    "    register. Core1.\n");
    if (npei_int_sum.s.c0_un_wf)
        PRINT_ERROR("NPEI_INT_SUM[C0_UN_WF]: Received Unsupported N-TLP for filtered window\n"
                    "    register. Core0.\n");
    if (npei_int_sum.s.c1_un_bx)
        PRINT_ERROR("NPEI_INT_SUM[C1_UN_BX]: Received Unsupported N-TLP for unknown Bar.\n"
                    "    Core 1.\n");
    if (npei_int_sum.s.c1_un_wi)
        PRINT_ERROR("NPEI_INT_SUM[C1_UN_WI]: Received Unsupported N-TLP for Window Register.\n"
                    "    Core 1.\n");
    if (npei_int_sum.s.c1_un_b2)
        PRINT_ERROR("NPEI_INT_SUM[C1_UN_B2]: Received Unsupported N-TLP for Bar2.\n"
                    "    Core 1.\n");
    if (npei_int_sum.s.c1_un_b1)
        PRINT_ERROR("NPEI_INT_SUM[C1_UN_B1]: Received Unsupported N-TLP for Bar1.\n"
                    "    Core 1.\n");
    if (npei_int_sum.s.c1_un_b0)
        PRINT_ERROR("NPEI_INT_SUM[C1_UN_B0]: Received Unsupported N-TLP for Bar0.\n"
                    "    Core 1.\n");
    if (npei_int_sum.s.c1_up_bx)
        PRINT_ERROR("NPEI_INT_SUM[C1_UP_BX]: Received Unsupported P-TLP for unknown Bar.\n"
                    "    Core 1.\n");
    if (npei_int_sum.s.c1_up_wi)
        PRINT_ERROR("NPEI_INT_SUM[C1_UP_WI]: Received Unsupported P-TLP for Window Register.\n"
                    "    Core 1.\n");
    if (npei_int_sum.s.c1_up_b2)
        PRINT_ERROR("NPEI_INT_SUM[C1_UP_B2]: Received Unsupported P-TLP for Bar2.\n"
                    "    Core 1.\n");
    if (npei_int_sum.s.c1_up_b1)
        PRINT_ERROR("NPEI_INT_SUM[C1_UP_B1]: Received Unsupported P-TLP for Bar1.\n"
                    "    Core 1.\n");
    if (npei_int_sum.s.c1_up_b0)
        PRINT_ERROR("NPEI_INT_SUM[C1_UP_B0]: Received Unsupported P-TLP for Bar0.\n"
                    "    Core 1.\n");
    if (npei_int_sum.s.c0_un_bx)
        PRINT_ERROR("NPEI_INT_SUM[C0_UN_BX]: Received Unsupported N-TLP for unknown Bar.\n"
                    "    Core 0.\n");
    if (npei_int_sum.s.c0_un_wi)
        PRINT_ERROR("NPEI_INT_SUM[C0_UN_WI]: Received Unsupported N-TLP for Window Register.\n"
                    "    Core 0.\n");
    if (npei_int_sum.s.c0_un_b2)
        PRINT_ERROR("NPEI_INT_SUM[C0_UN_B2]: Received Unsupported N-TLP for Bar2.\n"
                    "    Core 0.\n");
    if (npei_int_sum.s.c0_un_b1)
        PRINT_ERROR("NPEI_INT_SUM[C0_UN_B1]: Received Unsupported N-TLP for Bar1.\n"
                    "    Core 0.\n");
    if (npei_int_sum.s.c0_un_b0)
        PRINT_ERROR("NPEI_INT_SUM[C0_UN_B0]: Received Unsupported N-TLP for Bar0.\n"
                    "    Core 0.\n");
    if (npei_int_sum.s.c0_up_bx)
        PRINT_ERROR("NPEI_INT_SUM[C0_UP_BX]: Received Unsupported P-TLP for unknown Bar.\n"
                    "    Core 0.\n");
    if (npei_int_sum.s.c0_up_wi)
        PRINT_ERROR("NPEI_INT_SUM[C0_UP_WI]: Received Unsupported P-TLP for Window Register.\n"
                    "    Core 0.\n");
    if (npei_int_sum.s.c0_up_b2)
        PRINT_ERROR("NPEI_INT_SUM[C0_UP_B2]: Received Unsupported P-TLP for Bar2.\n"
                    "    Core 0.\n");
    if (npei_int_sum.s.c0_up_b1)
        PRINT_ERROR("NPEI_INT_SUM[C0_UP_B1]: Received Unsupported P-TLP for Bar1.\n"
                    "    Core 0.\n");
    if (npei_int_sum.s.c0_up_b0)
        PRINT_ERROR("NPEI_INT_SUM[C0_UP_B0]: Received Unsupported P-TLP for Bar0.\n"
                    "    Core 0.\n");
    if (npei_int_sum.s.c1_hpint)
        PRINT_ERROR("NPEI_INT_SUM[C1_HPINT]: Hot-Plug Interrupt.\n"
                    "    Pcie Core 1 (hp_int).\n"
                    "    This interrupt will only be generated when\n"
                    "    PCIERC1_CFG034[DLLS_C] is generated. Hot plug is\n"
                    "    not supported.\n");
    if (npei_int_sum.s.c1_pmei)
        PRINT_ERROR("NPEI_INT_SUM[C1_PMEI]: PME Interrupt.\n"
                    "    Pcie Core 1. (cfg_pme_int)\n");
    if (npei_int_sum.s.c1_wake)
        PRINT_ERROR("NPEI_INT_SUM[C1_WAKE]: Wake up from Power Management Unit.\n"
                    "    Pcie Core 1. (wake_n)\n"
                    "    Octeon will never generate this interrupt.\n");
    if (npei_int_sum.s.crs1_dr)
        PRINT_ERROR("NPEI_INT_SUM[CRS1_DR]: Had a CRS when Retries were disabled.\n");
    if (npei_int_sum.s.c1_se)
        PRINT_ERROR("NPEI_INT_SUM[C1_SE]: System Error, RC Mode Only.\n"
                    "    Pcie Core 1. (cfg_sys_err_rc)\n");
    if (npei_int_sum.s.crs1_er)
        PRINT_ERROR("NPEI_INT_SUM[CRS1_ER]: Had a CRS Timeout when Retries were enabled.\n");
    if (npei_int_sum.s.c1_aeri)
        PRINT_ERROR("NPEI_INT_SUM[C1_AERI]: Advanced Error Reporting Interrupt, RC Mode Only.\n"
                    "    Pcie Core 1.\n");
    if (npei_int_sum.s.c0_hpint)
        PRINT_ERROR("NPEI_INT_SUM[C0_HPINT]: Hot-Plug Interrupt.\n"
                    "    Pcie Core 0 (hp_int).\n"
                    "    This interrupt will only be generated when\n"
                    "    PCIERC0_CFG034[DLLS_C] is generated. Hot plug is\n"
                    "    not supported.\n");
    if (npei_int_sum.s.c0_pmei)
        PRINT_ERROR("NPEI_INT_SUM[C0_PMEI]: PME Interrupt.\n"
                    "    Pcie Core 0. (cfg_pme_int)\n");
    if (npei_int_sum.s.c0_wake)
        PRINT_ERROR("NPEI_INT_SUM[C0_WAKE]: Wake up from Power Management Unit.\n"
                    "    Pcie Core 0. (wake_n)\n"
                    "    Octeon will never generate this interrupt.\n");
    if (npei_int_sum.s.crs0_dr)
        PRINT_ERROR("NPEI_INT_SUM[CRS0_DR]: Had a CRS when Retries were disabled.\n");
    if (npei_int_sum.s.c0_se)
        PRINT_ERROR("NPEI_INT_SUM[C0_SE]: System Error, RC Mode Only.\n"
                    "    Pcie Core 0. (cfg_sys_err_rc)\n");
    if (npei_int_sum.s.crs0_er)
        PRINT_ERROR("NPEI_INT_SUM[CRS0_ER]: Had a CRS Timeout when Retries were enabled.\n");
    if (npei_int_sum.s.c0_aeri)
        PRINT_ERROR("NPEI_INT_SUM[C0_AERI]: Advanced Error Reporting Interrupt, RC Mode Only.\n"
                    "    Pcie Core 0 (cfg_aer_rc_err_int).\n");
    if (npei_int_sum.s.ptime)
        PRINT_ERROR("NPEI_INT_SUM[PTIME]: Packet Timer has an interrupt. Which rings can\n"
                    "    be found in NPEI_PKT_TIME_INT.\n");
    if (npei_int_sum.s.pcnt)
        PRINT_ERROR("NPEI_INT_SUM[PCNT]: Packet Counter has an interrupt. Which rings can\n"
                    "    be found in NPEI_PKT_CNT_INT.\n");
    if (npei_int_sum.s.pidbof)
        PRINT_ERROR("NPEI_INT_SUM[PIDBOF]: Packet Instruction Doorbell count overflowed. Which\n"
                    "    doorbell can be found in NPEI_INT_INFO[PIDBOF]\n");
    if (npei_int_sum.s.psldbof)
        PRINT_ERROR("NPEI_INT_SUM[PSLDBOF]: Packet Scatterlist Doorbell count overflowed. Which\n"
                    "    doorbell can be found in NPEI_INT_INFO[PSLDBOF]\n");
    if (npei_int_sum.s.dtime1)
        PRINT_ERROR("NPEI_INT_SUM[DTIME1]: Whenever NPEI_DMA_CNTS[DMA1] is not 0, the\n"
                    "    DMA_CNT1 timer increments every core clock. When\n"
                    "    DMA_CNT1 timer exceeds NPEI_DMA1_INT_LEVEL[TIME],\n"
                    "    this bit is set. Writing a '1' to this bit also\n"
                    "    clears the DMA_CNT1 timer.\n");
    if (npei_int_sum.s.dtime0)
        PRINT_ERROR("NPEI_INT_SUM[DTIME0]: Whenever NPEI_DMA_CNTS[DMA0] is not 0, the\n"
                    "    DMA_CNT0 timer increments every core clock. When\n"
                    "    DMA_CNT0 timer exceeds NPEI_DMA0_INT_LEVEL[TIME],\n"
                    "    this bit is set. Writing a '1' to this bit also\n"
                    "    clears the DMA_CNT0 timer.\n");
    if (npei_int_sum.s.dcnt1)
        PRINT_ERROR("NPEI_INT_SUM[DCNT1]: This bit indicates that NPEI_DMA_CNTS[DMA1] was/is\n"
                    "    greater than NPEI_DMA1_INT_LEVEL[CNT].\n");
    if (npei_int_sum.s.dcnt0)
        PRINT_ERROR("NPEI_INT_SUM[DCNT0]: This bit indicates that NPEI_DMA_CNTS[DMA0] was/is\n"
                    "    greater than NPEI_DMA0_INT_LEVEL[CNT].\n");
    if (npei_int_sum.s.dma1fi)
        PRINT_ERROR("NPEI_INT_SUM[DMA1FI]: DMA0 set Forced Interrupt.\n");
    if (npei_int_sum.s.dma0fi)
        PRINT_ERROR("NPEI_INT_SUM[DMA0FI]: DMA0 set Forced Interrupt.\n");
    if (npei_int_sum.s.dma4dbo)
        PRINT_ERROR("NPEI_INT_SUM[DMA4DBO]: DMA4 doorbell overflow.\n"
                    "    Bit[32] of the doorbell count was set.\n");
    if (npei_int_sum.s.dma3dbo)
        PRINT_ERROR("NPEI_INT_SUM[DMA3DBO]: DMA3 doorbell overflow.\n"
                    "    Bit[32] of the doorbell count was set.\n");
    if (npei_int_sum.s.dma2dbo)
        PRINT_ERROR("NPEI_INT_SUM[DMA2DBO]: DMA2 doorbell overflow.\n"
                    "    Bit[32] of the doorbell count was set.\n");
    if (npei_int_sum.s.dma1dbo)
        PRINT_ERROR("NPEI_INT_SUM[DMA1DBO]: DMA1 doorbell overflow.\n"
                    "    Bit[32] of the doorbell count was set.\n");
    if (npei_int_sum.s.dma0dbo)
        PRINT_ERROR("NPEI_INT_SUM[DMA0DBO]: DMA0 doorbell overflow.\n"
                    "    Bit[32] of the doorbell count was set.\n");
    if (npei_int_sum.s.iob2big)
        PRINT_ERROR("NPEI_INT_SUM[IOB2BIG]: A requested IOBDMA is to large.\n");
    if (npei_int_sum.s.bar0_to)
        PRINT_ERROR("NPEI_INT_SUM[BAR0_TO]: BAR0 R/W to a NCB device did not receive\n"
                    "    read-data/commit in 0xffff core clocks.\n");
    if (npei_int_sum.s.rml_wto)
        PRINT_ERROR("NPEI_INT_SUM[RML_WTO]: RML write did not get commit in 0xffff core clocks.\n");
    if (npei_int_sum.s.rml_rto)
        PRINT_ERROR("NPEI_INT_SUM[RML_RTO]: RML read did not return data in 0xffff core clocks.\n");
}


/**
 * __cvmx_interrupt_npei_int_enb2_enable enables all interrupt bits in cvmx_npei_int_enb2_t
 */
void __cvmx_interrupt_npei_int_enb2_enable(void)
{
    int enable_pcie0 = 0;
    int enable_pcie1 = 0;
    cvmx_npei_int_enb2_t npei_int_enb2;
    /* Reset NPEI_INT_SUM, as NPEI_INT_SUM2 is a read-only copy of NPEI_INT_SUM. */
    cvmx_write_csr(CVMX_PEXP_NPEI_INT_SUM, cvmx_read_csr(CVMX_PEXP_NPEI_INT_SUM));
    npei_int_enb2.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        cvmx_pescx_ctl_status2_t pescx_ctl_status2;
        pescx_ctl_status2.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS2(0));
        enable_pcie0 = !pescx_ctl_status2.s.pcierst;
        pescx_ctl_status2.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS2(1));
        enable_pcie1 = !pescx_ctl_status2.s.pcierst;

        // Skipping npei_int_enb2.s.reserved_62_63
        npei_int_enb2.s.int_a = 1;
        npei_int_enb2.s.c1_ldwn = enable_pcie1;
        npei_int_enb2.s.c0_ldwn = enable_pcie0;
        npei_int_enb2.s.c1_exc = enable_pcie1;
        npei_int_enb2.s.c0_exc = enable_pcie0;
        npei_int_enb2.s.c1_up_wf = enable_pcie1;
        npei_int_enb2.s.c0_up_wf = enable_pcie0;
        npei_int_enb2.s.c1_un_wf = enable_pcie1;
        npei_int_enb2.s.c0_un_wf = enable_pcie0;
        npei_int_enb2.s.c1_un_bx = enable_pcie1;
        npei_int_enb2.s.c1_un_wi = enable_pcie1;
        npei_int_enb2.s.c1_un_b2 = enable_pcie1;
        npei_int_enb2.s.c1_un_b1 = enable_pcie1;
        npei_int_enb2.s.c1_un_b0 = enable_pcie1;
        npei_int_enb2.s.c1_up_bx = enable_pcie1;
        npei_int_enb2.s.c1_up_wi = enable_pcie1;
        npei_int_enb2.s.c1_up_b2 = enable_pcie1;
        npei_int_enb2.s.c1_up_b1 = enable_pcie1;
        npei_int_enb2.s.c1_up_b0 = enable_pcie1;
        npei_int_enb2.s.c0_un_bx = enable_pcie0;
        npei_int_enb2.s.c0_un_wi = enable_pcie0;
        npei_int_enb2.s.c0_un_b2 = enable_pcie0;
        npei_int_enb2.s.c0_un_b1 = enable_pcie0;
        npei_int_enb2.s.c0_un_b0 = enable_pcie0;
        npei_int_enb2.s.c0_up_bx = enable_pcie0;
        npei_int_enb2.s.c0_up_wi = enable_pcie0;
        npei_int_enb2.s.c0_up_b2 = enable_pcie0;
        npei_int_enb2.s.c0_up_b1 = enable_pcie0;
        npei_int_enb2.s.c0_up_b0 = enable_pcie0;
        npei_int_enb2.s.c1_hpint = enable_pcie1;
        npei_int_enb2.s.c1_pmei = enable_pcie1;
        npei_int_enb2.s.c1_wake = enable_pcie1;
        npei_int_enb2.s.crs1_dr = enable_pcie1;
        npei_int_enb2.s.c1_se = enable_pcie1;
        npei_int_enb2.s.crs1_er = enable_pcie1;
        npei_int_enb2.s.c1_aeri = enable_pcie1;
        npei_int_enb2.s.c0_hpint = enable_pcie0;
        npei_int_enb2.s.c0_pmei = enable_pcie0;
        npei_int_enb2.s.c0_wake = enable_pcie0;
        npei_int_enb2.s.crs0_dr = enable_pcie0;
        npei_int_enb2.s.c0_se = enable_pcie0;
        npei_int_enb2.s.crs0_er = enable_pcie0;
        npei_int_enb2.s.c0_aeri = enable_pcie0;
        npei_int_enb2.s.ptime = 1;
        npei_int_enb2.s.pcnt = 1;
        npei_int_enb2.s.pidbof = 1;
        npei_int_enb2.s.psldbof = 1;
        npei_int_enb2.s.dtime1 = 1;
        npei_int_enb2.s.dtime0 = 1;
        npei_int_enb2.s.dcnt1 = 1;
        npei_int_enb2.s.dcnt0 = 1;
        npei_int_enb2.s.dma1fi = 1;
        npei_int_enb2.s.dma0fi = 1;
        npei_int_enb2.s.dma4dbo = 1;
        npei_int_enb2.s.dma3dbo = 1;
        npei_int_enb2.s.dma2dbo = 1;
        npei_int_enb2.s.dma1dbo = 1;
        npei_int_enb2.s.dma0dbo = 1;
        npei_int_enb2.s.iob2big = 1;
        npei_int_enb2.s.bar0_to = 1;
        npei_int_enb2.s.rml_wto = 1;
        npei_int_enb2.s.rml_rto = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        cvmx_pescx_ctl_status2_t pescx_ctl_status2;
        cvmx_npei_dbg_data_t npei_dbg_data;
        pescx_ctl_status2.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS2(0));
        enable_pcie0 = !pescx_ctl_status2.s.pcierst;
        npei_dbg_data.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
        if (!npei_dbg_data.cn52xx.qlm0_link_width)
        {
            pescx_ctl_status2.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS2(1));
            enable_pcie1 = !pescx_ctl_status2.s.pcierst;
        }

        // Skipping npei_int_enb2.s.reserved_62_63
        npei_int_enb2.s.int_a = 1;
        npei_int_enb2.s.c1_ldwn = enable_pcie1;
        npei_int_enb2.s.c0_ldwn = enable_pcie0;
        npei_int_enb2.s.c1_exc = enable_pcie1;
        npei_int_enb2.s.c0_exc = enable_pcie0;
        npei_int_enb2.s.c1_up_wf = enable_pcie1;
        npei_int_enb2.s.c0_up_wf = enable_pcie0;
        npei_int_enb2.s.c1_un_wf = enable_pcie1;
        npei_int_enb2.s.c0_un_wf = enable_pcie0;
        npei_int_enb2.s.c1_un_bx = enable_pcie1;
        npei_int_enb2.s.c1_un_wi = enable_pcie1;
        npei_int_enb2.s.c1_un_b2 = enable_pcie1;
        npei_int_enb2.s.c1_un_b1 = enable_pcie1;
        npei_int_enb2.s.c1_un_b0 = enable_pcie1;
        npei_int_enb2.s.c1_up_bx = enable_pcie1;
        npei_int_enb2.s.c1_up_wi = enable_pcie1;
        npei_int_enb2.s.c1_up_b2 = enable_pcie1;
        npei_int_enb2.s.c1_up_b1 = enable_pcie1;
        npei_int_enb2.s.c1_up_b0 = enable_pcie1;
        npei_int_enb2.s.c0_un_bx = enable_pcie0;
        npei_int_enb2.s.c0_un_wi = enable_pcie0;
        npei_int_enb2.s.c0_un_b2 = enable_pcie0;
        npei_int_enb2.s.c0_un_b1 = enable_pcie0;
        npei_int_enb2.s.c0_un_b0 = enable_pcie0;
        npei_int_enb2.s.c0_up_bx = enable_pcie0;
        npei_int_enb2.s.c0_up_wi = enable_pcie0;
        npei_int_enb2.s.c0_up_b2 = enable_pcie0;
        npei_int_enb2.s.c0_up_b1 = enable_pcie0;
        npei_int_enb2.s.c0_up_b0 = enable_pcie0;
        npei_int_enb2.s.c1_hpint = enable_pcie1;
        npei_int_enb2.s.c1_pmei = enable_pcie1;
        npei_int_enb2.s.c1_wake = enable_pcie1;
        npei_int_enb2.s.crs1_dr = enable_pcie1;
        npei_int_enb2.s.c1_se = enable_pcie1;
        npei_int_enb2.s.crs1_er = enable_pcie1;
        npei_int_enb2.s.c1_aeri = enable_pcie1;
        npei_int_enb2.s.c0_hpint = enable_pcie0;
        npei_int_enb2.s.c0_pmei = enable_pcie0;
        npei_int_enb2.s.c0_wake = enable_pcie0;
        npei_int_enb2.s.crs0_dr = enable_pcie0;
        npei_int_enb2.s.c0_se = enable_pcie0;
        npei_int_enb2.s.crs0_er = enable_pcie0;
        npei_int_enb2.s.c0_aeri = enable_pcie0;
        npei_int_enb2.s.ptime = 1;
        npei_int_enb2.s.pcnt = 1;
        npei_int_enb2.s.pidbof = 1;
        npei_int_enb2.s.psldbof = 1;
        npei_int_enb2.s.dtime1 = 1;
        npei_int_enb2.s.dtime0 = 1;
        npei_int_enb2.s.dcnt1 = 1;
        npei_int_enb2.s.dcnt0 = 1;
        npei_int_enb2.s.dma1fi = 1;
        npei_int_enb2.s.dma0fi = 1;
        if (!OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X))
            npei_int_enb2.s.dma4dbo = 1;
        npei_int_enb2.s.dma3dbo = 1;
        npei_int_enb2.s.dma2dbo = 1;
        npei_int_enb2.s.dma1dbo = 1;
        npei_int_enb2.s.dma0dbo = 1;
        npei_int_enb2.s.iob2big = 1;
        npei_int_enb2.s.bar0_to = 1;
        npei_int_enb2.s.rml_wto = 1;
        npei_int_enb2.s.rml_rto = 1;
    }
    cvmx_write_csr(CVMX_PEXP_NPEI_INT_ENB2, npei_int_enb2.u64);
    if (enable_pcie0)
        __cvmx_interrupt_pescx_dbg_info_en_enable(0);
    if (enable_pcie1)
        __cvmx_interrupt_pescx_dbg_info_en_enable(1);
}


/**
 * __cvmx_interrupt_npi_int_enb_enable enables all interrupt bits in cvmx_npi_int_enb_t
 */
void __cvmx_interrupt_npi_int_enb_enable(void)
{
    cvmx_npi_int_enb_t npi_int_enb;
    cvmx_write_csr(CVMX_NPI_INT_SUM, cvmx_read_csr(CVMX_NPI_INT_SUM));
    npi_int_enb.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        // Skipping npi_int_enb.s.reserved_62_63
        npi_int_enb.s.q1_a_f = 1;
        npi_int_enb.s.q1_s_e = 1;
        npi_int_enb.s.pdf_p_f = 1;
        npi_int_enb.s.pdf_p_e = 1;
        npi_int_enb.s.pcf_p_f = 1;
        npi_int_enb.s.pcf_p_e = 1;
        npi_int_enb.s.rdx_s_e = 1;
        npi_int_enb.s.rwx_s_e = 1;
        npi_int_enb.s.pnc_a_f = 1;
        npi_int_enb.s.pnc_s_e = 1;
        npi_int_enb.s.com_a_f = 1;
        npi_int_enb.s.com_s_e = 1;
        npi_int_enb.s.q3_a_f = 1;
        npi_int_enb.s.q3_s_e = 1;
        npi_int_enb.s.q2_a_f = 1;
        npi_int_enb.s.q2_s_e = 1;
        npi_int_enb.s.pcr_a_f = 1;
        npi_int_enb.s.pcr_s_e = 1;
        npi_int_enb.s.fcr_a_f = 1;
        npi_int_enb.s.fcr_s_e = 1;
        npi_int_enb.s.iobdma = 1;
        npi_int_enb.s.p_dperr = 1;
        npi_int_enb.s.win_rto = 1;
        // Skipping npi_int_enb.s.reserved_36_38
        npi_int_enb.s.i0_pperr = 1;
        // Skipping npi_int_enb.s.reserved_32_34
        npi_int_enb.s.p0_ptout = 1;
        // Skipping npi_int_enb.s.reserved_28_30
        npi_int_enb.s.p0_pperr = 1;
        // Skipping npi_int_enb.s.reserved_24_26
        npi_int_enb.s.g0_rtout = 1;
        // Skipping npi_int_enb.s.reserved_20_22
        npi_int_enb.s.p0_perr = 1;
        // Skipping npi_int_enb.s.reserved_16_18
        npi_int_enb.s.p0_rtout = 1;
        // Skipping npi_int_enb.s.reserved_12_14
        npi_int_enb.s.i0_overf = 1;
        // Skipping npi_int_enb.s.reserved_8_10
        npi_int_enb.s.i0_rtout = 1;
        // Skipping npi_int_enb.s.reserved_4_6
        npi_int_enb.s.po0_2sml = 1;
        npi_int_enb.s.pci_rsl = 1;
        npi_int_enb.s.rml_wto = 1;
        npi_int_enb.s.rml_rto = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        // Skipping npi_int_enb.s.reserved_62_63
        npi_int_enb.s.q1_a_f = 1;
        npi_int_enb.s.q1_s_e = 1;
        npi_int_enb.s.pdf_p_f = 1;
        npi_int_enb.s.pdf_p_e = 1;
        npi_int_enb.s.pcf_p_f = 1;
        npi_int_enb.s.pcf_p_e = 1;
        npi_int_enb.s.rdx_s_e = 1;
        npi_int_enb.s.rwx_s_e = 1;
        npi_int_enb.s.pnc_a_f = 1;
        npi_int_enb.s.pnc_s_e = 1;
        npi_int_enb.s.com_a_f = 1;
        npi_int_enb.s.com_s_e = 1;
        npi_int_enb.s.q3_a_f = 1;
        npi_int_enb.s.q3_s_e = 1;
        npi_int_enb.s.q2_a_f = 1;
        npi_int_enb.s.q2_s_e = 1;
        npi_int_enb.s.pcr_a_f = 1;
        npi_int_enb.s.pcr_s_e = 1;
        npi_int_enb.s.fcr_a_f = 1;
        npi_int_enb.s.fcr_s_e = 1;
        npi_int_enb.s.iobdma = 1;
        npi_int_enb.s.p_dperr = 1;
        npi_int_enb.s.win_rto = 1;
        // Skipping npi_int_enb.s.reserved_37_38
        npi_int_enb.s.i1_pperr = 1;
        npi_int_enb.s.i0_pperr = 1;
        // Skipping npi_int_enb.s.reserved_33_34
        npi_int_enb.s.p1_ptout = 1;
        npi_int_enb.s.p0_ptout = 1;
        // Skipping npi_int_enb.s.reserved_29_30
        npi_int_enb.s.p1_pperr = 1;
        npi_int_enb.s.p0_pperr = 1;
        // Skipping npi_int_enb.s.reserved_25_26
        npi_int_enb.s.g1_rtout = 1;
        npi_int_enb.s.g0_rtout = 1;
        // Skipping npi_int_enb.s.reserved_21_22
        npi_int_enb.s.p1_perr = 1;
        npi_int_enb.s.p0_perr = 1;
        // Skipping npi_int_enb.s.reserved_17_18
        npi_int_enb.s.p1_rtout = 1;
        npi_int_enb.s.p0_rtout = 1;
        // Skipping npi_int_enb.s.reserved_13_14
        npi_int_enb.s.i1_overf = 1;
        npi_int_enb.s.i0_overf = 1;
        // Skipping npi_int_enb.s.reserved_9_10
        npi_int_enb.s.i1_rtout = 1;
        npi_int_enb.s.i0_rtout = 1;
        // Skipping npi_int_enb.s.reserved_5_6
        npi_int_enb.s.po1_2sml = 1;
        npi_int_enb.s.po0_2sml = 1;
        npi_int_enb.s.pci_rsl = 1;
        npi_int_enb.s.rml_wto = 1;
        npi_int_enb.s.rml_rto = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping npi_int_enb.s.reserved_62_63
        npi_int_enb.s.q1_a_f = 1;
        npi_int_enb.s.q1_s_e = 1;
        npi_int_enb.s.pdf_p_f = 1;
        npi_int_enb.s.pdf_p_e = 1;
        npi_int_enb.s.pcf_p_f = 1;
        npi_int_enb.s.pcf_p_e = 1;
        npi_int_enb.s.rdx_s_e = 1;
        npi_int_enb.s.rwx_s_e = 1;
        npi_int_enb.s.pnc_a_f = 1;
        npi_int_enb.s.pnc_s_e = 1;
        npi_int_enb.s.com_a_f = 1;
        npi_int_enb.s.com_s_e = 1;
        npi_int_enb.s.q3_a_f = 1;
        npi_int_enb.s.q3_s_e = 1;
        npi_int_enb.s.q2_a_f = 1;
        npi_int_enb.s.q2_s_e = 1;
        npi_int_enb.s.pcr_a_f = 1;
        npi_int_enb.s.pcr_s_e = 1;
        npi_int_enb.s.fcr_a_f = 1;
        npi_int_enb.s.fcr_s_e = 1;
        npi_int_enb.s.iobdma = 1;
        npi_int_enb.s.p_dperr = 1;
        npi_int_enb.s.win_rto = 1;
        npi_int_enb.s.i3_pperr = 1;
        npi_int_enb.s.i2_pperr = 1;
        npi_int_enb.s.i1_pperr = 1;
        npi_int_enb.s.i0_pperr = 1;
        npi_int_enb.s.p3_ptout = 1;
        npi_int_enb.s.p2_ptout = 1;
        npi_int_enb.s.p1_ptout = 1;
        npi_int_enb.s.p0_ptout = 1;
        npi_int_enb.s.p3_pperr = 1;
        npi_int_enb.s.p2_pperr = 1;
        npi_int_enb.s.p1_pperr = 1;
        npi_int_enb.s.p0_pperr = 1;
        npi_int_enb.s.g3_rtout = 1;
        npi_int_enb.s.g2_rtout = 1;
        npi_int_enb.s.g1_rtout = 1;
        npi_int_enb.s.g0_rtout = 1;
        npi_int_enb.s.p3_perr = 1;
        npi_int_enb.s.p2_perr = 1;
        npi_int_enb.s.p1_perr = 1;
        npi_int_enb.s.p0_perr = 1;
        npi_int_enb.s.p3_rtout = 1;
        npi_int_enb.s.p2_rtout = 1;
        npi_int_enb.s.p1_rtout = 1;
        npi_int_enb.s.p0_rtout = 1;
        npi_int_enb.s.i3_overf = 1;
        npi_int_enb.s.i2_overf = 1;
        npi_int_enb.s.i1_overf = 1;
        npi_int_enb.s.i0_overf = 1;
        npi_int_enb.s.i3_rtout = 1;
        npi_int_enb.s.i2_rtout = 1;
        npi_int_enb.s.i1_rtout = 1;
        npi_int_enb.s.i0_rtout = 1;
        npi_int_enb.s.po3_2sml = 1;
        npi_int_enb.s.po2_2sml = 1;
        npi_int_enb.s.po1_2sml = 1;
        npi_int_enb.s.po0_2sml = 1;
        npi_int_enb.s.pci_rsl = 1;
        npi_int_enb.s.rml_wto = 1;
        npi_int_enb.s.rml_rto = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        // Skipping npi_int_enb.s.reserved_62_63
        npi_int_enb.s.q1_a_f = 1;
        npi_int_enb.s.q1_s_e = 1;
        npi_int_enb.s.pdf_p_f = 1;
        npi_int_enb.s.pdf_p_e = 1;
        npi_int_enb.s.pcf_p_f = 1;
        npi_int_enb.s.pcf_p_e = 1;
        npi_int_enb.s.rdx_s_e = 1;
        npi_int_enb.s.rwx_s_e = 1;
        npi_int_enb.s.pnc_a_f = 1;
        npi_int_enb.s.pnc_s_e = 1;
        npi_int_enb.s.com_a_f = 1;
        npi_int_enb.s.com_s_e = 1;
        npi_int_enb.s.q3_a_f = 1;
        npi_int_enb.s.q3_s_e = 1;
        npi_int_enb.s.q2_a_f = 1;
        npi_int_enb.s.q2_s_e = 1;
        npi_int_enb.s.pcr_a_f = 1;
        npi_int_enb.s.pcr_s_e = 1;
        npi_int_enb.s.fcr_a_f = 1;
        npi_int_enb.s.fcr_s_e = 1;
        npi_int_enb.s.iobdma = 1;
        npi_int_enb.s.p_dperr = 1;
        npi_int_enb.s.win_rto = 1;
        // Skipping npi_int_enb.s.reserved_37_38
        npi_int_enb.s.i1_pperr = 1;
        npi_int_enb.s.i0_pperr = 1;
        // Skipping npi_int_enb.s.reserved_33_34
        npi_int_enb.s.p1_ptout = 1;
        npi_int_enb.s.p0_ptout = 1;
        // Skipping npi_int_enb.s.reserved_29_30
        npi_int_enb.s.p1_pperr = 1;
        npi_int_enb.s.p0_pperr = 1;
        // Skipping npi_int_enb.s.reserved_25_26
        npi_int_enb.s.g1_rtout = 1;
        npi_int_enb.s.g0_rtout = 1;
        // Skipping npi_int_enb.s.reserved_21_22
        npi_int_enb.s.p1_perr = 1;
        npi_int_enb.s.p0_perr = 1;
        // Skipping npi_int_enb.s.reserved_17_18
        npi_int_enb.s.p1_rtout = 1;
        npi_int_enb.s.p0_rtout = 1;
        // Skipping npi_int_enb.s.reserved_13_14
        npi_int_enb.s.i1_overf = 1;
        npi_int_enb.s.i0_overf = 1;
        // Skipping npi_int_enb.s.reserved_9_10
        npi_int_enb.s.i1_rtout = 1;
        npi_int_enb.s.i0_rtout = 1;
        // Skipping npi_int_enb.s.reserved_5_6
        npi_int_enb.s.po1_2sml = 1;
        npi_int_enb.s.po0_2sml = 1;
        npi_int_enb.s.pci_rsl = 1;
        npi_int_enb.s.rml_wto = 1;
        npi_int_enb.s.rml_rto = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping npi_int_enb.s.reserved_62_63
        npi_int_enb.s.q1_a_f = 1;
        npi_int_enb.s.q1_s_e = 1;
        npi_int_enb.s.pdf_p_f = 1;
        npi_int_enb.s.pdf_p_e = 1;
        npi_int_enb.s.pcf_p_f = 1;
        npi_int_enb.s.pcf_p_e = 1;
        npi_int_enb.s.rdx_s_e = 1;
        npi_int_enb.s.rwx_s_e = 1;
        npi_int_enb.s.pnc_a_f = 1;
        npi_int_enb.s.pnc_s_e = 1;
        npi_int_enb.s.com_a_f = 1;
        npi_int_enb.s.com_s_e = 1;
        npi_int_enb.s.q3_a_f = 1;
        npi_int_enb.s.q3_s_e = 1;
        npi_int_enb.s.q2_a_f = 1;
        npi_int_enb.s.q2_s_e = 1;
        npi_int_enb.s.pcr_a_f = 1;
        npi_int_enb.s.pcr_s_e = 1;
        npi_int_enb.s.fcr_a_f = 1;
        npi_int_enb.s.fcr_s_e = 1;
        npi_int_enb.s.iobdma = 1;
        npi_int_enb.s.p_dperr = 1;
        npi_int_enb.s.win_rto = 1;
        npi_int_enb.s.i3_pperr = 1;
        npi_int_enb.s.i2_pperr = 1;
        npi_int_enb.s.i1_pperr = 1;
        npi_int_enb.s.i0_pperr = 1;
        npi_int_enb.s.p3_ptout = 1;
        npi_int_enb.s.p2_ptout = 1;
        npi_int_enb.s.p1_ptout = 1;
        npi_int_enb.s.p0_ptout = 1;
        npi_int_enb.s.p3_pperr = 1;
        npi_int_enb.s.p2_pperr = 1;
        npi_int_enb.s.p1_pperr = 1;
        npi_int_enb.s.p0_pperr = 1;
        npi_int_enb.s.g3_rtout = 1;
        npi_int_enb.s.g2_rtout = 1;
        npi_int_enb.s.g1_rtout = 1;
        npi_int_enb.s.g0_rtout = 1;
        npi_int_enb.s.p3_perr = 1;
        npi_int_enb.s.p2_perr = 1;
        npi_int_enb.s.p1_perr = 1;
        npi_int_enb.s.p0_perr = 1;
        npi_int_enb.s.p3_rtout = 1;
        npi_int_enb.s.p2_rtout = 1;
        npi_int_enb.s.p1_rtout = 1;
        npi_int_enb.s.p0_rtout = 1;
        npi_int_enb.s.i3_overf = 1;
        npi_int_enb.s.i2_overf = 1;
        npi_int_enb.s.i1_overf = 1;
        npi_int_enb.s.i0_overf = 1;
        npi_int_enb.s.i3_rtout = 1;
        npi_int_enb.s.i2_rtout = 1;
        npi_int_enb.s.i1_rtout = 1;
        npi_int_enb.s.i0_rtout = 1;
        npi_int_enb.s.po3_2sml = 1;
        npi_int_enb.s.po2_2sml = 1;
        npi_int_enb.s.po1_2sml = 1;
        npi_int_enb.s.po0_2sml = 1;
        npi_int_enb.s.pci_rsl = 1;
        npi_int_enb.s.rml_wto = 1;
        npi_int_enb.s.rml_rto = 1;
    }
    cvmx_write_csr(CVMX_NPI_INT_ENB, npi_int_enb.u64);
    __cvmx_interrupt_pci_int_enb2_enable();
}


/**
 * __cvmx_interrupt_npi_int_sum_decode decodes all interrupt bits in cvmx_npi_int_sum_t
 */
void __cvmx_interrupt_npi_int_sum_decode(void)
{
    cvmx_npi_int_sum_t npi_int_sum;
    npi_int_sum.u64 = cvmx_read_csr(CVMX_NPI_INT_SUM);
    npi_int_sum.u64 &= cvmx_read_csr(CVMX_NPI_INT_ENB);
    cvmx_write_csr(CVMX_NPI_INT_SUM, npi_int_sum.u64);
    // Skipping npi_int_sum.s.reserved_62_63
    if (npi_int_sum.s.q1_a_f)
        PRINT_ERROR("NPI_INT_SUM[Q1_A_F]: Attempted to add when Queue-1 FIFO is full.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.q1_s_e)
        PRINT_ERROR("NPI_INT_SUM[Q1_S_E]: Attempted to subtract when Queue-1 FIFO is empty.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.pdf_p_f)
        PRINT_ERROR("NPI_INT_SUM[PDF_P_F]: Attempted to push a full PCN-DATA-FIFO.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.pdf_p_e)
        PRINT_ERROR("NPI_INT_SUM[PDF_P_E]: Attempted to pop an empty PCN-DATA-FIFO.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.pcf_p_f)
        PRINT_ERROR("NPI_INT_SUM[PCF_P_F]: Attempted to push a full PCN-CNT-FIFO.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.pcf_p_e)
        PRINT_ERROR("NPI_INT_SUM[PCF_P_E]: Attempted to pop an empty PCN-CNT-FIFO.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.rdx_s_e)
        PRINT_ERROR("NPI_INT_SUM[RDX_S_E]: Attempted to subtract when DPI-XFR-Wait count is 0.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.rwx_s_e)
        PRINT_ERROR("NPI_INT_SUM[RWX_S_E]: Attempted to subtract when RDN-XFR-Wait count is 0.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.pnc_a_f)
        PRINT_ERROR("NPI_INT_SUM[PNC_A_F]: Attempted to add when PNI-NPI Credits are max.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.pnc_s_e)
        PRINT_ERROR("NPI_INT_SUM[PNC_S_E]: Attempted to subtract when PNI-NPI Credits are 0.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.com_a_f)
        PRINT_ERROR("NPI_INT_SUM[COM_A_F]: Attempted to add when PCN-Commit Counter is max.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.com_s_e)
        PRINT_ERROR("NPI_INT_SUM[COM_S_E]: Attempted to subtract when PCN-Commit Counter is 0.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.q3_a_f)
        PRINT_ERROR("NPI_INT_SUM[Q3_A_F]: Attempted to add when Queue-3 FIFO is full.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.q3_s_e)
        PRINT_ERROR("NPI_INT_SUM[Q3_S_E]: Attempted to subtract when Queue-3 FIFO is empty.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.q2_a_f)
        PRINT_ERROR("NPI_INT_SUM[Q2_A_F]: Attempted to add when Queue-2 FIFO is full.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.q2_s_e)
        PRINT_ERROR("NPI_INT_SUM[Q2_S_E]: Attempted to subtract when Queue-2 FIFO is empty.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.pcr_a_f)
        PRINT_ERROR("NPI_INT_SUM[PCR_A_F]: Attempted to add when POW Credits is full.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.pcr_s_e)
        PRINT_ERROR("NPI_INT_SUM[PCR_S_E]: Attempted to subtract when POW Credits is empty.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.fcr_a_f)
        PRINT_ERROR("NPI_INT_SUM[FCR_A_F]: Attempted to add when FPA Credits is full.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.fcr_s_e)
        PRINT_ERROR("NPI_INT_SUM[FCR_S_E]: Attempted to subtract when FPA Credits is empty.\n"
                    "    PASS3 Field.\n");
    if (npi_int_sum.s.iobdma)
        PRINT_ERROR("NPI_INT_SUM[IOBDMA]: Requested IOBDMA read size exceeded 128 words.\n");
    if (npi_int_sum.s.p_dperr)
        PRINT_ERROR("NPI_INT_SUM[P_DPERR]: If a parity error occured on data written to L2C\n"
                    "    from the PCI this bit may be set.\n");
    if (npi_int_sum.s.win_rto)
        PRINT_ERROR("NPI_INT_SUM[WIN_RTO]: Windowed Load Timed Out.\n");
    if (npi_int_sum.s.i3_pperr)
        PRINT_ERROR("NPI_INT_SUM[I3_PPERR]: If a parity error occured on the port's instruction\n"
                    "    this bit may be set.\n");
    if (npi_int_sum.s.i2_pperr)
        PRINT_ERROR("NPI_INT_SUM[I2_PPERR]: If a parity error occured on the port's instruction\n"
                    "    this bit may be set.\n");
    if (npi_int_sum.s.i1_pperr)
        PRINT_ERROR("NPI_INT_SUM[I1_PPERR]: If a parity error occured on the port's instruction\n"
                    "    this bit may be set.\n");
    if (npi_int_sum.s.i0_pperr)
        PRINT_ERROR("NPI_INT_SUM[I0_PPERR]: If a parity error occured on the port's instruction\n"
                    "    this bit may be set.\n");
    if (npi_int_sum.s.p3_ptout)
        PRINT_ERROR("NPI_INT_SUM[P3_PTOUT]: Port-3 output had a read timeout on a DATA/INFO\n"
                    "    pair.\n");
    if (npi_int_sum.s.p2_ptout)
        PRINT_ERROR("NPI_INT_SUM[P2_PTOUT]: Port-2 output had a read timeout on a DATA/INFO\n"
                    "    pair.\n");
    if (npi_int_sum.s.p1_ptout)
        PRINT_ERROR("NPI_INT_SUM[P1_PTOUT]: Port-1 output had a read timeout on a DATA/INFO\n"
                    "    pair.\n");
    if (npi_int_sum.s.p0_ptout)
        PRINT_ERROR("NPI_INT_SUM[P0_PTOUT]: Port-0 output had a read timeout on a DATA/INFO\n"
                    "    pair.\n");
    if (npi_int_sum.s.p3_pperr)
        PRINT_ERROR("NPI_INT_SUM[P3_PPERR]: If a parity error occured on the port DATA/INFO\n"
                    "    pointer-pair, this bit may be set.\n");
    if (npi_int_sum.s.p2_pperr)
        PRINT_ERROR("NPI_INT_SUM[P2_PPERR]: If a parity error occured on the port DATA/INFO\n"
                    "    pointer-pair, this bit may be set.\n");
    if (npi_int_sum.s.p1_pperr)
        PRINT_ERROR("NPI_INT_SUM[P1_PPERR]: If a parity error occured on the port DATA/INFO\n"
                    "    pointer-pair, this bit may be set.\n");
    if (npi_int_sum.s.p0_pperr)
        PRINT_ERROR("NPI_INT_SUM[P0_PPERR]: If a parity error occured on the port DATA/INFO\n"
                    "    pointer-pair, this bit may be set.\n");
    if (npi_int_sum.s.g3_rtout)
        PRINT_ERROR("NPI_INT_SUM[G3_RTOUT]: Port-3 had a read timeout while attempting to\n"
                    "    read a gather list.\n");
    if (npi_int_sum.s.g2_rtout)
        PRINT_ERROR("NPI_INT_SUM[G2_RTOUT]: Port-2 had a read timeout while attempting to\n"
                    "    read a gather list.\n");
    if (npi_int_sum.s.g1_rtout)
        PRINT_ERROR("NPI_INT_SUM[G1_RTOUT]: Port-1 had a read timeout while attempting to\n"
                    "    read a gather list.\n");
    if (npi_int_sum.s.g0_rtout)
        PRINT_ERROR("NPI_INT_SUM[G0_RTOUT]: Port-0 had a read timeout while attempting to\n"
                    "    read a gather list.\n");
    if (npi_int_sum.s.p3_perr)
        PRINT_ERROR("NPI_INT_SUM[P3_PERR]: If a parity error occured on the port's packet\n"
                    "    data this bit may be set.\n");
    if (npi_int_sum.s.p2_perr)
        PRINT_ERROR("NPI_INT_SUM[P2_PERR]: If a parity error occured on the port's packet\n"
                    "    data this bit may be set.\n");
    if (npi_int_sum.s.p1_perr)
        PRINT_ERROR("NPI_INT_SUM[P1_PERR]: If a parity error occured on the port's packet\n"
                    "    data this bit may be set.\n");
    if (npi_int_sum.s.p0_perr)
        PRINT_ERROR("NPI_INT_SUM[P0_PERR]: If a parity error occured on the port's packet\n"
                    "    data this bit may be set.\n");
    if (npi_int_sum.s.p3_rtout)
        PRINT_ERROR("NPI_INT_SUM[P3_RTOUT]: Port-3 had a read timeout while attempting to\n"
                    "    read packet data.\n");
    if (npi_int_sum.s.p2_rtout)
        PRINT_ERROR("NPI_INT_SUM[P2_RTOUT]: Port-2 had a read timeout while attempting to\n"
                    "    read packet data.\n");
    if (npi_int_sum.s.p1_rtout)
        PRINT_ERROR("NPI_INT_SUM[P1_RTOUT]: Port-1 had a read timeout while attempting to\n"
                    "    read packet data.\n");
    if (npi_int_sum.s.p0_rtout)
        PRINT_ERROR("NPI_INT_SUM[P0_RTOUT]: Port-0 had a read timeout while attempting to\n"
                    "    read packet data.\n");
    if (npi_int_sum.s.i3_overf)
        PRINT_ERROR("NPI_INT_SUM[I3_OVERF]: Port-3 had a doorbell overflow. Bit[31] of the\n"
                    "    doorbell count was set.\n");
    if (npi_int_sum.s.i2_overf)
        PRINT_ERROR("NPI_INT_SUM[I2_OVERF]: Port-2 had a doorbell overflow. Bit[31] of the\n"
                    "    doorbell count was set.\n");
    if (npi_int_sum.s.i1_overf)
        PRINT_ERROR("NPI_INT_SUM[I1_OVERF]: Port-1 had a doorbell overflow. Bit[31] of the\n"
                    "    doorbell count was set.\n");
    if (npi_int_sum.s.i0_overf)
        PRINT_ERROR("NPI_INT_SUM[I0_OVERF]: Port-0 had a doorbell overflow. Bit[31] of the\n"
                    "    doorbell count was set.\n");
    if (npi_int_sum.s.i3_rtout)
        PRINT_ERROR("NPI_INT_SUM[I3_RTOUT]: Port-3 had a read timeout while attempting to\n"
                    "    read instructions.\n");
    if (npi_int_sum.s.i2_rtout)
        PRINT_ERROR("NPI_INT_SUM[I2_RTOUT]: Port-2 had a read timeout while attempting to\n"
                    "    read instructions.\n");
    if (npi_int_sum.s.i1_rtout)
        PRINT_ERROR("NPI_INT_SUM[I1_RTOUT]: Port-1 had a read timeout while attempting to\n"
                    "    read instructions.\n");
    if (npi_int_sum.s.i0_rtout)
        PRINT_ERROR("NPI_INT_SUM[I0_RTOUT]: Port-0 had a read timeout while attempting to\n"
                    "    read instructions.\n");
    if (npi_int_sum.s.po3_2sml)
        PRINT_ERROR("NPI_INT_SUM[PO3_2SML]: The packet being sent out on Port3 is smaller\n"
                    "    than the NPI_BUFF_SIZE_OUTPUT3[ISIZE] field.\n");
    if (npi_int_sum.s.po2_2sml)
        PRINT_ERROR("NPI_INT_SUM[PO2_2SML]: The packet being sent out on Port2 is smaller\n"
                    "    than the NPI_BUFF_SIZE_OUTPUT2[ISIZE] field.\n");
    if (npi_int_sum.s.po1_2sml)
        PRINT_ERROR("NPI_INT_SUM[PO1_2SML]: The packet being sent out on Port1 is smaller\n"
                    "    than the NPI_BUFF_SIZE_OUTPUT1[ISIZE] field.\n");
    if (npi_int_sum.s.po0_2sml)
        PRINT_ERROR("NPI_INT_SUM[PO0_2SML]: The packet being sent out on Port0 is smaller\n"
                    "    than the NPI_BUFF_SIZE_OUTPUT0[ISIZE] field.\n");
    if (npi_int_sum.s.pci_rsl)
    {
#if 0
        PRINT_ERROR("NPI_INT_SUM[PCI_RSL]: This '1' when a bit in PCI_INT_SUM2 is SET and the\n"
                    "    corresponding bit in the PCI_INT_ENB2 is SET.\n");
#endif
        __cvmx_interrupt_pci_int_sum2_decode();
    }
    if (npi_int_sum.s.rml_wto)
        PRINT_ERROR("NPI_INT_SUM[RML_WTO]: Set '1' when the RML does not receive a commit\n"
                    "    back from a RSL after sending a write command to\n"
                    "    a RSL.\n");
    if (npi_int_sum.s.rml_rto)
        PRINT_ERROR("NPI_INT_SUM[RML_RTO]: Set '1' when the RML does not receive read data\n"
                    "    back from a RSL after sending a read command to\n"
                    "    a RSL.\n");
}


/**
 * __cvmx_interrupt_pci_int_enb2_enable enables all interrupt bits in cvmx_pci_int_enb2_t
 */
void __cvmx_interrupt_pci_int_enb2_enable(void)
{
    cvmx_pci_int_enb2_t pci_int_enb2;
    cvmx_write_csr(CVMX_NPI_PCI_INT_SUM2, cvmx_read_csr(CVMX_NPI_PCI_INT_SUM2));
    pci_int_enb2.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        // Skipping pci_int_enb2.s.reserved_34_63
        pci_int_enb2.s.ill_rd = 1;
        pci_int_enb2.s.ill_wr = 1;
        pci_int_enb2.s.win_wr = 1;
        // pci_int_enb2.s.dma1_fi = 1; // Not an error condition
        // pci_int_enb2.s.dma0_fi = 1; // Not an error condition
        // pci_int_enb2.s.rdtime1 = 1; // Not an error condition
        // pci_int_enb2.s.rdtime0 = 1; // Not an error condition
        // pci_int_enb2.s.rdcnt1 = 1; // Not an error condition
        // pci_int_enb2.s.rdcnt0 = 1; // Not an error condition
        // Skipping pci_int_enb2.s.reserved_22_24
        // pci_int_enb2.s.rptime0 = 1; // Not an error condition
        // Skipping pci_int_enb2.s.reserved_18_20
        // pci_int_enb2.s.rpcnt0 = 1; // Not an error condition
        // pci_int_enb2.s.rrsl_int = 1; // Not an error condition
        pci_int_enb2.s.ill_rrd = 1;
        pci_int_enb2.s.ill_rwr = 1;
        pci_int_enb2.s.rdperr = 1;
        pci_int_enb2.s.raperr = 1;
        pci_int_enb2.s.rserr = 1;
        pci_int_enb2.s.rtsr_abt = 1;
        pci_int_enb2.s.rmsc_msg = 1;
        pci_int_enb2.s.rmsi_mabt = 1;
        pci_int_enb2.s.rmsi_tabt = 1;
        pci_int_enb2.s.rmsi_per = 1;
        pci_int_enb2.s.rmr_tto = 1;
        pci_int_enb2.s.rmr_abt = 1;
        pci_int_enb2.s.rtr_abt = 1;
        pci_int_enb2.s.rmr_wtto = 1;
        pci_int_enb2.s.rmr_wabt = 1;
        pci_int_enb2.s.rtr_wabt = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        // Skipping pci_int_enb2.s.reserved_34_63
        pci_int_enb2.s.ill_rd = 1;
        pci_int_enb2.s.ill_wr = 1;
        pci_int_enb2.s.win_wr = 1;
        // pci_int_enb2.s.dma1_fi = 1; // Not an error condition
        // pci_int_enb2.s.dma0_fi = 1; // Not an error condition
        // pci_int_enb2.s.rdtime1 = 1; // Not an error condition
        // pci_int_enb2.s.rdtime0 = 1; // Not an error condition
        // pci_int_enb2.s.rdcnt1 = 1; // Not an error condition
        // pci_int_enb2.s.rdcnt0 = 1; // Not an error condition
        // Skipping pci_int_enb2.s.reserved_23_24
        // pci_int_enb2.s.rptime1 = 1; // Not an error condition
        // pci_int_enb2.s.rptime0 = 1; // Not an error condition
        // Skipping pci_int_enb2.s.reserved_19_20
        // pci_int_enb2.s.rpcnt1 = 1; // Not an error condition
        // pci_int_enb2.s.rpcnt0 = 1; // Not an error condition
        // pci_int_enb2.s.rrsl_int = 1; // Not an error condition
        pci_int_enb2.s.ill_rrd = 1;
        pci_int_enb2.s.ill_rwr = 1;
        pci_int_enb2.s.rdperr = 1;
        pci_int_enb2.s.raperr = 1;
        pci_int_enb2.s.rserr = 1;
        pci_int_enb2.s.rtsr_abt = 1;
        pci_int_enb2.s.rmsc_msg = 1;
        pci_int_enb2.s.rmsi_mabt = 1;
        pci_int_enb2.s.rmsi_tabt = 1;
        pci_int_enb2.s.rmsi_per = 1;
        pci_int_enb2.s.rmr_tto = 1;
        pci_int_enb2.s.rmr_abt = 1;
        pci_int_enb2.s.rtr_abt = 1;
        pci_int_enb2.s.rmr_wtto = 1;
        pci_int_enb2.s.rmr_wabt = 1;
        pci_int_enb2.s.rtr_wabt = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping pci_int_enb2.s.reserved_34_63
        pci_int_enb2.s.ill_rd = 1;
        pci_int_enb2.s.ill_wr = 1;
        pci_int_enb2.s.win_wr = 1;
        // pci_int_enb2.s.dma1_fi = 1; // Not an error condition
        // pci_int_enb2.s.dma0_fi = 1; // Not an error condition
        // pci_int_enb2.s.rdtime1 = 1; // Not an error condition
        // pci_int_enb2.s.rdtime0 = 1; // Not an error condition
        // pci_int_enb2.s.rdcnt1 = 1; // Not an error condition
        // pci_int_enb2.s.rdcnt0 = 1; // Not an error condition
        // pci_int_enb2.s.rptime3 = 1; // Not an error condition
        // pci_int_enb2.s.rptime2 = 1; // Not an error condition
        // pci_int_enb2.s.rptime1 = 1; // Not an error condition
        // pci_int_enb2.s.rptime0 = 1; // Not an error condition
        // pci_int_enb2.s.rpcnt3 = 1; // Not an error condition
        // pci_int_enb2.s.rpcnt2 = 1; // Not an error condition
        // pci_int_enb2.s.rpcnt1 = 1; // Not an error condition
        // pci_int_enb2.s.rpcnt0 = 1; // Not an error condition
        // pci_int_enb2.s.rrsl_int = 1; // Not an error condition
        pci_int_enb2.s.ill_rrd = 1;
        pci_int_enb2.s.ill_rwr = 1;
        pci_int_enb2.s.rdperr = 1;
        pci_int_enb2.s.raperr = 1;
        pci_int_enb2.s.rserr = 1;
        pci_int_enb2.s.rtsr_abt = 1;
        pci_int_enb2.s.rmsc_msg = 1;
        pci_int_enb2.s.rmsi_mabt = 1;
        pci_int_enb2.s.rmsi_tabt = 1;
        pci_int_enb2.s.rmsi_per = 1;
        pci_int_enb2.s.rmr_tto = 1;
        pci_int_enb2.s.rmr_abt = 1;
        pci_int_enb2.s.rtr_abt = 1;
        pci_int_enb2.s.rmr_wtto = 1;
        pci_int_enb2.s.rmr_wabt = 1;
        pci_int_enb2.s.rtr_wabt = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        // Skipping pci_int_enb2.s.reserved_34_63
        pci_int_enb2.s.ill_rd = 1;
        pci_int_enb2.s.ill_wr = 1;
        pci_int_enb2.s.win_wr = 1;
        // pci_int_enb2.s.dma1_fi = 1; // Not an error condition
        // pci_int_enb2.s.dma0_fi = 1; // Not an error condition
        // pci_int_enb2.s.rdtime1 = 1; // Not an error condition
        // pci_int_enb2.s.rdtime0 = 1; // Not an error condition
        // pci_int_enb2.s.rdcnt1 = 1; // Not an error condition
        // pci_int_enb2.s.rdcnt0 = 1; // Not an error condition
        // Skipping pci_int_enb2.s.reserved_23_24
        // pci_int_enb2.s.rptime1 = 1; // Not an error condition
        // pci_int_enb2.s.rptime0 = 1; // Not an error condition
        // Skipping pci_int_enb2.s.reserved_19_20
        // pci_int_enb2.s.rpcnt1 = 1; // Not an error condition
        // pci_int_enb2.s.rpcnt0 = 1; // Not an error condition
        // pci_int_enb2.s.rrsl_int = 1; // Not an error condition
        pci_int_enb2.s.ill_rrd = 1;
        pci_int_enb2.s.ill_rwr = 1;
        pci_int_enb2.s.rdperr = 1;
        pci_int_enb2.s.raperr = 1;
        pci_int_enb2.s.rserr = 1;
        pci_int_enb2.s.rtsr_abt = 1;
        pci_int_enb2.s.rmsc_msg = 1;
        pci_int_enb2.s.rmsi_mabt = 1;
        pci_int_enb2.s.rmsi_tabt = 1;
        pci_int_enb2.s.rmsi_per = 1;
        pci_int_enb2.s.rmr_tto = 1;
        pci_int_enb2.s.rmr_abt = 1;
        pci_int_enb2.s.rtr_abt = 1;
        pci_int_enb2.s.rmr_wtto = 1;
        pci_int_enb2.s.rmr_wabt = 1;
        pci_int_enb2.s.rtr_wabt = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping pci_int_enb2.s.reserved_34_63
        pci_int_enb2.s.ill_rd = 1;
        pci_int_enb2.s.ill_wr = 1;
        pci_int_enb2.s.win_wr = 1;
        // pci_int_enb2.s.dma1_fi = 1; // Not an error condition
        // pci_int_enb2.s.dma0_fi = 1; // Not an error condition
        // pci_int_enb2.s.rdtime1 = 1; // Not an error condition
        // pci_int_enb2.s.rdtime0 = 1; // Not an error condition
        // pci_int_enb2.s.rdcnt1 = 1; // Not an error condition
        // pci_int_enb2.s.rdcnt0 = 1; // Not an error condition
        // pci_int_enb2.s.rptime3 = 1; // Not an error condition
        // pci_int_enb2.s.rptime2 = 1; // Not an error condition
        // pci_int_enb2.s.rptime1 = 1; // Not an error condition
        // pci_int_enb2.s.rptime0 = 1; // Not an error condition
        // pci_int_enb2.s.rpcnt3 = 1; // Not an error condition
        // pci_int_enb2.s.rpcnt2 = 1; // Not an error condition
        // pci_int_enb2.s.rpcnt1 = 1; // Not an error condition
        // pci_int_enb2.s.rpcnt0 = 1; // Not an error condition
        // pci_int_enb2.s.rrsl_int = 1; // Not an error condition
        pci_int_enb2.s.ill_rrd = 1;
        pci_int_enb2.s.ill_rwr = 1;
        pci_int_enb2.s.rdperr = 1;
        pci_int_enb2.s.raperr = 1;
        pci_int_enb2.s.rserr = 1;
        pci_int_enb2.s.rtsr_abt = 1;
        pci_int_enb2.s.rmsc_msg = 1;
        pci_int_enb2.s.rmsi_mabt = 1;
        pci_int_enb2.s.rmsi_tabt = 1;
        pci_int_enb2.s.rmsi_per = 1;
        pci_int_enb2.s.rmr_tto = 1;
        pci_int_enb2.s.rmr_abt = 1;
        pci_int_enb2.s.rtr_abt = 1;
        pci_int_enb2.s.rmr_wtto = 1;
        pci_int_enb2.s.rmr_wabt = 1;
        pci_int_enb2.s.rtr_wabt = 1;
    }
    cvmx_write_csr(CVMX_NPI_PCI_INT_ENB2, pci_int_enb2.u64);
}


/**
 * __cvmx_interrupt_pci_int_sum2_decode decodes all interrupt bits in cvmx_pci_int_sum2_t
 */
void __cvmx_interrupt_pci_int_sum2_decode(void)
{
    cvmx_pci_int_sum2_t pci_int_sum2;
    pci_int_sum2.u64 = cvmx_read_csr(CVMX_NPI_PCI_INT_SUM2);
    pci_int_sum2.u64 &= cvmx_read_csr(CVMX_NPI_PCI_INT_ENB2);
    cvmx_write_csr(CVMX_NPI_PCI_INT_SUM2, pci_int_sum2.u64);
    // Skipping pci_int_sum2.s.reserved_34_63
    if (pci_int_sum2.s.ill_rd)
        PRINT_ERROR("PCI_INT_SUM2[ILL_RD]: A read to a disabled area of bar1 or bar2,\n"
                    "    when the mem area is disabled.\n");
    if (pci_int_sum2.s.ill_wr)
        PRINT_ERROR("PCI_INT_SUM2[ILL_WR]: A write to a disabled area of bar1 or bar2,\n"
                    "    when the mem area is disabled.\n");
    if (pci_int_sum2.s.win_wr)
        PRINT_ERROR("PCI_INT_SUM2[WIN_WR]: A write to the disabled Window Write Data or\n"
                    "    Read-Address Register took place.\n");
    if (pci_int_sum2.s.dma1_fi)
        PRINT_ERROR("PCI_INT_SUM2[DMA1_FI]: A DMA operation operation finished that was\n"
                    "    required to set the FORCE-INT bit for counter 1.\n");
    if (pci_int_sum2.s.dma0_fi)
        PRINT_ERROR("PCI_INT_SUM2[DMA0_FI]: A DMA operation operation finished that was\n"
                    "    required to set the FORCE-INT bit for counter 0.\n");
    if (pci_int_sum2.s.dtime1)
        PRINT_ERROR("PCI_INT_SUM2[DTIME1]: When the value in the PCI_DMA_CNT1\n"
                    "    register is not 0 the DMA_CNT1 timer counts.\n"
                    "    When the DMA1_CNT timer has a value greater\n"
                    "    than the PCI_DMA_TIME1 register this\n"
                    "    bit is set. The timer is reset when bit is\n"
                    "    written with a one.\n");
    if (pci_int_sum2.s.dtime0)
        PRINT_ERROR("PCI_INT_SUM2[DTIME0]: When the value in the PCI_DMA_CNT0\n"
                    "    register is not 0 the DMA_CNT0 timer counts.\n"
                    "    When the DMA0_CNT timer has a value greater\n"
                    "    than the PCI_DMA_TIME0 register this\n"
                    "    bit is set. The timer is reset when bit is\n"
                    "    written with a one.\n");
    if (pci_int_sum2.s.dcnt1)
        PRINT_ERROR("PCI_INT_SUM2[DCNT1]: This bit indicates that PCI_DMA_CNT1\n"
                    "    value is greater than the value\n"
                    "    in the PCI_DMA_INT_LEV1 register.\n");
    if (pci_int_sum2.s.dcnt0)
        PRINT_ERROR("PCI_INT_SUM2[DCNT0]: This bit indicates that PCI_DMA_CNT0\n"
                    "    value is greater than the value\n"
                    "    in the PCI_DMA_INT_LEV0 register.\n");
    if (pci_int_sum2.s.ptime3)
        PRINT_ERROR("PCI_INT_SUM2[PTIME3]: When the value in the PCI_PKTS_SENT3\n"
                    "    register is not 0 the Sent-3 timer counts.\n"
                    "    When the Sent-3 timer has a value greater\n"
                    "    than the PCI_PKTS_SENT_TIME3 register this\n"
                    "    bit is set. The timer is reset when bit is\n"
                    "    written with a one.\n");
    if (pci_int_sum2.s.ptime2)
        PRINT_ERROR("PCI_INT_SUM2[PTIME2]: When the value in the PCI_PKTS_SENT2\n"
                    "    register is not 0 the Sent-2 timer counts.\n"
                    "    When the Sent-2 timer has a value greater\n"
                    "    than the PCI_PKTS_SENT_TIME2 register this\n"
                    "    bit is set. The timer is reset when bit is\n"
                    "    written with a one.\n");
    if (pci_int_sum2.s.ptime1)
        PRINT_ERROR("PCI_INT_SUM2[PTIME1]: When the value in the PCI_PKTS_SENT1\n"
                    "    register is not 0 the Sent-1 timer counts.\n"
                    "    When the Sent-1 timer has a value greater\n"
                    "    than the PCI_PKTS_SENT_TIME1 register this\n"
                    "    bit is set. The timer is reset when bit is\n"
                    "    written with a one.\n");
    if (pci_int_sum2.s.ptime0)
        PRINT_ERROR("PCI_INT_SUM2[PTIME0]: When the value in the PCI_PKTS_SENT0\n"
                    "    register is not 0 the Sent-0 timer counts.\n"
                    "    When the Sent-0 timer has a value greater\n"
                    "    than the PCI_PKTS_SENT_TIME0 register this\n"
                    "    bit is set. The timer is reset when bit is\n"
                    "    written with a one.\n");
    if (pci_int_sum2.s.pcnt3)
        PRINT_ERROR("PCI_INT_SUM2[PCNT3]: This bit indicates that PCI_PKTS_SENT3\n"
                    "    value is greater than the value\n"
                    "    in the PCI_PKTS_SENT_INT_LEV3 register.\n");
    if (pci_int_sum2.s.pcnt2)
        PRINT_ERROR("PCI_INT_SUM2[PCNT2]: This bit indicates that PCI_PKTS_SENT2\n"
                    "    value is greater than the value\n"
                    "    in the PCI_PKTS_SENT_INT_LEV2 register.\n");
    if (pci_int_sum2.s.pcnt1)
        PRINT_ERROR("PCI_INT_SUM2[PCNT1]: This bit indicates that PCI_PKTS_SENT1\n"
                    "    value is greater than the value\n"
                    "    in the PCI_PKTS_SENT_INT_LEV1 register.\n");
    if (pci_int_sum2.s.pcnt0)
        PRINT_ERROR("PCI_INT_SUM2[PCNT0]: This bit indicates that PCI_PKTS_SENT0\n"
                    "    value is greater than the value\n"
                    "    in the PCI_PKTS_SENT_INT_LEV0 register.\n");
    if (pci_int_sum2.s.rsl_int)
        PRINT_ERROR("PCI_INT_SUM2[RSL_INT]: This bit is set when the RSL Chain has\n"
                    "    generated an interrupt.\n");
    if (pci_int_sum2.s.ill_rrd)
        PRINT_ERROR("PCI_INT_SUM2[ILL_RRD]: A read  to the disabled PCI registers took place.\n");
    if (pci_int_sum2.s.ill_rwr)
        PRINT_ERROR("PCI_INT_SUM2[ILL_RWR]: A write to the disabled PCI registers took place.\n");
    if (pci_int_sum2.s.dperr)
        PRINT_ERROR("PCI_INT_SUM2[DPERR]: Data Parity Error detected by PCX Core\n");
    if (pci_int_sum2.s.aperr)
        PRINT_ERROR("PCI_INT_SUM2[APERR]: Address Parity Error detected by PCX Core\n");
    if (pci_int_sum2.s.serr)
        PRINT_ERROR("PCI_INT_SUM2[SERR]: SERR# detected by PCX Core\n");
    if (pci_int_sum2.s.tsr_abt)
        PRINT_ERROR("PCI_INT_SUM2[TSR_ABT]: Target Split-Read Abort Detected\n");
    if (pci_int_sum2.s.msc_msg)
        PRINT_ERROR("PCI_INT_SUM2[MSC_MSG]: Master Split Completion Message Detected\n");
    if (pci_int_sum2.s.msi_mabt)
        PRINT_ERROR("PCI_INT_SUM2[MSI_MABT]: PCI MSI Master Abort.\n");
    if (pci_int_sum2.s.msi_tabt)
        PRINT_ERROR("PCI_INT_SUM2[MSI_TABT]: PCI MSI Target Abort.\n");
    if (pci_int_sum2.s.msi_per)
        PRINT_ERROR("PCI_INT_SUM2[MSI_PER]: PCI MSI Parity Error.\n");
    if (pci_int_sum2.s.mr_tto)
        PRINT_ERROR("PCI_INT_SUM2[MR_TTO]: PCI Master Retry Timeout On Read.\n");
    if (pci_int_sum2.s.mr_abt)
        PRINT_ERROR("PCI_INT_SUM2[MR_ABT]: PCI Master Abort On Read.\n");
    if (pci_int_sum2.s.tr_abt)
        PRINT_ERROR("PCI_INT_SUM2[TR_ABT]: PCI Target Abort On Read.\n");
    if (pci_int_sum2.s.mr_wtto)
        PRINT_ERROR("PCI_INT_SUM2[MR_WTTO]: PCI Master Retry Timeout on write.\n");
    if (pci_int_sum2.s.mr_wabt)
        PRINT_ERROR("PCI_INT_SUM2[MR_WABT]: PCI Master Abort detected on write.\n");
    if (pci_int_sum2.s.tr_wabt)
        PRINT_ERROR("PCI_INT_SUM2[TR_WABT]: PCI Target Abort detected on write.\n");
}


/**
 * __cvmx_interrupt_pcsx_intx_en_reg_enable enables all interrupt bits in cvmx_pcsx_intx_en_reg_t
 */
void __cvmx_interrupt_pcsx_intx_en_reg_enable(int index, int block)
{
    cvmx_pcsx_intx_en_reg_t pcs_int_en_reg;
    cvmx_write_csr(CVMX_PCSX_INTX_REG(index, block), cvmx_read_csr(CVMX_PCSX_INTX_REG(index, block)));
    pcs_int_en_reg.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping pcs_int_en_reg.s.reserved_12_63
        //pcs_int_en_reg.s.dup = 1; // This happens during normal operation
        pcs_int_en_reg.s.sync_bad_en = 1;
        pcs_int_en_reg.s.an_bad_en = 1;
        pcs_int_en_reg.s.rxlock_en = 1;
        pcs_int_en_reg.s.rxbad_en = 1;
        //pcs_int_en_reg.s.rxerr_en = 1; // This happens during normal operation
        pcs_int_en_reg.s.txbad_en = 1;
        pcs_int_en_reg.s.txfifo_en = 1;
        pcs_int_en_reg.s.txfifu_en = 1;
        pcs_int_en_reg.s.an_err_en = 1;
        //pcs_int_en_reg.s.xmit_en = 1; // This happens during normal operation
        //pcs_int_en_reg.s.lnkspd_en = 1; // This happens during normal operation
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping pcs_int_en_reg.s.reserved_12_63
        //pcs_int_en_reg.s.dup = 1; // This happens during normal operation
        pcs_int_en_reg.s.sync_bad_en = 1;
        pcs_int_en_reg.s.an_bad_en = 1;
        pcs_int_en_reg.s.rxlock_en = 1;
        pcs_int_en_reg.s.rxbad_en = 1;
        //pcs_int_en_reg.s.rxerr_en = 1; // This happens during normal operation
        pcs_int_en_reg.s.txbad_en = 1;
        pcs_int_en_reg.s.txfifo_en = 1;
        pcs_int_en_reg.s.txfifu_en = 1;
        pcs_int_en_reg.s.an_err_en = 1;
        //pcs_int_en_reg.s.xmit_en = 1; // This happens during normal operation
        //pcs_int_en_reg.s.lnkspd_en = 1; // This happens during normal operation
    }
    cvmx_write_csr(CVMX_PCSX_INTX_EN_REG(index, block), pcs_int_en_reg.u64);
}


/**
 * __cvmx_interrupt_pcsx_intx_reg_decode decodes all interrupt bits in cvmx_pcsx_intx_reg_t
 */
void __cvmx_interrupt_pcsx_intx_reg_decode(int index, int block)
{
    cvmx_pcsx_intx_reg_t pcs_int_reg;
    pcs_int_reg.u64 = cvmx_read_csr(CVMX_PCSX_INTX_REG(index, block));
    pcs_int_reg.u64 &= cvmx_read_csr(CVMX_PCSX_INTX_EN_REG(index, block));
    cvmx_write_csr(CVMX_PCSX_INTX_REG(index, block), pcs_int_reg.u64);
    // Skipping pcs_int_reg.s.reserved_12_63
    if (pcs_int_reg.s.dup)
        PRINT_ERROR("PCS%d_INT%d_REG[DUP]: Set whenever Duplex mode changes on the link\n", block, index);
    if (pcs_int_reg.s.sync_bad)
        PRINT_ERROR("PCS%d_INT%d_REG[SYNC_BAD]: Set by HW whenever rx sync st machine reaches a bad\n"
                    "    state. Should never be set during normal operation\n", block, index);
    if (pcs_int_reg.s.an_bad)
        PRINT_ERROR("PCS%d_INT%d_REG[AN_BAD]: Set by HW whenever AN st machine reaches a bad\n"
                    "    state. Should never be set during normal operation\n", block, index);
    if (pcs_int_reg.s.rxlock)
        PRINT_ERROR("PCS%d_INT%d_REG[RXLOCK]: Set by HW whenever code group Sync or bit lock\n"
                    "    failure occurs\n"
                    "    Cannot fire in loopback1 mode\n", block, index);
    if (pcs_int_reg.s.rxbad)
        PRINT_ERROR("PCS%d_INT%d_REG[RXBAD]: Set by HW whenever rx st machine reaches a  bad\n"
                    "    state. Should never be set during normal operation\n", block, index);
    if (pcs_int_reg.s.rxerr)
        PRINT_ERROR("PCS%d_INT%d_REG[RXERR]: Set whenever RX receives a code group error in\n"
                    "    10 bit to 8 bit decode logic\n"
                    "    Cannot fire in loopback1 mode\n", block, index);
    if (pcs_int_reg.s.txbad)
        PRINT_ERROR("PCS%d_INT%d_REG[TXBAD]: Set by HW whenever tx st machine reaches a bad\n"
                    "    state. Should never be set during normal operation\n", block, index);
    if (pcs_int_reg.s.txfifo)
        PRINT_ERROR("PCS%d_INT%d_REG[TXFIFO]: Set whenever HW detects a TX fifo overflow\n"
                    "    condition\n", block, index);
    if (pcs_int_reg.s.txfifu)
        PRINT_ERROR("PCS%d_INT%d_REG[TXFIFU]: Set whenever HW detects a TX fifo underflowflow\n"
                    "    condition\n", block, index);
    if (pcs_int_reg.s.an_err)
        PRINT_ERROR("PCS%d_INT%d_REG[AN_ERR]: AN Error, AN resolution function failed\n", block, index);
    if (pcs_int_reg.s.xmit)
        PRINT_ERROR("PCS%d_INT%d_REG[XMIT]: Set whenever HW detects a change in the XMIT\n"
                    "    variable. XMIT variable states are IDLE, CONFIG and\n"
                    "    DATA\n", block, index);
    if (pcs_int_reg.s.lnkspd)
        PRINT_ERROR("PCS%d_INT%d_REG[LNKSPD]: Set by HW whenever Link Speed has changed\n", block, index);
}


/**
 * __cvmx_interrupt_pcsxx_int_en_reg_enable enables all interrupt bits in cvmx_pcsxx_int_en_reg_t
 */
void __cvmx_interrupt_pcsxx_int_en_reg_enable(int index)
{
    cvmx_pcsxx_int_en_reg_t pcsx_int_en_reg;
    cvmx_write_csr(CVMX_PCSXX_INT_REG(index), cvmx_read_csr(CVMX_PCSXX_INT_REG(index)));
    pcsx_int_en_reg.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping pcsx_int_en_reg.s.reserved_6_63
        pcsx_int_en_reg.s.algnlos_en = 1;
        pcsx_int_en_reg.s.synlos_en = 1;
        pcsx_int_en_reg.s.bitlckls_en = 1;
        pcsx_int_en_reg.s.rxsynbad_en = 1;
        pcsx_int_en_reg.s.rxbad_en = 1;
        pcsx_int_en_reg.s.txflt_en = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping pcsx_int_en_reg.s.reserved_6_63
        pcsx_int_en_reg.s.algnlos_en = 1;
        pcsx_int_en_reg.s.synlos_en = 1;
        pcsx_int_en_reg.s.bitlckls_en = 0;  // Happens if XAUI module is not installed
        pcsx_int_en_reg.s.rxsynbad_en = 1;
        pcsx_int_en_reg.s.rxbad_en = 1;
        pcsx_int_en_reg.s.txflt_en = 1;
    }
    cvmx_write_csr(CVMX_PCSXX_INT_EN_REG(index), pcsx_int_en_reg.u64);
}


/**
 * __cvmx_interrupt_pcsxx_int_reg_decode decodes all interrupt bits in cvmx_pcsxx_int_reg_t
 */
void __cvmx_interrupt_pcsxx_int_reg_decode(int index)
{
    cvmx_pcsxx_int_reg_t pcsx_int_reg;
    pcsx_int_reg.u64 = cvmx_read_csr(CVMX_PCSXX_INT_REG(index));
    pcsx_int_reg.u64 &= cvmx_read_csr(CVMX_PCSXX_INT_EN_REG(index));
    cvmx_write_csr(CVMX_PCSXX_INT_REG(index), pcsx_int_reg.u64);
    // Skipping pcsx_int_reg.s.reserved_6_63
    if (pcsx_int_reg.s.algnlos)
        PRINT_ERROR("PCSX%d_INT_REG[ALGNLOS]: Set when XAUI lanes lose alignment\n", index);
    if (pcsx_int_reg.s.synlos)
        PRINT_ERROR("PCSX%d_INT_REG[SYNLOS]: Set when Code group sync lost on 1 or more  lanes\n", index);
    if (pcsx_int_reg.s.bitlckls)
        PRINT_ERROR("PCSX%d_INT_REG[BITLCKLS]: Set when Bit lock lost on 1 or more xaui lanes\n", index);
    if (pcsx_int_reg.s.rxsynbad)
        PRINT_ERROR("PCSX%d_INT_REG[RXSYNBAD]: Set when RX code grp sync st machine in bad state\n"
                    "    in one of the 4 xaui lanes\n", index);
    if (pcsx_int_reg.s.rxbad)
        PRINT_ERROR("PCSX%d_INT_REG[RXBAD]: Set when RX state machine in bad state\n", index);
    if (pcsx_int_reg.s.txflt)
        PRINT_ERROR("PCSX%d_INT_REG[TXFLT]: None defined at this time, always 0x0\n", index);
}


/**
 * __cvmx_interrupt_pescx_dbg_info_en_enable enables all interrupt bits in cvmx_pescx_dbg_info_en_t
 */
void __cvmx_interrupt_pescx_dbg_info_en_enable(int index)
{
    cvmx_pescx_dbg_info_en_t pesc_dbg_info_en;
    cvmx_write_csr(CVMX_PESCX_DBG_INFO(index), cvmx_read_csr(CVMX_PESCX_DBG_INFO(index)));
    pesc_dbg_info_en.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping pesc_dbg_info_en.s.reserved_31_63
        pesc_dbg_info_en.s.ecrc_e = 1;
        pesc_dbg_info_en.s.rawwpp = 1;
        pesc_dbg_info_en.s.racpp = 1;
        pesc_dbg_info_en.s.ramtlp = 1;
        pesc_dbg_info_en.s.rarwdns = 1;
        pesc_dbg_info_en.s.caar = 1;
        pesc_dbg_info_en.s.racca = 1;
        pesc_dbg_info_en.s.racur = 1;
        pesc_dbg_info_en.s.rauc = 1;
        pesc_dbg_info_en.s.rqo = 1;
        pesc_dbg_info_en.s.fcuv = 1;
        pesc_dbg_info_en.s.rpe = 1;
        pesc_dbg_info_en.s.fcpvwt = 1;
        pesc_dbg_info_en.s.dpeoosd = 1;
        pesc_dbg_info_en.s.rtwdle = 1;
        pesc_dbg_info_en.s.rdwdle = 1;
        pesc_dbg_info_en.s.mre = 1;
        pesc_dbg_info_en.s.rte = 1;
        pesc_dbg_info_en.s.acto = 1;
        pesc_dbg_info_en.s.rvdm = 1;
        pesc_dbg_info_en.s.rumep = 1;
        pesc_dbg_info_en.s.rptamrc = 1;
        pesc_dbg_info_en.s.rpmerc = 1;
        pesc_dbg_info_en.s.rfemrc = 1;
        pesc_dbg_info_en.s.rnfemrc = 1;
        pesc_dbg_info_en.s.rcemrc = 1;
        pesc_dbg_info_en.s.rpoison = 1;
        pesc_dbg_info_en.s.recrce = 1;
        pesc_dbg_info_en.s.rtlplle = 1;
#if 0
    /* RTLPMAL is disabled since it will be generated under normal conditions,
        like devices causing legacy PCI interrupts */
        pesc_dbg_info_en.s.rtlpmal = 1;
#endif
        pesc_dbg_info_en.s.spoison = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping pesc_dbg_info_en.s.reserved_31_63
        pesc_dbg_info_en.s.ecrc_e = 1;
        pesc_dbg_info_en.s.rawwpp = 1;
        pesc_dbg_info_en.s.racpp = 1;
        pesc_dbg_info_en.s.ramtlp = 1;
        pesc_dbg_info_en.s.rarwdns = 1;
        pesc_dbg_info_en.s.caar = 1;
        pesc_dbg_info_en.s.racca = 1;
        pesc_dbg_info_en.s.racur = 1;
        pesc_dbg_info_en.s.rauc = 1;
        pesc_dbg_info_en.s.rqo = 1;
        pesc_dbg_info_en.s.fcuv = 1;
        pesc_dbg_info_en.s.rpe = 1;
        pesc_dbg_info_en.s.fcpvwt = 1;
        pesc_dbg_info_en.s.dpeoosd = 1;
        pesc_dbg_info_en.s.rtwdle = 1;
        pesc_dbg_info_en.s.rdwdle = 1;
        pesc_dbg_info_en.s.mre = 1;
        pesc_dbg_info_en.s.rte = 1;
        pesc_dbg_info_en.s.acto = 1;
        pesc_dbg_info_en.s.rvdm = 1;
        pesc_dbg_info_en.s.rumep = 1;
        pesc_dbg_info_en.s.rptamrc = 1;
        pesc_dbg_info_en.s.rpmerc = 1;
        pesc_dbg_info_en.s.rfemrc = 1;
        pesc_dbg_info_en.s.rnfemrc = 1;
        pesc_dbg_info_en.s.rcemrc = 1;
        pesc_dbg_info_en.s.rpoison = 1;
        pesc_dbg_info_en.s.recrce = 1;
        pesc_dbg_info_en.s.rtlplle = 1;
#if 0
    /* RTLPMAL is disabled since it will be generated under normal conditions,
        like devices causing legacy PCI interrupts */
        pesc_dbg_info_en.s.rtlpmal = 1;
#endif
        pesc_dbg_info_en.s.spoison = 1;
    }
    cvmx_write_csr(CVMX_PESCX_DBG_INFO_EN(index), pesc_dbg_info_en.u64);
}


/**
 * __cvmx_interrupt_pescx_dbg_info_decode decodes all interrupt bits in cvmx_pescx_dbg_info_t
 */
void __cvmx_interrupt_pescx_dbg_info_decode(int index)
{
    cvmx_pescx_dbg_info_t pesc_dbg_info;
    pesc_dbg_info.u64 = cvmx_read_csr(CVMX_PESCX_DBG_INFO(index));
    pesc_dbg_info.u64 &= cvmx_read_csr(CVMX_PESCX_DBG_INFO_EN(index));
    cvmx_write_csr(CVMX_PESCX_DBG_INFO(index), pesc_dbg_info.u64);
    // Skipping pesc_dbg_info.s.reserved_31_63
    if (pesc_dbg_info.s.ecrc_e)
        PRINT_ERROR("PESC%d_DBG_INFO[ECRC_E]: Received a ECRC error.\n"
                    "    radm_ecrc_err\n", index);
    if (pesc_dbg_info.s.rawwpp)
        PRINT_ERROR("PESC%d_DBG_INFO[RAWWPP]: Received a write with poisoned payload\n"
                    "    radm_rcvd_wreq_poisoned\n", index);
    if (pesc_dbg_info.s.racpp)
        PRINT_ERROR("PESC%d_DBG_INFO[RACPP]: Received a completion with poisoned payload\n"
                    "    radm_rcvd_cpl_poisoned\n", index);
    if (pesc_dbg_info.s.ramtlp)
        PRINT_ERROR("PESC%d_DBG_INFO[RAMTLP]: Received a malformed TLP\n"
                    "    radm_mlf_tlp_err\n", index);
    if (pesc_dbg_info.s.rarwdns)
        PRINT_ERROR("PESC%d_DBG_INFO[RARWDNS]: Recieved a request which device does not support\n"
                    "    radm_rcvd_ur_req\n", index);
    if (pesc_dbg_info.s.caar)
        PRINT_ERROR("PESC%d_DBG_INFO[CAAR]: Completer aborted a request\n"
                    "    radm_rcvd_ca_req\n"
                    "    This bit will never be set because Octeon does\n"
                    "    not generate Completer Aborts.\n", index);
    if (pesc_dbg_info.s.racca)
        PRINT_ERROR("PESC%d_DBG_INFO[RACCA]: Received a completion with CA status\n"
                    "    radm_rcvd_cpl_ca\n", index);
    if (pesc_dbg_info.s.racur)
        PRINT_ERROR("PESC%d_DBG_INFO[RACUR]: Received a completion with UR status\n"
                    "    radm_rcvd_cpl_ur\n", index);
    if (pesc_dbg_info.s.rauc)
        PRINT_ERROR("PESC%d_DBG_INFO[RAUC]: Received an unexpected completion\n"
                    "    radm_unexp_cpl_err\n", index);
    if (pesc_dbg_info.s.rqo)
        PRINT_ERROR("PESC%d_DBG_INFO[RQO]: Receive queue overflow. Normally happens only when\n"
                    "    flow control advertisements are ignored\n"
                    "    radm_qoverflow\n", index);
    if (pesc_dbg_info.s.fcuv)
        PRINT_ERROR("PESC%d_DBG_INFO[FCUV]: Flow Control Update Violation (opt. checks)\n"
                    "    int_xadm_fc_prot_err\n", index);
    if (pesc_dbg_info.s.rpe)
        PRINT_ERROR("PESC%d_DBG_INFO[RPE]: When the PHY reports 8B/10B decode error\n"
                    "    (RxStatus = 3b100) or disparity error\n"
                    "    (RxStatus = 3b111), the signal rmlh_rcvd_err will\n"
                    "    be asserted.\n"
                    "    rmlh_rcvd_err\n", index);
    if (pesc_dbg_info.s.fcpvwt)
        PRINT_ERROR("PESC%d_DBG_INFO[FCPVWT]: Flow Control Protocol Violation (Watchdog Timer)\n"
                    "    rtlh_fc_prot_err\n", index);
    if (pesc_dbg_info.s.dpeoosd)
        PRINT_ERROR("PESC%d_DBG_INFO[DPEOOSD]: DLLP protocol error (out of sequence DLLP)\n"
                    "    rdlh_prot_err\n", index);
    if (pesc_dbg_info.s.rtwdle)
        PRINT_ERROR("PESC%d_DBG_INFO[RTWDLE]: Received TLP with DataLink Layer Error\n"
                    "    rdlh_bad_tlp_err\n", index);
    if (pesc_dbg_info.s.rdwdle)
        PRINT_ERROR("PESC%d_DBG_INFO[RDWDLE]: Received DLLP with DataLink Layer Error\n"
                    "    rdlh_bad_dllp_err\n", index);
    if (pesc_dbg_info.s.mre)
        PRINT_ERROR("PESC%d_DBG_INFO[MRE]: Max Retries Exceeded\n"
                    "    xdlh_replay_num_rlover_err\n", index);
    if (pesc_dbg_info.s.rte)
        PRINT_ERROR("PESC%d_DBG_INFO[RTE]: Replay Timer Expired\n"
                    "    xdlh_replay_timeout_err\n"
                    "    This bit is set when the REPLAY_TIMER expires in\n"
                    "    the PCIE core. The probability of this bit being\n"
                    "    set will increase with the traffic load.\n", index);
    if (pesc_dbg_info.s.acto)
        PRINT_ERROR("PESC%d_DBG_INFO[ACTO]: A Completion Timeout Occured\n"
                    "    pedc_radm_cpl_timeout\n", index);
    if (pesc_dbg_info.s.rvdm)
        PRINT_ERROR("PESC%d_DBG_INFO[RVDM]: Received Vendor-Defined Message\n"
                    "    pedc_radm_vendor_msg\n", index);
    if (pesc_dbg_info.s.rumep)
        PRINT_ERROR("PESC%d_DBG_INFO[RUMEP]: Received Unlock Message (EP Mode Only)\n"
                    "    pedc_radm_msg_unlock\n", index);
    if (pesc_dbg_info.s.rptamrc)
        PRINT_ERROR("PESC%d_DBG_INFO[RPTAMRC]: Received PME Turnoff Acknowledge Message\n"
                    "    (RC Mode only)\n"
                    "    pedc_radm_pm_to_ack\n", index);
    if (pesc_dbg_info.s.rpmerc)
        PRINT_ERROR("PESC%d_DBG_INFO[RPMERC]: Received PME Message (RC Mode only)\n"
                    "    pedc_radm_pm_pme\n", index);
    if (pesc_dbg_info.s.rfemrc)
        PRINT_ERROR("PESC%d_DBG_INFO[RFEMRC]: Received Fatal Error Message (RC Mode only)\n"
                    "    pedc_radm_fatal_err\n"
                    "    Bit set when a message with ERR_FATAL is set.\n", index);
    if (pesc_dbg_info.s.rnfemrc)
        PRINT_ERROR("PESC%d_DBG_INFO[RNFEMRC]: Received Non-Fatal Error Message (RC Mode only)\n"
                    "    pedc_radm_nonfatal_err\n", index);
    if (pesc_dbg_info.s.rcemrc)
        PRINT_ERROR("PESC%d_DBG_INFO[RCEMRC]: Received Correctable Error Message (RC Mode only)\n"
                    "    pedc_radm_correctable_err\n", index);
    if (pesc_dbg_info.s.rpoison)
        PRINT_ERROR("PESC%d_DBG_INFO[RPOISON]: Received Poisoned TLP\n"
                    "    pedc__radm_trgt1_poisoned & pedc__radm_trgt1_hv\n", index);
    if (pesc_dbg_info.s.recrce)
        PRINT_ERROR("PESC%d_DBG_INFO[RECRCE]: Received ECRC Error\n"
                    "    pedc_radm_trgt1_ecrc_err & pedc__radm_trgt1_eot\n", index);
    if (pesc_dbg_info.s.rtlplle)
        PRINT_ERROR("PESC%d_DBG_INFO[RTLPLLE]: Received TLP has link layer error\n"
                    "    pedc_radm_trgt1_dllp_abort & pedc__radm_trgt1_eot\n", index);
    if (pesc_dbg_info.s.rtlpmal)
        PRINT_ERROR("PESC%d_DBG_INFO[RTLPMAL]: Received TLP is malformed or a message.\n"
                    "    pedc_radm_trgt1_tlp_abort & pedc__radm_trgt1_eot\n"
                    "    If the core receives a MSG (or Vendor Message)\n"
                    "    this bit will be set.\n", index);
    if (pesc_dbg_info.s.spoison)
        PRINT_ERROR("PESC%d_DBG_INFO[SPOISON]: Poisoned TLP sent\n"
                    "    peai__client0_tlp_ep & peai__client0_tlp_hv\n", index);
}


/**
 * __cvmx_interrupt_pip_int_en_enable enables all interrupt bits in cvmx_pip_int_en_t
 */
void __cvmx_interrupt_pip_int_en_enable(void)
{
    cvmx_pip_int_en_t pip_int_en;
    cvmx_write_csr(CVMX_PIP_INT_REG, cvmx_read_csr(CVMX_PIP_INT_REG));
    pip_int_en.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping pip_int_en.s.reserved_13_63
        pip_int_en.s.punyerr = 1;
        //pip_int_en.s.lenerr = 1; // Signalled in packet WQE
        //pip_int_en.s.maxerr = 1; // Signalled in packet WQE
        //pip_int_en.s.minerr = 1; // Signalled in packet WQE
        pip_int_en.s.beperr = 1;
        pip_int_en.s.feperr = 1;
        pip_int_en.s.todoovr = 1;
        pip_int_en.s.skprunt = 1;
        pip_int_en.s.badtag = 1;
        pip_int_en.s.prtnxa = 1;
        //pip_int_en.s.bckprs = 1; // Don't care
        //pip_int_en.s.crcerr = 1; // Signalled in packet WQE
        //pip_int_en.s.pktdrp = 1; // Don't care
    }
    if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        // Skipping pip_int_en.s.reserved_9_63
        pip_int_en.s.beperr = 1;
        pip_int_en.s.feperr = 1;
        pip_int_en.s.todoovr = 1;
        pip_int_en.s.skprunt = 1;
        pip_int_en.s.badtag = 1;
        pip_int_en.s.prtnxa = 1;
        //pip_int_en.s.bckprs = 1; // Don't care
        //pip_int_en.s.crcerr = 1; // Signalled in packet WQE
        //pip_int_en.s.pktdrp = 1; // Don't care
    }
    if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        // Skipping pip_int_en.s.reserved_12_63
        //pip_int_en.s.lenerr = 1; // Signalled in packet WQE
        //pip_int_en.s.maxerr = 1; // Signalled in packet WQE
        //pip_int_en.s.minerr = 1; // Signalled in packet WQE
        pip_int_en.s.beperr = 1;
        pip_int_en.s.feperr = 1;
        pip_int_en.s.todoovr = 1;
        pip_int_en.s.skprunt = 1;
        pip_int_en.s.badtag = 1;
        pip_int_en.s.prtnxa = 1;
        //pip_int_en.s.bckprs = 1; // Don't care
        // Skipping pip_int_en.s.reserved_1_1
        //pip_int_en.s.pktdrp = 1; // Don't care
    }
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping pip_int_en.s.reserved_9_63
        pip_int_en.s.beperr = 1;
        pip_int_en.s.feperr = 1;
        pip_int_en.s.todoovr = 1;
        pip_int_en.s.skprunt = 1;
        pip_int_en.s.badtag = 1;
        pip_int_en.s.prtnxa = 1;
        //pip_int_en.s.bckprs = 1; // Don't care
        //pip_int_en.s.crcerr = 1; // Signalled in packet WQE
        //pip_int_en.s.pktdrp = 1; // Don't care
    }
    if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        // Skipping pip_int_en.s.reserved_9_63
        pip_int_en.s.beperr = 1;
        pip_int_en.s.feperr = 1;
        pip_int_en.s.todoovr = 1;
        pip_int_en.s.skprunt = 1;
        pip_int_en.s.badtag = 1;
        pip_int_en.s.prtnxa = 1;
        //pip_int_en.s.bckprs = 1; // Don't care
        //pip_int_en.s.crcerr = 1; // Signalled in packet WQE
        //pip_int_en.s.pktdrp = 1; // Don't care
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping pip_int_en.s.reserved_13_63
        pip_int_en.s.punyerr = 1;
        // Skipping pip_int_en.s.reserved_9_11
        pip_int_en.s.beperr = 1;
        pip_int_en.s.feperr = 1;
        pip_int_en.s.todoovr = 1;
        pip_int_en.s.skprunt = 1;
        pip_int_en.s.badtag = 1;
        pip_int_en.s.prtnxa = 1;
        //pip_int_en.s.bckprs = 1; // Don't care
        //pip_int_en.s.crcerr = 1; // Signalled in packet WQE
        //pip_int_en.s.pktdrp = 1; // Don't care
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping pip_int_en.s.reserved_13_63
        pip_int_en.s.punyerr = 1;
        //pip_int_en.s.lenerr = 1; // Signalled in packet WQE
        //pip_int_en.s.maxerr = 1; // Signalled in packet WQE
        //pip_int_en.s.minerr = 1; // Signalled in packet WQE
        pip_int_en.s.beperr = 1;
        pip_int_en.s.feperr = 1;
        pip_int_en.s.todoovr = 1;
        pip_int_en.s.skprunt = 1;
        pip_int_en.s.badtag = 1;
        pip_int_en.s.prtnxa = 1;
        //pip_int_en.s.bckprs = 1; // Don't care
        // Skipping pip_int_en.s.reserved_1_1
        //pip_int_en.s.pktdrp = 1; // Don't care
    }
    cvmx_write_csr(CVMX_PIP_INT_EN, pip_int_en.u64);
}


/**
 * __cvmx_interrupt_pip_int_reg_decode decodes all interrupt bits in cvmx_pip_int_reg_t
 */
void __cvmx_interrupt_pip_int_reg_decode(void)
{
    cvmx_pip_int_reg_t pip_int_reg;
    pip_int_reg.u64 = cvmx_read_csr(CVMX_PIP_INT_REG);
    pip_int_reg.u64 &= cvmx_read_csr(CVMX_PIP_INT_EN);
    cvmx_write_csr(CVMX_PIP_INT_REG, pip_int_reg.u64);
    // Skipping pip_int_reg.s.reserved_13_63
    if (pip_int_reg.s.punyerr)
        PRINT_ERROR("PIP_INT_REG[PUNYERR]: Frame was received with length <=4B when CRC\n"
                    "    stripping in IPD is enable\n");
    if (pip_int_reg.s.lenerr)
        PRINT_ERROR("PIP_INT_REG[LENERR]: Frame was received with length error\n");
    if (pip_int_reg.s.maxerr)
        PRINT_ERROR("PIP_INT_REG[MAXERR]: Frame was received with length > max_length\n");
    if (pip_int_reg.s.minerr)
        PRINT_ERROR("PIP_INT_REG[MINERR]: Frame was received with length < min_length\n");
    if (pip_int_reg.s.beperr)
        PRINT_ERROR("PIP_INT_REG[BEPERR]: Parity Error in back end memory\n");
    if (pip_int_reg.s.feperr)
        PRINT_ERROR("PIP_INT_REG[FEPERR]: Parity Error in front end memory\n");
    if (pip_int_reg.s.todoovr)
        PRINT_ERROR("PIP_INT_REG[TODOOVR]: Todo list overflow (see PIP_BCK_PRS[HIWATER])\n");
    if (pip_int_reg.s.skprunt)
        PRINT_ERROR("PIP_INT_REG[SKPRUNT]: Packet was engulfed by skipper\n"
                    "    This interrupt can occur with received PARTIAL\n"
                    "    packets that are truncated to SKIP bytes or\n"
                    "    smaller.\n");
    if (pip_int_reg.s.badtag)
        PRINT_ERROR("PIP_INT_REG[BADTAG]: A bad tag was sent from IPD\n");
    if (pip_int_reg.s.prtnxa)
        PRINT_ERROR("PIP_INT_REG[PRTNXA]: Non-existent port\n");
    if (pip_int_reg.s.bckprs)
        PRINT_ERROR("PIP_INT_REG[BCKPRS]: PIP asserted backpressure\n");
    if (pip_int_reg.s.crcerr)
        PRINT_ERROR("PIP_INT_REG[CRCERR]: PIP calculated bad CRC\n");
    if (pip_int_reg.s.pktdrp)
        PRINT_ERROR("PIP_INT_REG[PKTDRP]: Packet Dropped due to QOS\n");
}


/**
 * __cvmx_interrupt_pko_reg_int_mask_enable enables all interrupt bits in cvmx_pko_reg_int_mask_t
 */
void __cvmx_interrupt_pko_reg_int_mask_enable(void)
{
    cvmx_pko_reg_int_mask_t pko_reg_int_mask;
    cvmx_write_csr(CVMX_PKO_REG_ERROR, cvmx_read_csr(CVMX_PKO_REG_ERROR));
    pko_reg_int_mask.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping pko_reg_int_mask.s.reserved_3_63
        pko_reg_int_mask.s.currzero = 1;
        pko_reg_int_mask.s.doorbell = 1;
        pko_reg_int_mask.s.parity = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        // Skipping pko_reg_int_mask.s.reserved_2_63
        pko_reg_int_mask.s.doorbell = 1;
        pko_reg_int_mask.s.parity = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        // Skipping pko_reg_int_mask.s.reserved_3_63
        pko_reg_int_mask.s.currzero = 1;
        pko_reg_int_mask.s.doorbell = 1;
        pko_reg_int_mask.s.parity = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping pko_reg_int_mask.s.reserved_2_63
        pko_reg_int_mask.s.doorbell = 1;
        pko_reg_int_mask.s.parity = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        // Skipping pko_reg_int_mask.s.reserved_2_63
        pko_reg_int_mask.s.doorbell = 1;
        pko_reg_int_mask.s.parity = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping pko_reg_int_mask.s.reserved_3_63
        pko_reg_int_mask.s.currzero = 1;
        pko_reg_int_mask.s.doorbell = 1;
        pko_reg_int_mask.s.parity = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping pko_reg_int_mask.s.reserved_3_63
        pko_reg_int_mask.s.currzero = 1;
        pko_reg_int_mask.s.doorbell = 1;
        pko_reg_int_mask.s.parity = 1;
    }
    cvmx_write_csr(CVMX_PKO_REG_INT_MASK, pko_reg_int_mask.u64);
}


/**
 * __cvmx_interrupt_pko_reg_error_decode decodes all interrupt bits in cvmx_pko_reg_error_t
 */
void __cvmx_interrupt_pko_reg_error_decode(void)
{
    cvmx_pko_reg_error_t pko_reg_error;
    pko_reg_error.u64 = cvmx_read_csr(CVMX_PKO_REG_ERROR);
    pko_reg_error.u64 &= cvmx_read_csr(CVMX_PKO_REG_INT_MASK);
    cvmx_write_csr(CVMX_PKO_REG_ERROR, pko_reg_error.u64);
    // Skipping pko_reg_error.s.reserved_3_63
    if (pko_reg_error.s.currzero)
        PRINT_ERROR("PKO_REG_ERROR[CURRZERO]: A packet data pointer has size=0\n");
    if (pko_reg_error.s.doorbell)
        PRINT_ERROR("PKO_REG_ERROR[DOORBELL]: A doorbell count has overflowed\n");
    if (pko_reg_error.s.parity)
        PRINT_ERROR("PKO_REG_ERROR[PARITY]: Read parity error at port data buffer\n");
}


/**
 * __cvmx_interrupt_rad_reg_int_mask_enable enables all interrupt bits in cvmx_rad_reg_int_mask_t
 */
void __cvmx_interrupt_rad_reg_int_mask_enable(void)
{
    cvmx_rad_reg_int_mask_t rad_reg_int_mask;
    cvmx_write_csr(CVMX_RAD_REG_ERROR, cvmx_read_csr(CVMX_RAD_REG_ERROR));
    rad_reg_int_mask.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping rad_reg_int_mask.s.reserved_1_63
        rad_reg_int_mask.s.doorbell = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping rad_reg_int_mask.s.reserved_1_63
        rad_reg_int_mask.s.doorbell = 1;
    }
    cvmx_write_csr(CVMX_RAD_REG_INT_MASK, rad_reg_int_mask.u64);
}


/**
 * __cvmx_interrupt_rad_reg_error_decode decodes all interrupt bits in cvmx_rad_reg_error_t
 */
void __cvmx_interrupt_rad_reg_error_decode(void)
{
    cvmx_rad_reg_error_t rad_reg_error;
    rad_reg_error.u64 = cvmx_read_csr(CVMX_RAD_REG_ERROR);
    rad_reg_error.u64 &= cvmx_read_csr(CVMX_RAD_REG_INT_MASK);
    cvmx_write_csr(CVMX_RAD_REG_ERROR, rad_reg_error.u64);
    // Skipping rad_reg_error.s.reserved_1_63
    if (rad_reg_error.s.doorbell)
        PRINT_ERROR("RAD_REG_ERROR[DOORBELL]: A doorbell count has overflowed\n");
}


/**
 * __cvmx_interrupt_spxx_int_msk_enable enables all interrupt bits in cvmx_spxx_int_msk_t
 */
void __cvmx_interrupt_spxx_int_msk_enable(int index)
{
    cvmx_spxx_int_msk_t spx_int_msk;
    cvmx_write_csr(CVMX_SPXX_INT_REG(index), cvmx_read_csr(CVMX_SPXX_INT_REG(index)));
    spx_int_msk.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping spx_int_msk.s.reserved_12_63
        spx_int_msk.s.calerr = 1;
        spx_int_msk.s.syncerr = 1;
        spx_int_msk.s.diperr = 1;
        spx_int_msk.s.tpaovr = 1;
        spx_int_msk.s.rsverr = 1;
        spx_int_msk.s.drwnng = 1;
        spx_int_msk.s.clserr = 1;
        spx_int_msk.s.spiovr = 1;
        // Skipping spx_int_msk.s.reserved_2_3
        spx_int_msk.s.abnorm = 1;
        spx_int_msk.s.prtnxa = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping spx_int_msk.s.reserved_12_63
        spx_int_msk.s.calerr = 1;
        spx_int_msk.s.syncerr = 1;
        spx_int_msk.s.diperr = 1;
        spx_int_msk.s.tpaovr = 1;
        spx_int_msk.s.rsverr = 1;
        spx_int_msk.s.drwnng = 1;
        spx_int_msk.s.clserr = 1;
        spx_int_msk.s.spiovr = 1;
        // Skipping spx_int_msk.s.reserved_2_3
        spx_int_msk.s.abnorm = 1;
        spx_int_msk.s.prtnxa = 1;
    }
    cvmx_write_csr(CVMX_SPXX_INT_MSK(index), spx_int_msk.u64);
}


/**
 * __cvmx_interrupt_spxx_int_reg_decode decodes all interrupt bits in cvmx_spxx_int_reg_t
 */
void __cvmx_interrupt_spxx_int_reg_decode(int index)
{
    cvmx_spxx_int_reg_t spx_int_reg;
    spx_int_reg.u64 = cvmx_read_csr(CVMX_SPXX_INT_REG(index));
    spx_int_reg.u64 &= cvmx_read_csr(CVMX_SPXX_INT_MSK(index));
    cvmx_write_csr(CVMX_SPXX_INT_REG(index), spx_int_reg.u64);
    // Skipping spx_int_reg.s.reserved_32_63
    if (spx_int_reg.s.spf)
        PRINT_ERROR("SPX%d_INT_REG[SPF]: Spi interface down\n", index);
    // Skipping spx_int_reg.s.reserved_12_30
    if (spx_int_reg.s.calerr)
        PRINT_ERROR("SPX%d_INT_REG[CALERR]: Spi4 Calendar table parity error\n", index);
    if (spx_int_reg.s.syncerr)
        PRINT_ERROR("SPX%d_INT_REG[SYNCERR]: Consecutive Spi4 DIP4 errors have exceeded\n"
                    "    SPX_ERR_CTL[ERRCNT]\n", index);
    if (spx_int_reg.s.diperr)
        PRINT_ERROR("SPX%d_INT_REG[DIPERR]: Spi4 DIP4 error\n", index);
    if (spx_int_reg.s.tpaovr)
        PRINT_ERROR("SPX%d_INT_REG[TPAOVR]: Selected port has hit TPA overflow\n", index);
    if (spx_int_reg.s.rsverr)
        PRINT_ERROR("SPX%d_INT_REG[RSVERR]: Spi4 reserved control word detected\n", index);
    if (spx_int_reg.s.drwnng)
        PRINT_ERROR("SPX%d_INT_REG[DRWNNG]: Spi4 receive FIFO drowning/overflow\n", index);
    if (spx_int_reg.s.clserr)
        PRINT_ERROR("SPX%d_INT_REG[CLSERR]: Spi4 packet closed on non-16B alignment without EOP\n", index);
    if (spx_int_reg.s.spiovr)
        PRINT_ERROR("SPX%d_INT_REG[SPIOVR]: Spi async FIFO overflow\n", index);
    // Skipping spx_int_reg.s.reserved_2_3
    if (spx_int_reg.s.abnorm)
        PRINT_ERROR("SPX%d_INT_REG[ABNORM]: Abnormal packet termination (ERR bit)\n", index);
    if (spx_int_reg.s.prtnxa)
        PRINT_ERROR("SPX%d_INT_REG[PRTNXA]: Port out of range\n", index);
}


/**
 * __cvmx_interrupt_stxx_int_msk_enable enables all interrupt bits in cvmx_stxx_int_msk_t
 */
void __cvmx_interrupt_stxx_int_msk_enable(int index)
{
    cvmx_stxx_int_msk_t stx_int_msk;
    cvmx_write_csr(CVMX_STXX_INT_REG(index), cvmx_read_csr(CVMX_STXX_INT_REG(index)));
    stx_int_msk.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping stx_int_msk.s.reserved_8_63
        stx_int_msk.s.frmerr = 1;
        stx_int_msk.s.unxfrm = 1;
        stx_int_msk.s.nosync = 1;
        stx_int_msk.s.diperr = 1;
        stx_int_msk.s.datovr = 1;
        stx_int_msk.s.ovrbst = 1;
        stx_int_msk.s.calpar1 = 1;
        stx_int_msk.s.calpar0 = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping stx_int_msk.s.reserved_8_63
        stx_int_msk.s.frmerr = 1;
        stx_int_msk.s.unxfrm = 1;
        stx_int_msk.s.nosync = 1;
        stx_int_msk.s.diperr = 1;
        stx_int_msk.s.datovr = 1;
        stx_int_msk.s.ovrbst = 1;
        stx_int_msk.s.calpar1 = 1;
        stx_int_msk.s.calpar0 = 1;
    }
    cvmx_write_csr(CVMX_STXX_INT_MSK(index), stx_int_msk.u64);
}


/**
 * __cvmx_interrupt_stxx_int_reg_decode decodes all interrupt bits in cvmx_stxx_int_reg_t
 */
void __cvmx_interrupt_stxx_int_reg_decode(int index)
{
    cvmx_stxx_int_reg_t stx_int_reg;
    stx_int_reg.u64 = cvmx_read_csr(CVMX_STXX_INT_REG(index));
    stx_int_reg.u64 &= cvmx_read_csr(CVMX_STXX_INT_MSK(index));
    cvmx_write_csr(CVMX_STXX_INT_REG(index), stx_int_reg.u64);
    // Skipping stx_int_reg.s.reserved_9_63
    if (stx_int_reg.s.syncerr)
        PRINT_ERROR("STX%d_INT_REG[SYNCERR]: Interface encountered a fatal error\n", index);
    if (stx_int_reg.s.frmerr)
        PRINT_ERROR("STX%d_INT_REG[FRMERR]: FRMCNT has exceeded STX_DIP_CNT[MAXFRM]\n", index);
    if (stx_int_reg.s.unxfrm)
        PRINT_ERROR("STX%d_INT_REG[UNXFRM]: Unexpected framing sequence\n", index);
    if (stx_int_reg.s.nosync)
        PRINT_ERROR("STX%d_INT_REG[NOSYNC]: ERRCNT has exceeded STX_DIP_CNT[MAXDIP]\n", index);
    if (stx_int_reg.s.diperr)
        PRINT_ERROR("STX%d_INT_REG[DIPERR]: DIP2 error on the Spi4 Status channel\n", index);
    if (stx_int_reg.s.datovr)
        PRINT_ERROR("STX%d_INT_REG[DATOVR]: Spi4 FIFO overflow error\n", index);
    if (stx_int_reg.s.ovrbst)
        PRINT_ERROR("STX%d_INT_REG[OVRBST]: Transmit packet burst too big\n", index);
    if (stx_int_reg.s.calpar1)
        PRINT_ERROR("STX%d_INT_REG[CALPAR1]: STX Calendar Table Parity Error Bank1\n", index);
    if (stx_int_reg.s.calpar0)
        PRINT_ERROR("STX%d_INT_REG[CALPAR0]: STX Calendar Table Parity Error Bank0\n", index);
}


/**
 * __cvmx_interrupt_usbnx_int_enb_enable enables all interrupt bits in cvmx_usbnx_int_enb_t
 */
void __cvmx_interrupt_usbnx_int_enb_enable(int index)
{
    cvmx_usbnx_int_enb_t usbn_int_enb;
    cvmx_write_csr(CVMX_USBNX_INT_SUM(index), cvmx_read_csr(CVMX_USBNX_INT_SUM(index)));
    usbn_int_enb.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        // Skipping usbn_int_enb.s.reserved_38_63
        usbn_int_enb.s.nd4o_dpf = 1;
        usbn_int_enb.s.nd4o_dpe = 1;
        usbn_int_enb.s.nd4o_rpf = 1;
        usbn_int_enb.s.nd4o_rpe = 1;
        usbn_int_enb.s.ltl_f_pf = 1;
        usbn_int_enb.s.ltl_f_pe = 1;
        usbn_int_enb.s.u2n_c_pe = 1;
        usbn_int_enb.s.u2n_c_pf = 1;
        usbn_int_enb.s.u2n_d_pf = 1;
        usbn_int_enb.s.u2n_d_pe = 1;
        usbn_int_enb.s.n2u_pe = 1;
        usbn_int_enb.s.n2u_pf = 1;
        usbn_int_enb.s.uod_pf = 1;
        usbn_int_enb.s.uod_pe = 1;
        usbn_int_enb.s.rq_q3_e = 1;
        usbn_int_enb.s.rq_q3_f = 1;
        usbn_int_enb.s.rq_q2_e = 1;
        usbn_int_enb.s.rq_q2_f = 1;
        usbn_int_enb.s.rg_fi_f = 1;
        usbn_int_enb.s.rg_fi_e = 1;
        usbn_int_enb.s.l2_fi_f = 1;
        usbn_int_enb.s.l2_fi_e = 1;
        usbn_int_enb.s.l2c_a_f = 1;
        usbn_int_enb.s.l2c_s_e = 1;
        usbn_int_enb.s.dcred_f = 1;
        usbn_int_enb.s.dcred_e = 1;
        usbn_int_enb.s.lt_pu_f = 1;
        usbn_int_enb.s.lt_po_e = 1;
        usbn_int_enb.s.nt_pu_f = 1;
        usbn_int_enb.s.nt_po_e = 1;
        usbn_int_enb.s.pt_pu_f = 1;
        usbn_int_enb.s.pt_po_e = 1;
        usbn_int_enb.s.lr_pu_f = 1;
        usbn_int_enb.s.lr_po_e = 1;
        usbn_int_enb.s.nr_pu_f = 1;
        usbn_int_enb.s.nr_po_e = 1;
        usbn_int_enb.s.pr_pu_f = 1;
        usbn_int_enb.s.pr_po_e = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        // Skipping usbn_int_enb.s.reserved_38_63
        usbn_int_enb.s.nd4o_dpf = 1;
        usbn_int_enb.s.nd4o_dpe = 1;
        usbn_int_enb.s.nd4o_rpf = 1;
        usbn_int_enb.s.nd4o_rpe = 1;
        usbn_int_enb.s.ltl_f_pf = 1;
        usbn_int_enb.s.ltl_f_pe = 1;
        // Skipping usbn_int_enb.s.reserved_26_31
        usbn_int_enb.s.uod_pf = 1;
        usbn_int_enb.s.uod_pe = 1;
        usbn_int_enb.s.rq_q3_e = 1;
        usbn_int_enb.s.rq_q3_f = 1;
        usbn_int_enb.s.rq_q2_e = 1;
        usbn_int_enb.s.rq_q2_f = 1;
        usbn_int_enb.s.rg_fi_f = 1;
        usbn_int_enb.s.rg_fi_e = 1;
        usbn_int_enb.s.l2_fi_f = 1;
        usbn_int_enb.s.l2_fi_e = 1;
        usbn_int_enb.s.l2c_a_f = 1;
        usbn_int_enb.s.l2c_s_e = 1;
        usbn_int_enb.s.dcred_f = 1;
        usbn_int_enb.s.dcred_e = 1;
        usbn_int_enb.s.lt_pu_f = 1;
        usbn_int_enb.s.lt_po_e = 1;
        usbn_int_enb.s.nt_pu_f = 1;
        usbn_int_enb.s.nt_po_e = 1;
        usbn_int_enb.s.pt_pu_f = 1;
        usbn_int_enb.s.pt_po_e = 1;
        usbn_int_enb.s.lr_pu_f = 1;
        usbn_int_enb.s.lr_po_e = 1;
        usbn_int_enb.s.nr_pu_f = 1;
        usbn_int_enb.s.nr_po_e = 1;
        usbn_int_enb.s.pr_pu_f = 1;
        usbn_int_enb.s.pr_po_e = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        // Skipping usbn_int_enb.s.reserved_38_63
        usbn_int_enb.s.nd4o_dpf = 1;
        usbn_int_enb.s.nd4o_dpe = 1;
        usbn_int_enb.s.nd4o_rpf = 1;
        usbn_int_enb.s.nd4o_rpe = 1;
        usbn_int_enb.s.ltl_f_pf = 1;
        usbn_int_enb.s.ltl_f_pe = 1;
        usbn_int_enb.s.u2n_c_pe = 1;
        usbn_int_enb.s.u2n_c_pf = 1;
        usbn_int_enb.s.u2n_d_pf = 1;
        usbn_int_enb.s.u2n_d_pe = 1;
        usbn_int_enb.s.n2u_pe = 1;
        usbn_int_enb.s.n2u_pf = 1;
        usbn_int_enb.s.uod_pf = 1;
        usbn_int_enb.s.uod_pe = 1;
        usbn_int_enb.s.rq_q3_e = 1;
        usbn_int_enb.s.rq_q3_f = 1;
        usbn_int_enb.s.rq_q2_e = 1;
        usbn_int_enb.s.rq_q2_f = 1;
        usbn_int_enb.s.rg_fi_f = 1;
        usbn_int_enb.s.rg_fi_e = 1;
        usbn_int_enb.s.l2_fi_f = 1;
        usbn_int_enb.s.l2_fi_e = 1;
        usbn_int_enb.s.l2c_a_f = 1;
        usbn_int_enb.s.l2c_s_e = 1;
        usbn_int_enb.s.dcred_f = 1;
        usbn_int_enb.s.dcred_e = 1;
        usbn_int_enb.s.lt_pu_f = 1;
        usbn_int_enb.s.lt_po_e = 1;
        usbn_int_enb.s.nt_pu_f = 1;
        usbn_int_enb.s.nt_po_e = 1;
        usbn_int_enb.s.pt_pu_f = 1;
        usbn_int_enb.s.pt_po_e = 1;
        usbn_int_enb.s.lr_pu_f = 1;
        usbn_int_enb.s.lr_po_e = 1;
        usbn_int_enb.s.nr_pu_f = 1;
        usbn_int_enb.s.nr_po_e = 1;
        usbn_int_enb.s.pr_pu_f = 1;
        usbn_int_enb.s.pr_po_e = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping usbn_int_enb.s.reserved_38_63
        usbn_int_enb.s.nd4o_dpf = 1;
        usbn_int_enb.s.nd4o_dpe = 1;
        usbn_int_enb.s.nd4o_rpf = 1;
        usbn_int_enb.s.nd4o_rpe = 1;
        usbn_int_enb.s.ltl_f_pf = 1;
        usbn_int_enb.s.ltl_f_pe = 1;
        // Skipping usbn_int_enb.s.reserved_26_31
        usbn_int_enb.s.uod_pf = 1;
        usbn_int_enb.s.uod_pe = 1;
        usbn_int_enb.s.rq_q3_e = 1;
        usbn_int_enb.s.rq_q3_f = 1;
        usbn_int_enb.s.rq_q2_e = 1;
        usbn_int_enb.s.rq_q2_f = 1;
        usbn_int_enb.s.rg_fi_f = 1;
        usbn_int_enb.s.rg_fi_e = 1;
        usbn_int_enb.s.l2_fi_f = 1;
        usbn_int_enb.s.l2_fi_e = 1;
        usbn_int_enb.s.l2c_a_f = 1;
        usbn_int_enb.s.l2c_s_e = 1;
        usbn_int_enb.s.dcred_f = 1;
        usbn_int_enb.s.dcred_e = 1;
        usbn_int_enb.s.lt_pu_f = 1;
        usbn_int_enb.s.lt_po_e = 1;
        usbn_int_enb.s.nt_pu_f = 1;
        usbn_int_enb.s.nt_po_e = 1;
        usbn_int_enb.s.pt_pu_f = 1;
        usbn_int_enb.s.pt_po_e = 1;
        usbn_int_enb.s.lr_pu_f = 1;
        usbn_int_enb.s.lr_po_e = 1;
        usbn_int_enb.s.nr_pu_f = 1;
        usbn_int_enb.s.nr_po_e = 1;
        usbn_int_enb.s.pr_pu_f = 1;
        usbn_int_enb.s.pr_po_e = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        // Skipping usbn_int_enb.s.reserved_38_63
        usbn_int_enb.s.nd4o_dpf = 1;
        usbn_int_enb.s.nd4o_dpe = 1;
        usbn_int_enb.s.nd4o_rpf = 1;
        usbn_int_enb.s.nd4o_rpe = 1;
        usbn_int_enb.s.ltl_f_pf = 1;
        usbn_int_enb.s.ltl_f_pe = 1;
        // Skipping usbn_int_enb.s.reserved_26_31
        usbn_int_enb.s.uod_pf = 1;
        usbn_int_enb.s.uod_pe = 1;
        usbn_int_enb.s.rq_q3_e = 1;
        usbn_int_enb.s.rq_q3_f = 1;
        usbn_int_enb.s.rq_q2_e = 1;
        usbn_int_enb.s.rq_q2_f = 1;
        usbn_int_enb.s.rg_fi_f = 1;
        usbn_int_enb.s.rg_fi_e = 1;
        usbn_int_enb.s.l2_fi_f = 1;
        usbn_int_enb.s.l2_fi_e = 1;
        usbn_int_enb.s.l2c_a_f = 1;
        usbn_int_enb.s.l2c_s_e = 1;
        usbn_int_enb.s.dcred_f = 1;
        usbn_int_enb.s.dcred_e = 1;
        usbn_int_enb.s.lt_pu_f = 1;
        usbn_int_enb.s.lt_po_e = 1;
        usbn_int_enb.s.nt_pu_f = 1;
        usbn_int_enb.s.nt_po_e = 1;
        usbn_int_enb.s.pt_pu_f = 1;
        usbn_int_enb.s.pt_po_e = 1;
        usbn_int_enb.s.lr_pu_f = 1;
        usbn_int_enb.s.lr_po_e = 1;
        usbn_int_enb.s.nr_pu_f = 1;
        usbn_int_enb.s.nr_po_e = 1;
        usbn_int_enb.s.pr_pu_f = 1;
        usbn_int_enb.s.pr_po_e = 1;
    }
    cvmx_write_csr(CVMX_USBNX_INT_ENB(index), usbn_int_enb.u64);
}


/**
 * __cvmx_interrupt_usbnx_int_sum_decode decodes all interrupt bits in cvmx_usbnx_int_sum_t
 */
void __cvmx_interrupt_usbnx_int_sum_decode(int index)
{
    cvmx_usbnx_int_sum_t usbn_int_sum;
    usbn_int_sum.u64 = cvmx_read_csr(CVMX_USBNX_INT_SUM(index));
    usbn_int_sum.u64 &= cvmx_read_csr(CVMX_USBNX_INT_ENB(index));
    cvmx_write_csr(CVMX_USBNX_INT_SUM(index), usbn_int_sum.u64);
    // Skipping usbn_int_sum.s.reserved_38_63
    if (usbn_int_sum.s.nd4o_dpf)
        PRINT_ERROR("USBN%d_INT_SUM[ND4O_DPF]: NCB DMA Out Data Fifo Push Full.\n", index);
    if (usbn_int_sum.s.nd4o_dpe)
        PRINT_ERROR("USBN%d_INT_SUM[ND4O_DPE]: NCB DMA Out Data Fifo Pop Empty.\n", index);
    if (usbn_int_sum.s.nd4o_rpf)
        PRINT_ERROR("USBN%d_INT_SUM[ND4O_RPF]: NCB DMA Out Request Fifo Push Full.\n", index);
    if (usbn_int_sum.s.nd4o_rpe)
        PRINT_ERROR("USBN%d_INT_SUM[ND4O_RPE]: NCB DMA Out Request Fifo Pop Empty.\n", index);
    if (usbn_int_sum.s.ltl_f_pf)
        PRINT_ERROR("USBN%d_INT_SUM[LTL_F_PF]: L2C Transfer Length Fifo Push Full.\n", index);
    if (usbn_int_sum.s.ltl_f_pe)
        PRINT_ERROR("USBN%d_INT_SUM[LTL_F_PE]: L2C Transfer Length Fifo Pop Empty.\n", index);
    if (usbn_int_sum.s.u2n_c_pe)
        PRINT_ERROR("USBN%d_INT_SUM[U2N_C_PE]: U2N Control Fifo Pop Empty.\n", index);
    if (usbn_int_sum.s.u2n_c_pf)
        PRINT_ERROR("USBN%d_INT_SUM[U2N_C_PF]: U2N Control Fifo Push Full.\n", index);
    if (usbn_int_sum.s.u2n_d_pf)
        PRINT_ERROR("USBN%d_INT_SUM[U2N_D_PF]: U2N Data Fifo Push Full.\n", index);
    if (usbn_int_sum.s.u2n_d_pe)
        PRINT_ERROR("USBN%d_INT_SUM[U2N_D_PE]: U2N Data Fifo Pop Empty.\n", index);
    if (usbn_int_sum.s.n2u_pe)
        PRINT_ERROR("USBN%d_INT_SUM[N2U_PE]: N2U Fifo Pop Empty.\n", index);
    if (usbn_int_sum.s.n2u_pf)
        PRINT_ERROR("USBN%d_INT_SUM[N2U_PF]: N2U Fifo Push Full.\n", index);
    if (usbn_int_sum.s.uod_pf)
        PRINT_ERROR("USBN%d_INT_SUM[UOD_PF]: UOD Fifo Push Full.\n", index);
    if (usbn_int_sum.s.uod_pe)
        PRINT_ERROR("USBN%d_INT_SUM[UOD_PE]: UOD Fifo Pop Empty.\n", index);
    if (usbn_int_sum.s.rq_q3_e)
        PRINT_ERROR("USBN%d_INT_SUM[RQ_Q3_E]: Request Queue-3 Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.rq_q3_f)
        PRINT_ERROR("USBN%d_INT_SUM[RQ_Q3_F]: Request Queue-3 Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.rq_q2_e)
        PRINT_ERROR("USBN%d_INT_SUM[RQ_Q2_E]: Request Queue-2 Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.rq_q2_f)
        PRINT_ERROR("USBN%d_INT_SUM[RQ_Q2_F]: Request Queue-2 Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.rg_fi_f)
        PRINT_ERROR("USBN%d_INT_SUM[RG_FI_F]: Register Request Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.rg_fi_e)
        PRINT_ERROR("USBN%d_INT_SUM[RG_FI_E]: Register Request Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.lt_fi_f)
        PRINT_ERROR("USBN%d_INT_SUM[LT_FI_F]: L2C Request Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.lt_fi_e)
        PRINT_ERROR("USBN%d_INT_SUM[LT_FI_E]: L2C Request Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.l2c_a_f)
        PRINT_ERROR("USBN%d_INT_SUM[L2C_A_F]: L2C Credit Count Added When Full.\n", index);
    if (usbn_int_sum.s.l2c_s_e)
        PRINT_ERROR("USBN%d_INT_SUM[L2C_S_E]: L2C Credit Count Subtracted When Empty.\n", index);
    if (usbn_int_sum.s.dcred_f)
        PRINT_ERROR("USBN%d_INT_SUM[DCRED_F]: Data CreditFifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.dcred_e)
        PRINT_ERROR("USBN%d_INT_SUM[DCRED_E]: Data Credit Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.lt_pu_f)
        PRINT_ERROR("USBN%d_INT_SUM[LT_PU_F]: L2C Trasaction Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.lt_po_e)
        PRINT_ERROR("USBN%d_INT_SUM[LT_PO_E]: L2C Trasaction Fifo Popped When Full.\n", index);
    if (usbn_int_sum.s.nt_pu_f)
        PRINT_ERROR("USBN%d_INT_SUM[NT_PU_F]: NPI Trasaction Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.nt_po_e)
        PRINT_ERROR("USBN%d_INT_SUM[NT_PO_E]: NPI Trasaction Fifo Popped When Full.\n", index);
    if (usbn_int_sum.s.pt_pu_f)
        PRINT_ERROR("USBN%d_INT_SUM[PT_PU_F]: PP  Trasaction Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.pt_po_e)
        PRINT_ERROR("USBN%d_INT_SUM[PT_PO_E]: PP  Trasaction Fifo Popped When Full.\n", index);
    if (usbn_int_sum.s.lr_pu_f)
        PRINT_ERROR("USBN%d_INT_SUM[LR_PU_F]: L2C Request Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.lr_po_e)
        PRINT_ERROR("USBN%d_INT_SUM[LR_PO_E]: L2C Request Fifo Popped When Empty.\n", index);
    if (usbn_int_sum.s.nr_pu_f)
        PRINT_ERROR("USBN%d_INT_SUM[NR_PU_F]: NPI Request Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.nr_po_e)
        PRINT_ERROR("USBN%d_INT_SUM[NR_PO_E]: NPI Request Fifo Popped When Empty.\n", index);
    if (usbn_int_sum.s.pr_pu_f)
        PRINT_ERROR("USBN%d_INT_SUM[PR_PU_F]: PP  Request Fifo Pushed When Full.\n", index);
    if (usbn_int_sum.s.pr_po_e)
        PRINT_ERROR("USBN%d_INT_SUM[PR_PO_E]: PP  Request Fifo Popped When Empty.\n", index);
}


/**
 * __cvmx_interrupt_zip_int_mask_enable enables all interrupt bits in cvmx_zip_int_mask_t
 */
void __cvmx_interrupt_zip_int_mask_enable(void)
{
    cvmx_zip_int_mask_t zip_int_mask;
    cvmx_write_csr(CVMX_ZIP_ERROR, cvmx_read_csr(CVMX_ZIP_ERROR));
    zip_int_mask.u64 = 0;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        // Skipping zip_int_mask.s.reserved_1_63
        zip_int_mask.s.doorbell = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        // Skipping zip_int_mask.s.reserved_1_63
        zip_int_mask.s.doorbell = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
        // Skipping zip_int_mask.s.reserved_1_63
        zip_int_mask.s.doorbell = 1;
    }
    if (OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        // Skipping zip_int_mask.s.reserved_1_63
        zip_int_mask.s.doorbell = 1;
    }
    cvmx_write_csr(CVMX_ZIP_INT_MASK, zip_int_mask.u64);
}


/**
 * __cvmx_interrupt_zip_error_decode decodes all interrupt bits in cvmx_zip_error_t
 */
void __cvmx_interrupt_zip_error_decode(void)
{
    cvmx_zip_error_t zip_error;
    zip_error.u64 = cvmx_read_csr(CVMX_ZIP_ERROR);
    zip_error.u64 &= cvmx_read_csr(CVMX_ZIP_INT_MASK);
    cvmx_write_csr(CVMX_ZIP_ERROR, zip_error.u64);
    // Skipping zip_error.s.reserved_1_63
    if (zip_error.s.doorbell)
        PRINT_ERROR("ZIP_ERROR[DOORBELL]: A doorbell count has overflowed\n");
}

