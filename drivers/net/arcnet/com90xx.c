/*
 * Linux ARCnet driver - COM90xx chipset (memory-mapped buffers)
 * 
 * Written 1994-1999 by Avery Pennarun.
 * Written 1999 by Martin Mares <mj@ucw.cz>.
 * Derived from skeleton.c by Donald Becker.
 *
 * Special thanks to Contemporary Controls, Inc. (www.ccontrols.com)
 *  for sponsoring the further development of this driver.
 *
 * **********************
 *
 * The original copyright of skeleton.c was as follows:
 *
 * skeleton.c Written 1993 by Donald Becker.
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.  This software may only be used
 * and distributed according to the terms of the GNU General Public License as
 * modified by SRC, incorporated herein by reference.
 *
 * **********************
 *
 * For more details, see drivers/net/arcnet.c
 *
 * **********************
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/bootmem.h>
#include <asm/io.h>
#include <linux/arcdevice.h>


#define VERSION "arcnet: COM90xx chipset support\n"


/* Define this to speed up the autoprobe by assuming if only one io port and
 * shmem are left in the list at Stage 5, they must correspond to each
 * other.
 *
 * This is undefined by default because it might not always be true, and the
 * extra check makes the autoprobe even more careful.  Speed demons can turn
 * it on - I think it should be fine if you only have one ARCnet card
 * installed.
 *
 * If no ARCnet cards are installed, this delay never happens anyway and thus
 * the option has no effect.
 */
#undef FAST_PROBE


/* Internal function declarations */
static int com90xx_found(struct net_device *dev, int ioaddr, int airq,
			 u_long shmem);
static void com90xx_command(struct net_device *dev, int command);
static int com90xx_status(struct net_device *dev);
static void com90xx_setmask(struct net_device *dev, int mask);
static int com90xx_reset(struct net_device *dev, int really_reset);
static void com90xx_openclose(struct net_device *dev, bool open);
static void com90xx_copy_to_card(struct net_device *dev, int bufnum, int offset,
				 void *buf, int count);
static void com90xx_copy_from_card(struct net_device *dev, int bufnum, int offset,
				   void *buf, int count);

/* Known ARCnet cards */

static struct net_device *cards[16];
static int numcards;

/* Handy defines for ARCnet specific stuff */

/* The number of low I/O ports used by the card */
#define ARCNET_TOTAL_SIZE	16

/* Amount of I/O memory used by the card */
#define BUFFER_SIZE (512)
#define MIRROR_SIZE (BUFFER_SIZE*4)

/* COM 9026 controller chip --> ARCnet register addresses */
#define _INTMASK (ioaddr+0)	/* writable */
#define _STATUS  (ioaddr+0)	/* readable */
#define _COMMAND (ioaddr+1)	/* writable, returns random vals on read (?) */
#define _CONFIG  (ioaddr+2)	/* Configuration register */
#define _RESET   (ioaddr+8)	/* software reset (on read) */
#define _MEMDATA (ioaddr+12)	/* Data port for IO-mapped memory */
#define _ADDR_HI (ioaddr+15)	/* Control registers for said */
#define _ADDR_LO (ioaddr+14)

#undef ASTATUS
#undef ACOMMAND
#undef AINTMASK

#define ASTATUS()	inb(_STATUS)
#define ACOMMAND(cmd) 	outb((cmd),_COMMAND)
#define AINTMASK(msk)	outb((msk),_INTMASK)


static int com90xx_skip_probe __initdata = 0;

