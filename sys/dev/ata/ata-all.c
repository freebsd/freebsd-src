/*-
 * Copyright (c) 1998,1999,2000,2001,2002 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD$
 */

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/devicestat.h>
#include <sys/sysctl.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#ifdef __alpha__
#include <machine/md_var.h>
#endif
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/ata-raid.h>
#include <dev/ata/atapi-all.h>

/* device structures */
static	d_ioctl_t	ataioctl;
static struct cdevsw ata_cdevsw = {  
	/* open */	nullopen,
	/* close */	nullclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	ataioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"ata",
	/* maj */	159,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

/* prototypes */
static void ata_boot_attach(void);
static void ata_intr(void *);
static int ata_getparam(struct ata_device *, u_int8_t);
static int ata_service(struct ata_channel *);
static void bswap(int8_t *, int);
static void btrim(int8_t *, int);
static void bpack(int8_t *, int8_t *, int);
static void ata_change_mode(struct ata_device *, int);
static u_int8_t ata_drawersensor(struct ata_device *, int, u_int8_t, u_int8_t);

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, ata, CTLFLAG_RD, 0, "ATA driver parameters");

/* global vars */
devclass_t ata_devclass;

/* local vars */
static struct intr_config_hook *ata_delayed_attach = NULL;
static MALLOC_DEFINE(M_ATA, "ATA generic", "ATA driver generic layer");

int
ata_probe(device_t dev)
{
    struct ata_channel *ch;
    int rid;

    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

    if (ch->r_io || ch->r_altio || ch->r_irq)
	return EEXIST;

    /* initialize the softc basics */
    ch->active = ATA_IDLE;
    ch->dev = dev;

    rid = ATA_IOADDR_RID;
    ch->r_io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 
				  ATA_IOSIZE, RF_ACTIVE);
    if (!ch->r_io)
	goto failure;

    rid = ATA_ALTADDR_RID;
    ch->r_altio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
				     ATA_ALTIOSIZE, RF_ACTIVE);
    if (!ch->r_altio)
	goto failure;

    rid = ATA_BMADDR_RID;
    ch->r_bmio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
				    ATA_BMIOSIZE, RF_ACTIVE);
    if (bootverbose)
	ata_printf(ch, -1, "iobase=0x%04x altiobase=0x%04x bmaddr=0x%04x\n", 
		   (int)rman_get_start(ch->r_io),
		   (int)rman_get_start(ch->r_altio),
		   (ch->r_bmio) ? (int)rman_get_start(ch->r_bmio) : 0);

    ata_reset(ch);

    ch->device[MASTER].channel = ch;
    ch->device[MASTER].unit = ATA_MASTER;
    ch->device[MASTER].mode = ATA_PIO;
    ch->device[SLAVE].channel = ch;
    ch->device[SLAVE].unit = ATA_SLAVE;
    ch->device[SLAVE].mode = ATA_PIO;
    TAILQ_INIT(&ch->ata_queue);
    TAILQ_INIT(&ch->atapi_queue);
    return 0;
    
failure:
    if (ch->r_io)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, ch->r_io);
    if (ch->r_altio)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_ALTADDR_RID, ch->r_altio);
    if (ch->r_bmio)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_BMADDR_RID, ch->r_bmio);
    if (bootverbose)
	ata_printf(ch, -1, "probe allocation failed\n");
    return ENXIO;
}

int
ata_attach(device_t dev)
{
    struct ata_channel *ch;
    int error, rid;

    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

    rid = ATA_IRQ_RID;
    ch->r_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
				   RF_SHAREABLE | RF_ACTIVE);
    if (!ch->r_irq) {
	ata_printf(ch, -1, "unable to allocate interrupt\n");
	return ENXIO;
    }
    if ((error = bus_setup_intr(dev, ch->r_irq, INTR_TYPE_BIO | INTR_ENTROPY,
				ata_intr, ch, &ch->ih))) {
	ata_printf(ch, -1, "unable to setup interrupt\n");
	return error;
    }

    /*
     * do not attach devices if we are in early boot, this is done later 
     * when interrupts are enabled by a hook into the boot process.
     * otherwise attach what the probe has found in ch->devices.
     */
    if (!ata_delayed_attach) {
	if (ch->devices & ATA_ATA_SLAVE)
	    if (ata_getparam(&ch->device[SLAVE], ATA_C_ATA_IDENTIFY))
		ch->devices &= ~ATA_ATA_SLAVE;
	if (ch->devices & ATA_ATAPI_SLAVE)
	    if (ata_getparam(&ch->device[SLAVE], ATA_C_ATAPI_IDENTIFY))
		ch->devices &= ~ATA_ATAPI_SLAVE;
	if (ch->devices & ATA_ATA_MASTER)
	    if (ata_getparam(&ch->device[MASTER], ATA_C_ATA_IDENTIFY))
		ch->devices &= ~ATA_ATA_MASTER;
	if (ch->devices & ATA_ATAPI_MASTER)
	    if (ata_getparam(&ch->device[MASTER], ATA_C_ATAPI_IDENTIFY))
		ch->devices &= ~ATA_ATAPI_MASTER;
#ifdef DEV_ATADISK
	if (ch->devices & ATA_ATA_MASTER)
	    ad_attach(&ch->device[MASTER]);
	if (ch->devices & ATA_ATA_SLAVE)
	    ad_attach(&ch->device[SLAVE]);
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
	if (ch->devices & ATA_ATAPI_MASTER)
	    atapi_attach(&ch->device[MASTER]);
	if (ch->devices & ATA_ATAPI_SLAVE)
	    atapi_attach(&ch->device[SLAVE]);
#endif
    }
    return 0;
}

int
ata_detach(device_t dev)
{
    struct ata_channel *ch;
    int s;
 
    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

    /* make sure channel is not busy */
    ATA_SLEEPLOCK_CH(ch, ATA_CONTROL);

    s = splbio();
#ifdef DEV_ATADISK
    if (ch->devices & ATA_ATA_MASTER && ch->device[MASTER].driver)
	ad_detach(&ch->device[MASTER], 1);
    if (ch->devices & ATA_ATA_SLAVE && ch->device[SLAVE].driver)
	ad_detach(&ch->device[SLAVE], 1);
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    if (ch->devices & ATA_ATAPI_MASTER && ch->device[MASTER].driver)
	atapi_detach(&ch->device[MASTER]);
    if (ch->devices & ATA_ATAPI_SLAVE && ch->device[SLAVE].driver)
	atapi_detach(&ch->device[SLAVE]);
#endif
    splx(s);

    if (ch->device[MASTER].param) {
	free(ch->device[MASTER].param, M_ATA);
	ch->device[MASTER].param = NULL;
    }
    if (ch->device[SLAVE].param) {
	free(ch->device[SLAVE].param, M_ATA);
	ch->device[SLAVE].param = NULL;
    }
    ch->device[MASTER].driver = NULL;
    ch->device[SLAVE].driver = NULL;
    ch->device[MASTER].mode = ATA_PIO;
    ch->device[SLAVE].mode = ATA_PIO;
    ch->devices = 0;

    bus_teardown_intr(dev, ch->r_irq, ch->ih);
    bus_release_resource(dev, SYS_RES_IRQ, ATA_IRQ_RID, ch->r_irq);
    if (ch->r_bmio)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_BMADDR_RID, ch->r_bmio);
    bus_release_resource(dev, SYS_RES_IOPORT, ATA_ALTADDR_RID, ch->r_altio);
    bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, ch->r_io);
    ch->r_io = NULL;
    ch->r_altio = NULL;
    ch->r_bmio = NULL;
    ch->r_irq = NULL;
    ATA_UNLOCK_CH(ch);
    return 0;
}

