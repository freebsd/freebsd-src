/*-
 * Copyright (c) 2014 Robert N. M. Watson
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

#ifndef _CHERI_ENTER_H_
#define	_CHERI_ENTER_H_

typedef register_t (*cheri_system_user_fn_t)(register_t methodnum,
	    register_t a1, register_t a2, register_t a3, register_t a4,
	    register_t a5, register_t a6, register_t a7,
	    struct cheri_object system_object, __capability void *c3,
	    __capability void *c4, __capability void *c5,
	    __capability void *c6, __capability void *c7)
	    __attribute__((cheri_ccall));

void	cheri_system_user_register_fn(cheri_system_user_fn_t fn_ptr);

/*
 * Method numbers used by the sandbox runtime itself.
 *
 * WARNING: These values must match those currently hard coded in the sandbox
 * C runtime (lib/csu/cheri/crt_sb.S).
 *
 * NB: In the future, these should be via a reserved entry point rather than
 * the primary object-capability 'invoke' entry point, so that they can be
 * called only by the runtime.
 */
#define	SANDBOX_RUNTIME_CONSTRUCTORS	(-1)
#define	SANDBOX_RUNTIME_DESTROCTORS	(-2)

#define	CHERI_SYSTEM_USER_BASE		1000
#define	CHERI_SYSTEM_USER_CEILING	2000

#endif /* !_CHERI_ENTER_H_ */
