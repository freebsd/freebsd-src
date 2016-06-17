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

#ifndef LINUX_ISAPNP_H
#define LINUX_ISAPNP_H

#include <linux/config.h>
#include <linux/errno.h>

/*
 *  Configuration registers (TODO: change by specification)
 */ 

#define ISAPNP_CFG_ACTIVATE		0x30	/* byte */
#define ISAPNP_CFG_MEM			0x40	/* 4 * dword */
#define ISAPNP_CFG_PORT			0x60	/* 8 * word */
#define ISAPNP_CFG_IRQ			0x70	/* 2 * word */
#define ISAPNP_CFG_DMA			0x74	/* 2 * byte */

/*
 *
 */

#define ISAPNP_VENDOR(a,b,c)	(((((a)-'A'+1)&0x3f)<<2)|\
				((((b)-'A'+1)&0x18)>>3)|((((b)-'A'+1)&7)<<13)|\
				((((c)-'A'+1)&0x1f)<<8))
#define ISAPNP_DEVICE(x)	((((x)&0xf000)>>8)|\
				 (((x)&0x0f00)>>8)|\
				 (((x)&0x00f0)<<8)|\
				 (((x)&0x000f)<<8))
#define ISAPNP_FUNCTION(x)	ISAPNP_DEVICE(x)

/*
 *
 */

#ifdef __KERNEL__

#include <linux/pci.h>

#define ISAPNP_PORT_FLAG_16BITADDR	(1<<0)
#define ISAPNP_PORT_FLAG_FIXED		(1<<1)

struct isapnp_port {
	unsigned short min;		/* min base number */
	unsigned short max;		/* max base number */
	unsigned char align;		/* align boundary */
	unsigned char size;		/* size of range */
	unsigned char flags;		/* port flags */
	unsigned char pad;		/* pad */
	struct isapnp_resources *res;	/* parent */
	struct isapnp_port *next;	/* next port */
};

struct isapnp_irq {
	unsigned short map;		/* bitmaks for IRQ lines */
	unsigned char flags;		/* IRQ flags */
	unsigned char pad;		/* pad */
	struct isapnp_resources *res;	/* parent */
	struct isapnp_irq *next;	/* next IRQ */
};

struct isapnp_dma {
	unsigned char map;		/* bitmask for DMA channels */
	unsigned char flags;		/* DMA flags */
	struct isapnp_resources *res;	/* parent */
	struct isapnp_dma *next;	/* next port */
};

struct isapnp_mem {
	unsigned int min;		/* min base number */
	unsigned int max;		/* max base number */
	unsigned int align;		/* align boundary */
	unsigned int size;		/* size of range */
	unsigned char flags;		/* memory flags */
	unsigned char pad;		/* pad */
	struct isapnp_resources *res;	/* parent */
	struct isapnp_mem *next;	/* next memory resource */
};

struct isapnp_mem32 {
	/* TODO */
	unsigned char data[17];
	struct isapnp_resources *res;	/* parent */
	struct isapnp_mem32 *next;	/* next 32-bit memory resource */
};

struct isapnp_fixup {
	unsigned short vendor;		/* matching vendor */
	unsigned short device;		/* matching device */
	void (*quirk_function)(struct pci_dev *dev);	/* fixup function */
};


#define ISAPNP_RES_PRIORITY_PREFERRED	0
#define ISAPNP_RES_PRIORITY_ACCEPTABLE	1
#define ISAPNP_RES_PRIORITY_FUNCTIONAL	2
#define ISAPNP_RES_PRIORITY_INVALID	65535

struct isapnp_resources {
	unsigned short priority;	/* priority */
	unsigned short dependent;	/* dependent resources */
	struct isapnp_port *port;	/* first port */
	struct isapnp_irq *irq;		/* first IRQ */
	struct isapnp_dma *dma;		/* first DMA */
	struct isapnp_mem *mem;		/* first memory resource */
	struct isapnp_mem32 *mem32;	/* first 32-bit memory */
	struct pci_dev *dev;		/* parent */
	struct isapnp_resources *alt;	/* alternative resource (aka dependent resources) */
	struct isapnp_resources *next;	/* next resource */
};

#define ISAPNP_ANY_ID		0xffff
#define ISAPNP_CARD_DEVS	8

#define ISAPNP_CARD_ID(_va, _vb, _vc, _device) \
		card_vendor: ISAPNP_VENDOR(_va, _vb, _vc), card_device: ISAPNP_DEVICE(_device)
#define ISAPNP_CARD_END \
		card_vendor: 0, card_device: 0
#define ISAPNP_DEVICE_ID(_va, _vb, _vc, _function) \
		{ vendor: ISAPNP_VENDOR(_va, _vb, _vc), function: ISAPNP_FUNCTION(_function) }

/* export used IDs outside module */
#define ISAPNP_CARD_TABLE(name) \
		MODULE_GENERIC_TABLE(isapnp_card, name)

struct isapnp_card_id {
	unsigned long driver_data;	/* data private to the driver */
	unsigned short card_vendor, card_device;
	struct {
		unsigned short vendor, function;
	} devs[ISAPNP_CARD_DEVS];	/* logical devices */
};

#define ISAPNP_DEVICE_SINGLE(_cva, _cvb, _cvc, _cdevice, _dva, _dvb, _dvc, _dfunction) \
		card_vendor: ISAPNP_VENDOR(_cva, _cvb, _cvc), card_device: ISAPNP_DEVICE(_cdevice), \
		vendor: ISAPNP_VENDOR(_dva, _dvb, _dvc), function: ISAPNP_FUNCTION(_dfunction)
