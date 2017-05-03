/*-
 * Copyright (c) 2011-2017 Robert N. M. Watson
 * Copyright (c) 2015 SRI International
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

#ifndef _SYS_CHERI_H_
#define	_SYS_CHERI_H_

#ifdef _KERNEL
#include <sys/sysctl.h>		/* SYSCTL_DECL() */
#include <sys/systm.h>		/* CTASSERT() */
#endif

#include <sys/types.h>

#include <machine/cherireg.h>	/* CHERICAP_SIZE. */

/*
 * Canonical C-language representation of a capability for compilers that
 * don't support capabilities directly.  The in-memory layout is sensitive to
 * the microarchitecture, and hence treated as opaque.  Fields must be
 * accessed via the ISA.
 */
struct chericap {
	uint8_t		c_data[CHERICAP_SIZE];
} __packed __aligned(CHERICAP_SIZE);
#ifdef _KERNEL
CTASSERT(sizeof(struct chericap) == CHERICAP_SIZE);
#endif

/*
 * Canonical C-language representation of a CHERI object capability -- code and
 * data capabilities in registers or memory.
 */
struct cheri_object {
#if !defined(_KERNEL) && __has_feature(capabilities)
	__capability void	*co_codecap;
	__capability void	*co_datacap;
#else
	struct chericap		 co_codecap;
	struct chericap		 co_datacap;
#endif
};

#if !defined(_KERNEL) && __has_feature(capabilities)
#define	CHERI_OBJECT_INIT_NULL	{NULL, NULL}
#define	CHERI_OBJECT_ISNULL(co)	\
    ((co).co_codecap == NULL && (co).co_datacap == NULL)
#endif

/*
 * Data structure describing CHERI's sigaltstack-like extensions to signal
 * delivery.  In the event that a thread takes a signal when $pcc doesn't hold
 * CHERI_PERM_SYSCALL, we will need to install new $pcc, $ddc, $stc, and $idc
 * state, and move execution to the per-thread alternative stack, whose
 * pointer should (presumably) be relative to the $ddc/$stc defined here.
 */
struct cheri_signal {
#if !defined(_KERNEL) && __has_feature(capabilities)
	__capability void	*csig_pcc;
	__capability void	*csig_ddc;
	__capability void	*csig_stc;
	__capability void	*csig_idc;
	__capability void	*csig_default_stack;
	__capability void	*csig_sigcode;
#else
	struct chericap		 csig_pcc;
	struct chericap		 csig_ddc;
	struct chericap		 csig_stc;
	struct chericap		 csig_idc;
	struct chericap		 csig_default_stack;
	struct chericap		 csig_sigcode;
#endif
};

/*
 * Per-thread CHERI CCall/CReturn stack, which preserves the calling PC/PCC/
 * IDC across CCall so that CReturn can restore them.
 *
 * XXXRW: This is a very early experiment -- it's not clear if this idea will
 * persist in its current form, or at all.  For more complex userspace
 * language, there's a reasonable expectation that it, rather than the kernel,
 * will want to manage the idea of a "trusted stack".
 *
 * XXXRW: This is currently part of the kernel-user ABI due to the
 * CHERI_GET_STACK and CHERI_SET_STACK sysarch() calls.  In due course we need
 * to revise those APIs and differentiate the kernel-internal representation
 * from the public one.
 */
struct cheri_stack_frame {
	register_t	_csf_pad0;	/* Used to be MIPS program counter. */
	register_t	_csf_pad1;
	register_t	_csf_pad2;
	register_t	_csf_pad3;
#if !defined(_KERNEL) && __has_feature(capabilities)
	__capability void	*csf_pcc;
	__capability void	*csf_idc;
#else
	struct chericap	csf_pcc;
	struct chericap	csf_idc;
#endif
};

#define	CHERI_STACK_DEPTH	8	/* XXXRW: 8 is a nice round number. */
struct cheri_stack {
	register_t	cs_tsp;		/* Byte offset, not frame index. */
	register_t	cs_tsize;	/* Stack size, in bytes. */
	register_t	_cs_pad0;
	register_t	_cs_pad1;
	struct cheri_stack_frame	cs_frames[CHERI_STACK_DEPTH];
} __aligned(CHERICAP_SIZE);

