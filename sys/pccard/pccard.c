/*
 *	pccard.c - Interface code for PC-CARD controllers.
 *
 *	June 1995, Andrew McRae (andrew@mega.com.au)
 *-------------------------------------------------------------------------
 *
 * Copyright (c) 2001 M. Warner Losh.  All rights reserved.
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#define OBSOLETE_IN_6

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <machine/bus.h>

#include <pccard/cardinfo.h>
#include <pccard/driver.h>
#include <pccard/slot.h>
#include <pccard/pccard_nbk.h>

#include <machine/md_var.h>

static int		allocate_driver(struct slot *, struct dev_desc *);
static void		inserted(void *);
static void		disable_slot(struct slot *);
static void		disable_slot_to(struct slot *);
static void		power_off_slot(void *);

/*
 *	The driver interface for read/write uses a block
 *	of memory in the ISA I/O memory space allocated via
 *	an ioctl setting.
 *
 *	Now that we have different bus attachments, we should really
 *	use a better algorythm to allocate memory.
 */
static unsigned long pccard_mem;	/* Physical memory */
static unsigned char *pccard_kmem;	/* Kernel virtual address */
static struct resource *pccard_mem_res;
static int pccard_mem_rid;

static	d_open_t	crdopen;
static	d_close_t	crdclose;
static	d_read_t	crdread;
static	d_write_t	crdwrite;
static	d_ioctl_t	crdioctl;
static	d_poll_t	crdpoll;

#if __FreeBSD_version < 500000
#define CDEV_MAJOR 50
#else
#define CDEV_MAJOR MAJOR_AUTO
#endif
static struct cdevsw crd_cdevsw = {
	.d_open =	crdopen,
	.d_close =	crdclose,
	.d_read =	crdread,
	.d_write =	crdwrite,
	.d_ioctl =	crdioctl,
	.d_poll =	crdpoll,
	.d_name =	"crd",
	.d_maj =	CDEV_MAJOR,
};

/*
 *	Power off the slot.
 *	(doing it immediately makes the removal of some cards unstable)
 */
static void
power_off_slot(void *arg)
{
	struct slot *slt = (struct slot *)arg;
	int s;

	/*
	 * The following will generate an interrupt.  So, to hold off
	 * the interrupt unitl after disable runs so that we can get rid
	 * rid of the interrupt before it becomes unsafe to touch the
	 * device.
	 *
	 * XXX In current, the spl stuff is a nop.
	 */
	s = splhigh();
	/* Power off the slot. */
	slt->pwr_off_pending = 0;
	slt->ctrl->disable(slt);
	splx(s);
}

/*
 *	disable_slot - Disables the slot by removing
 *	the power and unmapping the I/O
 */
static void
disable_slot(struct slot *slt)
{
	device_t pccarddev;
	device_t *kids;
	int nkids;
	int i;
	int ret;

	/*
	 * Note that a race condition is possible here; if a
	 * driver is accessing the device and it is removed, then
	 * all bets are off...
	 */
	pccarddev = slt->dev;
	device_get_children(pccarddev, &kids, &nkids);
	for (i = 0; i < nkids; i++) {
		if ((ret = device_delete_child(pccarddev, kids[i])) != 0)
			printf("pccard: delete of %s failed: %d\n",
				device_get_nameunit(kids[i]), ret);
 	}
	free(kids, M_TEMP);

	/* Power off the slot 1/2 second after removal of the card */
	slt->poff_ch = timeout(power_off_slot, (caddr_t)slt, hz / 2);
	slt->pwr_off_pending = 1;
}

static void
disable_slot_to(struct slot *slt)
{
	disable_slot(slt);
	if (slt->state == empty)
		printf("pccard: card removed, slot %d\n", slt->slotnum);
	else
		printf("pccard: card deactivated, slot %d\n", slt->slotnum);
	pccard_remove_beep();
	selwakeuppri(&slt->selp, PZERO);
}

/*
 *	pccard_init_slot - Initialize the slot controller and attach various
 * things to it.  We also make the device for it.  We create the device that
 * will be exported to devfs.
 */
struct slot *
pccard_init_slot(device_t dev, struct slot_ctrl *ctrl)
{
	int		slotno;
	struct slot	*slt;

	slt = PCCARD_DEVICE2SOFTC(dev);
	slotno = device_get_unit(dev);
	slt->dev = dev;
	slt->d = make_dev(&crd_cdevsw, slotno, 0, 0, 0600, "card%d", slotno);
	slt->d->si_drv1 = slt;
	slt->ctrl = ctrl;
	slt->slotnum = slotno;
	callout_handle_init(&slt->insert_ch);
	callout_handle_init(&slt->poff_ch);

	return (slt);
}

