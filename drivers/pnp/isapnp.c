/*
 *  ISA Plug & Play support
 *  Copyright (c) by Jaroslav Kysela <perex@suse.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Changelog:
 *  2000-01-01	Added quirks handling for buggy hardware
 *		Peter Denison <peterd@pnd-pc.demon.co.uk>
 *  2000-06-14	Added isapnp_probe_devs() and isapnp_activate_dev()
 *		Christoph Hellwig <hch@infradead.org>
 *  2001-06-03  Added release_region calls to correspond with
 *		request_region calls when a failure occurs.  Also
 *		added KERN_* constants to printk() calls.
 *  2001-11-07  Added isapnp_{,un}register_driver calls along the lines
 *              of the pci driver interface
 *              Kai Germaschewski <kai.germaschewski@gmx.de>
 *  2002-06-06  Made the use of dma channel 0 configurable 
 *              Gerald Teschl <gerald.teschl@univie.ac.at>
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/isapnp.h>

LIST_HEAD(isapnp_cards);
LIST_HEAD(isapnp_devices);

#if 0
#define ISAPNP_REGION_OK
#endif
#if 0
#define ISAPNP_DEBUG
#endif

int isapnp_disable;			/* Disable ISA PnP */
int isapnp_rdp;				/* Read Data Port */
int isapnp_reset = 1;			/* reset all PnP cards (deactivate) */
int isapnp_allow_dma0 = -1;		/* allow dma 0 during auto activation: -1=off (:default), 0=off (set by user), 1=on */
int isapnp_skip_pci_scan;		/* skip PCI resource scanning */
int isapnp_verbose = 1;			/* verbose mode */
int isapnp_reserve_irq[16] = { [0 ... 15] = -1 };	/* reserve (don't use) some IRQ */
int isapnp_reserve_dma[8] = { [0 ... 7] = -1 };		/* reserve (don't use) some DMA */
int isapnp_reserve_io[16] = { [0 ... 15] = -1 };	/* reserve (don't use) some I/O region */
int isapnp_reserve_mem[16] = { [0 ... 15] = -1 };	/* reserve (don't use) some memory region */

MODULE_AUTHOR("Jaroslav Kysela <perex@suse.cz>");
MODULE_DESCRIPTION("Generic ISA Plug & Play support");
MODULE_PARM(isapnp_disable, "i");
MODULE_PARM_DESC(isapnp_disable, "ISA Plug & Play disable");
MODULE_PARM(isapnp_rdp, "i");
MODULE_PARM_DESC(isapnp_rdp, "ISA Plug & Play read data port");
MODULE_PARM(isapnp_reset, "i");
MODULE_PARM_DESC(isapnp_reset, "ISA Plug & Play reset all cards");
MODULE_PARM(isapnp_allow_dma0, "i");
MODULE_PARM_DESC(isapnp_allow_dma0, "Allow dma value 0 during auto activation");
MODULE_PARM(isapnp_skip_pci_scan, "i");
MODULE_PARM_DESC(isapnp_skip_pci_scan, "ISA Plug & Play skip PCI resource scanning");
MODULE_PARM(isapnp_verbose, "i");
MODULE_PARM_DESC(isapnp_verbose, "ISA Plug & Play verbose mode");
MODULE_PARM(isapnp_reserve_irq, "1-16i");
MODULE_PARM_DESC(isapnp_reserve_irq, "ISA Plug & Play - reserve IRQ line(s)");
MODULE_PARM(isapnp_reserve_dma, "1-8i");
MODULE_PARM_DESC(isapnp_reserve_dma, "ISA Plug & Play - reserve DMA channel(s)");
MODULE_PARM(isapnp_reserve_io, "1-16i");
MODULE_PARM_DESC(isapnp_reserve_io, "ISA Plug & Play - reserve I/O region(s) - port,size");
MODULE_PARM(isapnp_reserve_mem, "1-16i");
MODULE_PARM_DESC(isapnp_reserve_mem, "ISA Plug & Play - reserve memory region(s) - address,size");
MODULE_LICENSE("GPL");

#define _PIDXR		0x279
#define _PNPWRP		0xa79

/* short tags */
#define _STAG_PNPVERNO		0x01
#define _STAG_LOGDEVID		0x02
#define _STAG_COMPATDEVID	0x03
#define _STAG_IRQ		0x04
#define _STAG_DMA		0x05
#define _STAG_STARTDEP		0x06
#define _STAG_ENDDEP		0x07
#define _STAG_IOPORT		0x08
#define _STAG_FIXEDIO		0x09
#define _STAG_VENDOR		0x0e
#define _STAG_END		0x0f
/* long tags */
#define _LTAG_MEMRANGE		0x81
#define _LTAG_ANSISTR		0x82
#define _LTAG_UNICODESTR	0x83
#define _LTAG_VENDOR		0x84
#define _LTAG_MEM32RANGE	0x85
#define _LTAG_FIXEDMEM32RANGE	0x86

static unsigned char isapnp_checksum_value;
static DECLARE_MUTEX(isapnp_cfg_mutex);
static int isapnp_detected;

/* some prototypes */

static int isapnp_config_prepare(struct pci_dev *dev);
static int isapnp_config_activate(struct pci_dev *dev);
static int isapnp_config_deactivate(struct pci_dev *dev);

static inline void write_data(unsigned char x)
{
	outb(x, _PNPWRP);
}

static inline void write_address(unsigned char x)
{
	outb(x, _PIDXR);
	udelay(20);
}

static inline unsigned char read_data(void)
{
	unsigned char val = inb(isapnp_rdp);
	return val;
}

unsigned char isapnp_read_byte(unsigned char idx)
{
	write_address(idx);
	return read_data();
}

unsigned short isapnp_read_word(unsigned char idx)
{
	unsigned short val;

	val = isapnp_read_byte(idx);
	val = (val << 8) + isapnp_read_byte(idx+1);
	return val;
}

unsigned int isapnp_read_dword(unsigned char idx)
{
	unsigned int val;

	val = isapnp_read_byte(idx);
	val = (val << 8) + isapnp_read_byte(idx+1);
	val = (val << 8) + isapnp_read_byte(idx+2);
	val = (val << 8) + isapnp_read_byte(idx+3);
	return val;
}

void isapnp_write_byte(unsigned char idx, unsigned char val)
{
	write_address(idx);
	write_data(val);
}

void isapnp_write_word(unsigned char idx, unsigned short val)
{
	isapnp_write_byte(idx, val >> 8);
	isapnp_write_byte(idx+1, val);
}

void isapnp_write_dword(unsigned char idx, unsigned int val)
{
	isapnp_write_byte(idx, val >> 24);
	isapnp_write_byte(idx+1, val >> 16);
	isapnp_write_byte(idx+2, val >> 8);
	isapnp_write_byte(idx+3, val);
}

void *isapnp_alloc(long size)
{
	void *result;

	result = kmalloc(size, GFP_KERNEL);
	if (!result)
		return NULL;
	memset(result, 0, size);
	return result;
}

static void isapnp_key(void)
{
	unsigned char code = 0x6a, msb;
	int i;

	mdelay(1);
	write_address(0x00);
	write_address(0x00);

	write_address(code);

	for (i = 1; i < 32; i++) {
		msb = ((code & 0x01) ^ ((code & 0x02) >> 1)) << 7;
		code = (code >> 1) | msb;
		write_address(code);
	}
}

/* place all pnp cards in wait-for-key state */
static void isapnp_wait(void)
{
	isapnp_write_byte(0x02, 0x02);
}

void isapnp_wake(unsigned char csn)
{
	isapnp_write_byte(0x03, csn);
}

void isapnp_device(unsigned char logdev)
{
	isapnp_write_byte(0x07, logdev);
}

void isapnp_activate(unsigned char logdev)
{
	isapnp_device(logdev);
	isapnp_write_byte(ISAPNP_CFG_ACTIVATE, 1);
	udelay(250);
}

void isapnp_deactivate(unsigned char logdev)
{
	isapnp_device(logdev);
	isapnp_write_byte(ISAPNP_CFG_ACTIVATE, 0);
	udelay(500);
}

static void __init isapnp_peek(unsigned char *data, int bytes)
{
	int i, j;
	unsigned char d=0;

	for (i = 1; i <= bytes; i++) {
		for (j = 0; j < 20; j++) {
			d = isapnp_read_byte(0x05);
			if (d & 1)
				break;
			udelay(100);
		}
		if (!(d & 1)) {
			if (data != NULL)
				*data++ = 0xff;
			continue;
		}
		d = isapnp_read_byte(0x04);	/* PRESDI */
		isapnp_checksum_value += d;
		if (data != NULL)
			*data++ = d;
	}
}

#define RDP_STEP	32	/* minimum is 4 */

static int isapnp_next_rdp(void)
{
	int rdp = isapnp_rdp;
	while (rdp <= 0x3ff) {
		/*
		 *	We cannot use NE2000 probe spaces for ISAPnP or we
		 *	will lock up machines.
		 */
		if ((rdp < 0x280 || rdp >  0x380) && !check_region(rdp, 1)) 
		{
			isapnp_rdp = rdp;
			return 0;
		}
		rdp += RDP_STEP;
	}
	return -1;
}

/* Set read port address */
static inline void isapnp_set_rdp(void)
{
	isapnp_write_byte(0x00, isapnp_rdp >> 2);
	udelay(100);
}

/*
 *	Perform an isolation. The port selection code now tries to avoid
 *	"dangerous to read" ports.
 */

