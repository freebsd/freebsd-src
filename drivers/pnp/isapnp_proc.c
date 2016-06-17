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
 */

#define __NO_VERSION__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <linux/smp_lock.h>
#include <linux/isapnp.h>

struct isapnp_info_buffer {
	char *buffer;		/* pointer to begin of buffer */
	char *curr;		/* current position in buffer */
	unsigned long size;	/* current size */
	unsigned long len;	/* total length of buffer */
	int stop;		/* stop flag */
	int error;		/* error code */
};

typedef struct isapnp_info_buffer isapnp_info_buffer_t;

static struct proc_dir_entry *isapnp_proc_entry = NULL;
static struct proc_dir_entry *isapnp_proc_bus_dir = NULL;
static struct proc_dir_entry *isapnp_proc_devices_entry = NULL;

static void isapnp_info_read(isapnp_info_buffer_t *buffer);
static void isapnp_info_write(isapnp_info_buffer_t *buffer);

int isapnp_printf(isapnp_info_buffer_t * buffer, char *fmt,...)
{
	va_list args;
	int res;
	char sbuffer[512];

	if (buffer->stop || buffer->error)
		return 0;
	va_start(args, fmt);
	res = vsprintf(sbuffer, fmt, args);
	va_end(args);
	if (buffer->size + res >= buffer->len) {
		buffer->stop = 1;
		return 0;
	}
	strcpy(buffer->curr, sbuffer);
	buffer->curr += res;
	buffer->size += res;
	return res;
}

static void isapnp_devid(char *str, unsigned short vendor, unsigned short device)
{
	sprintf(str, "%c%c%c%x%x%x%x",
			'A' + ((vendor >> 2) & 0x3f) - 1,
			'A' + (((vendor & 3) << 3) | ((vendor >> 13) & 7)) - 1,
			'A' + ((vendor >> 8) & 0x1f) - 1,
			(device >> 4) & 0x0f,
			device & 0x0f,
			(device >> 12) & 0x0f,
			(device >> 8) & 0x0f);
}

static loff_t isapnp_info_entry_lseek(struct file *file, loff_t offset, int orig)
{
	switch (orig) {
	case 0:	/* SEEK_SET */
		file->f_pos = offset;
		return file->f_pos;
	case 1:	/* SEEK_CUR */
		file->f_pos += offset;
		return file->f_pos;
	case 2:	/* SEEK_END */
	default:
		return -EINVAL;
	}
	return -ENXIO;
}

static ssize_t isapnp_info_entry_read(struct file *file, char *buffer,
				      size_t count, loff_t * offset)
{
	isapnp_info_buffer_t *buf;
	long size = 0, size1;
	int mode;

	mode = file->f_flags & O_ACCMODE;
	if (mode != O_RDONLY)
		return -EINVAL;
	buf = (isapnp_info_buffer_t *) file->private_data;
	if (!buf)
		return -EIO;
	if (file->f_pos >= buf->size)
		return 0;
	size = buf->size < count ? buf->size : count;
	size1 = buf->size - file->f_pos;
	if (size1 < size)
		size = size1;
	if (copy_to_user(buffer, buf->buffer + file->f_pos, size))
		return -EFAULT;
	file->f_pos += size;
	return size;
}

static ssize_t isapnp_info_entry_write(struct file *file, const char *buffer,
				       size_t count, loff_t * offset)
{
	isapnp_info_buffer_t *buf;
	long size = 0, size1;
	int mode;

	mode = file->f_flags & O_ACCMODE;
	if (mode != O_WRONLY)
		return -EINVAL;
	buf = (isapnp_info_buffer_t *) file->private_data;
	if (!buf)
		return -EIO;
	if (file->f_pos < 0)
		return -EINVAL;
	if (file->f_pos >= buf->len)
		return -ENOMEM;
	size = buf->len < count ? buf->len : count;
	size1 = buf->len - file->f_pos;
	if (size1 < size)
		size = size1;
	if (copy_from_user(buf->buffer + file->f_pos, buffer, size))
		return -EFAULT;
	if (buf->size < file->f_pos + size)
		buf->size = file->f_pos + size;
	file->f_pos += size;
	return size;
}

static int isapnp_info_entry_open(struct inode *inode, struct file *file)
{
	isapnp_info_buffer_t *buffer;
	int mode;

	mode = file->f_flags & O_ACCMODE;
	if (mode != O_RDONLY && mode != O_WRONLY)
		return -EINVAL;
	buffer = (isapnp_info_buffer_t *)
				isapnp_alloc(sizeof(isapnp_info_buffer_t));
	if (!buffer)
		return -ENOMEM;
	buffer->len = 4 * PAGE_SIZE;
	buffer->buffer = vmalloc(buffer->len);
	if (!buffer->buffer) {
		kfree(buffer);
		return -ENOMEM;
	}
	lock_kernel();
	buffer->curr = buffer->buffer;
	file->private_data = buffer;
	if (mode == O_RDONLY)
		isapnp_info_read(buffer);
	unlock_kernel();
	return 0;
}