int
ata_resume(device_t dev)
{
    return ata_reinit(device_get_softc(dev));
}

static int
ataioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct thread *td)
{
    struct ata_cmd *iocmd = (struct ata_cmd *)addr;
    struct ata_channel *ch;
    device_t device = devclass_get_device(ata_devclass, iocmd->channel);
    int error;

    if (cmd != IOCATA)
	return ENOTTY;
    
    if (iocmd->channel < -1 || iocmd->device < -1 || iocmd->device > SLAVE)
	return ENXIO;

    switch (iocmd->cmd) {
	case ATAATTACH:
	    /* should enable channel HW on controller that can SOS XXX */   
	    error = ata_probe(device);
	    if (!error)
		error = ata_attach(device);
	    return error;

	case ATADETACH:
	    error = ata_detach(device);
	    /* should disable channel HW on controller that can SOS XXX */   
	    return error;

	case ATAREINIT:
	    if (!device || !(ch = device_get_softc(device)))
		return ENXIO;
	    ATA_SLEEPLOCK_CH(ch, ATA_ACTIVE);
	    error = ata_reinit(ch);
	    return error;

	case ATAGMODE:
	    if (!device || !(ch = device_get_softc(device)))
		return ENXIO;

	    if ((iocmd->device == MASTER || iocmd->device == -1) &&
		ch->device[MASTER].driver)
		iocmd->u.mode.mode[MASTER] = ch->device[MASTER].mode;
	    else
		iocmd->u.mode.mode[MASTER] = -1;

	    if ((iocmd->device == SLAVE || iocmd->device == -1) &&
		ch->device[SLAVE].param)
		iocmd->u.mode.mode[SLAVE] = ch->device[SLAVE].mode;
	    else
		iocmd->u.mode.mode[SLAVE] = -1;
	    return 0;

	case ATASMODE:
	    if (!device || !(ch = device_get_softc(device)))
		return ENXIO;

	    if ((iocmd->device == MASTER || iocmd->device == -1) &&
		iocmd->u.mode.mode[MASTER] >= 0 && ch->device[MASTER].param) {
		ata_change_mode(&ch->device[MASTER],iocmd->u.mode.mode[MASTER]);
		iocmd->u.mode.mode[MASTER] = ch->device[MASTER].mode;
	    }
	    else
		iocmd->u.mode.mode[MASTER] = -1;

	    if ((iocmd->device == SLAVE || iocmd->device == -1) &&
		iocmd->u.mode.mode[SLAVE] >= 0 && ch->device[SLAVE].param) {
		ata_change_mode(&ch->device[SLAVE], iocmd->u.mode.mode[SLAVE]);
		iocmd->u.mode.mode[SLAVE] = ch->device[SLAVE].mode;
	    }
	    else
		iocmd->u.mode.mode[SLAVE] = -1;
	    return 0;

	case ATAGPARM:
	    if (!device || !(ch = device_get_softc(device)))
		return ENXIO;

	    iocmd->u.param.type[MASTER] = 
		ch->devices & (ATA_ATA_MASTER | ATA_ATAPI_MASTER);
	    iocmd->u.param.type[SLAVE] =
		ch->devices & (ATA_ATA_SLAVE | ATA_ATAPI_SLAVE);

	    if (ch->device[MASTER].name)
		strcpy(iocmd->u.param.name[MASTER], ch->device[MASTER].name);
	    if (ch->device[SLAVE].name)
		strcpy(iocmd->u.param.name[SLAVE], ch->device[SLAVE].name);

	    if (ch->device[MASTER].param)
		bcopy(ch->device[MASTER].param, &iocmd->u.param.params[MASTER],
		      sizeof(struct ata_params));
	    if (ch->device[SLAVE].param)
		bcopy(ch->device[SLAVE].param, &iocmd->u.param.params[SLAVE],
		      sizeof(struct ata_params));
	    return 0;

	case ATAENCSTAT: {
	    struct ata_device *atadev;
	    u_int8_t id1, id2, cnt, div;
	    int fan, temp;

	    if (!device || !(ch = device_get_softc(device)))
		return ENXIO;

	    ATA_SLEEPLOCK_CH(ch, ATA_ACTIVE);
	    
	    if (iocmd->device == SLAVE)
		atadev = &ch->device[SLAVE];
	    else
		atadev = &ch->device[MASTER];

	    ata_drawersensor(atadev, 1, 0x4e, 0);
	    id1 = ata_drawersensor(atadev, 0, 0x4f, 0);
	    ata_drawersensor(atadev, 1, 0x4e, 0x80);
	    id2 = ata_drawersensor(atadev, 0, 0x4f, 0);
	    if (id1 != 0xa3 || id2 != 0x5c)
		return ENXIO;

	    div = 1 << (((ata_drawersensor(atadev, 0, 0x5d, 0)&0x20)>>3) +
			((ata_drawersensor(atadev, 0, 0x47, 0)&0x30)>>4) + 1);
	    cnt = ata_drawersensor(atadev, 0, 0x28, 0);
	    if (cnt == 0xff)
		fan = 0;
	    else
		fan = 1350000 / cnt / div;
	    ata_drawersensor(atadev, 1, 0x4e, 0x01);
	    temp = (ata_drawersensor(atadev, 0, 0x50, 0) * 10) +
		   (ata_drawersensor(atadev, 0, 0x50, 0) & 0x80 ? 5 : 0);
	
	    iocmd->u.enclosure.fan = fan;
	    iocmd->u.enclosure.temp = temp;
	    iocmd->u.enclosure.v05 = ata_drawersensor(atadev, 0, 0x23, 0) * 27;
	    iocmd->u.enclosure.v12 = ata_drawersensor(atadev, 0, 0x24, 0) * 61;
	
	    ATA_UNLOCK_CH(ch);
	    return 0;
	}

#ifdef DEV_ATADISK
	case ATARAIDREBUILD:
	    return ata_raid_rebuild(iocmd->channel);

	case ATARAIDCREATE:
	    return ata_raid_create(&iocmd->u.raid_setup);

	case ATARAIDDELETE:
	    return ata_raid_delete(iocmd->channel);

	case ATARAIDSTATUS:
	    return ata_raid_status(iocmd->channel, &iocmd->u.raid_status);
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
	case ATAPICMD: {
	    struct ata_device *atadev;
	    caddr_t buf;

	    if (!device || !(ch = device_get_softc(device)))
		return ENXIO;

	    if (!(atadev = &ch->device[iocmd->device]) ||
		!(ch->devices & (iocmd->device == MASTER ?
				 ATA_ATAPI_MASTER : ATA_ATAPI_SLAVE)))
		return ENODEV;

	    if (!(buf = malloc(iocmd->u.atapi.count, M_ATA, M_NOWAIT)))
		return ENOMEM;

	    if (iocmd->u.atapi.flags & ATAPI_CMD_WRITE) {
		error = copyin(iocmd->u.atapi.data, buf, iocmd->u.atapi.count);
		if (error)
		    return error;
	    }
	    error = atapi_queue_cmd(atadev, iocmd->u.atapi.ccb,
				    buf, iocmd->u.atapi.count,
				    (iocmd->u.atapi.flags == ATAPI_CMD_READ ?
				     ATPR_F_READ : 0) | ATPR_F_QUIET, 
				    iocmd->u.atapi.timeout, NULL, NULL);
	    if (error) {
		iocmd->u.atapi.error = error;
		bcopy(&atadev->result, iocmd->u.atapi.sense_data,
		      sizeof(struct atapi_reqsense));
		error = 0;
	    }
	    else if (iocmd->u.atapi.flags & ATAPI_CMD_READ)
		error = copyout(buf, iocmd->u.atapi.data, iocmd->u.atapi.count);

	    free(buf, M_ATA);
	    return error;
	}
#endif
	default:
	    break;
    }
    return ENOTTY;
}