static int __init isapnp_isolate_rdp_select(void)
{
	isapnp_wait();
	isapnp_key();

	/* Control: reset CSN and conditionally everything else too */
	isapnp_write_byte(0x02, isapnp_reset ? 0x05 : 0x04);
	mdelay(2);

	isapnp_wait();
	isapnp_key();
	isapnp_wake(0x00);

	if (isapnp_next_rdp() < 0) {
		isapnp_wait();
		return -1;
	}

	isapnp_set_rdp();
	udelay(1000);
	write_address(0x01);
	udelay(1000);
	return 0;
}

/*
 *  Isolate (assign uniqued CSN) to all ISA PnP devices.
 */

static int __init isapnp_isolate(void)
{
	unsigned char checksum = 0x6a;
	unsigned char chksum = 0x00;
	unsigned char bit = 0x00;
	int data;
	int csn = 0;
	int i;
	int iteration = 1;

	isapnp_rdp = 0x213;
	if (isapnp_isolate_rdp_select() < 0)
		return -1;

	while (1) {
		for (i = 1; i <= 64; i++) {
			data = read_data() << 8;
			udelay(250);
			data = data | read_data();
			udelay(250);
			if (data == 0x55aa)
				bit = 0x01;
			checksum = ((((checksum ^ (checksum >> 1)) & 0x01) ^ bit) << 7) | (checksum >> 1);
			bit = 0x00;
		}
		for (i = 65; i <= 72; i++) {
			data = read_data() << 8;
			udelay(250);
			data = data | read_data();
			udelay(250);
			if (data == 0x55aa)
				chksum |= (1 << (i - 65));
		}
		if (checksum != 0x00 && checksum == chksum) {
			csn++;

			isapnp_write_byte(0x06, csn);
			udelay(250);
			iteration++;
			isapnp_wake(0x00);
			isapnp_set_rdp();
			udelay(1000);
			write_address(0x01);
			udelay(1000);
			goto __next;
		}
		if (iteration == 1) {
			isapnp_rdp += RDP_STEP;
			if (isapnp_isolate_rdp_select() < 0)
				return -1;
		} else if (iteration > 1) {
			break;
		}
	      __next:
		checksum = 0x6a;
		chksum = 0x00;
		bit = 0x00;
	}
	isapnp_wait();
	return csn;
}

/*
 *  Read one tag from stream.
 */

static int __init isapnp_read_tag(unsigned char *type, unsigned short *size)
{
	unsigned char tag, tmp[2];

	isapnp_peek(&tag, 1);
	if (tag == 0)				/* invalid tag */
		return -1;
	if (tag & 0x80) {	/* large item */
		*type = tag;
		isapnp_peek(tmp, 2);
		*size = (tmp[1] << 8) | tmp[0];
	} else {
		*type = (tag >> 3) & 0x0f;
		*size = tag & 0x07;
	}
#if 0
	printk(KERN_DEBUG "tag = 0x%x, type = 0x%x, size = %i\n", tag, *type, *size);
#endif
	if (type == 0)				/* wrong type */
		return -1;
	if (*type == 0xff && *size == 0xffff)	/* probably invalid data */
		return -1;
	return 0;
}

/*
 *  Skip specified number of bytes from stream.
 */
 
static void __init isapnp_skip_bytes(int count)
{
	isapnp_peek(NULL, count);
}

/*
 *  Parse logical device tag.
 */

static struct pci_dev * __init isapnp_parse_device(struct pci_bus *card, int size, int number)
{
	unsigned char tmp[6];
	struct pci_dev *dev;

	isapnp_peek(tmp, size);
	dev = isapnp_alloc(sizeof(struct pci_dev));
	if (!dev)
		return NULL;
	dev->dma_mask = 0x00ffffff;
	dev->devfn = number;
	dev->vendor = (tmp[1] << 8) | tmp[0];
	dev->device = (tmp[3] << 8) | tmp[2];
	dev->regs = tmp[4];
	dev->bus = card;
	if (size > 5)
		dev->regs |= tmp[5] << 8;
	dev->prepare = isapnp_config_prepare;
	dev->activate = isapnp_config_activate;
	dev->deactivate = isapnp_config_deactivate;
	return dev;
}

/*
 *  Build new resources structure
 */

static struct isapnp_resources * __init isapnp_build_resources(struct pci_dev *dev, int dependent)
{
	struct isapnp_resources *res, *ptr, *ptra;
	
	res = isapnp_alloc(sizeof(struct isapnp_resources));
	if (!res)
		return NULL;
	res->dev = dev;
	ptr = (struct isapnp_resources *)dev->sysdata;
	while (ptr && ptr->next)
		ptr = ptr->next;
	if (ptr && ptr->dependent && dependent) { /* add to another list */
		ptra = ptr->alt;
		while (ptra && ptra->alt)
			ptra = ptra->alt;
		if (!ptra)
			ptr->alt = res;
		else
			ptra->alt = res;
	} else {
		if (!ptr)
			dev->sysdata = res;
		else
			ptr->next = res;
	}
	if (dependent) {
		res->priority = dependent & 0xff;
		if (res->priority > ISAPNP_RES_PRIORITY_FUNCTIONAL)
			res->priority = ISAPNP_RES_PRIORITY_INVALID;
		res->dependent = 1;
	} else {
		res->priority = ISAPNP_RES_PRIORITY_PREFERRED;
		res->dependent = 0;
	}
	return res;
}

/*
 *  Add IRQ resource to resources list.
 */

static void __init isapnp_add_irq_resource(struct pci_dev *dev,
                                    	       struct isapnp_resources **res,
                                               int dependent, int size)
{
	unsigned char tmp[3];
	struct isapnp_irq *irq, *ptr;

	isapnp_peek(tmp, size);
	irq = isapnp_alloc(sizeof(struct isapnp_irq));
	if (!irq)
		return;
	if (*res == NULL) {
		*res = isapnp_build_resources(dev, dependent);
		if (*res == NULL) {
			kfree(irq);
			return;
		}
	}
	irq->map = (tmp[1] << 8) | tmp[0];
	if (size > 2)
		irq->flags = tmp[2];
	else
		irq->flags = IORESOURCE_IRQ_HIGHEDGE;
	irq->res = *res;
	ptr = (*res)->irq;
	while (ptr && ptr->next)
		ptr = ptr->next;
	if (ptr)
		ptr->next = irq;
	else
		(*res)->irq = irq;
#ifdef CONFIG_PCI
	{
		int i;

		for (i=0; i<16; i++)
			if (irq->map & (1<<i))
				pcibios_penalize_isa_irq(i);
	}
#endif
}

/*
 *  Add DMA resource to resources list.
 */

static void __init isapnp_add_dma_resource(struct pci_dev *dev,
                                    	       struct isapnp_resources **res,
                                    	       int dependent, int size)
{
	unsigned char tmp[2];
	struct isapnp_dma *dma, *ptr;

	isapnp_peek(tmp, size);
	dma = isapnp_alloc(sizeof(struct isapnp_dma));
	if (!dma)
		return;
	if (*res == NULL) {
		*res = isapnp_build_resources(dev, dependent);
		if (*res == NULL) {
			kfree(dma);
			return;
		}
	}
	dma->map = tmp[0];
	dma->flags = tmp[1];
	dma->res = *res;
	ptr = (*res)->dma;
	while (ptr && ptr->next)
		ptr = ptr->next;
	if (ptr)
		ptr->next = dma;
	else
		(*res)->dma = dma;
}

/*
 *  Add port resource to resources list.
 */

static void __init isapnp_add_port_resource(struct pci_dev *dev,
						struct isapnp_resources **res,
						int dependent, int size)
{
	unsigned char tmp[7];
	struct isapnp_port *port, *ptr;

	isapnp_peek(tmp, size);
	port = isapnp_alloc(sizeof(struct isapnp_port));
	if (!port)
		return;
	if (*res == NULL) {
		*res = isapnp_build_resources(dev, dependent);
		if (*res == NULL) {
			kfree(port);
			return;
		}
	}
	port->min = (tmp[2] << 8) | tmp[1];
	port->max = (tmp[4] << 8) | tmp[3];
	port->align = tmp[5];
	port->size = tmp[6];
	port->flags = tmp[0] ? ISAPNP_PORT_FLAG_16BITADDR : 0;
	port->res = *res;
	ptr = (*res)->port;
	while (ptr && ptr->next)
		ptr = ptr->next;
	if (ptr)
		ptr->next = port;
	else
		(*res)->port = port;
}

/*
 *  Add fixed port resource to resources list.
 */

static void __init isapnp_add_fixed_port_resource(struct pci_dev *dev,
						      struct isapnp_resources **res,
						      int dependent, int size)
{
	unsigned char tmp[3];
	struct isapnp_port *port, *ptr;

	isapnp_peek(tmp, size);
	port = isapnp_alloc(sizeof(struct isapnp_port));
	if (!port)
		return;
	if (*res == NULL) {
		*res = isapnp_build_resources(dev, dependent);
		if (*res == NULL) {
			kfree(port);
			return;
		}
	}
	port->min = port->max = (tmp[1] << 8) | tmp[0];
	port->size = tmp[2];
	port->align = 0;
	port->flags = ISAPNP_PORT_FLAG_FIXED;
	port->res = *res;
	ptr = (*res)->port;
	while (ptr && ptr->next)
		ptr = ptr->next;
	if (ptr)
		ptr->next = port;
	else
		(*res)->port = port;
}

/*
 *  Add memory resource to resources list.
 */

