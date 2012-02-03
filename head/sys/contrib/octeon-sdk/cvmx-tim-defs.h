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
 * cvmx-tim-defs.h
 *
 * Configuration and status register (CSR) type definitions for
 * Octeon tim.
 *
 * This file is auto generated. Do not edit.
 *
 * <hr>$Revision$<hr>
 *
 */
#ifndef __CVMX_TIM_TYPEDEFS_H__
#define __CVMX_TIM_TYPEDEFS_H__

#define CVMX_TIM_MEM_DEBUG0 (CVMX_ADD_IO_SEG(0x0001180058001100ull))
#define CVMX_TIM_MEM_DEBUG1 (CVMX_ADD_IO_SEG(0x0001180058001108ull))
#define CVMX_TIM_MEM_DEBUG2 (CVMX_ADD_IO_SEG(0x0001180058001110ull))
#define CVMX_TIM_MEM_RING0 (CVMX_ADD_IO_SEG(0x0001180058001000ull))
#define CVMX_TIM_MEM_RING1 (CVMX_ADD_IO_SEG(0x0001180058001008ull))
#define CVMX_TIM_REG_BIST_RESULT (CVMX_ADD_IO_SEG(0x0001180058000080ull))
#define CVMX_TIM_REG_ERROR (CVMX_ADD_IO_SEG(0x0001180058000088ull))
#define CVMX_TIM_REG_FLAGS (CVMX_ADD_IO_SEG(0x0001180058000000ull))
#define CVMX_TIM_REG_INT_MASK (CVMX_ADD_IO_SEG(0x0001180058000090ull))
#define CVMX_TIM_REG_READ_IDX (CVMX_ADD_IO_SEG(0x0001180058000008ull))

/**
 * cvmx_tim_mem_debug0
 *
 * Notes:
 * Internal per-ring state intended for debug use only - tim.ctl[47:0]
 * This CSR is a memory of 16 entries, and thus, the TIM_REG_READ_IDX CSR must be written before any
 * CSR read operations to this address can be performed.
 */
union cvmx_tim_mem_debug0
{
	uint64_t u64;
	struct cvmx_tim_mem_debug0_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_48_63               : 16;
	uint64_t ena                          : 1;  /**< Ring timer enable */
	uint64_t reserved_46_46               : 1;
	uint64_t count                        : 22; /**< Time offset for the ring
                                                         Set to INTERVAL and counts down by 1 every 1024
                                                         cycles when ENA==1. The HW forces a bucket
                                                         traversal (and resets COUNT to INTERVAL) whenever
                                                         the decrement would cause COUNT to go negative.
                                                         COUNT is unpredictable whenever ENA==0.
                                                         COUNT is reset to INTERVAL whenever TIM_MEM_RING1
                                                         is written for the ring. */
	uint64_t reserved_22_23               : 2;
	uint64_t interval                     : 22; /**< Timer interval - 1 */
#else
	uint64_t interval                     : 22;
	uint64_t reserved_22_23               : 2;
	uint64_t count                        : 22;
	uint64_t reserved_46_46               : 1;
	uint64_t ena                          : 1;
	uint64_t reserved_48_63               : 16;
#endif
	} s;
	struct cvmx_tim_mem_debug0_s          cn30xx;
	struct cvmx_tim_mem_debug0_s          cn31xx;
	struct cvmx_tim_mem_debug0_s          cn38xx;
	struct cvmx_tim_mem_debug0_s          cn38xxp2;
	struct cvmx_tim_mem_debug0_s          cn50xx;
	struct cvmx_tim_mem_debug0_s          cn52xx;
	struct cvmx_tim_mem_debug0_s          cn52xxp1;
	struct cvmx_tim_mem_debug0_s          cn56xx;
	struct cvmx_tim_mem_debug0_s          cn56xxp1;
	struct cvmx_tim_mem_debug0_s          cn58xx;
	struct cvmx_tim_mem_debug0_s          cn58xxp1;
	struct cvmx_tim_mem_debug0_s          cn63xx;
	struct cvmx_tim_mem_debug0_s          cn63xxp1;
};
typedef union cvmx_tim_mem_debug0 cvmx_tim_mem_debug0_t;

/**
 * cvmx_tim_mem_debug1
 *
 * Notes:
 * Internal per-ring state intended for debug use only - tim.sta[63:0]
 * This CSR is a memory of 16 entries, and thus, the TIM_REG_READ_IDX CSR must be written before any
 * CSR read operations to this address can be performed.
 */
