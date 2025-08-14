/*-
 * Copyright (c) 2017 Ilya Bakulin.  All rights reserved.
 * Copyright (c) 2018-2019 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
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
 *
 * Portions of this software may have been developed with reference to
 * the SD Simplified Specification.  The following disclaimer may apply:
 *
 * The following conditions apply to the release of the simplified
 * specification ("Simplified Specification") by the SD Card Association and
 * the SD Group. The Simplified Specification is a subset of the complete SD
 * Specification which is owned by the SD Card Association and the SD
 * Group. This Simplified Specification is provided on a non-confidential
 * basis subject to the disclaimers below. Any implementation of the
 * Simplified Specification may require a license from the SD Card
 * Association, SD Group, SD-3C LLC or other third parties.
 *
 * Disclaimers:
 *
 * The information contained in the Simplified Specification is presented only
 * as a standard specification for SD Cards and SD Host/Ancillary products and
 * is provided "AS-IS" without any representations or warranties of any
 * kind. No responsibility is assumed by the SD Group, SD-3C LLC or the SD
 * Card Association for any damages, any infringements of patents or other
 * right of the SD Group, SD-3C LLC, the SD Card Association or any third
 * parties, which may result from its use. No license is granted by
 * implication, estoppel or otherwise under any patent or other rights of the
 * SD Group, SD-3C LLC, the SD Card Association or any third party. Nothing
 * herein shall be construed as an obligation by the SD Group, the SD-3C LLC
 * or the SD Card Association to disclose or distribute any technical
 * information, know-how or other confidential information to any third party.
 */
/*
 * Implements the (kernel specific) SDIO parts.
 * This will hide all cam(4) functionality from the SDIO driver implementations
 * which will just be newbus/device(9) and hence look like any other driver for,
 * e.g., PCI.
 * The sdiob(4) parts effetively "translate" between the two worlds "bridging"
 * messages from MMCCAM to newbus and back.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_queue.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_internal.h> /* for cam_path */
#include <cam/cam_debug.h>

#include <dev/mmc/mmcreg.h>

#include <dev/sdio/sdiob.h>
#include <dev/sdio/sdio_subr.h>

#include "sdio_if.h"

#ifdef DEBUG
#define	DPRINTF(...)		printf(__VA_ARGS__)
#define	DPRINTFDEV(_dev, ...)	device_printf((_dev), __VA_ARGS__)
#else
#define	DPRINTF(...)
#define	DPRINTFDEV(_dev, ...)
#endif

struct sdiob_softc {
	uint32_t			sdio_state;
#define	SDIO_STATE_DEAD			0x0001
#define	SDIO_STATE_INITIALIZING		0x0002
#define	SDIO_STATE_READY		0x0004
	uint32_t			nb_state;
#define	NB_STATE_DEAD			0x0001
#define	NB_STATE_SIM_ADDED		0x0002
#define	NB_STATE_READY			0x0004

	/* CAM side. */
	struct card_info		cardinfo;
	struct cam_periph		*periph;
	union ccb			*ccb;
	struct task			discover_task;

	/* Newbus side. */
	device_t			dev;	/* Ourselves. */
	device_t			child[8];
};

/* -------------------------------------------------------------------------- */
/*
 * SDIO CMD52 and CM53 implementations along with wrapper functions for
 * read/write and a CAM periph helper function.
 * These are the backend implementations of the sdio_if.m framework talking
 * through CAM to sdhci.
 * Note: these functions are also called during early discovery stage when
 * we are not a device(9) yet. Hence they cannot always use device_printf()
 * to log errors and have to call CAM_DEBUG() during these early stages.
 */

static int
sdioerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{

	return (cam_periph_error(ccb, cam_flags, sense_flags));
}

/* CMD52: direct byte access. */
static int
sdiob_rw_direct_sc(struct sdiob_softc *sc, uint8_t fn, uint32_t addr, bool wr,
    uint8_t *val)
{
	uint32_t arg, flags;
	int error;

	KASSERT((val != NULL), ("%s val passed as NULL\n", __func__));

	if (sc->ccb == NULL)
		sc->ccb = xpt_alloc_ccb();
	else
		memset(sc->ccb, 0, sizeof(*sc->ccb));
	xpt_setup_ccb(&sc->ccb->ccb_h, sc->periph->path, CAM_PRIORITY_NORMAL);
	CAM_DEBUG(sc->ccb->ccb_h.path, CAM_DEBUG_TRACE,
	    ("%s(fn=%d, addr=%#02x, wr=%d, *val=%#02x)\n", __func__,
	    fn, addr, wr, *val));

	flags = MMC_RSP_R5 | MMC_CMD_AC;
	arg = SD_IO_RW_FUNC(fn) | SD_IO_RW_ADR(addr);
	if (wr)
		arg |= SD_IO_RW_WR | SD_IO_RW_RAW | SD_IO_RW_DAT(*val);

	cam_fill_mmcio(&sc->ccb->mmcio,
		/*retries*/ 0,
		/*cbfcnp*/ NULL,
		/*flags*/ CAM_DIR_NONE,
		/*mmc_opcode*/ SD_IO_RW_DIRECT,
		/*mmc_arg*/ arg,
		/*mmc_flags*/ flags,
		/*mmc_data*/ 0,
		/*timeout*/ sc->cardinfo.f[fn].timeout);
	error = cam_periph_runccb(sc->ccb, sdioerror, CAM_FLAG_NONE, 0, NULL);
	if (error != 0) {
		if (sc->dev != NULL)
			device_printf(sc->dev,
			    "%s: Failed to %s address %#10x error=%d\n",
			    __func__, (wr) ? "write" : "read", addr, error);
		else
			CAM_DEBUG(sc->ccb->ccb_h.path, CAM_DEBUG_INFO,
			    ("%s: Failed to %s address: %#10x error=%d\n",
			    __func__, (wr) ? "write" : "read", addr, error));
		return (error);
	}

	/* TODO: Add handling of MMC errors */
	/* ccb->mmcio.cmd.error ? */
	if (wr == false)
		*val = sc->ccb->mmcio.cmd.resp[0] & 0xff;

	return (0);
}

