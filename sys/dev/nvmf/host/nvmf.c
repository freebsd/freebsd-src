/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/dnv.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/nv.h>
#include <sys/reboot.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <dev/nvme/nvme.h>
#include <dev/nvmf/nvmf.h>
#include <dev/nvmf/nvmf_transport.h>
#include <dev/nvmf/host/nvmf_var.h>

static struct cdevsw nvmf_cdevsw;
static struct taskqueue *nvmf_tq;

bool nvmf_fail_disconnect = false;
SYSCTL_BOOL(_kern_nvmf, OID_AUTO, fail_on_disconnection, CTLFLAG_RWTUN,
    &nvmf_fail_disconnect, 0, "Fail I/O requests on connection failure");

MALLOC_DEFINE(M_NVMF, "nvmf", "NVMe over Fabrics host");

static void	nvmf_controller_loss_task(void *arg, int pending);
static void	nvmf_disconnect_task(void *arg, int pending);
static void	nvmf_request_reconnect(struct nvmf_softc *sc);
static void	nvmf_request_reconnect_task(void *arg, int pending);
static void	nvmf_shutdown_pre_sync(void *arg, int howto);
static void	nvmf_shutdown_post_sync(void *arg, int howto);

void
nvmf_complete(void *arg, const struct nvme_completion *cqe)
{
	struct nvmf_completion_status *status = arg;
	struct mtx *mtx;

	status->cqe = *cqe;
	mtx = mtx_pool_find(mtxpool_sleep, status);
	mtx_lock(mtx);
	status->done = true;
	mtx_unlock(mtx);
	wakeup(status);
}

void
nvmf_io_complete(void *arg, size_t xfered, int error)
{
	struct nvmf_completion_status *status = arg;
	struct mtx *mtx;

	status->io_error = error;
	mtx = mtx_pool_find(mtxpool_sleep, status);
	mtx_lock(mtx);
	status->io_done = true;
	mtx_unlock(mtx);
	wakeup(status);
}

void
nvmf_wait_for_reply(struct nvmf_completion_status *status)
{
	struct mtx *mtx;

	mtx = mtx_pool_find(mtxpool_sleep, status);
	mtx_lock(mtx);
	while (!status->done || !status->io_done)
		mtx_sleep(status, mtx, 0, "nvmfcmd", 0);
	mtx_unlock(mtx);
}

static int
nvmf_read_property(struct nvmf_softc *sc, uint32_t offset, uint8_t size,
    uint64_t *value)
{
	const struct nvmf_fabric_prop_get_rsp *rsp;
	struct nvmf_completion_status status;

	nvmf_status_init(&status);
	if (!nvmf_cmd_get_property(sc, offset, size, nvmf_complete, &status,
	    M_WAITOK))
		return (ECONNABORTED);
	nvmf_wait_for_reply(&status);

	if (status.cqe.status != 0) {
		device_printf(sc->dev, "PROPERTY_GET failed, status %#x\n",
		    le16toh(status.cqe.status));
		return (EIO);
	}

	rsp = (const struct nvmf_fabric_prop_get_rsp *)&status.cqe;
	if (size == 8)
		*value = le64toh(rsp->value.u64);
	else
		*value = le32toh(rsp->value.u32.low);
	return (0);
}

static int
nvmf_write_property(struct nvmf_softc *sc, uint32_t offset, uint8_t size,
    uint64_t value)
{
	struct nvmf_completion_status status;

	nvmf_status_init(&status);
	if (!nvmf_cmd_set_property(sc, offset, size, value, nvmf_complete, &status,
	    M_WAITOK))
		return (ECONNABORTED);
	nvmf_wait_for_reply(&status);

	if (status.cqe.status != 0) {
		device_printf(sc->dev, "PROPERTY_SET failed, status %#x\n",
		    le16toh(status.cqe.status));
		return (EIO);
	}
	return (0);
}

static void
nvmf_shutdown_controller(struct nvmf_softc *sc)
{
	uint64_t cc;
	int error;

	error = nvmf_read_property(sc, NVMF_PROP_CC, 4, &cc);
	if (error != 0) {
		device_printf(sc->dev, "Failed to fetch CC for shutdown\n");
		return;
	}

	cc |= NVMEF(NVME_CC_REG_SHN, NVME_SHN_NORMAL);

	error = nvmf_write_property(sc, NVMF_PROP_CC, 4, cc);
	if (error != 0)
		device_printf(sc->dev,
		    "Failed to set CC to trigger shutdown\n");
}

static void
nvmf_check_keep_alive(void *arg)
{
	struct nvmf_softc *sc = arg;
	int traffic;

	traffic = atomic_readandclear_int(&sc->ka_active_rx_traffic);
	if (traffic == 0) {
		device_printf(sc->dev,
		    "disconnecting due to KeepAlive timeout\n");
		nvmf_disconnect(sc);
		return;
	}

	callout_schedule_sbt(&sc->ka_rx_timer, sc->ka_rx_sbt, 0, C_HARDCLOCK);
}

static void
nvmf_keep_alive_complete(void *arg, const struct nvme_completion *cqe)
{
	struct nvmf_softc *sc = arg;

	atomic_store_int(&sc->ka_active_rx_traffic, 1);
	if (cqe->status != 0) {
		device_printf(sc->dev,
		    "KeepAlive response reported status %#x\n",
		    le16toh(cqe->status));
	}
}

