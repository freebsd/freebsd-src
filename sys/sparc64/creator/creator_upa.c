/*-
 * Copyright (c) 2003 Jake Burkholder.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/consio.h>
#include <sys/eventhandler.h>
#include <sys/fbio.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <dev/ofw/openfirm.h>

#include <dev/fb/fbreg.h>
#include <dev/syscons/syscons.h>

#include <machine/nexusvar.h>
#include <machine/ofw_upa.h>

#include <sparc64/creator/creator.h>

static int creator_upa_attach(device_t dev);
static int creator_upa_probe(device_t dev);

static d_open_t creator_open;
static d_close_t creator_close;
static d_ioctl_t creator_ioctl;
static d_mmap_t creator_mmap;

static void creator_shutdown(void *v);

static device_method_t creator_upa_methods[] = {
	DEVMETHOD(device_probe,		creator_upa_probe),
	DEVMETHOD(device_attach,	creator_upa_attach),

	{ 0, 0 }
};

static driver_t creator_upa_driver = {
	CREATOR_DRIVER_NAME,
	creator_upa_methods,
	sizeof(struct creator_softc),
};

static devclass_t creator_upa_devclass;

static struct cdevsw creator_devsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	creator_open,
	.d_close =	creator_close,
	.d_ioctl =	creator_ioctl,
	.d_mmap =	creator_mmap,
	.d_name =	"fb",
};

struct ffb_map {
	uint64_t fm_virt;
	uint64_t fm_phys;
	uint64_t fm_size;
};

static struct ffb_map ffb_map[] = {
	{ FFB_VIRT_SFB8R,	FFB_PHYS_SFB8R,		0x00400000 },
	{ FFB_VIRT_SFB8G,	FFB_PHYS_SFB8G,		0x00400000 },
	{ FFB_VIRT_SFB8B,	FFB_PHYS_SFB8B,		0x00400000 },
	{ FFB_VIRT_SFB8X,	FFB_PHYS_SFB8X,		0x00400000 },
	{ FFB_VIRT_SFB32,	FFB_PHYS_SFB32,		0x01000000 },
	{ FFB_VIRT_SFB64,	FFB_PHYS_SFB64,		0x02000000 },
	{ FFB_VIRT_FBC,		FFB_PHYS_FBC,		0x00002000 },
	{ FFB_VIRT_FBC_BM,	FFB_PHYS_FBC_BM,	0x00002000 },
	{ FFB_VIRT_DFB8R,	FFB_PHYS_DFB8R,		0x00400000 },
	{ FFB_VIRT_DFB8G,	FFB_PHYS_DFB8G,		0x00400000 },
	{ FFB_VIRT_DFB8B,	FFB_PHYS_DFB8B,		0x00400000 },
	{ FFB_VIRT_DFB8X,	FFB_PHYS_DFB8X,		0x00400000 },
	{ FFB_VIRT_DFB24,	FFB_PHYS_DFB24,		0x01000000 },
	{ FFB_VIRT_DFB32,	FFB_PHYS_DFB32,		0x01000000 },
	{ FFB_VIRT_DFB422A,	FFB_PHYS_DFB422A,	0x00800000 },
	{ FFB_VIRT_DFB422AD,	FFB_PHYS_DFB422AD,	0x00800000 },
	{ FFB_VIRT_DFB24B,	FFB_PHYS_DFB24B,	0x01000000 },
	{ FFB_VIRT_DFB422B,	FFB_PHYS_DFB422B,	0x00800000 },
	{ FFB_VIRT_DFB422BD,	FFB_PHYS_DFB422BD,	0x00800000 },
	{ FFB_VIRT_SFB16Z,	FFB_PHYS_SFB16Z,	0x00800000 },
	{ FFB_VIRT_SFB8Z,	FFB_PHYS_SFB8Z,		0x00800000 },
	{ FFB_VIRT_SFB422,	FFB_PHYS_SFB422,	0x00800000 },
	{ FFB_VIRT_SFB422D,	FFB_PHYS_SFB422D,	0x00800000 },
	{ FFB_VIRT_FBC_KREG,	FFB_PHYS_FBC_KREG,	0x00002000 },
	{ FFB_VIRT_DAC,		FFB_PHYS_DAC,		0x00002000 },
	{ FFB_VIRT_PROM,	FFB_PHYS_PROM,		0x00010000 },
	{ FFB_VIRT_EXP,		FFB_PHYS_EXP,		0x00002000 },
	{ 0x0,			0x0,			0x00000000 }
};

DRIVER_MODULE(creator, nexus, creator_upa_driver, creator_upa_devclass, 0, 0);

static int
creator_upa_probe(device_t dev)
{
	const char *name;
	phandle_t node;
	int type;

	name = nexus_get_name(dev);
	node = nexus_get_node(dev);
	if (strcmp(name, "SUNW,ffb") == 0) {
		if (OF_getprop(node, "board_type", &type, sizeof(type)) == -1)
			return (ENXIO);
		switch (type & 7) {
		case 0x0:
			device_set_desc(dev, "Creator");
			break;
		case 0x3:
			device_set_desc(dev, "Creator3D");
			break;
		default:
			return (ENXIO);
		}
	} else if (strcmp(name, "SUNW,afb") == 0)
		device_set_desc(dev, "Elite3D");
	else
		return (ENXIO);
	return (BUS_PROBE_DEFAULT);
}

static int
creator_upa_attach(device_t dev)
{
	struct creator_softc *sc;
	struct upa_regs *reg;
	video_switch_t *sw;
	phandle_t node;
	bus_addr_t phys;
	bus_size_t size;
	int error;
	int nreg;
	int unit;
	int i;

	node = nexus_get_node(dev);
	if ((sc = (struct creator_softc *)vid_get_adapter(vid_find_adapter(
	    CREATOR_DRIVER_NAME, 0))) != NULL && sc->sc_node == node) {
	    	device_printf(dev, "console\n");
	    	device_set_softc(dev, sc);
	} else {
		sc = device_get_softc(dev);
		bzero(sc, sizeof(struct creator_softc));
		sc->sc_node = node;
		nreg = nexus_get_nreg(dev);
		reg = nexus_get_reg(dev);
		for (i = 0; i < nreg; i++) {
			phys = UPA_REG_PHYS(reg + i);
			size = UPA_REG_SIZE(reg + i);
			sc->sc_rid[i] = 0;
			sc->sc_reg[i] = bus_alloc_resource(dev, SYS_RES_MEMORY,
			    &sc->sc_rid[i], phys, phys + size - 1, size,
			    RF_ACTIVE);
			if (sc->sc_reg[i] == NULL) {
				device_printf(dev,
				    "cannot allocate resources\n");
				error = ENXIO;
				goto fail;
			}
			sc->sc_bt[i] = rman_get_bustag(sc->sc_reg[i]);
			sc->sc_bh[i] = rman_get_bushandle(sc->sc_reg[i]);
		}
		if (strcmp(nexus_get_name(dev), "SUNW,afb") == 0)
			sc->sc_flags |= CREATOR_AFB;
		if ((sw = vid_get_switch(CREATOR_DRIVER_NAME)) == NULL) {
			device_printf(dev, "cannot get video switch\n");
			error = ENODEV;
			goto fail;
		}
		/*
		 * During device configuration we don't necessarily probe
		 * the adapter which is the console first so we can't use
		 * the device unit number for the video adapter unit. The
		 * worst case would be that we use the video adapter unit
		 * 0 twice. As it doesn't really matter which unit number
		 * the corresponding video adapter has just use the next
		 * unused one.
		 */
		for (i = 0; i < devclass_get_maxunit(creator_upa_devclass); i++)
			if (vid_find_adapter(CREATOR_DRIVER_NAME, i) < 0)
				break;
		if ((error = sw->init(i, &sc->sc_va, 0)) != 0) {
			device_printf(dev, "cannot initialize adapter\n");
		    	goto fail;
		}
	}

	if (bootverbose) {
		if (sc->sc_flags & CREATOR_PAC1)
			device_printf(dev,
			    "BT9068/PAC1 RAMDAC (%s cursor control)\n",
			    sc->sc_flags & CREATOR_CURINV ? "inverted" :
			    "normal");
		else
			device_printf(dev, "BT498/PAC2 RAMDAC\n");
	}
	device_printf(dev, "resolution %dx%d\n", sc->sc_width, sc->sc_height);

	unit = device_get_unit(dev);
	sc->sc_si = make_dev(&creator_devsw, unit, UID_ROOT, GID_WHEEL,
	    0600, "fb%d", unit);
	sc->sc_si->si_drv1 = sc;

	EVENTHANDLER_REGISTER(shutdown_final, creator_shutdown, sc,
	    SHUTDOWN_PRI_DEFAULT);

	return (0);

 fail:
	for (i = 0; i < FFB_NREG; i++)
		if (sc->sc_reg[i] != NULL)
			bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rid[i],
			    sc->sc_reg[i]);
 	return (error);
}

