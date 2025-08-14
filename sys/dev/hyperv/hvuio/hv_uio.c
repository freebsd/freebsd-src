/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013-2016 Brocade Communications Systems, Inc.
 * Copyright (c) 2016 Microsoft Corp.
 * Copyright (c) 2021 One Convergence, Inc.
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
 * hv_uio - generic UIO driver for VMBus Network Devices
 *
 * Bind/unbind hv_uio with VMBus network device using devctl.
 * For example:
 * devctl set driver -f hn1 hv_uio
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/smp.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/cdefs.h>
#include <sys/sysctl.h>
#include <sys/rwlock.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/selinfo.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/malloc.h>

#include <machine/atomic.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/hyperv_busdma.h>
#include <dev/hyperv/vmbus/vmbus_reg.h>
#include <dev/hyperv/vmbus/vmbus_chanvar.h>

#include "vmbus_if.h"

#define UH_DNAME	"hv_uio"
#define DRIVER_VERSION  1

#define VMBUS_VPREF	1
#define VMBUS_VMIN	VMBUS_VPREF
#define VMBUS_VMAX	VMBUS_VPREF

#define HV_RING_SIZE     512    /* pages */
#define SEND_BUFFER_SIZE (16 * 1024 * 1024)
#define RECV_BUFFER_SIZE (31 * 1024 * 1024)

#define UH_NAMESIZE	32

/* Macros to distinguish mmap request
 * [7-0] - Device memory region
 * [15-8]- Sub-channel id
 */
#define UH_MEM_MASK		0x00ff
#define UH_SUBCHAN_MASK_SHIFT	8

/* ioctl */
#define HVIOOPENSUBCHAN     _IOW('h', 14, uint32_t)

/* Maximum number of VMBUS resources. */
enum uio_hv_map {
	HV_TXRX_RING_MAP = 0,
	HV_INT_PAGE_MAP,
	HV_MON_PAGE_MAP,
	HV_RECV_BUF_MAP,
	HV_SEND_BUF_MAP,
	HV_MAX_RESOURCE
};

enum uh_cdev_st {
	UH_ST_CLOSE,
	UH_ST_OPEN,
};

/*
 * Network GUID
 */
static const struct hyperv_guid hn_guid = {
	.hv_guid = {
		0x63, 0x51, 0x61, 0xf8, 0x3e, 0xdf, 0xc5, 0x46,
		0x91, 0x3f, 0xf2, 0xd2, 0xf9, 0x65, 0xed, 0x0e }
};

struct uh_res {
	struct hyperv_dma *hvdma;
	void	*buf;
	uint32_t gpadl;
	uint64_t size;
	struct sysctl_oid *oid;
	char	name[UH_NAMESIZE];
};

struct uio_hv_softc {
	device_t		dev;
	struct cdev		*uh_cdev;

	int			cdev_st;
	volatile uint32_t	revent;	/* total read events */
	uint32_t		uevent;	/* events read by user */

	struct vmbus_channel	*chan;
	uint32_t		subchan_cnt;

	uint32_t		cbr_txsz;
	uint32_t		cbr_rxsz;

	struct uh_res		res[HV_MAX_RESOURCE];

	struct selinfo		uh_selinfo;
	boolean_t		allow_read;
	struct sysctl_ctx_list	*ctx;
};

static void
uio_hv_sysctl_add(struct uio_hv_softc *sc, struct uh_res *res,
		  const char *descr, boolean_t publish_gpadl);

static inline void
uio_hv_event_notify(struct uio_hv_softc *sc)
{
	atomic_add_32(&sc->revent, 1);
	selwakeup(&sc->uh_selinfo);
}

/*
 * Callback from vmbus_event when something is in inbound ring.
 */
static void
uio_hv_channel_cb(struct vmbus_channel *chan, void *arg)
{
	struct uio_hv_softc *sc = (struct uio_hv_softc *)arg;

	vmbus_rxbr_intr_mask(&chan->ch_rxbr);
	uio_hv_event_notify(sc);
}

