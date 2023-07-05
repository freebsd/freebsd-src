/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2023, Broadcom Inc. All rights reserved. 
 * Support: <fbsd-storage-driver.pdl@broadcom.com>
 *
 * Authors: Sumit Saxena <sumit.saxena@broadcom.com>
 *	    Chandrakanth Patil <chandrakanth.patil@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 3. Neither the name of the Broadcom Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing
 * official policies,either expressed or implied, of the FreeBSD Project.
 *
 * Mail to: Broadcom Inc 1320 Ridder Park Dr, San Jose, CA 95131
 *
 * Broadcom Inc. (Broadcom) MPI3MR Adapter FreeBSD
 */

#include "mpi3mr.h"
#include "mpi3mr_cam.h"
#include "mpi3mr_app.h"

static int 	sc_ids;
static int	mpi3mr_pci_probe(device_t);
static int	mpi3mr_pci_attach(device_t);
static int	mpi3mr_pci_detach(device_t);
static int	mpi3mr_pci_suspend(device_t);
static int	mpi3mr_pci_resume(device_t);
static int 	mpi3mr_setup_resources(struct mpi3mr_softc *sc);
static void	mpi3mr_release_resources(struct mpi3mr_softc *);
static void	mpi3mr_teardown_irqs(struct mpi3mr_softc *sc);

extern void	mpi3mr_watchdog_thread(void *arg);

static device_method_t mpi3mr_methods[] = {
	DEVMETHOD(device_probe,		mpi3mr_pci_probe),
	DEVMETHOD(device_attach,	mpi3mr_pci_attach),
	DEVMETHOD(device_detach,	mpi3mr_pci_detach),
	DEVMETHOD(device_suspend,	mpi3mr_pci_suspend),
	DEVMETHOD(device_resume,	mpi3mr_pci_resume),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	{ 0, 0 }
};

char fmt_os_ver[16];

SYSCTL_NODE(_hw, OID_AUTO, mpi3mr, CTLFLAG_RD, 0, "MPI3MR Driver Parameters");
MALLOC_DEFINE(M_MPI3MR, "mpi3mrbuf", "Buffers for the MPI3MR driver");

static driver_t mpi3mr_pci_driver = {
	"mpi3mr",
	mpi3mr_methods,
	sizeof(struct mpi3mr_softc)
};

struct mpi3mr_ident {
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subvendor;
	uint16_t	subdevice;
	u_int		flags;
	const char	*desc;
} mpi3mr_identifiers[] = {
	{ MPI3_MFGPAGE_VENDORID_BROADCOM, MPI3_MFGPAGE_DEVID_SAS4116,
	    0xffff, 0xffff, 0, "Broadcom MPIMR 3.0 controller" },
};

DRIVER_MODULE(mpi3mr, pci, mpi3mr_pci_driver, 0, 0);
MODULE_PNP_INFO("U16:vendor;U16:device;U16:subvendor;U16:subdevice;D:#", pci,
    mpi3mr, mpi3mr_identifiers, nitems(mpi3mr_identifiers) - 1);

MODULE_DEPEND(mpi3mr, cam, 1, 1, 1);

/*
 * mpi3mr_setup_sysctl:	setup sysctl values for mpi3mr
 * input:		Adapter instance soft state
 *
 * Setup sysctl entries for mpi3mr driver.
 */
