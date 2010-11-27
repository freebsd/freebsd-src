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
 * Utility functions to decode Octeon's RSL_INT_BLOCKS
 * interrupts into error messages.
 *
 * <hr>$Revision: 32636 $<hr>
 */
#include "cvmx.h"
#include "cvmx-interrupt.h"
#include "cvmx-l2c.h"

#ifndef PRINT_ERROR
#define PRINT_ERROR(format, ...) cvmx_safe_printf("ERROR " format, ##__VA_ARGS__)
#endif

/* Change this to a 1 before calling cvmx_interrupt_rsl_enable() to report
    single bit ecc errors and other correctable errors */
CVMX_SHARED int __cvmx_interrupt_ecc_report_single_bit_errors = 0;

void __cvmx_interrupt_agl_gmx_rxx_int_en_enable(int index);
void __cvmx_interrupt_agl_gmx_rxx_int_reg_decode(int index);
void __cvmx_interrupt_fpa_int_enb_enable(void);
void __cvmx_interrupt_fpa_int_sum_decode(void);
void __cvmx_interrupt_gmxx_rxx_int_en_enable(int index, int block);
void __cvmx_interrupt_gmxx_rxx_int_reg_decode(int index, int block);
void __cvmx_interrupt_iob_int_enb_enable(void);
void __cvmx_interrupt_iob_int_sum_decode(void);
void __cvmx_interrupt_ipd_int_enb_enable(void);
void __cvmx_interrupt_ipd_int_sum_decode(void);
void __cvmx_interrupt_key_int_enb_enable(void);
void __cvmx_interrupt_key_int_sum_decode(void);
void __cvmx_interrupt_mio_boot_int_enable(void);
void __cvmx_interrupt_mio_boot_err_decode(void);
void __cvmx_interrupt_npei_int_sum_decode(void);
void __cvmx_interrupt_npei_int_enb2_enable(void);
void __cvmx_interrupt_npi_int_enb_enable(void);
void __cvmx_interrupt_npi_int_sum_decode(void);
void __cvmx_interrupt_pcsx_intx_en_reg_enable(int index, int block);
void __cvmx_interrupt_pcsx_intx_reg_decode(int index, int block);
void __cvmx_interrupt_pcsxx_int_en_reg_enable(int index);
void __cvmx_interrupt_pcsxx_int_reg_decode(int index);
void __cvmx_interrupt_pescx_dbg_info_en_enable(int index);
void __cvmx_interrupt_pescx_dbg_info_decode(int index);
void __cvmx_interrupt_pip_int_en_enable(void);
void __cvmx_interrupt_pip_int_reg_decode(void);
void __cvmx_interrupt_pko_reg_int_mask_enable(void);
void __cvmx_interrupt_pko_reg_error_decode(void);
void __cvmx_interrupt_rad_reg_int_mask_enable(void);
void __cvmx_interrupt_rad_reg_error_decode(void);
void __cvmx_interrupt_spxx_int_msk_enable(int index);
void __cvmx_interrupt_spxx_int_reg_decode(int index);
void __cvmx_interrupt_stxx_int_msk_enable(int index);
void __cvmx_interrupt_stxx_int_reg_decode(int index);
void __cvmx_interrupt_usbnx_int_enb_enable(int index);
void __cvmx_interrupt_usbnx_int_sum_decode(int index);
void __cvmx_interrupt_zip_int_mask_enable(void);
void __cvmx_interrupt_zip_error_decode(void);


/**
 * Enable ASX error interrupts that exist on CN3XXX, CN50XX, and
 * CN58XX.
 *
 * @param block  Interface to enable 0-1
 */
static void __cvmx_interrupt_asxx_enable(int block)
{
    int mask;
    cvmx_asxx_int_en_t csr;
    /* CN38XX and CN58XX have two interfaces with 4 ports per interface. All
        other chips have a max of 3 ports on interface 0 */
    if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX))
        mask = 0xf; /* Set enables for 4 ports */
    else
        mask = 0x7; /* Set enables for 3 ports */

    /* Enable interface interrupts */
    csr.u64 = cvmx_read_csr(CVMX_ASXX_INT_EN(block));
    csr.s.txpsh = mask;
    csr.s.txpop = mask;
    csr.s.ovrflw = mask;
    cvmx_write_csr(CVMX_ASXX_INT_EN(block), csr.u64);
}


/**
 * Decode ASX error interrupts for CN3XXX, CN50XX, and CN58XX
 *
 * @param block  Interface to decode 0-1
 */
