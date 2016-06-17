/*
 *
 *  Driver for the 3Com Bluetooth PCMCIA card
 *
 *  Copyright (C) 2001-2002  Marcel Holtmann <marcel@holtmann.org>
 *                           Jose Orlando Pereira <jop@di.uminho.pt>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation;
 *
 *  Software distributed under the License is distributed on an "AS
 *  IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *  implied. See the License for the specific language governing
 *  rights and limitations under the License.
 *
 *  The initial developer of the original code is David A. Hinds
 *  <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
 *  are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.
 *
 */

#include <linux/config.h>
#include <linux/module.h>

#define __KERNEL_SYSCALLS__

#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>

#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ciscode.h>
#include <pcmcia/ds.h>
#include <pcmcia/cisreg.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>



/* ======================== Module parameters ======================== */


/* Bit map of interrupts to choose from */
static u_int irq_mask = 0xffff;
static int irq_list[4] = { -1 };

MODULE_PARM(irq_mask, "i");
MODULE_PARM(irq_list, "1-4i");

MODULE_AUTHOR("Marcel Holtmann <marcel@holtmann.org>, Jose Orlando Pereira <jop@di.uminho.pt>");
MODULE_DESCRIPTION("BlueZ driver for the 3Com Bluetooth PCMCIA card");
MODULE_LICENSE("GPL");



/* ======================== Local structures ======================== */


typedef struct bt3c_info_t {
	dev_link_t link;
	dev_node_t node;

	struct hci_dev hdev;

	spinlock_t lock;		/* For serializing operations */

	struct sk_buff_head txq;
	unsigned long tx_state;

	unsigned long rx_state;
	unsigned long rx_count;
	struct sk_buff *rx_skb;
} bt3c_info_t;


void bt3c_config(dev_link_t *link);
void bt3c_release(u_long arg);
int bt3c_event(event_t event, int priority, event_callback_args_t *args);

static dev_info_t dev_info = "bt3c_cs";

dev_link_t *bt3c_attach(void);
void bt3c_detach(dev_link_t *);

static dev_link_t *dev_list = NULL;


/* Transmit states  */
#define XMIT_SENDING  1
#define XMIT_WAKEUP   2
#define XMIT_WAITING  8

/* Receiver states */
#define RECV_WAIT_PACKET_TYPE   0
#define RECV_WAIT_EVENT_HEADER  1
#define RECV_WAIT_ACL_HEADER    2
#define RECV_WAIT_SCO_HEADER    3
#define RECV_WAIT_DATA          4



/* ======================== Special I/O functions ======================== */


#define DATA_L   0
#define DATA_H   1
#define ADDR_L   2
#define ADDR_H   3
#define CONTROL  4


inline void bt3c_address(unsigned int iobase, unsigned short addr)
{
	outb(addr & 0xff, iobase + ADDR_L);
	outb((addr >> 8) & 0xff, iobase + ADDR_H);
}


inline void bt3c_put(unsigned int iobase, unsigned short value)
{
	outb(value & 0xff, iobase + DATA_L);
	outb((value >> 8) & 0xff, iobase + DATA_H);
}


inline void bt3c_io_write(unsigned int iobase, unsigned short addr, unsigned short value)
{
	bt3c_address(iobase, addr);
	bt3c_put(iobase, value);
}


inline unsigned short bt3c_get(unsigned int iobase)
{
	unsigned short value = inb(iobase + DATA_L);

	value |= inb(iobase + DATA_H) << 8;

	return value;
}


inline unsigned short bt3c_read(unsigned int iobase, unsigned short addr)
{
	bt3c_address(iobase, addr);

	return bt3c_get(iobase);
}



/* ======================== Interrupt handling ======================== */


static int bt3c_write(unsigned int iobase, int fifo_size, __u8 *buf, int len)
{
	int actual = 0;

	bt3c_address(iobase, 0x7080);

	/* Fill FIFO with current frame */
	while (actual < len) {
		/* Transmit next byte */
		bt3c_put(iobase, buf[actual]);
		actual++;
	}

	bt3c_io_write(iobase, 0x7005, actual);

	return actual;
}


