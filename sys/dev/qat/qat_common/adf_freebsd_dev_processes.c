/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

#include "qat_freebsd.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"
#include "adf_accel_devices.h"
#include "icp_qat_uclo.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_init_admin.h"
#include "adf_cfg_strings.h"
#include "adf_uio_control.h"
#include "adf_uio_cleanup.h"
#include "adf_uio.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"

#define ADF_DEV_PROCESSES_NAME "qat_dev_processes"
#define ADF_DEV_STATE_NAME "qat_dev_state"

#define ADF_STATE_CALLOUT_TIME 10

static const char *mtx_name = "state_callout_mtx";

static d_open_t adf_processes_open;
static void adf_processes_release(void *data);
static d_read_t adf_processes_read;
static d_write_t adf_processes_write;

static d_open_t adf_state_open;
static void adf_state_release(void *data);
static d_read_t adf_state_read;
static int adf_state_kqfilter(struct cdev *dev, struct knote *kn);
static int adf_state_kqread_event(struct knote *kn, long hint);
static void adf_state_kqread_detach(struct knote *kn);

static struct callout callout;
static struct mtx mtx;
static struct service_hndl adf_state_hndl;

struct entry_proc_events {
	struct adf_state_priv_data *proc_events;

	SLIST_ENTRY(entry_proc_events) entries_proc_events;
};

struct entry_state {
	struct adf_state state;

	STAILQ_ENTRY(entry_state) entries_state;
};

SLIST_HEAD(proc_events_head, entry_proc_events);
STAILQ_HEAD(state_head, entry_state);

static struct proc_events_head proc_events_head;

struct adf_processes_priv_data {
	char name[ADF_CFG_MAX_SECTION_LEN_IN_BYTES];
	int read_flag;
	struct list_head list;
};

struct adf_state_priv_data {
	struct cdev *cdev;
	struct selinfo rsel;
	struct state_head state_head;
};

static struct cdevsw adf_processes_cdevsw = {
	.d_version = D_VERSION,
	.d_open = adf_processes_open,
	.d_read = adf_processes_read,
	.d_write = adf_processes_write,
	.d_name = ADF_DEV_PROCESSES_NAME,
};

static struct cdevsw adf_state_cdevsw = {
	.d_version = D_VERSION,
	.d_open = adf_state_open,
	.d_read = adf_state_read,
	.d_kqfilter = adf_state_kqfilter,
	.d_name = ADF_DEV_STATE_NAME,
};

static struct filterops adf_state_read_filterops = {
	.f_isfd = 1,
	.f_attach = NULL,
	.f_detach = adf_state_kqread_detach,
	.f_event = adf_state_kqread_event,
};

static struct cdev *adf_processes_dev;
static struct cdev *adf_state_dev;

static LINUX_LIST_HEAD(processes_list);

struct sx processes_list_sema;
SX_SYSINIT(processes_list_sema, &processes_list_sema, "adf proc list");

static void
adf_chr_drv_destroy(void)
{
	destroy_dev(adf_processes_dev);
}

static int
adf_chr_drv_create(void)
{

	adf_processes_dev = make_dev(&adf_processes_cdevsw,
				     0,
				     UID_ROOT,
				     GID_WHEEL,
				     0600,
				     ADF_DEV_PROCESSES_NAME);
	if (adf_processes_dev == NULL) {
		printf("QAT: failed to create device\n");
		goto err_cdev_del;
	}
	return 0;
err_cdev_del:
	return EFAULT;
}

static int
adf_processes_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	int i = 0, devices = 0;
	struct adf_accel_dev *accel_dev = NULL;
	struct adf_processes_priv_data *prv_data = NULL;
	int error = 0;

	for (i = 0; i < ADF_MAX_DEVICES; i++) {
		accel_dev = adf_devmgr_get_dev_by_id(i);
		if (!accel_dev)
			continue;
		if (!adf_dev_started(accel_dev))
			continue;
		devices++;
	}
	if (!devices) {
		printf("QAT: No active devices found.\n");
		return ENXIO;
	}
	prv_data = malloc(sizeof(*prv_data), M_QAT, M_WAITOK | M_ZERO);
	if (!prv_data)
		return ENOMEM;
	INIT_LIST_HEAD(&prv_data->list);
	error = devfs_set_cdevpriv(prv_data, adf_processes_release);
	if (error) {
		free(prv_data, M_QAT);
		return error;
	}

	return 0;
}

