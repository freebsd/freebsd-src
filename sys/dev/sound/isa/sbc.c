/*-
 * Copyright (c) 1999 Seigo Tanimura
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

#include "sbc.h"
#include "isa.h"
#include "pnp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/soundcard.h>
#include <dev/sound/chip.h>

#if NISA > 0
#include <isa/isavar.h>
#include <isa/isa_common.h>
#ifdef __alpha__		/* XXX workaround a stupid warning */
#include <alpha/isa/isavar.h>
#endif
#endif /* NISA > 0 */

#if NSBC > 0

/* Here is the parameter structure per a device. */
struct sbc_softc {
	device_t dev; /* device */
	int io_rid[3]; /* io port rids */
	struct resource *io[3]; /* io port resources */
	int io_alloced[3]; /* io port alloc flag */
	int irq_rid; /* irq rids */
	struct resource *irq; /* irq resources */
	int irq_alloced; /* irq alloc flag */
	int drq_rid[2]; /* drq rids */
	struct resource *drq[2]; /* drq resources */
	int drq_alloced[2]; /* drq alloc flag */
};

typedef struct sbc_softc *sc_p;

#if NISA > 0 && NPNP > 0
static int sbc_probe(device_t dev);
static int sbc_attach(device_t dev);
#endif /* NISA > 0 && NPNP > 0 */
static struct resource *sbc_alloc_resource(device_t bus, device_t child, int type, int *rid,
					      u_long start, u_long end, u_long count, u_int flags);
static int sbc_release_resource(device_t bus, device_t child, int type, int rid,
				   struct resource *r);

static int alloc_resource(sc_p scp);
static int release_resource(sc_p scp);

static devclass_t sbc_devclass;

#if NISA > 0 && NPNP > 0
static struct isa_pnp_id sbc_ids[] = {
#if notdef
	{0x0000630e, "CS423x"},
#endif
	{0x01008c0e, "Creative ViBRA16C PnP"},
	{0x43008c0e, "Creative ViBRA16X PnP"},
	{0x31008c0e, "Creative SB16 PnP/SB32"},
	{0x42008c0e, "Creative SB AWE64"}, /* CTL00c1 */
	{0x45008c0e, "Creative SB AWE64"}, /* CTL0045 */
#if notdef
	{0x01200001, "Avance Logic ALS120"},
	{0x01100001, "Avance Asound 110"},
	{0x68187316, "ESS ES1868 Plug and Play AudioDrive"}, /* ESS1868 */
	{0x79187316, "ESS ES1879 Plug and Play AudioDrive"}, /* ESS1879 */
	{0x2100a865, "Yamaha OPL3-SA2/SAX Sound Board"},
	{0x80719304, "Terratec Soundsystem Base 1"},
#endif
	{0}
};

static int
sbc_probe(device_t dev)
{
	int error;

	/* Check pnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, sbc_ids);
	if (error)
		return error;
	else
		return -100;
}

static int
sbc_attach(device_t dev)
{
	sc_p scp;
	device_t child;
	int unit;
	struct sndcard_func *func;

	scp = device_get_softc(dev);
	unit = device_get_unit(dev);

	bzero(scp, sizeof(*scp));

	scp->dev = dev;
	if (alloc_resource(scp)) {
		release_resource(scp);
		return (ENXIO);
	}

	/* PCM Audio */
	func = malloc(sizeof(struct sndcard_func), M_DEVBUF, M_NOWAIT);
	if (func == NULL)
		return (ENOMEM);
	bzero(func, sizeof(*func));
	func->func = SCF_PCM;
	child = device_add_child(dev, "pcm", -1);
	device_set_ivars(child, func);

#if notyet
	/* Midi Interface */
	func = malloc(sizeof(struct sndcard_func), M_DEVBUF, M_NOWAIT);
	if (func == NULL)
		return (ENOMEM);
	bzero(func, sizeof(*func));
	func->func = SCF_MIDI;
	child = device_add_child(dev, "midi", -1);
	device_set_ivars(child, func);

	/* OPL FM Synthesizer */
	func = malloc(sizeof(struct sndcard_func), M_DEVBUF, M_NOWAIT);
	if (func == NULL)
		return (ENOMEM);
	bzero(func, sizeof(*func));
	func->func = SCF_SYNTH;
	child = device_add_child(dev, "midi", -1);
	device_set_ivars(child, func);
#endif /* notyet */

	bus_generic_attach(dev);

	return (0);
}
#endif /* NISA > 0 && NPNP > 0 */

