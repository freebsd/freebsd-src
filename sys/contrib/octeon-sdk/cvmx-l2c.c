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
 * Implementation of the Level 2 Cache (L2C) control,
 * measurement, and debugging facilities.
 *
 * <hr>$Revision: 41586 $<hr>
 *
 */
#include "cvmx-config.h"
#include "cvmx.h"
#include "cvmx-l2c.h"
#include "cvmx-spinlock.h"
#include "cvmx-interrupt.h"


#ifndef CVMX_BUILD_FOR_LINUX_HOST
/* This spinlock is used internally to ensure that only one core is performing
** certain L2 operations at a time.
**
** NOTE: This only protects calls from within a single application - if multiple applications
** or operating systems are running, then it is up to the user program to coordinate between them.
*/
CVMX_SHARED cvmx_spinlock_t cvmx_l2c_spinlock;
#endif

static inline int l2_size_half(void)
{
    uint64_t val = cvmx_read_csr(CVMX_L2D_FUS3);
    return !!(val & (1ull << 34));
}
int cvmx_l2c_get_core_way_partition(uint32_t core)
{
    uint32_t    field;

    /* Validate the core number */
    if (core >= cvmx_octeon_num_cores())
        return -1;

    /* Use the lower two bits of the coreNumber to determine the bit offset
     * of the UMSK[] field in the L2C_SPAR register.
     */
    field = (core & 0x3) * 8;

    /* Return the UMSK[] field from the appropriate L2C_SPAR register based
     * on the coreNumber.
     */

    switch (core & 0xC)
    {
        case 0x0:
            return((cvmx_read_csr(CVMX_L2C_SPAR0) & (0xFF << field)) >> field);
        case 0x4:
            return((cvmx_read_csr(CVMX_L2C_SPAR1) & (0xFF << field)) >> field);
        case 0x8:
            return((cvmx_read_csr(CVMX_L2C_SPAR2) & (0xFF << field)) >> field);
        case 0xC:
            return((cvmx_read_csr(CVMX_L2C_SPAR3) & (0xFF << field)) >> field);
    }
    return(0);
}

int cvmx_l2c_set_core_way_partition(uint32_t core, uint32_t mask)
{
    uint32_t    field;
    uint32_t    valid_mask;

    valid_mask = (0x1 << cvmx_l2c_get_num_assoc()) - 1;

    mask &= valid_mask;

    /* A UMSK setting which blocks all L2C Ways is an error. */
    if (mask == valid_mask)
        return -1;

    /* Validate the core number */
    if (core >= cvmx_octeon_num_cores())
        return -1;

    /* Check to make sure current mask & new mask don't block all ways */
    if (((mask | cvmx_l2c_get_core_way_partition(core)) & valid_mask) == valid_mask)
        return -1;


    /* Use the lower two bits of core to determine the bit offset of the
     * UMSK[] field in the L2C_SPAR register.
     */
    field = (core & 0x3) * 8;

    /* Assign the new mask setting to the UMSK[] field in the appropriate
     * L2C_SPAR register based on the core_num.
     *
     */
    switch (core & 0xC)
    {
        case 0x0:
            cvmx_write_csr(CVMX_L2C_SPAR0,
                           (cvmx_read_csr(CVMX_L2C_SPAR0) & ~(0xFF << field)) |
                           mask << field);
            break;
        case 0x4:
            cvmx_write_csr(CVMX_L2C_SPAR1,
                           (cvmx_read_csr(CVMX_L2C_SPAR1) & ~(0xFF << field)) |
                           mask << field);
            break;
        case 0x8:
            cvmx_write_csr(CVMX_L2C_SPAR2,
                           (cvmx_read_csr(CVMX_L2C_SPAR2) & ~(0xFF << field)) |
                           mask << field);
            break;
        case 0xC:
            cvmx_write_csr(CVMX_L2C_SPAR3,
                           (cvmx_read_csr(CVMX_L2C_SPAR3) & ~(0xFF << field)) |
                           mask << field);
            break;
    }
    return 0;
}


