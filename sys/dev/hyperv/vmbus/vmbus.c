/*-
 * Copyright (c) 2009-2012,2016-2017 Microsoft Corp.
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
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#if defined(__aarch64__)
#include <dev/psci/smccc.h>
#include <dev/hyperv/vmbus/aarch64/hyperv_machdep.h>
#include <dev/hyperv/vmbus/aarch64/hyperv_reg.h>
#else
#include <dev/hyperv/vmbus/x86/hyperv_machdep.h>
#include <dev/hyperv/vmbus/x86/hyperv_reg.h>
#include <machine/intr_machdep.h>
#include <x86/include/apicvar.h>
#endif
#include <machine/metadata.h>
#include <machine/md_var.h>
#include <machine/resource.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/vmbus_xact.h>
#include <dev/hyperv/vmbus/hyperv_var.h>
#include <dev/hyperv/vmbus/vmbus_reg.h>
#include <dev/hyperv/vmbus/vmbus_var.h>
#include <dev/hyperv/vmbus/vmbus_chanvar.h>
#include <dev/hyperv/vmbus/hyperv_common_reg.h>
#include "acpi_if.h"
#include "pcib_if.h"
#include "vmbus_if.h"

#define VMBUS_GPADL_START		0xe1e10

struct vmbus_msghc {
	struct vmbus_xact		*mh_xact;
	struct hypercall_postmsg_in	mh_inprm_save;
};

static void			vmbus_identify(driver_t *, device_t);
static int			vmbus_probe(device_t);
static int			vmbus_attach(device_t);
static int			vmbus_detach(device_t);
static int			vmbus_read_ivar(device_t, device_t, int,
				    uintptr_t *);
static int			vmbus_child_pnpinfo(device_t, device_t, struct sbuf *);
static struct resource		*vmbus_alloc_resource(device_t dev,
				    device_t child, int type, int *rid,
				    rman_res_t start, rman_res_t end,
				    rman_res_t count, u_int flags);
static int			vmbus_alloc_msi(device_t bus, device_t dev,
				    int count, int maxcount, int *irqs);
static int			vmbus_release_msi(device_t bus, device_t dev,
				    int count, int *irqs);
static int			vmbus_alloc_msix(device_t bus, device_t dev,
				    int *irq);
static int			vmbus_release_msix(device_t bus, device_t dev,
				    int irq);
static int			vmbus_map_msi(device_t bus, device_t dev,
				    int irq, uint64_t *addr, uint32_t *data);
static uint32_t			vmbus_get_version_method(device_t, device_t);
static int			vmbus_probe_guid_method(device_t, device_t,
				    const struct hyperv_guid *);
static uint32_t			vmbus_get_vcpu_id_method(device_t bus,
				    device_t dev, int cpu);
static struct taskqueue		*vmbus_get_eventtq_method(device_t, device_t,
				    int);
#if defined(EARLY_AP_STARTUP)
static void			vmbus_intrhook(void *);
#endif

static int			vmbus_init(struct vmbus_softc *);
static int			vmbus_connect(struct vmbus_softc *, uint32_t);
static int			vmbus_req_channels(struct vmbus_softc *sc);
static void			vmbus_disconnect(struct vmbus_softc *);
static int			vmbus_scan(struct vmbus_softc *);
static void			vmbus_scan_teardown(struct vmbus_softc *);
static void			vmbus_scan_done(struct vmbus_softc *,
				    const struct vmbus_message *);
static void			vmbus_chanmsg_handle(struct vmbus_softc *,
				    const struct vmbus_message *);
static void			vmbus_msg_task(void *, int);
static void			vmbus_synic_setup(void *);
static void			vmbus_synic_teardown(void *);
static int			vmbus_sysctl_version(SYSCTL_HANDLER_ARGS);
static int			vmbus_dma_alloc(struct vmbus_softc *);
static void			vmbus_dma_free(struct vmbus_softc *);
static int			vmbus_intr_setup(struct vmbus_softc *);
static void			vmbus_intr_teardown(struct vmbus_softc *);
static int			vmbus_doattach(struct vmbus_softc *);
static void			vmbus_event_proc_dummy(struct vmbus_softc *,
				    int);
static bus_dma_tag_t	vmbus_get_dma_tag(device_t parent, device_t child);
static struct vmbus_softc	*vmbus_sc;

SYSCTL_NODE(_hw, OID_AUTO, vmbus, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
    "Hyper-V vmbus");

static int			vmbus_pin_evttask = 1;
SYSCTL_INT(_hw_vmbus, OID_AUTO, pin_evttask, CTLFLAG_RDTUN,
    &vmbus_pin_evttask, 0, "Pin event tasks to their respective CPU");
uint32_t			vmbus_current_version;

static const uint32_t		vmbus_version[] = {
	VMBUS_VERSION_WIN10,
	VMBUS_VERSION_WIN8_1,
	VMBUS_VERSION_WIN8,
	VMBUS_VERSION_WIN7,
	VMBUS_VERSION_WS2008
};

static const vmbus_chanmsg_proc_t
vmbus_chanmsg_handlers[VMBUS_CHANMSG_TYPE_MAX] = {
	VMBUS_CHANMSG_PROC(CHOFFER_DONE, vmbus_scan_done),
	VMBUS_CHANMSG_PROC_WAKEUP(CONNECT_RESP)
};

static device_method_t vmbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,		vmbus_identify),
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
	DEVMETHOD(bus_child_pnpinfo,		vmbus_child_pnpinfo),
	DEVMETHOD(bus_alloc_resource,		vmbus_alloc_resource),
	DEVMETHOD(bus_release_resource,		bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),
	DEVMETHOD(bus_get_cpus,			bus_generic_get_cpus),
	DEVMETHOD(bus_get_dma_tag,		vmbus_get_dma_tag),

	/* pcib interface */
	DEVMETHOD(pcib_alloc_msi,		vmbus_alloc_msi),
	DEVMETHOD(pcib_release_msi,		vmbus_release_msi),
	DEVMETHOD(pcib_alloc_msix,		vmbus_alloc_msix),
	DEVMETHOD(pcib_release_msix,		vmbus_release_msix),
	DEVMETHOD(pcib_map_msi,			vmbus_map_msi),

	/* Vmbus interface */
	DEVMETHOD(vmbus_get_version,		vmbus_get_version_method),
	DEVMETHOD(vmbus_probe_guid,		vmbus_probe_guid_method),
	DEVMETHOD(vmbus_get_vcpu_id,		vmbus_get_vcpu_id_method),
	DEVMETHOD(vmbus_get_event_taskq,	vmbus_get_eventtq_method),

	DEVMETHOD_END
};

