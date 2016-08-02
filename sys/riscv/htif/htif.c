/*-
 * Copyright (c) 2015-2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/pcpu.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/asm.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

#include "htif.h"

static struct resource_spec htif_spec[] = {
	{ SYS_RES_IRQ,		0,	RF_ACTIVE | RF_SHAREABLE},
	{ -1, 0 }
};

struct intr_entry {
	void (*func) (void *, uint64_t);
	void *arg;
};

struct intr_entry intrs[HTIF_NDEV];

uint64_t
htif_command(uint64_t arg)
{

	return (machine_command(ECALL_HTIF_CMD, arg));
}

int
htif_setup_intr(int id, void *func, void *arg)
{

	if (id >= HTIF_NDEV)
		return (-1);

	intrs[id].func = func;
	intrs[id].arg = arg;

	return (0);
}

static void
htif_handle_entry(struct htif_softc *sc)
{
	uint64_t entry;
	uint8_t devcmd;
	uint8_t devid;

	entry = machine_command(ECALL_HTIF_GET_ENTRY, 0);
	while (entry) {
		devid = HTIF_DEV_ID(entry);
		devcmd = HTIF_DEV_CMD(entry);

		if (devcmd == HTIF_CMD_IDENTIFY) {
			/* Enumeration interrupt */
			if (devid == sc->identify_id)
				sc->identify_done = 1;
		} else {
			/* Device interrupt */
			if (intrs[devid].func != NULL)
				intrs[devid].func(intrs[devid].arg, entry);
		}

		entry = machine_command(ECALL_HTIF_GET_ENTRY, 0);
	}
}

static int
htif_intr(void *arg)
{
	struct htif_softc *sc;

	sc = arg;

	csr_clear(sip, SIP_SSIP);

	htif_handle_entry(sc);

	return (FILTER_HANDLED);
}

static int
htif_add_device(struct htif_softc *sc, int i, char *id, char *name)
{
	struct htif_dev_ivars *di;

	di = malloc(sizeof(struct htif_dev_ivars), M_DEVBUF, M_WAITOK | M_ZERO);
	di->sc = sc;
	di->index = i;
	di->id = malloc(HTIF_ID_LEN, M_DEVBUF, M_WAITOK | M_ZERO);
	memcpy(di->id, id, HTIF_ID_LEN);

	di->dev = device_add_child(sc->dev, name, -1);
	device_set_ivars(di->dev, di);

	return (0);
}

static int
htif_enumerate(struct htif_softc *sc)
{
	char id[HTIF_ID_LEN] __aligned(HTIF_ALIGN);
	uint64_t paddr;
	uint64_t data;
	uint64_t cmd;
	int len;
	int i;

	device_printf(sc->dev, "Enumerating devices\n");

	for (i = 0; i < HTIF_NDEV; i++) {
		paddr = pmap_kextract((vm_offset_t)&id);
		data = (paddr << IDENTIFY_PADDR_SHIFT);
		data |= IDENTIFY_IDENT;

		sc->identify_id = i;
		sc->identify_done = 0;

		cmd = i;
		cmd <<= HTIF_DEV_ID_SHIFT;
		cmd |= (HTIF_CMD_IDENTIFY << HTIF_CMD_SHIFT);
		cmd |= data;

		htif_command(cmd);

		len = strnlen(id, sizeof(id));
		if (len <= 0)
			break;

		if (bootverbose)
			printf(" %d %s\n", i, id);

		if (strncmp(id, "disk", 4) == 0)
			htif_add_device(sc, i, id, "htif_blk");
		else if (strncmp(id, "bcd", 3) == 0)
			htif_add_device(sc, i, id, "htif_console");
		else if (strncmp(id, "syscall_proxy", 13) == 0)
			htif_add_device(sc, i, id, "htif_syscall_proxy");
	}

	return (bus_generic_attach(sc->dev));
}

int
htif_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct htif_dev_ivars *ivars;

	ivars = device_get_ivars(child);

	switch (which) {
	case HTIF_IVAR_INDEX:
                *result = ivars->index;
		break;
	case HTIF_IVAR_ID:
		*result = (uintptr_t)ivars->id;
	default:
		return (EINVAL);
	}

	return (0);
}

static int
htif_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "riscv,htif"))
		return (ENXIO);

	device_set_desc(dev, "HTIF bus device");
	return (BUS_PROBE_DEFAULT);
}

static int
htif_attach(device_t dev)
{
	struct htif_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, htif_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Setup IRQs handler */
	error = bus_setup_intr(dev, sc->res[0], INTR_TYPE_CLK,
	    htif_intr, NULL, sc, &sc->ihl[0]);
	if (error) {
		device_printf(dev, "Unable to alloc int resource.\n");
		return (ENXIO);
	}

	csr_set(sie, SIE_SSIE);

	return (htif_enumerate(sc));
}

static device_method_t htif_methods[] = {
	DEVMETHOD(device_probe,		htif_probe),
	DEVMETHOD(device_attach,	htif_attach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	htif_read_ivar),

	DEVMETHOD_END
};

static driver_t htif_driver = {
	"htif",
	htif_methods,
	sizeof(struct htif_softc)
};

static devclass_t htif_devclass;

DRIVER_MODULE(htif, simplebus, htif_driver,
    htif_devclass, 0, 0);
