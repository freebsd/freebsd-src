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
 * cvmx-fpa-defs.h
 *
 * Configuration and status register (CSR) type definitions for
 * Octeon fpa.
 *
 * This file is auto generated. Do not edit.
 *
 * <hr>$Revision$<hr>
 *
 */
#ifndef __CVMX_FPA_TYPEDEFS_H__
#define __CVMX_FPA_TYPEDEFS_H__

#define CVMX_FPA_BIST_STATUS (CVMX_ADD_IO_SEG(0x00011800280000E8ull))
#define CVMX_FPA_CTL_STATUS (CVMX_ADD_IO_SEG(0x0001180028000050ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_FPA_FPF0_MARKS CVMX_FPA_FPF0_MARKS_FUNC()
static inline uint64_t CVMX_FPA_FPF0_MARKS_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_FPA_FPF0_MARKS not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180028000000ull);
}
#else
#define CVMX_FPA_FPF0_MARKS (CVMX_ADD_IO_SEG(0x0001180028000000ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_FPA_FPF0_SIZE CVMX_FPA_FPF0_SIZE_FUNC()
static inline uint64_t CVMX_FPA_FPF0_SIZE_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_FPA_FPF0_SIZE not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180028000058ull);
}
#else
#define CVMX_FPA_FPF0_SIZE (CVMX_ADD_IO_SEG(0x0001180028000058ull))
#endif
#define CVMX_FPA_FPF1_MARKS CVMX_FPA_FPFX_MARKS(1)
#define CVMX_FPA_FPF2_MARKS CVMX_FPA_FPFX_MARKS(2)
#define CVMX_FPA_FPF3_MARKS CVMX_FPA_FPFX_MARKS(3)
#define CVMX_FPA_FPF4_MARKS CVMX_FPA_FPFX_MARKS(4)
#define CVMX_FPA_FPF5_MARKS CVMX_FPA_FPFX_MARKS(5)
#define CVMX_FPA_FPF6_MARKS CVMX_FPA_FPFX_MARKS(6)
#define CVMX_FPA_FPF7_MARKS CVMX_FPA_FPFX_MARKS(7)
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_FPA_FPFX_MARKS(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset >= 1) && (offset <= 7)))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset >= 1) && (offset <= 7)))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset >= 1) && (offset <= 7)))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && (((offset >= 1) && (offset <= 7))))))
		cvmx_warn("CVMX_FPA_FPFX_MARKS(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180028000008ull) + ((offset) & 7) * 8 - 8*1;
}
#else
#define CVMX_FPA_FPFX_MARKS(offset) (CVMX_ADD_IO_SEG(0x0001180028000008ull) + ((offset) & 7) * 8 - 8*1)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_FPA_FPFX_SIZE(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset >= 1) && (offset <= 7)))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset >= 1) && (offset <= 7)))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset >= 1) && (offset <= 7)))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && (((offset >= 1) && (offset <= 7))))))
		cvmx_warn("CVMX_FPA_FPFX_SIZE(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180028000060ull) + ((offset) & 7) * 8 - 8*1;
}
#else
#define CVMX_FPA_FPFX_SIZE(offset) (CVMX_ADD_IO_SEG(0x0001180028000060ull) + ((offset) & 7) * 8 - 8*1)
#endif
#define CVMX_FPA_INT_ENB (CVMX_ADD_IO_SEG(0x0001180028000048ull))
#define CVMX_FPA_INT_SUM (CVMX_ADD_IO_SEG(0x0001180028000040ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_FPA_PACKET_THRESHOLD CVMX_FPA_PACKET_THRESHOLD_FUNC()
static inline uint64_t CVMX_FPA_PACKET_THRESHOLD_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_FPA_PACKET_THRESHOLD not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180028000460ull);
}
#else
#define CVMX_FPA_PACKET_THRESHOLD (CVMX_ADD_IO_SEG(0x0001180028000460ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_FPA_POOLX_THRESHOLD(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 7)))))
		cvmx_warn("CVMX_FPA_POOLX_THRESHOLD(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180028000140ull) + ((offset) & 7) * 8;
}
#else
#define CVMX_FPA_POOLX_THRESHOLD(offset) (CVMX_ADD_IO_SEG(0x0001180028000140ull) + ((offset) & 7) * 8)
#endif
#define CVMX_FPA_QUE0_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(0)
#define CVMX_FPA_QUE1_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(1)
#define CVMX_FPA_QUE2_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(2)
#define CVMX_FPA_QUE3_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(3)
#define CVMX_FPA_QUE4_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(4)
#define CVMX_FPA_QUE5_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(5)
#define CVMX_FPA_QUE6_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(6)
#define CVMX_FPA_QUE7_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(7)
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_FPA_QUEX_AVAILABLE(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 7)))))
		cvmx_warn("CVMX_FPA_QUEX_AVAILABLE(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180028000098ull) + ((offset) & 7) * 8;
}
#else
#define CVMX_FPA_QUEX_AVAILABLE(offset) (CVMX_ADD_IO_SEG(0x0001180028000098ull) + ((offset) & 7) * 8)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_FPA_QUEX_PAGE_INDEX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 7)))))
		cvmx_warn("CVMX_FPA_QUEX_PAGE_INDEX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x00011800280000F0ull) + ((offset) & 7) * 8;
}
#else
#define CVMX_FPA_QUEX_PAGE_INDEX(offset) (CVMX_ADD_IO_SEG(0x00011800280000F0ull) + ((offset) & 7) * 8)
#endif
#define CVMX_FPA_QUE_ACT (CVMX_ADD_IO_SEG(0x0001180028000138ull))
#define CVMX_FPA_QUE_EXP (CVMX_ADD_IO_SEG(0x0001180028000130ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_FPA_WART_CTL CVMX_FPA_WART_CTL_FUNC()
static inline uint64_t CVMX_FPA_WART_CTL_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN5XXX)))
		cvmx_warn("CVMX_FPA_WART_CTL not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x00011800280000D8ull);
}
#else
#define CVMX_FPA_WART_CTL (CVMX_ADD_IO_SEG(0x00011800280000D8ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_FPA_WART_STATUS CVMX_FPA_WART_STATUS_FUNC()
static inline uint64_t CVMX_FPA_WART_STATUS_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN5XXX)))
		cvmx_warn("CVMX_FPA_WART_STATUS not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x00011800280000E0ull);
}
#else
#define CVMX_FPA_WART_STATUS (CVMX_ADD_IO_SEG(0x00011800280000E0ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_FPA_WQE_THRESHOLD CVMX_FPA_WQE_THRESHOLD_FUNC()
static inline uint64_t CVMX_FPA_WQE_THRESHOLD_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_FPA_WQE_THRESHOLD not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180028000468ull);
}
#else
#define CVMX_FPA_WQE_THRESHOLD (CVMX_ADD_IO_SEG(0x0001180028000468ull))
#endif

