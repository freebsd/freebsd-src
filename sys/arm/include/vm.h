/*-
 * Copyright (c) 2009 Alan L. Cox <alc@cs.rice.edu>
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

#ifndef _MACHINE_VM_H_
#define	_MACHINE_VM_H_

#ifdef ARM_NEW_PMAP
#include <machine/pte-v6.h>

#define VM_MEMATTR_WB_WA		((vm_memattr_t)PTE2_ATTR_WB_WA)
#define VM_MEMATTR_NOCACHE		((vm_memattr_t)PTE2_ATTR_NOCACHE)
#define VM_MEMATTR_DEVICE		((vm_memattr_t)PTE2_ATTR_DEVICE)
#define VM_MEMATTR_SO			((vm_memattr_t)PTE2_ATTR_SO)
#define VM_MEMATTR_WT			((vm_memattr_t)PTE2_ATTR_WT)

#define VM_MEMATTR_DEFAULT		VM_MEMATTR_WB_WA
#define VM_MEMATTR_UNCACHEABLE		VM_MEMATTR_SO 	/* misused by DMA */
#define VM_MEMATTR_WRITE_COMBINING 	VM_MEMATTR_WT		/* for DRM */
#define VM_MEMATTR_WRITE_BACK		VM_MEMATTR_WB_WA	/* for DRM */

#else
/* Memory attribute configuration. */
#define	VM_MEMATTR_DEFAULT	0
#define	VM_MEMATTR_UNCACHEABLE	1
#endif

#endif /* !_MACHINE_VM_H_ */
