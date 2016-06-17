/*======================================================================

    A driver for PCMCIA IDE/ATA disk cards

    ide_cs.c 1.26 1999/11/16 02:10:49

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

    Alternatively, the contents of this file may be used under the
    terms of the GNU General Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
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
#include <linux/hdreg.h>
#include <linux/major.h>
#include <linux/ide.h>

#include <asm/io.h>
#include <asm/system.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>
#include <pcmcia/cisreg.h>

#ifdef PCMCIA_DEBUG
static int pc_debug = PCMCIA_DEBUG;
MODULE_PARM(pc_debug, "i");
#define DEBUG(n, args...) if (pc_debug>(n)) printk(KERN_DEBUG args)
static char *version =
"ide_cs.c 1.26 1999/11/16 02:10:49 (David Hinds)";
#else
#define DEBUG(n, args...)
#endif

/*====================================================================*/

/* Parameters that can be set with 'insmod' */

/* Bit map of interrupts to choose from */
static u_int irq_mask = 0xdeb8;
static int irq_list[4] = { -1 };

MODULE_PARM(irq_mask, "i");
MODULE_PARM(irq_list, "1-4i");

MODULE_LICENSE("GPL");


/*====================================================================*/

static const char ide_major[] = {
    IDE0_MAJOR, IDE1_MAJOR, IDE2_MAJOR, IDE3_MAJOR,
#ifdef IDE4_MAJOR
    IDE4_MAJOR, IDE5_MAJOR
#endif
};

typedef struct ide_info_t {
    dev_link_t	link;
    int		ndev;
    dev_node_t	node;
    int		hd;
    struct tq_struct rel_task;
} ide_info_t;

static void ide_config(dev_link_t *link);
static void ide_release(void *arg);
static int ide_event(event_t event, int priority,
		     event_callback_args_t *args);

static dev_info_t dev_info = "ide-cs";

static dev_link_t *ide_attach(void);
static void ide_detach(dev_link_t *);

static dev_link_t *dev_list = NULL;

/*====================================================================*/

static void cs_error(client_handle_t handle, int func, int ret)
{
    error_info_t err = { func, ret };
    CardServices(ReportError, handle, &err);
}

/*======================================================================

    ide_attach() creates an "instance" of the driver, allocating
    local data structures for one device.  The device is registered
    with Card Services.

======================================================================*/

static dev_link_t *ide_attach(void)
{
    ide_info_t *info;
    dev_link_t *link;
    client_reg_t client_reg;
    int i, ret;
    
    DEBUG(0, "ide_attach()\n");

    /* Create new ide device */
    info = kmalloc(sizeof(*info), GFP_KERNEL);
    if (!info) return NULL;
    memset(info, 0, sizeof(*info));
    link = &info->link; link->priv = info;
    INIT_TQUEUE(&info->rel_task, ide_release, link);

    link->io.Attributes1 = IO_DATA_PATH_WIDTH_AUTO;
    link->io.Attributes2 = IO_DATA_PATH_WIDTH_8;
    link->io.IOAddrLines = 3;
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
    
    /* Register with Card Services */
    link->next = dev_list;
    dev_list = link;
    client_reg.dev_info = &dev_info;
    client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
    client_reg.EventMask =
	CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
	CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
	CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
    client_reg.event_handler = &ide_event;
    client_reg.Version = 0x0210;
    client_reg.event_callback_args.client_data = link;
    ret = CardServices(RegisterClient, &link->handle, &client_reg);
    if (ret != CS_SUCCESS) {
	cs_error(link->handle, RegisterClient, ret);
	ide_detach(link);
	return NULL;
    }
    
    return link;
} /* ide_attach */

/*======================================================================

    This deletes a driver "instance".  The device is de-registered
    with Card Services.  If it has been released, all local data
    structures are freed.  Otherwise, the structures will be freed
    when the device is released.

======================================================================*/

static void ide_detach(dev_link_t *link)
{
    dev_link_t **linkp;
    ide_info_t *info = link->priv;
    int ret;

    DEBUG(0, "ide_detach(0x%p)\n", link);
    
    /* Locate device structure */
    for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
	if (*linkp == link) break;
    if (*linkp == NULL)
	return;

    if (link->state & DEV_CONFIG) {
	schedule_task(&info->rel_task);
	flush_scheduled_tasks();
    }
    
    if (link->handle) {
	ret = CardServices(DeregisterClient, link->handle);
	if (ret != CS_SUCCESS)
	    cs_error(link->handle, DeregisterClient, ret);
    }
    
    /* Unlink, free device structure */
    *linkp = link->next;
    kfree(info);
    
} /* ide_detach */

/*======================================================================

    ide_config() is scheduled to run after a CARD_INSERTION event
    is received, to configure the PCMCIA socket, and to make the
    ide device available to the system.

======================================================================*/

#define CS_CHECK(fn, args...) \
while ((last_ret=CardServices(last_fn=(fn), args))!=0) goto cs_failed

