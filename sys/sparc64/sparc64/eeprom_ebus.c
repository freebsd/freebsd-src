/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1996 Paul Kranenburg
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Paul Kranenburg.
 *	This product includes software developed by Harvard University.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)clock.c	8.1 (Berkeley) 6/11/93
 *	from: NetBSD: clock.c,v 1.41 2001/07/24 19:29:25 eeh Exp
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/resource.h>

#include <dev/ofw/ofw_bus.h>

#include <machine/bus.h>
#include <machine/idprom.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <machine/eeprom.h>

#include <dev/mk48txx/mk48txxreg.h>

#include "clock_if.h"

/*
 * clock (eeprom) attaches at the sbus or the ebus
 */

static int eeprom_ebus_attach(device_t);

static device_method_t eeprom_ebus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		eeprom_probe),
	DEVMETHOD(device_attach,	eeprom_ebus_attach),

	/* clock interface */
	DEVMETHOD(clock_gettime,	mk48txx_gettime),
	DEVMETHOD(clock_settime,	mk48txx_settime),

	{ 0, 0 }
};

static driver_t eeprom_ebus_driver = {
	"eeprom",
	eeprom_ebus_methods,
	0,
};

DRIVER_MODULE(eeprom, ebus, eeprom_ebus_driver, eeprom_devclass, 0, 0);

/*
 * Attach a clock (really `eeprom') to the ebus.
 *
 * This is mapped read-only on NetBSD for safety, but this is not possible
 * with the current FreeBSD bus code.
 *
 * the MK48T02 is 2K.  the MK48T08 is 8K, and the MK48T59 is supposed to be
 * identical to it.
 */
static int
eeprom_ebus_attach(device_t dev)
{
	struct resource *res;
	int rid, error;

	rid = 0;
	res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}
	error = eeprom_attach(dev, rman_get_bustag(res),
	    rman_get_bushandle(res));
	return (error);
}
