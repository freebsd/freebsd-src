/*
 * c 2001 PPC 64 Team, IBM Corp
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */
#ifndef __PPC_KERNEL_PCI_H__
#define __PPC_KERNEL_PCI_H__

#include <linux/pci.h>
#include <asm/pci-bridge.h>

extern unsigned long isa_io_base;
extern unsigned long isa_mem_base;
extern unsigned long pci_dram_offset;

/*******************************************************************
 * Platform independant variables referenced.
 *******************************************************************
 * Set pci_assign_all_busses to 1 if you want the kernel to re-assign
 * all PCI bus numbers.  
 *******************************************************************/
extern int pci_assign_all_busses;

extern struct pci_controller* pci_alloc_pci_controller(char *model, enum phb_types controller_type);
extern struct pci_controller* pci_find_hose_for_OF_device(struct device_node* node);

extern struct pci_controller* hose_head;
extern struct pci_controller** hose_tail;
/* PHB's are also in a table. */
#define PCI_MAX_PHB 64
extern int  global_phb_number;
extern struct pci_controller *phbtab[];

/*******************************************************************
 * Platform functions that are brand specific implementation. 
 *******************************************************************/
extern unsigned long find_and_init_phbs(void);

extern void   fixup_resources(struct pci_dev *dev);
extern void   ppc64_pcibios_init(void);

extern int    pci_set_reset(struct pci_dev*,int);
extern int    device_Location(struct pci_dev*,char*);
extern int    format_device_location(struct pci_dev*,char*, int );

extern struct pci_dev *ppc64_isabridge_dev;	/* may be NULL if no ISA bus */

/*******************************************************************
 * PCI device_node operations
 *******************************************************************/
struct device_node;
typedef void *(*traverse_func)(struct device_node *me, void *data);
void *traverse_pci_devices(struct device_node *start, traverse_func pre, traverse_func post, void *data);
void *traverse_all_pci_devices(traverse_func pre);

struct pci_dev *pci_find_dev_by_addr(unsigned long addr);
void pci_devs_phb_init(void);
void pci_fix_bus_sysdata(void);
struct device_node *fetch_dev_dn(struct pci_dev *dev);

void iSeries_pcibios_init_early(void);
void pSeries_pcibios_init_early(void);
void pSeries_pcibios_init(void);

/* Get a device_node from a pci_dev.  This code must be fast except in the case
 * where the sysdata is incorrect and needs to be fixed up (hopefully just once)
 */
static inline struct device_node *pci_device_to_OF_node(struct pci_dev *dev)
{
	struct device_node *dn = (struct device_node *)(dev->sysdata);
	if (dn->devfn == dev->devfn && dn->busno == (dev->bus->number&0xff))
		return dn;	/* fast path.  sysdata is good */
	else
		return fetch_dev_dn(dev);
}
/* Use this macro after the PCI bus walk for max performance when it
 * is known that sysdata is correct.
 */
#define PCI_GET_DN(dev) ((struct device_node *)((dev)->sysdata))


/*******************************************************************
 * Platform configuration flags.. (Live in pci.c)
 *******************************************************************/
extern int  Pci_Large_Bus_System;      /* System has > 256 buses   */
extern int  Pci_Manage_Phb_Space;      /* Manage Phb Space for IOAs*/

/*******************************************************************
 * Helper macros for extracting data from pci structures.  
 *   PCI_GET_PHB_PTR(struct pci_dev*)    returns the Phb pointer.
 *   PCI_GET_PHB_NUMBER(struct pci_dev*) returns the Phb number.
 *   PCI_GET_BUS_NUMBER(struct pci_dev*) returns the bus number.
 *******************************************************************/
#define PCI_GET_PHB_PTR(dev)    (((struct device_node *)(dev)->sysdata)->phb)
#define PCI_GET_PHB_NUMBER(dev) (((dev)->bus->number&0x00FFFF00)>>8)
#define PCI_GET_BUS_NUMBER(dev) ((dev)->bus->number&0x0000FF)

/*******************************************************************
 * Pci Flight Recorder support.
 *******************************************************************/
#define PCIFR(...) fr_Log_Entry(PciFr,__VA_ARGS__);
extern struct flightRecorder* PciFr;
extern int    Pci_Trace_Flag;

/*******************************************************************
 * Debugging  Routines.
 *******************************************************************/
extern void dumpResources(struct resource* Resource);
extern void dumpPci_Controller(struct pci_controller* phb);
extern void dumpPci_Bus(struct pci_bus* Pci_Bus);
extern void dumpPci_Dev(struct pci_dev* Pci_Dev);

extern void dump_Phb_tree(void);
extern void dump_Bus_tree(void);
extern void dump_Dev_tree(void);

#endif /* __PPC_KERNEL_PCI_H__ */
