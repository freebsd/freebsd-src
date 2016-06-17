#include <linux/mm.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/string.h>

/* 
 * Dummy IO MMU functions
 */

extern unsigned long end_pfn;

void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
			   dma_addr_t *dma_handle)
{
	void *ret;
	int gfp = GFP_ATOMIC;
	
	if (hwdev == NULL ||
	    end_pfn > (hwdev->dma_mask>>PAGE_SHIFT) ||  /* XXX */
	    (u32)hwdev->dma_mask < 0xffffffff)
		gfp |= GFP_DMA;
	ret = (void *)__get_free_pages(gfp, get_order(size));

	if (ret != NULL) {
		memset(ret, 0, size);
		*dma_handle = virt_to_bus(ret);
	}
	return ret;
}

void pci_free_consistent(struct pci_dev *hwdev, size_t size,
			 void *vaddr, dma_addr_t dma_handle)
{
	free_pages((unsigned long)vaddr, get_order(size));
}


static void __init check_ram(void) 
{ 
	if (end_pfn >= 0xffffffff>>PAGE_SHIFT) { 
		printk(KERN_ERR "WARNING more than 4GB of memory but no IOMMU.\n"
		       KERN_ERR "WARNING 32bit PCI may malfunction.\n"); 
		/* Could play with highmem_start_page here to trick some subsystems
		   into bounce buffers. Unfortunately that would require setting
		   CONFIG_HIGHMEM too. 
		 */ 
	} 
} 