static void __cvmx_interrupt_asxx_decode(int block)
{
    cvmx_asxx_int_reg_t err;
    err.u64 = cvmx_read_csr(CVMX_ASXX_INT_REG(block));
    cvmx_write_csr(CVMX_ASXX_INT_REG(block), err.u64);
    if (err.u64)
    {
        int port;
        for (port = 0; port < 4; port++)
        {
            if (err.s.ovrflw & (1 << port))
                PRINT_ERROR("ASX%d_INT_REG[OVRFLW]: RX FIFO overflow on RMGII port %d\n",
                             block, port + block*16);
            if (err.s.txpop & (1 << port))
                PRINT_ERROR("ASX%d_INT_REG[TXPOP]: TX FIFO underflow on RMGII port %d\n",
                             block, port + block*16);
            if (err.s.txpsh & (1 << port))
                PRINT_ERROR("ASX%d_INT_REG[TXPSH]: TX FIFO overflow on RMGII port %d\n",
                             block, port + block*16);
        }
    }
}


/**
 * Enable DFA errors for CN38XX, CN58XX, and CN31XX
 */
static void __cvmx_interrupt_dfa_enable(void)
{
    cvmx_dfa_err_t csr;
    csr.u64 = cvmx_read_csr(CVMX_DFA_ERR);
    csr.s.dblina = 1;
    csr.s.cp2pina = 1;
    csr.s.cp2parena = 0;
    csr.s.dtepina = 1;
    csr.s.dteparena = 1;
    csr.s.dtedbina = 1;
    csr.s.dtesbina = __cvmx_interrupt_ecc_report_single_bit_errors;
    csr.s.dteeccena = 1;
    csr.s.cp2dbina = 1;
    csr.s.cp2sbina = __cvmx_interrupt_ecc_report_single_bit_errors;
    csr.s.cp2eccena = 1;
    cvmx_write_csr(CVMX_DFA_ERR, csr.u64);
}


/**
 * Decode DFA errors for CN38XX, CN58XX, and CN31XX
 */
static void __cvmx_interrupt_dfa_decode(void)
{
    cvmx_dfa_err_t err;

    err.u64 = cvmx_read_csr(CVMX_DFA_ERR);
    cvmx_write_csr(CVMX_DFA_ERR, err.u64);
    if (err.u64)
    {
        if (err.s.dblovf)
            PRINT_ERROR("DFA_ERR[DBLOVF]: Doorbell Overflow detected\n");
        if (err.s.cp2perr)
            PRINT_ERROR("DFA_ERR[CP2PERR]: PP-CP2 Parity Error Detected\n");
        if (err.s.dteperr)
            PRINT_ERROR("DFA_ERR[DTEPERR]: DTE Parity Error Detected\n");

        if (err.s.dtedbe)
            PRINT_ERROR("DFA_ERR[DTEDBE]: DFA DTE 29b Double Bit Error Detected\n");
        if (err.s.dtesbe && __cvmx_interrupt_ecc_report_single_bit_errors)
            PRINT_ERROR("DFA_ERR[DTESBE]: DFA DTE 29b Single Bit Error Corrected\n");
        if (err.s.dtedbe || (err.s.dtesbe && __cvmx_interrupt_ecc_report_single_bit_errors))
            PRINT_ERROR("DFA_ERR[DTESYN]: Failing syndrome %u\n", err.s.dtesyn);

        if (err.s.cp2dbe)
            PRINT_ERROR("DFA_ERR[CP2DBE]: DFA PP-CP2 Double Bit Error Detected\n");
        if (err.s.cp2sbe && __cvmx_interrupt_ecc_report_single_bit_errors)
            PRINT_ERROR("DFA_ERR[CP2SBE]: DFA PP-CP2 Single Bit Error Corrected\n");
        if (err.s.cp2dbe || (err.s.cp2sbe && __cvmx_interrupt_ecc_report_single_bit_errors))
            PRINT_ERROR("DFA_ERR[CP2SYN]: Failing syndrome %u\n", err.s.cp2syn);
    }
}


/**
 * Enable L2 error interrupts for all chips
 */
