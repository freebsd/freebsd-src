/***********************license start***************
 * Copyright (c) 2003-2010  Cavium Networks (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.

 *   * Neither the name of Cavium Networks nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR
 * DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM
 * SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE,
 * MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
 * VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR
 * PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 ***********************license end**************************************/



/**
 * @file
 *
 * Interface to SRIO
 *
 * <hr>$Revision: 41586 $<hr>
 */
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
#include <asm/octeon/cvmx.h>
#include <asm/octeon/cvmx-srio.h>
#include <asm/octeon/cvmx-clock.h>
#include <asm/octeon/cvmx-atomic.h>
#ifdef CONFIG_CAVIUM_DECODE_RSL
#include <asm/octeon/cvmx-error.h>
#endif
#include <asm/octeon/cvmx-sriox-defs.h>
#include <asm/octeon/cvmx-sriomaintx-defs.h>
#include <asm/octeon/cvmx-sli-defs.h>
#include <asm/octeon/cvmx-dpi-defs.h>
#include <asm/octeon/cvmx-pexp-defs.h>
#include <asm/octeon/cvmx-helper.h>
#else
#include "cvmx.h"
#include "cvmx-srio.h"
#include "cvmx-clock.h"
#include "cvmx-helper.h"
#ifndef CVMX_BUILD_FOR_LINUX_HOST
#include "cvmx-atomic.h"
#include "cvmx-error.h"
#include "cvmx-helper-errata.h"
#endif
#endif

#define CVMX_SRIO_USE_FIFO_FOR_MAINT    1
#define CVMX_SRIO_CONFIG_TIMEOUT        10000 /* 10ms */
#define CVMX_SRIO_DOORBELL_TIMEOUT      10000 /* 10ms */
#define CVMX_SRIO_CONFIG_PRIORITY       0
#define ULL unsigned long long

typedef union
{
    uint64_t    u64;
    struct
    {
#if __BYTE_ORDER == __BIG_ENDIAN
        uint64_t    upper           : 2;    /* Normally 2 for XKPHYS */
        uint64_t    reserved_49_61  : 13;   /* Must be zero */
        uint64_t    io              : 1;    /* 1 for IO space access */
        uint64_t    did             : 5;    /* DID = 3 */
        uint64_t    subdid          : 3;    /* SubDID = 3-6 */
        uint64_t    reserved_36_39  : 4;    /* Must be zero */
        uint64_t    se              : 2;    /* SubDID extender */
        uint64_t    reserved_32_33  : 2;    /* Must be zero */
        uint64_t    hopcount        : 8;    /* Hopcount */
        uint64_t    address         : 24;   /* Mem address */
#else
        uint64_t    address         : 24;
        uint64_t    hopcount        : 8;
        uint64_t    reserved_32_33  : 2;
        uint64_t    se              : 2;
        uint64_t    reserved_36_39  : 4;
        uint64_t    subdid          : 3;
        uint64_t    did             : 5;
        uint64_t    io              : 1;
        uint64_t    reserved_49_61  : 13;
        uint64_t    upper           : 2;
#endif
    } config;
    struct
    {
#if __BYTE_ORDER == __BIG_ENDIAN
        uint64_t    upper           : 2;    /* Normally 2 for XKPHYS */
        uint64_t    reserved_49_61  : 13;   /* Must be zero */
        uint64_t    io              : 1;    /* 1 for IO space access */
        uint64_t    did             : 5;    /* DID = 3 */
        uint64_t    subdid          : 3;    /* SubDID = 3-6 */
        uint64_t    reserved_36_39  : 4;    /* Must be zero */
        uint64_t    se              : 2;    /* SubDID extender */
        uint64_t    address         : 34;   /* Mem address */
#else
        uint64_t    address         : 34;
        uint64_t    se              : 2;
        uint64_t    reserved_36_39  : 4;
        uint64_t    subdid          : 3;
        uint64_t    did             : 5;
        uint64_t    io              : 1;
        uint64_t    reserved_49_61  : 13;
        uint64_t    upper           : 2;
#endif
    } mem;
} cvmx_sli_address_t;

typedef struct
{
    cvmx_srio_initialize_flags_t flags;
    int32_t subidx_ref_count[16];   /* Reference count for SLI_MEM_ACCESS_SUBID[12-27]. Index=X-12 */
    int32_t s2m_ref_count[16];   /* Reference count for SRIOX_S2M_TYPE[0-15]. */
} __cvmx_srio_state_t;

static CVMX_SHARED __cvmx_srio_state_t __cvmx_srio_state[2];


#ifndef CVMX_BUILD_FOR_LINUX_HOST
/**
 * @INTERNAL
 * Allocate a SRIOX_S2M_TYPEX register for mapping a remote SRIO
 * device's address range into Octeons SLI address space. Reference
 * counting is used to allow sharing of duplicate setups. The current
 * implementation treats reads and writes as paired, but this could be
 * changed if we have trouble running out of indexes.
 *
 * @param srio_port SRIO port device is on
 * @param s2m       SRIOX_S2M_TYPEX setup required
 *
 * @return Index of CSR, or negative on failure
 */
static int __cvmx_srio_alloc_s2m(int srio_port, cvmx_sriox_s2m_typex_t s2m)
{
    int s2m_index;
    /* Search through the S2M_TYPE registers looking for an unsed one or one
        setup the way we need it */
    for (s2m_index=0; s2m_index<16; s2m_index++)
    {
        /* Increment ref count by 2 since we count read and write
            independently. We might need a more complicated search in the
            future */
        int ref_count = cvmx_atomic_fetch_and_add32(&__cvmx_srio_state[srio_port].s2m_ref_count[s2m_index], 2);
        if (ref_count == 0)
        {
            /* Unused location. Write our value */
            cvmx_write_csr(CVMX_SRIOX_S2M_TYPEX(s2m_index, srio_port), s2m.u64);
            return s2m_index;
        }
        else
        {
            /* In use, see if we can use it */
            if (cvmx_read_csr(CVMX_SRIOX_S2M_TYPEX(s2m_index, srio_port)) == s2m.u64)
                return s2m_index;
            else
                cvmx_atomic_add32(&__cvmx_srio_state[srio_port].s2m_ref_count[s2m_index], -2);
        }
    }
    cvmx_dprintf("SRIO%d: Unable to find free SRIOX_S2M_TYPEX\n", srio_port);
    return -1;
}


