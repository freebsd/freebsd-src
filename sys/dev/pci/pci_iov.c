/*-
 * Copyright (c) 2013-2015 Sandvine Inc.  All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/iov.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/pciio.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>
#include <dev/pci/pci_iov_private.h>

#include "pci_if.h"
#include "pcib_if.h"

static MALLOC_DEFINE(M_SRIOV, "sr_iov", "PCI SR-IOV allocations");

static d_ioctl_t pci_iov_ioctl;

static struct cdevsw iov_cdevsw = {
	.d_version = D_VERSION,
	.d_name = "iov",
	.d_ioctl = pci_iov_ioctl
};

#define IOV_READ(d, r, w) \
	pci_read_config((d)->cfg.dev, (d)->cfg.iov->iov_pos + r, w)

#define IOV_WRITE(d, r, v, w) \
	pci_write_config((d)->cfg.dev, (d)->cfg.iov->iov_pos + r, v, w)

int
pci_iov_attach_method(device_t bus, device_t dev)
{
	device_t pcib;
	struct pci_devinfo *dinfo;
	struct pcicfg_iov *iov;
	uint32_t version;
	int error;
	int iov_pos;

	dinfo = device_get_ivars(dev);
	pcib = device_get_parent(bus);
	
	error = pci_find_extcap(dev, PCIZ_SRIOV, &iov_pos);

	if (error != 0)
		return (error);

	version = pci_read_config(dev, iov_pos, 4); 
	if (PCI_EXTCAP_VER(version) != 1) {
		if (bootverbose)
			device_printf(dev, 
			    "Unsupported version of SR-IOV (%d) detected\n",
			    PCI_EXTCAP_VER(version));

		return (ENXIO);
	}

	iov = malloc(sizeof(*dinfo->cfg.iov), M_SRIOV, M_WAITOK | M_ZERO);

	mtx_lock(&Giant);
	if (dinfo->cfg.iov != NULL) {
		error = EBUSY;
		goto cleanup;
	}

	iov->iov_pos = iov_pos;

	iov->iov_cdev = make_dev(&iov_cdevsw, device_get_unit(dev),
	    UID_ROOT, GID_WHEEL, 0600, "iov/%s", device_get_nameunit(dev));

	if (iov->iov_cdev == NULL) {
		error = ENOMEM;
		goto cleanup;
	}
	
	dinfo->cfg.iov = iov;
	iov->iov_cdev->si_drv1 = dinfo;
	mtx_unlock(&Giant);

	return (0);

cleanup:
	free(iov, M_SRIOV);
	mtx_unlock(&Giant);
	return (error);
}

int
pci_iov_detach_method(device_t bus, device_t dev)
{
	struct pci_devinfo *dinfo;
	struct pcicfg_iov *iov;

	mtx_lock(&Giant);
	dinfo = device_get_ivars(dev);
	iov = dinfo->cfg.iov;

	if (iov == NULL) {
		mtx_unlock(&Giant);
		return (0);
	}

	if (iov->iov_num_vfs != 0) {
		mtx_unlock(&Giant);
		return (EBUSY);
	}

	dinfo->cfg.iov = NULL;

	if (iov->iov_cdev) {
		destroy_dev(iov->iov_cdev);
		iov->iov_cdev = NULL;
	}

	free(iov, M_SRIOV);
	mtx_unlock(&Giant);

	return (0);
}

/*
 * Set the ARI_EN bit in the lowest-numbered PCI function with the SR-IOV
 * capability.  This bit is only writeable on the lowest-numbered PF but
 * affects all PFs on the device.
 */
static int
pci_iov_set_ari(device_t bus)
{
	device_t lowest;
	device_t *devlist;
	int i, error, devcount, lowest_func, lowest_pos, iov_pos, dev_func;
	uint16_t iov_ctl;

	/* If ARI is disabled on the downstream port there is nothing to do. */
	if (!PCIB_ARI_ENABLED(device_get_parent(bus)))
		return (0);

	error = device_get_children(bus, &devlist, &devcount);

	if (error != 0)
		return (error);

	lowest = NULL;
	for (i = 0; i < devcount; i++) {
		if (pci_find_extcap(devlist[i], PCIZ_SRIOV, &iov_pos) == 0) {
			dev_func = pci_get_function(devlist[i]);
			if (lowest == NULL || dev_func < lowest_func) {
				lowest = devlist[i];
				lowest_func = dev_func;
				lowest_pos = iov_pos;
			}
		}
	}

	/*
	 * If we called this function some device must have the SR-IOV
	 * capability.
	 */
	KASSERT(lowest != NULL,
	    ("Could not find child of %s with SR-IOV capability",
	    device_get_nameunit(bus)));

	iov_ctl = pci_read_config(lowest, iov_pos + PCIR_SRIOV_CTL, 2);
	iov_ctl |= PCIM_SRIOV_ARI_EN;
	pci_write_config(lowest, iov_pos + PCIR_SRIOV_CTL, iov_ctl, 2);
	free(devlist, M_TEMP);
	return (0);
}