static struct resource *
sbc_alloc_resource(device_t bus, device_t child, int type, int *rid,
		      u_long start, u_long end, u_long count, u_int flags)
{
	sc_p scp;
	int *alloced, rid_max, alloced_max;
	struct resource **res;

	scp = device_get_softc(bus);
	switch (type) {
	case SYS_RES_IOPORT:
		alloced = scp->io_alloced;
		res = scp->io;
		rid_max = 2;
		alloced_max = 1;
		break;
	case SYS_RES_IRQ:
		alloced = &scp->irq_alloced;
		res = &scp->irq;
		rid_max = 0;
		alloced_max = 2; /* pcm and mpu may share the irq. */
		break;
	case SYS_RES_DRQ:
		alloced = scp->drq_alloced;
		res = scp->drq;
		rid_max = 1;
		alloced_max = 1;
		break;
	default:
		return (NULL);
	}

	if (*rid > rid_max || alloced[*rid] == alloced_max)
		return (NULL);

	alloced[*rid]++;
	return (res[*rid]);
}

static int
sbc_release_resource(device_t bus, device_t child, int type, int rid,
			struct resource *r)
{
	sc_p scp;
	int *alloced, rid_max;

	scp = device_get_softc(bus);
	switch (type) {
	case SYS_RES_IOPORT:
		alloced = scp->io_alloced;
		rid_max = 2;
		break;
	case SYS_RES_IRQ:
		alloced = &scp->irq_alloced;
		rid_max = 0;
		break;
	case SYS_RES_DRQ:
		alloced = scp->drq_alloced;
		rid_max = 1;
		break;
	default:
		return (1);
	}

	if (rid > rid_max || alloced[rid] == 0)
		return (1);

	alloced[rid]--;
	return (0);
}

static int io_range[3] = {0x10, 0x4, 0x4};
static int
alloc_resource(sc_p scp)
{
	int i;

	for (i = 0 ; i < sizeof(scp->io) / sizeof(*scp->io) ; i++) {
		if (scp->io[i] == NULL) {
			scp->io_rid[i] = i;
			scp->io[i] = bus_alloc_resource(scp->dev, SYS_RES_IOPORT, &scp->io_rid[i],
							0, ~0, io_range[i], RF_ACTIVE);
			if (scp->io[i] == NULL)
				return (1);
			scp->io_alloced[i] = 0;
		}
	}
	if (scp->irq == NULL) {
		scp->irq_rid = 0;
		scp->irq = bus_alloc_resource(scp->dev, SYS_RES_IRQ, &scp->irq_rid,
					      0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
		if (scp->irq == NULL)
			return (1);
		scp->irq_alloced = 0;
	}
	for (i = 0 ; i < sizeof(scp->drq) / sizeof(*scp->drq) ; i++) {
		if (scp->drq[i] == NULL) {
			scp->drq_rid[i] = i;
			scp->drq[i] = bus_alloc_resource(scp->dev, SYS_RES_DRQ, &scp->drq_rid[i],
							 0, ~0, 1, RF_ACTIVE);
			if (scp->drq[i] == NULL)
				return (1);
			scp->drq_alloced[i] = 0;
		}
	}
	return (0);
}

static int
release_resource(sc_p scp)
{
	int i;

	for (i = 0 ; i < sizeof(scp->io) / sizeof(*scp->io) ; i++) {
		if (scp->io[i] != NULL) {
			bus_release_resource(scp->dev, SYS_RES_IOPORT, scp->io_rid[i], scp->io[i]);
			scp->io[i] = NULL;
		}
	}
	if (scp->irq != NULL) {
		bus_release_resource(scp->dev, SYS_RES_IRQ, scp->irq_rid, scp->irq);
		scp->irq = NULL;
	}
	for (i = 0 ; i < sizeof(scp->drq) / sizeof(*scp->drq) ; i++) {
		if (scp->drq[i] != NULL) {
			bus_release_resource(scp->dev, SYS_RES_DRQ, scp->drq_rid[i], scp->drq[i]);
			scp->drq[i] = NULL;
		}
	}
	return (0);
}

#if NISA > 0 && NPNP > 0
static device_method_t sbc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sbc_probe),
	DEVMETHOD(device_attach,	sbc_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	sbc_alloc_resource),
	DEVMETHOD(bus_release_resource,	sbc_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static driver_t sbc_driver = {
	"sbc",
	sbc_methods,
	sizeof(struct sbc_softc),
};

/*
 * sbc can be attached to an isa bus.
 */
DRIVER_MODULE(sbc, isa, sbc_driver, sbc_devclass, 0, 0);
#endif /* NISA > 0 && NPNP > 0 */

#endif /* NSBC > 0 */
