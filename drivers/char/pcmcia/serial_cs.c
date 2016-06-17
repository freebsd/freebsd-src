/*======================================================================

    A driver for PCMCIA serial devices

    serial_cs.c 1.138 2002/10/25 06:24:52

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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/byteorder.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ciscode.h>
#include <pcmcia/ds.h>
#include <pcmcia/cisreg.h>

/*====================================================================*/

/* Module parameters */

MODULE_AUTHOR("David Hinds <dahinds@users.sourceforge.net>");
MODULE_DESCRIPTION("PCMCIA serial card driver");
MODULE_LICENSE("Dual MPL/GPL");

#define INT_MODULE_PARM(n, v) static int n = v; MODULE_PARM(n, "i")

/* Bit map of interrupts to choose from */
INT_MODULE_PARM(irq_mask, 0xdeb8);
static int irq_list[4] = { -1 };
MODULE_PARM(irq_list, "1-4i");

INT_MODULE_PARM(do_sound, 1);		/* Enable the speaker? */
INT_MODULE_PARM(buggy_uart, 0);		/* Skip strict UART tests? */

#ifdef PCMCIA_DEBUG
INT_MODULE_PARM(pc_debug, PCMCIA_DEBUG);
#define DEBUG(n, args...) if (pc_debug>(n)) printk(KERN_DEBUG args)
static char *version =
"serial_cs.c 1.138 2002/10/25 06:24:52 (David Hinds)";
#else
#define DEBUG(n, args...)
#endif

/*====================================================================*/

/* Table of multi-port card ID's */

typedef struct {
    u_short	manfid;
    u_short	prodid;
    int		multi;		/* 1 = multifunction, > 1 = # ports */
} multi_id_t;

static multi_id_t multi_id[] = {
    { MANFID_OMEGA, PRODID_OMEGA_QSP_100, 4 },
    { MANFID_QUATECH, PRODID_QUATECH_DUAL_RS232, 2 },
    { MANFID_QUATECH, PRODID_QUATECH_DUAL_RS232_D1, 2 },
    { MANFID_QUATECH, PRODID_QUATECH_DUAL_RS232_D2, 2 },
    { MANFID_QUATECH, PRODID_QUATECH_QUAD_RS232, 4 },
    { MANFID_QUATECH, PRODID_QUATECH_DUAL_RS422, 2 },
    { MANFID_QUATECH, PRODID_QUATECH_QUAD_RS422, 4 },
    { MANFID_SOCKET, PRODID_SOCKET_DUAL_RS232, 2 },
    { MANFID_INTEL, PRODID_INTEL_DUAL_RS232, 2 },
    { MANFID_NATINST, PRODID_NATINST_QUAD_RS232, 4 }
};
#define MULTI_COUNT (sizeof(multi_id)/sizeof(multi_id_t))

typedef struct serial_info_t {
    dev_link_t	link;
    int		ndev;
    int		multi;
    int		slave;
    int		manfid;
    dev_node_t	node[4];
    int		line[4];
} serial_info_t;

static void serial_config(dev_link_t *link);
static void serial_release(u_long arg);
static int serial_event(event_t event, int priority,
			event_callback_args_t *args);

static dev_info_t dev_info = "serial_cs";

static dev_link_t *serial_attach(void);
static void serial_detach(dev_link_t *);

static dev_link_t *dev_list = NULL;

/*====================================================================*/

static void cs_error(client_handle_t handle, int func, int ret)
{
    error_info_t err = { func, ret };
    CardServices(ReportError, handle, &err);
}

/*======================================================================

    serial_attach() creates an "instance" of the driver, allocating
    local data structures for one device.  The device is registered
    with Card Services.

======================================================================*/

