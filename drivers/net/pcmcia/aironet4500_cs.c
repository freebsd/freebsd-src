/*
 *	 Aironet 4500 Pcmcia driver
 *
 *		Elmer Joandi, Januar 1999
 *	Copyright Elmer Joandi, all rights restricted
 *	
 *
 *	Revision 0.1 ,started  30.12.1998
 *
 *
 */

#define DRV_NAME	"aironet4500_cs"
#define DRV_VERSION	"0.1"

static const char *awc_version =
DRV_NAME ".c v" DRV_VERSION " 1/1/99 Elmer Joandi, elmer@ylenurme.ee.\n";


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/ethtool.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/bitops.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>


#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ciscode.h>
#if LINUX_VERSION_CODE < 0x20300
#ifdef MODULE
#include <pcmcia/k_compat.h>
#endif
#endif
#include <pcmcia/ds.h>

#include "../aironet4500.h"


static u_int irq_mask = 0x5eF8;
static int 	awc_ports[] = {0x140,0x100,0xc0, 0x80 };
#if LINUX_VERSION_CODE > 0x20100
MODULE_PARM(irq_mask, "i");

#endif


#define RUN_AT(x)               (jiffies+(x))

#ifdef PCMCIA_DEBUG
static int pc_debug = PCMCIA_DEBUG;
MODULE_PARM(pc_debug, "i");
#define PC_DEBUG(n, args...) if (pc_debug>(n)) printk(KERN_DEBUG args)
static char *version =
"aironet4500_cs.c v0.1 1/1/99 Elmer Joandi, elmer@ylenurme.ee.\n";
#else
#define PC_DEBUG(n, args...)
#endif

/* Index of functions. */

static dev_info_t dev_info = "aironet4500_cs";

static dev_link_t *awc_attach(void);
static void awc_detach(dev_link_t *);
static void awc_release(u_long arg);
static int awc_event(event_t event, int priority,
					   event_callback_args_t *args);

static dev_link_t *dev_list;

static void cs_error(client_handle_t handle, int func, int ret)
{
#if CS_RELEASE_CODE < 0x2911
    CardServices(ReportError, dev_info, (void *)func, (void *)ret);
#else
	error_info_t err = { func, ret };
	CardServices(ReportError, handle, &err);
#endif
}

#define CFG_CHECK(fn, args...) if (CardServices(fn, args) != 0) goto next_entry

static void flush_stale_links(void)
{
    dev_link_t *link, *next;
    for (link = dev_list; link; link = next) {
	next = link->next;
	if (link->state & DEV_STALE_LINK)
	    awc_detach(link);
    }
}


/*
   We never need to do anything when a awc device is "initialized"
   by the net software, because we only register already-found cards.
*/

static int awc_pcmcia_init(struct net_device *dev)
{
	return awc_init(dev);

}

static int awc_pcmcia_open(struct net_device *dev)
{
	dev_link_t *link;
	int status;
	
	for (link = dev_list; link; link = link->next)
		if (link->priv == dev) break;
	if (!DEV_OK(link))
		return -ENODEV;
	
	status = awc_open(dev);
	
	if (!status )
		link->open++;
	
	return status;
}

static int awc_pcmcia_close(struct net_device *dev)
{
//	int ioaddr = dev->base_addr;
	dev_link_t *link;
	int ret;
	
	for (link = dev_list; link; link = link->next)
		if (link->priv == dev) break;
	if (link == NULL)
		return -ENODEV;

	PC_DEBUG(2, "%s: closing device.\n", dev->name);

	link->open--;
	ret = awc_close(dev);
	
	if (link->state & DEV_STALE_CONFIG) {
		link->release.expires = RUN_AT( HZ/20 );
		link->state |= DEV_RELEASE_PENDING;
		add_timer(&link->release);
	}
	return ret;
}

static void netdev_get_drvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	sprintf(info->bus_info, "PCMCIA 0x%lx", dev->base_addr);
}

#ifdef PCMCIA_DEBUG
static u32 netdev_get_msglevel(struct net_device *dev)
{
	return pc_debug;
}

