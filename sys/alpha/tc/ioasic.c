/* $FreeBSD$ */
/* from $NetBSD: ioasic.c,v 1.19 1998/05/27 00:18:13 thorpej Exp $ */

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Keith Bostic, Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "opt_cpu.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#include <machine/rpb.h>
#include <alpha/tc/tcreg.h>
#include <alpha/tc/tcvar.h>
#include <alpha/tc/tcdevs.h>
#include <alpha/tc/ioasicreg.h>
#include <alpha/tc/ioasicvar.h>


#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	ioasic_devclass;
static device_t		ioasic0;		/* there can be only one */

struct ioasic_softc {
	device_t        sc_dv;
	vm_offset_t	sc_base;	
	void            *sc_cookie;
};

#define IOASIC_SOFTC(dev)	(struct ioasic_softc*) device_get_softc(dev)

static int ioasic_probe(device_t dev);
static int ioasic_attach(device_t dev);
static driver_intr_t	ioasic_intrnull;
static int ioasic_print_child(device_t bus, device_t dev);
static void ioasic_lance_dma_setup(void *v);
int     ioasic_intr(void *);

caddr_t le_iomem = 0;

static device_method_t ioasic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ioasic_probe),
	DEVMETHOD(device_attach,	ioasic_attach),
	DEVMETHOD(bus_print_child,	ioasic_print_child),
	
	{ 0, 0 }
};

static driver_t ioasic_driver = {
	"ioasic",
	ioasic_methods,
	sizeof(struct ioasic_softc),
};

#define IOASIC_DEV_LANCE        0
#define IOASIC_DEV_SCC0         1
#define IOASIC_DEV_SCC1         2
#define IOASIC_DEV_ISDN         3

#define IOASIC_DEV_BOGUS        -1

#define IOASIC_NCOOKIES         4

#define C(x)    ((void *)(u_long)x)


struct ioasic_dev ioasic_devs[] = {
        { "le",    0x000c0000, 0, C(IOASIC_DEV_LANCE), IOASIC_INTR_LANCE, },
#ifdef notyet
        { "z8530   ", 0x00100000, 0, C(IOASIC_DEV_SCC0),  IOASIC_INTR_SCC_0, },
        { "z8530   ", 0x00180000, 0, C(IOASIC_DEV_SCC1),  IOASIC_INTR_SCC_1, },
#endif
        { "mcclock", 0x00200000, 0, C(IOASIC_DEV_BOGUS), 0,                 },
#ifdef notyet
        { "AMD79c30", 0x00240000, 0, C(IOASIC_DEV_ISDN),  IOASIC_INTR_ISDN,  },
#endif
};
int ioasic_ndevs = sizeof(ioasic_devs) / sizeof(ioasic_devs[0]);
struct ioasicintr {
        void     (*iai_func)(void *);
        void    *iai_arg;
} ioasicintrs[IOASIC_NCOOKIES];

tc_addr_t ioasic_base;          /* XXX XXX XXX */

static int
ioasic_probe(device_t dev)
{
	if (ioasic0)
		return ENXIO;
	if((hwrpb->rpb_type != ST_DEC_3000_300) &&
	   (hwrpb->rpb_type != ST_DEC_3000_500))
		return ENXIO;
	if(strcmp(device_get_name(dev),"ioasic")){
		return ENXIO;
	}
	ioasic0 = dev;
	if (hwrpb->rpb_type == ST_DEC_3000_300)
		device_set_desc(dev, "Turbochannel ioasic: slow mode");
	else
		device_set_desc(dev, "Turbochannel ioasic: fast mode");
	return 0;
}

static int
ioasic_attach(device_t dev)
{
	device_t child;
	struct ioasic_softc* sc = IOASIC_SOFTC(dev);
	struct tc_attach_args *ta = device_get_ivars(dev);
	device_t parent = device_get_parent(dev);
	u_long i;
	ioasic0 = dev;
	
	sc->sc_base = ta->ta_addr;
	sc->sc_cookie = ta->ta_cookie;
	ioasic_base = sc->sc_base;


#ifdef DEC_3000_300
        if (hwrpb->rpb_type == ST_DEC_3000_300) {
                *(volatile u_int *)IOASIC_REG_CSR(sc->sc_base) |=
                    IOASIC_CSR_FASTMODE;
                tc_mb();
        } 
#endif
        /*
         * Turn off all device interrupt bits.
         * (This does _not_ include 3000/300 TC option slot bits.
         */
        for (i = 0; i < ioasic_ndevs; i++)
                *(volatile u_int32_t *)IOASIC_REG_IMSK(ioasic_base) &=
                        ~ioasic_devs[i].iad_intrbits;
        tc_mb();

        /*
         * Set up interrupt handlers.
         */
        for (i = 0; i < IOASIC_NCOOKIES; i++) {
                ioasicintrs[i].iai_func = ioasic_intrnull;
                ioasicintrs[i].iai_arg = (void *)i;
        }

        tc_intr_establish(parent, sc->sc_cookie, 0, ioasic_intr, sc);

#define LANCE_DMA_SIZE 128*1024
#define LANCE_DMA_ALIGN 128*1024
        /*
         * Set up the LANCE DMA area.
         */
	le_iomem = (caddr_t)vm_page_alloc_contig(round_page(LANCE_DMA_SIZE), 
						 0, 0xffffffff,LANCE_DMA_ALIGN);
	le_iomem = (caddr_t)ALPHA_PHYS_TO_K0SEG(vtophys(le_iomem));
	ioasic_lance_dma_setup((void *)le_iomem);

	/*
	 * round up our children 
	 */

        for (i = 0; i < ioasic_ndevs; i++) {
		ioasic_devs[i].iada_addr = sc->sc_base + ioasic_devs[i].iad_offset;

		child = device_add_child(dev, ioasic_devs[i].iad_modname, -1);
		device_set_ivars(child, &ioasic_devs[i]);
		device_probe_and_attach(child);
	}
	return 0;
}

