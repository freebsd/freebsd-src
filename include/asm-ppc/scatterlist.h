#ifdef __KERNEL__
#ifndef _PPC_SCATTERLIST_H
#define _PPC_SCATTERLIST_H

#include <asm/dma.h>

struct scatterlist {
	char *address;		/* Location data is to be transferred to,
				 * or NULL for highmem page */
	struct page * page;	/* Location for highmem page, if any */
	unsigned int offset;	/* for highmem, page offset */
	unsigned int length;	/* length at address or page+offset */

	dma_addr_t dma_address;	/* phys/bus dma address	*/
	unsigned int dma_length; /* number of bytes mapped at dma_address */
	/* Note that if we coalesce multiple scatterlist elements,
	   dma_length can be different from length */
};

/*
 * These macros should be used after a pci_map_sg call has been done
 * to get bus addresses of each of the SG entries and their lengths.
 * You should only work with the number of sg entries pci_map_sg
 * returns, or alternatively stop on the first sg_dma_len(sg) which
 * is 0.
 */
#define sg_dma_address(sg)      ((sg)->dma_address)
#define sg_dma_len(sg)          ((sg)->dma_length)

#endif /* !(_PPC_SCATTERLIST_H) */
#endif /* __KERNEL__ */