static void
mpi3mr_setup_sysctl(struct mpi3mr_softc *sc)
{
	struct sysctl_ctx_list *sysctl_ctx = NULL;
	struct sysctl_oid *sysctl_tree = NULL;
	char tmpstr[80], tmpstr2[80];

	/*
	 * Setup the sysctl variable so the user can change the debug level
	 * on the fly.
	 */
	snprintf(tmpstr, sizeof(tmpstr), "MPI3MR controller %d",
	    device_get_unit(sc->mpi3mr_dev));
	snprintf(tmpstr2, sizeof(tmpstr2), "%d", device_get_unit(sc->mpi3mr_dev));

	sysctl_ctx = device_get_sysctl_ctx(sc->mpi3mr_dev);
	if (sysctl_ctx != NULL)
		sysctl_tree = device_get_sysctl_tree(sc->mpi3mr_dev);

	if (sysctl_tree == NULL) {
		sysctl_ctx_init(&sc->sysctl_ctx);
		sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
		    SYSCTL_STATIC_CHILDREN(_hw_mpi3mr), OID_AUTO, tmpstr2,
		    CTLFLAG_RD, 0, tmpstr);
		if (sc->sysctl_tree == NULL)
			return;
		sysctl_ctx = &sc->sysctl_ctx;
		sysctl_tree = sc->sysctl_tree;
	}

	SYSCTL_ADD_STRING(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "driver_version", CTLFLAG_RD, MPI3MR_DRIVER_VERSION,
	    strlen(MPI3MR_DRIVER_VERSION), "driver version");

	SYSCTL_ADD_INT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "fw_outstanding", CTLFLAG_RD,
	    &sc->fw_outstanding.val_rdonly, 0, "FW outstanding commands");

	SYSCTL_ADD_INT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "io_cmds_highwater", CTLFLAG_RD,
	    &sc->io_cmds_highwater, 0, "Max FW outstanding commands");

	SYSCTL_ADD_UINT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "mpi3mr_debug", CTLFLAG_RW, &sc->mpi3mr_debug, 0,
	    "Driver debug level");
	SYSCTL_ADD_UINT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "reset", CTLFLAG_RW, &sc->reset.type, 0,
	    "Soft reset(1)/Diag reset(2)");
	SYSCTL_ADD_UINT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "iot_enable", CTLFLAG_RW, &sc->iot_enable, 0,
	    "IO throttling enable at driver level(for debug purpose)");
}

/*
 * mpi3mr_get_tunables:	get tunable parameters.
 * input:		Adapter instance soft state
 *
 * Get tunable parameters. This will help to debug driver at boot time.
 */
static void
mpi3mr_get_tunables(struct mpi3mr_softc *sc)
{
	char tmpstr[80];

	sc->mpi3mr_debug =
		(MPI3MR_ERROR | MPI3MR_INFO | MPI3MR_FAULT);
	
	sc->reset_in_progress = 0;
	sc->reset.type = 0;
	sc->iot_enable = 1;
	/*
	 * Grab the global variables.
	 */
	TUNABLE_INT_FETCH("hw.mpi3mr.debug_level", &sc->mpi3mr_debug);
	TUNABLE_INT_FETCH("hw.mpi3mr.ctrl_reset", &sc->reset.type);
	TUNABLE_INT_FETCH("hw.mpi3mr.iot_enable", &sc->iot_enable);

	/* Grab the unit-instance variables */
	snprintf(tmpstr, sizeof(tmpstr), "dev.mpi3mr.%d.debug_level",
	    device_get_unit(sc->mpi3mr_dev));
	TUNABLE_INT_FETCH(tmpstr, &sc->mpi3mr_debug);
	
	snprintf(tmpstr, sizeof(tmpstr), "dev.mpi3mr.%d.reset",
	    device_get_unit(sc->mpi3mr_dev));
	TUNABLE_INT_FETCH(tmpstr, &sc->reset.type);
	
	snprintf(tmpstr, sizeof(tmpstr), "dev.mpi3mr.%d.iot_enable",
	    device_get_unit(sc->mpi3mr_dev));
	TUNABLE_INT_FETCH(tmpstr, &sc->iot_enable);
}

static struct mpi3mr_ident *
mpi3mr_find_ident(device_t dev)
{
	struct mpi3mr_ident *m;

	for (m = mpi3mr_identifiers; m->vendor != 0; m++) {
		if (m->vendor != pci_get_vendor(dev))
			continue;
		if (m->device != pci_get_device(dev))
			continue;
		if ((m->subvendor != 0xffff) &&
		    (m->subvendor != pci_get_subvendor(dev)))
			continue;
		if ((m->subdevice != 0xffff) &&
		    (m->subdevice != pci_get_subdevice(dev)))
			continue;
		return (m);
	}

	return (NULL);
}

