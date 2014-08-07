/*-
 * Copyright (c) 2014 Andrew Turner
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/frame.h>

int
cpu_fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{

	panic("cpu_fetch_syscall_args");
}

void do_el1h_sync(struct trapframe *frame);
void do_el1h_sync(struct trapframe *frame)
{
	uint32_t exception;
	uint64_t esr;
	u_int reg;

	/* Read the esr register to get the exception details */
	__asm __volatile("mrs %x0, esr_el1" : "=&r"(esr));
	KASSERT((esr & (1 << 25)) != 0,
	    ("Invalid instruction length in exception"));

	exception = (esr >> 26) & 0x3f;

	printf("In do_el1h_sync %llx %llx %x\n", frame->tf_elr, esr, exception);

	for (reg = 0; reg < 31; reg++) {
		printf("x%d: %llx\n", reg, frame->tf_x[reg]);
	}
	switch(exception) {
	case 0x25:
		panic("Data abort at %#llx", frame->tf_elr);
		break;
	case 0x3c:
		printf("Breakpoint %u\n", (uint32_t)(esr & 0xffffff));
		break;
	default:
		panic("Unknown exception %x\n", exception);
	}
	frame->tf_elr += 4;
}