static void
nvmf_send_keep_alive(void *arg)
{
	struct nvmf_softc *sc = arg;
	int traffic;

	/*
	 * Don't bother sending a KeepAlive command if TKAS is active
	 * and another command has been sent during the interval.
	 */
	traffic = atomic_load_int(&sc->ka_active_tx_traffic);
	if (traffic == 0 && !nvmf_cmd_keep_alive(sc, nvmf_keep_alive_complete,
	    sc, M_NOWAIT))
		device_printf(sc->dev,
		    "Failed to allocate KeepAlive command\n");

	/* Clear ka_active_tx_traffic after sending the keep alive command. */
	atomic_store_int(&sc->ka_active_tx_traffic, 0);

	callout_schedule_sbt(&sc->ka_tx_timer, sc->ka_tx_sbt, 0, C_HARDCLOCK);
}

int
nvmf_copyin_handoff(const struct nvmf_ioc_nv *nv, nvlist_t **nvlp)
{
	const struct nvme_discovery_log_entry *dle;
	const struct nvme_controller_data *cdata;
	const nvlist_t *const *io;
	const nvlist_t *admin, *rparams;
	nvlist_t *nvl;
	size_t i, num_io_queues;
	uint32_t qsize;
	int error;

	error = nvmf_unpack_ioc_nvlist(nv, &nvl);
	if (error != 0)
		return (error);

	if (!nvlist_exists_number(nvl, "trtype") ||
	    !nvlist_exists_nvlist(nvl, "admin") ||
	    !nvlist_exists_nvlist_array(nvl, "io") ||
	    !nvlist_exists_binary(nvl, "cdata") ||
	    !nvlist_exists_nvlist(nvl, "rparams"))
		goto invalid;

	rparams = nvlist_get_nvlist(nvl, "rparams");
	if (!nvlist_exists_binary(rparams, "dle") ||
	    !nvlist_exists_string(rparams, "hostnqn") ||
	    !nvlist_exists_number(rparams, "num_io_queues") ||
	    !nvlist_exists_number(rparams, "io_qsize"))
		goto invalid;

	admin = nvlist_get_nvlist(nvl, "admin");
	if (!nvmf_validate_qpair_nvlist(admin, false))
		goto invalid;
	if (!nvlist_get_bool(admin, "admin"))
		goto invalid;

	io = nvlist_get_nvlist_array(nvl, "io", &num_io_queues);
	if (num_io_queues < 1 ||
	    num_io_queues != nvlist_get_number(rparams, "num_io_queues"))
		goto invalid;
	for (i = 0; i < num_io_queues; i++) {
		if (!nvmf_validate_qpair_nvlist(io[i], false))
			goto invalid;
	}

	/* Require all I/O queues to be the same size. */
	qsize = nvlist_get_number(rparams, "io_qsize");
	for (i = 0; i < num_io_queues; i++) {
		if (nvlist_get_number(io[i], "qsize") != qsize)
			goto invalid;
	}

	cdata = nvlist_get_binary(nvl, "cdata", &i);
	if (i != sizeof(*cdata))
		goto invalid;
	dle = nvlist_get_binary(rparams, "dle", &i);
	if (i != sizeof(*dle))
		goto invalid;

	if (memcmp(dle->subnqn, cdata->subnqn, sizeof(cdata->subnqn)) != 0)
		goto invalid;

	*nvlp = nvl;
	return (0);
invalid:
	nvlist_destroy(nvl);
	return (EINVAL);
}

static int
nvmf_probe(device_t dev)
{
	const nvlist_t *nvl = device_get_ivars(dev);
	const struct nvme_controller_data *cdata;

	if (nvl == NULL)
		return (ENXIO);

	cdata = nvlist_get_binary(nvl, "cdata", NULL);
	device_set_descf(dev, "Fabrics: %.256s", cdata->subnqn);
	return (BUS_PROBE_DEFAULT);
}

static int
nvmf_establish_connection(struct nvmf_softc *sc, nvlist_t *nvl)
{
	const nvlist_t *const *io;
	const nvlist_t *admin;
	uint64_t kato;
	size_t num_io_queues;
	enum nvmf_trtype trtype;
	char name[16];

	trtype = nvlist_get_number(nvl, "trtype");
	admin = nvlist_get_nvlist(nvl, "admin");
	io = nvlist_get_nvlist_array(nvl, "io", &num_io_queues);
	kato = dnvlist_get_number(nvl, "kato", 0);
	sc->reconnect_delay = dnvlist_get_number(nvl, "reconnect_delay", 0);
	sc->controller_loss_timeout = dnvlist_get_number(nvl,
	    "controller_loss_timeout", 0);

	/* Setup the admin queue. */
	sc->admin = nvmf_init_qp(sc, trtype, admin, "admin queue", 0);
	if (sc->admin == NULL) {
		device_printf(sc->dev, "Failed to setup admin queue\n");
		return (ENXIO);
	}

	/* Setup I/O queues. */
	sc->io = malloc(num_io_queues * sizeof(*sc->io), M_NVMF,
	    M_WAITOK | M_ZERO);
	sc->num_io_queues = num_io_queues;
	for (u_int i = 0; i < sc->num_io_queues; i++) {
		snprintf(name, sizeof(name), "I/O queue %u", i);
		sc->io[i] = nvmf_init_qp(sc, trtype, io[i], name, i);
		if (sc->io[i] == NULL) {
			device_printf(sc->dev, "Failed to setup I/O queue %u\n",
			    i);
			return (ENXIO);
		}
	}

	/* Start KeepAlive timers. */
	if (kato != 0) {
		sc->ka_traffic = NVMEV(NVME_CTRLR_DATA_CTRATT_TBKAS,
		    sc->cdata->ctratt) != 0;
		sc->ka_rx_sbt = mstosbt(kato);
		sc->ka_tx_sbt = sc->ka_rx_sbt / 2;
		callout_reset_sbt(&sc->ka_rx_timer, sc->ka_rx_sbt, 0,
		    nvmf_check_keep_alive, sc, C_HARDCLOCK);
		callout_reset_sbt(&sc->ka_tx_timer, sc->ka_tx_sbt, 0,
		    nvmf_send_keep_alive, sc, C_HARDCLOCK);
	}

	memcpy(sc->cdata, nvlist_get_binary(nvl, "cdata", NULL),
	    sizeof(*sc->cdata));

	/* Save reconnect parameters. */
	nvlist_destroy(sc->rparams);
	sc->rparams = nvlist_take_nvlist(nvl, "rparams");

	return (0);
}

