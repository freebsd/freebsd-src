#ifndef _ASMS390X_SCATTERLIST_H
#define _ASMS390X_SCATTERLIST_H

struct scatterlist {
    /* This will disappear in 2.5.x */
    char *address;

    /* These two are only valid if ADDRESS member of this
     * struct is NULL.
     */
    struct page *page;
    unsigned int offset;

    unsigned int length;
};

#define ISA_DMA_THRESHOLD (0xffffffffffffffff)

#endif /* _ASMS390X_SCATTERLIST_H */
