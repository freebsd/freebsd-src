/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2007 Cisco Systems, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef IB_VERBS_H
#define IB_VERBS_H

#include <pthread.h>

#include <infiniband/driver.h>

#ifdef HAVE_VALGRIND_MEMCHECK_H

#  include <valgrind/memcheck.h>

#  ifndef VALGRIND_MAKE_MEM_DEFINED
#    warning "Valgrind support requested, but VALGRIND_MAKE_MEM_DEFINED not available"
#  endif

#endif /* HAVE_VALGRIND_MEMCHECK_H */

#ifndef VALGRIND_MAKE_MEM_DEFINED
#  define VALGRIND_MAKE_MEM_DEFINED(addr, len)
#endif

#define HIDDEN		__attribute__((visibility ("hidden")))

#define INIT		__attribute__((constructor))
#define FINI		__attribute__((destructor))

#define DEFAULT_ABI	"IBVERBS_1.1"

#ifdef HAVE_SYMVER_SUPPORT
#  define symver(name, api, ver) \
	asm(".symver " #name "," #api "@" #ver)
#  define default_symver(name, api) \
	asm(".symver " #name "," #api "@@" DEFAULT_ABI)
#else
#  define symver(name, api, ver)
#  define default_symver(name, api) \
	extern __typeof(name) api __attribute__((alias(#name)))
#endif /* HAVE_SYMVER_SUPPORT */

#define PFX		"libibverbs: "

struct ibv_abi_compat_v2 {
	struct ibv_comp_channel	channel;
	pthread_mutex_t		in_use;
};

extern HIDDEN int abi_ver;

HIDDEN int ibverbs_init(struct ibv_device ***list);

#define IBV_INIT_CMD(cmd, size, opcode)					\
	do {								\
		if (abi_ver > 2)					\
			(cmd)->command = IB_USER_VERBS_CMD_##opcode;	\
		else							\
			(cmd)->command = IB_USER_VERBS_CMD_##opcode##_V2; \
		(cmd)->in_words  = (size) / 4;				\
		(cmd)->out_words = 0;					\
	} while (0)

#define IBV_INIT_CMD_RESP(cmd, size, opcode, out, outsize)		\
	do {								\
		if (abi_ver > 2)					\
			(cmd)->command = IB_USER_VERBS_CMD_##opcode;	\
		else							\
			(cmd)->command = IB_USER_VERBS_CMD_##opcode##_V2; \
		(cmd)->in_words  = (size) / 4;				\
		(cmd)->out_words = (outsize) / 4;			\
		(cmd)->response  = (uintptr_t) (out);			\
	} while (0)

#endif /* IB_VERBS_H */
