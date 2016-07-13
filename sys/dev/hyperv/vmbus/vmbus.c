/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * VM Bus Driver Implementation
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/rtprio.h>
#include <sys/interrupt.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <machine/resource.h>
#include <sys/rman.h>

#include <machine/stdarg.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/segments.h>
#include <sys/pcpu.h>
#include <x86/apicvar.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/vmbus/hv_vmbus_priv.h>
#include <dev/hyperv/vmbus/hyperv_reg.h>
#include <dev/hyperv/vmbus/hyperv_var.h>
#include <dev/hyperv/vmbus/vmbus_reg.h>
#include <dev/hyperv/vmbus/vmbus_var.h>

#include <contrib/dev/acpica/include/acpi.h>
#include "acpi_if.h"
#include "vmbus_if.h"

#define VMBUS_GPADL_START		0xe1e10

struct vmbus_msghc {
	struct hypercall_postmsg_in	*mh_inprm;
	struct hypercall_postmsg_in	mh_inprm_save;
	struct hyperv_dma		mh_inprm_dma;

	struct vmbus_message		*mh_resp;
	struct vmbus_message		mh_resp0;
};

struct vmbus_msghc_ctx {
	struct vmbus_msghc		*mhc_free;
	struct mtx			mhc_free_lock;
	uint32_t			mhc_flags;

	struct vmbus_msghc		*mhc_active;
	struct mtx			mhc_active_lock;
};

#define VMBUS_MSGHC_CTXF_DESTROY	0x0001

static int			vmbus_init(struct vmbus_softc *);
static int			vmbus_connect(struct vmbus_softc *, uint32_t);
static int			vmbus_req_channels(struct vmbus_softc *sc);
static void			vmbus_disconnect(struct vmbus_softc *);
static int			vmbus_scan(struct vmbus_softc *);
static void			vmbus_scan_wait(struct vmbus_softc *);
static void			vmbus_scan_newdev(struct vmbus_softc *);

static int			vmbus_sysctl_version(SYSCTL_HANDLER_ARGS);

static struct vmbus_msghc_ctx	*vmbus_msghc_ctx_create(bus_dma_tag_t);
static void			vmbus_msghc_ctx_destroy(
				    struct vmbus_msghc_ctx *);
static void			vmbus_msghc_ctx_free(struct vmbus_msghc_ctx *);
static struct vmbus_msghc	*vmbus_msghc_alloc(bus_dma_tag_t);
static void			vmbus_msghc_free(struct vmbus_msghc *);
static struct vmbus_msghc	*vmbus_msghc_get1(struct vmbus_msghc_ctx *,
				    uint32_t);

struct vmbus_softc	*vmbus_sc;

extern inthand_t IDTVEC(vmbus_isr);

static const uint32_t		vmbus_version[] = {
	VMBUS_VERSION_WIN8_1,
	VMBUS_VERSION_WIN8,
	VMBUS_VERSION_WIN7,
	VMBUS_VERSION_WS2008
};

static struct vmbus_msghc *
vmbus_msghc_alloc(bus_dma_tag_t parent_dtag)
{
	struct vmbus_msghc *mh;

	mh = malloc(sizeof(*mh), M_DEVBUF, M_WAITOK | M_ZERO);

	mh->mh_inprm = hyperv_dmamem_alloc(parent_dtag,
	    HYPERCALL_POSTMSGIN_ALIGN, 0, HYPERCALL_POSTMSGIN_SIZE,
	    &mh->mh_inprm_dma, BUS_DMA_WAITOK);
	if (mh->mh_inprm == NULL) {
		free(mh, M_DEVBUF);
		return NULL;
	}
	return mh;
}

static void
vmbus_msghc_free(struct vmbus_msghc *mh)
{
	hyperv_dmamem_free(&mh->mh_inprm_dma, mh->mh_inprm);
	free(mh, M_DEVBUF);
}

static void
vmbus_msghc_ctx_free(struct vmbus_msghc_ctx *mhc)
{
	KASSERT(mhc->mhc_active == NULL, ("still have active msg hypercall"));
	KASSERT(mhc->mhc_free == NULL, ("still have hypercall msg"));

	mtx_destroy(&mhc->mhc_free_lock);
	mtx_destroy(&mhc->mhc_active_lock);
	free(mhc, M_DEVBUF);
}

static struct vmbus_msghc_ctx *
vmbus_msghc_ctx_create(bus_dma_tag_t parent_dtag)
{
	struct vmbus_msghc_ctx *mhc;

	mhc = malloc(sizeof(*mhc), M_DEVBUF, M_WAITOK | M_ZERO);
	mtx_init(&mhc->mhc_free_lock, "vmbus msghc free", NULL, MTX_DEF);
	mtx_init(&mhc->mhc_active_lock, "vmbus msghc act", NULL, MTX_DEF);

	mhc->mhc_free = vmbus_msghc_alloc(parent_dtag);
	if (mhc->mhc_free == NULL) {
		vmbus_msghc_ctx_free(mhc);
		return NULL;
	}
	return mhc;
}

static struct vmbus_msghc *
vmbus_msghc_get1(struct vmbus_msghc_ctx *mhc, uint32_t dtor_flag)
{
	struct vmbus_msghc *mh;

	mtx_lock(&mhc->mhc_free_lock);

	while ((mhc->mhc_flags & dtor_flag) == 0 && mhc->mhc_free == NULL) {
		mtx_sleep(&mhc->mhc_free, &mhc->mhc_free_lock, 0,
		    "gmsghc", 0);
	}
	if (mhc->mhc_flags & dtor_flag) {
		/* Being destroyed */
		mh = NULL;
	} else {
		mh = mhc->mhc_free;
		KASSERT(mh != NULL, ("no free hypercall msg"));
		KASSERT(mh->mh_resp == NULL,
		    ("hypercall msg has pending response"));
		mhc->mhc_free = NULL;
	}

	mtx_unlock(&mhc->mhc_free_lock);

	return mh;
}

