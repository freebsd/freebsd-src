/*
 * random.h -- A strong random number generator
 *
 * $FreeBSD$
 *
 * Version 0.95, last modified 18-Oct-95
 * 
 * Copyright Theodore Ts'o, 1994, 1995.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Many kernel routines will have a use for good random numbers,
 * for example, for truely random TCP sequence numbers, which prevent
 * certain forms of TCP spoofing attacks.
 * 
 */

#ifndef	_SYS_RANDOM_H_
#define	_SYS_RANDOM_H_

#include <sys/ioccom.h>

#define	MEM_SETIRQ	_IOW('r', 1, u_int16_t)	/* set interrupt */
#define	MEM_CLEARIRQ	_IOW('r', 2, u_int16_t)	/* clear interrupt */
#define	MEM_RETURNIRQ	_IOR('r', 3, u_int16_t)	/* return interrupt */

#ifdef _KERNEL

/* Type of the cookie passed to add_interrupt_randomness. */

struct random_softc {
	inthand2_t	*sc_handler;
	void		*sc_arg;
	int		sc_intr;
};

/* Exported functions */

void rand_initialize(void);
void add_keyboard_randomness(u_char scancode);
inthand2_t add_interrupt_randomness;
#ifdef notused
void add_blkdev_randomness(int major);
#endif
void add_true_randomness(int);

#ifdef notused
void get_random_bytes(void *buf, u_int nbytes);
#endif
u_int read_random(void *buf, u_int size);
u_int read_random_unlimited(void *buf, u_int size);
#ifdef notused
u_int write_random(const char *buf, u_int nbytes);
#endif
struct proc;
int random_poll(dev_t dev, int events, struct proc *p);

#endif /* _KERNEL */

#endif /* !_SYS_RANDOM_H_ */