static int
adf_get_first_started_dev(void)
{
	int i = 0;
	struct adf_accel_dev *accel_dev = NULL;

	for (i = 0; i < ADF_MAX_DEVICES; i++) {
		accel_dev = adf_devmgr_get_dev_by_id(i);
		if (!accel_dev)
			continue;
		if (adf_dev_started(accel_dev))
			return i;
	}

	return -1;
}

static int
adf_processes_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct adf_processes_priv_data *prv_data = NULL;
	struct adf_processes_priv_data *pdata = NULL;
	int dev_num = 0, pr_num = 0;
	struct list_head *lpos = NULL;
	char usr_name[ADF_CFG_MAX_SECTION_LEN_IN_BYTES] = { 0 };
	struct adf_accel_dev *accel_dev = NULL;
	struct adf_cfg_section *section_ptr = NULL;
	bool pr_name_available = 1;
	uint32_t num_accel_devs = 0;
	int error = 0;
	ssize_t count;
	int dev_id;

	error = devfs_get_cdevpriv((void **)&prv_data);
	if (error) {
		printf("QAT: invalid file descriptor\n");
		return error;
	}

	if (prv_data->read_flag == 1) {
		printf("QAT: can only write once\n");
		return EBADF;
	}
	count = uio->uio_resid;
	if ((count <= 0) || (count > ADF_CFG_MAX_SECTION_LEN_IN_BYTES)) {
		printf("QAT: wrong size %d\n", (int)count);
		return EIO;
	}

	error = uiomove(usr_name, count, uio);
	if (error) {
		printf("QAT: can't copy data\n");
		return error;
	}

	/* Lock other processes and try to find out the process name */
	if (sx_xlock_sig(&processes_list_sema)) {
		printf("QAT: can't aquire process info lock\n");
		return EBADF;
	}

	dev_id = adf_get_first_started_dev();
	if (-1 == dev_id) {
		pr_err("QAT: could not find started device\n");
		sx_xunlock(&processes_list_sema);
		return -EIO;
	}

	accel_dev = adf_devmgr_get_dev_by_id(dev_id);
	if (!accel_dev) {
		pr_err("QAT: could not find started device\n");
		sx_xunlock(&processes_list_sema);
		return -EIO;
	}

	/* If there is nothing there then take the first name and return */
	if (list_empty(&processes_list)) {
		snprintf(prv_data->name,
			 ADF_CFG_MAX_SECTION_LEN_IN_BYTES,
			 "%s" ADF_INTERNAL_USERSPACE_SEC_SUFF "%d",
			 usr_name,
			 0);
		list_add(&prv_data->list, &processes_list);
		sx_xunlock(&processes_list_sema);
		prv_data->read_flag = 1;
		return 0;
	}

	/* If there are processes running then search for a first free name */
	adf_devmgr_get_num_dev(&num_accel_devs);
	for (dev_num = 0; dev_num < num_accel_devs; dev_num++) {
		accel_dev = adf_devmgr_get_dev_by_id(dev_num);
		if (!accel_dev)
			continue;

		if (!adf_dev_started(accel_dev))
			continue; /* to next device */

		for (pr_num = 0; pr_num < GET_MAX_PROCESSES(accel_dev);
		     pr_num++) {
			snprintf(prv_data->name,
				 ADF_CFG_MAX_SECTION_LEN_IN_BYTES,
				 "%s" ADF_INTERNAL_USERSPACE_SEC_SUFF "%d",
				 usr_name,
				 pr_num);
			pr_name_available = 1;
			/* Figure out if section exists in the config table */
			section_ptr =
			    adf_cfg_sec_find(accel_dev, prv_data->name);
			if (NULL == section_ptr) {
				/* This section name doesn't exist */
				pr_name_available = 0;
				/* As process_num enumerates from 0, once we get
				 * to one which doesn't exist no further ones
				 * will exist. On to next device
				 */
				break;
			}
			/* Figure out if it's been taken already */
			list_for_each(lpos, &processes_list)
			{
				pdata =
				    list_entry(lpos,
					       struct adf_processes_priv_data,
					       list);
				if (!strncmp(
					pdata->name,
					prv_data->name,
					ADF_CFG_MAX_SECTION_LEN_IN_BYTES)) {
					pr_name_available = 0;
					break;
				}
			}
			if (pr_name_available)
				break;
		}
		if (pr_name_available)
			break;
	}
	/*
	 * If we have a valid name that is not on
	 * the list take it and add to the list
	 */
	if (pr_name_available) {
		list_add(&prv_data->list, &processes_list);
		sx_xunlock(&processes_list_sema);
		prv_data->read_flag = 1;
		return 0;
	}
	/* If not then the process needs to wait */
	sx_xunlock(&processes_list_sema);
	explicit_bzero(prv_data->name, ADF_CFG_MAX_SECTION_LEN_IN_BYTES);
	prv_data->read_flag = 0;
	return 1;
}