union cvmx_tim_mem_debug1
{
	uint64_t u64;
	struct cvmx_tim_mem_debug1_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t bucket                       : 13; /**< Current bucket[12:0]
                                                         Reset to 0 whenever TIM_MEM_RING0 is written for
                                                         the ring. Incremented (modulo BSIZE) once per
                                                         bucket traversal.
                                                         See TIM_MEM_DEBUG2[BUCKET]. */
	uint64_t base                         : 31; /**< Pointer[35:5] to bucket[0] */
	uint64_t bsize                        : 20; /**< Number of buckets - 1 */
#else
	uint64_t bsize                        : 20;
	uint64_t base                         : 31;
	uint64_t bucket                       : 13;
#endif
	} s;
	struct cvmx_tim_mem_debug1_s          cn30xx;
	struct cvmx_tim_mem_debug1_s          cn31xx;
	struct cvmx_tim_mem_debug1_s          cn38xx;
	struct cvmx_tim_mem_debug1_s          cn38xxp2;
	struct cvmx_tim_mem_debug1_s          cn50xx;
	struct cvmx_tim_mem_debug1_s          cn52xx;
	struct cvmx_tim_mem_debug1_s          cn52xxp1;
	struct cvmx_tim_mem_debug1_s          cn56xx;
	struct cvmx_tim_mem_debug1_s          cn56xxp1;
	struct cvmx_tim_mem_debug1_s          cn58xx;
	struct cvmx_tim_mem_debug1_s          cn58xxp1;
	struct cvmx_tim_mem_debug1_s          cn63xx;
	struct cvmx_tim_mem_debug1_s          cn63xxp1;
};
typedef union cvmx_tim_mem_debug1 cvmx_tim_mem_debug1_t;

/**
 * cvmx_tim_mem_debug2
 *
 * Notes:
 * Internal per-ring state intended for debug use only - tim.sta[95:64]
 * This CSR is a memory of 16 entries, and thus, the TIM_REG_READ_IDX CSR must be written before any
 * CSR read operations to this address can be performed.
 */
union cvmx_tim_mem_debug2
{
	uint64_t u64;
	struct cvmx_tim_mem_debug2_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_24_63               : 40;
	uint64_t cpool                        : 3;  /**< Free list used to free chunks */
	uint64_t csize                        : 13; /**< Number of words per chunk */
	uint64_t reserved_7_7                 : 1;
	uint64_t bucket                       : 7;  /**< Current bucket[19:13]
                                                         See TIM_MEM_DEBUG1[BUCKET]. */
#else
	uint64_t bucket                       : 7;
	uint64_t reserved_7_7                 : 1;
	uint64_t csize                        : 13;
	uint64_t cpool                        : 3;
	uint64_t reserved_24_63               : 40;
#endif
	} s;
	struct cvmx_tim_mem_debug2_s          cn30xx;
	struct cvmx_tim_mem_debug2_s          cn31xx;
	struct cvmx_tim_mem_debug2_s          cn38xx;
	struct cvmx_tim_mem_debug2_s          cn38xxp2;
	struct cvmx_tim_mem_debug2_s          cn50xx;
	struct cvmx_tim_mem_debug2_s          cn52xx;
	struct cvmx_tim_mem_debug2_s          cn52xxp1;
	struct cvmx_tim_mem_debug2_s          cn56xx;
	struct cvmx_tim_mem_debug2_s          cn56xxp1;
	struct cvmx_tim_mem_debug2_s          cn58xx;
	struct cvmx_tim_mem_debug2_s          cn58xxp1;
	struct cvmx_tim_mem_debug2_s          cn63xx;
	struct cvmx_tim_mem_debug2_s          cn63xxp1;
};
typedef union cvmx_tim_mem_debug2 cvmx_tim_mem_debug2_t;

/**
 * cvmx_tim_mem_ring0
 *
 * Notes:
 * TIM_MEM_RING0 must not be written for a ring when TIM_MEM_RING1[ENA] is set for the ring.
 * Every write to TIM_MEM_RING0 clears the current bucket for the ring. (The current bucket is
 * readable via TIM_MEM_DEBUG2[BUCKET],TIM_MEM_DEBUG1[BUCKET].)
 * BASE is a 32-byte aligned pointer[35:0].  Only pointer[35:5] are stored because pointer[4:0] = 0.
 * This CSR is a memory of 16 entries, and thus, the TIM_REG_READ_IDX CSR must be written before any
 * CSR read operations to this address can be performed.
 */
