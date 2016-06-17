/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <asm/sn/sgi.h>
#include <asm/sn/xtalk/xbow.h>	/* Must be before iograph.h to get MAX_PORT_NUM */
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/hcl_util.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/pci/bridge.h>
#include <asm/sn/ioerror_handling.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pciio_private.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/io.h>
#include <asm/sn/pci/pci_bus_cvlink.h>
#include <asm/sn/simulator.h>

#if !defined(DEV_FUNC)
extern pciio_provider_t *pciio_to_provider_fns(vertex_hdl_t dev);
#define DEV_FUNC(dev,func)	pciio_to_provider_fns(dev)->func
#define CAST_PIOMAP(x)		((pciio_piomap_t)(x))
#define CAST_DMAMAP(x)		((pciio_dmamap_t)(x))
#define CAST_INTR(x)		((pciio_intr_t)(x))
#endif

/*
 * Many functions are not passed their vertex
 * information directly; rather, they must
 * dive through a resource map. These macros
 * are available to coordinate this detail.
 */
#define PIOMAP_FUNC(map,func)		DEV_FUNC((map)->pp_dev,func)
#define DMAMAP_FUNC(map,func)		DEV_FUNC((map)->pd_dev,func)
#define INTR_FUNC(intr_hdl,func)	DEV_FUNC((intr_hdl)->pi_dev,func)

int
snia_badaddr_val(volatile void *addr, int len, volatile void *ptr)
{
	int ret = 0;
	volatile void *new_addr;

	switch (len) {
		case 4:
			new_addr = (void *) addr;
			ret = ia64_sn_probe_io_slot((long)new_addr, len, (void *)ptr);
			break;
		default:
			printk(KERN_WARNING "badaddr_val given len %x but supports len of 4 only\n", len);
	}

	if (ret < 0)
		panic("badaddr_val: unexpected status (%d) in probing", ret);
	return(ret);

}


nasid_t
snia_get_console_nasid(void)
{
	extern nasid_t console_nasid;
	extern nasid_t master_baseio_nasid;

	if (console_nasid < 0) {
		console_nasid = ia64_sn_get_console_nasid();
		if (console_nasid < 0) {
// ZZZ What do we do if we don't get a console nasid on the hardware????
			if (IS_RUNNING_ON_SIMULATOR() )
				console_nasid = master_baseio_nasid;
		}
	} 
	return console_nasid;
}

nasid_t
snia_get_master_baseio_nasid(void)
{
	extern nasid_t master_baseio_nasid;
	extern char master_baseio_wid;

	if (master_baseio_nasid < 0) {
		master_baseio_nasid = ia64_sn_get_master_baseio_nasid();

		if ( master_baseio_nasid >= 0 ) {
        		master_baseio_wid = WIDGETID_GET(KL_CONFIG_CH_CONS_INFO(master_baseio_nasid)->memory_base);
		}
	} 
	return master_baseio_nasid;
}