static int
ata_getparam(struct ata_device *atadev, u_int8_t command)
{
    struct ata_params *ata_parm;
    int retry = 0;

    if (!(ata_parm = malloc(sizeof(struct ata_params), M_ATA, M_NOWAIT))) {
	ata_prtdev(atadev, "malloc for identify data failed\n");
	return -1;
    }

    /* apparently some devices needs this repeated */
    do {
	if (ata_command(atadev, command, 0, 0, 0, ATA_WAIT_INTR)) {
	    ata_prtdev(atadev, "%s identify failed\n",
		       command == ATA_C_ATAPI_IDENTIFY ? "ATAPI" : "ATA");
	    free(ata_parm, M_ATA);
	    return -1;
	}
	if (retry++ > 4) {
	    ata_prtdev(atadev, "%s identify retries exceeded\n",
		       command == ATA_C_ATAPI_IDENTIFY ? "ATAPI" : "ATA");
	    free(ata_parm, M_ATA);
	    return -1;
	}
    } while (ata_wait(atadev, ((command == ATA_C_ATAPI_IDENTIFY) ?
			       ATA_S_DRQ : (ATA_S_READY|ATA_S_DSC|ATA_S_DRQ))));
    ATA_INSW(atadev->channel->r_io, ATA_DATA, (int16_t *)ata_parm,
	     sizeof(struct ata_params)/sizeof(int16_t));

    if (command == ATA_C_ATA_IDENTIFY ||
	!((ata_parm->model[0] == 'N' && ata_parm->model[1] == 'E') ||
	  (ata_parm->model[0] == 'F' && ata_parm->model[1] == 'X') ||
	  (ata_parm->model[0] == 'P' && ata_parm->model[1] == 'i')))
	bswap(ata_parm->model, sizeof(ata_parm->model));
    btrim(ata_parm->model, sizeof(ata_parm->model));
    bpack(ata_parm->model, ata_parm->model, sizeof(ata_parm->model));
    bswap(ata_parm->revision, sizeof(ata_parm->revision));
    btrim(ata_parm->revision, sizeof(ata_parm->revision));
    bpack(ata_parm->revision, ata_parm->revision, sizeof(ata_parm->revision));
    atadev->param = ata_parm;
    return 0;
}

