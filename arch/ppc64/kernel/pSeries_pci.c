/*
 * pSeries_pci.c
 *
 * Copyright (C) 2001 Dave Engebretsen, IBM Corporation
 *
 * pSeries specific routines for PCI.
 * 
 * Based on code from pci.c and chrp_pci.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bootmem.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/init.h>
#include <asm/pci-bridge.h>
#include <asm/ppcdebug.h>
#include <asm/naca.h>
#include <asm/pci_dma.h>

#include "xics.h"
#include "open_pic.h"
#include "pci.h"

extern struct device_node *allnodes;

/*******************************************************************
 * Forward declares of prototypes. 
 *******************************************************************/
unsigned long find_and_init_phbs(void);
struct pci_controller* alloc_phb(struct device_node *dev, char *model, unsigned int addr_size_words) ;
void pSeries_pcibios_fixup(void);
static int rtas_fake_read(struct device_node *dn, int offset, int nbytes, unsigned long *returnval);

/* RTAS tokens */
static int read_pci_config;
static int write_pci_config;
static int ibm_read_pci_config;
static int ibm_write_pci_config;

static int s7a_workaround;

/******************************************************************************
 *
 * pSeries I/O Operations to access the PCI configuration space.
 *
 *****************************************************************************/
#define RTAS_PCI_READ_OP(size, type, nbytes) \
int __chrp \
rtas_read_config_##size(struct device_node *dn, int offset, type val) {  \
	unsigned long returnval = ~0L; \
	unsigned long buid; \
	unsigned int addr; \
	int ret; \
	 \
	if (dn == NULL) { \
		ret = -2; \
	} else if (dn->status) { \
		ret = -1; \
	} else { \
		addr = (dn->busno << 16) | (dn->devfn << 8) | offset; \
		buid = dn->phb->buid; \
		if (buid) { \
			ret = rtas_call(ibm_read_pci_config, 4, 2, &returnval, addr, buid >> 32, buid & 0xffffffff, nbytes); \
                        if (ret < 0 || (returnval == 0xffffffff)) \
                               ret = rtas_fake_read(dn, offset, nbytes, &returnval); \
		} else { \
			ret = rtas_call(read_pci_config, 2, 2, &returnval, addr, nbytes); \
		} \
	} \
	*val = returnval; \
	return ret; \
} \
int __chrp \
rtas_pci_read_config_##size(struct pci_dev *dev, int offset, type val) {  \
        struct device_node *dn = pci_device_to_OF_node(dev); \
	int ret = rtas_read_config_##size(dn, offset, val); \
        /* udbg_printf("read bus=%x, devfn=%x, ret=%d phb=%lx, dn=%lx\n", dev->bus->number, dev->devfn, ret, dn ? dn->phb : 0, dn); */ \
        return ret ? PCIBIOS_DEVICE_NOT_FOUND : PCIBIOS_SUCCESSFUL; \
}

#define RTAS_PCI_WRITE_OP(size, type, nbytes) \
int __chrp \
rtas_write_config_##size(struct device_node *dn, int offset, type val) { \
	unsigned long buid; \
	unsigned int addr; \
	int ret; \
	 \
	if (dn == NULL) { \
		ret = -2; \
	} else if (dn->status) { \
		ret = -1; \
	} else { \
		buid = dn->phb->buid; \
		addr = (dn->busno << 16) | (dn->devfn << 8) | offset; \
		if (buid) { \
			ret = rtas_call(ibm_write_pci_config, 5, 1, NULL, addr, buid >> 32, buid & 0xffffffff, nbytes, (ulong) val); \
		} else { \
			ret = rtas_call(write_pci_config, 3, 1, NULL, addr, nbytes, (ulong)val); \
		} \
	} \
	return ret; \
} \
int __chrp \
rtas_pci_write_config_##size(struct pci_dev *dev, int offset, type val) { \
	struct device_node*  dn = pci_device_to_OF_node(dev); \
	int  ret = rtas_write_config_##size(dn, offset, val); \
	/* udbg_printf("write bus=%x, devfn=%x, ret=%d phb=%lx, dn=%lx\n", dev->bus->number, dev->devfn, ret, dn ? dn->phb : 0, dn); */ \
	return ret ? PCIBIOS_DEVICE_NOT_FOUND : PCIBIOS_SUCCESSFUL; \
}