static int
sdio_rw_direct(device_t dev, uint8_t fn, uint32_t addr, bool wr,
    uint8_t *val)
{
	struct sdiob_softc *sc;
	int error;

	sc = device_get_softc(dev);
	cam_periph_lock(sc->periph);
	error = sdiob_rw_direct_sc(sc, fn, addr, wr, val);
	cam_periph_unlock(sc->periph);
	return (error);
}

static int
sdiob_read_direct(device_t dev, uint8_t fn, uint32_t addr, uint8_t *val)
{
	int error;
	uint8_t v;

	error = sdio_rw_direct(dev, fn, addr, false, &v);
	/* Be polite and do not touch the value on read error. */
	if (error == 0 && val != NULL)
		*val = v;
	return (error);
}

static int
sdiob_write_direct(device_t dev, uint8_t fn, uint32_t addr, uint8_t val)
{

	return (sdio_rw_direct(dev, fn, addr, true, &val));
}

/*
 * CMD53: IO_RW_EXTENDED, read and write multiple I/O registers.
 * Increment false gets FIFO mode (single register address).
 */
/*
 * A b_count of 0 means byte mode, b_count > 0 gets block mode.
 * A b_count of >= 512 would mean infinitive block transfer, which would become
 * b_count = 0, is not yet supported.
 * For b_count == 0, blksz is the len of bytes, otherwise it is the amount of
 * full sized blocks (you must not round the blocks up and leave the last one
 * partial!)
 * For byte mode, the maximum of blksz is the functions cur_blksize.
 * This function should ever only be called by sdio_rw_extended_sc()! 
 */
