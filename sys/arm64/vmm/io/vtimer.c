/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 The FreeBSD Foundation
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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timeet.h>
#include <sys/timetc.h>

#include <machine/bus.h>
#include <machine/machdep.h>
#include <machine/vmm.h>
#include <machine/armreg.h>

#include <arm64/vmm/arm64.h>

#include "vgic.h"
#include "vtimer.h"

#define	RES1		0xffffffffffffffffUL

#define timer_enabled(ctl)	\
    (!((ctl) & CNTP_CTL_IMASK) && ((ctl) & CNTP_CTL_ENABLE))

static uint64_t cnthctl_el2_reg;
static uint32_t tmr_frq;

#define timer_condition_met(ctl)	((ctl) & CNTP_CTL_ISTATUS)

static void vtimer_schedule_irq(struct hypctx *hypctx, bool phys);

static int
vtimer_virtual_timer_intr(void *arg)
{
	struct hypctx *hypctx;
	uint64_t cntpct_el0;
	uint32_t cntv_ctl;

	hypctx = arm64_get_active_vcpu();
	cntv_ctl = READ_SPECIALREG(cntv_ctl_el0);

	if (!hypctx) {
		/* vm_destroy() was called. */
		eprintf("No active vcpu\n");
		cntv_ctl = READ_SPECIALREG(cntv_ctl_el0);
		goto out;
	}
	if (!timer_enabled(cntv_ctl)) {
		eprintf("Timer not enabled\n");
		goto out;
	}
	if (!timer_condition_met(cntv_ctl)) {
		eprintf("Timer condition not met\n");
		goto out;
	}

	cntpct_el0 = READ_SPECIALREG(cntpct_el0) -
	    hypctx->hyp->vtimer.cntvoff_el2;
	if (hypctx->vtimer_cpu.virt_timer.cntx_cval_el0 < cntpct_el0)
		vgic_inject_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    GT_VIRT_IRQ, true);

	cntv_ctl = hypctx->vtimer_cpu.virt_timer.cntx_ctl_el0;

out:
	/*
	 * Disable the timer interrupt. This will prevent the interrupt from
	 * being reasserted as soon as we exit the handler and getting stuck
	 * in an infinite loop.
	 *
	 * This is safe to do because the guest disabled the timer, and then
	 * enables it as part of the interrupt handling routine.
	 */
	cntv_ctl &= ~CNTP_CTL_ENABLE;
	WRITE_SPECIALREG(cntv_ctl_el0, cntv_ctl);

	return (FILTER_HANDLED);
}

int
vtimer_init(uint64_t cnthctl_el2)
{
	cnthctl_el2_reg = cnthctl_el2;
	/*
	 * The guest *MUST* use the same timer frequency as the host. The
	 * register CNTFRQ_EL0 is accessible to the guest and a different value
	 * in the guest dts file might have unforseen consequences.
	 */
	tmr_frq = READ_SPECIALREG(cntfrq_el0);

	return (0);
}

void
vtimer_vminit(struct hyp *hyp)
{
	uint64_t now;

	/*
	 * Configure the Counter-timer Hypervisor Control Register for the VM.
	 *
	 * CNTHCTL_EL1PCEN: trap access to CNTP_{CTL, CVAL, TVAL}_EL0 from EL1
	 * CNTHCTL_EL1PCTEN: trap access to CNTPCT_EL0
	 */
	hyp->vtimer.cnthctl_el2 = cnthctl_el2_reg & ~CNTHCTL_EL1PCEN;
	hyp->vtimer.cnthctl_el2 &= ~CNTHCTL_EL1PCTEN;

	now = READ_SPECIALREG(cntpct_el0);
	hyp->vtimer.cntvoff_el2 = now;

	return;
}

