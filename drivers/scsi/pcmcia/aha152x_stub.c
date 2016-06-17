/*======================================================================

    A driver for Adaptec AHA152X-compatible PCMCIA SCSI cards.

    This driver supports the Adaptec AHA-1460, the New Media Bus
    Toaster, and the New Media Toast & Jam.
    
    aha152x_cs.c 1.58 2001/10/13 00:08:51

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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <scsi/scsi.h>
#include <linux/major.h>
#include <linux/blk.h>

#include <../drivers/scsi/scsi.h>
#include <../drivers/scsi/hosts.h>
#include <scsi/scsi_ioctl.h>
#include <../drivers/scsi/aha152x.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>

/*====================================================================*/

/* Module parameters */

MODULE_AUTHOR("David Hinds <dahinds@users.sourceforge.net>");
MODULE_DESCRIPTION("Adaptec AHA152x-compatible PCMCIA SCSI driver");
MODULE_LICENSE("Dual MPL/GPL");

static int irq_list[4] = { -1 };
MODULE_PARM(irq_list, "1-4i");

#define INT_MODULE_PARM(n, v) static int n = v; MODULE_PARM(n, "i")

INT_MODULE_PARM(irq_mask,	0xdeb8);
INT_MODULE_PARM(host_id,	7);
INT_MODULE_PARM(reconnect,	1);
INT_MODULE_PARM(parity,		1);
INT_MODULE_PARM(synchronous,	0);
INT_MODULE_PARM(reset_delay,	100);
INT_MODULE_PARM(ext_trans,	0);

#ifdef AHA152X_DEBUG
INT_MODULE_PARM(debug,		0);
#endif

#ifdef PCMCIA_DEBUG
INT_MODULE_PARM(pc_debug, PCMCIA_DEBUG);
#define DEBUG(n, args...) if (pc_debug>(n)) printk(KERN_DEBUG args)
static char *version =
"aha152x_cs.c 1.58 2001/10/13 00:08:51 (David Hinds)";
#else
#define DEBUG(n, args...)
#endif

/*====================================================================*/

typedef struct scsi_info_t {
    dev_link_t		link;
    struct Scsi_Host	*host;
    int			ndev;
    dev_node_t		node[8];
} scsi_info_t;

extern void aha152x_setup(char *str, int *ints);

static void aha152x_release_cs(u_long arg);
static int aha152x_event(event_t event, int priority,
			 event_callback_args_t *args);

static dev_link_t *aha152x_attach(void);
static void aha152x_detach(dev_link_t *);

static Scsi_Host_Template driver_template = AHA152X;

static dev_link_t *dev_list = NULL;

static dev_info_t dev_info = "aha152x_cs";

/*====================================================================*/

static void cs_error(client_handle_t handle, int func, int ret)
{
    error_info_t err = { func, ret };
    CardServices(ReportError, handle, &err);
}

/*====================================================================*/

static dev_link_t *aha152x_attach(void)
{
    scsi_info_t *info;
    client_reg_t client_reg;
    dev_link_t *link;
    int i, ret;
    
    DEBUG(0, "aha152x_attach()\n");

    /* Create new SCSI device */
    info = kmalloc(sizeof(*info), GFP_KERNEL);
    if (!info) return NULL;
    memset(info, 0, sizeof(*info));
    link = &info->link; link->priv = info;

    link->io.NumPorts1 = 0x20;
    link->io.Attributes1 = IO_DATA_PATH_WIDTH_AUTO;
    link->io.IOAddrLines = 10;
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

    /* Register with Card Services */
    link->next = dev_list;
    dev_list = link;
    client_reg.dev_info = &dev_info;
    client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
    client_reg.event_handler = &aha152x_event;
    client_reg.EventMask =
	CS_EVENT_RESET_REQUEST | CS_EVENT_CARD_RESET |
	CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
	CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
    client_reg.Version = 0x0210;
    client_reg.event_callback_args.client_data = link;
    ret = CardServices(RegisterClient, &link->handle, &client_reg);
    if (ret != 0) {
	cs_error(link->handle, RegisterClient, ret);
	aha152x_detach(link);
	return NULL;
    }
    
    return link;
} /* aha152x_attach */

/*====================================================================*/