static dev_link_t *serial_attach(void)
{
    serial_info_t *info;
    client_reg_t client_reg;
    dev_link_t *link;
    int i, ret;

    DEBUG(0, "serial_attach()\n");

    /* Create new serial device */
    info = kmalloc(sizeof(*info), GFP_KERNEL);
    if (!info) return NULL;
    memset(info, 0, sizeof(*info));
    link = &info->link; link->priv = info;

    link->release.function = &serial_release;
    link->release.data = (u_long)link;
    link->io.Attributes1 = IO_DATA_PATH_WIDTH_8;
    link->io.Attributes2 = IO_DATA_PATH_WIDTH_8;
    link->irq.Attributes = IRQ_TYPE_EXCLUSIVE;
    link->irq.IRQInfo1 = IRQ_INFO2_VALID|IRQ_LEVEL_ID;
    if (irq_list[0] == -1)
	link->irq.IRQInfo2 = irq_mask;
    else
	for (i = 0; i < 4; i++)
	    link->irq.IRQInfo2 |= 1 << irq_list[i];
    link->conf.Attributes = CONF_ENABLE_IRQ;
    if (do_sound) {
	link->conf.Attributes |= CONF_ENABLE_SPKR;
	link->conf.Status = CCSR_AUDIO_ENA;
    }
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
    client_reg.event_handler = &serial_event;
    client_reg.Version = 0x0210;
    client_reg.event_callback_args.client_data = link;
    ret = CardServices(RegisterClient, &link->handle, &client_reg);
    if (ret != CS_SUCCESS) {
	cs_error(link->handle, RegisterClient, ret);
	serial_detach(link);
	return NULL;
    }

    return link;
} /* serial_attach */

/*======================================================================

    This deletes a driver "instance".  The device is de-registered
    with Card Services.  If it has been released, all local data
    structures are freed.  Otherwise, the structures will be freed
    when the device is released.

======================================================================*/

static void serial_detach(dev_link_t *link)
{
    serial_info_t *info = link->priv;
    dev_link_t **linkp;
    int ret;

    DEBUG(0, "serial_detach(0x%p)\n", link);

    /* Locate device structure */
    for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
	if (*linkp == link) break;
    if (*linkp == NULL)
	return;

    del_timer(&link->release);
    if (link->state & DEV_CONFIG)
	serial_release((u_long)link);

    if (link->handle) {
	ret = CardServices(DeregisterClient, link->handle);
	if (ret != CS_SUCCESS)
	    cs_error(link->handle, DeregisterClient, ret);
    }

    /* Unlink device structure, free bits */
    *linkp = link->next;
    kfree(info);

} /* serial_detach */

/*====================================================================*/

static int setup_serial(serial_info_t *info, ioaddr_t port, int irq)
{
    struct serial_struct serial;
    int line;

    memset(&serial, 0, sizeof(serial));
    serial.port = port;
    serial.irq = irq;
    serial.flags = ASYNC_SKIP_TEST | ASYNC_SHARE_IRQ;
    if (buggy_uart)
	serial.flags |= ASYNC_BUGGY_UART;
    line = register_serial(&serial);
    if (line < 0) {
	printk(KERN_NOTICE "serial_cs: register_serial() at 0x%04lx,"
	       " irq %d failed\n", (u_long)serial.port, serial.irq);
	return -1;
    }

    info->line[info->ndev] = line;
    sprintf(info->node[info->ndev].dev_name, "ttyS%d", line);
    info->node[info->ndev].major = TTY_MAJOR;
    info->node[info->ndev].minor = 0x40+line;
    if (info->ndev > 0)
	info->node[info->ndev-1].next = &info->node[info->ndev];
    info->ndev++;

    return 0;
}

/*====================================================================*/

static int get_tuple(int fn, client_handle_t handle, tuple_t *tuple,
		     cisparse_t *parse)
{
    int i;
    i = CardServices(fn, handle, tuple);
    if (i != CS_SUCCESS) return CS_NO_MORE_ITEMS;
    i = CardServices(GetTupleData, handle, tuple);
    if (i != CS_SUCCESS) return i;
    return CardServices(ParseTuple, handle, tuple, parse);
}

