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
 */

#include "crd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/devconf.h>
#include <sys/malloc.h>
#include <sys/devconf.h>
#include <sys/conf.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>

#include <pccard/card.h>
#include <pccard/driver.h>
#include <pccard/slot.h>

#include <i386/include/laptops.h>

extern struct kern_devconf kdc_cpu0;

struct kern_devconf kdc_pccard0 = {
	0, 0, 0,			/* filled in by dev_attach */
	"pccard", 0, { MDDT_BUS, 0 },
	0, 0, 0, BUS_EXTERNALLEN,
	&kdc_cpu0,			/* parent is the CPU */
	0,				/* no parentdata */
	DC_UNCONFIGURED,		/* until we see it */
	"PCCARD or PCMCIA bus",
	DC_CLS_BUS			/* class */
};

#define	PCCARD_MEMSIZE	(4*1024)

#define MIN(a,b)	((a)<(b)?(a):(b))

/*
 *	cdevsw entry points
 */
int	crdopen	__P((dev_t dev, int oflags, int devtype,
				 struct proc *p));
int	crdclose	__P((dev_t dev, int fflag, int devtype,
				 struct proc *p));
int	crdread	__P((dev_t dev, struct uio *uio, int ioflag));
int	crdwrite	__P((dev_t dev, struct uio *uio, int ioflag));
int	crdioctl	__P((dev_t dev, int cmd, caddr_t data,
				 int fflag, struct proc *p));
int	crdselect	__P((dev_t dev, int rw, struct proc *p));

static int		allocate_driver(struct slot *, struct drv_desc *);
static void		inserted(void *);
static void		disable_slot(struct slot *);
static int		invalid_io_memory(unsigned long, int);
static struct pccard_drv *find_driver(char *);
static void		remove_device(struct pccard_dev *);
static void		slot_irq_handler(int);

static struct slot	*pccard_slots[MAXSLOT];	/* slot entries */
static struct slot	*slot_list;
static struct slot_ctrl *cont_list;
static struct pccard_drv *drivers;		/* Card drivers */

/*
 *	The driver interface for read/write uses a block
 *	of memory in the ISA I/O memory space allocated via
 *	an ioctl setting.
 */
static unsigned long pccard_mem;	/* Physical memory */
static unsigned char *pccard_kmem;	/* Kernel virtual address */

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
pccard_configure()
{
	dev_attach(&kdc_pccard0);

#include "pcic.h"
#if NPCIC > 0
	pcic_probe();
#endif
}

/*
 *	pccard_add_driver - Add a new driver to the list of
 *	drivers available for allocation.
 */
void
pccard_add_driver(struct pccard_drv *dp)
{
	/*
	 *	If already loaded, then reject the driver.
	 */
	if (find_driver(dp->name)) {
		printf("Driver %s already loaded\n", dp->name);
		return;
	}
	printf("pccard driver %s added\n", dp->name);
	dp->next = drivers;
	drivers = dp;
}

/*
 *	pccard_remove_driver - called to unlink driver
 *	from devices. Usually called when drivers are
 *	are unloaded from kernel.
 */
void
pccard_remove_driver(struct pccard_drv *dp)
{
	struct slot *sp;
	struct pccard_dev *devp, *next;
	struct pccard_drv *drvp;

	for (sp = slot_list; sp; sp = sp->next)
		for (devp = sp->devices; devp; devp = next) {
			next = devp->next;
			if (devp->drv == dp)
				remove_device(devp);
		}
	/*
	 *	Once all the devices belonging to this driver have been
	 *	freed, then remove the driver from the list
	 *	of registered drivers.
	 */
	if (drivers == dp)
		drivers = dp->next;
	else
		for (drvp = drivers; drvp->next; drvp = drvp->next)
			if (drvp->next == dp) {
				drvp->next = dp->next;
				break;
			}
}

/*
 *	pccard_remove_controller - Called when the slot
 *	driver is unloaded. The plan is to unload
 *	drivers from the slots, and then remove the
 *	slots from the slot list, and then finally
 *	remove the controller structure. Messy...
 */
