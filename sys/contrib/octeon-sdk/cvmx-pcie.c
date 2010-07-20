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
 * Interface to PCIe as a host(RC) or target(EP)
 *
 * <hr>$Revision: 41586 $<hr>
 */
#include "cvmx.h"
#include "cvmx-csr-db.h"
#include "cvmx-pcie.h"
#include "cvmx-sysinfo.h"
#include "cvmx-swap.h"
#include "cvmx-wqe.h"
#include "cvmx-helper-errata.h"


/**
 * Return the Core virtual base address for PCIe IO access. IOs are
 * read/written as an offset from this address.
 *
 * @param pcie_port PCIe port the IO is for
 *
 * @return 64bit Octeon IO base address for read/write
 */
uint64_t cvmx_pcie_get_io_base_address(int pcie_port)
{
    cvmx_pcie_address_t pcie_addr;
    pcie_addr.u64 = 0;
    pcie_addr.io.upper = 0;
    pcie_addr.io.io = 1;
    pcie_addr.io.did = 3;
    pcie_addr.io.subdid = 2;
    pcie_addr.io.es = 1;
    pcie_addr.io.port = pcie_port;
    return pcie_addr.u64;
}


/**
 * Size of the IO address region returned at address
 * cvmx_pcie_get_io_base_address()
 *
 * @param pcie_port PCIe port the IO is for
 *
 * @return Size of the IO window
 */
uint64_t cvmx_pcie_get_io_size(int pcie_port)
{
    return 1ull<<32;
}


/**
 * Return the Core virtual base address for PCIe MEM access. Memory is
 * read/written as an offset from this address.
 *
 * @param pcie_port PCIe port the IO is for
 *
 * @return 64bit Octeon IO base address for read/write
 */
uint64_t cvmx_pcie_get_mem_base_address(int pcie_port)
{
    cvmx_pcie_address_t pcie_addr;
    pcie_addr.u64 = 0;
    pcie_addr.mem.upper = 0;
    pcie_addr.mem.io = 1;
    pcie_addr.mem.did = 3;
    pcie_addr.mem.subdid = 3 + pcie_port;
    return pcie_addr.u64;
}


/**
 * Size of the Mem address region returned at address
 * cvmx_pcie_get_mem_base_address()
 *
 * @param pcie_port PCIe port the IO is for
 *
 * @return Size of the Mem window
 */
uint64_t cvmx_pcie_get_mem_size(int pcie_port)
{
    return 1ull<<36;
}


/**
 * @INTERNAL
 * Initialize the RC config space CSRs
 *
 * @param pcie_port PCIe port to initialize
 */
