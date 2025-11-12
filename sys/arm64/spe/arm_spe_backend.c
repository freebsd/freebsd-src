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

/*
 * Arm Statistical Profiling Extension (SPE) backend
 *
 * Basic SPE operation
 *
 *   SPE is enabled and configured on a per-core basis, with each core requiring
 *   separate code to enable and configure. Each core also requires a separate
 *   buffer passed as config where the CPU will write profiling data. When the
 *   profiling buffer is full, an interrupt will be taken on the same CPU.
 *
 * Driver Design
 *
 * - HWT allocates a large single buffer per core. This buffer is split in half
 *   to create a 2 element circular buffer (aka ping-pong buffer) where the
 *   kernel writes to one half while userspace is copying the other half
 * - SMP calls are used to enable and configure each core, with SPE initially
 *   configured to write to the first half of the buffer
 * - When the first half of the buffer is full, a buffer full interrupt will
 *   immediately switch writing to the second half. The kernel adds the details
 *   of the half that needs copying to a FIFO STAILQ and notifies userspace via
 *   kqueue by sending a ARM_SPE_KQ_BUF kevent with how many buffers on the
 *   queue need servicing
 * - The kernel responds to HWT_IOC_BUFPTR_GET ioctl by sending details of the
 *   first item from the queue
 * - The buffers pending copying will not be overwritten until an
 *   HWT_IOC_SVC_BUF ioctl is received from userspace confirming the data has
 *   been copied out
 * - In the case where both halfs of the buffer are full, profiling will be
 *   paused until notification via HWT_IOC_SVC_BUF is received
 *
 * Future improvements and limitations
 *
 * - Using large buffer sizes should minimise pauses and loss of profiling
 *   data while kernel is waiting for userspace to copy out data. Since it is
 *   generally expected that consuming (copying) this data is faster than
 *   producing it, in practice this has not so far been an issue. If it does
 *   prove to be an issue even with large buffer sizes then additional buffering
 *   i.e. n element circular buffers might be required.
 *
 * - kqueue can only notify and queue one kevent of the same type, with
 *   subsequent events overwriting data in the first event. The kevent
 *   ARM_SPE_KQ_BUF can therefore only contain the number of buffers on the
 *   STAILQ, incrementing each time a new buffer is full. In this case kqueue
 *   serves just as a notification to userspace to wake up and query the kernel
 *   with the appropriate ioctl. An alternative might be custom kevents where
 *   the kevent identifier is encoded with something like n+cpu_id or n+tid. In
 *   this case data could be sent directly with kqueue via the kevent data and
 *   fflags elements, avoiding the extra ioctl.
 *
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/hwt.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/rwlock.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <arm64/spe/arm_spe_dev.h>

#include <dev/hwt/hwt_vm.h>
#include <dev/hwt/hwt_backend.h>
#include <dev/hwt/hwt_config.h>
#include <dev/hwt/hwt_context.h>
#include <dev/hwt/hwt_cpu.h>
#include <dev/hwt/hwt_thread.h>

MALLOC_DECLARE(M_ARM_SPE);

extern u_int mp_maxid;
extern struct taskqueue *taskqueue_arm_spe;

int spe_backend_disable_smp(struct hwt_context *ctx);

static device_t spe_dev;
static struct hwt_backend_ops spe_ops;
static struct hwt_backend backend = {
	.ops = &spe_ops,
	.name = "spe",
	.kva_req = 1,
};

static struct arm_spe_info *spe_info;

static int
spe_backend_init_thread(struct hwt_context *ctx)
{
	return (ENOTSUP);
}

static void
spe_backend_init_cpu(struct hwt_context *ctx)
{
	struct arm_spe_info *info;
	struct arm_spe_softc *sc = device_get_softc(spe_dev);
	char lock_name[32];
	char *tmp = "Arm SPE lock/cpu/";
	int cpu_id;

	spe_info = malloc(sizeof(struct arm_spe_info) * mp_ncpus,
	   M_ARM_SPE, M_WAITOK | M_ZERO);

	sc->spe_info = spe_info;

	CPU_FOREACH_ISSET(cpu_id, &ctx->cpu_map) {
		info = &spe_info[cpu_id];
		info->sc = sc;
		info->ident = cpu_id;
		info->buf_info[0].info = info;
		info->buf_info[0].buf_idx = 0;
		info->buf_info[1].info = info;
		info->buf_info[1].buf_idx = 1;
		snprintf(lock_name, sizeof(lock_name), "%s%d", tmp, cpu_id);
		mtx_init(&info->lock, lock_name, NULL, MTX_SPIN);
	}
}

static int
spe_backend_init(struct hwt_context *ctx)
{
	struct arm_spe_softc *sc = device_get_softc(spe_dev);
	int error = 0;

	/*
	 * HWT currently specifies buffer size must be a multiple of PAGE_SIZE,
	 * i.e. minimum 4KB + the maximum PMBIDR.Align is 2KB
	 * This should never happen but it's good to sense check
	 */
	if (ctx->bufsize % sc->kva_align != 0)
		return (EINVAL);

	/*
	 * Since we're splitting the buffer in half + PMBLIMITR needs to be page
	 * aligned, minimum buffer size needs to be 2x PAGE_SIZE
	 */
	if (ctx->bufsize < (2 * PAGE_SIZE))
		return (EINVAL);

	sc->ctx = ctx;
	sc->kqueue_fd = ctx->kqueue_fd;
	sc->hwt_td = ctx->hwt_td;

	if (ctx->mode == HWT_MODE_THREAD)
		error = spe_backend_init_thread(ctx);
	else
		spe_backend_init_cpu(ctx);

	return (error);
}

