/*-
 * Copyright (c) 1998,1999,2000,2001 Søren Schmidt
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

#include "pci.h"
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
#include <dev/ata/atapi-all.h>

/* device structures */
static  d_ioctl_t       ataioctl;
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
static int ata_getparam(struct ata_softc *, int, u_int8_t);
static int ata_service(struct ata_softc *);
static char *active2str(int);
static void bswap(int8_t *, int);
static void btrim(int8_t *, int);
static void bpack(int8_t *, int8_t *, int);
static void ata_change_mode(struct ata_softc *, int, int);

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, ata, CTLFLAG_RD, 0, "ATA driver parameters");

/* global vars */
devclass_t ata_devclass;

/* local vars */
static struct intr_config_hook *ata_delayed_attach = NULL;
static MALLOC_DEFINE(M_ATA, "ATA generic", "ATA driver generic layer");

/* misc defines */
#define MASTER	0
#define SLAVE	1

int
ata_probe(device_t dev)
{
    struct ata_softc *scp;
    int rid;

    if (!dev)
	return ENXIO;
    scp = device_get_softc(dev);
    if (!scp)
	return ENXIO;
    if (scp->r_io || scp->r_altio || scp->r_irq)
	return EEXIST;

    /* initialize the softc basics */
    scp->active = ATA_IDLE;
    scp->dev = dev;

    rid = ATA_IOADDR_RID;
    scp->r_io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 
				   ATA_IOSIZE, RF_ACTIVE);
    if (!scp->r_io)
	goto failure;

    rid = ATA_ALTADDR_RID;
    scp->r_altio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
				      ATA_ALTIOSIZE, RF_ACTIVE);
    if (!scp->r_altio)
	goto failure;

    rid = ATA_BMADDR_RID;
    scp->r_bmio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
				     ATA_BMIOSIZE, RF_ACTIVE);
    if (bootverbose)
	ata_printf(scp, -1, "iobase=0x%04x altiobase=0x%04x bmaddr=0x%04x\n", 
		   (int)rman_get_start(scp->r_io),
		   (int)rman_get_start(scp->r_altio),
		   (scp->r_bmio) ? (int)rman_get_start(scp->r_bmio) : 0);

    ata_reset(scp);

    TAILQ_INIT(&scp->ata_queue);
    TAILQ_INIT(&scp->atapi_queue);
    return 0;
    
failure:
    if (scp->r_io)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, scp->r_io);
    if (scp->r_altio)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_ALTADDR_RID,scp->r_altio);
    if (scp->r_bmio)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_BMADDR_RID, scp->r_bmio);
    if (bootverbose)
	ata_printf(scp, -1, "probe allocation failed\n");
    return ENXIO;
}

int
ata_attach(device_t dev)
{
    struct ata_softc *scp;
    int error, rid;

    if (!dev)
	return ENXIO;
    scp = device_get_softc(dev);
    if (!scp)
	return ENXIO;

    rid = ATA_IRQ_RID;
    scp->r_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
				    RF_SHAREABLE | RF_ACTIVE);
    if (!scp->r_irq) {
	ata_printf(scp, -1, "unable to allocate interrupt\n");
	return ENXIO;
    }
    if ((error = bus_setup_intr(dev, scp->r_irq, INTR_TYPE_BIO|INTR_ENTROPY,
				ata_intr, scp, &scp->ih)))
	return error;

    /*
     * do not attach devices if we are in early boot, this is done later 
     * when interrupts are enabled by a hook into the boot process.
     * otherwise attach what the probe has found in scp->devices.
     */
    if (!ata_delayed_attach) {
	if (scp->devices & ATA_ATA_SLAVE)
	    if (ata_getparam(scp, ATA_SLAVE, ATA_C_ATA_IDENTIFY))
		scp->devices &= ~ATA_ATA_SLAVE;
	if (scp->devices & ATA_ATAPI_SLAVE)
	    if (ata_getparam(scp, ATA_SLAVE, ATA_C_ATAPI_IDENTIFY))
		scp->devices &= ~ATA_ATAPI_SLAVE;
	if (scp->devices & ATA_ATA_MASTER)
	    if (ata_getparam(scp, ATA_MASTER, ATA_C_ATA_IDENTIFY))
		scp->devices &= ~ATA_ATA_MASTER;
	if (scp->devices & ATA_ATAPI_MASTER)
	    if (ata_getparam(scp, ATA_MASTER,ATA_C_ATAPI_IDENTIFY))
		scp->devices &= ~ATA_ATAPI_MASTER;
#ifdef DEV_ATADISK
	if (scp->devices & ATA_ATA_MASTER)
	    ad_attach(scp, ATA_MASTER);
	if (scp->devices & ATA_ATA_SLAVE)
	    ad_attach(scp, ATA_SLAVE);
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
	if (scp->devices & ATA_ATAPI_MASTER)
	    atapi_attach(scp, ATA_MASTER);
	if (scp->devices & ATA_ATAPI_SLAVE)
	    atapi_attach(scp, ATA_SLAVE);
#endif
    }
    return 0;
}

