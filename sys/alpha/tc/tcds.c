/* $FreeBSD$ */
/* from $NetBSD: tcds.c,v 1.25 1998/05/26 23:43:05 thorpej Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#include <machine/rpb.h>
#include <machine/clock.h>
#include <alpha/tc/tcreg.h>
#include <alpha/tc/tcvar.h>
#include <alpha/tc/tcdevs.h>
#include <alpha/tc/tcdsreg.h>
#include <alpha/tc/tcdsvar.h>


#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	tcds_devclass;
static device_t		tcds0;		/* there can be only one */

struct tcds_softc {
	device_t sc_dv;
	vm_offset_t	sc_base;
	void    *sc_cookie;
        volatile u_int32_t *sc_cir;
        volatile u_int32_t *sc_imer;
	struct tcds_slotconfig sc_slots[2];
};

#define TCDS_SOFTC(dev)	(struct tcds_softc*) device_get_softc(dev)

static int tcds_probe(device_t dev);
static int tcds_attach(device_t dev);
static void tcds_intrnull __P((void *));
static void tcds_lance_dma_setup(void *v);
static int  tcds_intr __P((void *));



static device_method_t tcds_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tcds_probe),
	DEVMETHOD(device_attach,	tcds_attach),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	
	{ 0, 0 }
};

static driver_t tcds_driver = {
	"tcds",
	tcds_methods,
	sizeof(struct tcds_softc),
};


extern device_t tc0;
static int
tcds_probe(device_t dev)
{
	if((hwrpb->rpb_type != ST_DEC_3000_300) &&
	   (hwrpb->rpb_type != ST_DEC_3000_500))
		return ENXIO;
	if(strcmp(device_get_name(dev),"tcds")){
		return ENXIO;
	}
	tcds0 = dev;
	device_set_desc(dev, "Turbochannel Dual Scsi");
	return 0;
}

struct tcdsdev_attach_args tcdsdev;


static int
tcds_attach(device_t dev)
{
	struct tcds_softc* sc = TCDS_SOFTC(dev);
	struct tc_attach_args *ta = device_get_ivars(dev);
	device_t parent = device_get_parent(dev);
	device_t child;
	vm_offset_t regs,va;
	u_long i;
	struct tcds_slotconfig *slotc;
	struct tcdsdev_attach_args *tcdsdev;
	tcds0 = dev;

/* 
   XXXXXX

 */	
	sc->sc_base = ta->ta_addr;
        sc->sc_cookie = ta->ta_cookie;
	sc->sc_cir = TCDS_REG(sc->sc_base, TCDS_CIR);
        sc->sc_imer = TCDS_REG(sc->sc_base, TCDS_IMER);

        tc_intr_establish(device_get_parent(dev), sc->sc_cookie, 0,  tcds_intr, sc);
	
        /*
         * XXX
         * IMER apparently has some random (or, not so random, but still
         * not useful) bits set in it when the system boots.  Clear it.
         */
        *sc->sc_imer = 0;
        alpha_wmb();
        /* fill in common information first */
        for (i = 0; i < 2; i++) {
                slotc = &sc->sc_slots[i];

                bzero(slotc, sizeof *slotc);    /* clear everything */

                slotc->sc_slot = i;
                slotc->sc_tcds = sc;
                slotc->sc_esp = NULL;
                slotc->sc_intrhand = tcds_intrnull;
                slotc->sc_intrarg = (void *)(long)i;
        }

        /* information for slot 0 */
        slotc = &sc->sc_slots[0];
        slotc->sc_resetbits = TCDS_CIR_SCSI0_RESET;
        slotc->sc_intrmaskbits =
            TCDS_IMER_SCSI0_MASK | TCDS_IMER_SCSI0_ENB;
        slotc->sc_intrbits = TCDS_CIR_SCSI0_INT;
        slotc->sc_dmabits = TCDS_CIR_SCSI0_DMAENA;
        slotc->sc_errorbits = 0;                                /* XXX */
        slotc->sc_sda = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_ADDR);
        slotc->sc_dic = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_INTR);
        slotc->sc_dud0 = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_DUD0);
        slotc->sc_dud1 = TCDS_REG(sc->sc_base, TCDS_SCSI0_DMA_DUD1);

        /* information for slot 1 */
        slotc = &sc->sc_slots[1];
        slotc->sc_resetbits = TCDS_CIR_SCSI1_RESET;
        slotc->sc_intrmaskbits =
            TCDS_IMER_SCSI1_MASK | TCDS_IMER_SCSI1_ENB;
        slotc->sc_intrbits = TCDS_CIR_SCSI1_INT;
        slotc->sc_dmabits = TCDS_CIR_SCSI1_DMAENA;
        slotc->sc_errorbits = 0;                                /* XXX */
        slotc->sc_sda = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_ADDR);
        slotc->sc_dic = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_INTR);
        slotc->sc_dud0 = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_DUD0);
        slotc->sc_dud1 = TCDS_REG(sc->sc_base, TCDS_SCSI1_DMA_DUD1);


        /* find the hardware attached to the TCDS ASIC */
	tcdsdev = malloc(sizeof(struct tcdsdev_attach_args),
			 M_DEVBUF, M_NOWAIT);
	if (tcdsdev) {
	    strncpy(tcdsdev->tcdsda_modname, "PMAZ-AA ", TC_ROM_LLEN);
	    tcdsdev->tcdsda_slot = 0;
	    tcdsdev->tcdsda_offset = 0;
	    tcdsdev->tcdsda_addr = (tc_addr_t)
		TC_DENSE_TO_SPARSE(sc->sc_base + TCDS_SCSI0_OFFSET);
	    tcdsdev->tcdsda_cookie = (void *)(long)0;
	    tcdsdev->tcdsda_sc = &sc->sc_slots[0];
	    tcdsdev->tcdsda_id = 7;                          /* XXX */
	    tcdsdev->tcdsda_freq = 25000000;                 /* XXX */

	    tcds_scsi_reset(tcdsdev->tcdsda_sc);
	    child = device_add_child(dev, "esp", -1);
	    device_set_ivars(child, tcdsdev);
	    device_probe_and_attach(child);
	}

        /* the second SCSI chip isn't present on the 3000/300 series. */
        if (hwrpb->rpb_type != ST_DEC_3000_300) {
		tcdsdev = malloc(sizeof(struct tcdsdev_attach_args),
				 M_DEVBUF, M_NOWAIT);
		if (tcdsdev) {
			strncpy(tcdsdev->tcdsda_modname, "PMAZ-AA ",
				TC_ROM_LLEN);
			tcdsdev->tcdsda_slot = 1;
			tcdsdev->tcdsda_offset = 0;
			tcdsdev->tcdsda_addr = (tc_addr_t)
				TC_DENSE_TO_SPARSE(sc->sc_base + TCDS_SCSI1_OFFSET);
			tcdsdev->tcdsda_cookie = (void *)(long)1;
			tcdsdev->tcdsda_sc = &sc->sc_slots[1];
			tcdsdev->tcdsda_id = 7;                  /* XXX */
			tcdsdev->tcdsda_freq = 25000000;         /* XXX */
			tcds_scsi_reset(tcdsdev->tcdsda_sc);
			child = device_add_child(dev, "esp", -1);
			device_set_ivars(child, tcdsdev);
			device_probe_and_attach(child);
		}
        }
	return 0;
}