/**
 * @INTERNAL
 * Free a handle allocated by __cvmx_srio_alloc_s2m
 *
 * @param srio_port SRIO port
 * @param index     Index to free
 */
static void __cvmx_srio_free_s2m(int srio_port, int index)
{
    /* Read to force pending transactions to complete */
    cvmx_read_csr(CVMX_SRIOX_S2M_TYPEX(index, srio_port));
    cvmx_atomic_add32(&__cvmx_srio_state[srio_port].s2m_ref_count[index], -2);
}


/**
 * @INTERNAL
 * Allocate a SLI SubID to map a region of memory. Reference
 * counting is used to allow sharing of duplicate setups.
 *
 * @param subid  SLI_MEM_ACCESS_SUBIDX we need an index for
 *
 * @return Index of CSR, or negative on failure
 */
static int __cvmx_srio_alloc_subid(cvmx_sli_mem_access_subidx_t subid)
{
    int mem_index;
    /* Search through the mem access subid registers looking for an unsed one
        or one setup the way we need it. PCIe uses the low indexes, so search
        backwards */
    for (mem_index=27; mem_index>=12; mem_index--)
    {
        int ref_count = cvmx_atomic_fetch_and_add32(&__cvmx_srio_state[0].subidx_ref_count[mem_index-12], 1);
        if (ref_count == 0)
        {
            /* Unused location. Write our value */
            cvmx_write_csr(CVMX_PEXP_SLI_MEM_ACCESS_SUBIDX(mem_index), subid.u64);
            return mem_index;
        }
        else
        {
            /* In use, see if we can use it */
            if (cvmx_read_csr(CVMX_PEXP_SLI_MEM_ACCESS_SUBIDX(mem_index)) == subid.u64)
                return mem_index;
            else
                cvmx_atomic_add32(&__cvmx_srio_state[0].subidx_ref_count[mem_index-12], -1);
        }
    }
    cvmx_dprintf("SRIO: Unable to find free SLI_MEM_ACCESS_SUBIDX\n");
    return -1;
}


/**
 * @INTERNAL
 * Free a handle allocated by __cvmx_srio_alloc_subid
 *
 * @param index  Index to free
 */
static void __cvmx_srio_free_subid(int index)
{
    /* Read to force pending transactions to complete */
    cvmx_read_csr(CVMX_PEXP_SLI_MEM_ACCESS_SUBIDX(index));
    cvmx_atomic_add32(&__cvmx_srio_state[0].subidx_ref_count[index-12], -1);
}
#endif


/**
 * @INTERNAL
 * Read 32bits from a local port
 *
 * @param srio_port SRIO port the device is on
 * @param offset    Offset in config space. This must be a multiple of 32 bits.
 * @param result    Result of the read. This will be unmodified on failure.
 *
 * @return Zero on success, negative on failure.
 */
static int __cvmx_srio_local_read32(int srio_port, uint32_t offset, uint32_t *result)
{
    cvmx_sriox_maint_op_t maint_op;
    cvmx_sriox_maint_rd_data_t maint_rd_data;
    maint_op.u64 = 0;
    maint_op.s.op = 0; /* Read */
    maint_op.s.addr = offset;

    /* Make sure SRIO isn't already busy */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_SRIOX_MAINT_OP(srio_port), cvmx_sriox_maint_op_t, pending, ==, 0, CVMX_SRIO_CONFIG_TIMEOUT))
    {
        cvmx_dprintf("SRIO%d: Pending bit stuck before config read\n", srio_port);
        return -1;
    }

    /* Issue the read to the hardware */
    cvmx_write_csr(CVMX_SRIOX_MAINT_OP(srio_port), maint_op.u64);

    /* Wait for the hardware to complete the operation */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_SRIOX_MAINT_OP(srio_port), cvmx_sriox_maint_op_t, pending, ==, 0, CVMX_SRIO_CONFIG_TIMEOUT))
    {
        cvmx_dprintf("SRIO%d: Config read timeout\n", srio_port);
        return -1;
    }

    /* Display and error and return if the operation failed to issue */
    maint_op.u64 = cvmx_read_csr(CVMX_SRIOX_MAINT_OP(srio_port));
    if (maint_op.s.fail)
    {
        cvmx_dprintf("SRIO%d: Config read addressing error (offset=0x%x)\n", srio_port, (unsigned int)offset);
        return -1;
    }

    /* Wait for the read data to become valid */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_SRIOX_MAINT_RD_DATA(srio_port), cvmx_sriox_maint_rd_data_t, valid, ==, 1, CVMX_SRIO_CONFIG_TIMEOUT))
    {
        cvmx_dprintf("SRIO%d: Config read data timeout\n", srio_port);
        return -1;
    }

    /* Get the read data */
    maint_rd_data.u64 = cvmx_read_csr(CVMX_SRIOX_MAINT_RD_DATA(srio_port));
    *result = maint_rd_data.s.rd_data;
    return 0;
}


/**
 * @INTERNAL
 * Write 32bits to a local port
 * @param srio_port SRIO port the device is on
 * @param offset    Offset in config space. This must be a multiple of 32 bits.
 * @param data      Data to write.
 *
 * @return Zero on success, negative on failure.
 */
static int __cvmx_srio_local_write32(int srio_port, uint32_t offset, uint32_t data)
{
    cvmx_sriox_maint_op_t maint_op;
    maint_op.u64 = 0;
    maint_op.s.wr_data = data;
    maint_op.s.op = 1; /* Write */
    maint_op.s.addr = offset;

    /* Make sure SRIO isn't already busy */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_SRIOX_MAINT_OP(srio_port), cvmx_sriox_maint_op_t, pending, ==, 0, CVMX_SRIO_CONFIG_TIMEOUT))
    {
        cvmx_dprintf("SRIO%d: Pending bit stuck before config write\n", srio_port);
        return -1;
    }

    /* Issue the write to the hardware */
    cvmx_write_csr(CVMX_SRIOX_MAINT_OP(srio_port), maint_op.u64);

    /* Wait for the hardware to complete the operation */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_SRIOX_MAINT_OP(srio_port), cvmx_sriox_maint_op_t, pending, ==, 0, CVMX_SRIO_CONFIG_TIMEOUT))
    {
        cvmx_dprintf("SRIO%d: Config write timeout\n", srio_port);
        return -1;
    }

    /* Display and error and return if the operation failed to issue */
    maint_op.u64 = cvmx_read_csr(CVMX_SRIOX_MAINT_OP(srio_port));
    if (maint_op.s.fail)
    {
        cvmx_dprintf("SRIO%d: Config write addressing error (offset=0x%x)\n", srio_port, (unsigned int)offset);
        return -1;
    }
    return 0;
}