static int
mpi3mr_pci_probe(device_t dev)
{
	static u_int8_t first_ctrl = 1;
	struct mpi3mr_ident *id;
	char raw_os_ver[16];

	if ((id = mpi3mr_find_ident(dev)) != NULL) {
		if (first_ctrl) {
			first_ctrl = 0;
			MPI3MR_OS_VERSION(raw_os_ver, fmt_os_ver);
			printf("mpi3mr: Loading Broadcom mpi3mr driver version: %s  OS version: %s\n",
			    MPI3MR_DRIVER_VERSION, fmt_os_ver);
		}
		device_set_desc(dev, id->desc);
		device_set_desc(dev, id->desc);
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static void
mpi3mr_release_resources(struct mpi3mr_softc *sc)
{
	if (sc->mpi3mr_parent_dmat != NULL) {
		bus_dma_tag_destroy(sc->mpi3mr_parent_dmat);
	}
	
	if (sc->mpi3mr_regs_resource != NULL) {
		bus_release_resource(sc->mpi3mr_dev, SYS_RES_MEMORY,
		    sc->mpi3mr_regs_rid, sc->mpi3mr_regs_resource);
	}
}

static int mpi3mr_setup_resources(struct mpi3mr_softc *sc)
{
	int i;
	device_t dev = sc->mpi3mr_dev;
	
	pci_enable_busmaster(dev);

	for (i = 0; i < PCI_MAXMAPS_0; i++) {
		sc->mpi3mr_regs_rid = PCIR_BAR(i);

		if ((sc->mpi3mr_regs_resource = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &sc->mpi3mr_regs_rid, RF_ACTIVE)) != NULL)
			break;
	}

	if (sc->mpi3mr_regs_resource == NULL) {
		mpi3mr_printf(sc, "Cannot allocate PCI registers\n");
		return (ENXIO);
	}

	sc->mpi3mr_btag = rman_get_bustag(sc->mpi3mr_regs_resource);
	sc->mpi3mr_bhandle = rman_get_bushandle(sc->mpi3mr_regs_resource);

	/* Allocate the parent DMA tag */
	if (bus_dma_tag_create(bus_get_dma_tag(dev),  	/* parent */
				1, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsize */
				BUS_SPACE_UNRESTRICTED,	/* nsegments */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &sc->mpi3mr_parent_dmat)) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot allocate parent DMA tag\n");
		return (ENOMEM);
        }

	sc->max_msix_vectors = pci_msix_count(dev);
	
	return 0;
}

static int
mpi3mr_startup(struct mpi3mr_softc *sc)
{
	sc->mpi3mr_flags &= ~MPI3MR_FLAGS_PORT_ENABLE_DONE;
	mpi3mr_issue_port_enable(sc, 1);
	return (0);
}

/* Run through any late-start handlers. */
static void
mpi3mr_ich_startup(void *arg)
{
	struct mpi3mr_softc *sc;

	sc = (struct mpi3mr_softc *)arg;
	mpi3mr_dprint(sc, MPI3MR_XINFO, "%s entry\n", __func__);

	mtx_lock(&sc->mpi3mr_mtx);
	
	mpi3mr_startup(sc);
	mtx_unlock(&sc->mpi3mr_mtx);

	mpi3mr_dprint(sc, MPI3MR_XINFO, "disestablish config intrhook\n");
	config_intrhook_disestablish(&sc->mpi3mr_ich);
	sc->mpi3mr_ich.ich_arg = NULL;

	mpi3mr_dprint(sc, MPI3MR_XINFO, "%s exit\n", __func__);
}

/**
 * mpi3mr_ctrl_security_status -Check controller secure status
 * @pdev: PCI device instance
 *
 * Read the Device Serial Number capability from PCI config
 * space and decide whether the controller is secure or not.
 *
 * Return: 0 on success, non-zero on failure.
 */