static void
uio_hv_rescind_cb(struct vmbus_channel *chan)
{
	struct uio_hv_softc *sc = device_get_softc(chan->ch_dev);

	sc->allow_read = false;
	uio_hv_event_notify(sc);
}

static int
uio_hv_schan_open(struct uio_hv_softc *sc, struct vmbus_channel *schan)
{
	int ret;

	vmbus_chan_cpu_set(schan, schan->ch_subidx % mp_ncpus);
	ret = vmbus_chan_open(schan, sc->cbr_txsz, sc->cbr_rxsz, NULL, 0,
			      uio_hv_channel_cb, sc);
	if (ret)
		return ret;

	/* Disable interrupts on sub channel */
	vmbus_rxbr_intr_mask(&schan->ch_rxbr);
	sc->subchan_cnt++;
	return 0;
}

static int
uio_hv_read(struct cdev *cdev, struct uio *uio, int ioflag __unused)
{
	struct uio_hv_softc *sc = cdev->si_drv1;
	uint32_t evcount;
	int ret;

	if (sc->allow_read == false)
		return EIO;

	if (uio->uio_resid < sizeof(uint32_t))
		return EINVAL;

	evcount = atomic_load_32(&sc->revent);
	ret = uiomove(&evcount, sizeof(uint32_t), uio);
	if (ret)
		device_printf(sc->dev, "Failed to read\n");
	else
		sc->uevent = evcount;

	return ret;
}

/* Write is used only to disable/enable interrupt from user space processes */
static int
uio_hv_write(struct cdev *cdev, struct uio *uio, int ioflag __unused)
{
	struct uio_hv_softc *sc = cdev->si_drv1;
	struct vmbus_channel *pchan = sc->chan;
	uint32_t irq_state;
	int ret;

	ret = copyin(uio->uio_iov->iov_base, &irq_state, sizeof(uint32_t));
	if (ret) {
		device_printf(sc->dev, "Failed to write\n");
		return ret;
	}

	pchan->ch_rxbr.rxbr_imask = !irq_state;
	mb();

	return 0;
}

static int
uio_hv_open(struct cdev *cdev, int oflags, int devtype, struct thread *td __unused)
{
	struct uio_hv_softc *sc = cdev->si_drv1;
	struct vmbus_channel *pchan = sc->chan;
	struct uh_res *res = NULL;
	int ret = 0;

	if (!atomic_cmpset_int(&sc->cdev_st, UH_ST_CLOSE, UH_ST_OPEN))
		return 0;

	pchan->ch_rescind_cb = uio_hv_rescind_cb;

	ret = vmbus_chan_open(pchan, sc->cbr_txsz, sc->cbr_rxsz, NULL, 0,
			      uio_hv_channel_cb, sc);
	if (ret) {
		device_printf(sc->dev, "failed opening primary chan%u\n",
			      vmbus_chan_id(pchan));
		goto cleanup;
	}
	vmbus_rxbr_intr_mask(&pchan->ch_rxbr);

	res = &sc->res[HV_TXRX_RING_MAP];
	res->hvdma = &pchan->ch_bufring_dma;
	uio_hv_sysctl_add(sc, res, "Channel Tx Rx rings size", false);

	sc->allow_read = true;

	if (bootverbose)
		device_printf(sc->dev, "device opened\n");

	return 0;
cleanup:
	device_printf(sc->dev, "device open failed\n");
	atomic_set_int(&sc->cdev_st, UH_ST_CLOSE);

	return ret;
}

static void
uio_hv_reset(struct uio_hv_softc *sc)
{
	if (!atomic_cmpset_int(&sc->cdev_st, UH_ST_OPEN, UH_ST_CLOSE))
		return;

	sc->allow_read = false;
	sysctl_remove_oid(sc->res[HV_TXRX_RING_MAP].oid, 1, 1);
	sc->res[HV_TXRX_RING_MAP].hvdma = NULL;
	vmbus_chan_close(sc->chan);
	sc->subchan_cnt = 0;
}

/* Only the last close call to the userspace process object will reach here */
static int
uio_hv_close(struct cdev *cdev, int fflag __unused, int devtype __unused,
	     struct thread *td __unused)
{
	struct uio_hv_softc *sc = cdev->si_drv1;