void
vtimer_cpuinit(struct hypctx *hypctx)
{
	struct vtimer_cpu *vtimer_cpu;

	vtimer_cpu = &hypctx->vtimer_cpu;
	/*
	 * Configure physical timer interrupts for the VCPU.
	 *
	 * CNTP_CTL_IMASK: mask interrupts
	 * ~CNTP_CTL_ENABLE: disable the timer
	 */
	vtimer_cpu->phys_timer.cntx_ctl_el0 = CNTP_CTL_IMASK & ~CNTP_CTL_ENABLE;

	mtx_init(&vtimer_cpu->phys_timer.mtx, "vtimer phys callout mutex", NULL,
	    MTX_DEF);
	callout_init_mtx(&vtimer_cpu->phys_timer.callout,
	    &vtimer_cpu->phys_timer.mtx, 0);
	vtimer_cpu->phys_timer.irqid = GT_PHYS_NS_IRQ;

	mtx_init(&vtimer_cpu->virt_timer.mtx, "vtimer virt callout mutex", NULL,
	    MTX_DEF);
	callout_init_mtx(&vtimer_cpu->virt_timer.callout,
	    &vtimer_cpu->virt_timer.mtx, 0);
	vtimer_cpu->virt_timer.irqid = GT_VIRT_IRQ;
}

void
vtimer_cpucleanup(struct hypctx *hypctx)
{
	struct vtimer_cpu *vtimer_cpu;

	vtimer_cpu = &hypctx->vtimer_cpu;
	callout_drain(&vtimer_cpu->phys_timer.callout);
	callout_drain(&vtimer_cpu->virt_timer.callout);
	mtx_destroy(&vtimer_cpu->phys_timer.mtx);
	mtx_destroy(&vtimer_cpu->virt_timer.mtx);
}

void
vtimer_vmcleanup(struct hyp *hyp)
{
	struct hypctx *hypctx;
	uint32_t cntv_ctl;

	hypctx = arm64_get_active_vcpu();
	if (!hypctx) {
		/* The active VM was destroyed, stop the timer. */
		cntv_ctl = READ_SPECIALREG(cntv_ctl_el0);
		cntv_ctl &= ~CNTP_CTL_ENABLE;
		WRITE_SPECIALREG(cntv_ctl_el0, cntv_ctl);
	}
}

void
vtimer_cleanup(void)
{
}

void
vtimer_sync_hwstate(struct hypctx *hypctx)
{
	struct vtimer_timer *timer;
	uint64_t cntpct_el0;

	timer = &hypctx->vtimer_cpu.virt_timer;
	cntpct_el0 = READ_SPECIALREG(cntpct_el0) -
	    hypctx->hyp->vtimer.cntvoff_el2;
	if (!timer_enabled(timer->cntx_ctl_el0)) {
		vgic_inject_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    timer->irqid, false);
	} else if (timer->cntx_cval_el0 < cntpct_el0) {
		vgic_inject_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    timer->irqid, true);
	} else {
		vgic_inject_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    timer->irqid, false);
		vtimer_schedule_irq(hypctx, false);
	}
}

static void
vtimer_inject_irq_callout_phys(void *context)
{
	struct hypctx *hypctx;

	hypctx = context;
	vgic_inject_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
	    hypctx->vtimer_cpu.phys_timer.irqid, true);
}

static void
vtimer_inject_irq_callout_virt(void *context)
{
	struct hypctx *hypctx;

	hypctx = context;
	vgic_inject_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
	    hypctx->vtimer_cpu.virt_timer.irqid, true);
}

static void
vtimer_schedule_irq(struct hypctx *hypctx, bool phys)
{
	sbintime_t time;
	struct vtimer_timer *timer;
	uint64_t cntpct_el0;
	uint64_t diff;

	if (phys)
		timer = &hypctx->vtimer_cpu.phys_timer;
	else
		timer = &hypctx->vtimer_cpu.virt_timer;
	cntpct_el0 = READ_SPECIALREG(cntpct_el0) -
	    hypctx->hyp->vtimer.cntvoff_el2;
	if (timer->cntx_cval_el0 < cntpct_el0) {
		/* Timer set in the past, trigger interrupt */
		vgic_inject_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    timer->irqid, true);
	} else {
		diff = timer->cntx_cval_el0 - cntpct_el0;
		time = diff * SBT_1S / tmr_frq;
		if (phys)
			callout_reset_sbt(&timer->callout, time, 0,
			    vtimer_inject_irq_callout_phys, hypctx, 0);
		else
			callout_reset_sbt(&timer->callout, time, 0,
			    vtimer_inject_irq_callout_virt, hypctx, 0);
	}
}

