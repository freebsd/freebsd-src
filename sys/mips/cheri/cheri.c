/*-
 * Copyright (c) 2011-2017 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/cheri_serial.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>

#include <ddb/ddb.h>
#include <sys/kdb.h>

#include <cheri/cheri.h>

#include <machine/atomic.h>
#include <machine/cherireg.h>
#include <machine/pcb.h>
#include <machine/proc.h>
#include <machine/sysarch.h>

/*
 * Beginnings of a programming interface for explicitly managing capability
 * registers.  Convert back and forth between capability registers and
 * general-purpose registers/memory so that we can program the context,
 * save/restore application contexts, etc.
 *
 * In the future, we'd like the compiler to do this sort of stuff for us
 * based on language-level properties and annotations, but in the mean
 * time...
 *
 * XXXRW: Any manipulation of $ddc should include a "memory" clobber for inline
 * assembler, so that the compiler will write back memory contents before the
 * call, and reload them afterwards.
 */

static void	cheri_capability_set_user_ddc(struct chericap *);
static void	cheri_capability_set_user_stc(struct chericap *);
static void	cheri_capability_set_user_pcc(struct chericap *);
static void	cheri_capability_set_user_entry(struct chericap *,
		    unsigned long);
static void	cheri_capability_set_user_sigcode(struct chericap *,
		   struct sysentvec *);

static union {
	struct chericap	ct_cap;
	uint8_t		ct_bytes[32];
} cheri_testunion __aligned(32);

/*
 * A set of compile-time assertions to ensure suitable alignment for
 * capabilities embedded within other MIPS data structures.  Otherwise changes
 * that work on other architectures might break alignment on CHERI.
 */
CTASSERT(offsetof(struct trapframe, ddc) % CHERICAP_SIZE == 0);
CTASSERT(offsetof(struct mdthread, md_tls_cap) % CHERICAP_SIZE == 0);

/*
 * For now, all we do is declare what we support, as most initialisation took
 * place in the MIPS machine-dependent assembly.  CHERI doesn't need a lot of
 * actual boot-time initialisation.
 */
static void
cheri_cpu_startup(void)
{

	/*
	 * The pragmatic way to test that the kernel we're booting has a
	 * capability size matching the CPU we're booting on is to store a
	 * capability in memory and then check what its footprint was.  Panic
	 * early if our assumptions are wrong.
	 */
	memset(&cheri_testunion, 0xff, sizeof(cheri_testunion));
	cheri_capability_set_null(&cheri_testunion.ct_cap);
#ifdef CPU_CHERI128
	printf("CHERI: compiled for 128-bit capabilities\n");
	if (cheri_testunion.ct_bytes[16] == 0)
		panic("CPU implements 256-bit capabilities");
#else
	printf("CHERI: compiled for 256-bit capabilities\n");
	if (cheri_testunion.ct_bytes[16] != 0)
		panic("CPU implements 128-bit capabilities");
#endif
}
SYSINIT(cheri_cpu_startup, SI_SUB_CPU, SI_ORDER_FIRST, cheri_cpu_startup,
    NULL);

/*
 * Build a new capabilty derived from $kdc with the contents of the passed
 * flattened representation.  Only unsealed capabilities are supported;
 * capabilities must be separately sealed if required.
 *
 * XXXRW: It's not yet clear how important ordering is here -- try to do the
 * privilege downgrade in a way that will work when doing an "in place"
 * downgrade, with permissions last.
 *
 * XXXRW: In the new world order of CSetBounds, it's not clear that taking
 * explicit base/length/offset arguments is quite the right thing.
 */