	if (bootverbose)
		device_printf(sc->dev, "device close\n");

	uio_hv_reset(sc);

	return 0;
}

static int
uio_hv_poll(struct cdev *cdev, int events, struct thread *td)
{
	struct uio_hv_softc *sc = cdev->si_drv1;
	int ret = 0;

	if (sc->uevent != atomic_load_32(&sc->revent))
		ret = POLLIN;
	else
		selrecord(td, &sc->uh_selinfo);

	return ret;
}

static int
uio_hv_cdev_pg_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
			  vm_ooffset_t foff, struct ucred *cred, u_short *color)
{
	struct uio_hv_softc *sc = handle;

	if (color)
		*color = 0;
	dev_ref(sc->uh_cdev);
	return 0;
}

static void
uio_hv_cdev_pg_dtor(void *handle)
{
	struct uio_hv_softc *sc = handle;

	dev_rel(sc->uh_cdev);
}

	static int
uio_hv_cdev_pg_fault(vm_object_t object, vm_ooffset_t offset, int prot,
			vm_page_t *mres)
{
	vm_memattr_t memattr, memattr1;
	vm_page_t page, m_paddr;
	vm_paddr_t paddr;

	memattr = object->memattr;
	VM_OBJECT_WUNLOCK(object);
	paddr = offset;

	m_paddr = vm_phys_paddr_to_vm_page(paddr);
	if (m_paddr != NULL) {
		memattr1 = pmap_page_get_memattr(m_paddr);
		if (memattr1 != memattr)
			memattr = memattr1;
	}
	if (((*mres)->flags & PG_FICTITIOUS) != 0) {
		/*
		 * If the passed in result page is a fake page, update it with
		 * the new physical address.
		 */
		page = *mres;
		VM_OBJECT_WLOCK(object);
		vm_page_updatefake(page, paddr, memattr);
	} else {
		/*
		 * Replace the passed in reqpage page with our own fake page and
		 * free up the original page.
		 */
		page = vm_page_getfake(paddr, memattr);
		VM_OBJECT_WLOCK(object);
#if __FreeBSD__ >= 13
		vm_page_replace(page, object, (*mres)->pindex, *mres);
#else
		vm_page_t mret = vm_page_replace(page, object, (*mres)->pindex);

		KASSERT(mret == *mres,
			("invalid page replacement, old=%p, ret=%p", *mres, mret));
		vm_page_lock(mret);
		vm_page_free(mret);
		vm_page_unlock(mret);
#endif
		*mres = page;
	}
	page->valid = VM_PAGE_BITS_ALL;
	return VM_PAGER_OK;
}

static struct cdev_pager_ops uh_cdev_pager_ops = {
	.cdev_pg_ctor = uio_hv_cdev_pg_ctor,
	.cdev_pg_dtor = uio_hv_cdev_pg_dtor,
	.cdev_pg_fault = uio_hv_cdev_pg_fault,
};

static int
uio_hv_mmap_single(struct cdev *cdev, vm_ooffset_t *offset, vm_size_t size,
		   struct vm_object **obj, int nprot)
{
	struct uio_hv_softc *sc = cdev->si_drv1;
	struct vmbus_channel *pchan = vmbus_get_channel(sc->dev);
	struct vmbus_channel **schans;
	uint32_t req = *offset/PAGE_SIZE;	/* map region is encoded in the offset */
	uint32_t reg = req & UH_MEM_MASK;
	uint32_t subchan_id = req >> UH_SUBCHAN_MASK_SHIFT;
	bool schan_found = false;
	int i, ret = 0;

	if ((pchan->ch_stflags & VMBUS_CHAN_ST_OPENED) == 0) {
		ret = ENODEV;
		goto end;
	}

	if (bootverbose)
		device_printf(sc->dev, "mmap request for reg %u subchan_id %u\n",
			      reg, subchan_id);

