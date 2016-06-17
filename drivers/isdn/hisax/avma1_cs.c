/*
 * PCMCIA client driver for AVM A1 / Fritz!PCMCIA
 *
 * Author       Carsten Paeth
 * Copyright    1998-2001 by Carsten Paeth <calle@calle.in-berlin.de>
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/module.h>


#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/system.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>

MODULE_DESCRIPTION("ISDN4Linux: PCMCIA client driver for AVM A1/Fritz!PCMCIA cards");
MODULE_AUTHOR("Carsten Paeth");
MODULE_LICENSE("GPL");

int avm_a1_init_pcmcia(void *pcm_iob, int pcm_irq, int *busy_flag, int prot);
void HiSax_closecard(int cardnr);


/*
   All the PCMCIA modules use PCMCIA_DEBUG to control debugging.  If
   you do not define PCMCIA_DEBUG at all, all the debug code will be
   left out.  If you compile with PCMCIA_DEBUG=0, the debug code will
   be present but disabled -- but it can then be enabled for specific
   modules at load time with a 'pc_debug=#' option to insmod.
*/
#ifdef PCMCIA_DEBUG
static int pc_debug = PCMCIA_DEBUG;
MODULE_PARM(pc_debug, "i");
#define DEBUG(n, args...) if (pc_debug>(n)) printk(KERN_DEBUG args);
static char *version =
"avma1_cs.c 1.00 1998/01/23 10:00:00 (Carsten Paeth)";
#else
#define DEBUG(n, args...)
#endif

/*====================================================================*/

/* Parameters that can be set with 'insmod' */

static int default_irq_list[11] = { 15, 13, 12, 11, 10, 9, 7, 5, 4, 3, -1 };
static int irq_list[11] = { -1 };
static int isdnprot = 2;

MODULE_PARM(irq_list, "1-11i");
MODULE_PARM(isdnprot, "1-4i");

/*====================================================================*/

/*
   The event() function is this driver's Card Services event handler.
   It will be called by Card Services when an appropriate card status
   event is received.  The config() and release() entry points are
   used to configure or release a socket, in response to card insertion
   and ejection events.  They are invoked from the skeleton event
   handler.
*/

static void avma1cs_config(dev_link_t *link);
static void avma1cs_release(u_long arg);
static int avma1cs_event(event_t event, int priority,
			  event_callback_args_t *args);

/*
   The attach() and detach() entry points are used to create and destroy
   "instances" of the driver, where each instance represents everything
   needed to manage one actual PCMCIA card.
*/

static dev_link_t *avma1cs_attach(void);
static void avma1cs_detach(dev_link_t *);

/*
   The dev_info variable is the "key" that is used to match up this
   device driver with appropriate cards, through the card configuration
   database.
*/

static dev_info_t dev_info = "avma1_cs";

/*
   A linked list of "instances" of the skeleton device.  Each actual
   PCMCIA card corresponds to one device instance, and is described
   by one dev_link_t structure (defined in ds.h).

   You may not want to use a linked list for this -- for example, the
   memory card driver uses an array of dev_link_t pointers, where minor
   device numbers are used to derive the corresponding array index.
*/

static dev_link_t *dev_list = NULL;

/*
   A dev_link_t structure has fields for most things that are needed
   to keep track of a socket, but there will usually be some device
   specific information that also needs to be kept track of.  The
   'priv' pointer in a dev_link_t structure can be used to point to
   a device-specific private data structure, like this.

   A driver needs to provide a dev_node_t structure for each device
   on a card.  In some cases, there is only one device per card (for
   example, ethernet cards, modems).  In other cases, there may be
   many actual or logical devices (SCSI adapters, memory cards with
   multiple partitions).  The dev_node_t structures need to be kept
   in a linked list starting at the 'dev' field of a dev_link_t
   structure.  We allocate them in the card's private data structure,
   because they generally can't be allocated dynamically.
*/
   
typedef struct local_info_t {
    dev_node_t	node;
} local_info_t;

/*====================================================================*/

static void cs_error(client_handle_t handle, int func, int ret)
{
    error_info_t err = { func, ret };
    CardServices(ReportError, handle, &err);
}

/*======================================================================

    avma1cs_attach() creates an "instance" of the driver, allocating
    local data structures for one device.  The device is registered
    with Card Services.

    The dev_link structure is initialized, but we don't actually
    configure the card at this point -- we wait until we receive a
    card insertion event.
    
======================================================================*/

