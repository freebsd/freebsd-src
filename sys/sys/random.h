/*
 * random.h -- A strong random number generator
 *
 * $Id: random.h,v 1.6 1996/06/17 16:47:39 bde Exp $
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

#ifndef _MACHINE_RANDOM_H_
#define	_MACHINE_RANDOM_H_

#include <sys/ioccom.h>

#define	MEM_SETIRQ	_IOW('r', 1, u_int16_t)	/* set interrupt */
#define	MEM_CLEARIRQ	_IOW('r', 2, u_int16_t)	/* clear interrupt */
#define	MEM_RETURNIRQ	_IOR('r', 3, u_int16_t)	/* return interrupt */

#ifdef KERNEL

/* XXX include from the wrong place(s) for inthand2_t. */
#include <i386/isa/isa_device.h>

/* Interrupts to be used in the randomizing process */

extern inthand2_t *sec_intr_handler[];
extern int sec_intr_unit[];

/* Exported functions */

void rand_initialize(void);
void add_keyboard_randomness(u_char scancode);
void add_interrupt_randomness(int irq);
#ifdef notused
void add_blkdev_randomness(int major);
#endif

#ifdef notused
void get_random_bytes(void *buf, u_int nbytes);
#endif
u_int read_random(char *buf, u_int size);
u_int read_random_unlimited(char *buf, u_int size);
#ifdef notused
u_int write_random(const char *buf, u_int nbytes);
#endif

#endif /* KERNEL */

#endif /* !_MACHINE_RANDOM_H_ */