	if (!subchan_id) {
		if (reg >= HV_MAX_RESOURCE) {
			device_printf(sc->dev, "Invalid resource request\n");
			ret = EINVAL;
			goto end;
		}

		*offset = (vm_ooffset_t) sc->res[reg].hvdma->hv_paddr;
	} else {
		schans = vmbus_subchan_get(pchan, sc->subchan_cnt);

		for (i = 0; i < sc->subchan_cnt; ++i) {
			if (schans[i]->ch_id == subchan_id) {
				schan_found = true;
				*offset =
				    (vm_ooffset_t)schans[i]->ch_bufring_dma.hv_paddr;
				break;
			}
		}

		vmbus_subchan_rel(schans, sc->subchan_cnt);
		if (schan_found == false) {
			device_printf(sc->dev, "Invalid subchan idx\n");
			ret = ENODEV;
			goto end;
		}
	}

	*obj = cdev_pager_allocate(sc, OBJT_DEVICE, &uh_cdev_pager_ops, size, nprot,
				   *offset, curthread->td_ucred);
	if (*obj == NULL) {
		device_printf(sc->dev, "vm_pager_allocate failed\n");
		ret = ENOMEM;
	}
end:
	return ret;
}

static int
uio_hv_schan_open_all(struct uio_hv_softc *sc, uint32_t subchan_cnt)
{
	struct vmbus_channel *pchan = sc->chan;
	struct vmbus_channel **subchans;
	int ret, i, sc_fail = 0;

	KASSERT(subchan_cnt > 0, ("subchan count should not be 0"));

	if (subchan_cnt < pchan->ch_subchan_cnt) {
		device_printf(sc->dev, "%u subchan open req received"
				"pchan has %u subchannels", subchan_cnt,
				pchan->ch_subchan_cnt);
		return EINVAL;
	}

	/* Waits for all sub-channels to become ready */
	subchans = vmbus_subchan_get(pchan, subchan_cnt);
	for (i = 0; i < subchan_cnt; ++i) {
		ret = uio_hv_schan_open(sc, subchans[i]);
		/* On err, all subchans will be closed later */
		if (ret)
			sc_fail = ret;
	}
	vmbus_subchan_rel(subchans, subchan_cnt);

	if (sc_fail) {
		device_printf(sc->dev, "subchan open failed, device closing\n");
		return sc_fail;
	}

	return 0;
}

static int
uio_hv_ioctl(struct cdev *cdev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct uio_hv_softc *sc = cdev->si_drv1;
	uint32_t nschan;
	int ret = 0;

	if (bootverbose)
		device_printf(sc->dev, "ioctl cmd %lu\n", cmd);
	switch (cmd) {
	case HVIOOPENSUBCHAN:
		/* userspace should pass the subchan cnt returned by NVS */
		nschan = *(uint32_t *)data;
		if (nschan == 0)
			break;

		ret = uio_hv_schan_open_all(sc, nschan);
		break;
	default:
		if (bootverbose)
			device_printf(sc->dev,
				      "invalid ioctl cmd %lu\n", cmd);
		ret = EINVAL;
	}
	return ret;
}

static struct cdevsw uh_cdevsw = {
	.d_name		= UH_DNAME,
	.d_version	= D_VERSION,
	.d_open		= uio_hv_open,
	.d_close	= uio_hv_close,
	.d_read		= uio_hv_read,
	.d_write	= uio_hv_write,
	.d_ioctl	= uio_hv_ioctl,
	.d_poll		= uio_hv_poll,
	.d_mmap_single	= uio_hv_mmap_single,
};

static void
uio_hv_sysctl_add(struct uio_hv_softc *sc, struct uh_res *res,
		  const char *descr, boolean_t publish_gpadl)
{
	struct sysctl_oid_list *child;
	struct sysctl_oid_list *child2;

	child = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));
	res->oid = SYSCTL_ADD_NODE(sc->ctx, child, OID_AUTO, res->name,
				   CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
	child2 = SYSCTL_CHILDREN(res->oid);
	SYSCTL_ADD_U64(sc->ctx, child2, OID_AUTO, "size", CTLFLAG_RD, NULL,
		       res->size, descr);
	if (publish_gpadl)
		SYSCTL_ADD_U32(sc->ctx, child2, OID_AUTO, "gpadl", CTLFLAG_RD,
			       &res->gpadl, 0, "");
}