RTAS_PCI_READ_OP(byte, u8 *, 1)
RTAS_PCI_READ_OP(word, u16 *, 2)
RTAS_PCI_READ_OP(dword, u32 *, 4)
RTAS_PCI_WRITE_OP(byte, u8, 1)
RTAS_PCI_WRITE_OP(word, u16, 2)
RTAS_PCI_WRITE_OP(dword, u32, 4)

struct pci_ops rtas_pci_ops = {
	rtas_pci_read_config_byte,
	rtas_pci_read_config_word,
	rtas_pci_read_config_dword,
	rtas_pci_write_config_byte,
 	rtas_pci_write_config_word,
	rtas_pci_write_config_dword,
};

/*
 * Handle the case where rtas refuses to do a pci config read.
 * This currently only happens with some PHBs in which case we totally fake
 * out the values (and call it a speedwagaon -- something we could look up
 * in the device tree).
 */
static int
rtas_fake_read(struct device_node *dn, int offset, int nbytes, unsigned long *returnval)
{
	char *device_type = (char *)get_property(dn, "device_type", 0);
	u32 *class_code = (u32 *)get_property(dn, "class-code", 0);

	*returnval = ~0;	/* float by default */

	/* udbg_printf("rtas_fake_read dn=%p, offset=0x%02x, nbytes=%d, device_type=%s\n", dn, offset, nbytes, device_type ? device_type : "<none>"); */
	if (device_type && strcmp(device_type, "pci") != 0)
		return -3;	/* Not a phb or bridge */

	/* NOTE: class_code != NULL => EADS pci bridge.  Else a PHB */
	if (nbytes == 1) {
		if (offset == PCI_HEADER_TYPE)
			*returnval = 0x80;	/* multifunction */
		else if (offset == PCI_INTERRUPT_PIN || offset == PCI_INTERRUPT_LINE)
			*returnval = 0;
	} else if (nbytes == 2) {
		if (offset == PCI_SUBSYSTEM_VENDOR_ID || offset == PCI_SUBSYSTEM_ID)
			*returnval = 0;
		else if (offset == PCI_COMMAND)
			*returnval = PCI_COMMAND_PARITY|PCI_COMMAND_MASTER|PCI_COMMAND_MEMORY;
	} else if (nbytes == 4) {
		if (offset == PCI_VENDOR_ID)
			*returnval = 0x1014 | ((class_code ? 0x8b : 0x102) << 16); /* a phb */
		else if (offset == PCI_REVISION_ID)
			*returnval = (class_code ? PCI_CLASS_BRIDGE_PCI : PCI_CLASS_BRIDGE_HOST) << 16; /* revs are zero */
		else if ((offset >= PCI_BASE_ADDRESS_0 && offset <= PCI_BASE_ADDRESS_5) || offset == PCI_ROM_ADDRESS)
			*returnval = 0;
	}

	/* printk("fake: %s nbytes=%d, offset=%lx ret=%lx\n", class_code ? "EADS" : "PHB", nbytes, offset, *returnval); */
	return 0;
}

/******************************************************************
 * pci_read_irq_line
 *
 * Reads the Interrupt Pin to determine if interrupt is use by card.
 * If the interrupt is used, then gets the interrupt line from the 
 * openfirmware and sets it in the pci_dev and pci_config line.
 *
 ******************************************************************/