void
cheri_capability_set(struct chericap *cp, uint32_t perms, void *basep,
    size_t length, off_t off)
{
#ifdef INVARIANTS
	register_t r;
#endif

	/* 'basep' is relative to $kdc. */
	CHERI_CINCOFFSET(CHERI_CR_CTEMP0, CHERI_CR_KDC, (register_t)basep);
	CHERI_CSETBOUNDS(CHERI_CR_CTEMP0, CHERI_CR_CTEMP0,
	    (register_t)length);
	CHERI_CANDPERM(CHERI_CR_CTEMP0, CHERI_CR_CTEMP0, (register_t)perms);
	CHERI_CINCOFFSET(CHERI_CR_CTEMP0, CHERI_CR_CTEMP0, (register_t)off);

	/*
	 * NB: With imprecise bounds, we want to assert that the results will
	 * be 'as requested' -- i.e., that the kernel always request bounds
	 * that can be represented precisly.
	 *
	 * XXXRW: Given these assupmtions, we actually don't need to do the
	 * '+= off' above.
	 */
#ifdef INVARIANTS
	CHERI_CGETTAG(r, CHERI_CR_CTEMP0);
	KASSERT(r != 0, ("%s: capability untagged", __func__));
	CHERI_CGETPERM(r, CHERI_CR_CTEMP0);
	KASSERT(r == (register_t)perms,
	    ("%s: permissions 0x%x rather than 0x%x", __func__,
	    (unsigned int)r, perms));
	CHERI_CGETBASE(r, CHERI_CR_CTEMP0);
	KASSERT(r == (register_t)basep,
	    ("%s: base %p rather than %p", __func__, (void *)r, basep));
	CHERI_CGETLEN(r, CHERI_CR_CTEMP0);
	KASSERT(r == (register_t)length,
	    ("%s: length 0x%x rather than %p", __func__,
	    (unsigned int)r, (void *)length));
	CHERI_CGETOFFSET(r, CHERI_CR_CTEMP0);
	KASSERT(r == (register_t)off,
	    ("%s: offset %p rather than %p", __func__, (void *)r,
	    (void *)off));
#if 0
	CHERI_CGETTYPE(r, CHERI_CR_CTEMP0);
	KASSERT(r == (register_t)otypep,
	    ("%s: otype %p rather than %p", __func__, (void *)r, otypep));
#endif
#endif
	CHERI_CSC(CHERI_CR_CTEMP0, CHERI_CR_KDC, (register_t)cp, 0);
}

/*
 * Functions to store a common set of capability values to in-memory
 * capabilities used in various aspects of user contexts.
 */
#ifdef _UNUSED
static void
cheri_capability_set_kern(struct chericap *cp)
{

	cheri_capability_set(cp, CHERI_CAP_KERN_PERMS, CHERI_CAP_KERN_BASE,
	    CHERI_CAP_KERN_LENGTH, CHERI_CAP_KERN_OFFSET);
}
#endif

static void
cheri_capability_set_user_ddc(struct chericap *cp)
{

	cheri_capability_set(cp, CHERI_CAP_USER_DATA_PERMS,
	    CHERI_CAP_USER_DATA_BASE, CHERI_CAP_USER_DATA_LENGTH,
	    CHERI_CAP_USER_DATA_OFFSET);
}

static void
cheri_capability_set_user_stc(struct chericap *cp)
{

	/*
	 * For now, initialise stack as ambient with identical rights as $ddc.
	 * In the future, we will likely want to change this to be local
	 * (non-global).
	 */
	cheri_capability_set_user_ddc(cp);
}

static void
cheri_capability_set_user_idc(struct chericap *cp)
{

	/*
	 * The default invoked data capability is also identical to $ddc.
	 */
	cheri_capability_set_user_ddc(cp);
}

static void
cheri_capability_set_user_pcc(struct chericap *cp)
{

	cheri_capability_set(cp, CHERI_CAP_USER_CODE_PERMS,
	    CHERI_CAP_USER_CODE_BASE, CHERI_CAP_USER_CODE_LENGTH,
	    CHERI_CAP_USER_CODE_OFFSET);
}

static void
cheri_capability_set_user_entry(struct chericap *cp, unsigned long entry_addr)
{

	/*
	 * Set the jump target regigster for the pure capability calling
	 * convention.
	 */
	cheri_capability_set(cp, CHERI_CAP_USER_CODE_PERMS,
	    CHERI_CAP_USER_CODE_BASE, CHERI_CAP_USER_CODE_LENGTH, entry_addr);
}

static void
cheri_capability_set_user_sigcode(struct chericap *cp, struct sysentvec *se)
{
	uintptr_t base;
	int szsigcode = *se->sv_szsigcode;

	/* XXX: true for mips64 and mip64-cheriabi... */
	base = (uintptr_t)se->sv_psstrings - szsigcode;
	base = rounddown2(base, sizeof(struct chericap));

	cheri_capability_set(cp, CHERI_CAP_USER_CODE_PERMS, (void *)base,
	    szsigcode, 0);
}

static void
cheri_capability_set_user_sealcap(struct chericap *cp)
{

	cheri_capability_set(cp, CHERI_SEALCAP_USERSPACE_PERMS,
	    CHERI_SEALCAP_USERSPACE_BASE, CHERI_SEALCAP_USERSPACE_LENGTH,
	    CHERI_SEALCAP_USERSPACE_OFFSET);
}