static dev_link_t *avma1cs_attach(void)
{
    client_reg_t client_reg;
    dev_link_t *link;
    local_info_t *local;
    int ret, i;
    
    DEBUG(0, "avma1cs_attach()\n");

    /* Initialize the dev_link_t structure */
    link = kmalloc(sizeof(struct dev_link_t), GFP_KERNEL);
    memset(link, 0, sizeof(struct dev_link_t));
    link->release.function = &avma1cs_release;
    link->release.data = (u_long)link;

    /* The io structure describes IO port mapping */
    link->io.NumPorts1 = 16;
    link->io.Attributes1 = IO_DATA_PATH_WIDTH_8;
    link->io.NumPorts2 = 16;
    link->io.Attributes2 = IO_DATA_PATH_WIDTH_16;
    link->io.IOAddrLines = 5;

    /* Interrupt setup */
    link->irq.Attributes = IRQ_TYPE_EXCLUSIVE;
    link->irq.Attributes = IRQ_TYPE_DYNAMIC_SHARING|IRQ_FIRST_SHARED;

    link->irq.IRQInfo1 = IRQ_INFO2_VALID|IRQ_LEVEL_ID;
    if (irq_list[0] != -1) {
	    for (i = 0; i < 10 && irq_list[i] > 0; i++)
	       link->irq.IRQInfo2 |= 1 << irq_list[i];
    } else {
	    for (i = 0; i < 10 && default_irq_list[i] > 0; i++)
	       link->irq.IRQInfo2 |= 1 << default_irq_list[i];
    }
    
    /* General socket configuration */
    link->conf.Attributes = CONF_ENABLE_IRQ;
    link->conf.Vcc = 50;
    link->conf.IntType = INT_MEMORY_AND_IO;
    link->conf.ConfigIndex = 1;
    link->conf.Present = PRESENT_OPTION;

    /* Allocate space for private device-specific data */
    local = kmalloc(sizeof(local_info_t), GFP_KERNEL);
    memset(local, 0, sizeof(local_info_t));
    link->priv = local;
    
    /* Register with Card Services */
    link->next = dev_list;
    dev_list = link;
    client_reg.dev_info = &dev_info;
    client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
    client_reg.EventMask =
	CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
	CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
	CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
    client_reg.event_handler = &avma1cs_event;
    client_reg.Version = 0x0210;
    client_reg.event_callback_args.client_data = link;
    ret = CardServices(RegisterClient, &link->handle, &client_reg);
    if (ret != 0) {
	cs_error(link->handle, RegisterClient, ret);
	avma1cs_detach(link);
	return NULL;
    }

    return link;
} /* avma1cs_attach */

/*======================================================================

    This deletes a driver "instance".  The device is de-registered
    with Card Services.  If it has been released, all local data
    structures are freed.  Otherwise, the structures will be freed
    when the device is released.

======================================================================*/

static void avma1cs_detach(dev_link_t *link)
{
    dev_link_t **linkp;

    DEBUG(0, "avma1cs_detach(0x%p)\n", link);
    
    /* Locate device structure */
    for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
	if (*linkp == link) break;
    if (*linkp == NULL)
	return;

    /*
       If the device is currently configured and active, we won't
       actually delete it yet.  Instead, it is marked so that when
       the release() function is called, that will trigger a proper
       detach().
    */
    if (link->state & DEV_CONFIG) {
#ifdef PCMCIA_DEBUG
	printk(KERN_DEBUG "avma1_cs: detach postponed, '%s' "
	       "still locked\n", link->dev->dev_name);
#endif
	link->state |= DEV_STALE_LINK;
	return;
    }

    /* Break the link with Card Services */
    if (link->handle)
	CardServices(DeregisterClient, link->handle);
    
    /* Unlink device structure, free pieces */
    *linkp = link->next;
    if (link->priv) {
	kfree(link->priv);
    }
    kfree(link);
    
} /* avma1cs_detach */

/*======================================================================

    avma1cs_config() is scheduled to run after a CARD_INSERTION event
    is received, to configure the PCMCIA socket, and to make the
    ethernet device available to the system.
    
======================================================================*/

static int get_tuple(int fn, client_handle_t handle, tuple_t *tuple,
		     cisparse_t *parse)
{
    int i;
    i = CardServices(fn, handle, tuple);
    if (i != CS_SUCCESS) return i;
    i = CardServices(GetTupleData, handle, tuple);
    if (i != CS_SUCCESS) return i;
    return CardServices(ParseTuple, handle, tuple, parse);
}

