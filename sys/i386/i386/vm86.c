/*-
 * Copyright (c) 1997 Jonathan Lemon
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
 *	$Id: vm86.c,v 1.8 1998/02/06 12:13:11 eivind Exp $
 */

#include "opt_vm86.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/lock.h>

#include <vm/vm.h>
#include <vm/vm_prot.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>

#include <sys/user.h>

#include <machine/md_var.h>
#include <machine/pcb_ext.h>	/* pcb.h included via sys/user.h */
#include <machine/psl.h>
#include <machine/specialreg.h>

extern int i386_extend_pcb	__P((struct proc *));
extern struct segment_descriptor common_tssd;
extern int vm86paddr, vm86pa;
extern struct pcb *vm86pcb;

extern int vm86_bioscall(struct vm86frame *);
extern void vm86_biosret(struct vm86frame *);

void vm86_prepcall(struct vm86frame);
 
#define	HLT	0xf4
#define	CLI	0xfa
#define	STI	0xfb
#define	PUSHF	0x9c
#define	POPF	0x9d
#define	INTn	0xcd
#define	IRET	0xcf
#define	CALLm	0xff
#define OPERAND_SIZE_PREFIX	0x66
#define ADDRESS_SIZE_PREFIX	0x67
#define PUSH_MASK	~(PSL_VM | PSL_RF | PSL_I)
#define POP_MASK	~(PSL_VIP | PSL_VIF | PSL_VM | PSL_RF | PSL_IOPL)

static inline caddr_t
MAKE_ADDR(u_short sel, u_short off)
{
	return ((caddr_t)((sel << 4) + off));
}

static inline void
GET_VEC(u_long vec, u_short *sel, u_short *off)
{
	*sel = vec >> 16;
	*off = vec & 0xffff;
}

static inline u_long
MAKE_VEC(u_short sel, u_short off)
{
	return ((sel << 16) | off);
}

static inline void
PUSH(u_short x, struct vm86frame *vmf)
{
	vmf->vmf_sp -= 2;
	susword(MAKE_ADDR(vmf->vmf_ss, vmf->vmf_sp), x);
}

static inline void
PUSHL(u_long x, struct vm86frame *vmf)
{
	vmf->vmf_sp -= 4;
	suword(MAKE_ADDR(vmf->vmf_ss, vmf->vmf_sp), x);
}

static inline u_short
POP(struct vm86frame *vmf)
{
	u_short x = fusword(MAKE_ADDR(vmf->vmf_ss, vmf->vmf_sp));

	vmf->vmf_sp += 2;
	return (x);
}

static inline u_long
POPL(struct vm86frame *vmf)
{
	u_long x = fuword(MAKE_ADDR(vmf->vmf_ss, vmf->vmf_sp));

	vmf->vmf_sp += 4;
	return (x);
}

int
vm86_emulate(vmf)
	struct vm86frame *vmf;
{
	struct vm86_kernel *vm86;
	caddr_t addr;
	u_char i_byte;
	u_long temp_flags;
	int inc_ip = 1;
	int retcode = 0;

	/*
	 * pcb_ext contains the address of the extension area, or zero if
	 * the extension is not present.  (This check should not be needed,
	 * as we can't enter vm86 mode until we set up an extension area)
	 */
	if (curpcb->pcb_ext == 0)
		return (SIGBUS);
	vm86 = &curpcb->pcb_ext->ext_vm86;

	if (vmf->vmf_eflags & PSL_T)
		retcode = SIGTRAP;

	addr = MAKE_ADDR(vmf->vmf_cs, vmf->vmf_ip);
	i_byte = fubyte(addr);
	if (i_byte == ADDRESS_SIZE_PREFIX) {
		i_byte = fubyte(++addr);
		inc_ip++;
	}

