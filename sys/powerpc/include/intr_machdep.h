/*-
 * Copyright (C) 2002 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_MACHINE_INTR_MACHDEP_H_
#define	_MACHINE_INTR_MACHDEP_H_

typedef void	ih_func_t(void *);

struct intr_event;

struct ppc_intr_handler {
	ih_func_t	*ih_func;
	void		*ih_arg;
	struct		intr_event *ih_event;
	u_int		ih_irq;
	u_int		ih_flags;
	u_int 		ih_index;
	u_long 		*ih_count;
	u_long 		*ih_straycount;
};

/* XXX temporary. */
void	ext_intr_install(void (*new_extint)(void));

void	intr_init(void (*)(void), int, void (*)(uintptr_t), void (*)(uintptr_t));
void	intr_setup(u_int, ih_func_t *, void *, u_int);
int	inthand_add(const char *, u_int, driver_filter_t *filter, 
	void (*)(void *), void *, int, void **);
int	inthand_remove(u_int, void *);
void	intr_handle(u_int);

#endif /* _MACHINE_INTR_MACHDEP_H_ */