static void __init isapnp_add_mem_resource(struct pci_dev *dev,
					       struct isapnp_resources **res,
					       int dependent, int size)
{
	unsigned char tmp[9];
	struct isapnp_mem *mem, *ptr;

	isapnp_peek(tmp, size);
	mem = isapnp_alloc(sizeof(struct isapnp_mem));
	if (!mem)
		return;
	if (*res == NULL) {
		*res = isapnp_build_resources(dev, dependent);
		if (*res == NULL) {
			kfree(mem);
			return;
		}
	}
	mem->min = ((tmp[2] << 8) | tmp[1]) << 8;
	mem->max = ((tmp[4] << 8) | tmp[3]) << 8;
	mem->align = (tmp[6] << 8) | tmp[5];
	mem->size = ((tmp[8] << 8) | tmp[7]) << 8;
	mem->flags = tmp[0];
	mem->res = *res;
	ptr = (*res)->mem;
	while (ptr && ptr->next)
		ptr = ptr->next;
	if (ptr)
		ptr->next = mem;
	else
		(*res)->mem = mem;
}

/*
 *  Add 32-bit memory resource to resources list.
 */

static void __init isapnp_add_mem32_resource(struct pci_dev *dev,
						 struct isapnp_resources **res,
						 int dependent, int size)
{
	unsigned char tmp[17];
	struct isapnp_mem32 *mem32, *ptr;

	isapnp_peek(tmp, size);
	mem32 = isapnp_alloc(sizeof(struct isapnp_mem32));
	if (!mem32)
		return;
	if (*res == NULL) {
		*res = isapnp_build_resources(dev, dependent);
		if (*res == NULL) {
			kfree(mem32);
			return;
		}
	}
	memcpy(mem32->data, tmp, 17);
	mem32->res = *res;
	ptr = (*res)->mem32;
	while (ptr && ptr->next)
		ptr = ptr->next;
	if (ptr)
		ptr->next = mem32;
	else
		(*res)->mem32 = mem32;
}

/*
 *  Add 32-bit fixed memory resource to resources list.
 */

static void __init isapnp_add_fixed_mem32_resource(struct pci_dev *dev,
						       struct isapnp_resources **res,
						       int dependent, int size)
{
	unsigned char tmp[17];
	struct isapnp_mem32 *mem32, *ptr;

	isapnp_peek(tmp, size);
	mem32 = isapnp_alloc(sizeof(struct isapnp_mem32));
	if (!mem32)
		return;
	if (*res == NULL) {
		*res = isapnp_build_resources(dev, dependent);
		if (*res == NULL) {
			kfree(mem32);
			return;
		}
	}
	memcpy(mem32->data, tmp, 17);
	mem32->res = *res;
	ptr = (*res)->mem32;
	while (ptr && ptr->next)
		ptr = ptr->next;
	if (ptr)
		ptr->next = mem32;
	else
		(*res)->mem32 = mem32;
}

/*
 *  Parse card name for ISA PnP device.
 */ 
 
static void __init 
isapnp_parse_name(char *name, unsigned int name_max, unsigned short *size)
{
	if (name[0] == '\0') {
		unsigned short size1 = *size >= name_max ? (name_max - 1) : *size;
		isapnp_peek(name, size1);
		name[size1] = '\0';
		*size -= size1;
		
		/* clean whitespace from end of string */
		while (size1 > 0  &&  name[--size1] == ' ') 
			name[size1] = '\0';
	}	
}

/*
 *  Parse resource map for logical device.
 */

static int __init isapnp_create_device(struct pci_bus *card,
					   unsigned short size)
{
	int number = 0, skip = 0, dependent = 0, compat = 0;
	unsigned char type, tmp[17];
	struct pci_dev *dev;
	struct isapnp_resources *res = NULL;
	
	if ((dev = isapnp_parse_device(card, size, number++)) == NULL)
		return 1;
	list_add(&dev->bus_list, &card->devices);
	list_add_tail(&dev->global_list, &isapnp_devices);
	while (1) {
		if (isapnp_read_tag(&type, &size)<0)
			return 1;
		if (skip && type != _STAG_LOGDEVID && type != _STAG_END)
			goto __skip;
		switch (type) {
		case _STAG_LOGDEVID:
			if (size >= 5 && size <= 6) {
				isapnp_config_prepare(dev);
				if ((dev = isapnp_parse_device(card, size, number++)) == NULL)
					return 1;
				list_add_tail(&dev->bus_list, &card->devices);
				list_add_tail(&dev->global_list, &isapnp_devices);
				size = 0;
				skip = 0;
			} else {
				skip = 1;
			}
			res = NULL;
			dependent = 0;
			compat = 0;
			break;
		case _STAG_COMPATDEVID:
			if (size == 4 && compat < DEVICE_COUNT_COMPATIBLE) {
				isapnp_peek(tmp, 4);
				dev->vendor_compatible[compat] = (tmp[1] << 8) | tmp[0];
				dev->device_compatible[compat] = (tmp[3] << 8) | tmp[2];
				compat++;
				size = 0;
			}
			break;
		case _STAG_IRQ:
			if (size < 2 || size > 3)
				goto __skip;
			isapnp_add_irq_resource(dev, &res, dependent, size);
			size = 0;
			break;
		case _STAG_DMA:
			if (size != 2)
				goto __skip;
			isapnp_add_dma_resource(dev, &res, dependent, size);
			size = 0;
			break;
		case _STAG_STARTDEP:
			if (size > 1)
				goto __skip;
			res = NULL;
			dependent = 0x100 | ISAPNP_RES_PRIORITY_ACCEPTABLE;
			if (size > 0) {
				isapnp_peek(tmp, size);
				dependent = 0x100 | tmp[0];
				size = 0;
			}
			break;
		case _STAG_ENDDEP:
			if (size != 0)
				goto __skip;
			res = NULL;
			dependent = 0;
			break;
		case _STAG_IOPORT:
			if (size != 7)
				goto __skip;
			isapnp_add_port_resource(dev, &res, dependent, size);
			size = 0;
			break;
		case _STAG_FIXEDIO:
			if (size != 3)
				goto __skip;
			isapnp_add_fixed_port_resource(dev, &res, dependent, size);
			size = 0;
			break;
		case _STAG_VENDOR:
			break;
		case _LTAG_MEMRANGE:
			if (size != 9)
				goto __skip;
			isapnp_add_mem_resource(dev, &res, dependent, size);
			size = 0;
			break;
		case _LTAG_ANSISTR:
			isapnp_parse_name(dev->name, sizeof(dev->name), &size);
			break;
		case _LTAG_UNICODESTR:
			/* silently ignore */
			/* who use unicode for hardware identification? */
			break;
		case _LTAG_VENDOR:
			break;
		case _LTAG_MEM32RANGE:
			if (size != 17)
				goto __skip;
			isapnp_add_mem32_resource(dev, &res, dependent, size);
			size = 0;
			break;
		case _LTAG_FIXEDMEM32RANGE:
			if (size != 17)
				goto __skip;
			isapnp_add_fixed_mem32_resource(dev, &res, dependent, size);
			size = 0;
			break;
		case _STAG_END:
			if (size > 0)
				isapnp_skip_bytes(size);
			isapnp_config_prepare(dev);
			return 1;
		default:
			printk(KERN_ERR "isapnp: unexpected or unknown tag type 0x%x for logical device %i (device %i), ignored\n", type, dev->devfn, card->number);
		}
	      __skip:
	      	if (size > 0)
		      	isapnp_skip_bytes(size);
	}
	isapnp_config_prepare(dev);
	return 0;
}

/*
 *  Parse resource map for ISA PnP card.
 */
 
static void __init isapnp_parse_resource_map(struct pci_bus *card)
{
	unsigned char type, tmp[17];
	unsigned short size;
	
	while (1) {
		if (isapnp_read_tag(&type, &size)<0)
			return;
		switch (type) {
		case _STAG_PNPVERNO:
			if (size != 2)
				goto __skip;
			isapnp_peek(tmp, 2);
			card->pnpver = tmp[0];
			card->productver = tmp[1];
			size = 0;
			break;
		case _STAG_LOGDEVID:
			if (size >= 5 && size <= 6) {
				if (isapnp_create_device(card, size)==1)
					return;
				size = 0;
			}
			break;
		case _STAG_VENDOR:
			break;
		case _LTAG_ANSISTR:
			isapnp_parse_name(card->name, sizeof(card->name), &size);
			break;
		case _LTAG_UNICODESTR:
			/* silently ignore */
			/* who use unicode for hardware identification? */
			break;
		case _LTAG_VENDOR:
			break;
		case _STAG_END:
			if (size > 0)
				isapnp_skip_bytes(size);
			return;
		default:
			printk(KERN_ERR "isapnp: unexpected or unknown tag type 0x%x for device %i, ignored\n", type, card->number);
		}
	      __skip:
	      	if (size > 0)
		      	isapnp_skip_bytes(size);
	}
}

/*
 *  Compute ISA PnP checksum for first eight bytes.
 */

static unsigned char __init isapnp_checksum(unsigned char *data)
{
	int i, j;
	unsigned char checksum = 0x6a, bit, b;
	
	for (i = 0; i < 8; i++) {
		b = data[i];
		for (j = 0; j < 8; j++) {
			bit = 0;
			if (b & (1 << j))
				bit = 1;
			checksum = ((((checksum ^ (checksum >> 1)) & 0x01) ^ bit) << 7) | (checksum >> 1);
		}
	}
	return checksum;
}

/*
 *  Build device list for all present ISA PnP devices.
 */