static void __cvmx_pcie_rc_initialize_config_space(int pcie_port)
{
    /* Max Payload Size (PCIE*_CFG030[MPS]) */
    /* Max Read Request Size (PCIE*_CFG030[MRRS]) */
    /* Relaxed-order, no-snoop enables (PCIE*_CFG030[RO_EN,NS_EN] */
    /* Error Message Enables (PCIE*_CFG030[CE_EN,NFE_EN,FE_EN,UR_EN]) */
    {
        cvmx_pciercx_cfg030_t pciercx_cfg030;
        pciercx_cfg030.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG030(pcie_port));
        pciercx_cfg030.s.mps = 0; /* Max payload size = 128 bytes for best Octeon DMA performance */
        pciercx_cfg030.s.mrrs = 0; /* Max read request size = 128 bytes for best Octeon DMA performance */
        pciercx_cfg030.s.ro_en = 1; /* Enable relaxed order processing. This will allow devices to affect read response ordering */
        pciercx_cfg030.s.ns_en = 1; /* Enable no snoop processing. Not used by Octeon */
        pciercx_cfg030.s.ce_en = 1; /* Correctable error reporting enable. */
        pciercx_cfg030.s.nfe_en = 1; /* Non-fatal error reporting enable. */
        pciercx_cfg030.s.fe_en = 1; /* Fatal error reporting enable. */
        pciercx_cfg030.s.ur_en = 1; /* Unsupported request reporting enable. */
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG030(pcie_port), pciercx_cfg030.u32);
    }

    /* Max Payload Size (NPEI_CTL_STATUS2[MPS]) must match PCIE*_CFG030[MPS] */
    /* Max Read Request Size (NPEI_CTL_STATUS2[MRRS]) must not exceed PCIE*_CFG030[MRRS] */
    {
        cvmx_npei_ctl_status2_t npei_ctl_status2;
        npei_ctl_status2.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_CTL_STATUS2);
        npei_ctl_status2.s.mps = 0; /* Max payload size = 128 bytes for best Octeon DMA performance */
        npei_ctl_status2.s.mrrs = 0; /* Max read request size = 128 bytes for best Octeon DMA performance */
        cvmx_write_csr(CVMX_PEXP_NPEI_CTL_STATUS2, npei_ctl_status2.u64);
    }

    /* ECRC Generation (PCIE*_CFG070[GE,CE]) */
    {
        cvmx_pciercx_cfg070_t pciercx_cfg070;
        pciercx_cfg070.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG070(pcie_port));
        pciercx_cfg070.s.ge = 1; /* ECRC generation enable. */
        pciercx_cfg070.s.ce = 1; /* ECRC check enable. */
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG070(pcie_port), pciercx_cfg070.u32);
    }

    /* Access Enables (PCIE*_CFG001[MSAE,ME]) */
        /* ME and MSAE should always be set. */
    /* Interrupt Disable (PCIE*_CFG001[I_DIS]) */
    /* System Error Message Enable (PCIE*_CFG001[SEE]) */
    {
        cvmx_pciercx_cfg001_t pciercx_cfg001;
        pciercx_cfg001.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG001(pcie_port));
        pciercx_cfg001.s.msae = 1; /* Memory space enable. */
        pciercx_cfg001.s.me = 1; /* Bus master enable. */
        pciercx_cfg001.s.i_dis = 1; /* INTx assertion disable. */
        pciercx_cfg001.s.see = 1; /* SERR# enable */
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG001(pcie_port), pciercx_cfg001.u32);
    }


    /* Advanced Error Recovery Message Enables */
    /* (PCIE*_CFG066,PCIE*_CFG067,PCIE*_CFG069) */
    cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG066(pcie_port), 0);
    /* Use CVMX_PCIERCX_CFG067 hardware default */
    cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG069(pcie_port), 0);


    /* Active State Power Management (PCIE*_CFG032[ASLPC]) */
    {
        cvmx_pciercx_cfg032_t pciercx_cfg032;
        pciercx_cfg032.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG032(pcie_port));
        pciercx_cfg032.s.aslpc = 0; /* Active state Link PM control. */
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG032(pcie_port), pciercx_cfg032.u32);
    }

    /* Entrance Latencies (PCIE*_CFG451[L0EL,L1EL]) */
    // FIXME: Anything needed here?

    /* Link Width Mode (PCIERCn_CFG452[LME]) - Set during cvmx_pcie_rc_initialize_link() */
    /* Primary Bus Number (PCIERCn_CFG006[PBNUM]) */
    {
        /* We set the primary bus number to 1 so IDT bridges are happy. They don't like zero */
        cvmx_pciercx_cfg006_t pciercx_cfg006;
        pciercx_cfg006.u32 = 0;
        pciercx_cfg006.s.pbnum = 1;
        pciercx_cfg006.s.sbnum = 1;
        pciercx_cfg006.s.subbnum = 1;
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG006(pcie_port), pciercx_cfg006.u32);
    }

    /* Memory-mapped I/O BAR (PCIERCn_CFG008) */
    /* Most applications should disable the memory-mapped I/O BAR by */
    /* setting PCIERCn_CFG008[ML_ADDR] < PCIERCn_CFG008[MB_ADDR] */
    {
        cvmx_pciercx_cfg008_t pciercx_cfg008;
        pciercx_cfg008.u32 = 0;
        pciercx_cfg008.s.mb_addr = 0x100;
        pciercx_cfg008.s.ml_addr = 0;
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG008(pcie_port), pciercx_cfg008.u32);
    }

    /* Prefetchable BAR (PCIERCn_CFG009,PCIERCn_CFG010,PCIERCn_CFG011) */
    /* Most applications should disable the prefetchable BAR by setting */
    /* PCIERCn_CFG011[UMEM_LIMIT],PCIERCn_CFG009[LMEM_LIMIT] < */
    /* PCIERCn_CFG010[UMEM_BASE],PCIERCn_CFG009[LMEM_BASE] */
    {
        cvmx_pciercx_cfg009_t pciercx_cfg009;
        cvmx_pciercx_cfg010_t pciercx_cfg010;
        cvmx_pciercx_cfg011_t pciercx_cfg011;
        pciercx_cfg009.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG009(pcie_port));
        pciercx_cfg010.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG010(pcie_port));
        pciercx_cfg011.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG011(pcie_port));
        pciercx_cfg009.s.lmem_base = 0x100;
        pciercx_cfg009.s.lmem_limit = 0;
        pciercx_cfg010.s.umem_base = 0x100;
        pciercx_cfg011.s.umem_limit = 0;
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG009(pcie_port), pciercx_cfg009.u32);
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG010(pcie_port), pciercx_cfg010.u32);
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG011(pcie_port), pciercx_cfg011.u32);
    }

    /* System Error Interrupt Enables (PCIERCn_CFG035[SECEE,SEFEE,SENFEE]) */
    /* PME Interrupt Enables (PCIERCn_CFG035[PMEIE]) */
    {
        cvmx_pciercx_cfg035_t pciercx_cfg035;
        pciercx_cfg035.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG035(pcie_port));
        pciercx_cfg035.s.secee = 1; /* System error on correctable error enable. */
        pciercx_cfg035.s.sefee = 1; /* System error on fatal error enable. */
        pciercx_cfg035.s.senfee = 1; /* System error on non-fatal error enable. */
        pciercx_cfg035.s.pmeie = 1; /* PME interrupt enable. */
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG035(pcie_port), pciercx_cfg035.u32);
    }

    /* Advanced Error Recovery Interrupt Enables */
    /* (PCIERCn_CFG075[CERE,NFERE,FERE]) */
    {
        cvmx_pciercx_cfg075_t pciercx_cfg075;
        pciercx_cfg075.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG075(pcie_port));
        pciercx_cfg075.s.cere = 1; /* Correctable error reporting enable. */
        pciercx_cfg075.s.nfere = 1; /* Non-fatal error reporting enable. */
        pciercx_cfg075.s.fere = 1; /* Fatal error reporting enable. */
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG075(pcie_port), pciercx_cfg075.u32);
    }

    /* HP Interrupt Enables (PCIERCn_CFG034[HPINT_EN], */
    /* PCIERCn_CFG034[DLLS_EN,CCINT_EN]) */
    {
        cvmx_pciercx_cfg034_t pciercx_cfg034;
        pciercx_cfg034.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG034(pcie_port));
        pciercx_cfg034.s.hpint_en = 1; /* Hot-plug interrupt enable. */
        pciercx_cfg034.s.dlls_en = 1; /* Data Link Layer state changed enable */
        pciercx_cfg034.s.ccint_en = 1; /* Command completed interrupt enable. */
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG034(pcie_port), pciercx_cfg034.u32);
    }
}