static void bt3c_write_wakeup(bt3c_info_t *info, int from)
{
	unsigned long flags;

	if (!info) {
		printk(KERN_WARNING "bt3c_cs: Call of write_wakeup for unknown device.\n");
		return;
	}

	if (test_and_set_bit(XMIT_SENDING, &(info->tx_state)))
		return;

	spin_lock_irqsave(&(info->lock), flags);

	do {
		register unsigned int iobase = info->link.io.BasePort1;
		register struct sk_buff *skb;
		register int len;

		if (!(info->link.state & DEV_PRESENT))
			break;


		if (!(skb = skb_dequeue(&(info->txq)))) {
			clear_bit(XMIT_SENDING, &(info->tx_state));
			break;
		}

		/* Send frame */
		len = bt3c_write(iobase, 256, skb->data, skb->len);

		if (len != skb->len) {
			printk(KERN_WARNING "bt3c_cs: very strange\n");
		}

		kfree_skb(skb);

		info->hdev.stat.byte_tx += len;

	} while (0);

	spin_unlock_irqrestore(&(info->lock), flags);
}


static void bt3c_receive(bt3c_info_t *info)
{
	unsigned int iobase;
	int size = 0, avail;

	if (!info) {
		printk(KERN_WARNING "bt3c_cs: Call of receive for unknown device.\n");
		return;
	}

	iobase = info->link.io.BasePort1;

	avail = bt3c_read(iobase, 0x7006);
	//printk("bt3c_cs: receiving %d bytes\n", avail);

	bt3c_address(iobase, 0x7480);
	while (size < avail) {
		size++;
		info->hdev.stat.byte_rx++;

		/* Allocate packet */
		if (info->rx_skb == NULL) {
			info->rx_state = RECV_WAIT_PACKET_TYPE;
			info->rx_count = 0;
			if (!(info->rx_skb = bluez_skb_alloc(HCI_MAX_FRAME_SIZE, GFP_ATOMIC))) {
				printk(KERN_WARNING "bt3c_cs: Can't allocate mem for new packet.\n");
				return;
			}
		}


		if (info->rx_state == RECV_WAIT_PACKET_TYPE) {

			info->rx_skb->dev = (void *)&(info->hdev);
			info->rx_skb->pkt_type = inb(iobase + DATA_L);
			inb(iobase + DATA_H);
			//printk("bt3c: PACKET_TYPE=%02x\n", info->rx_skb->pkt_type);

			switch (info->rx_skb->pkt_type) {

			case HCI_EVENT_PKT:
				info->rx_state = RECV_WAIT_EVENT_HEADER;
				info->rx_count = HCI_EVENT_HDR_SIZE;
				break;

			case HCI_ACLDATA_PKT:
				info->rx_state = RECV_WAIT_ACL_HEADER;
				info->rx_count = HCI_ACL_HDR_SIZE;
				break;

			case HCI_SCODATA_PKT:
				info->rx_state = RECV_WAIT_SCO_HEADER;
				info->rx_count = HCI_SCO_HDR_SIZE;
				break;

			default:
				/* Unknown packet */
				printk(KERN_WARNING "bt3c_cs: Unknown HCI packet with type 0x%02x received.\n", info->rx_skb->pkt_type);
				info->hdev.stat.err_rx++;
				clear_bit(HCI_RUNNING, &(info->hdev.flags));

				kfree_skb(info->rx_skb);
				info->rx_skb = NULL;
				break;

			}

		} else {

			__u8 x = inb(iobase + DATA_L);

			*skb_put(info->rx_skb, 1) = x;
			inb(iobase + DATA_H);
			info->rx_count--;

			if (info->rx_count == 0) {

				int dlen;
				hci_event_hdr *eh;
				hci_acl_hdr *ah;
				hci_sco_hdr *sh;

				switch (info->rx_state) {

				case RECV_WAIT_EVENT_HEADER:
					eh = (hci_event_hdr *)(info->rx_skb->data);
					info->rx_state = RECV_WAIT_DATA;
					info->rx_count = eh->plen;
					break;

				case RECV_WAIT_ACL_HEADER:
					ah = (hci_acl_hdr *)(info->rx_skb->data);
					dlen = __le16_to_cpu(ah->dlen);
					info->rx_state = RECV_WAIT_DATA;
					info->rx_count = dlen;
					break;

				case RECV_WAIT_SCO_HEADER:
					sh = (hci_sco_hdr *)(info->rx_skb->data);
					info->rx_state = RECV_WAIT_DATA;
					info->rx_count = sh->dlen;
					break;

				case RECV_WAIT_DATA:
					hci_recv_frame(info->rx_skb);
					info->rx_skb = NULL;
					break;

				}

			}

		}

	}

	bt3c_io_write(iobase, 0x7006, 0x0000);
}