void
vmbus_msghc_reset(struct vmbus_msghc *mh, size_t dsize)
{
	struct hypercall_postmsg_in *inprm;

	if (dsize > HYPERCALL_POSTMSGIN_DSIZE_MAX)
		panic("invalid data size %zu", dsize);

	inprm = mh->mh_inprm;
	memset(inprm, 0, HYPERCALL_POSTMSGIN_SIZE);
	inprm->hc_connid = VMBUS_CONNID_MESSAGE;
	inprm->hc_msgtype = HYPERV_MSGTYPE_CHANNEL;
	inprm->hc_dsize = dsize;
}

struct vmbus_msghc *
vmbus_msghc_get(struct vmbus_softc *sc, size_t dsize)
{
	struct vmbus_msghc *mh;

	if (dsize > HYPERCALL_POSTMSGIN_DSIZE_MAX)
		panic("invalid data size %zu", dsize);

	mh = vmbus_msghc_get1(sc->vmbus_msg_hc, VMBUS_MSGHC_CTXF_DESTROY);
	if (mh == NULL)
		return NULL;

	vmbus_msghc_reset(mh, dsize);
	return mh;
}

void
vmbus_msghc_put(struct vmbus_softc *sc, struct vmbus_msghc *mh)
{
	struct vmbus_msghc_ctx *mhc = sc->vmbus_msg_hc;

	KASSERT(mhc->mhc_active == NULL, ("msg hypercall is active"));
	mh->mh_resp = NULL;

	mtx_lock(&mhc->mhc_free_lock);
	KASSERT(mhc->mhc_free == NULL, ("has free hypercall msg"));
	mhc->mhc_free = mh;
	mtx_unlock(&mhc->mhc_free_lock);
	wakeup(&mhc->mhc_free);
}

void *
vmbus_msghc_dataptr(struct vmbus_msghc *mh)
{
	return mh->mh_inprm->hc_data;
}

static void
vmbus_msghc_ctx_destroy(struct vmbus_msghc_ctx *mhc)
{
	struct vmbus_msghc *mh;

	mtx_lock(&mhc->mhc_free_lock);
	mhc->mhc_flags |= VMBUS_MSGHC_CTXF_DESTROY;
	mtx_unlock(&mhc->mhc_free_lock);
	wakeup(&mhc->mhc_free);

	mh = vmbus_msghc_get1(mhc, 0);
	if (mh == NULL)
		panic("can't get msghc");

	vmbus_msghc_free(mh);
	vmbus_msghc_ctx_free(mhc);
}

int
vmbus_msghc_exec_noresult(struct vmbus_msghc *mh)
{
	sbintime_t time = SBT_1MS;
	int i;

	/*
	 * Save the input parameter so that we could restore the input
	 * parameter if the Hypercall failed.
	 *
	 * XXX
	 * Is this really necessary?!  i.e. Will the Hypercall ever
	 * overwrite the input parameter?
	 */
	memcpy(&mh->mh_inprm_save, mh->mh_inprm, HYPERCALL_POSTMSGIN_SIZE);

	/*
	 * In order to cope with transient failures, e.g. insufficient
	 * resources on host side, we retry the post message Hypercall
	 * several times.  20 retries seem sufficient.
	 */
#define HC_RETRY_MAX	20

	for (i = 0; i < HC_RETRY_MAX; ++i) {
		uint64_t status;

		status = hypercall_post_message(mh->mh_inprm_dma.hv_paddr);
		if (status == HYPERCALL_STATUS_SUCCESS)
			return 0;

		pause_sbt("hcpmsg", time, 0, C_HARDCLOCK);
		if (time < SBT_1S * 2)
			time *= 2;

		/* Restore input parameter and try again */
		memcpy(mh->mh_inprm, &mh->mh_inprm_save,
		    HYPERCALL_POSTMSGIN_SIZE);
	}

#undef HC_RETRY_MAX

	return EIO;
}

int
vmbus_msghc_exec(struct vmbus_softc *sc, struct vmbus_msghc *mh)
{
	struct vmbus_msghc_ctx *mhc = sc->vmbus_msg_hc;
	int error;

	KASSERT(mh->mh_resp == NULL, ("hypercall msg has pending response"));

	mtx_lock(&mhc->mhc_active_lock);
	KASSERT(mhc->mhc_active == NULL, ("pending active msg hypercall"));
	mhc->mhc_active = mh;
	mtx_unlock(&mhc->mhc_active_lock);

	error = vmbus_msghc_exec_noresult(mh);
	if (error) {
		mtx_lock(&mhc->mhc_active_lock);
		KASSERT(mhc->mhc_active == mh, ("msghc mismatch"));
		mhc->mhc_active = NULL;
		mtx_unlock(&mhc->mhc_active_lock);
	}
	return error;
}

const struct vmbus_message *
vmbus_msghc_wait_result(struct vmbus_softc *sc, struct vmbus_msghc *mh)
{
	struct vmbus_msghc_ctx *mhc = sc->vmbus_msg_hc;

	mtx_lock(&mhc->mhc_active_lock);

	KASSERT(mhc->mhc_active == mh, ("msghc mismatch"));
	while (mh->mh_resp == NULL) {
		mtx_sleep(&mhc->mhc_active, &mhc->mhc_active_lock, 0,
		    "wmsghc", 0);
	}
	mhc->mhc_active = NULL;

	mtx_unlock(&mhc->mhc_active_lock);

	return mh->mh_resp;
}