/**
 * Initialize a SRIO port for use.
 *
 * @param srio_port SRIO port to initialize
 * @param flags     Optional flags
 *
 * @return Zero on success
 */
int cvmx_srio_initialize(int srio_port, cvmx_srio_initialize_flags_t flags)
{
    cvmx_sriomaintx_port_lt_ctl_t port_lt_ctl;
    cvmx_sriomaintx_port_rt_ctl_t port_rt_ctl;
    cvmx_sriomaintx_port_0_ctl_t port_0_ctl;
    cvmx_sriomaintx_core_enables_t core_enables;
    cvmx_sriomaintx_port_gen_ctl_t port_gen_ctl;
    cvmx_sriox_status_reg_t sriox_status_reg;
    cvmx_mio_rst_ctlx_t mio_rst_ctl;
    cvmx_sriox_imsg_vport_thr_t sriox_imsg_vport_thr;
    cvmx_dpi_sli_prtx_cfg_t prt_cfg;
    cvmx_sli_s2m_portx_ctl_t sli_s2m_portx_ctl;

    sriox_status_reg.u64 = cvmx_read_csr(CVMX_SRIOX_STATUS_REG(srio_port));
    if (!sriox_status_reg.s.srio)
    {
        cvmx_dprintf("SRIO%d: Initialization called on a port not in SRIO mode\n", srio_port);
        return -1;
    }

    __cvmx_srio_state[srio_port].flags = flags;

    /* CN63XX Pass 1.0 errata G-14395 requires the QLM De-emphasis be
        programmed */
    if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_0))
    {
        if (srio_port)
        {
            cvmx_ciu_qlm1_t ciu_qlm;
            ciu_qlm.u64 = cvmx_read_csr(CVMX_CIU_QLM1);
            ciu_qlm.s.txbypass = 1;
            ciu_qlm.s.txdeemph = 5;
            ciu_qlm.s.txmargin = 0x17;
            cvmx_write_csr(CVMX_CIU_QLM1, ciu_qlm.u64);
        }
        else
        {
            cvmx_ciu_qlm0_t ciu_qlm;
            ciu_qlm.u64 = cvmx_read_csr(CVMX_CIU_QLM0);
            ciu_qlm.s.txbypass = 1;
            ciu_qlm.s.txdeemph = 5;
            ciu_qlm.s.txmargin = 0x17;
            cvmx_write_csr(CVMX_CIU_QLM0, ciu_qlm.u64);
        }
    }

    mio_rst_ctl.u64 = cvmx_read_csr(CVMX_MIO_RST_CTLX(srio_port));
    cvmx_dprintf("SRIO%d: Port in %s mode\n", srio_port,
        (mio_rst_ctl.s.prtmode) ? "host" : "endpoint");

    /* Bring the port out of reset if necessary */
    if (srio_port)
    {
        cvmx_ciu_soft_prst1_t prst;
        prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
        if (prst.s.soft_prst)
        {
            prst.s.soft_prst = 0;
            cvmx_write_csr(CVMX_CIU_SOFT_PRST1, prst.u64);
            cvmx_wait_usec(10000); /* 10ms for new link to stabalize */
        }
    }
    else
    {
        cvmx_ciu_soft_prst_t prst;
        prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
        if (prst.s.soft_prst)
        {
            prst.s.soft_prst = 0;
            cvmx_write_csr(CVMX_CIU_SOFT_PRST, prst.u64);
            cvmx_wait_usec(10000); /* 10ms for new link to stabalize */
        }
    }

    /* Disable the link while we make changes */
    if (__cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_PORT_0_CTL(srio_port), &port_0_ctl.u32))
        return -1;
    port_0_ctl.s.disable = 1;
    if (__cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_PORT_0_CTL(srio_port), port_0_ctl.u32))
        return -1;

    /* Errata SRIO-14485: Link speed is reported incorrectly in CN63XX
        pass 1.x */
    if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_X))
    {
        cvmx_sriomaintx_port_0_ctl2_t port_0_ctl2;
        if (__cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_PORT_0_CTL2(srio_port), &port_0_ctl2.u32))
            return -1;
        if (port_0_ctl2.s.enb_500g)
        {
            port_0_ctl2.u32 = 0;
            port_0_ctl2.s.enb_625g = 1;
        }
        else if (port_0_ctl2.s.enb_312g)
        {
            port_0_ctl2.u32 = 0;
            port_0_ctl2.s.enb_500g = 1;
        }
        else if (port_0_ctl2.s.enb_250g)
        {
            port_0_ctl2.u32 = 0;
            port_0_ctl2.s.enb_312g = 1;
        }
        else if (port_0_ctl2.s.enb_125g)
        {
            port_0_ctl2.u32 = 0;
            port_0_ctl2.s.enb_250g = 1;
        }
        else
        {
            port_0_ctl2.u32 = 0;
            port_0_ctl2.s.enb_125g = 1;
        }
        if (__cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_PORT_0_CTL2(srio_port), port_0_ctl2.u32))
            return -1;
    }

    /* Set the link layer timeout to 10us. The default is too high and causes
        core bus errors */
    if (__cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_PORT_LT_CTL(srio_port), &port_lt_ctl.u32))
        return -1;
    port_lt_ctl.s.timeout = 10000 / 200; /* 10us = 10000ns / 200ns */
    if (__cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_PORT_LT_CTL(srio_port), port_lt_ctl.u32))
        return -1;

    /* Set the logical layer timeout to 10ms. The default is too high and causes
        core bus errors */
    if (__cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_PORT_RT_CTL(srio_port), &port_rt_ctl.u32))
        return -1;
    port_rt_ctl.s.timeout = 10000000 / 200; /* 10ms = 10000000ns / 200ns */
    if (__cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_PORT_RT_CTL(srio_port), port_rt_ctl.u32))
        return -1;

    /* Allow memory and doorbells. Messaging is enabled later */
    if (__cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_CORE_ENABLES(srio_port), &core_enables.u32))
        return -1;
    core_enables.s.doorbell = 1;
    core_enables.s.memory = 1;
    if (__cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_CORE_ENABLES(srio_port), core_enables.u32))
        return -1;

    /* Allow us to master transactions */
    if (__cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_PORT_GEN_CTL(srio_port), &port_gen_ctl.u32))
        return -1;
    port_gen_ctl.s.menable = 1;
    if (__cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_PORT_GEN_CTL(srio_port), port_gen_ctl.u32))
        return -1;

    /* Set the MRRS and MPS for optimal SRIO performance */
    prt_cfg.u64 = cvmx_read_csr(CVMX_DPI_SLI_PRTX_CFG(srio_port));
    prt_cfg.s.mps = 1;
    prt_cfg.s.mrrs = 1;
    cvmx_write_csr(CVMX_DPI_SLI_PRTX_CFG(srio_port), prt_cfg.u64);

    sli_s2m_portx_ctl.u64 = cvmx_read_csr(CVMX_PEXP_SLI_S2M_PORTX_CTL(srio_port));
    sli_s2m_portx_ctl.s.mrrs = 1;
    cvmx_write_csr(CVMX_PEXP_SLI_S2M_PORTX_CTL(srio_port), sli_s2m_portx_ctl.u64);

    /* Setup RX messaging thresholds */
    sriox_imsg_vport_thr.u64 = cvmx_read_csr(CVMX_SRIOX_IMSG_VPORT_THR(srio_port));
    sriox_imsg_vport_thr.s.max_tot = 48;
    sriox_imsg_vport_thr.s.max_s1 = 24;
    sriox_imsg_vport_thr.s.max_s0 = 24;
    sriox_imsg_vport_thr.s.sp_vport = 1;
    sriox_imsg_vport_thr.s.buf_thr = 4;
    sriox_imsg_vport_thr.s.max_p1 = 12;
    sriox_imsg_vport_thr.s.max_p0 = 12;
    cvmx_write_csr(CVMX_SRIOX_IMSG_VPORT_THR(srio_port), sriox_imsg_vport_thr.u64);

    /* Errata SRIO-X: SRIO error behavior may not be optimal in CN63XX pass 1.x */
    if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_X))
    {
        cvmx_sriox_tx_ctrl_t sriox_tx_ctrl;
        sriox_tx_ctrl.u64 = cvmx_read_csr(CVMX_SRIOX_TX_CTRL(srio_port));
        sriox_tx_ctrl.s.tag_th2 = 2;
        sriox_tx_ctrl.s.tag_th1 = 3;
        sriox_tx_ctrl.s.tag_th0 = 4;
        cvmx_write_csr(CVMX_SRIOX_TX_CTRL(srio_port), sriox_tx_ctrl.u64);
    }

    /* Clear any pending interrupts */
    cvmx_write_csr(CVMX_SRIOX_INT_REG(srio_port), cvmx_read_csr(CVMX_SRIOX_INT_REG(srio_port)));

    /* Enable error reporting */