static void
ioasic_intrnull(void *val)
{

        panic("ioasic_intrnull: uncaught IOASIC intr for cookie %ld\n",
            (u_long)val);
}

static int
ioasic_print_child(device_t bus, device_t dev)
{
	struct ioasic_dev *ioasic = device_get_ivars(dev);
	int retval = 0;
	
	retval += bus_print_child_header(bus, dev);
	retval += printf(" on %s offset 0x%x\n", device_get_nameunit(bus),
			 ioasic->iad_offset);

	return (retval);
}

char *
ioasic_lance_ether_address()
{
        return (u_char *)IOASIC_SYS_ETHER_ADDRESS(ioasic_base);
}

static void
ioasic_lance_dma_setup(void *v)
{
        volatile u_int32_t *ldp;
        tc_addr_t tca;

        tca = (tc_addr_t)v;
	tca &= 0xffffffff;
        ldp = (volatile u_int *)IOASIC_REG_LANCE_DMAPTR(ioasic_base);
        *ldp = ((tca << 3) & ~(tc_addr_t)0x1f) | ((tca >> 29) & 0x1f);
        tc_wmb();
        *(volatile u_int32_t *)IOASIC_REG_CSR(ioasic_base) |=
            IOASIC_CSR_DMAEN_LANCE;
        tc_mb();
}

void
ioasic_intr_establish(ioa, cookie, level, func, arg)
        device_t ioa;
        void *cookie, *arg;
        tc_intrlevel_t level;
        void (*func)(void *);
{
        u_long dev, i;

        dev = (u_long)cookie;
#ifdef DIAGNOSTIC
        /* XXX check cookie. */
#endif

        if (ioasicintrs[dev].iai_func != ioasic_intrnull)
                panic("ioasic_intr_establish: cookie %ld twice", dev);

        ioasicintrs[dev].iai_func = func;
        ioasicintrs[dev].iai_arg = arg;

        /* Enable interrupts for the device. */
        for (i = 0; i < ioasic_ndevs; i++)
                if (ioasic_devs[i].iad_cookie == cookie)
                        break;
        if (i == ioasic_ndevs)
                panic("ioasic_intr_establish: invalid cookie.");
        *(volatile u_int32_t *)IOASIC_REG_IMSK(ioasic_base) |=
                ioasic_devs[i].iad_intrbits;
        tc_mb();
}

void
ioasic_intr_disestablish(ioa, cookie)
        device_t ioa;
        void *cookie;
{
        u_long dev, i;

        dev = (u_long)cookie;
#ifdef DIAGNOSTIC
        /* XXX check cookie. */
#endif

        if (ioasicintrs[dev].iai_func == ioasic_intrnull)
                panic("ioasic_intr_disestablish: cookie %ld missing intr", dev);

        /* Enable interrupts for the device. */
        for (i = 0; i < ioasic_ndevs; i++)
                if (ioasic_devs[i].iad_cookie == cookie)
                        break;
        if (i == ioasic_ndevs)
                panic("ioasic_intr_disestablish: invalid cookie.");
        *(volatile u_int32_t *)IOASIC_REG_IMSK(ioasic_base) &=
                ~ioasic_devs[i].iad_intrbits;
        tc_mb();

        ioasicintrs[dev].iai_func = ioasic_intrnull;
        ioasicintrs[dev].iai_arg = (void *)dev;
}




/*
 * asic_intr --
 *      ASIC interrupt handler.
 */
int
ioasic_intr(val)
        void *val;
{
        register struct ioasic_softc *sc = val;
        register int ifound;
        int gifound;
	u_int32_t sir;
        volatile u_int32_t *sirp;

        sirp = (volatile u_int32_t *)IOASIC_REG_INTR(sc->sc_base);

        gifound = 0;
        do {
                ifound = 0;
                tc_syncbus();

                sir = *sirp;

                /* XXX DUPLICATION OF INTERRUPT BIT INFORMATION... */
#define CHECKINTR(slot, bits)                                           \
                if (sir & bits) {                                       \
                        ifound = 1;                                     \
                        (*ioasicintrs[slot].iai_func)                   \
                            (ioasicintrs[slot].iai_arg);                \
                }
                CHECKINTR(IOASIC_DEV_SCC0, IOASIC_INTR_SCC_0);
                CHECKINTR(IOASIC_DEV_SCC1, IOASIC_INTR_SCC_1);
                CHECKINTR(IOASIC_DEV_LANCE, IOASIC_INTR_LANCE);
                CHECKINTR(IOASIC_DEV_ISDN, IOASIC_INTR_ISDN);

                gifound |= ifound;
        } while (ifound);

        return (gifound);
}


DRIVER_MODULE(ioasic, tc, ioasic_driver, ioasic_devclass, 0, 0);