static void uio_hv_sysctl_create(struct uio_hv_softc *sc)
{
	struct uh_res *res = NULL;

	sc->ctx = device_get_sysctl_ctx(sc->dev);

	res = &sc->res[HV_INT_PAGE_MAP];
	uio_hv_sysctl_add(sc, res, "VMBUS Interrupt Page size", false);

	res = &sc->res[HV_MON_PAGE_MAP];
	uio_hv_sysctl_add(sc, res, "VMBUS Monitor Page size", false);

	res = &sc->res[HV_RECV_BUF_MAP];
	uio_hv_sysctl_add(sc, res, "Channel Receive Buffer size", true);

	res = &sc->res[HV_SEND_BUF_MAP];
	uio_hv_sysctl_add(sc, res, "Channel Send Buffer size", true);

	SYSCTL_ADD_U32(sc->ctx, SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)),
		       OID_AUTO, "subchan_cnt", CTLFLAG_RD, &sc->subchan_cnt, 0,
		       "sub channels count");
}

static void
uio_hv_gpadl_free(struct uio_hv_softc *sc, enum uio_hv_map mr)
{
	struct uh_res *res = &sc->res[mr];

	if (res->gpadl) {
		vmbus_chan_gpadl_disconnect(sc->chan, res->gpadl);
		res->gpadl = 0;
	}

	if (res->buf) {
		hyperv_dmamem_free(res->hvdma, res->buf);
		res->buf = NULL;
	}

	if (res->hvdma) {
		free(res->hvdma, M_DEVBUF);
		res->hvdma = NULL;
	}
}

static int
uio_hv_gpadl_alloc(struct uio_hv_softc *sc, enum uio_hv_map mr, uint64_t size)
{
	struct uh_res *res = &sc->res[mr];
	int ret = 0;

	res->hvdma = malloc(sizeof(struct hyperv_dma), M_DEVBUF, M_WAITOK);
	if (!res->hvdma)
		return ENOMEM;

	res->buf = hyperv_dmamem_alloc(bus_get_dma_tag(sc->dev), PAGE_SIZE, 0,
				       size, res->hvdma,
				       BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (!res->buf) {
		ret = ENOMEM;
		goto cleanup;
	}

	ret = vmbus_chan_gpadl_connect(sc->chan, res->hvdma->hv_paddr, size,
				       &res->gpadl);
	if (ret)
		goto cleanup;

	return 0;
cleanup:
	uio_hv_gpadl_free(sc, mr);
	return ret;
}

static int
uio_hv_detach(device_t dev)
{
	struct uio_hv_softc *sc = device_get_softc(dev);

	if (sc->uh_cdev) {
		uio_hv_reset(sc);
		destroy_dev(sc->uh_cdev);
	}

	uio_hv_gpadl_free(sc, HV_SEND_BUF_MAP);
	uio_hv_gpadl_free(sc, HV_RECV_BUF_MAP);

	sc->chan = NULL;

	return 0;
}

static int
uio_hv_attach(device_t dev)
{
	struct vmbus_channel *chan = vmbus_get_channel(dev);
	struct uio_hv_softc *sc = device_get_softc(dev);
	int ret;

	/* Communicating with host has to be via shared memory not hypercall */
	if (!(chan->ch_txflags & VMBUS_CHAN_TXF_HASMNF)) {
		device_printf(dev, "vmbus channel requires shared memory communication\n");
		return ENOTSUP;
	}

	sc->dev = dev;
	sc->chan = chan;

	sc->cbr_txsz = HV_RING_SIZE * PAGE_SIZE;
	sc->cbr_rxsz = HV_RING_SIZE * PAGE_SIZE;

	vmbus_chan_cpu_set(chan, chan->ch_subidx % mp_ncpus);

	/* Channel TxRx rings are created while cdev open and not here
	 * since closing the channel destroys the rings
	 */
	strncpy(sc->res[HV_TXRX_RING_MAP].name, "txrx_rings", UH_NAMESIZE);
	sc->res[HV_TXRX_RING_MAP].size = sc->cbr_txsz + sc->cbr_rxsz;

	strncpy(sc->res[HV_INT_PAGE_MAP].name, "int_page", UH_NAMESIZE);
	sc->res[HV_INT_PAGE_MAP].hvdma = vmbus_get_mem_evtflags();
	sc->res[HV_INT_PAGE_MAP].size = PAGE_SIZE;

	strncpy(sc->res[HV_MON_PAGE_MAP].name, "monitor_page", UH_NAMESIZE);
	sc->res[HV_MON_PAGE_MAP].hvdma = vmbus_get_mem_mnf2();
	sc->res[HV_MON_PAGE_MAP].size = PAGE_SIZE;

	ret = uio_hv_gpadl_alloc(sc, HV_RECV_BUF_MAP, RECV_BUFFER_SIZE);
	if (ret) {
		device_printf(dev, "Failed to get recv gpadl\n");
		goto cleanup;
	}
	strncpy(sc->res[HV_RECV_BUF_MAP].name, "recv_buf", UH_NAMESIZE);
	sc->res[HV_RECV_BUF_MAP].size = RECV_BUFFER_SIZE;

	ret = uio_hv_gpadl_alloc(sc, HV_SEND_BUF_MAP, SEND_BUFFER_SIZE);
	if (ret) {
		device_printf(dev, "Failed to get send gpadl\n");
		goto cleanup;
	}
	strncpy(sc->res[HV_SEND_BUF_MAP].name, "send_buf", UH_NAMESIZE);
	sc->res[HV_SEND_BUF_MAP].size = SEND_BUFFER_SIZE;

	ret = make_dev_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK, &sc->uh_cdev,
			 &uh_cdevsw, NULL, UID_ROOT, GID_WHEEL, 0640, "%s",
			 device_get_nameunit(dev));
	if (ret)
		goto cleanup;

	sc->uh_cdev->si_drv1 = sc;

	/*Create sysctl tree for this device */
	uio_hv_sysctl_create(sc);

	return 0;
cleanup:
	uio_hv_detach(dev);
	return ret;
}

