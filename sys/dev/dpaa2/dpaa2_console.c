/*-
 * SPDX-License-Identifier: BSD-3-Clause AND BSD-2-Clause
 */
/*
 * // SPDX-License-Identifier: (GPL-2.0+ OR BSD-3-Clause)
 *
 * Freescale DPAA2 Platforms Console Driver
 *
 * Copyright 2015-2016 Freescale Semiconductor Inc.
 * Copyright 2018 NXP
 *
 * git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/drivers/soc/fsl/dpaa2-console.c#8120bd469f5525da229953c1197f2b826c0109f4
 */
/*
 * Copyright (c) 2022-2023 Bjoern A. Zeeb
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
 * Some docs are in:
 * - https://www.nxp.com.cn/docs/en/application-note/AN13329.pdf
 * - DPAA2UM
 * - git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/drivers/soc/fsl/dpaa2-console.c
 */

#include "opt_platform.h"
#ifdef __notyet__
#include "opt_acpi.h"
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

/* Table 6-1. MC Memory Map */
#define	MC_REG_MCFBA			0x20
#define	MC_REG_MCFBALR_OFF		0x00
#define	MC_REG_MCFBALR_MASK		0xe0000000
#define	MC_REG_MCFBAHR_OFF		0x04
#define	MC_REG_MCFBAHR_MASK		0x0001ffff

/* Firmware Consoles. */
#define	MC_BUFFER_OFFSET		0x01000000
#define	MC_BUFFER_SIZE			(1024 * 1024 * 16)
#define	MC_OFFSET_DELTA			MC_BUFFER_OFFSET

#define	AIOP_BUFFER_OFFSET		0x06000000
#define	AIOP_BUFFER_SIZE		(1024 * 1024 * 16)
#define	AIOP_OFFSET_DELTA		0

/* MC and AIOP Magic words */
#define	MAGIC_MC			0x4d430100
#define	MAGIC_AIOP			0X41494f50

#define	LOG_HEADER_FLAG_BUFFER_WRAPAROUND				\
    0x80000000
#define	LAST_BYTE(a)							\
    ((a) & ~(LOG_HEADER_FLAG_BUFFER_WRAPAROUND))

struct dpaa2_cons_dev {
	struct cdev			*cdev;
	struct mtx			mtx;
	size_t				offset;
	uint32_t			magic;

	uint32_t			hdr_magic;
	uint32_t			hdr_eobyte;
	uint32_t			hdr_start;
	uint32_t			hdr_len;

	uoff_t				start;
	uoff_t				end;
	uoff_t				eod;
	uoff_t				cur;

	bus_space_tag_t			bst;
	bus_space_handle_t		bsh;
	vm_size_t			size;
};

struct dpaa2_cons_softc {
	struct resource			*res;
	bus_space_tag_t			bst;
	uint64_t			mcfba;
	struct dpaa2_cons_dev		mc_cd;
	struct dpaa2_cons_dev		aiop_cd;
};

struct dpaa2_cons_hdr {
	uint32_t	magic;
	uint32_t	_reserved;
	uint32_t	start;
	uint32_t	len;
	uint32_t	eobyte;
};

#define	DPAA2_MC_READ_4(_sc, _r)	bus_read_4((_sc)->res, (_r))

/* Management interface */
static d_open_t				dpaa2_cons_open;
static d_close_t			dpaa2_cons_close;
static d_read_t				dpaa2_cons_read;

static struct cdevsw dpaa2_mc_cons_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_open =	dpaa2_cons_open,
	.d_close =	dpaa2_cons_close,
	.d_read =	dpaa2_cons_read,
	.d_name =	"fsl_mc_console",
};

static struct cdevsw dpaa2_aiop_cons_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_open =	dpaa2_cons_open,
	.d_close =	dpaa2_cons_close,
	.d_read =	dpaa2_cons_read,
	.d_name =	"fsl_aiop_console",
};

