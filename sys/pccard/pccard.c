/*
 *	pccard.c - Interface code for PC-CARD controllers.
 *
 *	June 1995, Andrew McRae (andrew@mega.com.au)
 *-------------------------------------------------------------------------
 *
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
 *	$Id: pccard.c,v 1.58 1998/04/09 14:01:13 nate Exp $
 */

#include "opt_devfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/interrupt.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>

#include "apm.h"
#if	NAPM > 0
#include <machine/apm_bios.h>
#endif	/* NAPM > 0 */

#include <pccard/cardinfo.h>
#include <pccard/driver.h>
#include <pccard/slot.h>

#include <machine/md_var.h>

/*
 * XXX We shouldn't be using processor-specific/bus-specific code in
 * here, but we need the start of the ISA hole (IOM_BEGIN).
 */
#include <i386/isa/isa.h>

SYSCTL_NODE(_machdep, OID_AUTO, pccard, CTLFLAG_RW, 0, "pccard");

static int pcic_resume_reset =
#ifdef PCIC_RESUME_RESET
	1;
#else
	0;
#endif

SYSCTL_INT(_machdep_pccard, OID_AUTO, pcic_resume_reset, CTLFLAG_RW, 
	&pcic_resume_reset, 0, "");

#define	PCCARD_MEMSIZE	(4*1024)

#define MIN(a,b)	((a)<(b)?(a):(b))

static int		allocate_driver(struct slot *, struct dev_desc *);
static void		inserted(void *);
static void		unregister_device_interrupt(struct pccard_devinfo *);
static void		disable_slot(struct slot *);
static int		invalid_io_memory(unsigned long, int);
static struct pccard_device *find_driver(char *);
static void		remove_device(struct pccard_devinfo *);
static void		slot_irq_handler(int);
static void		power_off_slot(void *);

#if	NAPM > 0
/*
 *    For the APM stuff, the apmhook structure is kept
 *    separate from the slot structure so that the slot
 *    drivers do not need to know about the hooks (or the
 *    data structures).
 */
static int	slot_suspend(void *arg);
static int	slot_resume(void *arg);
static struct	apmhook s_hook[MAXSLOT];	/* APM suspend */
static struct	apmhook r_hook[MAXSLOT];	/* APM resume */
#endif	/* NAPM > 0 */

static struct slot	*pccard_slots[MAXSLOT];	/* slot entries */
static struct slot	*slot_list;
static struct slot_ctrl *cont_list;
static struct pccard_device *drivers;		/* Card drivers */

/*
 *	The driver interface for read/write uses a block
 *	of memory in the ISA I/O memory space allocated via
 *	an ioctl setting.
 */
static unsigned long pccard_mem;	/* Physical memory */
static unsigned char *pccard_kmem;	/* Kernel virtual address */

static	d_open_t	crdopen;
static	d_close_t	crdclose;
static	d_read_t	crdread;
static	d_write_t	crdwrite;
static	d_ioctl_t	crdioctl;
static	d_poll_t	crdpoll;

#define CDEV_MAJOR 50
static struct cdevsw crd_cdevsw = 
	{ crdopen,	crdclose,	crdread,	crdwrite,	/*50*/
	  crdioctl,	nostop,		nullreset,	nodevtotty,/* pcmcia */
	  crdpoll,	nommap,		NULL,	"crd",	NULL,	-1 };


/*
 *	pccard_configure - called by autoconf code.
 *	Probes for various PC-CARD controllers, and
 *	initialises data structures to point to the
 *	various slots.
 *
 *	Each controller indicates the number of slots
 *	that it sees, and these are mapped to a master
 *	slot number accessed via the character device entries.
 */
void
pccard_configure(void)
{
	struct pccard_device **drivers, *drv;

#include "pcic.h"
#if NPCIC > 0
	pcic_probe();
#endif

	drivers = (struct pccard_device **)pccarddrv_set.ls_items;
	printf("Initializing PC-card drivers:");
	while ((drv = *drivers++)) {
		printf(" %s", drv->name);
		pccard_add_driver(drv);
	}
	printf("\n");
}

/*
 *	pccard_add_driver - Add a new driver to the list of
 *	drivers available for allocation.
 */
