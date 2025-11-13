/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Arm Ltd
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * Portions of this software were developed by Andrew Turner under sponsorship
 * from the FreeBSD Foundation.
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/event.h>
#include <sys/hwt.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/smp.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <arm64/spe/arm_spe.h>
#include <arm64/spe/arm_spe_dev.h>

MALLOC_DEFINE(M_ARM_SPE, "armspe", "Arm SPE tracing");

/*
 * taskqueue(9) used for sleepable routines called from interrupt handlers
 */
TASKQUEUE_FAST_DEFINE_THREAD(arm_spe);

void arm_spe_send_buffer(void *, int);
static void arm_spe_error(void *, int);
static int arm_spe_intr(void *);
device_attach_t arm_spe_attach;

static device_method_t arm_spe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_attach,        arm_spe_attach),

	DEVMETHOD_END,
};

DEFINE_CLASS_0(spe, arm_spe_driver, arm_spe_methods,
    sizeof(struct arm_spe_softc));

#define ARM_SPE_KVA_MAX_ALIGN	UL(2048)

int
arm_spe_attach(device_t dev)
{
	struct arm_spe_softc *sc;
	int error, rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->pmbidr = READ_SPECIALREG(PMBIDR_EL1_REG);
	sc->pmsidr = READ_SPECIALREG(PMSIDR_EL1_REG);
	device_printf(dev, "PMBIDR_EL1: %#lx\n", sc->pmbidr);
	device_printf(dev, "PMSIDR_EL1: %#lx\n", sc->pmsidr);
	if ((sc->pmbidr & PMBIDR_P) != 0) {
		device_printf(dev, "Profiling Buffer is owned by a higher Exception level\n");
		return (EPERM);
	}

	sc->kva_align = 1 << ((sc->pmbidr & PMBIDR_Align_MASK) >> PMBIDR_Align_SHIFT);
	if (sc->kva_align > ARM_SPE_KVA_MAX_ALIGN) {
		device_printf(dev, "Invalid PMBIDR.Align value of %d\n", sc->kva_align);
		return (EINVAL);
	}

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "Unable to allocate interrupt\n");
		return (ENXIO);
	}
	error = bus_setup_intr(dev, sc->sc_irq_res,
	    INTR_TYPE_MISC | INTR_MPSAFE, arm_spe_intr, NULL, sc,
	    &sc->sc_irq_cookie);
	if (error != 0) {
		device_printf(dev, "Unable to set up interrupt\n");
		return (error);
	}

	mtx_init(&sc->sc_lock, "Arm SPE lock", NULL, MTX_SPIN);

	STAILQ_INIT(&sc->pending);
	sc->npending = 0;

	spe_register(dev);

	return (0);
}