	if (vm86->vm86_has_vme) {
		switch (i_byte) {
		case OPERAND_SIZE_PREFIX:
			i_byte = fubyte(++addr);
			inc_ip++;
			switch (i_byte) {
			case PUSHF:
				if (vmf->vmf_eflags & PSL_VIF)
					PUSHL((vmf->vmf_eflags & PUSH_MASK)
					    | PSL_IOPL | PSL_I, vmf);
				else
					PUSHL((vmf->vmf_eflags & PUSH_MASK)
					    | PSL_IOPL, vmf);
				vmf->vmf_ip += inc_ip;
				return (0);

			case POPF:
				temp_flags = POPL(vmf) & POP_MASK;
				vmf->vmf_eflags = (vmf->vmf_eflags & ~POP_MASK)
				    | temp_flags | PSL_VM | PSL_I;
				vmf->vmf_ip += inc_ip;
				if (temp_flags & PSL_I) {
					vmf->vmf_eflags |= PSL_VIF;
					if (vmf->vmf_eflags & PSL_VIP)
						break;
				} else {
					vmf->vmf_eflags &= ~PSL_VIF;
				}
				return (0);
			}
			break;

		/* VME faults here if VIP is set, but does not set VIF. */
		case STI:
			vmf->vmf_eflags |= PSL_VIF;
			vmf->vmf_ip += inc_ip;
			if ((vmf->vmf_eflags & PSL_VIP) == 0) {
				uprintf("fatal sti\n");
				return (SIGKILL);
			}
			break;

		/* VME if no redirection support */
		case INTn:
			break;

		/* VME if trying to set PSL_TF, or PSL_I when VIP is set */
		case POPF:
			temp_flags = POP(vmf) & POP_MASK;
			vmf->vmf_flags = (vmf->vmf_flags & ~POP_MASK)
			    | temp_flags | PSL_VM | PSL_I;
			vmf->vmf_ip += inc_ip;
			if (temp_flags & PSL_I) {
				vmf->vmf_eflags |= PSL_VIF;
				if (vmf->vmf_eflags & PSL_VIP)
					break;
			} else {
				vmf->vmf_eflags &= ~PSL_VIF;
			}
			return (retcode);

		/* VME if trying to set PSL_TF, or PSL_I when VIP is set */
		case IRET:
			vmf->vmf_ip = POP(vmf);
			vmf->vmf_cs = POP(vmf);
			temp_flags = POP(vmf) & POP_MASK;
			vmf->vmf_flags = (vmf->vmf_flags & ~POP_MASK)
			    | temp_flags | PSL_VM | PSL_I;
			if (temp_flags & PSL_I) {
				vmf->vmf_eflags |= PSL_VIF;
				if (vmf->vmf_eflags & PSL_VIP)
					break;
			} else {
				vmf->vmf_eflags &= ~PSL_VIF;
			}
			return (retcode);

		}
		return (SIGBUS);
	}