static void netdev_set_msglevel(struct net_device *dev, u32 level)
{
	pc_debug = level;
}
#endif /* PCMCIA_DEBUG */

static struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= netdev_get_drvinfo,
#ifdef PCMCIA_DEBUG
	.get_msglevel		= netdev_get_msglevel,
	.set_msglevel		= netdev_set_msglevel,
#endif /* PCMCIA_DEBUG */
};

/*
	awc_attach() creates an "instance" of the driver, allocating
	local data structures for one device.  The device is registered
	with Card Services.
*/

static dev_link_t *awc_attach(void)
{
	client_reg_t client_reg;
	dev_link_t *link = NULL;
	struct net_device *dev = NULL;
	int  ret;

	PC_DEBUG(0, "awc_attach()\n");
	flush_stale_links();

	/* Create the PC card device object. */
	link = kmalloc(sizeof(struct dev_link_t), GFP_KERNEL);
	if (!link)
		return NULL;
	memset(link, 0, sizeof(struct dev_link_t));

	link->dev = kmalloc(sizeof(struct dev_node_t), GFP_KERNEL);
	if (!link->dev) {
		kfree(link);
		return NULL;
	}

	memset(link->dev, 0, sizeof(struct dev_node_t));

	link->release.function = &awc_release;
	link->release.data = (u_long)link;
//	link->io.NumPorts1 = 32;
	link->io.Attributes1 = IO_DATA_PATH_WIDTH_16;
//	link->io.IOAddrLines = 5;
	link->irq.Attributes = IRQ_HANDLE_PRESENT ; // |IRQ_TYPE_EXCLUSIVE  ;
	link->irq.IRQInfo1 = IRQ_INFO2_VALID|IRQ_LEVEL_ID;
	link->irq.IRQInfo2 = irq_mask;
	link->irq.Handler = &awc_interrupt;
	link->conf.Attributes = CONF_ENABLE_IRQ;
	link->conf.Vcc = 50;
	link->conf.IntType = INT_MEMORY_AND_IO;
	link->conf.ConfigIndex = 1;
	link->conf.Present = PRESENT_OPTION;

	/* Create the network device object. */

	dev = kmalloc(sizeof(struct net_device ), GFP_KERNEL);
//	dev =  init_etherdev(0, sizeof(struct awc_private) );
	if (!dev ) {
		printk(KERN_CRIT "out of mem on dev alloc \n");
		kfree(link->dev);
		kfree(link);
		return NULL;
	};
	memset(dev,0,sizeof(struct net_device));
	dev->priv = kmalloc(sizeof(struct awc_private), GFP_KERNEL);
	if (!dev->priv ) {printk(KERN_CRIT "out of mem on dev priv alloc \n"); kfree(dev); return NULL;};
	memset(dev->priv,0,sizeof(struct awc_private));
	
//	link->dev->minor = dev->minor;
//	link->dev->major = dev->major;

	/* The 4500-specific entries in the device structure. */

//	dev->tx_queue_len = tx_queue_len;

	dev->hard_start_xmit = 		&awc_start_xmit;
//	dev->set_config = 		&awc_config_misiganes,aga mitte awc_config;
	dev->get_stats = 		&awc_get_stats;
//	dev->set_multicast_list = 	&awc_set_multicast_list;
	SET_ETHTOOL_OPS(dev, &netdev_ethtool_ops);
	
	strcpy(dev->name, ((struct awc_private *)dev->priv)->node.dev_name);

	ether_setup(dev);

	dev->init = &awc_pcmcia_init;
	dev->open = &awc_pcmcia_open;
	dev->stop = &awc_pcmcia_close;
	
	link->priv = dev;
#if CS_RELEASE_CODE > 0x2911
	link->irq.Instance = dev;
#endif

	/* Register with Card Services */
	link->next = dev_list;
	dev_list = link;
	
        
	client_reg.dev_info = &dev_info;
	client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
	client_reg.EventMask =
		CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
			CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
				CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
	client_reg.event_handler = &awc_event;
	client_reg.Version = 0x0210;
	client_reg.event_callback_args.client_data = link;
	ret = CardServices(RegisterClient, &link->handle, &client_reg);
	if (ret != 0) {
		cs_error(link->handle, RegisterClient, ret);
		awc_detach(link);
		return NULL;
	}

	return link;
} /* awc_attach */