typedef bool nvmf_scan_active_ns_cb(struct nvmf_softc *, uint32_t,
    const struct nvme_namespace_data *, void *);

static bool
nvmf_scan_active_nslist(struct nvmf_softc *sc, struct nvme_ns_list *nslist,
    struct nvme_namespace_data *data, uint32_t *nsidp,
    nvmf_scan_active_ns_cb *cb, void *cb_arg)
{
	struct nvmf_completion_status status;
	uint32_t nsid;

	nvmf_status_init(&status);
	nvmf_status_wait_io(&status);
	if (!nvmf_cmd_identify_active_namespaces(sc, *nsidp, nslist,
	    nvmf_complete, &status, nvmf_io_complete, &status, M_WAITOK)) {
		device_printf(sc->dev,
		    "failed to send IDENTIFY active namespaces command\n");
		return (false);
	}
	nvmf_wait_for_reply(&status);

	if (status.cqe.status != 0) {
		device_printf(sc->dev,
		    "IDENTIFY active namespaces failed, status %#x\n",
		    le16toh(status.cqe.status));
		return (false);
	}

	if (status.io_error != 0) {
		device_printf(sc->dev,
		    "IDENTIFY active namespaces failed with I/O error %d\n",
		    status.io_error);
		return (false);
	}

	for (u_int i = 0; i < nitems(nslist->ns); i++) {
		nsid = nslist->ns[i];
		if (nsid == 0) {
			*nsidp = 0;
			return (true);
		}

		nvmf_status_init(&status);
		nvmf_status_wait_io(&status);
		if (!nvmf_cmd_identify_namespace(sc, nsid, data, nvmf_complete,
		    &status, nvmf_io_complete, &status, M_WAITOK)) {
			device_printf(sc->dev,
			    "failed to send IDENTIFY namespace %u command\n",
			    nsid);
			return (false);
		}
		nvmf_wait_for_reply(&status);

		if (status.cqe.status != 0) {
			device_printf(sc->dev,
			    "IDENTIFY namespace %u failed, status %#x\n", nsid,
			    le16toh(status.cqe.status));
			return (false);
		}

		if (status.io_error != 0) {
			device_printf(sc->dev,
			    "IDENTIFY namespace %u failed with I/O error %d\n",
			    nsid, status.io_error);
			return (false);
		}

		nvme_namespace_data_swapbytes(data);
		if (!cb(sc, nsid, data, cb_arg))
			return (false);
	}

	MPASS(nsid == nslist->ns[nitems(nslist->ns) - 1] && nsid != 0);

	if (nsid >= NVME_GLOBAL_NAMESPACE_TAG - 1)
		*nsidp = 0;
	else
		*nsidp = nsid;
	return (true);
}

static bool
nvmf_scan_active_namespaces(struct nvmf_softc *sc, nvmf_scan_active_ns_cb *cb,
    void *cb_arg)
{
	struct nvme_namespace_data *data;
	struct nvme_ns_list *nslist;
	uint32_t nsid;
	bool retval;

	nslist = malloc(sizeof(*nslist), M_NVMF, M_WAITOK);
	data = malloc(sizeof(*data), M_NVMF, M_WAITOK);

	nsid = 0;
	retval = true;
	for (;;) {
		if (!nvmf_scan_active_nslist(sc, nslist, data, &nsid, cb,
		    cb_arg)) {
			retval = false;
			break;
		}
		if (nsid == 0)
			break;
	}

	free(data, M_NVMF);
	free(nslist, M_NVMF);
	return (retval);
}

static bool
nvmf_add_ns(struct nvmf_softc *sc, uint32_t nsid,
    const struct nvme_namespace_data *data, void *arg __unused)
{
	if (sc->ns[nsid - 1] != NULL) {
		device_printf(sc->dev,
		    "duplicate namespace %u in active namespace list\n",
		    nsid);
		return (false);
	}

	/*
	 * As in nvme_ns_construct, a size of zero indicates an
	 * invalid namespace.
	 */
	if (data->nsze == 0) {
		device_printf(sc->dev,
		    "ignoring active namespace %u with zero size\n", nsid);
		return (true);
	}

	sc->ns[nsid - 1] = nvmf_init_ns(sc, nsid, data);

	nvmf_sim_rescan_ns(sc, nsid);
	return (true);
}

