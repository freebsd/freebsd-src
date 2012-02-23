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
 * Interface to power-throttle control, measurement, and debugging
 * facilities.
 *
 * <hr>$Revision<hr>
 *
 */

#include "cvmx.h"
#include "cvmx-asm.h"
#include "cvmx-power-throttle.h"

#define CVMX_PTH_PPID_BCAST	63
#define CVMX_PTH_PPID_MAX	64

/**
 * @INTERNAL
 * Set the POWLIM field as percentage% of the MAXPOW field in r.
 */
static uint64_t __cvmx_power_throttle_set_powlim(uint64_t r, uint8_t percentage)
{
    if (OCTEON_IS_MODEL(OCTEON_CN6XXX))
    {
        uint64_t t;

        assert(percentage < 101);
        t = percentage * cvmx_power_throttle_get_field(CVMX_PTH_INDEX_MAXPOW, r) / 100;
        r = cvmx_power_throttle_set_field(CVMX_PTH_INDEX_POWLIM, r, t);

        return r;
    }
    return 0;
}

/**
 * @INTERNAL
 * Given ppid, calculate its PowThrottle register's L2C_COP0_MAP CSR
 * address. (ppid == PTH_PPID_BCAST is for broadcasting)
 */
static uint64_t __cvmx_power_throttle_csr_addr(uint64_t ppid)
{
    if (OCTEON_IS_MODEL(OCTEON_CN6XXX))
    {
        uint64_t csr_addr, reg_num, reg_reg, reg_sel;

        assert(ppid < CVMX_PTH_PPID_MAX);
        /*
         * register 11 selection 6
         */
        reg_reg = 11;
        reg_sel = 6;
        reg_num = (ppid << 8) + (reg_reg << 3) + reg_sel;
        csr_addr = CVMX_L2C_COP0_MAPX(0) + ((reg_num) << 3);

        return csr_addr;
    }
    return 0;
}

/**
 * Throttle power to percentage% of configured maximum (MAXPOW).
 *
 * @param percentage	0 to 100
 * @return 0 for success
 */
int cvmx_power_throttle_self(uint8_t percentage)
{
    if (OCTEON_IS_MODEL(OCTEON_CN6XXX))
    {
        uint64_t r; 

        CVMX_MF_COP0(r, COP0_POWTHROTTLE);
        r = __cvmx_power_throttle_set_powlim(r, percentage);
        CVMX_MT_COP0(r, COP0_POWTHROTTLE);
    }

    return 0;
}

/**
 * Throttle power to percentage% of configured maximum (MAXPOW)
 * for the cores identified in coremask.
 *
 * @param percentage 	0 to 100
 * @param coremask	bit mask where each bit identifies a core.
 * @return 0 for success.
 */
int cvmx_power_throttle(uint8_t percentage, uint64_t coremask)
{
    if (OCTEON_IS_MODEL(OCTEON_CN6XXX))
    {
        uint64_t ppid, csr_addr, b, r;

        b = 1;
        /*
         * cvmx_read_csr() for PTH_PPID_BCAST does not make sense and
         * therefore limit ppid to less.
         */
        for (ppid = 0; ppid < CVMX_PTH_PPID_BCAST; ppid ++)
        {
            if ((b << ppid) & coremask) {
                csr_addr = __cvmx_power_throttle_csr_addr(ppid);
                r = cvmx_read_csr(csr_addr);
                r = __cvmx_power_throttle_set_powlim(r, percentage);
                cvmx_write_csr(csr_addr, r);
            }
        }
    }

    return 0;
}