int cvmx_l2c_set_hw_way_partition(uint32_t mask)
{
    uint32_t valid_mask;

    valid_mask = (0x1 << cvmx_l2c_get_num_assoc()) - 1;
    mask &= valid_mask;

    /* A UMSK setting which blocks all L2C Ways is an error. */
    if (mask == valid_mask)
        return -1;
    /* Check to make sure current mask & new mask don't block all ways */
    if (((mask | cvmx_l2c_get_hw_way_partition()) & valid_mask) == valid_mask)
        return -1;

    cvmx_write_csr(CVMX_L2C_SPAR4, (cvmx_read_csr(CVMX_L2C_SPAR4) & ~0xFF) | mask);
    return 0;
}

int cvmx_l2c_get_hw_way_partition(void)
{
    return(cvmx_read_csr(CVMX_L2C_SPAR4) & (0xFF));
}


void cvmx_l2c_config_perf(uint32_t counter, cvmx_l2c_event_t event,
                          uint32_t clear_on_read)
{   cvmx_l2c_pfctl_t pfctl;

    pfctl.u64 = cvmx_read_csr(CVMX_L2C_PFCTL);

    switch (counter)
    {
        case 0:
            pfctl.s.cnt0sel = event;
            pfctl.s.cnt0ena = 1;
            if (!cvmx_octeon_is_pass1())
                pfctl.s.cnt0rdclr = clear_on_read;
            break;
        case 1:
            pfctl.s.cnt1sel = event;
            pfctl.s.cnt1ena = 1;
            if (!cvmx_octeon_is_pass1())
                pfctl.s.cnt1rdclr = clear_on_read;
            break;
        case 2:
            pfctl.s.cnt2sel = event;
            pfctl.s.cnt2ena = 1;
            if (!cvmx_octeon_is_pass1())
                pfctl.s.cnt2rdclr = clear_on_read;
            break;
        case 3:
        default:
            pfctl.s.cnt3sel = event;
            pfctl.s.cnt3ena = 1;
            if (!cvmx_octeon_is_pass1())
                pfctl.s.cnt3rdclr = clear_on_read;
            break;
    }

    cvmx_write_csr(CVMX_L2C_PFCTL, pfctl.u64);
}

uint64_t cvmx_l2c_read_perf(uint32_t counter)
{
    switch (counter)
    {
        case 0:
            return(cvmx_read_csr(CVMX_L2C_PFC0));
        case 1:
            return(cvmx_read_csr(CVMX_L2C_PFC1));
        case 2:
            return(cvmx_read_csr(CVMX_L2C_PFC2));
        case 3:
        default:
            return(cvmx_read_csr(CVMX_L2C_PFC3));
    }
}

#ifndef CVMX_BUILD_FOR_LINUX_HOST
/**
 * @INTERNAL
 * Helper function use to fault in cache lines for L2 cache locking
 *
 * @param addr   Address of base of memory region to read into L2 cache
 * @param len    Length (in bytes) of region to fault in
 */
static void fault_in(uint64_t addr, int len)
{
    volatile char *ptr;
    volatile char dummy;
    /* Adjust addr and length so we get all cache lines even for
    ** small ranges spanning two cache lines */
    len += addr & CVMX_CACHE_LINE_MASK;
    addr &= ~CVMX_CACHE_LINE_MASK;
    ptr = (volatile char *)cvmx_phys_to_ptr(addr);
    CVMX_DCACHE_INVALIDATE;  /* Invalidate L1 cache to make sure all loads result in data being in L2 */
    while (len > 0)
    {
        dummy += *ptr;
        len -= CVMX_CACHE_LINE_SIZE;
        ptr += CVMX_CACHE_LINE_SIZE;
    }
}