/**
 * cvmx_fpa_bist_status
 *
 * FPA_BIST_STATUS = BIST Status of FPA Memories
 *
 * The result of the BIST run on the FPA memories.
 */
union cvmx_fpa_bist_status
{
	uint64_t u64;
	struct cvmx_fpa_bist_status_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_5_63                : 59;
	uint64_t frd                          : 1;  /**< fpa_frd  memory bist status. */
	uint64_t fpf0                         : 1;  /**< fpa_fpf0 memory bist status. */
	uint64_t fpf1                         : 1;  /**< fpa_fpf1 memory bist status. */
	uint64_t ffr                          : 1;  /**< fpa_ffr  memory bist status. */
	uint64_t fdr                          : 1;  /**< fpa_fdr  memory bist status. */
#else
	uint64_t fdr                          : 1;
	uint64_t ffr                          : 1;
	uint64_t fpf1                         : 1;
	uint64_t fpf0                         : 1;
	uint64_t frd                          : 1;
	uint64_t reserved_5_63                : 59;
#endif
	} s;
	struct cvmx_fpa_bist_status_s         cn30xx;
	struct cvmx_fpa_bist_status_s         cn31xx;
	struct cvmx_fpa_bist_status_s         cn38xx;
	struct cvmx_fpa_bist_status_s         cn38xxp2;
	struct cvmx_fpa_bist_status_s         cn50xx;
	struct cvmx_fpa_bist_status_s         cn52xx;
	struct cvmx_fpa_bist_status_s         cn52xxp1;
	struct cvmx_fpa_bist_status_s         cn56xx;
	struct cvmx_fpa_bist_status_s         cn56xxp1;
	struct cvmx_fpa_bist_status_s         cn58xx;
	struct cvmx_fpa_bist_status_s         cn58xxp1;
	struct cvmx_fpa_bist_status_s         cn63xx;
	struct cvmx_fpa_bist_status_s         cn63xxp1;
};
typedef union cvmx_fpa_bist_status cvmx_fpa_bist_status_t;

/**
 * cvmx_fpa_ctl_status
 *
 * FPA_CTL_STATUS = FPA's Control/Status Register
 *
 * The FPA's interrupt enable register.
 */
union cvmx_fpa_ctl_status
{
	uint64_t u64;
	struct cvmx_fpa_ctl_status_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_21_63               : 43;
	uint64_t free_en                      : 1;  /**< Enables the setting of the INT_SUM_[FREE*] bits. */
	uint64_t ret_off                      : 1;  /**< When set NCB devices returning pointer will be
                                                         stalled. */
	uint64_t req_off                      : 1;  /**< When set NCB devices requesting pointers will be
                                                         stalled. */
	uint64_t reset                        : 1;  /**< When set causes a reset of the FPA with the
                                                         exception of the RSL. This is a PASS-2 field. */
	uint64_t use_ldt                      : 1;  /**< When clear '0' the FPA will use LDT to load
                                                         pointers from the L2C. This is a PASS-2 field. */
	uint64_t use_stt                      : 1;  /**< When clear '0' the FPA will use STT to store
                                                         pointers to the L2C. This is a PASS-2 field. */
	uint64_t enb                          : 1;  /**< Must be set to 1 AFTER writing all config registers
                                                         and 10 cycles have past. If any of the config
                                                         register are written after writing this bit the
                                                         FPA may begin to operate incorrectly. */
	uint64_t mem1_err                     : 7;  /**< Causes a flip of the ECC bit associated 38:32
                                                         respective to bit 6:0 of this field, for FPF
                                                         FIFO 1. */
	uint64_t mem0_err                     : 7;  /**< Causes a flip of the ECC bit associated 38:32
                                                         respective to bit 6:0 of this field, for FPF
                                                         FIFO 0. */
#else
	uint64_t mem0_err                     : 7;
	uint64_t mem1_err                     : 7;
	uint64_t enb                          : 1;
	uint64_t use_stt                      : 1;
	uint64_t use_ldt                      : 1;
	uint64_t reset                        : 1;
	uint64_t req_off                      : 1;
	uint64_t ret_off                      : 1;
	uint64_t free_en                      : 1;
	uint64_t reserved_21_63               : 43;
#endif
	} s;
	struct cvmx_fpa_ctl_status_cn30xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_18_63               : 46;
	uint64_t reset                        : 1;  /**< When set causes a reset of the FPA with the
                                                         exception of the RSL. */
	uint64_t use_ldt                      : 1;  /**< When clear '0' the FPA will use LDT to load
                                                         pointers from the L2C. */
	uint64_t use_stt                      : 1;  /**< When clear '0' the FPA will use STT to store
                                                         pointers to the L2C. */
	uint64_t enb                          : 1;  /**< Must be set to 1 AFTER writing all config registers
                                                         and 10 cycles have past. If any of the config
                                                         register are written after writing this bit the
                                                         FPA may begin to operate incorrectly. */
	uint64_t mem1_err                     : 7;  /**< Causes a flip of the ECC bit associated 38:32
                                                         respective to bit 6:0 of this field, for FPF
                                                         FIFO 1. */
	uint64_t mem0_err                     : 7;  /**< Causes a flip of the ECC bit associated 38:32
                                                         respective to bit 6:0 of this field, for FPF
                                                         FIFO 0. */
#else
	uint64_t mem0_err                     : 7;
	uint64_t mem1_err                     : 7;
	uint64_t enb                          : 1;
	uint64_t use_stt                      : 1;
	uint64_t use_ldt                      : 1;
	uint64_t reset                        : 1;
	uint64_t reserved_18_63               : 46;
#endif
	} cn30xx;
	struct cvmx_fpa_ctl_status_cn30xx     cn31xx;
	struct cvmx_fpa_ctl_status_cn30xx     cn38xx;
	struct cvmx_fpa_ctl_status_cn30xx     cn38xxp2;
	struct cvmx_fpa_ctl_status_cn30xx     cn50xx;
	struct cvmx_fpa_ctl_status_cn30xx     cn52xx;
	struct cvmx_fpa_ctl_status_cn30xx     cn52xxp1;
	struct cvmx_fpa_ctl_status_cn30xx     cn56xx;
	struct cvmx_fpa_ctl_status_cn30xx     cn56xxp1;
	struct cvmx_fpa_ctl_status_cn30xx     cn58xx;
	struct cvmx_fpa_ctl_status_cn30xx     cn58xxp1;
	struct cvmx_fpa_ctl_status_s          cn63xx;
	struct cvmx_fpa_ctl_status_cn30xx     cn63xxp1;
};
typedef union cvmx_fpa_ctl_status cvmx_fpa_ctl_status_t;

