/*
 * Page Cache for Virtual FileSystem
 * uOS(m) - User OS Mobile
 */

#ifndef _PAGE_CACHE_H_
#define _PAGE_CACHE_H_

#include <stdint.h>
#include <stddef.h>
#include "vfs.h"

#define PAGE_CACHE_SIZE 128
#define PAGE_SIZE 4096

/* Page cache entry */
typedef struct page_cache_entry {
    uint32_t ino;           /* Inode number */
    uint32_t page_offset;   /* Page offset within file */
    uint8_t data[PAGE_SIZE];
    int dirty;
    int valid;
    int ref_count;
    struct page_cache_entry *next;
    struct page_cache_entry *prev;
} page_cache_entry_t;

/* Initialize page cache */
int page_cache_init(void);

/* Get a page from cache */
page_cache_entry_t *page_cache_get(uint32_t ino, uint32_t page_offset);

/* Put a page back */
void page_cache_put(page_cache_entry_t *page);

/* Mark page dirty */
void page_cache_dirty(page_cache_entry_t *page);

/* Flush all dirty pages for an inode */
int page_cache_flush_inode(uint32_t ino, int (*writeback)(uint32_t ino, uint32_t page_offset, uint8_t *data, uint32_t len));

/* Flush all dirty pages */
int page_cache_flush_all(void);

#endif /* _PAGE_CACHE_H_ */