#define first_tuple(a, b, c) get_tuple(GetFirstTuple, a, b, c)
#define next_tuple(a, b, c) get_tuple(GetNextTuple, a, b, c)

/*====================================================================*/

static int simple_config(dev_link_t *link)
{
    static ioaddr_t base[5] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8, 0x0 };
    client_handle_t handle = link->handle;
    serial_info_t *info = link->priv;
    tuple_t tuple;
    u_char buf[256];
    cisparse_t parse;
    cistpl_cftable_entry_t *cf = &parse.cftable_entry;
    config_info_t config;
    int i, j, try;

    /* If the card is already configured, look up the port and irq */
    i = CardServices(GetConfigurationInfo, handle, &config);
    if ((i == CS_SUCCESS) &&
	(config.Attributes & CONF_VALID_CLIENT)) {
	ioaddr_t port = 0;
	if ((config.BasePort2 != 0) && (config.NumPorts2 == 8)) {
	    port = config.BasePort2;
	    info->slave = 1;
	} else if ((info->manfid == MANFID_OSITECH) &&
		   (config.NumPorts1 == 0x40)) {
	    port = config.BasePort1 + 0x28;
	    info->slave = 1;
	}
	if (info->slave)
	    return setup_serial(info, port, config.AssignedIRQ);
    }
    link->conf.Vcc = config.Vcc;

    link->io.NumPorts1 = 8;
    link->io.NumPorts2 = 0;

    /* First pass: look for a config entry that looks normal. */
    tuple.TupleData = (cisdata_t *)buf;
    tuple.TupleOffset = 0; tuple.TupleDataMax = 255;
    tuple.Attributes = 0;
    tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
    /* Two tries: without IO aliases, then with aliases */
    for (try = 0; try < 2; try++) {
	i = first_tuple(handle, &tuple, &parse);
	while (i != CS_NO_MORE_ITEMS) {
	    if (i != CS_SUCCESS) goto next_entry;
	    if (cf->vpp1.present & (1<<CISTPL_POWER_VNOM))
		link->conf.Vpp1 = link->conf.Vpp2 =
		    cf->vpp1.param[CISTPL_POWER_VNOM]/10000;
	    if ((cf->io.nwin > 0) && (cf->io.win[0].len == 8) &&
		(cf->io.win[0].base != 0)) {
		link->conf.ConfigIndex = cf->index;
		link->io.BasePort1 = cf->io.win[0].base;
		link->io.IOAddrLines = (try == 0) ?
		    16 : cf->io.flags & CISTPL_IO_LINES_MASK;
		i = CardServices(RequestIO, link->handle, &link->io);
		if (i == CS_SUCCESS) goto found_port;
	    }
	next_entry:
	    i = next_tuple(handle, &tuple, &parse);
	}
    }

    /* Second pass: try to find an entry that isn't picky about
       its base address, then try to grab any standard serial port
       address, and finally try to get any free port. */
    i = first_tuple(handle, &tuple, &parse);
    while (i != CS_NO_MORE_ITEMS) {
	if ((i == CS_SUCCESS) && (cf->io.nwin > 0) &&
	    ((cf->io.flags & CISTPL_IO_LINES_MASK) <= 3)) {
	    link->conf.ConfigIndex = cf->index;
	    for (j = 0; j < 5; j++) {
		link->io.BasePort1 = base[j];
		link->io.IOAddrLines = base[j] ? 16 : 3;
		i = CardServices(RequestIO, link->handle, &link->io);
		if (i == CS_SUCCESS) goto found_port;
	    }
	}
	i = next_tuple(handle, &tuple, &parse);
    }