void bt3c_interrupt(int irq, void *dev_inst, struct pt_regs *regs)
{
	bt3c_info_t *info = dev_inst;
	unsigned int iobase;
	int iir;

	if (!info) {
		printk(KERN_WARNING "bt3c_cs: Call of irq %d for unknown device.\n", irq);
		return;
	}

	iobase = info->link.io.BasePort1;

	spin_lock(&(info->lock));

	iir = inb(iobase + CONTROL);
	if (iir & 0x80) {
		int stat = bt3c_read(iobase, 0x7001);

		if ((stat & 0xff) == 0x7f) {
			printk(KERN_WARNING "bt3c_cs: STRANGE stat=%04x\n", stat);
		} else if ((stat & 0xff) != 0xff) {
			if (stat & 0x0020) {
				int stat = bt3c_read(iobase, 0x7002) & 0x10;
				printk(KERN_WARNING "bt3c_cs: antena %s\n", stat ? "OUT" : "IN");
			}
			if (stat & 0x0001)
				bt3c_receive(info);
			if (stat & 0x0002) {
				//printk("bt3c_cs: ACK %04x\n", stat);
				clear_bit(XMIT_SENDING, &(info->tx_state));
				bt3c_write_wakeup(info, 1);
			}

			bt3c_io_write(iobase, 0x7001, 0x0000);

			outb(iir, iobase + CONTROL);
		}
	}

	spin_unlock(&(info->lock));
}




/* ======================== HCI interface ======================== */


static int bt3c_hci_flush(struct hci_dev *hdev)
{
	bt3c_info_t *info = (bt3c_info_t *)(hdev->driver_data);

	/* Drop TX queue */
	skb_queue_purge(&(info->txq));

	return 0;
}


static int bt3c_hci_open(struct hci_dev *hdev)
{
	set_bit(HCI_RUNNING, &(hdev->flags));

	return 0;
}


static int bt3c_hci_close(struct hci_dev *hdev)
{
	if (!test_and_clear_bit(HCI_RUNNING, &(hdev->flags)))
		return 0;

	bt3c_hci_flush(hdev);

	return 0;
}


static int bt3c_hci_send_frame(struct sk_buff *skb)
{
	bt3c_info_t *info;
	struct hci_dev *hdev = (struct hci_dev *)(skb->dev);

	if (!hdev) {
		printk(KERN_WARNING "bt3c_cs: Frame for unknown HCI device (hdev=NULL).");
		return -ENODEV;
	}

	info = (bt3c_info_t *) (hdev->driver_data);

	switch (skb->pkt_type) {
	case HCI_COMMAND_PKT:
		hdev->stat.cmd_tx++;
		break;
	case HCI_ACLDATA_PKT:
		hdev->stat.acl_tx++;
		break;
	case HCI_SCODATA_PKT:
		hdev->stat.sco_tx++;
		break;
	};

	/* Prepend skb with frame type */
	memcpy(skb_push(skb, 1), &(skb->pkt_type), 1);
	skb_queue_tail(&(info->txq), skb);

	bt3c_write_wakeup(info, 0);

	return 0;
}