static size_t
dpaa2_cons_read_bs(struct dpaa2_cons_dev *cd, size_t offset, void *dst, size_t len)
{
	size_t count, l;
	uint8_t *p;

	count = 0;
	p = dst;
	l = offset % 8;
	if (l != 0) {
		bus_space_read_region_1(cd->bst, cd->bsh, offset + count, p + count, l);
		count += l;
		len -= l;
	}

	l = len / 8;
	if (l != 0) {
		bus_space_read_region_8(cd->bst, cd->bsh, offset + count, (uint64_t *)(p + count), l);
		l *= 8;
		count += l;
		len -= l;
	}
	l = len / 4;
	if (l != 0) {
		bus_space_read_region_4(cd->bst, cd->bsh, offset + count, (uint32_t *)(p + count), l);
		l *= 4;
		count += l;
		len -= l;
	}
	l = len / 2;
	if (l != 0) {
		bus_space_read_region_2(cd->bst, cd->bsh, offset + count, (uint16_t *)(p + count), l);
		l *= 2;
		count += l;
		len -= l;
	}
	if (len != 0) {
		bus_space_read_region_1(cd->bst, cd->bsh, offset + count, p + count, len);
		count += len;
	}

	return (count);
}

static int
dpaa2_cons_open(struct cdev *cdev, int flags, int fmt, struct thread *td)
{
	struct dpaa2_cons_dev *cd;
	struct dpaa2_cons_hdr hdr;
	size_t rlen __diagused;
	uint32_t wrapped;

	if (flags & FWRITE)
		return (EACCES);

	cd = cdev->si_drv1;
	if (cd->size == 0)
		return (ENODEV);

	mtx_lock(&cd->mtx);
	rlen = dpaa2_cons_read_bs(cd, 0, &hdr, sizeof(hdr));
	KASSERT(rlen == sizeof(hdr), ("%s:%d: rlen %zu != count %zu, cdev %p "
	    "cd %p\n", __func__, __LINE__, rlen, sizeof(hdr), cdev, cd));

	cd->hdr_magic = hdr.magic;
	if (cd->hdr_magic != cd->magic) {
		mtx_unlock(&cd->mtx);
		return (ENODEV);
	}

	cd->hdr_eobyte = hdr.eobyte;
	cd->hdr_start = hdr.start;
	cd->hdr_len = hdr.len;

	cd->start = cd->hdr_start - cd->offset;
	cd->end = cd->start + cd->hdr_len;

	wrapped = cd->hdr_eobyte & LOG_HEADER_FLAG_BUFFER_WRAPAROUND;
	cd->eod = cd->start + LAST_BYTE(cd->hdr_eobyte);

	if (wrapped && cd->eod != cd->end)
		cd->cur = cd->eod + 1;
	else
		cd->cur = cd->start;
	mtx_unlock(&cd->mtx);

	return (0);
}

static int
dpaa2_cons_close(struct cdev *cdev, int flags, int fmt, struct thread *td)
{
	struct dpaa2_cons_dev *cd;

	cd = cdev->si_drv1;
	mtx_lock(&cd->mtx);
	cd->hdr_magic = cd->hdr_eobyte = cd->hdr_start = cd->hdr_len = 0;
	cd->start = cd->end = 0;
	mtx_unlock(&cd->mtx);

	return (0);
}

static int
dpaa2_cons_read(struct cdev *cdev, struct uio *uio, int flag)
{
        char buf[128];
	struct dpaa2_cons_dev *cd;
	size_t len, size, count;
	size_t rlen __diagused;
	int error;

	cd = cdev->si_drv1;
	mtx_lock(&cd->mtx);
	if (cd->cur == cd->eod) {
		mtx_unlock(&cd->mtx);
		return (0);
	} else if (cd->cur > cd->eod) {
		len = (cd->end - cd->cur) + (cd->eod - cd->start);
	} else {
		len = cd->eod - cd->cur;
	}
	size = cd->end - cd->cur;

	if (len > size) {
		/* dump cur [size] */
		while (uio->uio_resid > 0) {
			count = imin(sizeof(buf), uio->uio_resid);
			if (count > size)
				count = size;

			rlen = dpaa2_cons_read_bs(cd, cd->cur, buf, count);
			KASSERT(rlen == count, ("%s:%d: rlen %zu != count %zu, "
			    "cdev %p cd %p\n", __func__, __LINE__, rlen, count,
			    cdev, cd));

			cd->cur += count;
			len -= count;
			size -= count;
			mtx_unlock(&cd->mtx);
			error = uiomove(buf, count, uio);
			if (error != 0 || uio->uio_resid == 0)
				return (error);
			mtx_lock(&cd->mtx);
		}
		cd->cur = cd->start;
	}

	/* dump cur [len] */
	while (len > 0 && uio->uio_resid > 0) {
		count = imin(sizeof(buf), uio->uio_resid);
		if (count > len)
			count = len;

		rlen = dpaa2_cons_read_bs(cd, cd->cur, buf, count);
		KASSERT(rlen == count, ("%s:%d: rlen %zu != count %zu, cdev %p "
		    "cd %p\n", __func__, __LINE__, rlen, count, cdev, cd));

		cd->cur += count;
		len -= count;
		mtx_unlock(&cd->mtx);
		error = uiomove(buf, count, uio);
		if (error != 0 || uio->uio_resid == 0)
			return (error);
		mtx_lock(&cd->mtx);
	}
	mtx_unlock(&cd->mtx);
	return (0);
}

