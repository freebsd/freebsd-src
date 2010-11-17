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
 * Interface to the Level 2 Cache (L2C) control, measurement, and debugging
 * facilities.
 *
 * <hr>$Revision: 41586 $<hr>
 *
 */

#ifndef __CVMX_L2C_H__
#define __CVMX_L2C_H__

#define CVMX_L2_ASSOC     cvmx_l2c_get_num_assoc()   /* Deprecated macro, use function */
#define CVMX_L2_SET_BITS  cvmx_l2c_get_set_bits()    /* Deprecated macro, use function */
#define CVMX_L2_SETS      cvmx_l2c_get_num_sets()    /* Deprecated macro, use function */


#define CVMX_L2C_IDX_ADDR_SHIFT 7  /* based on 128 byte cache line size */
#define CVMX_L2C_IDX_MASK       (cvmx_l2c_get_num_sets() - 1)

/* Defines for index aliasing computations */
#define CVMX_L2C_TAG_ADDR_ALIAS_SHIFT (CVMX_L2C_IDX_ADDR_SHIFT + cvmx_l2c_get_set_bits())
#define CVMX_L2C_ALIAS_MASK (CVMX_L2C_IDX_MASK << CVMX_L2C_TAG_ADDR_ALIAS_SHIFT)


  /*------------*/
  /*  TYPEDEFS  */
  /*------------*/
typedef union {        // L2C Tag/Data Store Debug Register
  uint64_t    u64;
  struct {
    uint64_t  reserved: 32,
	      lfb_enum:  4,
	      lfb_dmp:   1,
	      ppnum:     4,
	      set:       3,
	      finv:      1,
	      l2d:       1,
	      l2t:       1;
  };
} cvmx_l2c_dbg;

typedef union
{
    uint64_t u64;
#if __BYTE_ORDER == __BIG_ENDIAN
    struct
    {
	uint64_t reserved		: 28;
	uint64_t V			: 1;	// Line valid
	uint64_t D			: 1;	// Line dirty
	uint64_t L			: 1;	// Line locked
	uint64_t U			: 1;	// Use, LRU eviction
	uint64_t addr			: 32;	// Phys mem (not all bits valid)
    } s;
#endif
} cvmx_l2c_tag_t;


  /* L2C Performance Counter events. */
