/* $Id: ptifddi.c,v 1.14 2001/04/14 01:12:04 davem Exp $
 * ptifddi.c: Network driver for Performance Technologies single-attach
 *            and dual-attach FDDI sbus cards.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

static char *version =
        "ptifddi.c:v1.0 10/Dec/96 David S. Miller (davem@caipfs.rutgers.edu)\n";

#include <linux/string.h>
#include <linux/init.h>

#include "ptifddi.h"

#include "ptifddi_asm.h"

#ifdef MODULE
static struct ptifddi *root_pti_dev;
#endif

static inline void pti_reset(struct ptifddi *pp)
{
	pp->reset = 1;
}

static inline void pti_unreset(struct ptifddi *pp)
{
	pp->unreset = 1;
}

static inline void pti_load_code_base(struct dfddi_ram *rp, unsigned short addr)
{
	rp->loader_addr = ((addr << 8) & 0xff00) | ((addr >> 8) & 0x00ff);
}

static inline void pti_clear_dpram(struct ptifddi *pp)
{
	memset(pp->dpram, 0, DPRAM_SIZE);
}

#define CARD_TEST_TIMEOUT	100000

static inline int pti_card_test(struct ptifddi *pp)
{
	struct dfddi_ram *rp	= pp->dpram;
	unsigned char *code	= &rp->loader;
	unsigned char *status	= (unsigned char *) rp;
	int clicks		= CARD_TEST_TIMEOUT;

	/* Clear it out. */
	pti_clear_dpram(pp);

	/* Load test data. */
	for(i = 0; i < test_firmware_size; i++)
		code[i] = test_firmware[i];

	/* Tell card where to execute the code. */
	pti_load_code_base(pp, test_firmware_dev_addr);

	/* Clear test run status in dpram. */
	*status = 0;

	/* Reset single attach state machine before the test. */
	rp->reset = 1;

	/* Unreset, to get the test code running. */
	pti_unreset(pp);

	/* Wait for dpram status to become 5, else fail if we time out. */
	while(--clicks) {
		if(*status == 5) {
			pti_reset(pp);
			return 0;
		}
		udelay(20);
	}
	return 1;
}

static inline void pti_init_firmware_loader(struct ptifddi *pp)
{
	struct dfddi_ram *rp = pp->dpram;
	int i;

	for(i = 0; i < firmware_loader_size; i++)
		rp->loader.loader_firmware[i] = firmware_loader[i];
}

static inline void pti_load_main_firmware(struct ptifddi *pp)
{
	struct dfddi_ram *rp		= pp->dpram;
	struct dpram_loader *lp		= &rp.loader;
	int i;


}

static void pti_init_rings(struct ptifddi *pp, int from_irq)
{
}

static int pti_init(struct ptifddi *pp, int from_irq)
{
}

static void pti_is_not_so_happy(struct ptifddi *pp)
{
}

static inline void pti_tx(struct ptifddi *pp, struct net_device *dev)
{
}

static inline void myri_rx(struct ptifddi *pp, struct net_device *dev)
{
}

static void pti_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev		= (struct net_device *) dev_id;
	struct ptifddi *pp		= (struct ptifddi *) dev->priv;

}

static int pti_open(struct net_device *dev)
{
	struct ptifddi *pp = (struct ptifddi *) dev->priv;

	return pti_init(pp, in_interrupt());
}

static int pti_close(struct net_device *dev)
{
	struct ptifddi *pp = (struct ptifddi *) dev->priv;

	return 0;
}

static int pti_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ptifddi *pp = (struct ptifddi *) dev->priv;
}

static struct net_device_stats *pti_get_stats(struct net_device *dev)
{ return &(((struct ptifddi *)dev->priv)->enet_stats); }

static void pti_set_multicast(struct net_device *dev)
{
}

static inline int pti_fddi_init(struct net_device *dev, struct sbus_dev *sdev, int num)
{
	static unsigned version_printed;
	struct ptifddi *pp;
	int i;

	dev = init_fddidev(0, sizeof(struct ptifddi));

	if(version_printed++ == 0)
		printk(version);

	/* Register 0 mapping contains DPRAM. */
	pp->dpram = (struct dfddi_ram *) sbus_ioremap(
	    &sdep->resource[0], 0, sizeof(sturct dfddi_ram), "PTI FDDI DPRAM");
	if(!pp->dpram) {
		printk("ptiFDDI: Cannot map DPRAM I/O area.\n");
		return -ENODEV;
	}

	/* Next, register 1 contains reset byte. */
	pp->reset = (unsigned char *) sbus_ioremap(
	    &sdep->resource[1], 0, 1, "PTI FDDI RESET Byte");
	if(!pp->reset) {
		printk("ptiFDDI: Cannot map RESET byte.\n");
		return -ENODEV;
	}

	/* Register 2 contains unreset byte. */
	pp->unreset = (unsigned char *) sbus_ioremap(
	    &sdep->resource[2], 0, 1, "PTI FDDI UNRESET Byte");
	if(!pp->unreset) {
		printk("ptiFDDI: Cannot map UNRESET byte.\n");
		return -ENODEV;
	}

	/* Reset the card. */
	pti_reset(pp);

	/* Run boot-up card tests. */
	i = pti_card_test(pp);
	if(i) {
		printk("ptiFDDI: Bootup card test fails.\n");
		return -ENODEV;
	}

	/* Clear DPRAM, start afresh. */
	pti_clear_dpram(pp);

	/* Init the firmware loader. */
	pti_init_firmware_loader(pp);

	/* Now load main card FDDI firmware, using the loader. */
	pti_load_main_firmware(pp);
}

int __init ptifddi_sbus_probe(struct net_device *dev)
{
	struct sbus_bus *bus;
	struct sbus_dev *sdev = 0;
	static int called;
	int cards = 0, v;

	if(called)
		return -ENODEV;
	called++;

	for_each_sbus(bus) {
		for_each_sbusdev(sdev, bus) {
			if(cards) dev = NULL;
			if(!strcmp(sdev->prom_name, "PTI,sbs600") ||
			   !strcmp(sdev->prom_name, "DPV,fddid")) {
				cards++;
				DET(("Found PTI FDDI as %s\n", sdev->prom_name));
				if((v = pti_fddi_init(dev, sdev, (cards - 1))))
					return v;
			}
		}
	}
	if(!cards)
		return -ENODEV;
	return 0;
}


#ifdef MODULE

int
init_module(void)
{
	root_pti_dev = NULL;
	return ptifddi_sbus_probe(NULL);
}

void
cleanup_module(void)
{
	struct ptifddi *pp;

	/* No need to check MOD_IN_USE, as sys_delete_module() checks. */
	while (root_pti_dev) {
		pp = root_pti_dev->next_module;

		unregister_netdev(root_pti_dev->dev);
		kfree(root_pti_dev->dev);
		root_pti_dev = mp;
	}
}

#endif /* MODULE */