int cvmx_l2c_lock_line(uint64_t addr)
{
    int retval = 0;
    cvmx_l2c_dbg_t l2cdbg;
    cvmx_l2c_lckbase_t lckbase;
    cvmx_l2c_lckoff_t lckoff;
    cvmx_l2t_err_t l2t_err;
    l2cdbg.u64 = 0;
    lckbase.u64 = 0;
    lckoff.u64 = 0;

    cvmx_spinlock_lock(&cvmx_l2c_spinlock);

    /* Clear l2t error bits if set */
    l2t_err.u64 = cvmx_read_csr(CVMX_L2T_ERR);
    l2t_err.s.lckerr = 1;
    l2t_err.s.lckerr2 = 1;
    cvmx_write_csr(CVMX_L2T_ERR, l2t_err.u64);

    addr &= ~CVMX_CACHE_LINE_MASK;

    /* Set this core as debug core */
    l2cdbg.s.ppnum = cvmx_get_core_num();
    CVMX_SYNC;
    cvmx_write_csr(CVMX_L2C_DBG, l2cdbg.u64);
    cvmx_read_csr(CVMX_L2C_DBG);

    lckoff.s.lck_offset = 0; /* Only lock 1 line at a time */
    cvmx_write_csr(CVMX_L2C_LCKOFF, lckoff.u64);
    cvmx_read_csr(CVMX_L2C_LCKOFF);

    if (((cvmx_l2c_cfg_t)(cvmx_read_csr(CVMX_L2C_CFG))).s.idxalias)
    {
        int alias_shift = CVMX_L2C_IDX_ADDR_SHIFT + 2 * CVMX_L2_SET_BITS - 1;
        uint64_t addr_tmp = addr ^ (addr & ((1 << alias_shift) - 1)) >> CVMX_L2_SET_BITS;
        lckbase.s.lck_base = addr_tmp >> 7;
    }
    else
    {
        lckbase.s.lck_base = addr >> 7;
    }

    lckbase.s.lck_ena = 1;
    cvmx_write_csr(CVMX_L2C_LCKBASE, lckbase.u64);
    cvmx_read_csr(CVMX_L2C_LCKBASE);    // Make sure it gets there

    fault_in(addr, CVMX_CACHE_LINE_SIZE);

    lckbase.s.lck_ena = 0;
    cvmx_write_csr(CVMX_L2C_LCKBASE, lckbase.u64);
    cvmx_read_csr(CVMX_L2C_LCKBASE);    // Make sure it gets there

    /* Stop being debug core */
    cvmx_write_csr(CVMX_L2C_DBG, 0);
    cvmx_read_csr(CVMX_L2C_DBG);

    l2t_err.u64 = cvmx_read_csr(CVMX_L2T_ERR);
    if (l2t_err.s.lckerr || l2t_err.s.lckerr2)
        retval = 1;  /* We were unable to lock the line */

    cvmx_spinlock_unlock(&cvmx_l2c_spinlock);

    return(retval);
}


int cvmx_l2c_lock_mem_region(uint64_t start, uint64_t len)
{
    int retval = 0;

    /* Round start/end to cache line boundaries */
    len += start & CVMX_CACHE_LINE_MASK;
    start &= ~CVMX_CACHE_LINE_MASK;
    len = (len + CVMX_CACHE_LINE_MASK) & ~CVMX_CACHE_LINE_MASK;

    while (len)
    {
        retval += cvmx_l2c_lock_line(start);
        start += CVMX_CACHE_LINE_SIZE;
        len -= CVMX_CACHE_LINE_SIZE;
    }

    return(retval);
}


