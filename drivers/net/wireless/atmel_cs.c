/*** -*- linux-c -*- **********************************************************

     Driver for Atmel at76c502 at76c504 and at76c506 wireless cards.

        Copyright 2000-2001 ATMEL Corporation.
        Copyright 2003 Simon Kelley.

    This code was developed from version 2.1.1 of the Atmel drivers, 
    released by Atmel corp. under the GPL in December 2002. It also 
    includes code from the Linux aironet drivers (C) Benjamin Reed, 
    and the Linux PCMCIA package, (C) David Hinds. 

    For all queries about this code, please contact the current author, 
    Simon Kelley <simon@thekelleys.org.uk> and not Atmel Corporation.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Atmel wireless lan drivers; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

#include <linux/config.h>
#ifdef __IN_PCMCIA_PACKAGE__
#include <pcmcia/k_compat.h>
#endif
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/netdevice.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ds.h>
#include <pcmcia/ciscode.h>

#include <asm/io.h>
#include <asm/system.h>
#include <linux/wireless.h>
#include <linux/802_11.h>


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
static char *version = "$Revision: 1.2 $";
#define DEBUG(n, args...) if (pc_debug>(n)) printk(KERN_DEBUG args);
#else
#define DEBUG(n, args...)
#endif

/*====================================================================*/

/* Parameters that can be set with 'insmod' */

/* The old way: bit map of interrupts to choose from */
/* This means pick from 15, 14, 12, 11, 10, 9, 7, 5, 4, and 3 */
static u_int irq_mask = 0xdeb8;
/* Newer, simpler way of listing specific interrupts */
static int irq_list[4] = { -1 };

MODULE_AUTHOR("Simon Kelley");
MODULE_DESCRIPTION("Support for Atmel at76c50x 802.11 wireless ethernet cards.");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("Atmel at76c50x PCMCIA cards");
MODULE_PARM(irq_mask, "i");
MODULE_PARM(irq_list, "1-4i");

/*====================================================================*/

/*
   The event() function is this driver's Card Services event handler.
   It will be called by Card Services when an appropriate card status
   event is received.  The config() and release() entry points are
   used to configure or release a socket, in response to card
   insertion and ejection events.  They are invoked from the atmel_cs
   event handler. 
*/

struct net_device *init_atmel_card(int, int, char *, int,
				    int (*present_func)(void *), void * );
void stop_atmel_card( struct net_device *, int );
int reset_atmel_card( struct net_device * );

static void atmel_config(dev_link_t *link);
static void atmel_release(dev_link_t *link);
static int atmel_event(event_t event, int priority,
		       event_callback_args_t *args);

/*
   The attach() and detach() entry points are used to create and destroy
   "instances" of the driver, where each instance represents everything
   needed to manage one actual PCMCIA card.
*/

static dev_link_t *atmel_attach(void);
static void atmel_detach(dev_link_t *);

/*
   You'll also need to prototype all the functions that will actually
   be used to talk to your device.  See 'pcmem_cs' for a good example
   of a fully self-sufficient driver; the other drivers rely more or
   less on other parts of the kernel.
*/

/*
   The dev_info variable is the "key" that is used to match up this
   device driver with appropriate cards, through the card configuration
   database.
*/

static dev_info_t dev_info = "atmel_cs";

/*
   A linked list of "instances" of the  atmelnet device.  Each actual
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
   because they generally shouldn't be allocated dynamically.

   In this case, we also provide a flag to indicate if a device is
   "stopped" due to a power management event, or card ejection.  The
   device IO routines can use a flag like this to throttle IO to a
   card that is not ready to accept it.
*/
   
typedef struct local_info_t {
	dev_node_t	node;
	struct net_device *eth_dev;
} local_info_t;

/*====================================================================*/

static void cs_error(client_handle_t handle, int func, int ret)
{
	error_info_t err = { func, ret };
	CardServices(ReportError, handle, &err);
}

/*======================================================================
  
  atmel_attach() creates an "instance" of the driver, allocating
  local data structures for one device.  The device is registered
  with Card Services.
  
  The dev_link structure is initialized, but we don't actually
  configure the card at this point -- we wait until we receive a
  card insertion event.
  
  ======================================================================*/