int
ata_detach(device_t dev)
{
    struct ata_softc *scp;
    int s;
 
    if (!dev)
	return ENXIO;
    scp = device_get_softc(dev);
    if (!scp || !scp->devices)
	return ENXIO;

    /* make sure channel is not busy SOS XXX */
    s = splbio();
    while (!atomic_cmpset_int(&scp->active, ATA_IDLE, ATA_CONTROL))
        tsleep((caddr_t)&s, PRIBIO, "atachm", hz/4);
    splx(s);

    /* disable interrupts on devices */
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_IDS | ATA_A_4BIT);
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_IDS | ATA_A_4BIT);

#ifdef DEV_ATADISK
    if (scp->devices & ATA_ATA_MASTER && scp->dev_softc[MASTER])
	ad_detach(scp->dev_softc[MASTER], 1);
    if (scp->devices & ATA_ATA_SLAVE && scp->dev_softc[SLAVE])
	ad_detach(scp->dev_softc[SLAVE], 1);
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    if (scp->devices & ATA_ATAPI_MASTER && scp->dev_softc[MASTER])
	atapi_detach(scp->dev_softc[MASTER]);
    if (scp->devices & ATA_ATAPI_SLAVE && scp->dev_softc[SLAVE])
	atapi_detach(scp->dev_softc[SLAVE]);
#endif

    if (scp->dev_param[MASTER]) {
	free(scp->dev_param[MASTER], M_ATA);
	scp->dev_param[MASTER] = NULL;
    }
    if (scp->dev_param[SLAVE]) {
	free(scp->dev_param[SLAVE], M_ATA);
	scp->dev_param[SLAVE] = NULL;
    }
    scp->dev_softc[MASTER] = NULL;
    scp->dev_softc[SLAVE] = NULL;
    scp->mode[MASTER] = ATA_PIO;
    scp->mode[SLAVE] = ATA_PIO;
    scp->devices = 0;

    bus_teardown_intr(dev, scp->r_irq, scp->ih);
    bus_release_resource(dev, SYS_RES_IRQ, ATA_IRQ_RID, scp->r_irq);
    if (scp->r_bmio)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_BMADDR_RID, scp->r_bmio);
    bus_release_resource(dev, SYS_RES_IOPORT, ATA_ALTADDR_RID, scp->r_altio);
    bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, scp->r_io);
    scp->r_io = NULL;
    scp->r_altio = NULL;
    scp->r_bmio = NULL;
    scp->r_irq = NULL;
    scp->active = ATA_IDLE;
    return 0;
}

int
ata_resume(device_t dev)
{
    struct ata_softc *scp = device_get_softc(dev);

    ata_reinit(scp);
    return 0;
}

static int
ataioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    int error = 0;

    switch (cmd) {
	case ATAATTACH: {
	    device_t device = devclass_get_device(ata_devclass, *(int *)addr);
	    /* should enable channel HW on controller that can SOS XXX */   
	    if (!device)
		error = ENXIO;
	    if (!error)
		error = ata_probe(device);
	    if (!error)
		error = ata_attach(device);
	    break;
	}

	case ATADETACH: {
	    device_t device = devclass_get_device(ata_devclass, *(int *)addr);
	    if (!device)
		error = ENXIO;
	    if (!error)
		error = ata_detach(device);
	    /* should disable channel HW on controller that can SOS XXX */   
	    break;
	}

	case ATAREINIT: {
	    device_t device = devclass_get_device(ata_devclass, *(int *)addr);
	    struct ata_softc *scp;
	    int s;

	    if (!device)
		return ENXIO;
	    scp = device_get_softc(device);
	    if (!scp)
		return ENXIO;

	    /* make sure channel is not busy SOS XXX */
	    s = splbio();
	    while (!atomic_cmpset_int(&scp->active, ATA_IDLE, ATA_ACTIVE))
        	tsleep((caddr_t)&s, PRIBIO, "atachm", hz/4);
	    error = ata_reinit(scp);
	    splx(s);
	    break;
	}

	case ATAGMODE: {
	    struct ata_modes *mode = (struct ata_modes *)addr;
	    device_t device = devclass_get_device(ata_devclass, mode->channel);
	    struct ata_softc *scp;

	    if (!device)
		return ENXIO;
	    scp = device_get_softc(device);
	    if (!scp)
		return ENXIO;
	    if (scp->dev_param[MASTER])
		mode->mode[MASTER] = scp->mode[MASTER];
	    if (scp->dev_param[SLAVE])
		mode->mode[SLAVE] = scp->mode[SLAVE];
	    break;
	}

	case ATASMODE: {
	    struct ata_modes *mode = (struct ata_modes *)addr;
	    device_t device = devclass_get_device(ata_devclass, mode->channel);
	    struct ata_softc *scp;

	    if (!device)
		return ENXIO;
	    scp = device_get_softc(device);
	    if (!scp)
		return ENXIO;
	    if (scp->dev_param[MASTER] && mode->mode[MASTER] >= 0) {
		ata_change_mode(scp, ATA_MASTER, mode->mode[MASTER]);
		mode->mode[MASTER] = scp->mode[MASTER];
	    }
	    if (scp->dev_param[SLAVE] && mode->mode[SLAVE] >= 0) {
		ata_change_mode(scp, ATA_SLAVE, mode->mode[SLAVE]);
		mode->mode[SLAVE] = scp->mode[SLAVE];
	    }
	    break;
	}

	case ATAGPARM: {
	    struct ata_param *parm = (struct ata_param *)addr;
	    device_t device = devclass_get_device(ata_devclass, parm->channel);
	    struct ata_softc *scp;

	    if (!device)
		return ENXIO;
	    scp = device_get_softc(device);
	    if (!scp)
		return ENXIO;

	    parm->type[MASTER] = 
		scp->devices & (ATA_ATA_MASTER | ATA_ATAPI_MASTER);
	    parm->type[SLAVE] =
		scp->devices & (ATA_ATA_SLAVE | ATA_ATAPI_SLAVE);

	    if (scp->dev_name[MASTER])
		strcpy(parm->name[MASTER], scp->dev_name[MASTER]);
	    if (scp->dev_name[SLAVE])
		strcpy(parm->name[SLAVE], scp->dev_name[SLAVE]);

	    if (scp->dev_param[MASTER])
		bcopy(scp->dev_param[MASTER], &parm->params[MASTER],
		      sizeof(struct ata_params));
	    if (scp->dev_param[SLAVE])
		bcopy(scp->dev_param[SLAVE], &parm->params[SLAVE],
		      sizeof(struct ata_params));
	    break;
	}

	default:
	    error = ENOTTY;
    }
    return error;
}