#if (!defined(CVMX_BUILD_FOR_LINUX_HOST) && !defined(CVMX_BUILD_FOR_LINUX_KERNEL)) || defined(CONFIG_CAVIUM_DECODE_RSL)
    cvmx_error_enable_group(CVMX_ERROR_GROUP_SRIO, srio_port);
#endif

    /* Finally enable the link */
    if (__cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_PORT_0_CTL(srio_port), &port_0_ctl.u32))
        return -1;
    port_0_ctl.s.o_enable = 1;
    port_0_ctl.s.i_enable = 1;
    port_0_ctl.s.disable = 0;
    port_0_ctl.s.prt_lock = 0;
    if (__cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_PORT_0_CTL(srio_port), port_0_ctl.u32))
        return -1;

    return 0;
}


/**
 * Read 32bits from a Device's config space
 *
 * @param srio_port SRIO port the device is on
 * @param srcid_index
 *                  Which SRIO source ID to use. 0 = Primary, 1 = Secondary
 * @param destid    RapidIO device ID, or -1 for the local Octeon.
 * @param is16bit   Non zero if the transactions should use 16bit device IDs. Zero
 *                  if transactions should use 8bit device IDs.
 * @param hopcount  Number of hops to the remote device. Use 0 for the local Octeon.
 * @param offset    Offset in config space. This must be a multiple of 32 bits.
 * @param result    Result of the read. This will be unmodified on failure.
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_srio_config_read32(int srio_port, int srcid_index, int destid,
                            int is16bit, uint8_t hopcount, uint32_t offset,
                            uint32_t *result)
{
    if (destid == -1)
    {
        int status = __cvmx_srio_local_read32(srio_port, offset, result);

        if ((status == 0) && (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG))
            cvmx_dprintf("SRIO%d: Local read [0x%06x] <= 0x%08x\n", srio_port, (unsigned int)offset, (unsigned int)*result);

        return status;
    }
    else
    {
#if CVMX_SRIO_USE_FIFO_FOR_MAINT
        int return_code;
        uint32_t pkt = 0;
        uint32_t sourceid;
        uint64_t stop_cycle;
        char rx_buffer[64];

        /* Tell the user */
        if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
            cvmx_dprintf("SRIO%d: Remote read [id=0x%04x hop=%3d offset=0x%06x] <= ", srio_port, destid, hopcount, (unsigned int)offset);

        /* Read the proper source ID */
        if (srcid_index)
            __cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_SEC_DEV_ID(srio_port), &sourceid);
        else
            __cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_PRI_DEV_ID(srio_port), &sourceid);

        if (is16bit)
        {
            /* Use the 16bit source ID */
            sourceid &= 0xffff;

            /* MAINT Reads are 11 bytes */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_CTRL(srio_port), 11<<16);

            pkt |= CVMX_SRIO_CONFIG_PRIORITY << 30; /* priority [31:30] */
            pkt |= 1 << 28;                         /* tt       [29:28] */
            pkt |= 0x8 << 24;                       /* ftype    [27:24] */
            pkt |= destid << 8;                     /* destID   [23:8] */
            pkt |= sourceid >> 8;                   /* sourceID [7:0] */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
            pkt = 0;
            pkt |= sourceid << 24;                  /* sourceID [31:24] */
            pkt |= 0 << 20;                         /* transaction [23:20] */
            pkt |= 8 << 16;                         /* rdsize [19:16] */
            pkt |= 0xc0 << 8;                       /* srcTID [15:8] */
            pkt |= hopcount;                        /* hopcount [7:0] */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
            pkt = 0;
            pkt |= offset << 8;                     /* offset [31:11, wdptr[10], reserved[9:8] */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
        }
        else
        {
            /* Use the 8bit source ID */
            sourceid = (sourceid >> 16) & 0xff;

            /* MAINT Reads are 9 bytes */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_CTRL(srio_port), 9<<16);

            pkt |= CVMX_SRIO_CONFIG_PRIORITY << 30; /* priority [31:30] */
            pkt |= 0 << 28;                         /* tt       [29:28] */
            pkt |= 0x8 << 24;                       /* ftype    [27:24] */
            pkt |= destid << 16;                    /* destID   [23:16] */
            pkt |= sourceid << 8;                   /* sourceID [15:8] */
            pkt |= 0 << 4;                          /* transaction [7:4] */
            pkt |= 8 << 0;                          /* rdsize [3:0] */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
            pkt = 0;
            pkt |= 0xc0 << 24;                      /* srcTID [31:24] */
            pkt |= hopcount << 16;                  /* hopcount [23:16] */
            pkt |= offset >> 8;                     /* offset [15:0] */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
            pkt = 0;
            pkt |= offset << 24;                    /* offset [31:27, wdptr[26], reserved[25:24] */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
        }

        stop_cycle = cvmx_clock_get_rate(CVMX_CLOCK_CORE)/10 + cvmx_clock_get_count(CVMX_CLOCK_CORE);
        do
        {
            return_code = cvmx_srio_receive_spf(srio_port, rx_buffer, sizeof(rx_buffer));
            if ((return_code == 0) && (cvmx_clock_get_count(CVMX_CLOCK_CORE) > stop_cycle))
            {
                if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
                    cvmx_dprintf("timeout\n");
                return_code = -1;
            }
        } while (return_code == 0);

        if (return_code == ((is16bit) ? 23 : 19))
        {
            if (is16bit)
            {
                if (offset & 4)
                    *result = *(uint32_t*)(rx_buffer + 15);
                else
                    *result = *(uint32_t*)(rx_buffer + 11);
            }
            else
            {
                if (offset & 4)
                    *result = *(uint32_t*)(rx_buffer + 13);
                else
                    *result = *(uint32_t*)(rx_buffer + 9);
            }
            if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
                cvmx_dprintf("0x%08x\n", (unsigned int)*result);
            return_code = 0;
        }
        else
        {
            *result = 0xffffffff;
            return_code = -1;
        }

        return return_code;
