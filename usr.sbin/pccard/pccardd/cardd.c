/*
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

#ifndef lint
static const char rcsid[] =
	"$Id: cardd.c,v 1.29 1998/03/02 20:51:06 guido Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "cardd.h"

static struct card_config *assign_driver(struct card *);
static int		 assign_io(struct slot *);
static int		 setup_slot(struct slot *);
static void		 card_inserted(struct slot *);
static void		 card_removed(struct slot *);
static void		 pr_cmd(struct cmd *);
static void		 read_ether(struct slot *);

/*
 *	Dump configuration file data.
 */
void
dump_config_file(void)
{
	struct card *cp;
	struct card_config *confp;

	for (cp = cards; cp; cp = cp->next) {
		printf("Card manuf %s, vers %s\n", cp->manuf, cp->version);
		printf("Configuration entries:\n");
		for (confp = cp->config; confp; confp = confp->next)
			printf("\tIndex code = 0x%x, driver name = %s\n",
			    confp->index, confp->driver->name);
		if (cp->insert) {
			printf("Insert commands are:\n");
			pr_cmd(cp->insert);
		}
		if (cp->remove) {
			printf("Remove commands are:\n");
			pr_cmd(cp->remove);
		}
	}
}

static void
pr_cmd(struct cmd *cp)
{
	while (cp) {
		printf("\t%s\n", cp->line);
		cp = cp->next;
	}
}

/*
 *	readslots - read all the PCMCIA slots, and build
 *	a list of the slots.
 */
struct slot *
readslots(void)
{
	char    name[128];
	int     i, fd;
	struct slot *slots, *sp;

	slots = NULL;
	for (i = 0; i < MAXSLOT; i++) {
		sprintf(name, CARD_DEVICE, i);
		fd = open(name, 2);
		if (fd < 0)
			continue;
		sp = xmalloc(sizeof(*sp));
		sp->fd = fd;
		sp->name = newstr(name);
		sp->slot = i;
		sp->state = empty;

		/* Check to see if the controller memory has been set up. */
		if (slots == 0) {
			unsigned long mem = 0;

			if (ioctl(fd, PIOCRWMEM, &mem))
				logerr("ioctl (PIOCRWMEM)");
#ifdef DEBUG
			logmsg("mem=0x%x\n", mem);
#endif
			if (mem == 0) {
				mem = alloc_memory(4 * 1024);
				if (mem == 0)
					die("can't allocate memory for controller access");
				if (ioctl(fd, PIOCRWMEM, &mem))
					logerr("ioctl (PIOCRWMEM)");
			}
		}
#ifdef DEBUG
		printf("%p %p\n", sp, &sp->next);
#endif
		sp->next = slots;
		slots = sp;
		slot_change(sp);
	}
	return (slots);
}

/*
 *	slot_change - Card status has changed.
 *	read new state and process.
 */
void
slot_change(struct slot *sp)
{
	struct slotstate state;

	if (ioctl(sp->fd, PIOCGSTATE, &state)) {
		logerr("ioctl (PIOCGSTATE)");
		return;
	}
#ifdef DEBUG
	if (state.state == sp->state)
		logmsg("State same as before, continuing anyway");
#endif
	switch (state.state) {
	case empty:
	case noslot:
		card_removed(sp);
		break;
	case filled:
		/*
		 * If state was already filled, fake a removal first to get
		 * our state in sync with the kernel. This happens when the
		 * systems resumes and we only get to process the state 
		 * change from suspend to empty after inserted() has run.
		 * In that case the kernel state is perfectly normal.
		 *
		 * The reason for not doing nothing is that the kernel
		 * has to be informed again about IRQ and IO window.
		 */
		if (state.state == sp->state)
			card_removed(sp);
		card_inserted(sp);
		break;
	case suspend:
		/* ignored */
		break;
	}
	sp->state = state.state;
}

/*
 *	card_removed - card has been removed from slot.
 *	Execute the remove commands, and clear the slot's state.
 *	Execute the device commands, then the driver commands
 *	and then the card commands. This is the reverse
 *	order to the insertion commands
 */
