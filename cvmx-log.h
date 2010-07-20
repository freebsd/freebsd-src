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





#ifndef __CVMX_LOG_H__
#define __CVMX_LOG_H__

/**
 * @file
 *
 * cvmx-log supplies a fast log buffer implementation. Each core writes
 * log data to a differnet buffer to avoid synchronization overhead. Function
 * call logging can be turned on with the GCC option "-pg".
 *
 * <hr>$Revision: 41586 $<hr>
 */

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * Enumeration of all supported performance counter types
 */
typedef enum
{
    CVMX_LOG_PERF_CNT_NONE      = 0,    /**< Turn off the performance counter */
    CVMX_LOG_PERF_CNT_CLK       = 1,    /**< Conditionally clocked cycles (as opposed to count/cvm_count which count even with no clocks) */
    CVMX_LOG_PERF_CNT_ISSUE     = 2,    /**< Instructions issued but not retired */
    CVMX_LOG_PERF_CNT_RET       = 3,    /**< Instructions retired */
    CVMX_LOG_PERF_CNT_NISSUE    = 4,    /**< Cycles no issue */
    CVMX_LOG_PERF_CNT_SISSUE    = 5,    /**< Cycles single issue */
    CVMX_LOG_PERF_CNT_DISSUE    = 6,    /**< Cycles dual issue */
    CVMX_LOG_PERF_CNT_IFI       = 7,    /**< Cycle ifetch issued (but not necessarily commit to pp_mem) */
    CVMX_LOG_PERF_CNT_BR        = 8,    /**< Branches retired */
    CVMX_LOG_PERF_CNT_BRMIS     = 9,    /**< Branch mispredicts */
    CVMX_LOG_PERF_CNT_J         = 10,   /**< Jumps retired */
    CVMX_LOG_PERF_CNT_JMIS      = 11,   /**< Jumps mispredicted */
    CVMX_LOG_PERF_CNT_REPLAY    = 12,   /**< Mem Replays */
    CVMX_LOG_PERF_CNT_IUNA      = 13,   /**< Cycles idle due to unaligned_replays */
    CVMX_LOG_PERF_CNT_TRAP      = 14,   /**< trap_6a signal */
    CVMX_LOG_PERF_CNT_UULOAD    = 16,   /**< Unexpected unaligned loads (REPUN=1) */
    CVMX_LOG_PERF_CNT_UUSTORE   = 17,   /**< Unexpected unaligned store (REPUN=1) */
    CVMX_LOG_PERF_CNT_ULOAD     = 18,   /**< Unaligned loads (REPUN=1 or USEUN=1) */
    CVMX_LOG_PERF_CNT_USTORE    = 19,   /**< Unaligned store (REPUN=1 or USEUN=1) */
    CVMX_LOG_PERF_CNT_EC        = 20,   /**< Exec clocks(must set CvmCtl[DISCE] for accurate timing) */
    CVMX_LOG_PERF_CNT_MC        = 21,   /**< Mul clocks(must set CvmCtl[DISCE] for accurate timing) */
    CVMX_LOG_PERF_CNT_CC        = 22,   /**< Crypto clocks(must set CvmCtl[DISCE] for accurate timing) */
    CVMX_LOG_PERF_CNT_CSRC      = 23,   /**< Issue_csr clocks(must set CvmCtl[DISCE] for accurate timing) */
    CVMX_LOG_PERF_CNT_CFETCH    = 24,   /**< Icache committed fetches (demand+prefetch) */
    CVMX_LOG_PERF_CNT_CPREF     = 25,   /**< Icache committed prefetches */
    CVMX_LOG_PERF_CNT_ICA       = 26,   /**< Icache aliases */
    CVMX_LOG_PERF_CNT_II        = 27,   /**< Icache invalidates */
    CVMX_LOG_PERF_CNT_IP        = 28,   /**< Icache parity error */
    CVMX_LOG_PERF_CNT_CIMISS    = 29,   /**< Cycles idle due to imiss (must set CvmCtl[DISCE] for accurate timing) */
    CVMX_LOG_PERF_CNT_WBUF      = 32,   /**< Number of write buffer entries created */
    CVMX_LOG_PERF_CNT_WDAT      = 33,   /**< Number of write buffer data cycles used (may need to set CvmCtl[DISCE] for accurate counts) */
    CVMX_LOG_PERF_CNT_WBUFLD    = 34,   /**< Number of write buffer entries forced out by loads */
    CVMX_LOG_PERF_CNT_WBUFFL    = 35,   /**< Number of cycles that there was no available write buffer entry (may need to set CvmCtl[DISCE] and CvmMemCtl[MCLK] for accurate counts) */
    CVMX_LOG_PERF_CNT_WBUFTR    = 36,   /**< Number of stores that found no available write buffer entries */
    CVMX_LOG_PERF_CNT_BADD      = 37,   /**< Number of address bus cycles used (may need to set CvmCtl[DISCE] for accurate counts) */
    CVMX_LOG_PERF_CNT_BADDL2    = 38,   /**< Number of address bus cycles not reflected (i.e. destined for L2) (may need to set CvmCtl[DISCE] for accurate counts) */
    CVMX_LOG_PERF_CNT_BFILL     = 39,   /**< Number of fill bus cycles used (may need to set CvmCtl[DISCE] for accurate counts) */
    CVMX_LOG_PERF_CNT_DDIDS     = 40,   /**< Number of Dstream DIDs created */
    CVMX_LOG_PERF_CNT_IDIDS     = 41,   /**< Number of Istream DIDs created */
    CVMX_LOG_PERF_CNT_DIDNA     = 42,   /**< Number of cycles that no DIDs were available (may need to set CvmCtl[DISCE] and CvmMemCtl[MCLK] for accurate counts) */
    CVMX_LOG_PERF_CNT_LDS       = 43,   /**< Number of load issues */
    CVMX_LOG_PERF_CNT_LMLDS     = 44,   /**< Number of local memory load */
    CVMX_LOG_PERF_CNT_IOLDS     = 45,   /**< Number of I/O load issues */
    CVMX_LOG_PERF_CNT_DMLDS     = 46,   /**< Number of loads that were not prefetches and missed in the cache */
    CVMX_LOG_PERF_CNT_STS       = 48,   /**< Number of store issues */
    CVMX_LOG_PERF_CNT_LMSTS     = 49,   /**< Number of local memory store issues */
    CVMX_LOG_PERF_CNT_IOSTS     = 50,   /**< Number of I/O store issues */
    CVMX_LOG_PERF_CNT_IOBDMA    = 51,   /**< Number of IOBDMAs */
    CVMX_LOG_PERF_CNT_DTLB      = 53,   /**< Number of dstream TLB refill, invalid, or modified exceptions */
    CVMX_LOG_PERF_CNT_DTLBAD    = 54,   /**< Number of dstream TLB address errors */
    CVMX_LOG_PERF_CNT_ITLB      = 55,   /**< Number of istream TLB refill, invalid, or address error exceptions */
    CVMX_LOG_PERF_CNT_SYNC      = 56,   /**< Number of SYNC stall cycles (may need to set CvmCtl[DISCE] for accurate counts) */
    CVMX_LOG_PERF_CNT_SYNCIOB   = 57,   /**< Number of SYNCIOBDMA stall cycles (may need to set CvmCtl[DISCE] for accurate counts) */
    CVMX_LOG_PERF_CNT_SYNCW     = 58,   /**< Number of SYNCWs */
} cvmx_log_perf_event_t;

