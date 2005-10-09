/*-
 * Copyright 2002 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * PSIM local bus 16550
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/tty.h>
#include <machine/bus.h>
#include <sys/timepps.h>

#include <dev/ofw/openfirm.h>
#include <powerpc/psim/iobusvar.h>

#include <dev/sio/sioreg.h>
#include <dev/sio/siovar.h>

#include <isa/isavar.h>  /* for isa_irq_pending() prototype */

static  int     sio_iobus_attach(device_t dev);
static  int     sio_iobus_probe(device_t dev);

static device_method_t sio_iobus_methods[] = {
        /* Device interface */
	DEVMETHOD(device_probe,         sio_iobus_probe),
	DEVMETHOD(device_attach,        sio_iobus_attach),

	{ 0, 0 }
};

static driver_t sio_iobus_driver = {
	sio_driver_name,
	sio_iobus_methods,
	0,
};

static int
sio_iobus_attach(device_t dev)
{
	return (sioattach(dev, 0, DEFAULT_RCLK));
}

static int
sio_iobus_probe(device_t dev)
{
	char *type = iobus_get_name(dev);

	if (strncmp(type, "com", 3) != 0)
		return (ENXIO);


	device_set_desc(dev, "PSIM serial port");

	/*
	 * Call sioprobe with noprobe=1, to avoid hitting a psim bug
	 */
	return (sioprobe(dev, 0, 0, 1));
}

DRIVER_MODULE(sio, iobus, sio_iobus_driver, sio_devclass, 0, 0);

/*
 * Stub function. Perhaps a way to get this to work correctly would
 * be for child devices to set a field in the dev structure to
 * inform the parent that they are isa devices, and then use a
 * intr_pending() call which would propagate up to nexus to see
 * if the interrupt controller had any intrs in the isa group set
 */
intrmask_t
isa_irq_pending(void)
{
	return (0);
}
