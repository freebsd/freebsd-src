#ifndef _M68K_SCATTERLIST_H
#define _M68K_SCATTERLIST_H

struct scatterlist {
	/* This will disappear in 2.5.x */
	char *address;

	/* These two are only valid if ADDRESS member of this
	 * struct is NULL.
	 */
	struct page *page;
	unsigned int offset;

	unsigned int length;

	__u32 dvma_address; /* A place to hang host-specific addresses at. */
};

struct mmu_sglist {
        char *addr;
        char *__dont_touch;
        unsigned int len;
        unsigned long dvma_addr;
};

/* This is bogus and should go away. */
#define ISA_DMA_THRESHOLD (0x00ffffff)

#endif /* !(_M68K_SCATTERLIST_H) */