static void __cvmx_interrupt_l2_enable(void)
{
    cvmx_l2t_err_t csr;
    cvmx_l2d_err_t csr2;

    /* Enable ECC Interrupts for double bit errors from L2C Tags */
    csr.u64 = cvmx_read_csr(CVMX_L2T_ERR);
    csr.s.lck_intena2 = 1;
    csr.s.lck_intena = 1;
    csr.s.ded_intena = 1;
    csr.s.sec_intena = __cvmx_interrupt_ecc_report_single_bit_errors;
    csr.s.ecc_ena = 1;
    cvmx_write_csr(CVMX_L2T_ERR, csr.u64);

    /* Enable ECC Interrupts for double bit errors from L2D Errors */
    csr2.u64 = cvmx_read_csr(CVMX_L2D_ERR);
    csr2.s.ded_intena = 1;
    csr2.s.sec_intena = __cvmx_interrupt_ecc_report_single_bit_errors;
    csr2.s.ecc_ena = 1;
    cvmx_write_csr(CVMX_L2D_ERR, csr2.u64);
}


/**
 * Decode L2 error interrupts for all chips
 */
static void __cvmx_interrupt_l2_decode(void)
{
    cvmx_l2t_err_t terr;
    cvmx_l2d_err_t derr;
    uint64_t clr_val;

    terr.u64 = cvmx_read_csr(CVMX_L2T_ERR);
    cvmx_write_csr(CVMX_L2T_ERR, terr.u64);
    if (terr.u64)
    {
        if (terr.s.ded_err)
            PRINT_ERROR("L2T_ERR[DED_ERR]: double bit:\tfadr: 0x%x, fset: 0x%x, fsyn: 0x%x\n",
                         terr.s.fadr, terr.s.fset, terr.s.fsyn);
        if (terr.s.sec_err && __cvmx_interrupt_ecc_report_single_bit_errors)
            PRINT_ERROR("L2T_ERR[SEC_ERR]: single bit:\tfadr: 0x%x, fset: 0x%x, fsyn: 0x%x\n",
                         terr.s.fadr, terr.s.fset, terr.s.fsyn);
        if (terr.s.ded_err || terr.s.sec_err)
        {
            if (!terr.s.fsyn)
            {
                /* Syndrome is zero, which means error was in non-hit line,
                    so flush all associations */
                int i;
                int l2_assoc = cvmx_l2c_get_num_assoc();

                for (i = 0; i < l2_assoc; i++)
                    cvmx_l2c_flush_line(i, terr.s.fadr);
            }
            else
                cvmx_l2c_flush_line(terr.s.fset, terr.s.fadr);

        }
        if (terr.s.lckerr2)
            PRINT_ERROR("L2T_ERR[LCKERR2]: HW detected a case where a Rd/Wr Miss from PP#n could not find an available/unlocked set (for replacement).\n");
        if (terr.s.lckerr)
            PRINT_ERROR("L2T_ERR[LCKERR]: SW attempted to LOCK DOWN the last available set of the INDEX (which is ignored by HW - but reported to SW).\n");
    }

    clr_val = derr.u64 = cvmx_read_csr(CVMX_L2D_ERR);
    if (derr.u64)
    {
        cvmx_l2d_fadr_t fadr;

        if (derr.s.ded_err || (derr.s.sec_err && __cvmx_interrupt_ecc_report_single_bit_errors))
        {
            const int coreid = (int) cvmx_get_core_num();
            uint64_t syn0 = cvmx_read_csr(CVMX_L2D_FSYN0);
            uint64_t syn1 = cvmx_read_csr(CVMX_L2D_FSYN1);
            fadr.u64 = cvmx_read_csr(CVMX_L2D_FADR);
            if (derr.s.ded_err)
                PRINT_ERROR("L2D_ERR[DED_ERR] ECC double (core %d): fadr: 0x%llx, syn0:0x%llx, syn1: 0x%llx\n",
                             coreid, (unsigned long long) fadr.u64, (unsigned long long) syn0, (unsigned long long) syn1);
            else
                PRINT_ERROR("L2D_ERR[SEC_ERR] ECC single (core %d): fadr: 0x%llx, syn0:0x%llx, syn1: 0x%llx\n",
                             coreid, (unsigned long long) fadr.u64, (unsigned long long) syn0, (unsigned long long) syn1);
            /* Flush the line that had the error */
            if (derr.s.ded_err || derr.s.sec_err)
                cvmx_l2c_flush_line(fadr.s.fset, fadr.s.fadr >> 1);
        }
    }
    cvmx_write_csr(CVMX_L2D_ERR, clr_val);
}


/**
 * Enable LMC (DDR controller) interrupts for all chips
 *
 * @param ddr_controller
 *               Which controller to enable for 0-1
 */