union cvmx_tim_mem_ring0
{
	uint64_t u64;
	struct cvmx_tim_mem_ring0_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_55_63               : 9;
	uint64_t first_bucket                 : 31; /**< Pointer[35:5] to bucket[0] */
	uint64_t num_buckets                  : 20; /**< Number of buckets - 1 */
	uint64_t ring                         : 4;  /**< Ring ID */
#else
	uint64_t ring                         : 4;
	uint64_t num_buckets                  : 20;
	uint64_t first_bucket                 : 31;
	uint64_t reserved_55_63               : 9;
#endif
	} s;
	struct cvmx_tim_mem_ring0_s           cn30xx;
	struct cvmx_tim_mem_ring0_s           cn31xx;
	struct cvmx_tim_mem_ring0_s           cn38xx;
	struct cvmx_tim_mem_ring0_s           cn38xxp2;
	struct cvmx_tim_mem_ring0_s           cn50xx;
	struct cvmx_tim_mem_ring0_s           cn52xx;
	struct cvmx_tim_mem_ring0_s           cn52xxp1;
	struct cvmx_tim_mem_ring0_s           cn56xx;
	struct cvmx_tim_mem_ring0_s           cn56xxp1;
	struct cvmx_tim_mem_ring0_s           cn58xx;
	struct cvmx_tim_mem_ring0_s           cn58xxp1;
	struct cvmx_tim_mem_ring0_s           cn63xx;
	struct cvmx_tim_mem_ring0_s           cn63xxp1;
};
typedef union cvmx_tim_mem_ring0 cvmx_tim_mem_ring0_t;

/**
 * cvmx_tim_mem_ring1
 *
 * Notes:
 * After a 1->0 transition on ENA, the HW will still complete a bucket traversal for the ring
 * if it was pending or active prior to the transition. (SW must delay to ensure the completion
 * of the traversal before reprogramming the ring.)
 * Every write to TIM_MEM_RING1 resets the current time offset for the ring to the INTERVAL value.
 * (The current time offset for the ring is readable via TIM_MEM_DEBUG0[COUNT].)
 * CSIZE must be at least 16.  It is illegal to program CSIZE to a value that is less than 16.
 * This CSR is a memory of 16 entries, and thus, the TIM_REG_READ_IDX CSR must be written before any
 * CSR read operations to this address can be performed.
 */
union cvmx_tim_mem_ring1
{
	uint64_t u64;
	struct cvmx_tim_mem_ring1_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_43_63               : 21;
	uint64_t enable                       : 1;  /**< Ring timer enable
                                                         When clear, the ring is disabled and TIM
                                                         will not traverse any new buckets for the ring. */
	uint64_t pool                         : 3;  /**< Free list used to free chunks */
	uint64_t words_per_chunk              : 13; /**< Number of words per chunk */
	uint64_t interval                     : 22; /**< Timer interval - 1, measured in 1024 cycle ticks */
	uint64_t ring                         : 4;  /**< Ring ID */
#else
	uint64_t ring                         : 4;
	uint64_t interval                     : 22;
	uint64_t words_per_chunk              : 13;
	uint64_t pool                         : 3;
	uint64_t enable                       : 1;
	uint64_t reserved_43_63               : 21;
#endif
	} s;
	struct cvmx_tim_mem_ring1_s           cn30xx;
	struct cvmx_tim_mem_ring1_s           cn31xx;
	struct cvmx_tim_mem_ring1_s           cn38xx;
	struct cvmx_tim_mem_ring1_s           cn38xxp2;
	struct cvmx_tim_mem_ring1_s           cn50xx;
	struct cvmx_tim_mem_ring1_s           cn52xx;
	struct cvmx_tim_mem_ring1_s           cn52xxp1;
	struct cvmx_tim_mem_ring1_s           cn56xx;
	struct cvmx_tim_mem_ring1_s           cn56xxp1;
	struct cvmx_tim_mem_ring1_s           cn58xx;
	struct cvmx_tim_mem_ring1_s           cn58xxp1;
	struct cvmx_tim_mem_ring1_s           cn63xx;
	struct cvmx_tim_mem_ring1_s           cn63xxp1;
};
typedef union cvmx_tim_mem_ring1 cvmx_tim_mem_ring1_t;

/**
 * cvmx_tim_reg_bist_result
 *
 * Notes:
 * Access to the internal BiST results
 * Each bit is the BiST result of an individual memory (per bit, 0=pass and 1=fail).
 */