static void bt3c_hci_destruct(struct hci_dev *hdev)
{
}


static int bt3c_hci_ioctl(struct hci_dev *hdev, unsigned int cmd, unsigned long arg)
{
	return -ENOIOCTLCMD;
}



/* ======================== User mode firmware loader ======================== */


#define FW_LOADER  "/sbin/bluefw"
static int errno;


static int bt3c_fw_loader_exec(void *dev)
{
	char *argv[] = { FW_LOADER, "pccard", dev, NULL };
	char *envp[] = { "HOME=/", "TERM=linux", "PATH=/sbin:/usr/sbin:/bin:/usr/bin", NULL };
	int err;

	err = exec_usermodehelper(FW_LOADER, argv, envp);
	if (err)
		printk(KERN_WARNING "bt3c_cs: Failed to exec \"%s pccard %s\".\n", FW_LOADER, (char *)dev);

	return err;
}


static int bt3c_firmware_load(bt3c_info_t *info)
{
	sigset_t tmpsig;
	char dev[16];
	pid_t pid;
	int result;

	/* Check if root fs is mounted */
	if (!current->fs->root) {
		printk(KERN_WARNING "bt3c_cs: Root filesystem is not mounted.\n");
		return -EPERM;
	}

	sprintf(dev, "%04x", info->link.io.BasePort1);

	pid = kernel_thread(bt3c_fw_loader_exec, (void *)dev, 0);
	if (pid < 0) {
		printk(KERN_WARNING "bt3c_cs: Forking of kernel thread failed (errno=%d).\n", -pid);
		return pid;
	}

	/* Block signals, everything but SIGKILL/SIGSTOP */
	spin_lock_irq(&current->sigmask_lock);
	tmpsig = current->blocked;
	siginitsetinv(&current->blocked, sigmask(SIGKILL) | sigmask(SIGSTOP));
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	result = waitpid(pid, NULL, __WCLONE);

	/* Allow signals again */
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = tmpsig;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	if (result != pid) {
		printk(KERN_WARNING "bt3c_cs: Waiting for pid %d failed (errno=%d).\n", pid, -result);
		return -result;
	}

	return 0;
}



/* ======================== Card services HCI interaction ======================== */


int bt3c_open(bt3c_info_t *info)
{
	struct hci_dev *hdev;
	int err;

	spin_lock_init(&(info->lock));

	skb_queue_head_init(&(info->txq));

	info->rx_state = RECV_WAIT_PACKET_TYPE;
	info->rx_count = 0;
	info->rx_skb = NULL;

	/* Load firmware */

	if ((err = bt3c_firmware_load(info)) < 0)
		return err;

	/* Timeout before it is safe to send the first HCI packet */

	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(HZ);


	/* Initialize and register HCI device */

	hdev = &(info->hdev);

	hdev->type = HCI_PCCARD;
	hdev->driver_data = info;

	hdev->open = bt3c_hci_open;
	hdev->close = bt3c_hci_close;
	hdev->flush = bt3c_hci_flush;
	hdev->send = bt3c_hci_send_frame;
	hdev->destruct = bt3c_hci_destruct;
	hdev->ioctl = bt3c_hci_ioctl;

	if (hci_register_dev(hdev) < 0) {
		printk(KERN_WARNING "bt3c_cs: Can't register HCI device %s.\n", hdev->name);
		return -ENODEV;
	}

	return 0;
}


int bt3c_close(bt3c_info_t *info)
{
	struct hci_dev *hdev = &(info->hdev);

	bt3c_hci_close(hdev);

	if (hci_unregister_dev(hdev) < 0)
		printk(KERN_WARNING "bt3c_cs: Can't unregister HCI device %s.\n", hdev->name);

	return 0;
}



/* ======================== Card services ======================== */


static void cs_error(client_handle_t handle, int func, int ret)
{
	error_info_t err = { func, ret };

	CardServices(ReportError, handle, &err);
}