static void __cvmx_interrupt_lmcx_enable(int ddr_controller)
{
    cvmx_lmc_mem_cfg0_t csr;

    /* The LMC controllers can be independently enabled/disabled on CN56XX.
        If a controller is disabled it can't be accessed at all since it
        isn't clocked */
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        cvmx_l2c_cfg_t l2c_cfg;
        l2c_cfg.u64 = cvmx_read_csr(CVMX_L2C_CFG);
        if (!l2c_cfg.s.dpres0 && (ddr_controller == 0))
            return;
        if (!l2c_cfg.s.dpres1 && (ddr_controller == 1))
            return;
    }

    csr.u64 = cvmx_read_csr(CVMX_LMCX_MEM_CFG0(ddr_controller));
    csr.s.intr_ded_ena = 1;
    csr.s.intr_sec_ena = __cvmx_interrupt_ecc_report_single_bit_errors;
    cvmx_write_csr(CVMX_LMCX_MEM_CFG0(ddr_controller), csr.u64);
}


/**
 * Decode LMC (DDR controller) interrupts for all chips
 *
 * @param ddr_controller
 *               Which controller to decode 0-1
 */
static void __cvmx_interrupt_lmcx_decode(int ddr_controller)
{
    /* These static counters are used to track ECC error counts */
    static CVMX_SHARED unsigned long single_bit_errors[2] = {0, 0};
    static CVMX_SHARED unsigned long double_bit_errors[2] = {0, 0};
    cvmx_lmcx_mem_cfg0_t mem_cfg0;
    cvmx_lmc_fadr_t fadr;

    mem_cfg0.u64 =cvmx_read_csr(CVMX_LMCX_MEM_CFG0(ddr_controller));
    fadr.u64 = cvmx_read_csr(CVMX_LMCX_FADR (ddr_controller));
    cvmx_write_csr(CVMX_LMCX_MEM_CFG0(ddr_controller),mem_cfg0.u64);
    if (mem_cfg0.s.sec_err || mem_cfg0.s.ded_err)
    {
        int pop_count;
        CVMX_DPOP(pop_count, mem_cfg0.s.sec_err);
        single_bit_errors[ddr_controller] += pop_count;
        CVMX_DPOP(pop_count, mem_cfg0.s.ded_err);
        double_bit_errors[ddr_controller] += pop_count;
        if (mem_cfg0.s.ded_err || (mem_cfg0.s.sec_err && __cvmx_interrupt_ecc_report_single_bit_errors))
        {
            PRINT_ERROR("DDR%d ECC: %lu Single bit corrections, %lu Double bit errors\n"
                         "DDR%d ECC:\tFailing dimm:   %u\n"
                         "DDR%d ECC:\tFailing rank:   %u\n"
                         "DDR%d ECC:\tFailing bank:   %u\n"
                         "DDR%d ECC:\tFailing row:    0x%x\n"
                         "DDR%d ECC:\tFailing column: 0x%x\n",
                         ddr_controller, single_bit_errors[ddr_controller], double_bit_errors[ddr_controller],
                         ddr_controller, fadr.s.fdimm,
                         ddr_controller, fadr.s.fbunk,
                         ddr_controller, fadr.s.fbank,
                         ddr_controller, fadr.s.frow,
                         ddr_controller, fadr.s.fcol);
        }
    }
}


/**
 * Decode GMX error interrupts
 *
 * @param block  GMX interface to decode
 */
static void __cvmx_interrupt_gmxx_decode(int block)
{
    int index;
    cvmx_gmxx_tx_int_reg_t csr;

    csr.u64 = cvmx_read_csr(CVMX_GMXX_TX_INT_REG(block)) & cvmx_read_csr(CVMX_GMXX_TX_INT_EN(block));
    cvmx_write_csr(CVMX_GMXX_TX_INT_REG(block), csr.u64);

    for (index=0; index<4; index++)
    {
        if (csr.s.late_col & (1<<index))
            PRINT_ERROR("GMX%d_TX%d_INT_REG[LATE_COL]: TX Late Collision\n", block, index);
        if (csr.s.xsdef & (1<<index))
            PRINT_ERROR("GMX%d_TX%d_INT_REG[XSDEF]: TX Excessive deferral\n", block, index);
        if (csr.s.xscol & (1<<index))
            PRINT_ERROR("GMX%d_TX%d_INT_REG[XSCOL]: TX Excessive collisions\n", block, index);
        if (csr.s.undflw & (1<<index))
            PRINT_ERROR("GMX%d_TX%d_INT_REG[UNDFLW]: TX Underflow\n", block, index);
    }
    if (csr.s.ncb_nxa)
        PRINT_ERROR("GMX%d_TX_INT_REG[NCB_NXA]: Port address out-of-range from NCB Interface\n", block);
    if (csr.s.pko_nxa)
        PRINT_ERROR("GMX%d_TX_INT_REG[PKO_NXA]: Port address out-of-range from PKO Interface\n", block);

    __cvmx_interrupt_gmxx_rxx_int_reg_decode(0, block);
    __cvmx_interrupt_gmxx_rxx_int_reg_decode(1, block);
    __cvmx_interrupt_gmxx_rxx_int_reg_decode(2, block);
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN50XX)))
        __cvmx_interrupt_gmxx_rxx_int_reg_decode(3, block);
}