static int isapnp_info_entry_release(struct inode *inode, struct file *file)
{
	isapnp_info_buffer_t *buffer;
	int mode;

	if ((buffer = (isapnp_info_buffer_t *) file->private_data) == NULL)
		return -EINVAL;
	mode = file->f_flags & O_ACCMODE;
	lock_kernel();
	if (mode == O_WRONLY)
		isapnp_info_write(buffer);
	vfree(buffer->buffer);
	kfree(buffer);
	unlock_kernel();
	return 0;
}

static unsigned int isapnp_info_entry_poll(struct file *file, poll_table * wait)
{
	if (!file->private_data)
		return 0;
	return POLLIN | POLLRDNORM;
}

static struct file_operations isapnp_info_entry_operations =
{
	llseek:		isapnp_info_entry_lseek,
	read:		isapnp_info_entry_read,
	write:		isapnp_info_entry_write,
	poll:		isapnp_info_entry_poll,
	open:		isapnp_info_entry_open,
	release:	isapnp_info_entry_release,
};

static loff_t isapnp_proc_bus_lseek(struct file *file, loff_t off, int whence)
{
	loff_t new;
	
	switch (whence) {
	case 0:
		new = off;
		break;
	case 1:
		new = file->f_pos + off;
		break;
	case 2:
		new = 256 + off;
		break;
	default:
		return -EINVAL;
	}
	if (new < 0 || new > 256)
		return -EINVAL;
	return (file->f_pos = new);
}

static ssize_t isapnp_proc_bus_read(struct file *file, char *buf, size_t nbytes, loff_t *ppos)
{
	struct inode *ino = file->f_dentry->d_inode;
	struct proc_dir_entry *dp = ino->u.generic_ip;
	struct pci_dev *dev = dp->data;
	int pos = *ppos;
	int cnt, size = 256;

	if (pos >= size)
		return 0;
	if (nbytes >= size)
		nbytes = size;
	if (pos + nbytes > size)
		nbytes = size - pos;
	cnt = nbytes;

	if (!access_ok(VERIFY_WRITE, buf, cnt))
		return -EINVAL;
		
	isapnp_cfg_begin(dev->bus->number, dev->devfn);
	for ( ; pos < 256 && cnt > 0; pos++, buf++, cnt--) {
		unsigned char val;
		val = isapnp_read_byte(pos);
		__put_user(val, buf);
	}
	isapnp_cfg_end();
	
	*ppos = pos;
	return nbytes;
}

static struct file_operations isapnp_proc_bus_file_operations =
{
	llseek:		isapnp_proc_bus_lseek,
	read:		isapnp_proc_bus_read,
};

static int isapnp_proc_attach_device(struct pci_dev *dev)
{
	struct pci_bus *bus = dev->bus;
	struct proc_dir_entry *de, *e;
	char name[16];

	if (!(de = bus->procdir)) {
		sprintf(name, "%02x", bus->number);
		de = bus->procdir = proc_mkdir(name, isapnp_proc_bus_dir);
		if (!de)
			return -ENOMEM;
	}
	sprintf(name, "%02x", dev->devfn);
	e = dev->procent = create_proc_entry(name, S_IFREG | S_IRUGO, de);
	if (!e)
		return -ENOMEM;
	e->proc_fops = &isapnp_proc_bus_file_operations;
	e->owner = THIS_MODULE;
	e->data = dev;
	e->size = 256;
	return 0;
}

#ifdef MODULE
static int __exit isapnp_proc_detach_device(struct pci_dev *dev)
{
	struct pci_bus *bus = dev->bus;
	struct proc_dir_entry *de;
	char name[16];

	if (!(de = bus->procdir))
		return -EINVAL;
	sprintf(name, "%02x", dev->devfn);
	remove_proc_entry(name, de);
	return 0;
}

static int __exit isapnp_proc_detach_bus(struct pci_bus *bus)
{
	struct proc_dir_entry *de;
	char name[16];

	if (!(de = bus->procdir))
		return -EINVAL;
	sprintf(name, "%02x", bus->number);
	remove_proc_entry(name, isapnp_proc_bus_dir);
	return 0;
}
#endif

