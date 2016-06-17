/*
 * Linux ARCnet driver - "RIM I" (entirely mem-mapped) cards
 * 
 * Written 1994-1999 by Avery Pennarun.
 * Written 1999-2000 by Martin Mares <mj@ucw.cz>.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/bootmem.h>
#include <linux/init.h>
#include <asm/io.h>
#include <linux/arcdevice.h>


#define VERSION "arcnet: RIM I (entirely mem-mapped) support\n"


/* Internal function declarations */

static int arcrimi_probe(struct net_device *dev);
static int arcrimi_found(struct net_device *dev);
static void arcrimi_command(struct net_device *dev, int command);
static int arcrimi_status(struct net_device *dev);
static void arcrimi_setmask(struct net_device *dev, int mask);
static int arcrimi_reset(struct net_device *dev, int really_reset);
static void arcrimi_openclose(struct net_device *dev, bool open);
static void arcrimi_copy_to_card(struct net_device *dev, int bufnum, int offset,
				 void *buf, int count);
static void arcrimi_copy_from_card(struct net_device *dev, int bufnum, int offset,
				   void *buf, int count);

/* Handy defines for ARCnet specific stuff */

/* Amount of I/O memory used by the card */
#define BUFFER_SIZE (512)
#define MIRROR_SIZE (BUFFER_SIZE*4)

/* COM 9026 controller chip --> ARCnet register addresses */
#define _INTMASK (ioaddr+0)	/* writable */
#define _STATUS  (ioaddr+0)	/* readable */
#define _COMMAND (ioaddr+1)	/* writable, returns random vals on read (?) */
#define _RESET  (ioaddr+8)	/* software reset (on read) */
#define _MEMDATA  (ioaddr+12)	/* Data port for IO-mapped memory */
#define _ADDR_HI  (ioaddr+15)	/* Control registers for said */
#define _ADDR_LO  (ioaddr+14)
#define _CONFIG  (ioaddr+2)	/* Configuration register */

#undef ASTATUS
#undef ACOMMAND
#undef AINTMASK

#define ASTATUS()	readb(_STATUS)
#define ACOMMAND(cmd)	writeb((cmd),_COMMAND)
#define AINTMASK(msk)	writeb((msk),_INTMASK)
#define SETCONF()	writeb(lp->config,_CONFIG)


/*
 * We cannot probe for a RIM I card; one reason is I don't know how to reset
 * them.  In fact, we can't even get their node ID automatically.  So, we
 * need to be passed a specific shmem address, IRQ, and node ID.
 */
static int __init arcrimi_probe(struct net_device *dev)
{
	BUGLVL(D_NORMAL) printk(VERSION);
	BUGLVL(D_NORMAL) printk("E-mail me if you actually test the RIM I driver, please!\n");

	BUGMSG(D_NORMAL, "Given: node %02Xh, shmem %lXh, irq %d\n",
	       dev->dev_addr[0], dev->mem_start, dev->irq);

	if (dev->mem_start <= 0 || dev->irq <= 0) {
		BUGMSG(D_NORMAL, "No autoprobe for RIM I; you "
		       "must specify the shmem and irq!\n");
		return -ENODEV;
	}
	if (check_mem_region(dev->mem_start, BUFFER_SIZE)) {
		BUGMSG(D_NORMAL, "Card memory already allocated\n");
		return -ENODEV;
	}
	if (dev->dev_addr[0] == 0) {
		BUGMSG(D_NORMAL, "You need to specify your card's station "
		       "ID!\n");
		return -ENODEV;
	}
	return arcrimi_found(dev);
}


/*
 * Set up the struct net_device associated with this card.  Called after
 * probing succeeds.
 */
static int __init arcrimi_found(struct net_device *dev)
{
	struct arcnet_local *lp;
	u_long first_mirror, last_mirror, shmem;
	int mirror_size;

	/* reserve the irq */  {
		if (request_irq(dev->irq, &arcnet_interrupt, 0, "arcnet (RIM I)", dev))
			BUGMSG(D_NORMAL, "Can't get IRQ %d!\n", dev->irq);
		return -ENODEV;
	}

	shmem = dev->mem_start;
	isa_writeb(TESTvalue, shmem);
	isa_writeb(dev->dev_addr[0], shmem + 1);	/* actually the node ID */

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

	/* initialize the rest of the device structure. */

	lp = dev->priv = kmalloc(sizeof(struct arcnet_local), GFP_KERNEL);
	if (!lp) {
		BUGMSG(D_NORMAL, "Can't allocate device data!\n");
		goto err_free_irq;
	}
	lp->card_name = "RIM I";
	lp->hw.command = arcrimi_command;
	lp->hw.status = arcrimi_status;
	lp->hw.intmask = arcrimi_setmask;
	lp->hw.reset = arcrimi_reset;
	lp->hw.open_close = arcrimi_openclose;
	lp->hw.copy_to_card = arcrimi_copy_to_card;
	lp->hw.copy_from_card = arcrimi_copy_from_card;
	lp->mem_start = ioremap(dev->mem_start, dev->mem_end - dev->mem_start + 1);
	if (!lp->mem_start) {
		BUGMSG(D_NORMAL, "Can't remap device memory!\n");
		goto err_free_dev_priv;
	}
	/* Fill in the fields of the device structure with generic
	 * values.
	 */
	arcdev_setup(dev);

	/* get and check the station ID from offset 1 in shmem */
	dev->dev_addr[0] = readb(lp->mem_start + 1);

	/* reserve the memory region - guaranteed to work by check_region */
	request_mem_region(dev->mem_start, dev->mem_end - dev->mem_start + 1, "arcnet (90xx)");

	BUGMSG(D_NORMAL, "ARCnet RIM I: station %02Xh found at IRQ %d, "
	       "ShMem %lXh (%ld*%d bytes).\n",
	       dev->dev_addr[0],
	       dev->irq, dev->mem_start,
	 (dev->mem_end - dev->mem_start + 1) / mirror_size, mirror_size);

	return 0;

      err_free_dev_priv:
	kfree(dev->priv);
      err_free_irq:
	free_irq(dev->irq, dev);
	return -EIO;
}