/**
 * Enable POW error interrupts for all chips
 */
static void __cvmx_interrupt_pow_enable(void)
{
    cvmx_pow_ecc_err_t csr;
    csr.u64 = cvmx_read_csr(CVMX_POW_ECC_ERR);
    if (!OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2) && !OCTEON_IS_MODEL(OCTEON_CN31XX))
    {
            /* These doesn't exist for chips CN31XX and CN38XXp2 */
            csr.s.iop_ie = 0x1fff;
    }
    csr.s.rpe_ie = 1;
    csr.s.dbe_ie = 1;
    csr.s.sbe_ie = __cvmx_interrupt_ecc_report_single_bit_errors;
    cvmx_write_csr(CVMX_POW_ECC_ERR, csr.u64);
}


/**
 * Decode POW error interrupts for all chips
 */
static void __cvmx_interrupt_pow_decode(void)
{
    cvmx_pow_ecc_err_t err;

    err.u64 = cvmx_read_csr(CVMX_POW_ECC_ERR);
    cvmx_write_csr(CVMX_POW_ECC_ERR, err.u64);
    if (err.u64)
    {
        if (err.s.sbe && __cvmx_interrupt_ecc_report_single_bit_errors)
            PRINT_ERROR("POW_ECC_ERR[SBE]: POW single bit error\n");
        if (err.s.dbe)
            PRINT_ERROR("POW_ECC_ERR[DBE]: POW double bit error\n");
        if (err.s.dbe || (err.s.sbe && __cvmx_interrupt_ecc_report_single_bit_errors))
            PRINT_ERROR("POW_ECC_ERR[SYN]: Failing syndrome %u\n", err.s.syn);
        if (err.s.rpe)
            PRINT_ERROR("POW_ECC_ERR[RPE]: Remote pointer error\n");
        if (err.s.iop & (1 << 0))
            PRINT_ERROR("POW_ECC_ERR[IOP0]: Received SWTAG/SWTAG_FULL/SWTAG_DESCH/DESCH/UPD_WQP from PP in NULL_NULL state\n");
        if (err.s.iop & (1 << 1))
            PRINT_ERROR("POW_ECC_ERR[IOP1]: Received SWTAG/SWTAG_DESCH/DESCH/UPD_WQP from PP in NULL state\n");
        if (err.s.iop & (1 << 2))
            PRINT_ERROR("POW_ECC_ERR[IOP2]: Received SWTAG/SWTAG_FULL/SWTAG_DESCH/GET_WORK from PP with pending tag switch to ORDERED or ATOMIC\n");
        if (err.s.iop & (1 << 3))
            PRINT_ERROR("POW_ECC_ERR[IOP3]: Received SWTAG/SWTAG_FULL/SWTAG_DESCH from PP with tag specified as NULL_NULL\n");
        if (err.s.iop & (1 << 4))
            PRINT_ERROR("POW_ECC_ERR[IOP4]: Received SWTAG_FULL/SWTAG_DESCH from PP with tag specified as NULL\n");
        if (err.s.iop & (1 << 5))
            PRINT_ERROR("POW_ECC_ERR[IOP5]: Received SWTAG/SWTAG_FULL/SWTAG_DESCH/DESCH/UPD_WQP/GET_WORK/NULL_RD from PP with GET_WORK pending\n");
        if (err.s.iop & (1 << 6))
            PRINT_ERROR("POW_ECC_ERR[IOP6]: Received SWTAG/SWTAG_FULL/SWTAG_DESCH/DESCH/UPD_WQP/GET_WORK/NULL_RD from PP with NULL_RD pending\n");
        if (err.s.iop & (1 << 7))
            PRINT_ERROR("POW_ECC_ERR[IOP7]: Received CLR_NSCHED from PP with SWTAG_DESCH/DESCH/CLR_NSCHED pending\n");
        if (err.s.iop & (1 << 8))
            PRINT_ERROR("POW_ECC_ERR[IOP8]: Received SWTAG/SWTAG_FULL/SWTAG_DESCH/DESCH/UPD_WQP/GET_WORK/NULL_RD from PP with CLR_NSCHED pending\n");
        if (err.s.iop & (1 << 9))
            PRINT_ERROR("POW_ECC_ERR[IOP9]: Received illegal opcode\n");
        if (err.s.iop & (1 << 10))
            PRINT_ERROR("POW_ECC_ERR[IOP10]: Received ADD_WORK with tag specified as NULL_NULL\n");
        if (err.s.iop & (1 << 11))
            PRINT_ERROR("POW_ECC_ERR[IOP11]: Received DBG load from PP with DBG load pending\n");
        if (err.s.iop & (1 << 12))
            PRINT_ERROR("POW_ECC_ERR[IOP12]: Received CSR load from PP with CSR load pending\n");
    }
}