/*
 *	allocate_driver - Create a new device entry for this
 *	slot, and attach a driver to it.
 */
static int
allocate_driver(struct slot *slt, struct dev_desc *desc)
{
	struct pccard_devinfo *devi;
	device_t pccarddev;
	int err, irq = 0;
	device_t child;
	device_t *devs;
	int count;

	pccarddev = slt->dev;
	err = device_get_children(pccarddev, &devs, &count);
	if (err != 0)
		return (err);
	free(devs, M_TEMP);
	if (count) {
		device_printf(pccarddev,
		    "Can not attach more than one child.\n");
		return (EIO);
	}
	irq = ffs(desc->irqmask) - 1;
	MALLOC(devi, struct pccard_devinfo *, sizeof(*devi), M_DEVBUF,
	    M_WAITOK | M_ZERO);
	strcpy(devi->name, desc->name);
	/*
	 *	Create an entry for the device under this slot.
	 */
	devi->running = 1;
	devi->slt = slt;
	bcopy(desc->misc, devi->misc, sizeof(desc->misc));
	strcpy(devi->manufstr, desc->manufstr);
	strcpy(devi->versstr, desc->versstr);
	strcpy(devi->cis3str, desc->cis3str);
	strcpy(devi->cis4str, desc->cis4str);
	devi->manufacturer = desc->manufacturer;
	devi->product = desc->product;
	devi->prodext = desc->prodext;
	resource_list_init(&devi->resources);
	child = device_add_child(pccarddev, devi->name, desc->unit);
	if (child == NULL) {
		if (desc->unit != -1)
			device_printf(pccarddev,
			    "Unit %d failed for %s, try a different unit\n",
			    desc->unit, devi->name);
		else
			device_printf(pccarddev,
			    "No units available for %s.  Impossible?\n",
			    devi->name);
		return (EIO);
	}
	device_set_flags(child, desc->flags);
	device_set_ivars(child, devi);
	if (bootverbose) {
		device_printf(pccarddev, "Assigning %s:",
		    device_get_nameunit(child));
		if (desc->iobase)
			printf(" io 0x%x-0x%x",
			    desc->iobase, desc->iobase + desc->iosize - 1);
		if (irq)
			printf(" irq %d", irq);
		if (desc->mem)
			printf(" mem 0x%lx-0x%lx", desc->mem,
			    desc->mem + desc->memsize - 1);
		printf(" flags 0x%x\n", desc->flags);
	}
	err = bus_set_resource(child, SYS_RES_IOPORT, 0, desc->iobase,
	    desc->iosize);
	if (err)
		goto err;
	if (irq)
		err = bus_set_resource(child, SYS_RES_IRQ, 0, irq, 1);
	if (err)
		goto err;
	if (desc->memsize) {
		err = bus_set_resource(child, SYS_RES_MEMORY, 0, desc->mem,
		    desc->memsize);
		if (err)
			goto err;
	}
	err = device_probe_and_attach(child);
	/*
	 * XXX We unwisely assume that the detach code won't run while the
	 * XXX the attach code is attaching.  Someone should put some
	 * XXX interlock code.  This can happen if probe/attach takes a while
	 * XXX and the user ejects the card, which causes the detach
	 * XXX function to be called.
	 */
	strncpy(desc->name, device_get_nameunit(child), sizeof(desc->name));
	desc->name[sizeof(desc->name) - 1] = '\0';
err:
	if (err)
		device_delete_child(pccarddev, child);
	return (err);
}

/*
 *	card insert routine - Called from a timeout to debounce
 *	insertion events.
 */
static void
inserted(void *arg)
{
	struct slot *slt = arg;

	slt->state = filled;
	/*
	 * Disable any pending timeouts for this slot, and explicitly
	 * power it off right now.  Then, re-enable the power using
	 * the (possibly new) power settings.
	 */
	untimeout(power_off_slot, (caddr_t)slt, slt->poff_ch);
	power_off_slot(slt);

	/*
	 *	Enable 5V to the card so that the CIS can be read.  Well,
	 * enable the most natural voltage so that the CIS can be read.
	 */
	slt->pwr.vcc = -1;
	slt->pwr.vpp = -1;
	slt->ctrl->power(slt);

	printf("pccard: card inserted, slot %d\n", slt->slotnum);
	pccard_insert_beep();
	slt->ctrl->reset(slt);
}