#define	CHERI_FRAME_SIZE	sizeof(struct cheri_stack_frame)
#define	CHERI_STACK_SIZE	(CHERI_STACK_DEPTH * CHERI_FRAME_SIZE)

/*
 * APIs that act on C language representations of capabilities -- but not
 * capabilities themselves.
 */
#ifdef _KERNEL
void	cheri_capability_set(struct chericap *cp, uint32_t uperms,
	    void *basep, size_t length, off_t off);
void	cheri_capability_set_null(struct chericap *cp);
void	cheri_capability_setoffset(struct chericap *cp, register_t offset);

/*
 * CHERI capability utility functions.
 */
void	 cheri_bcopy(void *src, void *dst, size_t len);
void	*cheri_memcpy(void *dst, void *src, size_t len);

/*
 * CHERI context management functions.
 */
struct cheri_frame;
struct thread;
struct trapframe;
const char	*cheri_exccode_string(uint8_t exccode);
void	cheri_exec_setregs(struct thread *td, u_long entry_addr);
void	cheri_log_cheri_frame(struct trapframe *frame);
void	cheri_log_exception(struct trapframe *frame, int trap_type);
void	cheri_log_exception_registers(struct trapframe *frame);
void	cheri_newthread_setregs(struct thread *td);
int	cheri_syscall_authorize(struct thread *td, u_int code,
	    int nargs, register_t *args);
int	cheri_signal_sandboxed(struct thread *td);
void	cheri_sendsig(struct thread *td);
void	cheri_trapframe_from_cheriframe(struct trapframe *frame,
	    struct cheri_frame *cfp);
void	cheri_trapframe_to_cheriframe(struct trapframe *frame,
	    struct cheri_frame *cfp);

/*
 * Functions to set up and manipulate CHERI contexts and stacks.
 */
struct pcb;
struct sysarch_args;
void	cheri_sealcap_copy(struct pcb *dst, struct pcb *src);
void	cheri_signal_copy(struct pcb *dst, struct pcb *src);
void	cheri_stack_copy(struct pcb *dst, struct pcb *src);
void	cheri_stack_init(struct pcb *pcb);
int	cheri_stack_unwind(struct thread *td, struct trapframe *tf,
	    int signum);
int	cheri_sysarch_getsealcap(struct thread *td, struct sysarch_args *uap);
int	cheri_sysarch_getstack(struct thread *td, struct sysarch_args *uap);
int	cheri_sysarch_setstack(struct thread *td, struct sysarch_args *uap);

/*
 * Global sysctl definitions.
 */
SYSCTL_DECL(_security_cheri);
SYSCTL_DECL(_security_cheri_stats);
extern u_int	security_cheri_debugger_on_sandbox_signal;
extern u_int	security_cheri_debugger_on_sandbox_syscall;
extern u_int	security_cheri_debugger_on_sandbox_unwind;
extern u_int	security_cheri_debugger_on_sigprot;
extern u_int	security_cheri_sandboxed_signals;
extern u_int	security_cheri_syscall_violations;
extern u_int	security_cheri_improve_tags;

/*
 * Functions exposed to machine-independent code that must interact with
 * CHERI-specific features; e.g., ktrace.
 */
struct ktr_ccall;
struct ktr_creturn;
struct ktr_cexception;
struct thr_param_c;
void	cheriabi_thr_new_md(struct thread *parent_td,
	    struct thr_param_c *param);
void	ktrccall_mdfill(struct pcb *pcb, struct ktr_ccall *kc);
void	ktrcreturn_mdfill(struct pcb *pcb, struct ktr_creturn *kr);
void	ktrcexception_mdfill(struct trapframe *frame,
	    struct ktr_cexception *ke);
#endif /* !_KERNEL */

/*
 * Nested include of machine-dependent definitions, which likely depend on
 * first having defined chericap.h.
 */
#include <machine/cheri.h>

#endif /* _SYS_CHERI_H_ */