static driver_t vmbus_driver = {
	"vmbus",
	vmbus_methods,
	sizeof(struct vmbus_softc)
};

DRIVER_MODULE(vmbus, pcib, vmbus_driver, NULL, NULL);
DRIVER_MODULE(vmbus, acpi_syscontainer, vmbus_driver, NULL, NULL);

MODULE_DEPEND(vmbus, acpi, 1, 1, 1);
MODULE_DEPEND(vmbus, pci, 1, 1, 1);
MODULE_VERSION(vmbus, 1);

static __inline struct vmbus_softc *
vmbus_get_softc(void)
{
	return vmbus_sc;
}

static bus_dma_tag_t
vmbus_get_dma_tag(device_t dev, device_t child)
{
	struct vmbus_softc *sc = vmbus_get_softc();
	return (sc->dmat);
}

void
vmbus_msghc_reset(struct vmbus_msghc *mh, size_t dsize)
{
	struct hypercall_postmsg_in *inprm;

	if (dsize > HYPERCALL_POSTMSGIN_DSIZE_MAX)
		panic("invalid data size %zu", dsize);

	inprm = vmbus_xact_req_data(mh->mh_xact);
	memset(inprm, 0, HYPERCALL_POSTMSGIN_SIZE);
	inprm->hc_connid = VMBUS_CONNID_MESSAGE;
	inprm->hc_msgtype = HYPERV_MSGTYPE_CHANNEL;
	inprm->hc_dsize = dsize;
}

struct vmbus_msghc *
vmbus_msghc_get(struct vmbus_softc *sc, size_t dsize)
{
	struct vmbus_msghc *mh;
	struct vmbus_xact *xact;

	if (dsize > HYPERCALL_POSTMSGIN_DSIZE_MAX)
		panic("invalid data size %zu", dsize);

	xact = vmbus_xact_get(sc->vmbus_xc,
	    dsize + __offsetof(struct hypercall_postmsg_in, hc_data[0]));
	if (xact == NULL)
		return (NULL);

	mh = vmbus_xact_priv(xact, sizeof(*mh));
	mh->mh_xact = xact;

	vmbus_msghc_reset(mh, dsize);
	return (mh);
}

void
vmbus_msghc_put(struct vmbus_softc *sc __unused, struct vmbus_msghc *mh)
{

	vmbus_xact_put(mh->mh_xact);
}

void *
vmbus_msghc_dataptr(struct vmbus_msghc *mh)
{
	struct hypercall_postmsg_in *inprm;

	inprm = vmbus_xact_req_data(mh->mh_xact);
	return (inprm->hc_data);
}

int
vmbus_msghc_exec_noresult(struct vmbus_msghc *mh)
{
	sbintime_t time = SBT_1MS;
	struct hypercall_postmsg_in *inprm;
	bus_addr_t inprm_paddr;
	int i;

	inprm = vmbus_xact_req_data(mh->mh_xact);
	inprm_paddr = vmbus_xact_req_paddr(mh->mh_xact);

	/*
	 * Save the input parameter so that we could restore the input
	 * parameter if the Hypercall failed.
	 *
	 * XXX
	 * Is this really necessary?!  i.e. Will the Hypercall ever
	 * overwrite the input parameter?
	 */
	memcpy(&mh->mh_inprm_save, inprm, HYPERCALL_POSTMSGIN_SIZE);

	/*
	 * In order to cope with transient failures, e.g. insufficient
	 * resources on host side, we retry the post message Hypercall
	 * several times.  20 retries seem sufficient.
	 */
#define HC_RETRY_MAX	20

	for (i = 0; i < HC_RETRY_MAX; ++i) {
		uint64_t status;

		status = hypercall_post_message(inprm_paddr);
		if (status == HYPERCALL_STATUS_SUCCESS)
			return 0;

		pause_sbt("hcpmsg", time, 0, C_HARDCLOCK);
		if (time < SBT_1S * 2)
			time *= 2;

		/* Restore input parameter and try again */
		memcpy(inprm, &mh->mh_inprm_save, HYPERCALL_POSTMSGIN_SIZE);
	}

#undef HC_RETRY_MAX

	return EIO;
}

