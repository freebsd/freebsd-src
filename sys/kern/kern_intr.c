/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/kern/kern_intr.c,v 1.24 1999/10/11 15:19:09 peter Exp $
 *
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/ipl.h>

#include <sys/interrupt.h>

struct swilist {
	swihand_t	*sl_handler;
	struct swilist	*sl_next;
};

static struct swilist swilists[NSWI];

void
register_swi(intr, handler)
	int intr;
	swihand_t *handler;
{
	struct swilist *slp, *slq;
	int s;

	if (intr < NHWI || intr >= NHWI + NSWI)
		panic("register_swi: bad intr %d", intr);
	if (handler == swi_generic || handler == swi_null)
		panic("register_swi: bad handler %p", (void *)handler);
	slp = &swilists[intr - NHWI];
	s = splhigh();
	if (ihandlers[intr] == swi_null)
		ihandlers[intr] = handler;
	else {
		if (slp->sl_next == NULL) {
			slp->sl_handler = ihandlers[intr];
			ihandlers[intr] = swi_generic;
		}
		slq = malloc(sizeof(*slq), M_DEVBUF, M_NOWAIT);
		if (slq == NULL)
			panic("register_swi: malloc failed");
		slq->sl_handler = handler;
		slq->sl_next = NULL;
		while (slp->sl_next != NULL)
			slp = slp->sl_next;
		slp->sl_next = slq;
	}
	splx(s);
}

void
swi_dispatcher(intr)
	int intr;
{
	struct swilist *slp;

	slp = &swilists[intr - NHWI];
	do {
		(*slp->sl_handler)();
		slp = slp->sl_next;
	} while (slp != NULL);
}

void
unregister_swi(intr, handler)
	int intr;
	swihand_t *handler;
{
	struct swilist *slfoundpred, *slp, *slq;
	int s;

	if (intr < NHWI || intr >= NHWI + NSWI)
		panic("unregister_swi: bad intr %d", intr);
	if (handler == swi_generic || handler == swi_null)
		panic("unregister_swi: bad handler %p", (void *)handler);
	slp = &swilists[intr - NHWI];
	s = splhigh();
	if (ihandlers[intr] == handler)
		ihandlers[intr] = swi_null;
	else if (slp->sl_next != NULL) {
		slfoundpred = NULL;
		for (slq = slp->sl_next; slq != NULL;
		    slp = slq, slq = slp->sl_next)
			if (slq->sl_handler == handler)
				slfoundpred = slp;
		slp = &swilists[intr - NHWI];
		if (slfoundpred != NULL) {
			slq = slfoundpred->sl_next;
			slfoundpred->sl_next = slq->sl_next;
			free(slq, M_DEVBUF);
		} else if (slp->sl_handler == handler) {
			slq = slp->sl_next;
			slp->sl_next = slq->sl_next;
			slp->sl_handler = slq->sl_handler;
			free(slq, M_DEVBUF);
		}
		if (slp->sl_next == NULL)
			ihandlers[intr] = slp->sl_handler;
	}
	splx(s);
}