/**
 * cvmx_fpa_fpf#_marks
 *
 * FPA_FPF1_MARKS = FPA's Queue 1 Free Page FIFO Read Write Marks
 *
 * The high and low watermark register that determines when we write and read free pages from L2C
 * for Queue 1. The value of FPF_RD and FPF_WR should have at least a 33 difference. Recommend value
 * is FPF_RD == (FPA_FPF#_SIZE[FPF_SIZ] * .25) and FPF_WR == (FPA_FPF#_SIZE[FPF_SIZ] * .75)
 */
union cvmx_fpa_fpfx_marks
{
	uint64_t u64;
	struct cvmx_fpa_fpfx_marks_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_22_63               : 42;
	uint64_t fpf_wr                       : 11; /**< When the number of free-page-pointers in a
                                                          queue exceeds this value the FPA will write
                                                          32-page-pointers of that queue to DRAM.
                                                         The MAX value for this field should be
                                                         FPA_FPF1_SIZE[FPF_SIZ]-2. */
	uint64_t fpf_rd                       : 11; /**< When the number of free-page-pointers in a
                                                          queue drops below this value and there are
                                                          free-page-pointers in DRAM, the FPA will
                                                          read one page (32 pointers) from DRAM.
                                                         This maximum value for this field should be
                                                         FPA_FPF1_SIZE[FPF_SIZ]-34. The min number
                                                         for this would be 16. */
#else
	uint64_t fpf_rd                       : 11;
	uint64_t fpf_wr                       : 11;
	uint64_t reserved_22_63               : 42;
#endif
	} s;
	struct cvmx_fpa_fpfx_marks_s          cn38xx;
	struct cvmx_fpa_fpfx_marks_s          cn38xxp2;
	struct cvmx_fpa_fpfx_marks_s          cn56xx;
	struct cvmx_fpa_fpfx_marks_s          cn56xxp1;
	struct cvmx_fpa_fpfx_marks_s          cn58xx;
	struct cvmx_fpa_fpfx_marks_s          cn58xxp1;
	struct cvmx_fpa_fpfx_marks_s          cn63xx;
	struct cvmx_fpa_fpfx_marks_s          cn63xxp1;
};
typedef union cvmx_fpa_fpfx_marks cvmx_fpa_fpfx_marks_t;

/**
 * cvmx_fpa_fpf#_size
 *
 * FPA_FPFX_SIZE = FPA's Queue 1-7 Free Page FIFO Size
 *
 * The number of page pointers that will be kept local to the FPA for this Queue. FPA Queues are
 * assigned in order from Queue 0 to Queue 7, though only Queue 0 through Queue x can be used.
 * The sum of the 8 (0-7) FPA_FPF#_SIZE registers must be limited to 2048.
 */
union cvmx_fpa_fpfx_size
{
	uint64_t u64;
	struct cvmx_fpa_fpfx_size_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_11_63               : 53;
	uint64_t fpf_siz                      : 11; /**< The number of entries assigned in the FPA FIFO
                                                         (used to hold page-pointers) for this Queue.
                                                         The value of this register must divisable by 2,
                                                         and the FPA will ignore bit [0] of this register.
                                                         The total of the FPF_SIZ field of the 8 (0-7)
                                                         FPA_FPF#_SIZE registers must not exceed 2048.
                                                         After writing this field the FPA will need 10
                                                         core clock cycles to be ready for operation. The
                                                         assignment of location in the FPA FIFO must
                                                         start with Queue 0, then 1, 2, etc.
                                                         The number of useable entries will be FPF_SIZ-2. */
#else
	uint64_t fpf_siz                      : 11;
	uint64_t reserved_11_63               : 53;
#endif
	} s;
	struct cvmx_fpa_fpfx_size_s           cn38xx;
	struct cvmx_fpa_fpfx_size_s           cn38xxp2;
	struct cvmx_fpa_fpfx_size_s           cn56xx;
	struct cvmx_fpa_fpfx_size_s           cn56xxp1;
	struct cvmx_fpa_fpfx_size_s           cn58xx;
	struct cvmx_fpa_fpfx_size_s           cn58xxp1;
	struct cvmx_fpa_fpfx_size_s           cn63xx;
	struct cvmx_fpa_fpfx_size_s           cn63xxp1;
};
typedef union cvmx_fpa_fpfx_size cvmx_fpa_fpfx_size_t;

/**
 * cvmx_fpa_fpf0_marks
 *
 * FPA_FPF0_MARKS = FPA's Queue 0 Free Page FIFO Read Write Marks
 *
 * The high and low watermark register that determines when we write and read free pages from L2C
 * for Queue 0. The value of FPF_RD and FPF_WR should have at least a 33 difference. Recommend value
 * is FPF_RD == (FPA_FPF#_SIZE[FPF_SIZ] * .25) and FPF_WR == (FPA_FPF#_SIZE[FPF_SIZ] * .75)
 */
union cvmx_fpa_fpf0_marks
{
	uint64_t u64;
	struct cvmx_fpa_fpf0_marks_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_24_63               : 40;
	uint64_t fpf_wr                       : 12; /**< When the number of free-page-pointers in a
                                                          queue exceeds this value the FPA will write
                                                          32-page-pointers of that queue to DRAM.
                                                         The MAX value for this field should be
                                                         FPA_FPF0_SIZE[FPF_SIZ]-2. */
	uint64_t fpf_rd                       : 12; /**< When the number of free-page-pointers in a
                                                         queue drops below this value and there are
                                                         free-page-pointers in DRAM, the FPA will
                                                         read one page (32 pointers) from DRAM.
                                                         This maximum value for this field should be
                                                         FPA_FPF0_SIZE[FPF_SIZ]-34. The min number
                                                         for this would be 16. */
#else
	uint64_t fpf_rd                       : 12;
	uint64_t fpf_wr                       : 12;
	uint64_t reserved_24_63               : 40;
#endif
	} s;
	struct cvmx_fpa_fpf0_marks_s          cn38xx;
	struct cvmx_fpa_fpf0_marks_s          cn38xxp2;
	struct cvmx_fpa_fpf0_marks_s          cn56xx;
	struct cvmx_fpa_fpf0_marks_s          cn56xxp1;
	struct cvmx_fpa_fpf0_marks_s          cn58xx;
	struct cvmx_fpa_fpf0_marks_s          cn58xxp1;
	struct cvmx_fpa_fpf0_marks_s          cn63xx;
	struct cvmx_fpa_fpf0_marks_s          cn63xxp1;
};
typedef union cvmx_fpa_fpf0_marks cvmx_fpa_fpf0_marks_t;