void
pccard_remove_controller(struct slot_ctrl *cp)
{
	struct slot *sp, *next, *last = 0;
	struct slot_ctrl *cl;
	struct pccard_dev *dp;

	for (sp = slot_list; sp; sp = next) {
		next = sp->next;
		/*
		 *	If this slot belongs to this controller,
		 *	remove this slot.
		 */
		if (sp->ctrl == cp) {
			pccard_slots[sp->slot] = 0;
			if (sp->insert_seq)
				untimeout(inserted, (void *)sp);
			/*
			 * Unload the drivers attached to this slot.
			 */
			while (dp = sp->devices)
				remove_device(dp);
			/*
			 * Disable the slot and unlink the slot from the 
			 * slot list.
			 */
			disable_slot(sp);
			if (last)
				last->next = next;
			else
				slot_list = next;
			if (cp->extra && sp->cdata)
				FREE(sp->cdata, M_DEVBUF);
			FREE(sp, M_DEVBUF);
			/*
			 *	xx Can't use sp after we have freed it.
			 */
		} else {
			last = sp;
		}
	}
	/*
	 *	Unlink controller structure from controller list.
	 */
	if (cont_list == cp)
		cont_list = cp->next;
	else
		for (cl = cont_list; cl->next; cl = cl->next)
			if (cl->next == cp) {
				cl->next = cp->next;
				break;
			}
}

/*
 *	disable_slot - Disables the slot by removing
 *	the power and unmapping the I/O
 */
static void
disable_slot(struct slot *sp)
{
	int i;
	struct pccard_dev *devp;
	/*
	 * Unload all the drivers on this slot. Note we can't
	 * call remove_device from here, because this may be called
	 * from the event routine, which is called from the slot
	 * controller's ISR, and this could remove the device
	 * structure out in the middle of some driver activity.
	 *
	 * Note that a race condition is possible here; if a
	 * driver is accessing the device and it is removed, then
	 * all bets are off...
	 */
	for (devp = sp->devices; devp; devp = devp->next) {
		if (devp->running) {
			int s = splhigh();
			devp->drv->unload(devp);
			devp->running = 0;
			if (devp->isahd.id_irq && --sp->irqref == 0) {
				printf("Return IRQ=%d\n",sp->irq);
				sp->ctrl->mapirq(sp, 0);
				INTRDIS(1<<sp->irq);
				unregister_intr(sp->irq, slot_irq_handler);
				if (devp->drv->imask)
					INTRUNMASK(*devp->drv->imask,(1<<sp->irq));
				sp->irq = 0;
				}
			splx(s);
		}
	}
	/* Power off the slot. */
	sp->ctrl->disable(sp);

	/* De-activate all contexts.  */
	for (i = 0; i < sp->ctrl->maxmem; i++)
		if (sp->mem[i].flags & MDF_ACTIVE) {
			sp->mem[i].flags = 0;
			(void)sp->ctrl->mapmem(sp, i);
		}
	for (i = 0; i < sp->ctrl->maxio; i++)
		if (sp->io[i].flags & IODF_ACTIVE) {
			sp->io[i].flags = 0;
			(void)sp->ctrl->mapio(sp, i);
		}
}

/*
 *	pccard_alloc_slot - Called from controller probe
 *	routine, this function allocates a new PC-CARD slot
 *	and initialises the data structures using the data provided.
 *	It returns the allocated structure to the probe routine
 *	to allow the controller specific data to be initialised.
 */
