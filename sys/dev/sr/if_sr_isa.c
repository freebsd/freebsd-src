/*
 * Copyright (c) 1996 - 2001 John Hay.
 * Copyright (c) 1996 SDL Communications, Inc.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
#include <sys/conf.h>		/* cdevsw stuff */
#include <sys/kernel.h>		/* SYSINIT stuff */
#include <sys/uio.h>		/* SYSINIT stuff */
#include <sys/malloc.h>		/* malloc region definitions */
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <sys/rman.h>
#include <sys/time.h>

#include <isa/isavar.h>
#include "isa_if.h"

#include <dev/ic/hd64570.h>
#include <dev/sr/if_srregs.h>

/*
 * List of valid interrupt numbers for the N2 ISA card.
 */
static int sr_irqtable[16] = {
	0,	/*  0 */
	0,	/*  1 */
	0,	/*  2 */
	1,	/*  3 */
	1,	/*  4 */
	1,	/*  5 */
	0,	/*  6 */
	1,	/*  7 */
	0,	/*  8 */
	0,	/*  9 */
	1,	/* 10 */
	1,	/* 11 */
	1,	/* 12 */
	0,	/* 13 */
	0,	/* 14 */
	1	/* 15 */
};

static int sr_isa_probe (device_t);
static int sr_isa_attach (device_t);

static struct isa_pnp_id sr_ids[] = {
	{0,		NULL}
};

static device_method_t sr_methods[] = {
	DEVMETHOD(device_probe,		sr_isa_probe),
	DEVMETHOD(device_attach,	sr_isa_attach),
	DEVMETHOD(device_detach,	sr_detach),
	{ 0, 0 }
};

static driver_t sr_isa_driver = {
	"sr",
	sr_methods,
	sizeof (struct sr_hardc)
};

DRIVER_MODULE(if_sr, isa, sr_isa_driver, sr_devclass, 0, 0);

static u_int	src_get8_io(u_int base, u_int off);
static u_int	src_get16_io(u_int base, u_int off);
static void	src_put8_io(u_int base, u_int off, u_int val);
static void	src_put16_io(u_int base, u_int off, u_int val);
static u_int	src_dpram_size(device_t device);

/*
 * Probe for an ISA card. If it is there, size its memory. Then get the
 * rest of its information and fill it in.
 */