void
card_removed(struct slot *sp)
{
	struct card *cp;

	if (sp->cis)
		freecis(sp->cis);
	if (sp->config) {
		sp->config->inuse = 0;
		sp->config->driver->inuse = 0;
	}
	if ((cp = sp->card) != 0)
		execute(cp->remove, sp);
	sp->cis = 0;
	sp->config = 0;
	/* release io */
	bit_nset(io_avail, sp->io.addr, sp->io.addr + sp->io.size - 1);
}

/*
 * card_inserted - Card has been inserted;
 *	- Read the CIS
 *	- match the card type.
 *	- Match the driver and allocate a driver instance.
 *	- Allocate I/O ports, memory and IRQ.
 *	- Set up the slot.
 *	- assign the driver (if failed, then terminate).
 *	- Run the card commands.
 *	- Run the driver commands
 *	- Run the device commands
 */
void
card_inserted(struct slot *sp)
{
	struct card *cp;

	sleep(5);
	sp->cis = readcis(sp->fd);
	if (sp->cis == 0) {
		logmsg("Error reading CIS on %s\n", sp->name);
		return;
	}
#if 0
	dumpcis(sp->cis);
#endif
	for (cp = cards; cp; cp = cp->next)
		if (strncmp(cp->manuf, sp->cis->manuf, CIS_MAXSTR) == 0 &&
		    strncmp(cp->version, sp->cis->vers, CIS_MAXSTR) == 0)
			break;
	sp->card = cp;
#if 0
	reset_slot(sp);
#endif
	if (cp == 0) {
		logmsg("No card in database for \"%s\"(\"%s\")",
			sp->cis->manuf, sp->cis->vers);
		return;
	}
	if (cp->ether)
		read_ether(sp);
	if ((sp->config = assign_driver(cp)) == NULL) 
		return;
	if (assign_io(sp)) {
		logmsg("Resource allocation failure for %s", sp->cis->manuf);
		return;
	}

	/*
	 *
	 * Once assigned, set up the I/O & mem contexts, set up the
	 * windows, and then attach the driver.
	 */
	if (setup_slot(sp))
		execute(cp->insert, sp);
#if 0
	else
		reset_slot(sp);
#endif
}

/*
 *	read_ether - read ethernet address from card. Offset is
 *	the offset into the attribute memory of the card.
 */
static void
read_ether(struct slot *sp)
{
	unsigned char net_addr[12];

	lseek(sp->fd, (off_t)sp->card->ether, SEEK_SET);
	if (read(sp->fd, net_addr, sizeof(net_addr)) != sizeof(net_addr)) {
		logerr("read err on net addr");
		return;
	}
	sp->eaddr[0] = net_addr[0];
	sp->eaddr[1] = net_addr[2];
	sp->eaddr[2] = net_addr[4];
	sp->eaddr[3] = net_addr[6];
	sp->eaddr[4] = net_addr[8];
	sp->eaddr[5] = net_addr[10];
	logmsg("Ether=%02x:%02x:%02x:%02x:%02x:%02x\n",
	    sp->eaddr[0], sp->eaddr[1], sp->eaddr[2],
	    sp->eaddr[3], sp->eaddr[4], sp->eaddr[5]);
}

/*
 *	assign_driver - Assign driver to card.
 *	First, see if an existing driver is already setup.
 */
static struct card_config *
assign_driver(struct card *cp)
{
	struct driver *drvp;
	struct card_config *conf;

	for (conf = cp->config; conf; conf = conf->next)
		if (conf->inuse == 0 && conf->driver->card == cp &&
		    conf->driver->config == conf) {
#ifdef	DEBUG
			logmsg("Found existing driver (%s) for %s\n",
			    conf->driver->name, cp->manuf);
#endif
			return (conf);
		}
	/*
	 * New driver must be allocated. Find one that matches the
	 * any configurations not in use.
	 */
	for (conf = cp->config; conf; conf = conf->next)
		if (conf->inuse == 0 && conf->driver->card == 0)
			break;
	if (conf == 0) {
		logmsg("No free configuration for card %s", cp->manuf);
		return (NULL);
	}
	/*
	 * Now we have a free driver and a matching configuration.
	 * Before assigning and allocating everything, check to
	 * see if a device class can be allocated to this.
	 */
	drvp = conf->driver;

	/* If none available, then we can't use this card. */
	if (drvp->inuse) {
		logmsg("Driver already being used for %s", cp->manuf);
		return (NULL);
	}
	/* Allocate a free IRQ if none has been specified */
	if (conf->irq == 0) {
		int     i;
		for (i = 1; i < 16; i++)
			if (pool_irq[i]) {
				conf->irq = i;
				pool_irq[i] = 0;
				break;
			}
		if (conf->irq == 0) {
			logmsg("Failed to allocate IRQ for %s\n", cp->manuf);
			return (NULL);
		}
	}
	drvp->card = cp;
	drvp->config = conf;
	drvp->inuse = 1;
	conf->inuse = 1;
	return (conf);
}