	switch (i_byte) {
	case OPERAND_SIZE_PREFIX:
		i_byte = fubyte(++addr);
		inc_ip++;
		switch (i_byte) {
		case PUSHF:
			if (vm86->vm86_eflags & PSL_VIF)
				PUSHL((vmf->vmf_flags & PUSH_MASK)
				    | PSL_IOPL | PSL_I, vmf);
			else
				PUSHL((vmf->vmf_flags & PUSH_MASK)
				    | PSL_IOPL, vmf);
			vmf->vmf_ip += inc_ip;
			return (retcode);

		case POPF:
			temp_flags = POPL(vmf) & POP_MASK;
			vmf->vmf_eflags = (vmf->vmf_eflags & ~POP_MASK)
			    | temp_flags | PSL_VM | PSL_I;
			vmf->vmf_ip += inc_ip;
			if (temp_flags & PSL_I) {
				vm86->vm86_eflags |= PSL_VIF;
				if (vm86->vm86_eflags & PSL_VIP)
					break;
			} else {
				vm86->vm86_eflags &= ~PSL_VIF;
			}
			return (retcode);
		}
		return (SIGBUS);

	case CLI:
		vm86->vm86_eflags &= ~PSL_VIF;
		vmf->vmf_ip += inc_ip;
		return (retcode);

	case STI:
		/* if there is a pending interrupt, go to the emulator */
		vm86->vm86_eflags |= PSL_VIF;
		vmf->vmf_ip += inc_ip;
		if (vm86->vm86_eflags & PSL_VIP)
			break;
		return (retcode);

	case PUSHF:
		if (vm86->vm86_eflags & PSL_VIF)
			PUSH((vmf->vmf_flags & PUSH_MASK)
			    | PSL_IOPL | PSL_I, vmf);
		else
			PUSH((vmf->vmf_flags & PUSH_MASK) | PSL_IOPL, vmf);
		vmf->vmf_ip += inc_ip;
		return (retcode);

	case INTn:
		i_byte = fubyte(addr + 1);
		if ((vm86->vm86_intmap[i_byte >> 3] & (1 << (i_byte & 7))) != 0)
			break;
		if (vm86->vm86_eflags & PSL_VIF)
			PUSH((vmf->vmf_flags & PUSH_MASK)
			    | PSL_IOPL | PSL_I, vmf);
		else
			PUSH((vmf->vmf_flags & PUSH_MASK) | PSL_IOPL, vmf);
		PUSH(vmf->vmf_cs, vmf);
		PUSH(vmf->vmf_ip + inc_ip + 1, vmf);	/* increment IP */
		GET_VEC(fuword((caddr_t)(i_byte * 4)),
		     &vmf->vmf_cs, &vmf->vmf_ip);
		vmf->vmf_flags &= ~PSL_T;
		vm86->vm86_eflags &= ~PSL_VIF;
		return (retcode);

	case IRET:
		vmf->vmf_ip = POP(vmf);
		vmf->vmf_cs = POP(vmf);
		temp_flags = POP(vmf) & POP_MASK;
		vmf->vmf_flags = (vmf->vmf_flags & ~POP_MASK)
		    | temp_flags | PSL_VM | PSL_I;
		if (temp_flags & PSL_I) {
			vm86->vm86_eflags |= PSL_VIF;
			if (vm86->vm86_eflags & PSL_VIP)
				break;
		} else {
			vm86->vm86_eflags &= ~PSL_VIF;
		}
		return (retcode);

	case POPF:
		temp_flags = POP(vmf) & POP_MASK;
		vmf->vmf_flags = (vmf->vmf_flags & ~POP_MASK)
		    | temp_flags | PSL_VM | PSL_I;
		vmf->vmf_ip += inc_ip;
		if (temp_flags & PSL_I) {
			vm86->vm86_eflags |= PSL_VIF;
			if (vm86->vm86_eflags & PSL_VIP)
				break;
		} else {
			vm86->vm86_eflags &= ~PSL_VIF;
		}
		return (retcode);
	}
	return (SIGBUS);
}

