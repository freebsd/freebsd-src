/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD */
/*
 * ATA driver for the XLR_PCMCIA Host adapter on the RMI XLR/XLS/.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h> 
#include <sys/rman.h>
#include <vm/uma.h>
#include <sys/ata.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <sys/bus_dma.h>

#include <dev/ata/ata-all.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/iomap.h>
#include <mips/include/resource.h>
#include <mips/rmi/interrupt.h>

#define	XLR_PCMCIA_DATA_REG		0x1f0
#define	XLR_PCMCIA_ERROR_REG		0x1f1
#define	XLR_PCMCIA_SECT_CNT_REG		0x1f2
#define	XLR_PCMCIA_SECT_NUM_REG		0x1f3
#define	XLR_PCMCIA_CYLINDER_LOW_REG	0x1f4
#define	XLR_PCMCIA_CYLINDER_HIGH_REG	0x1f5
#define	XLR_PCMCIA_SECT_DRIVE_HEAD_REG	0x1f6
#define	XLR_PCMCIA_CMD_STATUS_REG	0x1f7
#define	XLR_PCMCIA_ALT_STATUS_REG	0x3f6
#define	XLR_PCMCIA_CONTROL_REG		0x3f6

/*
 * Device methods
 */
static int xlr_pcmcia_probe(device_t);
static int xlr_pcmcia_attach(device_t);
static int xlr_pcmcia_detach(device_t);

static int
xlr_pcmcia_probe(device_t dev)
{
	struct ata_channel *ch = device_get_softc(dev);

	ch->unit = 0;
	ch->flags |= ATA_USE_16BIT | ATA_NO_SLAVE ;
	device_set_desc(dev, "PCMCIA ATA controller");

	return (ata_probe(dev));
}

/*
 * We add all the devices which we know about.
 * The generic attach routine will attach them if they are alive.
 */
static int
xlr_pcmcia_attach(device_t dev)
{
	struct ata_channel *ch = device_get_softc(dev);
	int i;
	int rid =0;
	struct resource *mem_res;


	mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,  RF_ACTIVE);
	for (i = 0; i < ATA_MAX_RES; i++)
		ch->r_io[i].res = mem_res;

	/*
	 * CF+ Specification.
	 */
	ch->r_io[ATA_DATA].offset 	= XLR_PCMCIA_DATA_REG;
	ch->r_io[ATA_FEATURE].offset 	= XLR_PCMCIA_ERROR_REG;
	ch->r_io[ATA_COUNT].offset 	= XLR_PCMCIA_SECT_CNT_REG;
	ch->r_io[ATA_SECTOR].offset 	= XLR_PCMCIA_SECT_NUM_REG;
	ch->r_io[ATA_CYL_LSB].offset 	= XLR_PCMCIA_CYLINDER_LOW_REG;
	ch->r_io[ATA_CYL_MSB].offset 	= XLR_PCMCIA_CYLINDER_HIGH_REG;
	ch->r_io[ATA_DRIVE].offset 	= XLR_PCMCIA_SECT_DRIVE_HEAD_REG;
	ch->r_io[ATA_COMMAND].offset 	= XLR_PCMCIA_CMD_STATUS_REG;
	ch->r_io[ATA_ERROR].offset 	= XLR_PCMCIA_ERROR_REG;
	ch->r_io[ATA_IREASON].offset 	= XLR_PCMCIA_SECT_CNT_REG;
	ch->r_io[ATA_STATUS].offset 	= XLR_PCMCIA_CMD_STATUS_REG;
	ch->r_io[ATA_ALTSTAT].offset 	= XLR_PCMCIA_ALT_STATUS_REG;
	ch->r_io[ATA_CONTROL].offset 	= XLR_PCMCIA_CONTROL_REG;

	/* Should point at the base of registers. */
	ch->r_io[ATA_IDX_ADDR].offset = XLR_PCMCIA_DATA_REG;

	ata_generic_hw(dev);

	return (ata_attach(dev)); 
}

static int
xlr_pcmcia_detach(device_t dev)
{
	bus_generic_detach(dev);
  
	return (0);
}

static device_method_t xlr_pcmcia_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,         xlr_pcmcia_probe),
    DEVMETHOD(device_attach,        xlr_pcmcia_attach),
    DEVMETHOD(device_detach,        xlr_pcmcia_detach),

    { 0, 0 }
};

static driver_t xlr_pcmcia_driver = {
        "ata",
        xlr_pcmcia_methods,
        sizeof(struct ata_channel),
};

DRIVER_MODULE(ata, iodi, xlr_pcmcia_driver, ata_devclass, 0, 0);