static int
uio_hv_probe(device_t dev)
{
	/* Presently only VMBus Network Device is driven */
	if (VMBUS_PROBE_GUID(device_get_parent(dev), dev, &hn_guid) == 0) {
		device_set_desc(dev, "Hyper-V UIO Network Interface");
		return BUS_PROBE_DEFAULT;
	}
	return ENXIO;
}

static int
uio_hv_shutdown(device_t dev)
{
	return 0;
}

static inline void
uio_hv_load(void)
{
	printf("%s ver %d module loaded\n", UH_DNAME, DRIVER_VERSION);
}

static inline void
uio_hv_unload(void)
{
	printf("%s module unloaded\n", UH_DNAME);
}

/* KLD event handler */
static int
uio_hv_modevh(module_t mod __unused, int event, void *arg __unused)
{
	int ret = 0;

	switch (event) {
	case MOD_LOAD:
		uio_hv_load();
		break;
	case MOD_UNLOAD:
		uio_hv_unload();
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		ret = EOPNOTSUPP;
		break;
	}
	return ret;
}

static device_method_t uio_hv_methods[] = {
	DEVMETHOD(device_probe,     uio_hv_probe),
	DEVMETHOD(device_shutdown,  uio_hv_shutdown),
	DEVMETHOD(device_attach,    uio_hv_attach),
	DEVMETHOD(device_detach,    uio_hv_detach),
	DEVMETHOD_END
};

static devclass_t uio_hv_devclass;

static driver_t uio_hv_driver = {
	UH_DNAME,
	uio_hv_methods,
	sizeof(struct uio_hv_softc)
};

DRIVER_MODULE(hv_uio, vmbus, uio_hv_driver, uio_hv_devclass, uio_hv_modevh, NULL);
MODULE_VERSION(hv_uio, DRIVER_VERSION);
MODULE_DEPEND(hv_uio, vmbus, VMBUS_VMIN, VMBUS_VPREF, VMBUS_VMAX);