typedef enum
{
    CVMX_L2C_EVENT_CYCLES           =  0,
    CVMX_L2C_EVENT_INSTRUCTION_MISS =  1,
    CVMX_L2C_EVENT_INSTRUCTION_HIT  =  2,
    CVMX_L2C_EVENT_DATA_MISS        =  3,
    CVMX_L2C_EVENT_DATA_HIT         =  4,
    CVMX_L2C_EVENT_MISS             =  5,
    CVMX_L2C_EVENT_HIT              =  6,
    CVMX_L2C_EVENT_VICTIM_HIT       =  7,
    CVMX_L2C_EVENT_INDEX_CONFLICT   =  8,
    CVMX_L2C_EVENT_TAG_PROBE        =  9,
    CVMX_L2C_EVENT_TAG_UPDATE       = 10,
    CVMX_L2C_EVENT_TAG_COMPLETE     = 11,
    CVMX_L2C_EVENT_TAG_DIRTY        = 12,
    CVMX_L2C_EVENT_DATA_STORE_NOP   = 13,
    CVMX_L2C_EVENT_DATA_STORE_READ  = 14,
    CVMX_L2C_EVENT_DATA_STORE_WRITE = 15,
    CVMX_L2C_EVENT_FILL_DATA_VALID  = 16,
    CVMX_L2C_EVENT_WRITE_REQUEST    = 17,
    CVMX_L2C_EVENT_READ_REQUEST     = 18,
    CVMX_L2C_EVENT_WRITE_DATA_VALID = 19,
    CVMX_L2C_EVENT_XMC_NOP          = 20,
    CVMX_L2C_EVENT_XMC_LDT          = 21,
    CVMX_L2C_EVENT_XMC_LDI          = 22,
    CVMX_L2C_EVENT_XMC_LDD          = 23,
    CVMX_L2C_EVENT_XMC_STF          = 24,
    CVMX_L2C_EVENT_XMC_STT          = 25,
    CVMX_L2C_EVENT_XMC_STP          = 26,
    CVMX_L2C_EVENT_XMC_STC          = 27,
    CVMX_L2C_EVENT_XMC_DWB          = 28,
    CVMX_L2C_EVENT_XMC_PL2          = 29,
    CVMX_L2C_EVENT_XMC_PSL1         = 30,
    CVMX_L2C_EVENT_XMC_IOBLD        = 31,
    CVMX_L2C_EVENT_XMC_IOBST        = 32,
    CVMX_L2C_EVENT_XMC_IOBDMA       = 33,
    CVMX_L2C_EVENT_XMC_IOBRSP       = 34,
    CVMX_L2C_EVENT_XMC_BUS_VALID    = 35,
    CVMX_L2C_EVENT_XMC_MEM_DATA     = 36,
    CVMX_L2C_EVENT_XMC_REFL_DATA    = 37,
    CVMX_L2C_EVENT_XMC_IOBRSP_DATA  = 38,
    CVMX_L2C_EVENT_RSC_NOP          = 39,
    CVMX_L2C_EVENT_RSC_STDN         = 40,
    CVMX_L2C_EVENT_RSC_FILL         = 41,
    CVMX_L2C_EVENT_RSC_REFL         = 42,
    CVMX_L2C_EVENT_RSC_STIN         = 43,
    CVMX_L2C_EVENT_RSC_SCIN         = 44,
    CVMX_L2C_EVENT_RSC_SCFL         = 45,
    CVMX_L2C_EVENT_RSC_SCDN         = 46,
    CVMX_L2C_EVENT_RSC_DATA_VALID   = 47,
    CVMX_L2C_EVENT_RSC_VALID_FILL   = 48,
    CVMX_L2C_EVENT_RSC_VALID_STRSP  = 49,
    CVMX_L2C_EVENT_RSC_VALID_REFL   = 50,
    CVMX_L2C_EVENT_LRF_REQ          = 51,
    CVMX_L2C_EVENT_DT_RD_ALLOC      = 52,
    CVMX_L2C_EVENT_DT_WR_INVAL      = 53
} cvmx_l2c_event_t;

/**
 * Configure one of the four L2 Cache performance counters to capture event
 * occurences.
 *
 * @param counter        The counter to configure. Range 0..3.
 * @param event          The type of L2 Cache event occurrence to count.
 * @param clear_on_read  When asserted, any read of the performance counter
 *                       clears the counter.
 *
 * @note The routine does not clear the counter.
 */
void cvmx_l2c_config_perf(uint32_t         counter,
                               cvmx_l2c_event_t event,
                               uint32_t         clear_on_read);
/**
 * Read the given L2 Cache performance counter. The counter must be configured
 * before reading, but this routine does not enforce this requirement.
 *
 * @param counter  The counter to configure. Range 0..3.
 *
 * @return The current counter value.
 */
uint64_t cvmx_l2c_read_perf(uint32_t counter);

/**
 * Return the L2 Cache way partitioning for a given core.
 *
 * @param core  The core processor of interest.
 *
 * @return    The mask specifying the partitioning. 0 bits in mask indicates
 *              the cache 'ways' that a core can evict from.
 *            -1 on error
 */
int cvmx_l2c_get_core_way_partition(uint32_t core);

/**
 * Partitions the L2 cache for a core
 *
 * @param core   The core that the partitioning applies to.
 * @param mask The partitioning of the ways expressed as a binary mask. A 0 bit allows the core
 *             to evict cache lines from a way, while a 1 bit blocks the core from evicting any lines
 *             from that way. There must be at least one allowed way (0 bit) in the mask.
 *
 * @note  If any ways are blocked for all cores and the HW blocks, then those ways will never have
 *        any cache lines evicted from them.  All cores and the hardware blocks are free to read from
 *        all ways regardless of the partitioning.
 */
int cvmx_l2c_set_core_way_partition(uint32_t core, uint32_t mask);

/**
 * Return the L2 Cache way partitioning for the hw blocks.
 *
 * @return    The mask specifying the reserved way. 0 bits in mask indicates
 *              the cache 'ways' that a core can evict from.
 *            -1 on error
 */