/**
 * @INTERNAL
 * Initialize a host mode PCIe link. This function takes a PCIe
 * port from reset to a link up state. Software can then begin
 * configuring the rest of the link.
 *
 * @param pcie_port PCIe port to initialize
 *
 * @return Zero on success
 */
static int __cvmx_pcie_rc_initialize_link(int pcie_port)
{
    uint64_t start_cycle;
    cvmx_pescx_ctl_status_t pescx_ctl_status;
    cvmx_pciercx_cfg452_t pciercx_cfg452;
    cvmx_pciercx_cfg032_t pciercx_cfg032;
    cvmx_pciercx_cfg448_t pciercx_cfg448;

    /* Set the lane width */
    pciercx_cfg452.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG452(pcie_port));
    pescx_ctl_status.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS(pcie_port));
    if (pescx_ctl_status.s.qlm_cfg == 0)
    {
        /* We're in 8 lane (56XX) or 4 lane (54XX) mode */
        pciercx_cfg452.s.lme = 0xf;
    }
    else
    {
        /* We're in 4 lane (56XX) or 2 lane (52XX) mode */
        pciercx_cfg452.s.lme = 0x7;
    }
    cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG452(pcie_port), pciercx_cfg452.u32);

    /* CN52XX pass 1.x has an errata where length mismatches on UR responses can
        cause bus errors on 64bit memory reads. Turning off length error
        checking fixes this */
    if (OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X))
    {
        cvmx_pciercx_cfg455_t pciercx_cfg455;
        pciercx_cfg455.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG455(pcie_port));
        pciercx_cfg455.s.m_cpl_len_err = 1;
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG455(pcie_port), pciercx_cfg455.u32);
    }

    /* Lane swap needs to be manually enabled for CN52XX */
    if (OCTEON_IS_MODEL(OCTEON_CN52XX) && (pcie_port == 1))
    {
      pescx_ctl_status.s.lane_swp = 1;
      cvmx_write_csr(CVMX_PESCX_CTL_STATUS(pcie_port),pescx_ctl_status.u64);
    }

    /* Bring up the link */
    pescx_ctl_status.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS(pcie_port));
    pescx_ctl_status.s.lnk_enb = 1;
    cvmx_write_csr(CVMX_PESCX_CTL_STATUS(pcie_port), pescx_ctl_status.u64);

    /* CN52XX pass 1.0: Due to a bug in 2nd order CDR, it needs to be disabled */
    if (OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_0))
        __cvmx_helper_errata_qlm_disable_2nd_order_cdr(0);

    /* Wait for the link to come up */
    start_cycle = cvmx_get_cycle();
    do
    {
        if (cvmx_get_cycle() - start_cycle > 2*cvmx_sysinfo_get()->cpu_clock_hz)
        {
            cvmx_dprintf("PCIe: Port %d link timeout\n", pcie_port);
            return -1;
        }
        cvmx_wait(10000);
        pciercx_cfg032.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG032(pcie_port));
    } while (pciercx_cfg032.s.dlla == 0);

    /* Update the Replay Time Limit. Empirically, some PCIe devices take a
        little longer to respond than expected under load. As a workaround for
        this we configure the Replay Time Limit to the value expected for a 512
        byte MPS instead of our actual 256 byte MPS. The numbers below are
        directly from the PCIe spec table 3-4 */
    pciercx_cfg448.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG448(pcie_port));
    switch (pciercx_cfg032.s.nlw)
    {
        case 1: /* 1 lane */
            pciercx_cfg448.s.rtl = 1677;
            break;
        case 2: /* 2 lanes */
            pciercx_cfg448.s.rtl = 867;
            break;
        case 4: /* 4 lanes */
            pciercx_cfg448.s.rtl = 462;
            break;
        case 8: /* 8 lanes */
            pciercx_cfg448.s.rtl = 258;
            break;
    }
    cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG448(pcie_port), pciercx_cfg448.u32);

    return 0;
}


/**
 * Initialize a PCIe port for use in host(RC) mode. It doesn't enumerate the bus.
 *
 * @param pcie_port PCIe port to initialize
 *
 * @return Zero on success
 */
