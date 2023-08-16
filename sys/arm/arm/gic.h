/*-
 * Copyright (c) 2011,2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
 *
 * Developed by Damjan Marion <damjan.marion@gmail.com>
 *
 * Based on OMAP4 GIC code by Ben Gray
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ARM_GIC_H_
#define _ARM_GIC_H_

/* The GICv1/2 only supports 8 CPUs */
#if MAXCPU > 8
#define	GIC_MAXCPU	8
#else
#define	GIC_MAXCPU	MAXCPU
#endif

struct arm_gic_softc {
	device_t		gic_dev;
	void *			gic_intrhand;
	struct gic_irqsrc *	gic_irqs;
#define	GIC_RES_DIST		0
#define	GIC_RES_CPU		1
	struct resource *	gic_res[3];
	uint8_t			ver;
	struct mtx		mutex;
	uint32_t		nirqs;
	uint32_t		typer;
	uint32_t		last_irq[GIC_MAXCPU];

	uint32_t		gic_iidr;
	u_int			gic_bus;

	int			nranges;
	struct arm_gic_range *	ranges;
};

DECLARE_CLASS(arm_gic_driver);

struct arm_gicv2m_softc {
	struct resource	*sc_mem;
	uintptr_t	sc_xref;
	u_int		sc_spi_start;
	u_int		sc_spi_count;
};

DECLARE_CLASS(arm_gicv2m_driver);

int arm_gic_attach(device_t);
int arm_gic_detach(device_t);
int arm_gicv2m_attach(device_t);
int arm_gic_intr(void *);

#endif /* _ARM_GIC_H_ */