int cvmx_l2c_get_hw_way_partition(void);

/**
 * Partitions the L2 cache for the hardware blocks.
 *
 * @param mask The partitioning of the ways expressed as a binary mask. A 0 bit allows the core
 *             to evict cache lines from a way, while a 1 bit blocks the core from evicting any lines
 *             from that way. There must be at least one allowed way (0 bit) in the mask.
 *
 * @note  If any ways are blocked for all cores and the HW blocks, then those ways will never have
 *        any cache lines evicted from them.  All cores and the hardware blocks are free to read from
 *        all ways regardless of the partitioning.
 */
int cvmx_l2c_set_hw_way_partition(uint32_t mask);


/**
 * Locks a line in the L2 cache at the specified physical address
 *
 * @param addr   physical address of line to lock
 *
 * @return 0 on success,
 *         1 if line not locked.
 */
int cvmx_l2c_lock_line(uint64_t addr);

/**
 * Locks a specified memory region in the L2 cache.
 *
 * Note that if not all lines can be locked, that means that all
 * but one of the ways (associations) available to the locking
 * core are locked.  Having only 1 association available for
 * normal caching may have a significant adverse affect on performance.
 * Care should be taken to ensure that enough of the L2 cache is left
 * unlocked to allow for normal caching of DRAM.
 *
 * @param start  Physical address of the start of the region to lock
 * @param len    Length (in bytes) of region to lock
 *
 * @return Number of requested lines that where not locked.
 *         0 on success (all locked)
 */
int cvmx_l2c_lock_mem_region(uint64_t start, uint64_t len);


/**
 * Unlock and flush a cache line from the L2 cache.
 * IMPORTANT: Must only be run by one core at a time due to use
 * of L2C debug features.
 * Note that this function will flush a matching but unlocked cache line.
 * (If address is not in L2, no lines are flushed.)
 *
 * @param address Physical address to unlock
 *
 * @return 0: line not unlocked
 *         1: line unlocked
 */
int cvmx_l2c_unlock_line(uint64_t address);

/**
 * Unlocks a region of memory that is locked in the L2 cache
 *
 * @param start  start physical address
 * @param len    length (in bytes) to unlock
 *
 * @return Number of locked lines that the call unlocked
 */
int cvmx_l2c_unlock_mem_region(uint64_t start, uint64_t len);




/**
 * Read the L2 controller tag for a given location in L2
 *
 * @param association
 *               Which association to read line from
 * @param index  Which way to read from.
 *
 * @return l2c tag structure for line requested.
 */
cvmx_l2c_tag_t cvmx_l2c_get_tag(uint32_t association, uint32_t index);

/* Wrapper around deprecated old function name */
static inline cvmx_l2c_tag_t cvmx_get_l2c_tag(uint32_t association, uint32_t index)
{
    return cvmx_l2c_get_tag(association, index);
}


/**
 * Returns the cache index for a given physical address
 *
 * @param addr   physical address
 *
 * @return L2 cache index
 */
uint32_t cvmx_l2c_address_to_index (uint64_t addr);


/**
 * Flushes (and unlocks) the entire L2 cache.
 * IMPORTANT: Must only be run by one core at a time due to use
 * of L2C debug features.
 */
void cvmx_l2c_flush(void);



/**
 *
 * @return Returns the size of the L2 cache in bytes,
 * -1 on error (unrecognized model)
 */
int cvmx_l2c_get_cache_size_bytes(void);

/**
 * Return the number of sets in the L2 Cache
 *
 * @return
 */
int cvmx_l2c_get_num_sets(void);

/**
 * Return log base 2 of the number of sets in the L2 cache
 * @return
 */
int cvmx_l2c_get_set_bits(void);
/**
 * Return the number of associations in the L2 Cache
 *
 * @return
 */
int cvmx_l2c_get_num_assoc(void);

/**
 * Flush a line from the L2 cache
 * This should only be called from one core at a time, as this routine
 * sets the core to the 'debug' core in order to flush the line.
 *
 * @param assoc  Association (or way) to flush
 * @param index  Index to flush
 */
void cvmx_l2c_flush_line(uint32_t assoc, uint32_t index);

#endif /* __CVMX_L2C_H__ */
