/*
 * support.h - Header file for specific support.c
 *
 * Copyright (C) 1997 Régis Duchesne
 * Copyright (c) 2001 Anton Altaparmakov (AIA)
 */

/* Debug levels */
#define DEBUG_OTHER	1
#define DEBUG_MALLOC	2
#define DEBUG_BSD       4
#define DEBUG_LINUX     8
#define DEBUG_DIR1     16
#define DEBUG_DIR2     32
#define DEBUG_DIR3     64
#define DEBUG_FILE1   128
#define DEBUG_FILE2   256
#define DEBUG_FILE3   512
#define DEBUG_NAME1  1024
#define DEBUG_NAME2  2048

#ifdef DEBUG
void ntfs_debug(int mask, const char *fmt, ...);
#else
#define ntfs_debug(mask, fmt...)	do {} while (0)
#endif

#include <linux/slab.h>
#include <linux/vmalloc.h>

#define ntfs_malloc(size)  kmalloc(size, GFP_KERNEL)

#define ntfs_free(ptr)     kfree(ptr)

/**
 * ntfs_vmalloc - allocate memory in multiples of pages
 * @size	number of bytes to allocate
 *
 * Allocates @size bytes of memory, rounded up to multiples of PAGE_SIZE and
 * returns a pointer to the allocated memory.
 *
 * If there was insufficient memory to complete the request, return NULL.
 */
static inline void *ntfs_vmalloc(unsigned long size)
{
	if (size <= PAGE_SIZE) {
		if (size) {
			/* kmalloc() has per-CPU caches so if faster for now. */
			return kmalloc(PAGE_SIZE, GFP_NOFS);
			/* return (void *)__get_free_page(GFP_NOFS |
					__GFP_HIGHMEM); */
		}
		BUG();
	}
	if (size >> PAGE_SHIFT < num_physpages)
		return __vmalloc(size, GFP_NOFS | __GFP_HIGHMEM, PAGE_KERNEL);
	return NULL;
}

static inline void ntfs_vfree(void *addr)
{
	if ((unsigned long)addr < VMALLOC_START) {
		return kfree(addr);
		/* return free_page((unsigned long)addr); */
	}
	vfree(addr);
}

void ntfs_bzero(void *s, int n);

void ntfs_memcpy(void *dest, const void *src, ntfs_size_t n);

void ntfs_memmove(void *dest, const void *src, ntfs_size_t n);

void ntfs_error(const char *fmt,...);

int ntfs_read_mft_record(ntfs_volume *vol, int mftno, char *buf);

int ntfs_getput_clusters(ntfs_volume *pvol, int cluster, ntfs_size_t offs,
			 ntfs_io *buf);

ntfs_time64_t ntfs_now(void);

int ntfs_dupuni2map(ntfs_volume *vol, ntfs_u16 *in, int in_len, char **out,
		    int *out_len);

int ntfs_dupmap2uni(ntfs_volume *vol, char* in, int in_len, ntfs_u16 **out,
		    int *out_len);

