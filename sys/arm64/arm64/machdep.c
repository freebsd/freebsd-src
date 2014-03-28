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
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/imgact.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>

#include <machine/bootinfo.h>
#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/reg.h>

struct pcpu __pcpu[MAXCPU];
struct pcpu *pcpup = &__pcpu[0];

vm_paddr_t phys_avail[10];

int cold = 1;
long realmem = 0;

void
bzero(void *buf, size_t len)
{
	memset(buf, 0, len);
}

int
fill_regs(struct thread *td, struct reg *regs)
{

	panic("fill_regs");
}

int
set_regs(struct thread *td, struct reg *regs)
{

	panic("set_regs");
}

int
fill_fpregs(struct thread *td, struct fpreg *regs)
{

	panic("fill_fpregs");
}

int
set_fpregs(struct thread *td, struct fpreg *regs)
{

	panic("set_fpregs");
}

int
fill_dbregs(struct thread *td, struct dbreg *regs)
{

	panic("fill_dbregs");
}

int
set_dbregs(struct thread *td, struct dbreg *regs)
{

	panic("set_dbregs");
}

void
DELAY(int delay)
{

	panic("DELAY");
}

int
ptrace_set_pc(struct thread *td, u_long addr)
{

	panic("ptrace_set_pc");
	return (0);
}

int
ptrace_single_step(struct thread *td)
{

	/* TODO; */
	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{

	/* TODO; */
	return (0);
}

void
exec_setregs(struct thread *td, struct image_params *imgp, u_long stack)
{

	panic("exec_setregs");
}

int
get_mcontext(struct thread *td, mcontext_t *mcp, int clear_ret)
{

	panic("get_mcontext");
}

int
set_mcontext(struct thread *td, const mcontext_t *mcp)
{

	panic("set_mcontext");
}

void
cpu_idle(int busy)
{

	/* Insert code to halt (until next interrupt) for the idle loop. */
}

void
cpu_halt(void)
{

	panic("cpu_halt");
}

/*
 * Flush the D-cache for non-DMA I/O so that the I-cache can
 * be made coherent later.
 */
void
cpu_flush_dcache(void *ptr, size_t len)
{

	/* TBD */
}

/* Get current clock frequency for the given CPU ID. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{

	panic("cpu_est_clockrate");
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{

	panic("cpu_pcpu_init");
}

/* TODO: Move to swtch.S and implemenet */
void cpu_throw(struct thread *old, struct thread *new)
{

	panic("cpu_throw");
}

void cpu_switch(struct thread *old, struct thread *new, struct mtx *mtx)
{

	panic("cpu_switch");
}

void
spinlock_enter(void)
{

	panic("spinlock_enter");
}

void
spinlock_exit(void)
{

	panic("spinlock_exit");
}

#ifndef	_SYS_SYSPROTO_H_
struct sigreturn_args {
	ucontext_t *ucp;
};
#endif

int
sys_sigreturn(struct thread *td, struct sigreturn_args *uap)
{

	panic("sys_sigreturn");
}

/*
 * Construct a PCB from a trapframe. This is called from kdb_trap() where
 * we want to start a backtrace from the function that caused us to enter
 * the debugger. We have the context in the trapframe, but base the trace
 * on the PCB. The PCB doesn't have to be perfect, as long as it contains
 * enough for a backtrace.
 */
void
makectx(struct trapframe *tf, struct pcb *pcb)
{

	panic("makectx");
}

void
sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{

	panic("sendsig");
}

void initarm(struct bootinfo *);

#ifdef EARLY_PRINTF
static void 
foundation_early_putc(int c)
{
	volatile uint32_t *uart = (uint32_t*)0x1c090000;

	/* TODO: Wait for space in the fifo */
	uart[0] = c;
}

early_putc_t *early_putc = foundation_early_putc;
#endif

typedef struct {
	uint32_t type;
	uint64_t phys_start;
	uint64_t virt_start;
	uint64_t num_pages;
	uint64_t attr;
} EFI_MEMORY_DESCRIPTOR;

void
initarm(struct bootinfo *bi)
{
	EFI_MEMORY_DESCRIPTOR *desc;
	const char str[] = "FreeBSD\r\n";
	volatile uint32_t *uart;
	int i;

	uart = (uint32_t*)0x1c090000;
	for (i = 0; i < sizeof(str); i++) {
		*uart = str[i];
	}

	printf("In initarm on arm64 %p\n", bi);
	printf("%llx\n", bi->bi_memmap);
	printf("%llx\n", bi->bi_memmap_size);
	printf("%llx\n", bi->bi_memdesc_size);
	printf("%llx\n", bi->bi_memdesc_version);

	desc = (void *)bi->bi_memmap;
	for (i = 0; i < bi->bi_memmap_size / bi->bi_memdesc_size; i++) {
		printf("%x %llx %llx %llx %llx\n", desc->type,
		    desc->phys_start, desc->virt_start, desc->num_pages,
		    desc->attr);

		desc = (void *)((uint8_t *)desc + bi->bi_memdesc_size);
	}
}

