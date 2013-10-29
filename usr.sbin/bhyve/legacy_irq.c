/*-
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <stdbool.h>
#include <assert.h>

/*
 * Used to keep track of legacy interrupt owners/requestors
 */
#define NLIRQ		16

static struct lirqinfo {
	bool	li_generic;
	bool	li_allocated;
} lirq[NLIRQ];

void
legacy_irq_init(void)
{

	/*
	 * Allow ISA IRQs 5,10,11,12, and 15 to be available for generic use.
	 */
	lirq[5].li_generic = true;
	lirq[10].li_generic = true;
	lirq[11].li_generic = true;
	lirq[12].li_generic = true;
	lirq[15].li_generic = true;
}

int
legacy_irq_alloc(int irq)
{
	int i;

	assert(irq < NLIRQ);

	if (irq < 0) {
		for (i = 0; i < NLIRQ; i++) {
			if (lirq[i].li_generic && !lirq[i].li_allocated) {
				irq = i;
				break;
			}
		}
	} else {
		if (lirq[irq].li_allocated)
			irq = -1;
	}

	if (irq >= 0) {
		lirq[irq].li_allocated = true;
		return (irq);
	} else
		return (-1);
}