#define CFG_CHECK(fn, args...) \
if (CardServices(fn, args) != 0) goto next_entry

int idecs_register (int arg1, int arg2, int irq)
{
        hw_regs_t hw;
        ide_init_hwif_ports(&hw, (ide_ioreg_t) arg1, (ide_ioreg_t) arg2, NULL);
        hw.irq = irq;
        hw.chipset = ide_pci; /* this enables IRQ sharing w/ PCI irqs */
        return ide_register_hw(&hw, NULL);
}

void ide_config(dev_link_t *link)
{
    client_handle_t handle = link->handle;
    ide_info_t *info = link->priv;
    tuple_t tuple;
    u_short buf[128];
    cisparse_t parse;
    config_info_t conf;
    cistpl_cftable_entry_t *cfg = &parse.cftable_entry;
    cistpl_cftable_entry_t dflt = { 0 };
    int i, pass, last_ret, last_fn, hd=-1, io_base, ctl_base;

    DEBUG(0, "ide_config(0x%p)\n", link);
    
    tuple.TupleData = (cisdata_t *)buf;
    tuple.TupleOffset = 0; tuple.TupleDataMax = 255;
    tuple.Attributes = 0;
    tuple.DesiredTuple = CISTPL_CONFIG;
    CS_CHECK(GetFirstTuple, handle, &tuple);
    CS_CHECK(GetTupleData, handle, &tuple);
    CS_CHECK(ParseTuple, handle, &tuple, &parse);
    link->conf.ConfigBase = parse.config.base;
    link->conf.Present = parse.config.rmask[0];
    
    /* Configure card */
    link->state |= DEV_CONFIG;

    /* Not sure if this is right... look up the current Vcc */
    CS_CHECK(GetConfigurationInfo, handle, &conf);
    link->conf.Vcc = conf.Vcc;
    
    pass = io_base = ctl_base = 0;
    tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
    tuple.Attributes = 0;
    CS_CHECK(GetFirstTuple, handle, &tuple);
    while (1) {
	CFG_CHECK(GetTupleData, handle, &tuple);
	CFG_CHECK(ParseTuple, handle, &tuple, &parse);

	/* Check for matching Vcc, unless we're desperate */
	if (!pass) {
	    if (cfg->vcc.present & (1<<CISTPL_POWER_VNOM)) {
		if (conf.Vcc != cfg->vcc.param[CISTPL_POWER_VNOM]/10000)
		    goto next_entry;
	    } else if (dflt.vcc.present & (1<<CISTPL_POWER_VNOM)) {
		if (conf.Vcc != dflt.vcc.param[CISTPL_POWER_VNOM]/10000)
		    goto next_entry;
	    }
	}
	
	if (cfg->vpp1.present & (1<<CISTPL_POWER_VNOM))
	    link->conf.Vpp1 = link->conf.Vpp2 =
		cfg->vpp1.param[CISTPL_POWER_VNOM]/10000;
	else if (dflt.vpp1.present & (1<<CISTPL_POWER_VNOM))
	    link->conf.Vpp1 = link->conf.Vpp2 =
		dflt.vpp1.param[CISTPL_POWER_VNOM]/10000;
	
	if ((cfg->io.nwin > 0) || (dflt.io.nwin > 0)) {
	    cistpl_io_t *io = (cfg->io.nwin) ? &cfg->io : &dflt.io;
	    link->conf.ConfigIndex = cfg->index;
	    link->io.BasePort1 = io->win[0].base;
	    link->io.IOAddrLines = io->flags & CISTPL_IO_LINES_MASK;
	    if (!(io->flags & CISTPL_IO_16BIT))
		link->io.Attributes1 = IO_DATA_PATH_WIDTH_8;
	    if (io->nwin == 2) {
		link->io.NumPorts1 = 8;
		link->io.BasePort2 = io->win[1].base;
		link->io.NumPorts2 = 1;
		CFG_CHECK(RequestIO, link->handle, &link->io);
		io_base = link->io.BasePort1;
		ctl_base = link->io.BasePort2;
	    } else if ((io->nwin == 1) && (io->win[0].len >= 16)) {
		link->io.NumPorts1 = io->win[0].len;
		link->io.NumPorts2 = 0;
		CFG_CHECK(RequestIO, link->handle, &link->io);
		io_base = link->io.BasePort1;
		ctl_base = link->io.BasePort1+0x0e;
	    } else goto next_entry;
	    /* If we've got this far, we're done */
	    break;
	}
	
    next_entry:
	if (cfg->flags & CISTPL_CFTABLE_DEFAULT) dflt = *cfg;
	if (pass) {
	    CS_CHECK(GetNextTuple, handle, &tuple);
	} else if (CardServices(GetNextTuple, handle, &tuple) != 0) {
	    CS_CHECK(GetFirstTuple, handle, &tuple);
	    memset(&dflt, 0, sizeof(dflt));
	    pass++;
	}
    }
    
    CS_CHECK(RequestIRQ, handle, &link->irq);
    CS_CHECK(RequestConfiguration, handle, &link->conf);

    /* deal with brain dead IDE resource management */
    release_region(link->io.BasePort1, link->io.NumPorts1);
    if (link->io.NumPorts2)
	release_region(link->io.BasePort2, link->io.NumPorts2);

    /* retry registration in case device is still spinning up */
    for (i = 0; i < 10; i++) {
	if (ctl_base)
	    outb(0x02, ctl_base); /* Set nIEN = disable device interrupts */
	hd = idecs_register(io_base, ctl_base, link->irq.AssignedIRQ);
	if (hd >= 0) break;
	if (link->io.NumPorts1 == 0x20) {
	    if (ctl_base)
		outb(0x02, ctl_base+0x10);
	    hd = idecs_register(io_base+0x10, ctl_base+0x10,
			      link->irq.AssignedIRQ);
	    if (hd >= 0) {
		io_base += 0x10; ctl_base += 0x10;
		break;
	    }
	}
	__set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(HZ/10);
    }
    
    if (hd < 0) {
	printk(KERN_NOTICE "ide_cs: ide_register() at 0x%03x & 0x%03x"
	       ", irq %u failed\n", io_base, ctl_base,
	       link->irq.AssignedIRQ);
	goto failed;
    }

    MOD_INC_USE_COUNT;
    info->ndev = 1;
    sprintf(info->node.dev_name, "hd%c", 'a'+(hd*2));
    info->node.major = ide_major[hd];
    info->node.minor = 0;
    info->hd = hd;
    link->dev = &info->node;
    printk(KERN_INFO "ide_cs: %s: Vcc = %d.%d, Vpp = %d.%d\n",
	   info->node.dev_name, link->conf.Vcc/10, link->conf.Vcc%10,
	   link->conf.Vpp1/10, link->conf.Vpp1%10);

    link->state &= ~DEV_CONFIG_PENDING;
    return;
    
cs_failed:
    cs_error(link->handle, last_fn, last_ret);
failed:
    ide_release(link);

} /* ide_config */