#define first_tuple(a, b, c) get_tuple(GetFirstTuple, a, b, c)
#define next_tuple(a, b, c) get_tuple(GetNextTuple, a, b, c)

static void avma1cs_config(dev_link_t *link)
{
    client_handle_t handle;
    tuple_t tuple;
    cisparse_t parse;
    cistpl_cftable_entry_t *cf = &parse.cftable_entry;
    local_info_t *dev;
    int i;
    u_char buf[64];
    char devname[128];
    int busy = 0;
    
    handle = link->handle;
    dev = link->priv;

    DEBUG(0, "avma1cs_config(0x%p)\n", link);

    /*
       This reads the card's CONFIG tuple to find its configuration
       registers.
    */
    do {
	tuple.DesiredTuple = CISTPL_CONFIG;
	i = CardServices(GetFirstTuple, handle, &tuple);
	if (i != CS_SUCCESS) break;
	tuple.TupleData = buf;
	tuple.TupleDataMax = 64;
	tuple.TupleOffset = 0;
	i = CardServices(GetTupleData, handle, &tuple);
	if (i != CS_SUCCESS) break;
	i = CardServices(ParseTuple, handle, &tuple, &parse);
	if (i != CS_SUCCESS) break;
	link->conf.ConfigBase = parse.config.base;
    } while (0);
    if (i != CS_SUCCESS) {
	cs_error(link->handle, ParseTuple, i);
	link->state &= ~DEV_CONFIG_PENDING;
	return;
    }
    
    /* Configure card */
    link->state |= DEV_CONFIG;

    do {

	tuple.Attributes = 0;
	tuple.TupleData = buf;
	tuple.TupleDataMax = 254;
	tuple.TupleOffset = 0;
	tuple.DesiredTuple = CISTPL_VERS_1;

	devname[0] = 0;
	if( !first_tuple(handle, &tuple, &parse) && parse.version_1.ns > 1 ) {
	    strncpy(devname,parse.version_1.str + parse.version_1.ofs[1], 
			sizeof(devname));
	}
	/*
         * find IO port
         */
	tuple.TupleData = (cisdata_t *)buf;
	tuple.TupleOffset = 0; tuple.TupleDataMax = 255;
	tuple.Attributes = 0;
	tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
	i = first_tuple(handle, &tuple, &parse);
	while (i == CS_SUCCESS) {
	    if (cf->io.nwin > 0) {
		link->conf.ConfigIndex = cf->index;
		link->io.BasePort1 = cf->io.win[0].base;
		link->io.NumPorts1 = cf->io.win[0].len;
		link->io.NumPorts2 = 0;
                printk(KERN_INFO "avma1_cs: testing i/o %#x-%#x\n",
			link->io.BasePort1,
		        link->io.BasePort1+link->io.NumPorts1 - 1);
		i = CardServices(RequestIO, link->handle, &link->io);
		if (i == CS_SUCCESS) goto found_port;
	    }
	    i = next_tuple(handle, &tuple, &parse);
	}

found_port:
	if (i != CS_SUCCESS) {
	    cs_error(link->handle, RequestIO, i);
	    break;
	}
	
	/*
	 * allocate an interrupt line
	 */
	i = CardServices(RequestIRQ, link->handle, &link->irq);
	if (i != CS_SUCCESS) {
	    cs_error(link->handle, RequestIRQ, i);
	    CardServices(ReleaseIO, link->handle, &link->io);
	    break;
	}
	
	/*
         * configure the PCMCIA socket
	  */
	i = CardServices(RequestConfiguration, link->handle, &link->conf);
	if (i != CS_SUCCESS) {
	    cs_error(link->handle, RequestConfiguration, i);
	    CardServices(ReleaseIO, link->handle, &link->io);
	    CardServices(ReleaseIRQ, link->handle, &link->irq);
	    break;
	}

    } while (0);

    /* At this point, the dev_node_t structure(s) should be
       initialized and arranged in a linked list at link->dev. */

    strcpy(dev->node.dev_name, "A1");
    dev->node.major = 45;
    dev->node.minor = 0;
    link->dev = &dev->node;
    
    link->state &= ~DEV_CONFIG_PENDING;
    /* If any step failed, release any partially configured state */
    if (i != 0) {
	avma1cs_release((u_long)link);
	return;
    }

    printk(KERN_NOTICE "avma1_cs: checking at i/o %#x, irq %d\n",
				link->io.BasePort1, link->irq.AssignedIRQ);

    if (avm_a1_init_pcmcia((void *)(int)link->io.BasePort1,
                           link->irq.AssignedIRQ,
                           &busy, isdnprot) != 0) {
       printk(KERN_ERR "avma1_cs: failed to initialize AVM A1 PCMCIA %d at i/o %#x\n", i, link->io.BasePort1);
       return;
    }

    i = 0; /* no returncode for cardnr :-( */

    dev->node.minor = i;

} /* avma1cs_config */