static bool
nvmf_add_namespaces(struct nvmf_softc *sc)
{
	sc->ns = mallocarray(sc->cdata->nn, sizeof(*sc->ns), M_NVMF,
	    M_WAITOK | M_ZERO);
	return (nvmf_scan_active_namespaces(sc, nvmf_add_ns, NULL));
}

static int
nvmf_attach(device_t dev)
{
	struct make_dev_args mda;
	struct nvmf_softc *sc = device_get_softc(dev);
	nvlist_t *nvl = device_get_ivars(dev);
	const nvlist_t * const *io;
	struct sysctl_oid *oid;
	uint64_t val;
	u_int i;
	int error;

	if (nvl == NULL)
		return (ENXIO);

	sc->dev = dev;
	sc->trtype = nvlist_get_number(nvl, "trtype");
	callout_init(&sc->ka_rx_timer, 1);
	callout_init(&sc->ka_tx_timer, 1);
	sx_init(&sc->connection_lock, "nvmf connection");
	TASK_INIT(&sc->disconnect_task, 0, nvmf_disconnect_task, sc);
	TIMEOUT_TASK_INIT(nvmf_tq, &sc->controller_loss_task, 0,
	    nvmf_controller_loss_task, sc);
	TIMEOUT_TASK_INIT(nvmf_tq, &sc->request_reconnect_task, 0,
	    nvmf_request_reconnect_task, sc);

	oid = SYSCTL_ADD_NODE(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "ioq",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "I/O Queues");
	sc->ioq_oid_list = SYSCTL_CHILDREN(oid);

	sc->cdata = malloc(sizeof(*sc->cdata), M_NVMF, M_WAITOK);

	nvmf_init_aer(sc);

	error = nvmf_establish_connection(sc, nvl);
	if (error != 0)
		goto out;

	error = nvmf_read_property(sc, NVMF_PROP_CAP, 8, &sc->cap);
	if (error != 0) {
		device_printf(sc->dev, "Failed to fetch CAP\n");
		error = ENXIO;
		goto out;
	}

	error = nvmf_read_property(sc, NVMF_PROP_VS, 4, &val);
	if (error != 0) {
		device_printf(sc->dev, "Failed to fetch VS\n");
		error = ENXIO;
		goto out;
	}
	sc->vs = val;

	/* Honor MDTS if it is set. */
	sc->max_xfer_size = maxphys;
	if (sc->cdata->mdts != 0) {
		sc->max_xfer_size = ulmin(sc->max_xfer_size,
		    1 << (sc->cdata->mdts + NVME_MPS_SHIFT +
		    NVME_CAP_HI_MPSMIN(sc->cap >> 32)));
	}

	io = nvlist_get_nvlist_array(nvl, "io", NULL);
	sc->max_pending_io = nvlist_get_number(io[0], "qsize") *
	    sc->num_io_queues;

	error = nvmf_init_sim(sc);
	if (error != 0)
		goto out;

	error = nvmf_start_aer(sc);
	if (error != 0) {
		nvmf_destroy_sim(sc);
		goto out;
	}

	if (!nvmf_add_namespaces(sc)) {
		nvmf_destroy_sim(sc);
		goto out;
	}

	make_dev_args_init(&mda);
	mda.mda_devsw = &nvmf_cdevsw;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_WHEEL;
	mda.mda_mode = 0600;
	mda.mda_si_drv1 = sc;
	error = make_dev_s(&mda, &sc->cdev, "%s", device_get_nameunit(dev));
	if (error != 0) {
		nvmf_destroy_sim(sc);
		goto out;
	}

	sc->shutdown_pre_sync_eh = EVENTHANDLER_REGISTER(shutdown_pre_sync,
	    nvmf_shutdown_pre_sync, sc, SHUTDOWN_PRI_FIRST);
	sc->shutdown_post_sync_eh = EVENTHANDLER_REGISTER(shutdown_post_sync,
	    nvmf_shutdown_post_sync, sc, SHUTDOWN_PRI_LAST);

	return (0);
out:
	if (sc->ns != NULL) {
		for (i = 0; i < sc->cdata->nn; i++) {
			if (sc->ns[i] != NULL)
				nvmf_destroy_ns(sc->ns[i]);
		}
		free(sc->ns, M_NVMF);
	}

	callout_drain(&sc->ka_tx_timer);
	callout_drain(&sc->ka_rx_timer);

	if (sc->admin != NULL)
		nvmf_shutdown_controller(sc);

	for (i = 0; i < sc->num_io_queues; i++) {
		if (sc->io[i] != NULL)
			nvmf_destroy_qp(sc->io[i]);
	}
	free(sc->io, M_NVMF);
	if (sc->admin != NULL)
		nvmf_destroy_qp(sc->admin);

	nvmf_destroy_aer(sc);

	taskqueue_drain_timeout(nvmf_tq, &sc->request_reconnect_task);
	taskqueue_drain_timeout(nvmf_tq, &sc->controller_loss_task);
	taskqueue_drain(nvmf_tq, &sc->disconnect_task);
	sx_destroy(&sc->connection_lock);
	nvlist_destroy(sc->rparams);
	free(sc->cdata, M_NVMF);
	return (error);
}