struct slot *
pccard_alloc_slot(struct slot_ctrl *cp)
{
	struct slot *sp;
	int	slotno;

	for (slotno = 0; slotno < MAXSLOT; slotno++)
		if (pccard_slots[slotno] == 0)
			break;
	if (slotno >= MAXSLOT)
		return(0);
	kdc_pccard0.kdc_state = DC_BUSY;
	MALLOC(sp, struct slot *, sizeof(*sp), M_DEVBUF, M_WAITOK);
	bzero(sp, sizeof(*sp));
	if (cp->extra) {
		MALLOC(sp->cdata, void *, cp->extra, M_DEVBUF, M_WAITOK);
		bzero(sp->cdata, cp->extra);
	}
	sp->ctrl = cp;
	sp->slot = slotno;
	pccard_slots[slotno] = sp;
	sp->next = slot_list;
	slot_list = sp;
	/*
	 *	If this controller hasn't been seen before, then
	 *	link it into the list of controllers.
	 */
	if (cp->slots++ == 0) {
		cp->next = cont_list;
		cont_list = cp;
		if (cp->maxmem > NUM_MEM_WINDOWS)
			cp->maxmem = NUM_MEM_WINDOWS;
		if (cp->maxio > NUM_IO_WINDOWS)
			cp->maxio = NUM_IO_WINDOWS;
		printf("PC-Card %s (%d mem & %d I/O windows)\n",
			cp->name, cp->maxmem, cp->maxio);
	}
	return(sp);
}

/*
 *	pccard_alloc_intr - allocate an interrupt from the
 *	free interrupts and return its number. The interrupts
 *	allowed are passed as a mask.
 */
int
pccard_alloc_intr(int imask, inthand2_t *hand, int unit, int *maskp)
{
	int irq;
	unsigned int mask;

	for (irq = 1; irq < 16; irq++) {
		mask = 1ul << irq;
		if (!(mask & imask))
			continue;
		if (maskp)
			INTRMASK (*maskp, mask);
		if (register_intr(irq, 0, 0, hand, maskp, unit)==0) {
			INTREN (mask);
			return(irq);
		}
		/* Well no luck, remove from mask again... */
		if (maskp)
			INTRUNMASK (*maskp, mask);
	}
	return(-1);
}

/*
 *	allocate_driver - Create a new device entry for this
 *	slot, and attach a driver to it.
 */
static int
allocate_driver(struct slot *sp, struct drv_desc *drvp)
{
	struct pccard_dev *devp;
	struct pccard_drv *dp;
	int err, irq = 0, s;

	dp = find_driver(drvp->name);
	if (dp == 0)
		return(ENXIO);
	/*
	 *	If an instance of this driver is already installed,
	 *	but not running, then remove it. If it is running,
	 *	then reject the request.
	 */
	for (devp = sp->devices; devp; devp = devp->next)
		if (devp->drv == dp && devp->isahd.id_unit == drvp->unit) {
			if (devp->running)
				return(EBUSY);
			remove_device(devp);
			break;
		}
	/*
	 *	If an interrupt mask has been given, then check it
	 *	against the slot interrupt (if one has been allocated).
	 */
	if (drvp->irqmask && dp->imask) {
		if ((sp->ctrl->irqs & drvp->irqmask)==0)
			return(EINVAL);
		if (sp->irq) {
			if (((1 << sp->irq)&drvp->irqmask)==0)
				return(EINVAL);
			sp->irqref++;
			irq = sp->irq;
		} else {
			/*
			 * Attempt to allocate an interrupt.
			 * XXX We lose at the moment if the second 
			 * device relies on a different interrupt mask.
			 */
			irq = pccard_alloc_intr(drvp->irqmask,
				slot_irq_handler, (int)sp, dp->imask);
			if (irq < 0)
				return(EINVAL);
			sp->irq = irq;
			sp->irqref = 1;
			sp->ctrl->mapirq(sp, sp->irq);
		}
	}
	MALLOC(devp, struct pccard_dev *, sizeof(*devp), M_DEVBUF, M_WAITOK);
	bzero(devp, sizeof(*devp));
	/*
	 *	Create an entry for the device under this slot.
	 */
	devp->drv = dp;
	devp->sp = sp;
	devp->isahd.id_irq = irq;
	devp->isahd.id_unit = drvp->unit;
	devp->isahd.id_msize = drvp->memsize;
	devp->isahd.id_iobase = drvp->iobase;
	bcopy(drvp->misc, devp->misc, sizeof drvp->misc);
	if (irq)
		devp->isahd.id_irq = 1 << irq;
	devp->isahd.id_flags = drvp->flags;
	/*
	 *	Convert the memory to kernel space.
	 */
	if (drvp->mem)
		devp->isahd.id_maddr = 
			(caddr_t)(drvp->mem + atdevbase - 0xA0000);
	else
		devp->isahd.id_maddr = 0;
	devp->next = sp->devices;
	sp->devices = devp;
	s = splhigh();
	err = dp->init(devp, 1);
	splx(s);
	/*
	 *	If the init functions returns no error, then the
	 *	device has been successfully installed. If so, then
	 *	attach it to the slot, otherwise free it and return
	 *	the error.
	 */
	if (err)
		remove_device(devp);
	else
		devp->running = 1;
	return(err);
}

