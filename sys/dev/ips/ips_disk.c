/*-
 * Written by: David Jeffery
 * Copyright (c) 2002 Adaptec Inc.
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

#include <dev/ips/ips.h>
#include <dev/ips/ips_disk.h>
#include <sys/stat.h>

static int ipsd_probe(device_t dev);
static int ipsd_attach(device_t dev);
static int ipsd_detach(device_t dev);

static disk_open_t ipsd_open;
static disk_close_t ipsd_close;
static disk_strategy_t ipsd_strategy;

static device_method_t ipsd_methods[] = {
	DEVMETHOD(device_probe,		ipsd_probe),
	DEVMETHOD(device_attach,	ipsd_attach),
	DEVMETHOD(device_detach,	ipsd_detach),
	{ 0, 0 }
};


static driver_t ipsd_driver = {
	"ipsd",
	ipsd_methods,
	sizeof(ipsdisk_softc_t)
};

static devclass_t ipsd_devclass;
DRIVER_MODULE(ipsd, ips, ipsd_driver, ipsd_devclass, 0, 0);

/* handle opening of disk device.  It must set up all
   information about the geometry and size of the disk */
static int ipsd_open(struct disk *dp)
{
	ipsdisk_softc_t *dsc = dp->d_drv1;

	dsc->state |= IPS_DEV_OPEN;
	DEVICE_PRINTF(2, dsc->dev, "I'm open\n");
       	return 0;
}

static int ipsd_close(struct disk *dp)
{
	ipsdisk_softc_t *dsc = dp->d_drv1;
	dsc->state &= ~IPS_DEV_OPEN;
	DEVICE_PRINTF(2, dsc->dev, "I'm closed for the day\n");
        return 0;
}

/* ipsd_finish is called to clean up and return a completed IO request */
void ipsd_finish(struct bio *iobuf)
{
	if (iobuf->bio_flags & BIO_ERROR) {
		ipsdisk_softc_t *dsc;
		dsc = iobuf->bio_disk->d_drv1; 
		device_printf(dsc->dev, "iobuf error %d\n", iobuf->bio_error);
	} else
		iobuf->bio_resid = 0;

	biodone(iobuf);	
}


static void ipsd_strategy(struct bio *iobuf)
{
	ipsdisk_softc_t *dsc;

	dsc = iobuf->bio_disk->d_drv1;	
	DEVICE_PRINTF(8,dsc->dev,"in strategy\n");
	iobuf->bio_driver1 = (void *)(uintptr_t)dsc->sc->drives[dsc->disk_number].drivenum;
	ips_start_io_request(dsc->sc, iobuf);
}

static int ipsd_probe(device_t dev)
{
	DEVICE_PRINTF(2,dev, "in probe\n");
	device_set_desc(dev, "Logical Drive");
	return 0;
}

static int ipsd_attach(device_t dev)
{
	device_t adapter;
	ipsdisk_softc_t *dsc;
	u_int totalsectors;

	DEVICE_PRINTF(2,dev, "in attach\n");

	dsc = (ipsdisk_softc_t *)device_get_softc(dev);
	bzero(dsc, sizeof(ipsdisk_softc_t));
	adapter = device_get_parent(dev);
	dsc->dev = dev;
	dsc->sc = device_get_softc(adapter);
	dsc->unit = device_get_unit(dev);
	dsc->disk_number = (uintptr_t) device_get_ivars(dev);
	dsc->ipsd_disk.d_drv1 = dsc;
	dsc->ipsd_disk.d_name = "ipsd";
	dsc->ipsd_disk.d_maxsize = IPS_MAX_IO_SIZE;
	dsc->ipsd_disk.d_open = ipsd_open;
	dsc->ipsd_disk.d_close = ipsd_close;
	dsc->ipsd_disk.d_strategy = ipsd_strategy;

	totalsectors = dsc->sc->drives[dsc->disk_number].sector_count;
   	if ((totalsectors > 0x400000) &&
       			((dsc->sc->adapter_info.miscflags & 0x8) == 0)) {
      		dsc->ipsd_disk.d_fwheads = IPS_NORM_HEADS;
      		dsc->ipsd_disk.d_fwsectors = IPS_NORM_SECTORS;
   	} else {
      		dsc->ipsd_disk.d_fwheads = IPS_COMP_HEADS;
      		dsc->ipsd_disk.d_fwsectors = IPS_COMP_SECTORS;
   	}
	dsc->ipsd_disk.d_sectorsize = IPS_BLKSIZE;
	dsc->ipsd_disk.d_mediasize = totalsectors * IPS_BLKSIZE;
	disk_create(dsc->unit, &dsc->ipsd_disk, 0, NULL, NULL);

	device_printf(dev, "Logical Drive  (%dMB)\n",
		      dsc->sc->drives[dsc->disk_number].sector_count >> 11);
	return 0;
}

static int ipsd_detach(device_t dev)
{
	ipsdisk_softc_t *dsc;

	DEVICE_PRINTF(2, dev,"in detach\n");
	dsc = (ipsdisk_softc_t *)device_get_softc(dev);
	if(dsc->state & IPS_DEV_OPEN)
		return (EBUSY);
	disk_destroy(&dsc->ipsd_disk);
	return 0;
}