static void
vm86_initialize(void)
{
	int i, offset;
	u_long *addr;
	struct pcb *pcb;
	struct pcb_ext *ext;
	struct segment_descriptor sd;
	struct soft_segment_descriptor ssd = {
		0,			/* segment base address (overwritten) */
		ctob(IOPAGES + 1) - 1,	/* length */
		SDT_SYS386TSS,		/* segment type */
		0,			/* priority level */
		1,			/* descriptor present */
		0, 0,
		0,			/* default 32 size */
		0			/* granularity */
	};

	/*
	 * Below is the memory layout that we use for the vm86 region.
	 *
	 * The last byte of the i/o map must be followed by an 0xff byte.
	 * We arbitrarily allocate 16 bytes here, to keep the starting
	 * address on a doubleword boundary.
	 *
	 * If a ~2K stack is enough for interrupt handling, then 
	 * it may be possible to get the page count down to 3 pages.
	 *
	 * +--------+ +--------+
	 * |        | |Page Tbl| 1M + 64K = 272 entries = 1088 bytes
	 * |        | +--------+
	 * | page 0 | 
	 * |        | +--------+
	 * |        | | stack  |
	 * +--------+ +--------+
	 * +--------+ +--------+
	 * |        | |  PCB   | size: ~240 bytes
	 * |        | |PCB Ext | size: ~140 bytes (includes TSS)
	 * |        | +--------+
	 * | page 1 | 
	 * |        | +--------+
	 * |        | |int map |
	 * |        | +--------+ <-- &(PAGE 1) - 16
	 * +--------+ |        |
	 * | page 2 | |  I/O   |
	 * +--------+ | bitmap |
	 * | page 3 | |        |
	 * +--------+ +--------+
	 */

	/*
	 * A rudimentary PCB must be installed, in order to get to the
	 * PCB extension area.  We use the PCB area as a scratchpad for
	 * data storage, the layout of which is shown below.
	 *
	 * pcb_esi	= new PTD entry 0
	 * pcb_ebp	= pointer to frame on vm86 stack
	 * pcb_esp	=    stack frame pointer at time of switch
	 * pcb_ebx	= va of vm86 page table
	 * pcb_eip	=    argument pointer to initial call
	 * pcb_fs	=    saved TSS descriptor, word 0
	 * pcb_gs	=    saved TSS descriptor, word 1
	 */

	pcb = (struct pcb *)(vm86paddr + PAGE_SIZE);
	bzero(pcb, sizeof(struct pcb)); 
	pcb->pcb_esi = vm86pa | PG_V | PG_RW | PG_U;
	pcb->pcb_ebp = vm86paddr + PAGE_SIZE - sizeof(struct vm86frame);
	pcb->pcb_ebx = vm86paddr;

	ext = (struct pcb_ext *)((u_int)pcb + sizeof(struct pcb));
	pcb->pcb_ext = ext;

	bzero(ext, sizeof(struct pcb_ext)); 
	ext->ext_tss.tss_esp0 = vm86paddr + PAGE_SIZE;
	ext->ext_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);

	offset = PAGE_SIZE - 16;
	ext->ext_tss.tss_ioopt = 
	    (offset - ((u_int)&ext->ext_tss & PAGE_MASK)) << 16;
	ext->ext_iomap = (caddr_t)(offset + ((u_int)&ext->ext_tss & PG_FRAME));

	ext->ext_vm86.vm86_intmap = ext->ext_iomap - 32;
	ext->ext_vm86.vm86_has_vme = (rcr4() & CR4_VME ? 1 : 0);

	addr = (u_long *)ext->ext_vm86.vm86_intmap;
	for (i = 0; i < (ctob(IOPAGES) + 32 + 16) / sizeof(u_long); i++)
		*addr++ = 0;

	ssd.ssd_base = (u_int)&ext->ext_tss;
	ssd.ssd_limit -= ((u_int)&ext->ext_tss & PAGE_MASK);
	ssdtosd(&ssd, &ext->ext_tssd);

	vm86pcb = pcb;
}

