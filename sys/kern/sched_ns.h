/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This software was developed by Olivier Certner <olce@FreeBSD.org>
 * at Kumacom SARL under sponsorship from the FreeBSD Foundation.
 */

/*
 * This file serves to generate a namespacing header for scheduler
 * implementations.
 *
 * During build, it is pre-processed with CPP and only the (expanded) lines
 * between the __START__ and __END__ lines below remain in the
 * implementation-specific namespacing header.
 *
 * Lines produced between __START__ and __END__ below look like:
 *
 * # define sched_clock sched_ule_clock __NL__
 * # define sched_switch sched_ule_switch __NL__
 *
 * and the build machinery substitutes __NL__ by newlines so the final result
 * can be included and its lines recognized as '#define' preprocessing
 * directives.
 *
 * A line between __START__ and __END__ starting with __CHECK_IMPLEMENTATION__
 * is treated specially and generates code that checks whether
 * SCHED_IMPLEMENTATION is the same as the value passed as the first argument
 * (which is that of SCHED_IMPLEMENTATION here).
 *
 * See files 'sys/conf/files' and 'sys/tools/sched_impl_ns.awk' for the
 * machinery.
 */

#define _KERNEL
#include <sys/cdefs.h>
#include <sys/sched.h>

#define _SCHED_IMPL_DEFINE(args, ret_type, fn, ...)			\
	_SCHED_ITF_ARG2 args define SCHED_ITF_FUNCTION_NAME(fn)		\
	SCHED_ITF_IMPL_FUNCTION_NAME(_SCHED_ITF_ARG1 args, fn)		\
	__NL__

#ifndef SCHED_IMPLEMENTATION
#error SCHED_IMPLEMENTATION must be defined (via sys/conf/files)
#endif

__START__
__CHECK_IMPLEMENTATION__ SCHED_IMPLEMENTATION
SCHED_ITF_NOCOMMON(_SCHED_IMPL_DEFINE, SCHED_IMPLEMENTATION, #)
__END__