void
tcds_scsi_reset(sc)
        struct tcds_slotconfig *sc;
{
        tcds_dma_enable(sc, 0);
        tcds_scsi_enable(sc, 0);

        TCDS_CIR_CLR(*sc->sc_tcds->sc_cir, sc->sc_resetbits);
        alpha_mb();
        DELAY(1);
        TCDS_CIR_SET(*sc->sc_tcds->sc_cir, sc->sc_resetbits);
        alpha_mb();

        tcds_scsi_enable(sc, 1);
        tcds_dma_enable(sc, 1);
}


void
tcds_scsi_enable(sc, on)
        struct tcds_slotconfig *sc;
        int on;
{

        if (on) 
                *sc->sc_tcds->sc_imer |= sc->sc_intrmaskbits;
        else
                *sc->sc_tcds->sc_imer &= ~sc->sc_intrmaskbits;
        alpha_mb();
}

void
tcds_dma_enable(sc, on)
        struct tcds_slotconfig *sc;
        int on;
{

        /* XXX Clear/set IOSLOT/PBS bits. */
        if (on) 
                TCDS_CIR_SET(*sc->sc_tcds->sc_cir, sc->sc_dmabits);
        else
                TCDS_CIR_CLR(*sc->sc_tcds->sc_cir, sc->sc_dmabits);
        alpha_mb();
}

int
tcds_scsi_isintr(sc, clear)
        struct tcds_slotconfig *sc;
        int clear;
{

        if ((*sc->sc_tcds->sc_cir & sc->sc_intrbits) != 0) {
                if (clear) {
                        TCDS_CIR_CLR(*sc->sc_tcds->sc_cir, sc->sc_intrbits);
                        alpha_mb();
                }
                return (1);
        } else
                return (0);
}


int
tcds_scsi_iserr(sc)
        struct tcds_slotconfig *sc;
{

        return ((*sc->sc_tcds->sc_cir & sc->sc_errorbits) != 0);
}

static void
tcds_intrnull(void *val)
{

        panic("tcds_intrnull: uncaught IOASIC intr for cookie %ld\n",
            (u_long)val);
}

