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
  "$FreeBSD$";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <regex.h>
#include <machine/resource.h>
#include <sys/ioctl.h>
#include "cardd.h"

static struct card_config *assign_driver(struct slot *, struct card *);
static int		 assign_io(struct slot *);
static int		 setup_slot(struct slot *);
static void		 card_inserted(struct slot *);
static void		 card_removed(struct slot *);
static void		 pr_cmd(struct cmd *);
static void		 read_ether(struct slot *);
static void		 read_ether_attr2(struct slot *sp);

struct slot *slots;

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
		for (confp = cp->config; confp; confp = confp->next) {
			printf("\tIndex code = ");
			switch (confp->index_type) {
			case DEFAULT_INDEX:
				printf("default");
				break;
			case AUTO_INDEX:
				printf("auto");
				break;
			default:
				printf("0x%x", confp->index);
				break;
			}
			printf(", driver name = %s\n", confp->driver->name);
		}
		if (cp->insert) {
			printf("Insert commands are:\n");
			pr_cmd(cp->insert);
		}
		if (cp->remove) {
			printf("Remove commands are:\n");
			pr_cmd(cp->remove);
		}
	}
	fflush(stdout);
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
	struct slot *sp;

	slots = NULL;
	for (i = 0; i < MAXSLOT; i++) {
		sprintf(name, CARD_DEVICE, i);
		fd = open(name, O_RDWR);
		if (fd < 0)
			continue;
		sp = xmalloc(sizeof(*sp));
		sp->fd = fd;
		sp->name = newstr(name);
		sp->slot = i;

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
					die("can't allocate memory for "
					    "controller access");
				if (ioctl(fd, PIOCRWMEM, &mem))
					logerr("ioctl (PIOCRWMEM)");
			}
		}
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
	switch (state.state) {
	case empty:
	case inactive:
	case noslot:
		/* Debounce potentially incorrectly reported removals */
		if (state.laststate == filled || state.laststate == suspend)
			card_removed(sp);
		break;
	case filled:
		/* KLUDGE: if we were suspended, remove card */
		if (state.laststate == suspend)
			card_removed(sp);
		card_inserted(sp);
		break;
	case suspend:
		/* ignored */
		break;
	}
	sp->state = state.state;
	stat_changed(sp);
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
	struct allocblk *sio;
	int in_use = 0;

	if (sp->config && sp->config->driver && sp->card)
		logmsg("%s%d: %s removed.", sp->config->driver->kernel,
		    sp->config->driver->unit, sp->card->logstr);
	if (sp->cis)
		freecis(sp->cis);
	if (sp->config) {
		if (sp->config->inuse && sp->config->driver->inuse)
			in_use = 1;
		sp->config->inuse = 0;
		sp->config->driver->inuse = 0;
	}
	if ((cp = sp->card) != 0 && in_use)
		execute(cp->remove, sp);
	sp->cis = 0;
	sp->config = 0;
	/* release io */
	if (sp->flags & IO_ASSIGNED) 
            for (sio = &sp->io; sio; sio = sio->next)
                if (sio->addr && sio->size)
			bit_nset(io_avail, sio->addr, sio->addr + sio->size - 1);
	/* release irq */
	if (sp->flags & IRQ_ASSIGNED)
		if (sp->irq >= 1 && sp->irq <= 15)
			pool_irq[sp->irq] = 1;
}

/* CIS string comparison */

#define	REGCOMP_FLAGS	(REG_EXTENDED | REG_NOSUB)
#define	REGEXEC_FLAGS	(0)