void cvmx_l2c_flush(void)
{
    uint64_t assoc, set;
    uint64_t n_assoc, n_set;
    cvmx_l2c_dbg_t l2cdbg;

    cvmx_spinlock_lock(&cvmx_l2c_spinlock);

    l2cdbg.u64 = 0;
    if (!OCTEON_IS_MODEL(OCTEON_CN30XX))
        l2cdbg.s.ppnum = cvmx_get_core_num();
    l2cdbg.s.finv = 1;
    n_set = CVMX_L2_SETS;
    n_assoc = l2_size_half() ? (CVMX_L2_ASSOC/2) : CVMX_L2_ASSOC ;
    for(set=0; set < n_set; set++)
    {
        for(assoc = 0; assoc < n_assoc; assoc++)
        {
            l2cdbg.s.set = assoc;
            /* Enter debug mode, and make sure all other writes complete before we
            ** enter debug mode */
            CVMX_SYNCW;
            cvmx_write_csr(CVMX_L2C_DBG, l2cdbg.u64);
            cvmx_read_csr(CVMX_L2C_DBG);

            CVMX_PREPARE_FOR_STORE (CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS, set*CVMX_CACHE_LINE_SIZE), 0);
            CVMX_SYNCW; /* Push STF out to L2 */
            /* Exit debug mode */
            CVMX_SYNC;
            cvmx_write_csr(CVMX_L2C_DBG, 0);
            cvmx_read_csr(CVMX_L2C_DBG);
        }
    }

    cvmx_spinlock_unlock(&cvmx_l2c_spinlock);
}


int cvmx_l2c_unlock_line(uint64_t address)
{
    int assoc;
    cvmx_l2c_tag_t tag;
    cvmx_l2c_dbg_t l2cdbg;
    uint32_t tag_addr;

    uint32_t index = cvmx_l2c_address_to_index(address);

    cvmx_spinlock_lock(&cvmx_l2c_spinlock);
    /* Compute portion of address that is stored in tag */
    tag_addr = ((address >> CVMX_L2C_TAG_ADDR_ALIAS_SHIFT) & ((1 << CVMX_L2C_TAG_ADDR_ALIAS_SHIFT) - 1));
    for(assoc = 0; assoc < CVMX_L2_ASSOC; assoc++)
    {
        tag = cvmx_get_l2c_tag(assoc, index);

        if (tag.s.V && (tag.s.addr == tag_addr))
        {
            l2cdbg.u64 = 0;
            l2cdbg.s.ppnum = cvmx_get_core_num();
            l2cdbg.s.set = assoc;
            l2cdbg.s.finv = 1;

            CVMX_SYNC;
            cvmx_write_csr(CVMX_L2C_DBG, l2cdbg.u64); /* Enter debug mode */
            cvmx_read_csr(CVMX_L2C_DBG);

            CVMX_PREPARE_FOR_STORE (CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS, address), 0);
            CVMX_SYNC;
            /* Exit debug mode */
            cvmx_write_csr(CVMX_L2C_DBG, 0);
            cvmx_read_csr(CVMX_L2C_DBG);
            cvmx_spinlock_unlock(&cvmx_l2c_spinlock);
            return tag.s.L;
        }
    }
    cvmx_spinlock_unlock(&cvmx_l2c_spinlock);
    return 0;
}

int cvmx_l2c_unlock_mem_region(uint64_t start, uint64_t len)
{
    int num_unlocked = 0;
    /* Round start/end to cache line boundaries */
    len += start & CVMX_CACHE_LINE_MASK;
    start &= ~CVMX_CACHE_LINE_MASK;
    len = (len + CVMX_CACHE_LINE_MASK) & ~CVMX_CACHE_LINE_MASK;
    while (len > 0)
    {
        num_unlocked += cvmx_l2c_unlock_line(start);
        start += CVMX_CACHE_LINE_SIZE;
        len -= CVMX_CACHE_LINE_SIZE;
    }

    return num_unlocked;
}


