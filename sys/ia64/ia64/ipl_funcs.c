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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/ktr.h>
#include <sys/interrupt.h>
#include <machine/ipl.h>
#include <machine/cpu.h>
#include <machine/globaldata.h>
#include <machine/globals.h>
#include <machine/mutex.h>
#include <net/netisr.h>

#include "sio.h"

unsigned int bio_imask;		/* XXX */
unsigned int cam_imask;		/* XXX */
unsigned int net_imask;		/* XXX */
unsigned int tty_imask;		/* XXX */

static void swi_net(void);

void	(*netisrs[32]) __P((void));
swihand_t *shandlers[32] = {	/* software interrupts */
	swi_null,	swi_net,	swi_null,	swi_null,
	swi_null,	swi_null,	softclock,	swi_null,
	swi_null,	swi_null,	swi_null,	swi_null,
	swi_null,	swi_null,	swi_null,	swi_null,
	swi_null,	swi_null,	swi_null,	swi_null,
	swi_null,	swi_null,	swi_null,	swi_null,
	swi_null,	swi_null,	swi_null,	swi_null,
	swi_null,	swi_null,	swi_null,	swi_null,
};

u_int32_t netisr;

void
swi_null()
{
    /* No interrupt registered, do nothing */
}

void
swi_generic()
{
    /* Just a placeholder, we call swi_dispatcher directly */
    panic("swi_generic() called");
}

static void
swi_net()
{
    u_int32_t bits = atomic_readandclear_32(&netisr);
    int i;

    for (i = 0; i < 32; i++) {
	if (bits & 1)
	    netisrs[i]();
	bits >>= 1;
    }
}