/**
 * Structure of the performance counter control register
 */
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t                M       : 1;
        uint32_t                W       : 1;
        uint32_t                reserved: 19;
        cvmx_log_perf_event_t   event   : 6;
        uint32_t                IE      : 1;
        uint32_t                U       : 1;
        uint32_t                S       : 1;
        uint32_t                K       : 1;
        uint32_t                EX      : 1;
    } s;
} cvmx_log_perf_control_t;

/*
 * Add CVMX_LOG_DISABLE_PC_LOGGING as an attribute to and function prototype
 * that you don't want logged when the gcc option "-pg" is supplied. We
 * use it on the cvmx-log functions since it is pointless to log the
 * calling of a function than in itself writes to the log.
 */
#define CVMX_LOG_DISABLE_PC_LOGGING __attribute__((no_instrument_function))

/**
 * Log a constant printf style format string with 0 to 4
 * arguments. The string must persist until the log is read,
 * but the parameters are copied into the log.
 *
 * @param format  Constant printf style format string.
 * @param numberx 64bit argument to the printf format string
 */
void cvmx_log_printf0(const char *format) CVMX_LOG_DISABLE_PC_LOGGING;
void cvmx_log_printf1(const char *format, uint64_t number1) CVMX_LOG_DISABLE_PC_LOGGING;
void cvmx_log_printf2(const char *format, uint64_t number1, uint64_t number2) CVMX_LOG_DISABLE_PC_LOGGING;
void cvmx_log_printf3(const char *format, uint64_t number1, uint64_t number2, uint64_t number3) CVMX_LOG_DISABLE_PC_LOGGING;
void cvmx_log_printf4(const char *format, uint64_t number1, uint64_t number2, uint64_t number3, uint64_t number4) CVMX_LOG_DISABLE_PC_LOGGING;

/**
 * Log an arbitrary block of 64bit words. At most 255 64bit
 * words can be logged. The words are copied into the log.
 *
 * @param size_in_dwords
 *               Number of 64bit dwords to copy into the log.
 * @param data   Array of 64bit dwords to copy
 */
void cvmx_log_data(uint64_t size_in_dwords, const uint64_t *data) CVMX_LOG_DISABLE_PC_LOGGING;

/**
 * Log a structured data object. Post processing will use the
 * debugging information in the ELF file to determine how to
 * display the structure. Max of 2032 bytes.
 *
 * Example:
 * cvmx_log_structure("cvmx_wqe_t", work, sizeof(*work));
 *
 * @param type   C typedef expressed as a string. This will be used to
 *               lookup the structure in the debugging infirmation.
 * @param data   Data to be written to the log.
 * @param size_in_bytes
 *               Size if the data in bytes. Normally you'll use the
 *               sizeof() operator here.
 */
void cvmx_log_structure(const char *type, void *data, int size_in_bytes) CVMX_LOG_DISABLE_PC_LOGGING;

/**
 * Setup the mips performance counters
 *
 * @param counter1 Event type for counter 1
 * @param counter2 Event type for counter 2
 */
void cvmx_log_perf_setup(cvmx_log_perf_event_t counter1, cvmx_log_perf_event_t counter2);

/**
 * Log the performance counters
 */
void cvmx_log_perf(void) CVMX_LOG_DISABLE_PC_LOGGING;

/**
 * Display the current log in a human readable format.
 */
void cvmx_log_display(void);

#ifdef	__cplusplus
}
#endif

#endif