void
vmbus_msghc_wakeup(struct vmbus_softc *sc, const struct vmbus_message *msg)
{
	struct vmbus_msghc_ctx *mhc = sc->vmbus_msg_hc;
	struct vmbus_msghc *mh;

	mtx_lock(&mhc->mhc_active_lock);

	mh = mhc->mhc_active;
	KASSERT(mh != NULL, ("no pending msg hypercall"));
	memcpy(&mh->mh_resp0, msg, sizeof(mh->mh_resp0));
	mh->mh_resp = &mh->mh_resp0;

	mtx_unlock(&mhc->mhc_active_lock);
	wakeup(&mhc->mhc_active);
}

uint32_t
vmbus_gpadl_alloc(struct vmbus_softc *sc)
{
	return atomic_fetchadd_int(&sc->vmbus_gpadl, 1);
}

static int
vmbus_connect(struct vmbus_softc *sc, uint32_t version)
{
	struct vmbus_chanmsg_connect *req;
	const struct vmbus_message *msg;
	struct vmbus_msghc *mh;
	int error, done = 0;

	mh = vmbus_msghc_get(sc, sizeof(*req));
	if (mh == NULL)
		return ENXIO;

	req = vmbus_msghc_dataptr(mh);
	req->chm_hdr.chm_type = VMBUS_CHANMSG_TYPE_CONNECT;
	req->chm_ver = version;
	req->chm_evtflags = sc->vmbus_evtflags_dma.hv_paddr;
	req->chm_mnf1 = sc->vmbus_mnf1_dma.hv_paddr;
	req->chm_mnf2 = sc->vmbus_mnf2_dma.hv_paddr;

	error = vmbus_msghc_exec(sc, mh);
	if (error) {
		vmbus_msghc_put(sc, mh);
		return error;
	}

	msg = vmbus_msghc_wait_result(sc, mh);
	done = ((const struct vmbus_chanmsg_connect_resp *)
	    msg->msg_data)->chm_done;

	vmbus_msghc_put(sc, mh);

	return (done ? 0 : EOPNOTSUPP);
}

static int
vmbus_init(struct vmbus_softc *sc)
{
	int i;

	for (i = 0; i < nitems(vmbus_version); ++i) {
		int error;

		error = vmbus_connect(sc, vmbus_version[i]);
		if (!error) {
			sc->vmbus_version = vmbus_version[i];
			device_printf(sc->vmbus_dev, "version %u.%u\n",
			    VMBUS_VERSION_MAJOR(sc->vmbus_version),
			    VMBUS_VERSION_MINOR(sc->vmbus_version));
			return 0;
		}
	}
	return ENXIO;
}

static void
vmbus_disconnect(struct vmbus_softc *sc)
{
	struct vmbus_chanmsg_disconnect *req;
	struct vmbus_msghc *mh;
	int error;

	mh = vmbus_msghc_get(sc, sizeof(*req));
	if (mh == NULL) {
		device_printf(sc->vmbus_dev,
		    "can not get msg hypercall for disconnect\n");
		return;
	}

	req = vmbus_msghc_dataptr(mh);
	req->chm_hdr.chm_type = VMBUS_CHANMSG_TYPE_DISCONNECT;

	error = vmbus_msghc_exec_noresult(mh);
	vmbus_msghc_put(sc, mh);

	if (error) {
		device_printf(sc->vmbus_dev,
		    "disconnect msg hypercall failed\n");
	}
}

static int
vmbus_req_channels(struct vmbus_softc *sc)
{
	struct vmbus_chanmsg_chrequest *req;
	struct vmbus_msghc *mh;
	int error;

	mh = vmbus_msghc_get(sc, sizeof(*req));
	if (mh == NULL)
		return ENXIO;

	req = vmbus_msghc_dataptr(mh);
	req->chm_hdr.chm_type = VMBUS_CHANMSG_TYPE_CHREQUEST;

	error = vmbus_msghc_exec_noresult(mh);
	vmbus_msghc_put(sc, mh);

	return error;
}

void
vmbus_scan_newchan(struct vmbus_softc *sc)
{
	mtx_lock(&sc->vmbus_scan_lock);
	if ((sc->vmbus_scan_chcnt & VMBUS_SCAN_CHCNT_DONE) == 0)
		sc->vmbus_scan_chcnt++;
	mtx_unlock(&sc->vmbus_scan_lock);
}

void
vmbus_scan_done(struct vmbus_softc *sc)
{
	mtx_lock(&sc->vmbus_scan_lock);
	sc->vmbus_scan_chcnt |= VMBUS_SCAN_CHCNT_DONE;
	mtx_unlock(&sc->vmbus_scan_lock);
	wakeup(&sc->vmbus_scan_chcnt);
}

static void
vmbus_scan_newdev(struct vmbus_softc *sc)
{
	mtx_lock(&sc->vmbus_scan_lock);
	sc->vmbus_scan_devcnt++;
	mtx_unlock(&sc->vmbus_scan_lock);
	wakeup(&sc->vmbus_scan_devcnt);
}

static void
vmbus_scan_wait(struct vmbus_softc *sc)
{
	uint32_t chancnt;

	mtx_lock(&sc->vmbus_scan_lock);
	while ((sc->vmbus_scan_chcnt & VMBUS_SCAN_CHCNT_DONE) == 0) {
		mtx_sleep(&sc->vmbus_scan_chcnt, &sc->vmbus_scan_lock, 0,
		    "waitch", 0);
	}
	chancnt = sc->vmbus_scan_chcnt & ~VMBUS_SCAN_CHCNT_DONE;

	while (sc->vmbus_scan_devcnt != chancnt) {
		mtx_sleep(&sc->vmbus_scan_devcnt, &sc->vmbus_scan_lock, 0,
		    "waitdev", 0);
	}
	mtx_unlock(&sc->vmbus_scan_lock);
}

