/* $Id$ */
/* $NetBSD: mcclock_tlsb.c,v 1.8 1998/05/13 02:50:29 thorpej Exp $ */

/*
 * Copyright (c) 1997 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/clockvar.h>
#include <dev/dec/mcclockvar.h>

#include <dev/dec/mc146818reg.h>

static int	mcclock_isa_probe(device_t dev);
static int	mcclock_isa_attach(device_t dev);
static void	mcclock_isa_write(device_t, u_int, u_int);
static u_int	mcclock_isa_read(device_t, u_int);

static device_method_t mcclock_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mcclock_isa_probe),
	DEVMETHOD(device_attach,	mcclock_isa_attach),

	/* mcclock interface */
	DEVMETHOD(mcclock_write,	mcclock_isa_write),
	DEVMETHOD(mcclock_read,		mcclock_isa_read),

	/* clock interface */
	DEVMETHOD(clock_init,		mcclock_init),
	DEVMETHOD(clock_get,		mcclock_get),
	DEVMETHOD(clock_set,		mcclock_set),

	{ 0, 0 }
};

static driver_t mcclock_isa_driver = {
	"mcclock",
	mcclock_isa_methods,
	DRIVER_TYPE_MISC,
	1,			/* XXX no softc */
};

static devclass_t mcclock_devclass;

int
mcclock_isa_probe(device_t dev)
{
	device_set_desc(dev, "MC146818A real time clock");
	return 0;
}

int
mcclock_isa_attach(device_t dev)
{
	mcclock_attach(dev);
	return 0;
}

static void
mcclock_isa_write(device_t dev, u_int reg, u_int val)
{
	outb(0x70, reg);
	outb(0x71, val);
}

static u_int
mcclock_isa_read(device_t dev, u_int reg)
{
	outb(0x70, reg);
	return inb(0x71);
}

/* XXX put it on the root for now, later on isa */
DRIVER_MODULE(mcclock_isa, root, mcclock_isa_driver, mcclock_devclass, 0, 0);
