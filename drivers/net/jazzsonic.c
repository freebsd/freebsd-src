/*
 * sonic.c
 *
 * (C) 1996,1998 by Thomas Bogendoerfer (tsbogend@alpha.franken.de)
 * 
 * This driver is based on work from Andreas Busse, but most of
 * the code is rewritten.
 * 
 * (C) 1995 by Andreas Busse (andy@waldorf-gmbh.de)
 *
 * A driver for the onboard Sonic ethernet controller on Mips Jazz
 * systems (Acer Pica-61, Mips Magnum 4000, Olivetti M700 and
 * perhaps others, too)
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <asm/bootinfo.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/pgtable.h>
#include <asm/segment.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/jazz.h>
#include <asm/jazzdma.h>
#include <linux/errno.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#define SREGS_PAD(n)    u16 n;

#include "sonic.h"

/*
 * Macros to access SONIC registers
 */
#define SONIC_READ(reg) (*((volatile unsigned int *)base_addr+reg))

#define SONIC_WRITE(reg,val)						\
do {									\
	*((volatile unsigned int *)base_addr+reg) = val;		\
}


/* use 0 for production, 1 for verification, >2 for debug */
#ifdef SONIC_DEBUG
static unsigned int sonic_debug = SONIC_DEBUG;
#else 
static unsigned int sonic_debug = 1;
#endif

/*
 * Base address and interrupt of the SONIC controller on JAZZ boards
 */
static struct {
	unsigned int port;
	unsigned int irq;
} sonic_portlist[] = { {JAZZ_ETHERNET_BASE, JAZZ_ETHERNET_IRQ}, {0, 0}};

/*
 * We cannot use station (ethernet) address prefixes to detect the
 * sonic controller since these are board manufacturer depended.
 * So we check for known Silicon Revision IDs instead. 
 */
static unsigned short known_revisions[] =
{
	0x04,			/* Mips Magnum 4000 */
	0xffff			/* end of list */
};

/* Index to functions, as function prototypes. */

extern int sonic_probe(struct net_device *dev);
static int sonic_probe1(struct net_device *dev, unsigned int base_addr,
                        unsigned int irq);


/*
 * Probe for a SONIC ethernet controller on a Mips Jazz board.
 * Actually probing is superfluous but we're paranoid.
 */
int __init sonic_probe(struct net_device *dev)
{
	unsigned int base_addr = dev ? dev->base_addr : 0;
	int i;

	/*
	 * Don't probe if we're not running on a Jazz board.
	 */
	if (mips_machgroup != MACH_GROUP_JAZZ)
		return -ENODEV;
	if (base_addr >= KSEG0)	/* Check a single specified location. */
		return sonic_probe1(dev, base_addr, dev->irq);
	else if (base_addr != 0)	/* Don't probe at all. */
		return -ENXIO;

	for (i = 0; sonic_portlist[i].port; i++) {
		int base_addr = sonic_portlist[i].port;
		if (check_region(base_addr, 0x100))
			continue;
		if (sonic_probe1(dev, base_addr, sonic_portlist[i].irq) == 0)
			return 0;
	}
	return -ENODEV;
}

