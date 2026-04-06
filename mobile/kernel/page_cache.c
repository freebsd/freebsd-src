/*
 * Page Cache Implementation
 * uOS(m) - User OS Mobile
 */

#include "page_cache.h"
#include "memory.h"
#include "blockdev.h"

static page_cache_entry_t page_cache[PAGE_CACHE_SIZE];
static page_cache_entry_t *lru_head = NULL;
static page_cache_entry_t *lru_tail = NULL;

extern void uart_puts(const char *s);

/* Simple hash for inode+offset */
static uint32_t hash_page(uint32_t ino, uint32_t offset) {
    return (ino * 31 + offset) % PAGE_CACHE_SIZE;
}

/* Remove from LRU list */
static void lru_remove(page_cache_entry_t *page) {
    if (page->prev) page->prev->next = page->next;
    if (page->next) page->next->prev = page->prev;
    if (page == lru_head) lru_head = page->next;
    if (page == lru_tail) lru_tail = page->prev;
    page->next = page->prev = NULL;
}

/* Add to LRU head */
static void lru_add_head(page_cache_entry_t *page) {
    page->next = lru_head;
    page->prev = NULL;
    if (lru_head) lru_head->prev = page;
    lru_head = page;
    if (!lru_tail) lru_tail = page;
}

/* Initialize page cache */
int page_cache_init(void) {
    uart_puts("Page cache initializing...\n");
    
    for (int i = 0; i < PAGE_CACHE_SIZE; i++) {
        page_cache[i].ino = 0;
        page_cache[i].page_offset = 0;
        page_cache[i].dirty = 0;
        page_cache[i].valid = 0;
        page_cache[i].ref_count = 0;
        page_cache[i].next = NULL;
        page_cache[i].prev = NULL;
    }
    
    uart_puts("Page cache ready\n");
    return 0;
}

/* Find page in cache */
static page_cache_entry_t *page_find(uint32_t ino, uint32_t offset) {
    uint32_t hash = hash_page(ino, offset);
    page_cache_entry_t *page = &page_cache[hash];
    
    if (page->valid && page->ino == ino && page->page_offset == offset) {
        return page;
    }
    
    return NULL;
}

/* Evict a page */
static page_cache_entry_t *page_evict(void) {
    if (!lru_tail) return NULL;
    
    page_cache_entry_t *page = lru_tail;
    lru_remove(page);
    
    if (page->dirty) {
        /* Write back - this would need filesystem integration */
        /* For now, just mark invalid */
    }
    
    page->valid = 0;
    page->ref_count = 0;
    return page;
}

/* Get a page from cache */
page_cache_entry_t *page_cache_get(uint32_t ino, uint32_t page_offset) {
    page_cache_entry_t *page = page_find(ino, page_offset);
    if (page) {
        page->ref_count++;
        lru_remove(page);
        lru_add_head(page);
        return page;
    }
    
    /* Not in cache, find free slot */
    for (int i = 0; i < PAGE_CACHE_SIZE; i++) {
        if (page_cache[i].ref_count == 0) {
            page = &page_cache[i];
            break;
        }
    }
    
    if (!page) {
        /* Evict one */
        page = page_evict();
    }
    
    if (page) {
        /* Load from disk - placeholder */
        /* In real implementation, read from filesystem */
        page->ino = ino;
        page->page_offset = page_offset;
        page->valid = 1;
        page->dirty = 0;
        page->ref_count = 1;
        lru_add_head(page);
    }
    
    return page;
}

/* Put a page back */
void page_cache_put(page_cache_entry_t *page) {
    if (page && page->ref_count > 0) {
        page->ref_count--;
    }
}

/* Mark page dirty */
void page_cache_dirty(page_cache_entry_t *page) {
    if (page) {
        page->dirty = 1;
    }
}

/* Flush all dirty pages for an inode */
int page_cache_flush_inode(uint32_t ino, int (*writeback)(uint32_t ino, uint32_t page_offset, uint8_t *data, uint32_t len)) {
    for (int i = 0; i < PAGE_CACHE_SIZE; i++) {
        if (page_cache[i].valid && page_cache[i].ino == ino && page_cache[i].dirty) {
            /* Write back to disk using callback */
            if (writeback) {
                writeback(ino, page_cache[i].page_offset, page_cache[i].data, PAGE_SIZE);
            }
            page_cache[i].dirty = 0;
        }
    }
    return 0;
}

/* Flush all dirty pages */
int page_cache_flush_all(void) {
    for (int i = 0; i < PAGE_CACHE_SIZE; i++) {
        if (page_cache[i].valid && page_cache[i].dirty) {
            /* Write back to disk */
            /* Placeholder */
            page_cache[i].dirty = 0;
        }
    }
    return 0;
}