found_port:
    if (i != CS_SUCCESS) {
	cs_error(link->handle, RequestIO, i);
	return -1;
    }

    i = CardServices(RequestIRQ, link->handle, &link->irq);
    if (i != CS_SUCCESS) {
	cs_error(link->handle, RequestIRQ, i);
	link->irq.AssignedIRQ = 0;
    }
    if (info->multi && (info->manfid == MANFID_3COM))
	link->conf.ConfigIndex &= ~(0x08);
    i = CardServices(RequestConfiguration, link->handle, &link->conf);
    if (i != CS_SUCCESS) {
	cs_error(link->handle, RequestConfiguration, i);
	return -1;
    }

    return setup_serial(info, link->io.BasePort1, link->irq.AssignedIRQ);
}

static int multi_config(dev_link_t *link)
{
    client_handle_t handle = link->handle;
    serial_info_t *info = link->priv;
    tuple_t tuple;
    u_char buf[256];
    cisparse_t parse;
    cistpl_cftable_entry_t *cf = &parse.cftable_entry;
    config_info_t config;
    int i, base2 = 0;

    CardServices(GetConfigurationInfo, handle, &config);
    link->conf.Vcc = config.Vcc;

    tuple.TupleData = (cisdata_t *)buf;
    tuple.TupleOffset = 0; tuple.TupleDataMax = 255;
    tuple.Attributes = 0;
    tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;

    /* First, look for a generic full-sized window */
    link->io.NumPorts1 = info->multi * 8;
    i = first_tuple(handle, &tuple, &parse);
    while (i != CS_NO_MORE_ITEMS) {
	/* The quad port cards have bad CIS's, so just look for a
	   window larger than 8 ports and assume it will be right */
	if ((i == CS_SUCCESS) && (cf->io.nwin == 1) &&
	    (cf->io.win[0].len > 8)) {
	    link->conf.ConfigIndex = cf->index;
	    link->io.BasePort1 = cf->io.win[0].base;
	    link->io.IOAddrLines = cf->io.flags & CISTPL_IO_LINES_MASK;
	    i = CardServices(RequestIO, link->handle, &link->io);
	    base2 = link->io.BasePort1 + 8;
	    if (i == CS_SUCCESS) break;
	}
	i = next_tuple(handle, &tuple, &parse);
    }

    /* If that didn't work, look for two windows */
    if (i != CS_SUCCESS) {
	link->io.NumPorts1 = link->io.NumPorts2 = 8;
	info->multi = 2;
	i = first_tuple(handle, &tuple, &parse);
	while (i != CS_NO_MORE_ITEMS) {
	    if ((i == CS_SUCCESS) && (cf->io.nwin == 2)) {
		link->conf.ConfigIndex = cf->index;
		link->io.BasePort1 = cf->io.win[0].base;
		link->io.BasePort2 = cf->io.win[1].base;
		link->io.IOAddrLines = cf->io.flags & CISTPL_IO_LINES_MASK;
		i = CardServices(RequestIO, link->handle, &link->io);
		base2 = link->io.BasePort2;
		if (i == CS_SUCCESS) break;
	    }
	    i = next_tuple(handle, &tuple, &parse);
	}
    }

    if (i != CS_SUCCESS) {
	/* At worst, try to configure as a single port */
	return simple_config(link);
    }

    i = CardServices(RequestIRQ, link->handle, &link->irq);
    if (i != CS_SUCCESS) {
	cs_error(link->handle, RequestIRQ, i);
	link->irq.AssignedIRQ = 0;
    }
    /* Socket Dual IO: this enables irq's for second port */
    if (info->multi && (info->manfid == MANFID_SOCKET)) {
	link->conf.Present |= PRESENT_EXT_STATUS;
	link->conf.ExtStatus = ESR_REQ_ATTN_ENA;
    }
    i = CardServices(RequestConfiguration, link->handle, &link->conf);
    if (i != CS_SUCCESS) {
	cs_error(link->handle, RequestConfiguration, i);
	return -1;
    }

    /* The Oxford Semiconductor OXCF950 cards are in fact single-port:
       8 registers are for the UART, the others are extra registers */
    if (info->manfid == MANFID_OXSEMI) {
	if (cf->index == 1 || cf->index == 3) {
	    setup_serial(info, base2, link->irq.AssignedIRQ);
	    outb(12,link->io.BasePort1+1);
	} else {
	    setup_serial(info, link->io.BasePort1, link->irq.AssignedIRQ);
	    outb(12,base2+1);
	}
	return 0;
    }

    setup_serial(info, link->io.BasePort1, link->irq.AssignedIRQ);
    /* The Nokia cards are not really multiport cards */
    if (info->manfid == MANFID_NOKIA)
	return 0;
    for (i = 0; i < info->multi-1; i++)
	setup_serial(info, base2+(8*i), link->irq.AssignedIRQ);

    return 0;
}