static int
adf_processes_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct adf_processes_priv_data *prv_data = NULL;
	int error = 0;

	error = devfs_get_cdevpriv((void **)&prv_data);
	if (error) {
		printf("QAT: invalid file descriptor\n");
		return error;
	}

	/*
	 * If there is a name that the process can use then give it
	 * to the proocess.
	 */
	if (prv_data->read_flag) {
		error = uiomove(prv_data->name,
				strnlen(prv_data->name,
					ADF_CFG_MAX_SECTION_LEN_IN_BYTES),
				uio);
		if (error) {
			printf("QAT: failed to copy data to user\n");
			return error;
		}
		return 0;
	}

	return EIO;
}

static void
adf_processes_release(void *data)
{
	struct adf_processes_priv_data *prv_data = NULL;

	prv_data = (struct adf_processes_priv_data *)data;
	sx_xlock(&processes_list_sema);
	list_del(&prv_data->list);
	sx_xunlock(&processes_list_sema);
	free(prv_data, M_QAT);
}

int
adf_processes_dev_register(void)
{
	return adf_chr_drv_create();
}

void
adf_processes_dev_unregister(void)
{
	adf_chr_drv_destroy();
}

static void
adf_state_callout_notify_ev(void *arg)
{
	int notified = 0;
	struct adf_state_priv_data *priv = NULL;
	struct entry_proc_events *proc_events = NULL;

	SLIST_FOREACH (proc_events, &proc_events_head, entries_proc_events) {
		if (!STAILQ_EMPTY(&proc_events->proc_events->state_head)) {
			notified = 1;
			priv = proc_events->proc_events;
			wakeup(priv);
			selwakeup(&priv->rsel);
			KNOTE_UNLOCKED(&priv->rsel.si_note, 0);
		}
	}
	if (notified)
		callout_schedule(&callout, ADF_STATE_CALLOUT_TIME);
}

static void
adf_state_set(int dev, enum adf_event event)
{
	struct adf_accel_dev *accel_dev = NULL;
	struct state_head *head = NULL;
	struct entry_proc_events *proc_events = NULL;
	struct entry_state *state = NULL;

	accel_dev = adf_devmgr_get_dev_by_id(dev);
	if (!accel_dev)
		return;
	mtx_lock(&mtx);
	SLIST_FOREACH (proc_events, &proc_events_head, entries_proc_events) {
		state = NULL;
		head = &proc_events->proc_events->state_head;
		state = malloc(sizeof(struct entry_state),
			       M_QAT,
			       M_NOWAIT | M_ZERO);
		if (!state)
			continue;
		state->state.dev_state = event;
		state->state.dev_id = dev;
		STAILQ_INSERT_TAIL(head, state, entries_state);
		if (event == ADF_EVENT_STOP) {
			state = NULL;
			state = malloc(sizeof(struct entry_state),
				       M_QAT,
				       M_NOWAIT | M_ZERO);
			if (!state)
				continue;
			state->state.dev_state = ADF_EVENT_SHUTDOWN;
			state->state.dev_id = dev;
			STAILQ_INSERT_TAIL(head, state, entries_state);
		}
	}
	callout_schedule(&callout, ADF_STATE_CALLOUT_TIME);
	mtx_unlock(&mtx);
}

