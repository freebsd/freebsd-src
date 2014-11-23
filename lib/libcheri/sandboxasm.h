/*-
 * Copyright (c) 2013 Robert N. M. Watson
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

#ifndef _SANDBOXASM_H_
#define	_SANDBOXASM_H_

/*
 * Per-sandbox meta-data structure mapped read-only within the sandbox at a
 * fixed address to allow sandboxed code to find its stack, heap, etc.
 *
 * The base address must match libcheri's sandbox.c as well as the linker
 * scripts used to statically link sandboxed code.  The offsets must match
 * struct sandbox_metadata in sandbox.h.  See the comment there for good
 * reasons not to change these definitions if you can avoid it.
 *
 * XXXRW: For reasons I don't understand, and should learn about, I can't get
 * this to usefully include in .S files.  But that is the actual goal -- they
 * should use these definitions rather than hard-coded values.
 */
#define	SANDBOX_METADATA_BASE			0x1000
#define	SANDBOX_METADATA_OFFSET_HEAPBASE	0
#define	SANDBOX_METADATA_OFFSET_HEAPLEN		8

#define	SANDBOX_BINARY_BASE	0x8000
#define	SANDBOX_VECTOR_SIZE	0x200

/*
 * These are the (defined) entry vectors for sandbox
 */
#define	SANDBOX_RTLD_VECTOR	(SANDBOX_BINARY_BASE + SANDBOX_VECTOR_SIZE * 0)
#define	SANDBOX_INVOKE_VECTOR	(SANDBOX_BINARY_BASE + SANDBOX_VECTOR_SIZE * 1)

#endif /* !_SANDBOXASM_H_ */