static void 
ata_boot_attach(void)
{
    struct ata_channel *ch;
    int ctlr;

    if (ata_delayed_attach) {
	config_intrhook_disestablish(ata_delayed_attach);
	free(ata_delayed_attach, M_TEMP);
	ata_delayed_attach = NULL;
    }

    /*
     * run through all ata devices and look for real ATA & ATAPI devices
     * using the hints we found in the early probe, this avoids some of
     * the delays probing of non-exsistent devices can cause.
     */
    for (ctlr=0; ctlr<devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(ch = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	if (ch->devices & ATA_ATA_SLAVE)
	    if (ata_getparam(&ch->device[SLAVE], ATA_C_ATA_IDENTIFY))
		ch->devices &= ~ATA_ATA_SLAVE;
	if (ch->devices & ATA_ATAPI_SLAVE)
	    if (ata_getparam(&ch->device[SLAVE], ATA_C_ATAPI_IDENTIFY))
		ch->devices &= ~ATA_ATAPI_SLAVE;
	if (ch->devices & ATA_ATA_MASTER)
	    if (ata_getparam(&ch->device[MASTER], ATA_C_ATA_IDENTIFY))
		ch->devices &= ~ATA_ATA_MASTER;
	if (ch->devices & ATA_ATAPI_MASTER)
	    if (ata_getparam(&ch->device[MASTER], ATA_C_ATAPI_IDENTIFY))
		ch->devices &= ~ATA_ATAPI_MASTER;
    }

#ifdef DEV_ATADISK
    /* now we know whats there, do the real attach, first the ATA disks */
    for (ctlr=0; ctlr<devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(ch = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	if (ch->devices & ATA_ATA_MASTER)
	    ad_attach(&ch->device[MASTER]);
	if (ch->devices & ATA_ATA_SLAVE)
	    ad_attach(&ch->device[SLAVE]);
    }
    ata_raid_attach();
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    /* then the atapi devices */
    for (ctlr=0; ctlr<devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(ch = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	if (ch->devices & ATA_ATAPI_MASTER)
	    atapi_attach(&ch->device[MASTER]);
	if (ch->devices & ATA_ATAPI_SLAVE)
	    atapi_attach(&ch->device[SLAVE]);
    }
#endif
}

static void
ata_intr(void *data)
{
    struct ata_channel *ch = (struct ata_channel *)data;
    /* 
     * on PCI systems we might share an interrupt line with another
     * device or our twin ATA channel, so call ch->intr_func to figure 
     * out if it is really an interrupt we should process here
     */
    if (ch->intr_func && ch->intr_func(ch))
	return;

    /* if drive is busy it didn't interrupt */
    if (ATA_INB(ch->r_altio, ATA_ALTSTAT) & ATA_S_BUSY) {
	DELAY(100);
	if (!(ATA_INB(ch->r_altio, ATA_ALTSTAT) & ATA_S_DRQ))
	    return;
    }

    /* clear interrupt and get status */
    ch->status = ATA_INB(ch->r_io, ATA_STATUS);

    if (ch->status & ATA_S_ERROR)
	ch->error = ATA_INB(ch->r_io, ATA_ERROR);

    /* find & call the responsible driver to process this interrupt */
    switch (ch->active) {
#ifdef DEV_ATADISK
    case ATA_ACTIVE_ATA:
	if (!ch->running || ad_interrupt(ch->running) == ATA_OP_CONTINUES)
	    return;
	break;
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    case ATA_ACTIVE_ATAPI:
	if (!ch->running || atapi_interrupt(ch->running) == ATA_OP_CONTINUES)
	    return;
	break;
#endif
    case ATA_WAIT_INTR:
    case ATA_WAIT_INTR | ATA_CONTROL:
	wakeup((caddr_t)ch);
	break;

    case ATA_WAIT_READY:
    case ATA_WAIT_READY | ATA_CONTROL:
	break;

    case ATA_IDLE:
	if (ch->flags & ATA_QUEUED) {
	    ch->active = ATA_ACTIVE;
	    if (ata_service(ch) == ATA_OP_CONTINUES)
		return;
	}
	/* FALLTHROUGH */

    default:
#ifdef ATA_DEBUG
    {
	static int intr_count = 0;

	if (intr_count++ < 10)
	    ata_printf(ch, -1, "unwanted interrupt #%d active=%02x s=%02x\n",
		       intr_count, ch->active, ch->status);
    }
#endif
	break;
    }
    ch->active &= ATA_CONTROL;
    if (ch->active & ATA_CONTROL)
	return;
    ch->running = NULL;
    ata_start(ch);
    return;
}

void
ata_start(struct ata_channel *ch)
{
#ifdef DEV_ATADISK
    struct ad_request *ad_request; 
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    struct atapi_request *atapi_request;
#endif
    int s;

    if (!ATA_LOCK_CH(ch, ATA_ACTIVE))
	return;

    s = splbio();
#ifdef DEV_ATADISK
    /* find & call the responsible driver if anything on the ATA queue */
    if (TAILQ_EMPTY(&ch->ata_queue)) {
	if (ch->devices & (ATA_ATA_MASTER) && ch->device[MASTER].driver)
	    ad_start(&ch->device[MASTER]);
	if (ch->devices & (ATA_ATA_SLAVE) && ch->device[SLAVE].driver)
	    ad_start(&ch->device[SLAVE]);
    }
    if ((ad_request = TAILQ_FIRST(&ch->ata_queue))) {
	TAILQ_REMOVE(&ch->ata_queue, ad_request, chain);
	ch->active = ATA_ACTIVE_ATA;
	ch->running = ad_request;
	if (ad_transfer(ad_request) == ATA_OP_CONTINUES) {
	    splx(s);
	    return;
	}
    }

#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    /* find & call the responsible driver if anything on the ATAPI queue */
    if (TAILQ_EMPTY(&ch->atapi_queue)) {
	if (ch->devices & (ATA_ATAPI_MASTER) && ch->device[MASTER].driver)
	    atapi_start(&ch->device[MASTER]);
	if (ch->devices & (ATA_ATAPI_SLAVE) && ch->device[SLAVE].driver)
	    atapi_start(&ch->device[SLAVE]);
    }
    if ((atapi_request = TAILQ_FIRST(&ch->atapi_queue))) {
	TAILQ_REMOVE(&ch->atapi_queue, atapi_request, chain);
	ch->active = ATA_ACTIVE_ATAPI;
	ch->running = atapi_request;
	if (atapi_transfer(atapi_request) == ATA_OP_CONTINUES) {
	    splx(s);
	    return;
	}
    }
#endif
    splx(s);
    ATA_UNLOCK_CH(ch);
}

void
ata_reset(struct ata_channel *ch)
{
    u_int8_t lsb, msb, ostat0, ostat1;
    u_int8_t stat0 = 0, stat1 = 0;
    int mask = 0, timeout;

    /* do we have any signs of ATA/ATAPI HW being present ? */
    ATA_OUTB(ch->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
    DELAY(10);
    ostat0 = ATA_INB(ch->r_io, ATA_STATUS);
    if ((ostat0 & 0xf8) != 0xf8 && ostat0 != 0xa5) {
	stat0 = ATA_S_BUSY;
	mask |= 0x01;
    }
    ATA_OUTB(ch->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
    DELAY(10);	
    ostat1 = ATA_INB(ch->r_io, ATA_STATUS);
    if ((ostat1 & 0xf8) != 0xf8 && ostat1 != 0xa5) {
	stat1 = ATA_S_BUSY;
	mask |= 0x02;
    }

    ch->devices = 0;
    if (!mask)
	return;

    /* in some setups we dont want to test for a slave */
    if (ch->flags & ATA_NO_SLAVE) {
	stat1 = 0x0;
	mask &= ~0x02;
    }

    if (bootverbose)
	ata_printf(ch, -1, "mask=%02x ostat0=%02x ostat2=%02x\n",
		   mask, ostat0, ostat1);

    /* reset channel */
    ATA_OUTB(ch->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
    DELAY(10);
    ATA_OUTB(ch->r_altio, ATA_ALTSTAT, ATA_A_IDS | ATA_A_RESET);
    DELAY(10000); 
    ATA_OUTB(ch->r_altio, ATA_ALTSTAT, ATA_A_IDS);
    DELAY(100000);
    ATA_INB(ch->r_io, ATA_ERROR);

    /* wait for BUSY to go inactive */
    for (timeout = 0; timeout < 310000; timeout++) {
	if (stat0 & ATA_S_BUSY) {
	    ATA_OUTB(ch->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
	    DELAY(10);
	    stat0 = ATA_INB(ch->r_io, ATA_STATUS);
	    if (!(stat0 & ATA_S_BUSY)) {
		/* check for ATAPI signature while its still there */
		lsb = ATA_INB(ch->r_io, ATA_CYL_LSB);
		msb = ATA_INB(ch->r_io, ATA_CYL_MSB);
		if (bootverbose)
		    ata_printf(ch, ATA_MASTER, "ATAPI %02x %02x\n", lsb, msb);
		if (lsb == ATAPI_MAGIC_LSB && msb == ATAPI_MAGIC_MSB)
		    ch->devices |= ATA_ATAPI_MASTER;
	    }
	}
	if (stat1 & ATA_S_BUSY) {
	    ATA_OUTB(ch->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
	    DELAY(10);
	    stat1 = ATA_INB(ch->r_io, ATA_STATUS);
	    if (!(stat1 & ATA_S_BUSY)) {
		/* check for ATAPI signature while its still there */
		lsb = ATA_INB(ch->r_io, ATA_CYL_LSB);
		msb = ATA_INB(ch->r_io, ATA_CYL_MSB);
		if (bootverbose)
		    ata_printf(ch, ATA_SLAVE, "ATAPI %02x %02x\n", lsb, msb);
		if (lsb == ATAPI_MAGIC_LSB && msb == ATAPI_MAGIC_MSB)
		    ch->devices |= ATA_ATAPI_SLAVE;
	    }
	}
	if (mask == 0x01)      /* wait for master only */
	    if (!(stat0 & ATA_S_BUSY))
		break;
	if (mask == 0x02)      /* wait for slave only */
	    if (!(stat1 & ATA_S_BUSY))
		break;
	if (mask == 0x03)      /* wait for both master & slave */
	    if (!(stat0 & ATA_S_BUSY) && !(stat1 & ATA_S_BUSY))
		break;
	DELAY(100);
    }	
    DELAY(10);
    ATA_OUTB(ch->r_altio, ATA_ALTSTAT, ATA_A_4BIT);

    if (stat0 & ATA_S_BUSY)
	mask &= ~0x01;
    if (stat1 & ATA_S_BUSY)
	mask &= ~0x02;
    if (bootverbose)
	ata_printf(ch, -1, "mask=%02x stat0=%02x stat1=%02x\n", 
		   mask, stat0, stat1);
    if (!mask)
	return;

    if (mask & 0x01 && ostat0 != 0x00 && !(ch->devices & ATA_ATAPI_MASTER)) {
	ATA_OUTB(ch->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
	DELAY(10);
	ATA_OUTB(ch->r_io, ATA_ERROR, 0x58);
	ATA_OUTB(ch->r_io, ATA_CYL_LSB, 0xa5);
	lsb = ATA_INB(ch->r_io, ATA_ERROR);
	msb = ATA_INB(ch->r_io, ATA_CYL_LSB);
	if (bootverbose)
	    ata_printf(ch, ATA_MASTER, "ATA %02x %02x\n", lsb, msb);
	if (lsb != 0x58 && msb == 0xa5)
	    ch->devices |= ATA_ATA_MASTER;
    }
    if (mask & 0x02 && ostat1 != 0x00 && !(ch->devices & ATA_ATAPI_SLAVE)) {
	ATA_OUTB(ch->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
	DELAY(10);
	ATA_OUTB(ch->r_io, ATA_ERROR, 0x58);
	ATA_OUTB(ch->r_io, ATA_CYL_LSB, 0xa5);
	lsb = ATA_INB(ch->r_io, ATA_ERROR);
	msb = ATA_INB(ch->r_io, ATA_CYL_LSB);
	if (bootverbose)
	    ata_printf(ch, ATA_SLAVE, "ATA %02x %02x\n", lsb, msb);
	if (lsb != 0x58 && msb == 0xa5)
	    ch->devices |= ATA_ATA_SLAVE;
    }
    if (bootverbose)
	ata_printf(ch, -1, "devices=%02x\n", ch->devices);
}

int
ata_reinit(struct ata_channel *ch)
{
    int devices, misdev, newdev;

    if (!ch->r_io || !ch->r_altio || !ch->r_irq)
	return ENXIO;
    ATA_FORCELOCK_CH(ch, ATA_CONTROL);
    ch->running = NULL;
    devices = ch->devices;
    ata_printf(ch, -1, "resetting devices .. ");
    ata_reset(ch);

    if ((misdev = devices & ~ch->devices)) {
	if (misdev)
	    printf("\n");
#ifdef DEV_ATADISK
	if (misdev & ATA_ATA_MASTER && ch->device[MASTER].driver)
	    ad_detach(&ch->device[MASTER], 0);
	if (misdev & ATA_ATA_SLAVE && ch->device[SLAVE].driver)
	    ad_detach(&ch->device[SLAVE], 0);
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
	if (misdev & ATA_ATAPI_MASTER && ch->device[MASTER].driver)
	    atapi_detach(&ch->device[MASTER]);
	if (misdev & ATA_ATAPI_SLAVE && ch->device[SLAVE].driver)
	    atapi_detach(&ch->device[SLAVE]);
#endif
	if (misdev & ATA_ATA_MASTER || misdev & ATA_ATAPI_MASTER) {
	    if (ch->device[MASTER].param)
		free(ch->device[MASTER].param, M_ATA);
	    ch->device[MASTER].param = NULL;
	}
	if (misdev & ATA_ATA_SLAVE || misdev & ATA_ATAPI_SLAVE) {
	    if (ch->device[SLAVE].param)
		free(ch->device[SLAVE].param, M_ATA);
	    ch->device[SLAVE].param = NULL;
	}
    }
    if ((newdev = ~devices & ch->devices)) {
	if (newdev & ATA_ATA_MASTER)
	    if (ata_getparam(&ch->device[MASTER], ATA_C_ATA_IDENTIFY))
		newdev &= ~ATA_ATA_MASTER;
	if (newdev & ATA_ATA_SLAVE)
	    if (ata_getparam(&ch->device[SLAVE], ATA_C_ATA_IDENTIFY))
		newdev &= ~ATA_ATA_SLAVE;
	if (newdev & ATA_ATAPI_MASTER)
	    if (ata_getparam(&ch->device[MASTER], ATA_C_ATAPI_IDENTIFY))
		newdev &= ~ATA_ATAPI_MASTER;
	if (newdev & ATA_ATAPI_SLAVE)
	    if (ata_getparam(&ch->device[SLAVE], ATA_C_ATAPI_IDENTIFY))
		newdev &= ~ATA_ATAPI_SLAVE;
    }
    if (!misdev && newdev)
	printf("\n");
#ifdef DEV_ATADISK
    if (newdev & ATA_ATA_MASTER && !ch->device[MASTER].driver)
	ad_attach(&ch->device[MASTER]);
    else if (ch->devices & ATA_ATA_MASTER && ch->device[MASTER].driver) {
	ata_getparam(&ch->device[MASTER], ATA_C_ATA_IDENTIFY);
	ad_reinit(&ch->device[MASTER]);
    }
    if (newdev & ATA_ATA_SLAVE && !ch->device[SLAVE].driver)
	ad_attach(&ch->device[SLAVE]);
    else if (ch->devices & (ATA_ATA_SLAVE) && ch->device[SLAVE].driver) {
	ata_getparam(&ch->device[SLAVE], ATA_C_ATA_IDENTIFY);
	ad_reinit(&ch->device[SLAVE]);
    }
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    if (newdev & ATA_ATAPI_MASTER && !ch->device[MASTER].driver)
	atapi_attach(&ch->device[MASTER]);
    else if (ch->devices & (ATA_ATAPI_MASTER) && ch->device[MASTER].driver) {
	ata_getparam(&ch->device[MASTER], ATA_C_ATAPI_IDENTIFY);
	atapi_reinit(&ch->device[MASTER]);
    }
    if (newdev & ATA_ATAPI_SLAVE && !ch->device[SLAVE].driver)
	atapi_attach(&ch->device[SLAVE]);
    else if (ch->devices & (ATA_ATAPI_SLAVE) && ch->device[SLAVE].driver) {
	ata_getparam(&ch->device[SLAVE], ATA_C_ATAPI_IDENTIFY);
	atapi_reinit(&ch->device[SLAVE]);
    }
#endif
    printf("done\n");
    ATA_UNLOCK_CH(ch);
    ata_start(ch);
    return 0;
}

static int
ata_service(struct ata_channel *ch)
{
    /* do we have a SERVICE request from the drive ? */
    if ((ch->status & (ATA_S_SERVICE|ATA_S_ERROR|ATA_S_DRQ)) == ATA_S_SERVICE) {
	ATA_OUTB(ch->r_bmio, ATA_BMSTAT_PORT,
		 ata_dmastatus(ch) | ATA_BMSTAT_INTERRUPT);
#ifdef DEV_ATADISK
	if ((ATA_INB(ch->r_io, ATA_DRIVE) & ATA_SLAVE) == ATA_MASTER) {
	    if ((ch->devices & ATA_ATA_MASTER) && ch->device[MASTER].driver)
		return ad_service((struct ad_softc *)
				  ch->device[MASTER].driver, 0);
	}
	else {
	    if ((ch->devices & ATA_ATA_SLAVE) && ch->device[SLAVE].driver)
		return ad_service((struct ad_softc *)
				  ch->device[SLAVE].driver, 0);
	}
#endif
    }
    return ATA_OP_FINISHED;
}

int
ata_wait(struct ata_device *atadev, u_int8_t mask)
{
    int timeout = 0;
    
    DELAY(1);
    while (timeout < 5000000) { /* timeout 5 secs */
	atadev->channel->status = ATA_INB(atadev->channel->r_io, ATA_STATUS);

	/* if drive fails status, reselect the drive just to be sure */
	if (atadev->channel->status == 0xff) {
	    ata_prtdev(atadev, "no status, reselecting device\n");
	    ATA_OUTB(atadev->channel->r_io, ATA_DRIVE, ATA_D_IBM|atadev->unit);
	    DELAY(10);
	    atadev->channel->status = ATA_INB(atadev->channel->r_io,ATA_STATUS);
	    if (atadev->channel->status == 0xff)
		return -1;
	}

	/* are we done ? */
	if (!(atadev->channel->status & ATA_S_BUSY))
	    break;	      

	if (timeout > 1000) {
	    timeout += 1000;
	    DELAY(1000);
	}
	else {
	    timeout += 10;
	    DELAY(10);
	}
    }	 
    if (atadev->channel->status & ATA_S_ERROR)
	atadev->channel->error = ATA_INB(atadev->channel->r_io, ATA_ERROR);
    if (timeout >= 5000000)	 
	return -1;	    
    if (!mask)	   
	return (atadev->channel->status & ATA_S_ERROR);	 
    
    /* Wait 50 msec for bits wanted. */	   
    timeout = 5000;
    while (timeout--) {	  
	atadev->channel->status = ATA_INB(atadev->channel->r_io, ATA_STATUS);
	if ((atadev->channel->status & mask) == mask) {
	    if (atadev->channel->status & ATA_S_ERROR)
		atadev->channel->error=ATA_INB(atadev->channel->r_io,ATA_ERROR);
	    return (atadev->channel->status & ATA_S_ERROR);	      
	}
	DELAY (10);	   
    }	  
    return -1;	    
}   

int
ata_command(struct ata_device *atadev, u_int8_t command,
	   u_int64_t lba, u_int16_t count, u_int8_t feature, int flags)
{
    int error = 0;
#ifdef ATA_DEBUG
    ata_prtdev(atadev, "ata_command: addr=%04lx, cmd=%02x, "
	       "lba=%lld, count=%d, feature=%d, flags=%02x\n",
	       rman_get_start(atadev->channel->r_io), 
	       command, lba, count, feature, flags);
#endif

    /* select device */
    ATA_OUTB(atadev->channel->r_io, ATA_DRIVE, ATA_D_IBM | atadev->unit);

    /* disable interrupt from device */
    if (atadev->channel->flags & ATA_QUEUED)
	ATA_OUTB(atadev->channel->r_altio, ATA_ALTSTAT, ATA_A_IDS | ATA_A_4BIT);

    /* ready to issue command ? */
    if (ata_wait(atadev, 0) < 0) { 
	ata_prtdev(atadev, "timeout sending command=%02x s=%02x e=%02x\n",
		   command, atadev->channel->status, atadev->channel->error);
	return -1;
    }

    /* only use 48bit addressing if needed because of the overhead */
    if ((lba > 268435455 || count > 256) && atadev->param &&
	atadev->param->support.address48) {
	ATA_OUTB(atadev->channel->r_io, ATA_FEATURE, (feature>>8) & 0xff);
	ATA_OUTB(atadev->channel->r_io, ATA_FEATURE, feature);
	ATA_OUTB(atadev->channel->r_io, ATA_COUNT, (count>>8) & 0xff);
	ATA_OUTB(atadev->channel->r_io, ATA_COUNT, count & 0xff);
	ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, (lba>>24) & 0xff);
	ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, lba & 0xff);
	ATA_OUTB(atadev->channel->r_io, ATA_CYL_LSB, (lba<<32) & 0xff);
	ATA_OUTB(atadev->channel->r_io, ATA_CYL_LSB, (lba>>8) & 0xff);
	ATA_OUTB(atadev->channel->r_io, ATA_CYL_MSB, (lba>>40) & 0xff);
	ATA_OUTB(atadev->channel->r_io, ATA_CYL_MSB, (lba>>16) & 0xff);
	ATA_OUTB(atadev->channel->r_io, ATA_DRIVE, ATA_D_LBA | atadev->unit);

	/* translate command into 48bit version */
	switch (command) {
	case ATA_C_READ:
	    command = ATA_C_READ48; break;
	case ATA_C_READ_MUL:
	    command = ATA_C_READ_MUL48; break;
	case ATA_C_READ_DMA:
	    command = ATA_C_READ_DMA48; break;
	case ATA_C_READ_DMA_QUEUED:
	    command = ATA_C_READ_DMA_QUEUED48; break;
	case ATA_C_WRITE:
	    command = ATA_C_WRITE48; break;
	case ATA_C_WRITE_MUL:
	    command = ATA_C_WRITE_MUL48; break;
	case ATA_C_WRITE_DMA:
	    command = ATA_C_WRITE_DMA48; break;
	case ATA_C_WRITE_DMA_QUEUED:
	    command = ATA_C_WRITE_DMA_QUEUED48; break;
	case ATA_C_FLUSHCACHE:
	    command = ATA_C_FLUSHCACHE48; break;
	default:
	    ata_prtdev(atadev, "can't translate cmd to 48bit version\n");
	    return -1;
	}
    }
    else {
	ATA_OUTB(atadev->channel->r_io, ATA_FEATURE, feature);
	ATA_OUTB(atadev->channel->r_io, ATA_COUNT, count);
	ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, lba & 0xff);
	ATA_OUTB(atadev->channel->r_io, ATA_CYL_LSB, (lba>>8) & 0xff);
	ATA_OUTB(atadev->channel->r_io, ATA_CYL_MSB, (lba>>16) & 0xff);
	if (atadev->flags & ATA_D_USE_CHS)
	    ATA_OUTB(atadev->channel->r_io, ATA_DRIVE,
		     ATA_D_IBM | atadev->unit | ((lba>>24) & 0xf));
	else
	    ATA_OUTB(atadev->channel->r_io, ATA_DRIVE,
		     ATA_D_IBM | ATA_D_LBA | atadev->unit | ((lba>>24) &0xf));
    }

    switch (flags & ATA_WAIT_MASK) {
    case ATA_IMMEDIATE:
	ATA_OUTB(atadev->channel->r_io, ATA_CMD, command);

	/* enable interrupt */
	if (atadev->channel->flags & ATA_QUEUED)
	    ATA_OUTB(atadev->channel->r_altio, ATA_ALTSTAT, ATA_A_4BIT);
	break;

    case ATA_WAIT_INTR:
	atadev->channel->active |= ATA_WAIT_INTR;
	ATA_OUTB(atadev->channel->r_io, ATA_CMD, command);

	/* enable interrupt */
	if (atadev->channel->flags & ATA_QUEUED)
	    ATA_OUTB(atadev->channel->r_altio, ATA_ALTSTAT, ATA_A_4BIT);

	if (tsleep((caddr_t)atadev->channel, PRIBIO, "atacmd", 10 * hz)) {
	    ata_prtdev(atadev, "timeout waiting for interrupt\n");
	    atadev->channel->active &= ~ATA_WAIT_INTR;
	    error = -1;
	}
	break;
    
    case ATA_WAIT_READY:
	atadev->channel->active |= ATA_WAIT_READY;
	ATA_OUTB(atadev->channel->r_io, ATA_CMD, command);
	if (ata_wait(atadev, ATA_S_READY) < 0) { 
	    ata_prtdev(atadev, "timeout waiting for cmd=%02x s=%02x e=%02x\n",
		       command, atadev->channel->status,atadev->channel->error);
	    error = -1;
	}
	atadev->channel->active &= ~ATA_WAIT_READY;
	break;
    }
    return error;
}

static void
ata_drawer_start(struct ata_device *atadev)
{
    ATA_INB(atadev->channel->r_io, ATA_DRIVE);	  
    DELAY(1);
    ATA_OUTB(atadev->channel->r_io, ATA_DRIVE, ATA_D_IBM | atadev->unit);    
    DELAY(1);
    ATA_OUTB(atadev->channel->r_io, ATA_DRIVE, ATA_D_IBM | atadev->unit);    
    DELAY(1);
    ATA_OUTB(atadev->channel->r_io, ATA_DRIVE, ATA_D_IBM | atadev->unit);    
    DELAY(1);
    ATA_INB(atadev->channel->r_io, ATA_COUNT);
    DELAY(1);
    ATA_INB(atadev->channel->r_io, ATA_DRIVE);
    DELAY(1);
}

static void
ata_drawer_end(struct ata_device *atadev)
{
    ATA_OUTB(atadev->channel->r_io, ATA_DRIVE, ATA_D_IBM | atadev->unit);    
    DELAY(1);
}

static void
ata_chip_start(struct ata_device *atadev)
{
    ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, 0x0b);
    ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, 0x0a);
    DELAY(25);
    ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, 0x08);
}

static void
ata_chip_end(struct ata_device *atadev)
{
    ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, 0x08);
    DELAY(64);
    ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, 0x0a);
    DELAY(25);
    ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, 0x0b);
    DELAY(64);
}

static u_int8_t
ata_chip_rdbit(struct ata_device *atadev)
{
    u_int8_t val;

    ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, 0);
    DELAY(64);
    ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, 0x02);
    DELAY(25);
    val = ATA_INB(atadev->channel->r_io, ATA_SECTOR) & 0x01;
    DELAY(38);
    return val;
}