int cvmx_pcie_rc_initialize(int pcie_port)
{
    int i;
    cvmx_ciu_soft_prst_t ciu_soft_prst;
    cvmx_pescx_bist_status_t pescx_bist_status;
    cvmx_pescx_bist_status2_t pescx_bist_status2;
    cvmx_npei_ctl_status_t npei_ctl_status;
    cvmx_npei_mem_access_ctl_t npei_mem_access_ctl;
    cvmx_npei_mem_access_subidx_t mem_access_subid;
    cvmx_npei_dbg_data_t npei_dbg_data;
    cvmx_pescx_ctl_status2_t pescx_ctl_status2;
    cvmx_pciercx_cfg032_t pciercx_cfg032;

retry:
    /* Make sure we aren't trying to setup a target mode interface in host mode */
    npei_ctl_status.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_CTL_STATUS);
    if ((pcie_port==0) && !npei_ctl_status.s.host_mode)
    {
        cvmx_dprintf("PCIe: ERROR: cvmx_pcie_rc_initialize() called on port0, but port0 is not in host mode\n");
        return -1;
    }

    /* Make sure a CN52XX isn't trying to bring up port 1 when it is disabled */
    if (OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        npei_dbg_data.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
        if ((pcie_port==1) && npei_dbg_data.cn52xx.qlm0_link_width)
        {
            cvmx_dprintf("PCIe: ERROR: cvmx_pcie_rc_initialize() called on port1, but port1 is disabled\n");
            return -1;
        }
    }

    /* PCIe switch arbitration mode. '0' == fixed priority NPEI, PCIe0, then PCIe1. '1' == round robin. */
    npei_ctl_status.s.arb = 1;
    /* Allow up to 0x20 config retries */
    npei_ctl_status.s.cfg_rtry = 0x20;
    /* CN52XX pass1.x has an errata where P0_NTAGS and P1_NTAGS don't reset */
    if (OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X))
    {
        npei_ctl_status.s.p0_ntags = 0x20;
        npei_ctl_status.s.p1_ntags = 0x20;
    }
    cvmx_write_csr(CVMX_PEXP_NPEI_CTL_STATUS, npei_ctl_status.u64);

    /* Bring the PCIe out of reset */
    if (cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_EBH5200)
    {
        /* The EBH5200 board swapped the PCIe reset lines on the board. As a
            workaround for this bug, we bring both PCIe ports out of reset at
            the same time instead of on separate calls. So for port 0, we bring
            both out of reset and do nothing on port 1 */
        if (pcie_port == 0)
        {
            ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
            /* After a chip reset the PCIe will also be in reset. If it isn't,
                most likely someone is trying to init it again without a proper
                PCIe reset */
            if (ciu_soft_prst.s.soft_prst == 0)
            {
		/* Reset the ports */
		ciu_soft_prst.s.soft_prst = 1;
		cvmx_write_csr(CVMX_CIU_SOFT_PRST, ciu_soft_prst.u64);
		ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
		ciu_soft_prst.s.soft_prst = 1;
		cvmx_write_csr(CVMX_CIU_SOFT_PRST1, ciu_soft_prst.u64);
		/* Wait until pcie resets the ports. */
		cvmx_wait_usec(2000);
            }
            ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
            ciu_soft_prst.s.soft_prst = 0;
            cvmx_write_csr(CVMX_CIU_SOFT_PRST1, ciu_soft_prst.u64);
            ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
            ciu_soft_prst.s.soft_prst = 0;
            cvmx_write_csr(CVMX_CIU_SOFT_PRST, ciu_soft_prst.u64);
        }
    }
    else
    {
        /* The normal case: The PCIe ports are completely separate and can be
            brought out of reset independently */
        if (pcie_port)
            ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
        else
            ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
        /* After a chip reset the PCIe will also be in reset. If it isn't,
            most likely someone is trying to init it again without a proper
            PCIe reset */
        if (ciu_soft_prst.s.soft_prst == 0)
        {
	    /* Reset the port */
	    ciu_soft_prst.s.soft_prst = 1;
	    if (pcie_port)
		cvmx_write_csr(CVMX_CIU_SOFT_PRST1, ciu_soft_prst.u64);
 	    else
		cvmx_write_csr(CVMX_CIU_SOFT_PRST, ciu_soft_prst.u64);
	    /* Wait until pcie resets the ports. */
	    cvmx_wait_usec(2000);
        }
        if (pcie_port)
        {
            ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
            ciu_soft_prst.s.soft_prst = 0;
            cvmx_write_csr(CVMX_CIU_SOFT_PRST1, ciu_soft_prst.u64);
        }
        else
        {
            ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
            ciu_soft_prst.s.soft_prst = 0;
            cvmx_write_csr(CVMX_CIU_SOFT_PRST, ciu_soft_prst.u64);
        }
    }

    /* Wait for PCIe reset to complete. Due to errata PCIE-700, we don't poll
       PESCX_CTL_STATUS2[PCIERST], but simply wait a fixed number of cycles */
    cvmx_wait(400000);

    /* PESCX_BIST_STATUS2[PCLK_RUN] was missing on pass 1 of CN56XX and
        CN52XX, so we only probe it on newer chips */
    if (!OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X) && !OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X))
    {
        /* Clear PCLK_RUN so we can check if the clock is running */
        pescx_ctl_status2.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS2(pcie_port));
        pescx_ctl_status2.s.pclk_run = 1;
        cvmx_write_csr(CVMX_PESCX_CTL_STATUS2(pcie_port), pescx_ctl_status2.u64);
        /* Now that we cleared PCLK_RUN, wait for it to be set again telling
            us the clock is running */
        if (CVMX_WAIT_FOR_FIELD64(CVMX_PESCX_CTL_STATUS2(pcie_port),
            cvmx_pescx_ctl_status2_t, pclk_run, ==, 1, 10000))
        {
            cvmx_dprintf("PCIe: Port %d isn't clocked, skipping.\n", pcie_port);
            return -1;
        }
    }

    /* Check and make sure PCIe came out of reset. If it doesn't the board
        probably hasn't wired the clocks up and the interface should be
        skipped */
    pescx_ctl_status2.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS2(pcie_port));
    if (pescx_ctl_status2.s.pcierst)
    {
        cvmx_dprintf("PCIe: Port %d stuck in reset, skipping.\n", pcie_port);
        return -1;
    }

    /* Check BIST2 status. If any bits are set skip this interface. This
        is an attempt to catch PCIE-813 on pass 1 parts */
    pescx_bist_status2.u64 = cvmx_read_csr(CVMX_PESCX_BIST_STATUS2(pcie_port));
    if (pescx_bist_status2.u64)
    {
        cvmx_dprintf("PCIe: Port %d BIST2 failed. Most likely this port isn't hooked up, skipping.\n", pcie_port);
        return -1;
    }

    /* Check BIST status */
    pescx_bist_status.u64 = cvmx_read_csr(CVMX_PESCX_BIST_STATUS(pcie_port));
    if (pescx_bist_status.u64)
        cvmx_dprintf("PCIe: BIST FAILED for port %d (0x%016llx)\n", pcie_port, CAST64(pescx_bist_status.u64));

    /* Initialize the config space CSRs */
    __cvmx_pcie_rc_initialize_config_space(pcie_port);

    /* Bring the link up */
    if (__cvmx_pcie_rc_initialize_link(pcie_port))
    {
        cvmx_dprintf("PCIe: ERROR: cvmx_pcie_rc_initialize_link() failed\n");
        return -1;
    }

    /* Store merge control (NPEI_MEM_ACCESS_CTL[TIMER,MAX_WORD]) */
    npei_mem_access_ctl.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_MEM_ACCESS_CTL);
    npei_mem_access_ctl.s.max_word = 0;     /* Allow 16 words to combine */
    npei_mem_access_ctl.s.timer = 127;      /* Wait up to 127 cycles for more data */
    cvmx_write_csr(CVMX_PEXP_NPEI_MEM_ACCESS_CTL, npei_mem_access_ctl.u64);

    /* Setup Mem access SubDIDs */
    mem_access_subid.u64 = 0;
    mem_access_subid.s.port = pcie_port; /* Port the request is sent to. */
    mem_access_subid.s.nmerge = 1;  /* Due to an errata on pass 1 chips, no merging is allowed. */
    mem_access_subid.s.esr = 1;     /* Endian-swap for Reads. */
    mem_access_subid.s.esw = 1;     /* Endian-swap for Writes. */
    mem_access_subid.s.nsr = 0;     /* Enable Snooping for Reads. Octeon doesn't care, but devices might want this more conservative setting */
    mem_access_subid.s.nsw = 0;     /* Enable Snoop for Writes. */
    mem_access_subid.s.ror = 0;     /* Disable Relaxed Ordering for Reads. */
    mem_access_subid.s.row = 0;     /* Disable Relaxed Ordering for Writes. */
    mem_access_subid.s.ba = 0;      /* PCIe Adddress Bits <63:34>. */

    /* Setup mem access 12-15 for port 0, 16-19 for port 1, supplying 36 bits of address space */
    for (i=12 + pcie_port*4; i<16 + pcie_port*4; i++)
    {
        cvmx_write_csr(CVMX_PEXP_NPEI_MEM_ACCESS_SUBIDX(i), mem_access_subid.u64);
        mem_access_subid.s.ba += 1; /* Set each SUBID to extend the addressable range */
    }

    /* Disable the peer to peer forwarding register. This must be setup
        by the OS after it enumerates the bus and assigns addresses to the
        PCIe busses */
    for (i=0; i<4; i++)
    {
        cvmx_write_csr(CVMX_PESCX_P2P_BARX_START(i, pcie_port), -1);
        cvmx_write_csr(CVMX_PESCX_P2P_BARX_END(i, pcie_port), -1);
    }

    /* Set Octeon's BAR0 to decode 0-16KB. It overlaps with Bar2 */
    cvmx_write_csr(CVMX_PESCX_P2N_BAR0_START(pcie_port), 0);

    /* Disable Octeon's BAR1. It isn't needed in RC mode since BAR2
        maps all of memory. BAR2 also maps 256MB-512MB into the 2nd
        256MB of memory */
    cvmx_write_csr(CVMX_PESCX_P2N_BAR1_START(pcie_port), -1);

    /* Set Octeon's BAR2 to decode 0-2^39. Bar0 and Bar1 take precedence
        where they overlap. It also overlaps with the device addresses, so
        make sure the peer to peer forwarding is set right */
    cvmx_write_csr(CVMX_PESCX_P2N_BAR2_START(pcie_port), 0);

    /* Setup BAR2 attributes */
    /* Relaxed Ordering (NPEI_CTL_PORTn[PTLP_RO,CTLP_RO, WAIT_COM]) */
    /* ­ PTLP_RO,CTLP_RO should normally be set (except for debug). */
    /* ­ WAIT_COM=0 will likely work for all applications. */
    /* Load completion relaxed ordering (NPEI_CTL_PORTn[WAITL_COM]) */
    if (pcie_port)
    {
        cvmx_npei_ctl_port1_t npei_ctl_port;
        npei_ctl_port.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_CTL_PORT1);
        npei_ctl_port.s.bar2_enb = 1;
        npei_ctl_port.s.bar2_esx = 1;
        npei_ctl_port.s.bar2_cax = 0;
        npei_ctl_port.s.ptlp_ro = 1;
        npei_ctl_port.s.ctlp_ro = 1;
        npei_ctl_port.s.wait_com = 0;
        npei_ctl_port.s.waitl_com = 0;
        cvmx_write_csr(CVMX_PEXP_NPEI_CTL_PORT1, npei_ctl_port.u64);
    }
    else
    {
        cvmx_npei_ctl_port0_t npei_ctl_port;
        npei_ctl_port.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_CTL_PORT0);
        npei_ctl_port.s.bar2_enb = 1;
        npei_ctl_port.s.bar2_esx = 1;
        npei_ctl_port.s.bar2_cax = 0;
        npei_ctl_port.s.ptlp_ro = 1;
        npei_ctl_port.s.ctlp_ro = 1;
        npei_ctl_port.s.wait_com = 0;
        npei_ctl_port.s.waitl_com = 0;
        cvmx_write_csr(CVMX_PEXP_NPEI_CTL_PORT0, npei_ctl_port.u64);
    }

    /* Both pass 1 and pass 2 of CN52XX and CN56XX have an errata that causes
        TLP ordering to not be preserved after multiple PCIe port resets. This
        code detects this fault and corrects it by aligning the TLP counters
        properly. Another link reset is then performed. See PCIE-13340 */
    if (OCTEON_IS_MODEL(OCTEON_CN56XX_PASS2_X) || OCTEON_IS_MODEL(OCTEON_CN52XX_PASS2_X) ||
        OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X) || OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X))
    {
        cvmx_npei_dbg_data_t dbg_data;
        int old_in_fif_p_count;
        int in_fif_p_count;
        int out_p_count;
        int in_p_offset = (OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X) || OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X)) ? 4 : 1;
        int i;

        /* Choose a write address of 1MB. It should be harmless as all bars
            haven't been setup */
        uint64_t write_address = (cvmx_pcie_get_mem_base_address(pcie_port) + 0x100000) | (1ull<<63);

        /* Make sure at least in_p_offset have been executed before we try and
            read in_fif_p_count */
        i = in_p_offset;
        while (i--)
        {
            cvmx_write64_uint32(write_address, 0);
            cvmx_wait(10000);
        }

        /* Read the IN_FIF_P_COUNT from the debug select. IN_FIF_P_COUNT can be
            unstable sometimes so read it twice with a write between the reads.
            This way we can tell the value is good as it will increment by one
            due to the write */
        cvmx_write_csr(CVMX_PEXP_NPEI_DBG_SELECT, (pcie_port) ? 0xd7fc : 0xcffc);
        cvmx_read_csr(CVMX_PEXP_NPEI_DBG_SELECT);
        do
        {
            dbg_data.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
            old_in_fif_p_count = dbg_data.s.data & 0xff;
            cvmx_write64_uint32(write_address, 0);
            cvmx_wait(10000);
            dbg_data.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
            in_fif_p_count = dbg_data.s.data & 0xff;
        } while (in_fif_p_count != ((old_in_fif_p_count+1) & 0xff));

        /* Update in_fif_p_count for it's offset with respect to out_p_count */
        in_fif_p_count = (in_fif_p_count + in_p_offset) & 0xff;

        /* Read the OUT_P_COUNT from the debug select */
        cvmx_write_csr(CVMX_PEXP_NPEI_DBG_SELECT, (pcie_port) ? 0xd00f : 0xc80f);
        cvmx_read_csr(CVMX_PEXP_NPEI_DBG_SELECT);
        dbg_data.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
        out_p_count = (dbg_data.s.data>>1) & 0xff;

        /* Check that the two counters are aligned */
        if (out_p_count != in_fif_p_count)
        {
            cvmx_dprintf("PCIe: Port %d aligning TLP counters as workaround to maintain ordering\n", pcie_port);
            while (in_fif_p_count != 0)
            {
                cvmx_write64_uint32(write_address, 0);
                cvmx_wait(10000);
                in_fif_p_count = (in_fif_p_count + 1) & 0xff;
            }
            /* The EBH5200 board swapped the PCIe reset lines on the board. This
                means we must bring both links down and up, which will cause the
                PCIe0 to need alignment again. Lots of messages will be displayed,
                but everything should work */
            if ((cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_EBH5200) &&
                (pcie_port == 1))
                cvmx_pcie_rc_initialize(0);
            /* Rety bringing this port up */
            goto retry;
        }
    }

    /* Display the link status */
    pciercx_cfg032.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG032(pcie_port));
    cvmx_dprintf("PCIe: Port %d link active, %d lanes\n", pcie_port, pciercx_cfg032.s.nlw);

    return 0;
}