/*======================================================================

    serial_config() is scheduled to run after a CARD_INSERTION event
    is received, to configure the PCMCIA socket, and to make the
    serial device available to the system.

======================================================================*/

#define CS_CHECK(fn, args...) \
while ((last_ret=CardServices(last_fn=(fn), args))!=0) goto cs_failed

void serial_config(dev_link_t *link)
{
    client_handle_t handle = link->handle;
    serial_info_t *info = link->priv;
    tuple_t tuple;
    u_short buf[128];
    cisparse_t parse;
    cistpl_cftable_entry_t *cf = &parse.cftable_entry;
    int i, last_ret, last_fn;

    DEBUG(0, "serial_config(0x%p)\n", link);

    tuple.TupleData = (cisdata_t *)buf;
    tuple.TupleOffset = 0; tuple.TupleDataMax = 255;
    tuple.Attributes = 0;
    /* Get configuration register information */
    tuple.DesiredTuple = CISTPL_CONFIG;
    last_ret = first_tuple(handle, &tuple, &parse);
    if (last_ret != CS_SUCCESS) {
	last_fn = ParseTuple;
	goto cs_failed;
    }
    link->conf.ConfigBase = parse.config.base;
    link->conf.Present = parse.config.rmask[0];

    /* Configure card */
    link->state |= DEV_CONFIG;

    /* Is this a compliant multifunction card? */
    tuple.DesiredTuple = CISTPL_LONGLINK_MFC;
    tuple.Attributes = TUPLE_RETURN_COMMON | TUPLE_RETURN_LINK;
    info->multi = (first_tuple(handle, &tuple, &parse) == CS_SUCCESS);

    /* Scan list of known multiport card ID's */
    tuple.DesiredTuple = CISTPL_MANFID;
    if (first_tuple(handle, &tuple, &parse) == CS_SUCCESS) {
	info->manfid = le16_to_cpu(buf[0]);
	for (i = 0; i < MULTI_COUNT; i++)
	    if ((info->manfid == multi_id[i].manfid) &&
		(le16_to_cpu(buf[1]) == multi_id[i].prodid))
		break;
	if (i < MULTI_COUNT)
	    info->multi = multi_id[i].multi;
    }

    /* Another check for dual-serial cards: look for either serial or
       multifunction cards that ask for appropriate IO port ranges */
    tuple.DesiredTuple = CISTPL_FUNCID;
    if ((info->multi == 0) &&
	((first_tuple(handle, &tuple, &parse) != CS_SUCCESS) ||
	 (parse.funcid.func == CISTPL_FUNCID_MULTI) ||
	 (parse.funcid.func == CISTPL_FUNCID_SERIAL))) {
	tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
	if (first_tuple(handle, &tuple, &parse) == CS_SUCCESS) {
	    if ((cf->io.nwin == 1) && (cf->io.win[0].len % 8 == 0))
		info->multi = cf->io.win[0].len >> 3;
	    if ((cf->io.nwin == 2) && (cf->io.win[0].len == 8) &&
		(cf->io.win[1].len == 8))
		info->multi = 2;
	}
    }

    if (info->multi > 1)
	multi_config(link);
    else
	simple_config(link);

    if (info->ndev == 0)
	goto failed;

    if (info->manfid == MANFID_IBM) {
	conf_reg_t reg = { 0, CS_READ, 0x800, 0 };
	CS_CHECK(AccessConfigurationRegister, link->handle, &reg);
	reg.Action = CS_WRITE;
	reg.Value = reg.Value | 1;
	CS_CHECK(AccessConfigurationRegister, link->handle, &reg);
    }

    link->dev = &info->node[0];
    link->state &= ~DEV_CONFIG_PENDING;
    return;

cs_failed:
    cs_error(link->handle, last_fn, last_ret);
failed:
    serial_release((u_long)link);
    link->state &= ~DEV_CONFIG_PENDING;

} /* serial_config */