static int
dpaa2_cons_create_dev(device_t dev, bus_addr_t pa, size_t size,
    size_t offset, uint32_t magic, struct cdevsw *devsw,
    struct dpaa2_cons_dev *cd)
{
	struct dpaa2_cons_softc *sc;
	struct dpaa2_cons_hdr hdr;
	struct make_dev_args md_args;
	size_t len;
	int error;

	sc = device_get_softc(dev);

	error = bus_space_map(sc->bst, pa, size, 0, &cd->bsh);
	if (error != 0) {
		device_printf(dev, "%s: failed to map bus space %#jx/%zu: %d\n",
		    __func__, (uintmax_t)pa, size, error);
		return (error);
	}

	cd->bst = sc->bst;
	cd->size = size;

	len = dpaa2_cons_read_bs(cd, 0, &hdr, sizeof(hdr));
	if (len != sizeof(hdr)) {
		device_printf(dev, "%s: failed to read hdr for %#jx/%zu: %d\n",
		    __func__, (uintmax_t)pa, size, error);
		bus_space_unmap(cd->bst, cd->bsh, cd->size);
		cd->size = 0;
		return (EIO);
	}

	/* This checks probably needs to be removed one day? */
	if (hdr.magic != magic) {
		if (bootverbose)
			device_printf(dev, "%s: magic wrong for %#jx/%zu: "
			    "%#010x != %#010x, no console?\n", __func__,
			    (uintmax_t)pa, size, hdr.magic, magic);
		bus_space_unmap(cd->bst, cd->bsh, cd->size);
		cd->size = 0;
		return (ENOENT);
	}

	if (hdr.start - offset > size) {
		device_printf(dev, "%s: range wrong for %#jx/%zu: %u - %zu > %zu\n",
		    __func__, (uintmax_t)pa, size, hdr.start, offset, size);
		bus_space_unmap(cd->bst, cd->bsh, cd->size);
		cd->size = 0;
		return (ERANGE);
	}

	cd->offset = offset;
	cd->magic = magic;
	mtx_init(&cd->mtx, "dpaa2 cons", NULL, MTX_DEF);

	make_dev_args_init(&md_args);
	md_args.mda_devsw = devsw;
	md_args.mda_uid = 0;
	md_args.mda_gid = 69;
	md_args.mda_mode = S_IRUSR | S_IRGRP;
	md_args.mda_unit = 0;
	md_args.mda_si_drv1 = cd;
	error = make_dev_s(&md_args, &cd->cdev, "%s", devsw->d_name);
	if (error != 0) {
		device_printf(dev, "%s: make_dev_s failed for %#jx/%zu: %d\n",
		    __func__, (uintmax_t)pa, size, error);
		mtx_destroy(&cd->mtx);
		bus_space_unmap(cd->bst, cd->bsh, cd->size);
		cd->size = 0;
		return (error);
	}

	if (bootverbose)
		device_printf(dev, "dpaa2 console %s at pa %#jx size %#010zx "
		    "(%zu) offset %zu, hdr: magic %#010x start %#010x len "
		    "%#010x eobyte %#010x\n", devsw->d_name, (uintmax_t)pa,
		    size, size, offset, hdr.magic, hdr.start, hdr.len, hdr.eobyte);

	return (0);
}

static int
dpaa2_cons_attach_common(device_t dev)
{
	struct dpaa2_cons_softc *sc;

	sc = device_get_softc(dev);

	dpaa2_cons_create_dev(dev, (bus_addr_t)(sc->mcfba + MC_BUFFER_OFFSET),
	    MC_BUFFER_SIZE, MC_OFFSET_DELTA, MAGIC_MC,
	    &dpaa2_mc_cons_cdevsw, &sc->mc_cd);

	dpaa2_cons_create_dev(dev, (bus_addr_t)(sc->mcfba + AIOP_BUFFER_OFFSET),
	    AIOP_BUFFER_SIZE, AIOP_OFFSET_DELTA, MAGIC_AIOP,
	    &dpaa2_aiop_cons_cdevsw, &sc->aiop_cd);

	return (0);
}

