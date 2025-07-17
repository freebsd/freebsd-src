/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VMM_STAT_H_
#define	_VMM_STAT_H_

#include <dev/vmm/vmm_stat.h>

#include "vmm_util.h"

VMM_STAT_DECLARE(VCPU_MIGRATIONS);
VMM_STAT_DECLARE(VMEXIT_COUNT);
VMM_STAT_DECLARE(VMEXIT_EXTINT);
VMM_STAT_DECLARE(VMEXIT_HLT);
VMM_STAT_DECLARE(VMEXIT_CR_ACCESS);
VMM_STAT_DECLARE(VMEXIT_RDMSR);
VMM_STAT_DECLARE(VMEXIT_WRMSR);
VMM_STAT_DECLARE(VMEXIT_MTRAP);
VMM_STAT_DECLARE(VMEXIT_PAUSE);
VMM_STAT_DECLARE(VMEXIT_INTR_WINDOW);
VMM_STAT_DECLARE(VMEXIT_NMI_WINDOW);
VMM_STAT_DECLARE(VMEXIT_INOUT);
VMM_STAT_DECLARE(VMEXIT_CPUID);
VMM_STAT_DECLARE(VMEXIT_NESTED_FAULT);
VMM_STAT_DECLARE(VMEXIT_INST_EMUL);
VMM_STAT_DECLARE(VMEXIT_UNKNOWN);
VMM_STAT_DECLARE(VMEXIT_ASTPENDING);
VMM_STAT_DECLARE(VMEXIT_USERSPACE);
VMM_STAT_DECLARE(VMEXIT_RENDEZVOUS);
VMM_STAT_DECLARE(VMEXIT_EXCEPTION);
VMM_STAT_DECLARE(VMEXIT_REQIDLE);

#define	VMM_STAT_INTEL(type, desc)	\
	VMM_STAT_DEFINE(type, 1, desc, vmm_is_intel)
#define	VMM_STAT_AMD(type, desc)	\
	VMM_STAT_DEFINE(type, 1, desc, vmm_is_svm)

#endif