static dev_link_t *atmel_attach(void)
{
	client_reg_t client_reg;
	dev_link_t *link;
	local_info_t *local;
	int ret, i;
	
	DEBUG(0, "atmel_attach()\n");

	/* Initialize the dev_link_t structure */
	link = kmalloc(sizeof(struct dev_link_t), GFP_KERNEL);
	if (!link) {
		printk(KERN_ERR "atmel_cs: no memory for new device\n");
		return NULL;
	}
	memset(link, 0, sizeof(struct dev_link_t));
	
	/* Interrupt setup */
	link->irq.Attributes = IRQ_TYPE_EXCLUSIVE;
	link->irq.IRQInfo1 = IRQ_INFO2_VALID|IRQ_LEVEL_ID;
	if (irq_list[0] == -1)
		link->irq.IRQInfo2 = irq_mask;
	else
		for (i = 0; i < 4; i++)
			link->irq.IRQInfo2 |= 1 << irq_list[i];
	link->irq.Handler = NULL;
	
	/*
	  General socket configuration defaults can go here.  In this
	  client, we assume very little, and rely on the CIS for almost
	  everything.  In most clients, many details (i.e., number, sizes,
	  and attributes of IO windows) are fixed by the nature of the
	  device, and can be hard-wired here.
	*/
	link->conf.Attributes = 0;
	link->conf.Vcc = 50;
	link->conf.IntType = INT_MEMORY_AND_IO;
	
	/* Allocate space for private device-specific data */
	local = kmalloc(sizeof(local_info_t), GFP_KERNEL);
	if (!local) {
		printk(KERN_ERR "atmel_cs: no memory for new device\n");
		kfree (link);
		return NULL;
	}
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
	client_reg.event_handler = &atmel_event;
	client_reg.Version = 0x0210;
	client_reg.event_callback_args.client_data = link;
	ret = CardServices(RegisterClient, &link->handle, &client_reg);
	if (ret != 0) {
		cs_error(link->handle, RegisterClient, ret);
		atmel_detach(link);
		return NULL;
	}
	
	return link;
} /* atmel_attach */

/*======================================================================
  
  This deletes a driver "instance".  The device is de-registered
  with Card Services.  If it has been released, all local data
  structures are freed.  Otherwise, the structures will be freed
  when the device is released.
  
  ======================================================================*/

static void atmel_detach(dev_link_t *link)
{
	dev_link_t **linkp;
	
	DEBUG(0, "atmel_detach(0x%p)\n", link);
	
	/* Locate device structure */
	for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
		if (*linkp == link) break;
	if (*linkp == NULL)
		return;

	if (link->state & DEV_CONFIG)
		atmel_release(link);
		
	/* Break the link with Card Services */
	if (link->handle)
		CardServices(DeregisterClient, link->handle);

	/* Unlink device structure, free pieces */
	*linkp = link->next;
	if (link->priv)
		kfree(link->priv);
	kfree(link);
}

/*======================================================================
  
  atmel_config() is scheduled to run after a CARD_INSERTION event
  is received, to configure the PCMCIA socket, and to make the
  device available to the system.
  
  ======================================================================*/

#define CS_CHECK(fn, args...) \
while ((last_ret=CardServices(last_fn=(fn),args))!=0) goto cs_failed

#define CFG_CHECK(fn, args...) \
if (CardServices(fn, args) != 0) goto next_entry

/* Call-back function to interrogate PCMCIA-specific information
   about the current existance of the card */
static int card_present(void *arg)
{ 
	dev_link_t *link = (dev_link_t *)arg;
	if (link->state & DEV_SUSPEND)
		return 0;
	else if (link->state & DEV_PRESENT)
		return 1;
	
	return 0;
}

/* list of cards we know about and their firmware requirements.
   Go either by Manfid or version strings.
   Cards not in this list will need a firmware parameter to the module
   in all probability. Note that the SMC 2632 V2 and V3 have the same
   manfids, so we ignore those and use the version1 strings. */