dev_link_t *bt3c_attach(void)
{
	bt3c_info_t *info;
	client_reg_t client_reg;
	dev_link_t *link;
	int i, ret;

	/* Create new info device */
	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return NULL;
	memset(info, 0, sizeof(*info));

	link = &info->link;
	link->priv = info;

	link->release.function = &bt3c_release;
	link->release.data = (u_long)link;
	link->io.Attributes1 = IO_DATA_PATH_WIDTH_8;
	link->io.NumPorts1 = 8;
	link->irq.Attributes = IRQ_TYPE_EXCLUSIVE | IRQ_HANDLE_PRESENT;
	link->irq.IRQInfo1 = IRQ_INFO2_VALID | IRQ_LEVEL_ID;

	if (irq_list[0] == -1)
		link->irq.IRQInfo2 = irq_mask;
	else
		for (i = 0; i < 4; i++)
			link->irq.IRQInfo2 |= 1 << irq_list[i];

	link->irq.Handler = bt3c_interrupt;
	link->irq.Instance = info;

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
	client_reg.event_handler = &bt3c_event;
	client_reg.Version = 0x0210;
	client_reg.event_callback_args.client_data = link;

	ret = CardServices(RegisterClient, &link->handle, &client_reg);
	if (ret != CS_SUCCESS) {
		cs_error(link->handle, RegisterClient, ret);
		bt3c_detach(link);
		return NULL;
	}

	return link;
}


void bt3c_detach(dev_link_t *link)
{
	bt3c_info_t *info = link->priv;
	dev_link_t **linkp;
	int ret;

	/* Locate device structure */
	for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
		if (*linkp == link)
			break;

	if (*linkp == NULL)
		return;

	del_timer(&link->release);

	if (link->state & DEV_CONFIG)
		bt3c_release((u_long)link);

	if (link->handle) {
		ret = CardServices(DeregisterClient, link->handle);
		if (ret != CS_SUCCESS)
			cs_error(link->handle, DeregisterClient, ret);
	}

	/* Unlink device structure, free bits */
	*linkp = link->next;

	kfree(info);
}


static int get_tuple(int fn, client_handle_t handle, tuple_t *tuple, cisparse_t *parse)
{
	int i;

	i = CardServices(fn, handle, tuple);
	if (i != CS_SUCCESS)
		return CS_NO_MORE_ITEMS;

	i = CardServices(GetTupleData, handle, tuple);
	if (i != CS_SUCCESS)
		return i;

	return CardServices(ParseTuple, handle, tuple, parse);
}


#define first_tuple(a, b, c) get_tuple(GetFirstTuple, a, b, c)
#define next_tuple(a, b, c) get_tuple(GetNextTuple, a, b, c)