static int
adf_state_event_handler(struct adf_accel_dev *accel_dev, enum adf_event event)
{
	int ret = 0;

#if defined(QAT_UIO) && defined(QAT_DBG)
	if (event > ADF_EVENT_DBG_SHUTDOWN)
		return -EINVAL;
#else
	if (event > ADF_EVENT_ERROR)
		return -EINVAL;
#endif /* defined(QAT_UIO) && defined(QAT_DBG) */

	switch (event) {
	case ADF_EVENT_INIT:
		return ret;
	case ADF_EVENT_SHUTDOWN:
		return ret;
	case ADF_EVENT_RESTARTING:
		break;
	case ADF_EVENT_RESTARTED:
		break;
	case ADF_EVENT_START:
		return ret;
	case ADF_EVENT_STOP:
		break;
	case ADF_EVENT_ERROR:
		break;
#if defined(QAT_UIO) && defined(QAT_DBG)
	case ADF_EVENT_PROC_CRASH:
		break;
	case ADF_EVENT_MANUAL_DUMP:
		break;
	case ADF_EVENT_SLICE_HANG:
		break;
	case ADF_EVENT_DBG_SHUTDOWN:
		break;
#endif /* defined(QAT_UIO) && defined(QAT_DBG) */
	default:
		return -1;
	}

	adf_state_set(accel_dev->accel_id, event);

	return 0;
}

static int
adf_state_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct adf_state_priv_data *priv;

	mtx_lock(&mtx);
	priv = dev->si_drv1;
	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &adf_state_read_filterops;
		kn->kn_hook = priv;
		knlist_add(&priv->rsel.si_note, kn, 0);
		mtx_unlock(&mtx);
		return 0;
	default:
		mtx_unlock(&mtx);
		return -EINVAL;
	}
}

static int
adf_state_kqread_event(struct knote *kn, long hint)
{
	return 1;
}

static void
adf_state_kqread_detach(struct knote *kn)
{
	struct adf_state_priv_data *priv = NULL;

	mtx_lock(&mtx);
	if (!kn) {
		mtx_unlock(&mtx);
		return;
	}
	priv = kn->kn_hook;
	if (!priv) {
		mtx_unlock(&mtx);
		return;
	}
	knlist_remove(&priv->rsel.si_note, kn, 1);
	mtx_unlock(&mtx);
}

void
adf_state_init(void)
{
	adf_state_dev = make_dev(&adf_state_cdevsw,
				 0,
				 UID_ROOT,
				 GID_WHEEL,
				 0600,
				 "%s",
				 ADF_DEV_STATE_NAME);
	SLIST_INIT(&proc_events_head);
	mtx_init(&mtx, mtx_name, NULL, MTX_DEF);
	callout_init_mtx(&callout, &mtx, 0);
	explicit_bzero(&adf_state_hndl, sizeof(adf_state_hndl));
	adf_state_hndl.event_hld = adf_state_event_handler;
	adf_state_hndl.name = "adf_state_event_handler";
	mtx_lock(&mtx);
	adf_service_register(&adf_state_hndl);
	callout_reset(&callout,
		      ADF_STATE_CALLOUT_TIME,
		      adf_state_callout_notify_ev,
		      NULL);
	mtx_unlock(&mtx);
}

void
adf_state_destroy(void)
{
	struct entry_proc_events *proc_events = NULL;

	mtx_lock(&mtx);
	adf_service_unregister(&adf_state_hndl);
	callout_stop(&callout);
	while (!SLIST_EMPTY(&proc_events_head)) {
		proc_events = SLIST_FIRST(&proc_events_head);
		SLIST_REMOVE_HEAD(&proc_events_head, entries_proc_events);
		free(proc_events, M_QAT);
	}
	destroy_dev(adf_state_dev);
	mtx_unlock(&mtx);
	mtx_destroy(&mtx);
}