static int isapnp_proc_read_devices(char *buf, char **start, off_t pos, int count)
{
	struct pci_dev *dev;
	off_t at = 0;
	int len, cnt, i;
	
	cnt = 0;
	isapnp_for_each_dev(dev) {
		char bus_id[8], device_id[8];
	
		isapnp_devid(bus_id, dev->bus->vendor, dev->bus->device);
		isapnp_devid(device_id, dev->vendor, dev->device);
		len = sprintf(buf, "%02x%02x\t%s%s\t",
			dev->bus->number,
			dev->devfn,
			bus_id,
			device_id);
		isapnp_cfg_begin(dev->bus->number, dev->devfn);
		len += sprintf(buf+len, "%02x", isapnp_read_byte(ISAPNP_CFG_ACTIVATE));
		for (i = 0; i < 8; i++)
			len += sprintf(buf+len, "%04x", isapnp_read_word(ISAPNP_CFG_PORT + (i << 1)));
		for (i = 0; i < 2; i++)
			len += sprintf(buf+len, "%04x", isapnp_read_word(ISAPNP_CFG_IRQ + (i << 1)));
		for (i = 0; i < 2; i++)
			len += sprintf(buf+len, "%04x", isapnp_read_word(ISAPNP_CFG_DMA + i));
		for (i = 0; i < 4; i++)
			len += sprintf(buf+len, "%08x", isapnp_read_dword(ISAPNP_CFG_MEM + (i << 3)));
		isapnp_cfg_end();
		buf[len++] = '\n';
		at += len;
		if (at >= pos) {
			if (!*start) {
				*start = buf + (pos - (at - len));
				cnt = at - pos;
			} else
				cnt += len;
			buf += len;
		}
	}
	return (count > cnt) ? cnt : count;
}

int __init isapnp_proc_init(void)
{
	struct proc_dir_entry *p;
	struct pci_dev *dev;

	isapnp_proc_entry = NULL;
	p = create_proc_entry("isapnp", S_IFREG | S_IRUGO | S_IWUSR, &proc_root);
	if (p) {
		p->proc_fops = &isapnp_info_entry_operations;
		p->owner = THIS_MODULE;
	}
	isapnp_proc_entry = p;
	isapnp_proc_bus_dir = proc_mkdir("isapnp", proc_bus);
	isapnp_proc_devices_entry = create_proc_info_entry("devices", 0,
							   isapnp_proc_bus_dir,
							   isapnp_proc_read_devices);
	isapnp_for_each_dev(dev) {
		isapnp_proc_attach_device(dev);
	}
	return 0;
}

#ifdef MODULE
int __exit isapnp_proc_done(void)
{
	struct pci_dev *dev;
	struct pci_bus *card;

	isapnp_for_each_dev(dev) {
		isapnp_proc_detach_device(dev);
	}
	isapnp_for_each_card(card) {
		isapnp_proc_detach_bus(card);
	}
	if (isapnp_proc_devices_entry)
		remove_proc_entry("devices", isapnp_proc_devices_entry);
	if (isapnp_proc_bus_dir)
		remove_proc_entry("isapnp", proc_bus);
	if (isapnp_proc_entry)
		remove_proc_entry("isapnp", &proc_root);
	return 0;
}
#endif /* MODULE */

/*
 *
 */

static void isapnp_print_devid(isapnp_info_buffer_t *buffer, unsigned short vendor, unsigned short device)
{
	char tmp[8];
	
	isapnp_devid(tmp, vendor, device);
	isapnp_printf(buffer, tmp);
}

static void isapnp_print_compatible(isapnp_info_buffer_t *buffer, struct pci_dev *dev)
{
	int idx;

	for (idx = 0; idx < DEVICE_COUNT_COMPATIBLE; idx++) {
		if (dev->vendor_compatible[idx] == 0)
			continue;
		isapnp_printf(buffer, "    Compatible device ");
		isapnp_print_devid(buffer,
				   dev->vendor_compatible[idx],
				   dev->device_compatible[idx]);
		isapnp_printf(buffer, "\n");
	}
}

static void isapnp_print_port(isapnp_info_buffer_t *buffer, char *space, struct isapnp_port *port)
{
	isapnp_printf(buffer, "%sPort 0x%x-0x%x, align 0x%x, size 0x%x, %i-bit address decoding\n",
			space, port->min, port->max, port->align ? (port->align-1) : 0, port->size,
			port->flags & ISAPNP_PORT_FLAG_16BITADDR ? 16 : 10);
}

static void isapnp_print_irq(isapnp_info_buffer_t *buffer, char *space, struct isapnp_irq *irq)
{
	int first = 1, i;

	isapnp_printf(buffer, "%sIRQ ", space);
	for (i = 0; i < 16; i++)
		if (irq->map & (1<<i)) {
			if (!first) {
				isapnp_printf(buffer, ",");
			} else {
				first = 0;
			}
			if (i == 2 || i == 9)
				isapnp_printf(buffer, "2/9");
			else
				isapnp_printf(buffer, "%i", i);
		}
	if (!irq->map)
		isapnp_printf(buffer, "<none>");
	if (irq->flags & IORESOURCE_IRQ_HIGHEDGE)
		isapnp_printf(buffer, " High-Edge");
	if (irq->flags & IORESOURCE_IRQ_LOWEDGE)
		isapnp_printf(buffer, " Low-Edge");
	if (irq->flags & IORESOURCE_IRQ_HIGHLEVEL)
		isapnp_printf(buffer, " High-Level");
	if (irq->flags & IORESOURCE_IRQ_LOWLEVEL)
		isapnp_printf(buffer, " Low-Level");
	isapnp_printf(buffer, "\n");
}

