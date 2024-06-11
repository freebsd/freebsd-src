/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2017 Alexandru Elisei <alexandru.elisei@gmail.com>
 *
 * This software was developed by Alexandru Elisei under sponsorship
 * from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VMM_MMU_H_
#define	_VMM_MMU_H_

#include <machine/machdep.h>
#include <machine/vmparam.h>
#include <machine/vmm.h>

#include "hyp.h"

extern char vmm_hyp_code;
extern char vmm_hyp_code_end;

extern char _vmm_start;
extern char _vmm_end;

bool	vmmpmap_init(void);
void	vmmpmap_fini(void);
uint64_t vmmpmap_to_ttbr0(void);
bool	vmmpmap_enter(vm_offset_t, vm_size_t, vm_paddr_t, vm_prot_t);
void	vmmpmap_remove(vm_offset_t, vm_size_t, bool);

#endif