static struct { 
	int manf, card;
	char *ver1;
	char *firmware;
	char *name;
} card_table[] = {
	{ 0, 0, "WLAN/802.11b PC CARD", "atmel_at76c502d.bin", "Actiontec 802CAT1" },  
	{ 0, 0, "ATMEL/AT76C502AR", "atmel_at76c502.bin", "NoName-RFMD" }, 
	{ 0, 0, "ATMEL/AT76C502AR_D", "atmel_at76c502d.bin", "NoName-revD" }, 
	{ 0, 0, "ATMEL/AT76C502AR_E", "atmel_at76c502e.bin", "NoName-revE" },
	{ 0, 0, "ATMEL/AT76C504", "atmel_at76c504.bin", "NoName-504" },
	{ MANFID_3COM, 0x0620, NULL, "atmel_at76c502_3com.bin", "3com 3CRWE62092B" }, 
	{ MANFID_3COM, 0x0696, NULL, "atmel_at76c502_3com.bin", "3com 3CRSHPW_96" }, 
	{ 0, 0, "SMC/2632W-V2", "atmel_at76c502.bin", "SMC 2632W-V2" },
        { 0, 0, "SMC/2632W", "atmel_at76c502d.bin", "SMC 2632W-V3" },
	{ 0xd601, 0x0007, NULL, "atmel_at76c502.bin", "Sitecom WLAN-011"}, /* suspect - from a usenet posting. */
	{ 0x01bf, 0x3302, NULL, "atmel_at76c502d.bin", "Belkin F5D6060u"},  /*    "        "  "    "      "     */
	{ 0, 0, "BT/Voyager 1020 Laptop Adapter", "atmel_at76c502.bin", "BT Voyager 1020"},
        { 0, 0, "IEEE 802.11b/Wireless LAN PC Card", "atmel_at76c502.bin", "Siemens Gigaset PC Card II" },
	{ 0, 0, "CNet/CNWLC 11Mbps Wireless PC Card V-5", "atmel_at76c502e.bin", "CNet CNWLC-811ARL" }
};