void
cheri_exec_setregs(struct thread *td, unsigned long entry_addr)
{
	struct trapframe *frame;
	struct cheri_signal *csigp;

	/*
	 * We assume that the caller has initialised the trapframe to zeroes
	 * -- but do a quick assertion or two to catch programmer error.  We
	 * might want to check this with a more thorough set of assertions in
	 * the future.
	 */
	frame = &td->td_pcb->pcb_regs;
	KASSERT(*(uint64_t *)&frame->ddc == 0, ("%s: non-zero initial $ddc",
	    __func__));
	KASSERT(*(uint64_t *)&frame->pcc == 0, ("%s: non-zero initial $epcc",
	    __func__));

	/*
	 * XXXRW: Experimental CHERI ABI initialises $ddc with full user
	 * privilege, and all other user-accessible capability registers with
	 * no rights at all.  The runtime linker/compiler/application can
	 * propagate around rights as required.
	 */
	cheri_capability_set_user_ddc(&frame->ddc);
	cheri_capability_set_user_stc(&frame->stc);
	cheri_capability_set_user_idc(&frame->idc);
	cheri_capability_set_user_pcc(&frame->pcc);
	cheri_capability_set_user_entry(&frame->c12, entry_addr);

	/*
	 * Also initialise signal-handling state; this can't yet be modified
	 * by userspace, but the principle is that signal handlers should run
	 * with ambient authority unless given up by the userspace runtime
	 * explicitly.
	 */
	csigp = &td->td_pcb->pcb_cherisignal;
	bzero(csigp, sizeof(*csigp));
	cheri_capability_set_user_ddc(&csigp->csig_ddc);
	cheri_capability_set_user_stc(&csigp->csig_stc);
	cheri_capability_set_user_stc(&csigp->csig_default_stack);
	cheri_capability_set_user_idc(&csigp->csig_idc);
	cheri_capability_set_user_pcc(&csigp->csig_pcc);
	cheri_capability_set_user_sigcode(&csigp->csig_sigcode,
	    td->td_proc->p_sysent);

	/*
	 * Set up root for the userspace object-type sealing capability tree.
	 * This can be queried using sysarch(2).
	 */
	cheri_capability_set_user_sealcap(&td->td_pcb->pcb_sealcap);
}

/*
 * Similar to a newly exec'd process, we need to set up the CHERI register
 * state for MIPS ABI processes with suitable capabilities.  We do this using
 * the existing MIPS registers as a starting point.
 *
 * XXX: Similar concerns exist here as exist above in cheri_exec_setregs().
 *
 * XXX: I also wonder if we should be inheriting signal-handling state...?
 */
void
cheri_newthread_setregs(struct thread *td)
{

	cheri_exec_setregs(td, td->td_pcb->pcb_regs.pc);
}

void
cheri_serialize(struct cheri_serial *csp, struct chericap *cap)
{
	register_t	r;
	cheri_capability_load(CHERI_CR_CTEMP0, cap);

#if CHERICAP_SIZE == 16
	csp->cs_storage = 3;
	csp->cs_typebits = 16;
	csp->cs_permbits = 23;
#else /* CHERICAP_SIZE == 32 */
	csp->cs_storage = 4;
	csp->cs_typebits = 24;
	csp->cs_permbits = 31;
#endif

	KASSERT(csp != NULL, ("Can't serialize to a NULL pointer"));
	if (cap == NULL) {
		memset(csp, 0, sizeof(*csp));
		return;
	}

	CHERI_CGETTAG(r, CHERI_CR_CTEMP0);
	csp->cs_tag = r;
	if (csp->cs_tag) {
		CHERI_CGETTYPE(r, CHERI_CR_CTEMP0);
		csp->cs_type = r;
		CHERI_CGETPERM(r, CHERI_CR_CTEMP0);
		csp->cs_perms = r;
		CHERI_CGETSEALED(r, CHERI_CR_CTEMP0);
		csp->cs_sealed = r;
		CHERI_CGETBASE(r, CHERI_CR_CTEMP0);
		csp->cs_base = r;
		CHERI_CGETLEN(r, CHERI_CR_CTEMP0);
		csp->cs_length = r;
		CHERI_CGETOFFSET(r, CHERI_CR_CTEMP0);
		csp->cs_offset = r;
	} else
		memcpy(&csp->cs_data, cap, CHERICAP_SIZE);
}
