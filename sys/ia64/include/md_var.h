/*-
 * Copyright (c) 1998 Doug Rabson
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
 * $FreeBSD$
 */

#ifndef _MACHINE_MD_VAR_H_
#define	_MACHINE_MD_VAR_H_

/*
 * Miscellaneous machine-dependent declarations.
 */

extern	char	sigcode[];
extern	char	esigcode[];
extern	int	szsigcode;
extern	long	Maxmem;

struct fpreg;
struct reg;
struct thread;
struct trapframe;

struct ia64_fdesc {
	u_int64_t	func;
	u_int64_t	gp;
};

#define FDESC_FUNC(fn)  (((struct ia64_fdesc *) fn)->func)
#define FDESC_GP(fn)    (((struct ia64_fdesc *) fn)->gp)

void	busdma_swi(void);
int	copyout_regstack(struct thread *, uint64_t *, uint64_t *);
void	cpu_mp_add(u_int, u_int, u_int);
int	do_ast(struct trapframe *);
int	ia64_count_cpus(void);
int	ia64_highfp_drop(struct thread *);
int	ia64_highfp_load(struct thread *);
int	ia64_highfp_save(struct thread *);
void	ia64_init(void);
void	ia64_probe_sapics(void);
int	interrupt(uint64_t, struct trapframe *);
void	map_gateway_page(void);
void	map_pal_code(void);
void	map_port_space(void);
void	os_boot_rendez(void);
void	os_mca(void);
void	spillfd(void *src, void *dst);
int	syscall(struct trapframe *);
void	trap(int, struct trapframe *);
int	unaligned_fixup(struct trapframe *, struct thread *);

#endif /* !_MACHINE_MD_VAR_H_ */