static int
ata_getparam(struct ata_softc *scp, int device, u_int8_t command)
{
    struct ata_params *ata_parm;
    int8_t buffer[DEV_BSIZE];
    int retry = 0;

    /* select drive */
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | device);
    DELAY(1);

    /* enable interrupt */
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_4BIT);
    DELAY(1);

    /* apparently some devices needs this repeated */
    do {
	if (ata_command(scp, device, command, 0, 0, 0, 0, 0, ATA_WAIT_INTR)) {
	    ata_printf(scp, device, "identify failed\n");
	    return -1;
	}
	if (retry++ > 4) {
	    ata_printf(scp, device, "identify retries exceeded\n");
	    return -1;
	}
    } while (ata_wait(scp, device, 
		      ((command == ATA_C_ATAPI_IDENTIFY) ?
			ATA_S_DRQ : (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ))));

    ATA_INSW(scp->r_io, ATA_DATA, (int16_t *)buffer,
	     sizeof(buffer)/sizeof(int16_t));
    ata_parm = malloc(sizeof(struct ata_params), M_ATA, M_NOWAIT);
    if (!ata_parm) {
	ata_printf(scp, device, "malloc for identify data failed\n");
        return -1;
    }
    bcopy(buffer, ata_parm, sizeof(struct ata_params));   
    if (command == ATA_C_ATA_IDENTIFY ||
	!((ata_parm->model[0] == 'N' && ata_parm->model[1] == 'E') ||
          (ata_parm->model[0] == 'F' && ata_parm->model[1] == 'X')))
        bswap(ata_parm->model, sizeof(ata_parm->model));
    btrim(ata_parm->model, sizeof(ata_parm->model));
    bpack(ata_parm->model, ata_parm->model, sizeof(ata_parm->model));
    bswap(ata_parm->revision, sizeof(ata_parm->revision));
    btrim(ata_parm->revision, sizeof(ata_parm->revision));
    bpack(ata_parm->revision, ata_parm->revision, sizeof(ata_parm->revision));
    scp->dev_param[ATA_DEV(device)] = ata_parm;
    return 0;
}