static int
sdiob_rw_extended_cam(struct sdiob_softc *sc, uint8_t fn, uint32_t addr,
    bool wr, uint8_t *buffer, bool incaddr, uint32_t b_count, uint16_t blksz)
{
	struct mmc_data mmcd;
	uint32_t arg, cam_flags, flags, len;
	int error;

	if (sc->ccb == NULL)
		sc->ccb = xpt_alloc_ccb();
	else
		memset(sc->ccb, 0, sizeof(*sc->ccb));
	xpt_setup_ccb(&sc->ccb->ccb_h, sc->periph->path, CAM_PRIORITY_NORMAL);
	CAM_DEBUG(sc->ccb->ccb_h.path, CAM_DEBUG_TRACE,
	    ("%s(fn=%d addr=%#0x wr=%d b_count=%u blksz=%u buf=%p incr=%d)\n",
	    __func__, fn, addr, wr, b_count, blksz, buffer, incaddr));

	KASSERT((b_count <= 511), ("%s: infinitive block transfer not yet "
	    "supported: b_count %u blksz %u, sc %p, fn %u, addr %#10x, %s, "
	    "buffer %p, %s\n", __func__, b_count, blksz, sc, fn, addr,
	    wr ? "wr" : "rd", buffer, incaddr ? "incaddr" : "fifo"));
	/* Blksz needs to be within bounds for both byte and block mode! */
	KASSERT((blksz <= sc->cardinfo.f[fn].cur_blksize), ("%s: blksz "
	    "%u > bur_blksize %u, sc %p, fn %u, addr %#10x, %s, "
	    "buffer %p, %s, b_count %u\n", __func__, blksz,
	    sc->cardinfo.f[fn].cur_blksize, sc, fn, addr,
	    wr ? "wr" : "rd", buffer, incaddr ? "incaddr" : "fifo",
	    b_count));
	if (b_count == 0) {
		/* Byte mode */
		len = blksz;
		if (blksz == 512)
			blksz = 0;
		arg = SD_IOE_RW_LEN(blksz);
	} else {
		/* Block mode. */
#ifdef __notyet__
		if (b_count > 511) {
			/* Infinitive block transfer. */
			b_count = 0;
		}
#endif
		len = b_count * blksz;
		arg = SD_IOE_RW_BLK | SD_IOE_RW_LEN(b_count);
	}

	flags = MMC_RSP_R5 | MMC_CMD_ADTC;
	arg |= SD_IOE_RW_FUNC(fn) | SD_IOE_RW_ADR(addr);
	if (incaddr)
		arg |= SD_IOE_RW_INCR;

	memset(&mmcd, 0, sizeof(mmcd));
	mmcd.data = buffer;
	mmcd.len = len;
	if (arg & SD_IOE_RW_BLK) {
		/* XXX both should be known from elsewhere, aren't they? */
		mmcd.block_size = blksz;
		mmcd.block_count = b_count;
	}

	if (wr) {
		arg |= SD_IOE_RW_WR;
		cam_flags = CAM_DIR_OUT;
		mmcd.flags = MMC_DATA_WRITE;
	} else {
		cam_flags = CAM_DIR_IN;
		mmcd.flags = MMC_DATA_READ;
	}
#ifdef __notyet__
	if (b_count == 0) {
		/* XXX-BZ TODO FIXME.  Cancel I/O: CCCR -> ASx */
		/* Stop cmd. */
	}
#endif
	cam_fill_mmcio(&sc->ccb->mmcio,
		/*retries*/ 0,
		/*cbfcnp*/ NULL,
		/*flags*/ cam_flags,
		/*mmc_opcode*/ SD_IO_RW_EXTENDED,
		/*mmc_arg*/ arg,
		/*mmc_flags*/ flags,
		/*mmc_data*/ &mmcd,
		/*timeout*/ sc->cardinfo.f[fn].timeout);
	if (arg & SD_IOE_RW_BLK) {
		mmcd.flags |= MMC_DATA_BLOCK_SIZE;
		if (b_count != 1)
			sc->ccb->mmcio.cmd.data->flags |= MMC_DATA_MULTI;
	}

	/* Execute. */
	error = cam_periph_runccb(sc->ccb, sdioerror, CAM_FLAG_NONE, 0, NULL);
	if (error != 0) {
		if (sc->dev != NULL)
			device_printf(sc->dev,
			    "%s: Failed to %s address %#10x buffer %p size %u "
			    "%s b_count %u blksz %u error=%d\n",
			    __func__, (wr) ? "write to" : "read from", addr,
			    buffer, len, (incaddr) ? "incr" : "fifo",
			    b_count, blksz, error);
		else
			CAM_DEBUG(sc->ccb->ccb_h.path, CAM_DEBUG_INFO,
			    ("%s: Failed to %s address %#10x buffer %p size %u "
			    "%s b_count %u blksz %u error=%d\n",
			    __func__, (wr) ? "write to" : "read from", addr,
			    buffer, len, (incaddr) ? "incr" : "fifo",
			    b_count, blksz, error));
		return (error);
	}

	/* TODO: Add handling of MMC errors */
	/* ccb->mmcio.cmd.error ? */
	error = sc->ccb->mmcio.cmd.resp[0] & 0xff;
	if (error != 0) {
		if (sc->dev != NULL)
			device_printf(sc->dev,
			    "%s: Failed to %s address %#10x buffer %p size %u "
			    "%s b_count %u blksz %u mmcio resp error=%d\n",
			    __func__, (wr) ? "write to" : "read from", addr,
			    buffer, len, (incaddr) ? "incr" : "fifo",
			    b_count, blksz, error);
		else
			CAM_DEBUG(sc->ccb->ccb_h.path, CAM_DEBUG_INFO,
			    ("%s: Failed to %s address %#10x buffer %p size %u "
			    "%s b_count %u blksz %u mmcio resp error=%d\n",
			    __func__, (wr) ? "write to" : "read from", addr,
			    buffer, len, (incaddr) ? "incr" : "fifo",
			    b_count, blksz, error));
	}
	return (error);
}

static int
sdiob_rw_extended_sc(struct sdiob_softc *sc, uint8_t fn, uint32_t addr,
    bool wr, uint32_t size, uint8_t *buffer, bool incaddr)
{
	int error;
	uint32_t len;
	uint32_t b_count;

	/*
	 * If block mode is supported and we have at least 4 bytes to write and
	 * the size is at least one block, then start doing blk transfers.
	 */
	while (sc->cardinfo.support_multiblk &&
	    size > 4 && size >= sc->cardinfo.f[fn].cur_blksize) {
		b_count = size / sc->cardinfo.f[fn].cur_blksize;
		KASSERT(b_count >= 1, ("%s: block count too small %u size %u "
		    "cur_blksize %u\n", __func__, b_count, size,
		    sc->cardinfo.f[fn].cur_blksize));

#ifdef __notyet__
		/* XXX support inifinite transfer with b_count = 0. */
#else
		if (b_count > 511)
			b_count = 511;
#endif
		len = b_count * sc->cardinfo.f[fn].cur_blksize;
		error = sdiob_rw_extended_cam(sc, fn, addr, wr, buffer, incaddr,
		    b_count, sc->cardinfo.f[fn].cur_blksize);
		if (error != 0)
			return (error);

		size -= len;
		buffer += len;
		if (incaddr)
			addr += len;
	}

	while (size > 0) {
		len = MIN(size, sc->cardinfo.f[fn].cur_blksize);

		error = sdiob_rw_extended_cam(sc, fn, addr, wr, buffer, incaddr,
		    0, len);
		if (error != 0)
			return (error);

		/* Prepare for next iteration. */
		size -= len;
		buffer += len;
		if (incaddr)
			addr += len;
	}

	return (0);
}