/*======================================================================

    After a card is removed, ide_release() will unregister the net
    device, and release the PCMCIA configuration.  If the device is
    still open, this will be postponed until it is closed.
    
======================================================================*/

static void ide_release(void *arg)
{
    dev_link_t *link = arg;
    ide_info_t *info = link->priv;
    
    if (!(link->state & DEV_CONFIG))
	return;

    DEBUG(0, "ide_do_release(0x%p)\n", link);

    if (info->ndev) {
        /* FIXME: if this fails we need to queue the cleanup somehow
           -- need to investigate the required PCMCIA magic */
	ide_unregister(info->hd);
	MOD_DEC_USE_COUNT;
    }

    request_region(link->io.BasePort1, link->io.NumPorts1,"ide-cs");
    if (link->io.NumPorts2)
	request_region(link->io.BasePort2, link->io.NumPorts2,"ide-cs");
    
    info->ndev = 0;
    link->dev = NULL;
    
    CardServices(ReleaseConfiguration, link->handle);
    CardServices(ReleaseIO, link->handle, &link->io);
    CardServices(ReleaseIRQ, link->handle, &link->irq);
    
    link->state &= ~DEV_CONFIG;

} /* ide_release */

/*======================================================================

    The card status event handler.  Mostly, this schedules other
    stuff to run after an event is received.  A CARD_REMOVAL event
    also sets some flags to discourage the ide drivers from
    talking to the ports.
    
======================================================================*/

int ide_event(event_t event, int priority,
	      event_callback_args_t *args)
{
    dev_link_t *link = args->client_data;
    ide_info_t *info = link->priv;

    DEBUG(1, "ide_event(0x%06x)\n", event);
    
    switch (event) {
    case CS_EVENT_CARD_REMOVAL:
	link->state &= ~DEV_PRESENT;
	if (link->state & DEV_CONFIG)
	    schedule_task(&info->rel_task);
	break;
    case CS_EVENT_CARD_INSERTION:
	link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
	ide_config(link);
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
	if (DEV_OK(link))
	    CardServices(RequestConfiguration, link->handle, &link->conf);
	break;
    }
    return 0;
} /* ide_event */

/*====================================================================*/

static int __init init_ide_cs(void)
{
    servinfo_t serv;
    DEBUG(0, "%s\n", version);
    CardServices(GetCardServicesInfo, &serv);
    if (serv.Revision != CS_RELEASE_CODE) {
	printk(KERN_NOTICE "ide_cs: Card Services release "
	       "does not match!\n");
	return -1;
    }
    register_pccard_driver(&dev_info, &ide_attach, &ide_detach);
    return 0;
}

static void __exit exit_ide_cs(void)
{
    DEBUG(0, "ide_cs: unloading\n");
    unregister_pccard_driver(&dev_info);
    while (dev_list != NULL)
	ide_detach(dev_list);
}

module_init(init_ide_cs);
module_exit(exit_ide_cs);