/*======================================================================

    After a card is removed, avma1cs_release() will unregister the net
    device, and release the PCMCIA configuration.  If the device is
    still open, this will be postponed until it is closed.
    
======================================================================*/

static void avma1cs_release(u_long arg)
{
    dev_link_t *link = (dev_link_t *)arg;
    local_info_t *local = link->priv;

    DEBUG(0, "avma1cs_release(0x%p)\n", link);

    /*
       If the device is currently in use, we won't release until it
       is actually closed.
    */
    if (link->open) {
	DEBUG(1, "avma1_cs: release postponed, '%s' still open\n",
	      link->dev->dev_name);
	link->state |= DEV_STALE_CONFIG;
	return;
    }

    /* no unregister function with hisax */
    HiSax_closecard(local->node.minor);

    /* Unlink the device chain */
    link->dev = NULL;
    
    /* Don't bother checking to see if these succeed or not */
    CardServices(ReleaseConfiguration, link->handle);
    CardServices(ReleaseIO, link->handle, &link->io);
    CardServices(ReleaseIRQ, link->handle, &link->irq);
    link->state &= ~DEV_CONFIG;
    
    if (link->state & DEV_STALE_LINK)
	avma1cs_detach(link);
    
} /* avma1cs_release */

/*======================================================================

    The card status event handler.  Mostly, this schedules other
    stuff to run after an event is received.  A CARD_REMOVAL event
    also sets some flags to discourage the net drivers from trying
    to talk to the card any more.

    When a CARD_REMOVAL event is received, we immediately set a flag
    to block future accesses to this device.  All the functions that
    actually access the device should check this flag to make sure
    the card is still present.
    
======================================================================*/

static int avma1cs_event(event_t event, int priority,
			  event_callback_args_t *args)
{
    dev_link_t *link = args->client_data;

    DEBUG(1, "avma1cs_event(0x%06x)\n", event);
    
    switch (event) {
    case CS_EVENT_CARD_REMOVAL:
	link->state &= ~DEV_PRESENT;
	if (link->state & DEV_CONFIG) {
	    link->release.expires =  jiffies + HZ/20;
	    add_timer(&link->release);
	}
	break;
    case CS_EVENT_CARD_INSERTION:
	link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
	avma1cs_config(link);
	break;
    case CS_EVENT_PM_SUSPEND:
	link->state |= DEV_SUSPEND;
	/* Fall through... */
    case CS_EVENT_RESET_PHYSICAL:
	if (link->state & DEV_CONFIG)
	    CardServices(ReleaseConfiguration, link->handle);
	break;
    case CS_EVENT_PM_RESUME:
	link->state &= ~DEV_SUSPEND;
	/* Fall through... */
    case CS_EVENT_CARD_RESET:
	if (link->state & DEV_CONFIG)
	    CardServices(RequestConfiguration, link->handle, &link->conf);
	break;
    }
    return 0;
} /* avma1cs_event */

/*====================================================================*/

static int __init init_avma1_cs(void)
{
    servinfo_t serv;
    DEBUG(0, "%s\n", version);
    CardServices(GetCardServicesInfo, &serv);
    if (serv.Revision != CS_RELEASE_CODE) {
        printk(KERN_NOTICE "avma1_cs: Card Services release "
               "does not match!\n");
        return -1;
    }
    register_pccard_driver(&dev_info, &avma1cs_attach, &avma1cs_detach);
    return 0;
}

static void __exit exit_avma1_cs(void)
{
    DEBUG(0, "avma1_cs: unloading\n");
    unregister_pccard_driver(&dev_info);
    while (dev_list != NULL)
	if (dev_list->state & DEV_CONFIG)
	    avma1cs_release((u_long)dev_list);
        avma1cs_detach(dev_list);
}

module_init(init_avma1_cs);
module_exit(exit_avma1_cs);