static int
cis_strcmp(char *db, char *cis)
{
	int     res, err;
	char    buf[256];
	regex_t rx;
	char *	p;
	size_t	n;

	if (!db || !cis) {
		return -1;
	}
	n = strlen(db);
	if (n > 2 && db[0] == '/' && db[n-1] == '/') {
		/* matching by regex */
		db++;
	} else {
		/* otherwise, matching by strncmp() */
		return strncmp(db, cis, n);
	}
	p = xmalloc(n);
	strncpy(p + 1, db, n-2);
	*p = '^';
	db = p;
	if ((err = regcomp(&rx, p, REGCOMP_FLAGS))) {
		regerror(err, &rx, buf, sizeof buf);
		logmsg("Warning: REGEX error for\"%s\" -- %s\n", p, buf);
		regfree(&rx);
		free(p);
		return -1;
	}
	res = regexec(&rx, cis, 0, NULL, REGEXEC_FLAGS);
	regfree(&rx);
	free(p);
	return res;
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
	int err;

	usleep(pccard_init_sleep);
	sp->cis = readcis(sp->fd);
	if (sp->cis == 0) {
		logmsg("Error reading CIS on %s\n", sp->name);
		return;
	}
#if 0
	dumpcis(sp->cis);
#endif
	for (cp = cards; cp; cp = cp->next) {
		switch (cp->deftype) {
		case DT_VERS:
			if (cis_strcmp(cp->manuf, sp->cis->manuf) == 0 &&
			    cis_strcmp(cp->version, sp->cis->vers) == 0) {
				if (cp->add_info1 != NULL &&
				    cis_strcmp(cp->add_info1, sp->cis->add_info1) != 0) {
					break;
				}
				if (cp->add_info2 != NULL &&
				    cis_strcmp(cp->add_info2, sp->cis->add_info2) != 0) {
					break;
				}

				logmsg("Card \"%s\"(\"%s\") "
					"[%s] [%s] "
					"matched \"%s\" (\"%s\") "
					"[%s] [%s] ",
				    sp->cis->manuf, sp->cis->vers,
				    sp->cis->add_info1, sp->cis->add_info2,
				    cp->manuf, cp->version,
				    cp->add_info1, cp->add_info2);
				goto escape;
			}
			break;
                case DT_FUNC:
                        if (cp->func_id == sp->cis->func_id1) {
                                logmsg("Card \"%s\"(\"%s\") "
                                       "[%s] [%s] "
                                       "has function ID %d\n",
                                    sp->cis->manuf, sp->cis->vers,
                                    sp->cis->add_info1, sp->cis->add_info2,
                                    cp->func_id);
                                goto escape;
                        }
                        break;
		default:
			logmsg("Unknown deftype %d\n", cp->deftype);
			die("cardd.c:card_inserted()");
		}
	}
escape:
	sp->card = cp;
#if 0
	reset_slot(sp);
#endif
	if (cp == 0) {
		logmsg("No card in database for \"%s\"(\"%s\")",
			sp->cis->manuf, sp->cis->vers);
		return;
	}
	if (sp->cis->lan_nid && sp->cis->lan_nid[0] == sizeof(sp->eaddr)) {
		bcopy(sp->cis->lan_nid + 1, sp->eaddr, sizeof(sp->eaddr));
		sp->flags |= EADDR_CONFIGED;
	} else {
		bzero(sp->eaddr, sizeof(sp->eaddr));
	}

	if (cp->ether) {
		struct ether *e = 0;
		e = cp->ether;
		switch (e->type) {
		case ETHTYPE_ATTR2:
			read_ether_attr2(sp);
			break;
		default:
			read_ether(sp);
			break;
		}
	}
	if ((sp->config = assign_driver(sp, cp)) == NULL) 
		return;
	if ((err = assign_io(sp))) {
		char *reason;

		switch (err) {
		case -1:
			reason = "specified CIS was not found";
			break;
		case -2:
			reason = "memory block allocation failed";
			break;
		case -3:
			reason = "I/O block allocation failed";
			break;
		case -4:
			reason = "requires more than one memory window";
			break;
		default:
			reason = "Unknown";
			break;
		}
                logmsg("Resource allocation failure for \"%s\"(\"%s\") "
                       "[%s] [%s]; Reason %s\n",
                    sp->cis->manuf, sp->cis->vers,
                    sp->cis->add_info1, sp->cis->add_info2, reason);
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
	int flags = MDF_ATTR;	/* attribute memory */

	ioctl(sp->fd, PIOCRWFLAG, &flags);
	lseek(sp->fd, (off_t)sp->card->ether->value, SEEK_SET);
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
	sp->flags |= EADDR_CONFIGED;
}

/*
 *      Megahertz X-Jack Ethernet uses unique way to get/set MAC
 *      address of the card.
 */
static void
read_ether_attr2(struct slot *sp)
{
	int	i;
	char	*hexaddr;

	hexaddr = sp->cis->add_info2;
	for (i = 0; i < 6; i++)
		sp->eaddr[i] = 0;
	if (!hexaddr)
		return;
	if (strlen(hexaddr) != 12)
		return;
	for (i = 0; i < 12; i++)
		if (!isxdigit(hexaddr[i]))
			return;
	for (i = 0; i < 6; i++) {
		u_int	d;
		char	s[3];
		s[0] = hexaddr[i * 2];
		s[1] = hexaddr[i * 2 + 1];
		s[2] = '\0';
		if (!sscanf(s, "%x", &d)) {
			int	j;
			for (j = 0; j < 6; j++)
				sp->eaddr[j] = 0;
			return;
		}
		sp->eaddr[i] = (u_char)d;
	}
	sp->flags |= EADDR_CONFIGED;
}


/*
 *	assign_driver - Assign driver to card.
 *	First, see if an existing driver is already setup.
 */
static struct card_config *
assign_driver(struct slot *sp, struct card *cp)
{
	struct driver *drvp;
	struct card_config *conf;
	struct pccard_resource res;
	int i;
	int irqmin, irqmax;

	for (conf = cp->config; conf; conf = conf->next)
		if (conf->inuse == 0 && conf->driver->card == cp &&
		    conf->driver->config == conf &&
		    conf->driver->inuse == 0) {
			if (debug_level > 0) {
				logmsg("Found existing driver (%s) for %s\n",
				    conf->driver->name, cp->manuf);
			}
			conf->driver->inuse = 1;
			conf->inuse = 1;
			return (conf);
		}
	/*
	 * New driver must be allocated. Find the first configuration
	 * not in use.
	 */
	for (conf = cp->config; conf; conf = conf->next)
		if (conf->inuse == 0 && conf->driver->inuse/*card*/ == 0)
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

	/*
	 * Allocate a free IRQ if none has been specified.  When we're
	 * sharing interrupts (cardbus bridge case), then we'll use what
	 * the kernel tells us to use, reguardless of what the user
	 * configured.  Asking the kernel for IRQ 0 is our way of asking
	 * if we should use a shared interrupt.
	 */
	res.type = SYS_RES_IRQ;
	res.size = 1;
	if (conf->irq == 0) {
		irqmin = 1;
		irqmax = 15;
	} else {
		irqmin = irqmax = conf->irq;
		conf->irq = 0;		/* Make sure we get it. */
	}
	res.min = 0;
	res.max = 0;
	if (ioctl(sp->fd, PIOCSRESOURCE, &res) < 0)
		err(1, "ioctl (PIOCSRESOURCE)");
	if (res.resource_addr != ~0ul) {
		conf->irq = res.resource_addr;
		pool_irq[conf->irq] = 0;
	} else {
		for (i = irqmin; i <= irqmax; i++) {
			/*
			 * Skip irqs not in the pool.
			 */
			if (pool_irq[i] == 0)
				continue;
			/*
			 * -I forces us to use the interrupt, so use it.
			 */
			if (!use_kern_irq) {
				conf->irq = i;
				pool_irq[i] = 0;
				break;
			}
			/*
			 * Ask the kernel if we have an free irq.
			 */
			res.min = i;
			res.max = i;
			if (ioctl(sp->fd, PIOCSRESOURCE, &res) < 0)
				err(1, "ioctl (PIOCSRESOURCE)");
			if (res.resource_addr == ~0ul)
				continue;
			/*
			 * res.resource_addr might be the kernel's
			 * better idea than i, so we have to check to
			 * see if that's in use too.  If not, mark it
			 * in use and break out of the loop.  I'm not
			 * sure this can happen when IRQ 0 above fails,
			 * but the test is cheap enough.
			 */
			if (pool_irq[res.resource_addr] == 0)
				continue;
			conf->irq = res.resource_addr;
			pool_irq[conf->irq] = 0;
			break;
		}
	}
	if (conf->irq == 0) {
		logmsg("Failed to allocate IRQ for %s\n", cp->manuf);
		return (NULL);
	}
	drvp->card = cp;
	drvp->config = conf;
	drvp->inuse = 1;
	conf->inuse = 1;
	return (conf);
}

/*
 *	Auto select config index
 */
static struct cis_config *
assign_card_index(struct slot *sp, struct cis * cis)
{
	struct cis_config *cp;
	struct cis_ioblk *cio;
	struct pccard_resource res;
	int i;

	res.type = SYS_RES_IOPORT;
	for (cp = cis->conf; cp; cp = cp->next) {
		if (!cp->iospace || !cp->io)
			continue;
		for (cio = cp->io; cio; cio = cio->next) {
			res.size = cio->size;
			res.min = cio->addr;
			res.max = res.min + cio->size - 1;
			if (ioctl(sp->fd, PIOCSRESOURCE, &res) < 0)
				err(1, "ioctl (PIOCSRESOURCE)");
			if (res.resource_addr != cio->addr)
				goto next;
			for (i = cio->addr; i < cio->addr + cio->size - 1; i++)
				if (!bit_test(io_avail, i))
					goto next;
		}
		return cp;	/* found */
	next:
	}
	return cis->def_config;
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
	switch (sp->config->index_type) {
	case DEFAULT_INDEX:	/* default */
		cisconf = defconf;
		sp->config->index = cisconf->id;
		break;
	case AUTO_INDEX:	/* auto */
		cisconf = assign_card_index(sp, cis);
		sp->config->index = cisconf->id;
		break;
	default:		/* normal, use index value */
		for (cisconf = cis->conf; cisconf; cisconf = cisconf->next)
			if (cisconf->id == sp->config->index)
				break;
	}

	if (cisconf == 0) {
		logmsg("Config id %d not present in this card",
		    sp->config->index);
		return (-1);
	}
	sp->card_config = cisconf;

	/*
	 * Found a matching configuration. Now look at the I/O, memory and IRQ
	 * to create the desired parameters. Look at memory first.
	 */

	/* Skip ed cards in PIO mode */
	if ((strncmp(sp->config->driver->name, "ed", 2) == 0) &&
	    (sp->config->flags & 0x10))
		goto memskip;

	if (cisconf->memspace || (defconf && defconf->memspace)) {
		struct cis_memblk *mp;

		mp = cisconf->mem;
		/* 
		 * Currently we do not handle the presence of multiple windows.
		 * Then again neither does the interface to the kernel!
		 * See setup_slot() and readcis.c:cis_conf()
		 */
		if (cisconf->memwins > 1) {
			logmsg("Card requires %d memory windows.",
		            cisconf->memwins);
			return (-4);
		}

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
		/* Driver specific set up */
		if (strncmp(sp->config->driver->name, "ed", 2) == 0) {
			sp->mem.cardaddr = 0x4000;
			sp->mem.flags = MDF_ACTIVE | MDF_16BITS;
		} else {
			sp->mem.flags = MDF_ACTIVE | MDF_16BITS;
		}

		if (sp->mem.flags & MDF_ACTIVE)
			sp->flags |= MEM_ASSIGNED;
		if (debug_level > 0) {
			logmsg("Using mem addr 0x%x, size %d, card addr 0x%x, flags 0x%x\n",
			    sp->mem.addr, sp->mem.size, sp->mem.cardaddr,
			    sp->mem.flags);
		}
	}
memskip:

	/* Now look at I/O. */
	bzero(&sp->io, sizeof(sp->io));
	if (cisconf->iospace || (defconf && defconf->iospace) 
	    || sp->card->iosize) {
		struct cis_config *cp;
		struct cis_ioblk *cio;
		struct allocblk *sio;
		int     x, xmax;
		int iosize;

		cp = cisconf;
		if (!cisconf->iospace)
			cp = defconf;
		iosize = sp->card->iosize;

		/* iosize auto */
		if (iosize < 0) {
			if (cp->io)
				iosize = cp->io->size;
			else
				iosize = 1 << cp->io_addr;
		}

		/*
 		* If # of I/O lines decoded == 10, then card does its
 		* own decoding.
 		*
 		* If an I/O block exists, then use it.
 		* If no address (but a length) is available, allocate
 		* from the pool.
 		*/
		cio = cp->io;
		sio = &(sp->io);
		xmax = 1;
		if (iosize == 0 && cio)
			xmax = cisconf->io_blks;
		for (x = 0; x < xmax; x++) {
			if (iosize) {
				sio->addr = 0;
				sio->size = iosize;
			} else if (cio) {
				sio->addr = cio->addr;
				sio->size = cio->size;
			} else {
				/*
				 * No I/O block, assume the address lines
				 * decode gives the size.
				 */
				sio->size = 1 << cp->io_addr;
			}
			if (sio->addr == 0) {
				struct pccard_resource res;
				int i, j;

				res.type = SYS_RES_IOPORT;
				res.size = sio->size;

				for (i = 0; i < IOPORTS; i++) {
					j = bit_fns(io_avail, IOPORTS, i,
							sio->size, sio->size);
					if ((j & (sio->size - 1)) != 0)
						continue;
					res.min = j;
					res.max = j + sio->size - 1;
					if (ioctl(sp->fd, PIOCSRESOURCE, &res) < 0)
						err(1, "ioctl (PIOCSRESOURCE)");
					if (res.resource_addr == j)
						break;
				}
				if (j < 0) {
					return (-3);
				} else {
					sio->addr = j;
				}
			}
			bit_nclear(io_avail, sio->addr,
				   sio->addr + sio->size - 1);
			sp->flags |= IO_ASSIGNED;

			/* Set up the size to take into account the decode lines. */
			sio->cardaddr = cp->io_addr;
			switch (cp->io_bus) {
			case 0:
				break;
			case 1:
				sio->flags = IODF_WS;
				break;
			case 2:
				sio->flags = IODF_WS | IODF_CS16;
				break;
			case 3:
				sio->flags = IODF_WS | IODF_CS16 | IODF_16BIT;
				break;
			}
			if (debug_level > 0) {
				logmsg("Using I/O addr 0x%x, size %d\n",
				    sio->addr, sio->size);
			}
			if (cio && cio->next) {
				sio->next = xmalloc(sizeof(*sio));
				sio = sio->next;
				cio = cio->next;
			}
		}
	}
	sp->irq = sp->config->irq;
	sp->flags |= IRQ_ASSIGNED;
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
	struct allocblk *sio;
	char    *p;
	char    c;
	off_t   offs;
	int     rw_flags;
	int	iowin;

	memset(&io, 0, sizeof io);
	memset(&drv, 0, sizeof drv);
	offs = sp->cis->reg_addr;
	rw_flags = MDF_ATTR;
	ioctl(sp->fd, PIOCRWFLAG, &rw_flags);
#if RESET_MAY_BE_HARMFUL
	lseek(sp->fd, offs, SEEK_SET);
	c = 0x80;
	write(sp->fd, &c, sizeof(c));
	usleep(sp->card->reset_time * 1000);
	lseek(sp->fd, offs, SEEK_SET);
	c = 0x00;
	write(sp->fd, &c, sizeof(c));
	usleep(sp->card->reset_time * 1000);
#endif
	lseek(sp->fd, offs, SEEK_SET);
	c = sp->config->index;
	c |= 0x40;
	write(sp->fd, &c, sizeof(c));
	if (debug_level > 0) {
		logmsg("Setting config reg at offs 0x%lx to 0x%x, "
		    "Reset time = %d ms\n", (unsigned long)offs, c,
		    sp->card->reset_time);
	}
	usleep(pccard_init_sleep);
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
	if (sp->flags & MEM_ASSIGNED) {
		mem.window = 0;
		mem.flags = sp->mem.flags;
		mem.start = (caddr_t) sp->mem.addr;
		mem.card = sp->mem.cardaddr;
		mem.size = sp->mem.size;
		if (ioctl(sp->fd, PIOCSMEM, &mem)) {
			logerr("ioctl (PIOCSMEM)");
			return (0);
		}
	}
	if (sp->flags & IO_ASSIGNED) {
	    for (iowin = 0, sio = &(sp->io); iowin <= 1; iowin++) {
		io.window = iowin;
		if (sio->size) {
			io.flags = sio->flags;
			io.start = sio->addr;
			io.size = sio->size;
		}
#if 0
		io.start = sp->io.addr & ~((1 << sp->io.cardaddr) - 1);
		io.size = 1 << sp->io.cardaddr;
		if (io.start < 0x100) {
			io.start = 0x100;
			io.size = 0x300;
		}
#endif
		if (debug_level > 0) {
			logmsg("Assigning I/O window %d, start 0x%x, "
			    "size 0x%x flags 0x%x\n", io.window, io.start,
			    io.size, io.flags);
		}
		io.flags |= IODF_ACTIVE;
		if (ioctl(sp->fd, PIOCSIO, &io)) {
			logerr("ioctl (PIOCSIO)");
			return (0);
		}
		if (ioctl(sp->fd, PIOCGIO, &io))
		{
		    logerr("ioctl (PIOCGIO)");
		    return(0);
		}
		if (io.start != sio->addr){
		    logmsg("I/O base address changed from 0x%x to 0x%x\n",
			   sio->addr, io.start);
		    sio->addr = io.start;
		}
		if (sio->next)
			sio = sio->next;
		else
			break;
	    }
	}
	strcpy(drv.name, drvp->kernel);
	drv.unit = drvp->unit;
	drv.irqmask = 1 << sp->irq;
	drv.flags = sp->config->flags;
	if (sp->flags & MEM_ASSIGNED) {
		drv.mem = sp->mem.addr;
		drv.memsize = sp->mem.size;
	} else {
		drv.mem = 0;
		drv.memsize = 0;
	}
	if (sp->flags & IO_ASSIGNED)
		drv.iobase = sp->io.addr;
	else
		drv.iobase = 0;
#ifdef DEV_DESC_HAS_SIZE
	drv.iosize = sp->io.size;
#endif
	if (debug_level > 0) {
		logmsg("Assign %s%d, io 0x%x-0x%x, mem 0x%lx, %d bytes, "
		    "irq %d, flags %x\n", drv.name, drv.unit, drv.iobase, 
		    drv.iobase + sp->io.size - 1, drv.mem, drv.memsize, 
		    sp->irq, drv.flags);
	}
	/*
	 * If the driver fails to be connected to the device,
	 * then it may mean that the driver did not recognise it.
	 */
	memcpy(drv.misc, sp->eaddr, 6);
	if (ioctl(sp->fd, PIOCSDRV, &drv)) {
		logmsg("driver allocation failed for %s(%s): %s",
		    sp->card->manuf, sp->card->version, strerror(errno));
		return (0);
	}
	drv.name[sizeof(drv.name) - 1] = '\0';
	if (strncmp(drv.name, drvp->kernel, sizeof(drv.name))) {
		drvp->kernel = newstr(drv.name);
		p = drvp->kernel;
		while (*p++) 
			if (*p >= '0' && *p <= '9') {
				drvp->unit = atoi(p);
				*p = '\0';
				break;
			}
	}
	logmsg("%s%d: %s inserted.", sp->config->driver->kernel,
	    sp->config->driver->unit, sp->card->logstr);
	return (1);
}
