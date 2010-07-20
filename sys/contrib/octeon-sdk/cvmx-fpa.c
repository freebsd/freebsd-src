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
 * Support library for the hardware Free Pool Allocator.
 *
 * <hr>$Revision: 41586 $<hr>
 *
 */

#include "cvmx-config.h"
#include "cvmx.h"
#include "cvmx-fpa.h"
#include "cvmx-ipd.h"

/**
 * Current state of all the pools. Use access functions
 * instead of using it directly.
 */
CVMX_SHARED cvmx_fpa_pool_info_t cvmx_fpa_pool_info[CVMX_FPA_NUM_POOLS];


/**
 * Setup a FPA pool to control a new block of memory. The
 * buffer pointer must be a physical address.
 *
 * @param pool       Pool to initialize
 *                   0 <= pool < 8
 * @param name       Constant character string to name this pool.
 *                   String is not copied.
 * @param buffer     Pointer to the block of memory to use. This must be
 *                   accessable by all processors and external hardware.
 * @param block_size Size for each block controlled by the FPA
 * @param num_blocks Number of blocks
 *
 * @return 0 on Success,
 *         -1 on failure
 */
int cvmx_fpa_setup_pool(uint64_t pool, const char *name, void *buffer,
                         uint64_t block_size, uint64_t num_blocks)
{
    char *ptr;
    if (!buffer)
    {
        cvmx_dprintf("ERROR: cvmx_fpa_setup_pool: NULL buffer pointer!\n");
        return(-1);
    }
    if (pool >= CVMX_FPA_NUM_POOLS)
    {
        cvmx_dprintf("ERROR: cvmx_fpa_setup_pool: Illegal pool!\n");
        return(-1);
    }

    if (block_size < CVMX_FPA_MIN_BLOCK_SIZE)
    {
        cvmx_dprintf("ERROR: cvmx_fpa_setup_pool: Block size too small.\n");
        return(-1);
    }

    if (((unsigned long)buffer & (CVMX_FPA_ALIGNMENT-1)) != 0)
    {
        cvmx_dprintf("ERROR: cvmx_fpa_setup_pool: Buffer not aligned properly.\n");
        return(-1);
    }

    cvmx_fpa_pool_info[pool].name = name;
    cvmx_fpa_pool_info[pool].size = block_size;
    cvmx_fpa_pool_info[pool].starting_element_count = num_blocks;
    cvmx_fpa_pool_info[pool].base = buffer;

    ptr = (char*)buffer;
    while (num_blocks--)
    {
        cvmx_fpa_free(ptr, pool, 0);
        ptr += block_size;
    }
    return(0);
}

/**
 * Shutdown a Memory pool and validate that it had all of
 * the buffers originally placed in it.
 *
 * @param pool   Pool to shutdown
 * @return Zero on success
 *         - Positive is count of missing buffers
 *         - Negative is too many buffers or corrupted pointers
 */
uint64_t cvmx_fpa_shutdown_pool(uint64_t pool)
{
    uint64_t errors = 0;
    uint64_t count  = 0;
    uint64_t base   = cvmx_ptr_to_phys(cvmx_fpa_pool_info[pool].base);
    uint64_t finish = base + cvmx_fpa_pool_info[pool].size * cvmx_fpa_pool_info[pool].starting_element_count;
    void *ptr;
    uint64_t address;

    count = 0;
    do
    {
        ptr = cvmx_fpa_alloc(pool);
        if (ptr)
            address = cvmx_ptr_to_phys(ptr);
        else
            address = 0;
        if (address)
        {
            if ((address >= base) && (address < finish) &&
                (((address - base) % cvmx_fpa_pool_info[pool].size) == 0))
            {
                count++;
            }
            else
            {
                cvmx_dprintf("ERROR: cvmx_fpa_shutdown_pool: Illegal address 0x%llx in pool %s(%d)\n",
                       (unsigned long long)address, cvmx_fpa_pool_info[pool].name, (int)pool);
                errors++;
            }
        }
    } while (address);

#ifdef CVMX_ENABLE_PKO_FUNCTIONS
    if (pool == 0)
	cvmx_ipd_free_ptr();
#endif

    if (errors)
    {
        cvmx_dprintf("ERROR: cvmx_fpa_shutdown_pool: Pool %s(%d) started at 0x%llx, ended at 0x%llx, with a step of 0x%llx\n",
               cvmx_fpa_pool_info[pool].name, (int)pool, (unsigned long long)base, (unsigned long long)finish, (unsigned long long)cvmx_fpa_pool_info[pool].size);
        return -errors;
    }
    else
        return 0;
}

uint64_t cvmx_fpa_get_block_size(uint64_t pool)
{
    switch (pool)
    {
        case 0: return(CVMX_FPA_POOL_0_SIZE);
        case 1: return(CVMX_FPA_POOL_1_SIZE);
        case 2: return(CVMX_FPA_POOL_2_SIZE);
        case 3: return(CVMX_FPA_POOL_3_SIZE);
        case 4: return(CVMX_FPA_POOL_4_SIZE);
        case 5: return(CVMX_FPA_POOL_5_SIZE);
        case 6: return(CVMX_FPA_POOL_6_SIZE);
        case 7: return(CVMX_FPA_POOL_7_SIZE);
        default: return(0);
    }
}
