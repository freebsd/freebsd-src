/*-
 * Copyright (c) 1999 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_UCONTEXT_H_
#define	_MACHINE_UCONTEXT_H_

#include <machine/_regset.h>

/*
 * The mc_flags field provides the necessary clues when dealing with the gory
 * details of ia64 specific contexts. A comprehensive explanation is added for
 * everybody's sanity, including the author's.
 *
 * The first and foremost variation in the context is synchronous contexts
 * (= synctx) versus asynchronous contexts (= asynctx). A synctx is created
 * synchronously WRT program execution and has the advantage that none of the
 * scratch registers have to be saved. They are assumed to be clobbered by the
 * call to the function that creates the context. An asynctx needs to have the
 * scratch registers preserved because it can describe any point in a thread's
 * (or process') execution.
 *
 * Below a description of the flags and their meaning:
 *
 *	_MC_FLAGS_ASYNC_CONTEXT
 *		If set, indicates that mc_scratch and mc_scratch_fp are both
 *		valid. IFF not set, _MC_FLAGS_RETURN_VALID indicates if the
 *		return registers are valid or not.
 *	_MC_FLAGS_HIGHFP_VALID
 *		If set, indicates that the high FP registers (f32-f127) are
 *		valid. This flag is very likely not going to be set for any
 *		sensible synctx, but is not explicitly disallowed. Any synctx
 *		that has this flag may or may not have the high FP registers
 *		restored. In short: don't do it.
 *	_MC_FLAGS_KSE_SET_MBOX
 *		This flag is special to setcontext(2) and swapcontext(2). It
 *		instructs the kernel to write the value in mc_special.isr to
 *		the memory address pointed to by mc_special.ifa. This allows
 *		the kernel to switch to a new context in a KSE based threaded
 *		program. Note that this is a non-srandard extension to the
 *		otherwise standard system calls and use of this flag should be
 *		limited to KSE.
 *	_MC_FLAGS_RETURN_VALID
 *		If set and _MC_FLAGS_ASYNC_CONTEXT is not set indicates that
 *		the ABI defined return registers are valid. Both getcontext(2)
 *		and swapcontext(2) need to save the system call return values.
 *		Any synctx that does not have this flag may still have the
 *		return registers restored from the context.
 *	_MC_FLAGS_SCRATCH_VALID
 *		If set and _MC_FLAGS_ASYNC_CONTEXT is not set indicates that
 *		the scratch registers, but not the FP registers are valid.
 *		This flag is set in contexts passed to signal handlers. This
 *		flag is a superset of _MC_FLAGS_RETURN_VALID. If both flags
 *		are set, this flag takes precedence.
 */

typedef struct __mcontext {
	unsigned long		mc_flags;
#define	_MC_FLAGS_ASYNC_CONTEXT		0x0001
#define	_MC_FLAGS_HIGHFP_VALID		0x0002
#define	_MC_FLAGS_KSE_SET_MBOX		0x0004
#define	_MC_FLAGS_RETURN_VALID		0x0008
#define	_MC_FLAGS_SCRATCH_VALID		0x0010
	unsigned long		_reserved_;
	struct _special		mc_special;
	struct _callee_saved	mc_preserved;
	struct _callee_saved_fp	mc_preserved_fp;
	struct _caller_saved	mc_scratch;
	struct _caller_saved_fp	mc_scratch_fp;
	struct _high_fp		mc_high_fp;
} mcontext_t;

#endif /* !_MACHINE_UCONTEXT_H_ */