void
pccard_add_driver(struct pccard_device *drv)
{
	/*
	 *	If already loaded, then reject the driver.
	 */
	if (find_driver(drv->name)) {
		printf("Driver %s already loaded\n", drv->name);
		return;
	}
	drv->next = drivers;
	drivers = drv;
}

#ifdef unused
/*
 *	pccard_remove_driver - called to unlink driver
 *	from devices. Usually called when drivers are
 *	are unloaded from kernel.
 */
void
pccard_remove_driver(struct pccard_device *drv)
{
	struct slot *slt;
	struct pccard_devinfo *devi, *next;
	struct pccard_device *drvlist;

	for (slt = slot_list; slt; slt = slt->next)
		for (devi = slt->devices; devi; devi = next) {
			next = devi->next;
			if (devi->drv == drv)
				remove_device(devi);
		}
	/*
	 *	Once all the devices belonging to this driver have been
	 *	freed, then remove the driver from the list
	 *	of registered drivers.
	 */
	if (drivers == drv)
		drivers = drv->next;
	else
		for (drvlist = drivers; drvlist->next; drvlist = drvlist->next)
			if (drvlist->next == drv) {
				drvlist->next = drv->next;
				break;
			}
}
#endif

/*
 *	pccard_remove_controller - Called when the slot
 *	driver is unloaded. The plan is to unload
 *	drivers from the slots, and then remove the
 *	slots from the slot list, and then finally
 *	remove the controller structure. Messy...
 */
void
pccard_remove_controller(struct slot_ctrl *ctrl)
{
	struct slot *slt, *next, *last = 0;
	struct slot_ctrl *cl;
	struct pccard_devinfo *devi;

	for (slt = slot_list; slt; slt = next) {
		next = slt->next;
		/*
		 *	If this slot belongs to this controller,
		 *	remove this slot.
		 */
		if (slt->ctrl == ctrl) {
			pccard_slots[slt->slotnum] = 0;
			if (slt->insert_seq)
				untimeout(inserted, (void *)slt, slt->insert_ch);
			/*
			 * Unload the drivers attached to this slot.
			 */
			while (devi = slt->devices)
				remove_device(devi);
			/*
			 * Disable the slot and unlink the slot from the 
			 * slot list.
			 */
			disable_slot(slt);
			if (last)
				last->next = next;
			else
				slot_list = next;
#if NAPM > 0
			apm_hook_disestablish(APM_HOOK_SUSPEND,
				&s_hook[slt->slotnum]);
			apm_hook_disestablish(APM_HOOK_RESUME,
				&r_hook[slt->slotnum]);
#endif
			if (ctrl->extra && slt->cdata)
				FREE(slt->cdata, M_DEVBUF);
			FREE(slt, M_DEVBUF);
			/*
			 * Can't use slot after we have freed it.
			 */
		} else {
			last = slt;
		}
	}
	/*
	 *	Unlink controller structure from controller list.
	 */
	if (cont_list == ctrl)
		cont_list = ctrl->next;
	else
		for (cl = cont_list; cl->next; cl = cl->next)
			if (cl->next == ctrl) {
				cl->next = ctrl->next;
				break;
			}
}

/*
 *	Power off the slot.
 *	(doing it immediately makes the removal of some cards unstable)
 */
static void
power_off_slot(void *arg)
{
	struct slot *slt = (struct slot *)arg;

	/* Power off the slot. */
	slt->pwr_off_pending = 0;
	slt->ctrl->disable(slt);
}

/*
 *	unregister_device_interrupt - Disable the interrupt generation to
 *	the device driver which is handling it, so we can remove it.
 */
static void
unregister_device_interrupt(struct pccard_devinfo *devi)
{
	struct slot *slt = devi->slt;
	int s;

	if (devi->running) {
		s = splhigh();
		devi->drv->disable(devi);
		devi->running = 0;
		if (devi->isahd.id_irq && --slt->irqref <= 0) {
			printf("Return IRQ=%d\n",slt->irq);
			slt->ctrl->mapirq(slt, 0);
			INTRDIS(1<<slt->irq);
			unregister_intr(slt->irq, slot_irq_handler);
			if (devi->drv->imask)
				INTRUNMASK(*devi->drv->imask,(1<<slt->irq));
			/* Remove from the PCIC controller imask */
			if (slt->ctrl->imask)
				INTRUNMASK(*(slt->ctrl->imask), (1<<slt->irq));
			slt->irq = 0;
		}
		splx(s);
	}
}