/**
 * cvmx_fpa_fpf0_size
 *
 * FPA_FPF0_SIZE = FPA's Queue 0 Free Page FIFO Size
 *
 * The number of page pointers that will be kept local to the FPA for this Queue. FPA Queues are
 * assigned in order from Queue 0 to Queue 7, though only Queue 0 through Queue x can be used.
 * The sum of the 8 (0-7) FPA_FPF#_SIZE registers must be limited to 2048.
 */
union cvmx_fpa_fpf0_size
{
	uint64_t u64;
	struct cvmx_fpa_fpf0_size_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_12_63               : 52;
	uint64_t fpf_siz                      : 12; /**< The number of entries assigned in the FPA FIFO
                                                         (used to hold page-pointers) for this Queue.
                                                         The value of this register must divisable by 2,
                                                         and the FPA will ignore bit [0] of this register.
                                                         The total of the FPF_SIZ field of the 8 (0-7)
                                                         FPA_FPF#_SIZE registers must not exceed 2048.
                                                         After writing this field the FPA will need 10
                                                         core clock cycles to be ready for operation. The
                                                         assignment of location in the FPA FIFO must
                                                         start with Queue 0, then 1, 2, etc.
                                                         The number of useable entries will be FPF_SIZ-2. */
#else
	uint64_t fpf_siz                      : 12;
	uint64_t reserved_12_63               : 52;
#endif
	} s;
	struct cvmx_fpa_fpf0_size_s           cn38xx;
	struct cvmx_fpa_fpf0_size_s           cn38xxp2;
	struct cvmx_fpa_fpf0_size_s           cn56xx;
	struct cvmx_fpa_fpf0_size_s           cn56xxp1;
	struct cvmx_fpa_fpf0_size_s           cn58xx;
	struct cvmx_fpa_fpf0_size_s           cn58xxp1;
	struct cvmx_fpa_fpf0_size_s           cn63xx;
	struct cvmx_fpa_fpf0_size_s           cn63xxp1;
};
typedef union cvmx_fpa_fpf0_size cvmx_fpa_fpf0_size_t;

/**
 * cvmx_fpa_int_enb
 *
 * FPA_INT_ENB = FPA's Interrupt Enable
 *
 * The FPA's interrupt enable register.
 */
