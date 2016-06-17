/* airport.c 0.13d
 *
 * A driver for "Hermes" chipset based Apple Airport wireless
 * card.
 *
 * Copyright notice & release notes in file orinoco.c
 * 
 * Note specific to airport stub:
 * 
 *  0.05 : first version of the new split driver
 *  0.06 : fix possible hang on powerup, add sleep support
 */

#include <linux/config.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <linux/adb.h>
#include <linux/pmu.h>

#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/pmac_feature.h>
#include <asm/irq.h>

#include "orinoco.h"

#define AIRPORT_IO_LEN	(0x1000)	/* one page */

struct airport {
	struct device_node *node;
	void *vaddr;
	int irq_requested;
	int ndev_registered;
};

#ifdef CONFIG_PMAC_PBOOK
static int airport_sleep_notify(struct pmu_sleep_notifier *self, int when);
static struct pmu_sleep_notifier airport_sleep_notifier = {
	airport_sleep_notify, SLEEP_LEVEL_NET,
};
#endif

/*
 * Function prototypes
 */

static struct net_device *airport_attach(struct device_node *of_node);
static void airport_detach(struct net_device *dev);

static struct net_device *airport_dev;

#ifdef CONFIG_PMAC_PBOOK
static int
airport_sleep_notify(struct pmu_sleep_notifier *self, int when)
{
	struct net_device *dev = airport_dev;
	struct orinoco_private *priv = dev->priv;
	struct airport *card = priv->card;
	unsigned long flags;
	int err;
	
	if (! airport_dev)
		return PBOOK_SLEEP_OK;

	switch (when) {
	case PBOOK_SLEEP_NOW:
		printk(KERN_DEBUG "%s: Airport entering sleep mode\n", dev->name);

		err = orinoco_lock(priv, &flags);
		if (err) {
			printk(KERN_ERR "%s: hw_unavailable on PBOOK_SLEEP_NOW\n",
			       dev->name);
			break;
		}

		err = __orinoco_down(dev);
		if (err)
			printk(KERN_WARNING "%s: PBOOK_SLEEP_NOW: Error %d downing interface\n",
			       dev->name, err);

		netif_device_detach(dev);

		priv->hw_unavailable++;

		orinoco_unlock(priv, &flags);

		disable_irq(dev->irq);
		pmac_call_feature(PMAC_FTR_AIRPORT_ENABLE, card->node, 0, 0);
		break;

	case PBOOK_WAKE:
		printk(KERN_DEBUG "%s: Airport waking up\n", dev->name);
		pmac_call_feature(PMAC_FTR_AIRPORT_ENABLE, card->node, 0, 1);
		mdelay(200);

		enable_irq(dev->irq);

		err = orinoco_reinit_firmware(dev);
		if (err) {
			printk(KERN_ERR "%s: Error %d re-initializing firmware on PBOOK_WAKE\n",
			       dev->name, err);
			break;
		}

		spin_lock_irqsave(&priv->lock, flags);

		netif_device_attach(dev);

		priv->hw_unavailable--;

		if (priv->open && (! priv->hw_unavailable)) {
			err = __orinoco_up(dev);
			if (err)
				printk(KERN_ERR "%s: Error %d restarting card on PBOOK_WAKE\n",
				       dev->name, err);
		}


		spin_unlock_irqrestore(&priv->lock, flags);

		break;
	}
	return PBOOK_SLEEP_OK;
}
#endif /* CONFIG_PMAC_PBOOK */

static int airport_hard_reset(struct orinoco_private *priv)
{
	/* It would be nice to power cycle the Airport for a real hard
	 * reset, but for some reason although it appears to
	 * re-initialize properly, it falls in a screaming heap
	 * shortly afterwards. */
#if 0
	struct net_device *dev = priv->ndev;
	struct airport *card = priv->card;

	/* Vitally important.  If we don't do this it seems we get an
	 * interrupt somewhere during the power cycle, since
	 * hw_unavailable is already set it doesn't get ACKed, we get
	 * into an interrupt loop and the the PMU decides to turn us
	 * off. */
	disable_irq(dev->irq);

	pmac_call_feature(PMAC_FTR_AIRPORT_ENABLE, card->node, 0, 0);
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(HZ);
	pmac_call_feature(PMAC_FTR_AIRPORT_ENABLE, card->node, 0, 1);
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(HZ);

	enable_irq(dev->irq);
	schedule_timeout(HZ);
#endif

	return 0;
}