static int __init sonic_probe1(struct net_device *dev, unsigned int base_addr,
                               unsigned int irq)
{
	static unsigned version_printed;
	unsigned int silicon_revision;
	unsigned int val;
	struct sonic_local *lp;
	int i;

	/*
	 * get the Silicon Revision ID. If this is one of the known
	 * one assume that we found a SONIC ethernet controller at
	 * the expected location.
	 */
	silicon_revision = SONIC_READ(SONIC_SR);
	if (sonic_debug > 1)
		printk("SONIC Silicon Revision = 0x%04x\n",silicon_revision);

	i = 0;
	while (known_revisions[i] != 0xffff
	       && known_revisions[i] != silicon_revision)
		i++;

	if (known_revisions[i] == 0xffff) {
		printk("SONIC ethernet controller not found (0x%4x)\n",
		       silicon_revision);
		return -ENODEV;
	}
    
	if (!request_region(base_addr, 0x100, dev->name))
		return -EBUSY;

	if (sonic_debug  &&  version_printed++ == 0)
		printk(version);

	printk("%s: Sonic ethernet found at 0x%08lx, ", dev->name, base_addr);

	/* Fill in the 'dev' fields. */
	dev->base_addr = base_addr;
	dev->irq = irq;

	/*
	 * Put the sonic into software reset, then
	 * retrieve and print the ethernet address.
	 */
	SONIC_WRITE(SONIC_CMD,SONIC_CR_RST);
	SONIC_WRITE(SONIC_CEP,0);
	for (i=0; i<3; i++) {
		val = SONIC_READ(SONIC_CAP0-i);
		dev->dev_addr[i*2] = val;
		dev->dev_addr[i*2+1] = val >> 8;
	}

	printk("HW Address ");
	for (i = 0; i < 6; i++) {
		printk("%2.2x", dev->dev_addr[i]);
		if (i<5)
			printk(":");
	}

	printk(" IRQ %d\n", irq);
    
	/* Initialize the device structure. */
	if (dev->priv == NULL) {
		/*
		 * the memory be located in the same 64kb segment
		 */
		lp = NULL;
		i = 0;
		do {
			lp = kmalloc(sizeof(*lp), GFP_KERNEL);
			if ((unsigned long) lp >> 16
			    != ((unsigned long)lp + sizeof(*lp) ) >> 16) {
				/* FIXME, free the memory later */
				kfree(lp);
				lp = NULL;
			}
		} while (lp == NULL && i++ < 20);

		if (lp == NULL) {
			printk("%s: couldn't allocate memory for descriptors\n",
			       dev->name);
			return -ENOMEM;
		}

		memset(lp, 0, sizeof(struct sonic_local));

		/* get the virtual dma address */
		lp->cda_laddr = vdma_alloc(PHYSADDR(lp),sizeof(*lp));
		if (lp->cda_laddr == ~0UL) {
			printk("%s: couldn't get DMA page entry for "
			       "descriptors\n", dev->name);
			return -ENOMEM;
		}

		lp->tda_laddr = lp->cda_laddr + sizeof (lp->cda);
		lp->rra_laddr = lp->tda_laddr + sizeof (lp->tda);
		lp->rda_laddr = lp->rra_laddr + sizeof (lp->rra);
	
		/* allocate receive buffer area */
		/* FIXME, maybe we should use skbs */
		lp->rba = kmalloc(SONIC_NUM_RRS * SONIC_RBSIZE, GFP_KERNEL);
		if (!lp->rba) {
			printk("%s: couldn't allocate receive buffers\n",
			       dev->name);
			return -ENOMEM;
		}

		/* get virtual dma address */
		lp->rba_laddr = vdma_alloc(PHYSADDR(lp->rba),
		                           SONIC_NUM_RRS * SONIC_RBSIZE);
		if (lp->rba_laddr == ~0UL) {
			printk("%s: couldn't get DMA page entry for receive "
			       "buffers\n",dev->name);
			return -ENOMEM;
		}

		/* now convert pointer to KSEG1 pointer */
		lp->rba = (char *)KSEG1ADDR(lp->rba);
		flush_cache_all();
		dev->priv = (struct sonic_local *)KSEG1ADDR(lp);
	}

	lp = (struct sonic_local *)dev->priv;
	dev->open = sonic_open;
	dev->stop = sonic_close;
	dev->hard_start_xmit = sonic_send_packet;
	dev->get_stats	= sonic_get_stats;
	dev->set_multicast_list = &sonic_multicast_list;
	dev->watchdog_timeo = TX_TIMEOUT;

	/*
	 * clear tally counter
	 */
	SONIC_WRITE(SONIC_CRCT,0xffff);
	SONIC_WRITE(SONIC_FAET,0xffff);
	SONIC_WRITE(SONIC_MPT,0xffff);

	/* Fill in the fields of the device structure with ethernet values. */
	ether_setup(dev);
	return 0;
}

/*
 *      SONIC uses a normal IRQ
 */
#define sonic_request_irq       request_irq
#define sonic_free_irq          free_irq

#define sonic_chiptomem(x)      KSEG1ADDR(vdma_log2phys(x))

#include "sonic.c"