static int
sdiob_rw_extended(device_t dev, uint8_t fn, uint32_t addr, bool wr,
    uint32_t size, uint8_t *buffer, bool incaddr)
{
	struct sdiob_softc *sc;
	int error;

	sc = device_get_softc(dev);
	cam_periph_lock(sc->periph);
	error = sdiob_rw_extended_sc(sc, fn, addr, wr, size, buffer, incaddr);
	cam_periph_unlock(sc->periph);
	return (error);
}

static int
sdiob_read_extended(device_t dev, uint8_t fn, uint32_t addr, uint32_t size,
    uint8_t *buffer, bool incaddr)
{

	return (sdiob_rw_extended(dev, fn, addr, false, size, buffer, incaddr));
}

static int
sdiob_write_extended(device_t dev, uint8_t fn, uint32_t addr, uint32_t size,
    uint8_t *buffer, bool incaddr)
{

	return (sdiob_rw_extended(dev, fn, addr, true, size, buffer, incaddr));
}

/* -------------------------------------------------------------------------- */
/* Bus interface, ivars handling. */

static int
sdiob_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct sdiob_softc *sc;
	struct sdio_func *f;

	f = device_get_ivars(child);
	KASSERT(f != NULL, ("%s: dev %p child %p which %d, child ivars NULL\n",
	    __func__, dev, child, which));

	switch (which) {
	case SDIOB_IVAR_SUPPORT_MULTIBLK:
		sc = device_get_softc(dev);
		KASSERT(sc != NULL, ("%s: dev %p child %p which %d, sc NULL\n",
		    __func__, dev, child, which));
		*result = sc->cardinfo.support_multiblk;
		break;
	case SDIOB_IVAR_FUNCTION:
		*result = (uintptr_t)f;
		break;
	case SDIOB_IVAR_FUNCNUM:
		*result = f->fn;
		break;
	case SDIOB_IVAR_CLASS:
		*result = f->class;
		break;
	case SDIOB_IVAR_VENDOR:
		*result = f->vendor;
		break;
	case SDIOB_IVAR_DEVICE:
		*result = f->device;
		break;
	case SDIOB_IVAR_DRVDATA:
		*result = f->drvdata;
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

static int
sdiob_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct sdio_func *f;

	f = device_get_ivars(child);
	KASSERT(f != NULL, ("%s: dev %p child %p which %d, child ivars NULL\n",
	    __func__, dev, child, which));

	switch (which) {
	case SDIOB_IVAR_SUPPORT_MULTIBLK:
	case SDIOB_IVAR_FUNCTION:
	case SDIOB_IVAR_FUNCNUM:
	case SDIOB_IVAR_CLASS:
	case SDIOB_IVAR_VENDOR:
	case SDIOB_IVAR_DEVICE:
		return (EINVAL);	/* Disallowed. */
	case SDIOB_IVAR_DRVDATA:
		f->drvdata = value;
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

/* -------------------------------------------------------------------------- */
/*
 * Newbus functions for ourselves to probe/attach/detach and become a proper
 * device(9).  Attach will also probe for child devices (another driver
 * implementing SDIO).
 */

static int
sdiob_probe(device_t dev)
{

	device_set_desc(dev, "SDIO CAM-Newbus bridge");
	return (BUS_PROBE_DEFAULT);
}

static int
sdiob_attach(device_t dev)
{
	struct sdiob_softc *sc;
	int error, i;

	sc = device_get_softc(dev);
	if (sc == NULL)
		return (ENXIO);

	/*
	 * Now that we are a dev, create one child device per function,
	 * initialize the backpointer, so we can pass them around and
	 * call CAM operations on the parent, and also set the function
	 * itself as ivars, so that we can query/update them.
	 * Do this before any child gets a chance to attach.
	 */
	for (i = 0; i < sc->cardinfo.num_funcs; i++) {
		sc->child[i] = device_add_child(dev, NULL, DEVICE_UNIT_ANY);
		if (sc->child[i] == NULL) {
			device_printf(dev, "%s: failed to add child\n", __func__);
			return (ENXIO);
		}
		sc->cardinfo.f[i].dev = sc->child[i];

		/* Set the function as ivar to the child device. */
		device_set_ivars(sc->child[i], &sc->cardinfo.f[i]);
	}

	/*
	 * No one will ever attach to F0; we do the above to have a "device"
	 * to talk to in a general way in the code.
	 * Also do the probe/attach in a 2nd loop, so that all devices are
	 * present as we do have drivers consuming more than one device/func
	 * and might play "tricks" in order to do that assuming devices and
	 * ivars are available for all.
	 */
	for (i = 1; i < sc->cardinfo.num_funcs; i++) {
		error = device_probe_and_attach(sc->child[i]);
		if (error != 0 && bootverbose)
			device_printf(dev, "%s: device_probe_and_attach(%p %s) "
			    "failed %d for function %d, no child yet\n",
			     __func__,
			     sc->child, device_get_nameunit(sc->child[i]),
			     error, i);
	}

	sc->nb_state = NB_STATE_READY;

	cam_periph_lock(sc->periph);
	xpt_announce_periph(sc->periph, NULL);
	cam_periph_unlock(sc->periph);

	return (0);
}

static int
sdiob_detach(device_t dev)
{

	/* XXX TODO? */
	return (EOPNOTSUPP);
}

/* -------------------------------------------------------------------------- */
/*
 * driver(9) and device(9) "control plane".
 * This is what we use when we are making ourselves a device(9) in order to
 * provide a newbus interface again, as well as the implementation of the
 * SDIO interface.
 */

static device_method_t sdiob_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		sdiob_probe),
	DEVMETHOD(device_attach,	sdiob_attach),
	DEVMETHOD(device_detach,	sdiob_detach),

	/* Bus interface. */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_read_ivar,	sdiob_read_ivar),
	DEVMETHOD(bus_write_ivar,	sdiob_write_ivar),

	/* SDIO interface. */
	DEVMETHOD(sdio_read_direct,	sdiob_read_direct),
	DEVMETHOD(sdio_write_direct,	sdiob_write_direct),
	DEVMETHOD(sdio_read_extended,	sdiob_read_extended),
	DEVMETHOD(sdio_write_extended,	sdiob_write_extended),

	DEVMETHOD_END
};