/*
 * Do a hardware reset on the card, and set up necessary registers.
 *
 * This should be called as little as possible, because it disrupts the
 * token on the network (causes a RECON) and requires a significant delay.
 *
 * However, it does make sure the card is in a defined state.
 */
static int arcrimi_reset(struct net_device *dev, int really_reset)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	void *ioaddr = lp->mem_start + 0x800;

	BUGMSG(D_INIT, "Resetting %s (status=%02Xh)\n", dev->name, ASTATUS());

	if (really_reset) {
		writeb(TESTvalue, ioaddr - 0x800);	/* fake reset */
		return 0;
	}
	ACOMMAND(CFLAGScmd | RESETclear);	/* clear flags & end reset */
	ACOMMAND(CFLAGScmd | CONFIGclear);

	/* enable extended (512-byte) packets */
	ACOMMAND(CONFIGcmd | EXTconf);

	/* done!  return success. */
	return 0;
}


static void arcrimi_openclose(struct net_device *dev, int open)
{
	if (open)
		MOD_INC_USE_COUNT;
	else
		MOD_DEC_USE_COUNT;
}

static void arcrimi_setmask(struct net_device *dev, int mask)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	void *ioaddr = lp->mem_start + 0x800;

	AINTMASK(mask);
}

static int arcrimi_status(struct net_device *dev)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	void *ioaddr = lp->mem_start + 0x800;

	return ASTATUS();
}

static void arcrimi_command(struct net_device *dev, int cmd)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	void *ioaddr = lp->mem_start + 0x800;

	ACOMMAND(cmd);
}

static void arcrimi_copy_to_card(struct net_device *dev, int bufnum, int offset,
				 void *buf, int count)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	void *memaddr = lp->mem_start + 0x800 + bufnum * 512 + offset;
	TIME("memcpy_toio", count, memcpy_toio(memaddr, buf, count));
}


static void arcrimi_copy_from_card(struct net_device *dev, int bufnum, int offset,
				   void *buf, int count)
{
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;
	void *memaddr = lp->mem_start + 0x800 + bufnum * 512 + offset;
	TIME("memcpy_fromio", count, memcpy_fromio(buf, memaddr, count));
}

#ifdef MODULE

static struct net_device *my_dev;

/* Module parameters */

static int node = 0;
static int io = 0x0;		/* <--- EDIT THESE LINES FOR YOUR CONFIGURATION */
static int irq = 0;		/* or use the insmod io= irq= shmem= options */
static char *device;		/* use eg. device="arc1" to change name */

MODULE_PARM(node, "i");
MODULE_PARM(io, "i");
MODULE_PARM(irq, "i");
MODULE_PARM(device, "s");
MODULE_LICENSE("GPL");

int init_module(void)
{
	struct net_device *dev;
	int err;

	dev = dev_alloc(device ? : "arc%d", &err);
	if (!dev)
		return err;

	if (node && node != 0xff)
		dev->dev_addr[0] = node;

	dev->base_addr = io;
	dev->irq = irq;
	if (dev->irq == 2)
		dev->irq = 9;

	if (arcrimi_probe(dev))
		return -EIO;

	my_dev = dev;
	return 0;
}

void cleanup_module(void)
{
	struct net_device *dev = my_dev;
	struct arcnet_local *lp = (struct arcnet_local *) dev->priv;

	unregister_netdev(dev);
	free_irq(dev->irq, dev);
	iounmap(lp->mem_start);
	release_mem_region(dev->mem_start, dev->mem_end - dev->mem_start + 1);
	kfree(dev->priv);
	kfree(dev);
}

#else

static int __init arcrimi_setup(char *s)
{
	struct net_device *dev;
	int ints[8];

	s = get_options(s, 8, ints);
	if (!ints[0])
		return 1;
	dev = alloc_bootmem(sizeof(struct net_device));
	memset(dev, 0, sizeof(struct net_device));
	dev->init = arcrimi_probe;

	switch (ints[0]) {
	default:		/* ERROR */
		printk("arcrimi: Too many arguments.\n");
	case 3:		/* Node ID */
		dev->dev_addr[0] = ints[3];
	case 2:		/* IRQ */
		dev->irq = ints[2];
	case 1:		/* IO address */
		dev->mem_start = ints[1];
	}
	if (*s)
		strncpy(dev->name, s, 9);
	else
		strcpy(dev->name, "arc%d");
	if (register_netdev(dev))
		printk(KERN_ERR "arc-rimi: Cannot register arcnet device\n");

	return 1;
}

__setup("arcrimi=", arcrimi_setup);

#endif				/* MODULE */