static struct net_device *
airport_attach(struct device_node *of_node)
{
	struct orinoco_private *priv;
	struct net_device *dev;
	struct airport *card;
	unsigned long phys_addr;
	hermes_t *hw;

	if (of_node->n_addrs < 1 || of_node->n_intrs < 1) {
		printk(KERN_ERR "airport: wrong interrupt/addresses in OF tree\n");
		return NULL;
	}

	/* Allocate space for private device-specific data */
	dev = alloc_orinocodev(sizeof(*card), airport_hard_reset);
	if (! dev) {
		printk(KERN_ERR "airport: can't allocate device datas\n");
		return NULL;
	}
	priv = dev->priv;
	card = priv->card;

	hw = &priv->hw;
	card->node = of_node;

	if (! request_OF_resource(of_node, 0, " (airport)")) {
		printk(KERN_ERR "airport: can't request IO resource !\n");
		kfree(dev);
		return NULL;
	}

	dev->name[0] = '\0';	/* register_netdev will give us an ethX name */
	SET_MODULE_OWNER(dev);

	/* Setup interrupts & base address */
	dev->irq = of_node->intrs[0].line;
	phys_addr = of_node->addrs[0].address;  /* Physical address */
	printk(KERN_DEBUG "Airport at physical address %lx\n", phys_addr);
	dev->base_addr = phys_addr;
	card->vaddr = ioremap(phys_addr, AIRPORT_IO_LEN);
	if (! card->vaddr) {
		printk("airport: ioremap() failed\n");
		goto failed;
	}

	hermes_struct_init(hw, (ulong)card->vaddr,
			HERMES_MEM, HERMES_16BIT_REGSPACING);
		
	/* Power up card */
	pmac_call_feature(PMAC_FTR_AIRPORT_ENABLE, card->node, 0, 1);
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(HZ);

	/* Reset it before we get the interrupt */
	hermes_init(hw);

	if (request_irq(dev->irq, orinoco_interrupt, 0, "Airport", dev)) {
		printk(KERN_ERR "airport: Couldn't get IRQ %d\n", dev->irq);
		goto failed;
	}
	card->irq_requested = 1;

	/* Tell the stack we exist */
	if (register_netdev(dev) != 0) {
		printk(KERN_ERR "airport: register_netdev() failed\n");
		goto failed;
	}
	printk(KERN_DEBUG "airport: card registered for interface %s\n", dev->name);
	card->ndev_registered = 1;

#ifdef CONFIG_PMAC_PBOOK
	pmu_register_sleep_notifier(&airport_sleep_notifier);
#endif
	return dev;
	
 failed:
	airport_detach(dev);
	return NULL;
}				/* airport_attach */

/*======================================================================
  This deletes a driver "instance".  
  ======================================================================*/

static void
airport_detach(struct net_device *dev)
{
	struct orinoco_private *priv = dev->priv;
	struct airport *card = priv->card;

#ifdef CONFIG_PMAC_PBOOK
	pmu_unregister_sleep_notifier(&airport_sleep_notifier);
#endif
	if (card->ndev_registered)
		unregister_netdev(dev);
	card->ndev_registered = 0;

	if (card->irq_requested)
		free_irq(dev->irq, dev);
	card->irq_requested = 0;

	if (card->vaddr)
		iounmap(card->vaddr);
	card->vaddr = 0;

	dev->base_addr = 0;

	release_OF_resource(card->node, 0);

	pmac_call_feature(PMAC_FTR_AIRPORT_ENABLE, card->node, 0, 0);
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout(HZ);

	kfree(dev);
}				/* airport_detach */

static char version[] __initdata = "airport.c 0.13d (Benjamin Herrenschmidt <benh@kernel.crashing.org>)";
MODULE_AUTHOR("Benjamin Herrenschmidt <benh@kernel.crashing.org>");
MODULE_DESCRIPTION("Driver for the Apple Airport wireless card.");
MODULE_LICENSE("Dual MPL/GPL");

static int __init
init_airport(void)
{
	struct device_node *airport_node;

	printk(KERN_DEBUG "%s\n", version);

	/* Lookup card in device tree */
	airport_node = find_devices("radio");
	if (airport_node && !strcmp(airport_node->parent->name, "mac-io"))
		airport_dev = airport_attach(airport_node);

	return airport_dev ? 0 : -ENODEV;
}

static void __exit
exit_airport(void)
{
	if (airport_dev)
		airport_detach(airport_dev);
	airport_dev = NULL;
}

module_init(init_airport);
module_exit(exit_airport);