/**
 * Shutdown a PCIe port and put it in reset
 *
 * @param pcie_port PCIe port to shutdown
 *
 * @return Zero on success
 */
int cvmx_pcie_rc_shutdown(int pcie_port)
{
    /* Wait for all pending operations to complete */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_PESCX_CPL_LUT_VALID(pcie_port), cvmx_pescx_cpl_lut_valid_t, tag, ==, 0, 2000))
        cvmx_dprintf("PCIe: Port %d shutdown timeout\n", pcie_port);

    /* Force reset */
    if (pcie_port)
    {
        cvmx_ciu_soft_prst_t ciu_soft_prst;
        ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
        ciu_soft_prst.s.soft_prst = 1;
        cvmx_write_csr(CVMX_CIU_SOFT_PRST1, ciu_soft_prst.u64);
    }
    else
    {
        cvmx_ciu_soft_prst_t ciu_soft_prst;
        ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
        ciu_soft_prst.s.soft_prst = 1;
        cvmx_write_csr(CVMX_CIU_SOFT_PRST, ciu_soft_prst.u64);
    }
    return 0;
}


/**
 * @INTERNAL
 * Build a PCIe config space request address for a device
 *
 * @param pcie_port PCIe port to access
 * @param bus       Sub bus
 * @param dev       Device ID
 * @param fn        Device sub function
 * @param reg       Register to access
 *
 * @return 64bit Octeon IO address
 */