/*

	This deletes a driver "instance".  The device is de-registered
	with Card Services.  If it has been released, all local data
	structures are freed.  Otherwise, the structures will be freed
	when the device is released.

*/

static void awc_detach(dev_link_t *link)
{
	dev_link_t **linkp;
	unsigned long flags;
	int i=0;

	DEBUG(0, "awc_detach(0x%p)\n", link);

	/* Locate device structure */
	for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
		if (*linkp == link) break;
	if (*linkp == NULL)
	return;

	save_flags(flags);
	cli();
	if (link->state & DEV_RELEASE_PENDING) {
		del_timer(&link->release);
		link->state &= ~DEV_RELEASE_PENDING;
	}
	restore_flags(flags);

	if (link->state & DEV_CONFIG) {
		awc_release((u_long)link);
		if (link->state & DEV_STALE_CONFIG) {
			link->state |= DEV_STALE_LINK;
			return;
		}
	}

	if (link->handle)
		CardServices(DeregisterClient, link->handle);

	/* Unlink device structure, free bits */
	*linkp = link->next;

	i=0;
	while ( i < MAX_AWCS) {
		if (!aironet4500_devices[i])
			{i++; continue;}
		if (aironet4500_devices[i] == link->priv){
			if (awc_proc_unset_fun)
				awc_proc_unset_fun(i);

			aironet4500_devices[i]=0;
		}
		i++;
	}
	
	if (link->priv) {
		//struct net_device *dev = link->priv;
		// dam dam damn mif (dev->priv)
		//	kfree(dev->priv);
		kfree(link->priv);
	}
	kfree(link->dev);
	kfree(link);

} /* awc_detach */

/*

	awc_pcmcia_config() is scheduled to run after a CARD_INSERTION event
	is received, to configure the PCMCIA socket, and to make the
	ethernet device available to the system.

*/

#define CS_CHECK(fn, args...) \
while ((last_ret=CardServices(last_fn=(fn), args))!=0) goto cs_failed

