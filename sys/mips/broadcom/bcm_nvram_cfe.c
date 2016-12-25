/*-
 * Copyright (c) 2016 Landon Fuller <landonf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * BHND CFE NVRAM driver.
 * 
 * Provides access to device NVRAM via CFE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/bhnd.h>

#include <dev/cfe/cfe_api.h>
#include <dev/cfe/cfe_error.h>
#include <dev/cfe/cfe_ioctl.h>

#include <dev/bhnd/nvram/bhnd_nvram_iovar.h>

#include "bhnd_nvram_if.h"

#include "bcm_nvram_cfevar.h"

/**
 * CFE-backed bhnd_nvram_io implementation.
 */
struct bhnd_nvram_iocfe {
	struct bhnd_nvram_io	 io;		/**< common I/O instance state */

	char			*dname;		/**< CFE device name (borrowed) */
	int			 fd;		/**< CFE file descriptor */
	size_t			 offset;	/**< base offset */
	size_t			 size;		/**< device size */
	bool			 req_blk_erase;	/**< flash blocks must be erased
						     before writing */
};

BHND_NVRAM_IOPS_DEFN(iocfe)

#define IOCFE_LOG(_io, _fmt, ...)	\
	printf("%s/%s: " _fmt, __FUNCTION__, (_io)->dname, ##__VA_ARGS__)

static int			 bhnd_nvram_iocfe_new(struct bhnd_nvram_io **io,
				     char *dname);

static struct bhnd_nvram_io	*bhnd_nvram_find_cfedev(device_t dev,
				     char **dname, bhnd_nvram_data_class **cls);

/** Known CFE NVRAM device names, in probe order. */
static char *nvram_cfe_devs[] = {
	"nflash0.nvram",	/* NAND */
	"nflash1.nvram",
	"flash0.nvram",
	"flash1.nvram",
};

/** Supported CFE NVRAM formats, in probe order. */
static bhnd_nvram_data_class * const nvram_cfe_fmts[] = {
	&bhnd_nvram_bcm_class,
	&bhnd_nvram_tlv_class
};


static int
bhnd_nvram_cfe_probe(device_t dev)
{
	struct bhnd_nvram_io	*io;
	bhnd_nvram_data_class	*cls;
	const char		*cls_desc;
	char			*dname;
	char			*desc;

	/* Locate a usable CFE device */
	io = bhnd_nvram_find_cfedev(dev, &dname, &cls);
	if (io == NULL)
		return (ENXIO);
	bhnd_nvram_io_free(io);

	/* Format the device description */
	cls_desc = bhnd_nvram_data_class_desc(cls);
	asprintf(&desc, M_DEVBUF, "%s CFE %s", cls_desc, dname);
	if (desc != NULL) {
		device_set_desc_copy(dev, desc);
		free(desc, M_DEVBUF);
	} else {
		device_set_desc(dev, cls_desc);
	}

	/* Refuse wildcard attachments */
	return (BUS_PROBE_NOWILDCARD);
}


static int
bhnd_nvram_cfe_attach(device_t dev)
{
	struct bhnd_nvram_cfe_softc	*sc;
	bhnd_nvram_data_class		*cls;
	struct bhnd_nvram_io		*io;
	char				*dname;
	int				 error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Locate NVRAM device via CFE */
	io = bhnd_nvram_find_cfedev(dev, &dname, &cls);
	if (io == NULL) {
		device_printf(dev, "CFE NVRAM device not found\n");
		return (ENXIO);
	}

	/* Initialize NVRAM store and free the I/O context */
	error = bhnd_nvram_store_parse_new(&sc->store, io, cls);
	bhnd_nvram_io_free(io);
	if (error)
		return (error);

	return (error);
}

static int
bhnd_nvram_cfe_resume(device_t dev)
{
	return (0);
}

static int
bhnd_nvram_cfe_suspend(device_t dev)
{
	return (0);
}

static int
bhnd_nvram_cfe_detach(device_t dev)
{
	struct bhnd_nvram_cfe_softc *sc;

	sc = device_get_softc(dev);

	bhnd_nvram_store_free(sc->store);

	return (0);
}

static int
bhnd_nvram_cfe_getvar(device_t dev, const char *name, void *buf, size_t *len,
    bhnd_nvram_type type)
{
	struct bhnd_nvram_cfe_softc *sc = device_get_softc(dev);

	return (bhnd_nvram_store_getvar(sc->store, name, buf, len, type));
}

static int
bhnd_nvram_cfe_setvar(device_t dev, const char *name, const void *buf,
    size_t len, bhnd_nvram_type type)
{
	struct bhnd_nvram_cfe_softc *sc = device_get_softc(dev);

	return (bhnd_nvram_store_setvar(sc->store, name, buf, len, type));
}

/**
 * Find, open, identify, and return an I/O context mapping our
 * CFE NVRAM device.
 * 
 * @param	dev		bhnd_nvram_cfe device.
 * @param[out]	dname		On success, the CFE device name.
 * @param[out]	cls		On success, the identified NVRAM data format
 *				class.
 *
 * @retval	non-NULL success. the caller inherits ownership of the returned
 * NVRAM I/O context.
 * @retval	NULL if no usable CFE NVRAM device could be found.
 */
static struct bhnd_nvram_io *
bhnd_nvram_find_cfedev(device_t dev, char **dname, bhnd_nvram_data_class **cls)
{
	struct bhnd_nvram_io	*io;
	int			 devinfo;
	int			 error, result;

	for (u_int i = 0; i < nitems(nvram_cfe_fmts); i++) {
		*cls = nvram_cfe_fmts[i];

		for (u_int j = 0; j < nitems(nvram_cfe_devs); j++) {
			*dname = nvram_cfe_devs[j];

			/* Does the device exist? */
			if ((devinfo = cfe_getdevinfo(*dname)) < 0) {
				if (devinfo != CFE_ERR_DEVNOTFOUND) {
					device_printf(dev, "cfe_getdevinfo(%s) "
					    "failed: %d\n", *dname, devinfo);
				}

				continue;
			}

			/* Open for reading */
			if ((error = bhnd_nvram_iocfe_new(&io, *dname)))
				continue;

			/* Probe */
			result = bhnd_nvram_data_probe(*cls, io);
			if (result <= 0) {
				/* Found a supporting NVRAM data class */
				return (io);
			}

			/* Keep searching */
			bhnd_nvram_io_free(io);
			io = NULL;
		}
	}

	return (NULL);
}


/**
 * Allocate and return a new I/O context backed by a CFE device.
 *
 * The caller is responsible for deallocating the returned I/O context via
 * bhnd_nvram_io_free().
 * 
 * @param[out] io On success, a valid I/O context for @p dname.
 * @param dname The name of the CFE device to be opened for reading.
 *
 * @retval 0 success.
 * @retval non-zero if opening @p dname otherwise fails, a standard unix error
 * will be returned.
 */
static int
bhnd_nvram_iocfe_new(struct bhnd_nvram_io **io, char *dname)
{
	struct bhnd_nvram_iocfe	*iocfe;
	nvram_info_t		 nvram_info;
	int			 cerr, devinfo, dtype, rlen;
	int64_t			 nv_offset;
	u_int			 nv_size;
	bool			 req_blk_erase;
	int			 error;

	iocfe = malloc(sizeof(*iocfe), M_DEVBUF, M_WAITOK);
	iocfe->io.iops = &bhnd_nvram_iocfe_ops;
	iocfe->dname = dname;

	/* Try to open the device */
	iocfe->fd = cfe_open(dname);
	if (iocfe->fd <= 0) {
		IOCFE_LOG(iocfe, "cfe_open() failed: %d\n", iocfe->fd);

		error = ENXIO;
		goto failed;
	}

	/* Try to fetch device info */
	if ((devinfo = cfe_getdevinfo(iocfe->dname)) < 0) {
		IOCFE_LOG(iocfe, "cfe_getdevinfo() failed: %d\n", devinfo);
		error = ENXIO;
		goto failed;
	}

	/* Verify device type */
	dtype = devinfo & CFE_DEV_MASK;
	switch (dtype) {
	case CFE_DEV_FLASH:
	case CFE_DEV_NVRAM:
		/* Valid device type */
		break;
	default:
		IOCFE_LOG(iocfe, "unknown device type: %d\n", dtype);
		error = ENXIO;
		goto failed;
	}

	/* Try to fetch nvram info from CFE */
	cerr = cfe_ioctl(iocfe->fd, IOCTL_NVRAM_GETINFO,
	    (unsigned char *)&nvram_info, sizeof(nvram_info), &rlen, 0);
	if (cerr == CFE_OK) {
		/* Sanity check the result; must not be a negative integer */
		if (nvram_info.nvram_size < 0 ||
		    nvram_info.nvram_offset < 0)
		{
			IOCFE_LOG(iocfe, "invalid NVRAM layout (%d/%d)\n",
			    nvram_info.nvram_size, nvram_info.nvram_offset);
			error = ENXIO;
			goto failed;
		}

		nv_offset	= nvram_info.nvram_offset;
		nv_size		= nvram_info.nvram_size;
		req_blk_erase	= (nvram_info.nvram_eraseflg != 0);
	} else if (cerr != CFE_OK && cerr != CFE_ERR_INV_COMMAND) {
		IOCFE_LOG(iocfe, "IOCTL_NVRAM_GETINFO failed: %d\n", cerr);
		error = ENXIO;
		goto failed;
	}

	/* Fall back on flash info.
	 * 
	 * This is known to be required on the Asus RT-N53 (CFE 5.70.55.33, 
	 * BBP 1.0.37, BCM5358UB0), where IOCTL_NVRAM_GETINFO returns
	 * CFE_ERR_INV_COMMAND.
	 */
	if (cerr == CFE_ERR_INV_COMMAND) {
		flash_info_t fi;

		cerr = cfe_ioctl(iocfe->fd, IOCTL_FLASH_GETINFO,
		    (unsigned char *)&fi, sizeof(fi), &rlen, 0);

		if (cerr != CFE_OK) {
			IOCFE_LOG(iocfe, "IOCTL_FLASH_GETINFO failed %d\n",
			    cerr);
			error = ENXIO;
			goto failed;
		}

		nv_offset	= 0x0;
		nv_size		= fi.flash_size;
		req_blk_erase	= !(fi.flash_flags & FLASH_FLAG_NOERASE);
	}

	
	/* Verify that the full NVRAM layout can be represented via size_t */
	if (nv_size > SIZE_MAX || SIZE_MAX - nv_size < nv_offset) {
		IOCFE_LOG(iocfe, "invalid NVRAM layout (%#x/%#jx)\n",
		    nv_size, (intmax_t)nv_offset);
		error = ENXIO;
		goto failed;
	}

	iocfe->offset = nv_offset;
	iocfe->size = nv_size;
	iocfe->req_blk_erase = req_blk_erase;

	*io = &iocfe->io;
	return (CFE_OK);

failed:
	if (iocfe->fd >= 0)
		cfe_close(iocfe->fd);

	free(iocfe, M_DEVBUF);

	*io = NULL;
	return (error);
}

static void
bhnd_nvram_iocfe_free(struct bhnd_nvram_io *io)
{
	struct bhnd_nvram_iocfe	*iocfe = (struct bhnd_nvram_iocfe *)io;

	cfe_close(iocfe->fd);
	free(io, M_DEVBUF);
}

static size_t
bhnd_nvram_iocfe_getsize(struct bhnd_nvram_io *io)
{
	struct bhnd_nvram_iocfe	*iocfe = (struct bhnd_nvram_iocfe *)io;
	return (iocfe->size);
}

static int
bhnd_nvram_iocfe_setsize(struct bhnd_nvram_io *io, size_t size)
{
	/* unsupported */
	return (ENODEV);
}

static int
bhnd_nvram_iocfe_read_ptr(struct bhnd_nvram_io *io, size_t offset,
    const void **ptr, size_t nbytes, size_t *navail)
{
	/* unsupported */
	return (ENODEV);
}

static int
bhnd_nvram_iocfe_write_ptr(struct bhnd_nvram_io *io, size_t offset,
    void **ptr, size_t nbytes, size_t *navail)
{
	/* unsupported */
	return (ENODEV);
}

static int
bhnd_nvram_iocfe_write(struct bhnd_nvram_io *io, size_t offset, void *buffer,
    size_t nbytes)
{
	/* unsupported */
	return (ENODEV);
}

static int
bhnd_nvram_iocfe_read(struct bhnd_nvram_io *io, size_t offset, void *buffer,
    size_t nbytes)
{
	struct bhnd_nvram_iocfe	*iocfe;
	size_t			 remain;
	int64_t			 cfe_offset;
	int			 nr, nreq;

	iocfe = (struct bhnd_nvram_iocfe *)io;

	/* Determine (and validate) the base CFE offset */
#if (SIZE_MAX > INT64_MAX)
	if (iocfe->offset > INT64_MAX || offset > INT64_MAX)
		return (ENXIO);
#endif

	if (INT64_MAX - offset < iocfe->offset)
		return (ENXIO);

	cfe_offset = iocfe->offset + offset;

	/* Verify that cfe_offset + nbytes is representable */
	if (INT64_MAX - cfe_offset < nbytes)
		return (ENXIO);

	/* Perform the read */
	for (remain = nbytes; remain > 0;) {
		void	*p;
		size_t	 nread;
		int64_t	 cfe_noff;

		nread = (nbytes - remain);
		cfe_noff = cfe_offset + nread;
		p = ((uint8_t *)buffer + nread);
		nreq = ummin(INT_MAX, remain);
	
		nr = cfe_readblk(iocfe->fd, cfe_noff, p, nreq);
		if (nr < 0) {
			IOCFE_LOG(iocfe, "cfe_readblk() failed: %d\n", nr);
			return (ENXIO);
		}

		/* Check for unexpected short read */
		if (nr == 0 && remain > 0) {
			/* If the request fits entirely within the CFE
			 * device range, we shouldn't hit EOF */
			if (remain < iocfe->size &&
			    iocfe->size - remain > offset)
			{
				IOCFE_LOG(iocfe, "cfe_readblk() returned "
				    "unexpected short read (%d/%d)\n", nr,
				    nreq);
				return (ENXIO);
			}
		}

		if (nr == 0)
			break;

		remain -= nr;
	}

	/* Check for short read */
	if (remain > 0)
		return (ENXIO);

	return (0);
}

static device_method_t bhnd_nvram_cfe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bhnd_nvram_cfe_probe),
	DEVMETHOD(device_attach,	bhnd_nvram_cfe_attach),
	DEVMETHOD(device_resume,	bhnd_nvram_cfe_resume),
	DEVMETHOD(device_suspend,	bhnd_nvram_cfe_suspend),
	DEVMETHOD(device_detach,	bhnd_nvram_cfe_detach),

	/* NVRAM interface */
	DEVMETHOD(bhnd_nvram_getvar,	bhnd_nvram_cfe_getvar),
	DEVMETHOD(bhnd_nvram_setvar,	bhnd_nvram_cfe_setvar),

	DEVMETHOD_END
};

DEFINE_CLASS_0(bhnd_nvram, bhnd_nvram_cfe, bhnd_nvram_cfe_methods,
    sizeof(struct bhnd_nvram_cfe_softc));
EARLY_DRIVER_MODULE(bhnd_nvram_cfe, nexus, bhnd_nvram_cfe,
    bhnd_nvram_devclass, NULL, NULL, BUS_PASS_BUS + BUS_PASS_ORDER_EARLY);