static int __init isapnp_build_device_list(void)
{
	int csn;
	unsigned char header[9], checksum;
	struct pci_bus *card;
	struct pci_dev *dev;

	isapnp_wait();
	isapnp_key();
	for (csn = 1; csn <= 10; csn++) {
		isapnp_wake(csn);
		isapnp_peek(header, 9);
		checksum = isapnp_checksum(header);
#if 0
		printk(KERN_DEBUG "vendor: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
			header[0], header[1], header[2], header[3],
			header[4], header[5], header[6], header[7], header[8]);
		printk(KERN_DEBUG "checksum = 0x%x\n", checksum);
#endif
		/* Don't be strict on the checksum, here !
                   e.g. 'SCM SwapBox Plug and Play' has header[8]==0 (should be: b7)*/
		if (header[8] == 0)
			;
		else if (checksum == 0x00 || checksum != header[8])	/* not valid CSN */
			continue;
		if ((card = isapnp_alloc(sizeof(struct pci_bus))) == NULL)
			continue;

		card->number = csn;
		card->vendor = (header[1] << 8) | header[0];
		card->device = (header[3] << 8) | header[2];
		card->serial = (header[7] << 24) | (header[6] << 16) | (header[5] << 8) | header[4];
		isapnp_checksum_value = 0x00;
		INIT_LIST_HEAD(&card->children);
		INIT_LIST_HEAD(&card->devices);
		isapnp_parse_resource_map(card);
		if (isapnp_checksum_value != 0x00)
			printk(KERN_ERR "isapnp: checksum for device %i is not valid (0x%x)\n", csn, isapnp_checksum_value);
		card->checksum = isapnp_checksum_value;

		list_add_tail(&card->node, &isapnp_cards);
	}
	isapnp_for_each_dev(dev) {
		isapnp_fixup_device(dev);
	}
	return 0;
}

/*
 *  Basic configuration routines.
 */

int isapnp_present(void)
{
	return !list_empty(&isapnp_devices);
}

int isapnp_cfg_begin(int csn, int logdev)
{
	if (csn < 1 || csn > 10 || logdev > 10)
		return -EINVAL;
	MOD_INC_USE_COUNT;
	down(&isapnp_cfg_mutex);
	isapnp_wait();
	isapnp_key();
	isapnp_wake(csn);
#if 1	/* to avoid malfunction when the isapnptools package is used */
	isapnp_set_rdp();
	udelay(1000);	/* delay 1000us */
	write_address(0x01);
	udelay(1000);	/* delay 1000us */
#endif
	if (logdev >= 0)
		isapnp_device(logdev);
	return 0;
}

int isapnp_cfg_end(void)
{
	isapnp_wait();
	up(&isapnp_cfg_mutex);
	MOD_DEC_USE_COUNT;
	return 0;
}

/*
 *  Resource manager.
 */

static struct isapnp_port *isapnp_find_port(struct pci_dev *dev, int index)
{
	struct isapnp_resources *res;
	struct isapnp_port *port;
	
	if (!dev || index < 0 || index > 7)
		return NULL;
	for (res = (struct isapnp_resources *)dev->sysdata; res; res = res->next) {
		for (port = res->port; port; port = port->next) {
			if (!index)
				return port;
			index--;
		}
	}
	return NULL;
}

struct isapnp_irq *isapnp_find_irq(struct pci_dev *dev, int index)
{
	struct isapnp_resources *res, *resa;
	struct isapnp_irq *irq;
	int index1, index2, index3;
	
	if (!dev || index < 0 || index > 7)
		return NULL;
	for (res = (struct isapnp_resources *)dev->sysdata; res; res = res->next) {
		index3 = 0;
		for (resa = res; resa; resa = resa->alt) {
			index1 = index;
			index2 = 0;
			for (irq = resa->irq; irq; irq = irq->next) {
				if (!index1)
					return irq;
				index1--;
				index2++;
			}
			if (index3 < index2)
				index3 = index2;
		}
		index -= index3;
	}
	return NULL;
}

struct isapnp_dma *isapnp_find_dma(struct pci_dev *dev, int index)
{
	struct isapnp_resources *res;
	struct isapnp_dma *dma;
	
	if (!dev || index < 0 || index > 7)
		return NULL;
	for (res = (struct isapnp_resources *)dev->sysdata; res; res = res->next) {
		for (dma = res->dma; dma; dma = dma->next) {
			if (!index)
				return dma;
			index--;
		}
	}
	return NULL;
}

struct isapnp_mem *isapnp_find_mem(struct pci_dev *dev, int index)
{
	struct isapnp_resources *res;
	struct isapnp_mem *mem;
	
	if (!dev || index < 0 || index > 7)
		return NULL;
	for (res = (struct isapnp_resources *)dev->sysdata; res; res = res->next) {
		for (mem = res->mem; mem; mem = mem->next) {
			if (!index)
				return mem;
			index--;
		}
	}
	return NULL;
}

struct isapnp_mem32 *isapnp_find_mem32(struct pci_dev *dev, int index)
{
	struct isapnp_resources *res;
	struct isapnp_mem32 *mem32;
	
	if (!dev || index < 0 || index > 7)
		return NULL;
	for (res = (struct isapnp_resources *)dev->sysdata; res; res = res->next) {
		for (mem32 = res->mem32; mem32; mem32 = mem32->next) {
			if (!index)
				return mem32;
			index--;
		}
	}
	return NULL;
}

/*
 *  Device manager.
 */

struct pci_bus *isapnp_find_card(unsigned short vendor,
				 unsigned short device,
				 struct pci_bus *from)
{
	struct list_head *list;

	list = isapnp_cards.next;
	if (from)
		list = from->node.next;

	while (list != &isapnp_cards) {
		struct pci_bus *card = pci_bus_b(list);
		if (card->vendor == vendor && card->device == device)
			return card;
		list = list->next;
	}
	return NULL;
}

struct pci_dev *isapnp_find_dev(struct pci_bus *card,
				unsigned short vendor,
				unsigned short function,
				struct pci_dev *from)
{
	if (card == NULL) {	/* look for a logical device from all cards */
		struct list_head *list;

		list = isapnp_devices.next;
		if (from)
			list = from->global_list.next;

		while (list != &isapnp_devices) {
			int idx;
			struct pci_dev *dev = pci_dev_g(list);

			if (dev->vendor == vendor && dev->device == function)
				return dev;
			for (idx = 0; idx < DEVICE_COUNT_COMPATIBLE; idx++)
				if (dev->vendor_compatible[idx] == vendor &&
				    dev->device_compatible[idx] == function)
					return dev;
			list = list->next;
		}
	} else {
		struct list_head *list;

		list = card->devices.next;
		if (from) {
			list = from->bus_list.next;
			if (from->bus != card)	/* something is wrong */
				return NULL;
		}
		while (list != &card->devices) {
			int idx;
			struct pci_dev *dev = pci_dev_b(list);

			if (dev->vendor == vendor && dev->device == function)
				return dev;
			for (idx = 0; idx < DEVICE_COUNT_COMPATIBLE; idx++)
				if (dev->vendor_compatible[idx] == vendor &&
				    dev->device_compatible[idx] == function)
					return dev;
			list = list->next;
		}
	}
	return NULL;
}

static const struct isapnp_card_id *
isapnp_match_card(const struct isapnp_card_id *ids, struct pci_bus *card)
{
	int idx;

	while (ids->card_vendor || ids->card_device) {
		if ((ids->card_vendor == ISAPNP_ANY_ID || ids->card_vendor == card->vendor) &&
		    (ids->card_device == ISAPNP_ANY_ID || ids->card_device == card->device)) {
			for (idx = 0; idx < ISAPNP_CARD_DEVS; idx++) {
				if (ids->devs[idx].vendor == 0 &&
				    ids->devs[idx].function == 0)
					return ids;
				if (isapnp_find_dev(card,
						    ids->devs[idx].vendor,
						    ids->devs[idx].function,
						    NULL) == NULL)
					goto __next;
			}
			return ids;
		}
	      __next:
		ids++;
	}
	return NULL;
}

int isapnp_probe_cards(const struct isapnp_card_id *ids,
		       int (*probe)(struct pci_bus *_card,
		       		    const struct isapnp_card_id *_id))
{
	struct pci_bus *card;	
	const struct isapnp_card_id *id;
	int count = 0;

	if (ids == NULL || probe == NULL)
		return -EINVAL;
	isapnp_for_each_card(card) {
		id = isapnp_match_card(ids, card);
		if (id != NULL && probe(card, id) >= 0)
			count++;
	}
	return count;
}

static const struct isapnp_device_id *
isapnp_match_dev(const struct isapnp_device_id *ids, struct pci_dev *dev)
{
	while (ids->card_vendor || ids->card_device) {
		if ((ids->card_vendor == ISAPNP_ANY_ID || ids->card_vendor == dev->bus->vendor) &&
		    (ids->card_device == ISAPNP_ANY_ID || ids->card_device == dev->bus->device) &&
                    (ids->vendor == ISAPNP_ANY_ID || ids->vendor == dev->vendor) &&
                    (ids->function == ISAPNP_ANY_ID || ids->function == dev->device))
			return ids;
		ids++;
	}
	return NULL;
}

int isapnp_probe_devs(const struct isapnp_device_id *ids,
		      int (*probe)(struct pci_dev *dev,
		                   const struct isapnp_device_id *id))
{
	
	struct pci_dev *dev;
	const struct isapnp_device_id *id;
	int count = 0;

	if (ids == NULL || probe == NULL)
		return -EINVAL;
	isapnp_for_each_dev(dev) {
		id = isapnp_match_dev(ids, dev);
		if (id != NULL && probe(dev, id) >= 0)
			count++;
	}
	return count;
}

int isapnp_activate_dev(struct pci_dev *dev, const char *name)
{
	int err;
	
	/* Device already active? Let's use it and inform the caller */
	if (dev->active)
		return -EBUSY;

	if ((err = dev->activate(dev)) < 0) {
		printk(KERN_ERR "isapnp: config of %s failed (out of resources?)[%d]\n", name, err);
		dev->deactivate(dev);
		return err;
	}

	return 0;
}