static void
dpaa2_cons_detach_common(struct dpaa2_cons_dev *cd)
{

	bus_space_unmap(cd->bst, cd->bsh, cd->size);
	mtx_destroy(&cd->mtx);
}

static int
dpaa2_cons_detach(device_t dev)
{
	struct dpaa2_cons_softc *sc;

	sc = device_get_softc(dev);

	if (sc->aiop_cd.cdev != NULL)
		destroy_dev(sc->aiop_cd.cdev);
	if (sc->mc_cd.cdev != NULL)
		destroy_dev(sc->mc_cd.cdev);

	if (sc->aiop_cd.size != 0)
		dpaa2_cons_detach_common(&sc->aiop_cd);
	if (sc->mc_cd.size != 0)
		dpaa2_cons_detach_common(&sc->mc_cd);

	if (sc->res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, rman_get_rid(sc->res),
		    sc->res);

	return (0);
}

#ifdef __notyet__
#ifdef ACPI
static int
dpaa2_cons_acpi_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	device_set_desc(dev, "DPAA2 Console Driver");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_cons_acpi_attach(device_t dev)
{
	struct dpaa2_cons_softc *sc;
	uint32_t val;
	int error, rid;

	sc = device_get_softc(dev);

	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "Could not allocate memory\n");
		return (ENXIO);
	}
	sc->bst = rman_get_bustag(sc->res);

	val = DPAA2_MC_READ_4(sc, MC_REG_MCFBALR_OFF);
	val &= MC_REG_MCFBALR_MASK;
	sc->mcfba |= val;
	val = DPAA2_MC_READ_4(sc, MC_REG_MCFBAHR_OFF);
	val &= MC_REG_MCFBAHR_MASK;
	sc->mcfba |= ((uint64_t)val << 32);

	error = dpaa2_cons_attach_common(dev);

	return (error);
}

static device_method_t dpaa2_cons_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_cons_acpi_probe),
	DEVMETHOD(device_attach,	dpaa2_cons_acpi_attach),
	DEVMETHOD(device_detach,	dpaa2_cons_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(dpaa2_cons_acpi, dpaa2_cons_acpi_driver, dpaa2_cons_acpi_methods,
    sizeof(struct dpaa2_cons_softc));

DRIVER_MODULE(dpaa2_cons_acpi, simplebus, dpaa2_cons_acpi_driver, 0, 0);
MODULE_DEPEND(dpaa2_cons_acpi, dpaa2_mc, 1, 1, 1);
#endif
#endif /* 0 */

#ifdef FDT
static struct ofw_compat_data compat_data[] = {
	{ "fsl,dpaa2-console",		1 },
	{ NULL,				0 }
};

static int
dpaa2_cons_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "DPAA2 Console Driver");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_cons_fdt_attach(device_t dev)
{
	struct dpaa2_cons_softc *sc;
	uint32_t val;
	int error, rid;

	sc = device_get_softc(dev);

	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "Could not allocate memory\n");
		return (ENXIO);
	}
	sc->bst = rman_get_bustag(sc->res);

	val = DPAA2_MC_READ_4(sc, MC_REG_MCFBALR_OFF);
	val &= MC_REG_MCFBALR_MASK;
	sc->mcfba |= val;
	val = DPAA2_MC_READ_4(sc, MC_REG_MCFBAHR_OFF);
	val &= MC_REG_MCFBAHR_MASK;
	sc->mcfba |= ((uint64_t)val << 32);

	error = dpaa2_cons_attach_common(dev);

	return (error);
}

static device_method_t dpaa2_cons_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_cons_fdt_probe),
	DEVMETHOD(device_attach,	dpaa2_cons_fdt_attach),
	DEVMETHOD(device_detach,	dpaa2_cons_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(dpaa2_cons_fdt, dpaa2_cons_fdt_driver, dpaa2_cons_fdt_methods,
    sizeof(struct dpaa2_cons_softc));

DRIVER_MODULE(dpaa2_cons_fdt, simplebus, dpaa2_cons_fdt_driver, 0, 0);
#endif