static void
remove_device(struct pccard_dev *devp)
{
	struct slot *sp = devp->sp;
	struct pccard_dev *list;
	int s;

	/*
	 *	If an interrupt is enabled on this slot,
	 *	then unregister it if no-one else is using it.
	 */
	s = splhigh();
	if (devp->running) {
		devp->drv->unload(devp);
		devp->running = 0;
	}
	if (devp->isahd.id_irq && --sp->irqref == 0) {
		printf("Return IRQ=%d\n",sp->irq);
		sp->ctrl->mapirq(sp, 0);
		INTRDIS(1<<sp->irq);
		unregister_intr(sp->irq, slot_irq_handler);
		if (devp->drv->imask)
			INTRUNMASK(*devp->drv->imask,(1<<sp->irq));
		sp->irq = 0;
	}
	splx(s);
	/*
	 *	Remove from device list on this slot.
	 */
	if (sp->devices == devp)
		sp->devices = devp->next;
	else
		for (list = sp->devices; list->next; list = list->next)
			if (list->next == devp) {
				list->next = devp->next;
				break;
			}
	/*
	 *	Finally, free the memory space.
	 */
	FREE(devp, M_DEVBUF);
}

/*
 *	card insert routine - Called from a timeout to debounce
 *	insertion events.
 */
static void
inserted(void *arg)
{
	struct slot *sp = arg;

	sp->state = filled;
	/*
	 *	Enable 5V to the card so that the CIS can be read.
	 */
	sp->pwr.vcc = 50;
	sp->pwr.vpp = 0;
	sp->ctrl->power(sp);
	printf("Card inserted, slot %d\n", sp->slot);
	/*
	 *	Now start resetting the card.
	 */
	sp->ctrl->reset(sp);
}

/*
 *	Insert/Remove beep
 */

static int beepok = 1;

static void enable_beep(void *dummy)
{
	beepok = 1;
}

/*
 *	Card event callback. Called at splhigh to prevent
 *	device interrupts from interceding.
 */
void
pccard_event(struct slot *sp, enum card_event event)
{
int s;

	if (sp->insert_seq) {
		sp->insert_seq = 0;
		untimeout(inserted, (void *)sp);
	}
	switch(event) {
	case card_removed:
		/*
		 *	The slot and devices are disabled, but the
		 *	data structures are not unlinked.
		 */
		if (sp->state == filled) {
			s = splhigh();
			disable_slot(sp);
			sp->state = empty;
			splx(s);
			printf("Card removed, slot %d\n", sp->slot);
			sysbeep(PCCARD_BEEP_PITCH0, PCCARD_BEEP_DURATION0);
			beepok = 0;
			timeout(enable_beep, (void *)NULL, hz/5);
			selwakeup(&sp->selp);
		}
		break;
	case card_inserted:
		sp->insert_seq = 1;
		timeout(inserted, (void *)sp, hz/4);
		sysbeep(PCCARD_BEEP_PITCH0, PCCARD_BEEP_DURATION0);
		beepok = 0;
		timeout(enable_beep, (void *)NULL, hz/5);
		break;
	}
}

