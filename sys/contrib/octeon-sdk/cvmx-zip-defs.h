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
 * cvmx-zip-defs.h
 *
 * Configuration and status register (CSR) type definitions for
 * Octeon zip.
 *
 * This file is auto generated. Do not edit.
 *
 * <hr>$Revision$<hr>
 *
 */
#ifndef __CVMX_ZIP_TYPEDEFS_H__
#define __CVMX_ZIP_TYPEDEFS_H__

#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_ZIP_CMD_BIST_RESULT CVMX_ZIP_CMD_BIST_RESULT_FUNC()
static inline uint64_t CVMX_ZIP_CMD_BIST_RESULT_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_ZIP_CMD_BIST_RESULT not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180038000080ull);
}
#else
#define CVMX_ZIP_CMD_BIST_RESULT (CVMX_ADD_IO_SEG(0x0001180038000080ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_ZIP_CMD_BUF CVMX_ZIP_CMD_BUF_FUNC()
static inline uint64_t CVMX_ZIP_CMD_BUF_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_ZIP_CMD_BUF not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180038000008ull);
}
#else
#define CVMX_ZIP_CMD_BUF (CVMX_ADD_IO_SEG(0x0001180038000008ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_ZIP_CMD_CTL CVMX_ZIP_CMD_CTL_FUNC()
static inline uint64_t CVMX_ZIP_CMD_CTL_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_ZIP_CMD_CTL not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180038000000ull);
}
#else
#define CVMX_ZIP_CMD_CTL (CVMX_ADD_IO_SEG(0x0001180038000000ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_ZIP_CONSTANTS CVMX_ZIP_CONSTANTS_FUNC()
static inline uint64_t CVMX_ZIP_CONSTANTS_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_ZIP_CONSTANTS not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x00011800380000A0ull);
}
#else
#define CVMX_ZIP_CONSTANTS (CVMX_ADD_IO_SEG(0x00011800380000A0ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_ZIP_DEBUG0 CVMX_ZIP_DEBUG0_FUNC()
static inline uint64_t CVMX_ZIP_DEBUG0_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_ZIP_DEBUG0 not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180038000098ull);
}
#else
#define CVMX_ZIP_DEBUG0 (CVMX_ADD_IO_SEG(0x0001180038000098ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_ZIP_ERROR CVMX_ZIP_ERROR_FUNC()
static inline uint64_t CVMX_ZIP_ERROR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_ZIP_ERROR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180038000088ull);
}
#else
#define CVMX_ZIP_ERROR (CVMX_ADD_IO_SEG(0x0001180038000088ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_ZIP_INT_MASK CVMX_ZIP_INT_MASK_FUNC()
static inline uint64_t CVMX_ZIP_INT_MASK_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_ZIP_INT_MASK not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180038000090ull);
}
#else
#define CVMX_ZIP_INT_MASK (CVMX_ADD_IO_SEG(0x0001180038000090ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_ZIP_THROTTLE CVMX_ZIP_THROTTLE_FUNC()
static inline uint64_t CVMX_ZIP_THROTTLE_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_ZIP_THROTTLE not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180038000010ull);
}
#else
#define CVMX_ZIP_THROTTLE (CVMX_ADD_IO_SEG(0x0001180038000010ull))
#endif

/**
 * cvmx_zip_cmd_bist_result
 *
 * Notes:
 * Access to the internal BiST results
 * Each bit is the BiST result of an individual memory (per bit, 0=pass and 1=fail).
 */
union cvmx_zip_cmd_bist_result
{
	uint64_t u64;
	struct cvmx_zip_cmd_bist_result_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_43_63               : 21;
	uint64_t zip_core                     : 39; /**< BiST result of the ZIP_CORE memories */
	uint64_t zip_ctl                      : 4;  /**< BiST result of the ZIP_CTL  memories */
#else
	uint64_t zip_ctl                      : 4;
	uint64_t zip_core                     : 39;
	uint64_t reserved_43_63               : 21;
#endif
	} s;
	struct cvmx_zip_cmd_bist_result_cn31xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_31_63               : 33;
	uint64_t zip_core                     : 27; /**< BiST result of the ZIP_CORE memories */
	uint64_t zip_ctl                      : 4;  /**< BiST result of the ZIP_CTL  memories */
#else
	uint64_t zip_ctl                      : 4;
	uint64_t zip_core                     : 27;
	uint64_t reserved_31_63               : 33;
#endif
	} cn31xx;
	struct cvmx_zip_cmd_bist_result_cn31xx cn38xx;
	struct cvmx_zip_cmd_bist_result_cn31xx cn38xxp2;
	struct cvmx_zip_cmd_bist_result_cn31xx cn56xx;
	struct cvmx_zip_cmd_bist_result_cn31xx cn56xxp1;
	struct cvmx_zip_cmd_bist_result_cn31xx cn58xx;
	struct cvmx_zip_cmd_bist_result_cn31xx cn58xxp1;
	struct cvmx_zip_cmd_bist_result_s     cn63xx;
	struct cvmx_zip_cmd_bist_result_s     cn63xxp1;
};
typedef union cvmx_zip_cmd_bist_result cvmx_zip_cmd_bist_result_t;