static int
mpi3mr_ctrl_security_status(device_t dev)
{
	int dev_serial_num, retval = 0;
	uint32_t cap_data, ctrl_status, debug_status;
	/* Check if Device serial number extended capability is supported */
	if (pci_find_extcap(dev, PCIZ_SERNUM, &dev_serial_num) != 0) {
		device_printf(dev,
		    "PCIZ_SERNUM is not supported\n");
		return -1;
	}

	cap_data = pci_read_config(dev, dev_serial_num + 4, 4);

	debug_status = cap_data & MPI3MR_CTLR_SECURE_DBG_STATUS_MASK;
	ctrl_status = cap_data & MPI3MR_CTLR_SECURITY_STATUS_MASK;

	switch (ctrl_status) {
	case MPI3MR_INVALID_DEVICE:
		device_printf(dev,
		    "Invalid (Non secure) controller is detected: DID: 0x%x: SVID: 0x%x: SDID: 0x%x\n",
		    pci_get_device(dev), pci_get_subvendor(dev),
		    pci_get_subdevice(dev));
		retval = -1;
		break;
	case MPI3MR_CONFIG_SECURE_DEVICE:
		if (!debug_status)
			device_printf(dev, "Config secure controller is detected\n");
		break;
	case MPI3MR_HARD_SECURE_DEVICE:
		device_printf(dev, "Hard secure controller is detected\n");
		break;
	case MPI3MR_TAMPERED_DEVICE:
		device_printf(dev,
		    "Tampered (Non secure) controller is detected: DID: 0x%x: SVID: 0x%x: SDID: 0x%x\n",
		    pci_get_device(dev), pci_get_subvendor(dev),
		    pci_get_subdevice(dev));
		retval = -1;
		break;
	default:
		retval = -1;
			break;
	}

	if (!retval && debug_status) {
		device_printf(dev,
		    "Secure Debug (Non secure) controller is detected: DID: 0x%x: SVID: 0x%x: SDID: 0x%x\n",
		    pci_get_device(dev), pci_get_subvendor(dev),
		    pci_get_subdevice(dev));
		retval = -1;
	}

	return retval;
}
/*
 * mpi3mr_pci_attach - PCI entry point
 * @dev: pointer to device struct
 *
 * This function does the setup of PCI and registers, allocates controller resources,
 * initializes mutexes, linked lists and registers interrupts, CAM and initializes
 * the controller.
 *
 * Return: 0 on success and proper error codes on failure
 */
static int
mpi3mr_pci_attach(device_t dev)
{
	struct mpi3mr_softc *sc;
	int error;

	sc = device_get_softc(dev);
	bzero(sc, sizeof(*sc));
	sc->mpi3mr_dev = dev;
	
	/* Don't load driver for Non-Secure controllers */
	if (mpi3mr_ctrl_security_status(dev)) {
		sc->secure_ctrl = false;
		return 0; 
	}
	
	sc->secure_ctrl = true;

	if ((error = mpi3mr_setup_resources(sc)) != 0)
		goto load_failed;
	
	sc->id = sc_ids++;
	mpi3mr_atomic_set(&sc->fw_outstanding, 0);
	mpi3mr_atomic_set(&sc->pend_ioctls, 0);
	sc->admin_req = NULL;
	sc->admin_reply = NULL;
	sprintf(sc->driver_name, "%s", MPI3MR_DRIVER_NAME);
	sprintf(sc->name, "%s%d", sc->driver_name, sc->id);

	sc->mpi3mr_dev = dev;
	mpi3mr_get_tunables(sc);
	
	if ((error = mpi3mr_initialize_ioc(sc, MPI3MR_INIT_TYPE_INIT)) != 0) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "FW initialization failed\n");
		goto load_failed;
	}
	
	if ((error = mpi3mr_alloc_requests(sc)) != 0) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Command frames allocation failed\n");
		goto load_failed;
	}

	if ((error = mpi3mr_cam_attach(sc)) != 0) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "CAM attach failed\n");
		goto load_failed;
	}
	
	error = mpi3mr_kproc_create(mpi3mr_watchdog_thread, sc,
	    &sc->watchdog_thread, 0, 0, "mpi3mr_watchdog%d",
	    device_get_unit(sc->mpi3mr_dev));
	if (error) {
		device_printf(sc->mpi3mr_dev, "Error %d starting OCR thread\n", error);
		goto load_failed;
	}
	
	sc->mpi3mr_ich.ich_func = mpi3mr_ich_startup;
	sc->mpi3mr_ich.ich_arg = sc;
	if (config_intrhook_establish(&sc->mpi3mr_ich) != 0) {
		mpi3mr_dprint(sc, MPI3MR_ERROR,
		    "Cannot establish MPI3MR ICH config hook\n");
		error = EINVAL;
	}

	mpi3mr_dprint(sc, MPI3MR_INFO, "allocating ioctl dma buffers\n");
	mpi3mr_alloc_ioctl_dma_memory(sc);
	
	if ((error = mpi3mr_app_attach(sc)) != 0) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "APP/IOCTL attach failed\n");
		goto load_failed;
	}

	mpi3mr_setup_sysctl(sc);

	return 0;