/*
 *	disable_slot - Disables the slot by removing
 *	the power and unmapping the I/O
 */
static void
disable_slot(struct slot *slt)
{
	struct pccard_devinfo *devi;
	int i;
	/*
	 * Unload all the drivers on this slot. Note we can't
	 * remove the device structures themselves, because this
	 * may be called from the event routine, which is called
	 * from the slot controller's ISR, and removing the structures
	 * shouldn't happen during the middle of some driver activity.
	 *
	 * Note that a race condition is possible here; if a
	 * driver is accessing the device and it is removed, then
	 * all bets are off...
	 */
	for (devi = slt->devices; devi; devi = devi->next)
		unregister_device_interrupt(devi);

	/* Power off the slot 1/2 second after removal of the card */
	slt->poff_ch = timeout(power_off_slot, (caddr_t)slt, hz / 2);
	slt->pwr_off_pending = 1;

	/* De-activate all contexts. */
	for (i = 0; i < slt->ctrl->maxmem; i++)
		if (slt->mem[i].flags & MDF_ACTIVE) {
			slt->mem[i].flags = 0;
			(void)slt->ctrl->mapmem(slt, i);
		}
	for (i = 0; i < slt->ctrl->maxio; i++)
		if (slt->io[i].flags & IODF_ACTIVE) {
			slt->io[i].flags = 0;
			(void)slt->ctrl->mapio(slt, i);
		}
}

/*
 *	APM hooks for suspending and resuming.
 */
#if   NAPM > 0
static int
slot_suspend(void *arg)
{
	struct slot *slt = arg;

	/* This code stolen from pccard_event:card_removed */
	if (slt->state == filled) {
		int s = splhigh();
		disable_slot(slt);
		slt->laststate = filled;
		slt->state = suspend;
		splx(s);
		printf("Card disabled, slot %d\n", slt->slotnum);
	}
	slt->ctrl->disable(slt);
	return (0);
}

static int
slot_resume(void *arg)
{
	struct slot *slt = arg;

	if (pcic_resume_reset)
		slt->ctrl->resume(slt);
	/* This code stolen from pccard_event:card_inserted */
	if (slt->state == suspend) {
		slt->laststate = suspend;
		slt->state = empty;
		slt->insert_seq = 1;
		slt->insert_ch = timeout(inserted, (void *)slt, hz/4);
		selwakeup(&slt->selp);
	}
	return (0);
}
#endif	/* NAPM > 0 */

/*
 *	pccard_alloc_slot - Called from controller probe
 *	routine, this function allocates a new PC-CARD slot
 *	and initialises the data structures using the data provided.
 *	It returns the allocated structure to the probe routine
 *	to allow the controller specific data to be initialised.
 */
