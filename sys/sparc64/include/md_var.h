/*-
 * Copyright (c) 1995 Bruce D. Evans.
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
 * 3. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	from: FreeBSD: src/sys/i386/include/md_var.h,v 1.40 2001/07/12
 * $FreeBSD$
 */

#ifndef	_MACHINE_MD_VAR_H_
#define	_MACHINE_MD_VAR_H_

extern	char	sigcode[];
extern	int	szsigcode;
extern	char	tl0_base[];
extern	char	_end[];

extern	int	Maxmem;

extern	vm_offset_t kstack0;
extern	vm_offset_t kstack0_phys;

struct	dbreg;
struct	fpreg;
struct	thread;
struct	reg;
struct	pcpu;

void	cpu_halt(void);
void	cpu_identify(u_int clock);
void	cpu_reset(void);
void	cpu_setregs(struct pcpu *pc);
int	is_physical_memory(vm_offset_t addr);
void	swi_vm(void *v);

#endif /* !_MACHINE_MD_VAR_H_ */
