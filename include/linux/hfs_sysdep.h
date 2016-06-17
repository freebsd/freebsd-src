/*
 * linux/include/linux/hfs_sysdep.h
 *
 * Copyright (C) 1996-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains constants, types and inline
 * functions for various system dependent things.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 */

#ifndef _HFS_SYSDEP_H
#define _HFS_SYSDEP_H

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/locks.h>
#include <linux/fs.h>

#include <asm/byteorder.h>
#include <asm/unaligned.h>

extern struct timezone sys_tz;

#undef offsetof
#define offsetof(TYPE, MEMB) ((size_t) &((TYPE *)0)->MEMB)

/* Typedefs for integer types by size and signedness */
typedef __u8            hfs_u8;
typedef __u16           hfs_u16;
typedef __u32           hfs_u32;
typedef __s8            hfs_s8;
typedef __s16           hfs_s16;
typedef __s32           hfs_s32;

/* Typedefs for unaligned integer types */
typedef unsigned char hfs_byte_t;
typedef unsigned char hfs_word_t[2];
typedef unsigned char hfs_lword_t[4];

/* these funny looking things are GCC variable argument macros */
#define hfs_warn(format, args...) printk(KERN_WARNING format , ## args)
#define hfs_error(format, args...) printk(KERN_ERR format , ## args)


#if defined(DEBUG_ALL) || defined(DEBUG_MEM)
extern long int hfs_alloc;
#endif

static inline void *hfs_malloc(unsigned int size) {
#if defined(DEBUG_ALL) || defined(DEBUG_MEM)
	hfs_warn("%ld bytes allocation at %s:%u\n",
		 (hfs_alloc += size), __FILE__, __LINE__);
#endif
	return kmalloc(size, GFP_KERNEL);
}

static inline void hfs_free(void *ptr, unsigned int size) {
	kfree(ptr);
#if defined(DEBUG_ALL) || defined(DEBUG_MEM)
	hfs_warn("%ld bytes allocation at %s:%u\n",
		  (hfs_alloc -= ptr ? size : 0), __FILE__, __LINE__);
#endif
}


/* handle conversion between times. 
 *
 * NOTE: hfs+ doesn't need this. also, we don't use tz_dsttime as that's
 *       not a good thing to do. instead, we depend upon tz_minuteswest
 *       having the correct daylight savings correction. 
 */
static inline hfs_u32 hfs_from_utc(hfs_s32 time)
{
	return time - sys_tz.tz_minuteswest*60; 
}

static inline hfs_s32 hfs_to_utc(hfs_u32 time)
{
	return time + sys_tz.tz_minuteswest*60;
}

static inline hfs_u32 hfs_time(void) {
	return htonl(hfs_from_utc(CURRENT_TIME)+2082844800U);
}


/*
 * hfs_wait_queue 
 */
typedef wait_queue_head_t hfs_wait_queue;

static inline void hfs_init_waitqueue(hfs_wait_queue *queue) {
        init_waitqueue_head(queue);
}

static inline void hfs_sleep_on(hfs_wait_queue *queue) {
	sleep_on(queue);
}

static inline void hfs_wake_up(hfs_wait_queue *queue) {
	wake_up(queue);
}

static inline void hfs_relinquish(void) {
	schedule();
}


/*
 * hfs_sysmdb 
 */
typedef struct super_block *hfs_sysmdb;

static inline void hfs_mdb_dirty(hfs_sysmdb sys_mdb) {
	sys_mdb->s_dirt = 1;
}

static inline const char *hfs_mdb_name(hfs_sysmdb sys_mdb) {
	return kdevname(sys_mdb->s_dev);
}


/*
 * hfs_sysentry
 */
typedef struct dentry *hfs_sysentry[4];

/*
 * hfs_buffer
 */
typedef struct buffer_head *hfs_buffer;

#define HFS_BAD_BUFFER NULL

/* In sysdep.c, since it needs HFS_SECTOR_SIZE */
extern hfs_buffer hfs_buffer_get(hfs_sysmdb, int, int);

static inline int hfs_buffer_ok(hfs_buffer buffer) {
	return (buffer != NULL);
}

static inline void hfs_buffer_put(hfs_buffer buffer) {
	brelse(buffer);
}

