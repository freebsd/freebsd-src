/*
 * Linux ARCnet driver - COM20020 PCMCIA support
 * 
 * Written 1994-1999 by Avery Pennarun,
 *    based on an ISA version by David Woodhouse.
 * Derived from ibmtr_cs.c by Steve Kipisz (pcmcia-cs 3.1.4)
 *    which was derived from pcnet_cs.c by David Hinds.
 * Some additional portions derived from skeleton.c by Donald Becker.
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
 * Changes:
 * Arnaldo Carvalho de Melo <acme@conectiva.com.br> - 08/08/2000
 * - reorganize kmallocs in com20020_attach, checking all for failure
 *   and releasing the previous allocations if one fails
 * **********************
 * 
 * For more details, see drivers/net/arcnet.c
 *
 * **********************
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <asm/io.h>
#include <asm/system.h>

#include <linux/netdevice.h>
#include <linux/arcdevice.h>
#include <linux/com20020.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>

#define VERSION "arcnet: COM20020 PCMCIA support loaded.\n"

#ifdef PCMCIA_DEBUG

static int pc_debug = PCMCIA_DEBUG;
MODULE_PARM(pc_debug, "i");
#define DEBUG(n, args...) if (pc_debug>(n)) printk(KERN_DEBUG args)

static void regdump(struct net_device *dev)
{
    int ioaddr = dev->base_addr;
    int count;
    
    printk("com20020 register dump:\n");
    for (count = ioaddr; count < ioaddr + 16; count++)
    {
	if (!(count % 16))
	    printk("\n%04X: ", count);
	printk("%02X ", inb(count));
    }
    printk("\n");
    
    printk("buffer0 dump:\n");
	/* set up the address register */
        count = 0;
	outb((count >> 8) | RDDATAflag | AUTOINCflag, _ADDR_HI);
	outb(count & 0xff, _ADDR_LO);
    
    for (count = 0; count < 256+32; count++)
    {
	if (!(count % 16))
	    printk("\n%04X: ", count);
	
	/* copy the data */
	printk("%02X ", inb(_MEMDATA));
    }
    printk("\n");
}

#else

#define DEBUG(n, args...) do { } while (0)
static inline void regdump(struct net_device *dev) { }

#endif


/*====================================================================*/

/* Parameters that can be set with 'insmod' */

static int node;
static int timeout = 3;
static int backplane;
static int clockp;
static int clockm;

MODULE_PARM(node, "i");
MODULE_PARM(timeout, "i");
MODULE_PARM(backplane, "i");
MODULE_PARM(clockp, "i");
MODULE_PARM(clockm, "i");

/* Bit map of interrupts to choose from */
static u_int irq_mask = 0xdeb8;
static int irq_list[4] = { -1 };

MODULE_PARM(irq_mask, "i");
MODULE_PARM(irq_list, "1-4i");
MODULE_LICENSE("GPL");

/*====================================================================*/

static void com20020_config(dev_link_t *link);
static void com20020_release(u_long arg);
static int com20020_event(event_t event, int priority,
                       event_callback_args_t *args);

static dev_info_t dev_info = "com20020_cs";

static dev_link_t *com20020_attach(void);
static void com20020_detach(dev_link_t *);

static dev_link_t *dev_list;

/*====================================================================*/

typedef struct com20020_dev_t {
    struct net_device       *dev;
    int dev_configured;
    dev_node_t          node;
} com20020_dev_t;

/*======================================================================

    This bit of code is used to avoid unregistering network devices
    at inappropriate times.  2.2 and later kernels are fairly picky
    about when this can happen.
    
======================================================================*/

static void flush_stale_links(void)
{
    dev_link_t *link, *next;
    for (link = dev_list; link; link = next) {
	next = link->next;
	if (link->state & DEV_STALE_LINK)
	    com20020_detach(link);
    }
}

/*====================================================================*/

static void cs_error(client_handle_t handle, int func, int ret)
{
    error_info_t err = { func, ret };
    CardServices(ReportError, handle, &err);
}

/*======================================================================

    com20020_attach() creates an "instance" of the driver, allocating
    local data structures for one device.  The device is registered
    with Card Services.

======================================================================*/