static unsigned int isapnp_dma_resource_flags(struct isapnp_dma *dma)
{
	return dma->flags | IORESOURCE_DMA | IORESOURCE_AUTO;
}

static unsigned int isapnp_mem_resource_flags(struct isapnp_mem *mem)
{
	unsigned int result;

	result = mem->flags | IORESOURCE_MEM | IORESOURCE_AUTO;
	if (!(mem->flags & IORESOURCE_MEM_WRITEABLE))
		result |= IORESOURCE_READONLY;
	if (mem->flags & IORESOURCE_MEM_CACHEABLE)
		result |= IORESOURCE_CACHEABLE;
	if (mem->flags & IORESOURCE_MEM_RANGELENGTH)
		result |= IORESOURCE_RANGELENGTH;
	if (mem->flags & IORESOURCE_MEM_SHADOWABLE)
		result |= IORESOURCE_SHADOWABLE;
	return result;
}

static unsigned int isapnp_irq_resource_flags(struct isapnp_irq *irq)
{
	return irq->flags | IORESOURCE_IRQ | IORESOURCE_AUTO;
}

static unsigned int isapnp_port_resource_flags(struct isapnp_port *port)
{
	return port->flags | IORESOURCE_IO | IORESOURCE_AUTO;
}

static int isapnp_config_prepare(struct pci_dev *dev)
{
	struct isapnp_resources *res, *resa;
	struct isapnp_port *port;
	struct isapnp_irq *irq;
	struct isapnp_dma *dma;
	struct isapnp_mem *mem;
	int port_count, port_count1;
	int irq_count, irq_count1;
	int dma_count, dma_count1;
	int mem_count, mem_count1;
	int idx;

	if (dev == NULL)
		return -EINVAL;
	if (dev->active || dev->ro)
		return -EBUSY;
	for (idx = 0; idx < DEVICE_COUNT_IRQ; idx++) {
		dev->irq_resource[idx].name = NULL;
		dev->irq_resource[idx].start = 0;
		dev->irq_resource[idx].end = 0;
		dev->irq_resource[idx].flags = 0;
	}
	for (idx = 0; idx < DEVICE_COUNT_DMA; idx++) {
		dev->dma_resource[idx].name = NULL;
		dev->dma_resource[idx].start = 0;
		dev->dma_resource[idx].end = 0;
		dev->dma_resource[idx].flags = 0;
	}
	for (idx = 0; idx < DEVICE_COUNT_RESOURCE; idx++) {
		dev->resource[idx].name = NULL;
		dev->resource[idx].start = 0;
		dev->resource[idx].end = 0;
		dev->resource[idx].flags = 0;
	}
	port_count = irq_count = dma_count = mem_count = 0;
	for (res = (struct isapnp_resources *)dev->sysdata; res; res = res->next) {
		port_count1 = irq_count1 = dma_count1 = mem_count1 = 0;
		for (resa = res; resa; resa = resa->alt) {
			for (port = resa->port, idx = 0; port; port = port->next, idx++) {
				if (dev->resource[port_count + idx].flags == 0) {
					dev->resource[port_count + idx].flags = isapnp_port_resource_flags(port);
					dev->resource[port_count + idx].end = port->size;
				}
			}
			if (port_count1 < idx)
				port_count1 = idx;
			for (irq = resa->irq, idx = 0; irq; irq = irq->next, idx++) {
				int count = irq_count + idx;
				if (count < DEVICE_COUNT_IRQ) {
					if (dev->irq_resource[count].flags == 0) {
						dev->irq_resource[count].flags = isapnp_irq_resource_flags(irq);
					}
				}
				
			}
			if (irq_count1 < idx)
				irq_count1 = idx;
			for (dma = resa->dma, idx = 0; dma; dma = dma->next, idx++)
				if (dev->dma_resource[idx].flags == 0) {
					dev->dma_resource[idx].flags = isapnp_dma_resource_flags(dma);
				}
			if (dma_count1 < idx)
				dma_count1 = idx;
			for (mem = resa->mem, idx = 0; mem; mem = mem->next, idx++)
				if (dev->resource[mem_count + idx + 8].flags == 0) {
					dev->resource[mem_count + idx + 8].flags = isapnp_mem_resource_flags(mem);
				}
			if (mem_count1 < idx)
				mem_count1 = idx;
		}
		port_count += port_count1;
		irq_count += irq_count1;
		dma_count += dma_count1;
		mem_count += mem_count1;
	}
	return 0;
}

struct isapnp_cfgtmp {
	struct isapnp_port *port[8];
	struct isapnp_irq *irq[2];
	struct isapnp_dma *dma[2];
	struct isapnp_mem *mem[4];
	struct pci_dev *request;
	struct pci_dev result;
};

static int isapnp_alternative_switch(struct isapnp_cfgtmp *cfg,
				     struct isapnp_resources *from,
				     struct isapnp_resources *to)
{
	int tmp, tmp1;
	struct isapnp_port *port;
	struct isapnp_irq *irq;
	struct isapnp_dma *dma;
	struct isapnp_mem *mem;

	if (!cfg)
		return -EINVAL;
	/* process port settings */
	for (tmp = 0; tmp < 8; tmp++) {
		if (!(cfg->request->resource[tmp].flags & IORESOURCE_AUTO))
			continue;		/* don't touch */
		port = cfg->port[tmp];
		if (!port) {
			cfg->port[tmp] = port = isapnp_find_port(cfg->request, tmp);
			if (!port)
				return -EINVAL;
		}
		if (from && port->res == from) {
			while (port->res != to) {
				if (!port->res->alt)
					return -EINVAL;
				port = port->res->alt->port;
				for (tmp1 = tmp; tmp1 > 0 && port; tmp1--)
					port = port->next;
				cfg->port[tmp] = port;
				if (!port)
					return -ENOENT;
				cfg->result.resource[tmp].flags = isapnp_port_resource_flags(port);
			}
		}
	}
	/* process irq settings */
	for (tmp = 0; tmp < 2; tmp++) {
		if (!(cfg->request->irq_resource[tmp].flags & IORESOURCE_AUTO))
			continue;		/* don't touch */
		irq = cfg->irq[tmp];
		if (!irq) {
			cfg->irq[tmp] = irq = isapnp_find_irq(cfg->request, tmp);
			if (!irq)
				return -EINVAL;
		}
		if (from && irq->res == from) {
			while (irq->res != to) {
				if (!irq->res->alt)
					return -EINVAL;
				irq = irq->res->alt->irq;
				for (tmp1 = tmp; tmp1 > 0 && irq; tmp1--)
					irq = irq->next;
				cfg->irq[tmp] = irq;
				if (!irq)
					return -ENOENT;
				cfg->result.irq_resource[tmp].flags = isapnp_irq_resource_flags(irq);
			}
		}
	}
	/* process dma settings */
	for (tmp = 0; tmp < 2; tmp++) {
		if (!(cfg->request->dma_resource[tmp].flags & IORESOURCE_AUTO))
			continue;		/* don't touch */
		dma = cfg->dma[tmp];
		if (!dma) {
			cfg->dma[tmp] = dma = isapnp_find_dma(cfg->request, tmp);
			if (!dma)
				return -EINVAL;
		}
		if (from && dma->res == from) {
			while (dma->res != to) {
				if (!dma->res->alt)
					return -EINVAL;
				dma = dma->res->alt->dma;
				for (tmp1 = tmp; tmp1 > 0 && dma; tmp1--)
					dma = dma->next;
				cfg->dma[tmp] = dma;
				if (!dma)
					return -ENOENT;
				cfg->result.dma_resource[tmp].flags = isapnp_dma_resource_flags(dma);
			}
		}
	}
	/* process memory settings */
	for (tmp = 0; tmp < 4; tmp++) {
		if (!(cfg->request->resource[tmp + 8].flags & IORESOURCE_AUTO))
			continue;		/* don't touch */
		mem = cfg->mem[tmp];
		if (!mem) {
			cfg->mem[tmp] = mem = isapnp_find_mem(cfg->request, tmp);
			if (!mem)
				return -EINVAL;
		}
		if (from && mem->res == from) {
			while (mem->res != to) {
				if (!mem->res->alt)
					return -EINVAL;
				mem = mem->res->alt->mem;
				for (tmp1 = tmp; tmp1 > 0 && mem; tmp1--)
					mem = mem->next;
				cfg->mem[tmp] = mem;
				if (!mem)
					return -ENOENT;
				cfg->result.resource[tmp + 8].flags = isapnp_mem_resource_flags(mem);
			}
		}
	}
	return 0;
}

static int isapnp_check_port(struct isapnp_cfgtmp *cfg, int port, int size, int idx)
{
	int i, tmp, rport, rsize;
	struct isapnp_port *xport;
	struct pci_dev *dev;

	if (check_region(port, size))
		return 1;
	for (i = 0; i < 8; i++) {
		rport = isapnp_reserve_io[i << 1];
		rsize = isapnp_reserve_io[(i << 1) + 1];
		if (port >= rport && port < rport + rsize)
			return 1;
		if (port + size > rport && port + size < (rport + rsize) - 1)
			return 1;
	}

	isapnp_for_each_dev(dev) {
		if (dev->active) {
			for (tmp = 0; tmp < 8; tmp++) {
				if (dev->resource[tmp].flags) {
					rport = dev->resource[tmp].start;
					rsize = (dev->resource[tmp].end - rport) + 1;
					if (port >= rport && port < rport + rsize)
						return 1;
					if (port + size > rport && port + size < (rport + rsize) - 1)
						return 1;
				}
			}
		}
	}
	for (i = 0; i < 8; i++) {
		unsigned int flags;
		if (i == idx)
			continue;
		flags = cfg->request->resource[i].flags;
		if (!flags)
			continue;
		tmp = cfg->request->resource[i].start;
		if (flags & IORESOURCE_AUTO) {		/* auto */
			xport = cfg->port[i];
			if (!xport)
				return 1;
			if (cfg->result.resource[i].flags & IORESOURCE_AUTO)
				continue;
			tmp = cfg->result.resource[i].start;
			if (tmp + xport->size >= port && tmp <= port + xport->size)
				return 1;
			continue;
		}
		if (port == tmp)
			return 1;
		xport = isapnp_find_port(cfg->request, i);
		if (!xport)
			return 1;
		if (tmp + xport->size >= port && tmp <= port + xport->size)
			return 1;
	}
	return 0;
}

