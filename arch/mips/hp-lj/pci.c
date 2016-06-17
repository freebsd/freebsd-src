/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * SNI specific PCI support for RM200/RM300.
 *
 * Copyright (C) 1997 - 2000 Ralf Baechle
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <asm/pci_channel.h>
#include <asm/hp-lj/asic.h>

#ifdef CONFIG_PCI

volatile u32* pci_config_address_reg = (volatile u32*)0xfdead000;
volatile u32* pci_config_data_reg = (volatile u32*)0xfdead000;



#define cfgaddr(dev, where) (((dev->bus->number & 0xff) << 0x10) |  \
                             ((dev->devfn & 0xff) << 0x08) |        \
                             (where & 0xfc))

/*
 * We can't address 8 and 16 bit words directly.  Instead we have to
 * read/write a 32bit word and mask/modify the data we actually want.
 */
static int pcimt_read_config_byte (struct pci_dev *dev,
                                   int where, unsigned char *val)
{
        *pci_config_address_reg = cfgaddr(dev, where);
	*val = (le32_to_cpu(*pci_config_data_reg) >> ((where&3)<<3)) & 0xff;
	//printk("pci_read_byte 0x%x == 0x%x\n", where, *val);
	return PCIBIOS_SUCCESSFUL;
}

static int pcimt_read_config_word (struct pci_dev *dev,
                                   int where, unsigned short *val)
{
	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;
	*pci_config_address_reg = cfgaddr(dev, where);
	*val = (le32_to_cpu(*pci_config_data_reg) >> ((where&3)<<3)) & 0xffff;
	//printk("pci_read_word 0x%x == 0x%x\n", where, *val);
	return PCIBIOS_SUCCESSFUL;
}

int pcimt_read_config_dword (struct pci_dev *dev,
                                    int where, unsigned int *val)
{
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;
	*pci_config_address_reg = cfgaddr(dev, where);
	*val = le32_to_cpu(*pci_config_data_reg);
	//printk("pci_read_dword 0x%x == 0x%x\n", where, *val);
	return PCIBIOS_SUCCESSFUL;
}

static int pcimt_write_config_byte (struct pci_dev *dev,
                                    int where, unsigned char val)
{
	*pci_config_address_reg = cfgaddr(dev, where);
	*(volatile u8 *)(((int)pci_config_data_reg) + (where & 3)) = val;
	//printk("pci_write_byte 0x%x = 0x%x\n", where, val);
	return PCIBIOS_SUCCESSFUL;
}

static int pcimt_write_config_word (struct pci_dev *dev,
                                    int where, unsigned short val)
{
	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;
	*pci_config_address_reg = cfgaddr(dev, where);
	*(volatile u16 *)(((int)pci_config_data_reg) + (where & 2)) =
                  le16_to_cpu(val);
	//printk("pci_write_word 0x%x = 0x%x\n", where, val);
	return PCIBIOS_SUCCESSFUL;
}

int pcimt_write_config_dword (struct pci_dev *dev,
                                     int where, unsigned int val)
{
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;
	*pci_config_address_reg = cfgaddr(dev, where);
	*pci_config_data_reg = le32_to_cpu(val);
	//printk("pci_write_dword 0x%x = 0x%x\n", where, val);
	return PCIBIOS_SUCCESSFUL;
}



struct pci_ops hp_pci_ops = {
	pcimt_read_config_byte,
	pcimt_read_config_word,
	pcimt_read_config_dword,
	pcimt_write_config_byte,
	pcimt_write_config_word,
	pcimt_write_config_dword
};


struct pci_channel mips_pci_channels[] = {
	{ &hp_pci_ops, &ioport_resource, &iomem_resource },
	{ NULL, NULL, NULL }
};

unsigned __init int pcibios_assign_all_busses(void)
{
        return 1;
}

void __init pcibios_fixup(void)
{
}