#ifdef ARM_SPE_DEBUG
static void hex_dump(uint8_t *buf, size_t len)
{
	size_t i;

	printf("--------------------------------------------------------------\n");
	for (i = 0; i < len; ++i) {
		if (i % 8 == 0) {
			printf(" ");
		}
		if (i % 16 == 0) {
			if (i != 0) {
				printf("\r\n");
			}
			printf("\t");
		}
		printf("%02X ", buf[i]);
	}
	printf("\r\n");
}
#endif

static int
spe_backend_deinit(struct hwt_context *ctx)
{
#ifdef ARM_SPE_DEBUG
	struct arm_spe_info *info;
	int cpu_id;

	CPU_FOREACH_ISSET(cpu_id, &ctx->cpu_map) {
		info = &spe_info[cpu_id];
		hex_dump((void *)info->kvaddr, 128);
		hex_dump((void *)(info->kvaddr + (info->buf_size/2)), 128);
	}
#endif

	if (ctx->state == CTX_STATE_RUNNING) {
		spe_backend_disable_smp(ctx);
		ctx->state = CTX_STATE_STOPPED;
	}

	free(spe_info, M_ARM_SPE);

	return (0);
}

static uint64_t
arm_spe_min_interval(struct arm_spe_softc *sc)
{
	/* IMPLEMENTATION DEFINED */
	switch (PMSIDR_Interval_VAL(sc->pmsidr))
	{
	case PMSIDR_Interval_256:
		return (256);
	case PMSIDR_Interval_512:
		return (512);
	case PMSIDR_Interval_768:
		return (768);
	case PMSIDR_Interval_1024:
		return (1024);
	case PMSIDR_Interval_1536:
		return (1536);
	case PMSIDR_Interval_2048:
		return (2048);
	case PMSIDR_Interval_3072:
		return (3072);
	case PMSIDR_Interval_4096:
		return (4096);
	default:
		return (4096);
	}
}

static inline void
arm_spe_set_interval(struct arm_spe_info *info, uint64_t interval)
{
	uint64_t min_interval = arm_spe_min_interval(info->sc);

	interval = MAX(interval, min_interval);
	interval = MIN(interval, 1 << 24);      /* max 24 bits */

	dprintf("%s %lu\n", __func__, interval);

	info->pmsirr &= ~(PMSIRR_INTERVAL_MASK);
	info->pmsirr |= (interval << PMSIRR_INTERVAL_SHIFT);
}

static int
spe_backend_configure(struct hwt_context *ctx, int cpu_id, int session_id)
{
	struct arm_spe_info *info = &spe_info[cpu_id];
	struct arm_spe_config *cfg;
	int err = 0;

	mtx_lock_spin(&info->lock);
	info->ident = cpu_id;
	/* Set defaults */
	info->pmsfcr = 0;
	info->pmsevfr = 0xFFFFFFFFFFFFFFFFUL;
	info->pmslatfr = 0;
	info->pmsirr =
	    (arm_spe_min_interval(info->sc) << PMSIRR_INTERVAL_SHIFT)
	    | PMSIRR_RND;
	info->pmsicr = 0;
	info->pmscr = PMSCR_TS | PMSCR_PA | PMSCR_CX | PMSCR_E1SPE | PMSCR_E0SPE;

	if (ctx->config != NULL &&
	    ctx->config_size == sizeof(struct arm_spe_config) &&
	    ctx->config_version == 1) {
		cfg = (struct arm_spe_config *)ctx->config;
		if (cfg->interval)
			arm_spe_set_interval(info, cfg->interval);
		if (cfg->level == ARM_SPE_KERNEL_ONLY)
			info->pmscr &= ~(PMSCR_E0SPE); /* turn off user */
		if (cfg->level == ARM_SPE_USER_ONLY)
			info->pmscr &= ~(PMSCR_E1SPE); /* turn off kern */
		if (cfg->ctx_field)
			info->ctx_field = cfg->ctx_field;
	} else
		err = (EINVAL);
	mtx_unlock_spin(&info->lock);

	return (err);
}


