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
 * Module to support operations on bitmap of cores. Coremask can be used to
 * select a specific core, a group of cores, or all available cores, for
 * initialization and differentiation of roles within a single shared binary
 * executable image.
 *
 * <hr>$Revision: 49448 $<hr>
 *
 */


#ifndef __CVMX_COREMASK_H__
#define __CVMX_COREMASK_H__

#include "cvmx-asm.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * coremask is simply unsigned int (32 bits).
 *
 * NOTE: supports up to 32 cores maximum.
 *
 * union of coremasks is simply bitwise-or.
 * intersection of coremasks is simply bitwise-and.
 *
 */

#define  CVMX_COREMASK_MAX  0xFFFFFFFFu    /* maximum supported mask */


/**
 * Compute coremask for a specific core.
 *
 * @param  core_id  The core ID
 *
 * @return  coremask for a specific core
 *
 */
static inline unsigned int cvmx_coremask_core(unsigned int core_id)
{
    return (1u << core_id);
}

/**
 * Compute coremask for num_cores cores starting with core 0.
 *
 * @param  num_cores  number of cores
 *
 * @return  coremask for num_cores cores
 *
 */
static inline unsigned int cvmx_coremask_numcores(unsigned int num_cores)
{
    return (CVMX_COREMASK_MAX >> (32 - num_cores));
}

/**
 * Compute coremask for a range of cores from core low to core high.
 *
 * @param  low   first core in the range
 * @param  high  last core in the range
 *
 * @return  coremask for the range of cores
 *
 */
static inline unsigned int cvmx_coremask_range(unsigned int low, unsigned int high)
{
    return ((CVMX_COREMASK_MAX >> (31 - high + low)) << low);
}


/**
 * Test to see if current core is a member of coremask.
 *
 * @param  coremask  the coremask to test against
 *
 * @return  1 if current core is a member of coremask, 0 otherwise
 *
 */
static inline int cvmx_coremask_is_member(unsigned int coremask)
{
    return ((cvmx_coremask_core(cvmx_get_core_num()) & coremask) != 0);
}

/**
 * Test to see if current core is first core in coremask.
 *
 * @param  coremask  the coremask to test against
 *
 * @return  1 if current core is first core in the coremask, 0 otherwise
 *
 */
static inline int cvmx_coremask_first_core(unsigned int coremask)
{
    return cvmx_coremask_is_member(coremask)
        && ((cvmx_get_core_num() == 0) ||
            ((cvmx_coremask_numcores(cvmx_get_core_num()) & coremask) == 0));
}

/**
 * Wait (stall) until all cores in the given coremask has reached this point
 * in the program execution before proceeding.
 *
 * @param  coremask  the group of cores performing the barrier sync
 *
 */
extern void cvmx_coremask_barrier_sync(unsigned int coremask);

#ifdef	__cplusplus
}
#endif

#endif /* __CVMX_COREMASK_H__ */