static void aha152x_detach(dev_link_t *link)
{
    dev_link_t **linkp;

    DEBUG(0, "aha152x_detach(0x%p)\n", link);
    
    /* Locate device structure */
    for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
	if (*linkp == link) break;
    if (*linkp == NULL)
	return;

    if (link->state & DEV_CONFIG) {
	aha152x_release_cs((u_long)link);
	if (link->state & DEV_STALE_CONFIG) {
	    link->state |= DEV_STALE_LINK;
	    return;
	}
    }

    if (link->handle)
	CardServices(DeregisterClient, link->handle);
    
    /* Unlink device structure, free bits */
    *linkp = link->next;
    kfree(link->priv);
    
} /* aha152x_detach */

/*====================================================================*/

#define CS_CHECK(fn, args...) \
while ((last_ret=CardServices(last_fn=(fn), args))!=0) goto cs_failed

#define CFG_CHECK(fn, args...) \
if (CardServices(fn, args) != 0) goto next_entry

static void aha152x_config_cs(dev_link_t *link)
{
    client_handle_t handle = link->handle;
    scsi_info_t *info = link->priv;
    tuple_t tuple;
    cisparse_t parse;
    int i, last_ret, last_fn, ints[8];
    u_char tuple_data[64];
    Scsi_Device *dev;
    dev_node_t *node, **tail;
    struct Scsi_Host *host;
    
    DEBUG(0, "aha152x_config(0x%p)\n", link);

    tuple.DesiredTuple = CISTPL_CONFIG;
    tuple.TupleData = tuple_data;
    tuple.TupleDataMax = 64;
    tuple.TupleOffset = 0;
    CS_CHECK(GetFirstTuple, handle, &tuple);
    CS_CHECK(GetTupleData, handle, &tuple);
    CS_CHECK(ParseTuple, handle, &tuple, &parse);
    link->conf.ConfigBase = parse.config.base;

    /* Configure card */
    driver_template.module = &__this_module;
    link->state |= DEV_CONFIG;

    tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
    CS_CHECK(GetFirstTuple, handle, &tuple);
    while (1) {
	CFG_CHECK(GetTupleData, handle, &tuple);
	CFG_CHECK(ParseTuple, handle, &tuple, &parse);
	/* For New Media T&J, look for a SCSI window */
	if (parse.cftable_entry.io.win[0].len >= 0x20)
	    link->io.BasePort1 = parse.cftable_entry.io.win[0].base;
	else if ((parse.cftable_entry.io.nwin > 1) &&
		 (parse.cftable_entry.io.win[1].len >= 0x20))
	    link->io.BasePort1 = parse.cftable_entry.io.win[1].base;
	if ((parse.cftable_entry.io.nwin > 0) &&
	    (link->io.BasePort1 < 0xffff)) {
	    link->conf.ConfigIndex = parse.cftable_entry.index;
	    i = CardServices(RequestIO, handle, &link->io);
	    if (i == CS_SUCCESS) break;
	}
    next_entry:
	CS_CHECK(GetNextTuple, handle, &tuple);
    }
    
    CS_CHECK(RequestIRQ, handle, &link->irq);
    CS_CHECK(RequestConfiguration, handle, &link->conf);
    
    /* A bad hack... */
    release_region(link->io.BasePort1, link->io.NumPorts1);
    
    /* Set configuration options for the aha152x driver */
    ints[1] = link->io.BasePort1;
    ints[2] = link->irq.AssignedIRQ;
    ints[3] = host_id;
    ints[4] = reconnect;
    ints[5] = parity;
    ints[6] = synchronous;
    ints[7] = reset_delay;
    ints[8] = ext_trans;
#ifdef AHA152X_DEBUG
    ints[9] = debug;
    ints[0] = 9;
#else
    ints[0] = 8;
#endif
    aha152x_setup("PCMCIA setup", ints);
    
    scsi_register_module(MODULE_SCSI_HA, &driver_template);

    tail = &link->dev;
    info->ndev = 0;
    for (host = scsi_hostlist; host; host = host->next)
	if (host->hostt == &driver_template)
	    for (dev = host->host_queue; dev; dev = dev->next) {
	    u_long arg[2], id;
	    kernel_scsi_ioctl(dev, SCSI_IOCTL_GET_IDLUN, arg);
	    id = (arg[0]&0x0f) + ((arg[0]>>4)&0xf0) +
		((arg[0]>>8)&0xf00) + ((arg[0]>>12)&0xf000);
	    node = &info->node[info->ndev];
	    node->minor = 0;
	    switch (dev->type) {
	    case TYPE_TAPE:
		node->major = SCSI_TAPE_MAJOR;
		sprintf(node->dev_name, "st#%04lx", id);
		break;
	    case TYPE_DISK:
	    case TYPE_MOD:
		node->major = SCSI_DISK0_MAJOR;
		sprintf(node->dev_name, "sd#%04lx", id);
		break;
	    case TYPE_ROM:
	    case TYPE_WORM:
		node->major = SCSI_CDROM_MAJOR;
		sprintf(node->dev_name, "sr#%04lx", id);
		break;
	    default:
		node->major = SCSI_GENERIC_MAJOR;
		sprintf(node->dev_name, "sg#%04lx", id);
		break;
	    }
	    *tail = node; tail = &node->next;
	    info->ndev++;
	    info->host = dev->host;
	}
    *tail = NULL;
    if (info->ndev == 0)
	printk(KERN_INFO "aha152x_cs: no SCSI devices found\n");
    
    link->state &= ~DEV_CONFIG_PENDING;
    return;
    
cs_failed:
    cs_error(link->handle, last_fn, last_ret);
    aha152x_release_cs((u_long)link);
    return;
    
} /* aha152x_config_cs */