static int
vmbus_scan(struct vmbus_softc *sc)
{
	int error;

	/*
	 * Start vmbus scanning.
	 */
	error = vmbus_req_channels(sc);
	if (error) {
		device_printf(sc->vmbus_dev, "channel request failed: %d\n",
		    error);
		return error;
	}

	/*
	 * Wait for all devices are added to vmbus.
	 */
	vmbus_scan_wait(sc);

	/*
	 * Identify, probe and attach.
	 */
	bus_generic_probe(sc->vmbus_dev);
	bus_generic_attach(sc->vmbus_dev);

	if (bootverbose) {
		device_printf(sc->vmbus_dev, "device scan, probe and attach "
		    "done\n");
	}
	return 0;
}

static void
vmbus_msg_task(void *xsc, int pending __unused)
{
	struct vmbus_softc *sc = xsc;
	volatile struct vmbus_message *msg;

	msg = VMBUS_PCPU_GET(sc, message, curcpu) + VMBUS_SINT_MESSAGE;
	for (;;) {
		if (msg->msg_type == HYPERV_MSGTYPE_NONE) {
			/* No message */
			break;
		} else if (msg->msg_type == HYPERV_MSGTYPE_CHANNEL) {
			/* Channel message */
			vmbus_chan_msgproc(sc,
			    __DEVOLATILE(const struct vmbus_message *, msg));
		}

		msg->msg_type = HYPERV_MSGTYPE_NONE;
		/*
		 * Make sure the write to msg_type (i.e. set to
		 * HYPERV_MSGTYPE_NONE) happens before we read the
		 * msg_flags and EOMing. Otherwise, the EOMing will
		 * not deliver any more messages since there is no
		 * empty slot
		 *
		 * NOTE:
		 * mb() is used here, since atomic_thread_fence_seq_cst()
		 * will become compiler fence on UP kernel.
		 */
		mb();
		if (msg->msg_flags & VMBUS_MSGFLAG_PENDING) {
			/*
			 * This will cause message queue rescan to possibly
			 * deliver another msg from the hypervisor
			 */
			wrmsr(MSR_HV_EOM, 0);
		}
	}
}

static __inline int
vmbus_handle_intr1(struct vmbus_softc *sc, struct trapframe *frame, int cpu)
{
	volatile struct vmbus_message *msg;
	struct vmbus_message *msg_base;

	msg_base = VMBUS_PCPU_GET(sc, message, cpu);

	/*
	 * Check event timer.
	 *
	 * TODO: move this to independent IDT vector.
	 */
	msg = msg_base + VMBUS_SINT_TIMER;
	if (msg->msg_type == HYPERV_MSGTYPE_TIMER_EXPIRED) {
		msg->msg_type = HYPERV_MSGTYPE_NONE;

		vmbus_et_intr(frame);

		/*
		 * Make sure the write to msg_type (i.e. set to
		 * HYPERV_MSGTYPE_NONE) happens before we read the
		 * msg_flags and EOMing. Otherwise, the EOMing will
		 * not deliver any more messages since there is no
		 * empty slot
		 *
		 * NOTE:
		 * mb() is used here, since atomic_thread_fence_seq_cst()
		 * will become compiler fence on UP kernel.
		 */
		mb();
		if (msg->msg_flags & VMBUS_MSGFLAG_PENDING) {
			/*
			 * This will cause message queue rescan to possibly
			 * deliver another msg from the hypervisor
			 */
			wrmsr(MSR_HV_EOM, 0);
		}
	}

	/*
	 * Check events.  Hot path for network and storage I/O data; high rate.
	 *
	 * NOTE:
	 * As recommended by the Windows guest fellows, we check events before
	 * checking messages.
	 */
	sc->vmbus_event_proc(sc, cpu);

	/*
	 * Check messages.  Mainly management stuffs; ultra low rate.
	 */
	msg = msg_base + VMBUS_SINT_MESSAGE;
	if (__predict_false(msg->msg_type != HYPERV_MSGTYPE_NONE)) {
		taskqueue_enqueue(VMBUS_PCPU_GET(sc, message_tq, cpu),
		    VMBUS_PCPU_PTR(sc, message_task, cpu));
	}

	return (FILTER_HANDLED);
}

void
vmbus_handle_intr(struct trapframe *trap_frame)
{
	struct vmbus_softc *sc = vmbus_get_softc();
	int cpu = curcpu;

	/*
	 * Disable preemption.
	 */
	critical_enter();

	/*
	 * Do a little interrupt counting.
	 */
	(*VMBUS_PCPU_GET(sc, intr_cnt, cpu))++;

	vmbus_handle_intr1(sc, trap_frame, cpu);

	/*
	 * Enable preemption.
	 */
	critical_exit();
}