struct slot *
pccard_alloc_slot(struct slot_ctrl *ctrl)
{
	struct slot *slt;
	int slotno;

	for (slotno = 0; slotno < MAXSLOT; slotno++)
		if (pccard_slots[slotno] == 0)
			break;
	if (slotno == MAXSLOT)
		return(0);

	MALLOC(slt, struct slot *, sizeof(*slt), M_DEVBUF, M_WAITOK);
	bzero(slt, sizeof(*slt));
#ifdef DEVFS
	slt->devfs_token = devfs_add_devswf(&crd_cdevsw, 
		slotno, DV_CHR, 0, 0, 0600, "card%d", slotno);
#endif
	if (ctrl->extra) {
		MALLOC(slt->cdata, void *, ctrl->extra, M_DEVBUF, M_WAITOK);
		bzero(slt->cdata, ctrl->extra);
	}
	slt->ctrl = ctrl;
	slt->slotnum = slotno;
	pccard_slots[slotno] = slt;
	slt->next = slot_list;
	slot_list = slt;
	/*
	 *	If this controller hasn't been seen before, then
	 *	link it into the list of controllers.
	 */
	if (ctrl->slots++ == 0) {
		ctrl->next = cont_list;
		cont_list = ctrl;
		if (ctrl->maxmem > NUM_MEM_WINDOWS)
			ctrl->maxmem = NUM_MEM_WINDOWS;
		if (ctrl->maxio > NUM_IO_WINDOWS)
			ctrl->maxio = NUM_IO_WINDOWS;
		printf("PC-Card %s (%d mem & %d I/O windows)\n",
			ctrl->name, ctrl->maxmem, ctrl->maxio);
	}
	callout_handle_init(&slt->insert_ch);
	callout_handle_init(&slt->poff_ch);
#if NAPM > 0
	{
		struct apmhook *ap;

		ap = &s_hook[slt->slotnum];
		ap->ah_fun = slot_suspend;
		ap->ah_arg = (void *)slt;
		ap->ah_name = ctrl->name;
		ap->ah_order = APM_MID_ORDER;
		apm_hook_establish(APM_HOOK_SUSPEND, ap);
		ap = &r_hook[slt->slotnum];
		ap->ah_fun = slot_resume;
		ap->ah_arg = (void *)slt;
		ap->ah_name = ctrl->name;
		ap->ah_order = APM_MID_ORDER;
		apm_hook_establish(APM_HOOK_RESUME, ap);
	}
#endif /* NAPM > 0 */
	return(slt);
}

/*
 *	pccard_alloc_intr - allocate an interrupt from the
 *	free interrupts and return its number. The interrupts
 *	allowed are passed as a mask.
 */
int
pccard_alloc_intr(u_int imask, inthand2_t *hand, int unit,
		  u_int *maskp, u_int *pcic_imask)
{
	int irq;
	unsigned int mask;

	for (irq = 1; irq < ICU_LEN; irq++) {
		mask = 1ul << irq;
		if (!(mask & imask))
			continue;
		INTRMASK(*maskp, mask);
		if (register_intr(irq, 0, 0, hand, maskp, unit) == 0) {
			/* add this to the PCIC controller's mask */
			if (pcic_imask)
				INTRMASK(*pcic_imask, (1 << irq));
			update_intr_masks();
			INTREN(mask);
			return(irq);
		}
		/* No luck, remove from mask again... */
		INTRUNMASK(*maskp, mask);
		update_intr_masks();
	}
	return(-1);
}

/*
 *	allocate_driver - Create a new device entry for this
 *	slot, and attach a driver to it.
 */
static int
allocate_driver(struct slot *slt, struct dev_desc *desc)
{
	struct pccard_devinfo *devi;
	struct pccard_device *drv;
	int err, irq = 0, s;

	drv = find_driver(desc->name);
	if (drv == 0)
		return(ENXIO);
	/*
	 *	If an instance of this driver is already installed,
	 *	but not running, then remove it. If it is running,
	 *	then reject the request.
	 */
	for (devi = slt->devices; devi; devi = devi->next)
		if (devi->drv == drv && devi->isahd.id_unit == desc->unit) {
			if (devi->running)
				return(EBUSY);
			remove_device(devi);
			break;
		}
	/*
	 *	If an interrupt mask has been given, then check it
	 *	against the slot interrupt (if one has been allocated).
	 */
	if (desc->irqmask && drv->imask) {
		if ((slt->ctrl->irqs & desc->irqmask) == 0)
			return(EINVAL);
		if (slt->irq) {
			if (((1 << slt->irq) & desc->irqmask) == 0)
				return(EINVAL);
			slt->irqref++;
			irq = slt->irq;
		} else {
			/*
			 * Attempt to allocate an interrupt.
			 * XXX We lose at the moment if the second 
			 * device relies on a different interrupt mask.
			 */
			irq = pccard_alloc_intr(desc->irqmask,
				slot_irq_handler, (int)slt,
				drv->imask, slt->ctrl->imask);
			if (irq < 0)
				return(EINVAL);
			slt->irq = irq;
			slt->irqref = 1;
			slt->ctrl->mapirq(slt, slt->irq);
		}
	}
	MALLOC(devi, struct pccard_devinfo *, sizeof(*devi), M_DEVBUF, M_WAITOK);
	bzero(devi, sizeof(*devi));
	/*
	 *	Create an entry for the device under this slot.
	 */
	devi->running = 1;
	devi->drv = drv;
	devi->slt = slt;
	devi->isahd.id_irq = irq;
	devi->isahd.id_unit = desc->unit;
	devi->isahd.id_msize = desc->memsize;
	devi->isahd.id_iobase = desc->iobase;
	bcopy(desc->misc, devi->misc, sizeof(desc->misc));
	if (irq)
		devi->isahd.id_irq = 1 << irq;
	devi->isahd.id_flags = desc->flags;
	/*
	 *	Convert the memory to kernel space.
	 */
	if (desc->mem)
		devi->isahd.id_maddr = 
			(caddr_t)(desc->mem + atdevbase - IOM_BEGIN);
	else
		devi->isahd.id_maddr = 0;
	devi->next = slt->devices;
	slt->devices = devi;
	s = splhigh();
	err = drv->enable(devi);
	splx(s);
	/*
	 *	If the enable functions returns no error, then the
	 *	device has been successfully installed. If so, then
	 *	attach it to the slot, otherwise free it and return
	 *	the error.  We assume that when we free the device,
	 *	it will also set 'running' to off.
	 */
	if (err)
		remove_device(devi);
	return(err);
}