#define ISAPNP_DEVICE_SINGLE_END \
		card_vendor: 0, card_device: 0

struct isapnp_device_id {
	unsigned short card_vendor, card_device;
	unsigned short vendor, function;
	unsigned long driver_data;	/* data private to the driver */
};

struct isapnp_driver {
	struct list_head node;
	char *name;
	const struct isapnp_device_id *id_table;	/* NULL if wants all devices */
	int  (*probe)  (struct pci_dev *dev, const struct isapnp_device_id *id);	/* New device inserted */
	void (*remove) (struct pci_dev *dev);	/* Device removed (NULL if not a hot-plug capable driver) */
};

#if defined(CONFIG_ISAPNP) || (defined(CONFIG_ISAPNP_MODULE) && defined(MODULE))

#define __ISAPNP__

/* lowlevel configuration */
int isapnp_present(void);
int isapnp_cfg_begin(int csn, int device);
int isapnp_cfg_end(void);
unsigned char isapnp_read_byte(unsigned char idx);
unsigned short isapnp_read_word(unsigned char idx);
unsigned int isapnp_read_dword(unsigned char idx);
void isapnp_write_byte(unsigned char idx, unsigned char val);
void isapnp_write_word(unsigned char idx, unsigned short val);
void isapnp_write_dword(unsigned char idx, unsigned int val);
void isapnp_wake(unsigned char csn);
void isapnp_device(unsigned char device);
void isapnp_activate(unsigned char device);
void isapnp_deactivate(unsigned char device);
void isapnp_fixup_device(struct pci_dev *dev);
void *isapnp_alloc(long size);
int isapnp_proc_init(void);
int isapnp_proc_done(void);
/* manager */
struct pci_bus *isapnp_find_card(unsigned short vendor,
				 unsigned short device,
				 struct pci_bus *from);
struct pci_dev *isapnp_find_dev(struct pci_bus *card,
				unsigned short vendor,
				unsigned short function,
				struct pci_dev *from);
int isapnp_probe_cards(const struct isapnp_card_id *ids,
		       int (*probe)(struct pci_bus *card,
				    const struct isapnp_card_id *id));
int isapnp_probe_devs(const struct isapnp_device_id *ids,
			int (*probe)(struct pci_dev *dev,
				     const struct isapnp_device_id *id));
/* misc */
void isapnp_resource_change(struct resource *resource,
			    unsigned long start,
			    unsigned long size);
int isapnp_activate_dev(struct pci_dev *dev, const char *name);
/* init/main.c */
int isapnp_init(void);

extern struct list_head isapnp_cards;
extern struct list_head isapnp_devices;

#define isapnp_for_each_card(card) \
	for(card = pci_bus_b(isapnp_cards.next); card != pci_bus_b(&isapnp_cards); card = pci_bus_b(card->node.next))
#define isapnp_for_each_dev(dev) \
	for(dev = pci_dev_g(isapnp_devices.next); dev != pci_dev_g(&isapnp_devices); dev = pci_dev_g(dev->global_list.next))

int isapnp_register_driver(struct isapnp_driver *drv);
void isapnp_unregister_driver(struct isapnp_driver *drv);

#else /* !CONFIG_ISAPNP */

/* lowlevel configuration */
static inline int isapnp_present(void) { return 0; }
static inline int isapnp_cfg_begin(int csn, int device) { return -ENODEV; }
static inline int isapnp_cfg_end(void) { return -ENODEV; }
static inline unsigned char isapnp_read_byte(unsigned char idx) { return 0xff; }
static inline unsigned short isapnp_read_word(unsigned char idx) { return 0xffff; }
static inline unsigned int isapnp_read_dword(unsigned char idx) { return 0xffffffff; }
static inline void isapnp_write_byte(unsigned char idx, unsigned char val) { ; }
static inline void isapnp_write_word(unsigned char idx, unsigned short val) { ; }
static inline void isapnp_write_dword(unsigned char idx, unsigned int val) { ; }
static inline void isapnp_wake(unsigned char csn) { ; }
static inline void isapnp_device(unsigned char device) { ; }
static inline void isapnp_activate(unsigned char device) { ; }
static inline void isapnp_deactivate(unsigned char device) { ; }
/* manager */
static inline struct pci_bus *isapnp_find_card(unsigned short vendor,
					       unsigned short device,
					       struct pci_bus *from) { return NULL; }
static inline struct pci_dev *isapnp_find_dev(struct pci_bus *card,
					      unsigned short vendor,
					      unsigned short function,
					      struct pci_dev *from) { return NULL; }
static inline int isapnp_probe_cards(const struct isapnp_card_id *ids,
				     int (*probe)(struct pci_bus *card,
						  const struct isapnp_card_id *id)) { return -ENODEV; }
static inline int isapnp_probe_devs(const struct isapnp_device_id *ids,
				    int (*probe)(struct pci_dev *dev,
						 const struct isapnp_device_id *id)) { return -ENODEV; }
static inline void isapnp_resource_change(struct resource *resource,
					  unsigned long start,
					  unsigned long size) { ; }
static inline int isapnp_activate_dev(struct pci_dev *dev, const char *name) { return -ENODEV; }

static inline int isapnp_register_driver(struct isapnp_driver *drv) { return 0; }

static inline void isapnp_unregister_driver(struct isapnp_driver *drv) { }

#endif /* CONFIG_ISAPNP */

#endif /* __KERNEL__ */
#endif /* LINUX_ISAPNP_H */
