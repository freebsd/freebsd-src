/*-
 * Copyright 2015 Justin Hibbits
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
 * From: FreeBSD: src/sys/powerpc/mpc85xx/pci_ocp.c,v 1.9 2010/03/23 23:46:28 marcel
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/endian.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include "pcib_if.h"

static int
fsl_pcib_rc_probe(device_t dev)
{
	if (pci_get_vendor(dev) != 0x1957)
		return (ENXIO);
	if (pci_get_progif(dev) != 0)
		return (ENXIO);
	if (pci_get_class(dev) != PCIC_PROCESSOR)
		return (ENXIO);
	if (pci_get_subclass(dev) != PCIS_PROCESSOR_POWERPC)
		return (ENXIO);

	return (BUS_PROBE_DEFAULT);
}

static int
fsl_pcib_rc_attach(device_t dev)
{
	struct pcib_softc *sc;
	device_t child;

	pcib_bridge_init(dev);
	pcib_attach_common(dev);

	sc = device_get_softc(dev);
	if (sc->bus.sec != 0) {
		child = device_add_child(dev, "pci", -1);
		if (child != NULL)
			return (bus_generic_attach(dev));
	}

	return (0);
}

static device_method_t fsl_pcib_rc_methods[] = {
	DEVMETHOD(device_probe,		fsl_pcib_rc_probe),
	DEVMETHOD(device_attach,	fsl_pcib_rc_attach),
	DEVMETHOD_END
};

static devclass_t fsl_pcib_rc_devclass;
DEFINE_CLASS_1(pcib, fsl_pcib_rc_driver, fsl_pcib_rc_methods,
    sizeof(struct pcib_softc), pcib_driver);
DRIVER_MODULE(rcpcib, pci, fsl_pcib_rc_driver, fsl_pcib_rc_devclass, 0, 0);

