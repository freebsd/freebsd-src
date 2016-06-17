/*======================================================================

    An elsa_cs PCMCIA client driver

    This driver is for the Elsa PCM ISDN Cards, i.e. the MicroLink


    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Modifications from dummy_cs.c are Copyright (C) 1999-2001 Klaus
    Lichtenwalder <Lichtenwalder@ACM.org>. All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU General Public License version 2 (the "GPL"), in
    which case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.

======================================================================*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/system.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ds.h>
#include <pcmcia/bus_ops.h>

MODULE_DESCRIPTION("ISDN4Linux: PCMCIA client driver for Elsa PCM cards");
MODULE_AUTHOR("Klaus Lichtenwalder");
MODULE_LICENSE("Dual MPL/GPL");

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
"elsa_cs.c $Revision: 1.1.4.1 $ $Date: 2001/11/20 14:19:35 $ (K.Lichtenwalder)";
#else
#define DEBUG(n, args...)
#endif

/*====================================================================*/

/* Parameters that can be set with 'insmod' */

/* Bit map of interrupts to choose from, the old way */
/* This means pick from 15, 14, 12, 11, 10, 9, 7, 5, 4, 3 */
static u_long irq_mask = 0xdeb8;

/* Newer, simpler way of listing specific interrupts */
static int irq_list[4] = { -1 };

MODULE_PARM(irq_mask, "i");
MODULE_PARM(irq_list, "1-4i");

static int protocol = 2;        /* EURO-ISDN Default */
MODULE_PARM(protocol, "i");

extern int elsa_init_pcmcia(int, int, int*, int);

/*====================================================================*/

/*
   The event() function is this driver's Card Services event handler.
   It will be called by Card Services when an appropriate card status
   event is received.  The config() and release() entry points are
   used to configure or release a socket, in response to card insertion
   and ejection events.  They are invoked from the elsa_cs event
   handler.
*/

static void elsa_cs_config(dev_link_t *link);
static void elsa_cs_release(u_long arg);
static int elsa_cs_event(event_t event, int priority,
                          event_callback_args_t *args);

/*
   The attach() and detach() entry points are used to create and destroy
   "instances" of the driver, where each instance represents everything
   needed to manage one actual PCMCIA card.
*/

static dev_link_t *elsa_cs_attach(void);
static void elsa_cs_detach(dev_link_t *);

/*
   The dev_info variable is the "key" that is used to match up this
   device driver with appropriate cards, through the card configuration
   database.
*/

static dev_info_t dev_info = "elsa_cs";

/*
   A linked list of "instances" of the elsa_cs device.  Each actual
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

   To simplify the data structure handling, we actually include the
   dev_link_t structure in the device's private data structure.

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

   The bus_operations pointer is used on platforms for which we need
   to use special socket-specific versions of normal IO primitives
   (inb, outb, readb, writeb, etc) for card IO.
*/

typedef struct local_info_t {
    dev_link_t          link;
    dev_node_t          node;
    int                 busy;
  struct bus_operations *bus;
} local_info_t;

/*====================================================================*/

static void cs_error(client_handle_t handle, int func, int ret)
{
    error_info_t err = { func, ret};
    CardServices(ReportError, handle, &err);
}

/*======================================================================

    elsa_cs_attach() creates an "instance" of the driver, allocatingx
    local data structures for one device.  The device is registered
    with Card Services.

    The dev_link structure is initialized, but we don't actually
    configure the card at this point -- we wait until we receive a
    card insertion event.

======================================================================*/

static dev_link_t *elsa_cs_attach(void)
{
    client_reg_t client_reg;
    dev_link_t *link;
    local_info_t *local;
    int ret, i;
    void elsa_interrupt(int, void *, struct pt_regs *);

    DEBUG(0, "elsa_cs_attach()\n");

    /* Allocate space for private device-specific data */
    local = kmalloc(sizeof(local_info_t), GFP_KERNEL);
    if (!local) return NULL;
    memset(local, 0, sizeof(local_info_t));
    link = &local->link; link->priv = local;

    /* Initialize the dev_link_t structure */
    link->release.function = &elsa_cs_release;
    link->release.data = (u_long)link;

    /* Interrupt setup */
    link->irq.Attributes = IRQ_TYPE_DYNAMIC_SHARING|IRQ_FIRST_SHARED;
    link->irq.IRQInfo1 = IRQ_INFO2_VALID|IRQ_LEVEL_ID|IRQ_SHARE_ID;
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
    link->io.NumPorts1 = 8;
    link->io.Attributes1 = IO_DATA_PATH_WIDTH_AUTO;
    link->io.IOAddrLines = 3;

    link->conf.Attributes = CONF_ENABLE_IRQ;
    link->conf.Vcc = 50;
    link->conf.IntType = INT_MEMORY_AND_IO;

    /* Register with Card Services */
    link->next = dev_list;
    dev_list = link;
    client_reg.dev_info = &dev_info;
    client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
    client_reg.EventMask =
        CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
        CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
        CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
    client_reg.event_handler = &elsa_cs_event;
    client_reg.Version = 0x0210;
    client_reg.event_callback_args.client_data = link;
    ret = CardServices(RegisterClient, &link->handle, &client_reg);
    if (ret != CS_SUCCESS) {
        cs_error(link->handle, RegisterClient, ret);
        elsa_cs_detach(link);
        return NULL;
    }

    return link;
} /* elsa_cs_attach */