/* Interrupt handler runs on the same core that triggered the exception */
static int
arm_spe_intr(void *arg)
{
	int cpu_id = PCPU_GET(cpuid);
	struct arm_spe_softc *sc = arg;
	uint64_t pmbsr;
	uint64_t base, limit;
	uint8_t ec;
	struct arm_spe_info *info = &sc->spe_info[cpu_id];
	uint8_t i = info->buf_idx;
	struct arm_spe_buf_info *buf = &info->buf_info[i];
	struct arm_spe_buf_info *prev_buf = &info->buf_info[!i];
	device_t dev = sc->dev;

	/* Make sure the profiling data is visible to the CPU */
	psb_csync();
	dsb(nsh);

	/* Make sure any HW update of PMBPTR_EL1 is visible to the CPU */
	isb();

	pmbsr = READ_SPECIALREG(PMBSR_EL1_REG);

	if (!(pmbsr & PMBSR_S))
		return (FILTER_STRAY);

	/* Event Class */
	ec = PMBSR_EC_VAL(pmbsr);
	switch (ec)
	{
	case PMBSR_EC_OTHER_BUF_MGMT: /* Other buffer management event */
		break;
	case PMBSR_EC_GRAN_PROT_CHK: /* Granule Protection Check fault */
		device_printf(dev, "PMBSR_EC_GRAN_PROT_CHK\n");
		break;
	case PMBSR_EC_STAGE1_DA: /* Stage 1 Data Abort */
		device_printf(dev, "PMBSR_EC_STAGE1_DA\n");
		break;
	case PMBSR_EC_STAGE2_DA: /* Stage 2 Data Abort */
		device_printf(dev, "PMBSR_EC_STAGE2_DA\n");
		break;
	default:
		/* Unknown EC */
		device_printf(dev, "unknown PMBSR_EC: %#x\n", ec);
		arm_spe_disable(NULL);
		TASK_INIT(&sc->task, 0, (task_fn_t *)arm_spe_error, sc->ctx);
		taskqueue_enqueue(taskqueue_arm_spe, &sc->task);
		return (FILTER_HANDLED);
	}

	switch (ec) {
	case PMBSR_EC_OTHER_BUF_MGMT:
		/* Buffer Status Code = buffer filled */
		if ((pmbsr & PMBSR_MSS_BSC_MASK) == PMBSR_MSS_BSC_BUFFER_FILLED) {
			dprintf("%s SPE buffer full event (cpu:%d)\n",
			    __func__, cpu_id);
			break;
		}
	case PMBSR_EC_GRAN_PROT_CHK:
	case PMBSR_EC_STAGE1_DA:
	case PMBSR_EC_STAGE2_DA:
		/*
		 * If we have one of these, we've messed up the
		 * programming somehow (e.g. passed invalid memory to
		 * SPE) and can't recover
		 */
		arm_spe_disable(NULL);
		TASK_INIT(&sc->task, 0, (task_fn_t *)arm_spe_error, sc->ctx);
		taskqueue_enqueue(taskqueue_arm_spe, &sc->task);
		/* PMBPTR_EL1 is fault address if PMBSR_DL is 1 */
		device_printf(dev, "CPU:%d PMBSR_EL1:%#lx\n", cpu_id, pmbsr);
		device_printf(dev, "PMBPTR_EL1:%#lx PMBLIMITR_EL1:%#lx\n",
		    READ_SPECIALREG(PMBPTR_EL1_REG),
		    READ_SPECIALREG(PMBLIMITR_EL1_REG));
		return (FILTER_HANDLED);
	}

	mtx_lock_spin(&info->lock);

	/*
	 * Data Loss bit - pmbptr might not be pointing to the end of the last
	 * complete record
	 */
	if ((pmbsr & PMBSR_DL) == PMBSR_DL)
		buf->partial_rec = 1;
	buf->pmbptr = READ_SPECIALREG(PMBPTR_EL1_REG);
	buf->buf_svc = true;

	/* Setup regs ready to start writing to the other half of the buffer */
	info->buf_idx = !info->buf_idx;
	base = buf_start_addr(info->buf_idx, info);
	limit = base + (info->buf_size/2);
	limit &= PMBLIMITR_LIMIT_MASK;
	limit |= PMBLIMITR_E;
	WRITE_SPECIALREG(PMBPTR_EL1_REG, base);
	WRITE_SPECIALREG(PMBLIMITR_EL1_REG, limit);
	isb();

	/*
	 * Notify userspace via kqueue that buffer is full and needs copying
	 * out - since kqueue can sleep, don't do this in the interrupt handler,
	 * add to a taskqueue to be scheduled later instead
	 */
	TASK_INIT(&info->task[i], 0, (task_fn_t *)arm_spe_send_buffer, buf);
	taskqueue_enqueue(taskqueue_arm_spe, &info->task[i]);

	/*
	 * It's possible userspace hasn't yet notified us they've copied out the
	 * other half of the buffer
	 *
	 * This might be because:
	 *      a) Kernel hasn't scheduled the task via taskqueue to notify
	 *         userspace to copy out the data
	 *      b) Userspace is still copying the buffer or hasn't notified us
	 *         back via the HWT_IOC_SVC_BUF ioctl
	 *
	 * Either way we need to avoid overwriting uncopied data in the
	 * buffer, so disable profiling until we receive that SVC_BUF
	 * ioctl
	 *
	 * Using a larger buffer size should help to minimise these events and
	 * loss of profiling data while profiling is disabled
	 */
	if (prev_buf->buf_svc) {
		device_printf(sc->dev, "cpu%d: buffer full interrupt, but other"
		    " half of buffer has not been copied out - consider"
		    " increasing buffer size to minimise loss of profiling data\n",
		    cpu_id);
		WRITE_SPECIALREG(PMSCR_EL1_REG, 0x0);
		prev_buf->buf_wait = true;
	}

	mtx_unlock_spin(&info->lock);

	/* Clear Profiling Buffer Status Register */
	WRITE_SPECIALREG(PMBSR_EL1_REG, 0);

	isb();

	return (FILTER_HANDLED);
}

/* note: Scheduled and run via taskqueue, so can run on any CPU at any time */
void
arm_spe_send_buffer(void *arg, int pending __unused)
{
	struct arm_spe_buf_info *buf = (struct arm_spe_buf_info *)arg;
	struct arm_spe_info *info = buf->info;
	struct arm_spe_queue *queue;
	struct kevent kev;
	int ret;

	queue = malloc(sizeof(struct arm_spe_queue), M_ARM_SPE,
	    M_WAITOK | M_ZERO);

	mtx_lock_spin(&info->lock);

	/* Add to queue for userspace to pickup */
	queue->ident = info->ident;
	queue->offset = buf->pmbptr - buf_start_addr(buf->buf_idx, info);
	queue->buf_idx = buf->buf_idx;
	queue->final_buf = !info->enabled;
	queue->partial_rec = buf->partial_rec;
	mtx_unlock_spin(&info->lock);

	mtx_lock_spin(&info->sc->sc_lock);
	STAILQ_INSERT_TAIL(&info->sc->pending, queue, next);
	info->sc->npending++;
	EV_SET(&kev, ARM_SPE_KQ_BUF, EVFILT_USER, 0, NOTE_TRIGGER,
	    info->sc->npending, NULL);
	mtx_unlock_spin(&info->sc->sc_lock);

	/* Notify userspace */
	ret = kqfd_register(info->sc->kqueue_fd, &kev, info->sc->hwt_td,
	    M_WAITOK);
	if (ret) {
		dprintf("%s kqfd_register ret:%d\n", __func__, ret);
		arm_spe_error(info->sc->ctx, 0);
	}
}

static void
arm_spe_error(void *arg, int pending __unused)
{
	struct hwt_context *ctx = arg;
	struct kevent kev;
	int ret;

	smp_rendezvous_cpus(ctx->cpu_map, smp_no_rendezvous_barrier,
	    arm_spe_disable, smp_no_rendezvous_barrier, NULL);

	EV_SET(&kev, ARM_SPE_KQ_SHUTDOWN, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
	ret = kqfd_register(ctx->kqueue_fd, &kev, ctx->hwt_td, M_WAITOK);
	if (ret)
		dprintf("%s kqfd_register ret:%d\n", __func__, ret);
}

MODULE_DEPEND(spe, hwt, 1, 1, 1);
MODULE_VERSION(spe, 1);