int 
pci_read_irq_line(struct pci_dev *Pci_Dev)
{
	u8 InterruptPin;
	struct device_node *Node;

    	pci_read_config_byte(Pci_Dev, PCI_INTERRUPT_PIN, &InterruptPin);
	if (InterruptPin == 0) {
		PPCDBG(PPCDBG_BUSWALK,"\tDevice: %s No Interrupt used by device.\n",Pci_Dev->slot_name);
		return 0;	
	}

	Node = pci_device_to_OF_node(Pci_Dev);
	if ( Node == NULL) { 
		PPCDBG(PPCDBG_BUSWALK,"\tDevice: %s Device Node not found.\n",Pci_Dev->slot_name);
		return -1;	
	}
	if (Node->n_intrs == 0) 	{
		PPCDBG(PPCDBG_BUSWALK,"\tDevice: %s No Device OF interrupts defined.\n",Pci_Dev->slot_name);
		return -1;	
	}
	Pci_Dev->irq = Node->intrs[0].line;

	if (s7a_workaround) {
		if (Pci_Dev->irq > 16)
			Pci_Dev->irq -= 3;
	}

	pci_write_config_byte(Pci_Dev, PCI_INTERRUPT_LINE, Pci_Dev->irq);
	
	PPCDBG(PPCDBG_BUSWALK,"\tDevice: %s pci_dev->irq = 0x%02X\n",Pci_Dev->slot_name,Pci_Dev->irq);
	return 0;
}

/******************************************************************
 * Find all PHBs in the system and initialize a set of data 
 * structures to represent them.
 ******************************************************************/