static inline uint64_t __cvmx_pcie_build_config_addr(int pcie_port, int bus, int dev, int fn, int reg)
{
    cvmx_pcie_address_t pcie_addr;
    cvmx_pciercx_cfg006_t pciercx_cfg006;

    pciercx_cfg006.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG006(pcie_port));
    if ((bus <= pciercx_cfg006.s.pbnum) && (dev != 0))
        return 0;

    pcie_addr.u64 = 0;
    pcie_addr.config.upper = 2;
    pcie_addr.config.io = 1;
    pcie_addr.config.did = 3;
    pcie_addr.config.subdid = 1;
    pcie_addr.config.es = 1;
    pcie_addr.config.port = pcie_port;
    pcie_addr.config.ty = (bus > pciercx_cfg006.s.pbnum);
    pcie_addr.config.bus = bus;
    pcie_addr.config.dev = dev;
    pcie_addr.config.func = fn;
    pcie_addr.config.reg = reg;
    return pcie_addr.u64;
}


/**
 * Read 8bits from a Device's config space
 *
 * @param pcie_port PCIe port the device is on
 * @param bus       Sub bus
 * @param dev       Device ID
 * @param fn        Device sub function
 * @param reg       Register to access
 *
 * @return Result of the read
 */
uint8_t cvmx_pcie_config_read8(int pcie_port, int bus, int dev, int fn, int reg)
{
    uint64_t address = __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
    if (address)
        return cvmx_read64_uint8(address);
    else
        return 0xff;
}