static void isapnp_print_dma(isapnp_info_buffer_t *buffer, char *space, struct isapnp_dma *dma)
{
	int first = 1, i;
	char *s;

	isapnp_printf(buffer, "%sDMA ", space);
	for (i = 0; i < 8; i++)
		if (dma->map & (1<<i)) {
			if (!first) {
				isapnp_printf(buffer, ",");
			} else {
				first = 0;
			}
			isapnp_printf(buffer, "%i", i);
		}
	if (!dma->map)
		isapnp_printf(buffer, "<none>");
	switch (dma->flags & IORESOURCE_DMA_TYPE_MASK) {
	case IORESOURCE_DMA_8BIT:
		s = "8-bit";
		break;
	case IORESOURCE_DMA_8AND16BIT:
		s = "8-bit&16-bit";
		break;
	default:
		s = "16-bit";
	}
	isapnp_printf(buffer, " %s", s);
	if (dma->flags & IORESOURCE_DMA_MASTER)
		isapnp_printf(buffer, " master");
	if (dma->flags & IORESOURCE_DMA_BYTE)
		isapnp_printf(buffer, " byte-count");
	if (dma->flags & IORESOURCE_DMA_WORD)
		isapnp_printf(buffer, " word-count");
	switch (dma->flags & IORESOURCE_DMA_SPEED_MASK) {
	case IORESOURCE_DMA_TYPEA:
		s = "type-A";
		break;
	case IORESOURCE_DMA_TYPEB:
		s = "type-B";
		break;
	case IORESOURCE_DMA_TYPEF:
		s = "type-F";
		break;
	default:
		s = "compatible";
		break;
	}
	isapnp_printf(buffer, " %s\n", s);
}

static void isapnp_print_mem(isapnp_info_buffer_t *buffer, char *space, struct isapnp_mem *mem)
{
	char *s;

	isapnp_printf(buffer, "%sMemory 0x%x-0x%x, align 0x%x, size 0x%x",
			space, mem->min, mem->max, mem->align, mem->size);
	if (mem->flags & IORESOURCE_MEM_WRITEABLE)
		isapnp_printf(buffer, ", writeable");
	if (mem->flags & IORESOURCE_MEM_CACHEABLE)
		isapnp_printf(buffer, ", cacheable");
	if (mem->flags & IORESOURCE_MEM_RANGELENGTH)
		isapnp_printf(buffer, ", range-length");
	if (mem->flags & IORESOURCE_MEM_SHADOWABLE)
		isapnp_printf(buffer, ", shadowable");
	if (mem->flags & IORESOURCE_MEM_EXPANSIONROM)
		isapnp_printf(buffer, ", expansion ROM");
	switch (mem->flags & IORESOURCE_MEM_TYPE_MASK) {
	case IORESOURCE_MEM_8BIT:
		s = "8-bit";
		break;
	case IORESOURCE_MEM_8AND16BIT:
		s = "8-bit&16-bit";
		break;
	default:
		s = "16-bit";
	}
	isapnp_printf(buffer, ", %s\n", s);
}

static void isapnp_print_mem32(isapnp_info_buffer_t *buffer, char *space, struct isapnp_mem32 *mem32)
{
	int first = 1, i;

	isapnp_printf(buffer, "%s32-bit memory ", space);
	for (i = 0; i < 17; i++) {
		if (first) {
			first = 0;
		} else {
			isapnp_printf(buffer, ":");
		}
		isapnp_printf(buffer, "%02x", mem32->data[i]);
	}
}