/* Internal l2c tag types.  These are converted to a generic structure
** that can be used on all chips */
typedef union
{
    uint64_t u64;
#if __BYTE_ORDER == __BIG_ENDIAN
    struct cvmx_l2c_tag_cn50xx
    {
	uint64_t reserved		: 40;
	uint64_t V			: 1;	// Line valid
	uint64_t D			: 1;	// Line dirty
	uint64_t L			: 1;	// Line locked
	uint64_t U			: 1;	// Use, LRU eviction
	uint64_t addr			: 20;	// Phys mem addr (33..14)
    } cn50xx;
    struct cvmx_l2c_tag_cn30xx
    {
	uint64_t reserved		: 41;
	uint64_t V			: 1;	// Line valid
	uint64_t D			: 1;	// Line dirty
	uint64_t L			: 1;	// Line locked
	uint64_t U			: 1;	// Use, LRU eviction
	uint64_t addr			: 19;	// Phys mem addr (33..15)
    } cn30xx;
    struct cvmx_l2c_tag_cn31xx
    {
	uint64_t reserved		: 42;
	uint64_t V			: 1;	// Line valid
	uint64_t D			: 1;	// Line dirty
	uint64_t L			: 1;	// Line locked
	uint64_t U			: 1;	// Use, LRU eviction
	uint64_t addr			: 18;	// Phys mem addr (33..16)
    } cn31xx;
    struct cvmx_l2c_tag_cn38xx
    {
	uint64_t reserved		: 43;
	uint64_t V			: 1;	// Line valid
	uint64_t D			: 1;	// Line dirty
	uint64_t L			: 1;	// Line locked
	uint64_t U			: 1;	// Use, LRU eviction
	uint64_t addr			: 17;	// Phys mem addr (33..17)
    } cn38xx;
    struct cvmx_l2c_tag_cn58xx
    {
	uint64_t reserved		: 44;
	uint64_t V			: 1;	// Line valid
	uint64_t D			: 1;	// Line dirty
	uint64_t L			: 1;	// Line locked
	uint64_t U			: 1;	// Use, LRU eviction
	uint64_t addr			: 16;	// Phys mem addr (33..18)
    } cn58xx;
    struct cvmx_l2c_tag_cn58xx   cn56xx; /* 2048 sets */
    struct cvmx_l2c_tag_cn31xx   cn52xx; /* 512 sets */
#endif
} __cvmx_l2c_tag_t;


/**
 * @INTERNAL
 * Function to read a L2C tag.  This code make the current core
 * the 'debug core' for the L2.  This code must only be executed by
 * 1 core at a time.
 *
 * @param assoc  Association (way) of the tag to dump
 * @param index  Index of the cacheline
 *
 * @return The Octeon model specific tag structure.  This is translated by a wrapper
 *         function to a generic form that is easier for applications to use.
 */
static __cvmx_l2c_tag_t __read_l2_tag(uint64_t assoc, uint64_t index)
{

    uint64_t debug_tag_addr = (((1ULL << 63) | (index << 7)) + 96);
    uint64_t core = cvmx_get_core_num();
    __cvmx_l2c_tag_t tag_val;
    uint64_t dbg_addr = CVMX_L2C_DBG;
    uint32_t flags;

    cvmx_l2c_dbg_t debug_val;
    debug_val.u64 = 0;
    /* For low core count parts, the core number is always small enough
    ** to stay in the correct field and not set any reserved bits */
    debug_val.s.ppnum = core;
    debug_val.s.l2t = 1;
    debug_val.s.set = assoc;

    CVMX_SYNC;  /* Make sure core is quiet (no prefetches, etc.) before entering debug mode */
    CVMX_DCACHE_INVALIDATE;  /* Flush L1 to make sure debug load misses L1 */

    flags = cvmx_interrupt_disable_save();

    /* The following must be done in assembly as when in debug mode all data loads from
    ** L2 return special debug data, not normal memory contents.  Also, interrupts must be disabled,
    ** since if an interrupt occurs while in debug mode the ISR will get debug data from all its memory
    ** reads instead of the contents of memory */

        asm volatile (
    "        .set push              \n"
    "        .set mips64              \n"
    "        .set noreorder           \n"
    "        sd    %[dbg_val], 0(%[dbg_addr])  \n"   /* Enter debug mode, wait for store */
    "        ld    $0, 0(%[dbg_addr]) \n"
    "        ld    %[tag_val], 0(%[tag_addr]) \n"   /* Read L2C tag data */
    "        sd    $0, 0(%[dbg_addr])  \n"          /* Exit debug mode, wait for store */
    "        ld    $0, 0(%[dbg_addr]) \n"
    "        cache 9, 0($0) \n"             /* Invalidate dcache to discard debug data */
    "        .set pop             \n"
    :[tag_val] "=r" (tag_val):  [dbg_addr] "r" (dbg_addr), [dbg_val] "r" (debug_val), [tag_addr] "r" (debug_tag_addr) : "memory");

    cvmx_interrupt_restore(flags);

    return(tag_val);

}