static void awc_pcmcia_config(dev_link_t *link)
{
	client_handle_t handle;
	struct net_device *dev;
	struct awc_private *lp;
	tuple_t tuple;
	int ii;
	cisparse_t parse;
	u_short buf[64];
	int last_fn, last_ret, i = 0;
//	int ioaddr;
	u16 *phys_addr;
	int retval;
	
	handle = link->handle;
	dev = link->priv;
	phys_addr = (u16 *)dev->dev_addr;

	PC_DEBUG(0, "awc_pcmcia_config(0x%p)\n", link);

	tuple.Attributes = 0;
	tuple.DesiredTuple = CISTPL_CONFIG;
	CS_CHECK(GetFirstTuple, handle, &tuple);
	tuple.TupleData = (cisdata_t *)buf;
	tuple.TupleDataMax = 64;
	tuple.TupleOffset = 0;
	CS_CHECK(GetTupleData, handle, &tuple);
	CS_CHECK(ParseTuple, handle, &tuple, &parse);
	link->conf.ConfigBase = parse.config.base;
	link->conf.Present = parse.config.rmask[0];


	/* Configure card */
	link->state |= DEV_CONFIG;

     	tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
        CS_CHECK(GetFirstTuple, handle, &tuple);

    	while (1) {
		cistpl_cftable_entry_t dflt = { 0 };
		cistpl_cftable_entry_t *cfg = &(parse.cftable_entry);
		CFG_CHECK(GetTupleData, handle, &tuple);
		CFG_CHECK(ParseTuple, handle, &tuple, &parse);

		if (cfg->flags & CISTPL_CFTABLE_DEFAULT) dflt = *cfg;
		if (cfg->index == 0) goto next_entry;
		link->conf.ConfigIndex = cfg->index;

		/* Use power settings for Vcc and Vpp if present */
		/*  Note that the CIS values need to be rescaled */
		if (cfg->vcc.present & (1<<CISTPL_POWER_VNOM))
		    link->conf.Vcc = cfg->vcc.param[CISTPL_POWER_VNOM]/10000;
		else if (dflt.vcc.present & (1<<CISTPL_POWER_VNOM))
		    link->conf.Vcc = dflt.vcc.param[CISTPL_POWER_VNOM]/10000;
	    
		if (cfg->vpp1.present & (1<<CISTPL_POWER_VNOM))
		    link->conf.Vpp1 = link->conf.Vpp2 =
			cfg->vpp1.param[CISTPL_POWER_VNOM]/10000;
		else if (dflt.vpp1.present & (1<<CISTPL_POWER_VNOM))
		    link->conf.Vpp1 = link->conf.Vpp2 =
			dflt.vpp1.param[CISTPL_POWER_VNOM]/10000;
	
		/* Do we need to allocate an interrupt? */
		if (cfg->irq.IRQInfo1 || dflt.irq.IRQInfo1)
		    link->conf.Attributes |= CONF_ENABLE_IRQ;
	
		/* IO window settings */
		link->io.NumPorts1 = link->io.NumPorts2 = 0;
		if ((cfg->io.nwin > 0) || (dflt.io.nwin > 0)) {
	    		cistpl_io_t *io = (cfg->io.nwin) ? &cfg->io : &dflt.io;
	    		link->io.Attributes1 = IO_DATA_PATH_WIDTH_AUTO;
	    		if (!(io->flags & CISTPL_IO_8BIT))
				link->io.Attributes1 = IO_DATA_PATH_WIDTH_16;
	    		if (!(io->flags & CISTPL_IO_16BIT)) {
	    			
				link->io.Attributes1 = IO_DATA_PATH_WIDTH_8;
				printk(KERN_CRIT "8-bit IO not supported on this aironet 4500 driver \n");
	    		}
	    		link->io.BasePort1 = io->win[0].base;
	    		
	    		link->io.NumPorts1 = io->win[0].len;
	    		if (io->nwin > 1) {
				link->io.Attributes2 = link->io.Attributes1;
				link->io.BasePort2 = io->win[1].base;
				link->io.NumPorts2 = io->win[1].len;
	    		}
		}
		ii = 0;
		last_fn = RequestIO;
		while ((last_ret = CardServices(RequestIO, link->handle, &link->io)) ){
		
			if (ii > 4) 
				goto cs_failed;
			link->io.BasePort1 = awc_ports[ii];
			ii++;
		};


		break;
	
    	next_entry:
		if (CardServices(GetNextTuple, handle, &tuple))
			break;
    	}
    
    	if (link->conf.Attributes & CONF_ENABLE_IRQ){
	
		ii = 0;  last_fn = RequestIRQ; 
		while ((last_ret  = CardServices(RequestIRQ, link->handle, &link->irq)) ){
		
			ii++;	
			while (!(irq_mask & (1 << ii) ) && ii < 15)
			 	ii++;
			link->irq.IRQInfo2 = 1 << ii;

			if(ii > 15)
				goto cs_failed;
			printk("trying irq %d , mask %x \n",ii, link->irq.IRQInfo2); 
			 
		};
	}

    	CS_CHECK(RequestConfiguration, link->handle, &link->conf);


    	dev->irq = link->irq.AssignedIRQ;
    	dev->base_addr = link->io.BasePort1;


	awc_private_init( dev);


	
	retval = register_netdev(dev);
	if (retval != 0) {
		printk(KERN_NOTICE "awc_cs: register_netdev() failed for dev %x retval %x\n",(unsigned int)dev,retval);
		goto failed;
	}

    	if(awc_pcmcia_init(dev)) goto failed;

	i=0;
	while (aironet4500_devices[i] && i < MAX_AWCS-1) i++;
	if (!aironet4500_devices[i]){
		aironet4500_devices[i]=dev;
		if (awc_proc_set_fun)
			awc_proc_set_fun(i);
	}
	

	link->state &= ~DEV_CONFIG_PENDING;

	lp = (struct awc_private *)dev->priv;

	DEBUG(1,"pcmcia config complete on port %x \n",(unsigned int)dev->base_addr);

	return;

cs_failed:
	cs_error(link->handle, last_fn, last_ret);
	link->dev=NULL;
failed:

	awc_release((u_long)link);
	return;

} /* awc_pcmcia_config */