unsigned long __init
find_and_init_phbs(void)
{
        struct device_node *Pci_Node;
        struct pci_controller *phb;
        unsigned int root_addr_size_words = 0, this_addr_size_words = 0;
	unsigned int this_addr_count = 0, range_stride;
        unsigned int *ui_ptr = NULL, *ranges;
        char *model;
	struct pci_range64 range;
	struct resource *res;
	unsigned int memno, rlen, i, index;
	unsigned int *opprop;
	int has_isa = 0;
        PPCDBG(PPCDBG_PHBINIT, "find_and_init_phbs\n"); 

	read_pci_config = rtas_token("read-pci-config");
	write_pci_config = rtas_token("write-pci-config");
	ibm_read_pci_config = rtas_token("ibm,read-pci-config");
	ibm_write_pci_config = rtas_token("ibm,write-pci-config");

	if (naca->interrupt_controller == IC_OPEN_PIC) {
		opprop = (unsigned int *)get_property(find_path_device("/"),
				"platform-open-pic", NULL);
	}

	/* Get the root address word size. */
	ui_ptr = (unsigned int *) get_property(find_path_device("/"), 
					       "#size-cells", NULL);
	if (ui_ptr) {
		root_addr_size_words = *ui_ptr;
	} else {
		PPCDBG(PPCDBG_PHBINIT, "\tget #size-cells failed.\n"); 
		return(-1);
	}

	if (find_type_devices("isa")) {
		has_isa = 1;
		PPCDBG(PPCDBG_PHBINIT, "\tFound an ISA bus.\n"); 
	}

	index = 0;

	/******************************************************************
	* Find all PHB devices and create an object for them.
	******************************************************************/
	for (Pci_Node = find_devices("pci"); Pci_Node != NULL; Pci_Node = Pci_Node->next) {
		model = (char *) get_property(Pci_Node, "model", NULL);
		if (model != NULL)  {
			phb = alloc_phb(Pci_Node, model, root_addr_size_words);
			if (phb == NULL) return(-1);
		}
		else {
         		continue;
		}
		
		/* Get this node's address word size. */
		ui_ptr = (unsigned int *) get_property(Pci_Node, "#size-cells", NULL);
		if (ui_ptr)
			this_addr_size_words = *ui_ptr;
		else
			this_addr_size_words = 1;
		/* Get this node's address word count. */
		ui_ptr = (unsigned int *) get_property(Pci_Node, "#address-cells", NULL);
		if (ui_ptr)
			this_addr_count = *ui_ptr;
		else
			this_addr_count = 3;
		
		range_stride = this_addr_count + root_addr_size_words + this_addr_size_words;
	      
		memno = 0;
		phb->io_base_phys = 0;
         
		ranges = (unsigned int *) get_property(Pci_Node, "ranges", &rlen);
		PPCDBG(PPCDBG_PHBINIT, "\trange_stride = 0x%lx, rlen = 0x%x\n", range_stride, rlen);
                
		for (i = 0; i < (rlen/sizeof(*ranges)); i+=range_stride) {
		  	/* Put the PCI addr part of the current element into a 
			 * '64' struct. 
			 */
		  	range = *((struct pci_range64 *)(ranges + i));

			/* If this is a '32' element, map into a 64 struct. */
			if ((range_stride * sizeof(int)) == 
			   sizeof(struct pci_range32)) {
				range.parent_addr = 
					(unsigned long)(*(ranges + i + 3));
				range.size = 
					(((unsigned long)(*(ranges + i + 4)))<<32) | 
					(*(ranges + i + 5));
			} else {
				range.parent_addr = 
					(((unsigned long)(*(ranges + i + 3)))<<32) | 
					(*(ranges + i + 4));
				range.size = 
					(((unsigned long)(*(ranges + i + 5)))<<32) | 
					(*(ranges + i + 6));
			}
			
			PPCDBG(PPCDBG_PHBINIT, "\trange.parent_addr    = 0x%lx\n", 
			       range.parent_addr);
			PPCDBG(PPCDBG_PHBINIT, "\trange.child_addr.hi  = 0x%lx\n", 
			       range.child_addr.a_hi);
			PPCDBG(PPCDBG_PHBINIT, "\trange.child_addr.mid = 0x%lx\n", 
			       range.child_addr.a_mid);
			PPCDBG(PPCDBG_PHBINIT, "\trange.child_addr.lo  = 0x%lx\n", 
			       range.child_addr.a_lo);
			PPCDBG(PPCDBG_PHBINIT, "\trange.size           = 0x%lx\n", 
			       range.size);

			res = NULL;
		        switch ((range.child_addr.a_hi >> 24) & 0x3) {
			case 1:		/* I/O space */
				PPCDBG(PPCDBG_PHBINIT, "\tIO Space\n");
				phb->io_base_phys = range.parent_addr;
				res = &phb->io_resource;
				res->name = Pci_Node->full_name;
				res->flags = IORESOURCE_IO;
				phb->io_base_virt = __ioremap(phb->io_base_phys, range.size, _PAGE_NO_CACHE);
				if (!pci_io_base) {
					pci_io_base = (unsigned long)phb->io_base_virt;
					if (has_isa)
						isa_io_base = pci_io_base;
				}
				res->start = ((((unsigned long) range.child_addr.a_mid) << 32) | (range.child_addr.a_lo));
				res->start += (unsigned long)phb->io_base_virt - pci_io_base;
				res->end =   res->start + range.size - 1;
				res->parent = NULL;
				res->sibling = NULL;
				res->child = NULL;
				phb->pci_io_offset = range.parent_addr - 
					((((unsigned long)
					   range.child_addr.a_mid) << 32) | 
					 (range.child_addr.a_lo));
				PPCDBG(PPCDBG_PHBINIT, "\tpci_io_offset  = 0x%lx\n", 
				       phb->pci_io_offset);
			  	break;
			case 2:		/* mem space */
				PPCDBG(PPCDBG_PHBINIT, "\tMem Space\n");
				phb->pci_mem_offset = range.parent_addr - 
					((((unsigned long)
					   range.child_addr.a_mid) << 32) | 
					 (range.child_addr.a_lo));
				PPCDBG(PPCDBG_PHBINIT, "\tpci_mem_offset = 0x%lx\n", 
				       phb->pci_mem_offset);
				if (memno < sizeof(phb->mem_resources)/sizeof(phb->mem_resources[0])) {
					res = &(phb->mem_resources[memno]);
					++memno;
					res->name = Pci_Node->full_name;
					res->flags = IORESOURCE_MEM;
					res->start = range.parent_addr;
					res->end =   range.parent_addr + range.size - 1;
					res->parent = NULL;
					res->sibling = NULL;
					res->child = NULL;
				}
			  	break;
			}
		}
		PPCDBG(PPCDBG_PHBINIT, "\tphb->io_base_phys   = 0x%lx\n", 
		       phb->io_base_phys); 
		PPCDBG(PPCDBG_PHBINIT, "\tphb->pci_mem_offset = 0x%lx\n", 
		       phb->pci_mem_offset); 

		if (naca->interrupt_controller == IC_OPEN_PIC) {
			int addr = root_addr_size_words * (index + 2) - 1;
			openpic_setup_ISU(index, opprop[addr]); 
		}
		index++;
	}
	pci_devs_phb_init();
	return 0;	 /*Success */
}