/**
 * Enable TIM tiemr wheel interrupts for all chips
 */
static void __cvmx_interrupt_tim_enable(void)
{
    cvmx_tim_reg_int_mask_t csr;
    csr.u64 = cvmx_read_csr(CVMX_TIM_REG_INT_MASK);
    csr.s.mask = 0xffff;
    cvmx_write_csr(CVMX_TIM_REG_INT_MASK, csr.u64);
}


/**
 * Decode TIM timer wheel interrupts
 */
static void __cvmx_interrupt_tim_decode(void)
{
    cvmx_tim_reg_error_t err;

    err.u64 = cvmx_read_csr(CVMX_TIM_REG_ERROR);
    cvmx_write_csr(CVMX_TIM_REG_ERROR, err.u64);
    if (err.u64)
    {
        int i;
        for (i = 0; i < 16; i++)
            if (err.s.mask & (1 << i))
                PRINT_ERROR("TIM_REG_ERROR[MASK]: Timer wheel %d error\n", i);
    }
}


/**
 * Utility function to decode Octeon's RSL_INT_BLOCKS interrupts
 * into error messages.
 */
void cvmx_interrupt_rsl_decode(void)
{
    uint64_t rsl_int_blocks;

    /* Reading the RSL interrupts is different between PCI and PCIe chips */
    if (octeon_has_feature(OCTEON_FEATURE_PCIE))
        rsl_int_blocks = cvmx_read_csr(CVMX_PEXP_NPEI_RSL_INT_BLOCKS);
    else
        rsl_int_blocks = cvmx_read_csr(CVMX_NPI_RSL_INT_BLOCKS);

    /* Not all chips support all error interrupts. This code assumes
        that unsupported interrupts always are zero */

    /* Bits 63-31 are unused on all chips */
    if (rsl_int_blocks & (1ull<<30)) __cvmx_interrupt_iob_int_sum_decode();
    if (rsl_int_blocks & (1ull<<29)) __cvmx_interrupt_lmcx_decode(1);
    if (rsl_int_blocks & (1ull<<28))
    {
        __cvmx_interrupt_agl_gmx_rxx_int_reg_decode(0);
        if (OCTEON_IS_MODEL(OCTEON_CN52XX))
            __cvmx_interrupt_agl_gmx_rxx_int_reg_decode(1);
    }
    /* Bit 27-24 are unused on all chips */
    if (rsl_int_blocks & (1ull<<23))
    {
        if (octeon_has_feature(OCTEON_FEATURE_PCIE))
        {
            __cvmx_interrupt_pcsx_intx_reg_decode(0, 1);
            __cvmx_interrupt_pcsx_intx_reg_decode(1, 1);
            __cvmx_interrupt_pcsx_intx_reg_decode(2, 1);
            __cvmx_interrupt_pcsx_intx_reg_decode(3, 1);
            __cvmx_interrupt_pcsxx_int_reg_decode(1);
        }
        else
            __cvmx_interrupt_asxx_decode(1);
    }
    if (rsl_int_blocks & (1ull<<22))
    {
        if (octeon_has_feature(OCTEON_FEATURE_PCIE))
        {
            __cvmx_interrupt_pcsx_intx_reg_decode(0, 0);
            __cvmx_interrupt_pcsx_intx_reg_decode(1, 0);
            __cvmx_interrupt_pcsx_intx_reg_decode(2, 0);
            __cvmx_interrupt_pcsx_intx_reg_decode(3, 0);
            __cvmx_interrupt_pcsxx_int_reg_decode(0);
        }
        else
            __cvmx_interrupt_asxx_decode(0);
    }
    /* Bit 21 is unsed on all chips */
    if (rsl_int_blocks & (1ull<<20)) __cvmx_interrupt_pip_int_reg_decode();
    if (rsl_int_blocks & (1ull<<19))
    {
        __cvmx_interrupt_spxx_int_reg_decode(1);
        __cvmx_interrupt_stxx_int_reg_decode(1);
    }
    if (rsl_int_blocks & (1ull<<18))
    {
        __cvmx_interrupt_spxx_int_reg_decode(0);
        __cvmx_interrupt_stxx_int_reg_decode(0);
    }
    if (rsl_int_blocks & (1ull<<17)) __cvmx_interrupt_lmcx_decode(0);
    if (rsl_int_blocks & (1ull<<16)) __cvmx_interrupt_l2_decode();
    if (rsl_int_blocks & (1ull<<15)) __cvmx_interrupt_usbnx_int_sum_decode(1);
    if (rsl_int_blocks & (1ull<<14)) __cvmx_interrupt_rad_reg_error_decode();
    if (rsl_int_blocks & (1ull<<13)) __cvmx_interrupt_usbnx_int_sum_decode(0);
    if (rsl_int_blocks & (1ull<<12)) __cvmx_interrupt_pow_decode();
    if (rsl_int_blocks & (1ull<<11)) __cvmx_interrupt_tim_decode();
    if (rsl_int_blocks & (1ull<<10)) __cvmx_interrupt_pko_reg_error_decode();
    if (rsl_int_blocks & (1ull<< 9)) __cvmx_interrupt_ipd_int_sum_decode();
    /* Bit 8 is unused on all chips */
    if (rsl_int_blocks & (1ull<< 7)) __cvmx_interrupt_zip_error_decode();
    if (rsl_int_blocks & (1ull<< 6)) __cvmx_interrupt_dfa_decode();
    if (rsl_int_blocks & (1ull<< 5)) __cvmx_interrupt_fpa_int_sum_decode();
    if (rsl_int_blocks & (1ull<< 4)) __cvmx_interrupt_key_int_sum_decode();
    if (rsl_int_blocks & (1ull<< 3))
    {
        if (octeon_has_feature(OCTEON_FEATURE_PCIE))
            __cvmx_interrupt_npei_int_sum_decode();
        else
            __cvmx_interrupt_npi_int_sum_decode();
    }
    if (rsl_int_blocks & (1ull<< 2)) __cvmx_interrupt_gmxx_decode(1);
    if (rsl_int_blocks & (1ull<< 1)) __cvmx_interrupt_gmxx_decode(0);
    if (rsl_int_blocks & (1ull<< 0)) __cvmx_interrupt_mio_boot_err_decode();
}