cvmx_l2c_tag_t cvmx_l2c_get_tag(uint32_t association, uint32_t index)
{
    __cvmx_l2c_tag_t tmp_tag;
    cvmx_l2c_tag_t tag;
    tag.u64 = 0;

    if ((int)association >= cvmx_l2c_get_num_assoc())
    {
        cvmx_dprintf("ERROR: cvmx_get_l2c_tag association out of range\n");
        return(tag);
    }
    if ((int)index >= cvmx_l2c_get_num_sets())
    {
        cvmx_dprintf("ERROR: cvmx_get_l2c_tag index out of range (arg: %d, max: %d)\n", (int)index, cvmx_l2c_get_num_sets());
        return(tag);
    }
    /* __read_l2_tag is intended for internal use only */
    tmp_tag = __read_l2_tag(association, index);

    /* Convert all tag structure types to generic version, as it can represent all models */
    if (OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN56XX))
    {
        tag.s.V    = tmp_tag.cn58xx.V;
        tag.s.D    = tmp_tag.cn58xx.D;
        tag.s.L    = tmp_tag.cn58xx.L;
        tag.s.U    = tmp_tag.cn58xx.U;
        tag.s.addr = tmp_tag.cn58xx.addr;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN38XX))
    {
        tag.s.V    = tmp_tag.cn38xx.V;
        tag.s.D    = tmp_tag.cn38xx.D;
        tag.s.L    = tmp_tag.cn38xx.L;
        tag.s.U    = tmp_tag.cn38xx.U;
        tag.s.addr = tmp_tag.cn38xx.addr;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN52XX))
    {
        tag.s.V    = tmp_tag.cn31xx.V;
        tag.s.D    = tmp_tag.cn31xx.D;
        tag.s.L    = tmp_tag.cn31xx.L;
        tag.s.U    = tmp_tag.cn31xx.U;
        tag.s.addr = tmp_tag.cn31xx.addr;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN30XX))
    {
        tag.s.V    = tmp_tag.cn30xx.V;
        tag.s.D    = tmp_tag.cn30xx.D;
        tag.s.L    = tmp_tag.cn30xx.L;
        tag.s.U    = tmp_tag.cn30xx.U;
        tag.s.addr = tmp_tag.cn30xx.addr;
    }
    else if (OCTEON_IS_MODEL(OCTEON_CN50XX))
    {
        tag.s.V    = tmp_tag.cn50xx.V;
        tag.s.D    = tmp_tag.cn50xx.D;
        tag.s.L    = tmp_tag.cn50xx.L;
        tag.s.U    = tmp_tag.cn50xx.U;
        tag.s.addr = tmp_tag.cn50xx.addr;
    }
    else
    {
        cvmx_dprintf("Unsupported OCTEON Model in %s\n", __FUNCTION__);
    }

    return tag;
}

#endif

uint32_t cvmx_l2c_address_to_index (uint64_t addr)
{
    uint64_t idx = addr >> CVMX_L2C_IDX_ADDR_SHIFT;
    cvmx_l2c_cfg_t l2c_cfg;
    l2c_cfg.u64 = cvmx_read_csr(CVMX_L2C_CFG);

    if (l2c_cfg.s.idxalias)
    {
        idx ^= ((addr & CVMX_L2C_ALIAS_MASK) >> CVMX_L2C_TAG_ADDR_ALIAS_SHIFT);
    }
    idx &= CVMX_L2C_IDX_MASK;
    return(idx);
}