static void
ata_chip_wrbit(struct ata_device *atadev, u_int8_t data)
{
    ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, 0x08 | (data & 0x01));
    DELAY(64);
    ATA_OUTB(atadev->channel->r_io, ATA_SECTOR, 0x08 | 0x02 | (data & 0x01));
    DELAY(64);
}

static u_int8_t
ata_chip_rw(struct ata_device *atadev, int rw, u_int8_t val)
{
    int i;

    if (rw) {
	for (i = 0; i < 8; i++)
	    ata_chip_wrbit(atadev, (val & (0x80 >> i)) ? 1 : 0);
    }
    else {
	for (i = 0; i < 8; i++)
	    val = (val << 1) | ata_chip_rdbit(atadev);
    }
    ata_chip_wrbit(atadev, 0);
    return val;
}

static u_int8_t
ata_drawersensor(struct ata_device *atadev, int rw, u_int8_t idx, u_int8_t data)
{
    ata_drawer_start(atadev);
    ata_chip_start(atadev);
    ata_chip_rw(atadev, 1, 0x5a);
    ata_chip_rw(atadev, 1, idx);
    if (rw) {
	ata_chip_rw(atadev, 1, data);
    }
    else {
	ata_chip_end(atadev);
	ata_chip_start(atadev);
	ata_chip_rw(atadev, 1, 0x5b);
	data = ata_chip_rw(atadev, 0, 0);
    }
    ata_chip_end(atadev); 
    ata_drawer_end(atadev);
    return data;
}