/*
	After a card is removed, awc_release() will unregister the net
	device, and release the PCMCIA configuration.  If the device is
	still open, this will be postponed until it is closed.

*/

static void awc_release(u_long arg)
{
	dev_link_t *link = (dev_link_t *)arg;
	struct net_device *dev = link->priv;

	DEBUG(0, "awc_release(0x%p)\n", link);

	if (link->open) {
		DEBUG(1, "awc_cs: release postponed, '%s' still open\n",
			  link->dev->dev_name);
		link->state |= DEV_STALE_CONFIG;
		return;
	}

	CardServices(ReleaseConfiguration, link->handle);
	CardServices(ReleaseIO, link->handle, &link->io);
	CardServices(ReleaseIRQ, link->handle, &link->irq);

	CardServices(ReleaseWindow, link->win);
	if (link->dev)
		unregister_netdev(dev);
	// link->dev = NULL;

	link->state &= ~DEV_CONFIG;
	if (link->state & DEV_STALE_LINK)
		awc_detach(link);

} /* awc_release */

/*

	The card status event handler.  Mostly, this schedules other
	stuff to run after an event is received.  A CARD_REMOVAL event
	also sets some flags to discourage the net drivers from trying
	to talk to the card any more.
*/

static int awc_event(event_t event, int priority,
					   event_callback_args_t *args)
{
	dev_link_t *link = args->client_data;
	struct net_device *dev = link->priv;

	PC_DEBUG(1, "awc_event(0x%06x)\n", event);

	switch (event) {
	case CS_EVENT_CARD_REMOVAL:
		link->state &= ~DEV_PRESENT;
		if (link->state & DEV_CONFIG) {
			netif_device_detach(dev);
			link->release.expires = RUN_AT( HZ/20 );
			add_timer(&link->release);
		}
		break;
	case CS_EVENT_CARD_INSERTION:
		link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
		awc_pcmcia_config(link);
		break;
	case CS_EVENT_PM_SUSPEND:
		link->state |= DEV_SUSPEND;
		/* Fall through... */
	case CS_EVENT_RESET_PHYSICAL:
		if (link->state & DEV_CONFIG) {
			if (link->open)
				netif_device_detach(dev);

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
				// awc_reset(dev);
				netif_device_attach(dev);
			}
		}
		break;
	}
	return 0;
} /* awc_event */


        
static int __init aironet_cs_init(void)
{
	servinfo_t serv;

	/* Always emit the version, before any failure. */
	printk(KERN_INFO"%s", awc_version);
	PC_DEBUG(0, "%s\n", version);
	CardServices(GetCardServicesInfo, &serv);
	if (serv.Revision != CS_RELEASE_CODE) {
		printk(KERN_NOTICE "awc_cs: Card Services release "
			   "does not match!\n");
		return -1;
	}
	register_pcmcia_driver(&dev_info, &awc_attach, &awc_detach);
	return 0;
}

static void __exit aironet_cs_exit(void)
{
	DEBUG(0, "awc_cs: unloading %c ",'\n');
	unregister_pcmcia_driver(&dev_info);
	
	while (dev_list != NULL) {
		if (dev_list->state & DEV_CONFIG)
			awc_release((u_long)dev_list);
		awc_detach(dev_list);
	}
	        
//	while (dev_list != NULL)
//		awc_detach(dev_list);
}

module_init(aironet_cs_init);
module_exit(aironet_cs_exit);
MODULE_LICENSE("GPL");
