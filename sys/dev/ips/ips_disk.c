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
 *
 * $FreeBSD$
 */

#include <dev/ips/ips.h>
#include <dev/ips/ips_disk.h>
#include <sys/stat.h>

static int ipsd_probe(device_t dev);
static int ipsd_attach(device_t dev);
static int ipsd_detach(device_t dev);

static d_open_t ipsd_open;
static d_close_t ipsd_close;
static d_strategy_t ipsd_strategy;

#define IPSD_CDEV_MAJOR 176

static struct cdevsw ipsd_cdevsw = {
	ipsd_open,
	ipsd_close,
	physread,
	physwrite,
	noioctl,
	nopoll,
	nommap,
	ipsd_strategy,
	"ipsd",
	IPSD_CDEV_MAJOR,
	nodump,
	nopsize,
	D_DISK,
	-1
};

static struct cdevsw ipsd_disk_cdevsw;
static int disks_registered;

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
static int ipsd_open(dev_t dev, int flags, int fmt, struct proc *p)
{
	ipsdisk_softc_t *dsc = dev->si_drv1;
	struct disklabel *label;

	dsc->state |= IPS_DEV_OPEN;
	DEVICE_PRINTF(2, dsc->dev, "I'm open\n");

	label = &dsc->ipsd_disk.d_label;
	bzero(label, sizeof(*label));
	label->d_type = DTYPE_ESDI;
	label->d_secsize = IPS_BLKSIZE;
	label->d_nsectors = dsc->sectors;
	label->d_ntracks = dsc->heads;
	label->d_ncylinders = dsc->cylinders;
	label->d_secpercyl = dsc->sectors * dsc->heads;
	label->d_secperunit = dsc->size;

       	return 0;
}

static int ipsd_close(dev_t dev, int flags, int fmt, struct proc *p)
{
	ipsdisk_softc_t *dsc = dev->si_drv1;
	dsc->state &= ~IPS_DEV_OPEN;
	DEVICE_PRINTF(2, dsc->dev, "I'm closed for the day\n");
        return 0;
}

/* ipsd_finish is called to clean up and return a completed IO request */
void ipsd_finish(struct buf *iobuf)
{
	ipsdisk_softc_t *dsc;
	dsc = iobuf->b_dev->si_drv1;	

	if (iobuf->b_flags & B_ERROR) {
		device_printf(dsc->dev, "iobuf error %d\n", iobuf->b_error);
	} else
		iobuf->b_resid = 0;

	devstat_end_transaction_buf(&dsc->stats, iobuf);
	biodone(iobuf);	
	ips_start_io_request(dsc->sc);
}


static void ipsd_strategy(struct buf *iobuf)
{
	ipsdisk_softc_t *dsc;

	dsc = iobuf->b_dev->si_drv1;	
	DEVICE_PRINTF(8,dsc->dev,"in strategy\n");
	devstat_start_transaction(&dsc->stats);
	iobuf->b_driver1 = (void *)(uintptr_t)dsc->sc->drives[dsc->disk_number].drivenum;
	bufqdisksort(&dsc->sc->queue, iobuf);
	ips_start_io_request(dsc->sc);
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

	DEVICE_PRINTF(2,dev, "in attach\n");

	dsc = (ipsdisk_softc_t *)device_get_softc(dev);
	bzero(dsc, sizeof(ipsdisk_softc_t));
	adapter = device_get_parent(dev);
	dsc->dev = dev;
	dsc->sc = device_get_softc(adapter);
	dsc->unit = device_get_unit(dev);
	dsc->disk_number = (uintptr_t) device_get_ivars(dev);

	dsc->size = dsc->sc->drives[dsc->disk_number].sector_count;
   	if ((dsc->size > 0x400000) &&
       	    ((dsc->sc->adapter_info.miscflags & 0x8) == 0)) {
      		dsc->heads = IPS_NORM_HEADS;
      		dsc->sectors = IPS_NORM_SECTORS;
   	} else {
      		dsc->heads = IPS_COMP_HEADS;
      		dsc->sectors = IPS_COMP_SECTORS;
   	}
	dsc->cylinders = (dsc->size / (dsc->heads * dsc->sectors));
	dsc->ipsd_dev = disk_create(dsc->unit, &dsc->ipsd_disk, 0,
	    &ipsd_cdevsw, &ipsd_disk_cdevsw);
	dsc->ipsd_dev->si_drv1 = dsc;
	dsc->ipsd_dev->si_iosize_max = IPS_MAX_IO_SIZE;

	devstat_add_entry(&dsc->stats, "ipsd", dsc->unit, IPS_BLKSIZE,
	    DEVSTAT_NO_ORDERED_TAGS, DEVSTAT_TYPE_STORARRAY |
	    DEVSTAT_TYPE_IF_OTHER, DEVSTAT_PRIORITY_ARRAY);

	disks_registered++;

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
	devstat_remove_entry(&dsc->stats);
	disk_destroy(dsc->ipsd_dev);
	if (--disks_registered == 0)
		cdevsw_remove(&ipsd_cdevsw);
	return 0;
}
