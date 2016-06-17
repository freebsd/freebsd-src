#ifndef _ASM_PARISC_SCATTERLIST_H
#define _ASM_PARISC_SCATTERLIST_H

#include <asm/page.h>

struct scatterlist {
	/* This will disappear in 2.5.x */
	char *address;

	/* page/offset only valid if ADDRESS member is NULL.
	** Needed to support CONFIG_HIGHMEM on x386.
	** I still think davem is a dork for forcing other
	** arches to add this to 2.4.x. -ggg
	*/
	struct page *page;
	unsigned int offset;

	unsigned int length;

	/* an IOVA can be 64-bits on some PA-Risc platforms. */
	dma_addr_t iova;	/* I/O Virtual Address */
	__u32      iova_length; /* bytes mapped */
};

#define sg_virt_addr(sg) (((sg)->address) ? ((sg)->address) : \
		(page_address((sg)->page) + (sg)->offset))

#define sg_dma_address(sg) ((sg)->iova)
#define sg_dma_len(sg)     ((sg)->iova_length)

#define ISA_DMA_THRESHOLD (~0UL)

#endif /* _ASM_PARISC_SCATTERLIST_H */