#elif !defined(CVMX_BUILD_FOR_LINUX_HOST)
        uint64_t physical;
        physical = cvmx_srio_physical_map(srio_port,
            CVMX_SRIO_WRITE_MODE_MAINTENANCE, CVMX_SRIO_CONFIG_PRIORITY,
            CVMX_SRIO_READ_MODE_MAINTENANCE, CVMX_SRIO_CONFIG_PRIORITY,
            srcid_index, destid, is16bit, offset + (hopcount<<24), 4);
        if (!physical)
            return -1;

        if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
            cvmx_dprintf("SRIO%d: Remote read [id=0x%04x hop=%3d offset=0x%06x] <= ", srio_port, destid, hopcount, offset);

        /* Finally do the maintenance read to complete the config request */
        *result = cvmx_read64_uint32(CVMX_ADD_IO_SEG(physical));
        cvmx_srio_physical_unmap(physical, 4);

        if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
            cvmx_dprintf("0x%08x\n", *result);

        return 0;
#else
        return -1;
#endif
    }
}


/**
 * Write 32bits to a Device's config space
 *
 * @param srio_port SRIO port the device is on
 * @param srcid_index
 *                  Which SRIO source ID to use. 0 = Primary, 1 = Secondary
 * @param destid    RapidIO device ID, or -1 for the local Octeon.
 * @param is16bit   Non zero if the transactions should use 16bit device IDs. Zero
 *                  if transactions should use 8bit device IDs.
 * @param hopcount  Number of hops to the remote device. Use 0 for the local Octeon.
 * @param offset    Offset in config space. This must be a multiple of 32 bits.
 * @param data      Data to write.
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_srio_config_write32(int srio_port, int srcid_index, int destid,
                             int is16bit, uint8_t hopcount, uint32_t offset,
                             uint32_t data)
{
    if (destid == -1)
    {
        if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
            cvmx_dprintf("SRIO%d: Local write[0x%06x] => 0x%08x\n", srio_port, (unsigned int)offset, (unsigned int)data);

        return __cvmx_srio_local_write32(srio_port, offset, data);
    }
    else
    {
#if CVMX_SRIO_USE_FIFO_FOR_MAINT
        int return_code;
        uint32_t pkt = 0;
        uint32_t sourceid;
        uint64_t stop_cycle;
        char rx_buffer[64];

        /* Tell the user */
        if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
            cvmx_dprintf("SRIO%d: Remote write[id=0x%04x hop=%3d offset=0x%06x] => 0x%08x\n", srio_port, destid, hopcount, (unsigned int)offset, (unsigned int)data);

        /* Read the proper source ID */
        if (srcid_index)
            __cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_SEC_DEV_ID(srio_port), &sourceid);
        else
            __cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_PRI_DEV_ID(srio_port), &sourceid);

        if (is16bit)
        {
            /* Use the 16bit source ID */
            sourceid &= 0xffff;

            /* MAINT Writes are 19 bytes */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_CTRL(srio_port), 19<<16);

            pkt |= CVMX_SRIO_CONFIG_PRIORITY << 30; /* priority [31:30] */
            pkt |= 1 << 28;                         /* tt       [29:28] */
            pkt |= 0x8 << 24;                       /* ftype    [27:24] */
            pkt |= destid << 8;                     /* destID   [23:8] */
            pkt |= sourceid >> 8;                   /* sourceID [7:0] */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
            pkt = 0;
            pkt |= sourceid << 24;                  /* sourceID [31:24] */
            pkt |= 1 << 20;                         /* transaction [23:20] */
            pkt |= 8 << 16;                         /* wrsize [19:16] */
            pkt |= 0xc0 << 8;                       /* srcTID [15:8] */
            pkt |= hopcount;                        /* hopcount [7:0] */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
            pkt = 0;
            pkt |= offset << 8;                     /* offset [31:11, wdptr[10], reserved[9:8] */
            if ((offset & 4) == 0)
                pkt |= 0xff & (data >> 24);       /* data [7:0] */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
            if (offset & 4)
            {
                pkt = 0xff & (data >> 24);
                __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
                pkt = data << 8;
                __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
            }
            else
            {
                pkt = data << 8;
                __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
                __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), 0);
            }
        }
        else
        {
            /* Use the 8bit source ID */
            sourceid = (sourceid >> 16) & 0xff;

            /* MAINT Writes are 17 bytes */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_CTRL(srio_port), 17<<16);

            pkt |= CVMX_SRIO_CONFIG_PRIORITY << 30; /* priority [31:30] */
            pkt |= 0 << 28;                         /* tt       [29:28] */
            pkt |= 0x8 << 24;                       /* ftype    [27:24] */
            pkt |= destid << 16;                    /* destID   [23:16] */
            pkt |= sourceid << 8;                   /* sourceID [15:8] */
            pkt |= 1 << 4;                          /* transaction [7:4] */
            pkt |= 8 << 0;                          /* wrsize [3:0] */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
            pkt = 0;
            pkt |= 0xc0 << 24;                      /* srcTID [31:24] */
            pkt |= hopcount << 16;                  /* hopcount [23:16] */
            pkt |= offset >> 8;                     /* offset [15:0] */
            __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
            pkt = 0;
            pkt |= offset << 24;                    /* offset [31:27, wdptr[26], reserved[25:24] */
            if (offset & 4)
            {
                __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
                pkt = data >> 8;
                __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
                pkt = data << 24;
                __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
            }
            else
            {
                pkt |= data >> 8;                    /* data [23:0] */
                __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
                pkt = data << 24;                    /* data [31:24] */
                __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), pkt);
                __cvmx_srio_local_write32(srio_port, CVMX_SRIOMAINTX_IR_SP_TX_DATA(srio_port), 0);
            }
        }

        stop_cycle = cvmx_clock_get_rate(CVMX_CLOCK_CORE)/10 + cvmx_clock_get_count(CVMX_CLOCK_CORE);
        do
        {
            return_code = cvmx_srio_receive_spf(srio_port, rx_buffer, sizeof(rx_buffer));
            if ((return_code == 0) && (cvmx_clock_get_count(CVMX_CLOCK_CORE) > stop_cycle))
            {
                if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
                    cvmx_dprintf("timeout\n");
                return_code = -1;
            }
        } while (return_code == 0);

        if (return_code == ((is16bit) ? 15 : 11))
            return_code = 0;
        else
        {
            cvmx_dprintf("SRIO%d: Remote write failed\n", srio_port);
            return_code = -1;
        }

        return return_code;