/*======================================================================

    This deletes a driver "instance".  The device is de-registered
    with Card Services.  If it has been released, all local data
    structures are freed.  Otherwise, the structures will be freed
    when the device is released.

======================================================================*/

static void elsa_cs_detach(dev_link_t *link)
{
    dev_link_t **linkp;
    local_info_t *info = link->priv;
    int ret;

    DEBUG(0, "elsa_cs_detach(0x%p)\n", link);

    /* Locate device structure */
    for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
        if (*linkp == link) break;
    if (*linkp == NULL)
        return;

    del_timer(&link->release);
    if (link->state & DEV_CONFIG)
        elsa_cs_release((u_long)link);

    /*
       If the device is currently configured and active, we won't
       actually delete it yet.  Instead, it is marked so that when
       the release() function is called, that will trigger a proper
       detach().
    */
    if (link->state & DEV_CONFIG) {
      DEBUG(0, "elsa_cs: detach postponed, '%s' "
               "still locked\n", link->dev->dev_name);
        link->state |= DEV_STALE_LINK;
        return;
    }

    /* Break the link with Card Services */
    if (link->handle) {
        ret = CardServices(DeregisterClient, link->handle);
	if (ret != CS_SUCCESS)
	    cs_error(link->handle, DeregisterClient, ret);
    }

    /* Unlink device structure and free it */
    *linkp = link->next;
    kfree(info);

} /* elsa_cs_detach */

/*======================================================================

    elsa_cs_config() is scheduled to run after a CARD_INSERTION event
    is received, to configure the PCMCIA socket, and to make the
    device available to the system.

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

static void elsa_cs_config(dev_link_t *link)
{
    client_handle_t handle;
    tuple_t tuple;
    cisparse_t parse;
    local_info_t *dev;
    int i, j, last_fn;
    u_short buf[128];
    cistpl_cftable_entry_t *cf = &parse.cftable_entry;

    DEBUG(0, "elsa_config(0x%p)\n", link);
    handle = link->handle;
    dev = link->priv;

    /*
       This reads the card's CONFIG tuple to find its configuration
       registers.
    */
    tuple.DesiredTuple = CISTPL_CONFIG;
    tuple.TupleData = (cisdata_t *)buf;
    tuple.TupleDataMax = 255;
    tuple.TupleOffset = 0;
    tuple.Attributes = 0;
    i = first_tuple(handle, &tuple, &parse);
    if (i != CS_SUCCESS) {
        last_fn = ParseTuple;
	goto cs_failed;
    }
    link->conf.ConfigBase = parse.config.base;
    link->conf.Present = parse.config.rmask[0];

    /* Configure card */
    link->state |= DEV_CONFIG;

    tuple.TupleData = (cisdata_t *)buf;
    tuple.TupleOffset = 0; tuple.TupleDataMax = 255;
    tuple.Attributes = 0;
    tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
    i = first_tuple(handle, &tuple, &parse);
    while (i == CS_SUCCESS) {
        if ( (cf->io.nwin > 0) && cf->io.win[0].base) {
            printk(KERN_INFO "(elsa_cs: looks like the 96 model)\n");
            link->conf.ConfigIndex = cf->index;
            link->io.BasePort1 = cf->io.win[0].base;
            i = CardServices(RequestIO, link->handle, &link->io);
            if (i == CS_SUCCESS) break;
        } else {
          printk(KERN_INFO "(elsa_cs: looks like the 97 model)\n");
          link->conf.ConfigIndex = cf->index;
          for (i = 0, j = 0x2f0; j > 0x100; j -= 0x10) {
            link->io.BasePort1 = j;
            i = CardServices(RequestIO, link->handle, &link->io);
            if (i == CS_SUCCESS) break;
          }
          break;
        }
        i = next_tuple(handle, &tuple, &parse);
    }

    if (i != CS_SUCCESS) {
	last_fn = RequestIO;
	goto cs_failed;
    }

    i = CardServices(RequestIRQ, link->handle, &link->irq);
    if (i != CS_SUCCESS) {
        link->irq.AssignedIRQ = 0;
	last_fn = RequestIRQ;
        goto cs_failed;
    }

    i = CardServices(RequestConfiguration, link->handle, &link->conf);
    if (i != CS_SUCCESS) {
      last_fn = RequestConfiguration;
      goto cs_failed;
    }

    /* At this point, the dev_node_t structure(s) should be
       initialized and arranged in a linked list at link->dev. *//*  */
    sprintf(dev->node.dev_name, "elsa");
    dev->node.major = dev->node.minor = 0x0;

    link->dev = &dev->node;

    /* Finally, report what we've done */
    printk(KERN_INFO "%s: index 0x%02x: Vcc %d.%d",
           dev->node.dev_name, link->conf.ConfigIndex,
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

    elsa_init_pcmcia(link->io.BasePort1, link->irq.AssignedIRQ,
                     &(((local_info_t*)link->priv)->busy),
                     protocol);
    return;
cs_failed:
    cs_error(link->handle, last_fn, i);
    elsa_cs_release((u_long)link);
} /* elsa_cs_config */