static int
pci_iov_config_page_size(struct pci_devinfo *dinfo)
{
	uint32_t page_cap, page_size;

	page_cap = IOV_READ(dinfo, PCIR_SRIOV_PAGE_CAP, 4);

	/*
	 * If the system page size is less than the smallest SR-IOV page size
	 * then round up to the smallest SR-IOV page size.
	 */
	if (PAGE_SHIFT < PCI_SRIOV_BASE_PAGE_SHIFT)
		page_size = (1 << 0);
	else
		page_size = (1 << (PAGE_SHIFT - PCI_SRIOV_BASE_PAGE_SHIFT));

	/* Check that the device supports the system page size. */
	if (!(page_size & page_cap))
		return (ENXIO);

	IOV_WRITE(dinfo, PCIR_SRIOV_PAGE_SIZE, page_size, 4);
	return (0);
}

static void
pci_iov_enumerate_vfs(struct pci_devinfo *dinfo, const char *driver,
    uint16_t first_rid, uint16_t rid_stride)
{
	device_t bus, dev, vf;
	struct pcicfg_iov *iov;
	struct pci_devinfo *vfinfo;
	size_t size;
	int i, error;
	uint16_t vid, did, next_rid;

	iov = dinfo->cfg.iov;
	dev = dinfo->cfg.dev;
	bus = device_get_parent(dev);
	size = dinfo->cfg.devinfo_size;
	next_rid = first_rid;
	vid = pci_get_vendor(dev);
	did = IOV_READ(dinfo, PCIR_SRIOV_VF_DID, 2);

	for (i = 0; i < iov->iov_num_vfs; i++, next_rid += rid_stride) {


		vf = PCI_CREATE_IOV_CHILD(bus, dev, next_rid, vid, did);
		if (vf == NULL)
			break;

		vfinfo = device_get_ivars(vf);

		vfinfo->cfg.iov = iov;
		vfinfo->cfg.vf.index = i;

		error = PCI_ADD_VF(dev, i);
		if (error != 0) {
			device_printf(dev, "Failed to add VF %d\n", i);
			pci_delete_child(bus, vf);
		}
	}

	bus_generic_attach(bus);
}

static int
pci_iov_config(struct cdev *cdev, struct pci_iov_arg *arg)
{
	device_t bus, dev;
	const char *driver;
	struct pci_devinfo *dinfo;
	struct pcicfg_iov *iov;
	int error;
	uint16_t rid_off, rid_stride;
	uint16_t first_rid, last_rid;
	uint16_t iov_ctl;
	uint16_t total_vfs;
	int iov_inited;

	mtx_lock(&Giant);
	dinfo = cdev->si_drv1;
	iov = dinfo->cfg.iov;
	dev = dinfo->cfg.dev;
	bus = device_get_parent(dev);
	iov_inited = 0;

	if (iov->iov_num_vfs != 0) {
		mtx_unlock(&Giant);
		return (EBUSY);
	}

	total_vfs = IOV_READ(dinfo, PCIR_SRIOV_TOTAL_VFS, 2);

	if (arg->num_vfs > total_vfs) {
		error = EINVAL;
		goto out;
	}

	/*
	 * If we are creating passthrough devices then force the ppt driver to
	 * attach to prevent a VF driver from claming the VFs.
	 */
	if (arg->passthrough)
		driver = "ppt";
	else
		driver = NULL;

	error = pci_iov_config_page_size(dinfo);
	if (error != 0)
		goto out;

	error = pci_iov_set_ari(bus);
	if (error != 0)
		goto out;

	error = PCI_INIT_IOV(dev, arg->num_vfs);

	if (error != 0)
		goto out;

	iov_inited = 1;
	IOV_WRITE(dinfo, PCIR_SRIOV_NUM_VFS, arg->num_vfs, 2);

	rid_off = IOV_READ(dinfo, PCIR_SRIOV_VF_OFF, 2);
	rid_stride = IOV_READ(dinfo, PCIR_SRIOV_VF_STRIDE, 2);

	first_rid = pci_get_rid(dev) + rid_off;
	last_rid = first_rid + (arg->num_vfs - 1) * rid_stride;

	/* We don't yet support allocating extra bus numbers for VFs. */
	if (pci_get_bus(dev) != PCI_RID2BUS(last_rid)) {
		error = ENOSPC;
		goto out;
	}

	iov_ctl = IOV_READ(dinfo, PCIR_SRIOV_CTL, 2);
	iov_ctl &= ~(PCIM_SRIOV_VF_EN | PCIM_SRIOV_VF_MSE);
	IOV_WRITE(dinfo, PCIR_SRIOV_CTL, iov_ctl, 2);

	iov->iov_num_vfs = arg->num_vfs;

	iov_ctl = IOV_READ(dinfo, PCIR_SRIOV_CTL, 2);
	iov_ctl |= PCIM_SRIOV_VF_EN;
	IOV_WRITE(dinfo, PCIR_SRIOV_CTL, iov_ctl, 2);

	/* Per specification, we must wait 100ms before accessing VFs. */
	pause("iov", roundup(hz, 10));
	pci_iov_enumerate_vfs(dinfo, driver, first_rid, rid_stride);
	mtx_unlock(&Giant);

	return (0);
out:
	if (iov_inited)
		PCI_UNINIT_IOV(dev);
	iov->iov_num_vfs = 0;
	mtx_unlock(&Giant);
	return (error);
}

static int
pci_iov_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{

	switch (cmd) {
	case IOV_CONFIG:
		return (pci_iov_config(dev, (struct pci_iov_arg *)data));
	default:
		return (EINVAL);
	}
}