/*
 *	assign_io - Allocate resources to slot matching the
 *	configuration index selected.
 */
static int
assign_io(struct slot *sp)
{
	struct cis *cis;
	struct cis_config *cisconf, *defconf;

	cis = sp->cis;
	defconf = cis->def_config;
	for (cisconf = cis->conf; cisconf; cisconf = cisconf->next)
		if (cisconf->id == sp->config->index)
			break;
	if (cisconf == 0)
		return (-1);
	sp->card_config = cisconf;

	/*
	 * Found a matching configuration. Now look at the I/O, memory and IRQ
	 * to create the desired parameters. Look at memory first.
	 */
	if (cisconf->memspace || (defconf && defconf->memspace)) {
		struct cis_memblk *mp;

		mp = cisconf->mem;
		if (!cisconf->memspace)
			mp = defconf->mem;
		sp->mem.size = mp->length;
		sp->mem.cardaddr = mp->address;

		/* For now, we allocate our own memory from the pool. */
		sp->mem.addr = sp->config->driver->mem;
		/*
		 * Host memory address is required. Allocate one
		 * from our pool.
		 */
		if (sp->mem.size && sp->mem.addr == 0) {
			sp->mem.addr = alloc_memory(mp->length);
			if (sp->mem.addr == 0)
				return (-2);
			sp->config->driver->mem = sp->mem.addr;
		}
		sp->mem.cardaddr = 0x4000;
#ifdef	DEBUG
		logmsg("Using mem addr 0x%x, size %d, card addr 0x%x\n",
			sp->mem.addr, sp->mem.size, sp->mem.cardaddr);
#endif
	}

	/* Now look at I/O. */
	bzero(&sp->io, sizeof(sp->io));
	if (cisconf->iospace || (defconf && defconf->iospace)) {
		struct cis_config *cp;

		cp = cisconf;
		if (!cisconf->iospace)
			cp = defconf;
		/*
 		* If # of I/O lines decoded == 10, then card does its
 		* own decoding.
 		*
 		* If an I/O block exists, then use it.
 		* If no address (but a length) is available, allocate
 		* from the pool.
 		*/
		if (cp->io) {
			sp->io.addr = cp->io->addr;
			sp->io.size = cp->io->size;
		} else
			/*
			 * No I/O block, assume the address lines
			 * decode gives the size.
			 */
			sp->io.size = 1 << cp->io_addr;

		if (sp->io.addr == 0) {
			int     i = bit_fns(io_avail, IOPORTS, sp->io.size);

			if (i < 0)
				return (-1);
			sp->io.addr = i;
		}
		bit_nclear(io_avail, sp->io.addr,
			   sp->io.addr + sp->io.size - 1);

		/* Set up the size to take into account the decode lines. */
		sp->io.cardaddr = cp->io_addr;
		switch (cp->io_bus) {
		case 0:
			break;
		case 1:
			sp->io.flags = IODF_WS;
			break;
		case 2:
			sp->io.flags = IODF_WS | IODF_CS16;
			break;
		case 3:
			sp->io.flags = IODF_WS | IODF_CS16 | IODF_16BIT;
			break;
		}
#ifdef	DEBUG
		logmsg("Using I/O addr 0x%x, size %d\n",
		    sp->io.addr, sp->io.size);
#endif
	}
	sp->irq = sp->config->irq;
	return (0);
}

/*
 *	setup_slot - Allocate the I/O and memory contexts
 *	return true if completed OK.
 */