static void isapnp_print_resources(isapnp_info_buffer_t *buffer, char *space, struct isapnp_resources *res)
{
	char *s;
	struct isapnp_port *port;
	struct isapnp_irq *irq;
	struct isapnp_dma *dma;
	struct isapnp_mem *mem;
	struct isapnp_mem32 *mem32;

	switch (res->priority) {
	case ISAPNP_RES_PRIORITY_PREFERRED:
		s = "preferred";
		break;
	case ISAPNP_RES_PRIORITY_ACCEPTABLE:
		s = "acceptable";
		break;
	case ISAPNP_RES_PRIORITY_FUNCTIONAL:
		s = "functional";
		break;
	default:
		s = "invalid";
	}
	isapnp_printf(buffer, "%sPriority %s\n", space, s);
	for (port = res->port; port; port = port->next)
		isapnp_print_port(buffer, space, port);
	for (irq = res->irq; irq; irq = irq->next)
		isapnp_print_irq(buffer, space, irq);
	for (dma = res->dma; dma; dma = dma->next)
		isapnp_print_dma(buffer, space, dma);
	for (mem = res->mem; mem; mem = mem->next)
		isapnp_print_mem(buffer, space, mem);
	for (mem32 = res->mem32; mem32; mem32 = mem32->next)
		isapnp_print_mem32(buffer, space, mem32);
}

static void isapnp_print_configuration(isapnp_info_buffer_t *buffer, struct pci_dev *dev)
{
	int i, tmp, next;
	char *space = "    ";

	isapnp_cfg_begin(dev->bus->number, dev->devfn);
	isapnp_printf(buffer, "%sDevice is %sactive\n",
			space, isapnp_read_byte(ISAPNP_CFG_ACTIVATE)?"":"not ");
	for (i = next = 0; i < 8; i++) {
		tmp = isapnp_read_word(ISAPNP_CFG_PORT + (i << 1));
		if (!tmp)
			continue;
		if (!next) {
			isapnp_printf(buffer, "%sActive port ", space);
			next = 1;
		}
		isapnp_printf(buffer, "%s0x%x", i > 0 ? "," : "", tmp);
	}
	if (next)
		isapnp_printf(buffer, "\n");
	for (i = next = 0; i < 2; i++) {
		tmp = isapnp_read_word(ISAPNP_CFG_IRQ + (i << 1));
		if (!(tmp >> 8))
			continue;
		if (!next) {
			isapnp_printf(buffer, "%sActive IRQ ", space);
			next = 1;
		}
		isapnp_printf(buffer, "%s%i", i > 0 ? "," : "", tmp >> 8);
		if (tmp & 0xff)
			isapnp_printf(buffer, " [0x%x]", tmp & 0xff);
	}
	if (next)
		isapnp_printf(buffer, "\n");
	for (i = next = 0; i < 2; i++) {
		tmp = isapnp_read_byte(ISAPNP_CFG_DMA + i);
		if (tmp == 4)
			continue;
		if (!next) {
			isapnp_printf(buffer, "%sActive DMA ", space);
			next = 1;
		}
		isapnp_printf(buffer, "%s%i", i > 0 ? "," : "", tmp);
	}
	if (next)
		isapnp_printf(buffer, "\n");
	for (i = next = 0; i < 4; i++) {
		tmp = isapnp_read_dword(ISAPNP_CFG_MEM + (i << 3));
		if (!tmp)
			continue;
		if (!next) {
			isapnp_printf(buffer, "%sActive memory ", space);
			next = 1;
		}
		isapnp_printf(buffer, "%s0x%x", i > 0 ? "," : "", tmp);
	}
	if (next)
		isapnp_printf(buffer, "\n");
	isapnp_cfg_end();
}

static void isapnp_print_device(isapnp_info_buffer_t *buffer, struct pci_dev *dev)
{
	int block, block1;
	char *space = "    ";
	struct isapnp_resources *res, *resa;

	if (!dev)
		return;
	isapnp_printf(buffer, "  Logical device %i '", dev->devfn);
	isapnp_print_devid(buffer, dev->vendor, dev->device);
	isapnp_printf(buffer, ":%s'", dev->name[0]?dev->name:"Unknown");
	isapnp_printf(buffer, "\n");
#if 0
	isapnp_cfg_begin(dev->bus->number, dev->devfn);
	for (block = 0; block < 128; block++)
		if ((block % 16) == 15)
			isapnp_printf(buffer, "%02x\n", isapnp_read_byte(block));
		else
			isapnp_printf(buffer, "%02x:", isapnp_read_byte(block));
	isapnp_cfg_end();
#endif
	if (dev->regs)
		isapnp_printf(buffer, "%sSupported registers 0x%x\n", space, dev->regs);
	isapnp_print_compatible(buffer, dev);
	isapnp_print_configuration(buffer, dev);
	for (res = (struct isapnp_resources *)dev->sysdata, block = 0; res; res = res->next, block++) {
		isapnp_printf(buffer, "%sResources %i\n", space, block);
		isapnp_print_resources(buffer, "      ", res);
		for (resa = res->alt, block1 = 1; resa; resa = resa->alt, block1++) {
			isapnp_printf(buffer, "%s  Alternate resources %i:%i\n", space, block, block1);
			isapnp_print_resources(buffer, "        ", resa);
		}
	}
}

/*
 *  Main read routine
 */
 