int
vmbus_msghc_exec(struct vmbus_softc *sc __unused, struct vmbus_msghc *mh)
{
	int error;

	vmbus_xact_activate(mh->mh_xact);
	error = vmbus_msghc_exec_noresult(mh);
	if (error)
		vmbus_xact_deactivate(mh->mh_xact);
	return error;
}

void
vmbus_msghc_exec_cancel(struct vmbus_softc *sc __unused, struct vmbus_msghc *mh)
{

	vmbus_xact_deactivate(mh->mh_xact);
}

const struct vmbus_message *
vmbus_msghc_wait_result(struct vmbus_softc *sc __unused, struct vmbus_msghc *mh)
{
	size_t resp_len;

	return (vmbus_xact_wait(mh->mh_xact, &resp_len));
}

const struct vmbus_message *
vmbus_msghc_poll_result(struct vmbus_softc *sc __unused, struct vmbus_msghc *mh)
{
	size_t resp_len;

	return (vmbus_xact_poll(mh->mh_xact, &resp_len));
}

void
vmbus_msghc_wakeup(struct vmbus_softc *sc, const struct vmbus_message *msg)
{

	vmbus_xact_ctx_wakeup(sc->vmbus_xc, msg, sizeof(*msg));
}

uint32_t
vmbus_gpadl_alloc(struct vmbus_softc *sc)
{
	uint32_t gpadl;

again:
	gpadl = atomic_fetchadd_int(&sc->vmbus_gpadl, 1);
	if (gpadl == 0)
		goto again;
	return (gpadl);
}

/* Used for Hyper-V socket when guest client connects to host */
int
vmbus_req_tl_connect(struct hyperv_guid *guest_srv_id,
    struct hyperv_guid *host_srv_id)
{
	struct vmbus_softc *sc = vmbus_get_softc();
	struct vmbus_chanmsg_tl_connect *req;
	struct vmbus_msghc *mh;
	int error;

	if (!sc)
		return ENXIO;

	mh = vmbus_msghc_get(sc, sizeof(*req));
	if (mh == NULL) {
		device_printf(sc->vmbus_dev,
		    "can not get msg hypercall for tl connect\n");
		return ENXIO;
	}

	req = vmbus_msghc_dataptr(mh);
	req->chm_hdr.chm_type = VMBUS_CHANMSG_TYPE_TL_CONN;
	req->guest_endpoint_id = *guest_srv_id;
	req->host_service_id = *host_srv_id;

	error = vmbus_msghc_exec_noresult(mh);
	vmbus_msghc_put(sc, mh);

	if (error) {
		device_printf(sc->vmbus_dev,
		    "tl connect msg hypercall failed\n");
	}