static int isapnp_valid_port(struct isapnp_cfgtmp *cfg, int idx)
{
	int err;
	unsigned long *value1, *value2;
	struct isapnp_port *port;

	if (!cfg || idx < 0 || idx > 7)
		return -EINVAL;
	if (!(cfg->result.resource[idx].flags & IORESOURCE_AUTO)) /* don't touch */
		return 0;
      __again:
      	port = cfg->port[idx];
      	if (!port)
      		return -EINVAL;
      	value1 = &cfg->result.resource[idx].start;
      	value2 = &cfg->result.resource[idx].end;
	if (cfg->result.resource[idx].flags & IORESOURCE_AUTO) {
		cfg->result.resource[idx].flags &= ~IORESOURCE_AUTO;
		*value1 = port->min;
		*value2 = port->min + port->size - 1;
		if (!isapnp_check_port(cfg, *value1, port->size, idx))
			return 0;
	}
	do {
		*value1 += port->align;
		*value2 = *value1 + port->size - 1;
		if (*value1 > port->max || !port->align) {
			if (port->res && port->res->alt) {
				if ((err = isapnp_alternative_switch(cfg, port->res, port->res->alt))<0)
					return err;
				goto __again;
			}
			return -ENOENT;
		}
	} while (isapnp_check_port(cfg, *value1, port->size, idx));
	return 0;
}

static void isapnp_test_handler(int irq, void *dev_id, struct pt_regs *regs)
{
}

static int isapnp_check_interrupt(struct isapnp_cfgtmp *cfg, int irq, int idx)
{
	int i;
	struct pci_dev *dev;

	if (irq < 0 || irq > 15)
		return 1;
	for (i = 0; i < 16; i++) {
		if (isapnp_reserve_irq[i] == irq)
			return 1;
	}
	isapnp_for_each_dev(dev) {
		if (dev->active) {
			if ((dev->irq_resource[0].flags && dev->irq_resource[0].start == irq) ||
			    (dev->irq_resource[1].flags && dev->irq_resource[1].start == irq))
				return 1;
		}
	}
#ifdef CONFIG_PCI
	if (!isapnp_skip_pci_scan) {
		pci_for_each_dev(dev) {
			if (dev->irq == irq)
				return 1;
		}
	}
#endif
	if (request_irq(irq, isapnp_test_handler, SA_INTERRUPT, "isapnp", NULL))
		return 1;
	free_irq(irq, NULL);
	for (i = 0; i < DEVICE_COUNT_IRQ; i++) {
		if (i == idx)
			continue;
		if (!cfg->result.irq_resource[i].flags)
			continue;
		if (cfg->result.irq_resource[i].flags & IORESOURCE_AUTO)
			continue;
		if (cfg->result.irq_resource[i].start == irq)
			return 1;
	}
	return 0;
}

static int isapnp_valid_irq(struct isapnp_cfgtmp *cfg, int idx)
{
	/* IRQ priority: this table is good for i386 */
	static unsigned short xtab[16] = {
		5, 10, 11, 12, 9, 14, 15, 7, 3, 4, 13, 0, 1, 6, 8, 2
	};
	int err, i;
	unsigned long *value1, *value2;
	struct isapnp_irq *irq;

	if (!cfg || idx < 0 || idx > 1)
		return -EINVAL;
	if (!(cfg->result.irq_resource[idx].flags & IORESOURCE_AUTO))
		return 0;
      __again:
      	irq = cfg->irq[idx];
      	if (!irq)
      		return -EINVAL;
      	value1 = &cfg->result.irq_resource[idx].start;
      	value2 = &cfg->result.irq_resource[idx].end;
	if (cfg->result.irq_resource[idx].flags & IORESOURCE_AUTO) {
		for (i = 0; i < 16 && !(irq->map & (1<<xtab[i])); i++);
		if (i >= 16)
			return -ENOENT;
		cfg->result.irq_resource[idx].flags &= ~IORESOURCE_AUTO;
		if (!isapnp_check_interrupt(cfg, *value1 = *value2 = xtab[i], idx))
			return 0;
	}
	do {
		for (i = 0; i < 16 && xtab[i] != *value1; i++);
		for (i++; i < 16 && !(irq->map & (1<<xtab[i])); i++);
		if (i >= 16) {
			if (irq->res && irq->res->alt) {
				if ((err = isapnp_alternative_switch(cfg, irq->res, irq->res->alt))<0)
					return err;
				goto __again;
			}
			return -ENOENT;
		} else {
			*value1 = *value2 = xtab[i];
		}
	} while (isapnp_check_interrupt(cfg, *value1, idx));
	return 0;
}

static int isapnp_check_dma(struct isapnp_cfgtmp *cfg, int dma, int idx)
{
	int i, mindma =1;
	struct pci_dev *dev;

	/* Some machines allow DMA 0, but others don't. In fact on some 
	   boxes DMA 0 is the memory refresh. Play safe */
	if (isapnp_allow_dma0 == 1)
		mindma = 0;
	if (dma < mindma || dma == 4 || dma > 7)
		return 1;
	for (i = 0; i < 8; i++) {
		if (isapnp_reserve_dma[i] == dma)
			return 1;
	}
	isapnp_for_each_dev(dev) {
		if (dev->active) {
			if ((dev->dma_resource[0].flags && dev->dma_resource[0].start == dma) ||
			    (dev->dma_resource[1].flags && dev->dma_resource[1].start == dma))
				return 1;
		}
	}
	if (request_dma(dma, "isapnp"))
		return 1;
	free_dma(dma);
	for (i = 0; i < 2; i++) {
		if (i == idx)
			continue;
		if (!cfg->result.dma_resource[i].flags ||
		    (cfg->result.dma_resource[i].flags & IORESOURCE_AUTO))
			continue;
		if (cfg->result.dma_resource[i].start == dma)
			return 1;
	}
	return 0;
}

static int isapnp_valid_dma(struct isapnp_cfgtmp *cfg, int idx)
{
	/* DMA priority: this table is good for i386 */
	static unsigned short xtab[16] = {
		1, 3, 5, 6, 7, 0, 2, 4
	};
	int err, i;
	unsigned long *value1, *value2;
	struct isapnp_dma *dma;

	if (!cfg || idx < 0 || idx > 1)
		return -EINVAL;
	if (!(cfg->result.dma_resource[idx].flags & IORESOURCE_AUTO))	/* don't touch */
		return 0;
      __again:
      	dma = cfg->dma[idx];
      	if (!dma)
      		return -EINVAL;
      	value1 = &cfg->result.dma_resource[idx].start;
      	value2 = &cfg->result.dma_resource[idx].end;
	if (cfg->result.dma_resource[idx].flags & IORESOURCE_AUTO) {
		for (i = 0; i < 8 && !(dma->map & (1<<xtab[i])); i++);
		if (i >= 8)
			return -ENOENT;
		cfg->result.dma_resource[idx].flags &= ~IORESOURCE_AUTO;
		if (!isapnp_check_dma(cfg, *value1 = *value2 = xtab[i], idx))
			return 0;
	}
	do {
		for (i = 0; i < 8 && xtab[i] != *value1; i++);
		for (i++; i < 8 && !(dma->map & (1<<xtab[i])); i++);
		if (i >= 8) {
			if (dma->res && dma->res->alt) {
				if ((err = isapnp_alternative_switch(cfg, dma->res, dma->res->alt))<0)
					return err;
				goto __again;
			}
			return -ENOENT;
		} else {
			*value1 = *value2 = xtab[i];
		}
	} while (isapnp_check_dma(cfg, *value1, idx));
	return 0;
}

static int isapnp_check_mem(struct isapnp_cfgtmp *cfg, unsigned int addr, unsigned int size, int idx)
{
	int i, tmp;
	unsigned int raddr, rsize;
	struct isapnp_mem *xmem;
	struct pci_dev *dev;

	for (i = 0; i < 8; i++) {
		raddr = (unsigned int)isapnp_reserve_mem[i << 1];
		rsize = (unsigned int)isapnp_reserve_mem[(i << 1) + 1];
		if (addr >= raddr && addr < raddr + rsize)
			return 1;
		if (addr + size > raddr && addr + size < (raddr + rsize) - 1)
			return 1;
		if (__check_region(&iomem_resource, addr, size))
			return 1;
	}
	isapnp_for_each_dev(dev) {
		if (dev->active) {
			for (tmp = 0; tmp < 4; tmp++) {
				if (dev->resource[tmp].flags) {
					raddr = dev->resource[tmp + 8].start;
					rsize = (dev->resource[tmp + 8].end - raddr) + 1;
					if (addr >= raddr && addr < raddr + rsize)
						return 1;
					if (addr + size > raddr && addr + size < (raddr + rsize) - 1)
						return 1;
				}
			}
		}
	}
	for (i = 0; i < 4; i++) {
		unsigned int flags = cfg->request->resource[i + 8].flags;
		if (i == idx)
			continue;
		if (!flags)
			continue;
		tmp = cfg->result.resource[i + 8].start;
		if (flags & IORESOURCE_AUTO) {		/* auto */
			xmem = cfg->mem[i];
			if (!xmem)
				return 1;
			if (cfg->result.resource[i + 8].flags & IORESOURCE_AUTO)
				continue;
			if (tmp + xmem->size >= addr && tmp <= addr + xmem->size)
				return 1;
			continue;
		}
		if (addr == tmp)
			return 1;
		xmem = isapnp_find_mem(cfg->request, i);
		if (!xmem)
			return 1;
		if (tmp + xmem->size >= addr && tmp <= addr + xmem->size)
			return 1;
	}
	return 0;
}