void
ata_drawerleds(struct ata_device *atadev, u_int8_t color)
{
    ata_drawer_start(atadev);
    ATA_OUTB(atadev->channel->r_io, ATA_COUNT, color);	  
    DELAY(1);
    ata_drawer_end(atadev);
}

static void
ata_change_mode(struct ata_device *atadev, int mode)
{
    int umode, wmode, pmode;

    umode = ata_umode(atadev->param);
    wmode = ata_wmode(atadev->param);
    pmode = ata_pmode(atadev->param);
    
    switch (mode & ATA_DMA_MASK) {
    case ATA_UDMA:
	if ((mode & ATA_MODE_MASK) < umode)
	    umode = mode & ATA_MODE_MASK;
	break;
    case ATA_WDMA:
	if ((mode & ATA_MODE_MASK) < wmode)
	    wmode = mode & ATA_MODE_MASK;
	umode = -1;
	break;
    default:
	if (((mode & ATA_MODE_MASK) - ATA_PIO0) < pmode)
	    pmode = (mode & ATA_MODE_MASK) - ATA_PIO0;
	umode = -1;
	wmode = -1;
    }

    ATA_SLEEPLOCK_CH(atadev->channel, ATA_ACTIVE);
    ata_dmainit(atadev->channel, atadev->unit, pmode, wmode, umode);
    ATA_UNLOCK_CH(atadev->channel);
    ata_start(atadev->channel); /* XXX SOS */
}

