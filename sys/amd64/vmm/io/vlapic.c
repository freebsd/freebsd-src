/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/smp.h>

#include <machine/clock.h>
#include <x86/specialreg.h>
#include <x86/apicreg.h>

#include <machine/vmm.h>

#include "vmm_stat.h"
#include "vmm_lapic.h"
#include "vmm_ktr.h"
#include "vlapic.h"
#include "vioapic.h"

#define	VLAPIC_CTR0(vlapic, format)					\
	VCPU_CTR0((vlapic)->vm, (vlapic)->vcpuid, format)

#define	VLAPIC_CTR1(vlapic, format, p1)					\
	VCPU_CTR1((vlapic)->vm, (vlapic)->vcpuid, format, p1)

#define	VLAPIC_CTR2(vlapic, format, p1, p2)				\
	VCPU_CTR2((vlapic)->vm, (vlapic)->vcpuid, format, p1, p2)

#define	VLAPIC_CTR_IRR(vlapic, msg)					\
do {									\
	uint32_t *irrptr = &(vlapic)->apic.irr0;			\
	irrptr[0] = irrptr[0];	/* silence compiler */			\
	VLAPIC_CTR1((vlapic), msg " irr0 0x%08x", irrptr[0 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr1 0x%08x", irrptr[1 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr2 0x%08x", irrptr[2 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr3 0x%08x", irrptr[3 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr4 0x%08x", irrptr[4 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr5 0x%08x", irrptr[5 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr6 0x%08x", irrptr[6 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " irr7 0x%08x", irrptr[7 << 2]);	\
} while (0)

#define	VLAPIC_CTR_ISR(vlapic, msg)					\
do {									\
	uint32_t *isrptr = &(vlapic)->apic.isr0;			\
	isrptr[0] = isrptr[0];	/* silence compiler */			\
	VLAPIC_CTR1((vlapic), msg " isr0 0x%08x", isrptr[0 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr1 0x%08x", isrptr[1 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr2 0x%08x", isrptr[2 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr3 0x%08x", isrptr[3 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr4 0x%08x", isrptr[4 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr5 0x%08x", isrptr[5 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr6 0x%08x", isrptr[6 << 2]);	\
	VLAPIC_CTR1((vlapic), msg " isr7 0x%08x", isrptr[7 << 2]);	\
} while (0)

static MALLOC_DEFINE(M_VLAPIC, "vlapic", "vlapic");

#define	PRIO(x)			((x) >> 4)

#define VLAPIC_VERSION		(16)
#define VLAPIC_MAXLVT_ENTRIES	(5)

#define	x2apic(vlapic)	(((vlapic)->msr_apicbase & APICBASE_X2APIC) ? 1 : 0)

enum boot_state {
	BS_INIT,
	BS_SIPI,
	BS_RUNNING
};

struct vlapic {
	struct vm		*vm;
	int			vcpuid;

	struct LAPIC		apic;

	int			 esr_update;

	struct callout	callout;	/* vlapic timer */
	struct bintime	timer_fire_bt;	/* callout expiry time */
	struct bintime	timer_freq_bt;	/* timer frequency */
	struct bintime	timer_period_bt; /* timer period */
	struct mtx	timer_mtx;

	/*
	 * The 'isrvec_stk' is a stack of vectors injected by the local apic.
	 * A vector is popped from the stack when the processor does an EOI.
	 * The vector on the top of the stack is used to compute the
	 * Processor Priority in conjunction with the TPR.
	 */
	uint8_t			 isrvec_stk[ISRVEC_STK_SIZE];
	int			 isrvec_stk_top;

	uint64_t		msr_apicbase;
	enum boot_state		boot_state;
};

/*
 * The 'vlapic->timer_mtx' is used to provide mutual exclusion between the
 * vlapic_callout_handler() and vcpu accesses to the following registers:
 * - initial count register aka icr_timer
 * - current count register aka ccr_timer
 * - divide config register aka dcr_timer
 * - timer LVT register
 *
 * Note that the vlapic_callout_handler() does not write to any of these
 * registers so they can be safely read from the vcpu context without locking.
 */
#define	VLAPIC_TIMER_LOCK(vlapic)	mtx_lock_spin(&((vlapic)->timer_mtx))
#define	VLAPIC_TIMER_UNLOCK(vlapic)	mtx_unlock_spin(&((vlapic)->timer_mtx))
#define	VLAPIC_TIMER_LOCKED(vlapic)	mtx_owned(&((vlapic)->timer_mtx))

#define VLAPIC_BUS_FREQ	tsc_freq

static int
vlapic_timer_divisor(uint32_t dcr)
{
	switch (dcr & 0xB) {
	case APIC_TDCR_1:
		return (1);
	case APIC_TDCR_2:
		return (2);
	case APIC_TDCR_4:
		return (4);
	case APIC_TDCR_8:
		return (8);
	case APIC_TDCR_16:
		return (16);
	case APIC_TDCR_32:
		return (32);
	case APIC_TDCR_64:
		return (64);
	case APIC_TDCR_128:
		return (128);
	default:
		panic("vlapic_timer_divisor: invalid dcr 0x%08x", dcr);
	}
}

static void
vlapic_mask_lvts(uint32_t *lvts, int num_lvt)
{
	int i;
	for (i = 0; i < num_lvt; i++) {
		*lvts |= APIC_LVT_M;
		lvts += 4;
	}
}

#if 0
static inline void
vlapic_dump_lvt(uint32_t offset, uint32_t *lvt)
{
	printf("Offset %x: lvt %08x (V:%02x DS:%x M:%x)\n", offset,
	    *lvt, *lvt & APIC_LVTT_VECTOR, *lvt & APIC_LVTT_DS,
	    *lvt & APIC_LVTT_M);
}
#endif

static uint32_t
vlapic_get_ccr(struct vlapic *vlapic)
{
	struct bintime bt_now, bt_rem;
	struct LAPIC *lapic;
	uint32_t ccr;
	
	ccr = 0;
	lapic = &vlapic->apic;

	VLAPIC_TIMER_LOCK(vlapic);
	if (callout_active(&vlapic->callout)) {
		/*
		 * If the timer is scheduled to expire in the future then
		 * compute the value of 'ccr' based on the remaining time.
		 */
		binuptime(&bt_now);
		if (bintime_cmp(&vlapic->timer_fire_bt, &bt_now, >)) {
			bt_rem = vlapic->timer_fire_bt;
			bintime_sub(&bt_rem, &bt_now);
			ccr += bt_rem.sec * BT2FREQ(&vlapic->timer_freq_bt);
			ccr += bt_rem.frac / vlapic->timer_freq_bt.frac;
		}
	}
	KASSERT(ccr <= lapic->icr_timer, ("vlapic_get_ccr: invalid ccr %#x, "
	    "icr_timer is %#x", ccr, lapic->icr_timer));
	VLAPIC_CTR2(vlapic, "vlapic ccr_timer = %#x, icr_timer = %#x",
	    ccr, lapic->icr_timer);
	VLAPIC_TIMER_UNLOCK(vlapic);
	return (ccr);
}

static void
vlapic_set_dcr(struct vlapic *vlapic, uint32_t dcr)
{
	struct LAPIC *lapic;
	int divisor;
	
	lapic = &vlapic->apic;
	VLAPIC_TIMER_LOCK(vlapic);

	lapic->dcr_timer = dcr;
	divisor = vlapic_timer_divisor(dcr);
	VLAPIC_CTR2(vlapic, "vlapic dcr_timer=%#x, divisor=%d", dcr, divisor);

	/*
	 * Update the timer frequency and the timer period.
	 *
	 * XXX changes to the frequency divider will not take effect until
	 * the timer is reloaded.
	 */
	FREQ2BT(VLAPIC_BUS_FREQ / divisor, &vlapic->timer_freq_bt);
	vlapic->timer_period_bt = vlapic->timer_freq_bt;
	bintime_mul(&vlapic->timer_period_bt, lapic->icr_timer);

	VLAPIC_TIMER_UNLOCK(vlapic);
}

static void
vlapic_update_errors(struct vlapic *vlapic)
{
	struct LAPIC    *lapic = &vlapic->apic;
	lapic->esr = 0; // XXX 
}

static void
vlapic_init_ipi(struct vlapic *vlapic)
{
	struct LAPIC    *lapic = &vlapic->apic;
	lapic->version = VLAPIC_VERSION;
	lapic->version |= (VLAPIC_MAXLVT_ENTRIES < MAXLVTSHIFT);
	lapic->dfr = 0xffffffff;
	lapic->svr = APIC_SVR_VECTOR;
	vlapic_mask_lvts(&lapic->lvt_timer, VLAPIC_MAXLVT_ENTRIES+1);
}

static int
vlapic_reset(struct vlapic *vlapic)
{
	struct LAPIC	*lapic = &vlapic->apic;

	memset(lapic, 0, sizeof(*lapic));
	lapic->apr = vlapic->vcpuid;
	vlapic_init_ipi(vlapic);
	vlapic_set_dcr(vlapic, 0);

	if (vlapic->vcpuid == 0)
		vlapic->boot_state = BS_RUNNING;	/* BSP */
	else
		vlapic->boot_state = BS_INIT;		/* AP */
	
	return 0;

}

void
vlapic_set_intr_ready(struct vlapic *vlapic, int vector, bool level)
{
	struct LAPIC	*lapic = &vlapic->apic;
	uint32_t	*irrptr, *tmrptr, mask;
	int		idx;

	if (vector < 0 || vector >= 256)
		panic("vlapic_set_intr_ready: invalid vector %d\n", vector);

	if (!(lapic->svr & APIC_SVR_ENABLE)) {
		VLAPIC_CTR1(vlapic, "vlapic is software disabled, ignoring "
		    "interrupt %d", vector);
		return;
	}

	idx = (vector / 32) * 4;
	mask = 1 << (vector % 32);

	irrptr = &lapic->irr0;
	atomic_set_int(&irrptr[idx], mask);

	/*
	 * Upon acceptance of an interrupt into the IRR the corresponding
	 * TMR bit is cleared for edge-triggered interrupts and set for
	 * level-triggered interrupts.
	 */
	tmrptr = &lapic->tmr0;
	if (level)
		atomic_set_int(&tmrptr[idx], mask);
	else
		atomic_clear_int(&tmrptr[idx], mask);

	VLAPIC_CTR_IRR(vlapic, "vlapic_set_intr_ready");
}

static __inline uint32_t *
vlapic_get_lvtptr(struct vlapic *vlapic, uint32_t offset)
{
	struct LAPIC	*lapic = &vlapic->apic;
	int 		 i;

	if (offset < APIC_OFFSET_TIMER_LVT || offset > APIC_OFFSET_ERROR_LVT) {
		panic("vlapic_get_lvt: invalid LVT\n");
	}
	i = (offset - APIC_OFFSET_TIMER_LVT) >> 2;
	return ((&lapic->lvt_timer) + i);;
}

static __inline uint32_t
vlapic_get_lvt(struct vlapic *vlapic, uint32_t offset)
{

	return (*vlapic_get_lvtptr(vlapic, offset));
}

static void
vlapic_set_lvt(struct vlapic *vlapic, uint32_t offset, uint32_t val)
{
	uint32_t *lvtptr;
	struct LAPIC *lapic;
	
	lapic = &vlapic->apic;
	lvtptr = vlapic_get_lvtptr(vlapic, offset);	

	if (offset == APIC_OFFSET_TIMER_LVT)
		VLAPIC_TIMER_LOCK(vlapic);

	if (!(lapic->svr & APIC_SVR_ENABLE))
		val |= APIC_LVT_M;
	*lvtptr = val;

	if (offset == APIC_OFFSET_TIMER_LVT)
		VLAPIC_TIMER_UNLOCK(vlapic);
}

#if 1
static void
dump_isrvec_stk(struct vlapic *vlapic)
{
	int i;
	uint32_t *isrptr;

	isrptr = &vlapic->apic.isr0;
	for (i = 0; i < 8; i++)
		printf("ISR%d 0x%08x\n", i, isrptr[i * 4]);

	for (i = 0; i <= vlapic->isrvec_stk_top; i++)
		printf("isrvec_stk[%d] = %d\n", i, vlapic->isrvec_stk[i]);
}
#endif

/*
 * Algorithm adopted from section "Interrupt, Task and Processor Priority"
 * in Intel Architecture Manual Vol 3a.
 */
static void
vlapic_update_ppr(struct vlapic *vlapic)
{
	int isrvec, tpr, ppr;

	/*
	 * Note that the value on the stack at index 0 is always 0.
	 *
	 * This is a placeholder for the value of ISRV when none of the
	 * bits is set in the ISRx registers.
	 */
	isrvec = vlapic->isrvec_stk[vlapic->isrvec_stk_top];
	tpr = vlapic->apic.tpr;

#if 1
	{
		int i, lastprio, curprio, vector, idx;
		uint32_t *isrptr;

		if (vlapic->isrvec_stk_top == 0 && isrvec != 0)
			panic("isrvec_stk is corrupted: %d", isrvec);

		/*
		 * Make sure that the priority of the nested interrupts is
		 * always increasing.
		 */
		lastprio = -1;
		for (i = 1; i <= vlapic->isrvec_stk_top; i++) {
			curprio = PRIO(vlapic->isrvec_stk[i]);
			if (curprio <= lastprio) {
				dump_isrvec_stk(vlapic);
				panic("isrvec_stk does not satisfy invariant");
			}
			lastprio = curprio;
		}

		/*
		 * Make sure that each bit set in the ISRx registers has a
		 * corresponding entry on the isrvec stack.
		 */
		i = 1;
		isrptr = &vlapic->apic.isr0;
		for (vector = 0; vector < 256; vector++) {
			idx = (vector / 32) * 4;
			if (isrptr[idx] & (1 << (vector % 32))) {
				if (i > vlapic->isrvec_stk_top ||
				    vlapic->isrvec_stk[i] != vector) {
					dump_isrvec_stk(vlapic);
					panic("ISR and isrvec_stk out of sync");
				}
				i++;
			}
		}
	}
#endif

	if (PRIO(tpr) >= PRIO(isrvec))
		ppr = tpr;
	else
		ppr = isrvec & 0xf0;

	vlapic->apic.ppr = ppr;
	VLAPIC_CTR1(vlapic, "vlapic_update_ppr 0x%02x", ppr);
}

static void
vlapic_process_eoi(struct vlapic *vlapic)
{
	struct LAPIC	*lapic = &vlapic->apic;
	uint32_t	*isrptr, *tmrptr;
	int		i, idx, bitpos, vector;

	isrptr = &lapic->isr0;
	tmrptr = &lapic->tmr0;

	/*
	 * The x86 architecture reserves the the first 32 vectors for use
	 * by the processor.
	 */
	for (i = 7; i > 0; i--) {
		idx = i * 4;
		bitpos = fls(isrptr[idx]);
		if (bitpos-- != 0) {
			if (vlapic->isrvec_stk_top <= 0) {
				panic("invalid vlapic isrvec_stk_top %d",
				      vlapic->isrvec_stk_top);
			}
			isrptr[idx] &= ~(1 << bitpos);
			VLAPIC_CTR_ISR(vlapic, "vlapic_process_eoi");
			vlapic->isrvec_stk_top--;
			vlapic_update_ppr(vlapic);
			if ((tmrptr[idx] & (1 << bitpos)) != 0) {
				vector = i * 32 + bitpos;
				vioapic_process_eoi(vlapic->vm, vlapic->vcpuid,
				    vector);
			}
			return;
		}
	}
}

static __inline int
vlapic_get_lvt_field(uint32_t lvt, uint32_t mask)
{

	return (lvt & mask);
}

static __inline int
vlapic_periodic_timer(struct vlapic *vlapic)
{
	uint32_t lvt;
	
	lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_TIMER_LVT);

	return (vlapic_get_lvt_field(lvt, APIC_LVTT_TM_PERIODIC));
}

static VMM_STAT(VLAPIC_INTR_TIMER, "timer interrupts generated by vlapic");

static void
vlapic_fire_timer(struct vlapic *vlapic)
{
	int vector;
	uint32_t lvt;

	KASSERT(VLAPIC_TIMER_LOCKED(vlapic), ("vlapic_fire_timer not locked"));
	
	lvt = vlapic_get_lvt(vlapic, APIC_OFFSET_TIMER_LVT);

	if (!vlapic_get_lvt_field(lvt, APIC_LVTT_M)) {
		vmm_stat_incr(vlapic->vm, vlapic->vcpuid, VLAPIC_INTR_TIMER, 1);
		vector = vlapic_get_lvt_field(lvt, APIC_LVTT_VECTOR);
		vlapic_set_intr_ready(vlapic, vector, false);
		vcpu_notify_event(vlapic->vm, vlapic->vcpuid);
	}
}

static void
vlapic_callout_handler(void *arg)
{
	struct vlapic *vlapic;
	struct bintime bt, btnow;
	sbintime_t rem_sbt;

	vlapic = arg;

	VLAPIC_TIMER_LOCK(vlapic);
	if (callout_pending(&vlapic->callout))	/* callout was reset */
		goto done;

	if (!callout_active(&vlapic->callout))	/* callout was stopped */
		goto done;

	callout_deactivate(&vlapic->callout);

	KASSERT(vlapic->apic.icr_timer != 0, ("vlapic timer is disabled"));

	vlapic_fire_timer(vlapic);

	if (vlapic_periodic_timer(vlapic)) {
		binuptime(&btnow);
		KASSERT(bintime_cmp(&btnow, &vlapic->timer_fire_bt, >=),
		    ("vlapic callout at %#lx.%#lx, expected at %#lx.#%lx",
		    btnow.sec, btnow.frac, vlapic->timer_fire_bt.sec,
		    vlapic->timer_fire_bt.frac));

		/*
		 * Compute the delta between when the timer was supposed to
		 * fire and the present time.
		 */
		bt = btnow;
		bintime_sub(&bt, &vlapic->timer_fire_bt);

		rem_sbt = bttosbt(vlapic->timer_period_bt);
		if (bintime_cmp(&bt, &vlapic->timer_period_bt, <)) {
			/*
			 * Adjust the time until the next countdown downward
			 * to account for the lost time.
			 */
			rem_sbt -= bttosbt(bt);
		} else {
			/*
			 * If the delta is greater than the timer period then
			 * just reset our time base instead of trying to catch
			 * up.
			 */
			vlapic->timer_fire_bt = btnow;
			VLAPIC_CTR2(vlapic, "vlapic timer lagging by %lu "
			    "usecs, period is %lu usecs - resetting time base",
			    bttosbt(bt) / SBT_1US,
			    bttosbt(vlapic->timer_period_bt) / SBT_1US);
		}

		bintime_add(&vlapic->timer_fire_bt, &vlapic->timer_period_bt);
		callout_reset_sbt(&vlapic->callout, rem_sbt, 0,
		    vlapic_callout_handler, vlapic, 0);
	}
done:
	VLAPIC_TIMER_UNLOCK(vlapic);
}

static void
vlapic_set_icr_timer(struct vlapic *vlapic, uint32_t icr_timer)
{
	struct LAPIC *lapic;
	sbintime_t sbt;

	VLAPIC_TIMER_LOCK(vlapic);

	lapic = &vlapic->apic;
	lapic->icr_timer = icr_timer;

	vlapic->timer_period_bt = vlapic->timer_freq_bt;
	bintime_mul(&vlapic->timer_period_bt, icr_timer);

	if (icr_timer != 0) {
		binuptime(&vlapic->timer_fire_bt);
		bintime_add(&vlapic->timer_fire_bt, &vlapic->timer_period_bt);

		sbt = bttosbt(vlapic->timer_period_bt);
		callout_reset_sbt(&vlapic->callout, sbt, 0,
		    vlapic_callout_handler, vlapic, 0);
	} else
		callout_stop(&vlapic->callout);

	VLAPIC_TIMER_UNLOCK(vlapic);
}

static VMM_STAT_ARRAY(IPIS_SENT, VM_MAXCPU, "ipis sent to vcpu");

static int
lapic_process_icr(struct vlapic *vlapic, uint64_t icrval, bool *retu)
{
	int i;
	cpuset_t dmask;
	uint32_t dest, vec, mode;
	struct vlapic *vlapic2;
	struct vm_exit *vmexit;
	
	if (x2apic(vlapic))
		dest = icrval >> 32;
	else
		dest = icrval >> (32 + 24);
	vec = icrval & APIC_VECTOR_MASK;
	mode = icrval & APIC_DELMODE_MASK;

	if (mode == APIC_DELMODE_FIXED || mode == APIC_DELMODE_NMI) {
		switch (icrval & APIC_DEST_MASK) {
		case APIC_DEST_DESTFLD:
			CPU_SETOF(dest, &dmask);
			break;
		case APIC_DEST_SELF:
			CPU_SETOF(vlapic->vcpuid, &dmask);
			break;
		case APIC_DEST_ALLISELF:
			dmask = vm_active_cpus(vlapic->vm);
			break;
		case APIC_DEST_ALLESELF:
			dmask = vm_active_cpus(vlapic->vm);
			CPU_CLR(vlapic->vcpuid, &dmask);
			break;
		default:
			CPU_ZERO(&dmask);	/* satisfy gcc */
			break;
		}

		while ((i = CPU_FFS(&dmask)) != 0) {
			i--;
			CPU_CLR(i, &dmask);
			if (mode == APIC_DELMODE_FIXED) {
				lapic_intr_edge(vlapic->vm, i, vec);
				vmm_stat_array_incr(vlapic->vm, vlapic->vcpuid,
						    IPIS_SENT, i, 1);
			} else
				vm_inject_nmi(vlapic->vm, i);
		}

		return (0);	/* handled completely in the kernel */
	}

	if (mode == APIC_DELMODE_INIT) {
		if ((icrval & APIC_LEVEL_MASK) == APIC_LEVEL_DEASSERT)
			return (0);

		if (vlapic->vcpuid == 0 && dest != 0 && dest < VM_MAXCPU) {
			vlapic2 = vm_lapic(vlapic->vm, dest);

			/* move from INIT to waiting-for-SIPI state */
			if (vlapic2->boot_state == BS_INIT) {
				vlapic2->boot_state = BS_SIPI;
			}

			return (0);
		}
	}

	if (mode == APIC_DELMODE_STARTUP) {
		if (vlapic->vcpuid == 0 && dest != 0 && dest < VM_MAXCPU) {
			vlapic2 = vm_lapic(vlapic->vm, dest);

			/*
			 * Ignore SIPIs in any state other than wait-for-SIPI
			 */
			if (vlapic2->boot_state != BS_SIPI)
				return (0);

			/*
			 * XXX this assumes that the startup IPI always succeeds
			 */
			vlapic2->boot_state = BS_RUNNING;
			vm_activate_cpu(vlapic2->vm, dest);

			*retu = true;
			vmexit = vm_exitinfo(vlapic->vm, vlapic->vcpuid);
			vmexit->exitcode = VM_EXITCODE_SPINUP_AP;
			vmexit->u.spinup_ap.vcpu = dest;
			vmexit->u.spinup_ap.rip = vec << PAGE_SHIFT;

			return (0);
		}
	}

	/*
	 * This will cause a return to userland.
	 */
	return (1);
}

int
vlapic_pending_intr(struct vlapic *vlapic)
{
	struct LAPIC	*lapic = &vlapic->apic;
	int	  	 idx, i, bitpos, vector;
	uint32_t	*irrptr, val;

	irrptr = &lapic->irr0;

	/*
	 * The x86 architecture reserves the the first 32 vectors for use
	 * by the processor.
	 */
	for (i = 7; i > 0; i--) {
		idx = i * 4;
		val = atomic_load_acq_int(&irrptr[idx]);
		bitpos = fls(val);
		if (bitpos != 0) {
			vector = i * 32 + (bitpos - 1);
			if (PRIO(vector) > PRIO(lapic->ppr)) {
				VLAPIC_CTR1(vlapic, "pending intr %d", vector);
				return (vector);
			} else 
				break;
		}
	}
	return (-1);
}

void
vlapic_intr_accepted(struct vlapic *vlapic, int vector)
{
	struct LAPIC	*lapic = &vlapic->apic;
	uint32_t	*irrptr, *isrptr;
	int		idx, stk_top;

	/*
	 * clear the ready bit for vector being accepted in irr 
	 * and set the vector as in service in isr.
	 */
	idx = (vector / 32) * 4;

	irrptr = &lapic->irr0;
	atomic_clear_int(&irrptr[idx], 1 << (vector % 32));
	VLAPIC_CTR_IRR(vlapic, "vlapic_intr_accepted");

	isrptr = &lapic->isr0;
	isrptr[idx] |= 1 << (vector % 32);
	VLAPIC_CTR_ISR(vlapic, "vlapic_intr_accepted");

	/*
	 * Update the PPR
	 */
	vlapic->isrvec_stk_top++;

	stk_top = vlapic->isrvec_stk_top;
	if (stk_top >= ISRVEC_STK_SIZE)
		panic("isrvec_stk_top overflow %d", stk_top);

	vlapic->isrvec_stk[stk_top] = vector;
	vlapic_update_ppr(vlapic);
}

static void
lapic_set_svr(struct vlapic *vlapic, uint32_t new)
{
	struct LAPIC *lapic;
	uint32_t old, changed;

	lapic = &vlapic->apic;
	old = lapic->svr;
	changed = old ^ new;
	if ((changed & APIC_SVR_ENABLE) != 0) {
		if ((new & APIC_SVR_ENABLE) == 0) {
			/*
			 * The apic is now disabled so stop the apic timer.
			 */
			VLAPIC_CTR0(vlapic, "vlapic is software-disabled");
			VLAPIC_TIMER_LOCK(vlapic);
			callout_stop(&vlapic->callout);
			VLAPIC_TIMER_UNLOCK(vlapic);
		} else {
			/*
			 * The apic is now enabled so restart the apic timer
			 * if it is configured in periodic mode.
			 */
			VLAPIC_CTR0(vlapic, "vlapic is software-enabled");
			if (vlapic_periodic_timer(vlapic))
				vlapic_set_icr_timer(vlapic, lapic->icr_timer);
		}
	}
	lapic->svr = new;
}

int
vlapic_read(struct vlapic *vlapic, uint64_t offset, uint64_t *data, bool *retu)
{
	struct LAPIC	*lapic = &vlapic->apic;
	uint32_t	*reg;
	int		 i;

	if (offset > sizeof(*lapic)) {
		*data = 0;
		goto done;
	}
	
	offset &= ~3;
	switch(offset)
	{
		case APIC_OFFSET_ID:
			if (x2apic(vlapic))
				*data = vlapic->vcpuid;
			else
				*data = vlapic->vcpuid << 24;
			break;
		case APIC_OFFSET_VER:
			*data = lapic->version;
			break;
		case APIC_OFFSET_TPR:
			*data = lapic->tpr;
			break;
		case APIC_OFFSET_APR:
			*data = lapic->apr;
			break;
		case APIC_OFFSET_PPR:
			*data = lapic->ppr;
			break;
		case APIC_OFFSET_EOI:
			*data = lapic->eoi;
			break;
		case APIC_OFFSET_LDR:
			*data = lapic->ldr;
			break;
		case APIC_OFFSET_DFR:
			*data = lapic->dfr;
			break;
		case APIC_OFFSET_SVR:
			*data = lapic->svr;
			break;
		case APIC_OFFSET_ISR0 ... APIC_OFFSET_ISR7:
			i = (offset - APIC_OFFSET_ISR0) >> 2;
			reg = &lapic->isr0;
			*data = *(reg + i);
			break;
		case APIC_OFFSET_TMR0 ... APIC_OFFSET_TMR7:
			i = (offset - APIC_OFFSET_TMR0) >> 2;
			reg = &lapic->tmr0;
			*data = *(reg + i);
			break;
		case APIC_OFFSET_IRR0 ... APIC_OFFSET_IRR7:
			i = (offset - APIC_OFFSET_IRR0) >> 2;
			reg = &lapic->irr0;
			*data = atomic_load_acq_int(reg + i);
			break;
		case APIC_OFFSET_ESR:
			*data = lapic->esr;
			break;
		case APIC_OFFSET_ICR_LOW: 
			*data = lapic->icr_lo;
			break;
		case APIC_OFFSET_ICR_HI: 
			*data = lapic->icr_hi;
			break;
		case APIC_OFFSET_TIMER_LVT ... APIC_OFFSET_ERROR_LVT:
			*data = vlapic_get_lvt(vlapic, offset);	
			break;
		case APIC_OFFSET_ICR:
			*data = lapic->icr_timer;
			break;
		case APIC_OFFSET_CCR:
			*data = vlapic_get_ccr(vlapic);
			break;
		case APIC_OFFSET_DCR:
			*data = lapic->dcr_timer;
			break;
		case APIC_OFFSET_RRR:
		default:
			*data = 0;
			break;
	}
done:
	VLAPIC_CTR2(vlapic, "vlapic read offset %#x, data %#lx", offset, *data);
	return 0;
}

int
vlapic_write(struct vlapic *vlapic, uint64_t offset, uint64_t data, bool *retu)
{
	struct LAPIC	*lapic = &vlapic->apic;
	int		retval;

	VLAPIC_CTR2(vlapic, "vlapic write offset %#x, data %#lx", offset, data);

	if (offset > sizeof(*lapic)) {
		return 0;
	}

	retval = 0;
	offset &= ~3;
	switch(offset)
	{
		case APIC_OFFSET_ID:
			break;
		case APIC_OFFSET_TPR:
			lapic->tpr = data & 0xff;
			vlapic_update_ppr(vlapic);
			break;
		case APIC_OFFSET_EOI:
			vlapic_process_eoi(vlapic);
			break;
		case APIC_OFFSET_LDR:
			break;
		case APIC_OFFSET_DFR:
			break;
		case APIC_OFFSET_SVR:
			lapic_set_svr(vlapic, data);
			break;
		case APIC_OFFSET_ICR_LOW: 
			if (!x2apic(vlapic)) {
				data &= 0xffffffff;
				data |= (uint64_t)lapic->icr_hi << 32;
			}
			retval = lapic_process_icr(vlapic, data, retu);
			break;
		case APIC_OFFSET_ICR_HI:
			if (!x2apic(vlapic)) {
				retval = 0;
				lapic->icr_hi = data;
			}
			break;
		case APIC_OFFSET_TIMER_LVT ... APIC_OFFSET_ERROR_LVT:
			vlapic_set_lvt(vlapic, offset, data);
			break;
		case APIC_OFFSET_ICR:
			vlapic_set_icr_timer(vlapic, data);
			break;

		case APIC_OFFSET_DCR:
			vlapic_set_dcr(vlapic, data);
			break;

		case APIC_OFFSET_ESR:
			vlapic_update_errors(vlapic);
			break;
		case APIC_OFFSET_VER:
		case APIC_OFFSET_APR:
		case APIC_OFFSET_PPR:
		case APIC_OFFSET_RRR:
		case APIC_OFFSET_ISR0 ... APIC_OFFSET_ISR7:
		case APIC_OFFSET_TMR0 ... APIC_OFFSET_TMR7:
		case APIC_OFFSET_IRR0 ... APIC_OFFSET_IRR7:
		case APIC_OFFSET_CCR:
		default:
			// Read only.
			break;
	}

	return (retval);
}

struct vlapic *
vlapic_init(struct vm *vm, int vcpuid)
{
	struct vlapic 		*vlapic;

	vlapic = malloc(sizeof(struct vlapic), M_VLAPIC, M_WAITOK | M_ZERO);
	vlapic->vm = vm;
	vlapic->vcpuid = vcpuid;

	/*
	 * If the vlapic is configured in x2apic mode then it will be
	 * accessed in the critical section via the MSR emulation code.
	 *
	 * Therefore the timer mutex must be a spinlock because blockable
	 * mutexes cannot be acquired in a critical section.
	 */
	mtx_init(&vlapic->timer_mtx, "vlapic timer mtx", NULL, MTX_SPIN);
	callout_init(&vlapic->callout, 1);

	vlapic->msr_apicbase = DEFAULT_APIC_BASE | APICBASE_ENABLED;

	if (vcpuid == 0)
		vlapic->msr_apicbase |= APICBASE_BSP;

	vlapic_reset(vlapic);

	return (vlapic);
}

void
vlapic_cleanup(struct vlapic *vlapic)
{

	callout_drain(&vlapic->callout);
	free(vlapic, M_VLAPIC);
}

uint64_t
vlapic_get_apicbase(struct vlapic *vlapic)
{

	return (vlapic->msr_apicbase);
}

void
vlapic_set_apicbase(struct vlapic *vlapic, uint64_t val)
{
	int err;
	enum x2apic_state state;

	err = vm_get_x2apic_state(vlapic->vm, vlapic->vcpuid, &state);
	if (err)
		panic("vlapic_set_apicbase: err %d fetching x2apic state", err);

	if (state == X2APIC_DISABLED)
		val &= ~APICBASE_X2APIC;

	vlapic->msr_apicbase = val;
}

void
vlapic_set_x2apic_state(struct vm *vm, int vcpuid, enum x2apic_state state)
{
	struct vlapic *vlapic;

	vlapic = vm_lapic(vm, vcpuid);

	if (state == X2APIC_DISABLED)
		vlapic->msr_apicbase &= ~APICBASE_X2APIC;
}

bool
vlapic_enabled(struct vlapic *vlapic)
{
	struct LAPIC *lapic = &vlapic->apic;

	if ((vlapic->msr_apicbase & APICBASE_ENABLED) != 0 &&
	    (lapic->svr & APIC_SVR_ENABLE) != 0)
		return (true);
	else
		return (false);
}