static driver_t sdiob_driver = {
	SDIOB_NAME_S,
	sdiob_methods,
	0
};

/* -------------------------------------------------------------------------- */
/*
 * CIS related.
 * Read card and function information and populate the cardinfo structure.
 */

static int
sdio_read_direct_sc(struct sdiob_softc *sc, uint8_t fn, uint32_t addr,
    uint8_t *val)
{
	int error;
	uint8_t v;

	error = sdiob_rw_direct_sc(sc, fn, addr, false, &v);
	if (error == 0 && val != NULL)
		*val = v;
	return (error);
}

static int
sdio_func_read_cis(struct sdiob_softc *sc, uint8_t fn, uint32_t cis_addr)
{
	char cis1_info_buf[256];
	char *cis1_info[4];
	int start, i, count, ret;
	uint32_t addr;
	uint8_t ch, tuple_id, tuple_len, tuple_count, v;

	/* If we encounter any read errors, abort and return. */
#define	ERR_OUT(ret)							\
	if (ret != 0)							\
		goto err;
	ret = 0;
	/* Use to prevent infinite loop in case of parse errors. */
	tuple_count = 0;
	memset(cis1_info_buf, 0, 256);
	do {
		addr = cis_addr;
		ret = sdio_read_direct_sc(sc, 0, addr++, &tuple_id);
		ERR_OUT(ret);
		if (tuple_id == SD_IO_CISTPL_END)
			break;
		if (tuple_id == 0) {
			cis_addr++;
			continue;
		}
		ret = sdio_read_direct_sc(sc, 0, addr++, &tuple_len);
		ERR_OUT(ret);
		if (tuple_len == 0) {
			CAM_DEBUG(sc->ccb->ccb_h.path, CAM_DEBUG_PERIPH,
			    ("%s: parse error: 0-length tuple %#02x\n",
			    __func__, tuple_id));
			return (EIO);
		}

		switch (tuple_id) {
		case SD_IO_CISTPL_VERS_1:
			addr += 2;
			for (count = 0, start = 0, i = 0;
			     (count < 4) && ((i + 4) < 256); i++) {
				ret = sdio_read_direct_sc(sc, 0, addr + i, &ch);
				ERR_OUT(ret);
				DPRINTF("%s: count=%d, start=%d, i=%d, got "
				    "(%#02x)\n", __func__, count, start, i, ch);
				if (ch == 0xff)
					break;
				cis1_info_buf[i] = ch;
				if (ch == 0) {
					cis1_info[count] =
					    cis1_info_buf + start;
					start = i + 1;
					count++;
				}
			}
			DPRINTF("Card info: ");
			for (i=0; i < 4; i++)
				if (cis1_info[i])
					DPRINTF(" %s", cis1_info[i]);
			DPRINTF("\n");
			break;
		case SD_IO_CISTPL_MANFID:
			/* TPLMID_MANF */
			ret = sdio_read_direct_sc(sc, 0, addr++, &v);
			ERR_OUT(ret);
			sc->cardinfo.f[fn].vendor = v;
			ret = sdio_read_direct_sc(sc, 0, addr++, &v);
			ERR_OUT(ret);
			sc->cardinfo.f[fn].vendor |= (v << 8);
			/* TPLMID_CARD */
			ret = sdio_read_direct_sc(sc, 0, addr++, &v);
			ERR_OUT(ret);
			sc->cardinfo.f[fn].device = v;
			ret = sdio_read_direct_sc(sc, 0, addr, &v);
			ERR_OUT(ret);
			sc->cardinfo.f[fn].device |= (v << 8);
			break;
		case SD_IO_CISTPL_FUNCID:
			/* Not sure if we need to parse it? */
			break;
		case SD_IO_CISTPL_FUNCE:
			if (tuple_len < 4) {
				printf("%s: FUNCE is too short: %d\n",
				    __func__, tuple_len);
				break;
			}
			/* TPLFE_TYPE (Extended Data) */
			ret = sdio_read_direct_sc(sc, 0, addr++, &v);
			ERR_OUT(ret);
			if (fn == 0) {
				if (v != 0x00)
					break;
			} else {
				if (v != 0x01)
					break;
				addr += 0x0b;
			}
			ret = sdio_read_direct_sc(sc, 0, addr, &v);
			ERR_OUT(ret);
			sc->cardinfo.f[fn].max_blksize = v;
			ret = sdio_read_direct_sc(sc, 0, addr+1, &v);
			ERR_OUT(ret);
			sc->cardinfo.f[fn].max_blksize |= (v << 8);
			break;
		default:
			CAM_DEBUG(sc->ccb->ccb_h.path, CAM_DEBUG_PERIPH,
			    ("%s: Skipping fn %d tuple %d ID %#02x "
			    "len %#02x\n", __func__, fn, tuple_count,
			    tuple_id, tuple_len));
		}
		if (tuple_len == 0xff) {
			/* Also marks the end of a tuple chain (E1 16.2) */
			/* The tuple is valid, hence this going at the end. */
			break;
		}
		cis_addr += 2 + tuple_len;
		tuple_count++;
	} while (tuple_count < 20);
err:
#undef ERR_OUT
	return (ret);
}

