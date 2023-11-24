/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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
 * Intel Stratix 10 FPGA Manager.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/sx.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm64/intel/stratix10-svc.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#define	SVC_BUF_SIZE	(2 * 1024 * 1024)

struct fpgamgr_s10_softc {
	struct cdev		*mgr_cdev;
	struct cdev		*mgr_cdev_partial;
	device_t		dev;
	device_t		s10_svc_dev;
	struct s10_svc_mem	mem;
	struct sx		sx;
	int			opened;
};

static int
fpga_open(struct cdev *dev, int flags __unused,
    int fmt __unused, struct thread *td __unused)
{
	struct fpgamgr_s10_softc *sc;
	struct s10_svc_msg msg;
	int ret;
	int err;

	sc = dev->si_drv1;

	sx_xlock(&sc->sx);
	if (sc->opened) {
		sx_xunlock(&sc->sx);
		return (EBUSY);
	}

	err = s10_svc_allocate_memory(sc->s10_svc_dev,
	    &sc->mem, SVC_BUF_SIZE);
	if (err != 0) {
		sx_xunlock(&sc->sx);
		return (ENXIO);
	}

	bzero(&msg, sizeof(struct s10_svc_msg));
	msg.command = COMMAND_RECONFIG;
	if (dev == sc->mgr_cdev_partial)
		msg.flags |= COMMAND_RECONFIG_FLAG_PARTIAL;
	ret = s10_svc_send(sc->s10_svc_dev, &msg);
	if (ret != 0) {
		sx_xunlock(&sc->sx);
		return (ENXIO);
	}

	sc->opened = 1;
	sx_xunlock(&sc->sx);

	return (0);
}

static int
fpga_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct fpgamgr_s10_softc *sc;
	vm_offset_t addr;
	int amnt;

	sc = dev->si_drv1;

	sx_xlock(&sc->sx);
	if (sc->opened == 0) {
		/* Device closed. */
		sx_xunlock(&sc->sx);
		return (ENXIO);
	}

	while (uio->uio_resid > 0) {
		addr = sc->mem.vaddr + sc->mem.fill;
		if (sc->mem.fill >= SVC_BUF_SIZE)
			return (ENOMEM);
		amnt = MIN(uio->uio_resid, (SVC_BUF_SIZE - sc->mem.fill));
		uiomove((void *)addr, amnt, uio);
		sc->mem.fill += amnt;
	}

	sx_xunlock(&sc->sx);

	return (0);
}

static int
fpga_close(struct cdev *dev, int flags __unused,
    int fmt __unused, struct thread *td __unused)
{
	struct fpgamgr_s10_softc *sc;
	struct s10_svc_msg msg;
	int ret;

	sc = dev->si_drv1;

	sx_xlock(&sc->sx);
	if (sc->opened == 0) {
		/* Device closed. */
		sx_xunlock(&sc->sx);
		return (ENXIO);
	}

	/* Submit bitstream */
	bzero(&msg, sizeof(struct s10_svc_msg));
	msg.command = COMMAND_RECONFIG_DATA_SUBMIT;
	msg.payload = (void *)sc->mem.paddr;
	msg.payload_length = sc->mem.fill;
	ret = s10_svc_send(sc->s10_svc_dev, &msg);
	if (ret != 0) {
		device_printf(sc->dev, "Failed to submit data\n");
		s10_svc_free_memory(sc->s10_svc_dev, &sc->mem);
		sc->opened = 0;
		sx_xunlock(&sc->sx);
		return (0);
	}

	/* Claim memory buffer back */
	bzero(&msg, sizeof(struct s10_svc_msg));
	msg.command = COMMAND_RECONFIG_DATA_CLAIM;
	s10_svc_send(sc->s10_svc_dev, &msg);

	s10_svc_free_memory(sc->s10_svc_dev, &sc->mem);
	sc->opened = 0;
	sx_xunlock(&sc->sx);

	return (0);
}

static int
fpga_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags,
    struct thread *td)
{

	return (0);
}

static struct cdevsw fpga_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	fpga_open,
	.d_close =	fpga_close,
	.d_write =	fpga_write,
	.d_ioctl =	fpga_ioctl,
	.d_name =	"FPGA Manager",
};

static int
fpgamgr_s10_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "intel,stratix10-soc-fpga-mgr"))
		return (ENXIO);

	device_set_desc(dev, "Stratix 10 SOC FPGA Manager");

	return (BUS_PROBE_DEFAULT);
}

static int
fpgamgr_s10_attach(device_t dev)
{
	struct fpgamgr_s10_softc *sc;
	devclass_t dc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	dc = devclass_find("s10_svc");
	if (dc == NULL)
		return (ENXIO);

	sc->s10_svc_dev = devclass_get_device(dc, 0);
	if (sc->s10_svc_dev == NULL)
		return (ENXIO);

	sc->mgr_cdev = make_dev(&fpga_cdevsw, 0, UID_ROOT, GID_WHEEL,
	    0600, "fpga%d", device_get_unit(sc->dev));
	if (sc->mgr_cdev == NULL) {
		device_printf(dev, "Failed to create character device.\n");
		return (ENXIO);
	}

	sc->mgr_cdev_partial = make_dev(&fpga_cdevsw, 0, UID_ROOT, GID_WHEEL,
	    0600, "fpga_partial%d", device_get_unit(sc->dev));
	if (sc->mgr_cdev_partial == NULL) {
		device_printf(dev, "Failed to create character device.\n");
		return (ENXIO);
	}

	sx_init(&sc->sx, "s10 fpga");

	sc->mgr_cdev->si_drv1 = sc;
	sc->mgr_cdev_partial->si_drv1 = sc;

	return (0);
}

static int
fpgamgr_s10_detach(device_t dev)
{
	struct fpgamgr_s10_softc *sc;

	sc = device_get_softc(dev);

	destroy_dev(sc->mgr_cdev);
	destroy_dev(sc->mgr_cdev_partial);

	sx_destroy(&sc->sx);

	return (0);
}

static device_method_t fpgamgr_s10_methods[] = {
	DEVMETHOD(device_probe,		fpgamgr_s10_probe),
	DEVMETHOD(device_attach,	fpgamgr_s10_attach),
	DEVMETHOD(device_detach,	fpgamgr_s10_detach),
	{ 0, 0 }
};

static driver_t fpgamgr_s10_driver = {
	"fpgamgr_s10",
	fpgamgr_s10_methods,
	sizeof(struct fpgamgr_s10_softc),
};

DRIVER_MODULE(fpgamgr_s10, simplebus, fpgamgr_s10_driver, 0, 0);