void
nvmf_disconnect(struct nvmf_softc *sc)
{
	taskqueue_enqueue(nvmf_tq, &sc->disconnect_task);
}

static void
nvmf_disconnect_task(void *arg, int pending __unused)
{
	struct nvmf_softc *sc = arg;
	u_int i;

	sx_xlock(&sc->connection_lock);
	if (sc->admin == NULL) {
		/*
		 * Ignore transport errors if there is no active
		 * association.
		 */
		sx_xunlock(&sc->connection_lock);
		return;
	}

	if (sc->detaching) {
		if (sc->admin != NULL) {
			/*
			 * This unsticks the detach process if a
			 * transport error occurs during detach.
			 */
			nvmf_shutdown_qp(sc->admin);
		}
		sx_xunlock(&sc->connection_lock);
		return;
	}

	if (sc->cdev == NULL) {
		/*
		 * Transport error occurred during attach (nvmf_add_namespaces).
		 * Shutdown the admin queue.
		 */
		nvmf_shutdown_qp(sc->admin);
		sx_xunlock(&sc->connection_lock);
		return;
	}

	nanotime(&sc->last_disconnect);
	callout_drain(&sc->ka_tx_timer);
	callout_drain(&sc->ka_rx_timer);
	sc->ka_traffic = false;

	/* Quiesce namespace consumers. */
	nvmf_disconnect_sim(sc);
	for (i = 0; i < sc->cdata->nn; i++) {
		if (sc->ns[i] != NULL)
			nvmf_disconnect_ns(sc->ns[i]);
	}

	/* Shutdown the existing qpairs. */
	for (i = 0; i < sc->num_io_queues; i++) {
		nvmf_destroy_qp(sc->io[i]);
	}
	free(sc->io, M_NVMF);
	sc->io = NULL;
	sc->num_io_queues = 0;
	nvmf_destroy_qp(sc->admin);
	sc->admin = NULL;

	if (sc->reconnect_delay != 0)
		nvmf_request_reconnect(sc);
	if (sc->controller_loss_timeout != 0)
		taskqueue_enqueue_timeout(nvmf_tq,
		    &sc->controller_loss_task, sc->controller_loss_timeout *
		    hz);

	sx_xunlock(&sc->connection_lock);
}

static void
nvmf_controller_loss_task(void *arg, int pending)
{
	struct nvmf_softc *sc = arg;
	device_t dev;
	int error;

	bus_topo_lock();
	sx_xlock(&sc->connection_lock);
	if (sc->admin != NULL || sc->detaching) {
		/* Reconnected or already detaching. */
		sx_xunlock(&sc->connection_lock);
		bus_topo_unlock();
		return;
	}

	sc->controller_timedout = true;
	sx_xunlock(&sc->connection_lock);

	/*
	 * XXX: Doing this from here is a bit ugly.  We don't have an
	 * extra reference on `dev` but bus_topo_lock should block any
	 * concurrent device_delete_child invocations.
	 */
	dev = sc->dev;
	error = device_delete_child(root_bus, dev);
	if (error != 0)
		device_printf(dev,
		    "failed to detach after controller loss: %d\n", error);
	bus_topo_unlock();
}

static void
nvmf_request_reconnect(struct nvmf_softc *sc)
{
	char buf[64];

	sx_assert(&sc->connection_lock, SX_LOCKED);

	snprintf(buf, sizeof(buf), "name=\"%s\"", device_get_nameunit(sc->dev));
	devctl_notify("nvme", "controller", "RECONNECT", buf);
	taskqueue_enqueue_timeout(nvmf_tq, &sc->request_reconnect_task,
	    sc->reconnect_delay * hz);
}

static void
nvmf_request_reconnect_task(void *arg, int pending)
{
	struct nvmf_softc *sc = arg;

	sx_xlock(&sc->connection_lock);
	if (sc->admin != NULL || sc->detaching || sc->controller_timedout) {
		/* Reconnected or already detaching. */
		sx_xunlock(&sc->connection_lock);
		return;
	}

	nvmf_request_reconnect(sc);
	sx_xunlock(&sc->connection_lock);
}

static int
nvmf_reconnect_host(struct nvmf_softc *sc, struct nvmf_ioc_nv *nv)
{
	const struct nvme_controller_data *cdata;
	nvlist_t *nvl;
	u_int i;
	int error;

	error = nvmf_copyin_handoff(nv, &nvl);
	if (error != 0)
		return (error);

	/* XXX: Should we permit changing the transport type? */
	if (sc->trtype != nvlist_get_number(nvl, "trtype")) {
		device_printf(sc->dev,
		    "transport type mismatch on reconnect\n");
		return (EINVAL);
	}

	sx_xlock(&sc->connection_lock);
	if (sc->admin != NULL || sc->detaching || sc->controller_timedout) {
		error = EBUSY;
		goto out;
	}

	/*
	 * Ensure this is for the same controller.  Note that the
	 * controller ID can vary across associations if the remote
	 * system is using the dynamic controller model.  This merely
	 * ensures the new association is connected to the same NVMe
	 * subsystem.
	 */
	cdata = nvlist_get_binary(nvl, "cdata", NULL);
	if (memcmp(sc->cdata->subnqn, cdata->subnqn,
	    sizeof(cdata->subnqn)) != 0) {
		device_printf(sc->dev,
		    "controller subsystem NQN mismatch on reconnect\n");
		error = EINVAL;
		goto out;
	}

	/*
	 * XXX: Require same number and size of I/O queues so that
	 * max_pending_io is still correct?
	 */

	error = nvmf_establish_connection(sc, nvl);
	if (error != 0)
		goto out;

	error = nvmf_start_aer(sc);
	if (error != 0)
		goto out;

	device_printf(sc->dev,
	    "established new association with %u I/O queues\n",
	    sc->num_io_queues);

	/* Restart namespace consumers. */
	for (i = 0; i < sc->cdata->nn; i++) {
		if (sc->ns[i] != NULL)
			nvmf_reconnect_ns(sc->ns[i]);
	}
	nvmf_reconnect_sim(sc);

	nvmf_rescan_all_ns(sc);

	taskqueue_cancel_timeout(nvmf_tq, &sc->request_reconnect_task, NULL);
	taskqueue_cancel_timeout(nvmf_tq, &sc->controller_loss_task, NULL);
out:
	sx_xunlock(&sc->connection_lock);
	nvlist_destroy(nvl);
	return (error);
}