static int
sdio_get_common_cis_addr(struct sdiob_softc *sc, uint32_t *addr)
{
	int error;
	uint32_t a;
	uint8_t val;

	error = sdio_read_direct_sc(sc, 0, SD_IO_CCCR_CISPTR + 0, &val);
	if (error != 0)
		goto err;
	a = val;
	error = sdio_read_direct_sc(sc, 0, SD_IO_CCCR_CISPTR + 1, &val);
	if (error != 0)
		goto err;
	a |= (val << 8);
	error = sdio_read_direct_sc(sc, 0, SD_IO_CCCR_CISPTR + 2, &val);
	if (error != 0)
		goto err;
	a |= (val << 16);

	if (a < SD_IO_CIS_START || a > SD_IO_CIS_START + SD_IO_CIS_SIZE) {
err:
		CAM_DEBUG(sc->ccb->ccb_h.path, CAM_DEBUG_PERIPH,
		    ("%s: bad CIS address: %#04x, error %d\n", __func__, a,
		    error));
	} else if (error == 0 && addr != NULL)
		*addr = a;

	return (error);
}

static int
sdiob_get_card_info(struct sdiob_softc *sc)
{
	struct mmc_params *mmcp;
	uint32_t cis_addr, fbr_addr;
	int fn, error;
	uint8_t fn_max, val;

	error = sdio_get_common_cis_addr(sc, &cis_addr);
	if (error != 0)
		return (-1);

	memset(&sc->cardinfo, 0, sizeof(sc->cardinfo));

	/* F0 must always be present. */
	fn = 0;
	error = sdio_func_read_cis(sc, fn, cis_addr);
	if (error != 0)
		return (error);
	sc->cardinfo.num_funcs++;
	/* Read CCCR Card Capability. */
	error = sdio_read_direct_sc(sc, 0, SD_IO_CCCR_CARDCAP, &val);
	if (error != 0)
		return (error);
	sc->cardinfo.support_multiblk = (val & CCCR_CC_SMB) ? true : false;
	DPRINTF("%s: F%d: Vendor %#04x product %#04x max block size %d bytes "
	    "support_multiblk %s\n",
	    __func__, fn, sc->cardinfo.f[fn].vendor, sc->cardinfo.f[fn].device,
	    sc->cardinfo.f[fn].max_blksize,
	    sc->cardinfo.support_multiblk ? "yes" : "no");

	/* mmcp->sdio_func_count contains the number of functions w/o F0. */
	mmcp = &sc->ccb->ccb_h.path->device->mmc_ident_data;
	fn_max = MIN(mmcp->sdio_func_count + 1, nitems(sc->cardinfo.f));
	for (fn = 1; fn < fn_max; fn++) {
		fbr_addr = SD_IO_FBR_START * fn + SD_IO_FBR_CIS_OFFSET;

		error = sdio_read_direct_sc(sc, 0, fbr_addr++, &val);
		if (error != 0)
			break;
		cis_addr = val;
		error = sdio_read_direct_sc(sc, 0, fbr_addr++, &val);
		if (error != 0)
			break;
		cis_addr |= (val << 8);
		error = sdio_read_direct_sc(sc, 0, fbr_addr++, &val);
		if (error != 0)
			break;
		cis_addr |= (val << 16);

		error = sdio_func_read_cis(sc, fn, cis_addr);
		if (error != 0)
			break;

		/* Read the Standard SDIO Function Interface Code. */
		fbr_addr = SD_IO_FBR_START * fn;
		error = sdio_read_direct_sc(sc, 0, fbr_addr++, &val);
		if (error != 0)
			break;
		sc->cardinfo.f[fn].class = (val & 0x0f);
		if (sc->cardinfo.f[fn].class == 0x0f) {
			error = sdio_read_direct_sc(sc, 0, fbr_addr, &val);
			if (error != 0)
				break;
			sc->cardinfo.f[fn].class = val;
		}

		sc->cardinfo.f[fn].fn = fn;
		sc->cardinfo.f[fn].cur_blksize = sc->cardinfo.f[fn].max_blksize;
		sc->cardinfo.f[fn].retries = 0;
		sc->cardinfo.f[fn].timeout = 5000;

		DPRINTF("%s: F%d: Class %d Vendor %#04x product %#04x "
		    "max_blksize %d bytes\n", __func__, fn,
		    sc->cardinfo.f[fn].class,
		    sc->cardinfo.f[fn].vendor, sc->cardinfo.f[fn].device,
		    sc->cardinfo.f[fn].max_blksize);
		if (sc->cardinfo.f[fn].vendor == 0) {
			DPRINTF("%s: F%d doesn't exist\n", __func__, fn);
			break;
		}
		sc->cardinfo.num_funcs++;
	}
	return (error);
}