union cvmx_fpa_int_enb
{
	uint64_t u64;
	struct cvmx_fpa_int_enb_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_44_63               : 20;
	uint64_t free7                        : 1;  /**< When set (1) and bit 43 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t free6                        : 1;  /**< When set (1) and bit 42 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t free5                        : 1;  /**< When set (1) and bit 41 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t free4                        : 1;  /**< When set (1) and bit 40 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t free3                        : 1;  /**< When set (1) and bit 39 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t free2                        : 1;  /**< When set (1) and bit 38 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t free1                        : 1;  /**< When set (1) and bit 37 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t free0                        : 1;  /**< When set (1) and bit 36 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t pool7th                      : 1;  /**< When set (1) and bit 35 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t pool6th                      : 1;  /**< When set (1) and bit 34 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t pool5th                      : 1;  /**< When set (1) and bit 33 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t pool4th                      : 1;  /**< When set (1) and bit 32 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t pool3th                      : 1;  /**< When set (1) and bit 31 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t pool2th                      : 1;  /**< When set (1) and bit 30 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t pool1th                      : 1;  /**< When set (1) and bit 29 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t pool0th                      : 1;  /**< When set (1) and bit 28 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q7_perr                      : 1;  /**< When set (1) and bit 27 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q7_coff                      : 1;  /**< When set (1) and bit 26 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q7_und                       : 1;  /**< When set (1) and bit 25 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q6_perr                      : 1;  /**< When set (1) and bit 24 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q6_coff                      : 1;  /**< When set (1) and bit 23 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q6_und                       : 1;  /**< When set (1) and bit 22 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q5_perr                      : 1;  /**< When set (1) and bit 21 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q5_coff                      : 1;  /**< When set (1) and bit 20 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q5_und                       : 1;  /**< When set (1) and bit 19 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q4_perr                      : 1;  /**< When set (1) and bit 18 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q4_coff                      : 1;  /**< When set (1) and bit 17 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q4_und                       : 1;  /**< When set (1) and bit 16 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q3_perr                      : 1;  /**< When set (1) and bit 15 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q3_coff                      : 1;  /**< When set (1) and bit 14 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q3_und                       : 1;  /**< When set (1) and bit 13 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q2_perr                      : 1;  /**< When set (1) and bit 12 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q2_coff                      : 1;  /**< When set (1) and bit 11 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q2_und                       : 1;  /**< When set (1) and bit 10 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q1_perr                      : 1;  /**< When set (1) and bit 9 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q1_coff                      : 1;  /**< When set (1) and bit 8 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q1_und                       : 1;  /**< When set (1) and bit 7 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q0_perr                      : 1;  /**< When set (1) and bit 6 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q0_coff                      : 1;  /**< When set (1) and bit 5 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q0_und                       : 1;  /**< When set (1) and bit 4 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t fed1_dbe                     : 1;  /**< When set (1) and bit 3 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t fed1_sbe                     : 1;  /**< When set (1) and bit 2 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t fed0_dbe                     : 1;  /**< When set (1) and bit 1 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t fed0_sbe                     : 1;  /**< When set (1) and bit 0 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
#else
	uint64_t fed0_sbe                     : 1;
	uint64_t fed0_dbe                     : 1;
	uint64_t fed1_sbe                     : 1;
	uint64_t fed1_dbe                     : 1;
	uint64_t q0_und                       : 1;
	uint64_t q0_coff                      : 1;
	uint64_t q0_perr                      : 1;
	uint64_t q1_und                       : 1;
	uint64_t q1_coff                      : 1;
	uint64_t q1_perr                      : 1;
	uint64_t q2_und                       : 1;
	uint64_t q2_coff                      : 1;
	uint64_t q2_perr                      : 1;
	uint64_t q3_und                       : 1;
	uint64_t q3_coff                      : 1;
	uint64_t q3_perr                      : 1;
	uint64_t q4_und                       : 1;
	uint64_t q4_coff                      : 1;
	uint64_t q4_perr                      : 1;
	uint64_t q5_und                       : 1;
	uint64_t q5_coff                      : 1;
	uint64_t q5_perr                      : 1;
	uint64_t q6_und                       : 1;
	uint64_t q6_coff                      : 1;
	uint64_t q6_perr                      : 1;
	uint64_t q7_und                       : 1;
	uint64_t q7_coff                      : 1;
	uint64_t q7_perr                      : 1;
	uint64_t pool0th                      : 1;
	uint64_t pool1th                      : 1;
	uint64_t pool2th                      : 1;
	uint64_t pool3th                      : 1;
	uint64_t pool4th                      : 1;
	uint64_t pool5th                      : 1;
	uint64_t pool6th                      : 1;
	uint64_t pool7th                      : 1;
	uint64_t free0                        : 1;
	uint64_t free1                        : 1;
	uint64_t free2                        : 1;
	uint64_t free3                        : 1;
	uint64_t free4                        : 1;
	uint64_t free5                        : 1;
	uint64_t free6                        : 1;
	uint64_t free7                        : 1;
	uint64_t reserved_44_63               : 20;
#endif
	} s;
	struct cvmx_fpa_int_enb_cn30xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_28_63               : 36;
	uint64_t q7_perr                      : 1;  /**< When set (1) and bit 27 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q7_coff                      : 1;  /**< When set (1) and bit 26 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q7_und                       : 1;  /**< When set (1) and bit 25 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q6_perr                      : 1;  /**< When set (1) and bit 24 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q6_coff                      : 1;  /**< When set (1) and bit 23 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q6_und                       : 1;  /**< When set (1) and bit 22 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q5_perr                      : 1;  /**< When set (1) and bit 21 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q5_coff                      : 1;  /**< When set (1) and bit 20 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q5_und                       : 1;  /**< When set (1) and bit 19 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q4_perr                      : 1;  /**< When set (1) and bit 18 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q4_coff                      : 1;  /**< When set (1) and bit 17 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q4_und                       : 1;  /**< When set (1) and bit 16 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q3_perr                      : 1;  /**< When set (1) and bit 15 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q3_coff                      : 1;  /**< When set (1) and bit 14 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q3_und                       : 1;  /**< When set (1) and bit 13 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q2_perr                      : 1;  /**< When set (1) and bit 12 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q2_coff                      : 1;  /**< When set (1) and bit 11 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q2_und                       : 1;  /**< When set (1) and bit 10 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q1_perr                      : 1;  /**< When set (1) and bit 9 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q1_coff                      : 1;  /**< When set (1) and bit 8 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q1_und                       : 1;  /**< When set (1) and bit 7 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q0_perr                      : 1;  /**< When set (1) and bit 6 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q0_coff                      : 1;  /**< When set (1) and bit 5 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t q0_und                       : 1;  /**< When set (1) and bit 4 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t fed1_dbe                     : 1;  /**< When set (1) and bit 3 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t fed1_sbe                     : 1;  /**< When set (1) and bit 2 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t fed0_dbe                     : 1;  /**< When set (1) and bit 1 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
	uint64_t fed0_sbe                     : 1;  /**< When set (1) and bit 0 of the FPA_INT_SUM
                                                         register is asserted the FPA will assert an
                                                         interrupt. */
#else
	uint64_t fed0_sbe                     : 1;
	uint64_t fed0_dbe                     : 1;
	uint64_t fed1_sbe                     : 1;
	uint64_t fed1_dbe                     : 1;
	uint64_t q0_und                       : 1;
	uint64_t q0_coff                      : 1;
	uint64_t q0_perr                      : 1;
	uint64_t q1_und                       : 1;
	uint64_t q1_coff                      : 1;
	uint64_t q1_perr                      : 1;
	uint64_t q2_und                       : 1;
	uint64_t q2_coff                      : 1;
	uint64_t q2_perr                      : 1;
	uint64_t q3_und                       : 1;
	uint64_t q3_coff                      : 1;
	uint64_t q3_perr                      : 1;
	uint64_t q4_und                       : 1;
	uint64_t q4_coff                      : 1;
	uint64_t q4_perr                      : 1;
	uint64_t q5_und                       : 1;
	uint64_t q5_coff                      : 1;
	uint64_t q5_perr                      : 1;
	uint64_t q6_und                       : 1;
	uint64_t q6_coff                      : 1;
	uint64_t q6_perr                      : 1;
	uint64_t q7_und                       : 1;
	uint64_t q7_coff                      : 1;
	uint64_t q7_perr                      : 1;
	uint64_t reserved_28_63               : 36;
#endif
	} cn30xx;
	struct cvmx_fpa_int_enb_cn30xx        cn31xx;
	struct cvmx_fpa_int_enb_cn30xx        cn38xx;
	struct cvmx_fpa_int_enb_cn30xx        cn38xxp2;
	struct cvmx_fpa_int_enb_cn30xx        cn50xx;
	struct cvmx_fpa_int_enb_cn30xx        cn52xx;
	struct cvmx_fpa_int_enb_cn30xx        cn52xxp1;
	struct cvmx_fpa_int_enb_cn30xx        cn56xx;
	struct cvmx_fpa_int_enb_cn30xx        cn56xxp1;
	struct cvmx_fpa_int_enb_cn30xx        cn58xx;
	struct cvmx_fpa_int_enb_cn30xx        cn58xxp1;
	struct cvmx_fpa_int_enb_s             cn63xx;
	struct cvmx_fpa_int_enb_cn30xx        cn63xxp1;
};
typedef union cvmx_fpa_int_enb cvmx_fpa_int_enb_t;

/**
 * cvmx_fpa_int_sum
 *
 * FPA_INT_SUM = FPA's Interrupt Summary Register
 *
 * Contains the different interrupt summary bits of the FPA.
 */
