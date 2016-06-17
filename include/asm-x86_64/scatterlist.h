#ifndef _X8664_SCATTERLIST_H
#define _X8664_SCATTERLIST_H

struct scatterlist {
    char *  address;    /* Location data is to be transferred to, NULL for
			 * highmem page */
    struct page * page; /* Location for highmem page, if any */
    unsigned int offset;/* for highmem, page offset */

    unsigned int length;
    dma_addr_t dma_address;
    unsigned int dma_length;
};

#define ISA_DMA_THRESHOLD (0x00ffffff)

#endif 
