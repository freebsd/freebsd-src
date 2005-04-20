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
#include <sys/resource.h>

#include <dev/ofw/ofw_bus.h>

#include <machine/bus.h>
#include <machine/idprom.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <machine/eeprom.h>

#include <dev/mk48txx/mk48txxreg.h>

#include "clock_if.h"

devclass_t eeprom_devclass;

#define IDPROM_OFFSET (8 * 1024 - 40)	/* XXX - get nvram size from driver */

int
eeprom_probe(device_t dev)
{
 
	if (strcmp("eeprom", ofw_bus_get_name(dev)) == 0) {
		device_set_desc(dev, "EEPROM/clock");
		return (0);
	}
	return (ENXIO);
}

int
eeprom_attach(device_t dev, bus_space_tag_t bt, bus_space_handle_t bh)
{
	struct timespec ts;
	struct idprom *idp;
	const char *model;
	int error, i;
	u_int32_t h;

	if ((model = ofw_bus_get_model(dev)) == NULL)
		panic("eeprom_attach: no model property");

	/* Our TOD clock year 0 is 1968 */
	if ((error = mk48txx_attach(dev, bt, bh, model, 1968)) != 0) {
		device_printf(dev, "Can't attach %s tod clock", model);
		return (error);
	}
	/* XXX: register clock device */

	/* Get the host ID from the prom. */
	idp = (struct idprom *)((u_long)bh + IDPROM_OFFSET);
	h = bus_space_read_1(bt, bh, IDPROM_OFFSET +
	    offsetof(struct idprom, id_machine)) << 24;
	for (i = 0; i < 3; i++) {
		h |= bus_space_read_1(bt, bh, IDPROM_OFFSET +
		    offsetof(struct idprom, id_hostid[i])) << ((2 - i) * 8);
	}
	/* XXX: register host id */
	device_printf(dev, "hostid %x\n", (u_int)h);
	if (bootverbose) {
		mk48txx_gettime(dev, &ts);
		device_printf(dev, "current time: %ld.%09ld\n", (long)ts.tv_sec,
		    ts.tv_nsec);
	}

	return (0);
}