static void
nvmf_shutdown_pre_sync(void *arg, int howto)
{
	struct nvmf_softc *sc = arg;

	if ((howto & RB_NOSYNC) != 0 || SCHEDULER_STOPPED())
		return;

	/*
	 * If this association is disconnected, abort any pending
	 * requests with an error to permit filesystems to unmount
	 * without hanging.
	 */
	sx_xlock(&sc->connection_lock);
	if (sc->admin != NULL || sc->detaching) {
		sx_xunlock(&sc->connection_lock);
		return;
	}

	for (u_int i = 0; i < sc->cdata->nn; i++) {
		if (sc->ns[i] != NULL)
			nvmf_shutdown_ns(sc->ns[i]);
	}
	nvmf_shutdown_sim(sc);
	sx_xunlock(&sc->connection_lock);
}

static void
nvmf_shutdown_post_sync(void *arg, int howto)
{
	struct nvmf_softc *sc = arg;

	if ((howto & RB_NOSYNC) != 0 || SCHEDULER_STOPPED())
		return;

	/*
	 * If this association is connected, disconnect gracefully.
	 */
	sx_xlock(&sc->connection_lock);
	if (sc->admin == NULL || sc->detaching) {
		sx_xunlock(&sc->connection_lock);
		return;
	}

	callout_drain(&sc->ka_tx_timer);
	callout_drain(&sc->ka_rx_timer);

	nvmf_shutdown_controller(sc);

	/*
	 * Quiesce consumers so that any commands submitted after this
	 * fail with an error.  Notably, nda(4) calls nda_flush() from
	 * a post_sync handler that might be ordered after this one.
	 */
	for (u_int i = 0; i < sc->cdata->nn; i++) {
		if (sc->ns[i] != NULL)
			nvmf_shutdown_ns(sc->ns[i]);
	}
	nvmf_shutdown_sim(sc);

	for (u_int i = 0; i < sc->num_io_queues; i++) {
		nvmf_destroy_qp(sc->io[i]);
	}
	nvmf_destroy_qp(sc->admin);
	sc->admin = NULL;
	sx_xunlock(&sc->connection_lock);
}

static int
nvmf_detach(device_t dev)
{
	struct nvmf_softc *sc = device_get_softc(dev);
	u_int i;

	destroy_dev(sc->cdev);

	sx_xlock(&sc->connection_lock);
	sc->detaching = true;
	sx_xunlock(&sc->connection_lock);

	EVENTHANDLER_DEREGISTER(shutdown_pre_sync, sc->shutdown_pre_sync_eh);
	EVENTHANDLER_DEREGISTER(shutdown_post_sync, sc->shutdown_post_sync_eh);

	nvmf_destroy_sim(sc);
	for (i = 0; i < sc->cdata->nn; i++) {
		if (sc->ns[i] != NULL)
			nvmf_destroy_ns(sc->ns[i]);
	}
	free(sc->ns, M_NVMF);

	callout_drain(&sc->ka_tx_timer);
	callout_drain(&sc->ka_rx_timer);

	if (sc->admin != NULL)
		nvmf_shutdown_controller(sc);

	for (i = 0; i < sc->num_io_queues; i++) {
		nvmf_destroy_qp(sc->io[i]);
	}
	free(sc->io, M_NVMF);

	taskqueue_drain(nvmf_tq, &sc->disconnect_task);
	if (taskqueue_cancel_timeout(nvmf_tq, &sc->request_reconnect_task,
	    NULL) != 0)
		taskqueue_drain_timeout(nvmf_tq, &sc->request_reconnect_task);

	/*
	 * Don't cancel/drain the controller loss task if that task
	 * has fired and is triggering the detach.
	 */
	if (!sc->controller_timedout) {
		if (taskqueue_cancel_timeout(nvmf_tq, &sc->controller_loss_task,
		    NULL) != 0)
			taskqueue_drain_timeout(nvmf_tq,
			    &sc->controller_loss_task);
	}

	if (sc->admin != NULL)
		nvmf_destroy_qp(sc->admin);

	nvmf_destroy_aer(sc);

	sx_destroy(&sc->connection_lock);
	nvlist_destroy(sc->rparams);
	free(sc->cdata, M_NVMF);
	return (0);
}

