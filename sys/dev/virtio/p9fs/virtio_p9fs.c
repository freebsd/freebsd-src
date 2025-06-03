/*-
 * Copyright (c) 2017 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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
 *
 */
/*
 * The Virtio 9P transport driver. This file contains all functions related to
 * the virtqueue infrastructure which include creating the virtqueue, host
 * interactions, interrupts etc.
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/sglist.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/kthread.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <fs/p9fs/p9_client.h>
#include <fs/p9fs/p9_debug.h>
#include <fs/p9fs/p9_protocol.h>
#include <fs/p9fs/p9_transport.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/virtio_ring.h>
#include <dev/virtio/p9fs/virtio_p9fs.h>

#define VT9P_MTX(_sc) (&(_sc)->vt9p_mtx)
#define VT9P_LOCK(_sc) mtx_lock(VT9P_MTX(_sc))
#define VT9P_UNLOCK(_sc) mtx_unlock(VT9P_MTX(_sc))
#define VT9P_LOCK_INIT(_sc) mtx_init(VT9P_MTX(_sc), \
    "VIRTIO 9P CHAN lock", NULL, MTX_DEF)
#define VT9P_LOCK_DESTROY(_sc) mtx_destroy(VT9P_MTX(_sc))
#define MAX_SUPPORTED_SGS 20
static MALLOC_DEFINE(M_P9FS_MNTTAG, "p9fs_mount_tag", "P9fs Mounttag");

struct vt9p_softc {
	device_t vt9p_dev;
	struct mtx vt9p_mtx;
	struct sglist *vt9p_sglist;
	struct cv submit_cv;
	bool busy;
	struct virtqueue *vt9p_vq;
	int max_nsegs;
	uint16_t mount_tag_len;
	char *mount_tag;
	STAILQ_ENTRY(vt9p_softc) chan_next;
};

/* Global channel list, Each channel will correspond to a mount point */
static STAILQ_HEAD( ,vt9p_softc) global_chan_list =
    STAILQ_HEAD_INITIALIZER(global_chan_list);
struct mtx global_chan_list_mtx;
MTX_SYSINIT(global_chan_list_mtx, &global_chan_list_mtx, "9pglobal", MTX_DEF);

static struct virtio_feature_desc virtio_9p_feature_desc[] = {
	{ VIRTIO_9PNET_F_MOUNT_TAG,	"9PMountTag" },
	{ 0, NULL }
};

/* We don't currently allow canceling of virtio requests */
static int
vt9p_cancel(void *handle, struct p9_req_t *req)
{
	return (1);
}

SYSCTL_NODE(_vfs, OID_AUTO, 9p, CTLFLAG_RW, 0, "9P File System Protocol");

/*
 * Maximum number of seconds vt9p_request thread sleep waiting for an
 * ack from the host, before exiting
 */
static unsigned int vt9p_ackmaxidle = 120;
SYSCTL_UINT(_vfs_9p, OID_AUTO, ackmaxidle, CTLFLAG_RW, &vt9p_ackmaxidle, 0,
    "Maximum time request thread waits for ack from host");

/*
 * Wait for completion of a p9 request.
 *
 * This routine will sleep and release the chan mtx during the period.
 * chan mtx will be acquired again upon return.
 */
static int
vt9p_req_wait(struct vt9p_softc *chan, struct p9_req_t *req)
{
	KASSERT(req->tc->tag != req->rc->tag,
	    ("%s: request %p already completed", __func__, req));

	if (msleep(req, VT9P_MTX(chan), 0, "chan lock", vt9p_ackmaxidle * hz)) {
		/*
		 * Waited for 120s. No response from host.
		 * Can't wait for ever..
		 */
		P9_DEBUG(ERROR, "Timeout after waiting %u seconds"
		    "for an ack from host\n", vt9p_ackmaxidle);
		return (EIO);
	}
	KASSERT(req->tc->tag == req->rc->tag,
	    ("%s spurious event on request %p", __func__, req));
	return (0);
}

/*
 * Request handler. This is called for every request submitted to the host
 * It basically maps the tc/rc buffers to sg lists and submits the requests
 * into the virtqueue. Since we have implemented a synchronous version, the
 * submission thread sleeps until the ack in the interrupt wakes it up. Once
 * it wakes up, it returns back to the P9fs layer. The rc buffer is then
 * processed and completed to its upper layers.
 */