static void isapnp_info_read(isapnp_info_buffer_t *buffer)
{
	struct pci_bus *card;

	isapnp_for_each_card(card) {
		struct list_head *dev_list;

		isapnp_printf(buffer, "Card %i '", card->number);
		isapnp_print_devid(buffer, card->vendor, card->device);
		isapnp_printf(buffer, ":%s'", card->name[0]?card->name:"Unknown");
		if (card->pnpver)
			isapnp_printf(buffer, " PnP version %x.%x", card->pnpver >> 4, card->pnpver & 0x0f);
		if (card->productver)
			isapnp_printf(buffer, " Product version %x.%x", card->productver >> 4, card->productver & 0x0f);
		isapnp_printf(buffer,"\n");
		for (dev_list = card->devices.next; dev_list != &card->devices; dev_list = dev_list->next)
			isapnp_print_device(buffer, pci_dev_b(dev_list));
	}
}

/*
 *
 */

static struct pci_bus *isapnp_info_card;
static struct pci_dev *isapnp_info_device;

static char *isapnp_get_str(char *dest, char *src, int len)
{
	int c;

	while (*src == ' ' || *src == '\t')
		src++;
	if (*src == '"' || *src == '\'') {
		c = *src++;
		while (--len > 0 && *src && *src != c) {
			*dest++ = *src++;
		}
		if (*src == c)
			src++;
	} else {
		while (--len > 0 && *src && *src != ' ' && *src != '\t') {
			*dest++ = *src++;
		}
	}
	*dest = 0;
	while (*src == ' ' || *src == '\t')
		src++;
	return src;
}

static unsigned char isapnp_get_hex(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return (c - 'a') + 10;
	if (c >= 'A' && c <= 'F')
		return (c - 'A') + 10;
	return 0;
}

static unsigned int isapnp_parse_id(const char *id)
{
	if (strlen(id) != 7) {
		printk("isapnp: wrong PnP ID\n");
		return 0;
	}
	return (ISAPNP_VENDOR(id[0], id[1], id[2])<<16) |
			(isapnp_get_hex(id[3])<<4) |
			(isapnp_get_hex(id[4])<<0) |
			(isapnp_get_hex(id[5])<<12) |
			(isapnp_get_hex(id[6])<<8);
}

static int isapnp_set_card(char *line)
{
	int idx, idx1;
	unsigned int id;
	char index[16], value[32];

	if (isapnp_info_card) {
		isapnp_cfg_end();
		isapnp_info_card = NULL;
	}
	line = isapnp_get_str(index, line, sizeof(index));
	isapnp_get_str(value, line, sizeof(value));
	idx = idx1 = simple_strtoul(index, NULL, 0);
	id = isapnp_parse_id(value);
	isapnp_info_card = isapnp_find_card(id >> 16, id & 0xffff, NULL);
	while (isapnp_info_card && idx1-- > 0)
		isapnp_info_card = isapnp_find_card(id >> 16, id & 0xffff, isapnp_info_card);
	if (isapnp_info_card == NULL) {
		printk("isapnp: card '%s' order %i not found\n", value, idx);
		return 1;
	}
	if (isapnp_cfg_begin(isapnp_info_card->number, -1)<0) {
		printk("isapnp: configuration start sequence for device '%s' failed\n", value);
		isapnp_info_card = NULL;
		return 1;
	}
	return 0;
}

static int isapnp_select_csn(char *line)
{
	int csn;
	struct list_head *list;
	char index[16], value[32];

	isapnp_info_device = NULL;
	isapnp_get_str(index, line, sizeof(index));
	csn = simple_strtoul(index, NULL, 0);

	for (list = isapnp_cards.next; list != &isapnp_cards; list = list->next) {
		isapnp_info_card = pci_bus_b(list);
		if (isapnp_info_card->number == csn)
			break;
	}
	if (list == &isapnp_cards) {
		printk("isapnp: cannot find CSN %i\n", csn);
		return 1;
	}
	if (isapnp_cfg_begin(isapnp_info_card->number, -1)<0) {
		printk("isapnp: configuration start sequence for device '%s' failed\n", value);
		isapnp_info_card = NULL;
		return 1;
	}
	return 0;
}

static int isapnp_set_device(char *line)
{
	int idx, idx1;
	unsigned int id;
	char index[16], value[32];

	line = isapnp_get_str(index, line, sizeof(index));
	isapnp_get_str(value, line, sizeof(value));
	idx = idx1 = simple_strtoul(index, NULL, 0);
	id = isapnp_parse_id(value);
	isapnp_info_device = isapnp_find_dev(isapnp_info_card, id >> 16, id & 0xffff, NULL);
	while (isapnp_info_device && idx-- > 0)
		isapnp_info_device = isapnp_find_dev(isapnp_info_card, id >> 16, id & 0xffff, isapnp_info_device);
	if (isapnp_info_device == NULL) {
		printk("isapnp: device '%s' order %i not found\n", value, idx);
		return 1;
	}
	isapnp_device(isapnp_info_device->devfn);
	return 0;
}