static void
nvmf_rescan_ns_1(struct nvmf_softc *sc, uint32_t nsid,
    const struct nvme_namespace_data *data)
{
	struct nvmf_namespace *ns;

	/* XXX: Needs locking around sc->ns[]. */
	ns = sc->ns[nsid - 1];
	if (data->nsze == 0) {
		/* XXX: Needs locking */
		if (ns != NULL) {
			nvmf_destroy_ns(ns);
			sc->ns[nsid - 1] = NULL;
		}
	} else {
		/* XXX: Needs locking */
		if (ns == NULL) {
			sc->ns[nsid - 1] = nvmf_init_ns(sc, nsid, data);
		} else {
			if (!nvmf_update_ns(ns, data)) {
				nvmf_destroy_ns(ns);
				sc->ns[nsid - 1] = NULL;
			}
		}
	}

	nvmf_sim_rescan_ns(sc, nsid);
}

void
nvmf_rescan_ns(struct nvmf_softc *sc, uint32_t nsid)
{
	struct nvmf_completion_status status;
	struct nvme_namespace_data *data;

	data = malloc(sizeof(*data), M_NVMF, M_WAITOK);

	nvmf_status_init(&status);
	nvmf_status_wait_io(&status);
	if (!nvmf_cmd_identify_namespace(sc, nsid, data, nvmf_complete,
	    &status, nvmf_io_complete, &status, M_WAITOK)) {
		device_printf(sc->dev,
		    "failed to send IDENTIFY namespace %u command\n", nsid);
		free(data, M_NVMF);
		return;
	}
	nvmf_wait_for_reply(&status);

	if (status.cqe.status != 0) {
		device_printf(sc->dev,
		    "IDENTIFY namespace %u failed, status %#x\n", nsid,
		    le16toh(status.cqe.status));
		free(data, M_NVMF);
		return;
	}

	if (status.io_error != 0) {
		device_printf(sc->dev,
		    "IDENTIFY namespace %u failed with I/O error %d\n",
		    nsid, status.io_error);
		free(data, M_NVMF);
		return;
	}

	nvme_namespace_data_swapbytes(data);

	nvmf_rescan_ns_1(sc, nsid, data);

	free(data, M_NVMF);
}

static void
nvmf_purge_namespaces(struct nvmf_softc *sc, uint32_t first_nsid,
    uint32_t next_valid_nsid)
{
	struct nvmf_namespace *ns;

	for (uint32_t nsid = first_nsid; nsid < next_valid_nsid; nsid++)
	{
		/* XXX: Needs locking around sc->ns[]. */
		ns = sc->ns[nsid - 1];
		if (ns != NULL) {
			nvmf_destroy_ns(ns);
			sc->ns[nsid - 1] = NULL;

			nvmf_sim_rescan_ns(sc, nsid);
		}
	}
}

static bool
nvmf_rescan_ns_cb(struct nvmf_softc *sc, uint32_t nsid,
    const struct nvme_namespace_data *data, void *arg)
{
	uint32_t *last_nsid = arg;

	/* Check for any gaps prior to this namespace. */
	nvmf_purge_namespaces(sc, *last_nsid + 1, nsid);
	*last_nsid = nsid;

	nvmf_rescan_ns_1(sc, nsid, data);
	return (true);
}

void
nvmf_rescan_all_ns(struct nvmf_softc *sc)
{
	uint32_t last_nsid;

	last_nsid = 0;
	if (!nvmf_scan_active_namespaces(sc, nvmf_rescan_ns_cb, &last_nsid))
		return;

	/*
	 * Check for any namespace devices after the last active
	 * namespace.
	 */
	nvmf_purge_namespaces(sc, last_nsid + 1, sc->cdata->nn + 1);
}