/*
 *	slot_irq_handler - Interrupt handler for shared irq devices.
 */
static void
slot_irq_handler(int sp)
{
	struct pccard_dev *dp;

	/*
	 *	For each device that has the shared interrupt,
	 *	call the interrupt handler. If the interrupt was
	 *	caught, the handler returns true.
	 */
	for (dp = ((struct slot *)sp)->devices; dp; dp = dp->next)
		if (dp->isahd.id_irq && dp->running && dp->drv->handler(dp))
			return;
	printf("Slot %d, unfielded interrupt (%d)\n",
		((struct slot *)sp)->slot, ((struct slot *)sp)->irq);
}

	/*
	 *	Device driver interface.
	 */
int
crdopen(dev_t dev, int oflags, int devtype, struct proc *p)
{
struct slot *sp;

	if (minor(dev) >= MAXSLOT)
		return(ENXIO);
	sp = pccard_slots[minor(dev)];
	if (sp==0)
		return(ENXIO);
	if (sp->rwmem == 0)
		sp->rwmem = MDF_ATTR;
	return(0);
}

/*
 *	Close doesn't de-allocate any resources, since
 *	slots may be assigned to drivers already.
 */
int
crdclose(dev_t dev, int fflag, int devtype, struct proc *p)
{
	return(0);
}

/*
 *	read interface. Map memory at lseek offset,
 *	then transfer to user space.
 */