/* -------------------------------------------------------------------------- */
/*
 * CAM periph registration, allocation, and detached from that a discovery
 * task, which goes off reads cardinfo, and then adds ourselves to our SIM's
 * device adding the devclass and registering the driver.  This keeps the
 * newbus chain connected though we will talk CAM in the middle (until one
 * day CAM might be newbusyfied).
 */

static int
sdio_newbus_sim_add(struct sdiob_softc *sc)
{
	device_t pdev;
	devclass_t bus_devclass;
	int error;

	/* Add ourselves to our parent (SIM) device. */

	/* Add ourselves to our parent. That way we can become a parent. */
	pdev = xpt_path_sim_device(sc->periph->path);
	KASSERT(pdev != NULL,
	    ("%s: pdev is NULL, sc %p periph %p sim %p\n",
	    __func__, sc, sc->periph, sc->periph->sim));

	if (sc->dev == NULL)
		sc->dev = BUS_ADD_CHILD(pdev, 0, SDIOB_NAME_S, DEVICE_UNIT_ANY);
	if (sc->dev == NULL)
		return (ENXIO);
	device_set_softc(sc->dev, sc);

	/*
	 * Don't set description here; devclass_add_driver() ->
	 * device_probe_child() -> device_set_driver() will nuke it again.
	 */
	bus_devclass = device_get_devclass(pdev);
	if (bus_devclass == NULL) {
		printf("%s: Failed to get devclass from %s.\n", __func__,
		    device_get_nameunit(pdev));
		return (ENXIO);
	}

	bus_topo_lock();
	error = devclass_add_driver(bus_devclass, &sdiob_driver,
	    BUS_PASS_DEFAULT, NULL);
	bus_topo_unlock();
	if (error != 0) {
		printf("%s: Failed to add driver to devclass: %d.\n",
		    __func__, error);
		return (error);
	}

	/* Done. */
	sc->nb_state = NB_STATE_SIM_ADDED;

	return (0);
}

static void
sdiobdiscover(void *context, int pending)
{
	struct cam_periph *periph;
	struct sdiob_softc *sc;
	int error;

	KASSERT(context != NULL, ("%s: context is NULL\n", __func__));
	periph = (struct cam_periph *)context;
	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s\n", __func__));

	/* Periph was held for us when this task was enqueued. */
	if ((periph->flags & CAM_PERIPH_INVALID) != 0) {
		cam_periph_release(periph);
		return;
	}

	sc = periph->softc;
	sc->sdio_state = SDIO_STATE_INITIALIZING;

	if (sc->ccb == NULL)
		sc->ccb = xpt_alloc_ccb();

	/*
	 * Read CCCR and FBR of each function, get manufacturer and device IDs,
	 * max block size, and whatever else we deem necessary.
	 */
	cam_periph_lock(periph);
	error = sdiob_get_card_info(sc);
	if  (error == 0)
		sc->sdio_state = SDIO_STATE_READY;
	else
		sc->sdio_state = SDIO_STATE_DEAD;
	cam_periph_unlock(periph);

	if (error)
		return;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s: num_func %d\n",
	    __func__, sc->cardinfo.num_funcs));

	/*
	 * Now CAM portion of the driver has been initialized and
	 * we know VID/PID of all the functions on the card.
	 * Time to hook into the newbus.
	 */
	error = sdio_newbus_sim_add(sc);
	if (error != 0)
		sc->nb_state = NB_STATE_DEAD;

	return;
}