static int isapnp_autoconfigure(void)
{
	isapnp_cfg_end();
	if (isapnp_info_device->active)
		isapnp_info_device->deactivate(isapnp_info_device);
	if (isapnp_info_device->prepare(isapnp_info_device) < 0) {
		printk("isapnp: cannot prepare device for the activation");
		return 0;
	}
	if (isapnp_info_device->activate(isapnp_info_device) < 0) {
		printk("isapnp: cannot activate device");
		return 0;
	}
	if (isapnp_cfg_begin(isapnp_info_card->number, -1)<0) {
		printk("isapnp: configuration start sequence for card %d failed\n", isapnp_info_card->number);
		isapnp_info_card = NULL;
		isapnp_info_device = NULL;
		return 1;
	}
	isapnp_device(isapnp_info_device->devfn);
	return 0;
}

static int isapnp_set_port(char *line)
{
	int idx, port;
	char index[16], value[32];

	line = isapnp_get_str(index, line, sizeof(index));
	isapnp_get_str(value, line, sizeof(value));
	idx = simple_strtoul(index, NULL, 0);
	port = simple_strtoul(value, NULL, 0);
	if (idx < 0 || idx > 7) {
		printk("isapnp: wrong port index %i\n", idx);
		return 1;
	}
	if (port < 0 || port > 0xffff) {
		printk("isapnp: wrong port value 0x%x\n", port);
		return 1;
	}
	isapnp_write_word(ISAPNP_CFG_PORT + (idx << 1), port);
	if (!isapnp_info_device->resource[idx].flags)
		return 0;
	if (isapnp_info_device->resource[idx].flags & IORESOURCE_AUTO) {
		isapnp_info_device->resource[idx].start = port;
		isapnp_info_device->resource[idx].end += port - 1;
		isapnp_info_device->resource[idx].flags &= ~IORESOURCE_AUTO;
	} else {
		isapnp_info_device->resource[idx].end -= isapnp_info_device->resource[idx].start;
		isapnp_info_device->resource[idx].start = port;
		isapnp_info_device->resource[idx].end += port;
	}
	return 0;
}

static void isapnp_set_irqresource(struct resource *res, int irq)
{
	res->start = res->end = irq;
	res->flags = IORESOURCE_IRQ;
}
 
static int isapnp_set_irq(char *line)
{
	int idx, irq;
	char index[16], value[32];

	line = isapnp_get_str(index, line, sizeof(index));
	isapnp_get_str(value, line, sizeof(value));
	idx = simple_strtoul(index, NULL, 0);
	irq = simple_strtoul(value, NULL, 0);
	if (idx < 0 || idx > 1) {
		printk("isapnp: wrong IRQ index %i\n", idx);
		return 1;
	}
	if (irq == 2)
		irq = 9;
	if (irq < 0 || irq > 15) {
		printk("isapnp: wrong IRQ value %i\n", irq);
		return 1;
	}
	isapnp_write_byte(ISAPNP_CFG_IRQ + (idx << 1), irq);
	isapnp_set_irqresource(isapnp_info_device->irq_resource + idx, irq);
	return 0;
}
 
static void isapnp_set_dmaresource(struct resource *res, int dma)
{
	res->start = res->end = dma;
	res->flags = IORESOURCE_DMA;
}

extern int isapnp_allow_dma0;
static int isapnp_set_allow_dma0(char *line)
{
	int i;
	char value[32];

	isapnp_get_str(value, line, sizeof(value));
	i = simple_strtoul(value, NULL, 0);
	if (i < 0 || i > 1) {
		printk("isapnp: wrong value %i for allow_dma0\n", i);
		return 1;
	}
	isapnp_allow_dma0 = i;
	return 0;
}
 
static int isapnp_set_dma(char *line)
{
	int idx, dma;
	char index[16], value[32];

	line = isapnp_get_str(index, line, sizeof(index));
	isapnp_get_str(value, line, sizeof(value));
	idx = simple_strtoul(index, NULL, 0);
	dma = simple_strtoul(value, NULL, 0);
	if (idx < 0 || idx > 1) {
		printk("isapnp: wrong DMA index %i\n", idx);
		return 1;
	}
	if (dma < 0 || dma > 7) {
		printk("isapnp: wrong DMA value %i\n", dma);
		return 1;
	}
	isapnp_write_byte(ISAPNP_CFG_DMA + idx, dma);
	isapnp_set_dmaresource(isapnp_info_device->dma_resource + idx, dma);
	return 0;
}
 