static void
vmbus_synic_setup(void *xsc)
{
	struct vmbus_softc *sc = xsc;
	int cpu = curcpu;
	uint64_t val, orig;
	uint32_t sint;

	if (hyperv_features & CPUID_HV_MSR_VP_INDEX) {
		/*
		 * Save virtual processor id.
		 */
		VMBUS_PCPU_GET(sc, vcpuid, cpu) = rdmsr(MSR_HV_VP_INDEX);
	} else {
		/*
		 * XXX
		 * Virtual processoor id is only used by a pretty broken
		 * channel selection code from storvsc.  It's nothing
		 * critical even if CPUID_HV_MSR_VP_INDEX is not set; keep
		 * moving on.
		 */
		VMBUS_PCPU_GET(sc, vcpuid, cpu) = cpu;
	}

	/*
	 * Setup the SynIC message.
	 */
	orig = rdmsr(MSR_HV_SIMP);
	val = MSR_HV_SIMP_ENABLE | (orig & MSR_HV_SIMP_RSVD_MASK) |
	    ((VMBUS_PCPU_GET(sc, message_dma.hv_paddr, cpu) >> PAGE_SHIFT) <<
	     MSR_HV_SIMP_PGSHIFT);
	wrmsr(MSR_HV_SIMP, val);

	/*
	 * Setup the SynIC event flags.
	 */
	orig = rdmsr(MSR_HV_SIEFP);
	val = MSR_HV_SIEFP_ENABLE | (orig & MSR_HV_SIEFP_RSVD_MASK) |
	    ((VMBUS_PCPU_GET(sc, event_flags_dma.hv_paddr, cpu)
	      >> PAGE_SHIFT) << MSR_HV_SIEFP_PGSHIFT);
	wrmsr(MSR_HV_SIEFP, val);


	/*
	 * Configure and unmask SINT for message and event flags.
	 */
	sint = MSR_HV_SINT0 + VMBUS_SINT_MESSAGE;
	orig = rdmsr(sint);
	val = sc->vmbus_idtvec | MSR_HV_SINT_AUTOEOI |
	    (orig & MSR_HV_SINT_RSVD_MASK);
	wrmsr(sint, val);

	/*
	 * Configure and unmask SINT for timer.
	 */
	sint = MSR_HV_SINT0 + VMBUS_SINT_TIMER;
	orig = rdmsr(sint);
	val = sc->vmbus_idtvec | MSR_HV_SINT_AUTOEOI |
	    (orig & MSR_HV_SINT_RSVD_MASK);
	wrmsr(sint, val);

	/*
	 * All done; enable SynIC.
	 */
	orig = rdmsr(MSR_HV_SCONTROL);
	val = MSR_HV_SCTRL_ENABLE | (orig & MSR_HV_SCTRL_RSVD_MASK);
	wrmsr(MSR_HV_SCONTROL, val);
}

static void
vmbus_synic_teardown(void *arg)
{
	uint64_t orig;
	uint32_t sint;

	/*
	 * Disable SynIC.
	 */
	orig = rdmsr(MSR_HV_SCONTROL);
	wrmsr(MSR_HV_SCONTROL, (orig & MSR_HV_SCTRL_RSVD_MASK));

	/*
	 * Mask message and event flags SINT.
	 */
	sint = MSR_HV_SINT0 + VMBUS_SINT_MESSAGE;
	orig = rdmsr(sint);
	wrmsr(sint, orig | MSR_HV_SINT_MASKED);

	/*
	 * Mask timer SINT.
	 */
	sint = MSR_HV_SINT0 + VMBUS_SINT_TIMER;
	orig = rdmsr(sint);
	wrmsr(sint, orig | MSR_HV_SINT_MASKED);

	/*
	 * Teardown SynIC message.
	 */
	orig = rdmsr(MSR_HV_SIMP);
	wrmsr(MSR_HV_SIMP, (orig & MSR_HV_SIMP_RSVD_MASK));

	/*
	 * Teardown SynIC event flags.
	 */
	orig = rdmsr(MSR_HV_SIEFP);
	wrmsr(MSR_HV_SIEFP, (orig & MSR_HV_SIEFP_RSVD_MASK));
}