int
crdread(dev_t dev, struct uio *uio, int ioflag)
{
	struct slot *sp = pccard_slots[minor(dev)];
	unsigned char *p;
	int	error = 0, win, count;
	struct mem_desc *mp, oldmap;
	unsigned int offs;

	if (sp == 0 || sp->state != filled)
		return(ENXIO);
	if (pccard_mem == 0)
		return(ENOMEM);
	for (win = 0; win < sp->ctrl->maxmem; win++)
		if ((sp->mem[win].flags & MDF_ACTIVE)==0)
			break;
	if (win >= sp->ctrl->maxmem)
		return(EBUSY);
	mp = &sp->mem[win];
	oldmap = *mp;
	mp->flags = sp->rwmem|MDF_ACTIVE;
#if 0
	printf("Rd at offs %d, size %d\n", (int)uio->uio_offset,
				uio->uio_resid);
#endif
	while (uio->uio_resid && error == 0) {
		mp->card = uio->uio_offset;
		mp->size = PCCARD_MEMSIZE;
		mp->start = (caddr_t)pccard_mem;
		if (error = sp->ctrl->mapmem(sp, win))
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
	sp->ctrl->mapmem(sp, win);

	return(error);
}

/*
 *	crdwrite - Write data to card memory.
 *	Handles wrap around so that only one memory
 *	window is used.
 */
int
crdwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct slot *sp = pccard_slots[minor(dev)];
	unsigned char *p;
	int	error = 0, win, count;
	struct mem_desc *mp, oldmap;
	unsigned int offs;

	if (sp == 0 || sp->state != filled)
		return(ENXIO);
	if (pccard_mem == 0)
		return(ENOMEM);
	for (win = 0; win < sp->ctrl->maxmem; win++)
		if ((sp->mem[win].flags & MDF_ACTIVE)==0)
			break;
	if (win >= sp->ctrl->maxmem)
		return(EBUSY);
	mp = &sp->mem[win];
	oldmap = *mp;
	mp->flags = sp->rwmem|MDF_ACTIVE;
#if 0
	printf("Wr at offs %d, size %d\n", (int)uio->uio_offset,
				uio->uio_resid);
#endif
	while (uio->uio_resid && error == 0) {
		mp->card = uio->uio_offset;
		mp->size = PCCARD_MEMSIZE;
		mp->start = (caddr_t)pccard_mem;
		if (error = sp->ctrl->mapmem(sp, win))
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
	sp->ctrl->mapmem(sp, win);

	return(error);
}

/*
 *	ioctl calls - allows setting/getting of memory and I/O
 *	descriptors, and assignment of drivers.
 */
int
crdioctl(dev_t dev, int cmd, caddr_t data, int fflag, struct proc *p)
{
	int	s, err;
	struct slot *sp = pccard_slots[minor(dev)];
	struct mem_desc *mp;
	struct io_desc *ip;

	if (sp == 0 && cmd != PIOCRWMEM)
		return(ENXIO);
	switch(cmd) {
	default:
		if (sp->ctrl->ioctl)
			return(sp->ctrl->ioctl(sp, cmd, data));
		return(EINVAL);
	case PIOCGSTATE:
		s = splhigh();
		((struct slotstate *)data)->state = sp->state;
		sp->laststate = sp->state;
		splx(s);
		((struct slotstate *)data)->maxmem = sp->ctrl->maxmem;
		((struct slotstate *)data)->maxio = sp->ctrl->maxio;
		((struct slotstate *)data)->irqs = sp->ctrl->irqs;
		break;
	/*
	 * Get memory context.
	 */
	case PIOCGMEM:
		s = ((struct mem_desc *)data)->window;
		if (s < 0 || s >= sp->ctrl->maxmem)
			return(EINVAL);
		mp = &sp->mem[s];
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
		if (sp->state != filled)
			return(ENXIO);
		s = ((struct mem_desc *)data)->window;
		if (s < 0 || s >= sp->ctrl->maxmem)
			return(EINVAL);
		sp->mem[s] = *((struct mem_desc *)data);
		return(sp->ctrl->mapmem(sp, s));
	/*
	 * Get I/O port context.
	 */
	case PIOCGIO:
		s = ((struct io_desc *)data)->window;
		if (s < 0 || s >= sp->ctrl->maxio)
			return(EINVAL);
		ip = &sp->io[s];
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
		if (sp->state != filled)
			return(ENXIO);
		s = ((struct io_desc *)data)->window;
		if (s < 0 || s >= sp->ctrl->maxio)
			return(EINVAL);
		sp->io[s] = *((struct io_desc *)data);
		return(sp->ctrl->mapio(sp, s));
		break;
	/*
	 *	Set memory window flags for read/write interface.
	 */
	case PIOCRWFLAG:
		sp->rwmem = *(int *)data;
		break;
	/*
	 *	Set the memory window to be used for the read/write
	 *	interface.
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
				+ atdevbase - 0xA0000);
		break;
	/*
	 *	Set power values
	 */
	case PIOCSPOW:
		sp->pwr = *(struct power *)data;
		return(sp->ctrl->power(sp));
	/*
	 *	Allocate a driver to this slot.
	 */
	case PIOCSDRV:
		if (suser(p->p_ucred, &p->p_acflag))
			return(EPERM);
		err = allocate_driver(sp, (struct drv_desc *)data);
		if (!err)
			sysbeep(PCCARD_BEEP_PITCH1, PCCARD_BEEP_DURATION1);
		else
			sysbeep(PCCARD_BEEP_PITCH2, PCCARD_BEEP_DURATION2);
		return err;
		}
	return(0);
}

/*
 *	select - Selects on exceptions will return true
 *	when a change in card status occurs.
 */
int
crdselect(dev_t dev, int rw, struct proc *p)
{
	int s;
	struct slot *sp = pccard_slots[minor(dev)];

	switch (rw) {
	case FREAD:
		return 1;
	case FWRITE:
		return 1;
	/*
	 *	select for exception - card event.
	 */
	case 0:
		s = splhigh();
		if (sp == 0 || sp->laststate != sp->state) {
			splx(s);
			return(1);
		}
		selrecord(p, &sp->selp);
		splx(s);
	}
	return(0);
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
	if (adr < 0xC0000 || (adr+size) > 0x100000)
		return(1);
	return(0);
}

static struct pccard_drv *
find_driver(char *name)
{
	struct pccard_drv *dp;

	for (dp = drivers; dp; dp = dp->next)
		if (strcmp(dp->name, name)==0)
			return(dp);
	return(0);
}