void
snia_ioerror_dump(char *name, int error_code, int error_mode, ioerror_t *ioerror)
{
#ifdef	LATER
	/* This needs to be tested */

	static char *error_mode_string[] =
		{ "probe", "kernel", "user", "reenable" };

	printk("%s%s%s%s%s error in %s mode\n",
               name,
               (error_code & IOECODE_PIO) ? " PIO" : "",
               (error_code & IOECODE_DMA) ? " DMA" : "",
               (error_code & IOECODE_READ) ? " Read" : "",
               (error_code & IOECODE_WRITE) ? " Write" : "",
               error_mode_string[error_mode]);

#define PRFIELD(f)                                  \
        if (IOERROR_FIELDVALID(ioerror,f)) {        \
		int tmp;                            \
		IOERROR_GETVALUE(tmp, ioerror, f);  \
                printk("\t%20s: 0x%x\n", #f, tmp);  \
	}

        PRFIELD(errortype);             /* error type: extra info about error */
        PRFIELD(widgetnum);             /* Widget number that's in error */
        PRFIELD(widgetdev);             /* Device within widget in error */
        PRFIELD(srccpu);                /* CPU on srcnode generating error */
        PRFIELD(srcnode);               /* Node which caused the error   */
        PRFIELD(errnode);               /* Node where error was noticed  */
        PRFIELD(sysioaddr);             /* Sys specific IO address       */
        PRFIELD(xtalkaddr);             /* Xtalk (48bit) addr of Error   */
        PRFIELD(busspace);              /* Bus specific address space    */
        PRFIELD(busaddr);               /* Bus specific address          */
        PRFIELD(vaddr);                 /* Virtual address of error      */
        PRFIELD(memaddr);               /* Physical memory address       */
        PRFIELD(epc);                   /* pc when error reported        */
        PRFIELD(ef);                    /* eframe when error reported    */

#undef  PRFIELD

        {
                /* Print a more descriptive CPU string */
                cpuid_t srccpu;
		IOERROR_GETVALUE(srccpu, ioerror, srccpu);
		// smp_processor_id()
                printk("(NOTE: CPU %d)\n", srccpu);
                printk("\n");
        }
#endif	/* LATER */
}


int
snia_pcibr_rrb_alloc(struct pci_dev *pci_dev,
	int *count_vchan0,
	int *count_vchan1)
{
	vertex_hdl_t dev = PCIDEV_VERTEX(pci_dev);

	return pcibr_rrb_alloc(dev, count_vchan0, count_vchan1);
}

pciio_endian_t
snia_pciio_endian_set(struct pci_dev *pci_dev,
	pciio_endian_t device_end,
	pciio_endian_t desired_end)
{
	vertex_hdl_t dev = PCIDEV_VERTEX(pci_dev);
	
	return DEV_FUNC(dev, endian_set)
		(dev, device_end, desired_end);
}

iopaddr_t
snia_pciio_dmatrans_addr(struct pci_dev *pci_dev, /* translate for this device */
                    device_desc_t dev_desc,     /* device descriptor */
                    paddr_t paddr,      /* system physical address */
                    size_t byte_count,  /* length */
                    unsigned flags)
{                                       /* defined in dma.h */

    vertex_hdl_t dev = PCIDEV_VERTEX(pci_dev);

    /*
     * If the device is not a PIC, we always want the PCIIO_BYTE_STREAM to be 
     * set.  Otherwise, it must not be set.  This applies to SN1 and SN2.
     */
    return DEV_FUNC(dev, dmatrans_addr)
        (dev, dev_desc, paddr, byte_count, flags & ~PCIIO_BYTE_STREAM);
}

pciio_dmamap_t
snia_pciio_dmamap_alloc(struct pci_dev *pci_dev,  /* set up mappings for this device */
                   device_desc_t dev_desc,      /* device descriptor */
                   size_t byte_count_max,       /* max size of a mapping */
                   unsigned flags)
{                                       /* defined in dma.h */

    vertex_hdl_t dev = PCIDEV_VERTEX(pci_dev);

    /*
     * If the device is not a PIC, we always want the PCIIO_BYTE_STREAM to be
     * set.  Otherwise, it must not be set.  This applies to SN1 and SN2.
     */
    return (pciio_dmamap_t) DEV_FUNC(dev, dmamap_alloc)
        (dev, dev_desc, byte_count_max, flags & ~PCIIO_BYTE_STREAM);
}

void
snia_pciio_dmamap_free(pciio_dmamap_t pciio_dmamap)
{
    DMAMAP_FUNC(pciio_dmamap, dmamap_free)
        (CAST_DMAMAP(pciio_dmamap));
}

iopaddr_t
snia_pciio_dmamap_addr(pciio_dmamap_t pciio_dmamap,  /* use these mapping resources */
                  paddr_t paddr,        /* map for this address */
                  size_t byte_count)
{                                       /* map this many bytes */
    return DMAMAP_FUNC(pciio_dmamap, dmamap_addr)
        (CAST_DMAMAP(pciio_dmamap), paddr, byte_count);
}

void
snia_pciio_dmamap_done(pciio_dmamap_t pciio_dmamap)
{
    DMAMAP_FUNC(pciio_dmamap, dmamap_done)
        (CAST_DMAMAP(pciio_dmamap));
}

#include <linux/module.h>
EXPORT_SYMBOL(snia_pciio_dmatrans_addr);
EXPORT_SYMBOL(snia_pciio_dmamap_alloc);
EXPORT_SYMBOL(snia_pciio_dmamap_free);
EXPORT_SYMBOL(snia_pciio_dmamap_addr);
EXPORT_SYMBOL(snia_pciio_dmamap_done);
EXPORT_SYMBOL(snia_pciio_endian_set);
EXPORT_SYMBOL(snia_pcibr_rrb_alloc);