int
ata_printf(struct ata_channel *ch, int device, const char * fmt, ...)
{
    va_list ap;
    int ret;

    if (device == -1)
	ret = printf("ata%d: ", device_get_unit(ch->dev));
    else {
	if (ch->device[ATA_DEV(device)].name)
	    ret = printf("%s: ", ch->device[ATA_DEV(device)].name);
	else
	    ret = printf("ata%d-%s: ", device_get_unit(ch->dev),
			 (device == ATA_MASTER) ? "master" : "slave");
    }
    va_start(ap, fmt);
    ret += vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

int
ata_prtdev(struct ata_device *atadev, const char * fmt, ...)
{
    va_list ap;
    int ret;

    if (atadev->name)
	ret = printf("%s: ", atadev->name);
    else
	ret = printf("ata%d-%s: ", device_get_unit(atadev->channel->dev),
		     (atadev->unit == ATA_MASTER) ? "master" : "slave");
    va_start(ap, fmt);
    ret += vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

void
ata_set_name(struct ata_device *atadev, char *name, int lun)
{
    atadev->name = malloc(strlen(name) + 4, M_ATA, M_NOWAIT);
    if (atadev->name)
	sprintf(atadev->name, "%s%d", name, lun);
}

void
ata_free_name(struct ata_device *atadev)
{
    if (atadev->name)
	free(atadev->name, M_ATA);
    atadev->name = NULL;
}
    
int
ata_get_lun(u_int32_t *map)
{
    int lun = ffs(~*map) - 1;

    *map |= (1 << lun);
    return lun;
}

int
ata_test_lun(u_int32_t *map, int lun)
{
    return (*map & (1 << lun));
}

void
ata_free_lun(u_int32_t *map, int lun)
{
    *map &= ~(1 << lun);
}
 
char *
ata_mode2str(int mode)
{
    switch (mode) {
    case ATA_PIO: return "BIOSPIO";
    case ATA_PIO0: return "PIO0";
    case ATA_PIO1: return "PIO1";
    case ATA_PIO2: return "PIO2";
    case ATA_PIO3: return "PIO3";
    case ATA_PIO4: return "PIO4";
    case ATA_DMA: return "BIOSDMA";
    case ATA_WDMA2: return "WDMA2";
    case ATA_UDMA2: return "UDMA33";
    case ATA_UDMA4: return "UDMA66";
    case ATA_UDMA5: return "UDMA100";
    case ATA_UDMA6: return "UDMA133";
    default: return "???";
    }
}

int
ata_pmode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_64_70) {
	if (ap->apiomodes & 2)
	    return 4;
	if (ap->apiomodes & 1) 
	    return 3;
    }	
    if (ap->retired_piomode == 2)
	return 2;
    if (ap->retired_piomode == 1)
	return 1;
    if (ap->retired_piomode == 0)
	return 0;
    return -1; 
} 