static void 
ata_boot_attach(void)
{
    struct ata_softc *scp;
    int ctlr;

    /*
     * run through all ata devices and look for real ATA & ATAPI devices
     * using the hints we found in the early probe, this avoids some of
     * the delays probing of non-exsistent devices can cause.
     */
    for (ctlr=0; ctlr<devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(scp = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	if (scp->devices & ATA_ATA_SLAVE)
	    if (ata_getparam(scp, ATA_SLAVE, ATA_C_ATA_IDENTIFY))
		scp->devices &= ~ATA_ATA_SLAVE;
	if (scp->devices & ATA_ATAPI_SLAVE)
	    if (ata_getparam(scp, ATA_SLAVE, ATA_C_ATAPI_IDENTIFY))
		scp->devices &= ~ATA_ATAPI_SLAVE;
	if (scp->devices & ATA_ATA_MASTER)
	    if (ata_getparam(scp, ATA_MASTER, ATA_C_ATA_IDENTIFY))
		scp->devices &= ~ATA_ATA_MASTER;
	if (scp->devices & ATA_ATAPI_MASTER)
	    if (ata_getparam(scp, ATA_MASTER, ATA_C_ATAPI_IDENTIFY))
		scp->devices &= ~ATA_ATAPI_MASTER;
    }

#ifdef DEV_ATADISK
    /* now we know whats there, do the real attach, first the ATA disks */
    for (ctlr=0; ctlr<devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(scp = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	if (scp->devices & ATA_ATA_MASTER)
	    ad_attach(scp, ATA_MASTER);
	if (scp->devices & ATA_ATA_SLAVE)
	    ad_attach(scp, ATA_SLAVE);
    }
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    /* then the atapi devices */
    for (ctlr=0; ctlr<devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(scp = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	if (scp->devices & ATA_ATAPI_MASTER)
	    atapi_attach(scp, ATA_MASTER);
	if (scp->devices & ATA_ATAPI_SLAVE)
	    atapi_attach(scp, ATA_SLAVE);
    }
#endif
    if (ata_delayed_attach) {
	config_intrhook_disestablish(ata_delayed_attach);
	free(ata_delayed_attach, M_ATA);
	ata_delayed_attach = NULL;
    }
}

static void
ata_intr(void *data)
{
    struct ata_softc *scp = (struct ata_softc *)data;

    /* 
     * on PCI systems we might share an interrupt line with another
     * device or our twin ATA channel, so call scp->intr_func to figure 
     * out if it is really an interrupt we should process here
     */
    if (scp->intr_func && scp->intr_func(scp))
	return;

    /* if drive is busy it didn't interrupt */
    if (ATA_INB(scp->r_altio, ATA_ALTSTAT) & ATA_S_BUSY) {
	DELAY(100);
	if (!(ATA_INB(scp->r_altio, ATA_ALTSTAT) & ATA_S_DRQ))
	    return;
    }

    /* clear interrupt and get status */
    scp->status = ATA_INB(scp->r_io, ATA_STATUS);

    if (scp->status & ATA_S_ERROR)
	scp->error = ATA_INB(scp->r_io, ATA_ERROR);

    /* find & call the responsible driver to process this interrupt */
    switch (scp->active) {
#ifdef DEV_ATADISK
    case ATA_ACTIVE_ATA:
	if (!scp->running || ad_interrupt(scp->running) == ATA_OP_CONTINUES)
	    return;
	break;
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    case ATA_ACTIVE_ATAPI:
	if (!scp->running || atapi_interrupt(scp->running) == ATA_OP_CONTINUES)
	    return;
	break;
#endif
    case ATA_WAIT_INTR:
    case ATA_WAIT_INTR | ATA_CONTROL:
	wakeup((caddr_t)scp);
	break;

    case ATA_WAIT_READY:
    case ATA_WAIT_READY | ATA_CONTROL:
	break;

    case ATA_IDLE:
	if (scp->flags & ATA_QUEUED) {
	    scp->active = ATA_ACTIVE; /* XXX */
	    if (ata_service(scp) == ATA_OP_CONTINUES)
		return;
	}
	/* FALLTHROUGH */

    default:
#ifdef ATA_DEBUG
    {
	static int intr_count = 0;

	if (intr_count++ < 10)
	    ata_printf(scp, -1, "unwanted interrupt %d %sstatus = %02x\n", 
		       intr_count, active2str(scp->active), scp->status);
    }
#endif
    }
    scp->active &= ATA_CONTROL;
    if (scp->active & ATA_CONTROL)
	return;
    scp->running = NULL;
    ata_start(scp);
    return;
}

void
ata_start(struct ata_softc *scp)
{
#ifdef DEV_ATADISK
    struct ad_request *ad_request; 
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    struct atapi_request *atapi_request;
#endif

    if (!atomic_cmpset_int(&scp->active, ATA_IDLE, ATA_ACTIVE))
	return;

#ifdef DEV_ATADISK
    /* find & call the responsible driver if anything on the ATA queue */
    if (TAILQ_EMPTY(&scp->ata_queue)) {
	if (scp->devices & (ATA_ATA_MASTER) && scp->dev_softc[MASTER])
	    ad_start((struct ad_softc *)scp->dev_softc[MASTER]);
	if (scp->devices & (ATA_ATA_SLAVE) && scp->dev_softc[SLAVE])
	    ad_start((struct ad_softc *)scp->dev_softc[SLAVE]);
    }
    if ((ad_request = TAILQ_FIRST(&scp->ata_queue))) {
	TAILQ_REMOVE(&scp->ata_queue, ad_request, chain);
	scp->active = ATA_ACTIVE_ATA;
	scp->running = ad_request;
	if (ad_transfer(ad_request) == ATA_OP_CONTINUES)
	    return;
    }

#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    /* find & call the responsible driver if anything on the ATAPI queue */
    if (TAILQ_EMPTY(&scp->atapi_queue)) {
	if (scp->devices & (ATA_ATAPI_MASTER) && scp->dev_softc[MASTER])
	    atapi_start((struct atapi_softc *)scp->dev_softc[MASTER]);
	if (scp->devices & (ATA_ATAPI_SLAVE) && scp->dev_softc[SLAVE])
	    atapi_start((struct atapi_softc *)scp->dev_softc[SLAVE]);
    }
    if ((atapi_request = TAILQ_FIRST(&scp->atapi_queue))) {
	TAILQ_REMOVE(&scp->atapi_queue, atapi_request, chain);
	scp->active = ATA_ACTIVE_ATAPI;
	scp->running = atapi_request;
	atapi_transfer(atapi_request);
	return;
    }
#endif
    scp->active = ATA_IDLE;
}

void
ata_reset(struct ata_softc *scp)
{
    u_int8_t lsb, msb, ostat0, ostat1;
    u_int8_t stat0 = ATA_S_BUSY, stat1 = ATA_S_BUSY;
    int mask = 0, timeout;

    /* do we have any signs of ATA/ATAPI HW being present ? */
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
    DELAY(10);
    ostat0 = ATA_INB(scp->r_io, ATA_STATUS);
    if ((ostat0 & 0xf8) != 0xf8 && ostat0 != 0xa5)
	mask |= 0x01;
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
    DELAY(10);	
    ostat1 = ATA_INB(scp->r_io, ATA_STATUS);
    if ((ostat1 & 0xf8) != 0xf8 && ostat1 != 0xa5)
	mask |= 0x02;

    scp->devices = 0;
    if (!mask)
	return;

    /* in some setups we dont want to test for a slave */
    if (scp->flags & ATA_NO_SLAVE)
	mask &= ~0x02;

    if (bootverbose)
	ata_printf(scp, -1, "mask=%02x ostat0=%02x ostat2=%02x\n",
		   mask, ostat0, ostat1);

    /* reset channel */
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_IDS | ATA_A_RESET);
    DELAY(10000); 
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_IDS);
    DELAY(100000);
    ATA_INB(scp->r_io, ATA_ERROR);

    /* wait for BUSY to go inactive */
    for (timeout = 0; timeout < 310000; timeout++) {
	if (stat0 & ATA_S_BUSY) {
            ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
            DELAY(10);
            stat0 = ATA_INB(scp->r_io, ATA_STATUS);
            if (!(stat0 & ATA_S_BUSY)) {
                /* check for ATAPI signature while its still there */
		lsb = ATA_INB(scp->r_io, ATA_CYL_LSB);
		msb = ATA_INB(scp->r_io, ATA_CYL_MSB);
		if (bootverbose)
		    ata_printf(scp, ATA_MASTER,
			       "ATAPI probe %02x %02x\n", lsb, msb);
		if (lsb == ATAPI_MAGIC_LSB && msb == ATAPI_MAGIC_MSB)
                    scp->devices |= ATA_ATAPI_MASTER;
            }
        }
        if (stat1 & ATA_S_BUSY) {
            ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
            DELAY(10);
            stat1 = ATA_INB(scp->r_io, ATA_STATUS);
            if (!(stat1 & ATA_S_BUSY)) {
                /* check for ATAPI signature while its still there */
		lsb = ATA_INB(scp->r_io, ATA_CYL_LSB);
		msb = ATA_INB(scp->r_io, ATA_CYL_MSB);
		if (bootverbose)
		    ata_printf(scp, ATA_SLAVE,
			       "ATAPI probe %02x %02x\n", lsb, msb);
		if (lsb == ATAPI_MAGIC_LSB && msb == ATAPI_MAGIC_MSB)
                    scp->devices |= ATA_ATAPI_SLAVE;
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
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_4BIT);

    if (stat0 & ATA_S_BUSY)
	mask &= ~0x01;
    if (stat1 & ATA_S_BUSY)
	mask &= ~0x02;
    if (bootverbose)
	ata_printf(scp, -1, "mask=%02x stat0=%02x stat1=%02x\n", 
		   mask, stat0, stat1);
    if (!mask)
	return;

    if (mask & 0x01 && ostat0 != 0x00 && !(scp->devices & ATA_ATAPI_MASTER)) {
        ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
        DELAY(10);
	ATA_OUTB(scp->r_io, ATA_ERROR, 0x58);
	ATA_OUTB(scp->r_io, ATA_CYL_LSB, 0xa5);
	lsb = ATA_INB(scp->r_io, ATA_ERROR);
	msb = ATA_INB(scp->r_io, ATA_CYL_LSB);
	if (bootverbose)
	    ata_printf(scp, ATA_MASTER, "ATA probe %02x %02x\n", lsb, msb);
        if (lsb != 0x58 && msb == 0xa5)
            scp->devices |= ATA_ATA_MASTER;
    }
    if (mask & 0x02 && ostat1 != 0x00 && !(scp->devices & ATA_ATAPI_SLAVE)) {
        ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
        DELAY(10);
	ATA_OUTB(scp->r_io, ATA_ERROR, 0x58);
	ATA_OUTB(scp->r_io, ATA_CYL_LSB, 0xa5);
	lsb = ATA_INB(scp->r_io, ATA_ERROR);
	msb = ATA_INB(scp->r_io, ATA_CYL_LSB);
	if (bootverbose)
	    ata_printf(scp, ATA_SLAVE, "ATA probe %02x %02x\n", lsb, msb);
        if (lsb != 0x58 && msb == 0xa5)
            scp->devices |= ATA_ATA_SLAVE;
    }
    if (bootverbose)
	ata_printf(scp, -1, "devices=%02x\n", scp->devices);
}

int
ata_reinit(struct ata_softc *scp)
{
    int devices, misdev, newdev;

    if (!scp->r_io || !scp->r_altio || !scp->r_irq)
	return ENXIO;
    scp->active = ATA_CONTROL;
    scp->running = NULL;
    devices = scp->devices;
    ata_printf(scp, -1, "resetting devices .. ");
    ata_reset(scp);

    if ((misdev = devices & ~scp->devices)) {
	if (misdev)
	    printf("\n");
#ifdef DEV_ATADISK
	if (misdev & ATA_ATA_MASTER && scp->dev_softc[MASTER])
	    ad_detach(scp->dev_softc[MASTER], 0);
	if (misdev & ATA_ATA_SLAVE && scp->dev_softc[SLAVE])
	    ad_detach(scp->dev_softc[SLAVE], 0);
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
	if (misdev & ATA_ATAPI_MASTER && scp->dev_softc[MASTER])
	    atapi_detach(scp->dev_softc[MASTER]);
	if (misdev & ATA_ATAPI_SLAVE && scp->dev_softc[SLAVE])
	    atapi_detach(scp->dev_softc[SLAVE]);
#endif
	if (misdev & ATA_ATA_MASTER || misdev & ATA_ATAPI_MASTER) {
	    free(scp->dev_param[MASTER], M_ATA);
	    scp->dev_param[MASTER] = NULL;
	}
	if (misdev & ATA_ATA_SLAVE || misdev & ATA_ATAPI_SLAVE) {
	    free(scp->dev_param[SLAVE], M_ATA);
	    scp->dev_param[SLAVE] = NULL;
	}
    }
    if ((newdev = ~devices & scp->devices)) {
	if (newdev & ATA_ATA_MASTER)
	    if (ata_getparam(scp, ATA_MASTER, ATA_C_ATA_IDENTIFY))
		newdev &= ~ATA_ATA_MASTER;
	if (newdev & ATA_ATA_SLAVE)
	    if (ata_getparam(scp, ATA_SLAVE, ATA_C_ATA_IDENTIFY))
		newdev &= ~ATA_ATA_SLAVE;
	if (newdev & ATA_ATAPI_MASTER)
	    if (ata_getparam(scp, ATA_MASTER, ATA_C_ATAPI_IDENTIFY))
		newdev &= ~ATA_ATAPI_MASTER;
	if (newdev & ATA_ATAPI_SLAVE)
	    if (ata_getparam(scp, ATA_SLAVE, ATA_C_ATAPI_IDENTIFY))
		newdev &= ~ATA_ATAPI_SLAVE;
    }
    if (!misdev && newdev)
	printf("\n");
#ifdef DEV_ATADISK
    if (newdev & ATA_ATA_MASTER && !scp->dev_softc[MASTER])
	ad_attach(scp, ATA_MASTER);
    else if (scp->devices & ATA_ATA_MASTER && scp->dev_softc[MASTER])
	ad_reinit((struct ad_softc *)scp->dev_softc[MASTER]);
    if (newdev & ATA_ATA_SLAVE && !scp->dev_softc[SLAVE])
	ad_attach(scp, ATA_SLAVE);
    else if (scp->devices & (ATA_ATA_SLAVE) && scp->dev_softc[SLAVE])
	ad_reinit((struct ad_softc *)scp->dev_softc[SLAVE]);
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    if (newdev & ATA_ATAPI_MASTER && !scp->dev_softc[MASTER])
	atapi_attach(scp, ATA_MASTER);
    else if (scp->devices & (ATA_ATAPI_MASTER) && scp->dev_softc[MASTER])
	atapi_reinit((struct atapi_softc *)scp->dev_softc[MASTER]);
    if (newdev & ATA_ATAPI_SLAVE && !scp->dev_softc[SLAVE])
	atapi_attach(scp, ATA_SLAVE);
    else if (scp->devices & (ATA_ATAPI_SLAVE) && scp->dev_softc[SLAVE])
	atapi_reinit((struct atapi_softc *)scp->dev_softc[SLAVE]);
#endif
    printf("done\n");
    scp->active = ATA_IDLE;
    ata_start(scp);
    return 0;
}

static int
ata_service(struct ata_softc *scp)
{
    /* do we have a SERVICE request from the drive ? */
    if ((scp->status & (ATA_S_SERVICE|ATA_S_ERROR|ATA_S_DRQ)) == ATA_S_SERVICE){
	ATA_OUTB(scp->r_bmio, ATA_BMSTAT_PORT,
		 ata_dmastatus(scp) | ATA_BMSTAT_INTERRUPT);
#ifdef DEV_ATADISK
	if ((ATA_INB(scp->r_io, ATA_DRIVE) & ATA_SLAVE) == ATA_MASTER) {
	    if ((scp->devices & ATA_ATA_MASTER) && scp->dev_softc[MASTER])
		return ad_service((struct ad_softc *)scp->dev_softc[MASTER], 0);
	}
	else {
	    if ((scp->devices & ATA_ATA_SLAVE) && scp->dev_softc[SLAVE])
		return ad_service((struct ad_softc *)scp->dev_softc[SLAVE], 0);
	}
#endif
    }
    return ATA_OP_FINISHED;
}

int
ata_wait(struct ata_softc *scp, int device, u_int8_t mask)
{
    int timeout = 0;
    
    DELAY(1);
    while (timeout < 5000000) {	/* timeout 5 secs */
	scp->status = ATA_INB(scp->r_io, ATA_STATUS);

	/* if drive fails status, reselect the drive just to be sure */
	if (scp->status == 0xff) {
	    ata_printf(scp, device, "no status, reselecting device\n");
	    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | device);
	    DELAY(10);
	    scp->status = ATA_INB(scp->r_io, ATA_STATUS);
	    if (scp->status == 0xff)
		return -1;
	}

	/* are we done ? */
	if (!(scp->status & ATA_S_BUSY))
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
    if (scp->status & ATA_S_ERROR)
	scp->error = ATA_INB(scp->r_io, ATA_ERROR);
    if (timeout >= 5000000)	 
	return -1;	    
    if (!mask)	   
	return (scp->status & ATA_S_ERROR);	 
    
    /* Wait 50 msec for bits wanted. */	   
    timeout = 5000;
    while (timeout--) {	  
	scp->status = ATA_INB(scp->r_io, ATA_STATUS);
	if ((scp->status & mask) == mask) {
	    if (scp->status & ATA_S_ERROR)
		scp->error = ATA_INB(scp->r_io, ATA_ERROR);
	    return (scp->status & ATA_S_ERROR);	      
	}
	DELAY (10);	   
    }	  
    return -1;	    
}   

int
ata_command(struct ata_softc *scp, int device, u_int8_t command,
	   u_int16_t cylinder, u_int8_t head, u_int8_t sector, 
	   u_int8_t count, u_int8_t feature, int flags)
{
    int error = 0;
#ifdef ATA_DEBUG
    ata_printf(scp, device, "ata_command: addr=%04x, cmd=%02x, "
	       "c=%d, h=%d, s=%d, count=%d, feature=%d, flags=%02x\n",
	       rman_get_start(scp->r_io), command, cylinder, head, sector,
	       count, feature, flags);

    /* sanity checks */
    switch(scp->active) {
    case ATA_IDLE:
	break;

    case ATA_CONTROL:
	if (flags == ATA_WAIT_INTR || flags == ATA_WAIT_READY)
	    break;
	goto out;

    case ATA_ACTIVE_ATA:
    case ATA_ACTIVE_ATAPI:
	if (flags == ATA_IMMEDIATE)
	    break;

    default:
out:
	printf("ata_command called %s flags=%s cmd=%02x\n",
	       active2str(scp->active), active2str(flags), command);
	break;
    }
#endif

    /* disable interrupt from device */
    if (scp->flags & ATA_QUEUED)
	ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_IDS | ATA_A_4BIT);

    /* select device */
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | device);

    /* ready to issue command ? */
    if (ata_wait(scp, device, 0) < 0) { 
	ata_printf(scp, device, 
		   "timeout waiting to give command=%02x s=%02x e=%02x\n",
		   command, scp->status, scp->error);
	return -1;
    }

    ATA_OUTB(scp->r_io, ATA_FEATURE, feature);
    ATA_OUTB(scp->r_io, ATA_COUNT, count);
    ATA_OUTB(scp->r_io, ATA_SECTOR, sector);
    ATA_OUTB(scp->r_io, ATA_CYL_MSB, cylinder >> 8);
    ATA_OUTB(scp->r_io, ATA_CYL_LSB, cylinder);
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | device | head);

    switch (flags) {
    case ATA_WAIT_INTR:
	scp->active |= ATA_WAIT_INTR;
	asleep((caddr_t)scp, PRIBIO, "atacmd", 10 * hz);
	ATA_OUTB(scp->r_io, ATA_CMD, command);

	/* enable interrupt */
	if (scp->flags & ATA_QUEUED)
	    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_4BIT);

	if (await(PRIBIO, 10 * hz)) {
	    ata_printf(scp, device, "ata_command: timeout waiting for intr\n");
	    scp->active &= ~ATA_WAIT_INTR;
	    error = -1;
	}
	break;
    
    case ATA_WAIT_READY:
	scp->active |= ATA_WAIT_READY;
	ATA_OUTB(scp->r_io, ATA_CMD, command);
	if (ata_wait(scp, device, ATA_S_READY) < 0) { 
	    ata_printf(scp, device, 
		       "timeout waiting for command=%02x s=%02x e=%02x\n",
		       command, scp->status, scp->error);
	    error = -1;
	}
	scp->active &= ~ATA_WAIT_READY;
	break;

    case ATA_IMMEDIATE:
	ATA_OUTB(scp->r_io, ATA_CMD, command);
	break;

    default:
	ata_printf(scp, device, "DANGER: illegal interrupt flag=%s\n",
		   active2str(flags));
    }
    /* enable interrupt */
    if (scp->flags & ATA_QUEUED)
	ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_4BIT);
    return error;
}