load_failed:
	mpi3mr_cleanup_interrupts(sc);
	mpi3mr_free_mem(sc);
	mpi3mr_app_detach(sc);
	mpi3mr_cam_detach(sc);
	mpi3mr_destory_mtx(sc);
	mpi3mr_release_resources(sc);
	return error;
}

void mpi3mr_cleanup_interrupts(struct mpi3mr_softc *sc)
{
	mpi3mr_disable_interrupts(sc);
	
	mpi3mr_teardown_irqs(sc);
	
	if (sc->irq_ctx) {
		free(sc->irq_ctx, M_MPI3MR);
		sc->irq_ctx = NULL;
	}
	
	if (sc->msix_enable)
		pci_release_msi(sc->mpi3mr_dev);

	sc->msix_count = 0;
	
}

int mpi3mr_setup_irqs(struct mpi3mr_softc *sc)
{
	device_t dev;
	int error;
	int i, rid, initial_rid;
	struct mpi3mr_irq_context *irq_ctx;
	struct irq_info *irq_info;

	dev = sc->mpi3mr_dev;
	error = -1;

	if (sc->msix_enable)
		initial_rid = 1;
	else
		initial_rid = 0;

	for (i = 0; i < sc->msix_count; i++) {
		irq_ctx = &sc->irq_ctx[i];
		irq_ctx->msix_index = i;
		irq_ctx->sc = sc;
		irq_info = &irq_ctx->irq_info;
		rid = i + initial_rid;
		irq_info->irq_rid = rid;
		irq_info->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &irq_info->irq_rid, RF_ACTIVE);
		if (irq_info->irq == NULL) {
			mpi3mr_dprint(sc, MPI3MR_ERROR,
			    "Cannot allocate interrupt RID %d\n", rid);
			sc->msix_count = i;
			break;
		}
		error = bus_setup_intr(dev, irq_info->irq,
		    INTR_MPSAFE | INTR_TYPE_CAM, NULL, mpi3mr_isr,
		    irq_ctx, &irq_info->intrhand);
		if (error) {
			mpi3mr_dprint(sc, MPI3MR_ERROR,
			    "Cannot setup interrupt RID %d\n", rid);
			sc->msix_count = i;
			break;
		}
	}

        mpi3mr_dprint(sc, MPI3MR_INFO, "Set up %d MSI-x interrupts\n", sc->msix_count);

	return (error);

}

static void
mpi3mr_teardown_irqs(struct mpi3mr_softc *sc)
{
	struct irq_info *irq_info;
	int i;

	for (i = 0; i < sc->msix_count; i++) {
		irq_info = &sc->irq_ctx[i].irq_info;
		if (irq_info->irq != NULL) {
			bus_teardown_intr(sc->mpi3mr_dev, irq_info->irq,
			    irq_info->intrhand);
			bus_release_resource(sc->mpi3mr_dev, SYS_RES_IRQ,
			    irq_info->irq_rid, irq_info->irq);
		}
	}

}

/*
 * Allocate, but don't assign interrupts early.  Doing it before requesting
 * the IOCFacts message informs the firmware that we want to do MSI-X
 * multiqueue.  We might not use all of the available messages, but there's
 * no reason to re-alloc if we don't.
 */