static int isapnp_set_mem(char *line)
{
	int idx;
	unsigned int mem;
	char index[16], value[32];

	line = isapnp_get_str(index, line, sizeof(index));
	isapnp_get_str(value, line, sizeof(value));
	idx = simple_strtoul(index, NULL, 0);
	mem = simple_strtoul(value, NULL, 0);
	if (idx < 0 || idx > 3) {
		printk("isapnp: wrong memory index %i\n", idx);
		return 1;
	}
	mem >>= 8;
	isapnp_write_word(ISAPNP_CFG_MEM + (idx<<2), mem & 0xffff);
	if (!isapnp_info_device->resource[idx + 8].flags)
		return 0;
	if (isapnp_info_device->resource[idx + 8].flags & IORESOURCE_AUTO) {
		isapnp_info_device->resource[idx + 8].start = mem & ~0x00ffff00;
		isapnp_info_device->resource[idx + 8].end += (mem & ~0x00ffff00) - 1;
		isapnp_info_device->resource[idx + 8].flags &= ~IORESOURCE_AUTO;
	} else {
		isapnp_info_device->resource[idx + 8].end -= isapnp_info_device->resource[idx + 8].start;
		isapnp_info_device->resource[idx + 8].start = mem & ~0x00ffff00;
		isapnp_info_device->resource[idx + 8].end += mem & ~0x00ffff00;
	}
	return 0;
}
 
static int isapnp_poke(char *line, int what)
{
	int reg;
	unsigned int val;
	char index[16], value[32];

	line = isapnp_get_str(index, line, sizeof(index));
	isapnp_get_str(value, line, sizeof(value));
	reg = simple_strtoul(index, NULL, 0);
	val = simple_strtoul(value, NULL, 0);
	if (reg < 0 || reg > 127) {
		printk("isapnp: wrong register %i\n", reg);
		return 1;
	}
	switch (what) {
	case 1:
		isapnp_write_word(reg, val);
		break;
	case 2:
		isapnp_write_dword(reg, val);
		break;
	default:
		isapnp_write_byte(reg, val);
		break;
	}
	return 0;
}
 
static int isapnp_decode_line(char *line)
{
	char cmd[32];

	line = isapnp_get_str(cmd, line, sizeof(cmd));
	if (!strcmp(cmd, "allow_dma0"))
		return isapnp_set_allow_dma0(line);
	if (!strcmp(cmd, "card"))
		return isapnp_set_card(line);
	if (!strcmp(cmd, "csn"))
		return isapnp_select_csn(line);
	if (!isapnp_info_card) {
		printk("isapnp: card is not selected\n");
		return 1;
	}
	if (!strncmp(cmd, "dev", 3))
		return isapnp_set_device(line);
	if (!isapnp_info_device) {
		printk("isapnp: device is not selected\n");
		return 1;
	}
	if (!strncmp(cmd, "auto", 4))
		return isapnp_autoconfigure();
	if (!strncmp(cmd, "act", 3)) {
		isapnp_activate(isapnp_info_device->devfn);
		isapnp_info_device->active = 1;
		return 0;
	}
	if (!strncmp(cmd, "deact", 5)) {
		isapnp_deactivate(isapnp_info_device->devfn);
		isapnp_info_device->active = 0;
		return 0;
	}
	if (!strcmp(cmd, "port"))
		return isapnp_set_port(line);
	if (!strcmp(cmd, "irq"))
		return isapnp_set_irq(line);
	if (!strcmp(cmd, "dma"))
		return isapnp_set_dma(line);
	if (!strncmp(cmd, "mem", 3))
		return isapnp_set_mem(line);
	if (!strcmp(cmd, "poke"))
		return isapnp_poke(line, 0);
	if (!strcmp(cmd, "pokew"))
		return isapnp_poke(line, 1);
	if (!strcmp(cmd, "poked"))
		return isapnp_poke(line, 2);
	printk("isapnp: wrong command '%s'\n", cmd);
	return 1;
}

/*
 *  Main write routine
 */

static void isapnp_info_write(isapnp_info_buffer_t *buffer)
{
	int c, idx, idx1 = 0;
	char line[128];

	if (buffer->size <= 0)
		return;
	isapnp_info_card = NULL;
	isapnp_info_device = NULL;
	for (idx = 0; idx < buffer->size; idx++) {
		c = buffer->buffer[idx];
		if (c == '\n') {
			line[idx1] = '\0';
			if (line[0] != '#') {
				if (isapnp_decode_line(line))
					goto __end;
			}
			idx1 = 0;
			continue;
		}
		if (idx1 >= sizeof(line)-1) {
			printk("isapnp: line too long, aborting\n");
			return;
		}
		line[idx1++] = c;
	}
      __end:
	if (isapnp_info_card)
		isapnp_cfg_end();
}
