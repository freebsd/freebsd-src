/*-
 * Copyright (c) 1997 Jonathan Lemon
 * All rights reserved.
 *
 * Derived from register.h, which is
 *     Copyright (c) 1996 Michael Smith.  All rights reserved.
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
 *	$Id: vm86.h,v 1.2 1997/08/20 19:57:24 jlemon Exp $
 */

#ifndef _MACHINE_VM86_H_
#define _MACHINE_VM86_H_ 1

struct vm86_kernel {
	caddr_t	vm86_intmap;			/* interrupt map */
        u_long	vm86_eflags;			/* emulated flags */
	int	vm86_has_vme;			/* VME support */
	int	vm86_inited;			/* we were initialized */
	int	vm86_debug;
};

struct i386_vm86_args {
	int	sub_op;			/* sub-operation to perform */
	char 	*sub_args;		/* args */
};

#define VM86_INIT	1
#define VM86_SET_VME	2
#define VM86_GET_VME	3

struct vm86_init_args {
        int     debug;                  /* debug flag */
        int     cpu_type;               /* cpu type to emulate */
        u_char  int_map[32];            /* interrupt map */ 
};

struct vm86_vme_args {
	int	state;			/* status */
};

/* standard register representation */
typedef union {
	u_long	r_ex;
	struct {
		u_short	r_x;
		u_short	:16;
	} r_w;
	struct {
		u_char	r_l;
		u_char	r_h;
		u_short	:16;
	} r_b;
} reg86_t;

/* layout must match definition of struct trapframe_vm86 in <machine/frame.h> */

struct vm86frame {
	int	:32;			/* kernel ES */
	int	:32;			/* kernel DS */
	reg86_t	edi;
	reg86_t	esi;
	reg86_t	ebp;
	reg86_t	isp;
	reg86_t	ebx;
	reg86_t	edx;
	reg86_t	ecx;
	reg86_t	eax;
	int	:32;			/* trapno */
	int	:32;			/* err */
	reg86_t	eip;
	reg86_t	cs;
	reg86_t	eflags;
	reg86_t	esp;
	reg86_t	ss;
	reg86_t	es;
	reg86_t	ds;
	reg86_t	fs;
	reg86_t	gs;
#define vmf_cs		cs.r_w.r_x
#define vmf_ss		ss.r_w.r_x
#define vmf_sp		esp.r_w.r_x
#define vmf_ip		eip.r_w.r_x
#define vmf_flags	eflags.r_w.r_x
#define vmf_eflags	eflags.r_ex
};

struct proc;
extern	int vm86_emulate __P((struct vm86frame *));
extern	int vm86_sysarch __P((struct proc *, char *, int *));

#endif /* _MACHINE_VM86_H_ */