/*
 *	Card event callback. Called at splhigh to prevent
 *	device interrupts from interceding.
 */
void
pccard_event(struct slot *slt, enum card_event event)
{
	if (slt->insert_seq) {
		slt->insert_seq = 0;
		untimeout(inserted, (void *)slt, slt->insert_ch);
	}

	switch(event) {
	case card_removed:
	case card_deactivated:
		if (slt->state == filled || slt->state == inactive) {
			if (event == card_removed)
				slt->state = empty;
			else
				slt->state = inactive;
			disable_slot_to(slt);
		}
		break;
	case card_inserted:
		slt->insert_seq = 1;
		slt->insert_ch = timeout(inserted, (void *)slt, hz/4);
		break;
	}
}

/*
 *	Device driver interface.
 */
static	int
crdopen(dev_t dev, int oflags, int devtype, d_thread_t *td)
{
	struct slot *slt = PCCARD_DEV2SOFTC(dev);

	if (slt == NULL)
		return (ENXIO);
	if (slt->rwmem == 0)
		slt->rwmem = MDF_ATTR;
	return (0);
}

/*
 *	Close doesn't de-allocate any resources, since
 *	slots may be assigned to drivers already.
 */
static	int
crdclose(dev_t dev, int fflag, int devtype, d_thread_t *td)
{
	return (0);
}

/*
 *	read interface. Map memory at lseek offset,
 *	then transfer to user space.
 */
static	int
crdread(dev_t dev, struct uio *uio, int ioflag)
{
	struct slot *slt = PCCARD_DEV2SOFTC(dev);
	struct mem_desc *mp, oldmap;
	unsigned char *p;
	unsigned int offs;
	int error = 0, win, count;

	if (slt == 0 || slt->state != filled)
		return (ENXIO);
	if (pccard_mem == 0)
		return (ENOMEM);
	for (win = slt->ctrl->maxmem - 1; win >= 0; win--)
		if ((slt->mem[win].flags & MDF_ACTIVE) == 0)
			break;
	if (win < 0)
		return (EBUSY);
	mp = &slt->mem[win];
	oldmap = *mp;
	mp->flags = slt->rwmem | MDF_ACTIVE;
	while (uio->uio_resid && error == 0) {
		mp->card = uio->uio_offset;
		mp->size = PCCARD_MEMSIZE;
		mp->start = (caddr_t)(void *)(uintptr_t)pccard_mem;
		if ((error = slt->ctrl->mapmem(slt, win)) != 0)
			break;
		offs = (unsigned int)uio->uio_offset & (PCCARD_MEMSIZE - 1);
		p = pccard_kmem + offs;
		count = MIN(PCCARD_MEMSIZE - offs, uio->uio_resid);
		error = uiomove(p, count, uio);
	}
	/*
	 *	Restore original map.
	 */
	*mp = oldmap;
	slt->ctrl->mapmem(slt, win);

	return (error);
}

/*
 *	crdwrite - Write data to card memory.
 *	Handles wrap around so that only one memory
 *	window is used.
 */
static	int
crdwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct slot *slt = PCCARD_DEV2SOFTC(dev);
	struct mem_desc *mp, oldmap;
	unsigned char *p;
	unsigned int offs;
	int error = 0, win, count;

	if (slt == 0 || slt->state != filled)
		return (ENXIO);
	if (pccard_mem == 0)
		return (ENOMEM);
	for (win = slt->ctrl->maxmem - 1; win >= 0; win--)
		if ((slt->mem[win].flags & MDF_ACTIVE) == 0)
			break;
	if (win < 0)
		return (EBUSY);
	mp = &slt->mem[win];
	oldmap = *mp;
	mp->flags = slt->rwmem | MDF_ACTIVE;
	while (uio->uio_resid && error == 0) {
		mp->card = uio->uio_offset;
		mp->size = PCCARD_MEMSIZE;
		mp->start = (caddr_t)(void *)(uintptr_t)pccard_mem;
		if ((error = slt->ctrl->mapmem(slt, win)) != 0)
			break;
		offs = (unsigned int)uio->uio_offset & (PCCARD_MEMSIZE - 1);
		p = pccard_kmem + offs;
		count = MIN(PCCARD_MEMSIZE - offs, uio->uio_resid);
		error = uiomove(p, count, uio);
	}
	/*
	 *	Restore original map.
	 */
	*mp = oldmap;
	slt->ctrl->mapmem(slt, win);

	return (error);
}