static int
vmbus_dma_alloc(struct vmbus_softc *sc)
{
	bus_dma_tag_t parent_dtag;
	uint8_t *evtflags;
	int cpu;

	parent_dtag = bus_get_dma_tag(sc->vmbus_dev);
	CPU_FOREACH(cpu) {
		void *ptr;

		/*
		 * Per-cpu messages and event flags.
		 */
		ptr = hyperv_dmamem_alloc(parent_dtag, PAGE_SIZE, 0,
		    PAGE_SIZE, VMBUS_PCPU_PTR(sc, message_dma, cpu),
		    BUS_DMA_WAITOK | BUS_DMA_ZERO);
		if (ptr == NULL)
			return ENOMEM;
		VMBUS_PCPU_GET(sc, message, cpu) = ptr;

		ptr = hyperv_dmamem_alloc(parent_dtag, PAGE_SIZE, 0,
		    PAGE_SIZE, VMBUS_PCPU_PTR(sc, event_flags_dma, cpu),
		    BUS_DMA_WAITOK | BUS_DMA_ZERO);
		if (ptr == NULL)
			return ENOMEM;
		VMBUS_PCPU_GET(sc, event_flags, cpu) = ptr;
	}

	evtflags = hyperv_dmamem_alloc(parent_dtag, PAGE_SIZE, 0,
	    PAGE_SIZE, &sc->vmbus_evtflags_dma, BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (evtflags == NULL)
		return ENOMEM;
	sc->vmbus_rx_evtflags = (u_long *)evtflags;
	sc->vmbus_tx_evtflags = (u_long *)(evtflags + (PAGE_SIZE / 2));
	sc->vmbus_evtflags = evtflags;

	sc->vmbus_mnf1 = hyperv_dmamem_alloc(parent_dtag, PAGE_SIZE, 0,
	    PAGE_SIZE, &sc->vmbus_mnf1_dma, BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (sc->vmbus_mnf1 == NULL)
		return ENOMEM;

	sc->vmbus_mnf2 = hyperv_dmamem_alloc(parent_dtag, PAGE_SIZE, 0,
	    PAGE_SIZE, &sc->vmbus_mnf2_dma, BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (sc->vmbus_mnf2 == NULL)
		return ENOMEM;

	return 0;
}

static void
vmbus_dma_free(struct vmbus_softc *sc)
{
	int cpu;

	if (sc->vmbus_evtflags != NULL) {
		hyperv_dmamem_free(&sc->vmbus_evtflags_dma, sc->vmbus_evtflags);
		sc->vmbus_evtflags = NULL;
		sc->vmbus_rx_evtflags = NULL;
		sc->vmbus_tx_evtflags = NULL;
	}
	if (sc->vmbus_mnf1 != NULL) {
		hyperv_dmamem_free(&sc->vmbus_mnf1_dma, sc->vmbus_mnf1);
		sc->vmbus_mnf1 = NULL;
	}
	if (sc->vmbus_mnf2 != NULL) {
		hyperv_dmamem_free(&sc->vmbus_mnf2_dma, sc->vmbus_mnf2);
		sc->vmbus_mnf2 = NULL;
	}

	CPU_FOREACH(cpu) {
		if (VMBUS_PCPU_GET(sc, message, cpu) != NULL) {
			hyperv_dmamem_free(
			    VMBUS_PCPU_PTR(sc, message_dma, cpu),
			    VMBUS_PCPU_GET(sc, message, cpu));
			VMBUS_PCPU_GET(sc, message, cpu) = NULL;
		}
		if (VMBUS_PCPU_GET(sc, event_flags, cpu) != NULL) {
			hyperv_dmamem_free(
			    VMBUS_PCPU_PTR(sc, event_flags_dma, cpu),
			    VMBUS_PCPU_GET(sc, event_flags, cpu));
			VMBUS_PCPU_GET(sc, event_flags, cpu) = NULL;
		}
	}
}

static int
vmbus_intr_setup(struct vmbus_softc *sc)
{
	int cpu;

	CPU_FOREACH(cpu) {
		char buf[MAXCOMLEN + 1];
		cpuset_t cpu_mask;

		/* Allocate an interrupt counter for Hyper-V interrupt */
		snprintf(buf, sizeof(buf), "cpu%d:hyperv", cpu);
		intrcnt_add(buf, VMBUS_PCPU_PTR(sc, intr_cnt, cpu));

		/*
		 * Setup taskqueue to handle events.  Task will be per-
		 * channel.
		 */
		VMBUS_PCPU_GET(sc, event_tq, cpu) = taskqueue_create_fast(
		    "hyperv event", M_WAITOK, taskqueue_thread_enqueue,
		    VMBUS_PCPU_PTR(sc, event_tq, cpu));
		CPU_SETOF(cpu, &cpu_mask);
		taskqueue_start_threads_cpuset(
		    VMBUS_PCPU_PTR(sc, event_tq, cpu), 1, PI_NET, &cpu_mask,
		    "hvevent%d", cpu);

		/*
		 * Setup tasks and taskqueues to handle messages.
		 */
		VMBUS_PCPU_GET(sc, message_tq, cpu) = taskqueue_create_fast(
		    "hyperv msg", M_WAITOK, taskqueue_thread_enqueue,
		    VMBUS_PCPU_PTR(sc, message_tq, cpu));
		CPU_SETOF(cpu, &cpu_mask);
		taskqueue_start_threads_cpuset(
		    VMBUS_PCPU_PTR(sc, message_tq, cpu), 1, PI_NET, &cpu_mask,
		    "hvmsg%d", cpu);
		TASK_INIT(VMBUS_PCPU_PTR(sc, message_task, cpu), 0,
		    vmbus_msg_task, sc);
	}

	/*
	 * All Hyper-V ISR required resources are setup, now let's find a
	 * free IDT vector for Hyper-V ISR and set it up.
	 */
	sc->vmbus_idtvec = lapic_ipi_alloc(IDTVEC(vmbus_isr));
	if (sc->vmbus_idtvec < 0) {
		device_printf(sc->vmbus_dev, "cannot find free IDT vector\n");
		return ENXIO;
	}
	if(bootverbose) {
		device_printf(sc->vmbus_dev, "vmbus IDT vector %d\n",
		    sc->vmbus_idtvec);
	}
	return 0;
}

static void
vmbus_intr_teardown(struct vmbus_softc *sc)
{
	int cpu;

	if (sc->vmbus_idtvec >= 0) {
		lapic_ipi_free(sc->vmbus_idtvec);
		sc->vmbus_idtvec = -1;
	}

	CPU_FOREACH(cpu) {
		if (VMBUS_PCPU_GET(sc, event_tq, cpu) != NULL) {
			taskqueue_free(VMBUS_PCPU_GET(sc, event_tq, cpu));
			VMBUS_PCPU_GET(sc, event_tq, cpu) = NULL;
		}
		if (VMBUS_PCPU_GET(sc, message_tq, cpu) != NULL) {
			taskqueue_drain(VMBUS_PCPU_GET(sc, message_tq, cpu),
			    VMBUS_PCPU_PTR(sc, message_task, cpu));
			taskqueue_free(VMBUS_PCPU_GET(sc, message_tq, cpu));
			VMBUS_PCPU_GET(sc, message_tq, cpu) = NULL;
		}
	}
}

static int
vmbus_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	struct hv_device *child_dev_ctx = device_get_ivars(child);

	switch (index) {
	case HV_VMBUS_IVAR_TYPE:
		*result = (uintptr_t)&child_dev_ctx->class_id;
		return (0);

	case HV_VMBUS_IVAR_INSTANCE:
		*result = (uintptr_t)&child_dev_ctx->device_id;
		return (0);

	case HV_VMBUS_IVAR_DEVCTX:
		*result = (uintptr_t)child_dev_ctx;
		return (0);

	case HV_VMBUS_IVAR_NODE:
		*result = (uintptr_t)child_dev_ctx->device;
		return (0);
	}
	return (ENOENT);
}

static int
vmbus_child_pnpinfo_str(device_t dev, device_t child, char *buf, size_t buflen)
{
	struct hv_device *dev_ctx = device_get_ivars(child);
	char guidbuf[HYPERV_GUID_STRLEN];

	if (dev_ctx == NULL)
		return (0);

	strlcat(buf, "classid=", buflen);
	hyperv_guid2str(&dev_ctx->class_id, guidbuf, sizeof(guidbuf));
	strlcat(buf, guidbuf, buflen);

	strlcat(buf, " deviceid=", buflen);
	hyperv_guid2str(&dev_ctx->device_id, guidbuf, sizeof(guidbuf));
	strlcat(buf, guidbuf, buflen);

	return (0);
}

struct hv_device *
hv_vmbus_child_device_create(struct hv_vmbus_channel *channel)
{
	hv_device *child_dev;

	/*
	 * Allocate the new child device
	 */
	child_dev = malloc(sizeof(hv_device), M_DEVBUF, M_WAITOK | M_ZERO);

	child_dev->channel = channel;
	child_dev->class_id = channel->ch_guid_type;
	child_dev->device_id = channel->ch_guid_inst;

	return (child_dev);
}

void
hv_vmbus_child_device_register(struct vmbus_softc *sc,
    struct hv_device *child_dev)
{
	device_t child, parent;

	parent = sc->vmbus_dev;
	if (bootverbose) {
		char name[HYPERV_GUID_STRLEN];

		hyperv_guid2str(&child_dev->class_id, name, sizeof(name));
		device_printf(parent, "add device, classid: %s\n", name);
	}

	child = device_add_child(parent, NULL, -1);
	child_dev->device = child;
	device_set_ivars(child, child_dev);

	/* New device was added to vmbus */
	vmbus_scan_newdev(sc);
}

int
hv_vmbus_child_device_unregister(struct hv_device *child_dev)
{
	int ret = 0;
	/*
	 * XXXKYS: Ensure that this is the opposite of
	 * device_add_child()
	 */
	mtx_lock(&Giant);
	ret = device_delete_child(vmbus_get_device(), child_dev->device);
	mtx_unlock(&Giant);
	return(ret);
}

static int
vmbus_sysctl_version(SYSCTL_HANDLER_ARGS)
{
	struct vmbus_softc *sc = arg1;
	char verstr[16];

	snprintf(verstr, sizeof(verstr), "%u.%u",
	    VMBUS_VERSION_MAJOR(sc->vmbus_version),
	    VMBUS_VERSION_MINOR(sc->vmbus_version));
	return sysctl_handle_string(oidp, verstr, sizeof(verstr), req);
}

static uint32_t
vmbus_get_version_method(device_t bus, device_t dev)
{
	struct vmbus_softc *sc = device_get_softc(bus);

	return sc->vmbus_version;
}

static int
vmbus_probe_guid_method(device_t bus, device_t dev, const struct hv_guid *guid)
{
	struct hv_device *hv_dev = device_get_ivars(dev);

	if (memcmp(&hv_dev->class_id, guid, sizeof(struct hv_guid)) == 0)
		return 0;
	return ENXIO;
}

static int
vmbus_probe(device_t dev)
{
	char *id[] = { "VMBUS", NULL };

	if (ACPI_ID_PROBE(device_get_parent(dev), dev, id) == NULL ||
	    device_get_unit(dev) != 0 || vm_guest != VM_GUEST_HV ||
	    (hyperv_features & CPUID_HV_MSR_SYNIC) == 0)
		return (ENXIO);

	device_set_desc(dev, "Hyper-V Vmbus");

	return (BUS_PROBE_DEFAULT);
}

/**
 * @brief Main vmbus driver initialization routine.
 *
 * Here, we
 * - initialize the vmbus driver context
 * - setup various driver entry points
 * - invoke the vmbus hv main init routine
 * - get the irq resource
 * - invoke the vmbus to add the vmbus root device
 * - setup the vmbus root device
 * - retrieve the channel offers
 */
static int
vmbus_doattach(struct vmbus_softc *sc)
{
	struct sysctl_oid_list *child;
	struct sysctl_ctx_list *ctx;
	int ret;

	if (sc->vmbus_flags & VMBUS_FLAG_ATTACHED)
		return (0);
	sc->vmbus_flags |= VMBUS_FLAG_ATTACHED;

	mtx_init(&sc->vmbus_scan_lock, "vmbus scan", NULL, MTX_DEF);
	sc->vmbus_gpadl = VMBUS_GPADL_START;
	mtx_init(&sc->vmbus_chlist_lock, "vmbus chlist", NULL, MTX_DEF);
	TAILQ_INIT(&sc->vmbus_chlist);
	sc->vmbus_chmap = malloc(
	    sizeof(struct hv_vmbus_channel *) * VMBUS_CHAN_MAX, M_DEVBUF,
	    M_WAITOK | M_ZERO);

	/*
	 * Create context for "post message" Hypercalls
	 */
	sc->vmbus_msg_hc = vmbus_msghc_ctx_create(
	    bus_get_dma_tag(sc->vmbus_dev));
	if (sc->vmbus_msg_hc == NULL) {
		ret = ENXIO;
		goto cleanup;
	}

	/*
	 * Allocate DMA stuffs.
	 */
	ret = vmbus_dma_alloc(sc);
	if (ret != 0)
		goto cleanup;

	/*
	 * Setup interrupt.
	 */
	ret = vmbus_intr_setup(sc);
	if (ret != 0)
		goto cleanup;

	/*
	 * Setup SynIC.
	 */
	if (bootverbose)
		device_printf(sc->vmbus_dev, "smp_started = %d\n", smp_started);
	smp_rendezvous(NULL, vmbus_synic_setup, NULL, sc);
	sc->vmbus_flags |= VMBUS_FLAG_SYNIC;

	/*
	 * Initialize vmbus, e.g. connect to Hypervisor.
	 */
	ret = vmbus_init(sc);
	if (ret != 0)
		goto cleanup;

	if (sc->vmbus_version == VMBUS_VERSION_WS2008 ||
	    sc->vmbus_version == VMBUS_VERSION_WIN7)
		sc->vmbus_event_proc = vmbus_event_proc_compat;
	else
		sc->vmbus_event_proc = vmbus_event_proc;

	ret = vmbus_scan(sc);
	if (ret != 0)
		goto cleanup;

	ctx = device_get_sysctl_ctx(sc->vmbus_dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->vmbus_dev));
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "version",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    vmbus_sysctl_version, "A", "vmbus version");

	return (ret);

cleanup:
	vmbus_intr_teardown(sc);
	vmbus_dma_free(sc);
	if (sc->vmbus_msg_hc != NULL) {
		vmbus_msghc_ctx_destroy(sc->vmbus_msg_hc);
		sc->vmbus_msg_hc = NULL;
	}
	free(sc->vmbus_chmap, M_DEVBUF);
	mtx_destroy(&sc->vmbus_scan_lock);
	mtx_destroy(&sc->vmbus_chlist_lock);

	return (ret);
}