static void
arm_spe_enable(void *arg __unused)
{
	struct arm_spe_info *info = &spe_info[PCPU_GET(cpuid)];
	uint64_t base, limit;

	dprintf("%s on cpu:%d\n", __func__, PCPU_GET(cpuid));

	mtx_lock_spin(&info->lock);

	if (info->ctx_field == ARM_SPE_CTX_CPU_ID)
		WRITE_SPECIALREG(CONTEXTIDR_EL1_REG, PCPU_GET(cpuid));

	WRITE_SPECIALREG(PMSFCR_EL1_REG, info->pmsfcr);
	WRITE_SPECIALREG(PMSEVFR_EL1_REG, info->pmsevfr);
	WRITE_SPECIALREG(PMSLATFR_EL1_REG, info->pmslatfr);

	/* Set the sampling interval */
	WRITE_SPECIALREG(PMSIRR_EL1_REG, info->pmsirr);
	isb();

	/* Write 0 here before enabling sampling */
	WRITE_SPECIALREG(PMSICR_EL1_REG, info->pmsicr);
	isb();

	base = info->kvaddr;
	limit = base + (info->buf_size/2);
	/* Enable the buffer */
	limit &= PMBLIMITR_LIMIT_MASK; /* Zero lower 12 bits */
	limit |= PMBLIMITR_E;
	/* Set the base and limit */
	WRITE_SPECIALREG(PMBPTR_EL1_REG, base);
	WRITE_SPECIALREG(PMBLIMITR_EL1_REG, limit);
	isb();

	/* Enable sampling */
	WRITE_SPECIALREG(PMSCR_EL1_REG, info->pmscr);
	isb();

	info->enabled = true;

	mtx_unlock_spin(&info->lock);
}

static int
spe_backend_enable_smp(struct hwt_context *ctx)
{
	struct arm_spe_info *info;
	struct hwt_vm *vm;
	int cpu_id;

	HWT_CTX_LOCK(ctx);
	CPU_FOREACH_ISSET(cpu_id, &ctx->cpu_map) {
		vm = hwt_cpu_get(ctx, cpu_id)->vm;

		info = &spe_info[cpu_id];

		mtx_lock_spin(&info->lock);
		info->kvaddr = vm->kvaddr;
		info->buf_size = ctx->bufsize;
		mtx_unlock_spin(&info->lock);
	}
	HWT_CTX_UNLOCK(ctx);

	cpu_id = CPU_FFS(&ctx->cpu_map) - 1;
	info = &spe_info[cpu_id];
	if (info->ctx_field == ARM_SPE_CTX_PID)
		arm64_pid_in_contextidr = true;
	else
		arm64_pid_in_contextidr = false;

	smp_rendezvous_cpus(ctx->cpu_map, smp_no_rendezvous_barrier,
	    arm_spe_enable, smp_no_rendezvous_barrier, NULL);

	return (0);
}

void
arm_spe_disable(void *arg __unused)
{
	struct arm_spe_info *info = &spe_info[PCPU_GET(cpuid)];
	struct arm_spe_buf_info *buf = &info->buf_info[info->buf_idx];

	if (!info->enabled)
		return;

	dprintf("%s on cpu:%d\n", __func__, PCPU_GET(cpuid));

	/* Disable profiling */
	WRITE_SPECIALREG(PMSCR_EL1_REG, 0x0);
	isb();

	/* Drain any remaining tracing data */
	psb_csync();
	dsb(nsh);

	/* Disable the profiling buffer */
	WRITE_SPECIALREG(PMBLIMITR_EL1_REG, 0);
	isb();

	/* Clear interrupt status reg */
	WRITE_SPECIALREG(PMBSR_EL1_REG, 0x0);

	/* Clear PID/CPU_ID from context ID reg */
	WRITE_SPECIALREG(CONTEXTIDR_EL1_REG, 0);

	mtx_lock_spin(&info->lock);
	buf->pmbptr = READ_SPECIALREG(PMBPTR_EL1_REG);
	info->enabled = false;
	mtx_unlock_spin(&info->lock);
}