/*======================================================================

    After a card is removed, elsa_cs_release() will unregister the net
    device, and release the PCMCIA configuration.  If the device is
    still open, this will be postponed until it is closed.

======================================================================*/

static void elsa_cs_release(u_long arg)
{
    dev_link_t *link = (dev_link_t *)arg;

    DEBUG(0, "elsa_cs_release(0x%p)\n", link);

    /*
       If the device is currently in use, we won't release until it
       is actually closed, because until then, we can't be sure that
       no one will try to access the device or its data structures.
    */
    if (link->open) {
        DEBUG(1, "elsa_cs: release postponed, '%s' "
                   "still open\n", link->dev->dev_name);
        link->state |= DEV_STALE_CONFIG;
        return;
    }

    /* Unlink the device chain */
    link->dev = NULL;

    /* Don't bother checking to see if these succeed or not */
    if (link->win)
        CardServices(ReleaseWindow, link->win);
    CardServices(ReleaseConfiguration, link->handle);
    CardServices(ReleaseIO, link->handle, &link->io);
    CardServices(ReleaseIRQ, link->handle, &link->irq);
    link->state &= ~DEV_CONFIG;

    if (link->state & DEV_STALE_LINK)
        elsa_cs_detach(link);

} /* elsa_cs_release */

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

static int elsa_cs_event(event_t event, int priority,
                          event_callback_args_t *args)
{
    dev_link_t *link = args->client_data;
    local_info_t *dev = link->priv;

    DEBUG(1, "elsa_cs_event(%d)\n", event);

    switch (event) {
    case CS_EVENT_CARD_REMOVAL:
        link->state &= ~DEV_PRESENT;
        if (link->state & DEV_CONFIG) {
            ((local_info_t*)link->priv)->busy = 1;
            mod_timer(&link->release, jiffies + HZ/20);
        }
        break;
    case CS_EVENT_CARD_INSERTION:
        link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
        dev->bus = args->bus;
        elsa_cs_config(link);
        break;
    case CS_EVENT_PM_SUSPEND:
        link->state |= DEV_SUSPEND;
        /* Fall through... */
    case CS_EVENT_RESET_PHYSICAL:
        /* Mark the device as stopped, to block IO until later */
        dev->busy = 1;
        if (link->state & DEV_CONFIG)
            CardServices(ReleaseConfiguration, link->handle);
        break;
    case CS_EVENT_PM_RESUME:
        link->state &= ~DEV_SUSPEND;
        /* Fall through... */
    case CS_EVENT_CARD_RESET:
        if (link->state & DEV_CONFIG)
            CardServices(RequestConfiguration, link->handle, &link->conf);
        dev->busy = 0;
        break;
    }
    return 0;
} /* elsa_cs_event */

/*====================================================================*/

static int __init init_elsa_cs(void)
{
    servinfo_t serv;
    DEBUG(0, "%s\n", version);
    CardServices(GetCardServicesInfo, &serv);
    if (serv.Revision != CS_RELEASE_CODE) {
        printk(KERN_NOTICE "elsa_cs: Card Services release "
               "does not match!\n");
        return -1;
    }
    register_pccard_driver(&dev_info, &elsa_cs_attach, &elsa_cs_detach);
    return 0;
}

static void __exit exit_elsa_cs(void)
{
    DEBUG(0, "elsa_cs: unloading\n");
    unregister_pccard_driver(&dev_info);
    while (dev_list != NULL)
        elsa_cs_detach(dev_list);
}

module_init(init_elsa_cs);
module_exit(exit_elsa_cs);
