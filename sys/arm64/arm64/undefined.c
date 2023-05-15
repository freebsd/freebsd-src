/*-
 * Copyright (c) 2017 Andrew Turner
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>

#include <machine/atomic.h>
#include <machine/frame.h>
#define _MD_WANT_SWAPWORD
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/undefined.h>
#include <machine/vmparam.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

/* Low bit masked off */
#define	INSN_COND(insn)	((insn >> 28) & ~0x1)
#define	INSN_COND_INVERTED(insn)	((insn >> 28) & 0x1)
#define	INSN_COND_EQ	0x00	/* NE */
#define	INSN_COND_CS	0x02	/* CC */
#define	INSN_COND_MI	0x04	/* PL */
#define	INSN_COND_VS	0x06	/* VC */
#define	INSN_COND_HI	0x08	/* LS */
#define	INSN_COND_GE	0x0a	/* LT */
#define	INSN_COND_GT	0x0c	/* LE */
#define	INSN_COND_AL	0x0e	/* Always */

MALLOC_DEFINE(M_UNDEF, "undefhandler", "Undefined instruction handler data");

#ifdef COMPAT_FREEBSD32
#ifndef EMUL_SWP
#define	EMUL_SWP	0
#endif

SYSCTL_DECL(_compat_arm);

static bool compat32_emul_swp = EMUL_SWP;
SYSCTL_BOOL(_compat_arm, OID_AUTO, emul_swp,
    CTLFLAG_RWTUN | CTLFLAG_MPSAFE, &compat32_emul_swp, 0,
    "Enable SWP/SWPB emulation");
#endif

struct undef_handler {
	LIST_ENTRY(undef_handler) uh_link;
	undef_handler_t		uh_handler;
};

/*
 * Create two undefined instruction handler lists, one for userspace, one for
 * the kernel. This allows us to handle instructions that will trap
 */
LIST_HEAD(, undef_handler) undef_handlers[2];

/*
 * Work around a bug in QEMU prior to 2.5.1 where reading unknown ID
 * registers would raise an exception when they should return 0.
 */
static int
id_aa64mmfr2_handler(vm_offset_t va, uint32_t insn, struct trapframe *frame,
    uint32_t esr)
{
	int reg;

#define	 MRS_ID_AA64MMFR2_EL0_MASK	(MRS_MASK | 0x000fffe0)
#define	 MRS_ID_AA64MMFR2_EL0_VALUE	(MRS_VALUE | 0x00080740)

	/* mrs xn, id_aa64mfr2_el1 */
	if ((insn & MRS_ID_AA64MMFR2_EL0_MASK) == MRS_ID_AA64MMFR2_EL0_VALUE) {
		reg = MRS_REGISTER(insn);

		frame->tf_elr += INSN_SIZE;
		if (reg < nitems(frame->tf_x)) {
			frame->tf_x[reg] = 0;
		} else if (reg == 30) {
			frame->tf_lr = 0;
		}
		/* If reg is 32 then write to xzr, i.e. do nothing */

		return (1);
	}
	return (0);
}

static bool
arm_cond_match(uint32_t insn, struct trapframe *frame)
{
	uint64_t spsr;
	uint32_t cond;
	bool invert;
	bool match;

	/*
	 * Generally based on the function of the same name in NetBSD, though
	 * condition bits left in their original position rather than shifting
	 * over the low bit that indicates inversion for quicker sanity checking
	 * against spec.
	 */
	spsr = frame->tf_spsr;
	cond = INSN_COND(insn);
	invert = INSN_COND_INVERTED(insn);

	switch (cond) {
	case INSN_COND_EQ:
		match = (spsr & PSR_Z) != 0;
		break;
	case INSN_COND_CS:
		match = (spsr & PSR_C) != 0;
		break;
	case INSN_COND_MI:
		match = (spsr & PSR_N) != 0;
		break;
	case INSN_COND_VS:
		match = (spsr & PSR_V) != 0;
		break;
	case INSN_COND_HI:
		match = (spsr & (PSR_C | PSR_Z)) == PSR_C;
		break;
	case INSN_COND_GE:
		match = (!(spsr & PSR_N) == !(spsr & PSR_V));
		break;
	case INSN_COND_GT:
		match = !(spsr & PSR_Z) && (!(spsr & PSR_N) == !(spsr & PSR_V));
		break;
	case INSN_COND_AL:
		match = true;
		break;
	default:
		__assert_unreachable();
	}

	return (match != invert);
}

