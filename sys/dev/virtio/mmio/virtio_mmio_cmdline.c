/*-
 * Copyright (c) 2022 Colin Percival
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/virtio/mmio/virtio_mmio.h>

/* Parse <size>@<baseaddr>:<irq>[:<id>] and add a child. */
static void
parsearg(driver_t *driver, device_t parent, char * arg)
{
	device_t child;
	char * p;
	unsigned long sz;
	unsigned long baseaddr;
	unsigned long irq;
	unsigned long id;

	/* <size> */
	sz = strtoul(arg, &p, 0);
	if ((sz == 0) || (sz == ULONG_MAX))
		goto bad;
	switch (*p) {
	case 'E': case 'e':
		sz <<= 10;
		/* FALLTHROUGH */
	case 'P': case 'p':
		sz <<= 10;
		/* FALLTHROUGH */
	case 'T': case 't':
		sz <<= 10;
		/* FALLTHROUGH */
	case 'G': case 'g':
		sz <<= 10;
		/* FALLTHROUGH */
	case 'M': case 'm':
		sz <<= 10;
		/* FALLTHROUGH */
	case 'K': case 'k':
		sz <<= 10;
		p++;
		break;
	}

	/* @<baseaddr> */
	if (*p++ != '@')
		goto bad;
	baseaddr = strtoul(p, &p, 0);
	if ((baseaddr == 0) || (baseaddr == ULONG_MAX))
		goto bad;

	/* :<irq> */
	if (*p++ != ':')
		goto bad;
	irq = strtoul(p, &p, 0);
	if ((irq == 0) || (irq == ULONG_MAX))
		goto bad;

	/* Optionally, :<id> */
	if (*p) {
		if (*p++ != ':')
			goto bad;
		id = strtoul(p, &p, 0);
		if ((id == 0) || (id == ULONG_MAX))
			goto bad;
	} else {
		id = 0;
	}

	/* Should have reached the end of the string. */
	if (*p)
		goto bad;

	/* Create the child and assign its resources. */
	child = BUS_ADD_CHILD(parent, 0, driver->name, id ? id : -1);
	bus_set_resource(child, SYS_RES_MEMORY, 0, baseaddr, sz);
	bus_set_resource(child, SYS_RES_IRQ, 0, irq, 1);
	device_set_driver(child, driver);

	return;

bad:
	printf("Error parsing virtio_mmio parameter: %s\n", arg);
}

static void
vtmmio_cmdline_identify(driver_t *driver, device_t parent)
{
	size_t n;
	char name[] = "virtio_mmio.device_XXXX";
	char * val;

	/* First variable just has its own name. */
	if ((val = kern_getenv("virtio_mmio.device")) == NULL)
		return;
	parsearg(driver, parent, val);
	freeenv(val);

	/* The rest have _%zu suffixes. */
	for (n = 1; n <= 9999; n++) {
		sprintf(name, "virtio_mmio.device_%zu", n);
		if ((val = kern_getenv(name)) == NULL)
			return;
		parsearg(driver, parent, val);
		freeenv(val);
	}
}

static device_method_t vtmmio_cmdline_methods[] = {
        /* Device interface. */
	DEVMETHOD(device_identify,	vtmmio_cmdline_identify),
	DEVMETHOD(device_probe,		vtmmio_probe),

        DEVMETHOD_END
};
DEFINE_CLASS_1(virtio_mmio, vtmmio_cmdline_driver, vtmmio_cmdline_methods,
    sizeof(struct vtmmio_softc), vtmmio_driver);
DRIVER_MODULE(vtmmio_cmdline, nexus, vtmmio_cmdline_driver, 0, 0);