/**
 * cvmx_zip_cmd_buf
 *
 * Notes:
 * Sets the command buffer parameters
 * The size of the command buffer segments is measured in uint64s.  The pool specifies (1 of 8 free
 * lists to be used when freeing command buffer segments.  The PTR field is overwritten with the next
 * pointer each time that the command buffer segment is exhausted.
 * When quiescent (i.e. outstanding doorbell count is 0), it is safe to rewrite
 * this register to effectively reset the command buffer state machine.  New commands will then be
 * read from the newly specified command buffer pointer.
 */
union cvmx_zip_cmd_buf
{
	uint64_t u64;
	struct cvmx_zip_cmd_buf_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_58_63               : 6;
	uint64_t dwb                          : 9;  /**< Number of DontWriteBacks */
	uint64_t pool                         : 3;  /**< Free list used to free command buffer segments */
	uint64_t size                         : 13; /**< Number of uint64s per command buffer segment */
	uint64_t ptr                          : 33; /**< Initial command buffer pointer[39:7] (128B-aligned) */
#else
	uint64_t ptr                          : 33;
	uint64_t size                         : 13;
	uint64_t pool                         : 3;
	uint64_t dwb                          : 9;
	uint64_t reserved_58_63               : 6;
#endif
	} s;
	struct cvmx_zip_cmd_buf_s             cn31xx;
	struct cvmx_zip_cmd_buf_s             cn38xx;
	struct cvmx_zip_cmd_buf_s             cn38xxp2;
	struct cvmx_zip_cmd_buf_s             cn56xx;
	struct cvmx_zip_cmd_buf_s             cn56xxp1;
	struct cvmx_zip_cmd_buf_s             cn58xx;
	struct cvmx_zip_cmd_buf_s             cn58xxp1;
	struct cvmx_zip_cmd_buf_s             cn63xx;
	struct cvmx_zip_cmd_buf_s             cn63xxp1;
};
typedef union cvmx_zip_cmd_buf cvmx_zip_cmd_buf_t;

/**
 * cvmx_zip_cmd_ctl
 */
union cvmx_zip_cmd_ctl
{
	uint64_t u64;
	struct cvmx_zip_cmd_ctl_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t forceclk                     : 1;  /**< Force zip_ctl__clock_on_b == 1 when set */
	uint64_t reset                        : 1;  /**< Reset oneshot pulse for zip core */
#else
	uint64_t reset                        : 1;
	uint64_t forceclk                     : 1;
	uint64_t reserved_2_63                : 62;
#endif
	} s;
	struct cvmx_zip_cmd_ctl_s             cn31xx;
	struct cvmx_zip_cmd_ctl_s             cn38xx;
	struct cvmx_zip_cmd_ctl_s             cn38xxp2;
	struct cvmx_zip_cmd_ctl_s             cn56xx;
	struct cvmx_zip_cmd_ctl_s             cn56xxp1;
	struct cvmx_zip_cmd_ctl_s             cn58xx;
	struct cvmx_zip_cmd_ctl_s             cn58xxp1;
	struct cvmx_zip_cmd_ctl_s             cn63xx;
	struct cvmx_zip_cmd_ctl_s             cn63xxp1;
};
typedef union cvmx_zip_cmd_ctl cvmx_zip_cmd_ctl_t;

/**
 * cvmx_zip_constants
 *
 * Notes:
 * Note that this CSR is present only in chip revisions beginning with pass2.
 *
 */
union cvmx_zip_constants
{
	uint64_t u64;
	struct cvmx_zip_constants_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_48_63               : 16;
	uint64_t depth                        : 16; /**< Maximum search depth for compression */
	uint64_t onfsize                      : 12; /**< Output near full threshhold in bytes */
	uint64_t ctxsize                      : 12; /**< Context size in bytes */
	uint64_t reserved_1_7                 : 7;
	uint64_t disabled                     : 1;  /**< 1=zip unit isdisabled, 0=zip unit not disabled */
#else
	uint64_t disabled                     : 1;
	uint64_t reserved_1_7                 : 7;
	uint64_t ctxsize                      : 12;
	uint64_t onfsize                      : 12;
	uint64_t depth                        : 16;
	uint64_t reserved_48_63               : 16;
#endif
	} s;
	struct cvmx_zip_constants_s           cn31xx;
	struct cvmx_zip_constants_s           cn38xx;
	struct cvmx_zip_constants_s           cn38xxp2;
	struct cvmx_zip_constants_s           cn56xx;
	struct cvmx_zip_constants_s           cn56xxp1;
	struct cvmx_zip_constants_s           cn58xx;
	struct cvmx_zip_constants_s           cn58xxp1;
	struct cvmx_zip_constants_s           cn63xx;
	struct cvmx_zip_constants_s           cn63xxp1;
};
typedef union cvmx_zip_constants cvmx_zip_constants_t;