/* Called at the end of cam_periph_alloc() for us to finish allocation. */
static cam_status
sdiobregister(struct cam_periph *periph, void *arg)
{
	struct sdiob_softc *sc;
	int error;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s: arg %p\n", __func__, arg));
	if (arg == NULL) {
		printf("%s: no getdev CCB, can't register device pariph %p\n",
		    __func__, periph);
		return(CAM_REQ_CMP_ERR);
	}
	if (xpt_path_sim_device(periph->path) == NULL) {
		printf("%s: no device_t for sim %p\n", __func__, periph->sim);
		return(CAM_REQ_CMP_ERR);
	}

	sc = (struct sdiob_softc *) malloc(sizeof(*sc), M_DEVBUF,
	    M_NOWAIT|M_ZERO);
	if (sc == NULL) {
		printf("%s: unable to allocate sc\n", __func__);
		return (CAM_REQ_CMP_ERR);
	}
	sc->sdio_state = SDIO_STATE_DEAD;
	sc->nb_state = NB_STATE_DEAD;
	TASK_INIT(&sc->discover_task, 0, sdiobdiscover, periph);

	/* Refcount until we are setup.  Can't block. */
	error = cam_periph_hold(periph, PRIBIO);
	if (error != 0) {
		printf("%s: lost periph during registration!\n", __func__);
		free(sc, M_DEVBUF);
		return(CAM_REQ_CMP_ERR);
	}
	periph->softc = sc;
	sc->periph = periph;
	cam_periph_unlock(periph);

	error = taskqueue_enqueue(taskqueue_thread, &sc->discover_task);

	cam_periph_lock(periph);
	/* We will continue to hold a refcount for discover_task. */
	/* cam_periph_unhold(periph); */

	xpt_schedule(periph, CAM_PRIORITY_XPT);

	return (CAM_REQ_CMP);
}

static void
sdioboninvalidate(struct cam_periph *periph)
{

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s:\n", __func__));

	return;
}

static void
sdiobcleanup(struct cam_periph *periph)
{

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s:\n", __func__));

	return;
}

static void
sdiobstart(struct cam_periph *periph, union ccb *ccb)
{

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("%s: ccb %p\n", __func__, ccb));

	return;
}

static void
sdiobasync(void *softc, uint32_t code, struct cam_path *path, void *arg)
{
	struct cam_periph *periph;
	struct ccb_getdev *cgd;
	cam_status status;

	periph = (struct cam_periph *)softc;

	CAM_DEBUG(path, CAM_DEBUG_TRACE, ("%s(code=%d)\n", __func__, code));
	switch (code) {
	case AC_FOUND_DEVICE:
		if (arg == NULL)
			break;
		cgd = (struct ccb_getdev *)arg;
		if (cgd->protocol != PROTO_MMCSD)
			break;

		/* We do not support SD memory (Combo) Cards. */
		if ((path->device->mmc_ident_data.card_features &
		    CARD_FEATURE_MEMORY)) {
			CAM_DEBUG(path, CAM_DEBUG_TRACE,
			     ("Memory card, not interested\n"));
			break;
		}

		/*
		 * Allocate a peripheral instance for this device which starts
		 * the probe process.
		 */
		status = cam_periph_alloc(sdiobregister, sdioboninvalidate,
		    sdiobcleanup, sdiobstart, SDIOB_NAME_S, CAM_PERIPH_BIO, path,
		    sdiobasync, AC_FOUND_DEVICE, cgd);
		if (status != CAM_REQ_CMP && status != CAM_REQ_INPROG)
			CAM_DEBUG(path, CAM_DEBUG_PERIPH,
			     ("%s: Unable to attach to new device due to "
			     "status %#02x\n", __func__, status));
		break;
	default:
		CAM_DEBUG(path, CAM_DEBUG_PERIPH,
		     ("%s: cannot handle async code %#02x\n", __func__, code));
		cam_periph_async(periph, code, path, arg);
		break;
	}
}

static void
sdiobinit(void)
{
	cam_status status;

	/*
	 * Register for new device notification.  We will be notified for all
	 * already existing ones.
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, sdiobasync, NULL, NULL);
	if (status != CAM_REQ_CMP)
		printf("%s: Failed to attach async callback, statux %#02x",
		    __func__, status);
}

/* This function will allow unloading the KLD. */
static int
sdiobdeinit(void)
{

	return (EOPNOTSUPP);
}

static struct periph_driver sdiobdriver =
{
	.init =		sdiobinit,
	.driver_name =	SDIOB_NAME_S,
	.units =	TAILQ_HEAD_INITIALIZER(sdiobdriver.units),
	.generation =	0,
	.flags =	0,
	.deinit =	sdiobdeinit,
};

PERIPHDRIVER_DECLARE(SDIOB_NAME, sdiobdriver);
MODULE_VERSION(SDIOB_NAME, 1);