static void com20020cs_open_close(struct net_device *dev, bool open)
{
    if (open)
	MOD_INC_USE_COUNT;
    else
	MOD_DEC_USE_COUNT;
}

static dev_link_t *com20020_attach(void)
{
    client_reg_t client_reg;
    dev_link_t *link;
    com20020_dev_t *info;
    struct net_device *dev;
    int i, ret;
    struct arcnet_local *lp;
    
    DEBUG(0, "com20020_attach()\n");
    flush_stale_links();

    /* Create new network device */
    link = kmalloc(sizeof(struct dev_link_t), GFP_KERNEL);
    if (!link)
	return NULL;

    info = kmalloc(sizeof(struct com20020_dev_t), GFP_KERNEL);
    if (!info)
	goto fail_alloc_info;

    lp =  kmalloc(sizeof(struct arcnet_local), GFP_KERNEL);
    if (!lp)
	goto fail_alloc_lp;

    dev = dev_alloc("arc%d", &ret);
    if (!dev)
	goto fail_alloc_dev;

    memset(info, 0, sizeof(struct com20020_dev_t));
    memset(lp, 0, sizeof(struct arcnet_local));
    memset(link, 0, sizeof(struct dev_link_t));
    dev->priv = lp;

    link->release.function = &com20020_release;
    link->release.data = (u_long)link;
    link->io.Attributes1 = IO_DATA_PATH_WIDTH_8;
    link->io.NumPorts1 = 16;
    link->io.IOAddrLines = 16;
    link->irq.Attributes = IRQ_TYPE_EXCLUSIVE;
    link->irq.IRQInfo1 = IRQ_INFO2_VALID|IRQ_LEVEL_ID;
    if (irq_list[0] == -1)
	link->irq.IRQInfo2 = irq_mask;
    else
	for (i = 0; i < 4; i++)
	    link->irq.IRQInfo2 |= 1 << irq_list[i];
    link->conf.Attributes = CONF_ENABLE_IRQ;
    link->conf.Vcc = 50;
    link->conf.IntType = INT_MEMORY_AND_IO;
    link->conf.Present = PRESENT_OPTION;

    /* fill in our module parameters as defaults */
    dev->dev_addr[0] = node;
    lp->timeout = timeout;
    lp->backplane = backplane;
    lp->clockp = clockp;
    lp->clockm = clockm & 3;
    lp->hw.open_close_ll = com20020cs_open_close;

    link->irq.Instance = info->dev = dev;
    link->priv = info;

    /* Register with Card Services */
    link->next = dev_list;
    dev_list = link;
    client_reg.dev_info = &dev_info;
    client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
    client_reg.EventMask =
        CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
        CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
        CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
    client_reg.event_handler = &com20020_event;
    client_reg.Version = 0x0210;
    client_reg.event_callback_args.client_data = link;
    ret = CardServices(RegisterClient, &link->handle, &client_reg);
    if (ret != 0) {
        cs_error(link->handle, RegisterClient, ret);
        com20020_detach(link);
        return NULL;
    }

    return link;

fail_alloc_dev:
    kfree(lp);
fail_alloc_lp:
    kfree(info);
fail_alloc_info:
    kfree(link);
    return NULL;
} /* com20020_attach */

/*======================================================================

    This deletes a driver "instance".  The device is de-registered
    with Card Services.  If it has been released, all local data
    structures are freed.  Otherwise, the structures will be freed
    when the device is released.

======================================================================*/

