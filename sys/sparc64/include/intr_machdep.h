/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#ifndef	_MACHINE_INTR_MACHDEP_H_
#define	_MACHINE_INTR_MACHDEP_H_

#define	NPIL		(1 << 4)
#define	NIV		(1 << 11)

#define	IQ_SIZE		(NPIL * 2)
#define	IQ_MASK		(IQ_SIZE - 1)

#define	IH_SHIFT	PTR_SHIFT
#define	IQE_SHIFT	5
#define	IV_SHIFT	5

#define	PIL_LOW		1	/* stray interrupts */
#define	PIL_ITHREAD	2	/* interrupts that use ithreads */
#define	PIL_FAST	13	/* fast interrupts */
#define	PIL_TICK	14

typedef	void ih_func_t(struct trapframe *);
typedef	void iv_func_t(void *);

struct iqe {
	u_int	iqe_tag;
	u_int	iqe_pri;
	u_long	iqe_vec;
	iv_func_t *iqe_func;
	void	*iqe_arg;
};

struct intr_queue {
	struct	iqe iq_queue[IQ_SIZE];	/* must be first */
	u_long	iq_head;
	u_long	iq_tail;
};

struct ithd;

struct intr_vector {
	iv_func_t *iv_func;
	void	*iv_arg;
	struct	ithd *iv_ithd;
	u_int	iv_pri;
	u_int	iv_vec;
};

extern ih_func_t *intr_handlers[];
extern struct intr_vector intr_vectors[];

void	intr_setup(int level, ih_func_t *ihf, int pri, iv_func_t *ivf,
		   void *iva);
void	intr_init(void);
int	inthand_add(const char *name, int vec, void (*handler)(void *),
    void *arg, int flags, void **cookiep);
int	inthand_remove(int vec, void *cookie);

ih_func_t intr_dequeue;

#endif