int
nvmf_passthrough_cmd(struct nvmf_softc *sc, struct nvme_pt_command *pt,
    bool admin)
{
	struct nvmf_completion_status status;
	struct nvme_command cmd;
	struct memdesc mem;
	struct nvmf_host_qpair *qp;
	struct nvmf_request *req;
	void *buf;
	int error;

	if (pt->len > sc->max_xfer_size)
		return (EINVAL);

	buf = NULL;
	if (pt->len != 0) {
		/*
		 * XXX: Depending on the size we may want to pin the
		 * user pages and use a memdesc with vm_page_t's
		 * instead.
		 */
		buf = malloc(pt->len, M_NVMF, M_WAITOK);
		if (pt->is_read == 0) {
			error = copyin(pt->buf, buf, pt->len);
			if (error != 0) {
				free(buf, M_NVMF);
				return (error);
			}
		} else {
			/* Ensure no kernel data is leaked to userland. */
			memset(buf, 0, pt->len);
		}
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.opc = pt->cmd.opc;
	cmd.fuse = pt->cmd.fuse;
	cmd.nsid = pt->cmd.nsid;
	cmd.cdw10 = pt->cmd.cdw10;
	cmd.cdw11 = pt->cmd.cdw11;
	cmd.cdw12 = pt->cmd.cdw12;
	cmd.cdw13 = pt->cmd.cdw13;
	cmd.cdw14 = pt->cmd.cdw14;
	cmd.cdw15 = pt->cmd.cdw15;

	sx_slock(&sc->connection_lock);
	if (sc->admin == NULL || sc->detaching) {
		device_printf(sc->dev,
		    "failed to send passthrough command\n");
		error = ECONNABORTED;
		sx_sunlock(&sc->connection_lock);
		goto error;
	}
	if (admin)
		qp = sc->admin;
	else
		qp = nvmf_select_io_queue(sc);
	nvmf_status_init(&status);
	req = nvmf_allocate_request(qp, &cmd, nvmf_complete, &status, M_WAITOK);
	sx_sunlock(&sc->connection_lock);
	if (req == NULL) {
		device_printf(sc->dev, "failed to send passthrough command\n");
		error = ECONNABORTED;
		goto error;
	}

	if (pt->len != 0) {
		mem = memdesc_vaddr(buf, pt->len);
		nvmf_capsule_append_data(req->nc, &mem, pt->len,
		    pt->is_read == 0, nvmf_io_complete, &status);
		nvmf_status_wait_io(&status);
	}

	nvmf_submit_request(req);
	nvmf_wait_for_reply(&status);

	memset(&pt->cpl, 0, sizeof(pt->cpl));
	pt->cpl.cdw0 = status.cqe.cdw0;
	pt->cpl.status = status.cqe.status;

	error = status.io_error;
	if (error == 0 && pt->len != 0 && pt->is_read != 0)
		error = copyout(buf, pt->buf, pt->len);
error:
	free(buf, M_NVMF);
	return (error);
}

static int
nvmf_reconnect_params(struct nvmf_softc *sc, struct nvmf_ioc_nv *nv)
{
	int error;

	sx_slock(&sc->connection_lock);
	error = nvmf_pack_ioc_nvlist(sc->rparams, nv);
	sx_sunlock(&sc->connection_lock);

	return (error);
}

static int
nvmf_connection_status(struct nvmf_softc *sc, struct nvmf_ioc_nv *nv)
{
	nvlist_t *nvl, *nvl_ts;
	int error;

	nvl = nvlist_create(0);
	nvl_ts = nvlist_create(0);

	sx_slock(&sc->connection_lock);
	nvlist_add_bool(nvl, "connected", sc->admin != NULL);
	nvlist_add_number(nvl_ts, "tv_sec", sc->last_disconnect.tv_sec);
	nvlist_add_number(nvl_ts, "tv_nsec", sc->last_disconnect.tv_nsec);
	sx_sunlock(&sc->connection_lock);
	nvlist_move_nvlist(nvl, "last_disconnect", nvl_ts);

	error = nvmf_pack_ioc_nvlist(nvl, nv);
	nvlist_destroy(nvl);
	return (error);
}

static int
nvmf_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int flag,
    struct thread *td)
{
	struct nvmf_softc *sc = cdev->si_drv1;
	struct nvme_get_nsid *gnsid;
	struct nvme_pt_command *pt;
	struct nvmf_ioc_nv *nv;

	switch (cmd) {
	case NVME_PASSTHROUGH_CMD:
		pt = (struct nvme_pt_command *)arg;
		return (nvmf_passthrough_cmd(sc, pt, true));
	case NVME_GET_NSID:
		gnsid = (struct nvme_get_nsid *)arg;
		strlcpy(gnsid->cdev, device_get_nameunit(sc->dev),
		    sizeof(gnsid->cdev));
		gnsid->nsid = 0;
		return (0);
	case NVME_GET_MAX_XFER_SIZE:
		*(uint64_t *)arg = sc->max_xfer_size;
		return (0);
	case NVME_GET_CONTROLLER_DATA:
		memcpy(arg, sc->cdata, sizeof(*sc->cdata));
		return (0);
	case NVMF_RECONNECT_PARAMS:
		nv = (struct nvmf_ioc_nv *)arg;
		return (nvmf_reconnect_params(sc, nv));
	case NVMF_RECONNECT_HOST:
		nv = (struct nvmf_ioc_nv *)arg;
		return (nvmf_reconnect_host(sc, nv));
	case NVMF_CONNECTION_STATUS:
		nv = (struct nvmf_ioc_nv *)arg;
		return (nvmf_connection_status(sc, nv));
	default:
		return (ENOTTY);
	}
}

static struct cdevsw nvmf_cdevsw = {
	.d_version = D_VERSION,
	.d_ioctl = nvmf_ioctl
};

static int
nvmf_modevent(module_t mod, int what, void *arg)
{
	int error;

	switch (what) {
	case MOD_LOAD:
		error = nvmf_ctl_load();
		if (error != 0)
			return (error);

		nvmf_tq = taskqueue_create("nvmf", M_WAITOK | M_ZERO,
		    taskqueue_thread_enqueue, &nvmf_tq);
		taskqueue_start_threads(&nvmf_tq, 1, PWAIT, "nvmf taskq");
		return (0);
	case MOD_QUIESCE:
		return (0);
	case MOD_UNLOAD:
		nvmf_ctl_unload();
		destroy_dev_drain(&nvmf_cdevsw);
		if (nvmf_tq != NULL)
			taskqueue_free(nvmf_tq);
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

static device_method_t nvmf_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     nvmf_probe),
	DEVMETHOD(device_attach,    nvmf_attach),
	DEVMETHOD(device_detach,    nvmf_detach),
	DEVMETHOD_END
};

driver_t nvme_nvmf_driver = {
	"nvme",
	nvmf_methods,
	sizeof(struct nvmf_softc),
};

DRIVER_MODULE(nvme, root, nvme_nvmf_driver, nvmf_modevent, NULL);
MODULE_DEPEND(nvmf, nvmf_transport, 1, 1, 1);