#elif !defined(CVMX_BUILD_FOR_LINUX_HOST)
        uint64_t physical = cvmx_srio_physical_map(srio_port,
                CVMX_SRIO_WRITE_MODE_MAINTENANCE, CVMX_SRIO_CONFIG_PRIORITY,
                CVMX_SRIO_READ_MODE_MAINTENANCE, CVMX_SRIO_CONFIG_PRIORITY,
                srcid_index, destid, is16bit, offset + (hopcount<<24), 4);
        if (!physical)
            return -1;

        if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
            cvmx_dprintf("SRIO%d: Remote write[id=0x%04x hop=%3d offset=0x%06x] => 0x%08x\n", srio_port, destid, hopcount, offset, data);

        /* Finally do the maintenance write to complete the config request */
        cvmx_write64_uint32(CVMX_ADD_IO_SEG(physical), data);
        return cvmx_srio_physical_unmap(physical, 4);
#else
        return -1;
#endif
    }
}


/**
 * Send a RapidIO doorbell to a remote device
 *
 * @param srio_port SRIO port the device is on
 * @param srcid_index
 *                  Which SRIO source ID to use. 0 = Primary, 1 = Secondary
 * @param destid    RapidIO device ID.
 * @param is16bit   Non zero if the transactions should use 16bit device IDs. Zero
 *                  if transactions should use 8bit device IDs.
 * @param priority  Doorbell priority (0-3)
 * @param data      Data for doorbell.
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_srio_send_doorbell(int srio_port, int srcid_index, int destid, int is16bit, int priority, uint16_t data)
{
    cvmx_sriox_tx_bell_t tx_bell;
    tx_bell.u64 = 0;
    tx_bell.s.data = data;
    tx_bell.s.dest_id = destid;
    tx_bell.s.src_id = srcid_index;
    tx_bell.s.id16 = !!is16bit;
    tx_bell.s.priority = priority;

    /* Make sure the previous doorbell has completed */
    if (CVMX_WAIT_FOR_FIELD64(CVMX_SRIOX_TX_BELL(srio_port), cvmx_sriox_tx_bell_t, pending, ==, 0, CVMX_SRIO_DOORBELL_TIMEOUT))
    {
        cvmx_dprintf("SRIO%d: Pending bit stuck before doorbell\n", srio_port);
        return -1;
    }

    if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
        cvmx_dprintf("SRIO%d: Send doorbell destid=0x%x, priority=%d, data=0x%x\n", srio_port, destid, priority, 0xffff & data);

    /* Send the doorbell. We don't wait for it to complete. The next doorbell
        may delay on the pending bit, but this gives the caller the ability to
        do other stuff while the doorbell processes */
    cvmx_write_csr(CVMX_SRIOX_TX_BELL(srio_port), tx_bell.u64);
    return 0;
}


/**
 * Get the status of the last doorbell sent. If the dooorbell
 * hardware is done, then the status is cleared to get ready for
 * the next doorbell (or retry).
 *
 * @param srio_port SRIO port to check doorbell on
 *
 * @return Doorbell status
 */