static void
remove_device(struct pccard_devinfo *devi)
{
	struct slot *slt = devi->slt;
	struct pccard_devinfo *list;

	/*
	 *	If an interrupt is enabled on this slot,
	 *	then unregister it if no-one else is using it.
	 */
	unregister_device_interrupt(devi);
	/*
	 *	Remove from device list on this slot.
	 */
	if (slt->devices == devi)
		slt->devices = devi->next;
	else
		for (list = slt->devices; list->next; list = list->next)
			if (list->next == devi) {
				list->next = devi->next;
				break;
			}
	/*
	 *	Finally, free the memory space.
	 */
	FREE(devi, M_DEVBUF);
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
	 *	Enable 5V to the card so that the CIS can be read.
	 */
	slt->pwr.vcc = 50;
	slt->pwr.vpp = 0;
	/*
	 * Disable any pending timeouts for this slot, and explicitly
	 * power it off right now.  Then, re-enable the power using
	 * the (possibly new) power settings.
	 */
	untimeout(power_off_slot, (caddr_t)slt, slt->poff_ch);
	power_off_slot(slt);
	slt->ctrl->power(slt);

	printf("Card inserted, slot %d\n", slt->slotnum);
	/*
	 *	Now start resetting the card.
	 */
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
		/*
		 *	The slot and devices are disabled, but the
		 *	data structures are not unlinked.
		 */
		if (slt->state == filled) {
			int s = splhigh();
			disable_slot(slt);
			slt->state = empty;
			splx(s);
			printf("Card removed, slot %d\n", slt->slotnum);
			pccard_remove_beep();
			selwakeup(&slt->selp);
		}
		break;
	case card_inserted:
		slt->insert_seq = 1;
		slt->insert_ch = timeout(inserted, (void *)slt, hz/4);
		pccard_remove_beep();
		break;
	}
}

/*
 *	slot_irq_handler - Interrupt handler for shared irq devices.
 */
static void
slot_irq_handler(int arg)
{
	struct pccard_devinfo *devi;
	struct slot *slt = (struct slot *)arg;

	/*
	 *	For each device that has the shared interrupt,
	 *	call the interrupt handler. If the interrupt was
	 *	caught, the handler returns true.
	 */
	for (devi = slt->devices; devi; devi = devi->next)
		if (devi->isahd.id_irq && devi->running &&
		    devi->drv->handler(devi))
			return;
	/*
	 * XXX - Should 'debounce' these for drivers that have recently
	 * been removed.
	 */
	printf("Slot %d, unfielded interrupt (%d)\n", slt->slotnum, slt->irq);
}

/*
 *	Device driver interface.
 */
static	int
crdopen(dev_t dev, int oflags, int devtype, struct proc *p)
{
	struct slot *slt;

	if (minor(dev) >= MAXSLOT)
		return(ENXIO);
	slt = pccard_slots[minor(dev)];
	if (slt == 0)
		return(ENXIO);
	if (slt->rwmem == 0)
		slt->rwmem = MDF_ATTR;
	return(0);
}