int
mpi3mr_alloc_interrupts(struct mpi3mr_softc *sc, U16 setup_one)
{
	int error, msgs;
	U16 num_queues;

	error = 0;
	msgs = 0;

	mpi3mr_cleanup_interrupts(sc);

	if (setup_one) {
		msgs = 1;
	} else {
		msgs = min(sc->max_msix_vectors, sc->cpu_count);
		num_queues = min(sc->facts.max_op_reply_q, sc->facts.max_op_req_q);
		msgs = min(msgs, num_queues);

		mpi3mr_dprint(sc, MPI3MR_INFO, "Supported MSI-x count: %d "
			" CPU count: %d Requested MSI-x count: %d\n",
			sc->max_msix_vectors,
			sc->cpu_count, msgs);
	}

	if (msgs != 0) {
		error = pci_alloc_msix(sc->mpi3mr_dev, &msgs);
		if (error) {
			mpi3mr_dprint(sc, MPI3MR_ERROR,
			    "Could not allocate MSI-x interrupts Error: %x\n", error);
			goto out_failed;
		} else
			sc->msix_enable = 1;
	}

	sc->msix_count = msgs;
	sc->irq_ctx = malloc(sizeof(struct mpi3mr_irq_context) * msgs,
		M_MPI3MR, M_NOWAIT | M_ZERO);

	if (!sc->irq_ctx) {
		mpi3mr_dprint(sc, MPI3MR_ERROR, "Cannot alloc memory for interrupt info\n");
		error = -1;
		goto out_failed;
	}

	mpi3mr_dprint(sc, MPI3MR_XINFO, "Allocated %d MSI-x interrupts\n", msgs);
	
	return error;
out_failed:
	mpi3mr_cleanup_interrupts(sc);
	return (error);
}

static int
mpi3mr_pci_detach(device_t dev)
{
	struct mpi3mr_softc *sc;
	int i = 0;

	sc = device_get_softc(dev);

	if (!sc->secure_ctrl)
		return 0;
	
	sc->mpi3mr_flags |= MPI3MR_FLAGS_SHUTDOWN;
	
	if (sc->sysctl_tree != NULL)
		sysctl_ctx_free(&sc->sysctl_ctx);
	
	if (sc->watchdog_thread_active)
		wakeup(&sc->watchdog_chan);
	
	while (sc->reset_in_progress && (i < PEND_IOCTLS_COMP_WAIT_TIME)) {
		i++;
		if (!(i % 5)) {
			mpi3mr_dprint(sc, MPI3MR_INFO,
			    "[%2d]waiting for reset to be finished from %s\n", i, __func__);
		}
		pause("mpi3mr_shutdown", hz);
	}

	i = 0;
	while (sc->watchdog_thread_active && (i < 180)) {
		i++;
		if (!(i % 5)) {
			mpi3mr_dprint(sc, MPI3MR_INFO,
			    "[%2d]waiting for "
			    "mpi3mr_reset thread to quit reset %d\n", i,
			    sc->watchdog_thread_active);
		}
		pause("mpi3mr_shutdown", hz);
	}

	i = 0;
	while (mpi3mr_atomic_read(&sc->pend_ioctls) && (i < 180)) {
		i++;
		if (!(i % 5)) {
			mpi3mr_dprint(sc, MPI3MR_INFO,
			    "[%2d]waiting for IOCTL to be finished from %s\n", i, __func__);
		}
		pause("mpi3mr_shutdown", hz);
	}

	mpi3mr_cleanup_ioc(sc);
	mpi3mr_cleanup_event_taskq(sc);
	mpi3mr_app_detach(sc);
	mpi3mr_cam_detach(sc);
	mpi3mr_cleanup_interrupts(sc);
	mpi3mr_destory_mtx(sc);
	mpi3mr_free_mem(sc);
	mpi3mr_release_resources(sc);
	sc_ids--;
	return (0);
}

static int
mpi3mr_pci_suspend(device_t dev)
{
	return (EINVAL);
}

static int
mpi3mr_pci_resume(device_t dev)
{
	return (EINVAL);
}