cvmx_srio_doorbell_status_t cvmx_srio_send_doorbell_status(int srio_port)
{
    cvmx_sriox_tx_bell_t tx_bell;
    cvmx_sriox_tx_bell_info_t tx_bell_info;
    cvmx_sriox_int_reg_t int_reg;
    cvmx_sriox_int_reg_t int_reg_clear;

    /* Return busy if the doorbell is still processing */
    tx_bell.u64 = cvmx_read_csr(CVMX_SRIOX_TX_BELL(srio_port));
    if (tx_bell.s.pending)
        return CVMX_SRIO_DOORBELL_BUSY;

    /* Read and clear the TX doorbell interrupts */
    int_reg.u64 = cvmx_read_csr(CVMX_SRIOX_INT_REG(srio_port));
    int_reg_clear.u64 = 0;
    int_reg_clear.s.bell_err = int_reg.s.bell_err;
    int_reg_clear.s.txbell = int_reg.s.txbell;
    cvmx_write_csr(CVMX_SRIOX_INT_REG(srio_port), int_reg_clear.u64);

    /* Check for errors */
    if (int_reg.s.bell_err)
    {
        if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
            cvmx_dprintf("SRIO%d: Send doorbell failed\n", srio_port);
        tx_bell_info.u64 = cvmx_read_csr(CVMX_SRIOX_TX_BELL_INFO(srio_port));
        if (tx_bell_info.s.timeout || tx_bell_info.s.error)
            return CVMX_SRIO_DOORBELL_ERROR;
        if (tx_bell_info.s.retry)
            return CVMX_SRIO_DOORBELL_RETRY;
    }

    /* Check if we're done */
    if (int_reg.s.txbell)
        return CVMX_SRIO_DOORBELL_DONE;

    /* No doorbell found */
    return CVMX_SRIO_DOORBELL_NONE;
}


/**
 * Read a received doorbell and report data about it.
 *
 * @param srio_port SRIO port to check for the received doorbell
 * @param destid_index
 *                  Which Octeon destination ID was the doorbell for
 * @param sequence_num
 *                  Sequence number of doorbell (32bits)
 * @param srcid     RapidIO source ID of the doorbell sender
 * @param priority  Priority of the doorbell (0-3)
 * @param is16bit   Non zero if the transactions should use 16bit device IDs. Zero
 *                  if transactions should use 8bit device IDs.
 * @param data      Data in the doorbell (16 bits)
 *
 * @return Doorbell status. Either DONE, NONE, or ERROR.
 */
cvmx_srio_doorbell_status_t cvmx_srio_receive_doorbell(int srio_port,
        int *destid_index, uint32_t *sequence_num, int *srcid, int *priority,
        int *is16bit, uint16_t *data)
{
    cvmx_sriox_rx_bell_seq_t rx_bell_seq;
    cvmx_sriox_rx_bell_t rx_bell;

    /* Check if there are any pending doorbells */
    rx_bell_seq.u64 = cvmx_read_csr(CVMX_SRIOX_RX_BELL_SEQ(srio_port));
    if (!rx_bell_seq.s.count)
        return CVMX_SRIO_DOORBELL_NONE;

    /* Read the doorbell and write our return parameters */
    rx_bell.u64 = cvmx_read_csr(CVMX_SRIOX_RX_BELL(srio_port));
    *sequence_num = rx_bell_seq.s.seq;
    *srcid = rx_bell.s.src_id;
    *priority = rx_bell.s.priority;
    *is16bit = rx_bell.s.id16;
    *data = rx_bell.s.data;
    *destid_index = rx_bell.s.dest_id;

    if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
        cvmx_dprintf("SRIO%d: Receive doorbell sequence=0x%x, srcid=0x%x, priority=%d, data=0x%x\n",
            srio_port, rx_bell_seq.s.seq, rx_bell.s.src_id, rx_bell.s.priority, rx_bell.s.data);

    return CVMX_SRIO_DOORBELL_DONE;
}


/**
 * Receive a packet from the Soft Packet FIFO (SPF).
 *
 * @param srio_port SRIO port to read the packet from.
 * @param buffer    Buffer to receive the packet.
 * @param buffer_length
 *                  Length of the buffer in bytes.
 *
 * @return Returns the length of the packet read. Negative on failure.
 *         Zero if no packets are available.
 */
int cvmx_srio_receive_spf(int srio_port, void *buffer, int buffer_length)
{
    uint32_t *ptr = (uint32_t *)buffer;
    cvmx_sriomaintx_ir_sp_rx_stat_t sriomaintx_ir_sp_rx_stat;

    /* Read the SFP status */
    if (__cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_IR_SP_RX_STAT(srio_port), &sriomaintx_ir_sp_rx_stat.u32))
        return -1;

    /* Return zero if there isn't a packet available */
    if (sriomaintx_ir_sp_rx_stat.s.buffers < 1)
        return 0;

    if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
        cvmx_dprintf("SRIO%d: Soft packet FIFO received %d bytes", srio_port, sriomaintx_ir_sp_rx_stat.s.octets);

    /* Return error if the packet is larger than our buffer */
    if (sriomaintx_ir_sp_rx_stat.s.octets > buffer_length)
        return -1;

    /* Read out the packet four bytes at a time */
    buffer_length = sriomaintx_ir_sp_rx_stat.s.octets;
    while (buffer_length > 0)
    {
        __cvmx_srio_local_read32(srio_port, CVMX_SRIOMAINTX_IR_SP_RX_DATA(srio_port), ptr);
        if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
            cvmx_dprintf(" %08x", (unsigned int)*ptr);
        ptr++;
        buffer_length-=4;
    }

    if (__cvmx_srio_state[srio_port].flags & CVMX_SRIO_INITIALIZE_DEBUG)
        cvmx_dprintf("\n");

    /* Return the number of bytes in the buffer */
    return sriomaintx_ir_sp_rx_stat.s.octets;
}