static void atmel_config(dev_link_t *link)
{
	client_handle_t handle;
	tuple_t tuple;
	cisparse_t parse;
	local_info_t *dev;
	int last_fn, last_ret;
	u_char buf[64];
	int card_index = -1, done = 0;
	
	handle = link->handle;
	dev = link->priv;

	DEBUG(0, "atmel_config(0x%p)\n", link);
	
	tuple.Attributes = 0;
	tuple.TupleData = buf;
	tuple.TupleDataMax = sizeof(buf);
	tuple.TupleOffset = 0;
	
	tuple.DesiredTuple = CISTPL_MANFID;
	if (CardServices(GetFirstTuple, handle, &tuple) == 0) {
		int i;
		cistpl_manfid_t *manfid;
		CS_CHECK(GetTupleData, handle, &tuple);
		CS_CHECK(ParseTuple, handle, &tuple, &parse);
		manfid = &(parse.manfid);
		for (i = 0; i < sizeof(card_table)/sizeof(card_table[0]); i++) {
			if (!card_table[i].ver1 &&
			    manfid->manf == card_table[i].manf &&
			    manfid->card == card_table[i].card) {
				card_index = i;
				done = 1;
			}
		}
	}

	tuple.DesiredTuple = CISTPL_VERS_1;
	if (!done && (CardServices(GetFirstTuple, handle, &tuple) == 0)) {
		int i, j, k;
		cistpl_vers_1_t *ver1;
		CS_CHECK(GetTupleData, handle, &tuple);
		CS_CHECK(ParseTuple, handle, &tuple, &parse);
		ver1 = &(parse.version_1);
		
		for (i = 0; i < sizeof(card_table)/sizeof(card_table[0]); i++) {
			for (j = 0; j < ver1->ns; j++) {
				char *p = card_table[i].ver1;
				char *q = &ver1->str[ver1->ofs[j]];
				if (!p)
					goto mismatch;
				for (k = 0; k < j; k++) {
					while ((*p != '\0') && (*p != '/')) p++;
					if (*p == '\0') {
					  if (*q != '\0')
						goto mismatch;
					} else
					  p++;
				}
				while((*q != '\0') && (*p != '\0') && 
				      (*p != '/') && (*p == *q)) p++, q++;
				if (((*p != '\0') && *p != '/') || *q != '\0')
					goto mismatch;
			}
			card_index = i;
			break;	/* done */
			
		mismatch:
			j = 0; /* dummy stmt to shut up compiler */
		}
	}		

	/*
	  This reads the card's CONFIG tuple to find its configuration
	  registers.
	*/
	tuple.DesiredTuple = CISTPL_CONFIG;
	CS_CHECK(GetFirstTuple, handle, &tuple);
	CS_CHECK(GetTupleData, handle, &tuple);
	CS_CHECK(ParseTuple, handle, &tuple, &parse);
	link->conf.ConfigBase = parse.config.base;
	link->conf.Present = parse.config.rmask[0];
	
	/* Configure card */
	link->state |= DEV_CONFIG;
	
	/*
	  In this loop, we scan the CIS for configuration table entries,
	  each of which describes a valid card configuration, including
	  voltage, IO window, memory window, and interrupt settings.
	  
	  We make no assumptions about the card to be configured: we use
	  just the information available in the CIS.  In an ideal world,
	  this would work for any PCMCIA card, but it requires a complete
	  and accurate CIS.  In practice, a driver usually "knows" most of
	  these things without consulting the CIS, and most client drivers
	  will only use the CIS to fill in implementation-defined details.
	*/
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
		
		/* Does this card need audio output? */
		if (cfg->flags & CISTPL_CFTABLE_AUDIO) {
			link->conf.Attributes |= CONF_ENABLE_SPKR;
			link->conf.Status = CCSR_AUDIO_ENA;
		}
		
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
			if (!(io->flags & CISTPL_IO_16BIT))
				link->io.Attributes1 = IO_DATA_PATH_WIDTH_8;
			link->io.BasePort1 = io->win[0].base;
			link->io.NumPorts1 = io->win[0].len;
			if (io->nwin > 1) {
				link->io.Attributes2 = link->io.Attributes1;
				link->io.BasePort2 = io->win[1].base;
				link->io.NumPorts2 = io->win[1].len;
			}
		}
		
		/* This reserves IO space but doesn't actually enable it */
		CFG_CHECK(RequestIO, link->handle, &link->io); 
		/* If we got this far, we're cool! */
		break;
		
	next_entry:
		CS_CHECK(GetNextTuple, handle, &tuple);
	}
	
	/*
	  Allocate an interrupt line.  Note that this does not assign a
	  handler to the interrupt, unless the 'Handler' member of the
	  irq structure is initialized.
	*/
	if (link->conf.Attributes & CONF_ENABLE_IRQ)
		CS_CHECK(RequestIRQ, link->handle, &link->irq);
	
	/*
	  This actually configures the PCMCIA socket -- setting up
	  the I/O windows and the interrupt mapping, and putting the
	  card and host interface into "Memory and IO" mode.
	*/
	CS_CHECK(RequestConfiguration, link->handle, &link->conf);
	
	if (link->irq.AssignedIRQ == 0) {
		printk(KERN_ALERT 
		       "atmel: cannot assign IRQ: check that CONFIG_ISA is set in kernel config.");
		goto cs_failed;
	}
	
	((local_info_t*)link->priv)->eth_dev = 
		init_atmel_card(link->irq.AssignedIRQ,
				link->io.BasePort1,
				card_index == -1 ? NULL :  card_table[card_index].firmware,
				card_index == -1 ? 0 : (card_table[card_index].manf == MANFID_3COM),
				card_present, 
				link);
	if (!((local_info_t*)link->priv)->eth_dev) 
		goto cs_failed;
	
	/*
	  At this point, the dev_node_t structure(s) need to be
	  initialized and arranged in a linked list at link->dev.
	*/
	strcpy(dev->node.dev_name, ((local_info_t*)link->priv)->eth_dev->name );
	dev->node.major = dev->node.minor = 0;
	link->dev = &dev->node;
	
	/* Finally, report what we've done */
	printk(KERN_INFO "%s: %s%sindex 0x%02x: Vcc %d.%d",
	       dev->node.dev_name,
	       card_index == -1 ? "" :  card_table[card_index].name,
	       card_index == -1 ? "" : " ",
	       link->conf.ConfigIndex,
	       link->conf.Vcc/10, link->conf.Vcc%10);
	if (link->conf.Vpp1)
		printk(", Vpp %d.%d", link->conf.Vpp1/10, link->conf.Vpp1%10);
	if (link->conf.Attributes & CONF_ENABLE_IRQ)
		printk(", irq %d", link->irq.AssignedIRQ);
	if (link->io.NumPorts1)
		printk(", io 0x%04x-0x%04x", link->io.BasePort1,
		       link->io.BasePort1+link->io.NumPorts1-1);
	if (link->io.NumPorts2)
		printk(" & 0x%04x-0x%04x", link->io.BasePort2,
		       link->io.BasePort2+link->io.NumPorts2-1);
	printk("\n");
	
	link->state &= ~DEV_CONFIG_PENDING;
	return;
	
 cs_failed:
	cs_error(link->handle, last_fn, last_ret);
	atmel_release(link);
}