static void
vmbus_event_proc_dummy(struct vmbus_softc *sc __unused, int cpu __unused)
{
}

static int
vmbus_attach(device_t dev)
{
	vmbus_sc = device_get_softc(dev);
	vmbus_sc->vmbus_dev = dev;
	vmbus_sc->vmbus_idtvec = -1;

	/*
	 * Event processing logic will be configured:
	 * - After the vmbus protocol version negotiation.
	 * - Before we request channel offers.
	 */
	vmbus_sc->vmbus_event_proc = vmbus_event_proc_dummy;

#ifndef EARLY_AP_STARTUP
	/* 
	 * If the system has already booted and thread
	 * scheduling is possible indicated by the global
	 * cold set to zero, we just call the driver
	 * initialization directly.
	 */
	if (!cold)
#endif
		vmbus_doattach(vmbus_sc);

	return (0);
}

static void
vmbus_sysinit(void *arg __unused)
{
	struct vmbus_softc *sc = vmbus_get_softc();

	if (vm_guest != VM_GUEST_HV || sc == NULL)
		return;

#ifndef EARLY_AP_STARTUP
	/* 
	 * If the system has already booted and thread
	 * scheduling is possible, as indicated by the
	 * global cold set to zero, we just call the driver
	 * initialization directly.
	 */
	if (!cold) 
#endif
		vmbus_doattach(sc);
}

