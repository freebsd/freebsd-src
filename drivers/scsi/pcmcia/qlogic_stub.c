/*======================================================================

    A driver for the Qlogic SCSI card

    qlogic_cs.c 1.83 2001/10/13 00:08:53

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
#include <asm/io.h>
#include <asm/byteorder.h>
#include <scsi/scsi.h>
#include <linux/major.h>
#include <linux/blk.h>

#include <../drivers/scsi/scsi.h>
#include <../drivers/scsi/hosts.h>
#include <scsi/scsi_ioctl.h>

#include <../drivers/scsi/qlogicfas.h>

#define qlogic_reset(h) qlogicfas_reset(h, 0)

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>
#include <pcmcia/ciscode.h>

extern void qlogicfas_preset(int port, int irq);

/*====================================================================*/

/* Module parameters */

MODULE_AUTHOR("David Hinds <dahinds@users.sourceforge.net>");
MODULE_DESCRIPTION("Qlogic PCMCIA SCSI driver");
MODULE_LICENSE("Dual MPL/GPL");

#define INT_MODULE_PARM(n, v) static int n = v; MODULE_PARM(n, "i")

/* Bit map of interrupts to choose from */
INT_MODULE_PARM(irq_mask, 0xdeb8);
static int irq_list[4] = { -1 };
MODULE_PARM(irq_list, "1-4i");

#ifdef PCMCIA_DEBUG
INT_MODULE_PARM(pc_debug, PCMCIA_DEBUG);
#define DEBUG(n, args...) if (pc_debug>(n)) printk(KERN_DEBUG args)
static char *version =
"qlogic_cs.c 1.83 2001/10/13 00:08:53 (David Hinds)";
#else
#define DEBUG(n, args...)
#endif

/*====================================================================*/

typedef struct scsi_info_t {
    dev_link_t		link;
    u_short		manf_id;
    int			ndev;
    dev_node_t		node[8];
} scsi_info_t;

static void qlogic_release(u_long arg);
static int qlogic_event(event_t event, int priority,
			event_callback_args_t *args);

static dev_link_t *qlogic_attach(void);
static void qlogic_detach(dev_link_t *);

static Scsi_Host_Template driver_template = QLOGICFAS;

static dev_link_t *dev_list = NULL;

static dev_info_t dev_info = "qlogic_cs";

/*====================================================================*/

static void cs_error(client_handle_t handle, int func, int ret)
{
    error_info_t err = { func, ret };
    CardServices(ReportError, handle, &err);
}

/*====================================================================*/

static dev_link_t *qlogic_attach(void)
{
    scsi_info_t *info;
    client_reg_t client_reg;
    dev_link_t *link;
    int i, ret;
    
    DEBUG(0, "qlogic_attach()\n");

    /* Create new SCSI device */
    info = kmalloc(sizeof(*info), GFP_KERNEL);
    if (!info) return NULL;
    memset(info, 0, sizeof(*info));
    link = &info->link; link->priv = info;

    link->io.NumPorts1 = 16;
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
    client_reg.event_handler = &qlogic_event;
    client_reg.EventMask =
	CS_EVENT_RESET_REQUEST | CS_EVENT_CARD_RESET |
	CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
	CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
    client_reg.Version = 0x0210;
    client_reg.event_callback_args.client_data = link;
    ret = CardServices(RegisterClient, &link->handle, &client_reg);
    if (ret != 0) {
	cs_error(link->handle, RegisterClient, ret);
	qlogic_detach(link);
	return NULL;
    }
    
    return link;
} /* qlogic_attach */

/*====================================================================*/