void
initial_bioscalls(u_int *basemem, u_int *extmem)
{
	int i, method;
	struct vm86frame vmf;
	u_int64_t highwat = 0;
	struct {
		u_int64_t base;
		u_int64_t length;
		u_int32_t type;
	} smap;

	bzero(&vmf, sizeof(struct vm86frame));		/* safety */
	vm86_initialize();

	vm86_intcall(0x12, &vmf);
	*basemem = vmf.vmf_ax;
	*extmem = 0;

	/*
	 * if basemem != 640, map pages r/w into vm86 page table so 
	 * that the bios can scribble on it.
	 */
	for (i = *basemem / 4; i < 160; i++) {
		u_int *pte = (u_int *)vm86paddr;
		pte[i] = (i << PAGE_SHIFT) | PG_V | PG_RW | PG_U;
	}

	/*
	 * get memory map with INT 15:E820
	 */
#define SMAPSIZ 	sizeof(smap)
#define SMAP_SIG	0x534D4150			/* 'SMAP' */
	vmf.vmf_ebx = 0;
	do {
		vmf.vmf_eax = 0xE820;
		vmf.vmf_edx = SMAP_SIG;
		vmf.vmf_ecx = SMAPSIZ;
		i = vm86_datacall(0x15, &vmf,
		    (char *)&smap, SMAPSIZ, &vmf.vmf_es, &vmf.vmf_di);
		if (i || vmf.vmf_eax != SMAP_SIG)
			break;
		if (smap.type == 0x01 && smap.base >= highwat) {
			*extmem += (smap.length / 1024);
			highwat = smap.base + smap.length;
		}
	} while (vmf.vmf_ebx != 0);

	if (*extmem != 0) {
		if (*extmem > *basemem) {
			*extmem -= *basemem;
			method = 0xE820;
			goto done;
		}
		printf("E820: extmem (%d) < basemem (%d)\n", *extmem, *basemem);
	}

	/*
	 * try memory map with INT 15:E801
	 */
	vmf.vmf_ax = 0xE801;
	if (vm86_intcall(0x15, &vmf) == 0) {
		*extmem = vmf.vmf_cx + vmf.vmf_dx * 64;
		method = 0xE801;
		goto done;
	}

	vmf.vmf_ah = 0x88;
	vm86_intcall(0x15, &vmf);
	*extmem = vmf.vmf_ax;
	method = 0x88;

done:
	printf("BIOS basemem: %dK, extmem: %dK (from %p call)\n",
	    *basemem, *extmem, method);

#if 0
	/* VESA setup -- ? */
	vmf.vmf_ax = 0x4f02;
	error = vm86_intcall(0x10, &vmf);
#endif
}

static void
vm86_initflags(struct vm86frame *vmf)
{
	int eflags = vmf->vmf_eflags;
	struct vm86_kernel *vm86 = &curpcb->pcb_ext->ext_vm86;

	if (vm86->vm86_has_vme) {
		eflags = (vmf->vmf_eflags & ~VME_USERCHANGE) |
		    (eflags & VME_USERCHANGE) | PSL_VM;
	} else {
		vm86->vm86_eflags = eflags;     /* save VIF, VIP */
		eflags = (vmf->vmf_eflags & ~VM_USERCHANGE) |             
		    (eflags & VM_USERCHANGE) | PSL_VM;
	}
	vmf->vmf_eflags = eflags | PSL_VM;
}

/*
 * called from vm86_bioscall, while in vm86 address space, to finalize setup.
 */
void
vm86_prepcall(struct vm86frame vmf)
{
	u_long addr[] = { 0xA00, 0x1000 };	/* code, stack */
	u_char intcall[] = {
		CLI, INTn, 0x00, STI, HLT
	};

	if ((vmf.vmf_trapno & PAGE_MASK) <= 0xff) {
		/* interrupt call requested */
        	intcall[2] = (u_char)(vmf.vmf_trapno & 0xff);
		memcpy((void *)addr[0], (void *)intcall, sizeof(intcall));
		vmf.vmf_ip = addr[0];
		vmf.vmf_cs = 0;
	}
	vmf.vmf_sp = addr[1] - 2;              /* keep aligned */
	vmf.kernel_es = vmf.kernel_ds = 0;
	vmf.vmf_ss = 0;
	vmf.vmf_eflags = PSL_VIF | PSL_VM | PSL_USER;
	vm86_initflags(&vmf);
}

/*
 * vm86 trap handler; determines whether routine succeeded or not.
 * Called while in vm86 space, returns to calling process.
 */
void
vm86_trap(struct vm86frame *vmf)
{
	caddr_t addr;

	/* "should not happen" */
	if ((vmf->vmf_eflags & PSL_VM) == 0)
		panic("vm86_trap called, but not in vm86 mode");

	addr = MAKE_ADDR(vmf->vmf_cs, vmf->vmf_ip);
	if (*(u_char *)addr == HLT)
		vmf->vmf_trapno = vmf->vmf_eflags & PSL_C;
	else
		vmf->vmf_trapno = vmf->vmf_trapno << 16;

	vm86_biosret(vmf);
}