#ifdef COMPAT_FREEBSD32
/* arm32 GDB breakpoints */
#define GDB_BREAKPOINT	0xe6000011
#define GDB5_BREAKPOINT	0xe7ffdefe
static int
gdb_trapper(vm_offset_t va, uint32_t insn, struct trapframe *frame,
		uint32_t esr)
{
	struct thread *td = curthread;

	if (insn == GDB_BREAKPOINT || insn == GDB5_BREAKPOINT) {
		if (SV_PROC_FLAG(td->td_proc, SV_ILP32) &&
		    va < VM_MAXUSER_ADDRESS) {
			ksiginfo_t ksi;

			ksiginfo_init_trap(&ksi);
			ksi.ksi_signo = SIGTRAP;
			ksi.ksi_code = TRAP_BRKPT;
			ksi.ksi_addr = (void *)va;
			trapsignal(td, &ksi);
			return 1;
		}
	}
	return 0;
}

static int
swp_emulate(vm_offset_t va, uint32_t insn, struct trapframe *frame,
    uint32_t esr)
{
	ksiginfo_t ksi;
	struct thread *td;
	vm_offset_t vaddr;
	uint64_t *regs;
	uint32_t val;
	int attempts, error, Rn, Rd, Rm;
	bool is_swpb;

	td = curthread;

	/*
	 * swp, swpb only; there are no Thumb swp/swpb instructions so we can
	 * safely bail out if we're in Thumb mode.
	 */
	if (!compat32_emul_swp || !SV_PROC_FLAG(td->td_proc, SV_ILP32) ||
	    (frame->tf_spsr & PSR_T) != 0)
		return (0);
	else if ((insn & 0x0fb00ff0) != 0x01000090)
		return (0);
	else if (!arm_cond_match(insn, frame))
		goto next;	/* Handled, but does nothing */

	Rn = (insn & 0xf0000) >> 16;
	Rd = (insn & 0xf000) >> 12;
	Rm = (insn & 0xf);

	regs = frame->tf_x;
	vaddr = regs[Rn] & 0xffffffff;
	val = regs[Rm];

	/* Enforce alignment for swp. */
	is_swpb = (insn & 0x00400000) != 0;
	if (!is_swpb && (vaddr & 3) != 0)
		goto fault;

	attempts = 0;

	do {
		if (is_swpb) {
			uint8_t bval;

			bval = val;
			error = swapueword8((void *)vaddr, &bval);
			val = bval;
		} else {
			error = swapueword32((void *)vaddr, &val);
		}

		if (error == -1)
			goto fault;

		/*
		 * Avoid potential DoS, e.g., on CPUs that don't implement
		 * global monitors.
		 */
		if (error != 0 && (++attempts % 5) == 0)
			maybe_yield();
	} while (error != 0);

	regs[Rd] = val;

next:
	/* No thumb SWP/SWPB */
	frame->tf_elr += 4; //INSN_SIZE;

	return (1);
fault:
	ksiginfo_init_trap(&ksi);
	ksi.ksi_signo = SIGSEGV;
	ksi.ksi_code = SEGV_MAPERR;
	ksi.ksi_addr = (void *)va;
	trapsignal(td, &ksi);

	return (1);
}
#endif

void
undef_init(void)
{

	LIST_INIT(&undef_handlers[0]);
	LIST_INIT(&undef_handlers[1]);

	install_undef_handler(false, id_aa64mmfr2_handler);
#ifdef COMPAT_FREEBSD32
	install_undef_handler(true, gdb_trapper);
	install_undef_handler(true, swp_emulate);
#endif
}

void *
install_undef_handler(bool user, undef_handler_t func)
{
	struct undef_handler *uh;

	uh = malloc(sizeof(*uh), M_UNDEF, M_WAITOK);
	uh->uh_handler = func;
	LIST_INSERT_HEAD(&undef_handlers[user ? 0 : 1], uh, uh_link);

	return (uh);
}

void
remove_undef_handler(void *handle)
{
	struct undef_handler *uh;

	uh = handle;
	LIST_REMOVE(uh, uh_link);
	free(handle, M_UNDEF);
}

int
undef_insn(u_int el, struct trapframe *frame)
{
	struct undef_handler *uh;
	uint32_t insn;
	int ret;

	KASSERT(el < 2, ("Invalid exception level %u", el));

	if (el == 0) {
		ret = fueword32((uint32_t *)frame->tf_elr, &insn);
		if (ret != 0)
			panic("Unable to read userspace faulting instruction");
	} else {
		insn = *(uint32_t *)frame->tf_elr;
	}

	LIST_FOREACH(uh, &undef_handlers[el], uh_link) {
		ret = uh->uh_handler(frame->tf_elr, insn, frame, frame->tf_esr);
		if (ret)
			return (1);
	}

	return (0);
}