static void com20020_detach(dev_link_t *link)
{
    struct com20020_dev_t *info = link->priv;
    dev_link_t **linkp;
    struct net_device *dev; 
    
    DEBUG(1,"detach...\n");

    DEBUG(0, "com20020_detach(0x%p)\n", link);

    /* Locate device structure */
    for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
        if (*linkp == link) break;
    if (*linkp == NULL)
        return;

    dev = info->dev;

    if (link->state & DEV_CONFIG) {
        com20020_release((u_long)link);
        if (link->state & DEV_STALE_CONFIG) {
            link->state |= DEV_STALE_LINK;
            return;
        }
    }

    if (link->handle)
        CardServices(DeregisterClient, link->handle);

    /* Unlink device structure, free bits */
    DEBUG(1,"unlinking...\n");
    *linkp = link->next;
    if (link->priv)
    {
	dev = info->dev;
	if (dev)
	{
	    if (info->dev_configured)
	    {
		DEBUG(1,"unregister...\n");

		if (netif_running(dev))
		    dev->stop(dev);
	    
		/*
		 * this is necessary because we register our IRQ separately
		 * from card services.
		 */
		if (dev->irq)
		    free_irq(dev->irq, dev);
		
		/* ...but I/O ports are done automatically by card services */
		
		unregister_netdev(dev);
		MOD_DEC_USE_COUNT;
	    }
	    
	    DEBUG(1,"kfree...\n");
	    kfree(dev->priv);
	    kfree(dev);
	}
	DEBUG(1,"kfree2...\n");
	kfree(info);
    }
    DEBUG(1,"kfree3...\n");
    kfree(link);

} /* com20020_detach */

/*======================================================================

    com20020_config() is scheduled to run after a CARD_INSERTION event
    is received, to configure the PCMCIA socket, and to make the
    device available to the system.

======================================================================*/

#define CS_CHECK(fn, args...) \
while ((last_ret=CardServices(last_fn=(fn), args))!=0) goto cs_failed

static void com20020_config(dev_link_t *link)
{
    struct arcnet_local *lp;
    client_handle_t handle;
    tuple_t tuple;
    cisparse_t parse;
    com20020_dev_t *info;
    struct net_device *dev;
    int i, last_ret, last_fn;
    u_char buf[64];
    int ioaddr;

    handle = link->handle;
    info = link->priv;
    dev = info->dev;

    DEBUG(1,"config...\n");

    DEBUG(0, "com20020_config(0x%p)\n", link);

    tuple.Attributes = 0;
    tuple.TupleData = buf;
    tuple.TupleDataMax = 64;
    tuple.TupleOffset = 0;
    tuple.DesiredTuple = CISTPL_CONFIG;
    CS_CHECK(GetFirstTuple, handle, &tuple);
    CS_CHECK(GetTupleData, handle, &tuple);
    CS_CHECK(ParseTuple, handle, &tuple, &parse);
    link->conf.ConfigBase = parse.config.base;

    /* Configure card */
    link->state |= DEV_CONFIG;
    strcpy(info->node.dev_name, dev->name);

    DEBUG(1,"arcnet: baseport1 is %Xh\n", link->io.BasePort1);
    i = !CS_SUCCESS;
    if (!link->io.BasePort1)
    {
	for (ioaddr = 0x100; ioaddr < 0x400; ioaddr += 0x10)
	{
	    link->io.BasePort1 = ioaddr;
	    i = CardServices(RequestIO, link->handle, &link->io);
	    if (i == CS_SUCCESS)
		break;
	}
    }
    else
	i = CardServices(RequestIO, link->handle, &link->io);
    
    if (i != CS_SUCCESS)
    {
	DEBUG(1,"arcnet: requestIO failed totally!\n");
	goto failed;
    }
	
    ioaddr = dev->base_addr = link->io.BasePort1;
    DEBUG(1,"arcnet: got ioaddr %Xh\n", ioaddr);

    DEBUG(1,"arcnet: request IRQ %d (%Xh/%Xh)\n",
	   link->irq.AssignedIRQ,
	   link->irq.IRQInfo1, link->irq.IRQInfo2);
    i = CardServices(RequestIRQ, link->handle, &link->irq);
    if (i != CS_SUCCESS)
    {
	DEBUG(1,"arcnet: requestIRQ failed totally!\n");
	goto failed;
    }

    dev->irq = link->irq.AssignedIRQ;

    CS_CHECK(RequestConfiguration, link->handle, &link->conf);

    if (com20020_check(dev))
    {
	regdump(dev);
	goto failed;
    }
    
    MOD_INC_USE_COUNT;

    lp = dev->priv;
    lp->card_name = "PCMCIA COM20020";
    lp->card_flags = ARC_CAN_10MBIT; /* pretend all of them can 10Mbit */

    i = com20020_found(dev, 0);
    
    if (i != 0) {
	DEBUG(1,KERN_NOTICE "com20020_cs: com20020_found() failed\n");
	goto failed;
    }

    info->dev_configured = 1;
    link->dev = &info->node;
    link->state &= ~DEV_CONFIG_PENDING;

    DEBUG(1,KERN_INFO "%s: port %#3lx, irq %d\n",
           dev->name, dev->base_addr, dev->irq);
    return;

cs_failed:
    cs_error(link->handle, last_fn, last_ret);
failed:
    DEBUG(1,"com20020_config failed...\n");
    com20020_release((u_long)link);
} /* com20020_config */