static void qlogic_detach(dev_link_t *link)
{
    dev_link_t **linkp;

    DEBUG(0, "qlogic_detach(0x%p)\n", link);
    
    /* Locate device structure */
    for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
	if (*linkp == link) break;
    if (*linkp == NULL)
	return;

    if (link->state & DEV_CONFIG) {
	qlogic_release((u_long)link);
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
    
} /* qlogic_detach */

/*====================================================================*/

#define CS_CHECK(fn, args...) \
while ((last_ret=CardServices(last_fn=(fn), args))!=0) goto cs_failed

#define CFG_CHECK(fn, args...) \
if (CardServices(fn, args) != 0) goto next_entry

static void qlogic_config(dev_link_t *link)
{
    client_handle_t handle = link->handle;
    scsi_info_t *info = link->priv;
    tuple_t tuple;
    cisparse_t parse;
    int i, last_ret, last_fn;
    u_short tuple_data[32];
    Scsi_Device *dev;
    dev_node_t **tail, *node;
    struct Scsi_Host *host;

    DEBUG(0, "qlogic_config(0x%p)\n", link);

    tuple.TupleData = (cisdata_t *)tuple_data;
    tuple.TupleDataMax = 64;
    tuple.TupleOffset = 0;
    tuple.DesiredTuple = CISTPL_CONFIG;
    CS_CHECK(GetFirstTuple, handle, &tuple);
    CS_CHECK(GetTupleData, handle, &tuple);
    CS_CHECK(ParseTuple, handle, &tuple, &parse);
    link->conf.ConfigBase = parse.config.base;
    
    tuple.DesiredTuple = CISTPL_MANFID;
    if ((CardServices(GetFirstTuple, handle, &tuple) == CS_SUCCESS) &&
	(CardServices(GetTupleData, handle, &tuple) == CS_SUCCESS))
	info->manf_id = le16_to_cpu(tuple.TupleData[0]);

    /* Configure card */
    driver_template.module = &__this_module;
    link->state |= DEV_CONFIG;

    tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
    CS_CHECK(GetFirstTuple, handle, &tuple);
    while (1) {
	CFG_CHECK(GetTupleData, handle, &tuple);
	CFG_CHECK(ParseTuple, handle, &tuple, &parse);
	link->conf.ConfigIndex = parse.cftable_entry.index;
	link->io.BasePort1 = parse.cftable_entry.io.win[0].base;
	link->io.NumPorts1 = parse.cftable_entry.io.win[0].len;
	if (link->io.BasePort1 != 0) {
	    i = CardServices(RequestIO, handle, &link->io);
	    if (i == CS_SUCCESS) break;
	}
    next_entry:
	CS_CHECK(GetNextTuple, handle, &tuple);
    }
    
    CS_CHECK(RequestIRQ, handle, &link->irq);
    CS_CHECK(RequestConfiguration, handle, &link->conf);

    if ((info->manf_id == MANFID_MACNICA) ||
	(info->manf_id == MANFID_PIONEER) ||
	(info->manf_id == 0x0098)) {
	/* set ATAcmd */
	outb(0xb4, link->io.BasePort1+0xd);
	outb(0x24, link->io.BasePort1+0x9);
	outb(0x04, link->io.BasePort1+0xd);
    }

    /* A bad hack... */
    release_region(link->io.BasePort1, link->io.NumPorts1);

    /* The KXL-810AN has a bigger IO port window */
    if (link->io.NumPorts1 == 32)
	qlogicfas_preset(link->io.BasePort1+16, link->irq.AssignedIRQ);
    else
	qlogicfas_preset(link->io.BasePort1, link->irq.AssignedIRQ);
    
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
	}
    *tail = NULL;
    if (info->ndev == 0)
	printk(KERN_INFO "qlogic_cs: no SCSI devices found\n");
    
    link->state &= ~DEV_CONFIG_PENDING;
    return;

cs_failed:
    cs_error(link->handle, last_fn, last_ret);
    qlogic_release((u_long)link);
    return;

} /* qlogic_config */

/*====================================================================*/

static void qlogic_release(u_long arg)
{
    dev_link_t *link = (dev_link_t *)arg;

    DEBUG(0, "qlogic_release(0x%p)\n", link);

    if (GET_USE_COUNT(&__this_module) != 0) {
	DEBUG(0, "qlogic_cs: release postponed, device still open\n");
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
	qlogic_detach(link);
    
} /* qlogic_release */

/*====================================================================*/

static int qlogic_event(event_t event, int priority,
			event_callback_args_t *args)
{
    dev_link_t *link = args->client_data;

    DEBUG(1, "qlogic_event(0x%06x)\n", event);
    
    switch (event) {
    case CS_EVENT_CARD_REMOVAL:
	link->state &= ~DEV_PRESENT;
	if (link->state & DEV_CONFIG)
	    qlogic_release((u_long)link);
	break;
    case CS_EVENT_CARD_INSERTION:
	link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
	qlogic_config(link);
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
	    scsi_info_t *info = link->priv;
	    CardServices(RequestConfiguration, link->handle, &link->conf);
	    if ((info->manf_id == MANFID_MACNICA) ||
		(info->manf_id == MANFID_PIONEER) ||
		(info->manf_id == 0x0098)) {
		outb( 0x80, link->io.BasePort1+0xd);
		outb( 0x24, link->io.BasePort1+0x9);
		outb( 0x04, link->io.BasePort1+0xd);
	    }
	    qlogic_reset(NULL);
	}
	break;
    }
    return 0;
} /* qlogic_event */

/*====================================================================*/

static int __init init_qlogic_cs(void) {
    servinfo_t serv;
    DEBUG(0, "%s\n", version);
    CardServices(GetCardServicesInfo, &serv);
    if (serv.Revision != CS_RELEASE_CODE) {
	printk(KERN_NOTICE "qlogic_cs: Card Services release "
	       "does not match!\n");
	return -1;
    }
    register_pccard_driver(&dev_info, &qlogic_attach, &qlogic_detach);
    return 0;
}

static void __exit exit_qlogic_cs(void) {
    DEBUG(0, "qlogic_cs: unloading\n");
    unregister_pccard_driver(&dev_info);
    while (dev_list != NULL)
	qlogic_detach(dev_list);
}

module_init(init_qlogic_cs);
module_exit(exit_qlogic_cs);