static int
creator_open(struct cdev *dev, int flags, int mode, struct thread *td)
{

	return (0);
}

static int
creator_close(struct cdev *dev, int flags, int mode, struct thread *td)
{

	return (0);
}

static int
creator_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flags,
    struct thread *td)
{
	struct creator_softc *sc;

	sc = dev->si_drv1;
	return ((*vidsw[sc->sc_va.va_index]->ioctl)(&sc->sc_va, cmd, data));
}

static int
creator_mmap(struct cdev *dev, vm_offset_t offset, vm_paddr_t *paddr, int prot)
{
	struct creator_softc *sc;
	struct ffb_map *fm;

	sc = dev->si_drv1;
	for (fm = ffb_map; fm->fm_size != 0; fm++) {
		if (offset >= fm->fm_virt &&
		    offset < fm->fm_virt + fm->fm_size) {
			*paddr = sc->sc_bh[0] + fm->fm_phys +
			    (offset - fm->fm_virt);
			return (0);
		}
	}
	return (EINVAL);
}

static void
creator_shutdown(void *v)
{
	struct creator_softc *sc = v;

	FFB_WRITE(sc, FFB_DAC, FFB_DAC_TYPE2, FFB_DAC_CUR_CTRL);
	FFB_WRITE(sc, FFB_DAC, FFB_DAC_VALUE2,
	    sc->sc_flags & CREATOR_CURINV ? 0 :
	    FFB_DAC_CUR_CTRL_P0 | FFB_DAC_CUR_CTRL_P1);
}