static int isapnp_valid_mem(struct isapnp_cfgtmp *cfg, int idx)
{
	int err;
	unsigned long *value1, *value2;
	struct isapnp_mem *mem;

	if (!cfg || idx < 0 || idx > 3)
		return -EINVAL;
	if (!(cfg->result.resource[idx + 8].flags & IORESOURCE_AUTO)) /* don't touch */
		return 0;
      __again:
      	mem = cfg->mem[idx];
      	if (!mem)
      		return -EINVAL;
      	value1 = &cfg->result.resource[idx + 8].start;
      	value2 = &cfg->result.resource[idx + 8].end;
	if (cfg->result.resource[idx + 8].flags & IORESOURCE_AUTO) {
		cfg->result.resource[idx + 8].flags &= ~IORESOURCE_AUTO;
		*value1 = mem->min;
		*value2 = mem->min + mem->size - 1;
		if (!isapnp_check_mem(cfg, *value1, mem->size, idx))
			return 0;
	}
	do {
		*value1 += mem->align;
		*value2 = *value1 + mem->size - 1;
		if (*value1 > mem->max || !mem->align) {
			if (mem->res && mem->res->alt) {
				if ((err = isapnp_alternative_switch(cfg, mem->res, mem->res->alt))<0)
					return err;
				goto __again;
			}
			return -ENOENT;
		}
	} while (isapnp_check_mem(cfg, *value1, mem->size, idx));
	return 0;
}

static int isapnp_check_valid(struct isapnp_cfgtmp *cfg)
{
	int tmp;
	
	for (tmp = 0; tmp < 8; tmp++)
		if (cfg->result.resource[tmp].flags & IORESOURCE_AUTO)
			return -EAGAIN;
	for (tmp = 0; tmp < 2; tmp++)
		if (cfg->result.irq_resource[tmp].flags & IORESOURCE_AUTO)
			return -EAGAIN;
	for (tmp = 0; tmp < 2; tmp++)
		if (cfg->result.dma_resource[tmp].flags & IORESOURCE_AUTO)
			return -EAGAIN;
	for (tmp = 0; tmp < 4; tmp++)
		if (cfg->result.resource[tmp + 8].flags & IORESOURCE_AUTO)
			return -EAGAIN;
	return 0;
}

static int isapnp_config_activate(struct pci_dev *dev)
{
	struct isapnp_cfgtmp cfg;
	int tmp, fauto, err;
	
	if (!dev)
		return -EINVAL;
	if (dev->active)
		return -EBUSY;
	memset(&cfg, 0, sizeof(cfg));
	cfg.request = dev;
	memcpy(&cfg.result, dev, sizeof(struct pci_dev));
	/* check if all values are set, otherwise try auto-configuration */
	for (tmp = fauto = 0; !fauto && tmp < 8; tmp++) {
		if (dev->resource[tmp].flags & IORESOURCE_AUTO)
			fauto++;
	}
	for (tmp = 0; !fauto && tmp < 2; tmp++) {
		if (dev->irq_resource[tmp].flags & IORESOURCE_AUTO)
			fauto++;
	}
	for (tmp = 0; !fauto && tmp < 2; tmp++) {
		if (dev->dma_resource[tmp].flags & IORESOURCE_AUTO)
			fauto++;
	}
	for (tmp = 0; !fauto && tmp < 4; tmp++) {
		if (dev->resource[tmp + 8].flags & IORESOURCE_AUTO)
			fauto++;
	}
	if (!fauto)
		goto __skip_auto;
	/* set variables to initial values */
	if ((err = isapnp_alternative_switch(&cfg, NULL, NULL))<0)
		return err;
	/* find first valid configuration */
	fauto = 0;
	do {
		for (tmp = 0; tmp < 8 && cfg.result.resource[tmp].flags; tmp++)
			if ((err = isapnp_valid_port(&cfg, tmp))<0)
				return err;
		for (tmp = 0; tmp < 2 && cfg.result.irq_resource[tmp].flags; tmp++)
			if ((err = isapnp_valid_irq(&cfg, tmp))<0)
				return err;
		for (tmp = 0; tmp < 2 && cfg.result.dma_resource[tmp].flags; tmp++)
			if ((err = isapnp_valid_dma(&cfg, tmp))<0)
				return err;
		for (tmp = 0; tmp < 4 && cfg.result.resource[tmp + 8].flags; tmp++)
			if ((err = isapnp_valid_mem(&cfg, tmp))<0)
				return err;
	} while (isapnp_check_valid(&cfg)<0 && fauto++ < 20);
	if (fauto >= 20)
		return -EAGAIN;
      __skip_auto:
      	/* we have valid configuration, try configure hardware */
      	isapnp_cfg_begin(dev->bus->number, dev->devfn);
	dev->active = 1;
	dev->irq_resource[0] = cfg.result.irq_resource[0];
	dev->irq_resource[1] = cfg.result.irq_resource[1];
	dev->dma_resource[0] = cfg.result.dma_resource[0];
	dev->dma_resource[1] = cfg.result.dma_resource[1];
	for (tmp = 0; tmp < 12; tmp++) {
		dev->resource[tmp] = cfg.result.resource[tmp];
	}	
	for (tmp = 0; tmp < 8 && dev->resource[tmp].flags; tmp++)
		isapnp_write_word(ISAPNP_CFG_PORT+(tmp<<1), dev->resource[tmp].start);
	for (tmp = 0; tmp < 2 && dev->irq_resource[tmp].flags; tmp++) {
		int irq = dev->irq_resource[tmp].start;
		if (irq == 2)
			irq = 9;
		isapnp_write_byte(ISAPNP_CFG_IRQ+(tmp<<1), irq);
	}
	for (tmp = 0; tmp < 2 && dev->dma_resource[tmp].flags; tmp++)
		isapnp_write_byte(ISAPNP_CFG_DMA+tmp, dev->dma_resource[tmp].start);
	for (tmp = 0; tmp < 4 && dev->resource[tmp+8].flags; tmp++)
		isapnp_write_word(ISAPNP_CFG_MEM+(tmp<<2), (dev->resource[tmp + 8].start >> 8) & 0xffff);
	isapnp_activate(dev->devfn);
	isapnp_cfg_end();
	return 0;
}

static int isapnp_config_deactivate(struct pci_dev *dev)
{
	if (!dev || !dev->active)
		return -EINVAL;
      	isapnp_cfg_begin(dev->bus->number, dev->devfn);
	isapnp_deactivate(dev->devfn);
	dev->active = 0;
	isapnp_cfg_end();
	return 0;
}

void isapnp_resource_change(struct resource *resource,
			    unsigned long start,
			    unsigned long size)
{
	if (resource == NULL)
		return;
	resource->flags &= ~IORESOURCE_AUTO;
	resource->start = start;
	resource->end = start + size - 1;
}

/*
 *  Inititialization.
 */

#ifdef MODULE

static void isapnp_free_port(struct isapnp_port *port)
{
	struct isapnp_port *next;

	while (port) {
		next = port->next;
		kfree(port);
		port = next;
	}
}

static void isapnp_free_irq(struct isapnp_irq *irq)
{
	struct isapnp_irq *next;

	while (irq) {
		next = irq->next;
		kfree(irq);
		irq = next;
	}
}

static void isapnp_free_dma(struct isapnp_dma *dma)
{
	struct isapnp_dma *next;

	while (dma) {
		next = dma->next;
		kfree(dma);
		dma = next;
	}
}

static void isapnp_free_mem(struct isapnp_mem *mem)
{
	struct isapnp_mem *next;

	while (mem) {
		next = mem->next;
		kfree(mem);
		mem = next;
	}
}

static void isapnp_free_mem32(struct isapnp_mem32 *mem32)
{
	struct isapnp_mem32 *next;

	while (mem32) {
		next = mem32->next;
		kfree(mem32);
		mem32 = next;
	}
}

static void isapnp_free_resources(struct isapnp_resources *resources, int alt)
{
	struct isapnp_resources *next;

	while (resources) {
		next = alt ? resources->alt : resources->next;
		isapnp_free_port(resources->port);
		isapnp_free_irq(resources->irq);
		isapnp_free_dma(resources->dma);
		isapnp_free_mem(resources->mem);
		isapnp_free_mem32(resources->mem32);
		if (!alt && resources->alt)
			isapnp_free_resources(resources->alt, 1);
		kfree(resources);
		resources = next;
	}
}

static void isapnp_free_card(struct pci_bus *card)
{
	while (!list_empty(&card->devices)) {
		struct list_head *list = card->devices.next;
		struct pci_dev *dev = pci_dev_b(list);
		list_del(list);
		isapnp_free_resources((struct isapnp_resources *)dev->sysdata, 0);
		kfree(dev);
	}
	kfree(card);
}