/******************************************************************
 *
 * Allocate and partially initialize a structure to represent a PHB.
 *
 ******************************************************************/
struct pci_controller *
alloc_phb(struct device_node *dev, char *model, unsigned int addr_size_words)
{
	struct pci_controller *phb;
	unsigned int *ui_ptr = NULL, len;
	struct reg_property64 reg_struct;
	struct property *of_prop;
	int *bus_range;
	int *buid_vals;

	PPCDBG(PPCDBG_PHBINIT, "alloc_phb: %s\n", dev->full_name); 
	PPCDBG(PPCDBG_PHBINIT, "\tdev             = 0x%lx\n", dev); 
	PPCDBG(PPCDBG_PHBINIT, "\tmodel           = 0x%lx\n", model); 
	PPCDBG(PPCDBG_PHBINIT, "\taddr_size_words = 0x%lx\n", addr_size_words); 
  
	/* Found a PHB, now figure out where his registers are mapped. */
	ui_ptr = (unsigned int *) get_property(dev, "reg", &len);
	if (ui_ptr == NULL) {
		PPCDBG(PPCDBG_PHBINIT, "\tget reg failed.\n"); 
		return(NULL);
	}

	if (addr_size_words == 1) {
		reg_struct.address = ((struct reg_property32 *)ui_ptr)->address;
		reg_struct.size    = ((struct reg_property32 *)ui_ptr)->size;
	} else {
		reg_struct = *((struct reg_property64 *)ui_ptr);
	}

	PPCDBG(PPCDBG_PHBINIT, "\treg_struct.address = 0x%lx\n", reg_struct.address);
	PPCDBG(PPCDBG_PHBINIT, "\treg_struct.size    = 0x%lx\n", reg_struct.size); 

	/***************************************************************
	* Set chip specific data in the phb, including types & 
	* register pointers.
	***************************************************************/

	/****************************************************************
	* Python
	***************************************************************/
	if (strstr(model, "Python")) {
		PPCDBG(PPCDBG_PHBINIT, "\tCreate python\n");
	        phb = pci_alloc_pci_controller("PHB PY",phb_type_python);
		if (phb == NULL) return NULL;
	
       		phb->cfg_addr = (volatile unsigned long *) 
			ioremap(reg_struct.address + 0xf8000, PAGE_SIZE);
		PPCDBG(PPCDBG_PHBINIT, "\tcfg_addr_r = 0x%lx\n", 
		       reg_struct.address + 0xf8000);
		PPCDBG(PPCDBG_PHBINIT, "\tcfg_addr_v = 0x%lx\n", 
		       phb->cfg_addr);
		phb->cfg_data = (char*)(phb->cfg_addr + 0x02);
       		phb->phb_regs = (volatile unsigned long *) 
			ioremap(reg_struct.address + 0xf7000, PAGE_SIZE);
		/* Python's register file is 1 MB in size. */
		phb->chip_regs = ioremap(reg_struct.address & ~(0xfffffUL), 
					 0x100000); 

		/* 
		 * Firmware doesn't always clear this bit which is critical
		 * for good performance - Anton
		 */
		{
			volatile u32 *tmp, i;

#define PRG_CL_RESET_VALID 0x00010000

			tmp = (u32 *)((unsigned long)phb->chip_regs + 0xf6030);

			if (*tmp & PRG_CL_RESET_VALID) {
				printk("Python workaround: ");
				*tmp &= ~PRG_CL_RESET_VALID;
				/*
				 * We must read it back for changes to
				 * take effect
				 */
				i = *tmp;
				printk("reg0: %x\n", i);
			}
		}

	/***************************************************************
	* Speedwagon
	*   include Winnipeg as well for the time being.
	***************************************************************/
	} else if ((strstr(model, "Speedwagon")) || 
		   (strstr(model, "Winnipeg"))) {
		PPCDBG(PPCDBG_PHBINIT, "\tCreate speedwagon\n");
	        phb = pci_alloc_pci_controller("PHB SW",phb_type_speedwagon);
		if (phb == NULL) return NULL;

		if (systemcfg->platform == PLATFORM_PSERIES) {
			phb->cfg_addr = (volatile unsigned long *) 
			  ioremap(reg_struct.address + 0x140, PAGE_SIZE);
			phb->cfg_data = (char*)(phb->cfg_addr - 0x02); /* minus is correct */
			phb->phb_regs = (volatile unsigned long *) 
			  ioremap(reg_struct.address, PAGE_SIZE);
			/* Speedwagon's register file is 1 MB in size. */
			phb->chip_regs = ioremap(reg_struct.address & ~(0xfffffUL),
						 0x100000); 
			PPCDBG(PPCDBG_PHBINIT, "\tmapping chip_regs from 0x%lx -> 0x%lx\n", 
			       reg_struct.address & 0xfffff, phb->chip_regs);
		} else {
			phb->cfg_addr = NULL;
			phb->cfg_data = NULL; 
			phb->phb_regs = NULL;
			phb->chip_regs = NULL;
		}

		phb->local_number = ((reg_struct.address >> 12) & 0xf) - 0x8;
	} else {
		PPCDBG(PPCDBG_PHBINIT, "\tUnknown PHB Type!\n");

		if (systemcfg->platform == PLATFORM_PSERIES_LPAR) {

			phb=pci_alloc_pci_controller("PHB UK",phb_type_unknown);
			if (phb == NULL) return NULL;

			phb->cfg_addr = NULL;
			phb->cfg_data = NULL;
			phb->phb_regs = NULL;
			phb->chip_regs = NULL;
		} else {
			printk("PCI: Unknown Phb Type!\n");
			return NULL;
		}
	}

	/* Add a linux,phbnum property to the device tree so user code
	 * can translate bus numbers.
	 */
	of_prop = (struct property *) alloc_bootmem(sizeof(struct property) +
						    sizeof(phb->global_number));
	if (of_prop) {
		memset(of_prop, 0, sizeof(struct property));
		of_prop->name = "linux,phbnum";
		of_prop->length = sizeof(phb->global_number);
		of_prop->value = (unsigned char *)&of_prop[1];
		memcpy(of_prop->value, &phb->global_number,
		       sizeof(phb->global_number));
		prom_add_property(dev, of_prop);
	}

	bus_range = (int *) get_property(dev, "bus-range", &len);
	if (bus_range == NULL || len < 2 * sizeof(int)) {
		PPCDBG(PPCDBG_PHBINIT, "Can't get bus-range for %s\n", dev->full_name);
		kfree(phb);
		return(NULL);
	}

	/***************************************************************
	* Finished with the initialization
	***************************************************************/
	phb->first_busno =  bus_range[0];
	phb->last_busno  =  bus_range[1];

	phb->arch_data   = dev;
	phb->ops = &rtas_pci_ops;

	buid_vals = (int *) get_property(dev, "ibm,fw-phb-id", &len);
	
  if (buid_vals == NULL) {
		phb->buid = 0;
	} 
  else {
		struct pci_bus check;
		if (sizeof(check.number) == 1 || sizeof(check.primary) == 1 ||
		    sizeof(check.secondary) == 1 || sizeof(check.subordinate) == 1) {
			udbg_printf("pSeries_pci:  this system has large bus numbers and the kernel was not\n"
			      "built with the patch that fixes include/linux/pci.h struct pci_bus so\n"
			      "number, primary, secondary and subordinate are ints.\n");
			panic("pSeries_pci:  this system has large bus numbers and the kernel was not\n"
			      "built with the patch that fixes include/linux/pci.h struct pci_bus so\n"
			      "number, primary, secondary and subordinate are ints.\n");
    }
    
    if (len < 2 * sizeof(int))
      phb->buid = (unsigned long)buid_vals[0];  // Support for new OF that only has 1 integer for buid.
    else
      phb->buid = (((unsigned long)buid_vals[0]) << 32UL) |
                  (((unsigned long)buid_vals[1]) & 0xffffffff);
  	
		phb->first_busno += (phb->global_number << 8);
		phb->last_busno += (phb->global_number << 8);
	}

	/* Dump PHB information for Debug */
	PPCDBGCALL(PPCDBG_PHBINIT,dumpPci_Controller(phb) );

	return phb;
}