/**
 * Enable GMX error reporting for the supplied interface
 *
 * @param interface Interface to enable
 */
static void __cvmx_interrupt_gmxx_enable(int interface)
{
    cvmx_gmxx_inf_mode_t mode;
    cvmx_gmxx_tx_int_en_t gmx_tx_int_en;
    int num_ports;
    int index;

    mode.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(interface));

    if (OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        if (mode.s.en)
        {
            switch(mode.cn56xx.mode)
            {
                case 1: /* XAUI */
                    num_ports = 1;
                    break;
                case 2: /* SGMII */
                case 3: /* PICMG */
                    num_ports = 4;
                    break;
                default: /* Disabled */
                    num_ports = 0;
                    break;
            }
        }
        else
            num_ports = 0;
    }
    else
    {
        if (mode.s.en)
        {
            if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX))
            {
                /* SPI on CN38XX and CN58XX report all errors through port 0.
                    RGMII needs to check all 4 ports */
                if (mode.s.type)
                    num_ports = 1;
                else
                    num_ports = 4;
            }
            else
            {
                /* CN30XX, CN31XX, and CN50XX have two or three ports. GMII
                    and MII has 2, RGMII has three */
                if (mode.s.type)
                    num_ports = 2;
                else
                    num_ports = 3;
            }
        }
        else
            num_ports = 0;
    }

    gmx_tx_int_en.u64 = 0;
    if (num_ports)
    {
        gmx_tx_int_en.s.ncb_nxa = 1;
        gmx_tx_int_en.s.pko_nxa = 1;
    }
    gmx_tx_int_en.s.undflw = (1<<num_ports)-1;
    cvmx_write_csr(CVMX_GMXX_TX_INT_EN(interface), gmx_tx_int_en.u64);
    for (index=0; index<num_ports;index++)
        __cvmx_interrupt_gmxx_rxx_int_en_enable(index, interface);
}