void
ata_set_name(struct ata_softc *scp, int device, char *name)
{
    scp->dev_name[ATA_DEV(device)] = malloc(strlen(name) + 1, M_ATA, M_NOWAIT);
    if (scp->dev_name[ATA_DEV(device)])
	strcpy(scp->dev_name[ATA_DEV(device)], name);
}

void
ata_free_name(struct ata_softc *scp, int device)
{
    if (scp->dev_name[ATA_DEV(device)])
	free(scp->dev_name[ATA_DEV(device)], M_ATA);
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
 
int
ata_printf(struct ata_softc *scp, int device, const char * fmt, ...)
{
    va_list ap;
    int ret;

    if (device == -1)
	ret = printf("ata%d: ", device_get_unit(scp->dev));
    else {
	if (scp->dev_name[ATA_DEV(device)])
	    ret = printf("%s: ", scp->dev_name[ATA_DEV(device)]);
	else
	    ret = printf("ata%d-%s: ", device_get_unit(scp->dev),
			 (device == ATA_MASTER) ? "master" : "slave");
    }
    va_start(ap, fmt);
    ret += vprintf(fmt, ap);
    va_end(ap);
    return ret;
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
    case ATA_WDMA2: return "WDMA2";
    case ATA_UDMA2: return "UDMA33";
    case ATA_UDMA4: return "UDMA66";
    case ATA_UDMA5: return "UDMA100";
    case ATA_DMA: return "BIOSDMA";
    default: return "???";
    }
}

