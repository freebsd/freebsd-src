/*-
 * Copyright (c) 2004, 2005 Kip Macy
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

#ifndef _XEN_XENFUNC_H_
#define _XEN_XENFUNC_H_

#include <xen/xen-os.h>
#include <xen/hypervisor.h>

#include <vm/pmap.h>

#include <machine/xen/xenpmap.h>
#include <machine/segments.h>

#include <sys/pcpu.h>
#define BKPT __asm__("int3");
#define XPQ_CALL_DEPTH 5
#define XPQ_CALL_COUNT 2
#define PG_PRIV PG_AVAIL3
typedef struct { 
	unsigned long pt_ref;
	unsigned long pt_eip[XPQ_CALL_COUNT][XPQ_CALL_DEPTH];
} pteinfo_t;

extern pteinfo_t *pteinfo_list;
#ifdef XENDEBUG_LOW
#define	__PRINTK(x) printk x
#else
#define	__PRINTK(x)
#endif

char *xen_setbootenv(char *cmd_line);

int  xen_boothowto(char *envp);

void _xen_machphys_update(vm_paddr_t, vm_paddr_t, char *file, int line);

#ifdef INVARIANTS
#define xen_machphys_update(a, b) _xen_machphys_update((a), (b), __FILE__, __LINE__)
#else
#define xen_machphys_update(a, b) _xen_machphys_update((a), (b), NULL, 0)
#endif	

void xen_update_descriptor(union descriptor *, union descriptor *);

extern struct mtx balloon_lock;
#if 0
#define balloon_lock(__flags)   mtx_lock_irqsave(&balloon_lock, __flags)
#define balloon_unlock(__flags) mtx_unlock_irqrestore(&balloon_lock, __flags)
#else
#define balloon_lock(__flags)   __flags = 1
#define balloon_unlock(__flags) __flags = 0
#endif



#endif /* _XEN_XENFUNC_H_ */
