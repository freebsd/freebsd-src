/*-
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	from: @(#)genassym.c	5.11 (Berkeley) 5/10/91
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/assym.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/ucontext.h>
#include <machine/frame.h>
#include <machine/mutex.h>
#include <machine/elf.h>
#include <machine/pal.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <net/if.h>
#include <netinet/in.h>

ASSYM(DT_NULL,		DT_NULL);
ASSYM(DT_RELA,		DT_RELA);
ASSYM(DT_RELAENT,	DT_RELAENT);
ASSYM(DT_RELASZ,	DT_RELASZ);
ASSYM(DT_SYMTAB,	DT_SYMTAB);
ASSYM(DT_SYMENT,	DT_SYMENT);

ASSYM(EFAULT,		EFAULT);
ASSYM(ENAMETOOLONG,	ENAMETOOLONG);
ASSYM(ERESTART,		ERESTART);

ASSYM(FRAME_SYSCALL,	FRAME_SYSCALL);

ASSYM(KSTACK_PAGES,	KSTACK_PAGES);

ASSYM(MC_PRESERVED,	offsetof(mcontext_t, mc_preserved));
ASSYM(MC_PRESERVED_FP,	offsetof(mcontext_t, mc_preserved_fp));
ASSYM(MC_SPECIAL,	offsetof(mcontext_t, mc_special));
ASSYM(MC_SPECIAL_BSPSTORE, offsetof(mcontext_t, mc_special.bspstore));
ASSYM(MC_SPECIAL_RNAT,	offsetof(mcontext_t, mc_special.rnat));

ASSYM(PAGE_SHIFT,	PAGE_SHIFT);
ASSYM(PAGE_SIZE,	PAGE_SIZE);

ASSYM(PC_CPUID,		offsetof(struct pcpu, pc_cpuid));
ASSYM(PC_CURRENT_PMAP,	offsetof(struct pcpu, pc_current_pmap));
ASSYM(PC_CURTHREAD,	offsetof(struct pcpu, pc_curthread));
ASSYM(PC_IDLETHREAD,	offsetof(struct pcpu, pc_idlethread));

ASSYM(PCB_CURRENT_PMAP,	offsetof(struct pcb, pcb_current_pmap));
ASSYM(PCB_ONFAULT,	offsetof(struct pcb, pcb_onfault));
ASSYM(PCB_SPECIAL_RP,	offsetof(struct pcb, pcb_special.rp));

ASSYM(R_IA64_DIR64LSB,	R_IA64_DIR64LSB);
ASSYM(R_IA64_FPTR64LSB,	R_IA64_FPTR64LSB);
ASSYM(R_IA64_NONE,	R_IA64_NONE);
ASSYM(R_IA64_REL64LSB,	R_IA64_REL64LSB);

ASSYM(SIZEOF_PCB,	sizeof(struct pcb));
ASSYM(SIZEOF_SPECIAL,	sizeof(struct _special));
ASSYM(SIZEOF_TRAPFRAME,	sizeof(struct trapframe));

ASSYM(TD_FLAGS,		offsetof(struct thread, td_flags));
ASSYM(TD_KSTACK,	offsetof(struct thread, td_kstack));
ASSYM(TD_PCB,		offsetof(struct thread, td_pcb));

ASSYM(TDF_ASTPENDING,	TDF_ASTPENDING);
ASSYM(TDF_NEEDRESCHED,	TDF_NEEDRESCHED);

ASSYM(TF_SPECIAL_IIP, offsetof(struct trapframe, tf_special.iip));

ASSYM(UC_MCONTEXT,	offsetof(ucontext_t, uc_mcontext));

ASSYM(VM_MAX_ADDRESS,	VM_MAX_ADDRESS);