/*
 *	ioctl calls - allows setting/getting of memory and I/O
 *	descriptors, and assignment of drivers.
 */
static	int
crdioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, d_thread_t *td)
{
	u_int32_t	addr;
	int		err;
	struct io_desc	*ip;
	struct mem_desc *mp;
	device_t	pccarddev;
	int		pwval;
	int		s;
	struct slot	*slt = PCCARD_DEV2SOFTC(dev);
/*XXX*/#if __FreeBSD_version < 5000000		/* 4.x compatibility only. */
	struct dev_desc d;
	struct dev_desc_old *odp;
/*XXX*/#endif

	if (slt == 0 && cmd != PIOCRWMEM)
		return (ENXIO);
	switch(cmd) {
	default:
		if (slt->ctrl->ioctl)
			return (slt->ctrl->ioctl(slt, cmd, data));
		return (ENOTTY);
	/*
	 * Get slot state.
	 */
	case PIOCGSTATE:
		s = splhigh();
		((struct slotstate *)data)->state = slt->state;
		((struct slotstate *)data)->laststate = slt->laststate;
		slt->laststate = slt->state;
		splx(s);
		((struct slotstate *)data)->maxmem = slt->ctrl->maxmem;
		((struct slotstate *)data)->maxio = slt->ctrl->maxio;
		((struct slotstate *)data)->irqs = 0;
		break;
	/*
	 * Get memory context.
	 */
	case PIOCGMEM:
		s = ((struct mem_desc *)data)->window;
		if (s < 0 || s >= slt->ctrl->maxmem)
			return (EINVAL);
		mp = &slt->mem[s];
		((struct mem_desc *)data)->flags = mp->flags;
		((struct mem_desc *)data)->start = mp->start;
		((struct mem_desc *)data)->size = mp->size;
		((struct mem_desc *)data)->card = mp->card;
		break;
	/*
	 * Set memory context. If context already active, then unmap it.
	 * It is hard to see how the parameters can be checked.
	 * At the very least, we only allow root to set the context.
	 */
	case PIOCSMEM:
		if (suser(td))
			return (EPERM);
		if (slt->state != filled)
			return (ENXIO);
		s = ((struct mem_desc *)data)->window;
		if (s < 0 || s >= slt->ctrl->maxmem)
			return (EINVAL);
		slt->mem[s] = *((struct mem_desc *)data);
		return (slt->ctrl->mapmem(slt, s));
	/*
	 * Get I/O port context.
	 */
	case PIOCGIO:
		s = ((struct io_desc *)data)->window;
		if (s < 0 || s >= slt->ctrl->maxio)
			return (EINVAL);
		ip = &slt->io[s];
		((struct io_desc *)data)->flags = ip->flags;
		((struct io_desc *)data)->start = ip->start;
		((struct io_desc *)data)->size = ip->size;
		break;
	/*
	 * Set I/O port context.
	 */
	case PIOCSIO:
		if (suser(td))
			return (EPERM);
		if (slt->state != filled)
			return (ENXIO);
		s = ((struct io_desc *)data)->window;
		if (s < 0 || s >= slt->ctrl->maxio)
			return (EINVAL);
		slt->io[s] = *((struct io_desc *)data);
		/* XXX Don't actually map */
		return (0);
		break;
	/*
	 * Set memory window flags for read/write interface.
	 */
	case PIOCRWFLAG:
		slt->rwmem = *(int *)data;
		break;
	/*
	 * Set the memory window to be used for the read/write interface.
	 */
	case PIOCRWMEM:
		if (*(unsigned long *)data == 0) {
			*(unsigned long *)data = pccard_mem;
			break;
		}
		if (suser(td))
			return (EPERM);
		/*
		 * Validate the memory by checking it against the I/O
		 * memory range. It must also start on an aligned block size.
		 */
		if (*(unsigned long *)data & (PCCARD_MEMSIZE-1))
			return (EINVAL);
		pccarddev = PCCARD_DEV2SOFTC(dev)->dev;
		pccard_mem_rid = 0;
		addr = *(unsigned long *)data;
		if (pccard_mem_res)
			bus_release_resource(pccarddev, SYS_RES_MEMORY,
			    pccard_mem_rid, pccard_mem_res);
		pccard_mem_res = bus_alloc_resource(pccarddev, SYS_RES_MEMORY,
		    &pccard_mem_rid, addr, addr, PCCARD_MEMSIZE,
		    RF_ACTIVE | rman_make_alignment_flags(PCCARD_MEMSIZE));
		if (pccard_mem_res == NULL)
			return (EINVAL);
		pccard_mem = rman_get_start(pccard_mem_res);
		pccard_kmem = rman_get_virtual(pccard_mem_res);
		break;
	/*
	 * Set power values.
	 */
	case PIOCSPOW:
		slt->pwr = *(struct power *)data;
		return (slt->ctrl->power(slt));
	/*
	 * Allocate a driver to this slot.
	 */
	case PIOCSDRV:
		if (suser(td))
			return (EPERM);
		err = allocate_driver(slt, (struct dev_desc *)data);
		if (!err)
			pccard_success_beep();
		else
			pccard_failure_beep();
		return (err);
/***/#if __FreeBSD_version < 5000000		/* 4.x compatibility only. */
	case PIOCSDRVOLD:
		if (suser(td))
			return (EPERM);
		odp = (struct dev_desc_old *) data;
		strlcpy(d.name, odp->name, sizeof(d.name));
		d.unit = odp->unit;
		d.mem = odp->mem;
		d.memsize = odp->memsize;
		d.iobase = odp->iobase;
		d.iosize = odp->iosize;
		d.irqmask = odp->irqmask;
		d.flags = odp->flags;
		memcpy(d.misc, odp->misc, sizeof(odp->misc));
		strlcpy(d.manufstr, odp->manufstr, sizeof(d.manufstr));
		strlcpy(d.versstr, odp->versstr, sizeof(d.versstr));
		*d.cis3str = '\0';
		*d.cis4str = '\0';
		d.manufacturer = odp->manufacturer;
		d.product = odp->product;
		d.prodext = odp->prodext;
		err = allocate_driver(slt, &d);
		if (!err)
			pccard_success_beep();
		else
			pccard_failure_beep();
		return (err);
/***/#endif
	/*
	 * Virtual removal/insertion
	 */
	case PIOCSVIR:
		pwval = *(int *)data;
		if (!pwval) {
			if (slt->state != filled)
				return (EINVAL);
			pccard_event(slt, card_deactivated);
		} else {
			if (slt->state != empty && slt->state != inactive)
				return (EINVAL);
			pccard_event(slt, card_inserted);
		}
		break;
	case PIOCSBEEP:
		if (pccard_beep_select(*(int *)data)) {
			return (EINVAL);
		}
		break;
	}
	return (0);
}

