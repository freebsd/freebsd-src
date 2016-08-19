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

#include "bhnd_nvram_if.h"

#include "bhnd_nvramvar.h"

static int	 nvram_open_cfedev(device_t dev, char *devname, int fd,
		     int64_t *offset, uint32_t *size, bhnd_nvram_format fmt);
static char	*nvram_find_cfedev(device_t dev, int *fd, int64_t *offset,
		     uint32_t *size, bhnd_nvram_format *fmt);

/** Known CFE NVRAM device names, in probe order. */
static char *nvram_cfe_devs[] = {
	"nflash0.nvram",	/* NAND */
	"nflash1.nvram",
	"flash0.nvram",
	"flash1.nvram",
};

/** Supported CFE NVRAM formats, in probe order. */
bhnd_nvram_format nvram_cfe_fmts[] = {
	BHND_NVRAM_FMT_BCM,
	BHND_NVRAM_FMT_TLV
};


static int
bhnd_nvram_cfe_probe(device_t dev)
{
	char				*devname;
	bhnd_nvram_format		 fmt;
	int64_t				 offset;
	uint32_t			 size;
	int				 error;
	int				 fd;

	/* Defer to default driver implementation */
	if ((error = bhnd_nvram_probe(dev)) > 0)
		return (error);

	/* Locate a usable CFE device */
	devname = nvram_find_cfedev(dev, &fd, &offset, &size, &fmt);
	if (devname == NULL)
		return (ENXIO);
	cfe_close(fd);

	switch (fmt) {
	case BHND_NVRAM_FMT_BCM:
		device_set_desc(dev, "Broadcom NVRAM");
		break;
	case BHND_NVRAM_FMT_TLV:
		device_set_desc(dev, "Broadcom WGT634U NVRAM");
		break;
	default:
		device_printf(dev, "unknown NVRAM format: %d\n", fmt);
		return (ENXIO);
	}

	/* Refuse wildcard attachments */
	return (BUS_PROBE_NOWILDCARD);
}


static int
bhnd_nvram_cfe_attach(device_t dev)
{
	char			*devname;
	unsigned char		*buffer;
	bhnd_nvram_format	 fmt;
	int64_t			 offset;
	uint32_t		 size;
	int			 error;
	int			 fd;

	error = 0;
	buffer = NULL;
	fd = CFE_ERR;

	/* Locate NVRAM device via CFE */
	devname = nvram_find_cfedev(dev, &fd, &offset, &size, &fmt);
	if (devname == NULL) {
		device_printf(dev, "CFE NVRAM device not found\n");
		return (ENXIO);
	}

	/* Copy out NVRAM buffer */
	buffer = malloc(size, M_TEMP, M_NOWAIT);
	if (buffer == NULL)
		return (ENOMEM);

	for (size_t remain = size; remain > 0;) {
		int nr, req;
		
		req = ulmin(INT_MAX, remain);
		nr = cfe_readblk(fd, size-remain, buffer+(size-remain),
		    req);
		if (nr < 0) {
			device_printf(dev, "%s: cfe_readblk() failed: %d\n",
			    devname, fd);

			error = ENXIO;
			goto cleanup;
		}

		remain -= nr;

		if (nr == 0 && remain > 0) {
			device_printf(dev, "%s: cfe_readblk() unexpected EOF: "
			    "%zu of %zu pending\n", devname, remain, size);

			error = ENXIO;
			goto cleanup;
		}
	}

	device_printf(dev, "CFE %s (%#jx+%#jx)\n", devname, (uintmax_t)offset,
	    (uintmax_t)size);

	/* Delegate to default driver implementation */
	error = bhnd_nvram_attach(dev, buffer, size, fmt);

cleanup:
	if (buffer != NULL)
		free(buffer, M_TEMP);

	if (fd >= 0)
		cfe_close(fd);

	return (error);
}

/**
 * Identify and open a CFE NVRAM device.
 * 
 * @param	dev	bhnd_nvram_cfe device.
 * @param	devname	The name of the CFE device to be probed.
 * @param	fd	An open CFE file descriptor for @p devname.
 * @param[out]	offset	On success, the NVRAM data offset within @p @fd.
 * @param[out]	size	On success, maximum the NVRAM data size within @p fd.
 * @param	fmt	The expected NVRAM data format for this device.
 * 
 * @retval	0		success
 * @retval	non-zero	If probing @p devname fails, a regular unix
 * 				error code will be returned.
 */
