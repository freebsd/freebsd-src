/*-
 * Copyright (c) 1999 Luoqi Chen.
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
 
#include <i386/isa/isa_device.h>
#include <dev/aic/aic6360reg.h>
#include <dev/aic/aicvar.h>
  
static int aic_isa_probe __P((struct isa_device *dev));
static int aic_isa_attach __P((struct isa_device *dev));
static void aic_isa_intr __P((void *arg));

static u_int aic_isa_ports[] = { 0x340, 0x140 };
#define	AIC_ISA_NUMPORTS (sizeof(aic_isa_ports) / sizeof(aic_isa_ports[0]))
#define	AIC_ISA_PORTSIZE 0x20

static int
aic_isa_probe(struct isa_device *dev)
{
	struct aic_softc _aic, *aic = &_aic;
	int numports, i;
	u_int port, *ports;
	u_int8_t porta;

	port = dev->id_iobase;
	if (port != -1) {
		ports = &port;
		numports = 1;
	} else {
		ports = aic_isa_ports;
		numports = AIC_ISA_NUMPORTS;
	}

	for (i = 0; i < numports; i++) {
		aic->unit = aic_unit;
		aic->tag = I386_BUS_SPACE_IO;
		aic->bsh = ports[i];
		if (!aic_probe(aic))
			break;
	}

	if (i == numports)
		return (0);

	porta = aic_inb(aic, PORTA);
	if (dev->id_irq <= 0)
		dev->id_irq = 1 << PORTA_IRQ(porta);
	if ((aic->flags & AIC_DMA_ENABLE) && dev->id_drq == -1)
		dev->id_drq = PORTA_DRQ(porta);
	dev->id_iobase = aic->bsh;
	dev->id_intr = aic_isa_intr;
	dev->id_unit = aic_unit++;
	return (AIC_ISA_PORTSIZE);
}

static int
aic_isa_attach(struct isa_device *dev)
{
	struct aic_softc *aic = &aic_softcs[dev->id_unit];

	aic->unit = dev->id_unit;
	aic->tag = I386_BUS_SPACE_IO;
	aic->bsh = dev->id_iobase;

	return (aic_attach(aic));
}

static void
aic_isa_intr(void *arg)
{
	aic_intr((void *)&aic_softcs[(int)arg]);
}

struct isa_driver aicdriver = {
	aic_isa_probe,
	aic_isa_attach,
	"aic"
};