	return error;
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
	req->chm_evtflags = pmap_kextract((vm_offset_t)sc->vmbus_evtflags);
	req->chm_mnf1 = pmap_kextract((vm_offset_t)sc->vmbus_mnf1);
	req->chm_mnf2 = pmap_kextract((vm_offset_t)sc->vmbus_mnf2);

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
			vmbus_current_version = vmbus_version[i];
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

static void
vmbus_scan_done_task(void *xsc, int pending __unused)
{
	struct vmbus_softc *sc = xsc;

	bus_topo_lock();
	sc->vmbus_scandone = true;
	bus_topo_unlock();
	wakeup(&sc->vmbus_scandone);
}

static void
vmbus_scan_done(struct vmbus_softc *sc,
    const struct vmbus_message *msg __unused)
{

	taskqueue_enqueue(sc->vmbus_devtq, &sc->vmbus_scandone_task);
}

static int
vmbus_scan(struct vmbus_softc *sc)
{
	int error;

	/*
	 * Identify, probe and attach for non-channel devices.
	 */
	bus_generic_probe(sc->vmbus_dev);
	bus_generic_attach(sc->vmbus_dev);

	/*
	 * This taskqueue serializes vmbus devices' attach and detach
	 * for channel offer and rescind messages.
	 */
	sc->vmbus_devtq = taskqueue_create("vmbus dev", M_WAITOK,
	    taskqueue_thread_enqueue, &sc->vmbus_devtq);
	taskqueue_start_threads(&sc->vmbus_devtq, 1, PI_NET, "vmbusdev");
	TASK_INIT(&sc->vmbus_scandone_task, 0, vmbus_scan_done_task, sc);

	/*
	 * This taskqueue handles sub-channel detach, so that vmbus
	 * device's detach running in vmbus_devtq can drain its sub-
	 * channels.
	 */
	sc->vmbus_subchtq = taskqueue_create("vmbus subch", M_WAITOK,
	    taskqueue_thread_enqueue, &sc->vmbus_subchtq);
	taskqueue_start_threads(&sc->vmbus_subchtq, 1, PI_NET, "vmbussch");

	/*
	 * Start vmbus scanning.
	 */
	error = vmbus_req_channels(sc);
	if (error) {
		device_printf(sc->vmbus_dev, "channel request failed: %d\n",
		    error);
		return (error);
	}

	/*
	 * Wait for all vmbus devices from the initial channel offers to be
	 * attached.
	 */
	bus_topo_assert();
	while (!sc->vmbus_scandone)
		mtx_sleep(&sc->vmbus_scandone, bus_topo_mtx(), 0, "vmbusdev", 0);

	if (bootverbose) {
		device_printf(sc->vmbus_dev, "device scan, probe and attach "
		    "done\n");
	}
	return (0);
}

static void
vmbus_scan_teardown(struct vmbus_softc *sc)
{

	bus_topo_assert();
	if (sc->vmbus_devtq != NULL) {
		bus_topo_unlock();
		taskqueue_free(sc->vmbus_devtq);
		bus_topo_lock();
		sc->vmbus_devtq = NULL;
	}
	if (sc->vmbus_subchtq != NULL) {
		bus_topo_unlock();
		taskqueue_free(sc->vmbus_subchtq);
		bus_topo_lock();
		sc->vmbus_subchtq = NULL;
	}
}

static void
vmbus_chanmsg_handle(struct vmbus_softc *sc, const struct vmbus_message *msg)
{
	vmbus_chanmsg_proc_t msg_proc;
	uint32_t msg_type;

	msg_type = ((const struct vmbus_chanmsg_hdr *)msg->msg_data)->chm_type;
	if (msg_type >= VMBUS_CHANMSG_TYPE_MAX) {
		device_printf(sc->vmbus_dev, "unknown message type 0x%x\n",
		    msg_type);
		return;
	}

	msg_proc = vmbus_chanmsg_handlers[msg_type];
	if (msg_proc != NULL)
		msg_proc(sc, msg);

	/* Channel specific processing */
	vmbus_chan_msgproc(sc, msg);
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
			vmbus_chanmsg_handle(sc,
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
			WRMSR(MSR_HV_EOM, 0);
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
	vmbus_handle_timer_intr1(msg_base, frame);
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
	 * Do a little interrupt counting. This used x86 specific
	 * intrcnt_add function
	 */
#if !defined(__aarch64__)
	(*VMBUS_PCPU_GET(sc, intr_cnt, cpu))++;
#endif /* not for aarch64 */
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
		/* Save virtual processor id. */
		VMBUS_PCPU_GET(sc, vcpuid, cpu) = RDMSR(MSR_HV_VP_INDEX);
	} else {
		/* Set virtual processor id to 0 for compatibility. */
		VMBUS_PCPU_GET(sc, vcpuid, cpu) = 0;
	}

	/*
	 * Setup the SynIC message.
	 */
	orig = RDMSR(MSR_HV_SIMP);
	val = pmap_kextract((vm_offset_t)VMBUS_PCPU_GET(sc, message, cpu)) &
	    MSR_HV_SIMP_PGMASK;
	val |= MSR_HV_SIMP_ENABLE | (orig & MSR_HV_SIMP_RSVD_MASK);
	WRMSR(MSR_HV_SIMP, val);
	/*
	 * Setup the SynIC event flags.
	 */
	orig = RDMSR(MSR_HV_SIEFP);
	val = pmap_kextract((vm_offset_t)VMBUS_PCPU_GET(sc, event_flags, cpu)) &
	    MSR_HV_SIMP_PGMASK;
	val |= MSR_HV_SIEFP_ENABLE | (orig & MSR_HV_SIEFP_RSVD_MASK);
	WRMSR(MSR_HV_SIEFP, val);

	/*
	 * Configure and unmask SINT for message and event flags.
	 */
	sint = MSR_HV_SINT0 + VMBUS_SINT_MESSAGE;
	orig = RDMSR(sint);
	val = sc->vmbus_idtvec | MSR_HV_SINT_AUTOEOI |
	    (orig & MSR_HV_SINT_RSVD_MASK);
	WRMSR(sint, val);

	/*
	 * Configure and unmask SINT for timer.
	 */
	vmbus_synic_setup1(sc);
	/*
	 * All done; enable SynIC.
	 */
	orig = RDMSR(MSR_HV_SCONTROL);
	val = MSR_HV_SCTRL_ENABLE | (orig & MSR_HV_SCTRL_RSVD_MASK);
	WRMSR(MSR_HV_SCONTROL, val);
}

static void
vmbus_synic_teardown(void *arg)
{
	uint64_t orig;
	uint32_t sint;

	/*
	 * Disable SynIC.
	 */
	orig = RDMSR(MSR_HV_SCONTROL);
	WRMSR(MSR_HV_SCONTROL, (orig & MSR_HV_SCTRL_RSVD_MASK));

	/*
	 * Mask message and event flags SINT.
	 */
	sint = MSR_HV_SINT0 + VMBUS_SINT_MESSAGE;
	orig = RDMSR(sint);
	WRMSR(sint, orig | MSR_HV_SINT_MASKED);

	/*
	 * Mask timer SINT.
	 */
	vmbus_synic_teardown1();
	/*
	 * Teardown SynIC message.
	 */
	orig = RDMSR(MSR_HV_SIMP);
	WRMSR(MSR_HV_SIMP, (orig & MSR_HV_SIMP_RSVD_MASK));

	/*
	 * Teardown SynIC event flags.
	 */
	orig = RDMSR(MSR_HV_SIEFP);
	WRMSR(MSR_HV_SIEFP, (orig & MSR_HV_SIEFP_RSVD_MASK));
}

static int
vmbus_dma_alloc(struct vmbus_softc *sc)
{
	uint8_t *evtflags;
	int cpu;

	CPU_FOREACH(cpu) {
		void *ptr;

		/*
		 * Per-cpu messages and event flags.
		 */
		ptr = contigmalloc(PAGE_SIZE, M_DEVBUF, M_WAITOK | M_ZERO,
		    0ul, ~0ul, PAGE_SIZE, 0);
		if (ptr == NULL)
			return ENOMEM;
		VMBUS_PCPU_GET(sc, message, cpu) = ptr;

		ptr = contigmalloc(PAGE_SIZE, M_DEVBUF, M_WAITOK | M_ZERO,
		    0ul, ~0ul, PAGE_SIZE, 0);
		if (ptr == NULL)
			return ENOMEM;
		VMBUS_PCPU_GET(sc, event_flags, cpu) = ptr;
	}

	evtflags = contigmalloc(PAGE_SIZE, M_DEVBUF, M_WAITOK | M_ZERO,
	    0ul, ~0ul, PAGE_SIZE, 0);
	if (evtflags == NULL)
		return ENOMEM;
	sc->vmbus_rx_evtflags = (u_long *)evtflags;
	sc->vmbus_tx_evtflags = (u_long *)(evtflags + (PAGE_SIZE / 2));
	sc->vmbus_evtflags = evtflags;

	sc->vmbus_mnf1 = contigmalloc(PAGE_SIZE, M_DEVBUF, M_WAITOK | M_ZERO,
	    0ul, ~0ul, PAGE_SIZE, 0);
	if (sc->vmbus_mnf1 == NULL)
		return ENOMEM;

	sc->vmbus_mnf2 = contigmalloc(sizeof(struct vmbus_mnf), M_DEVBUF,
	    M_WAITOK | M_ZERO, 0ul, ~0ul, PAGE_SIZE, 0);
	if (sc->vmbus_mnf2 == NULL)
		return ENOMEM;

	return 0;
}

static void
vmbus_dma_free(struct vmbus_softc *sc)
{
	int cpu;

	if (sc->vmbus_evtflags != NULL) {
		contigfree(sc->vmbus_evtflags, PAGE_SIZE, M_DEVBUF);
		sc->vmbus_evtflags = NULL;
		sc->vmbus_rx_evtflags = NULL;
		sc->vmbus_tx_evtflags = NULL;
	}
	if (sc->vmbus_mnf1 != NULL) {
		contigfree(sc->vmbus_mnf1, PAGE_SIZE, M_DEVBUF);
		sc->vmbus_mnf1 = NULL;
	}
	if (sc->vmbus_mnf2 != NULL) {
		contigfree(sc->vmbus_mnf2, sizeof(struct vmbus_mnf), M_DEVBUF);
		sc->vmbus_mnf2 = NULL;
	}

	CPU_FOREACH(cpu) {
		if (VMBUS_PCPU_GET(sc, message, cpu) != NULL) {
			contigfree(VMBUS_PCPU_GET(sc, message, cpu), PAGE_SIZE,
			    M_DEVBUF);
			VMBUS_PCPU_GET(sc, message, cpu) = NULL;
		}
		if (VMBUS_PCPU_GET(sc, event_flags, cpu) != NULL) {
			contigfree(VMBUS_PCPU_GET(sc, event_flags, cpu),
			    PAGE_SIZE, M_DEVBUF);
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
#if !defined(__aarch64__)
		intrcnt_add(buf, VMBUS_PCPU_PTR(sc, intr_cnt, cpu));
#endif /* not for aarch64 */
		/*
		 * Setup taskqueue to handle events.  Task will be per-
		 * channel.
		 */
		VMBUS_PCPU_GET(sc, event_tq, cpu) = taskqueue_create_fast(
		    "hyperv event", M_WAITOK, taskqueue_thread_enqueue,
		    VMBUS_PCPU_PTR(sc, event_tq, cpu));
		if (vmbus_pin_evttask) {
			CPU_SETOF(cpu, &cpu_mask);
			taskqueue_start_threads_cpuset(
			    VMBUS_PCPU_PTR(sc, event_tq, cpu), 1, PI_NET,
			    &cpu_mask, "hvevent%d", cpu);
		} else {
			taskqueue_start_threads(
			    VMBUS_PCPU_PTR(sc, event_tq, cpu), 1, PI_NET,
			    "hvevent%d", cpu);
		}

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
	return (vmbus_setup_intr1(sc));
}
static void
vmbus_intr_teardown(struct vmbus_softc *sc)
{
	vmbus_intr_teardown1(sc);
}

static int
vmbus_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	return (ENOENT);
}

static int
vmbus_child_pnpinfo(device_t dev, device_t child, struct sbuf *sb)
{
	const struct vmbus_channel *chan;
	char guidbuf[HYPERV_GUID_STRLEN];

	chan = vmbus_get_channel(child);
	if (chan == NULL) {
		/* Event timer device, which does not belong to a channel */
		return (0);
	}

	hyperv_guid2str(&chan->ch_guid_type, guidbuf, sizeof(guidbuf));
	sbuf_printf(sb, "classid=%s", guidbuf);

	hyperv_guid2str(&chan->ch_guid_inst, guidbuf, sizeof(guidbuf));
	sbuf_printf(sb, " deviceid=%s", guidbuf);

	return (0);
}

int
vmbus_add_child(struct vmbus_channel *chan)
{
	struct vmbus_softc *sc = chan->ch_vmbus;
	device_t parent = sc->vmbus_dev;

	bus_topo_lock();
	chan->ch_dev = device_add_child(parent, NULL, -1);
	if (chan->ch_dev == NULL) {
		bus_topo_unlock();
		device_printf(parent, "device_add_child for chan%u failed\n",
		    chan->ch_id);
		return (ENXIO);
	}
	device_set_ivars(chan->ch_dev, chan);
	device_probe_and_attach(chan->ch_dev);
	bus_topo_unlock();

	return (0);
}

int
vmbus_delete_child(struct vmbus_channel *chan)
{
	int error = 0;

	bus_topo_lock();
	if (chan->ch_dev != NULL) {
		error = device_delete_child(chan->ch_vmbus->vmbus_dev,
		    chan->ch_dev);
		chan->ch_dev = NULL;
	}
	bus_topo_unlock();
	return (error);
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

/*
 * We need the function to make sure the MMIO resource is allocated from the
 * ranges found in _CRS.
 *
 * For the release function, we can use bus_generic_release_resource().
 */
static struct resource *
vmbus_alloc_resource(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	device_t parent = device_get_parent(dev);
	struct resource *res;

#ifdef NEW_PCIB
	if (type == SYS_RES_MEMORY) {
		struct vmbus_softc *sc = device_get_softc(dev);

		res = pcib_host_res_alloc(&sc->vmbus_mmio_res, child, type,
		    rid, start, end, count, flags);
	} else
#endif
	{
		res = BUS_ALLOC_RESOURCE(parent, child, type, rid, start,
		    end, count, flags);
	}

	return (res);
}

static int
vmbus_alloc_msi(device_t bus, device_t dev, int count, int maxcount, int *irqs)
{

	return (PCIB_ALLOC_MSI(device_get_parent(bus), dev, count, maxcount,
	    irqs));
}

static int
vmbus_release_msi(device_t bus, device_t dev, int count, int *irqs)
{

	return (PCIB_RELEASE_MSI(device_get_parent(bus), dev, count, irqs));
}

static int
vmbus_alloc_msix(device_t bus, device_t dev, int *irq)
{

	return (PCIB_ALLOC_MSIX(device_get_parent(bus), dev, irq));
}

static int
vmbus_release_msix(device_t bus, device_t dev, int irq)
{

	return (PCIB_RELEASE_MSIX(device_get_parent(bus), dev, irq));
}

static int
vmbus_map_msi(device_t bus, device_t dev, int irq, uint64_t *addr,
	uint32_t *data)
{

	return (PCIB_MAP_MSI(device_get_parent(bus), dev, irq, addr, data));
}

static uint32_t
vmbus_get_version_method(device_t bus, device_t dev)
{
	struct vmbus_softc *sc = device_get_softc(bus);

	return sc->vmbus_version;
}

static int
vmbus_probe_guid_method(device_t bus, device_t dev,
    const struct hyperv_guid *guid)
{
	const struct vmbus_channel *chan = vmbus_get_channel(dev);

	if (memcmp(&chan->ch_guid_type, guid, sizeof(struct hyperv_guid)) == 0)
		return 0;
	return ENXIO;
}

static uint32_t
vmbus_get_vcpu_id_method(device_t bus, device_t dev, int cpu)
{
	const struct vmbus_softc *sc = device_get_softc(bus);

	return (VMBUS_PCPU_GET(sc, vcpuid, cpu));
}

static struct taskqueue *
vmbus_get_eventtq_method(device_t bus, device_t dev __unused, int cpu)
{
	const struct vmbus_softc *sc = device_get_softc(bus);

	KASSERT(cpu >= 0 && cpu < mp_ncpus, ("invalid cpu%d", cpu));
	return (VMBUS_PCPU_GET(sc, event_tq, cpu));
}

#ifdef NEW_PCIB
#define VTPM_BASE_ADDR 0xfed40000
#define FOUR_GB (1ULL << 32)

enum parse_pass { parse_64, parse_32 };

struct parse_context {
	device_t vmbus_dev;
	enum parse_pass pass;
};

static ACPI_STATUS
parse_crs(ACPI_RESOURCE *res, void *ctx)
{
	const struct parse_context *pc = ctx;
	device_t vmbus_dev = pc->vmbus_dev;

	struct vmbus_softc *sc = device_get_softc(vmbus_dev);
	UINT64 start, end;

	switch (res->Type) {
	case ACPI_RESOURCE_TYPE_ADDRESS32:
		start = res->Data.Address32.Address.Minimum;
		end = res->Data.Address32.Address.Maximum;
		break;

	case ACPI_RESOURCE_TYPE_ADDRESS64:
		start = res->Data.Address64.Address.Minimum;
		end = res->Data.Address64.Address.Maximum;
		break;

	default:
		/* Unused types. */
		return (AE_OK);
	}

	/*
	 * We don't use <1MB addresses.
	 */
	if (end < 0x100000)
		return (AE_OK);

	/* Don't conflict with vTPM. */
	if (end >= VTPM_BASE_ADDR && start < VTPM_BASE_ADDR)
		end = VTPM_BASE_ADDR - 1;

	if ((pc->pass == parse_32 && start < FOUR_GB) ||
	    (pc->pass == parse_64 && start >= FOUR_GB))
		pcib_host_res_decodes(&sc->vmbus_mmio_res, SYS_RES_MEMORY,
		    start, end, 0);

	return (AE_OK);
}

static void
vmbus_get_crs(device_t dev, device_t vmbus_dev, enum parse_pass pass)
{
	struct parse_context pc;
	ACPI_STATUS status;

	if (bootverbose)
		device_printf(dev, "walking _CRS, pass=%d\n", pass);

	pc.vmbus_dev = vmbus_dev;
	pc.pass = pass;
	status = AcpiWalkResources(acpi_get_handle(dev), "_CRS",
			parse_crs, &pc);

	if (bootverbose && ACPI_FAILURE(status))
		device_printf(dev, "_CRS: not found, pass=%d\n", pass);
}

static void
vmbus_get_mmio_res_pass(device_t dev, enum parse_pass pass)
{
	device_t acpi0, parent;

	parent = device_get_parent(dev);

	acpi0 = device_get_parent(parent);
	if (strcmp("acpi0", device_get_nameunit(acpi0)) == 0) {
		device_t *children;
		int count;

		/*
		 * Try to locate VMBUS resources and find _CRS on them.
		 */
		if (device_get_children(acpi0, &children, &count) == 0) {
			int i;

			for (i = 0; i < count; ++i) {
				if (!device_is_attached(children[i]))
					continue;

				if (strcmp("vmbus_res",
				    device_get_name(children[i])) == 0)
					vmbus_get_crs(children[i], dev, pass);
			}
			free(children, M_TEMP);
		}

		/*
		 * Try to find _CRS on acpi.
		 */
		vmbus_get_crs(acpi0, dev, pass);
	} else {
		device_printf(dev, "not grandchild of acpi\n");
	}

	/*
	 * Try to find _CRS on parent.
	 */
	vmbus_get_crs(parent, dev, pass);
}

static void
vmbus_get_mmio_res(device_t dev)
{
	struct vmbus_softc *sc = device_get_softc(dev);
	/*
	 * We walk the resources twice to make sure that: in the resource
	 * list, the 32-bit resources appear behind the 64-bit resources.
	 * NB: resource_list_add() uses INSERT_TAIL. This way, when we
	 * iterate through the list to find a range for a 64-bit BAR in
	 * vmbus_alloc_resource(), we can make sure we try to use >4GB
	 * ranges first.
	 */
	pcib_host_res_init(dev, &sc->vmbus_mmio_res);

	vmbus_get_mmio_res_pass(dev, parse_64);
	vmbus_get_mmio_res_pass(dev, parse_32);
}

/*
 * On Gen2 VMs, Hyper-V provides mmio space for framebuffer.
 * This mmio address range is not useable for other PCI devices.
 * Currently only efifb and vbefb drivers are using this range without
 * reserving it from system.
 * Therefore, vmbus driver reserves it before any other PCI device
 * drivers start to request mmio addresses.
 */
static struct resource *hv_fb_res;

static void
vmbus_fb_mmio_res(device_t dev)
{
	struct efi_fb *efifb;
#if !defined(__aarch64__)
	struct vbe_fb *vbefb;
#endif /* aarch64 */
	rman_res_t fb_start, fb_end, fb_count;
	int fb_height, fb_width;
	caddr_t kmdp;

	struct vmbus_softc *sc = device_get_softc(dev);
	int rid = 0;

	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	efifb = (struct efi_fb *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_FB);
#if !defined(__aarch64__)
	vbefb = (struct vbe_fb *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_VBE_FB);
#endif /* aarch64 */
	if (efifb != NULL) {
		fb_start = efifb->fb_addr;
		fb_end = efifb->fb_addr + efifb->fb_size;
		fb_count = efifb->fb_size;
		fb_height = efifb->fb_height;
		fb_width = efifb->fb_width;
	}
#if !defined(__aarch64__)
	else if (vbefb != NULL) {
		fb_start = vbefb->fb_addr;
		fb_end = vbefb->fb_addr + vbefb->fb_size;
		fb_count = vbefb->fb_size;
		fb_height = vbefb->fb_height;
		fb_width = vbefb->fb_width;
	}
#endif /* aarch64 */
	else {
		if (bootverbose)
			device_printf(dev,
			    "no preloaded kernel fb information\n");
		/* We are on Gen1 VM, just return. */
		return;
	}

	if (bootverbose)
		device_printf(dev,
		    "fb: fb_addr: %#jx, size: %#jx, "
		    "actual size needed: 0x%x\n",
		    fb_start, fb_count, fb_height * fb_width);

	hv_fb_res = pcib_host_res_alloc(&sc->vmbus_mmio_res, dev,
	    SYS_RES_MEMORY, &rid, fb_start, fb_end, fb_count,
	    RF_ACTIVE | rman_make_alignment_flags(PAGE_SIZE));

	if (hv_fb_res && bootverbose)
		device_printf(dev,
		    "successfully reserved memory for framebuffer "
		    "starting at %#jx, size %#jx\n",
		    fb_start, fb_count);
}

static void
vmbus_free_mmio_res(device_t dev)
{
	struct vmbus_softc *sc = device_get_softc(dev);

	pcib_host_res_free(dev, &sc->vmbus_mmio_res);

	if (hv_fb_res)
		hv_fb_res = NULL;
}
#endif	/* NEW_PCIB */

static void
vmbus_identify(driver_t *driver, device_t parent)
{

	if (device_get_unit(parent) != 0 || vm_guest != VM_GUEST_HV ||
	    (hyperv_features & CPUID_HV_MSR_SYNIC) == 0)
		return;
	device_add_child(parent, "vmbus", -1);
}

static int
vmbus_probe(device_t dev)
{

	if (device_get_unit(dev) != 0 || vm_guest != VM_GUEST_HV ||
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
	device_t dev_res;
	ACPI_HANDLE handle;
	unsigned int coherent;

	if (sc->vmbus_flags & VMBUS_FLAG_ATTACHED)
		return (0);

#ifdef NEW_PCIB
	vmbus_get_mmio_res(sc->vmbus_dev);
	vmbus_fb_mmio_res(sc->vmbus_dev);
#endif

	sc->vmbus_flags |= VMBUS_FLAG_ATTACHED;

	sc->vmbus_gpadl = VMBUS_GPADL_START;
	mtx_init(&sc->vmbus_prichan_lock, "vmbus prichan", NULL, MTX_DEF);
	TAILQ_INIT(&sc->vmbus_prichans);
	mtx_init(&sc->vmbus_chan_lock, "vmbus channel", NULL, MTX_DEF);
	TAILQ_INIT(&sc->vmbus_chans);
	sc->vmbus_chmap = malloc(
	    sizeof(struct vmbus_channel *) * VMBUS_CHAN_MAX, M_DEVBUF,
	    M_WAITOK | M_ZERO);

	/* Coherency attribute */
	dev_res =  devclass_get_device(devclass_find("vmbus_res"), 0);
	handle = acpi_get_handle(dev_res);

	if (ACPI_FAILURE(acpi_GetInteger(handle, "_CCA", &coherent)))
		coherent = 0;
	if (bootverbose)
		device_printf(sc->vmbus_dev, "Bus is%s cache-coherent\n",
			coherent ? "" : " not");

	bus_dma_tag_create(bus_get_dma_tag(sc->vmbus_dev),
		1, 0,
		BUS_SPACE_MAXADDR,
		BUS_SPACE_MAXADDR,
		NULL, NULL,
		BUS_SPACE_MAXSIZE,
		BUS_SPACE_UNRESTRICTED,
		BUS_SPACE_MAXSIZE,
		coherent ? BUS_DMA_COHERENT : 0,
		NULL, NULL,
		&sc->dmat);
	/*
	 * Create context for "post message" Hypercalls
	 */
	sc->vmbus_xc = vmbus_xact_ctx_create(bus_get_dma_tag(sc->vmbus_dev),
	    HYPERCALL_POSTMSGIN_SIZE, VMBUS_MSG_SIZE,
	    sizeof(struct vmbus_msghc));
	if (sc->vmbus_xc == NULL) {
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
	vmbus_scan_teardown(sc);
	vmbus_intr_teardown(sc);
	vmbus_dma_free(sc);
	if (sc->vmbus_xc != NULL) {
		vmbus_xact_ctx_destroy(sc->vmbus_xc);
		sc->vmbus_xc = NULL;
	}
	free(__DEVOLATILE(void *, sc->vmbus_chmap), M_DEVBUF);
	mtx_destroy(&sc->vmbus_prichan_lock);
	mtx_destroy(&sc->vmbus_chan_lock);

	return (ret);
}

static void
vmbus_event_proc_dummy(struct vmbus_softc *sc __unused, int cpu __unused)
{
}

#if defined(EARLY_AP_STARTUP)

static void
vmbus_intrhook(void *xsc)
{
	struct vmbus_softc *sc = xsc;

	if (bootverbose)
		device_printf(sc->vmbus_dev, "intrhook\n");
	vmbus_doattach(sc);
	config_intrhook_disestablish(&sc->vmbus_intrhook);
}

#endif /* EARLY_AP_STARTUP */

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

#if defined(EARLY_AP_STARTUP)
	/*
	 * Defer the real attach until the pause(9) works as expected.
	 */
	vmbus_sc->vmbus_intrhook.ich_func = vmbus_intrhook;
	vmbus_sc->vmbus_intrhook.ich_arg = vmbus_sc;
	config_intrhook_establish(&vmbus_sc->vmbus_intrhook);
#endif /* EARLY_AP_STARTUP  and aarch64 */

	return (0);
}

static int
vmbus_detach(device_t dev)
{
	struct vmbus_softc *sc = device_get_softc(dev);

	bus_generic_detach(dev);
	vmbus_chan_destroy_all(sc);

	vmbus_scan_teardown(sc);

	vmbus_disconnect(sc);

	if (sc->vmbus_flags & VMBUS_FLAG_SYNIC) {
		sc->vmbus_flags &= ~VMBUS_FLAG_SYNIC;
		smp_rendezvous(NULL, vmbus_synic_teardown, NULL, NULL);
	}

	vmbus_intr_teardown(sc);
	vmbus_dma_free(sc);

	if (sc->vmbus_xc != NULL) {
		vmbus_xact_ctx_destroy(sc->vmbus_xc);
		sc->vmbus_xc = NULL;
	}

	free(__DEVOLATILE(void *, sc->vmbus_chmap), M_DEVBUF);
	mtx_destroy(&sc->vmbus_prichan_lock);
	mtx_destroy(&sc->vmbus_chan_lock);

#ifdef NEW_PCIB
	vmbus_free_mmio_res(dev);
#endif

#if defined(__aarch64__)
	bus_release_resource(device_get_parent(dev), SYS_RES_IRQ, sc->vector,
	    sc->ires);
#endif
	return (0);
}

#if !defined(EARLY_AP_STARTUP)

static void
vmbus_sysinit(void *arg __unused)
{
	struct vmbus_softc *sc = vmbus_get_softc();

	if (vm_guest != VM_GUEST_HV || sc == NULL)
		return;

	vmbus_doattach(sc);
}
/*
 * NOTE:
 * We have to start as the last step of SI_SUB_SMP, i.e. after SMP is
 * initialized.
 */
SYSINIT(vmbus_initialize, SI_SUB_SMP, SI_ORDER_ANY, vmbus_sysinit, NULL);
#endif	/* !EARLY_AP_STARTUP */