/**
 * Read 16bits from a Device's config space
 *
 * @param pcie_port PCIe port the device is on
 * @param bus       Sub bus
 * @param dev       Device ID
 * @param fn        Device sub function
 * @param reg       Register to access
 *
 * @return Result of the read
 */
uint16_t cvmx_pcie_config_read16(int pcie_port, int bus, int dev, int fn, int reg)
{
    uint64_t address = __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
    if (address)
        return cvmx_le16_to_cpu(cvmx_read64_uint16(address));
    else
        return 0xffff;
}


/**
 * Read 32bits from a Device's config space
 *
 * @param pcie_port PCIe port the device is on
 * @param bus       Sub bus
 * @param dev       Device ID
 * @param fn        Device sub function
 * @param reg       Register to access
 *
 * @return Result of the read
 */
uint32_t cvmx_pcie_config_read32(int pcie_port, int bus, int dev, int fn, int reg)
{
    uint64_t address = __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
    if (address)
        return cvmx_le32_to_cpu(cvmx_read64_uint32(address));
    else
        return 0xffffffff;
}


/**
 * Write 8bits to a Device's config space
 *
 * @param pcie_port PCIe port the device is on
 * @param bus       Sub bus
 * @param dev       Device ID
 * @param fn        Device sub function
 * @param reg       Register to access
 * @param val       Value to write
 */
void cvmx_pcie_config_write8(int pcie_port, int bus, int dev, int fn, int reg, uint8_t val)
{
    uint64_t address = __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
    if (address)
        cvmx_write64_uint8(address, val);
}


/**
 * Write 16bits to a Device's config space
 *
 * @param pcie_port PCIe port the device is on
 * @param bus       Sub bus
 * @param dev       Device ID
 * @param fn        Device sub function
 * @param reg       Register to access
 * @param val       Value to write
 */
void cvmx_pcie_config_write16(int pcie_port, int bus, int dev, int fn, int reg, uint16_t val)
{
    uint64_t address = __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
    if (address)
        cvmx_write64_uint16(address, cvmx_cpu_to_le16(val));
}


/**
 * Write 32bits to a Device's config space
 *
 * @param pcie_port PCIe port the device is on
 * @param bus       Sub bus
 * @param dev       Device ID
 * @param fn        Device sub function
 * @param reg       Register to access
 * @param val       Value to write
 */
void cvmx_pcie_config_write32(int pcie_port, int bus, int dev, int fn, int reg, uint32_t val)
{
    uint64_t address = __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
    if (address)
        cvmx_write64_uint32(address, cvmx_cpu_to_le32(val));
}


/**
 * Read a PCIe config space register indirectly. This is used for
 * registers of the form PCIEEP_CFG??? and PCIERC?_CFG???.
 *
 * @param pcie_port  PCIe port to read from
 * @param cfg_offset Address to read
 *
 * @return Value read
 */
uint32_t cvmx_pcie_cfgx_read(int pcie_port, uint32_t cfg_offset)
{
    cvmx_pescx_cfg_rd_t pescx_cfg_rd;
    pescx_cfg_rd.u64 = 0;
    pescx_cfg_rd.s.addr = cfg_offset;
    cvmx_write_csr(CVMX_PESCX_CFG_RD(pcie_port), pescx_cfg_rd.u64);
    pescx_cfg_rd.u64 = cvmx_read_csr(CVMX_PESCX_CFG_RD(pcie_port));
    return pescx_cfg_rd.s.data;
}


/**
 * Write a PCIe config space register indirectly. This is used for
 * registers of the form PCIEEP_CFG??? and PCIERC?_CFG???.
 *
 * @param pcie_port  PCIe port to write to
 * @param cfg_offset Address to write
 * @param val        Value to write
 */
void cvmx_pcie_cfgx_write(int pcie_port, uint32_t cfg_offset, uint32_t val)
{
    cvmx_pescx_cfg_wr_t pescx_cfg_wr;
    pescx_cfg_wr.u64 = 0;
    pescx_cfg_wr.s.addr = cfg_offset;
    pescx_cfg_wr.s.data = val;
    cvmx_write_csr(CVMX_PESCX_CFG_WR(pcie_port), pescx_cfg_wr.u64);
}