void 
fixup_resources(struct pci_dev *dev)
{
 	int i;
 	struct pci_controller *phb = PCI_GET_PHB_PTR(dev);
	struct device_node *dn;

	/* Add IBM loc code (slot) as a prefix to the device names for service */
	dn = pci_device_to_OF_node(dev);
	if (dn) {
		char *loc_code = get_property(dn, "ibm,loc-code", 0);
		if (loc_code) {
			int loc_len = strlen(loc_code);
			if (loc_len < sizeof(dev->name)) {
				memmove(dev->name+loc_len+1, dev->name, sizeof(dev->name)-loc_len-1);
				memcpy(dev->name, loc_code, loc_len);
				dev->name[loc_len] = ' ';
				dev->name[sizeof(dev->name)-1] = '\0';
			}
		}
	}

	PPCDBG(PPCDBG_PHBINIT, "fixup_resources:\n"); 
	PPCDBG(PPCDBG_PHBINIT, "\tphb                 = 0x%016LX\n", phb); 
	PPCDBG(PPCDBG_PHBINIT, "\tphb->pci_io_offset  = 0x%016LX\n", phb->pci_io_offset); 
	PPCDBG(PPCDBG_PHBINIT, "\tphb->pci_mem_offset = 0x%016LX\n", phb->pci_mem_offset); 

	PPCDBG(PPCDBG_PHBINIT, "\tdev->name   = %s\n", dev->name);
	PPCDBG(PPCDBG_PHBINIT, "\tdev->vendor:device = 0x%04X : 0x%04X\n", dev->vendor, dev->device);

	if (phb == NULL)
		return;

 	for (i = 0; i <  DEVICE_COUNT_RESOURCE; ++i) {
		PPCDBG(PPCDBG_PHBINIT, "\tdevice %x.%x[%d] (flags %x) [%lx..%lx]\n",
			    dev->bus->number, dev->devfn, i,
			    dev->resource[i].flags,
			    dev->resource[i].start,
			    dev->resource[i].end);

		if ((dev->resource[i].start == 0) && (dev->resource[i].end == 0)) {
			continue;
		}

		if (dev->resource[i].start > dev->resource[i].end) {
			/* Bogus resource.  Just clear it out. */
			dev->resource[i].start = dev->resource[i].end = 0;
			continue;
		}


		if (dev->resource[i].flags & IORESOURCE_IO) {
			unsigned long offset = (unsigned long)phb->io_base_virt - pci_io_base;
			dev->resource[i].start += offset;
			dev->resource[i].end += offset;
			PPCDBG(PPCDBG_PHBINIT, "\t\t-> now [%lx .. %lx]\n",
			       dev->resource[i].start, dev->resource[i].end);
		} else if (dev->resource[i].flags & IORESOURCE_MEM) {
			if (dev->resource[i].start == 0) {
				/* Bogus.  Probably an unused bridge. */
				dev->resource[i].end = 0;
			} else {
				dev->resource[i].start += phb->pci_mem_offset;
				dev->resource[i].end += phb->pci_mem_offset;
			}
			PPCDBG(PPCDBG_PHBINIT, "\t\t-> now [%lx..%lx]\n",
			       dev->resource[i].start, dev->resource[i].end);

		} else {
			continue;
		}

 		/* zap the 2nd function of the winbond chip */
 		if (dev->resource[i].flags & IORESOURCE_IO
 		    && dev->bus->number == 0 && dev->devfn == 0x81)
 			dev->resource[i].flags &= ~IORESOURCE_IO;
 	}
}   