static int
nvram_open_cfedev(device_t dev, char *devname, int fd, int64_t *offset,
    uint32_t *size, bhnd_nvram_format fmt)
{
	union bhnd_nvram_ident	ident;
	nvram_info_t		nvram_info;
	int			cerr, devinfo, dtype, rlen;
	int			error;

	/* Try to fetch device info */
	if ((devinfo = cfe_getdevinfo(devname)) == CFE_ERR_DEVNOTFOUND)
		return (ENODEV);

	if (devinfo < 0) {
		device_printf(dev, "cfe_getdevinfo() failed: %d",
		    devinfo);
		return (ENXIO);
	}

	/* Verify device type */
	dtype = devinfo & CFE_DEV_MASK;
	switch (dtype) {
	case CFE_DEV_FLASH:
	case CFE_DEV_NVRAM:
		/* Valid device type */
		break;
	default:
		device_printf(dev, "%s: unknown device type %d\n",
		    devname, dtype);
		return (ENXIO);
	}

	/* Try to fetch nvram info from CFE */
	cerr = cfe_ioctl(fd, IOCTL_NVRAM_GETINFO, (unsigned char *)&nvram_info,
	    sizeof(nvram_info), &rlen, 0);
	if (cerr != CFE_OK && cerr != CFE_ERR_INV_COMMAND) {
		device_printf(dev, "%s: IOCTL_NVRAM_GETINFO failed: %d\n",
		    devname, cerr);
		return (ENXIO);
	}

	/* Fall back on flash info.
	 * 
	 * This is known to be required on the Asus RT-N53 (CFE 5.70.55.33, 
	 * BBP 1.0.37, BCM5358UB0), where IOCTL_NVRAM_GETINFO returns
	 * CFE_ERR_INV_COMMAND.
	 */
	if (cerr == CFE_ERR_INV_COMMAND) {
		flash_info_t fi;

		cerr = cfe_ioctl(fd, IOCTL_FLASH_GETINFO, (unsigned char *)&fi,
		    sizeof(fi), &rlen, 0);

		if (cerr != CFE_OK) {
			device_printf(dev, "%s: IOCTL_FLASH_GETINFO failed: "
			    "%d\n", devname, cerr);
			return (ENXIO);
		}

		nvram_info.nvram_eraseflg	=
		    !(fi.flash_flags & FLASH_FLAG_NOERASE);
		nvram_info.nvram_offset		= 0x0;
		nvram_info.nvram_size		= fi.flash_size;
	}

	/* Try to read NVRAM header/format identification */
	cerr = cfe_readblk(fd, 0, (unsigned char *)&ident, sizeof(ident));
	if (cerr < 0) {
		device_printf(dev, "%s: cfe_readblk() failed: %d\n",
		    devname, cerr);
		return (ENXIO);
	} else if (cerr == 0) {
		/* EOF */
		return (ENODEV);
	} else if (cerr != sizeof(ident)) {
		device_printf(dev, "%s: cfe_readblk() short read: %d\n",
			devname, cerr);
		return (ENXIO);
	}

	/* Verify expected format */
	if ((error = bhnd_nvram_parser_identify(&ident, fmt)))
		return (error);

	/* Provide offset and size */
	switch (fmt) {
	case BHND_NVRAM_FMT_TLV:
		/* No size field is available; must assume the NVRAM data
		 * consumes up to the full CFE NVRAM range */
		*offset = nvram_info.nvram_offset;
		*size = nvram_info.nvram_size;
		break;
	case BHND_NVRAM_FMT_BCM:
		if (ident.bcm.size > nvram_info.nvram_size) {
			device_printf(dev, "%s: NVRAM size %#x overruns %#x "
			    "device limit\n", devname, ident.bcm.size,
			    nvram_info.nvram_size);
			return (ENODEV);
		}

		*offset = nvram_info.nvram_offset;
		*size = ident.bcm.size;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

/**
 * Find (and open) a CFE NVRAM device.
 * 
 * @param	dev	bhnd_nvram_cfe device.
 * @param[out]	fd	On success, a valid CFE file descriptor. The callee
 *			is responsible for closing this file descriptor via
 *			cfe_close().
 * @param[out]	offset	On success, the NVRAM data offset within @p @fd.
 * @param[out]	size	On success, maximum the NVRAM data size within @p fd.
 * @param	fmt	The expected NVRAM data format for this device.
 * 
 * @return	On success, the opened CFE device's name will be returned. On
 *		error, returns NULL.
 */
static char *
nvram_find_cfedev(device_t dev, int *fd, int64_t *offset,
    uint32_t *size, bhnd_nvram_format *fmt)
{
	char	*devname;
	int	 error;

	for (u_int i = 0; i < nitems(nvram_cfe_fmts); i++) {
		*fmt = nvram_cfe_fmts[i];

		for (u_int j = 0; j < nitems(nvram_cfe_devs); j++) {
			devname = nvram_cfe_devs[j];

			/* Open for reading */
			*fd = cfe_open(devname);
			if (*fd == CFE_ERR_DEVNOTFOUND) {
				continue;
			} else if (*fd < 0) {
				device_printf(dev, "%s: cfe_open() failed: "
				    "%d\n", devname, *fd);
				continue;
			}

			/* Probe */
			error = nvram_open_cfedev(dev, devname, *fd, offset,
			    size, *fmt);
			if (error == 0)
				return (devname);

			/* Keep searching */
			devname = NULL;
			cfe_close(*fd);
		}
	}

	return (NULL);
}

static device_method_t bhnd_nvram_cfe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			bhnd_nvram_cfe_probe),
	DEVMETHOD(device_attach,		bhnd_nvram_cfe_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(bhnd_nvram, bhnd_nvram_cfe, bhnd_nvram_cfe_methods, 
    sizeof(struct bhnd_nvram_softc), bhnd_nvram_driver);
EARLY_DRIVER_MODULE(bhnd_nvram_cfe, nexus, bhnd_nvram_cfe,
    bhnd_nvram_devclass, NULL, NULL, BUS_PASS_BUS + BUS_PASS_ORDER_EARLY);