static int
sr_isa_probe (device_t device)
{
	struct sr_hardc *hc;
	int error;
	u_int32_t flags;
	u_int i, tmp;
	u_short port;
	u_long irq, junk, membase, memsize, port_start, port_count;
	sca_regs *sca = 0;

	error = ISA_PNP_PROBE(device_get_parent(device), device, sr_ids);
	if (error == ENXIO || error == 0)
		return (error);

	hc = device_get_softc(device);
	bzero(hc, sizeof(struct sr_hardc));

	if (sr_allocate_ioport(device, 0, SRC_IO_SIZ)) {
		return (ENXIO);
	}

	/*
	 * Now see if the card is realy there.
	 */
	error = bus_get_resource(device, SYS_RES_IOPORT, 0, &port_start,
	    &port_count);
	port = port_start;

	hc->cardtype = SR_CRD_N2;
	hc->cunit = device_get_unit(device);
	hc->iobase = port_start;
	/*
	 * We have to fill these in early because the SRC_PUT* and SRC_GET*
	 * macros use them.
	 */
	hc->src_get8 = src_get8_io;
	hc->src_get16 = src_get16_io;
	hc->src_put8 = src_put8_io;
	hc->src_put16 = src_put16_io;

	hc->sca = 0;
	hc->numports = NCHAN;	/* assumed # of channels on the card */

	flags = device_get_flags(device);
	if (flags & SR_FLAGS_NCHAN_MSK)
		hc->numports = flags & SR_FLAGS_NCHAN_MSK;

	outb(port + SR_PCR, 0);	/* turn off the card */

	/*
	 * Next, we'll test the Base Address Register to retension of
	 * data... ... seeing if we're *really* talking to an N2.
	 */
	for (i = 0; i < 0x100; i++) {
		outb(port + SR_BAR, i);
		inb(port + SR_PCR);
		tmp = inb(port + SR_BAR);
		if (tmp != i) {
			printf("sr%d: probe failed BAR %x, %x.\n",
			       hc->cunit, i, tmp);
			goto errexit;
		}
	}

	/*
	 * Now see if we can see the SCA.
	 */
	outb(port + SR_PCR, SR_PCR_SCARUN | inb(port + SR_PCR));
	SRC_PUT8(port, sca->wcrl, 0);
	SRC_PUT8(port, sca->wcrm, 0);
	SRC_PUT8(port, sca->wcrh, 0);
	SRC_PUT8(port, sca->pcr, 0);
	SRC_PUT8(port, sca->msci[0].tmc, 0);
	inb(port);

	tmp = SRC_GET8(port, sca->msci[0].tmc);
	if (tmp != 0) {
		printf("sr%d: Error reading SCA 0, %x\n", hc->cunit, tmp);
		goto errexit;
	}
	SRC_PUT8(port, sca->msci[0].tmc, 0x5A);
	inb(port);

	tmp = SRC_GET8(port, sca->msci[0].tmc);
	if (tmp != 0x5A) {
		printf("sr%d: Error reading SCA 0x5A, %x\n", hc->cunit, tmp);
		goto errexit;
	}
	SRC_PUT16(port, sca->dmac[0].cda, 0);
	inb(port);

	tmp = SRC_GET16(port, sca->dmac[0].cda);
	if (tmp != 0) {
		printf("sr%d: Error reading SCA 0, %x\n", hc->cunit, tmp);
		goto errexit;
	}
	SRC_PUT16(port, sca->dmac[0].cda, 0x55AA);
	inb(port);

	tmp = SRC_GET16(port, sca->dmac[0].cda);
	if (tmp != 0x55AA) {
		printf("sr%d: Error reading SCA 0x55AA, %x\n",
		       hc->cunit, tmp);
		goto errexit;
	}

	membase = bus_get_resource_start(device, SYS_RES_MEMORY, 0);
	memsize = SRC_WIN_SIZ;
	if (bus_set_resource(device, SYS_RES_MEMORY, 0, membase, memsize))
		goto errexit;

	if (sr_allocate_memory(device, 0, SRC_WIN_SIZ))
		goto errexit;

	if (src_dpram_size(device) < 4)
		goto errexit;

	if (sr_allocate_irq(device, 0, 1))
		goto errexit;

	if (bus_get_resource(device, SYS_RES_IRQ, 0, &irq, &junk)) {
		goto errexit;
	}
	/*
	 * Do a little sanity check.
	 */
	if (sr_irqtable[irq] == 0)
		printf("sr%d: Warning: illegal interrupt %ld chosen.\n",
		       hc->cunit, irq);

	/*
	 * Bogus card configuration
	 */
	if ((hc->numports > NCHAN)	/* only 2 ports/card */
	    ||(hc->memsize > (512 * 1024)))	/* no more than 256K */
		goto errexit;

	sr_deallocate_resources(device);
	return (0);

errexit:
	sr_deallocate_resources(device);
	return (ENXIO);
}

/*
 * srattach_isa and srattach_pci allocate memory for hardc, softc and
 * data buffers. It also does any initialization that is bus specific.
 * At the end they call the common srattach() function.
 */
static int
sr_isa_attach (device_t device)
{
	u_char mar;
	u_int32_t flags;
	struct sr_hardc *hc;

	hc = device_get_softc(device);
	bzero(hc, sizeof(struct sr_hardc));

	if (sr_allocate_ioport(device, 0, SRC_IO_SIZ))
		goto errexit;
	if (sr_allocate_memory(device, 0, SRC_WIN_SIZ))
		goto errexit;
	if (sr_allocate_irq(device, 0, 1))
		goto errexit;

	/*
	 * We have to fill these in early because the SRC_PUT* and SRC_GET*
	 * macros use them.
	 */
	hc->src_get8 = src_get8_io;
	hc->src_get16 = src_get16_io;
	hc->src_put8 = src_put8_io;
	hc->src_put16 = src_put16_io;

	hc->cardtype = SR_CRD_N2;
	hc->cunit = device_get_unit(device);
	hc->sca = 0;
	hc->numports = NCHAN;	/* assumed # of channels on the card */
	flags = device_get_flags(device);
	if (flags & SR_FLAGS_NCHAN_MSK)
		hc->numports = flags & SR_FLAGS_NCHAN_MSK;

	hc->iobase = rman_get_start(hc->res_ioport);
	hc->sca_base = hc->iobase;
	hc->mem_start = (caddr_t)rman_get_virtual(hc->res_memory);
	hc->mem_end = hc->mem_start + SRC_WIN_SIZ;
	hc->mem_pstart = 0;
	hc->winmsk = SRC_WIN_MSK;

	hc->mempages = src_dpram_size(device);
	hc->memsize = hc->mempages * SRC_WIN_SIZ;

	outb(hc->iobase + SR_PCR, inb(hc->iobase + SR_PCR) | SR_PCR_SCARUN);
	outb(hc->iobase + SR_PSR, inb(hc->iobase + SR_PSR) | SR_PSR_EN_SCA_DMA);
	outb(hc->iobase + SR_MCR,
	     SR_MCR_DTR0 | SR_MCR_DTR1 | SR_MCR_TE0 | SR_MCR_TE1);

	SRC_SET_ON(hc->iobase);

	/*
	 * Configure the card. Mem address, irq,
	 */
	mar = (rman_get_start(hc->res_memory) >> 16) & SR_PCR_16M_SEL;
	outb(hc->iobase + SR_PCR,
	     mar | (inb(hc->iobase + SR_PCR) & ~SR_PCR_16M_SEL));
	mar = rman_get_start(hc->res_memory) >> 12;
	outb(hc->iobase + SR_BAR, mar);

	return sr_attach(device);

errexit:
	sr_deallocate_resources(device);
	return (ENXIO);
}