int
spe_backend_disable_smp(struct hwt_context *ctx)
{
	struct kevent kev;
	struct arm_spe_info *info;
	struct arm_spe_buf_info *buf;
	int cpu_id;
	int ret;

	/* Disable and send out remaining data in bufs */
	smp_rendezvous_cpus(ctx->cpu_map, smp_no_rendezvous_barrier,
	    arm_spe_disable, smp_no_rendezvous_barrier, NULL);

	CPU_FOREACH_ISSET(cpu_id, &ctx->cpu_map) {
		info = &spe_info[cpu_id];
		buf = &info->buf_info[info->buf_idx];
		arm_spe_send_buffer(buf, 0);
	}

	arm64_pid_in_contextidr = false;

	/*
	 * Tracing on all CPUs has been disabled, and we've sent write ptr
	 * offsets for all bufs - let userspace know it can shutdown
	 */
	EV_SET(&kev, ARM_SPE_KQ_SHUTDOWN, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
	ret = kqfd_register(ctx->kqueue_fd, &kev, ctx->hwt_td, M_WAITOK);
	if (ret)
		dprintf("%s kqfd_register ret:%d\n", __func__, ret);

	return (0);
}

static void
spe_backend_stop(struct hwt_context *ctx)
{
	spe_backend_disable_smp(ctx);
}

static void
arm_spe_reenable(void *arg __unused)
{
	struct arm_spe_info *info = &spe_info[PCPU_GET(cpuid)];;

	WRITE_SPECIALREG(PMSCR_EL1_REG, info->pmscr);
	isb();
}

static int
spe_backend_svc_buf(struct hwt_context *ctx, void *data, size_t data_size,
    int data_version)
{
	struct arm_spe_info *info;
	struct arm_spe_buf_info *buf;
	struct arm_spe_svc_buf *s;
	int err = 0;
	cpuset_t cpu_set;

	if (data_size != sizeof(struct arm_spe_svc_buf))
		return (E2BIG);

	if (data_version != 1)
		return (EINVAL);

	s = (struct arm_spe_svc_buf *)data;
	if (s->buf_idx > 1)
		return (ENODEV);
	if (s->ident >= mp_ncpus)
		return (EINVAL);

	info = &spe_info[s->ident];
	mtx_lock_spin(&info->lock);

	buf = &info->buf_info[s->buf_idx];

	if (!info->enabled) {
		err = ENXIO;
		goto end;
	}

	/* Clear the flag the signals buffer needs servicing */
	buf->buf_svc = false;

	/* Re-enable profiling if we've been waiting for this notification */
	if (buf->buf_wait) {
		CPU_SETOF(s->ident, &cpu_set);

		mtx_unlock_spin(&info->lock);
		smp_rendezvous_cpus(cpu_set, smp_no_rendezvous_barrier,
		    arm_spe_reenable, smp_no_rendezvous_barrier, NULL);
		mtx_lock_spin(&info->lock);

		buf->buf_wait = false;
	}

end:
	mtx_unlock_spin(&info->lock);
	return (err);
}

static int
spe_backend_read(struct hwt_vm *vm, int *ident, vm_offset_t *offset,
    uint64_t *data)
{
	struct arm_spe_queue *q;
	struct arm_spe_softc *sc = device_get_softc(spe_dev);
	int error = 0;

	mtx_lock_spin(&sc->sc_lock);

	/* Return the first pending buffer that needs servicing */
	q = STAILQ_FIRST(&sc->pending);
	if (q == NULL) {
		error = ENOENT;
		goto error;
	}
	*ident = q->ident;
	*offset = q->offset;
	*data = (q->buf_idx << KQ_BUF_POS_SHIFT) |
	    (q->partial_rec << KQ_PARTREC_SHIFT) |
	    (q->final_buf << KQ_FINAL_BUF_SHIFT);

	STAILQ_REMOVE_HEAD(&sc->pending, next);
	sc->npending--;

error:
	mtx_unlock_spin(&sc->sc_lock);
	if (error)
		return (error);

	free(q, M_ARM_SPE);
	return (0);
}

static struct hwt_backend_ops spe_ops = {
	.hwt_backend_init = spe_backend_init,
	.hwt_backend_deinit = spe_backend_deinit,

	.hwt_backend_configure = spe_backend_configure,
	.hwt_backend_svc_buf = spe_backend_svc_buf,
	.hwt_backend_stop = spe_backend_stop,

	.hwt_backend_enable_smp = spe_backend_enable_smp,
	.hwt_backend_disable_smp = spe_backend_disable_smp,

	.hwt_backend_read = spe_backend_read,
};

int
spe_register(device_t dev)
{
	spe_dev = dev;

	return (hwt_backend_register(&backend));
}
