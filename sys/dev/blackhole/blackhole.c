/*-
 * Copyright (c) 2011 NetApp, Inc.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/linker.h>
#include <sys/libkern.h>

#include <dev/pci/pcivar.h>

static int
linker_file_iterator(linker_file_t lf, void *arg)
{
	const char *file = arg;

	if (strcmp(lf->filename, file) == 0)
		return (1);
	else
		return (0);
}

static boolean_t
pptdev(int bus, int slot, int func)
{
	int found, b, s, f, n;
	char *val, *cp, *cp2;

	/*
	 * setenv pptdevs "1/2/3 4/5/6 7/8/9 10/11/12"
	 */
	found = 0;
	cp = val = getenv("pptdevs");
	while (cp != NULL && *cp != '\0') {
		if ((cp2 = strchr(cp, ' ')) != NULL)
			*cp2 = '\0';

		n = sscanf(cp, "%d/%d/%d", &b, &s, &f);
		if (n == 3 && bus == b && slot == s && func == f) {
			found = 1;
			break;
		}
		
		if (cp2 != NULL)
			*cp2++ = ' ';

		cp = cp2;
	}
	freeenv(val);
	return (found);
}

static int
pci_blackhole_probe(device_t dev)
{
	int bus, slot, func;

	/*
	 * If 'vmm.ko' has also been loaded the don't try to claim
	 * any pci devices.
	 */
	if (linker_file_foreach(linker_file_iterator, "vmm.ko"))
		return (ENXIO);

	bus = pci_get_bus(dev);
	slot = pci_get_slot(dev);
	func = pci_get_function(dev);
	if (pptdev(bus, slot, func))
		return (0);
	else
		return (ENXIO);
}

static int
pci_blackhole_attach(device_t dev)
{
	/*
	 * We never really want to claim the devices but just want to prevent
	 * other drivers from getting to them.
	 */
	return (ENXIO);
}

static device_method_t pci_blackhole_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,         pci_blackhole_probe),
        DEVMETHOD(device_attach,        pci_blackhole_attach),

	{ 0, 0 }
};

static driver_t pci_blackhole_driver = {
        "blackhole",
        pci_blackhole_methods,
};

devclass_t blackhole_devclass;

DRIVER_MODULE(blackhole, pci, pci_blackhole_driver, blackhole_devclass, 0, 0);
MODULE_DEPEND(blackhole, pci, 1, 1, 1);