int
vm86_intcall(int intnum, struct vm86frame *vmf)
{
	if (intnum < 0 || intnum > 0xff)
		return (EINVAL);

	vmf->vmf_trapno = intnum;
	return (vm86_bioscall(vmf));
}

/*
 * buffer must be entirely contained in a wired down page in kernel memory,
 * and is mapped into page 1 in vm86 space.  segment/offset will be filled
 * in to create a vm86 pointer to the buffer. If intnum is a valid
 * interrupt number (0-255), then the "interrupt trampoline" will be
 * used, otherwise we use the caller's cs:ip routine.  
 *
 * a future revision may allow multiple pages to be mapped, or allow  
 * the caller to pass in a custom page table to use.
 */
int
vm86_datacall(intnum, vmf, buffer, buflen, segment, offset)
	int intnum;
	struct vm86frame *vmf;
	char *buffer;
	int buflen;
	u_short *segment, *offset;
{
	u_int page;

        page = (u_int)buffer & PG_FRAME;
        *offset = (u_int)buffer & PAGE_MASK;
        if ((*offset + buflen) & PG_FRAME)
		return (-1);				/* XXX fixme! */
	*segment = 0x100;
	page = vtophys(page);
	vmf->vmf_trapno = page | (intnum & PAGE_MASK);
	return (vm86_bioscall(vmf));
}

#if 0
int
vm86_datacall(int intnum, u_int kpage, struct vm86frame *vmf)
{
	if (kpage & PAGE_MASK)
		return (EINVAL);
	kpage = vtophys(kpage);

	vmf->vmf_trapno = kpage | (intnum & PAGE_MASK);
	return (vm86_bioscall(vmf));
}
#endif

int
vm86_sysarch(p, args)
	struct proc *p;
	char *args;
{
	int error = 0;
	struct i386_vm86_args ua;
	struct vm86_kernel *vm86;

	if (error = copyin(args, &ua, sizeof(struct i386_vm86_args)))
		return (error);

	if (p->p_addr->u_pcb.pcb_ext == 0)
		if (error = i386_extend_pcb(p))
			return (error);
	vm86 = &p->p_addr->u_pcb.pcb_ext->ext_vm86;

	switch (ua.sub_op) {
	case VM86_INIT: {
		struct vm86_init_args sa;

		if (error = copyin(ua.sub_args, &sa, sizeof(sa)))
			return (error);
		if (cpu_feature & CPUID_VME)
			vm86->vm86_has_vme = (rcr4() & CR4_VME ? 1 : 0);
		else
			vm86->vm86_has_vme = 0;
		vm86->vm86_inited = 1;
		vm86->vm86_debug = sa.debug;
		bcopy(&sa.int_map, vm86->vm86_intmap, 32);
		}
		break;

#if 0
	case VM86_SET_VME: {
		struct vm86_vme_args sa;
	
		if ((cpu_feature & CPUID_VME) == 0)
			return (ENODEV);

		if (error = copyin(ua.sub_args, &sa, sizeof(sa)))
			return (error);
		if (sa.state)
			load_cr4(rcr4() | CR4_VME);
		else
			load_cr4(rcr4() & ~CR4_VME);
		}
		break;
#endif

	case VM86_GET_VME: {
		struct vm86_vme_args sa;

		sa.state = (rcr4() & CR4_VME ? 1 : 0);
        	error = copyout(&sa, ua.sub_args, sizeof(sa));
		}
		break;

#if 0
	case VM86_INTCALL: {
		struct vm86_intcall_args sa;

		if (error = copyin(ua.sub_args, &sa, sizeof(sa)))
			return (error);
		if (error = vm86_intcall(sa.intnum, &sa.vmf))
			return (error);
		error = copyout(&sa, ua.sub_args, sizeof(sa));
		}
		break;
#endif

	default:
		error = EINVAL;
	}
	return (error);
}