static int
vt9p_request(void *handle, struct p9_req_t *req)
{
	int error;
	struct vt9p_softc *chan;
	int readable, writable;
	struct sglist *sg;
	struct virtqueue *vq;

	chan = handle;
	sg = chan->vt9p_sglist;
	vq = chan->vt9p_vq;

	P9_DEBUG(TRANS, "%s: req=%p\n", __func__, req);

	/* Grab the channel lock*/
	VT9P_LOCK(chan);
req_retry:
	sglist_reset(sg);
	/* Handle out VirtIO ring buffers */
	error = sglist_append(sg, req->tc->sdata, req->tc->size);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: sglist append failed\n", __func__);
		VT9P_UNLOCK(chan);
		return (error);
	}
	readable = sg->sg_nseg;

	error = sglist_append(sg, req->rc->sdata, req->rc->capacity);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: sglist append failed\n", __func__);
		VT9P_UNLOCK(chan);
		return (error);
	}
	writable = sg->sg_nseg - readable;

	error = virtqueue_enqueue(vq, req, sg, readable, writable);
	if (error != 0) {
		if (error == ENOSPC) {
			/*
			 * Condvar for the submit queue. Unlock the chan
			 * since wakeup needs one.
			 */
			cv_wait(&chan->submit_cv, VT9P_MTX(chan));
			P9_DEBUG(TRANS, "%s: retry virtio request\n", __func__);
			goto req_retry;
		} else {
			P9_DEBUG(ERROR, "%s: virtio enuqueue failed \n", __func__);
			VT9P_UNLOCK(chan);
			return (EIO);
		}
	}

	/* We have to notify */
	virtqueue_notify(vq);

	error = vt9p_req_wait(chan, req);
	if (error != 0) {
		VT9P_UNLOCK(chan);
		return (error);
	}

	VT9P_UNLOCK(chan);

	P9_DEBUG(TRANS, "%s: virtio request kicked\n", __func__);

	return (0);
}

/*
 * Completion of the request from the virtqueue. This interrupt handler is
 * setup at initialization and is called for every completing request. It
 * just wakes up the sleeping submission requests.
 */
static void
vt9p_intr_complete(void *xsc)
{
	struct vt9p_softc *chan;
	struct virtqueue *vq;
	struct p9_req_t *curreq;

	chan = (struct vt9p_softc *)xsc;
	vq = chan->vt9p_vq;

	P9_DEBUG(TRANS, "%s: completing\n", __func__);

	VT9P_LOCK(chan);
again:
	while ((curreq = virtqueue_dequeue(vq, NULL)) != NULL) {
		curreq->rc->tag = curreq->tc->tag;
		wakeup_one(curreq);
	}
	if (virtqueue_enable_intr(vq) != 0) {
		virtqueue_disable_intr(vq);
		goto again;
	}
	cv_signal(&chan->submit_cv);
	VT9P_UNLOCK(chan);
}

/*
 * Allocation of the virtqueue with interrupt complete routines.
 */
static int
vt9p_alloc_virtqueue(struct vt9p_softc *sc)
{
	struct vq_alloc_info vq_info;
	device_t dev;

	dev = sc->vt9p_dev;

	VQ_ALLOC_INFO_INIT(&vq_info, sc->max_nsegs,
	    vt9p_intr_complete, sc, &sc->vt9p_vq,
	    "%s request", device_get_nameunit(dev));

	return (virtio_alloc_virtqueues(dev, 1, &vq_info));
}

/* Probe for existence of 9P virtio channels */
static int
vt9p_probe(device_t dev)
{

	/* If the virtio device type is a 9P device, then we claim and attach it */
	if (virtio_get_device_type(dev) != VIRTIO_ID_9P)
		return (ENXIO);
	device_set_desc(dev, "VirtIO 9P Transport");

	return (BUS_PROBE_DEFAULT);
}

static void
vt9p_stop(struct vt9p_softc *sc)
{

	/* Device specific stops .*/
	virtqueue_disable_intr(sc->vt9p_vq);
	virtio_stop(sc->vt9p_dev);
}

/* Detach the 9P virtio PCI device */
static int
vt9p_detach(device_t dev)
{
	struct vt9p_softc *sc;

	sc = device_get_softc(dev);
	VT9P_LOCK(sc);
	vt9p_stop(sc);
	VT9P_UNLOCK(sc);

	if (sc->vt9p_sglist) {
		sglist_free(sc->vt9p_sglist);
		sc->vt9p_sglist = NULL;
	}
	if (sc->mount_tag) {
		free(sc->mount_tag, M_P9FS_MNTTAG);
		sc->mount_tag = NULL;
	}
	mtx_lock(&global_chan_list_mtx);
	STAILQ_REMOVE(&global_chan_list, sc, vt9p_softc, chan_next);
	mtx_unlock(&global_chan_list_mtx);

	VT9P_LOCK_DESTROY(sc);
	cv_destroy(&sc->submit_cv);

	return (0);
}