/*====================================================================*/

static void aha152x_release_cs(u_long arg)
{
    dev_link_t *link = (dev_link_t *)arg;

    DEBUG(0, "aha152x_release_cs(0x%p)\n", link);

    if (GET_USE_COUNT(driver_template.module) != 0) {
	DEBUG(1, "aha152x_cs: release postponed, "
	      "device still open\n");
	link->state |= DEV_STALE_CONFIG;
	return;
    }

    scsi_unregister_module(MODULE_SCSI_HA, &driver_template);
    link->dev = NULL;
    
    CardServices(ReleaseConfiguration, link->handle);
    CardServices(ReleaseIO, link->handle, &link->io);
    CardServices(ReleaseIRQ, link->handle, &link->irq);
    
    link->state &= ~DEV_CONFIG;
    if (link->state & DEV_STALE_LINK)
	aha152x_detach(link);
    
} /* aha152x_release_cs */

/*====================================================================*/

static int aha152x_event(event_t event, int priority,
			 event_callback_args_t *args)
{
    dev_link_t *link = args->client_data;
    scsi_info_t *info = link->priv;
    
    DEBUG(0, "aha152x_event(0x%06x)\n", event);
    
    switch (event) {
    case CS_EVENT_CARD_REMOVAL:
	link->state &= ~DEV_PRESENT;
	if (link->state & DEV_CONFIG)
	    aha152x_release_cs((u_long)link);
	break;
    case CS_EVENT_CARD_INSERTION:
	link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
	aha152x_config_cs(link);
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
	if (link->state & DEV_CONFIG) {
	    Scsi_Cmnd tmp;
	    CardServices(RequestConfiguration, link->handle, &link->conf);
	    tmp.host = info->host;
	    aha152x_host_reset(&tmp);
	}
	break;
    }
    return 0;
} /* aha152x_event */

/*====================================================================*/

static int __init init_aha152x_cs(void) {
    servinfo_t serv;
    DEBUG(0, "%s\n", version);
    CardServices(GetCardServicesInfo, &serv);
    if (serv.Revision != CS_RELEASE_CODE) {
	printk(KERN_NOTICE "aha152x_cs: Card Services release "
	       "does not match!\n");
	return -1;
    }
    register_pccard_driver(&dev_info, &aha152x_attach, &aha152x_detach);
    return 0;
}

static void __exit exit_aha152x_cs(void) {
    DEBUG(0, "aha152x_cs: unloading\n");
    unregister_pccard_driver(&dev_info);
    while (dev_list != NULL)
	aha152x_detach(dev_list);
}

module_init(init_aha152x_cs);
module_exit(exit_aha152x_cs);
 
