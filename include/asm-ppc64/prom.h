#ifndef _PPC64_PROM_H
#define _PPC64_PROM_H

/*
 * Definitions for talking to the Open Firmware PROM on
 * Power Macintosh computers.
 *
 * Copyright (C) 1996 Paul Mackerras.
 *
 * Updates for PPC64 by Peter Bergner & David Engebretsen, IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define PTRRELOC(x)     ((typeof(x))((unsigned long)(x) - offset))
#define PTRUNRELOC(x)   ((typeof(x))((unsigned long)(x) + offset))
#define RELOC(x)        (*PTRRELOC(&(x)))

#define LONG_LSW(X) (((unsigned long)X) & 0xffffffff)
#define LONG_MSW(X) (((unsigned long)X) >> 32)

typedef u32 phandle;
typedef void *ihandle;
typedef u32 phandle32;
typedef u32 ihandle32;

extern char *prom_display_paths[];
extern unsigned int prom_num_displays;

struct address_range {
	unsigned long space;
	unsigned long address;
	unsigned long size;
};

struct interrupt_info {
	int	line;
	int	sense;		/* +ve/-ve logic, edge or level, etc. */
};

struct pci_address {
	u32 a_hi;
	u32 a_mid;
	u32 a_lo;
};

struct pci_range32 {
	struct pci_address child_addr;
	unsigned int  parent_addr;
  	unsigned long size; 
};

struct pci_range64 {
	struct pci_address child_addr;
  	unsigned long parent_addr;
        unsigned long size; 
};

union pci_range {
	struct {
		struct pci_address addr;
		u32 phys;
		u32 size_hi;
	} pci32;
	struct {
		struct pci_address addr;
		u32 phys_hi;
		u32 phys_lo;
		u32 size_hi;
		u32 size_lo;
	} pci64;
};

struct _of_tce_table {
	phandle node;
	unsigned long base;
	unsigned long size;
};

struct reg_property {
	unsigned long address;
	unsigned long size;
};

struct reg_property32 {
	unsigned int address;
	unsigned int size;
};

struct reg_property64 {
	unsigned long address;
	unsigned long size;
};

struct translation_property {
	unsigned long virt;
	unsigned long size;
	unsigned long phys;
	unsigned int flags;
};

struct property {
	char	*name;
	int	length;
	unsigned char *value;
	struct property *next;
};

/* NOTE: the device_node contains PCI specific info for pci devices.
 * This perhaps could be hung off the device_node with another struct,
 * but for now it is directly in the node.  The phb ptr is a good
 * indication of a real PCI node.  Other nodes leave these fields zeroed.
 */
struct pci_controller;
struct TceTable;
struct device_node {
	char	*name;
	char	*type;
	phandle	node;
	phandle linux_phandle;
	int	n_addrs;
	struct	address_range *addrs;
	int	n_intrs;
	struct	interrupt_info *intrs;
	char	*full_name;

	/* PCI stuff probably doesn't belong here */
	int	busno;			/* for pci devices */
	int	devfn;			/* for pci devices */
#define DN_STATUS_BIST_FAILED (1<<0)
	int	status;			/* Current device status (non-zero is bad) */
	int	eeh_mode;		/* See eeh.h for possible EEH_MODEs */
	int	eeh_config_addr;
	struct  pci_controller *phb;	/* for pci devices */
	struct	TceTable *tce_table;	/* for phb's or bridges */

	struct	property *properties;
	struct	device_node *parent;
	struct	device_node *child;
	struct	device_node *sibling;
	struct	device_node *next;	/* next device of same type */
	struct	device_node *allnext;	/* next in list of all nodes */
};

typedef u32 prom_arg_t;

struct prom_args {
        u32 service;
        u32 nargs;
        u32 nret;
        prom_arg_t args[10];
        prom_arg_t *rets;     /* Pointer to return values in args[16]. */
};

typedef struct {
	u32  printf;	/* void (*printf)(char *, ...); */
	u32  memdump;	/* void (*memdump)(unsigned char *, unsigned long); */
	u32  dummy;		/* void (*dummy)(void); */
} yaboot_debug_t;

struct prom_t {
	unsigned long entry;
	ihandle chosen;
	int cpu;
	ihandle stdout;
	ihandle disp_node;
	struct prom_args args;
	unsigned long version;
	unsigned long encode_phys_size;
	struct bi_record *bi_recs;
#ifdef DEBUG_YABOOT
	yaboot_debug_t *yaboot;
#endif
};

extern struct prom_t prom;
extern char *of_stdout_device;

/* Prototypes */
extern unsigned long prom_init(unsigned long, unsigned long, unsigned long,
    unsigned long, unsigned long, yaboot_debug_t *);
extern void prom_print(const char *msg);
extern void relocate_nodes(void);
extern void finish_device_tree(void);
extern struct device_node *find_devices(const char *name);
extern struct device_node *find_type_devices(const char *type);
extern struct device_node *find_path_device(const char *path);
extern struct device_node *find_compatible_devices(const char *type,
						   const char *compat);
extern struct device_node *find_pci_device_OFnode(unsigned char bus,
	unsigned char dev_fn);
extern struct device_node *find_all_nodes(void);
extern int device_is_compatible(struct device_node *device, const char *);
extern int machine_is_compatible(const char *compat);
extern unsigned char *get_property(struct device_node *node, const char *name,
				   int *lenp);
extern void print_properties(struct device_node *node);
extern int prom_n_addr_cells(struct device_node* np);
extern int prom_n_size_cells(struct device_node* np);
extern void prom_get_irq_senses(unsigned char *senses, int off, int max);
extern void prom_add_property(struct device_node* np, struct property* prop);

#endif /* _PPC64_PROM_H */