union cvmx_fpa_int_sum
{
	uint64_t u64;
	struct cvmx_fpa_int_sum_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_44_63               : 20;
	uint64_t free7                        : 1;  /**< When a pointer for POOL7 is freed bit is set. */
	uint64_t free6                        : 1;  /**< When a pointer for POOL6 is freed bit is set. */
	uint64_t free5                        : 1;  /**< When a pointer for POOL5 is freed bit is set. */
	uint64_t free4                        : 1;  /**< When a pointer for POOL4 is freed bit is set. */
	uint64_t free3                        : 1;  /**< When a pointer for POOL3 is freed bit is set. */
	uint64_t free2                        : 1;  /**< When a pointer for POOL2 is freed bit is set. */
	uint64_t free1                        : 1;  /**< When a pointer for POOL1 is freed bit is set. */
	uint64_t free0                        : 1;  /**< When a pointer for POOL0 is freed bit is set. */
	uint64_t pool7th                      : 1;  /**< Set when FPA_QUE7_AVAILABLE is equal to
                                                         FPA_POOL7_THRESHOLD[THRESH] and a pointer is
                                                         allocated or de-allocated. */
	uint64_t pool6th                      : 1;  /**< Set when FPA_QUE6_AVAILABLE is equal to
                                                         FPA_POOL6_THRESHOLD[THRESH] and a pointer is
                                                         allocated or de-allocated. */
	uint64_t pool5th                      : 1;  /**< Set when FPA_QUE5_AVAILABLE is equal to
                                                         FPA_POOL5_THRESHOLD[THRESH] and a pointer is
                                                         allocated or de-allocated. */
	uint64_t pool4th                      : 1;  /**< Set when FPA_QUE4_AVAILABLE is equal to
                                                         FPA_POOL4_THRESHOLD[THRESH] and a pointer is
                                                         allocated or de-allocated. */
	uint64_t pool3th                      : 1;  /**< Set when FPA_QUE3_AVAILABLE is equal to
                                                         FPA_POOL3_THRESHOLD[THRESH] and a pointer is
                                                         allocated or de-allocated. */
	uint64_t pool2th                      : 1;  /**< Set when FPA_QUE2_AVAILABLE is equal to
                                                         FPA_POOL2_THRESHOLD[THRESH] and a pointer is
                                                         allocated or de-allocated. */
	uint64_t pool1th                      : 1;  /**< Set when FPA_QUE1_AVAILABLE is equal to
                                                         FPA_POOL1_THRESHOLD[THRESH] and a pointer is
                                                         allocated or de-allocated. */
	uint64_t pool0th                      : 1;  /**< Set when FPA_QUE0_AVAILABLE is equal to
                                                         FPA_POOL`_THRESHOLD[THRESH] and a pointer is
                                                         allocated or de-allocated. */
	uint64_t q7_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q7_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q7_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q6_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q6_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q6_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q5_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q5_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q5_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q4_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q4_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q4_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q3_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q3_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q3_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q2_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q2_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q2_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q1_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q1_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than pointers
                                                         present in the FPA. */
	uint64_t q1_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q0_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q0_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than pointers
                                                         present in the FPA. */
	uint64_t q0_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t fed1_dbe                     : 1;  /**< Set when a Double Bit Error is detected in FPF1. */
	uint64_t fed1_sbe                     : 1;  /**< Set when a Single Bit Error is detected in FPF1. */
	uint64_t fed0_dbe                     : 1;  /**< Set when a Double Bit Error is detected in FPF0. */
	uint64_t fed0_sbe                     : 1;  /**< Set when a Single Bit Error is detected in FPF0. */
#else
	uint64_t fed0_sbe                     : 1;
	uint64_t fed0_dbe                     : 1;
	uint64_t fed1_sbe                     : 1;
	uint64_t fed1_dbe                     : 1;
	uint64_t q0_und                       : 1;
	uint64_t q0_coff                      : 1;
	uint64_t q0_perr                      : 1;
	uint64_t q1_und                       : 1;
	uint64_t q1_coff                      : 1;
	uint64_t q1_perr                      : 1;
	uint64_t q2_und                       : 1;
	uint64_t q2_coff                      : 1;
	uint64_t q2_perr                      : 1;
	uint64_t q3_und                       : 1;
	uint64_t q3_coff                      : 1;
	uint64_t q3_perr                      : 1;
	uint64_t q4_und                       : 1;
	uint64_t q4_coff                      : 1;
	uint64_t q4_perr                      : 1;
	uint64_t q5_und                       : 1;
	uint64_t q5_coff                      : 1;
	uint64_t q5_perr                      : 1;
	uint64_t q6_und                       : 1;
	uint64_t q6_coff                      : 1;
	uint64_t q6_perr                      : 1;
	uint64_t q7_und                       : 1;
	uint64_t q7_coff                      : 1;
	uint64_t q7_perr                      : 1;
	uint64_t pool0th                      : 1;
	uint64_t pool1th                      : 1;
	uint64_t pool2th                      : 1;
	uint64_t pool3th                      : 1;
	uint64_t pool4th                      : 1;
	uint64_t pool5th                      : 1;
	uint64_t pool6th                      : 1;
	uint64_t pool7th                      : 1;
	uint64_t free0                        : 1;
	uint64_t free1                        : 1;
	uint64_t free2                        : 1;
	uint64_t free3                        : 1;
	uint64_t free4                        : 1;
	uint64_t free5                        : 1;
	uint64_t free6                        : 1;
	uint64_t free7                        : 1;
	uint64_t reserved_44_63               : 20;
#endif
	} s;
	struct cvmx_fpa_int_sum_cn30xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_28_63               : 36;
	uint64_t q7_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q7_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q7_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q6_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q6_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q6_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q5_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q5_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q5_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q4_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q4_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q4_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q3_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q3_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q3_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q2_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q2_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than than pointers
                                                         present in the FPA. */
	uint64_t q2_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q1_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q1_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than pointers
                                                         present in the FPA. */
	uint64_t q1_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t q0_perr                      : 1;  /**< Set when a Queue0 pointer read from the stack in
                                                         the L2C does not have the FPA owner ship bit set. */
	uint64_t q0_coff                      : 1;  /**< Set when a Queue0 stack end tag is present and
                                                         the count available is greater than pointers
                                                         present in the FPA. */
	uint64_t q0_und                       : 1;  /**< Set when a Queue0 page count available goes
                                                         negative. */
	uint64_t fed1_dbe                     : 1;  /**< Set when a Double Bit Error is detected in FPF1. */
	uint64_t fed1_sbe                     : 1;  /**< Set when a Single Bit Error is detected in FPF1. */
	uint64_t fed0_dbe                     : 1;  /**< Set when a Double Bit Error is detected in FPF0. */
	uint64_t fed0_sbe                     : 1;  /**< Set when a Single Bit Error is detected in FPF0. */
#else
	uint64_t fed0_sbe                     : 1;
	uint64_t fed0_dbe                     : 1;
	uint64_t fed1_sbe                     : 1;
	uint64_t fed1_dbe                     : 1;
	uint64_t q0_und                       : 1;
	uint64_t q0_coff                      : 1;
	uint64_t q0_perr                      : 1;
	uint64_t q1_und                       : 1;
	uint64_t q1_coff                      : 1;
	uint64_t q1_perr                      : 1;
	uint64_t q2_und                       : 1;
	uint64_t q2_coff                      : 1;
	uint64_t q2_perr                      : 1;
	uint64_t q3_und                       : 1;
	uint64_t q3_coff                      : 1;
	uint64_t q3_perr                      : 1;
	uint64_t q4_und                       : 1;
	uint64_t q4_coff                      : 1;
	uint64_t q4_perr                      : 1;
	uint64_t q5_und                       : 1;
	uint64_t q5_coff                      : 1;
	uint64_t q5_perr                      : 1;
	uint64_t q6_und                       : 1;
	uint64_t q6_coff                      : 1;
	uint64_t q6_perr                      : 1;
	uint64_t q7_und                       : 1;
	uint64_t q7_coff                      : 1;
	uint64_t q7_perr                      : 1;
	uint64_t reserved_28_63               : 36;
