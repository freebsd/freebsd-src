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
 *	$Id: md_var.h,v 1.4 1995/09/03 05:43:25 julian Exp $
 */

#ifndef _MACHINE_MD_VAR_H_
#define	_MACHINE_MD_VAR_H_

/*
 * Miscellaneous machine-dependent declarations.
 */

extern	int	Maxmem;
extern	u_long	cpu_feature;
extern	u_long	cpu_high;
extern	u_long	cpu_id;
extern	char	cpu_vendor[];
extern	char	etext[];
extern	vm_offset_t isaphysmem;
extern	char	kstack[];
extern	void	(*netisrs[32]) __P((void));
extern	int	nfs_diskless_valid;
extern	int	sigcode;
extern	int	szsigcode;

struct	proc;
struct	reg;

void	cpu_reset __P((void));
void	doreti_iret __P((void)) __asm(__STRING(doreti_iret));
void	doreti_iret_fault __P((void)) __asm(__STRING(doreti_iret_fault));
void	doreti_popl_ds __P((void)) __asm(__STRING(doreti_popl_ds));
void	doreti_popl_ds_fault __P((void)) __asm(__STRING(doreti_popl_ds_fault));
void	doreti_popl_es __P((void)) __asm(__STRING(doreti_popl_es));
void	doreti_popl_es_fault __P((void)) __asm(__STRING(doreti_popl_es_fault));
int	fill_regs __P((struct proc *p, struct reg *regs));
int	mvesp __P((void));
void	userconfig __P((void));
void	vm_bounce_init __P((void));
int	vm_page_zero_idle __P((void));

#endif /* !_MACHINE_MD_VAR_H_ */