static void isapnp_free_all_resources(void)
{
#ifdef ISAPNP_REGION_OK
	release_region(_PIDXR, 1);
#endif
	release_region(_PNPWRP, 1);
	release_region(isapnp_rdp, 1);
#ifdef CONFIG_PROC_FS
	isapnp_proc_done();
#endif
	while (!list_empty(&isapnp_cards)) {
		struct list_head *list = isapnp_cards.next;
		list_del(list);
		isapnp_free_card(pci_bus_b(list));
	}
}

#endif /* MODULE */

static int isapnp_announce_device(struct isapnp_driver *drv, 
				  struct pci_dev *dev)
{
	const struct isapnp_device_id *id;
	int ret = 0;

	if (drv->id_table) {
		id = isapnp_match_dev(drv->id_table, dev);
		if (!id) {
			ret = 0;
			goto out;
		}
	} else
		id = NULL;

	if (drv->probe(dev, id) >= 0) {
		dev->driver = (struct pci_driver *) drv;
		ret = 1;
	}
out:
	return ret;
}

/**
 * isapnp_dev_driver - get the isapnp_driver of a device
 * @dev: the device to query
 *
 * Returns the appropriate isapnp_driver structure or %NULL if there is no 
 * registered driver for the device.
 */
static struct isapnp_driver *isapnp_dev_driver(const struct pci_dev *dev)
{
	return (struct isapnp_driver *) dev->driver;
}

static LIST_HEAD(isapnp_drivers);

/**
 * isapnp_register_driver - register a new ISAPnP driver
 * @drv: the driver structure to register
 * 
 * Adds the driver structure to the list of registered ISAPnP drivers
 * Returns the number of isapnp devices which were claimed by the driver
 * during registration.  The driver remains registered even if the
 * return value is zero.
 */
int isapnp_register_driver(struct isapnp_driver *drv)
{
	struct pci_dev *dev;
	int count = 0;

	list_add_tail(&drv->node, &isapnp_drivers);

	isapnp_for_each_dev(dev) {
		if (!isapnp_dev_driver(dev))
			count += isapnp_announce_device(drv, dev);
	}
	return count;
}

/**
 * isapnp_unregister_driver - unregister an isapnp driver
 * @drv: the driver structure to unregister
 * 
 * Deletes the driver structure from the list of registered ISAPnP drivers,
 * gives it a chance to clean up by calling its remove() function for
 * each device it was responsible for, and marks those devices as
 * driverless.
 */
void isapnp_unregister_driver(struct isapnp_driver *drv)
{
	struct pci_dev *dev;

	list_del(&drv->node);
	isapnp_for_each_dev(dev) {
		if (dev->driver == (struct pci_driver *) drv) {
			if (drv->remove)
				drv->remove(dev);
			dev->driver = NULL;
		}
	}
}

EXPORT_SYMBOL(isapnp_cards);
EXPORT_SYMBOL(isapnp_devices);
EXPORT_SYMBOL(isapnp_present);
EXPORT_SYMBOL(isapnp_cfg_begin);
EXPORT_SYMBOL(isapnp_cfg_end);
EXPORT_SYMBOL(isapnp_read_byte);
EXPORT_SYMBOL(isapnp_read_word);
EXPORT_SYMBOL(isapnp_read_dword);
EXPORT_SYMBOL(isapnp_write_byte);
EXPORT_SYMBOL(isapnp_write_word);
EXPORT_SYMBOL(isapnp_write_dword);
EXPORT_SYMBOL(isapnp_wake);
EXPORT_SYMBOL(isapnp_device);
EXPORT_SYMBOL(isapnp_activate);
EXPORT_SYMBOL(isapnp_deactivate);
EXPORT_SYMBOL(isapnp_find_card);
EXPORT_SYMBOL(isapnp_find_dev);
EXPORT_SYMBOL(isapnp_probe_cards);
EXPORT_SYMBOL(isapnp_probe_devs);
EXPORT_SYMBOL(isapnp_activate_dev);
EXPORT_SYMBOL(isapnp_resource_change);
EXPORT_SYMBOL(isapnp_register_driver);
EXPORT_SYMBOL(isapnp_unregister_driver);

int __init isapnp_init(void)
{
	int cards;
	struct pci_bus *card;

	if (isapnp_disable) {
		isapnp_detected = 0;
		printk(KERN_INFO "isapnp: ISA Plug & Play support disabled\n");
		return 0;
	}
#ifdef ISAPNP_REGION_OK
	if (!request_region(_PIDXR, 1, "isapnp index")) {
		printk(KERN_ERR "isapnp: Index Register 0x%x already used\n", _PIDXR);
		return -EBUSY;
	}
#endif
	if (!request_region(_PNPWRP, 1, "isapnp write")) {
		printk(KERN_ERR "isapnp: Write Data Register 0x%x already used\n", _PNPWRP);
#ifdef ISAPNP_REGION_OK
		release_region(_PIDXR, 1);
#endif
		return -EBUSY;
	}
	
	/*
	 *	Print a message. The existing ISAPnP code is hanging machines
	 *	so let the user know where.
	 */
	 
	printk(KERN_INFO "isapnp: Scanning for PnP cards...\n");
	if (isapnp_rdp >= 0x203 && isapnp_rdp <= 0x3ff) {
		isapnp_rdp |= 3;
		if (!request_region(isapnp_rdp, 1, "isapnp read")) {
			printk(KERN_ERR "isapnp: Read Data Register 0x%x already used\n", isapnp_rdp);
#ifdef ISAPNP_REGION_OK
			release_region(_PIDXR, 1);
#endif
			release_region(_PNPWRP, 1);
			return -EBUSY;
		}
		isapnp_set_rdp();
	}
	isapnp_detected = 1;
	if (isapnp_rdp < 0x203 || isapnp_rdp > 0x3ff) {
		cards = isapnp_isolate();
		if (cards < 0 || 
		    (isapnp_rdp < 0x203 || isapnp_rdp > 0x3ff)) {
#ifdef ISAPNP_REGION_OK
			release_region(_PIDXR, 1);
#endif
			release_region(_PNPWRP, 1);
			isapnp_detected = 0;
			printk(KERN_INFO "isapnp: No Plug & Play device found\n");
			return 0;
		}
		request_region(isapnp_rdp, 1, "isapnp read");
	}
	isapnp_build_device_list();
	cards = 0;

	isapnp_for_each_card(card) {
		cards++;
		if (isapnp_verbose) {
			struct list_head *devlist;
			printk(KERN_INFO "isapnp: Card '%s'\n", card->name[0]?card->name:"Unknown");
			if (isapnp_verbose < 2)
				continue;
			for (devlist = card->devices.next; devlist != &card->devices; devlist = devlist->next) {
				struct pci_dev *dev = pci_dev_b(devlist);
				printk(KERN_INFO "isapnp:   Device '%s'\n", dev->name[0]?card->name:"Unknown");
			}
		}
	}
	if (cards) {
		printk(KERN_INFO "isapnp: %i Plug & Play card%s detected total\n", cards, cards>1?"s":"");
	} else {
		printk(KERN_INFO "isapnp: No Plug & Play card found\n");
	}
#ifdef CONFIG_PROC_FS
	isapnp_proc_init();
#endif
	return 0;
}

#ifdef MODULE

int init_module(void)
{
	return isapnp_init();
}

void cleanup_module(void)
{
	if (isapnp_detected)
		isapnp_free_all_resources();
}

#else

/* format is: noisapnp */

static int __init isapnp_setup_disable(char *str)
{
	isapnp_disable = 1;
	return 1;
}

__setup("noisapnp", isapnp_setup_disable);

/* format is: isapnp=rdp,reset,skip_pci_scan,verbose */

static int __init isapnp_setup_isapnp(char *str)
{
	(void)((get_option(&str,&isapnp_rdp) == 2) &&
	       (get_option(&str,&isapnp_reset) == 2) &&
	       (get_option(&str,&isapnp_skip_pci_scan) == 2) &&
	       (get_option(&str,&isapnp_verbose) == 2));
	return 1;
}

__setup("isapnp=", isapnp_setup_isapnp);

/* format is: isapnp_reserve_irq=irq1[,irq2] .... */

static int __init isapnp_setup_reserve_irq(char *str)
{
	int i;

	for (i = 0; i < 16; i++)
		if (get_option(&str,&isapnp_reserve_irq[i]) != 2)
			break;
	return 1;
}

__setup("isapnp_reserve_irq=", isapnp_setup_reserve_irq);

/* format is: isapnp_reserve_dma=dma1[,dma2] .... */

static int __init isapnp_setup_reserve_dma(char *str)
{
	int i;

	for (i = 0; i < 8; i++)
		if (get_option(&str,&isapnp_reserve_dma[i]) != 2)
			break;
	return 1;
}

__setup("isapnp_reserve_dma=", isapnp_setup_reserve_dma);

/* format is: isapnp_reserve_io=io1,size1[,io2,size2] .... */

static int __init isapnp_setup_reserve_io(char *str)
{
	int i;

	for (i = 0; i < 16; i++)
		if (get_option(&str,&isapnp_reserve_io[i]) != 2)
			break;
	return 1;
}

__setup("isapnp_reserve_io=", isapnp_setup_reserve_io);

/* format is: isapnp_reserve_mem=mem1,size1[,mem2,size2] .... */

static int __init isapnp_setup_reserve_mem(char *str)
{
	int i;

	for (i = 0; i < 16; i++)
		if (get_option(&str,&isapnp_reserve_mem[i]) != 2)
			break;
	return 1;
}

__setup("isapnp_reserve_mem=", isapnp_setup_reserve_mem);

#endif
