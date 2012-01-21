/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	from: @(#)npx.h	5.3 (Berkeley) 1/18/91
 * $FreeBSD$
 */

/*
 * Floating Point Data Structures and Constants
 * W. Jolitz 1/90
 */

#ifndef _MACHINE_FPU_H_
#define	_MACHINE_FPU_H_

/* Contents of each x87 floating point accumulator */
struct fpacc87 {
	uint8_t	fp_bytes[10];
};

/* Contents of each SSE extended accumulator */
struct  xmmacc {
	uint8_t	xmm_bytes[16];
};

/* Contents of the upper 16 bytes of each AVX extended accumulator */
struct  ymmacc {
	uint8_t  ymm_bytes[16];
};

struct  envxmm {
	uint16_t	en_cw;		/* control word (16bits) */
	uint16_t	en_sw;		/* status word (16bits) */
	uint8_t		en_tw;		/* tag word (8bits) */
	uint8_t		en_zero;
	uint16_t	en_opcode;	/* opcode last executed (11 bits ) */
	uint64_t	en_rip;		/* floating point instruction pointer */
	uint64_t	en_rdp;		/* floating operand pointer */
	uint32_t	en_mxcsr;	/* SSE sontorol/status register */
	uint32_t	en_mxcsr_mask;	/* valid bits in mxcsr */
};

struct  savefpu {
	struct	envxmm	sv_env;
	struct {
		struct fpacc87	fp_acc;
		uint8_t		fp_pad[6];      /* padding */
	} sv_fp[8];
	struct xmmacc	sv_xmm[16];
	uint8_t sv_pad[96];
} __aligned(16);

struct xstate_hdr {
	uint64_t xstate_bv;
	uint8_t xstate_rsrv0[16];
	uint8_t	xstate_rsrv[40];
};

struct savefpu_xstate {
	struct xstate_hdr sx_hd;
	struct ymmacc	sx_ymm[16];
};

struct savefpu_ymm {
	struct	envxmm	sv_env;
	struct {
		struct fpacc87	fp_acc;
		int8_t		fp_pad[6];      /* padding */
	} sv_fp[8];
	struct xmmacc	sv_xmm[16];
	uint8_t sv_pad[96];
	struct savefpu_xstate sv_xstate;
} __aligned(64);

#ifdef _KERNEL

struct fpu_kern_ctx;

#define	PCB_USER_FPU(pcb) (((pcb)->pcb_flags & PCB_KERNFPU) == 0)

#define	XSAVE_AREA_ALIGN	64

#endif

/*
 * The hardware default control word for i387's and later coprocessors is
 * 0x37F, giving:
 *
 *	round to nearest
 *	64-bit precision
 *	all exceptions masked.
 *
 * FreeBSD/i386 uses 53 bit precision for things like fadd/fsub/fsqrt etc
 * because of the difference between memory and fpu register stack arguments.
 * If its using an intermediate fpu register, it has 80/64 bits to work
 * with.  If it uses memory, it has 64/53 bits to work with.  However,
 * gcc is aware of this and goes to a fair bit of trouble to make the
 * best use of it.
 *
 * This is mostly academic for AMD64, because the ABI prefers the use
 * SSE2 based math.  For FreeBSD/amd64, we go with the default settings.
 */
#define	__INITIAL_FPUCW__	0x037F
#define	__INITIAL_FPUCW_I386__	0x127F
#define	__INITIAL_MXCSR__	0x1F80
#define	__INITIAL_MXCSR_MASK__	0xFFBF

#ifdef _KERNEL
void	fpudna(void);
void	fpudrop(void);
void	fpuexit(struct thread *td);
int	fpuformat(void);
int	fpugetregs(struct thread *td);
void	fpuinit(void);
void	fpusave(void *addr);
int	fpusetregs(struct thread *td, struct savefpu *addr,
	    char *xfpustate, size_t xfpustate_size);
int	fpusetxstate(struct thread *td, char *xfpustate,
	    size_t xfpustate_size);
int	fputrap(void);
void	fpuuserinited(struct thread *td);
struct fpu_kern_ctx *fpu_kern_alloc_ctx(u_int flags);
void	fpu_kern_free_ctx(struct fpu_kern_ctx *ctx);
int	fpu_kern_enter(struct thread *td, struct fpu_kern_ctx *ctx,
	    u_int flags);
int	fpu_kern_leave(struct thread *td, struct fpu_kern_ctx *ctx);
int	fpu_kern_thread(u_int flags);
int	is_fpu_kern_thread(u_int flags);

/*
 * Flags for fpu_kern_alloc_ctx(), fpu_kern_enter() and fpu_kern_thread().
 */
#define	FPU_KERN_NORMAL	0x0000
#define	FPU_KERN_NOWAIT	0x0001

#endif

#endif /* !_MACHINE_FPU_H_ */