int
ata_wmode(struct ata_params *ap)
{
    if (ap->mwdmamodes & 0x04)
	return 2;
    if (ap->mwdmamodes & 0x02)
	return 1;
    if (ap->mwdmamodes & 0x01)
	return 0;
    return -1;
}

int
ata_umode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_88) {
	if (ap->udmamodes & 0x40)
	    return 6;
	if (ap->udmamodes & 0x20)
	    return 5;
	if (ap->udmamodes & 0x10)
	    return 4;
	if (ap->udmamodes & 0x08)
	    return 3;
	if (ap->udmamodes & 0x04)
	    return 2;
	if (ap->udmamodes & 0x02)
	    return 1;
	if (ap->udmamodes & 0x01)
	    return 0;
    }
    return -1;
}

static void
bswap(int8_t *buf, int len) 
{
    u_int16_t *ptr = (u_int16_t*)(buf + len);

    while (--ptr >= (u_int16_t*)buf)
	*ptr = ntohs(*ptr);
} 

static void
btrim(int8_t *buf, int len)
{ 
    int8_t *ptr;

    for (ptr = buf; ptr < buf+len; ++ptr) 
	if (!*ptr)
	    *ptr = ' ';
    for (ptr = buf + len - 1; ptr >= buf && *ptr == ' '; --ptr)
	*ptr = 0;
}

static void
bpack(int8_t *src, int8_t *dst, int len)
{
    int i, j, blank;

    for (i = j = blank = 0 ; i < len; i++) {
	if (blank && src[i] == ' ') continue;
	if (blank && src[i] != ' ') {
	    dst[j++] = src[i];
	    blank = 0;
	    continue;
	}
	if (src[i] == ' ') {
	    blank = 1;
	    if (i == 0)
		continue;
	}
	dst[j++] = src[i];
    }
    if (j < len) 
	dst[j] = 0x00;
}

static void
ata_init(void)
{
    /* register controlling device */
    make_dev(&ata_cdevsw, 0, UID_ROOT, GID_OPERATOR, 0600, "ata");

    /* register boot attach to be run when interrupts are enabled */
    if (!(ata_delayed_attach = (struct intr_config_hook *)
			       malloc(sizeof(struct intr_config_hook),
				      M_TEMP, M_NOWAIT | M_ZERO))) {
	printf("ata: malloc of delayed attach hook failed\n");
	return;
    }

    ata_delayed_attach->ich_func = (void*)ata_boot_attach;
    if (config_intrhook_establish(ata_delayed_attach) != 0) {
	printf("ata: config_intrhook_establish failed\n");
	free(ata_delayed_attach, M_TEMP);
    }
}
SYSINIT(atadev, SI_SUB_DRIVERS, SI_ORDER_SECOND, ata_init, NULL)