static int
setup_slot(struct slot *sp)
{
	struct mem_desc mem;
	struct io_desc io;
	struct dev_desc drv;
	struct driver *drvp = sp->config->driver;
	char    c;
	off_t   offs;
	int     rw_flags;

	memset(&io, 0, sizeof io);
	memset(&drv, 0, sizeof drv);
	offs = sp->cis->reg_addr;
	rw_flags = MDF_ATTR;
	ioctl(sp->fd, PIOCRWFLAG, &rw_flags);
	lseek(sp->fd, offs, SEEK_SET);
	c = 0x80;
	write(sp->fd, &c, sizeof(c));
	usleep(sp->card->reset_time * 1000);
	lseek(sp->fd, offs, SEEK_SET);
	c = 0x00;
	write(sp->fd, &c, sizeof(c));
	usleep(sp->card->reset_time * 1000);
	lseek(sp->fd, offs, SEEK_SET);
	c = sp->config->index;
	c |= 0x40;
	write(sp->fd, &c, sizeof(c));
#ifdef	DEBUG
	logmsg("Setting config reg at offs 0x%lx to 0x%x, Reset time = %d ms\n",
		(unsigned long)offs, c, sp->card->reset_time);
#endif
	sleep(5);
	usleep(sp->card->reset_time * 1000);

	/* If other config registers exist, set them up. */
	if (sp->cis->ccrs & 2) {
		/* CCSR */
		c = 0;
		if (sp->cis->def_config && sp->cis->def_config->misc_valid &&
		    (sp->cis->def_config->misc & 0x8))
			c |= 0x08;
		if (sp->card_config->io_bus == 1)
			c |= 0x20;
		lseek(sp->fd, offs + 2, SEEK_SET);
		write(sp->fd, &c, sizeof(c));
	}
	mem.window = 0;
	if (sp->mem.addr) {
		mem.window = 0;
		mem.flags = sp->mem.flags | MDF_ACTIVE | MDF_16BITS;
		mem.start = (caddr_t) sp->mem.addr;
		mem.card = sp->mem.cardaddr;
		mem.size = sp->mem.size;
		if (ioctl(sp->fd, PIOCSMEM, &mem)) {
			logerr("ioctl (PIOCSMEM)");
			return (0);
		}
	}
	io.window = 0;
	if (sp->io.size) {
		io.flags = sp->io.flags;
		io.start = sp->io.addr;
		io.size = sp->io.size;
#if 0
		io.start = sp->io.addr & ~((1 << sp->io.cardaddr) - 1);
		io.size = 1 << sp->io.cardaddr;
		if (io.start < 0x100) {
			io.start = 0x100;
			io.size = 0x300;
		}
#endif
#ifdef	DEBUG
		logmsg("Assigning I/O window %d, start 0x%x, size 0x%x flags 0x%x\n",
			io.window, io.start, io.size, io.flags);
#endif
		io.flags |= IODF_ACTIVE;
		if (ioctl(sp->fd, PIOCSIO, &io)) {
			logerr("ioctl (PIOCSIO)");
			return (0);
		}
	}
	strcpy(drv.name, drvp->kernel);
	drv.unit = drvp->unit;
	drv.irqmask = 1 << sp->irq;
	drv.flags = 0x80;
	if (sp->mem.size) {
		drv.mem = sp->mem.addr;
		drv.memsize = sp->mem.size;
	} else {
		drv.mem = 0;
		drv.memsize = 0;
	}
	if (sp->io.size)
		drv.iobase = sp->io.addr;
	else
		drv.iobase = 0;
#ifdef	DEBUG
	logmsg("Assign %s%d, io 0x%x, mem 0x%lx, %d bytes, irq %d, flags %x\n",
	    drv.name, drv.unit, drv.iobase, drv.mem, drv.memsize, sp->irq, drv.flags);
#endif

	/*
	 * If the driver fails to be connected to the device,
	 * then it may mean that the driver did not recognise it.
	 */
	memcpy(drv.misc, sp->eaddr, 6);
	if (ioctl(sp->fd, PIOCSDRV, &drv)) {
		logmsg("driver allocation failed for %s", sp->card->manuf);
		return (0);
	}
	return (1);
}