void
tcds_intr_establish(tcds, cookie, level, func, arg)
        device_t tcds;
        void *cookie, *arg;
        tc_intrlevel_t level;
        int (*func) __P((void *));
{
        struct tcds_softc *sc = device_get_softc(tcds);
        u_long slot;

        slot = (u_long)cookie;
#ifdef DIAGNOSTIC
        /* XXX check cookie. */
#endif
        if (sc->sc_slots[slot].sc_intrhand != tcds_intrnull){
                panic("tcds_intr_establish: cookie %d twice, intrhdlr = 0x%lx", 
		      slot,sc->sc_slots[slot].sc_intrhand );
	}
        sc->sc_slots[slot].sc_intrhand = func;
        sc->sc_slots[slot].sc_intrarg = arg;
        tcds_scsi_reset(&sc->sc_slots[slot]);
}
void
tcds_intr_disestablish(tcds, cookie)
        device_t tcds;
        void *cookie;
{
        struct tcds_softc *sc = device_get_softc(tcds);
        u_long slot;

        slot = (u_long)cookie;
#ifdef DIAGNOSTIC
        /* XXX check cookie. */
#endif

        if (sc->sc_slots[slot].sc_intrhand == tcds_intrnull)
                panic("tcds_intr_disestablish: cookie %d missing intr",
                    slot);

        sc->sc_slots[slot].sc_intrhand = tcds_intrnull;
        sc->sc_slots[slot].sc_intrarg = (void *)slot;

        tcds_dma_enable(&sc->sc_slots[slot], 0);
        tcds_scsi_enable(&sc->sc_slots[slot], 0);
}

static int
tcds_intr(val)
        void *val;
{
        struct tcds_softc *sc;
        u_int32_t ir;

        sc = val;

        /*
         * XXX
         * Copy and clear (gag!) the interrupts.
         */
        ir = *sc->sc_cir;
        alpha_mb();
        TCDS_CIR_CLR(*sc->sc_cir, TCDS_CIR_ALLINTR);
        alpha_mb();
        tc_syncbus();
        alpha_mb();

#define CHECKINTR(slot)                                                 \
        if (ir & sc->sc_slots[slot].sc_intrbits) {                      \
                (void)(*sc->sc_slots[slot].sc_intrhand)                 \
                    (sc->sc_slots[slot].sc_intrarg);                    \
        }
        CHECKINTR(0);
        CHECKINTR(1);
#undef CHECKINTR

#ifdef DIAGNOSTIC
        /* 
         * Interrupts not currently handled, but would like to know if they
         * occur.
         *
         * XXX
         * Don't know if we have to set the interrupt mask and enable bits
         * in the IMER to allow some of them to happen?
         */
#define PRINTINTR(msg, bits)                                            \
        if (ir & bits)                                                  \
                printf(msg);
        PRINTINTR("SCSI0 DREQ interrupt.\n", TCDS_CIR_SCSI0_DREQ);
        PRINTINTR("SCSI1 DREQ interrupt.\n", TCDS_CIR_SCSI1_DREQ);
        PRINTINTR("SCSI0 prefetch interrupt.\n", TCDS_CIR_SCSI0_PREFETCH);
        PRINTINTR("SCSI1 prefetch interrupt.\n", TCDS_CIR_SCSI1_PREFETCH);
        PRINTINTR("SCSI0 DMA error.\n", TCDS_CIR_SCSI0_DMA);
        PRINTINTR("SCSI1 DMA error.\n", TCDS_CIR_SCSI1_DMA);
        PRINTINTR("SCSI0 DB parity error.\n", TCDS_CIR_SCSI0_DB);
        PRINTINTR("SCSI1 DB parity error.\n", TCDS_CIR_SCSI1_DB);
        PRINTINTR("SCSI0 DMA buffer parity error.\n", TCDS_CIR_SCSI0_DMAB_PAR);
        PRINTINTR("SCSI1 DMA buffer parity error.\n", TCDS_CIR_SCSI1_DMAB_PAR);
        PRINTINTR("SCSI0 DMA read parity error.\n", TCDS_CIR_SCSI0_DMAR_PAR);
        PRINTINTR("SCSI1 DMA read parity error.\n", TCDS_CIR_SCSI1_DMAR_PAR);
        PRINTINTR("TC write parity error.\n", TCDS_CIR_TCIOW_PAR);
        PRINTINTR("TC I/O address parity error.\n", TCDS_CIR_TCIOA_PAR);
#undef PRINTINTR
#endif

        /*
         * XXX
         * The MACH source had this, with the comment:
         *      This is wrong, but machine keeps dying.
         */
        DELAY(1);
	return 1;
}

DRIVER_MODULE(tcds, tc, tcds_driver, tcds_devclass, 0, 0);

