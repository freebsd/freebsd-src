/* $FreeBSD$ */
/* From: NetBSD: cpu.h,v 1.18 1997/09/23 23:17:49 mjacob Exp */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * from: Utah $Hdr: cpu.h 1.16 91/03/25$
 *
 *	@(#)cpu.h	8.4 (Berkeley) 1/5/94
 */

#ifndef _MACHINE_CPU_H_
#define _MACHINE_CPU_H_

#include <machine/frame.h>

/*
 * Arguments to hardclock and gatherstats encapsulate the previous machine
 * state in an opaque clockframe.
 */
struct clockframe {
	struct trapframe cf_tf;
};
#define	CLKF_PC(cf)		((cf)->cf_tf.tf_special.iip)
#define	CLKF_USERMODE(cf)	((CLKF_PC(cf) >> 61) < 5)

/* Used by signaling code. */
#define	cpu_getstack(td)	((td)->td_frame->tf_special.sp)

/* XXX */
#define	TRAPF_PC(tf)		((tf)->tf_special.iip)
#define	TRAPF_USERMODE(tf)	((TRAPF_PC(tf) >> 61) < 5)

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_ROOT_DEVICE		2	/* string: root device name */
#define	CPU_BOOTED_KERNEL	3	/* string: booted kernel name */
#define	CPU_ADJKERNTZ		4	/* int:	timezone offset	(seconds) */
#define	CPU_DISRTCSET		5	/* int: disable resettodr() call */
#define	CPU_WALLCLOCK		6	/* int:	indicates wall CMOS clock */
#define	CPU_MAXID		7	/* valid machdep IDs */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
	{ "root_device", CTLTYPE_STRING }, \
	{ "booted_kernel", CTLTYPE_STRING }, \
	{ "adjkerntz", CTLTYPE_INT }, \
	{ "disable_rtc_set", CTLTYPE_INT }, \
	{ "wall_cmos_clock", CTLTYPE_INT }, \
}

#ifdef _KERNEL

struct pcb;
struct thread;
struct reg;
struct rpb;
struct trapframe;

extern struct rpb *hwrpb;
extern volatile int mc_expected, mc_received;

int	badaddr(void *, size_t);
int	badaddr_read(void *, size_t, void *);
u_int64_t console_restart(u_int64_t, u_int64_t, u_int64_t);
int	do_ast(struct trapframe *);
void	dumpconf(void);
void	frametoreg(struct trapframe *, struct reg *);
long	fswintrberr(void);				/* MAGIC */
int	ia64_highfp_drop(struct thread *);
int	ia64_highfp_load(struct thread *);
int	ia64_highfp_save(struct thread *);
void	ia64_init(void);
int	ia64_pa_access(u_long);
void	init_prom_interface(struct rpb*);
void	interrupt(u_int64_t, struct trapframe *);
void	machine_check(unsigned long, struct trapframe *, unsigned long,
    unsigned long);
u_int64_t hwrpb_checksum(void);
void	hwrpb_restart_setup(void);
void	regdump(struct trapframe *);
void	regtoframe(struct reg *, struct trapframe *);
void	set_iointr(void (*)(void *, unsigned long));
void	fork_trampoline(void);				/* MAGIC */
int	syscall(struct trapframe *);
void	trap(int vector, struct trapframe *framep);
void	ia64_probe_sapics(void);
int	ia64_count_cpus(void);
void	map_gateway_page(void);
void	map_pal_code(void);
void	map_port_space(void);
void	cpu_mp_add(uint, uint, uint);

/*
 * Return contents of in-cpu fast counter as a sort of "bogo-time"
 * for non-critical timing.
 */
static __inline u_int64_t
get_cyclecount(void)
{
	return (ia64_get_itc());
}

#endif /* _KERNEL */

#endif /* _MACHINE_CPU_H_ */