static int
vmbus_detach(device_t dev)
{
	struct vmbus_softc *sc = device_get_softc(dev);

	hv_vmbus_release_unattached_channels(sc);

	vmbus_disconnect(sc);

	if (sc->vmbus_flags & VMBUS_FLAG_SYNIC) {
		sc->vmbus_flags &= ~VMBUS_FLAG_SYNIC;
		smp_rendezvous(NULL, vmbus_synic_teardown, NULL, NULL);
	}

	vmbus_intr_teardown(sc);
	vmbus_dma_free(sc);

	if (sc->vmbus_msg_hc != NULL) {
		vmbus_msghc_ctx_destroy(sc->vmbus_msg_hc);
		sc->vmbus_msg_hc = NULL;
	}

	free(sc->vmbus_chmap, M_DEVBUF);
	mtx_destroy(&sc->vmbus_scan_lock);
	mtx_destroy(&sc->vmbus_chlist_lock);

	return (0);
}

static device_method_t vmbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			vmbus_probe),
	DEVMETHOD(device_attach,		vmbus_attach),
	DEVMETHOD(device_detach,		vmbus_detach),
	DEVMETHOD(device_shutdown,		bus_generic_shutdown),
	DEVMETHOD(device_suspend,		bus_generic_suspend),
	DEVMETHOD(device_resume,		bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_add_child,		bus_generic_add_child),
	DEVMETHOD(bus_print_child,		bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,		vmbus_read_ivar),
	DEVMETHOD(bus_child_pnpinfo_str,	vmbus_child_pnpinfo_str),

	/* Vmbus interface */
	DEVMETHOD(vmbus_get_version,		vmbus_get_version_method),
	DEVMETHOD(vmbus_probe_guid,		vmbus_probe_guid_method),

	DEVMETHOD_END
};

static driver_t vmbus_driver = {
	"vmbus",
	vmbus_methods,
	sizeof(struct vmbus_softc)
};

static devclass_t vmbus_devclass;

DRIVER_MODULE(vmbus, acpi, vmbus_driver, vmbus_devclass, NULL, NULL);
MODULE_DEPEND(vmbus, acpi, 1, 1, 1);
MODULE_VERSION(vmbus, 1);

#ifndef EARLY_AP_STARTUP
/*
 * NOTE:
 * We have to start as the last step of SI_SUB_SMP, i.e. after SMP is
 * initialized.
 */
SYSINIT(vmbus_initialize, SI_SUB_SMP, SI_ORDER_ANY, vmbus_sysinit, NULL);
#endif
