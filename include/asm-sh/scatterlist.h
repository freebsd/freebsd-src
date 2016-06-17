#ifndef __ASM_SH_SCATTERLIST_H
#define __ASM_SH_SCATTERLIST_H

struct scatterlist {
    char *  address;    /* Location data is to be transferred to, NULL for
			 * highmem page */
    struct page * page; /* Location for highmem page, if any */
    unsigned int offset;/* for highmem, page offset */

    dma_addr_t dma_address;
    unsigned int length;
};

#define ISA_DMA_THRESHOLD (0x1fffffff)

#endif /* !(__ASM_SH_SCATTERLIST_H) */