/*
 *	Close doesn't de-allocate any resources, since
 *	slots may be assigned to drivers already.
 */
static	int
crdclose(dev_t dev, int fflag, int devtype, struct proc *p)
{
	return(0);
}

/*
 *	read interface. Map memory at lseek offset,
 *	then transfer to user space.
 */
static	int
crdread(dev_t dev, struct uio *uio, int ioflag)
{
	struct slot *slt = pccard_slots[minor(dev)];
	struct mem_desc *mp, oldmap;
	unsigned char *p;
	unsigned int offs;
	int error = 0, win, count;

	if (slt == 0 || slt->state != filled)
		return(ENXIO);
	if (pccard_mem == 0)
		return(ENOMEM);
	for (win = 0; win < slt->ctrl->maxmem; win++)
		if ((slt->mem[win].flags & MDF_ACTIVE) == 0)
			break;
	if (win >= slt->ctrl->maxmem)
		return(EBUSY);
	mp = &slt->mem[win];
	oldmap = *mp;
	mp->flags = slt->rwmem|MDF_ACTIVE;
#if 0
	printf("Rd at offs %d, size %d\n", (int)uio->uio_offset,
				uio->uio_resid);
#endif
	while (uio->uio_resid && error == 0) {
		mp->card = uio->uio_offset;
		mp->size = PCCARD_MEMSIZE;
		mp->start = (caddr_t)pccard_mem;
		if (error = slt->ctrl->mapmem(slt, win))
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

	return(error);
}

/*
 *	crdwrite - Write data to card memory.
 *	Handles wrap around so that only one memory
 *	window is used.
 */
static	int
crdwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct slot *slt = pccard_slots[minor(dev)];
	struct mem_desc *mp, oldmap;
	unsigned char *p;
	unsigned int offs;
	int error = 0, win, count;

	if (slt == 0 || slt->state != filled)
		return(ENXIO);
	if (pccard_mem == 0)
		return(ENOMEM);
	for (win = 0; win < slt->ctrl->maxmem; win++)
		if ((slt->mem[win].flags & MDF_ACTIVE)==0)
			break;
	if (win >= slt->ctrl->maxmem)
		return(EBUSY);
	mp = &slt->mem[win];
	oldmap = *mp;
	mp->flags = slt->rwmem|MDF_ACTIVE;
#if 0
	printf("Wr at offs %d, size %d\n", (int)uio->uio_offset,
				uio->uio_resid);
#endif
	while (uio->uio_resid && error == 0) {
		mp->card = uio->uio_offset;
		mp->size = PCCARD_MEMSIZE;
		mp->start = (caddr_t)pccard_mem;
		if (error = slt->ctrl->mapmem(slt, win))
			break;
		offs = (unsigned int)uio->uio_offset & (PCCARD_MEMSIZE - 1);
		p = pccard_kmem + offs;
		count = MIN(PCCARD_MEMSIZE - offs, uio->uio_resid);
#if 0
	printf("Writing %d bytes to address 0x%x\n", count, p);
#endif
		error = uiomove(p, count, uio);
	}
	/*
	 *	Restore original map.
	 */
	*mp = oldmap;
	slt->ctrl->mapmem(slt, win);

	return(error);
}

/*
 *	ioctl calls - allows setting/getting of memory and I/O
 *	descriptors, and assignment of drivers.
 */