/**
 * Initialize a PCIe port for use in target(EP) mode.
 *
 * @return Zero on success
 */
int cvmx_pcie_ep_initialize(void)
{
    int pcie_port = 0;
    cvmx_npei_ctl_status_t npei_ctl_status;

    npei_ctl_status.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_CTL_STATUS);
    if (npei_ctl_status.s.host_mode)
        return -1;

    /* Enable bus master and memory */
    cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIEEP_CFG001, 0x6);

    /* Max Payload Size (PCIE*_CFG030[MPS]) */
    /* Max Read Request Size (PCIE*_CFG030[MRRS]) */
    /* Relaxed-order, no-snoop enables (PCIE*_CFG030[RO_EN,NS_EN] */
    /* Error Message Enables (PCIE*_CFG030[CE_EN,NFE_EN,FE_EN,UR_EN]) */
    {
        cvmx_pciercx_cfg030_t pciercx_cfg030;
        pciercx_cfg030.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG030(pcie_port));
        pciercx_cfg030.s.mps = 0; /* Max payload size = 128 bytes (Limit of most PCs) */
        pciercx_cfg030.s.mrrs = 0; /* Max read request size = 128 bytes for best Octeon DMA performance */
        pciercx_cfg030.s.ro_en = 1; /* Enable relaxed ordering. */
        pciercx_cfg030.s.ns_en = 1; /* Enable no snoop. */
        pciercx_cfg030.s.ce_en = 1; /* Correctable error reporting enable. */
        pciercx_cfg030.s.nfe_en = 1; /* Non-fatal error reporting enable. */
        pciercx_cfg030.s.fe_en = 1; /* Fatal error reporting enable. */
        pciercx_cfg030.s.ur_en = 1; /* Unsupported request reporting enable. */
        cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG030(pcie_port), pciercx_cfg030.u32);
    }

    /* Max Payload Size (NPEI_CTL_STATUS2[MPS]) must match PCIE*_CFG030[MPS] */
    /* Max Read Request Size (NPEI_CTL_STATUS2[MRRS]) must not exceed PCIE*_CFG030[MRRS] */
    {
        cvmx_npei_ctl_status2_t npei_ctl_status2;
        npei_ctl_status2.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_CTL_STATUS2);
        npei_ctl_status2.s.mps = 0; /* Max payload size = 128 bytes (Limit of most PCs) */
        npei_ctl_status2.s.mrrs = 0; /* Max read request size = 128 bytes for best Octeon DMA performance */
        cvmx_write_csr(CVMX_PEXP_NPEI_CTL_STATUS2, npei_ctl_status2.u64);
    }

    /* Setup Mem access SubDID 12 to access Host memory */
    {
        cvmx_npei_mem_access_subidx_t mem_access_subid;
        mem_access_subid.u64 = 0;
        mem_access_subid.s.port = pcie_port; /* Port the request is sent to. */
        mem_access_subid.s.nmerge = 1;  /* Merging is allowed in this window. */
        mem_access_subid.s.esr = 0;     /* Endian-swap for Reads. */
        mem_access_subid.s.esw = 0;     /* Endian-swap for Writes. */
        mem_access_subid.s.nsr = 0;     /* Enable Snooping for Reads. Octeon doesn't care, but devices might want this more conservative setting */
        mem_access_subid.s.nsw = 0;     /* Enable Snoop for Writes. */
        mem_access_subid.s.ror = 0;     /* Disable Relaxed Ordering for Reads. */
        mem_access_subid.s.row = 0;     /* Disable Relaxed Ordering for Writes. */
        mem_access_subid.s.ba = 0;      /* PCIe Adddress Bits <63:34>. */
        cvmx_write_csr(CVMX_PEXP_NPEI_MEM_ACCESS_SUBIDX(12), mem_access_subid.u64);
    }
    return 0;
}


/**
 * Wait for posted PCIe read/writes to reach the other side of
 * the internal PCIe switch. This will insure that core
 * read/writes are posted before anything after this function
 * is called. This may be necessary when writing to memory that
 * will later be read using the DMA/PKT engines.
 *
 * @param pcie_port PCIe port to wait for
 */
void cvmx_pcie_wait_for_pending(int pcie_port)
{
    cvmx_npei_data_out_cnt_t npei_data_out_cnt;
    int a;
    int b;
    int c;

    /* See section 9.8, PCIe Core-initiated Requests, in the manual for a
        description of how this code works */
    npei_data_out_cnt.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DATA_OUT_CNT);
    if (pcie_port)
    {
        if (!npei_data_out_cnt.s.p1_fcnt)
            return;
        a = npei_data_out_cnt.s.p1_ucnt;
        b = (a + npei_data_out_cnt.s.p1_fcnt-1) & 0xffff;
    }
    else
    {
        if (!npei_data_out_cnt.s.p0_fcnt)
            return;
        a = npei_data_out_cnt.s.p0_ucnt;
        b = (a + npei_data_out_cnt.s.p0_fcnt-1) & 0xffff;
    }

    while (1)
    {
        npei_data_out_cnt.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DATA_OUT_CNT);
        c = (pcie_port) ? npei_data_out_cnt.s.p1_ucnt : npei_data_out_cnt.s.p0_ucnt;
        if (a<=b)
        {
            if ((c<a) || (c>b))
                return;
        }
        else
        {
            if ((c>b) && (c<a))
                return;
        }
    }
}