/*======================================================================

    After a card is removed, serial_release() will unregister the net
    device, and release the PCMCIA configuration.

======================================================================*/

void serial_release(u_long arg)
{
    dev_link_t *link = (dev_link_t *)arg;
    serial_info_t *info = link->priv;
    int i;

    DEBUG(0, "serial_release(0x%p)\n", link);

    for (i = 0; i < info->ndev; i++) {
	unregister_serial(info->line[i]);
    }
    link->dev = NULL;

    if (!info->slave) {
	CardServices(ReleaseConfiguration, link->handle);
	CardServices(ReleaseIO, link->handle, &link->io);
	CardServices(ReleaseIRQ, link->handle, &link->irq);
    }

    link->state &= ~DEV_CONFIG;

} /* serial_release */

/*======================================================================

    The card status event handler.  Mostly, this schedules other
    stuff to run after an event is received.  A CARD_REMOVAL event
    also sets some flags to discourage the serial drivers from
    talking to the ports.

======================================================================*/

static int serial_event(event_t event, int priority,
			event_callback_args_t *args)
{
    dev_link_t *link = args->client_data;
    serial_info_t *info = link->priv;

    DEBUG(1, "serial_event(0x%06x)\n", event);

    switch (event) {
    case CS_EVENT_CARD_REMOVAL:
	link->state &= ~DEV_PRESENT;
	if (link->state & DEV_CONFIG)
	    mod_timer(&link->release, jiffies + HZ/20);
	break;
    case CS_EVENT_CARD_INSERTION:
	link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
	serial_config(link);
	break;
    case CS_EVENT_PM_SUSPEND:
	link->state |= DEV_SUSPEND;
	/* Fall through... */
    case CS_EVENT_RESET_PHYSICAL:
	if ((link->state & DEV_CONFIG) && !info->slave)
	    CardServices(ReleaseConfiguration, link->handle);
	break;
    case CS_EVENT_PM_RESUME:
	link->state &= ~DEV_SUSPEND;
	/* Fall through... */
    case CS_EVENT_CARD_RESET:
	if (DEV_OK(link) && !info->slave)
	    CardServices(RequestConfiguration, link->handle, &link->conf);
	break;
    }
    return 0;
} /* serial_event */

/*====================================================================*/

static int __init init_serial_cs(void)
{
    servinfo_t serv;
    DEBUG(0, "%s\n", version);
    CardServices(GetCardServicesInfo, &serv);
    if (serv.Revision != CS_RELEASE_CODE) {
	printk(KERN_NOTICE "serial_cs: Card Services release "
	       "does not match!\n");
	return -EINVAL;
    }
    register_pccard_driver(&dev_info, &serial_attach, &serial_detach);
    return 0;
}

static void __exit exit_serial_cs(void)
{
    DEBUG(0, "serial_cs: unloading\n");
    unregister_pccard_driver(&dev_info);
    while (dev_list != NULL)
	serial_detach(dev_list);
}

module_init(init_serial_cs);
module_exit(exit_serial_cs);
