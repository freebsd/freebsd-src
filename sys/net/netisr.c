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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>

#include <net/netisr.h>

static void swi_net(void *);

void *net_ih;
volatile unsigned int netisr;
void (*netisrs[32])(void);

void
legacy_setsoftnet(void)
{
	swi_sched(net_ih, 0);
}

int
register_netisr(int num, netisr_t *handler)
{
	
	if (num < 0 || num >= (sizeof(netisrs)/sizeof(*netisrs)) ) {
		printf("register_netisr: bad isr number: %d\n", num);
		return (EINVAL);
	}
	netisrs[num] = handler;
	return (0);
}

int
unregister_netisr(int num)
{
	
	if (num < 0 || num >= (sizeof(netisrs)/sizeof(*netisrs)) ) {
		printf("unregister_netisr: bad isr number: %d\n", num);
		return (EINVAL);
	}
	netisrs[num] = NULL;
	return (0);
}

#ifdef DEVICE_POLLING
void netisr_pollmore(void);
#endif

static void
swi_net(void *dummy)
{
	u_int bits;
	int i;

#ifdef DEVICE_POLLING
    for (;;) {
	int pollmore;
#endif
	bits = atomic_readandclear_int(&netisr);
#ifdef DEVICE_POLLING
	if (bits == 0)
		return;
	pollmore = bits & (1 << NETISR_POLL);
#endif
	while ((i = ffs(bits)) != 0) {
		i--;
		if (netisrs[i] != NULL)
			netisrs[i]();
		else
			printf("swi_net: unregistered isr number: %d.\n", i);
		bits &= ~(1 << i);
	}
#ifdef DEVICE_POLLING
	if (pollmore)
		netisr_pollmore();
    }
#endif
}

static void
start_netisr(void *dummy)
{

	if (swi_add(NULL, "net", swi_net, NULL, SWI_NET, 0, &net_ih))
		panic("start_netisr");
}
SYSINIT(start_netisr, SI_SUB_SOFTINTR, SI_ORDER_FIRST, start_netisr, NULL)