static int
adf_state_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct adf_state_priv_data *prv_data = NULL;
	struct entry_proc_events *entry_proc_events = NULL;
	int ret = 0;

	prv_data = malloc(sizeof(*prv_data), M_QAT, M_WAITOK | M_ZERO);
	if (!prv_data)
		return -ENOMEM;
	entry_proc_events =
	    malloc(sizeof(struct entry_proc_events), M_QAT, M_WAITOK | M_ZERO);
	if (!entry_proc_events) {
		free(prv_data, M_QAT);
		return -ENOMEM;
	}
	mtx_lock(&mtx);
	prv_data->cdev = dev;
	prv_data->cdev->si_drv1 = prv_data;
	knlist_init_mtx(&prv_data->rsel.si_note, &mtx);
	STAILQ_INIT(&prv_data->state_head);
	entry_proc_events->proc_events = prv_data;
	SLIST_INSERT_HEAD(&proc_events_head,
			  entry_proc_events,
			  entries_proc_events);
	ret = devfs_set_cdevpriv(prv_data, adf_state_release);
	if (ret) {
		SLIST_REMOVE(&proc_events_head,
			     entry_proc_events,
			     entry_proc_events,
			     entries_proc_events);
		free(entry_proc_events, M_QAT);
		free(prv_data, M_QAT);
	}
	callout_schedule(&callout, ADF_STATE_CALLOUT_TIME);
	mtx_unlock(&mtx);
	return ret;
}

static int
adf_state_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	int ret = 0;
	struct adf_state_priv_data *prv_data = NULL;
	struct state_head *state_head = NULL;
	struct entry_state *entry_state = NULL;
	struct adf_state *state = NULL;
	struct entry_proc_events *proc_events = NULL;

	mtx_lock(&mtx);
	ret = devfs_get_cdevpriv((void **)&prv_data);
	if (ret) {
		mtx_unlock(&mtx);
		return 0;
	}
	state_head = &prv_data->state_head;
	if (STAILQ_EMPTY(state_head)) {
		mtx_unlock(&mtx);
		return 0;
	}
	entry_state = STAILQ_FIRST(state_head);
	state = &entry_state->state;
	ret = uiomove(state, sizeof(struct adf_state), uio);
	if (!ret && !STAILQ_EMPTY(state_head)) {
		STAILQ_REMOVE_HEAD(state_head, entries_state);
		free(entry_state, M_QAT);
	}
	SLIST_FOREACH (proc_events, &proc_events_head, entries_proc_events) {
		if (!STAILQ_EMPTY(&proc_events->proc_events->state_head)) {
			prv_data = proc_events->proc_events;
			wakeup(prv_data);
			selwakeup(&prv_data->rsel);
			KNOTE_UNLOCKED(&prv_data->rsel.si_note, 0);
		}
	}
	callout_schedule(&callout, ADF_STATE_CALLOUT_TIME);
	mtx_unlock(&mtx);
	return ret;
}

static void
adf_state_release(void *data)
{
	struct adf_state_priv_data *prv_data = NULL;
	struct entry_state *entry_state = NULL;
	struct entry_proc_events *entry_proc_events = NULL;
	struct entry_proc_events *tmp = NULL;

	mtx_lock(&mtx);
	prv_data = (struct adf_state_priv_data *)data;
	knlist_delete(&prv_data->rsel.si_note, curthread, 1);
	knlist_destroy(&prv_data->rsel.si_note);
	seldrain(&prv_data->rsel);
	while (!STAILQ_EMPTY(&prv_data->state_head)) {
		entry_state = STAILQ_FIRST(&prv_data->state_head);
		STAILQ_REMOVE_HEAD(&prv_data->state_head, entries_state);
		free(entry_state, M_QAT);
	}
	SLIST_FOREACH_SAFE (entry_proc_events,
			    &proc_events_head,
			    entries_proc_events,
			    tmp) {
		if (entry_proc_events->proc_events == prv_data) {
			SLIST_REMOVE(&proc_events_head,
				     entry_proc_events,
				     entry_proc_events,
				     entries_proc_events);
			free(entry_proc_events, M_QAT);
		}
	}
	free(prv_data, M_QAT);
	mtx_unlock(&mtx);
}