/* Attach the 9P virtio PCI device */
static int
vt9p_attach(device_t dev)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct vt9p_softc *chan;
	char *mount_tag;
	int error;
	uint16_t mount_tag_len;

	chan = device_get_softc(dev);
	chan->vt9p_dev = dev;

	/* Init the channel lock. */
	VT9P_LOCK_INIT(chan);
	/* Initialize the condition variable */
	cv_init(&chan->submit_cv, "Conditional variable for submit queue" );
	chan->max_nsegs = MAX_SUPPORTED_SGS;
	chan->vt9p_sglist = sglist_alloc(chan->max_nsegs, M_WAITOK);

	/* Negotiate the features from the host */
	virtio_set_feature_desc(dev, virtio_9p_feature_desc);
	virtio_negotiate_features(dev, VIRTIO_9PNET_F_MOUNT_TAG);

	/*
	 * If mount tag feature is supported read the mount tag
	 * from device config
	 */
	if (virtio_with_feature(dev, VIRTIO_9PNET_F_MOUNT_TAG))
		mount_tag_len = virtio_read_dev_config_2(dev,
		    offsetof(struct virtio_9pnet_config, mount_tag_len));
	else {
		error = EINVAL;
		P9_DEBUG(ERROR, "%s: Mount tag feature not supported by host\n", __func__);
		goto out;
	}
	mount_tag = malloc(mount_tag_len + 1, M_P9FS_MNTTAG,
	    M_WAITOK | M_ZERO);

	virtio_read_device_config_array(dev,
	    offsetof(struct virtio_9pnet_config, mount_tag),
	    mount_tag, 1, mount_tag_len);

	device_printf(dev, "Mount tag: %s\n", mount_tag);

	mount_tag_len++;
	chan->mount_tag_len = mount_tag_len;
	chan->mount_tag = mount_tag;

	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	SYSCTL_ADD_STRING(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "p9fs_mount_tag",
	    CTLFLAG_RD, chan->mount_tag, 0, "Mount tag");

	/* We expect one virtqueue, for requests. */
	error = vt9p_alloc_virtqueue(chan);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: Allocating the virtqueue failed \n", __func__);
		goto out;
	}
	error = virtio_setup_intr(dev, INTR_TYPE_MISC|INTR_MPSAFE);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: Cannot setup virtqueue interrupt\n", __func__);
		goto out;
	}
	error = virtqueue_enable_intr(chan->vt9p_vq);
	if (error != 0) {
		P9_DEBUG(ERROR, "%s: Cannot enable virtqueue interrupt\n", __func__);
		goto out;
	}

	mtx_lock(&global_chan_list_mtx);
	/* Insert the channel in global channel list */
	STAILQ_INSERT_HEAD(&global_chan_list, chan, chan_next);
	mtx_unlock(&global_chan_list_mtx);

	return (0);
out:
	/* Something went wrong, detach the device */
	vt9p_detach(dev);
	return (error);
}

/*
 * Allocate a new virtio channel. This sets up a transport channel
 * for 9P communication
 */
static int
vt9p_create(const char *mount_tag, void **handlep)
{
	struct vt9p_softc *sc, *chan;

	chan = NULL;

	/*
	 * Find out the corresponding channel for a client from global list
	 * of channels based on mount tag and attach it to client
	 */
	mtx_lock(&global_chan_list_mtx);
	STAILQ_FOREACH(sc, &global_chan_list, chan_next) {
		if (!strcmp(sc->mount_tag, mount_tag)) {
			chan = sc;
			break;
		}
	}
	mtx_unlock(&global_chan_list_mtx);

	/*
	 * If chan is already attached to a client then it cannot be used for
	 * another client.
	 */
	if (chan && chan->busy) {
		//p9_debug(TRANS, "Channel busy: used by clnt=%p\n", chan->client);
		return (EBUSY);
	}

	/* If we dont have one, for now bail out.*/
	if (chan) {
		*handlep = (void *)chan;
		chan->busy = true;
	} else {
		P9_DEBUG(TRANS, "%s: No Global channel with mount_tag=%s\n",
		    __func__, mount_tag);
		return (EINVAL);
	}

	return (0);
}

static void
vt9p_close(void *handle)
{
	struct vt9p_softc *chan = handle;

	chan->busy = false;
}

static struct p9_trans_module vt9p_trans = {
	.name = "virtio",
	.create = vt9p_create,
	.close = vt9p_close,
	.request = vt9p_request,
	.cancel = vt9p_cancel,
};

static device_method_t vt9p_mthds[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,	 vt9p_probe),
	DEVMETHOD(device_attach, vt9p_attach),
	DEVMETHOD(device_detach, vt9p_detach),
	DEVMETHOD_END
};

static driver_t vt9p_drv = {
	"virtio_p9fs",
	vt9p_mthds,
	sizeof(struct vt9p_softc)
};

static int
vt9p_modevent(module_t mod, int type, void *unused)
{
	int error;

	error = 0;

	switch (type) {
	case MOD_LOAD:
		p9_init_zones();
		p9_register_trans(&vt9p_trans);
		break;
	case MOD_UNLOAD:
		p9_destroy_zones();
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

VIRTIO_DRIVER_MODULE(virtio_p9fs, vt9p_drv, vt9p_modevent, NULL);
MODULE_VERSION(virtio_p9fs, 1);
MODULE_DEPEND(virtio_p9fs, virtio, 1, 1, 1);
MODULE_DEPEND(virtio_p9fs, p9fs, 1, 1, 1);