#endif
	} cn30xx;
	struct cvmx_fpa_int_sum_cn30xx        cn31xx;
	struct cvmx_fpa_int_sum_cn30xx        cn38xx;
	struct cvmx_fpa_int_sum_cn30xx        cn38xxp2;
	struct cvmx_fpa_int_sum_cn30xx        cn50xx;
	struct cvmx_fpa_int_sum_cn30xx        cn52xx;
	struct cvmx_fpa_int_sum_cn30xx        cn52xxp1;
	struct cvmx_fpa_int_sum_cn30xx        cn56xx;
	struct cvmx_fpa_int_sum_cn30xx        cn56xxp1;
	struct cvmx_fpa_int_sum_cn30xx        cn58xx;
	struct cvmx_fpa_int_sum_cn30xx        cn58xxp1;
	struct cvmx_fpa_int_sum_s             cn63xx;
	struct cvmx_fpa_int_sum_cn30xx        cn63xxp1;
};
typedef union cvmx_fpa_int_sum cvmx_fpa_int_sum_t;

/**
 * cvmx_fpa_packet_threshold
 *
 * FPA_PACKET_THRESHOLD = FPA's Packet Threshold
 *
 * When the value of FPA_QUE0_AVAILABLE[QUE_SIZ] is Less than the value of this register a low pool count signal is sent to the
 * PCIe packet instruction engine (to make it stop reading instructions) and to the Packet-Arbiter informing it to not give grants
 * to packets MAC with the exception of the PCIe MAC.
 */
union cvmx_fpa_packet_threshold
{
	uint64_t u64;
	struct cvmx_fpa_packet_threshold_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t thresh                       : 32; /**< Packet Threshold. */
#else
	uint64_t thresh                       : 32;
	uint64_t reserved_32_63               : 32;
#endif
	} s;
	struct cvmx_fpa_packet_threshold_s    cn63xx;
};
typedef union cvmx_fpa_packet_threshold cvmx_fpa_packet_threshold_t;

/**
 * cvmx_fpa_pool#_threshold
 *
 * FPA_POOLX_THRESHOLD = FPA's Pool 0-7 Threshold
 *
 * When the value of FPA_QUEX_AVAILABLE is equal to FPA_POOLX_THRESHOLD[THRESH] when a pointer is allocated
 * or deallocated, set interrupt FPA_INT_SUM[POOLXTH].
 */
union cvmx_fpa_poolx_threshold
{
	uint64_t u64;
	struct cvmx_fpa_poolx_threshold_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_29_63               : 35;
	uint64_t thresh                       : 29; /**< The Threshold. */
#else
	uint64_t thresh                       : 29;
	uint64_t reserved_29_63               : 35;
#endif
	} s;
	struct cvmx_fpa_poolx_threshold_s     cn63xx;
};
typedef union cvmx_fpa_poolx_threshold cvmx_fpa_poolx_threshold_t;

/**
 * cvmx_fpa_que#_available
 *
 * FPA_QUEX_PAGES_AVAILABLE = FPA's Queue 0-7 Free Page Available Register
 *
 * The number of page pointers that are available in the FPA and local DRAM.
 */
union cvmx_fpa_quex_available
{
	uint64_t u64;
	struct cvmx_fpa_quex_available_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_29_63               : 35;
	uint64_t que_siz                      : 29; /**< The number of free pages available in this Queue.
                                                         In PASS-1 this field was [25:0]. */
#else
	uint64_t que_siz                      : 29;
	uint64_t reserved_29_63               : 35;
#endif
	} s;
	struct cvmx_fpa_quex_available_s      cn30xx;
	struct cvmx_fpa_quex_available_s      cn31xx;
	struct cvmx_fpa_quex_available_s      cn38xx;
	struct cvmx_fpa_quex_available_s      cn38xxp2;
	struct cvmx_fpa_quex_available_s      cn50xx;
	struct cvmx_fpa_quex_available_s      cn52xx;
	struct cvmx_fpa_quex_available_s      cn52xxp1;
	struct cvmx_fpa_quex_available_s      cn56xx;
	struct cvmx_fpa_quex_available_s      cn56xxp1;
	struct cvmx_fpa_quex_available_s      cn58xx;
	struct cvmx_fpa_quex_available_s      cn58xxp1;
	struct cvmx_fpa_quex_available_s      cn63xx;
	struct cvmx_fpa_quex_available_s      cn63xxp1;
};
typedef union cvmx_fpa_quex_available cvmx_fpa_quex_available_t;

/**
 * cvmx_fpa_que#_page_index
 *
 * FPA_QUE0_PAGE_INDEX = FPA's Queue0 Page Index
 *
 * The present index page for queue 0 of the FPA, this is a PASS-2 register.
 * This number reflects the number of pages of pointers that have been written to memory
 * for this queue.
 */
union cvmx_fpa_quex_page_index
{
	uint64_t u64;
	struct cvmx_fpa_quex_page_index_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_25_63               : 39;
	uint64_t pg_num                       : 25; /**< Page number. */
#else
	uint64_t pg_num                       : 25;
	uint64_t reserved_25_63               : 39;
#endif
	} s;
	struct cvmx_fpa_quex_page_index_s     cn30xx;
	struct cvmx_fpa_quex_page_index_s     cn31xx;
	struct cvmx_fpa_quex_page_index_s     cn38xx;
	struct cvmx_fpa_quex_page_index_s     cn38xxp2;
	struct cvmx_fpa_quex_page_index_s     cn50xx;
	struct cvmx_fpa_quex_page_index_s     cn52xx;
	struct cvmx_fpa_quex_page_index_s     cn52xxp1;
	struct cvmx_fpa_quex_page_index_s     cn56xx;
	struct cvmx_fpa_quex_page_index_s     cn56xxp1;
	struct cvmx_fpa_quex_page_index_s     cn58xx;
	struct cvmx_fpa_quex_page_index_s     cn58xxp1;
	struct cvmx_fpa_quex_page_index_s     cn63xx;
	struct cvmx_fpa_quex_page_index_s     cn63xxp1;
};
typedef union cvmx_fpa_quex_page_index cvmx_fpa_quex_page_index_t;

/**
 * cvmx_fpa_que_act
 *
 * FPA_QUE_ACT = FPA's Queue# Actual Page Index
 *
 * When a INT_SUM[PERR#] occurs this will be latched with the value read from L2C. PASS-2 register.
 * This is latched on the first error and will not latch again unitl all errors are cleared.
 */