/**
 * cvmx_zip_debug0
 *
 * Notes:
 * Note that this CSR is present only in chip revisions beginning with pass2.
 *
 */
union cvmx_zip_debug0
{
	uint64_t u64;
	struct cvmx_zip_debug0_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_17_63               : 47;
	uint64_t asserts                      : 17; /**< FIFO assertion checks */
#else
	uint64_t asserts                      : 17;
	uint64_t reserved_17_63               : 47;
#endif
	} s;
	struct cvmx_zip_debug0_cn31xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_14_63               : 50;
	uint64_t asserts                      : 14; /**< FIFO assertion checks */
#else
	uint64_t asserts                      : 14;
	uint64_t reserved_14_63               : 50;
#endif
	} cn31xx;
	struct cvmx_zip_debug0_cn31xx         cn38xx;
	struct cvmx_zip_debug0_cn31xx         cn38xxp2;
	struct cvmx_zip_debug0_cn31xx         cn56xx;
	struct cvmx_zip_debug0_cn31xx         cn56xxp1;
	struct cvmx_zip_debug0_cn31xx         cn58xx;
	struct cvmx_zip_debug0_cn31xx         cn58xxp1;
	struct cvmx_zip_debug0_s              cn63xx;
	struct cvmx_zip_debug0_s              cn63xxp1;
};
typedef union cvmx_zip_debug0 cvmx_zip_debug0_t;

/**
 * cvmx_zip_error
 *
 * Notes:
 * Note that this CSR is present only in chip revisions beginning with pass2.
 *
 */
union cvmx_zip_error
{
	uint64_t u64;
	struct cvmx_zip_error_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t doorbell                     : 1;  /**< A doorbell count has overflowed */
#else
	uint64_t doorbell                     : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_zip_error_s               cn31xx;
	struct cvmx_zip_error_s               cn38xx;
	struct cvmx_zip_error_s               cn38xxp2;
	struct cvmx_zip_error_s               cn56xx;
	struct cvmx_zip_error_s               cn56xxp1;
	struct cvmx_zip_error_s               cn58xx;
	struct cvmx_zip_error_s               cn58xxp1;
	struct cvmx_zip_error_s               cn63xx;
	struct cvmx_zip_error_s               cn63xxp1;
};
typedef union cvmx_zip_error cvmx_zip_error_t;

/**
 * cvmx_zip_int_mask
 *
 * Notes:
 * Note that this CSR is present only in chip revisions beginning with pass2.
 * When a mask bit is set, the corresponding interrupt is enabled.
 */
union cvmx_zip_int_mask
{
	uint64_t u64;
	struct cvmx_zip_int_mask_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t doorbell                     : 1;  /**< Bit mask corresponding to ZIP_ERROR[0] above */
#else
	uint64_t doorbell                     : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_zip_int_mask_s            cn31xx;
	struct cvmx_zip_int_mask_s            cn38xx;
	struct cvmx_zip_int_mask_s            cn38xxp2;
	struct cvmx_zip_int_mask_s            cn56xx;
	struct cvmx_zip_int_mask_s            cn56xxp1;
	struct cvmx_zip_int_mask_s            cn58xx;
	struct cvmx_zip_int_mask_s            cn58xxp1;
	struct cvmx_zip_int_mask_s            cn63xx;
	struct cvmx_zip_int_mask_s            cn63xxp1;
};
typedef union cvmx_zip_int_mask cvmx_zip_int_mask_t;

/**
 * cvmx_zip_throttle
 *
 * Notes:
 * The maximum number of inflight data fetch transactions.  Values > 8 are illegal.
 * Writing 0 to this register causes the ZIP module to temporarily suspend NCB
 * accesses; it is not recommended for normal operation, but may be useful for
 * diagnostics.
 */
union cvmx_zip_throttle
{
	uint64_t u64;
	struct cvmx_zip_throttle_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_4_63                : 60;
	uint64_t max_infl                     : 4;  /**< Maximum number of inflight data fetch transactions on NCB */
#else
	uint64_t max_infl                     : 4;
	uint64_t reserved_4_63                : 60;
#endif
	} s;
	struct cvmx_zip_throttle_s            cn63xx;
	struct cvmx_zip_throttle_s            cn63xxp1;
};
typedef union cvmx_zip_throttle cvmx_zip_throttle_t;

#endif