int
ata_pio2mode(int pio)
{
    switch (pio) {
    default:
    case 0: return ATA_PIO0;
    case 1: return ATA_PIO1;
    case 2: return ATA_PIO2;
    case 3: return ATA_PIO3;
    case 4: return ATA_PIO4;
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
    if (ap->opiomode == 2)
	return 2;
    if (ap->opiomode == 1)
	return 1;
    if (ap->opiomode == 0)
	return 0;
    return -1; 
} 

int
ata_wmode(struct ata_params *ap)
{
    if (ap->wdmamodes & 4)
	return 2;
    if (ap->wdmamodes & 2)
	return 1;
    if (ap->wdmamodes & 1)
	return 0;
    return -1;
}

int
ata_umode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_88) {
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

static char *
active2str(int active)
{
    static char buf[64];

    bzero(buf, sizeof(buf));
    if (active & ATA_IDLE)
	strcat(buf, "ATA_IDLE ");
    if (active & ATA_IMMEDIATE)
	strcat(buf, "ATA_IMMEDIATE ");
    if (active & ATA_WAIT_INTR)
	strcat(buf, "ATA_WAIT_INTR ");
    if (active & ATA_WAIT_READY)
	strcat(buf, "ATA_WAIT_READY ");
    if (active & ATA_ACTIVE)
	strcat(buf, "ATA_ACTIVE ");
    if (active & ATA_ACTIVE_ATA)
	strcat(buf, "ATA_ACTIVE_ATA ");
    if (active & ATA_ACTIVE_ATAPI)
	strcat(buf, "ATA_ACTIVE_ATAPI ");
    if (active & ATA_CONTROL)
	strcat(buf, "ATA_CONTROL ");
    return buf;
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
ata_change_mode(struct ata_softc *scp, int device, int mode)
{
    int umode, wmode, pmode;
    int s = splbio();

    while (!atomic_cmpset_int(&scp->active, ATA_IDLE, ATA_ACTIVE))
	tsleep((caddr_t)&s, PRIBIO, "atachm", hz/4);

    umode = ata_umode(ATA_PARAM(scp, device));
    wmode = ata_wmode(ATA_PARAM(scp, device));
    pmode = ata_pmode(ATA_PARAM(scp, device));
    
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
    ata_dmainit(scp, device, pmode, wmode, umode);

    scp->active = ATA_IDLE;
    ata_start(scp);
    splx(s);
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