void __init pcibios_fixup_irqs(void)
{
   struct pci_dev *dev;
   int slot_num;


   pci_for_each_dev(dev) {
      slot_num = PCI_SLOT(dev->devfn);
      switch(slot_num) {
         case 2: dev->irq = 3;  break;
         case 3: dev->irq = 4;  break;
         case 4: dev->irq = 5;  break;
         default: break;
      }
   }
}

#define IO_MEM_LOGICAL_START   0x3e000000
#define IO_MEM_LOGICAL_END     0x3fefffff

#define IO_PORT_LOGICAL_START  0x3ff00000
#define IO_PORT_LOGICAL_END    0x3fffffff


#define IO_MEM_VIRTUAL_OFFSET  0xb0000000
#define IO_PORT_VIRTUAL_OFFSET 0xb0000000

#define ONE_MEG   (1024 * 1024)

void __init pci_setup(void)
{
   u32 pci_regs_base_offset = 0xfdead000;

   switch(GetAsicId()) {
      case AndrosAsic:   pci_regs_base_offset = 0xbff80000;   break;
      case HarmonyAsic:  pci_regs_base_offset = 0xbff70000;   break;
      default:
         printk("ERROR: PCI does not support %s Asic\n", GetAsicName());
	 while(1);
         break;
   }

   // set bus stat/command reg
   // REVIST this setting may need vary depending on the hardware
   *((volatile unsigned int*)(pci_regs_base_offset | 0x0004)) =  0x38000007;


   iomem_resource.start =  IO_MEM_LOGICAL_START + IO_MEM_VIRTUAL_OFFSET;
   iomem_resource.end =    IO_MEM_LOGICAL_END + IO_MEM_VIRTUAL_OFFSET;

   ioport_resource.start = IO_PORT_LOGICAL_START + IO_PORT_VIRTUAL_OFFSET;
   ioport_resource.end =   IO_PORT_LOGICAL_END + IO_PORT_VIRTUAL_OFFSET;

   // KLUDGE (mips_io_port_base is screwed up, we've got to work around it here)
   // by letting both low (illegal) and high (legal) addresses appear in pci io space
   ioport_resource.start = 0x0;

   set_io_port_base(IO_PORT_LOGICAL_START + IO_PORT_VIRTUAL_OFFSET);

   // map the PCI address space
   // global map - all levels & processes can access
   // except that the range is outside user space
   // parameters: lo0, lo1, hi, pagemask
   // lo indicates physical page, hi indicates virtual address
   add_wired_entry((IO_MEM_LOGICAL_START >> 6) | 0x17,
                   ((IO_MEM_LOGICAL_START + (16 * ONE_MEG)) >> 6) | 0x17,
                   0xee000000, PM_16M);


   // These are used in pci r/w routines so need to preceed bus scan
   pci_config_data_reg = (u32*) (((u32)mips_io_port_base) | 0xcfc);
   pci_config_address_reg = (u32*) (((u32)pci_regs_base_offset) | 0xcf8);

}


void __init pcibios_fixup_resources(struct pci_dev *dev)
{
    int pos;
    int bases;

    printk("adjusting pci device: %s\n", dev->name);

    switch (dev->hdr_type) {
       case PCI_HEADER_TYPE_NORMAL: bases = 6; break;
       case PCI_HEADER_TYPE_BRIDGE: bases = 2; break;
       case PCI_HEADER_TYPE_CARDBUS: bases = 1; break;
       default: bases = 0; break;
    }
    for (pos=0; pos < bases; pos++) {
       struct resource* res = &dev->resource[pos];
       if (res->start >= IO_MEM_LOGICAL_START &&
           res->end <= IO_MEM_LOGICAL_END) {
              res->start += IO_MEM_VIRTUAL_OFFSET;
              res->end += IO_MEM_VIRTUAL_OFFSET;
       }
       if (res->start >= IO_PORT_LOGICAL_START &&
           res->end <= IO_PORT_LOGICAL_END) {
              res->start += IO_PORT_VIRTUAL_OFFSET;
              res->end += IO_PORT_VIRTUAL_OFFSET;
       }
    }

}


#endif /* CONFIG_PCI */