int __init com90xx_probe(struct net_device *dev)
{
	int count, status, ioaddr, numprint, airq, retval = -ENODEV,
	 openparen = 0;
	unsigned long airqmask;
	int ports[(0x3f0 - 0x200) / 16 + 1] =
	{0};
	u_long shmems[(0xFF800 - 0xA0000) / 2048 + 1] =
	{0};
	int numports, numshmems, *port;
	u_long *shmem;

	if (!dev && com90xx_skip_probe)
		return -ENODEV;

#ifndef MODULE
	arcnet_init();
#endif

	BUGLVL(D_NORMAL) printk(VERSION);

	/* set up the arrays where we'll store the possible probe addresses */
	numports = numshmems = 0;
	if (dev && dev->base_addr)
		ports[numports++] = dev->base_addr;
	else
		for (count = 0x200; count <= 0x3f0; count += 16)
			ports[numports++] = count;
	if (dev && dev->mem_start)
		shmems[numshmems++] = dev->mem_start;
	else
		for (count = 0xA0000; count <= 0xFF800; count += 2048)
			shmems[numshmems++] = count;

	/* Stage 1: abandon any reserved ports, or ones with status==0xFF
	 * (empty), and reset any others by reading the reset port.
	 */
	numprint = -1;
	for (port = &ports[0]; port - ports < numports; port++) {
		numprint++;
		numprint %= 8;
		if (!numprint) {
			BUGMSG2(D_INIT, "\n");
			BUGMSG2(D_INIT, "S1: ");
		}
		BUGMSG2(D_INIT, "%Xh ", *port);

		ioaddr = *port;

		if (check_region(*port, ARCNET_TOTAL_SIZE)) {
			BUGMSG2(D_INIT_REASONS, "(check_region)\n");
			BUGMSG2(D_INIT_REASONS, "S1: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			*port = ports[numports - 1];
			numports--;
			port--;
			continue;
		}
		if (ASTATUS() == 0xFF) {
			BUGMSG2(D_INIT_REASONS, "(empty)\n");
			BUGMSG2(D_INIT_REASONS, "S1: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			*port = ports[numports - 1];
			numports--;
			port--;
			continue;
		}
		inb(_RESET);	/* begin resetting card */

		BUGMSG2(D_INIT_REASONS, "\n");
		BUGMSG2(D_INIT_REASONS, "S1: ");
		BUGLVL(D_INIT_REASONS) numprint = 0;
	}
	BUGMSG2(D_INIT, "\n");

	if (!numports) {
		BUGMSG2(D_NORMAL, "S1: No ARCnet cards found.\n");
		return -ENODEV;
	}
	/* Stage 2: we have now reset any possible ARCnet cards, so we can't
	 * do anything until they finish.  If D_INIT, print the list of
	 * cards that are left.
	 */
	numprint = -1;
	for (port = &ports[0]; port - ports < numports; port++) {
		numprint++;
		numprint %= 8;
		if (!numprint) {
			BUGMSG2(D_INIT, "\n");
			BUGMSG2(D_INIT, "S2: ");
		}
		BUGMSG2(D_INIT, "%Xh ", *port);
	}
	BUGMSG2(D_INIT, "\n");
	mdelay(RESETtime);

	/* Stage 3: abandon any shmem addresses that don't have the signature
	 * 0xD1 byte in the right place, or are read-only.
	 */
	numprint = -1;
	for (shmem = &shmems[0]; shmem - shmems < numshmems; shmem++) {
		u_long ptr = *shmem;

		numprint++;
		numprint %= 8;
		if (!numprint) {
			BUGMSG2(D_INIT, "\n");
			BUGMSG2(D_INIT, "S3: ");
		}
		BUGMSG2(D_INIT, "%lXh ", *shmem);

		if (check_mem_region(*shmem, BUFFER_SIZE)) {
			BUGMSG2(D_INIT_REASONS, "(check_mem_region)\n");
			BUGMSG2(D_INIT_REASONS, "Stage 3: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			*shmem = shmems[numshmems - 1];
			numshmems--;
			shmem--;
			continue;
		}
		if (isa_readb(ptr) != TESTvalue) {
			BUGMSG2(D_INIT_REASONS, "(%02Xh != %02Xh)\n",
				isa_readb(ptr), TESTvalue);
			BUGMSG2(D_INIT_REASONS, "S3: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			*shmem = shmems[numshmems - 1];
			numshmems--;
			shmem--;
			continue;
		}
		/* By writing 0x42 to the TESTvalue location, we also make
		 * sure no "mirror" shmem areas show up - if they occur
		 * in another pass through this loop, they will be discarded
		 * because *cptr != TESTvalue.
		 */
		isa_writeb(0x42, ptr);
		if (isa_readb(ptr) != 0x42) {
			BUGMSG2(D_INIT_REASONS, "(read only)\n");
			BUGMSG2(D_INIT_REASONS, "S3: ");
			*shmem = shmems[numshmems - 1];
			numshmems--;
			shmem--;
			continue;
		}
		BUGMSG2(D_INIT_REASONS, "\n");
		BUGMSG2(D_INIT_REASONS, "S3: ");
		BUGLVL(D_INIT_REASONS) numprint = 0;
	}
	BUGMSG2(D_INIT, "\n");

	if (!numshmems) {
		BUGMSG2(D_NORMAL, "S3: No ARCnet cards found.\n");
		return -ENODEV;
	}
	/* Stage 4: something of a dummy, to report the shmems that are
	 * still possible after stage 3.
	 */
	numprint = -1;
	for (shmem = &shmems[0]; shmem - shmems < numshmems; shmem++) {
		numprint++;
		numprint %= 8;
		if (!numprint) {
			BUGMSG2(D_INIT, "\n");
			BUGMSG2(D_INIT, "S4: ");
		}
		BUGMSG2(D_INIT, "%lXh ", *shmem);
	}
	BUGMSG2(D_INIT, "\n");

	/* Stage 5: for any ports that have the correct status, can disable
	 * the RESET flag, and (if no irq is given) generate an autoirq,
	 * register an ARCnet device.
	 *
	 * Currently, we can only register one device per probe, so quit
	 * after the first one is found.
	 */
	numprint = -1;
	for (port = &ports[0]; port - ports < numports; port++) {
		numprint++;
		numprint %= 8;
		if (!numprint) {
			BUGMSG2(D_INIT, "\n");
			BUGMSG2(D_INIT, "S5: ");
		}
		BUGMSG2(D_INIT, "%Xh ", *port);

		ioaddr = *port;
		status = ASTATUS();

		if ((status & 0x9D)
		    != (NORXflag | RECONflag | TXFREEflag | RESETflag)) {
			BUGMSG2(D_INIT_REASONS, "(status=%Xh)\n", status);
			BUGMSG2(D_INIT_REASONS, "S5: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			*port = ports[numports - 1];
			numports--;
			port--;
			continue;
		}
		ACOMMAND(CFLAGScmd | RESETclear | CONFIGclear);
		status = ASTATUS();
		if (status & RESETflag) {
			BUGMSG2(D_INIT_REASONS, " (eternal reset, status=%Xh)\n",
				status);
			BUGMSG2(D_INIT_REASONS, "S5: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			*port = ports[numports - 1];
			numports--;
			port--;
			continue;
		}
		/* skip this completely if an IRQ was given, because maybe
		 * we're on a machine that locks during autoirq!
		 */
		if (!dev || !dev->irq) {
			/* if we do this, we're sure to get an IRQ since the
			 * card has just reset and the NORXflag is on until
			 * we tell it to start receiving.
			 */
			airqmask = probe_irq_on();
			AINTMASK(NORXflag);
			udelay(1);
			AINTMASK(0);
			airq = probe_irq_off(airqmask);

			if (airq <= 0) {
				BUGMSG2(D_INIT_REASONS, "(airq=%d)\n", airq);
				BUGMSG2(D_INIT_REASONS, "S5: ");
				BUGLVL(D_INIT_REASONS) numprint = 0;
				*port = ports[numports - 1];
				numports--;
				port--;
				continue;
			}
		} else {
			airq = dev->irq;
		}

		BUGMSG2(D_INIT, "(%d,", airq);
		openparen = 1;

		/* Everything seems okay.  But which shmem, if any, puts
		 * back its signature byte when the card is reset?
		 *
		 * If there are multiple cards installed, there might be
		 * multiple shmems still in the list.
		 */
#ifdef FAST_PROBE
		if (numports > 1 || numshmems > 1) {
			inb(_RESET);
			mdelay(RESETtime);
		} else {
			/* just one shmem and port, assume they match */
			isa_writeb(TESTvalue, shmems[0]);
		}
#else
		inb(_RESET);
		mdelay(RESETtime);
#endif

		for (shmem = &shmems[0]; shmem - shmems < numshmems; shmem++) {
			u_long ptr = *shmem;

			if (isa_readb(ptr) == TESTvalue) {	/* found one */
				BUGMSG2(D_INIT, "%lXh)\n", *shmem);
				openparen = 0;

				/* register the card */
				retval = com90xx_found(dev, *port, airq, *shmem);
				numprint = -1;

				/* remove shmem from the list */
				*shmem = shmems[numshmems - 1];
				numshmems--;

				break;	/* go to the next I/O port */
			} else {
				BUGMSG2(D_INIT_REASONS, "%Xh-", isa_readb(ptr));
			}
		}

		if (openparen) {
			BUGLVL(D_INIT) printk("no matching shmem)\n");
			BUGLVL(D_INIT_REASONS) printk("S5: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
		}
		*port = ports[numports - 1];
		numports--;
		port--;
	}

	BUGLVL(D_INIT_REASONS) printk("\n");

	/* Now put back TESTvalue on all leftover shmems. */
	for (shmem = &shmems[0]; shmem - shmems < numshmems; shmem++)
		isa_writeb(TESTvalue, *shmem);

	if (retval && dev && !numcards)
		BUGMSG2(D_NORMAL, "S5: No ARCnet cards found.\n");
	return retval;
}


/* Set up the struct net_device associated with this card.  Called after
 * probing succeeds.
 */
static int __init com90xx_found(struct net_device *dev0, int ioaddr, int airq,
				u_long shmem)
{
	struct net_device *dev = dev0;
	struct arcnet_local *lp;
	u_long first_mirror, last_mirror;
	int mirror_size, err;

	/* allocate struct net_device if we don't have one yet */
	if (!dev && !(dev = dev_alloc("arc%d", &err))) {
		BUGMSG2(D_NORMAL, "com90xx: Can't allocate device!\n");
		return err;
	}
	lp = dev->priv = kmalloc(sizeof(struct arcnet_local), GFP_KERNEL);
	if (!lp) {
		BUGMSG(D_NORMAL, "Can't allocate device data!\n");
		goto err_free_dev;
	}
	/* find the real shared memory start/end points, including mirrors */

	/* guess the actual size of one "memory mirror" - the number of
	 * bytes between copies of the shared memory.  On most cards, it's
	 * 2k (or there are no mirrors at all) but on some, it's 4k.
	 */
	mirror_size = MIRROR_SIZE;
	if (isa_readb(shmem) == TESTvalue
	    && isa_readb(shmem - mirror_size) != TESTvalue
	    && isa_readb(shmem - 2 * mirror_size) == TESTvalue)
		mirror_size *= 2;

	first_mirror = last_mirror = shmem;
	while (isa_readb(first_mirror) == TESTvalue)
		first_mirror -= mirror_size;
	first_mirror += mirror_size;

	while (isa_readb(last_mirror) == TESTvalue)
		last_mirror += mirror_size;
	last_mirror -= mirror_size;

	dev->mem_start = first_mirror;
	dev->mem_end = last_mirror + MIRROR_SIZE - 1;
	dev->rmem_start = dev->mem_start + BUFFER_SIZE * 0;
	dev->rmem_end = dev->mem_start + BUFFER_SIZE * 2 - 1;

	/* Initialize the rest of the device structure. */
	memset(lp, 0, sizeof(struct arcnet_local));
	lp->card_name = "COM90xx";
	lp->hw.command = com90xx_command;
	lp->hw.status = com90xx_status;
	lp->hw.intmask = com90xx_setmask;
	lp->hw.reset = com90xx_reset;
	lp->hw.open_close = com90xx_openclose;
	lp->hw.copy_to_card = com90xx_copy_to_card;
	lp->hw.copy_from_card = com90xx_copy_from_card;
	lp->mem_start = ioremap(dev->mem_start, dev->mem_end - dev->mem_start + 1);
	if (!lp->mem_start) {
		BUGMSG(D_NORMAL, "Can't remap device memory!\n");
		goto err_free_dev_priv;
	}
	/* Fill in the fields of the device structure with generic values. */
	arcdev_setup(dev);

	/* get and check the station ID from offset 1 in shmem */
	dev->dev_addr[0] = readb(lp->mem_start + 1);

	/* reserve the irq */
	if (request_irq(airq, &arcnet_interrupt, 0, "arcnet (90xx)", dev)) {
		BUGMSG(D_NORMAL, "Can't get IRQ %d!\n", airq);
		goto err_unmap;
	}
	dev->irq = airq;

	/* reserve the I/O and memory regions - guaranteed to work by check_region */
	request_region(ioaddr, ARCNET_TOTAL_SIZE, "arcnet (90xx)");
	request_mem_region(dev->mem_start, dev->mem_end - dev->mem_start + 1, "arcnet (90xx)");
	dev->base_addr = ioaddr;

	BUGMSG(D_NORMAL, "COM90xx station %02Xh found at %03lXh, IRQ %d, "
	       "ShMem %lXh (%ld*%xh).\n",
	       dev->dev_addr[0],
	       dev->base_addr, dev->irq, dev->mem_start,
	 (dev->mem_end - dev->mem_start + 1) / mirror_size, mirror_size);

	if (!dev0 && register_netdev(dev))
		goto err_release;

	cards[numcards++] = dev;
	return 0;

      err_release:
	free_irq(dev->irq, dev);
	release_region(dev->base_addr, ARCNET_TOTAL_SIZE);
	release_mem_region(dev->mem_start, dev->mem_end - dev->mem_start + 1);
      err_unmap:
	iounmap(lp->mem_start);
      err_free_dev_priv:
	kfree(dev->priv);
      err_free_dev:
	if (!dev0)
		kfree(dev);
	return -EIO;
}


static void com90xx_command(struct net_device *dev, int cmd)
{
	short ioaddr = dev->base_addr;

	ACOMMAND(cmd);
}


static int com90xx_status(struct net_device *dev)
{
	short ioaddr = dev->base_addr;

	return ASTATUS();
}


static void com90xx_setmask(struct net_device *dev, int mask)
{
	short ioaddr = dev->base_addr;

	AINTMASK(mask);
}


/*
 * Do a hardware reset on the card, and set up necessary registers.
 * 
 * This should be called as little as possible, because it disrupts the
 * token on the network (causes a RECON) and requires a significant delay.
 *
 * However, it does make sure the card is in a defined state.
 */
int com90xx_reset(struct net_device *dev, int really_reset)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	short ioaddr = dev->base_addr;

	BUGMSG(D_INIT, "Resetting (status=%02Xh)\n", ASTATUS());

	if (really_reset) {
		/* reset the card */
		inb(_RESET);
		mdelay(RESETtime);
	}
	ACOMMAND(CFLAGScmd | RESETclear);	/* clear flags & end reset */
	ACOMMAND(CFLAGScmd | CONFIGclear);

	/* don't do this until we verify that it doesn't hurt older cards! */
	/* outb(inb(_CONFIG) | ENABLE16flag, _CONFIG); */

	/* verify that the ARCnet signature byte is present */
	if (readb(lp->mem_start) != TESTvalue) {
		if (really_reset)
			BUGMSG(D_NORMAL, "reset failed: TESTvalue not present.\n");
		return 1;
	}
	/* enable extended (512-byte) packets */
	ACOMMAND(CONFIGcmd | EXTconf);

	/* clean out all the memory to make debugging make more sense :) */
	BUGLVL(D_DURING)
	    memset_io(lp->mem_start, 0x42, 2048);

	/* done!  return success. */
	return 0;
}


static void com90xx_openclose(struct net_device *dev, bool open)
{
	if (open)
		MOD_INC_USE_COUNT;
	else
		MOD_DEC_USE_COUNT;
}


static void com90xx_copy_to_card(struct net_device *dev, int bufnum, int offset,
				 void *buf, int count)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	void *memaddr = lp->mem_start + bufnum * 512 + offset;
	TIME("memcpy_toio", count, memcpy_toio(memaddr, buf, count));
}


static void com90xx_copy_from_card(struct net_device *dev, int bufnum, int offset,
				   void *buf, int count)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	void *memaddr = lp->mem_start + bufnum * 512 + offset;
	TIME("memcpy_fromio", count, memcpy_fromio(buf, memaddr, count));
}



#ifdef MODULE

/* Module parameters */

static int io;			/* use the insmod io= irq= shmem= options */
static int irq;
static int shmem;
static char *device;		/* use eg. device=arc1 to change name */

MODULE_PARM(io, "i");
MODULE_PARM(irq, "i");
MODULE_PARM(shmem, "i");
MODULE_PARM(device, "s");
MODULE_LICENSE("GPL");

int init_module(void)
{
	struct net_device *dev;
	int err;

	if (io || irq || shmem || device) {
		dev = dev_alloc(device ? : "arc%d", &err);
		if (!dev)
			return err;
		dev->base_addr = io;
		dev->irq = irq;
		if (dev->irq == 2)
			dev->irq = 9;
		dev->mem_start = shmem;
		com90xx_probe(dev);
	} else
		com90xx_probe(NULL);

	if (!numcards)
		return -EIO;
	return 0;
}


void cleanup_module(void)
{
	struct net_device *dev;
	struct arcnet_local *lp;
	int count;

	for (count = 0; count < numcards; count++) {
		dev = cards[count];
		lp = (struct arcnet_local *) dev->priv;

		unregister_netdev(dev);
		free_irq(dev->irq, dev);
		iounmap(lp->mem_start);
		release_region(dev->base_addr, ARCNET_TOTAL_SIZE);
		release_mem_region(dev->mem_start, dev->mem_end - dev->mem_start + 1);
		kfree(dev->priv);
		kfree(dev);
	}
}

#else

static int __init com90xx_setup(char *s)
{
	struct net_device *dev;
	int ints[8];

	com90xx_skip_probe = 1;

	s = get_options(s, 8, ints);
	if (!ints[0] && !*s) {
		printk("com90xx: Disabled.\n");
		return 1;
	}
	dev = alloc_bootmem(sizeof(struct net_device));
	memset(dev, 0, sizeof(struct net_device));
	dev->init = com90xx_probe;

	switch (ints[0]) {
	default:		/* ERROR */
		printk("com90xx: Too many arguments.\n");
	case 3:		/* Mem address */
		dev->mem_start = ints[3];
	case 2:		/* IRQ */
		dev->irq = ints[2];
	case 1:		/* IO address */
		dev->base_addr = ints[1];
	}
	if (*s)
		strncpy(dev->name, s, 9);
	else
		strcpy(dev->name, "arc%d");
	if (register_netdev(dev))
		printk(KERN_ERR "com90xx: Cannot register arcnet device\n");

	return 1;
}

__setup("com90xx=", com90xx_setup);

#endif				/* MODULE */