int cvmx_l2c_get_cache_size_bytes(void)
{
    return (cvmx_l2c_get_num_sets() * cvmx_l2c_get_num_assoc() * CVMX_CACHE_LINE_SIZE);
}

/**
 * Return log base 2 of the number of sets in the L2 cache
 * @return
 */
int cvmx_l2c_get_set_bits(void)
{
    int l2_set_bits;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX) ||
        OCTEON_IS_MODEL(OCTEON_CN58XX))
        l2_set_bits =  11; /* 2048 sets */
    else if (OCTEON_IS_MODEL(OCTEON_CN38XX))
        l2_set_bits =  10; /* 1024 sets */
    else if (OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN52XX))
        l2_set_bits =  9; /* 512 sets */
    else if (OCTEON_IS_MODEL(OCTEON_CN30XX))
        l2_set_bits =  8; /* 256 sets */
    else if (OCTEON_IS_MODEL(OCTEON_CN50XX))
        l2_set_bits =  7; /* 128 sets */
    else
    {
        cvmx_dprintf("Unsupported OCTEON Model in %s\n", __FUNCTION__);
        l2_set_bits =  11; /* 2048 sets */
    }
    return(l2_set_bits);

}

/* Return the number of sets in the L2 Cache */
int cvmx_l2c_get_num_sets(void)
{
    return (1 << cvmx_l2c_get_set_bits());
}

/* Return the number of associations in the L2 Cache */
int cvmx_l2c_get_num_assoc(void)
{
    int l2_assoc;
    if (OCTEON_IS_MODEL(OCTEON_CN56XX) ||
        OCTEON_IS_MODEL(OCTEON_CN52XX) ||
        OCTEON_IS_MODEL(OCTEON_CN58XX) ||
        OCTEON_IS_MODEL(OCTEON_CN50XX) ||
        OCTEON_IS_MODEL(OCTEON_CN38XX))
        l2_assoc =  8;
    else if (OCTEON_IS_MODEL(OCTEON_CN31XX) || 
             OCTEON_IS_MODEL(OCTEON_CN30XX))
        l2_assoc =  4;
    else
    {
        cvmx_dprintf("Unsupported OCTEON Model in %s\n", __FUNCTION__);
        l2_assoc =  8;
    }

    /* Check to see if part of the cache is disabled */
    if (cvmx_fuse_read(265))
        l2_assoc = l2_assoc >> 2;
    else if (cvmx_fuse_read(264))
        l2_assoc = l2_assoc >> 1;

    return(l2_assoc);
}


#ifndef CVMX_BUILD_FOR_LINUX_HOST
/**
 * Flush a line from the L2 cache
 * This should only be called from one core at a time, as this routine
 * sets the core to the 'debug' core in order to flush the line.
 *
 * @param assoc  Association (or way) to flush
 * @param index  Index to flush
 */
void cvmx_l2c_flush_line(uint32_t assoc, uint32_t index)
{
    cvmx_l2c_dbg_t l2cdbg;

    l2cdbg.u64 = 0;
    l2cdbg.s.ppnum = cvmx_get_core_num();
    l2cdbg.s.finv = 1;

    l2cdbg.s.set = assoc;
    /* Enter debug mode, and make sure all other writes complete before we
    ** enter debug mode */
    asm volatile ("sync \n"::: "memory");
    cvmx_write_csr(CVMX_L2C_DBG, l2cdbg.u64);
    cvmx_read_csr(CVMX_L2C_DBG);

    CVMX_PREPARE_FOR_STORE (((1ULL << 63) + (index)*128), 0);
    /* Exit debug mode */
    asm volatile ("sync \n"::: "memory");
    cvmx_write_csr(CVMX_L2C_DBG, 0);
    cvmx_read_csr(CVMX_L2C_DBG);
}
#endif