#ifndef CVMX_BUILD_FOR_LINUX_HOST
/**
 * Map a remote device's memory region into Octeon's physical
 * address area. The caller can then map this into a core using
 * the TLB or XKPHYS.
 *
 * @param srio_port SRIO port to map the device on
 * @param write_op  Type of operation to perform on a write to the device.
 *                  Normally should be CVMX_SRIO_WRITE_MODE_AUTO.
 * @param write_priority
 *                  SRIO priority of writes (0-3)
 * @param read_op   Type of operation to perform on reads to the device.
 *                  Normally should be CVMX_SRIO_READ_MODE_NORMAL.
 * @param read_priority
 *                  SRIO priority of reads (0-3)
 * @param srcid_index
 *                  Which SRIO source ID to use. 0 = Primary, 1 = Secondary
 * @param destid    RapidIO device ID.
 * @param is16bit   Non zero if the transactions should use 16bit device IDs. Zero
 *                  if transactions should use 8bit device IDs.
 * @param base      Device base address to start the mapping
 * @param size      Size of the mapping in bytes
 *
 * @return Octeon 64bit physical address that accesses the remote device,
 *         or zero on failure.
 */
uint64_t cvmx_srio_physical_map(int srio_port, cvmx_srio_write_mode_t write_op,
        int write_priority, cvmx_srio_read_mode_t read_op, int read_priority,
        int srcid_index, int destid, int is16bit, uint64_t base, uint64_t size)
{
    cvmx_sriox_s2m_typex_t needed_s2m_type;
    cvmx_sli_mem_access_subidx_t needed_subid;
    int s2m_index;
    int subdid;
    cvmx_sli_address_t sli_address;

    /* We currently don't support mapping regions that span a 34 bit boundary.
        Keeping track of multiple regions to span 34 bits is hard and not
        likely to be needed */
    if (((base+size-1)>>34) != (base>>34))
    {
        cvmx_dprintf("SRIO%d: Failed to map range 0x%llx-0x%llx spanning a 34bit boundary\n",
            srio_port, (ULL)base, (ULL)base+size-1);
        return 0;
    }

    /* Build the S2M_TYPE we are going to need */
    needed_s2m_type.u64 = 0;
    needed_s2m_type.s.wr_op = write_op;
    needed_s2m_type.s.rd_op = read_op;
    needed_s2m_type.s.wr_prior = write_priority;
    needed_s2m_type.s.rd_prior = read_priority;
    needed_s2m_type.s.src_id = srcid_index;
    needed_s2m_type.s.id16 = !!is16bit;

    /* Build the needed SubID config */
    needed_subid.u64 = 0;
    needed_subid.s.port = srio_port;
    needed_subid.s.nmerge = 1;

    /* FIXME: We might want to use the device ID swapping modes so the device
        ID is part of the lower address bits. This would allow many more
        devices to share S2M_TYPE indexes. This would require "base+size-1"
        to fit in bits [17:0] or bits[25:0] for 8 bits of device ID */
    if (base < (1ull<<34))
    {
        needed_subid.s.ba = destid;
        needed_s2m_type.s.iaow_sel = 0;
    }
    else if (base < (1ull<<42))
    {
        needed_subid.s.ba = (base>>34) & 0xff;
        needed_subid.s.ba |= ((uint64_t)destid & 0xff) << (42-34);
        needed_subid.s.ba |= (((uint64_t)destid>>8) & 0xff) << (51-34);
        needed_s2m_type.s.iaow_sel = 1;
    }
    else
    {
        if (destid>>8)
        {
            cvmx_dprintf("SRIO%d: Attempt to map 16bit device ID 0x%x using 66bit addressing\n", srio_port, destid);
            return 0;
        }
        if (base>>50)
        {
            cvmx_dprintf("SRIO%d: Attempt to map address 0x%llx using 66bit addressing\n", srio_port, (ULL)base);
            return 0;
        }
        needed_subid.s.ba = (base>>34) & 0xffff;
        needed_subid.s.ba |= ((uint64_t)destid & 0xff) << (51-34);
        needed_s2m_type.s.iaow_sel = 2;
    }

    /* Find a S2M_TYPE index to use. If this fails return 0 */
    s2m_index = __cvmx_srio_alloc_s2m(srio_port, needed_s2m_type);
    if (s2m_index == -1)
        return 0;

    /* Attach the SubID to the S2M_TYPE index */
    needed_subid.s.rtype = s2m_index & 3;
    needed_subid.s.wtype = s2m_index & 3;
    needed_subid.s.ba |= (((uint64_t)s2m_index >> 2) & 1) << (50-34);
    needed_subid.s.ba |= (((uint64_t)s2m_index >> 3) & 1) << (59-34);

    /* Allocate a SubID for use */
    subdid = __cvmx_srio_alloc_subid(needed_subid);
    if (subdid == -1)
    {
        /* Free the s2m_index as we aren't using it */
        __cvmx_srio_free_s2m(srio_port, s2m_index);
        return 0;
    }

    /* Build the final core physical address */
    sli_address.u64 = 0;
    sli_address.mem.io = 1;
    sli_address.mem.did = 3;
    sli_address.mem.subdid = subdid>>2;
    sli_address.mem.se = subdid & 3;
    sli_address.mem.address = base; /* Bits[33:0] of full address */
    return sli_address.u64;
}


/**
 * Unmap a physical address window created by cvmx_srio_phys_map().
 *
 * @param physical_address
 *               Physical address returned by cvmx_srio_phys_map().
 * @param size   Size used on original call.
 *
 * @return Zero on success, negative on failure.
 */
int cvmx_srio_physical_unmap(uint64_t physical_address, uint64_t size)
{
    cvmx_sli_mem_access_subidx_t subid;
    int subdid = (physical_address >> 40) & 7;
    int extender = (physical_address >> 34) & 3;
    int mem_index = subdid * 4 + extender;
    int read_s2m_type;

    /* Get the subid setup so we can figure out where this mapping was for */
    subid.u64 = cvmx_read_csr(CVMX_PEXP_SLI_MEM_ACCESS_SUBIDX(mem_index));
    /* Type[0] is mapped to the Relaxed Ordering
       Type[1] is mapped to the No Snoop
       Type[2] is mapped directly to bit 50 of the SLI address
       Type[3] is mapped directly to bit 59 of the SLI address */
    read_s2m_type = ((subid.s.ba>>(50-34))&1<<2) | ((subid.s.ba>>(59-34))&1<<3);
    read_s2m_type |= subid.s.rtype;
    __cvmx_srio_free_subid(mem_index);
    __cvmx_srio_free_s2m(subid.s.port, read_s2m_type);
    return 0;
}
#endif
