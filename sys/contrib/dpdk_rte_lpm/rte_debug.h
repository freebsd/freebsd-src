/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_DEBUG_H_
#define _RTE_DEBUG_H_

/**
 * @file
 *
 * Debug Functions in RTE
 *
 * This file defines a generic API for debug operations. Part of
 * the implementation is architecture-specific.
 */

//#include "rte_log.h"
#include "rte_branch_prediction.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dump the stack of the calling core to the console.
 */
void rte_dump_stack(void);

/**
 * Dump the registers of the calling core to the console.
 *
 * Note: Not implemented in a userapp environment; use gdb instead.
 */
void rte_dump_registers(void);

/**
 * Provide notification of a critical non-recoverable error and terminate
 * execution abnormally.
 *
 * Display the format string and its expanded arguments (printf-like).
 *
 * In a linux environment, this function dumps the stack and calls
 * abort() resulting in a core dump if enabled.
 *
 * The function never returns.
 *
 * @param ...
 *   The format string, followed by the variable list of arguments.
 */
#define rte_panic(...) rte_panic_(__func__, __VA_ARGS__, "dummy")
#define rte_panic_(func, format, ...) __rte_panic(func, format "%.0s", __VA_ARGS__)

#ifdef RTE_ENABLE_ASSERT
#define RTE_ASSERT(exp)	RTE_VERIFY(exp)
#else
#define RTE_ASSERT(exp) do {} while (0)
#endif
#define	RTE_VERIFY(exp)	do {                                                  \
	if (unlikely(!(exp)))                                                           \
		rte_panic("line %d\tassert \"%s\" failed\n", __LINE__, #exp); \
} while (0)

/*
 * Provide notification of a critical non-recoverable error and stop.
 *
 * This function should not be called directly. Refer to rte_panic() macro
 * documentation.
 */
void __rte_panic(const char *funcname , const char *format, ...)
{
#ifdef __GNUC__
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 2))
	__rte_cold
#endif
#endif
	//__rte_noreturn
	//__rte_format_printf(2, 3);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_DEBUG_H_ */