static	int
crdioctl(dev_t dev, int cmd, caddr_t data, int fflag, struct proc *p)
{
	struct slot *slt = pccard_slots[minor(dev)];
	struct mem_desc *mp;
	struct io_desc *ip;
	int s, err;

	/* beep is disabled until the 1st call of crdioctl() */
	pccard_beep_select(BEEP_ON);

	if (slt == 0 && cmd != PIOCRWMEM)
		return(ENXIO);
	switch(cmd) {
	default:
		if (slt->ctrl->ioctl)
			return(slt->ctrl->ioctl(slt, cmd, data));
		return(EINVAL);
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
		((struct slotstate *)data)->irqs = slt->ctrl->irqs;
		break;
	/*
	 * Get memory context.
	 */
	case PIOCGMEM:
		s = ((struct mem_desc *)data)->window;
		if (s < 0 || s >= slt->ctrl->maxmem)
			return(EINVAL);
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
		if (suser(p->p_ucred, &p->p_acflag))
			return(EPERM);
		if (slt->state != filled)
			return(ENXIO);
		s = ((struct mem_desc *)data)->window;
		if (s < 0 || s >= slt->ctrl->maxmem)
			return(EINVAL);
		slt->mem[s] = *((struct mem_desc *)data);
		return(slt->ctrl->mapmem(slt, s));
	/*
	 * Get I/O port context.
	 */
	case PIOCGIO:
		s = ((struct io_desc *)data)->window;
		if (s < 0 || s >= slt->ctrl->maxio)
			return(EINVAL);
		ip = &slt->io[s];
		((struct io_desc *)data)->flags = ip->flags;
		((struct io_desc *)data)->start = ip->start;
		((struct io_desc *)data)->size = ip->size;
		break;
	/*
	 * Set I/O port context.
	 */
	case PIOCSIO:
		if (suser(p->p_ucred, &p->p_acflag))
			return(EPERM);
		if (slt->state != filled)
			return(ENXIO);
		s = ((struct io_desc *)data)->window;
		if (s < 0 || s >= slt->ctrl->maxio)
			return(EINVAL);
		slt->io[s] = *((struct io_desc *)data);
		return(slt->ctrl->mapio(slt, s));
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
			if (pccard_mem)
				*(unsigned long *)data = pccard_mem;
			break;
		}
		if (suser(p->p_ucred, &p->p_acflag))
			return(EPERM);
		/*
		 * Validate the memory by checking it against the I/O
		 * memory range. It must also start on an aligned block size.
		 */
		if (invalid_io_memory(*(unsigned long *)data, PCCARD_MEMSIZE))
			return(EINVAL);
		if (*(unsigned long *)data & (PCCARD_MEMSIZE-1))
			return(EINVAL);
		/*
		 *	Map it to kernel VM.
		 */
		pccard_mem = *(unsigned long *)data;
		pccard_kmem = (unsigned char *)(pccard_mem
				+ atdevbase - IOM_BEGIN);
		break;
	/*
	 * Set power values.
	 */
	case PIOCSPOW:
		slt->pwr = *(struct power *)data;
		return(slt->ctrl->power(slt));
	/*
	 * Allocate a driver to this slot.
	 */
	case PIOCSDRV:
		if (suser(p->p_ucred, &p->p_acflag))
			return(EPERM);
		err = allocate_driver(slt, (struct dev_desc *)data);
		if (!err)
			pccard_success_beep();
		else
			pccard_failure_beep();
		return err;
	}
	return(0);
}

/*
 *	poll - Poll on exceptions will return true
 *	when a change in card status occurs.
 */
static	int
crdpoll(dev_t dev, int events, struct proc *p)
{
	int s;
	struct slot *slt = pccard_slots[minor(dev)];
	int revents = 0;

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
		selrecord(p, &slt->selp);

	splx(s);
	return (revents);
}

/*
 *	invalid_io_memory - verify that the ISA I/O memory block
 *	is a valid and unallocated address.
 *	A simple check of the range is done, and then a
 *	search of the current devices is done to check for
 *	overlapping regions.
 */
static int
invalid_io_memory(unsigned long adr, int size)
{
	/* XXX - What's magic about 0xC0000?? */
	if (adr < 0xC0000 || (adr+size) > IOM_END)
		return(1);
	return(0);
}

static struct pccard_device *
find_driver(char *name)
{
	struct pccard_device *drv;

	for (drv = drivers; drv; drv = drv->next)
		if (strcmp(drv->name, name)==0)
			return(drv);
	return(0);
}

static crd_devsw_installed = 0;

static void
crd_drvinit(void *unused)
{
	dev_t dev;

	if (!crd_devsw_installed) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev, &crd_cdevsw, NULL);
		crd_devsw_installed = 1;
	}
}

SYSINIT(crddev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,crd_drvinit,NULL)