static void check_s7a(void)
{
	struct device_node *root;
	char *model;

	root = find_path_device("/");
	if (root) {
		model = get_property(root, "model", NULL);
		if (model && !strcmp(model, "IBM,7013-S7A"))
			s7a_workaround = 1;
	}
}

void __init
pSeries_pcibios_fixup(void)
{
	struct pci_dev *dev;

	PPCDBG(PPCDBG_PHBINIT, "pSeries_pcibios_fixup: start\n");
	pci_assign_all_busses = 0;

	check_s7a();
	
	pci_for_each_dev(dev) {
		pci_read_irq_line(dev);
		PPCDBGCALL(PPCDBG_PHBINIT, dumpPci_Dev(dev) );
	}
}

/*********************************************************************** 
 * pci_find_hose_for_OF_device
 *
 * This function finds the PHB that matching device_node in the 
 * OpenFirmware by scanning all the pci_controllers.
 * 
 ***********************************************************************/
struct pci_controller*
pci_find_hose_for_OF_device(struct device_node *node)
{
	while (node) {
		struct pci_controller *hose;
		for (hose=hose_head;hose;hose=hose->next)
			if (hose->arch_data == node)
				return hose;
		node=node->parent;
	}
	return NULL;
}

/*********************************************************************** 
 * ppc64_pcibios_init
 *  
 * Chance to initialize and structures or variable before PCI Bus walk.
 *  
 ***********************************************************************/
