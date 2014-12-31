/*-
 * Copyright (c) 2014 Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _X86_HYPERVISOR_H_
#define _X86_HYPERVISOR_H_

#include <sys/param.h>
#include <sys/kernel.h>

typedef void hypervisor_init_func_t(void);
typedef void hypervisor_op_cpu_stop_t(int);

/*
 * The guest hypervisor support may provide paravirtualized or have special
 * requirements for various operations. The callback functions are provided
 * when a hypervisor is detected and registered.
 */
struct hypervisor_ops {
	hypervisor_op_cpu_stop_t	*hvo_cpu_stop;
};

void	hypervisor_sysinit(void *func);
void	hypervisor_register(const char *vendor, enum VM_GUEST guest,
	    struct hypervisor_ops *ops);
int	hypervisor_cpuid_base(const char *signature, int leaves,
	    uint32_t *base, uint32_t *high);
void	hypervisor_print_info(void);

#define HYPERVISOR_SYSINIT(name, func)				\
	SYSINIT(name ## _hypervisor_sysinit, SI_SUB_HYPERVISOR,	\
	    SI_ORDER_FIRST, hypervisor_sysinit, func)

#endif /* !_X86_HYPERVISOR_H_ */