union cvmx_tim_reg_bist_result
{
	uint64_t u64;
	struct cvmx_tim_reg_bist_result_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_4_63                : 60;
	uint64_t sta                          : 2;  /**< BiST result of the STA   memories (0=pass, !0=fail) */
	uint64_t ncb                          : 1;  /**< BiST result of the NCB   memories (0=pass, !0=fail) */
	uint64_t ctl                          : 1;  /**< BiST result of the CTL   memories (0=pass, !0=fail) */
#else
	uint64_t ctl                          : 1;
	uint64_t ncb                          : 1;
	uint64_t sta                          : 2;
	uint64_t reserved_4_63                : 60;
#endif
	} s;
	struct cvmx_tim_reg_bist_result_s     cn30xx;
	struct cvmx_tim_reg_bist_result_s     cn31xx;
	struct cvmx_tim_reg_bist_result_s     cn38xx;
	struct cvmx_tim_reg_bist_result_s     cn38xxp2;
	struct cvmx_tim_reg_bist_result_s     cn50xx;
	struct cvmx_tim_reg_bist_result_s     cn52xx;
	struct cvmx_tim_reg_bist_result_s     cn52xxp1;
	struct cvmx_tim_reg_bist_result_s     cn56xx;
	struct cvmx_tim_reg_bist_result_s     cn56xxp1;
	struct cvmx_tim_reg_bist_result_s     cn58xx;
	struct cvmx_tim_reg_bist_result_s     cn58xxp1;
	struct cvmx_tim_reg_bist_result_s     cn63xx;
	struct cvmx_tim_reg_bist_result_s     cn63xxp1;
};
typedef union cvmx_tim_reg_bist_result cvmx_tim_reg_bist_result_t;

/**
 * cvmx_tim_reg_error
 *
 * Notes:
 * A ring is in error if its interval has elapsed more than once without having been serviced.
 * During a CSR write to this register, the write data is used as a mask to clear the selected mask
 * bits (mask'[15:0] = mask[15:0] & ~write_data[15:0]).
 */
union cvmx_tim_reg_error
{
	uint64_t u64;
	struct cvmx_tim_reg_error_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_16_63               : 48;
	uint64_t mask                         : 16; /**< Bit mask indicating the rings in error */
#else
	uint64_t mask                         : 16;
	uint64_t reserved_16_63               : 48;
#endif
	} s;
	struct cvmx_tim_reg_error_s           cn30xx;
	struct cvmx_tim_reg_error_s           cn31xx;
	struct cvmx_tim_reg_error_s           cn38xx;
	struct cvmx_tim_reg_error_s           cn38xxp2;
	struct cvmx_tim_reg_error_s           cn50xx;
	struct cvmx_tim_reg_error_s           cn52xx;
	struct cvmx_tim_reg_error_s           cn52xxp1;
	struct cvmx_tim_reg_error_s           cn56xx;
	struct cvmx_tim_reg_error_s           cn56xxp1;
	struct cvmx_tim_reg_error_s           cn58xx;
	struct cvmx_tim_reg_error_s           cn58xxp1;
	struct cvmx_tim_reg_error_s           cn63xx;
	struct cvmx_tim_reg_error_s           cn63xxp1;
};
typedef union cvmx_tim_reg_error cvmx_tim_reg_error_t;

/**
 * cvmx_tim_reg_flags
 *
 * Notes:
 * TIM has a counter that causes a periodic tick every 1024 cycles. This counter is shared by all
 * rings. (Each tick causes the HW to decrement the time offset (i.e. COUNT) for all enabled rings.)
 * When ENA_TIM==0, the HW stops this shared periodic counter, so there are no more ticks, and there
 * are no more new bucket traversals (for any ring).
 *
 * If ENA_TIM transitions 1->0, TIM will no longer create new bucket traversals, but there may
 * have been previous ones. If there are ring bucket traversals that were already pending but
 * not currently active (i.e. bucket traversals that need to be done by the HW, but haven't been yet)
 * during this ENA_TIM 1->0 transition, then these bucket traversals will remain pending until
 * ENA_TIM is later set to one. Bucket traversals that were already in progress will complete
 * after the 1->0 ENA_TIM transition, though.
 */