/*
 * I/O for ISA N2 card(s)
 */
#define SRC_REG(iobase,y)	((((y) & 0xf) + (((y) & 0xf0) << 6) +       \
				(iobase)) | 0x8000)

static u_int
src_get8_io(u_int base, u_int off)
{
	return inb(SRC_REG(base, off));
}

static u_int
src_get16_io(u_int base, u_int off)
{
	return inw(SRC_REG(base, off));
}

static void
src_put8_io(u_int base, u_int off, u_int val)
{
	outb(SRC_REG(base, off), val);
}

static void
src_put16_io(u_int base, u_int off, u_int val)
{
	outw(SRC_REG(base, off), val);
}

static u_int
src_dpram_size(device_t device)
{
	u_int pgs, i;
	u_short port;
	u_short *smem;
	u_char mar;
	u_long membase;
	struct sr_hardc *hc;

	hc = device_get_softc(device);
	port = hc->iobase;

	/*
	 * OK, the board's interface registers seem to work. Now we'll see
	 * if the Dual-Ported RAM is fully accessible...
	 */
	outb(port + SR_PCR, SR_PCR_EN_VPM | SR_PCR_ISA16);
	outb(port + SR_PSR, SR_PSR_WIN_16K);

	/*
	 * Take the kernel "virtual" address supplied to us and convert
	 * it to a "real" address. Then program the card to use that.
	 */
	membase = rman_get_start(hc->res_memory);
	mar = (membase >> 16) & SR_PCR_16M_SEL;
	outb(port + SR_PCR, mar | inb(port + SR_PCR));
	mar = membase >> 12;
	outb(port + SR_BAR, mar);
	outb(port + SR_PCR, inb(port + SR_PCR) | SR_PCR_MEM_WIN);
	smem = (u_short *)rman_get_virtual(hc->res_memory);/* DP RAM Address */
	/*
	 * Here we will perform the memory scan to size the device.
	 *
	 * This is done by marking each potential page with a magic number.
	 * We then loop through the pages looking for that magic number. As
	 * soon as we no longer see that magic number, we'll quit the scan,
	 * knowing that no more memory is present. This provides the number
	 * of pages present on the card.
	 *
	 * Note: We're sizing 16K memory granules.
	 */
	for (i = 0; i <= SR_PSR_PG_SEL; i++) {
		outb(port + SR_PSR,
		     (inb(port + SR_PSR) & ~SR_PSR_PG_SEL) | i);

		*smem = 0xAA55;
	}

	for (i = 0; i <= SR_PSR_PG_SEL; i++) {
		outb(port + SR_PSR,
		     (inb(port + SR_PSR) & ~SR_PSR_PG_SEL) | i);

		if (*smem != 0xAA55) {
			/*
			 * If we have less than 64k of memory, give up. That
			 * is 4 x 16k pages.
			 */
			if (i < 4) {
				printf("sr%d: Bad mem page %d, mem %x, %x.\n",
				       hc->cunit, i, 0xAA55, *smem);
				return 0;
			}
			break;
		}
		*smem = i;
	}

	hc->mempages = i;
	hc->memsize = i * SRC_WIN_SIZ;
	hc->winmsk = SRC_WIN_MSK;
	pgs = i;		/* final count of 16K pages */

	/*
	 * This next loop erases the contents of that page in DPRAM
	 */
	for (i = 0; i <= pgs; i++) {
		outb(port + SR_PSR,
		     (inb(port + SR_PSR) & ~SR_PSR_PG_SEL) | i);
		bzero(smem, SRC_WIN_SIZ);
	}

	SRC_SET_OFF(port);
	return (pgs);
}