/*
 *	poll - Poll on exceptions will return true
 *	when a change in card status occurs.
 */
static	int
crdpoll(dev_t dev, int events, d_thread_t *td)
{
	int	revents = 0;
	int	s;
	struct slot *slt = PCCARD_DEV2SOFTC(dev);

	if (events & (POLLIN | POLLRDNORM))
		revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLIN | POLLRDNORM);

	s = splhigh();
	/*
	 *	select for exception - card event.
	 */
	if (events & POLLRDBAND)
		if (slt == 0 || slt->laststate != slt->state)
			revents |= POLLRDBAND;

	if (revents == 0)
		selrecord(td, &slt->selp);

	splx(s);
	return (revents);
}

/*
 *	APM hooks for suspending and resuming.
 */
int
pccard_suspend(device_t dev)
{
	struct slot *slt = PCCARD_DEVICE2SOFTC(dev);

	/* This code stolen from pccard_event:card_removed */
	if (slt->state == filled) {
		int s = splhigh();		/* nop on current */
		disable_slot(slt);
		slt->laststate = suspend;	/* for pccardd */
		slt->state = empty;
		splx(s);
		printf("pccard: card disabled, slot %d\n", slt->slotnum);
	}
	/*
	 * Disable any pending timeouts for this slot since we're
	 * powering it down/disabling now.
	 */
	untimeout(power_off_slot, (caddr_t)slt, slt->poff_ch);
	slt->ctrl->disable(slt);
	return (0);
}

int
pccard_resume(device_t dev)
{
	struct slot *slt = PCCARD_DEVICE2SOFTC(dev);

	slt->ctrl->resume(slt);
	return (0);
}