union cvmx_tim_reg_flags
{
	uint64_t u64;
	struct cvmx_tim_reg_flags_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_3_63                : 61;
	uint64_t reset                        : 1;  /**< Reset oneshot pulse for free-running structures */
	uint64_t enable_dwb                   : 1;  /**< Enables non-zero DonwWriteBacks when set
                                                         When set, enables the use of
                                                         DontWriteBacks during the buffer freeing
                                                         operations. */
	uint64_t enable_timers                : 1;  /**< Enables the TIM section when set
                                                         When set, TIM is in normal operation.
                                                         When clear, time is effectively stopped for all
                                                         rings in TIM. */
#else
	uint64_t enable_timers                : 1;
	uint64_t enable_dwb                   : 1;
	uint64_t reset                        : 1;
	uint64_t reserved_3_63                : 61;
#endif
	} s;
	struct cvmx_tim_reg_flags_s           cn30xx;
	struct cvmx_tim_reg_flags_s           cn31xx;
	struct cvmx_tim_reg_flags_s           cn38xx;
	struct cvmx_tim_reg_flags_s           cn38xxp2;
	struct cvmx_tim_reg_flags_s           cn50xx;
	struct cvmx_tim_reg_flags_s           cn52xx;
	struct cvmx_tim_reg_flags_s           cn52xxp1;
	struct cvmx_tim_reg_flags_s           cn56xx;
	struct cvmx_tim_reg_flags_s           cn56xxp1;
	struct cvmx_tim_reg_flags_s           cn58xx;
	struct cvmx_tim_reg_flags_s           cn58xxp1;
	struct cvmx_tim_reg_flags_s           cn63xx;
	struct cvmx_tim_reg_flags_s           cn63xxp1;
};
typedef union cvmx_tim_reg_flags cvmx_tim_reg_flags_t;

/**
 * cvmx_tim_reg_int_mask
 *
 * Notes:
 * Note that this CSR is present only in chip revisions beginning with pass2.
 * When mask bit is set, the interrupt is enabled.
 */
union cvmx_tim_reg_int_mask
{
	uint64_t u64;
	struct cvmx_tim_reg_int_mask_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_16_63               : 48;
	uint64_t mask                         : 16; /**< Bit mask corresponding to TIM_REG_ERROR.MASK above */
#else
	uint64_t mask                         : 16;
	uint64_t reserved_16_63               : 48;
#endif
	} s;
	struct cvmx_tim_reg_int_mask_s        cn30xx;
	struct cvmx_tim_reg_int_mask_s        cn31xx;
	struct cvmx_tim_reg_int_mask_s        cn38xx;
	struct cvmx_tim_reg_int_mask_s        cn38xxp2;
	struct cvmx_tim_reg_int_mask_s        cn50xx;
	struct cvmx_tim_reg_int_mask_s        cn52xx;
	struct cvmx_tim_reg_int_mask_s        cn52xxp1;
	struct cvmx_tim_reg_int_mask_s        cn56xx;
	struct cvmx_tim_reg_int_mask_s        cn56xxp1;
	struct cvmx_tim_reg_int_mask_s        cn58xx;
	struct cvmx_tim_reg_int_mask_s        cn58xxp1;
	struct cvmx_tim_reg_int_mask_s        cn63xx;
	struct cvmx_tim_reg_int_mask_s        cn63xxp1;
};
typedef union cvmx_tim_reg_int_mask cvmx_tim_reg_int_mask_t;

/**
 * cvmx_tim_reg_read_idx
 *
 * Notes:
 * Provides the read index during a CSR read operation to any of the CSRs that are physically stored
 * as memories.  The names of these CSRs begin with the prefix "TIM_MEM_".
 * IDX[7:0] is the read index.  INC[7:0] is an increment that is added to IDX[7:0] after any CSR read.
 * The intended use is to initially write this CSR such that IDX=0 and INC=1.  Then, the entire
 * contents of a CSR memory can be read with consecutive CSR read commands.
 */
union cvmx_tim_reg_read_idx
{
	uint64_t u64;
	struct cvmx_tim_reg_read_idx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_16_63               : 48;
	uint64_t inc                          : 8;  /**< Increment to add to current index for next index */
	uint64_t index                        : 8;  /**< Index to use for next memory CSR read */
#else
	uint64_t index                        : 8;
	uint64_t inc                          : 8;
	uint64_t reserved_16_63               : 48;
#endif
	} s;
	struct cvmx_tim_reg_read_idx_s        cn30xx;
	struct cvmx_tim_reg_read_idx_s        cn31xx;
	struct cvmx_tim_reg_read_idx_s        cn38xx;
	struct cvmx_tim_reg_read_idx_s        cn38xxp2;
	struct cvmx_tim_reg_read_idx_s        cn50xx;
	struct cvmx_tim_reg_read_idx_s        cn52xx;
	struct cvmx_tim_reg_read_idx_s        cn52xxp1;
	struct cvmx_tim_reg_read_idx_s        cn56xx;
	struct cvmx_tim_reg_read_idx_s        cn56xxp1;
	struct cvmx_tim_reg_read_idx_s        cn58xx;
	struct cvmx_tim_reg_read_idx_s        cn58xxp1;
	struct cvmx_tim_reg_read_idx_s        cn63xx;
	struct cvmx_tim_reg_read_idx_s        cn63xxp1;
};
typedef union cvmx_tim_reg_read_idx cvmx_tim_reg_read_idx_t;

#endif