union cvmx_fpa_que_act
{
	uint64_t u64;
	struct cvmx_fpa_que_act_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_29_63               : 35;
	uint64_t act_que                      : 3;  /**< FPA-queue-number read from memory. */
	uint64_t act_indx                     : 26; /**< Page number read from memory. */
#else
	uint64_t act_indx                     : 26;
	uint64_t act_que                      : 3;
	uint64_t reserved_29_63               : 35;
#endif
	} s;
	struct cvmx_fpa_que_act_s             cn30xx;
	struct cvmx_fpa_que_act_s             cn31xx;
	struct cvmx_fpa_que_act_s             cn38xx;
	struct cvmx_fpa_que_act_s             cn38xxp2;
	struct cvmx_fpa_que_act_s             cn50xx;
	struct cvmx_fpa_que_act_s             cn52xx;
	struct cvmx_fpa_que_act_s             cn52xxp1;
	struct cvmx_fpa_que_act_s             cn56xx;
	struct cvmx_fpa_que_act_s             cn56xxp1;
	struct cvmx_fpa_que_act_s             cn58xx;
	struct cvmx_fpa_que_act_s             cn58xxp1;
	struct cvmx_fpa_que_act_s             cn63xx;
	struct cvmx_fpa_que_act_s             cn63xxp1;
};
typedef union cvmx_fpa_que_act cvmx_fpa_que_act_t;

/**
 * cvmx_fpa_que_exp
 *
 * FPA_QUE_EXP = FPA's Queue# Expected Page Index
 *
 * When a INT_SUM[PERR#] occurs this will be latched with the expected value. PASS-2 register.
 * This is latched on the first error and will not latch again unitl all errors are cleared.
 */
union cvmx_fpa_que_exp
{
	uint64_t u64;
	struct cvmx_fpa_que_exp_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_29_63               : 35;
	uint64_t exp_que                      : 3;  /**< Expected fpa-queue-number read from memory. */
	uint64_t exp_indx                     : 26; /**< Expected page number read from memory. */
#else
	uint64_t exp_indx                     : 26;
	uint64_t exp_que                      : 3;
	uint64_t reserved_29_63               : 35;
#endif
	} s;
	struct cvmx_fpa_que_exp_s             cn30xx;
	struct cvmx_fpa_que_exp_s             cn31xx;
	struct cvmx_fpa_que_exp_s             cn38xx;
	struct cvmx_fpa_que_exp_s             cn38xxp2;
	struct cvmx_fpa_que_exp_s             cn50xx;
	struct cvmx_fpa_que_exp_s             cn52xx;
	struct cvmx_fpa_que_exp_s             cn52xxp1;
	struct cvmx_fpa_que_exp_s             cn56xx;
	struct cvmx_fpa_que_exp_s             cn56xxp1;
	struct cvmx_fpa_que_exp_s             cn58xx;
	struct cvmx_fpa_que_exp_s             cn58xxp1;
	struct cvmx_fpa_que_exp_s             cn63xx;
	struct cvmx_fpa_que_exp_s             cn63xxp1;
};
typedef union cvmx_fpa_que_exp cvmx_fpa_que_exp_t;

/**
 * cvmx_fpa_wart_ctl
 *
 * FPA_WART_CTL = FPA's WART Control
 *
 * Control and status for the WART block.
 */
union cvmx_fpa_wart_ctl
{
	uint64_t u64;
	struct cvmx_fpa_wart_ctl_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_16_63               : 48;
	uint64_t ctl                          : 16; /**< Control information. */
#else
	uint64_t ctl                          : 16;
	uint64_t reserved_16_63               : 48;
#endif
	} s;
	struct cvmx_fpa_wart_ctl_s            cn30xx;
	struct cvmx_fpa_wart_ctl_s            cn31xx;
	struct cvmx_fpa_wart_ctl_s            cn38xx;
	struct cvmx_fpa_wart_ctl_s            cn38xxp2;
	struct cvmx_fpa_wart_ctl_s            cn50xx;
	struct cvmx_fpa_wart_ctl_s            cn52xx;
	struct cvmx_fpa_wart_ctl_s            cn52xxp1;
	struct cvmx_fpa_wart_ctl_s            cn56xx;
	struct cvmx_fpa_wart_ctl_s            cn56xxp1;
	struct cvmx_fpa_wart_ctl_s            cn58xx;
	struct cvmx_fpa_wart_ctl_s            cn58xxp1;
};
typedef union cvmx_fpa_wart_ctl cvmx_fpa_wart_ctl_t;

/**
 * cvmx_fpa_wart_status
 *
 * FPA_WART_STATUS = FPA's WART Status
 *
 * Control and status for the WART block.
 */
union cvmx_fpa_wart_status
{
	uint64_t u64;
	struct cvmx_fpa_wart_status_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t status                       : 32; /**< Status information. */
#else
	uint64_t status                       : 32;
	uint64_t reserved_32_63               : 32;
#endif
	} s;
	struct cvmx_fpa_wart_status_s         cn30xx;
	struct cvmx_fpa_wart_status_s         cn31xx;
	struct cvmx_fpa_wart_status_s         cn38xx;
	struct cvmx_fpa_wart_status_s         cn38xxp2;
	struct cvmx_fpa_wart_status_s         cn50xx;
	struct cvmx_fpa_wart_status_s         cn52xx;
	struct cvmx_fpa_wart_status_s         cn52xxp1;
	struct cvmx_fpa_wart_status_s         cn56xx;
	struct cvmx_fpa_wart_status_s         cn56xxp1;
	struct cvmx_fpa_wart_status_s         cn58xx;
	struct cvmx_fpa_wart_status_s         cn58xxp1;
};
typedef union cvmx_fpa_wart_status cvmx_fpa_wart_status_t;

/**
 * cvmx_fpa_wqe_threshold
 *
 * FPA_WQE_THRESHOLD = FPA's WQE Threshold
 *
 * When the value of FPA_QUE#_AVAILABLE[QUE_SIZ] (\# is determined by the value of IPD_WQE_FPA_QUEUE) is Less than the value of this
 * register a low pool count signal is sent to the PCIe packet instruction engine (to make it stop reading instructions) and to the
 * Packet-Arbiter informing it to not give grants to packets MAC with the exception of the PCIe MAC.
 */
union cvmx_fpa_wqe_threshold
{
	uint64_t u64;
	struct cvmx_fpa_wqe_threshold_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t thresh                       : 32; /**< WQE Threshold. */
#else
	uint64_t thresh                       : 32;
	uint64_t reserved_32_63               : 32;
#endif
	} s;
	struct cvmx_fpa_wqe_threshold_s       cn63xx;
};
typedef union cvmx_fpa_wqe_threshold cvmx_fpa_wqe_threshold_t;

#endif