/*======================================================================
  
  After a card is removed, atmel_release() will unregister the
  device, and release the PCMCIA configuration.  If the device is
  still open, this will be postponed until it is closed.
  
  ======================================================================*/

static void atmel_release(dev_link_t *link)
{
	struct net_device *dev = ((local_info_t*)link->priv)->eth_dev;
		
	DEBUG(0, "atmel_release(0x%p)\n", link);
	
	/* Unlink the device chain */
	link->dev = NULL;
	
	if (dev) 
		stop_atmel_card(dev, 0);
	((local_info_t*)link->priv)->eth_dev = 0; 
	
	/* Don't bother checking to see if these succeed or not */
	CardServices(ReleaseConfiguration, link->handle);
	if (link->io.NumPorts1)
		CardServices(ReleaseIO, link->handle, &link->io);
	if (link->irq.AssignedIRQ)
		CardServices(ReleaseIRQ, link->handle, &link->irq);
	link->state &= ~DEV_CONFIG;
}

/*======================================================================
  
  The card status event handler.  Mostly, this schedules other
  stuff to run after an event is received.

  When a CARD_REMOVAL event is received, we immediately set a
  private flag to block future accesses to this device.  All the
  functions that actually access the device should check this flag
  to make sure the card is still present.
  
  ======================================================================*/

static int atmel_event(event_t event, int priority,
		      event_callback_args_t *args)
{
	dev_link_t *link = args->client_data;
	local_info_t *local = link->priv;
	
	DEBUG(1, "atmel_event(0x%06x)\n", event);
	
	switch (event) {
	case CS_EVENT_CARD_REMOVAL:
		link->state &= ~DEV_PRESENT;
		if (link->state & DEV_CONFIG) {
			netif_device_detach(local->eth_dev);
			atmel_release(link);
		}
		break;
	case CS_EVENT_CARD_INSERTION:
		link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
		atmel_config(link);
		break;
	case CS_EVENT_PM_SUSPEND:
		link->state |= DEV_SUSPEND;
		/* Fall through... */
	case CS_EVENT_RESET_PHYSICAL:
		if (link->state & DEV_CONFIG) {
			netif_device_detach(local->eth_dev);
			CardServices(ReleaseConfiguration, link->handle);
		}
		break;
	case CS_EVENT_PM_RESUME:
		link->state &= ~DEV_SUSPEND;
		/* Fall through... */
	case CS_EVENT_CARD_RESET:
		if (link->state & DEV_CONFIG) {
			CardServices(RequestConfiguration, link->handle, &link->conf);
			reset_atmel_card(local->eth_dev);
			netif_device_attach(local->eth_dev);
		}
		break;
	}
	return 0;
} /* atmel_event */

static int atmel_cs_init(void)
{
	servinfo_t serv;
	DEBUG(0, "%s\n", version);
	CardServices(GetCardServicesInfo, &serv);
	if (serv.Revision != CS_RELEASE_CODE) {
		printk(KERN_NOTICE "atmel_cs: Card Services release "
		       "does not match!\n");
		return -1;
	}
	register_pcmcia_driver(&dev_info, &atmel_attach, &atmel_detach);
        return 0;
}

static void atmel_cs_cleanup(void)
{
        unregister_pcmcia_driver(&dev_info);

        /* XXX: this really needs to move into generic code.. */
        while (dev_list != NULL) {
                if (dev_list->state & DEV_CONFIG)
                        atmel_release(dev_list);
                atmel_detach(dev_list);
        }
}

/*
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    In addition:

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. The name of the author may not be used to endorse or promote
       products derived from this software without specific prior written
       permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.    
*/

module_init(atmel_cs_init);
module_exit(atmel_cs_cleanup);