/*======================================================================

    After a card is removed, com20020_release() will unregister the net
    device, and release the PCMCIA configuration.  If the device is
    still open, this will be postponed until it is closed.

======================================================================*/

static void com20020_release(u_long arg)
{
    dev_link_t *link = (dev_link_t *)arg;

    DEBUG(1,"release...\n");

    DEBUG(0, "com20020_release(0x%p)\n", link);

    if (link->open) {
	DEBUG(1,"postpone...\n");
	DEBUG(1, "com20020_cs: release postponed, device stll open\n");
        link->state |= DEV_STALE_CONFIG;
        return;
    }

    CardServices(ReleaseConfiguration, link->handle);
    CardServices(ReleaseIO, link->handle, &link->io);
    CardServices(ReleaseIRQ, link->handle, &link->irq);

    link->state &= ~(DEV_CONFIG | DEV_RELEASE_PENDING);

} /* com20020_release */

/*======================================================================

    The card status event handler.  Mostly, this schedules other
    stuff to run after an event is received.  A CARD_REMOVAL event
    also sets some flags to discourage the net drivers from trying
    to talk to the card any more.

======================================================================*/

static int com20020_event(event_t event, int priority,
			  event_callback_args_t *args)
{
    dev_link_t *link = args->client_data;
    com20020_dev_t *info = link->priv;
    struct net_device *dev = info->dev;

    DEBUG(1, "com20020_event(0x%06x)\n", event);
    
    switch (event) {
    case CS_EVENT_CARD_REMOVAL:
        link->state &= ~DEV_PRESENT;
        if (link->state & DEV_CONFIG) {
            netif_device_detach(dev);
            link->release.expires = jiffies + HZ/20;
            link->state |= DEV_RELEASE_PENDING;
            add_timer(&link->release);
        }
        break;
    case CS_EVENT_CARD_INSERTION:
        link->state |= DEV_PRESENT;
	com20020_config(link); 
	break;
    case CS_EVENT_PM_SUSPEND:
        link->state |= DEV_SUSPEND;
        /* Fall through... */
    case CS_EVENT_RESET_PHYSICAL:
        if (link->state & DEV_CONFIG) {
            if (link->open) {
                netif_device_detach(dev);
            }
            CardServices(ReleaseConfiguration, link->handle);
        }
        break;
    case CS_EVENT_PM_RESUME:
        link->state &= ~DEV_SUSPEND;
        /* Fall through... */
    case CS_EVENT_CARD_RESET:
        if (link->state & DEV_CONFIG) {
            CardServices(RequestConfiguration, link->handle, &link->conf);
            if (link->open) {
		int ioaddr = dev->base_addr;
		struct arcnet_local *lp = (struct arcnet_local *)dev->priv;
		ARCRESET;
            }
        }
        break;
    }
    return 0;
} /* com20020_event */


/*====================================================================*/

static int __init init_com20020_cs(void)
{
    servinfo_t serv;

    DEBUG(0, "%s\n", VERSION);
    CardServices(GetCardServicesInfo, &serv);
    if (serv.Revision != CS_RELEASE_CODE) {
	printk(KERN_NOTICE "com20020_cs: Card Services release "
	       "does not match!\n");
        return -1;
    }
    register_pccard_driver(&dev_info, &com20020_attach, &com20020_detach);
    return 0;
}

static void __exit exit_com20020_cs(void)
{
    DEBUG(0, "com20020_cs: unloading\n");
    unregister_pccard_driver(&dev_info);
    while (dev_list != NULL)
        com20020_detach(dev_list);
}

module_init(init_com20020_cs);
module_exit(exit_com20020_cs);
