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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>

#include <ddb/ddb.h>
#include <sys/kdb.h>

#include <machine/atomic.h>
#include <machine/cheri.h>
#include <machine/pcb.h>
#include <machine/sysarch.h>

/*-
 * Some user-level security models rely on strict call-return semantics, which
 * is implemented via a trusted stack in the object-capability invocation
 * path.  This file contains a simple implementation of a CHERI trusted stack
 * for the software exception path.
 *
 * XXXRW: Lots to think about here.
 *
 * 1. How do we want to handle user software models that aren't just
 *    call-return -- e.g., a closure-passing model.  Should the language
 *    runtime be able to rewrite return paths?
 * 2. Do we want some sort of kernel-implemented timeout/resource model, or
 *    just let userspace do it with signals?
 * 3. More generally, how do we want to deal with signals?  (a) switching to
 *    a suitable signal processing context; (b) extending sigcontext_t for
 *    capability state?
 */

/*
 * Initialise the trusted stack of a process (thread) control block.
 *
 * XXXRW: Someday, depth should perhaps be configurable.
 *
 * XXXRW: It makes sense to me that the stack starts empty, and the first
 * CCall populates it with a default return context (e.g., the language
 * runtime).  But does that make sense to anyone else?
 *
 * XXXRW: I wonder if somewhere near the CHERI stack is where signal
 * configuration goes -- e.g., the capability to "invoke" to enter a signal
 * handler.
 *
 * XXXRW: A fixed-size stack here may or may not be the right thing.
 */
void
cheri_stack_init(struct pcb *pcb)
{

	bzero(&pcb->pcb_cheristack, sizeof(pcb->pcb_cheristack));
	pcb->pcb_cheristack.cs_tsp = CHERI_STACK_SIZE;
}

/*
 * On fork(), we exactly reproduce the current thread's CHERI call stack in
 * the child, as the address space is exactly reproduced.
 *
 * XXXRW: Is this the right thing?
 */
void
cheri_stack_copy(struct pcb *pcb2, struct pcb *pcb1)
{

	cheri_memcpy(pcb2, pcb1, sizeof(*pcb2));
}