static void
vtimer_remove_irq(struct hypctx *hypctx, struct vcpu *vcpu)
{
	struct vtimer_cpu *vtimer_cpu;
	struct vtimer_timer *timer;

	vtimer_cpu = &hypctx->vtimer_cpu;
	timer = &vtimer_cpu->phys_timer;

	callout_drain(&timer->callout);
	/*
	 * The interrupt needs to be deactivated here regardless of the callout
	 * function having been executed. The timer interrupt can be masked with
	 * the CNTP_CTL_EL0.IMASK bit instead of reading the IAR register.
	 * Masking the interrupt doesn't remove it from the list registers.
	 */
	vgic_inject_irq(hypctx->hyp, vcpu_vcpuid(vcpu), timer->irqid, false);
}

/*
 * Timer emulation functions.
 *
 * The guest should use the virtual timer, however some software, e.g. u-boot,
 * used the physical timer. Emulate this in software for the guest to use.
 *
 * Adjust for cntvoff_el2 so the physical and virtual timers are at similar
 * times. This simplifies interrupt handling in the virtual timer as the
 * adjustment will have already happened.
 */

int
vtimer_phys_ctl_read(struct vcpu *vcpu, uint64_t *rval, void *arg)
{
	struct hyp *hyp;
	struct hypctx *hypctx;
	struct vtimer_cpu *vtimer_cpu;
	uint64_t cntpct_el0;

	hypctx = vcpu_get_cookie(vcpu);
	hyp = hypctx->hyp;
	vtimer_cpu = &hypctx->vtimer_cpu;

	cntpct_el0 = READ_SPECIALREG(cntpct_el0) - hyp->vtimer.cntvoff_el2;
	if (vtimer_cpu->phys_timer.cntx_cval_el0 < cntpct_el0)
		/* Timer condition met */
		*rval = vtimer_cpu->phys_timer.cntx_ctl_el0 | CNTP_CTL_ISTATUS;
	else
		*rval = vtimer_cpu->phys_timer.cntx_ctl_el0 & ~CNTP_CTL_ISTATUS;

	return (0);
}

int
vtimer_phys_ctl_write(struct vcpu *vcpu, uint64_t wval, void *arg)
{
	struct hypctx *hypctx;
	struct vtimer_cpu *vtimer_cpu;
	uint64_t ctl_el0;
	bool timer_toggled_on;

	hypctx = vcpu_get_cookie(vcpu);
	vtimer_cpu = &hypctx->vtimer_cpu;

	timer_toggled_on = false;
	ctl_el0 = vtimer_cpu->phys_timer.cntx_ctl_el0;

	if (!timer_enabled(ctl_el0) && timer_enabled(wval))
		timer_toggled_on = true;
	else if (timer_enabled(ctl_el0) && !timer_enabled(wval))
		vtimer_remove_irq(hypctx, vcpu);

	vtimer_cpu->phys_timer.cntx_ctl_el0 = wval;

	if (timer_toggled_on)
		vtimer_schedule_irq(hypctx, true);

	return (0);
}

int
vtimer_phys_cnt_read(struct vcpu *vcpu, uint64_t *rval, void *arg)
{
	struct vm *vm;
	struct hyp *hyp;

	vm = vcpu_vm(vcpu);
	hyp = vm_get_cookie(vm);
	*rval = READ_SPECIALREG(cntpct_el0) - hyp->vtimer.cntvoff_el2;
	return (0);
}

int
vtimer_phys_cnt_write(struct vcpu *vcpu, uint64_t wval, void *arg)
{
	return (0);
}

