/*
  $Id: fore200e.c,v 1.5 2000/04/14 10:10:34 davem Exp $

  A FORE Systems 200E-series driver for ATM on Linux.
  Christophe Lizzi (lizzi@cnam.fr), October 1999-March 2000.

  Based on the PCA-200E driver from Uwe Dannowski (Uwe.Dannowski@inf.tu-dresden.de).

  This driver simultaneously supports PCA-200E and SBA-200E adapters
  on i386, alpha (untested), powerpc, sparc and sparc64 architectures.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include <linux/version.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/atmdev.h>
#include <linux/sonet.h>
#include <linux/atm_suni.h>
#include <asm/io.h>
#include <asm/string.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/pci.h>

#ifdef CONFIG_ATM_FORE200E_SBA
#include <asm/idprom.h>
#include <asm/sbus.h>
#include <asm/openprom.h>
#include <asm/oplib.h>
#include <asm/pgtable.h>
#endif

#include <linux/module.h>

#include "fore200e.h"
#include "suni.h"

#if 1   /* ensure correct handling of 52-byte AAL0 SDUs used by atmdump-like apps */
#define FORE200E_52BYTE_AAL0_SDU
#endif

#define FORE200E_VERSION "0.2d"


#define FORE200E         "fore200e: "

#if defined(CONFIG_ATM_FORE200E_DEBUG) && (CONFIG_ATM_FORE200E_DEBUG > 0)
#define DPRINTK(level, format, args...)  do { if (CONFIG_ATM_FORE200E_DEBUG >= (level)) \
                                                  printk(FORE200E format, ##args); } while(0)
#else
#define DPRINTK(level, format, args...)  while(0)
#endif


#define FORE200E_ALIGN(addr, alignment) \
        ((((unsigned long)(addr) + (alignment - 1)) & ~(alignment - 1)) - (unsigned long)(addr))

#define FORE200E_DMA_INDEX(dma_addr, type, index)  ((dma_addr) + (index) * sizeof(type))

#define FORE200E_INDEX(virt_addr, type, index)     (&((type *)(virt_addr))[ index ])

#define FORE200E_NEXT_ENTRY(index, modulo)    (index = ++(index) % (modulo))


#define MSECS(ms)  (((ms)*HZ/1000)+1)


extern const struct atmdev_ops   fore200e_ops;
extern const struct fore200e_bus fore200e_bus[];

static struct fore200e* fore200e_boards = NULL;


#ifdef MODULE
MODULE_AUTHOR("Christophe Lizzi - credits to Uwe Dannowski and Heikki Vatiainen");
MODULE_DESCRIPTION("FORE Systems 200E-series ATM driver - version " FORE200E_VERSION);
MODULE_SUPPORTED_DEVICE("PCA-200E, SBA-200E");
#endif


static const int fore200e_rx_buf_nbr[ BUFFER_SCHEME_NBR ][ BUFFER_MAGN_NBR ] = {
    { BUFFER_S1_NBR, BUFFER_L1_NBR },
    { BUFFER_S2_NBR, BUFFER_L2_NBR }
};

static const int fore200e_rx_buf_size[ BUFFER_SCHEME_NBR ][ BUFFER_MAGN_NBR ] = {
    { BUFFER_S1_SIZE, BUFFER_L1_SIZE },
    { BUFFER_S2_SIZE, BUFFER_L2_SIZE }
};


#if defined(CONFIG_ATM_FORE200E_DEBUG) && (CONFIG_ATM_FORE200E_DEBUG > 0)
static const char* fore200e_traffic_class[] = { "NONE", "UBR", "CBR", "VBR", "ABR", "ANY" };
#endif


#if 0 /* currently unused */
static int 
fore200e_fore2atm_aal(enum fore200e_aal aal)
{
    switch(aal) {
    case FORE200E_AAL0:  return ATM_AAL0;
    case FORE200E_AAL34: return ATM_AAL34;
    case FORE200E_AAL5:  return ATM_AAL5;
    }

    return -EINVAL;
}
#endif


static enum fore200e_aal
fore200e_atm2fore_aal(int aal)
{
    switch(aal) {
    case ATM_AAL0:  return FORE200E_AAL0;
    case ATM_AAL34: return FORE200E_AAL34;
    case ATM_AAL1:
    case ATM_AAL2:
    case ATM_AAL5:  return FORE200E_AAL5;
    }

    return -EINVAL;
}


static char*
fore200e_irq_itoa(int irq)
{
#if defined(__sparc_v9__)
    return __irq_itoa(irq);
#else
    static char str[8];
    sprintf(str, "%d", irq);
    return str;
#endif
}


static void*
fore200e_kmalloc(int size, int flags)
{
    void* chunk = kmalloc(size, flags);

    if (chunk)
	memset(chunk, 0x00, size);
    else
	printk(FORE200E "kmalloc() failed, requested size = %d, flags = 0x%x\n", size, flags);
    
    return chunk;
}


static void
fore200e_kfree(void* chunk)
{
    kfree(chunk);
}


/* allocate and align a chunk of memory intended to hold the data behing exchanged
   between the driver and the adapter (using streaming DVMA) */

static int
fore200e_chunk_alloc(struct fore200e* fore200e, struct chunk* chunk, int size, int alignment, int direction)
{
    unsigned long offset = 0;

    if (alignment <= sizeof(int))
	alignment = 0;

    chunk->alloc_size = size + alignment;
    chunk->align_size = size;
    chunk->direction  = direction;

    chunk->alloc_addr = fore200e_kmalloc(chunk->alloc_size, GFP_KERNEL | GFP_DMA);
    if (chunk->alloc_addr == NULL)
	return -ENOMEM;

    if (alignment > 0)
	offset = FORE200E_ALIGN(chunk->alloc_addr, alignment); 
    
    chunk->align_addr = chunk->alloc_addr + offset;

    chunk->dma_addr = fore200e->bus->dma_map(fore200e, chunk->align_addr, chunk->align_size, direction);
    
    return 0;
}


/* free a chunk of memory */

static void
fore200e_chunk_free(struct fore200e* fore200e, struct chunk* chunk)
{
    fore200e->bus->dma_unmap(fore200e, chunk->dma_addr, chunk->dma_size, chunk->direction);

    fore200e_kfree(chunk->alloc_addr);
}



#if 0 /* currently unused */
static int
fore200e_checkup(struct fore200e* fore200e)
{
    u32 hb1, hb2;

    hb1 = fore200e->bus->read(&fore200e->cp_queues->heartbeat);
    fore200e_spin(10);
    hb2 = fore200e->bus->read(&fore200e->cp_queues->heartbeat);
    
    if (hb2 <= hb1) {
	printk(FORE200E "device %s heartbeat is not counting upwards, hb1 = %x; hb2 = %x\n",
	       fore200e->name, hb1, hb2);
	return -EIO;
    }
    printk(FORE200E "device %s heartbeat is ok\n", fore200e->name);
    
    return 0;
}
#endif 


static void
fore200e_spin(int msecs)
{
    unsigned long timeout = jiffies + MSECS(msecs);
    while (time_before(jiffies, timeout));
}


static int
fore200e_poll(struct fore200e* fore200e, volatile u32* addr, u32 val, int msecs)
{
    unsigned long timeout = jiffies + MSECS(msecs);
    int           ok;

    mb();
    do {
	if ((ok = (*addr == val)) || (*addr & STATUS_ERROR))
	    break;

    } while (time_before(jiffies, timeout));

#if 1
    if (!ok) {
	printk(FORE200E "cmd polling failed, got status 0x%08x, expected 0x%08x\n",
	       *addr, val);
    }
#endif

    return ok;
}


static int
fore200e_io_poll(struct fore200e* fore200e, volatile u32* addr, u32 val, int msecs)
{
    unsigned long timeout = jiffies + MSECS(msecs);
    int           ok;

    do {
	if ((ok = (fore200e->bus->read(addr) == val)))
	    break;

    } while (time_before(jiffies, timeout));

#if 1
    if (!ok) {
	printk(FORE200E "I/O polling failed, got status 0x%08x, expected 0x%08x\n",
	       fore200e->bus->read(addr), val);
    }
#endif

    return ok;
}


static void
fore200e_free_rx_buf(struct fore200e* fore200e)
{
    int scheme, magn, nbr;
    struct buffer* buffer;

    for (scheme = 0; scheme < BUFFER_SCHEME_NBR; scheme++) {
	for (magn = 0; magn < BUFFER_MAGN_NBR; magn++) {

	    if ((buffer = fore200e->host_bsq[ scheme ][ magn ].buffer) != NULL) {

		for (nbr = 0; nbr < fore200e_rx_buf_nbr[ scheme ][ magn ]; nbr++) {

		    struct chunk* data = &buffer[ nbr ].data;

		    if (data->alloc_addr != NULL)
			fore200e_chunk_free(fore200e, data);
		}
	    }
	}
    }
}


static void
fore200e_uninit_bs_queue(struct fore200e* fore200e)
{
    int scheme, magn;
    
    for (scheme = 0; scheme < BUFFER_SCHEME_NBR; scheme++) {
	for (magn = 0; magn < BUFFER_MAGN_NBR; magn++) {

	    struct chunk* status    = &fore200e->host_bsq[ scheme ][ magn ].status;
	    struct chunk* rbd_block = &fore200e->host_bsq[ scheme ][ magn ].rbd_block;
	    
	    if (status->alloc_addr)
		fore200e->bus->dma_chunk_free(fore200e, status);
	    
	    if (rbd_block->alloc_addr)
		fore200e->bus->dma_chunk_free(fore200e, rbd_block);
	}
    }
}


static int
fore200e_reset(struct fore200e* fore200e, int diag)
{
    int ok;

    fore200e->cp_monitor = (struct cp_monitor*)(fore200e->virt_base + FORE200E_CP_MONITOR_OFFSET);
    
    fore200e->bus->write(BSTAT_COLD_START, &fore200e->cp_monitor->bstat);

    fore200e->bus->reset(fore200e);

    if (diag) {
	ok = fore200e_io_poll(fore200e, &fore200e->cp_monitor->bstat, BSTAT_SELFTEST_OK, 1000);
	if (ok == 0) {
	    
	    printk(FORE200E "device %s self-test failed\n", fore200e->name);
	    return -ENODEV;
	}

	printk(FORE200E "device %s self-test passed\n", fore200e->name);
	
	fore200e->state = FORE200E_STATE_RESET;
    }

    return 0;
}


static void
fore200e_shutdown(struct fore200e* fore200e)
{
    printk(FORE200E "removing device %s at 0x%lx, IRQ %s\n",
	   fore200e->name, fore200e->phys_base, 
	   fore200e_irq_itoa(fore200e->irq));
    
    if (fore200e->state > FORE200E_STATE_RESET) {
	/* first, reset the board to prevent further interrupts or data transfers */
	fore200e_reset(fore200e, 0);
    }
    
    /* then, release all allocated resources */
    switch(fore200e->state) {

    case FORE200E_STATE_COMPLETE:
	if (fore200e->stats)
	    kfree(fore200e->stats);

    case FORE200E_STATE_IRQ:
	free_irq(fore200e->irq, fore200e->atm_dev);

    case FORE200E_STATE_ALLOC_BUF:
	fore200e_free_rx_buf(fore200e);

    case FORE200E_STATE_INIT_BSQ:
	fore200e_uninit_bs_queue(fore200e);

    case FORE200E_STATE_INIT_RXQ:
	fore200e->bus->dma_chunk_free(fore200e, &fore200e->host_rxq.status);
	fore200e->bus->dma_chunk_free(fore200e, &fore200e->host_rxq.rpd);

    case FORE200E_STATE_INIT_TXQ:
	fore200e->bus->dma_chunk_free(fore200e, &fore200e->host_txq.status);
	fore200e->bus->dma_chunk_free(fore200e, &fore200e->host_txq.tpd);

    case FORE200E_STATE_INIT_CMDQ:
	fore200e->bus->dma_chunk_free(fore200e, &fore200e->host_cmdq.status);

    case FORE200E_STATE_INITIALIZE:
	/* nothing to do for that state */

    case FORE200E_STATE_START_FW:
	/* nothing to do for that state */

    case FORE200E_STATE_LOAD_FW:
	/* nothing to do for that state */

    case FORE200E_STATE_RESET:
	/* nothing to do for that state */

    case FORE200E_STATE_MAP:
	fore200e->bus->unmap(fore200e);

    case FORE200E_STATE_CONFIGURE:
	/* nothing to do for that state */

    case FORE200E_STATE_REGISTER:
	/* XXX shouldn't we *start* by deregistering the device? */
	atm_dev_deregister(fore200e->atm_dev);

    case FORE200E_STATE_BLANK:
	/* nothing to do for that state */
	break;
    }
}



#ifdef CONFIG_ATM_FORE200E_PCA

static u32 fore200e_pca_read(volatile u32* addr)
{
    /* on big-endian hosts, the board is configured to convert
       the endianess of slave RAM accesses  */
    return le32_to_cpu(readl(addr));
}


static void fore200e_pca_write(u32 val, volatile u32* addr)
{
    /* on big-endian hosts, the board is configured to convert
       the endianess of slave RAM accesses  */
    writel(cpu_to_le32(val), addr);
}


static u32
fore200e_pca_dma_map(struct fore200e* fore200e, void* virt_addr, int size, int direction)
{
    u32 dma_addr = pci_map_single((struct pci_dev*)fore200e->bus_dev, virt_addr, size, direction);

    DPRINTK(3, "PCI DVMA mapping: virt_addr = 0x%p, size = %d, direction = %d,  --> dma_addr = 0x%08x\n",
	    virt_addr, size, direction, dma_addr);
    
    return dma_addr;
}


static void
fore200e_pca_dma_unmap(struct fore200e* fore200e, u32 dma_addr, int size, int direction)
{
    DPRINTK(3, "PCI DVMA unmapping: dma_addr = 0x%08x, size = %d, direction = %d\n",
	    dma_addr, size, direction);

    pci_unmap_single((struct pci_dev*)fore200e->bus_dev, dma_addr, size, direction);
}


static void
fore200e_pca_dma_sync(struct fore200e* fore200e, u32 dma_addr, int size, int direction)
{
    DPRINTK(3, "PCI DVMA sync: dma_addr = 0x%08x, size = %d, direction = %d\n", dma_addr, size, direction);

    pci_dma_sync_single((struct pci_dev*)fore200e->bus_dev, dma_addr, size, direction);
}


/* allocate a DMA consistent chunk of memory intended to act as a communication mechanism
   (to hold descriptors, status, queues, etc.) shared by the driver and the adapter */

static int
fore200e_pca_dma_chunk_alloc(struct fore200e* fore200e, struct chunk* chunk,
			     int size, int nbr, int alignment)
{
#if defined(__sparc_v9__)
    /* returned chunks are page-aligned */
    chunk->alloc_addr = pci_alloc_consistent((struct pci_dev*)fore200e->bus_dev,
					     chunk->alloc_size,
					     &chunk->dma_addr);
    
    if (chunk->alloc_addr == NULL || chunk->dma_addr == 0)
	return -ENOMEM;

    chunk->align_addr = chunk->alloc_addr;
#else
    if (fore200e_chunk_alloc(fore200e, chunk, size * nbr, alignment, FORE200E_DMA_BIDIRECTIONAL) < 0)
	return -ENOMEM;
#endif
    
    return 0;
}


/* free a DMA consistent chunk of memory */

static void
fore200e_pca_dma_chunk_free(struct fore200e* fore200e, struct chunk* chunk)
{
#if defined(__sparc_v9__)
    pci_free_consistent((struct pci_dev*)fore200e->bus_dev,
			chunk->alloc_size,
			chunk->alloc_addr,
			chunk->dma_addr);
#else
    fore200e_chunk_free(fore200e, chunk);
#endif
}


static int
fore200e_pca_irq_check(struct fore200e* fore200e)
{
    /* this is a 1 bit register */
    return readl(fore200e->regs.pca.psr);
}


static void
fore200e_pca_irq_ack(struct fore200e* fore200e)
{
    writel(PCA200E_HCR_CLRINTR, fore200e->regs.pca.hcr);
}


static void
fore200e_pca_reset(struct fore200e* fore200e)
{
    writel(PCA200E_HCR_RESET, fore200e->regs.pca.hcr);
    fore200e_spin(10);
    writel(0, fore200e->regs.pca.hcr);
}


static int __init
fore200e_pca_map(struct fore200e* fore200e)
{
    DPRINTK(2, "device %s being mapped in memory\n", fore200e->name);

    fore200e->virt_base = ioremap(fore200e->phys_base, PCA200E_IOSPACE_LENGTH);
    
    if (fore200e->virt_base == NULL) {
	printk(FORE200E "can't map device %s\n", fore200e->name);
	return -EFAULT;
    }

    DPRINTK(1, "device %s mapped to 0x%p\n", fore200e->name, fore200e->virt_base);

    /* gain access to the PCA-200E specific registers  */
    fore200e->regs.pca.hcr = (u32*)(fore200e->virt_base + PCA200E_HCR_OFFSET);
    fore200e->regs.pca.imr = (u32*)(fore200e->virt_base + PCA200E_IMR_OFFSET);
    fore200e->regs.pca.psr = (u32*)(fore200e->virt_base + PCA200E_PSR_OFFSET);

    fore200e->state = FORE200E_STATE_MAP;
    return 0;
}


static void
fore200e_pca_unmap(struct fore200e* fore200e)
{
    DPRINTK(2, "device %s being unmapped from memory\n", fore200e->name);

    /* XXX iounmap() does nothing on PowerPC (at least in 2.2.12 and 2.3.41),
       this leads to a kernel panic if the module is loaded and unloaded several times */
    if (fore200e->virt_base != NULL)
	iounmap(fore200e->virt_base);
}


static int __init
fore200e_pca_configure(struct fore200e* fore200e)
{
    struct pci_dev* pci_dev = (struct pci_dev*)fore200e->bus_dev;
    u8              master_ctrl;

    DPRINTK(2, "device %s being configured\n", fore200e->name);

    if ((pci_dev->irq == 0) || (pci_dev->irq == 0xFF)) {
	printk(FORE200E "incorrect IRQ setting - misconfigured PCI-PCI bridge?\n");
	return -EIO;
    }

	pci_read_config_byte(pci_dev, PCA200E_PCI_MASTER_CTRL, &master_ctrl);

    master_ctrl = master_ctrl
#if 0
	| PCA200E_CTRL_DIS_CACHE_RD
        | PCA200E_CTRL_DIS_WRT_INVAL
#endif
#if defined(__BIG_ENDIAN)
	/* request the PCA board to convert the endianess of slave RAM accesses */
	| PCA200E_CTRL_CONVERT_ENDIAN
#endif
	| PCA200E_CTRL_LARGE_PCI_BURSTS;
    
    pci_write_config_byte(pci_dev, PCA200E_PCI_MASTER_CTRL, master_ctrl);

    fore200e->state = FORE200E_STATE_CONFIGURE;
    return 0;
}


static struct fore200e* __init
fore200e_pca_detect(const struct fore200e_bus* bus, int index)
{
    struct fore200e* fore200e;
    struct pci_dev*  pci_dev = NULL;
    int              count = index;
    
    if (pci_present() == 0) {
	printk(FORE200E "no PCI subsystem\n");
	return NULL;
    }

    do {
	pci_dev = pci_find_device(PCI_VENDOR_ID_FORE, PCI_DEVICE_ID_FORE_PCA200E, pci_dev);
	if (pci_dev == NULL)
	    return NULL;
    } while (count--);

    if (pci_enable_device(pci_dev))
	return NULL;
    
    fore200e = fore200e_kmalloc(sizeof(struct fore200e), GFP_KERNEL);
    if (fore200e == NULL)
	return NULL;

    fore200e->bus       = bus;
    fore200e->bus_dev   = pci_dev;    
    fore200e->irq       = pci_dev->irq;
    fore200e->phys_base = pci_resource_start (pci_dev, 0);

#if defined(__powerpc__)
    fore200e->phys_base += KERNELBASE;
#endif

    sprintf(fore200e->name, "%s-%d", bus->model_name, index - 1);

    pci_set_master(pci_dev);

    return fore200e;
}


static int __init
fore200e_pca_prom_read(struct fore200e* fore200e, struct prom_data* prom)
{
    struct host_cmdq*       cmdq  = &fore200e->host_cmdq;
    struct host_cmdq_entry* entry = &cmdq->host_entry[ cmdq->head ];
    struct prom_opcode      opcode;
    int                     ok;
    u32                     prom_dma;

    FORE200E_NEXT_ENTRY(cmdq->head, QUEUE_SIZE_CMD);

    opcode.opcode = OPCODE_GET_PROM;
    opcode.pad    = 0;

    prom_dma = fore200e->bus->dma_map(fore200e, prom, sizeof(struct prom_data), FORE200E_DMA_FROMDEVICE);

    fore200e->bus->write(prom_dma, &entry->cp_entry->cmd.prom_block.prom_haddr);
    
    *entry->status = STATUS_PENDING;

    fore200e->bus->write(*(u32*)&opcode, (u32*)&entry->cp_entry->cmd.prom_block.opcode);

    ok = fore200e_poll(fore200e, entry->status, STATUS_COMPLETE, 400);

    *entry->status = STATUS_FREE;

    fore200e->bus->dma_unmap(fore200e, prom_dma, sizeof(struct prom_data), FORE200E_DMA_FROMDEVICE);

    if (ok == 0) {
	printk(FORE200E "unable to get PROM data from device %s\n", fore200e->name);
	return -EIO;
    }

#if defined(__BIG_ENDIAN)
    
#define swap_here(addr) (*((u32*)(addr)) = swab32( *((u32*)(addr)) ))

    /* MAC address is stored as little-endian */
    swap_here(&prom->mac_addr[0]);
    swap_here(&prom->mac_addr[4]);
#endif
    
    return 0;
}


static int
fore200e_pca_proc_read(struct fore200e* fore200e, char *page)
{
    struct pci_dev* pci_dev = (struct pci_dev*)fore200e->bus_dev;

    return sprintf(page, "   PCI bus/slot/function:\t%d/%d/%d\n",
		   pci_dev->bus->number, PCI_SLOT(pci_dev->devfn), PCI_FUNC(pci_dev->devfn));
}

#endif /* CONFIG_ATM_FORE200E_PCA */




#ifdef CONFIG_ATM_FORE200E_SBA

static u32
fore200e_sba_read(volatile u32* addr)
{
    return sbus_readl(addr);
}


static void
fore200e_sba_write(u32 val, volatile u32* addr)
{
    sbus_writel(val, addr);
}


static u32
fore200e_sba_dma_map(struct fore200e* fore200e, void* virt_addr, int size, int direction)
{
    u32 dma_addr = sbus_map_single((struct sbus_dev*)fore200e->bus_dev, virt_addr, size, direction);

    DPRINTK(3, "SBUS DVMA mapping: virt_addr = 0x%p, size = %d, direction = %d --> dma_addr = 0x%08x\n",
	    virt_addr, size, direction, dma_addr);
    
    return dma_addr;
}


static void
fore200e_sba_dma_unmap(struct fore200e* fore200e, u32 dma_addr, int size, int direction)
{
    DPRINTK(3, "SBUS DVMA unmapping: dma_addr = 0x%08x, size = %d, direction = %d,\n",
	    dma_addr, size, direction);

    sbus_unmap_single((struct sbus_dev*)fore200e->bus_dev, dma_addr, size, direction);
}


static void
fore200e_sba_dma_sync(struct fore200e* fore200e, u32 dma_addr, int size, int direction)
{
    DPRINTK(3, "SBUS DVMA sync: dma_addr = 0x%08x, size = %d, direction = %d\n", dma_addr, size, direction);
    
    sbus_dma_sync_single((struct sbus_dev*)fore200e->bus_dev, dma_addr, size, direction);
}


/* allocate a DVMA consistent chunk of memory intended to act as a communication mechanism
   (to hold descriptors, status, queues, etc.) shared by the driver and the adapter */

static int
fore200e_sba_dma_chunk_alloc(struct fore200e* fore200e, struct chunk* chunk,
			     int size, int nbr, int alignment)
{
    chunk->alloc_size = chunk->align_size = size * nbr;

    /* returned chunks are page-aligned */
    chunk->alloc_addr = sbus_alloc_consistent((struct sbus_dev*)fore200e->bus_dev,
					      chunk->alloc_size,
					      &chunk->dma_addr);

    if (chunk->alloc_addr == NULL || chunk->dma_addr == 0)
	return -ENOMEM;

    chunk->align_addr = chunk->alloc_addr;
    
    return 0;
}


/* free a DVMA consistent chunk of memory */

static void
fore200e_sba_dma_chunk_free(struct fore200e* fore200e, struct chunk* chunk)
{
    sbus_free_consistent((struct sbus_dev*)fore200e->bus_dev,
			 chunk->alloc_size,
			 chunk->alloc_addr,
			 chunk->dma_addr);
}


static void
fore200e_sba_irq_enable(struct fore200e* fore200e)
{
    u32 hcr = fore200e->bus->read(fore200e->regs.sba.hcr) & SBA200E_HCR_STICKY;
    fore200e->bus->write(hcr | SBA200E_HCR_INTR_ENA, fore200e->regs.sba.hcr);
}


static int
fore200e_sba_irq_check(struct fore200e* fore200e)
{
    return fore200e->bus->read(fore200e->regs.sba.hcr) & SBA200E_HCR_INTR_REQ;
}


static void
fore200e_sba_irq_ack(struct fore200e* fore200e)
{
    u32 hcr = fore200e->bus->read(fore200e->regs.sba.hcr) & SBA200E_HCR_STICKY;
    fore200e->bus->write(hcr | SBA200E_HCR_INTR_CLR, fore200e->regs.sba.hcr);
}


static void
fore200e_sba_reset(struct fore200e* fore200e)
{
    fore200e->bus->write(SBA200E_HCR_RESET, fore200e->regs.sba.hcr);
    fore200e_spin(10);
    fore200e->bus->write(0, fore200e->regs.sba.hcr);
}


static int __init
fore200e_sba_map(struct fore200e* fore200e)
{
    struct sbus_dev* sbus_dev = (struct sbus_dev*)fore200e->bus_dev;
    unsigned int bursts;

    /* gain access to the SBA-200E specific registers  */
    
    fore200e->regs.sba.hcr = (u32*)sbus_ioremap(&sbus_dev->resource[0], 0, SBA200E_HCR_LENGTH, "SBA HCR");
    fore200e->regs.sba.bsr = (u32*)sbus_ioremap(&sbus_dev->resource[1], 0, SBA200E_BSR_LENGTH, "SBA BSR");
    fore200e->regs.sba.isr = (u32*)sbus_ioremap(&sbus_dev->resource[2], 0, SBA200E_ISR_LENGTH, "SBA ISR");
    fore200e->virt_base    = (u32*)sbus_ioremap(&sbus_dev->resource[3], 0, SBA200E_RAM_LENGTH, "SBA RAM");

    if (fore200e->virt_base == NULL) {
	printk(FORE200E "unable to map RAM of device %s\n", fore200e->name);
	return -EFAULT;
    }

    DPRINTK(1, "device %s mapped to 0x%p\n", fore200e->name, fore200e->virt_base);
    
    fore200e->bus->write(0x02, fore200e->regs.sba.isr); /* XXX hardwired interrupt level */

    /* get the supported DVMA burst sizes */
    bursts = prom_getintdefault(sbus_dev->bus->prom_node, "burst-sizes", 0x00);

    if (sbus_can_dma_64bit(sbus_dev))
	sbus_set_sbus64(sbus_dev, bursts);

#if 0
    if (bursts & DMA_BURST16)
	fore200e->bus->write(SBA200E_BSR_BURST16, fore200e->regs.sba.bsr);
    else
    if (bursts & DMA_BURST8)
       fore200e->bus->write(SBA200E_BSR_BURST8, fore200e->regs.sba.bsr);
    else
    if (bursts & DMA_BURST4)
        fore200e->bus->write(SBA200E_BSR_BURST4, fore200e->regs.sba.bsr);
#endif

    fore200e->state = FORE200E_STATE_MAP;
    return 0;
}


static void
fore200e_sba_unmap(struct fore200e* fore200e)
{
    sbus_iounmap((ulong)fore200e->regs.sba.hcr, SBA200E_HCR_LENGTH);
    sbus_iounmap((ulong)fore200e->regs.sba.bsr, SBA200E_BSR_LENGTH);
    sbus_iounmap((ulong)fore200e->regs.sba.isr, SBA200E_ISR_LENGTH);
    sbus_iounmap((ulong)fore200e->virt_base,    SBA200E_RAM_LENGTH);
}


static int __init
fore200e_sba_configure(struct fore200e* fore200e)
{
    fore200e->state = FORE200E_STATE_CONFIGURE;
    return 0;
}


static struct fore200e* __init
fore200e_sba_detect(const struct fore200e_bus* bus, int index)
{
    struct fore200e*          fore200e;
    struct sbus_bus* sbus_bus;
    struct sbus_dev* sbus_dev = NULL;
    
    unsigned int     count = 0;
    
    for_each_sbus (sbus_bus) {
	for_each_sbusdev (sbus_dev, sbus_bus) {
	    if (strcmp(sbus_dev->prom_name, SBA200E_PROM_NAME) == 0) {
		if (count >= index)
		    goto found;
		count++;
	    }
	}
    }
    return NULL;
    
  found:
#if 1
    if (sbus_dev->num_registers != 4) {
	printk(FORE200E "this %s device has %d instead of 4 registers\n",
	       bus->model_name, sbus_dev->num_registers);
	return NULL;
    }
#endif

    fore200e = fore200e_kmalloc(sizeof(struct fore200e), GFP_KERNEL);
    if (fore200e == NULL)
	return NULL;

    fore200e->bus     = bus;
    fore200e->bus_dev = sbus_dev;
    fore200e->irq     = sbus_dev->irqs[ 0 ];

    fore200e->phys_base = (unsigned long)sbus_dev;

    sprintf(fore200e->name, "%s-%d", bus->model_name, index - 1);
    
    return fore200e;
}


static int __init
fore200e_sba_prom_read(struct fore200e* fore200e, struct prom_data* prom)
{
    struct sbus_dev* sbus_dev = (struct sbus_dev*) fore200e->bus_dev;
    int                       len;

    len = prom_getproperty(sbus_dev->prom_node, "macaddrlo2", &prom->mac_addr[ 4 ], 4);
    if (len < 0)
	return -EBUSY;

    len = prom_getproperty(sbus_dev->prom_node, "macaddrhi4", &prom->mac_addr[ 2 ], 4);
    if (len < 0)
	return -EBUSY;
    
    prom_getproperty(sbus_dev->prom_node, "serialnumber",
		     (char*)&prom->serial_number, sizeof(prom->serial_number));
    
    prom_getproperty(sbus_dev->prom_node, "promversion",
		     (char*)&prom->hw_revision, sizeof(prom->hw_revision));
    
    return 0;
}


static int
fore200e_sba_proc_read(struct fore200e* fore200e, char *page)
{
    struct sbus_dev* sbus_dev = (struct sbus_dev*)fore200e->bus_dev;

    return sprintf(page, "   SBUS slot/device:\t\t%d/'%s'\n", sbus_dev->slot, sbus_dev->prom_name);
}
#endif /* CONFIG_ATM_FORE200E_SBA */


static void
fore200e_irq_tx(struct fore200e* fore200e)
{
    struct host_txq_entry* entry;
    int i;
    
    entry = fore200e->host_txq.host_entry;

    for (i = 0; i < QUEUE_SIZE_TX; i++) {

	if (*entry->status & STATUS_COMPLETE) {

	    DPRINTK(3, "TX COMPLETED: entry = %p, vcc = %p, skb = %p\n", entry, entry->vcc, entry->skb);

	    /* free copy of misaligned data */
	    if (entry->data)
		kfree(entry->data);

	    /* remove DMA mapping */
	    fore200e->bus->dma_unmap(fore200e, entry->tpd->tsd[ 0 ].buffer, entry->tpd->tsd[ 0 ].length,
				     FORE200E_DMA_TODEVICE);

	    /* notify tx completion */
	    if (entry->vcc->pop)
		entry->vcc->pop(entry->vcc, entry->skb);
	    else
		dev_kfree_skb_irq(entry->skb);

	    /* check error condition */
	    if (*entry->status & STATUS_ERROR)
		atomic_inc(&entry->vcc->stats->tx_err);
	    else
		atomic_inc(&entry->vcc->stats->tx);

	    *entry->status = STATUS_FREE;
	    
	    fore200e->host_txq.txing--;
	}
	entry++;
    }
}


static void
fore200e_supply(struct fore200e* fore200e)
{
    int  scheme, magn, i;

    struct host_bsq*       bsq;
    struct host_bsq_entry* entry;
    struct buffer*         buffer;

    for (scheme = 0; scheme < BUFFER_SCHEME_NBR; scheme++) {
	for (magn = 0; magn < BUFFER_MAGN_NBR; magn++) {

	    bsq = &fore200e->host_bsq[ scheme ][ magn ];

	    if (fore200e_rx_buf_nbr[ scheme ][ magn ] - bsq->count > RBD_BLK_SIZE) {

		DPRINTK(2, "supplying rx buffers to queue %d / %d, count = %d\n",
			scheme, magn, bsq->count);

		entry = &bsq->host_entry[ bsq->head ];
		
		FORE200E_NEXT_ENTRY(bsq->head, QUEUE_SIZE_BS);

		for (i = 0; i < RBD_BLK_SIZE; i++) {

		    buffer = &bsq->buffer[ bsq->free ];
		    
		    FORE200E_NEXT_ENTRY(bsq->free, fore200e_rx_buf_nbr[ scheme ][ magn ]);
		    
		    entry->rbd_block->rbd[ i ].buffer_haddr = buffer->data.dma_addr;
		    entry->rbd_block->rbd[ i ].handle       = FORE200E_BUF2HDL(buffer);
		}

		/* increase the number of supplied rx buffers */
		bsq->count += RBD_BLK_SIZE;
		
		*entry->status = STATUS_PENDING;
		fore200e->bus->write(entry->rbd_block_dma, &entry->cp_entry->rbd_block_haddr);
	    }
	}
    }
}



static struct atm_vcc* 
fore200e_find_vcc(struct fore200e* fore200e, struct rpd* rpd)
{
    struct sock *s;
    struct atm_vcc* vcc;

    read_lock(&vcc_sklist_lock);
    for(s = vcc_sklist; s; s = s->next) {
	vcc = s->protinfo.af_atm;
	if (vcc->dev != fore200e->atm_dev)
		continue;
	if (vcc->vpi == rpd->atm_header.vpi && vcc->vci == rpd->atm_header.vci) {
            read_unlock(&vcc_sklist_lock);
	    return vcc;
	}
    }
    read_unlock(&vcc_sklist_lock);

    return NULL;
}


static void
fore200e_push_rpd(struct fore200e* fore200e, struct rpd* rpd)
{
    struct atm_vcc*      vcc;
    struct sk_buff*      skb;
    struct buffer*       buffer;
    struct fore200e_vcc* fore200e_vcc;
    int                  i, pdu_len = 0;
#ifdef FORE200E_52BYTE_AAL0_SDU
    u32                  cell_header = 0;
#endif

    vcc = fore200e_find_vcc(fore200e, rpd);
    if (vcc == NULL) {
	
	printk(FORE200E "no vcc found for PDU received on %d.%d.%d\n",
	       fore200e->atm_dev->number, rpd->atm_header.vpi, rpd->atm_header.vci);
	return;
    }

    fore200e_vcc = FORE200E_VCC(vcc);

#ifdef FORE200E_52BYTE_AAL0_SDU
    if ((vcc->qos.aal == ATM_AAL0) && (vcc->qos.rxtp.max_sdu == ATM_AAL0_SDU)) {

	cell_header = (rpd->atm_header.gfc << ATM_HDR_GFC_SHIFT) |
	              (rpd->atm_header.vpi << ATM_HDR_VPI_SHIFT) |
                      (rpd->atm_header.vci << ATM_HDR_VCI_SHIFT) |
                      (rpd->atm_header.plt << ATM_HDR_PTI_SHIFT) | 
                       rpd->atm_header.clp;
	pdu_len = 4;
    }
#endif
    
    /* compute total PDU length */
    for (i = 0; i < rpd->nseg; i++)
	pdu_len += rpd->rsd[ i ].length;
    
    skb = alloc_skb(pdu_len, GFP_ATOMIC);
    if (skb == NULL) {
	
	printk(FORE200E "unable to alloc new skb, rx PDU length = %d\n", pdu_len);
	atomic_inc(&vcc->stats->rx_drop);
	return;
    } 

    skb->stamp = xtime;
    
#ifdef FORE200E_52BYTE_AAL0_SDU
    if (cell_header) {
	*((u32*)skb_put(skb, 4)) = cell_header;
    }
#endif

    /* reassemble segments */
    for (i = 0; i < rpd->nseg; i++) {
	
	/* rebuild rx buffer address from rsd handle */
	buffer = FORE200E_HDL2BUF(rpd->rsd[ i ].handle);
	
	/* ensure DMA synchronisation */
	fore200e->bus->dma_sync(fore200e, buffer->data.dma_addr, rpd->rsd[ i ].length, FORE200E_DMA_FROMDEVICE);
	
	memcpy(skb_put(skb, rpd->rsd[ i ].length), buffer->data.align_addr, rpd->rsd[ i ].length);
    }
    
    DPRINTK(3, "rx skb: len = %d, truesize = %d\n", skb->len, skb->truesize);
    
    if (pdu_len < fore200e_vcc->rx_min_pdu)
	fore200e_vcc->rx_min_pdu = pdu_len;
    if (pdu_len > fore200e_vcc->rx_max_pdu)
	fore200e_vcc->rx_max_pdu = pdu_len;

    /* push PDU */
    if (atm_charge(vcc, skb->truesize) == 0) {

	DPRINTK(2, "receive buffers saturated for %d.%d.%d - PDU dropped\n",
		vcc->itf, vcc->vpi, vcc->vci);

	dev_kfree_skb_irq(skb);
	return;
    }

    vcc->push(vcc, skb);
    atomic_inc(&vcc->stats->rx);
}


static void
fore200e_collect_rpd(struct fore200e* fore200e, struct rpd* rpd)
{
    struct buffer* buffer;
    int            i;
    
    for (i = 0; i < rpd->nseg; i++) {

	/* rebuild rx buffer address from rsd handle */
	buffer = FORE200E_HDL2BUF(rpd->rsd[ i ].handle);

	/* decrease the number of supplied rx buffers */
	fore200e->host_bsq[ buffer->scheme ][ buffer->magn ].count--;
    }
}


static void
fore200e_irq_rx(struct fore200e* fore200e)
{
    struct host_rxq*       rxq = &fore200e->host_rxq;
    struct host_rxq_entry* entry;

    for (;;) {
	
	entry = &rxq->host_entry[ rxq->head ];

	/* no more received PDUs */
	if ((*entry->status & STATUS_COMPLETE) == 0)
	    break;

	FORE200E_NEXT_ENTRY(rxq->head, QUEUE_SIZE_RX);

	if ((*entry->status & STATUS_ERROR) == 0) {

	    fore200e_push_rpd(fore200e, entry->rpd);
	}
	else {
	    printk(FORE200E "damaged PDU on %d.%d.%d\n", 
		   fore200e->atm_dev->number, entry->rpd->atm_header.vpi, entry->rpd->atm_header.vci);
	}

	fore200e_collect_rpd(fore200e, entry->rpd);

	fore200e_supply(fore200e);

	/* rewrite the rpd address to ack the received PDU */
	fore200e->bus->write(entry->rpd_dma, &entry->cp_entry->rpd_haddr);
	*entry->status = STATUS_FREE;
    }
}


static void
fore200e_interrupt(int irq, void* dev, struct pt_regs* regs)
{
    struct fore200e* fore200e = FORE200E_DEV((struct atm_dev*)dev);

    if (fore200e->bus->irq_check(fore200e) == 0) {
	
	DPRINTK(3, "unexpected interrupt on device %c\n", fore200e->name[9]);
	return;
    }
    DPRINTK(3, "valid interrupt on device %c\n", fore200e->name[9]);

    tasklet_schedule(&fore200e->tasklet);
    
    fore200e->bus->irq_ack(fore200e);
}


static void
fore200e_tasklet(unsigned long data)
{
    struct fore200e* fore200e = (struct fore200e*) data;

    fore200e_irq_rx(fore200e);
    
    if (fore200e->host_txq.txing)
	fore200e_irq_tx(fore200e);
}



static int
fore200e_select_scheme(struct atm_vcc* vcc)
{
    int scheme;

#if 1
    /* fairly balance VCs over (identical) buffer schemes */
    scheme =  vcc->vci % 2 ? BUFFER_SCHEME_ONE : BUFFER_SCHEME_TWO;
#else
    /* bit 7 of VPI magically selects the second buffer scheme */
    if (vcc->vpi & (1<<7)) {
	vcc->vpi &= ((1<<7) - 1);    /* reset the magic bit */
	scheme = BUFFER_SCHEME_TWO;
    }
    else {
	scheme = BUFFER_SCHEME_ONE;
    }
#endif
    
    DPRINTK(1, "vpvc %d.%d.%d uses the %s buffer scheme\n",
	    vcc->itf, vcc->vpi, vcc->vci, scheme == BUFFER_SCHEME_ONE ? "first" : "second");

    return scheme;
}



static int 
fore200e_activate_vcin(struct fore200e* fore200e, int activate, struct atm_vcc* vcc, int mtu)
{
    struct host_cmdq*        cmdq  = &fore200e->host_cmdq;
    struct host_cmdq_entry*  entry = &cmdq->host_entry[ cmdq->head ];
    struct activate_opcode   activ_opcode;
    struct deactivate_opcode deactiv_opcode;
    struct vpvc              vpvc;
    int                      ok;
    enum fore200e_aal        aal = fore200e_atm2fore_aal(vcc->qos.aal);

    FORE200E_NEXT_ENTRY(cmdq->head, QUEUE_SIZE_CMD);
    
    if (activate) {
	FORE200E_VCC(vcc)->scheme = fore200e_select_scheme(vcc);
	
	activ_opcode.opcode = OPCODE_ACTIVATE_VCIN;
	activ_opcode.aal    = aal;
	activ_opcode.scheme = FORE200E_VCC(vcc)->scheme;
	activ_opcode.pad    = 0;
    }
    else {
	deactiv_opcode.opcode = OPCODE_DEACTIVATE_VCIN;
	deactiv_opcode.pad    = 0;
    }

    vpvc.vci = vcc->vci;
    vpvc.vpi = vcc->vpi;

    *entry->status = STATUS_PENDING;

    if (activate) {

#ifdef FORE200E_52BYTE_AAL0_SDU
	mtu = 48;
#endif
	/* the MTU is unused by the cp, except in the case of AAL0 */
	fore200e->bus->write(mtu,                        &entry->cp_entry->cmd.activate_block.mtu);
	fore200e->bus->write(*(u32*)&vpvc,         (u32*)&entry->cp_entry->cmd.activate_block.vpvc);
	fore200e->bus->write(*(u32*)&activ_opcode, (u32*)&entry->cp_entry->cmd.activate_block.opcode);
    }
    else {
	fore200e->bus->write(*(u32*)&vpvc,           (u32*)&entry->cp_entry->cmd.deactivate_block.vpvc);
	fore200e->bus->write(*(u32*)&deactiv_opcode, (u32*)&entry->cp_entry->cmd.deactivate_block.opcode);
    }

    ok = fore200e_poll(fore200e, entry->status, STATUS_COMPLETE, 400);

    *entry->status = STATUS_FREE;

    if (ok == 0) {
	printk(FORE200E "unable to %s vpvc %d.%d on device %s\n",
	       activate ? "open" : "close", vcc->vpi, vcc->vci, fore200e->name);
	return -EIO;
    }

    DPRINTK(1, "vpvc %d.%d %sed on device %s\n", vcc->vpi, vcc->vci, 
	    activate ? "open" : "clos", fore200e->name);

    return 0;
}


static int
fore200e_walk_vccs(struct atm_vcc *vcc, short *vpi, int *vci)
{
    struct atm_vcc* walk;
    struct sock *s;

    /* find a free VPI */

    read_lock(&vcc_sklist_lock);

    if (*vpi == ATM_VPI_ANY) {

	for (*vpi = 0, s = vcc_sklist; s; s = s->next) {
	    walk = s->protinfo.af_atm;
	    if (walk->dev != vcc->dev)
		continue;

	    if ((walk->vci == *vci) && (walk->vpi == *vpi)) {
		(*vpi)++;
		s = vcc_sklist;
	    }
	}
    }

    /* find a free VCI */
    if (*vci == ATM_VCI_ANY) {
	
	for (*vci = ATM_NOT_RSV_VCI, s = vcc_sklist; s; s = s->next) {
	    walk = s->protinfo.af_atm;
	    if (walk->dev != vcc->dev)
		continue;

	    if ((walk->vpi = *vpi) && (walk->vci == *vci)) {
		*vci = walk->vci + 1;
		s = vcc_sklist;
	    }
	}
    }

    read_unlock(&vcc_sklist_lock);

    return 0;
}


#define FORE200E_MAX_BACK2BACK_CELLS 255    /* XXX depends on CDVT */

static void
fore200e_rate_ctrl(struct atm_qos* qos, struct tpd_rate* rate)
{
    if (qos->txtp.max_pcr < ATM_OC3_PCR) {
    
	/* compute the data cells to idle cells ratio from the PCR */
	rate->data_cells = qos->txtp.max_pcr * FORE200E_MAX_BACK2BACK_CELLS / ATM_OC3_PCR;
	rate->idle_cells = FORE200E_MAX_BACK2BACK_CELLS - rate->data_cells;
    }
    else {
	/* disable rate control */
	rate->data_cells = rate->idle_cells = 0;
    }
}


static int
fore200e_open(struct atm_vcc *vcc, short vpi, int vci)
{
    struct fore200e*     fore200e = FORE200E_DEV(vcc->dev);
    struct fore200e_vcc* fore200e_vcc;
    
    /* find a free VPI/VCI */
    fore200e_walk_vccs(vcc, &vpi, &vci);

    vcc->vpi = vpi;
    vcc->vci = vci;

    /* ressource checking only? */
    if (vci == ATM_VCI_UNSPEC || vpi == ATM_VPI_UNSPEC)
	return 0;

    set_bit(ATM_VF_ADDR, &vcc->flags);
    vcc->itf    = vcc->dev->number;

    DPRINTK(2, "opening %d.%d.%d:%d QoS = (tx: cl=%s, pcr=%d-%d, cdv=%d, max_sdu=%d; "
	    "rx: cl=%s, pcr=%d-%d, cdv=%d, max_sdu=%d)\n",
	    vcc->itf, vcc->vpi, vcc->vci, fore200e_atm2fore_aal(vcc->qos.aal),
	    fore200e_traffic_class[ vcc->qos.txtp.traffic_class ],
	    vcc->qos.txtp.min_pcr, vcc->qos.txtp.max_pcr, vcc->qos.txtp.max_cdv, vcc->qos.txtp.max_sdu,
	    fore200e_traffic_class[ vcc->qos.rxtp.traffic_class ],
	    vcc->qos.rxtp.min_pcr, vcc->qos.rxtp.max_pcr, vcc->qos.rxtp.max_cdv, vcc->qos.rxtp.max_sdu);
    
    if ((vcc->qos.txtp.traffic_class == ATM_CBR) && (vcc->qos.txtp.max_pcr > 0)) {
	
	down(&fore200e->rate_sf);
	if (fore200e->available_cell_rate < vcc->qos.txtp.max_pcr) {
	    up(&fore200e->rate_sf);
	    return -EAGAIN;
	}
	/* reserving the pseudo-CBR bandwidth at this point grants us
	   to reduce the length of the critical section protected
	   by 'rate_sf'. in counterpart, we have to reset the available
	   bandwidth if we later encounter an error */

	fore200e->available_cell_rate -= vcc->qos.txtp.max_pcr;
	up(&fore200e->rate_sf);
    }
    
    fore200e_vcc = fore200e_kmalloc(sizeof(struct fore200e_vcc), GFP_KERNEL);
    if (fore200e_vcc == NULL) {
	down(&fore200e->rate_sf);
	fore200e->available_cell_rate += vcc->qos.txtp.max_pcr;
	up(&fore200e->rate_sf);
	return -ENOMEM;
    }

    FORE200E_VCC(vcc) = fore200e_vcc;
    
    if (fore200e_activate_vcin(fore200e, 1, vcc, vcc->qos.rxtp.max_sdu) < 0) {
	kfree(fore200e_vcc);
	down(&fore200e->rate_sf);
	fore200e->available_cell_rate += vcc->qos.txtp.max_pcr;
	up(&fore200e->rate_sf);
	return -EBUSY;
    }
    
    /* compute rate control parameters */
    if ((vcc->qos.txtp.traffic_class == ATM_CBR) && (vcc->qos.txtp.max_pcr > 0)) {
	
	fore200e_rate_ctrl(&vcc->qos, &fore200e_vcc->rate);

	DPRINTK(3, "tx on %d.%d.%d:%d, tx PCR = %d, rx PCR = %d, data_cells = %u, idle_cells = %u\n",
		vcc->itf, vcc->vpi, vcc->vci, fore200e_atm2fore_aal(vcc->qos.aal),
		vcc->qos.txtp.max_pcr, vcc->qos.rxtp.max_pcr, 
		fore200e_vcc->rate.data_cells, fore200e_vcc->rate.idle_cells);
    }
    
    fore200e_vcc->tx_min_pdu = fore200e_vcc->rx_min_pdu = 65536;
    fore200e_vcc->tx_max_pdu = fore200e_vcc->rx_max_pdu = 0;
    
    set_bit(ATM_VF_READY, &vcc->flags);
    return 0;
}



static void
fore200e_close(struct atm_vcc* vcc)
{
    struct fore200e* fore200e = FORE200E_DEV(vcc->dev);
    
    DPRINTK(2, "closing %d.%d.%d:%d\n", vcc->itf, vcc->vpi, vcc->vci, fore200e_atm2fore_aal(vcc->qos.aal));
    
    fore200e_activate_vcin(fore200e, 0, vcc, 0);
    
    kfree(FORE200E_VCC(vcc));
	
    if ((vcc->qos.txtp.traffic_class == ATM_CBR) && (vcc->qos.txtp.max_pcr > 0)) {
	down(&fore200e->rate_sf);
	fore200e->available_cell_rate += vcc->qos.txtp.max_pcr;
	up(&fore200e->rate_sf);
    }

    clear_bit(ATM_VF_READY, &vcc->flags);
}


#if 0
#define FORE200E_SYNC_SEND    /* wait tx completion before returning */
#endif


static int
fore200e_send(struct atm_vcc *vcc, struct sk_buff *skb)
{
    struct fore200e*       fore200e     = FORE200E_DEV(vcc->dev);
    struct fore200e_vcc*   fore200e_vcc = FORE200E_VCC(vcc);
    struct host_txq*       txq          = &fore200e->host_txq;
    struct host_txq_entry* entry;
    struct tpd*            tpd;
    struct tpd_haddr       tpd_haddr;
    //unsigned long          flags;
    int                    retry        = CONFIG_ATM_FORE200E_TX_RETRY;
    int                    tx_copy      = 0;
    int                    tx_len       = skb->len;
    u32*                   cell_header  = NULL;
    unsigned char*         skb_data;
    int                    skb_len;

#ifdef FORE200E_52BYTE_AAL0_SDU
    if ((vcc->qos.aal == ATM_AAL0) && (vcc->qos.txtp.max_sdu == ATM_AAL0_SDU)) {
	cell_header = (u32*) skb->data;
	skb_data    = skb->data + 4;    /* skip 4-byte cell header */
	skb_len     = tx_len = skb->len  - 4;

	DPRINTK(3, "skipping user-supplied cell header 0x%08x", *cell_header);
    }
    else 
#endif
    {
	skb_data = skb->data;
	skb_len  = skb->len;
    }
    
  retry_here:
    
    tasklet_disable(&fore200e->tasklet);

    entry = &txq->host_entry[ txq->head ];
    
    if (*entry->status != STATUS_FREE) {
	
	/* try to free completed tx queue entries */
	fore200e_irq_tx(fore200e);
	
	if (*entry->status != STATUS_FREE) {
	    
	    tasklet_enable(&fore200e->tasklet);

	    /* retry once again? */
	    if(--retry > 0)
		goto retry_here;
	    
	    atomic_inc(&vcc->stats->tx_err);
	    
	    printk(FORE200E "tx queue of device %s is saturated, PDU dropped - heartbeat is %08x\n",
		   fore200e->name, fore200e->cp_queues->heartbeat);
	    if (vcc->pop)
		vcc->pop(vcc, skb);
	    else
		dev_kfree_skb(skb);
	    return -EIO;
	}
    }

    tpd = entry->tpd;

    if (((unsigned long)skb_data) & 0x3) {

	DPRINTK(2, "misaligned tx PDU on device %s\n", fore200e->name);
	tx_copy = 1;
	tx_len  = skb_len;
    }

    if ((vcc->qos.aal == ATM_AAL0) && (skb_len % ATM_CELL_PAYLOAD)) {

	/* this simply NUKES the PCA-200E board */
	DPRINTK(2, "incomplete tx AAL0 PDU on device %s\n", fore200e->name);
	tx_copy = 1;
	tx_len  = ((skb_len / ATM_CELL_PAYLOAD) + 1) * ATM_CELL_PAYLOAD;
    }
    
    if (tx_copy) {
	
	entry->data = kmalloc(tx_len, GFP_ATOMIC | GFP_DMA);
	if (entry->data == NULL) {
	    
	    tasklet_enable(&fore200e->tasklet);
	    if (vcc->pop)
		vcc->pop(vcc, skb);
	    else
		dev_kfree_skb(skb);
	    return -ENOMEM;
	}

	memcpy(entry->data, skb_data, skb_len);
	if (skb_len < tx_len)
	    memset(entry->data + skb_len, 0x00, tx_len - skb_len);
	
	tpd->tsd[ 0 ].buffer = fore200e->bus->dma_map(fore200e, entry->data, tx_len, FORE200E_DMA_TODEVICE);
    }
    else {
	entry->data = NULL;
	tpd->tsd[ 0 ].buffer = fore200e->bus->dma_map(fore200e, skb_data, tx_len, FORE200E_DMA_TODEVICE);
    }

    tpd->tsd[ 0 ].length = tx_len;

    FORE200E_NEXT_ENTRY(txq->head, QUEUE_SIZE_TX);
    txq->txing++;

    tasklet_enable(&fore200e->tasklet);

    /* ensure DMA synchronisation */
    fore200e->bus->dma_sync(fore200e, tpd->tsd[ 0 ].buffer, tpd->tsd[ 0 ].length, FORE200E_DMA_TODEVICE);
    
    DPRINTK(3, "tx on %d.%d.%d:%d, len = %u (%u)\n", 
	    vcc->itf, vcc->vpi, vcc->vci, fore200e_atm2fore_aal(vcc->qos.aal),
	    tpd->tsd[0].length, skb_len);

    if (skb_len < fore200e_vcc->tx_min_pdu)
	fore200e_vcc->tx_min_pdu = skb_len;
    if (skb_len > fore200e_vcc->tx_max_pdu)
	fore200e_vcc->tx_max_pdu = skb_len;
    
    entry->vcc = vcc;
    entry->skb = skb;

    /* set tx rate control information */
    tpd->rate.data_cells = fore200e_vcc->rate.data_cells;
    tpd->rate.idle_cells = fore200e_vcc->rate.idle_cells;

    if (cell_header) {
	tpd->atm_header.clp = (*cell_header & ATM_HDR_CLP);
	tpd->atm_header.plt = (*cell_header & ATM_HDR_PTI_MASK) >> ATM_HDR_PTI_SHIFT;
	tpd->atm_header.vci = (*cell_header & ATM_HDR_VCI_MASK) >> ATM_HDR_VCI_SHIFT;
	tpd->atm_header.vpi = (*cell_header & ATM_HDR_VPI_MASK) >> ATM_HDR_VPI_SHIFT;
	tpd->atm_header.gfc = (*cell_header & ATM_HDR_GFC_MASK) >> ATM_HDR_GFC_SHIFT;
    }
    else {
	/* set the ATM header, common to all cells conveying the PDU */
	tpd->atm_header.clp = 0;
	tpd->atm_header.plt = 0;
	tpd->atm_header.vci = vcc->vci;
	tpd->atm_header.vpi = vcc->vpi;
	tpd->atm_header.gfc = 0;
    }

    tpd->spec.length = tx_len;
    tpd->spec.nseg   = 1;
    tpd->spec.aal    = fore200e_atm2fore_aal(vcc->qos.aal);
#ifdef FORE200E_SYNC_SEND
    tpd->spec.intr   = 0;
#else
    tpd->spec.intr   = 1;
#endif

    tpd_haddr.size  = sizeof(struct tpd) / 32;    /* size is expressed in 32 byte blocks */
    tpd_haddr.pad   = 0;
    tpd_haddr.haddr = entry->tpd_dma >> 5;        /* shift the address, as we are in a bitfield */

    *entry->status = STATUS_PENDING;
    fore200e->bus->write(*(u32*)&tpd_haddr, (u32*)&entry->cp_entry->tpd_haddr);


#ifdef FORE200E_SYNC_SEND
    {
	int ok = fore200e_poll(fore200e, entry->status, STATUS_COMPLETE, 10);

	fore200e->bus->dma_unmap(fore200e, entry->tpd->tsd[ 0 ].buffer, entry->tpd->tsd[ 0 ].length,
				 FORE200E_DMA_TODEVICE);
	
	/* free tmp copy of misaligned data */
	if (entry->data)
	    kfree(entry->data);

	/* notify tx completion */
	if (vcc->pop)
	    vcc->pop(vcc, skb);
	else
	    dev_kfree_skb(skb);

	if (ok == 0) {
	    printk(FORE200E "synchronous tx on %d:%d:%d failed\n", vcc->itf, vcc->vpi, vcc->vci);

	    atomic_inc(&entry->vcc->stats->tx_err);
	    return -EIO;
	}
	atomic_inc(&entry->vcc->stats->tx);

	DPRINTK(3, "synchronous tx on %d:%d:%d succeeded\n", vcc->itf, vcc->vpi, vcc->vci);

    }
#endif

    return 0;
}


static int
fore200e_getstats(struct fore200e* fore200e)
{
    struct host_cmdq*       cmdq  = &fore200e->host_cmdq;
    struct host_cmdq_entry* entry = &cmdq->host_entry[ cmdq->head ];
    struct stats_opcode     opcode;
    int                     ok;
    u32                     stats_dma_addr;

    if (fore200e->stats == NULL) {
	fore200e->stats = fore200e_kmalloc(sizeof(struct stats), GFP_KERNEL | GFP_DMA);
	if (fore200e->stats == NULL)
	    return -ENOMEM;
    }
    
    stats_dma_addr = fore200e->bus->dma_map(fore200e, fore200e->stats, sizeof(struct stats), FORE200E_DMA_FROMDEVICE);
    
    FORE200E_NEXT_ENTRY(cmdq->head, QUEUE_SIZE_CMD);

    opcode.opcode = OPCODE_GET_STATS;
    opcode.pad    = 0;

    fore200e->bus->write(stats_dma_addr, &entry->cp_entry->cmd.stats_block.stats_haddr);
    
    *entry->status = STATUS_PENDING;

    fore200e->bus->write(*(u32*)&opcode, (u32*)&entry->cp_entry->cmd.stats_block.opcode);

    ok = fore200e_poll(fore200e, entry->status, STATUS_COMPLETE, 400);

    *entry->status = STATUS_FREE;

    fore200e->bus->dma_unmap(fore200e, stats_dma_addr, sizeof(struct stats), FORE200E_DMA_FROMDEVICE);
    
    if (ok == 0) {
	printk(FORE200E "unable to get statistics from device %s\n", fore200e->name);
	return -EIO;
    }

    return 0;
}


static int
fore200e_getsockopt (struct atm_vcc* vcc, int level, int optname, void* optval, int optlen)
{
    // struct fore200e* fore200e = FORE200E_DEV(vcc->dev);

    DPRINTK(2, "getsockopt %d.%d.%d, level = %d, optname = 0x%x, optval = 0x%p, optlen = %d\n",
	    vcc->itf, vcc->vpi, vcc->vci, level, optname, optval, optlen);

    return -EINVAL;
}


static int
fore200e_setsockopt(struct atm_vcc* vcc, int level, int optname, void* optval, int optlen)
{
    // struct fore200e* fore200e = FORE200E_DEV(vcc->dev);
    
    DPRINTK(2, "setsockopt %d.%d.%d, level = %d, optname = 0x%x, optval = 0x%p, optlen = %d\n",
	    vcc->itf, vcc->vpi, vcc->vci, level, optname, optval, optlen);
    
    return -EINVAL;
}


#if 0 /* currently unused */
static int
fore200e_get_oc3(struct fore200e* fore200e, struct oc3_regs* regs)
{
    struct host_cmdq*       cmdq  = &fore200e->host_cmdq;
    struct host_cmdq_entry* entry = &cmdq->host_entry[ cmdq->head ];
    struct oc3_opcode       opcode;
    int                     ok;
    u32                     oc3_regs_dma_addr;

    oc3_regs_dma_addr = fore200e->bus->dma_map(fore200e, regs, sizeof(struct oc3_regs), FORE200E_DMA_FROMDEVICE);

    FORE200E_NEXT_ENTRY(cmdq->head, QUEUE_SIZE_CMD);

    opcode.opcode = OPCODE_GET_OC3;
    opcode.reg    = 0;
    opcode.value  = 0;
    opcode.mask   = 0;

    fore200e->bus->write(oc3_regs_dma_addr, &entry->cp_entry->cmd.oc3_block.regs_haddr);
    
    *entry->status = STATUS_PENDING;

    fore200e->bus->write(*(u32*)&opcode, (u32*)&entry->cp_entry->cmd.oc3_block.opcode);

    ok = fore200e_poll(fore200e, entry->status, STATUS_COMPLETE, 400);

    *entry->status = STATUS_FREE;

    fore200e->bus->dma_unmap(fore200e, oc3_regs_dma_addr, sizeof(struct oc3_regs), FORE200E_DMA_FROMDEVICE);
    
    if (ok == 0) {
	printk(FORE200E "unable to get OC-3 regs of device %s\n", fore200e->name);
	return -EIO;
    }

    return 0;
}
#endif


static int
fore200e_set_oc3(struct fore200e* fore200e, u32 reg, u32 value, u32 mask)
{
    struct host_cmdq*       cmdq  = &fore200e->host_cmdq;
    struct host_cmdq_entry* entry = &cmdq->host_entry[ cmdq->head ];
    struct oc3_opcode       opcode;
    int                     ok;

    FORE200E_NEXT_ENTRY(cmdq->head, QUEUE_SIZE_CMD);

    opcode.opcode = OPCODE_SET_OC3;
    opcode.reg    = reg;
    opcode.value  = value;
    opcode.mask   = mask;

    fore200e->bus->write(0, &entry->cp_entry->cmd.oc3_block.regs_haddr);
    
    *entry->status = STATUS_PENDING;

    fore200e->bus->write(*(u32*)&opcode, (u32*)&entry->cp_entry->cmd.oc3_block.opcode);

    ok = fore200e_poll(fore200e, entry->status, STATUS_COMPLETE, 400);

    *entry->status = STATUS_FREE;

    if (ok == 0) {
	printk(FORE200E "unable to set OC-3 reg 0x%02x of device %s\n", reg, fore200e->name);
	return -EIO;
    }

    return 0;
}


static int
fore200e_setloop(struct fore200e* fore200e, int loop_mode)
{
    u32 mct_value, mct_mask;
    int error;

    if (!capable(CAP_NET_ADMIN))
	return -EPERM;
    
    switch (loop_mode) {

    case ATM_LM_NONE:
	mct_value = 0; 
	mct_mask  = SUNI_MCT_DLE | SUNI_MCT_LLE;
	break;
	
    case ATM_LM_LOC_PHY:
	mct_value = mct_mask = SUNI_MCT_DLE;
	break;

    case ATM_LM_RMT_PHY:
	mct_value = mct_mask = SUNI_MCT_LLE;
	break;

    default:
	return -EINVAL;
    }

    error = fore200e_set_oc3(fore200e, SUNI_MCT, mct_value, mct_mask);
    if ( error == 0)
	fore200e->loop_mode = loop_mode;

    return error;
}


static inline unsigned int
fore200e_swap(unsigned int in)
{
#if defined(__LITTLE_ENDIAN)
    return swab32(in);
#else
    return in;
#endif
}


static int
fore200e_fetch_stats(struct fore200e* fore200e, struct sonet_stats* arg)
{
    struct sonet_stats tmp;

    if (fore200e_getstats(fore200e) < 0)
	return -EIO;

    tmp.section_bip = fore200e_swap(fore200e->stats->oc3.section_bip8_errors);
    tmp.line_bip    = fore200e_swap(fore200e->stats->oc3.line_bip24_errors);
    tmp.path_bip    = fore200e_swap(fore200e->stats->oc3.path_bip8_errors);
    tmp.line_febe   = fore200e_swap(fore200e->stats->oc3.line_febe_errors);
    tmp.path_febe   = fore200e_swap(fore200e->stats->oc3.path_febe_errors);
    tmp.corr_hcs    = fore200e_swap(fore200e->stats->oc3.corr_hcs_errors);
    tmp.uncorr_hcs  = fore200e_swap(fore200e->stats->oc3.ucorr_hcs_errors);
    tmp.tx_cells    = fore200e_swap(fore200e->stats->aal0.cells_transmitted)  +
	              fore200e_swap(fore200e->stats->aal34.cells_transmitted) +
	              fore200e_swap(fore200e->stats->aal5.cells_transmitted);
    tmp.rx_cells    = fore200e_swap(fore200e->stats->aal0.cells_received)     +
	              fore200e_swap(fore200e->stats->aal34.cells_received)    +
	              fore200e_swap(fore200e->stats->aal5.cells_received);

    if (arg)
	return copy_to_user(arg, &tmp, sizeof(struct sonet_stats)) ? -EFAULT : 0;	
    
    return 0;
}


static int
fore200e_ioctl(struct atm_dev* dev, unsigned int cmd, void* arg)
{
    struct fore200e* fore200e = FORE200E_DEV(dev);
    
    DPRINTK(2, "ioctl cmd = 0x%x (%u), arg = 0x%p (%lu)\n", cmd, cmd, arg, (unsigned long)arg);

    switch (cmd) {

    case SONET_GETSTAT:
	return fore200e_fetch_stats(fore200e, (struct sonet_stats*)arg);

    case SONET_GETDIAG:
	return put_user(0, (int*)arg) ? -EFAULT : 0;

    case ATM_SETLOOP:
	return fore200e_setloop(fore200e, (int)(unsigned long)arg);

    case ATM_GETLOOP:
	return put_user(fore200e->loop_mode, (int*)arg) ? -EFAULT : 0;

    case ATM_QUERYLOOP:
	return put_user(ATM_LM_LOC_PHY | ATM_LM_RMT_PHY, (int*)arg) ? -EFAULT : 0;
    }

    return -ENOSYS; /* not implemented */
}


static int
fore200e_change_qos(struct atm_vcc* vcc,struct atm_qos* qos, int flags)
{
    struct fore200e_vcc* fore200e_vcc = FORE200E_VCC(vcc);
    struct fore200e*     fore200e     = FORE200E_DEV(vcc->dev);

    DPRINTK(2, "change_qos %d.%d.%d, "
	    "(tx: cl=%s, pcr=%d-%d, cdv=%d, max_sdu=%d; "
	    "rx: cl=%s, pcr=%d-%d, cdv=%d, max_sdu=%d), flags = 0x%x\n"
	    "available_cell_rate = %u",
	    vcc->itf, vcc->vpi, vcc->vci,
	    fore200e_traffic_class[ qos->txtp.traffic_class ],
	    qos->txtp.min_pcr, qos->txtp.max_pcr, qos->txtp.max_cdv, qos->txtp.max_sdu,
	    fore200e_traffic_class[ qos->rxtp.traffic_class ],
	    qos->rxtp.min_pcr, qos->rxtp.max_pcr, qos->rxtp.max_cdv, qos->rxtp.max_sdu,
	    flags, fore200e->available_cell_rate);

    if ((qos->txtp.traffic_class == ATM_CBR) && (qos->txtp.max_pcr > 0)) {

	down(&fore200e->rate_sf);
	if (fore200e->available_cell_rate + vcc->qos.txtp.max_pcr < qos->txtp.max_pcr) {
	    up(&fore200e->rate_sf);
	    return -EAGAIN;
	}

	fore200e->available_cell_rate += vcc->qos.txtp.max_pcr;
	fore200e->available_cell_rate -= qos->txtp.max_pcr;
	up(&fore200e->rate_sf);
	
	memcpy(&vcc->qos, qos, sizeof(struct atm_qos));
	
	/* update rate control parameters */
	fore200e_rate_ctrl(qos, &fore200e_vcc->rate);

	set_bit(ATM_VF_HASQOS, &vcc->flags);
	return 0;
    }
    
    return -EINVAL;
}
    

static int __init
fore200e_irq_request(struct fore200e* fore200e)
{
    if (request_irq(fore200e->irq, fore200e_interrupt, SA_SHIRQ, fore200e->name, fore200e->atm_dev) < 0) {

	printk(FORE200E "unable to reserve IRQ %s for device %s\n",
	       fore200e_irq_itoa(fore200e->irq), fore200e->name);
	return -EBUSY;
    }

    printk(FORE200E "IRQ %s reserved for device %s\n",
	   fore200e_irq_itoa(fore200e->irq), fore200e->name);

    tasklet_init(&fore200e->tasklet, fore200e_tasklet, (unsigned long)fore200e);

    fore200e->state = FORE200E_STATE_IRQ;
    return 0;
}


static int __init
fore200e_get_esi(struct fore200e* fore200e)
{
    struct prom_data* prom = fore200e_kmalloc(sizeof(struct prom_data), GFP_KERNEL | GFP_DMA);
    int ok, i;

    if (!prom)
	return -ENOMEM;
    ok = fore200e->bus->prom_read(fore200e, prom);
    if (ok < 0) {
	fore200e_kfree(prom);
	return -EBUSY;
    }
	
    printk(FORE200E "device %s, rev. %c, S/N: %d, ESI: %02x:%02x:%02x:%02x:%02x:%02x\n", 
	   fore200e->name, 
	   (prom->hw_revision & 0xFF) + '@',    /* probably meaningless with SBA boards */
	   prom->serial_number & 0xFFFF,
	   prom->mac_addr[ 2 ], prom->mac_addr[ 3 ], prom->mac_addr[ 4 ],
	   prom->mac_addr[ 5 ], prom->mac_addr[ 6 ], prom->mac_addr[ 7 ]);
	
    for (i = 0; i < ESI_LEN; i++) {
	fore200e->esi[ i ] = fore200e->atm_dev->esi[ i ] = prom->mac_addr[ i + 2 ];
    }
    
    fore200e_kfree(prom);

    return 0;
}


static int __init
fore200e_alloc_rx_buf(struct fore200e* fore200e)
{
    int scheme, magn, nbr, size, i;

    struct host_bsq* bsq;
    struct buffer*   buffer;

    for (scheme = 0; scheme < BUFFER_SCHEME_NBR; scheme++) {
	for (magn = 0; magn < BUFFER_MAGN_NBR; magn++) {

	    bsq = &fore200e->host_bsq[ scheme ][ magn ];

	    nbr  = fore200e_rx_buf_nbr[ scheme ][ magn ];
	    size = fore200e_rx_buf_size[ scheme ][ magn ];

	    DPRINTK(2, "rx buffers %d / %d are being allocated\n", scheme, magn);

	    /* allocate the array of receive buffers */
	    buffer = bsq->buffer = fore200e_kmalloc(nbr * sizeof(struct buffer), GFP_KERNEL);

	    if (buffer == NULL)
		return -ENOMEM;

	    for (i = 0; i < nbr; i++) {

		buffer[ i ].scheme = scheme;
		buffer[ i ].magn   = magn;

		/* allocate the receive buffer body */
		if (fore200e_chunk_alloc(fore200e,
					 &buffer[ i ].data, size, fore200e->bus->buffer_alignment,
					 FORE200E_DMA_FROMDEVICE) < 0) {
		    
		    while (i > 0)
			fore200e_chunk_free(fore200e, &buffer[ --i ].data);
		    fore200e_kfree(buffer);
		    
		    return -ENOMEM;
		}
	    }
	    /* set next free buffer index */
	    bsq->free = 0;
	}
    }

    fore200e->state = FORE200E_STATE_ALLOC_BUF;
    return 0;
}


static int __init
fore200e_init_bs_queue(struct fore200e* fore200e)
{
    int scheme, magn, i;

    struct host_bsq*     bsq;
    struct cp_bsq_entry* cp_entry;

    for (scheme = 0; scheme < BUFFER_SCHEME_NBR; scheme++) {
	for (magn = 0; magn < BUFFER_MAGN_NBR; magn++) {

	    DPRINTK(2, "buffer supply queue %d / %d is being initialized\n", scheme, magn);

	    bsq = &fore200e->host_bsq[ scheme ][ magn ];

	    /* allocate and align the array of status words */
	    if (fore200e->bus->dma_chunk_alloc(fore200e,
					       &bsq->status,
					       sizeof(enum status), 
					       QUEUE_SIZE_BS,
					       fore200e->bus->status_alignment) < 0) {
		return -ENOMEM;
	    }

	    /* allocate and align the array of receive buffer descriptors */
	    if (fore200e->bus->dma_chunk_alloc(fore200e,
					       &bsq->rbd_block,
					       sizeof(struct rbd_block),
					       QUEUE_SIZE_BS,
					       fore200e->bus->descr_alignment) < 0) {
		
		fore200e->bus->dma_chunk_free(fore200e, &bsq->status);
		return -ENOMEM;
	    }
	    
	    /* get the base address of the cp resident buffer supply queue entries */
	    cp_entry = (struct cp_bsq_entry*)(fore200e->virt_base + 
		       fore200e->bus->read(&fore200e->cp_queues->cp_bsq[ scheme ][ magn ]));
	    
	    /* fill the host resident and cp resident buffer supply queue entries */
	    for (i = 0; i < QUEUE_SIZE_BS; i++) {
		
		bsq->host_entry[ i ].status = 
		                     FORE200E_INDEX(bsq->status.align_addr, enum status, i);
	        bsq->host_entry[ i ].rbd_block =
		                     FORE200E_INDEX(bsq->rbd_block.align_addr, struct rbd_block, i);
		bsq->host_entry[ i ].rbd_block_dma =
		                     FORE200E_DMA_INDEX(bsq->rbd_block.dma_addr, struct rbd_block, i);
		bsq->host_entry[ i ].cp_entry      = &cp_entry[ i ];
		
		*bsq->host_entry[ i ].status   = STATUS_FREE;
		
		fore200e->bus->write(FORE200E_DMA_INDEX(bsq->status.dma_addr, enum status, i), 
				     &cp_entry[ i ].status_haddr);
	    }
	}
    }

    fore200e->state = FORE200E_STATE_INIT_BSQ;
    return 0;
}


static int __init
fore200e_init_rx_queue(struct fore200e* fore200e)
{
    struct host_rxq*     rxq =  &fore200e->host_rxq;
    struct cp_rxq_entry* cp_entry;
    int i;

    DPRINTK(2, "receive queue is being initialized\n");

    /* allocate and align the array of status words */
    if (fore200e->bus->dma_chunk_alloc(fore200e,
				       &rxq->status,
				       sizeof(enum status), 
				       QUEUE_SIZE_RX,
				       fore200e->bus->status_alignment) < 0) {
	return -ENOMEM;
    }

    /* allocate and align the array of receive PDU descriptors */
    if (fore200e->bus->dma_chunk_alloc(fore200e,
				       &rxq->rpd,
				       sizeof(struct rpd), 
				       QUEUE_SIZE_RX,
				       fore200e->bus->descr_alignment) < 0) {
	
	fore200e->bus->dma_chunk_free(fore200e, &rxq->status);
	return -ENOMEM;
    }

    /* get the base address of the cp resident rx queue entries */
    cp_entry = (struct cp_rxq_entry*)(fore200e->virt_base + 
				      fore200e->bus->read(&fore200e->cp_queues->cp_rxq));

    /* fill the host resident and cp resident rx entries */
    for (i=0; i < QUEUE_SIZE_RX; i++) {
	
	rxq->host_entry[ i ].status = 
	                     FORE200E_INDEX(rxq->status.align_addr, enum status, i);
	rxq->host_entry[ i ].rpd = 
	                     FORE200E_INDEX(rxq->rpd.align_addr, struct rpd, i);
	rxq->host_entry[ i ].rpd_dma = 
	                     FORE200E_DMA_INDEX(rxq->rpd.dma_addr, struct rpd, i);
	rxq->host_entry[ i ].cp_entry = &cp_entry[ i ];

	*rxq->host_entry[ i ].status = STATUS_FREE;

	fore200e->bus->write(FORE200E_DMA_INDEX(rxq->status.dma_addr, enum status, i), 
			     &cp_entry[ i ].status_haddr);

	fore200e->bus->write(FORE200E_DMA_INDEX(rxq->rpd.dma_addr, struct rpd, i),
			     &cp_entry[ i ].rpd_haddr);
    }

    /* set the head entry of the queue */
    rxq->head = 0;

    fore200e->state = FORE200E_STATE_INIT_RXQ;
    return 0;
}


static int __init
fore200e_init_tx_queue(struct fore200e* fore200e)
{
    struct host_txq*     txq =  &fore200e->host_txq;
    struct cp_txq_entry* cp_entry;
    int i;

    DPRINTK(2, "transmit queue is being initialized\n");

    /* allocate and align the array of status words */
    if (fore200e->bus->dma_chunk_alloc(fore200e,
				       &txq->status,
				       sizeof(enum status), 
				       QUEUE_SIZE_TX,
				       fore200e->bus->status_alignment) < 0) {
	return -ENOMEM;
    }

    /* allocate and align the array of transmit PDU descriptors */
    if (fore200e->bus->dma_chunk_alloc(fore200e,
				       &txq->tpd,
				       sizeof(struct tpd), 
				       QUEUE_SIZE_TX,
				       fore200e->bus->descr_alignment) < 0) {
	
	fore200e->bus->dma_chunk_free(fore200e, &txq->status);
	return -ENOMEM;
    }

    /* get the base address of the cp resident tx queue entries */
    cp_entry = (struct cp_txq_entry*)(fore200e->virt_base + 
				      fore200e->bus->read(&fore200e->cp_queues->cp_txq));

    /* fill the host resident and cp resident tx entries */
    for (i=0; i < QUEUE_SIZE_TX; i++) {
	
	txq->host_entry[ i ].status = 
	                     FORE200E_INDEX(txq->status.align_addr, enum status, i);
	txq->host_entry[ i ].tpd = 
	                     FORE200E_INDEX(txq->tpd.align_addr, struct tpd, i);
	txq->host_entry[ i ].tpd_dma  = 
                             FORE200E_DMA_INDEX(txq->tpd.dma_addr, struct tpd, i);
	txq->host_entry[ i ].cp_entry = &cp_entry[ i ];

	*txq->host_entry[ i ].status = STATUS_FREE;
	
	fore200e->bus->write(FORE200E_DMA_INDEX(txq->status.dma_addr, enum status, i), 
			     &cp_entry[ i ].status_haddr);
	
        /* although there is a one-to-one mapping of tx queue entries and tpds,
	   we do not write here the DMA (physical) base address of each tpd into
	   the related cp resident entry, because the cp relies on this write
	   operation to detect that a new pdu has been submitted for tx */
}

    /* set the head entry of the queue */
    txq->head = 0;

    fore200e->state = FORE200E_STATE_INIT_TXQ;
    return 0;
}


static int __init
fore200e_init_cmd_queue(struct fore200e* fore200e)
{
    struct host_cmdq*     cmdq =  &fore200e->host_cmdq;
    struct cp_cmdq_entry* cp_entry;
    int i;

    DPRINTK(2, "command queue is being initialized\n");

    /* allocate and align the array of status words */
    if (fore200e->bus->dma_chunk_alloc(fore200e,
				       &cmdq->status,
				 sizeof(enum status), 
				 QUEUE_SIZE_CMD,
				 fore200e->bus->status_alignment) < 0) {
	return -ENOMEM;
    }
    
    /* get the base address of the cp resident cmd queue entries */
    cp_entry = (struct cp_cmdq_entry*)(fore200e->virt_base + 
				       fore200e->bus->read(&fore200e->cp_queues->cp_cmdq));

    /* fill the host resident and cp resident cmd entries */
    for (i=0; i < QUEUE_SIZE_CMD; i++) {
	
	cmdq->host_entry[ i ].status   = 
                              FORE200E_INDEX(cmdq->status.align_addr, enum status, i);
	cmdq->host_entry[ i ].cp_entry = &cp_entry[ i ];

	*cmdq->host_entry[ i ].status = STATUS_FREE;

	fore200e->bus->write(FORE200E_DMA_INDEX(cmdq->status.dma_addr, enum status, i), 
                             &cp_entry[ i ].status_haddr);
    }

    /* set the head entry of the queue */
    cmdq->head = 0;

    fore200e->state = FORE200E_STATE_INIT_CMDQ;
    return 0;
}


static void __init
fore200e_param_bs_queue(struct fore200e* fore200e,
			enum buffer_scheme scheme, enum buffer_magn magn,
			int queue_length, int pool_size, int supply_blksize)
{
    struct bs_spec* bs_spec = &fore200e->cp_queues->init.bs_spec[ scheme ][ magn ];

    /* dumb value; the firmware doesn't allow us to activate a VC while
       selecting a buffer scheme with zero-sized rbd pools */

    if (pool_size == 0)
	pool_size = 64;

    fore200e->bus->write(queue_length,                           &bs_spec->queue_length);
    fore200e->bus->write(fore200e_rx_buf_size[ scheme ][ magn ], &bs_spec->buffer_size);
    fore200e->bus->write(pool_size,                              &bs_spec->pool_size);
    fore200e->bus->write(supply_blksize,                         &bs_spec->supply_blksize);
}


static int __init
fore200e_initialize(struct fore200e* fore200e)
{
    struct cp_queues* cpq;
    int               ok, scheme, magn;

    DPRINTK(2, "device %s being initialized\n", fore200e->name);

    init_MUTEX(&fore200e->rate_sf);
    
    cpq = fore200e->cp_queues = (struct cp_queues*) (fore200e->virt_base + FORE200E_CP_QUEUES_OFFSET);

    /* enable cp to host interrupts */
    fore200e->bus->write(1, &cpq->imask);

    if (fore200e->bus->irq_enable)
	fore200e->bus->irq_enable(fore200e);
    
    fore200e->bus->write(NBR_CONNECT, &cpq->init.num_connect);

    fore200e->bus->write(QUEUE_SIZE_CMD, &cpq->init.cmd_queue_len);
    fore200e->bus->write(QUEUE_SIZE_RX,  &cpq->init.rx_queue_len);
    fore200e->bus->write(QUEUE_SIZE_TX,  &cpq->init.tx_queue_len);

    fore200e->bus->write(RSD_EXTENSION,  &cpq->init.rsd_extension);
    fore200e->bus->write(TSD_EXTENSION,  &cpq->init.tsd_extension);

    for (scheme = 0; scheme < BUFFER_SCHEME_NBR; scheme++)
	for (magn = 0; magn < BUFFER_MAGN_NBR; magn++)
	    fore200e_param_bs_queue(fore200e, scheme, magn,
				    QUEUE_SIZE_BS, 
				    fore200e_rx_buf_nbr[ scheme ][ magn ],
				    RBD_BLK_SIZE);

    /* issue the initialize command */
    fore200e->bus->write(STATUS_PENDING,    &cpq->init.status);
    fore200e->bus->write(OPCODE_INITIALIZE, &cpq->init.opcode);

    ok = fore200e_io_poll(fore200e, &cpq->init.status, STATUS_COMPLETE, 3000);
    if (ok == 0) {
	printk(FORE200E "device %s initialization failed\n", fore200e->name);
	return -ENODEV;
    }

    printk(FORE200E "device %s initialized\n", fore200e->name);

    fore200e->state = FORE200E_STATE_INITIALIZE;
    return 0;
}


static void __init
fore200e_monitor_putc(struct fore200e* fore200e, char c)
{
    struct cp_monitor* monitor = fore200e->cp_monitor;

#if 0
    printk("%c", c);
#endif
    fore200e->bus->write(((u32) c) | FORE200E_CP_MONITOR_UART_AVAIL, &monitor->soft_uart.send);
}


static int __init
fore200e_monitor_getc(struct fore200e* fore200e)
{
    struct cp_monitor* monitor = fore200e->cp_monitor;
    unsigned long      timeout = jiffies + MSECS(50);
    int                c;

    while (time_before(jiffies, timeout)) {

	c = (int) fore200e->bus->read(&monitor->soft_uart.recv);

	if (c & FORE200E_CP_MONITOR_UART_AVAIL) {

	    fore200e->bus->write(FORE200E_CP_MONITOR_UART_FREE, &monitor->soft_uart.recv);
#if 0
	    printk("%c", c & 0xFF);
#endif
	    return c & 0xFF;
	}
    }

    return -1;
}


static void __init
fore200e_monitor_puts(struct fore200e* fore200e, char* str)
{
    while(*str) {

	/* the i960 monitor doesn't accept any new character if it has something to say */
	while (fore200e_monitor_getc(fore200e) >= 0);
	
	fore200e_monitor_putc(fore200e, *str++);
    }

    while (fore200e_monitor_getc(fore200e) >= 0);
}


static int __init
fore200e_start_fw(struct fore200e* fore200e)
{
    int               ok;
    char              cmd[ 48 ];
    struct fw_header* fw_header = (struct fw_header*) fore200e->bus->fw_data;

    DPRINTK(2, "device %s firmware being started\n", fore200e->name);

    sprintf(cmd, "\rgo %x\r", le32_to_cpu(fw_header->start_offset));

    fore200e_monitor_puts(fore200e, cmd);

    ok = fore200e_io_poll(fore200e, &fore200e->cp_monitor->bstat, BSTAT_CP_RUNNING, 1000);
    if (ok == 0) {
	printk(FORE200E "device %s firmware didn't start\n", fore200e->name);
	return -ENODEV;
    }

    printk(FORE200E "device %s firmware started\n", fore200e->name);

    fore200e->state = FORE200E_STATE_START_FW;
    return 0;
}


static int __init
fore200e_load_fw(struct fore200e* fore200e)
{
    u32* fw_data = (u32*) fore200e->bus->fw_data;
    u32  fw_size = (u32) *fore200e->bus->fw_size / sizeof(u32);

    struct fw_header* fw_header = (struct fw_header*) fw_data;

    u32* load_addr = fore200e->virt_base + le32_to_cpu(fw_header->load_offset);

    DPRINTK(2, "device %s firmware being loaded at 0x%p (%d words)\n", 
	    fore200e->name, load_addr, fw_size);

#if 1
    if (le32_to_cpu(fw_header->magic) != FW_HEADER_MAGIC) {
	printk(FORE200E "corrupted %s firmware image\n", fore200e->bus->model_name);
	return -ENODEV;
    }
#endif

    for (; fw_size--; fw_data++, load_addr++)
	fore200e->bus->write(le32_to_cpu(*fw_data), load_addr);

    fore200e->state = FORE200E_STATE_LOAD_FW;
    return 0;
}


static int __init
fore200e_register(struct fore200e* fore200e)
{
    struct atm_dev* atm_dev;

    DPRINTK(2, "device %s being registered\n", fore200e->name);

    atm_dev = atm_dev_register(fore200e->bus->proc_name, &fore200e_ops, -1,
      NULL); 
    if (atm_dev == NULL) {
	printk(FORE200E "unable to register device %s\n", fore200e->name);
	return -ENODEV;
    }

    FORE200E_DEV(atm_dev) = fore200e;
    fore200e->atm_dev = atm_dev;

    atm_dev->ci_range.vpi_bits = 8;
    atm_dev->ci_range.vci_bits = 10;

    fore200e->available_cell_rate = ATM_OC3_PCR;

    fore200e->state = FORE200E_STATE_REGISTER;
    return 0;
}


static int __init
fore200e_init(struct fore200e* fore200e)
{
    if (fore200e_register(fore200e) < 0)
	return -ENODEV;
    
    if (fore200e->bus->configure(fore200e) < 0)
	return -ENODEV;

    if (fore200e->bus->map(fore200e) < 0)
	return -ENODEV;

    if (fore200e_reset(fore200e, 1) < 0)
	return -ENODEV;

    if (fore200e_load_fw(fore200e) < 0)
	return -ENODEV;

    if (fore200e_start_fw(fore200e) < 0)
	return -ENODEV;

    if (fore200e_initialize(fore200e) < 0)
	return -ENODEV;

    if (fore200e_init_cmd_queue(fore200e) < 0)
	return -ENOMEM;

    if (fore200e_init_tx_queue(fore200e) < 0)
	return -ENOMEM;

    if (fore200e_init_rx_queue(fore200e) < 0)
	return -ENOMEM;

    if (fore200e_init_bs_queue(fore200e) < 0)
	return -ENOMEM;

    if (fore200e_alloc_rx_buf(fore200e) < 0)
	return -ENOMEM;

    if (fore200e_get_esi(fore200e) < 0)
	return -EIO;

    if (fore200e_irq_request(fore200e) < 0)
	return -EBUSY;

    fore200e_supply(fore200e);
    
    /* all done, board initialization is now complete */
    fore200e->state = FORE200E_STATE_COMPLETE;
    return 0;
}


int __init
fore200e_detect(void)
{
    const struct fore200e_bus* bus;
    struct       fore200e*     fore200e;
    int                        index, link;

    printk(FORE200E "FORE Systems 200E-series driver - version " FORE200E_VERSION "\n");

    /* for each configured bus interface */
    for (link = 0, bus = fore200e_bus; bus->model_name; bus++) {

	/* detect all boards present on that bus */
	for (index = 0; (fore200e = bus->detect(bus, index)); index++) {
	    
	    printk(FORE200E "device %s found at 0x%lx, IRQ %s\n",
		   fore200e->bus->model_name, 
		   fore200e->phys_base, fore200e_irq_itoa(fore200e->irq));

	    sprintf(fore200e->name, "%s-%d", bus->model_name, index);

	    if (fore200e_init(fore200e) < 0) {

		fore200e_shutdown(fore200e);
		break;
	    }

	    link++;

	    fore200e->next  = fore200e_boards;
	    fore200e_boards = fore200e;
	}
    }

    return link;
}


#ifdef MODULE
static void
fore200e_cleanup(struct fore200e** head)
{
    struct fore200e* fore200e = *head;

    fore200e_shutdown(fore200e);

    *head = fore200e->next;

    kfree(fore200e);
}
#endif


static int
fore200e_proc_read(struct atm_dev *dev,loff_t* pos,char* page)
{
    struct sock *s;
    struct fore200e* fore200e  = FORE200E_DEV(dev);
    int              len, left = *pos;

    if (!left--) {

	if (fore200e_getstats(fore200e) < 0)
	    return -EIO;

	len = sprintf(page,"\n"
		       " device:\n"
		       "   internal name:\t\t%s\n", fore200e->name);

	/* print bus-specific information */
	if (fore200e->bus->proc_read)
	    len += fore200e->bus->proc_read(fore200e, page + len);
	
	len += sprintf(page + len,
		"   interrupt line:\t\t%s\n"
		"   physical base address:\t0x%p\n"
		"   virtual base address:\t0x%p\n"
		"   factory address (ESI):\t%02x:%02x:%02x:%02x:%02x:%02x\n"
		"   board serial number:\t\t%d\n\n",
		fore200e_irq_itoa(fore200e->irq),
		(void*)fore200e->phys_base,
		(void*)fore200e->virt_base,
		fore200e->esi[0], fore200e->esi[1], fore200e->esi[2],
		fore200e->esi[3], fore200e->esi[4], fore200e->esi[5],
		fore200e->esi[4] * 256 + fore200e->esi[5]);

	return len;
    }

    if (!left--)
	return sprintf(page,
		       "   supplied small bufs (1):\t%d\n"
		       "   supplied large bufs (1):\t%d\n"
		       "   supplied small bufs (2):\t%d\n"
		       "   supplied large bufs (2):\t%d\n",
		       fore200e->host_bsq[ BUFFER_SCHEME_ONE ][ BUFFER_MAGN_SMALL ].count,
		       fore200e->host_bsq[ BUFFER_SCHEME_ONE ][ BUFFER_MAGN_LARGE ].count,
		       fore200e->host_bsq[ BUFFER_SCHEME_TWO ][ BUFFER_MAGN_SMALL ].count,
		       fore200e->host_bsq[ BUFFER_SCHEME_TWO ][ BUFFER_MAGN_LARGE ].count);
    if (!left--) {
	u32 hb = fore200e->bus->read(&fore200e->cp_queues->heartbeat);

	len = sprintf(page,"\n\n"
		      " cell processor:\n"
		      "   heartbeat state:\t\t");
	
	if (hb >> 16 != 0xDEAD)
	    len += sprintf(page + len, "0x%08x\n", hb);
	else
	    len += sprintf(page + len, "*** FATAL ERROR %04x ***\n", hb & 0xFFFF);

	return len;
    }

    if (!left--) {
	static const char* media_name[] = {
	    "unshielded twisted pair",
	    "multimode optical fiber ST",
	    "multimode optical fiber SC",
	    "single-mode optical fiber ST",
	    "single-mode optical fiber SC",
	    "unknown"
	};

	static const char* oc3_mode[] = {
	    "normal operation",
	    "diagnostic loopback",
	    "line loopback",
	    "unknown"
	};

	u32 fw_release     = fore200e->bus->read(&fore200e->cp_queues->fw_release);
	u32 mon960_release = fore200e->bus->read(&fore200e->cp_queues->mon960_release);
	u32 oc3_revision   = fore200e->bus->read(&fore200e->cp_queues->oc3_revision);
	u32 media_index    = FORE200E_MEDIA_INDEX(fore200e->bus->read(&fore200e->cp_queues->media_type));
	u32 oc3_index;

	if (media_index < 0 || media_index > 4)
	    media_index = 5;
	
	switch (fore200e->loop_mode) {
	    case ATM_LM_NONE:    oc3_index = 0;
		                 break;
	    case ATM_LM_LOC_PHY: oc3_index = 1;
		                 break;
	    case ATM_LM_RMT_PHY: oc3_index = 2;
		                 break;
	    default:             oc3_index = 3;
	}

	return sprintf(page,
		       "   firmware release:\t\t%d.%d.%d\n"
		       "   monitor release:\t\t%d.%d\n"
		       "   media type:\t\t\t%s\n"
		       "   OC-3 revision:\t\t0x%x\n"
                       "   OC-3 mode:\t\t\t%s",
		       fw_release >> 16, fw_release << 16 >> 24,  fw_release << 24 >> 24,
		       mon960_release >> 16, mon960_release << 16 >> 16,
		       media_name[ media_index ],
		       oc3_revision,
		       oc3_mode[ oc3_index ]);
    }

    if (!left--) {
	struct cp_monitor* cp_monitor = fore200e->cp_monitor;

	return sprintf(page,
		       "\n\n"
		       " monitor:\n"
		       "   version number:\t\t%d\n"
		       "   boot status word:\t\t0x%08x\n",
		       fore200e->bus->read(&cp_monitor->mon_version),
		       fore200e->bus->read(&cp_monitor->bstat));
    }

    if (!left--)
	return sprintf(page,
		       "\n"
		       " device statistics:\n"
		       "  4b5b:\n"
		       "     crc_header_errors:\t\t%10u\n"
		       "     framing_errors:\t\t%10u\n",
		       fore200e_swap(fore200e->stats->phy.crc_header_errors),
		       fore200e_swap(fore200e->stats->phy.framing_errors));
    
    if (!left--)
	return sprintf(page, "\n"
		       "  OC-3:\n"
		       "     section_bip8_errors:\t%10u\n"
		       "     path_bip8_errors:\t\t%10u\n"
		       "     line_bip24_errors:\t\t%10u\n"
		       "     line_febe_errors:\t\t%10u\n"
		       "     path_febe_errors:\t\t%10u\n"
		       "     corr_hcs_errors:\t\t%10u\n"
		       "     ucorr_hcs_errors:\t\t%10u\n",
		       fore200e_swap(fore200e->stats->oc3.section_bip8_errors),
		       fore200e_swap(fore200e->stats->oc3.path_bip8_errors),
		       fore200e_swap(fore200e->stats->oc3.line_bip24_errors),
		       fore200e_swap(fore200e->stats->oc3.line_febe_errors),
		       fore200e_swap(fore200e->stats->oc3.path_febe_errors),
		       fore200e_swap(fore200e->stats->oc3.corr_hcs_errors),
		       fore200e_swap(fore200e->stats->oc3.ucorr_hcs_errors));

    if (!left--)
	return sprintf(page,"\n"
		       "   ATM:\t\t\t\t     cells\n"
		       "     TX:\t\t\t%10u\n"
		       "     RX:\t\t\t%10u\n"
		       "     vpi out of range:\t\t%10u\n"
		       "     vpi no conn:\t\t%10u\n"
		       "     vci out of range:\t\t%10u\n"
		       "     vci no conn:\t\t%10u\n",
		       fore200e_swap(fore200e->stats->atm.cells_transmitted),
		       fore200e_swap(fore200e->stats->atm.cells_received),
		       fore200e_swap(fore200e->stats->atm.vpi_bad_range),
		       fore200e_swap(fore200e->stats->atm.vpi_no_conn),
		       fore200e_swap(fore200e->stats->atm.vci_bad_range),
		       fore200e_swap(fore200e->stats->atm.vci_no_conn));
    
    if (!left--)
	return sprintf(page,"\n"
		       "   AAL0:\t\t\t     cells\n"
		       "     TX:\t\t\t%10u\n"
		       "     RX:\t\t\t%10u\n"
		       "     dropped:\t\t\t%10u\n",
		       fore200e_swap(fore200e->stats->aal0.cells_transmitted),
		       fore200e_swap(fore200e->stats->aal0.cells_received),
		       fore200e_swap(fore200e->stats->aal0.cells_dropped));
    
    if (!left--)
	return sprintf(page,"\n"
		       "   AAL3/4:\n"
		       "     SAR sublayer:\t\t     cells\n"
		       "       TX:\t\t\t%10u\n"
		       "       RX:\t\t\t%10u\n"
		       "       dropped:\t\t\t%10u\n"
		       "       CRC errors:\t\t%10u\n"
		       "       protocol errors:\t\t%10u\n\n"
		       "     CS  sublayer:\t\t      PDUs\n"
		       "       TX:\t\t\t%10u\n"
		       "       RX:\t\t\t%10u\n"
		       "       dropped:\t\t\t%10u\n"
		       "       protocol errors:\t\t%10u\n",
		       fore200e_swap(fore200e->stats->aal34.cells_transmitted),
		       fore200e_swap(fore200e->stats->aal34.cells_received),
		       fore200e_swap(fore200e->stats->aal34.cells_dropped),
		       fore200e_swap(fore200e->stats->aal34.cells_crc_errors),
		       fore200e_swap(fore200e->stats->aal34.cells_protocol_errors),
		       fore200e_swap(fore200e->stats->aal34.cspdus_transmitted),
		       fore200e_swap(fore200e->stats->aal34.cspdus_received),
		       fore200e_swap(fore200e->stats->aal34.cspdus_dropped),
		       fore200e_swap(fore200e->stats->aal34.cspdus_protocol_errors));
    
    if (!left--)
	return sprintf(page,"\n"
		       "   AAL5:\n"
		       "     SAR sublayer:\t\t     cells\n"
		       "       TX:\t\t\t%10u\n"
		       "       RX:\t\t\t%10u\n"
		       "       dropped:\t\t\t%10u\n"
		       "       congestions:\t\t%10u\n\n"
		       "     CS  sublayer:\t\t      PDUs\n"
		       "       TX:\t\t\t%10u\n"
		       "       RX:\t\t\t%10u\n"
		       "       dropped:\t\t\t%10u\n"
		       "       CRC errors:\t\t%10u\n"
		       "       protocol errors:\t\t%10u\n",
		       fore200e_swap(fore200e->stats->aal5.cells_transmitted),
		       fore200e_swap(fore200e->stats->aal5.cells_received),
		       fore200e_swap(fore200e->stats->aal5.cells_dropped),
		       fore200e_swap(fore200e->stats->aal5.congestion_experienced),
		       fore200e_swap(fore200e->stats->aal5.cspdus_transmitted),
		       fore200e_swap(fore200e->stats->aal5.cspdus_received),
		       fore200e_swap(fore200e->stats->aal5.cspdus_dropped),
		       fore200e_swap(fore200e->stats->aal5.cspdus_crc_errors),
		       fore200e_swap(fore200e->stats->aal5.cspdus_protocol_errors));
    
    if (!left--)
	return sprintf(page,"\n"
		       "   AUX:\t\t       allocation failures\n"
		       "     small b1:\t\t\t%10u\n"
		       "     large b1:\t\t\t%10u\n"
		       "     small b2:\t\t\t%10u\n"
		       "     large b2:\t\t\t%10u\n"
		       "     RX PDUs:\t\t\t%10u\n",
		       fore200e_swap(fore200e->stats->aux.small_b1_failed),
		       fore200e_swap(fore200e->stats->aux.large_b1_failed),
		       fore200e_swap(fore200e->stats->aux.small_b2_failed),
		       fore200e_swap(fore200e->stats->aux.large_b2_failed),
		       fore200e_swap(fore200e->stats->aux.rpd_alloc_failed));
    
    if (!left--)
	return sprintf(page,"\n"
		       " receive carrier:\t\t\t%s\n",
		       fore200e->stats->aux.receive_carrier ? "ON" : "OFF!");
    
    if (!left--) {
	struct atm_vcc *vcc;
	struct fore200e_vcc* fore200e_vcc;
	
	len = sprintf(page,"\n"    
		      " VCCs:\n  address\tVPI.VCI:AAL\t(min/max tx PDU size) (min/max rx PDU size)\n");
	
	read_lock(&vcc_sklist_lock);
	for (s = vcc_sklist; s; s = s->next) {
	    vcc = s->protinfo.af_atm;

	    if (vcc->dev != fore200e->atm_dev)
		    continue;

	    fore200e_vcc = FORE200E_VCC(vcc);
	    
	    len += sprintf(page + len,
			   "  %x\t%d.%d:%d\t\t(%d/%d)\t(%d/%d)\n",
			   (u32)(unsigned long)vcc,
			   vcc->vpi, vcc->vci, fore200e_atm2fore_aal(vcc->qos.aal),
			   fore200e_vcc->tx_min_pdu > 0xFFFF ? 0 : fore200e_vcc->tx_min_pdu,
			   fore200e_vcc->tx_max_pdu,
			   fore200e_vcc->rx_min_pdu > 0xFFFF ? 0 : fore200e_vcc->rx_min_pdu,
			   fore200e_vcc->rx_max_pdu
		);
	}
	read_unlock(&vcc_sklist_lock);

	return len;
    }
    
    return 0;
}


#ifdef MODULE
static int __init
fore200e_module_init(void)
{
    DPRINTK(1, "module loaded\n");
    return fore200e_detect() == 0;
}

static void __exit
fore200e_module_cleanup(void)
{
    while (fore200e_boards) {
	fore200e_cleanup(&fore200e_boards);
    }
    DPRINTK(1, "module being removed\n");
}

module_init(fore200e_module_init);
module_exit(fore200e_module_cleanup);
#endif


static const struct atmdev_ops fore200e_ops =
{
	open:         fore200e_open,
	close:        fore200e_close,
	ioctl:        fore200e_ioctl,
	getsockopt:   fore200e_getsockopt,
	setsockopt:   fore200e_setsockopt,
	send:         fore200e_send,
	change_qos:   fore200e_change_qos,
	proc_read:    fore200e_proc_read,
	owner:        THIS_MODULE,
};


#ifdef CONFIG_ATM_FORE200E_PCA
extern const unsigned char _fore200e_pca_fw_data[];
extern const unsigned int  _fore200e_pca_fw_size;
#endif
#ifdef CONFIG_ATM_FORE200E_SBA
extern const unsigned char _fore200e_sba_fw_data[];
extern const unsigned int  _fore200e_sba_fw_size;
#endif

static const struct fore200e_bus fore200e_bus[] = {
#ifdef CONFIG_ATM_FORE200E_PCA
    { "PCA-200E", "pca200e", 32, 4, 32, 
      _fore200e_pca_fw_data, &_fore200e_pca_fw_size,
      fore200e_pca_read,
      fore200e_pca_write,
      fore200e_pca_dma_map,
      fore200e_pca_dma_unmap,
      fore200e_pca_dma_sync,
      fore200e_pca_dma_chunk_alloc,
      fore200e_pca_dma_chunk_free,
      fore200e_pca_detect,
      fore200e_pca_configure,
      fore200e_pca_map,
      fore200e_pca_reset,
      fore200e_pca_prom_read,
      fore200e_pca_unmap,
      NULL,
      fore200e_pca_irq_check,
      fore200e_pca_irq_ack,
      fore200e_pca_proc_read,
    },
#endif
#ifdef CONFIG_ATM_FORE200E_SBA
    { "SBA-200E", "sba200e", 32, 64, 32,
      _fore200e_sba_fw_data, &_fore200e_sba_fw_size,
      fore200e_sba_read,
      fore200e_sba_write,
      fore200e_sba_dma_map,
      fore200e_sba_dma_unmap,
      fore200e_sba_dma_sync,
      fore200e_sba_dma_chunk_alloc,
      fore200e_sba_dma_chunk_free,
      fore200e_sba_detect, 
      fore200e_sba_configure,
      fore200e_sba_map,
      fore200e_sba_reset,
      fore200e_sba_prom_read,
      fore200e_sba_unmap,
      fore200e_sba_irq_enable,
      fore200e_sba_irq_check,
      fore200e_sba_irq_ack,
      fore200e_sba_proc_read,
    },
#endif
    {}
};

MODULE_LICENSE("GPL");
