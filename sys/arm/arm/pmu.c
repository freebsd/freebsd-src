/*-
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

/*
 * Performance Monitoring Unit
 */

#include <sys/cdefs.h>
#include "opt_hwpmc_hooks.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include "pmu.h"

/* CCNT */
#if defined(__arm__) && (__ARM_ARCH > 6)
int pmu_attched = 0;
uint32_t ccnt_hi[MAXCPU];
#endif

#define	PMU_OVSR_C		0x80000000	/* Cycle Counter */
#define	PMU_IESR_C		0x80000000	/* Cycle Counter */

static int
pmu_intr(void *arg)
{
#ifdef HWPMC_HOOKS
	struct trapframe *tf;
#endif
	uint32_t r;
#if defined(__arm__) && (__ARM_ARCH > 6)
	u_int cpu;

	cpu = PCPU_GET(cpuid);

	r = cp15_pmovsr_get();
	if (r & PMU_OVSR_C) {
		atomic_add_32(&ccnt_hi[cpu], 1);
		/* Clear the event. */
		r &= ~PMU_OVSR_C;
		cp15_pmovsr_set(PMU_OVSR_C);
	}
#else
	r = 1;
#endif

#ifdef HWPMC_HOOKS
	/* Only call into the HWPMC framework if we know there is work. */
	if (r != 0 && pmc_intr) {
		tf = arg;
		(*pmc_intr)(tf);
	}
#endif

	return (FILTER_HANDLED);
}

int
pmu_attach(device_t dev)
{
	struct pmu_softc *sc;
#if defined(__arm__) && (__ARM_ARCH > 6)
	uint32_t iesr;
#endif
	int err, i;

	sc = device_get_softc(dev);
	sc->dev = dev;

	for (i = 0; i < MAX_RLEN; i++) {
		if (sc->irq[i].res == NULL)
			break;
		err = bus_setup_intr(dev, sc->irq[i].res,
		    INTR_MPSAFE | INTR_TYPE_MISC, pmu_intr, NULL, NULL,
		    &sc->irq[i].ih);
		if (err != 0) {
			device_printf(dev,
			    "Unable to setup interrupt handler.\n");
			goto fail;
		}
		if (sc->irq[i].cpuid != -1) {
			err = bus_bind_intr(dev, sc->irq[i].res,
			    sc->irq[i].cpuid);
			if (err != 0) {
				device_printf(sc->dev,
				    "Unable to bind interrupt.\n");
				goto fail;
			}
		}
	}

#if defined(__arm__) && (__ARM_ARCH > 6)
	/* Initialize to 0. */
	for (i = 0; i < MAXCPU; i++)
		ccnt_hi[i] = 0;

	/* Enable the interrupt to fire on overflow. */
	iesr = cp15_pminten_get();
	iesr |= PMU_IESR_C;
	cp15_pminten_set(iesr);

	/* Need this for getcyclecount() fast path. */
	pmu_attched |= 1;
#endif

	return (0);

fail:
	for (i = 1; i < MAX_RLEN; i++) {
		if (sc->irq[i].ih != NULL)
			bus_teardown_intr(dev, sc->irq[i].res, sc->irq[i].ih);
		if (sc->irq[i].res != NULL)
			bus_release_resource(dev, SYS_RES_IRQ, i,
			    sc->irq[i].res);
	}
	return(err);
}