void 
pSeries_pcibios_init(void)
{
	PPCDBG(PPCDBG_PHBINIT, "\tppc64_pcibios_init Entry.\n"); 

	if (get_property(find_path_device("/rtas"),"ibm,fw-phb-id",NULL) != NULL) {
		PPCDBG(PPCDBG_PHBINIT, "\tFound: ibm,fw-phb-id\n"); 
		Pci_Large_Bus_System = 1;
	}
}

/*
 * This is called very early before the page table is setup.
 */
void 
pSeries_pcibios_init_early(void)
{
	ppc_md.pcibios_read_config_byte = rtas_read_config_byte;
	ppc_md.pcibios_read_config_word = rtas_read_config_word;
	ppc_md.pcibios_read_config_dword = rtas_read_config_dword;
	ppc_md.pcibios_write_config_byte = rtas_write_config_byte;
	ppc_md.pcibios_write_config_word = rtas_write_config_word;
	ppc_md.pcibios_write_config_dword = rtas_write_config_dword;
}
/************************************************************************/
/* Get a char* of the device physical location(U0.3-P1-I8)              */
/* See the Product Topology in the RS/6000 Architecture.                */
/************************************************************************/
int device_Location(struct pci_dev *PciDev, char *BufPtr)
{
	struct device_node *DevNode = (struct device_node *)PciDev->sysdata;
	return sprintf(BufPtr,"PCI: Bus%3d, Device%3d, Vendor %04X, Location %-12s",
		       PciDev->bus->number,
		       PCI_SLOT(PciDev->devfn),
		       PciDev->vendor,
		       (char*)get_property(DevNode,"ibm,loc-code",0));
}
/************************************************************************/
/* Set the slot reset line to the state passed in.                      */
/* This is the platform specific for code for the pci_reset_device      */
/* function.                                                            */
/************************************************************************/
int pci_set_reset(struct pci_dev *PciDev, int state)
{
	return -1;
}