static inline void hfs_buffer_dirty(hfs_buffer buffer) {
	mark_buffer_dirty(buffer);
}

static inline void hfs_buffer_sync(hfs_buffer buffer) {
	while (buffer_locked(buffer)) {
		wait_on_buffer(buffer);
	}
	if (buffer_dirty(buffer)) {
		ll_rw_block(WRITE, 1, &buffer);
		wait_on_buffer(buffer);
	}
}

static inline void *hfs_buffer_data(const hfs_buffer buffer) {
	return buffer->b_data;
}


/*
 * bit operations
 */

#undef BITNR
#if defined(__BIG_ENDIAN)
#	define BITNR(X)	((X)^31)
#	if !defined(__constant_htonl)
#		define __constant_htonl(x) (x)
#	endif
#	if !defined(__constant_htons)
#		define __constant_htons(x) (x)
#	endif
#elif defined(__LITTLE_ENDIAN)
#	define BITNR(X)	((X)^7)
#	if !defined(__constant_htonl)
#		define __constant_htonl(x) \
        ((unsigned long int)((((unsigned long int)(x) & 0x000000ffU) << 24) | \
                             (((unsigned long int)(x) & 0x0000ff00U) <<  8) | \
                             (((unsigned long int)(x) & 0x00ff0000U) >>  8) | \
                             (((unsigned long int)(x) & 0xff000000U) >> 24)))
#	endif
#	if !defined(__constant_htons)
#		define __constant_htons(x) \
        ((unsigned short int)((((unsigned short int)(x) & 0x00ff) << 8) | \
                              (((unsigned short int)(x) & 0xff00) >> 8)))
#	endif
#else
#	error "Don't know if bytes are big- or little-endian!"
#endif

static inline int hfs_clear_bit(int bitnr, hfs_u32 *lword) {
	return test_and_clear_bit(BITNR(bitnr), lword);
}

static inline int hfs_set_bit(int bitnr, hfs_u32 *lword) {
	return test_and_set_bit(BITNR(bitnr), lword);
}

static inline int hfs_test_bit(int bitnr, const hfs_u32 *lword) {
	/* the kernel should declare the second arg of test_bit as const */
	return test_bit(BITNR(bitnr), (void *)lword);
}

#undef BITNR

/*
 * HFS structures have fields aligned to 16-bit boundaries.
 * So, 16-bit get/put are easy while 32-bit get/put need
 * some care on architectures like the DEC Alpha.
 *
 * In what follows:
 *	ns  = 16-bit integer in network byte-order w/ 16-bit alignment
 *	hs  = 16-bit integer in host byte-order w/ 16-bit alignment
 *	nl  = 32-bit integer in network byte-order w/ unknown alignment
 *	hl  = 32-bit integer in host byte-order w/ unknown alignment
 *	anl = 32-bit integer in network byte-order w/ 32-bit alignment
 *	ahl = 32-bit integer in host byte-order w/ 32-bit alignment
 * Example: hfs_get_hl() gets an unaligned 32-bit integer converting
 *	it to host byte-order.
 */
#define hfs_get_hs(addr)	ntohs(*((hfs_u16 *)(addr)))
#define hfs_get_ns(addr)	(*((hfs_u16 *)(addr)))
#define hfs_get_hl(addr)	ntohl(get_unaligned((hfs_u32 *)(addr)))
#define hfs_get_nl(addr)	get_unaligned((hfs_u32 *)(addr))
#define hfs_get_ahl(addr)	ntohl(*((hfs_u32 *)(addr)))
#define hfs_get_anl(addr)	(*((hfs_u32 *)(addr)))
#define hfs_put_hs(val, addr) 	((void)(*((hfs_u16 *)(addr)) = ntohs(val)))
#define hfs_put_ns(val, addr) 	((void)(*((hfs_u16 *)(addr)) = (val)))
#define hfs_put_hl(val, addr) 	put_unaligned(htonl(val), (hfs_u32 *)(addr))
#define hfs_put_nl(val, addr) 	put_unaligned((val), (hfs_u32 *)(addr))
#define hfs_put_ahl(val, addr) 	((void)(*((hfs_u32 *)(addr)) = ntohl(val)))
#define hfs_put_anl(val, addr) 	((void)(*((hfs_u32 *)(addr)) = (val)))

#endif