void bt3c_config(dev_link_t *link)
{
	static ioaddr_t base[5] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8, 0x0 };
	client_handle_t handle = link->handle;
	bt3c_info_t *info = link->priv;
	tuple_t tuple;
	u_short buf[256];
	cisparse_t parse;
	cistpl_cftable_entry_t *cf = &parse.cftable_entry;
	config_info_t config;
	int i, j, try, last_ret, last_fn;

	tuple.TupleData = (cisdata_t *)buf;
	tuple.TupleOffset = 0;
	tuple.TupleDataMax = 255;
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
	i = CardServices(GetConfigurationInfo, handle, &config);
	link->conf.Vcc = config.Vcc;

	/* First pass: look for a config entry that looks normal. */
	tuple.TupleData = (cisdata_t *)buf;
	tuple.TupleOffset = 0;
	tuple.TupleDataMax = 255;
	tuple.Attributes = 0;
	tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
	/* Two tries: without IO aliases, then with aliases */
	for (try = 0; try < 2; try++) {
		i = first_tuple(handle, &tuple, &parse);
		while (i != CS_NO_MORE_ITEMS) {
			if (i != CS_SUCCESS)
				goto next_entry;
			if (cf->vpp1.present & (1 << CISTPL_POWER_VNOM))
				link->conf.Vpp1 = link->conf.Vpp2 = cf->vpp1.param[CISTPL_POWER_VNOM] / 10000;
			if ((cf->io.nwin > 0) && (cf->io.win[0].len == 8) && (cf->io.win[0].base != 0)) {
				link->conf.ConfigIndex = cf->index;
				link->io.BasePort1 = cf->io.win[0].base;
				link->io.IOAddrLines = (try == 0) ? 16 : cf->io.flags & CISTPL_IO_LINES_MASK;
				i = CardServices(RequestIO, link->handle, &link->io);
				if (i == CS_SUCCESS)
					goto found_port;
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
		if ((i == CS_SUCCESS) && (cf->io.nwin > 0) && ((cf->io.flags & CISTPL_IO_LINES_MASK) <= 3)) {
			link->conf.ConfigIndex = cf->index;
			for (j = 0; j < 5; j++) {
				link->io.BasePort1 = base[j];
				link->io.IOAddrLines = base[j] ? 16 : 3;
				i = CardServices(RequestIO, link->handle, &link->io);
				if (i == CS_SUCCESS)
					goto found_port;
			}
		}
		i = next_tuple(handle, &tuple, &parse);
	}

found_port:
	if (i != CS_SUCCESS) {
		printk(KERN_NOTICE "bt3c_cs: No usable port range found. Giving up.\n");
		cs_error(link->handle, RequestIO, i);
		goto failed;
	}

	i = CardServices(RequestIRQ, link->handle, &link->irq);
	if (i != CS_SUCCESS) {
		cs_error(link->handle, RequestIRQ, i);
		link->irq.AssignedIRQ = 0;
	}

	i = CardServices(RequestConfiguration, link->handle, &link->conf);
	if (i != CS_SUCCESS) {
		cs_error(link->handle, RequestConfiguration, i);
		goto failed;
	}

	MOD_INC_USE_COUNT;

	if (bt3c_open(info) != 0)
		goto failed;

	strcpy(info->node.dev_name, info->hdev.name);
	link->dev = &info->node;
	link->state &= ~DEV_CONFIG_PENDING;

	return;

cs_failed:
	cs_error(link->handle, last_fn, last_ret);

failed:
	bt3c_release((u_long)link);
}


void bt3c_release(u_long arg)
{
	dev_link_t *link = (dev_link_t *)arg;
	bt3c_info_t *info = link->priv;

	if (link->state & DEV_PRESENT)
		bt3c_close(info);

	MOD_DEC_USE_COUNT;

	link->dev = NULL;

	CardServices(ReleaseConfiguration, link->handle);
	CardServices(ReleaseIO, link->handle, &link->io);
	CardServices(ReleaseIRQ, link->handle, &link->irq);

	link->state &= ~DEV_CONFIG;
}


int bt3c_event(event_t event, int priority, event_callback_args_t *args)
{
	dev_link_t *link = args->client_data;
	bt3c_info_t *info = link->priv;

	switch (event) {
	case CS_EVENT_CARD_REMOVAL:
		link->state &= ~DEV_PRESENT;
		if (link->state & DEV_CONFIG) {
			bt3c_close(info);
			mod_timer(&link->release, jiffies + HZ / 20);
		}
		break;
	case CS_EVENT_CARD_INSERTION:
		link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
		bt3c_config(link);
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
}



/* ======================== Module initialization ======================== */


int __init init_bt3c_cs(void)
{
	servinfo_t serv;
	int err;

	CardServices(GetCardServicesInfo, &serv);
	if (serv.Revision != CS_RELEASE_CODE) {
		printk(KERN_NOTICE "bt3c_cs: Card Services release does not match!\n");
		return -1;
	}

	err = register_pccard_driver(&dev_info, &bt3c_attach, &bt3c_detach);

	return err;
}


void __exit exit_bt3c_cs(void)
{
	unregister_pccard_driver(&dev_info);

	while (dev_list != NULL)
		bt3c_detach(dev_list);
}


module_init(init_bt3c_cs);
module_exit(exit_bt3c_cs);

EXPORT_NO_SYMBOLS;