/**
 * Utility function to enable all RSL error interupts
 */
void cvmx_interrupt_rsl_enable(void)
{
    /* Bits 63-31 are unused on all chips */
    __cvmx_interrupt_iob_int_enb_enable();
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
        __cvmx_interrupt_lmcx_enable(1);
    if (octeon_has_feature(OCTEON_FEATURE_MGMT_PORT))
    {
        // FIXME __cvmx_interrupt_agl_gmx_rxx_int_en_enable(0);
        //if (OCTEON_IS_MODEL(OCTEON_CN52XX))
        //    __cvmx_interrupt_agl_gmx_rxx_int_en_enable(1);
    }
    /* Bit 27-24 are unused on all chips */
    if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX))
        __cvmx_interrupt_asxx_enable(1);
    if (OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        __cvmx_interrupt_pcsx_intx_en_reg_enable(0, 1);
        __cvmx_interrupt_pcsx_intx_en_reg_enable(1, 1);
        __cvmx_interrupt_pcsx_intx_en_reg_enable(2, 1);
        __cvmx_interrupt_pcsx_intx_en_reg_enable(3, 1);
        __cvmx_interrupt_pcsxx_int_en_reg_enable(1);
    }
    if (octeon_has_feature(OCTEON_FEATURE_PCIE))
    {
        __cvmx_interrupt_pcsx_intx_en_reg_enable(0, 0);
        __cvmx_interrupt_pcsx_intx_en_reg_enable(1, 0);
        __cvmx_interrupt_pcsx_intx_en_reg_enable(2, 0);
        __cvmx_interrupt_pcsx_intx_en_reg_enable(3, 0);
        __cvmx_interrupt_pcsxx_int_en_reg_enable(0);
    }
    else
        __cvmx_interrupt_asxx_enable(0);
    /* Bit 21 is unsed on all chips */
    __cvmx_interrupt_pip_int_en_enable();
    if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX))
    {
        __cvmx_interrupt_spxx_int_msk_enable(1);
        __cvmx_interrupt_stxx_int_msk_enable(1);
        __cvmx_interrupt_spxx_int_msk_enable(0);
        __cvmx_interrupt_stxx_int_msk_enable(0);
    }
    __cvmx_interrupt_lmcx_enable(0);
    __cvmx_interrupt_l2_enable();
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
        __cvmx_interrupt_usbnx_int_enb_enable(1);
    if (octeon_has_feature(OCTEON_FEATURE_RAID))
        __cvmx_interrupt_rad_reg_int_mask_enable();
    if (octeon_has_feature(OCTEON_FEATURE_USB))
        __cvmx_interrupt_usbnx_int_enb_enable(0);
    __cvmx_interrupt_pow_enable();
    __cvmx_interrupt_tim_enable();
    __cvmx_interrupt_pko_reg_int_mask_enable();
    __cvmx_interrupt_ipd_int_enb_enable();
    /* Bit 8 is unused on all chips */
    if (octeon_has_feature(OCTEON_FEATURE_ZIP))
        __cvmx_interrupt_zip_int_mask_enable();
    if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX))
        __cvmx_interrupt_dfa_enable();
    __cvmx_interrupt_fpa_int_enb_enable();
    if (octeon_has_feature(OCTEON_FEATURE_KEY_MEMORY))
        __cvmx_interrupt_key_int_enb_enable();
    if (octeon_has_feature(OCTEON_FEATURE_PCIE))
    {
        cvmx_ciu_soft_prst_t ciu_soft_prst;
        ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
        if (ciu_soft_prst.s.soft_prst == 0)
            __cvmx_interrupt_npei_int_enb2_enable();
    }
    else if (cvmx_sysinfo_get()->bootloader_config_flags & CVMX_BOOTINFO_CFG_FLAG_PCI_HOST)
        __cvmx_interrupt_npi_int_enb_enable();

    if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX) ||
        OCTEON_IS_MODEL(OCTEON_CN56XX))
        __cvmx_interrupt_gmxx_enable(1);
    __cvmx_interrupt_gmxx_enable(0);

    __cvmx_interrupt_mio_boot_int_enable();
}