int
vtimer_phys_cval_read(struct vcpu *vcpu, uint64_t *rval, void *arg)
{
	struct hypctx *hypctx;
	struct vtimer_cpu *vtimer_cpu;

	hypctx = vcpu_get_cookie(vcpu);
	vtimer_cpu = &hypctx->vtimer_cpu;

	*rval = vtimer_cpu->phys_timer.cntx_cval_el0;

	return (0);
}

int
vtimer_phys_cval_write(struct vcpu *vcpu, uint64_t wval, void *arg)
{
	struct hypctx *hypctx;
	struct vtimer_cpu *vtimer_cpu;

	hypctx = vcpu_get_cookie(vcpu);
	vtimer_cpu = &hypctx->vtimer_cpu;

	vtimer_cpu->phys_timer.cntx_cval_el0 = wval;

	vtimer_remove_irq(hypctx, vcpu);
	if (timer_enabled(vtimer_cpu->phys_timer.cntx_ctl_el0)) {
		vtimer_schedule_irq(hypctx, true);
	}

	return (0);
}

int
vtimer_phys_tval_read(struct vcpu *vcpu, uint64_t *rval, void *arg)
{
	struct hyp *hyp;
	struct hypctx *hypctx;
	struct vtimer_cpu *vtimer_cpu;
	uint32_t cntpct_el0;

	hypctx = vcpu_get_cookie(vcpu);
	hyp = hypctx->hyp;
	vtimer_cpu = &hypctx->vtimer_cpu;

	if (!(vtimer_cpu->phys_timer.cntx_ctl_el0 & CNTP_CTL_ENABLE)) {
		/*
		 * ARMv8 Architecture Manual, p. D7-2702: the result of reading
		 * TVAL when the timer is disabled is UNKNOWN. I have chosen to
		 * return the maximum value possible on 32 bits which means the
		 * timer will fire very far into the future.
		 */
		*rval = (uint32_t)RES1;
	} else {
		cntpct_el0 = READ_SPECIALREG(cntpct_el0) -
		    hyp->vtimer.cntvoff_el2;
		*rval = vtimer_cpu->phys_timer.cntx_cval_el0 - cntpct_el0;
	}

	return (0);
}

int
vtimer_phys_tval_write(struct vcpu *vcpu, uint64_t wval, void *arg)
{
	struct hyp *hyp;
	struct hypctx *hypctx;
	struct vtimer_cpu *vtimer_cpu;
	uint64_t cntpct_el0;

	hypctx = vcpu_get_cookie(vcpu);
	hyp = hypctx->hyp;
	vtimer_cpu = &hypctx->vtimer_cpu;

	cntpct_el0 = READ_SPECIALREG(cntpct_el0) - hyp->vtimer.cntvoff_el2;
	vtimer_cpu->phys_timer.cntx_cval_el0 = (int32_t)wval + cntpct_el0;

	vtimer_remove_irq(hypctx, vcpu);
	if (timer_enabled(vtimer_cpu->phys_timer.cntx_ctl_el0)) {
		vtimer_schedule_irq(hypctx, true);
	}

	return (0);
}

struct vtimer_softc {
	struct resource *res;
	void *ihl;
	int rid;
};

static int
vtimer_probe(device_t dev)
{
	device_set_desc(dev, "Virtual timer");
	return (BUS_PROBE_DEFAULT);
}

static int
vtimer_attach(device_t dev)
{
	struct vtimer_softc *sc;

	sc = device_get_softc(dev);

	sc->rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->rid, RF_ACTIVE);
	if (sc->res == NULL)
		return (ENXIO);

	bus_setup_intr(dev, sc->res, INTR_TYPE_CLK, vtimer_virtual_timer_intr,
	    NULL, NULL, &sc->ihl);

	return (0);
}

static device_method_t vtimer_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vtimer_probe),
	DEVMETHOD(device_attach,	vtimer_attach),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_0(vtimer, vtimer_driver, vtimer_methods,
    sizeof(struct vtimer_softc));

DRIVER_MODULE(vtimer, generic_timer, vtimer_driver, 0, 0);
