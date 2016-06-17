/* drmP.h -- Private header for Direct Rendering Manager -*- linux-c -*-
 * Created: Mon Jan  4 10:05:05 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 */

#ifndef _DRM_P_H_
#define _DRM_P_H_

#ifdef __KERNEL__
#ifdef __alpha__
/* add include of current.h so that "current" is defined
 * before static inline funcs in wait.h. Doing this so we
 * can build the DRM (part of PI DRI). 4/21/2000 S + B */
#include <asm/current.h>
#endif /* __alpha__ */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/pci.h>
#include <linux/wrapper.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>	/* For (un)lock_kernel */
#include <linux/mm.h>
#include <linux/pagemap.h>
#if defined(__alpha__) || defined(__powerpc__)
#include <asm/pgtable.h> /* For pte_wrprotect */
#endif
#include <asm/io.h>
#include <asm/mman.h>
#include <asm/uaccess.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif
#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
#include <linux/types.h>
#include <linux/agp_backend.h>
#endif
#include <linux/tqueue.h>
#include <linux/poll.h>
#include <asm/pgalloc.h>
#include "drm.h"

#include "drm_os_linux.h"

/* DRM template customization defaults
 */
#ifndef __HAVE_AGP
#define __HAVE_AGP		0
#endif
#ifndef __HAVE_MTRR
#define __HAVE_MTRR		0
#endif
#ifndef __HAVE_CTX_BITMAP
#define __HAVE_CTX_BITMAP	0
#endif
#ifndef __HAVE_DMA
#define __HAVE_DMA		0
#endif
#ifndef __HAVE_DMA_IRQ
#define __HAVE_DMA_IRQ		0
#endif
#ifndef __HAVE_DMA_WAITLIST
#define __HAVE_DMA_WAITLIST	0
#endif
#ifndef __HAVE_DMA_FREELIST
#define __HAVE_DMA_FREELIST	0
#endif
#ifndef __HAVE_DMA_HISTOGRAM
#define __HAVE_DMA_HISTOGRAM	0
#endif

#define __REALLY_HAVE_AGP	(__HAVE_AGP && (defined(CONFIG_AGP) || \
						defined(CONFIG_AGP_MODULE)))
#define __REALLY_HAVE_MTRR	(__HAVE_MTRR && defined(CONFIG_MTRR))


				/* Generic cmpxchg added in 2.3.x */
#ifndef __HAVE_ARCH_CMPXCHG
				/* Include this here so that driver can be
                                   used with older kernels. */
#if defined(__alpha__)
static __inline__ unsigned long
__cmpxchg_u32(volatile int *m, int old, int new)
{
	unsigned long prev, cmp;

	__asm__ __volatile__(
	"1:	ldl_l %0,%2\n"
	"	cmpeq %0,%3,%1\n"
	"	beq %1,2f\n"
	"	mov %4,%1\n"
	"	stl_c %1,%2\n"
	"	beq %1,3f\n"
	"2:	mb\n"
	".subsection 2\n"
	"3:	br 1b\n"
	".previous"
	: "=&r"(prev), "=&r"(cmp), "=m"(*m)
	: "r"((long) old), "r"(new), "m"(*m));

	return prev;
}

static __inline__ unsigned long
__cmpxchg_u64(volatile long *m, unsigned long old, unsigned long new)
{
	unsigned long prev, cmp;

	__asm__ __volatile__(
	"1:	ldq_l %0,%2\n"
	"	cmpeq %0,%3,%1\n"
	"	beq %1,2f\n"
	"	mov %4,%1\n"
	"	stq_c %1,%2\n"
	"	beq %1,3f\n"
	"2:	mb\n"
	".subsection 2\n"
	"3:	br 1b\n"
	".previous"
	: "=&r"(prev), "=&r"(cmp), "=m"(*m)
	: "r"((long) old), "r"(new), "m"(*m));

	return prev;
}

static __inline__ unsigned long
__cmpxchg(volatile void *ptr, unsigned long old, unsigned long new, int size)
{
	switch (size) {
		case 4:
			return __cmpxchg_u32(ptr, old, new);
		case 8:
			return __cmpxchg_u64(ptr, old, new);
	}
	return old;
}
#define cmpxchg(ptr,o,n)						 \
  ({									 \
     __typeof__(*(ptr)) _o_ = (o);					 \
     __typeof__(*(ptr)) _n_ = (n);					 \
     (__typeof__(*(ptr))) __cmpxchg((ptr), (unsigned long)_o_,		 \
				    (unsigned long)_n_, sizeof(*(ptr))); \
  })

#elif __i386__
static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
				      unsigned long new, int size)
{
	unsigned long prev;
	switch (size) {
	case 1:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgb %b1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	case 2:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgw %w1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	case 4:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgl %1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	}
	return old;
}

#define cmpxchg(ptr,o,n)						\
  ((__typeof__(*(ptr)))__cmpxchg((ptr),(unsigned long)(o),		\
				 (unsigned long)(n),sizeof(*(ptr))))
#endif /* i386 & alpha */
#endif
#define __REALLY_HAVE_SG	(__HAVE_SG)

/* Begin the DRM...
 */

#define DRM_DEBUG_CODE 2	  /* Include debugging code (if > 1, then
				     also include looping detection. */

#define DRM_HASH_SIZE	      16 /* Size of key hash table		  */
#define DRM_KERNEL_CONTEXT    0	 /* Change drm_resctx if changed	  */
#define DRM_RESERVED_CONTEXTS 1	 /* Change drm_resctx if changed	  */
#define DRM_LOOPING_LIMIT     5000000
#define DRM_BSZ		      1024 /* Buffer size for /dev/drm? output	  */
#define DRM_TIME_SLICE	      (HZ/20)  /* Time slice for GLXContexts	  */
#define DRM_LOCK_SLICE	      1	/* Time slice for lock, in jiffies	  */

#define DRM_FLAG_DEBUG	  0x01
#define DRM_FLAG_NOCTX	  0x02

#define DRM_MEM_DMA	   0
#define DRM_MEM_SAREA	   1
#define DRM_MEM_DRIVER	   2
#define DRM_MEM_MAGIC	   3
#define DRM_MEM_IOCTLS	   4
#define DRM_MEM_MAPS	   5
#define DRM_MEM_VMAS	   6
#define DRM_MEM_BUFS	   7
#define DRM_MEM_SEGS	   8
#define DRM_MEM_PAGES	   9
#define DRM_MEM_FILES	  10
#define DRM_MEM_QUEUES	  11
#define DRM_MEM_CMDS	  12
#define DRM_MEM_MAPPINGS  13
#define DRM_MEM_BUFLISTS  14
#define DRM_MEM_AGPLISTS  15
#define DRM_MEM_TOTALAGP  16
#define DRM_MEM_BOUNDAGP  17
#define DRM_MEM_CTXBITMAP 18
#define DRM_MEM_STUB      19
#define DRM_MEM_SGLISTS   20

#define DRM_MAX_CTXBITMAP (PAGE_SIZE * 8)

				/* Backward compatibility section */
#ifndef minor
#define minor(x) MINOR((x))
#endif

#ifndef MODULE_LICENSE
#define MODULE_LICENSE(x) 
#endif


#ifndef pte_offset_map 
#define pte_offset_map pte_offset
#define pte_unmap(pte)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,19)
static inline struct page * vmalloc_to_page(void * vmalloc_addr)
{
	unsigned long addr = (unsigned long) vmalloc_addr;
	struct page *page = NULL;
	pgd_t *pgd = pgd_offset_k(addr);
	pmd_t *pmd;
	pte_t *ptep, pte;
  
	if (!pgd_none(*pgd)) {
		pmd = pmd_offset(pgd, addr);
		if (!pmd_none(*pmd)) {
			ptep = pte_offset_map(pmd, addr);
			pte = *ptep;
			if (pte_present(pte))
				page = pte_page(pte);
			pte_unmap(ptep);
		}
	}
	return page;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#define DRM_RPR_ARG(vma)
#else
#define DRM_RPR_ARG(vma) vma,
#endif


#define VM_OFFSET(vma) ((vma)->vm_pgoff << PAGE_SHIFT)

				/* Macros to make printk easier */
#define DRM_ERROR(fmt, arg...) \
	printk(KERN_ERR "[" DRM_NAME ":%s] *ERROR* " fmt , __FUNCTION__ , ##arg)
#define DRM_MEM_ERROR(area, fmt, arg...) \
	printk(KERN_ERR "[" DRM_NAME ":%s:%s] *ERROR* " fmt , __FUNCTION__, \
	       DRM(mem_stats)[area].name , ##arg)
#define DRM_INFO(fmt, arg...)  printk(KERN_INFO "[" DRM_NAME "] " fmt , ##arg)

#if DRM_DEBUG_CODE
#define DRM_DEBUG(fmt, arg...)						\
	do {								\
		if ( DRM(flags) & DRM_FLAG_DEBUG )			\
			printk(KERN_DEBUG				\
			       "[" DRM_NAME ":%s] " fmt ,	\
			       __FUNCTION__ , ##arg);			\
	} while (0)
#else
#define DRM_DEBUG(fmt, arg...)		 do { } while (0)
#endif

#define DRM_PROC_LIMIT (PAGE_SIZE-80)

#define DRM_PROC_PRINT(fmt, arg...)					\
   len += sprintf(&buf[len], fmt , ##arg);				\
   if (len > DRM_PROC_LIMIT) { *eof = 1; return len - offset; }

#define DRM_PROC_PRINT_RET(ret, fmt, arg...)				\
   len += sprintf(&buf[len], fmt , ##arg);				\
   if (len > DRM_PROC_LIMIT) { ret; *eof = 1; return len - offset; }

				/* Mapping helper macros */
#define DRM_IOREMAP(map, dev)					\
	(map)->handle = DRM(ioremap)((map)->offset, (map)->size, (dev) )

#define DRM_IOREMAP_NOCACHE(map, dev)					\
	(map)->handle = DRM(ioremap_nocache)((map)->offset, (map)->size, (dev) )

#define DRM_IOREMAPFREE(map, dev)						\
	do {								\
		if ( (map)->handle && (map)->size )			\
			DRM(ioremapfree)( (map)->handle, (map)->size, (dev) );	\
	} while (0)

#define DRM_FIND_MAP(_map, _o)						\
do {									\
	struct list_head *_list;					\
	list_for_each( _list, &dev->maplist->head ) {			\
		drm_map_list_t *_entry = (drm_map_list_t *)_list;	\
		if ( _entry->map &&					\
		     _entry->map->offset == (_o) ) {			\
			(_map) = _entry->map;				\
			break;						\
 		}							\
	}								\
} while(0)

				/* Internal types and structures */
#define DRM_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define DRM_MIN(a,b) ((a)<(b)?(a):(b))
#define DRM_MAX(a,b) ((a)>(b)?(a):(b))

#define DRM_LEFTCOUNT(x) (((x)->rp + (x)->count - (x)->wp) % ((x)->count + 1))
#define DRM_BUFCOUNT(x) ((x)->count - DRM_LEFTCOUNT(x))
#define DRM_WAITCOUNT(dev,idx) DRM_BUFCOUNT(&dev->queuelist[idx]->waitlist)

#define DRM_GET_PRIV_SAREA(_dev, _ctx, _map) do {	\
	(_map) = (_dev)->context_sareas[_ctx];		\
} while(0)

typedef int drm_ioctl_t( struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg );

typedef struct drm_pci_list {
	u16 vendor;
	u16 device;
} drm_pci_list_t;

typedef struct drm_ioctl_desc {
	drm_ioctl_t	     *func;
	int		     auth_needed;
	int		     root_only;
} drm_ioctl_desc_t;

typedef struct drm_devstate {
	pid_t		  owner;	/* X server pid holding x_lock */

} drm_devstate_t;

typedef struct drm_magic_entry {
	drm_magic_t	       magic;
	struct drm_file	       *priv;
	struct drm_magic_entry *next;
} drm_magic_entry_t;

typedef struct drm_magic_head {
	struct drm_magic_entry *head;
	struct drm_magic_entry *tail;
} drm_magic_head_t;

typedef struct drm_vma_entry {
	struct vm_area_struct *vma;
	struct drm_vma_entry  *next;
	pid_t		      pid;
} drm_vma_entry_t;

typedef struct drm_buf {
	int		  idx;	       /* Index into master buflist	     */
	int		  total;       /* Buffer size			     */
	int		  order;       /* log-base-2(total)		     */
	int		  used;	       /* Amount of buffer in use (for DMA)  */
	unsigned long	  offset;      /* Byte offset (used internally)	     */
	void		  *address;    /* Address of buffer		     */
	unsigned long	  bus_address; /* Bus address of buffer		     */
	struct drm_buf	  *next;       /* Kernel-only: used for free list    */
	__volatile__ int  waiting;     /* On kernel DMA queue		     */
	__volatile__ int  pending;     /* On hardware DMA queue		     */
	wait_queue_head_t dma_wait;    /* Processes waiting		     */
	pid_t		  pid;	       /* PID of holding process	     */
	int		  context;     /* Kernel queue for this buffer	     */
	int		  while_locked;/* Dispatch this buffer while locked  */
	enum {
		DRM_LIST_NONE	 = 0,
		DRM_LIST_FREE	 = 1,
		DRM_LIST_WAIT	 = 2,
		DRM_LIST_PEND	 = 3,
		DRM_LIST_PRIO	 = 4,
		DRM_LIST_RECLAIM = 5
	}		  list;	       /* Which list we're on		     */

#if DRM_DMA_HISTOGRAM
	cycles_t	  time_queued;	   /* Queued to kernel DMA queue     */
	cycles_t	  time_dispatched; /* Dispatched to hardware	     */
	cycles_t	  time_completed;  /* Completed by hardware	     */
	cycles_t	  time_freed;	   /* Back on freelist		     */
#endif

	int		  dev_priv_size; /* Size of buffer private stoarge   */
	void		  *dev_private;  /* Per-buffer private storage       */
} drm_buf_t;

#if DRM_DMA_HISTOGRAM
#define DRM_DMA_HISTOGRAM_SLOTS		  9
#define DRM_DMA_HISTOGRAM_INITIAL	 10
#define DRM_DMA_HISTOGRAM_NEXT(current)	 ((current)*10)
typedef struct drm_histogram {
	atomic_t	  total;

	atomic_t	  queued_to_dispatched[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  dispatched_to_completed[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  completed_to_freed[DRM_DMA_HISTOGRAM_SLOTS];

	atomic_t	  queued_to_completed[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  queued_to_freed[DRM_DMA_HISTOGRAM_SLOTS];

	atomic_t	  dma[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  schedule[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  ctx[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  lacq[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  lhld[DRM_DMA_HISTOGRAM_SLOTS];
} drm_histogram_t;
#endif

				/* bufs is one longer than it has to be */
typedef struct drm_waitlist {
	int		  count;	/* Number of possible buffers	   */
	drm_buf_t	  **bufs;	/* List of pointers to buffers	   */
	drm_buf_t	  **rp;		/* Read pointer			   */
	drm_buf_t	  **wp;		/* Write pointer		   */
	drm_buf_t	  **end;	/* End pointer			   */
	spinlock_t	  read_lock;
	spinlock_t	  write_lock;
} drm_waitlist_t;

typedef struct drm_freelist {
	int		  initialized; /* Freelist in use		   */
	atomic_t	  count;       /* Number of free buffers	   */
	drm_buf_t	  *next;       /* End pointer			   */

	wait_queue_head_t waiting;     /* Processes waiting on free bufs   */
	int		  low_mark;    /* Low water mark		   */
	int		  high_mark;   /* High water mark		   */
	atomic_t	  wfh;	       /* If waiting for high mark	   */
	spinlock_t        lock;
} drm_freelist_t;

typedef struct drm_buf_entry {
	int		  buf_size;
	int		  buf_count;
	drm_buf_t	  *buflist;
	int		  seg_count;
	int		  page_order;
	unsigned long	  *seglist;

	drm_freelist_t	  freelist;
} drm_buf_entry_t;

typedef struct drm_hw_lock {
	__volatile__ unsigned int lock;
	char			  padding[60]; /* Pad to cache line */
} drm_hw_lock_t;

typedef struct drm_file {
	int		  authenticated;
	int		  minor;
	pid_t		  pid;
	uid_t		  uid;
	drm_magic_t	  magic;
	unsigned long	  ioctl_count;
	struct drm_file	  *next;
	struct drm_file	  *prev;
	struct drm_device *dev;
	int 		  remove_auth_on_close;
} drm_file_t;


typedef struct drm_queue {
	atomic_t	  use_count;	/* Outstanding uses (+1)	    */
	atomic_t	  finalization;	/* Finalization in progress	    */
	atomic_t	  block_count;	/* Count of processes waiting	    */
	atomic_t	  block_read;	/* Queue blocked for reads	    */
	wait_queue_head_t read_queue;	/* Processes waiting on block_read  */
	atomic_t	  block_write;	/* Queue blocked for writes	    */
	wait_queue_head_t write_queue;	/* Processes waiting on block_write */
#if 1
	atomic_t	  total_queued;	/* Total queued statistic	    */
	atomic_t	  total_flushed;/* Total flushes statistic	    */
	atomic_t	  total_locks;	/* Total locks statistics	    */
#endif
	drm_ctx_flags_t	  flags;	/* Context preserving and 2D-only   */
	drm_waitlist_t	  waitlist;	/* Pending buffers		    */
	wait_queue_head_t flush_queue;	/* Processes waiting until flush    */
} drm_queue_t;

typedef struct drm_lock_data {
	drm_hw_lock_t	  *hw_lock;	/* Hardware lock		   */
	pid_t		  pid;		/* PID of lock holder (0=kernel)   */
	wait_queue_head_t lock_queue;	/* Queue of blocked processes	   */
	unsigned long	  lock_time;	/* Time of last lock in jiffies	   */
} drm_lock_data_t;

typedef struct drm_device_dma {
#if 0
				/* Performance Counters */
	atomic_t	  total_prio;	/* Total DRM_DMA_PRIORITY	   */
	atomic_t	  total_bytes;	/* Total bytes DMA'd		   */
	atomic_t	  total_dmas;	/* Total DMA buffers dispatched	   */

	atomic_t	  total_missed_dma;  /* Missed drm_do_dma	    */
	atomic_t	  total_missed_lock; /* Missed lock in drm_do_dma   */
	atomic_t	  total_missed_free; /* Missed drm_free_this_buffer */
	atomic_t	  total_missed_sched;/* Missed drm_dma_schedule	    */

	atomic_t	  total_tried;	/* Tried next_buffer		    */
	atomic_t	  total_hit;	/* Sent next_buffer		    */
	atomic_t	  total_lost;	/* Lost interrupt		    */
#endif

	drm_buf_entry_t	  bufs[DRM_MAX_ORDER+1];
	int		  buf_count;
	drm_buf_t	  **buflist;	/* Vector of pointers info bufs	   */
	int		  seg_count;
	int		  page_count;
	unsigned long	  *pagelist;
	unsigned long	  byte_count;
	enum {
		_DRM_DMA_USE_AGP = 0x01,
		_DRM_DMA_USE_SG  = 0x02
	} flags;

				/* DMA support */
	drm_buf_t	  *this_buffer;	/* Buffer being sent		   */
	drm_buf_t	  *next_buffer; /* Selected buffer to send	   */
	drm_queue_t	  *next_queue;	/* Queue from which buffer selected*/
	wait_queue_head_t waiting;	/* Processes waiting on free bufs  */
} drm_device_dma_t;

#if __REALLY_HAVE_AGP
typedef struct drm_agp_mem {
	unsigned long      handle;
	agp_memory         *memory;
	unsigned long      bound; /* address */
	int                pages;
	struct drm_agp_mem *prev;
	struct drm_agp_mem *next;
} drm_agp_mem_t;

typedef struct drm_agp_head {
	agp_kern_info      agp_info;
	const char         *chipset;
	drm_agp_mem_t      *memory;
	unsigned long      mode;
	int                enabled;
	int                acquired;
	unsigned long      base;
   	int 		   agp_mtrr;
	int		   cant_use_aperture;
	unsigned long	   page_mask;
} drm_agp_head_t;
#endif

typedef struct drm_sg_mem {
	unsigned long   handle;
	void            *virtual;
	int             pages;
	struct page     **pagelist;
	dma_addr_t	*busaddr;
} drm_sg_mem_t;

typedef struct drm_sigdata {
	int           context;
	drm_hw_lock_t *lock;
} drm_sigdata_t;

typedef struct drm_map_list {
	struct list_head	head;
	drm_map_t		*map;
} drm_map_list_t;

#if __HAVE_VBL_IRQ

typedef struct drm_vbl_sig {
	struct list_head	head;
	unsigned int		sequence;
	struct siginfo		info;
	struct task_struct	*task;
} drm_vbl_sig_t;

#endif

typedef struct drm_device {
	const char	  *name;	/* Simple driver name		   */
	char		  *unique;	/* Unique identifier: e.g., busid  */
	int		  unique_len;	/* Length of unique field	   */
	dev_t		  device;	/* Device number for mknod	   */
	char		  *devname;	/* For /proc/interrupts		   */

	int		  blocked;	/* Blocked due to VC switch?	   */
	struct proc_dir_entry *root;	/* Root for this device's entries  */

				/* Locks */
	spinlock_t	  count_lock;	/* For inuse, open_count, buf_use  */
	struct semaphore  struct_sem;	/* For others			   */

				/* Usage Counters */
	int		  open_count;	/* Outstanding files open	   */
	atomic_t	  ioctl_count;	/* Outstanding IOCTLs pending	   */
	atomic_t	  vma_count;	/* Outstanding vma areas open	   */
	int		  buf_use;	/* Buffers in use -- cannot alloc  */
	atomic_t	  buf_alloc;	/* Buffer allocation in progress   */

				/* Performance counters */
	unsigned long     counters;
	drm_stat_type_t   types[15];
	atomic_t          counts[15];

				/* Authentication */
	drm_file_t	  *file_first;
	drm_file_t	  *file_last;
	drm_magic_head_t  magiclist[DRM_HASH_SIZE];

				/* Memory management */
	drm_map_list_t	  *maplist;	/* Linked list of regions	   */
	int		  map_count;	/* Number of mappable regions	   */

	drm_map_t	  **context_sareas;
	int		  max_context;

	drm_vma_entry_t	  *vmalist;	/* List of vmas (for debugging)	   */
	drm_lock_data_t	  lock;		/* Information on hardware lock	   */

				/* DMA queues (contexts) */
	int		  queue_count;	/* Number of active DMA queues	   */
	int		  queue_reserved; /* Number of reserved DMA queues */
	int		  queue_slots;	/* Actual length of queuelist	   */
	drm_queue_t	  **queuelist;	/* Vector of pointers to DMA queues */
	drm_device_dma_t  *dma;		/* Optional pointer for DMA support */

				/* Context support */
	int		  irq;		/* Interrupt used by board	   */
	__volatile__ long context_flag;	/* Context swapping flag	   */
	__volatile__ long interrupt_flag; /* Interruption handler flag	   */
	__volatile__ long dma_flag;	/* DMA dispatch flag		   */
	struct timer_list timer;	/* Timer for delaying ctx switch   */
	wait_queue_head_t context_wait; /* Processes waiting on ctx switch */
	int		  last_checked;	/* Last context checked for DMA	   */
	int		  last_context;	/* Last current context		   */
	unsigned long	  last_switch;	/* jiffies at last context switch  */
	struct tq_struct  tq;
#if __HAVE_VBL_IRQ
   	wait_queue_head_t vbl_queue;
   	atomic_t          vbl_received;
	spinlock_t        vbl_lock;
	drm_vbl_sig_t     vbl_sigs;
	unsigned int      vbl_pending;
#endif
	cycles_t	  ctx_start;
	cycles_t	  lck_start;
#if __HAVE_DMA_HISTOGRAM
	drm_histogram_t	  histo;
#endif

				/* Callback to X server for context switch
				   and for heavy-handed reset. */
	char		  buf[DRM_BSZ]; /* Output buffer		   */
	char		  *buf_rp;	/* Read pointer			   */
	char		  *buf_wp;	/* Write pointer		   */
	char		  *buf_end;	/* End pointer			   */
	struct fasync_struct *buf_async;/* Processes waiting for SIGIO	   */
	wait_queue_head_t buf_readers;	/* Processes waiting to read	   */
	wait_queue_head_t buf_writers;	/* Processes waiting to ctx switch */

#if __REALLY_HAVE_AGP
	drm_agp_head_t    *agp;
#endif
	struct pci_dev *pdev;
#ifdef __alpha__
	struct pci_controller *hose;
#endif
	drm_sg_mem_t      *sg;  /* Scatter gather memory */
	unsigned long     *ctx_bitmap;
	void		  *dev_private;
	drm_sigdata_t     sigdata; /* For block_all_signals */
	sigset_t          sigmask;
} drm_device_t;


/* ================================================================
 * Internal function definitions
 */

				/* Misc. support (drm_init.h) */
extern int	     DRM(flags);
extern void	     DRM(parse_options)( char *s );
extern int           DRM(cpu_valid)( void );

				/* Driver support (drm_drv.h) */
extern int           DRM(version)(struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg);
extern int           DRM(open)(struct inode *inode, struct file *filp);
extern int           DRM(release)(struct inode *inode, struct file *filp);
extern int           DRM(ioctl)(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long arg);
extern int           DRM(lock)(struct inode *inode, struct file *filp,
			       unsigned int cmd, unsigned long arg);
extern int           DRM(unlock)(struct inode *inode, struct file *filp,
				 unsigned int cmd, unsigned long arg);

				/* Device support (drm_fops.h) */
extern int	     DRM(open_helper)(struct inode *inode, struct file *filp,
				      drm_device_t *dev);
extern int	     DRM(flush)(struct file *filp);
extern int	     DRM(release_fuck)(struct inode *inode, struct file *filp);
extern int	     DRM(fasync)(int fd, struct file *filp, int on);
extern ssize_t	     DRM(read)(struct file *filp, char *buf, size_t count,
			       loff_t *off);
extern int	     DRM(write_string)(drm_device_t *dev, const char *s);
extern unsigned int  DRM(poll)(struct file *filp,
			       struct poll_table_struct *wait);

				/* Mapping support (drm_vm.h) */
extern struct page *DRM(vm_nopage)(struct vm_area_struct *vma,
				   unsigned long address,
				   int write_access);
extern struct page *DRM(vm_shm_nopage)(struct vm_area_struct *vma,
				       unsigned long address,
				       int write_access);
extern struct page *DRM(vm_dma_nopage)(struct vm_area_struct *vma,
				       unsigned long address,
				       int write_access);
extern struct page *DRM(vm_sg_nopage)(struct vm_area_struct *vma,
				      unsigned long address,
				      int write_access);
extern void	     DRM(vm_open)(struct vm_area_struct *vma);
extern void	     DRM(vm_close)(struct vm_area_struct *vma);
extern void	     DRM(vm_shm_close)(struct vm_area_struct *vma);
extern int	     DRM(mmap_dma)(struct file *filp,
				   struct vm_area_struct *vma);
extern int	     DRM(mmap)(struct file *filp, struct vm_area_struct *vma);

				/* Memory management support (drm_memory.h) */
extern void	     DRM(mem_init)(void);
extern int	     DRM(mem_info)(char *buf, char **start, off_t offset,
				   int request, int *eof, void *data);
extern void	     *DRM(alloc)(size_t size, int area);
extern void	     *DRM(realloc)(void *oldpt, size_t oldsize, size_t size,
				   int area);
extern char	     *DRM(strdup)(const char *s, int area);
extern void	     DRM(strfree)(const char *s, int area);
extern void	     DRM(free)(void *pt, size_t size, int area);
extern unsigned long DRM(alloc_pages)(int order, int area);
extern void	     DRM(free_pages)(unsigned long address, int order,
				     int area);
extern void	     *DRM(ioremap)(unsigned long offset, unsigned long size, drm_device_t *dev);
extern void	     *DRM(ioremap_nocache)(unsigned long offset, unsigned long size, drm_device_t *dev);
extern void	     DRM(ioremapfree)(void *pt, unsigned long size, drm_device_t *dev);

#if __REALLY_HAVE_AGP
extern agp_memory    *DRM(alloc_agp)(int pages, u32 type);
extern int           DRM(free_agp)(agp_memory *handle, int pages);
extern int           DRM(bind_agp)(agp_memory *handle, unsigned int start);
extern int           DRM(unbind_agp)(agp_memory *handle);
#endif

				/* Misc. IOCTL support (drm_ioctl.h) */
extern int	     DRM(irq_busid)(struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg);
extern int	     DRM(getunique)(struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg);
extern int	     DRM(setunique)(struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg);
extern int	     DRM(getmap)(struct inode *inode, struct file *filp,
				 unsigned int cmd, unsigned long arg);
extern int	     DRM(getclient)(struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg);
extern int	     DRM(getstats)(struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg);

				/* Context IOCTL support (drm_context.h) */
extern int	     DRM(resctx)( struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg );
extern int	     DRM(addctx)( struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg );
extern int	     DRM(modctx)( struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg );
extern int	     DRM(getctx)( struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg );
extern int	     DRM(switchctx)( struct inode *inode, struct file *filp,
				     unsigned int cmd, unsigned long arg );
extern int	     DRM(newctx)( struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg );
extern int	     DRM(rmctx)( struct inode *inode, struct file *filp,
				 unsigned int cmd, unsigned long arg );

extern int	     DRM(context_switch)(drm_device_t *dev, int old, int new);
extern int	     DRM(context_switch_complete)(drm_device_t *dev, int new);

#if __HAVE_CTX_BITMAP
extern int	     DRM(ctxbitmap_init)( drm_device_t *dev );
extern void	     DRM(ctxbitmap_cleanup)( drm_device_t *dev );
#endif

extern int	     DRM(setsareactx)( struct inode *inode, struct file *filp,
				       unsigned int cmd, unsigned long arg );
extern int	     DRM(getsareactx)( struct inode *inode, struct file *filp,
				       unsigned int cmd, unsigned long arg );

				/* Drawable IOCTL support (drm_drawable.h) */
extern int	     DRM(adddraw)(struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg);
extern int	     DRM(rmdraw)(struct inode *inode, struct file *filp,
				 unsigned int cmd, unsigned long arg);


				/* Authentication IOCTL support (drm_auth.h) */
extern int	     DRM(add_magic)(drm_device_t *dev, drm_file_t *priv,
				    drm_magic_t magic);
extern int	     DRM(remove_magic)(drm_device_t *dev, drm_magic_t magic);
extern int	     DRM(getmagic)(struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg);
extern int	     DRM(authmagic)(struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg);


				/* Locking IOCTL support (drm_lock.h) */
extern int	     DRM(block)(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long arg);
extern int	     DRM(unblock)(struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg);
extern int	     DRM(lock_take)(__volatile__ unsigned int *lock,
				    unsigned int context);
extern int	     DRM(lock_transfer)(drm_device_t *dev,
					__volatile__ unsigned int *lock,
					unsigned int context);
extern int	     DRM(lock_free)(drm_device_t *dev,
				    __volatile__ unsigned int *lock,
				    unsigned int context);
extern int	     DRM(finish)(struct inode *inode, struct file *filp,
				 unsigned int cmd, unsigned long arg);
extern int	     DRM(flush_unblock)(drm_device_t *dev, int context,
					drm_lock_flags_t flags);
extern int	     DRM(flush_block_and_flush)(drm_device_t *dev, int context,
						drm_lock_flags_t flags);
extern int           DRM(notifier)(void *priv);

				/* Buffer management support (drm_bufs.h) */
extern int	     DRM(order)( unsigned long size );
extern int	     DRM(addmap)( struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg );
extern int	     DRM(rmmap)( struct inode *inode, struct file *filp,
				 unsigned int cmd, unsigned long arg );
#if __HAVE_DMA
extern int	     DRM(addbufs)( struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg );
extern int	     DRM(infobufs)( struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg );
extern int	     DRM(markbufs)( struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg );
extern int	     DRM(freebufs)( struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg );
extern int	     DRM(mapbufs)( struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg );

				/* DMA support (drm_dma.h) */
extern int	     DRM(dma_setup)(drm_device_t *dev);
extern void	     DRM(dma_takedown)(drm_device_t *dev);
extern void	     DRM(free_buffer)(drm_device_t *dev, drm_buf_t *buf);
extern void	     DRM(reclaim_buffers)(drm_device_t *dev, pid_t pid);
#if __HAVE_OLD_DMA
/* GH: This is a dirty hack for now...
 */
extern void	     DRM(clear_next_buffer)(drm_device_t *dev);
extern int	     DRM(select_queue)(drm_device_t *dev,
				       void (*wrapper)(unsigned long));
extern int	     DRM(dma_enqueue)(drm_device_t *dev, drm_dma_t *dma);
extern int	     DRM(dma_get_buffers)(drm_device_t *dev, drm_dma_t *dma);
#endif
#if __HAVE_DMA_IRQ
extern int           DRM(control)( struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg );
extern int           DRM(irq_install)( drm_device_t *dev, int irq );
extern int           DRM(irq_uninstall)( drm_device_t *dev );
extern void          DRM(dma_service)( int irq, void *device,
				       struct pt_regs *regs );
extern void          DRM(driver_irq_preinstall)( drm_device_t *dev );
extern void          DRM(driver_irq_postinstall)( drm_device_t *dev );
extern void          DRM(driver_irq_uninstall)( drm_device_t *dev );
#if __HAVE_VBL_IRQ
extern int           DRM(wait_vblank)(struct inode *inode, struct file *filp,
				      unsigned int cmd, unsigned long arg);
extern int           DRM(vblank_wait)(drm_device_t *dev, unsigned int *vbl_seq);
extern void          DRM(vbl_send_signals)( drm_device_t *dev );
#endif
#if __HAVE_DMA_IRQ_BH
extern void          DRM(dma_immediate_bh)( void *dev );
#endif
#endif
#if DRM_DMA_HISTOGRAM
extern int	     DRM(histogram_slot)(unsigned long count);
extern void	     DRM(histogram_compute)(drm_device_t *dev, drm_buf_t *buf);
#endif

				/* Buffer list support (drm_lists.h) */
#if __HAVE_DMA_WAITLIST
extern int	     DRM(waitlist_create)(drm_waitlist_t *bl, int count);
extern int	     DRM(waitlist_destroy)(drm_waitlist_t *bl);
extern int	     DRM(waitlist_put)(drm_waitlist_t *bl, drm_buf_t *buf);
extern drm_buf_t     *DRM(waitlist_get)(drm_waitlist_t *bl);
#endif
#if __HAVE_DMA_FREELIST
extern int	     DRM(freelist_create)(drm_freelist_t *bl, int count);
extern int	     DRM(freelist_destroy)(drm_freelist_t *bl);
extern int	     DRM(freelist_put)(drm_device_t *dev, drm_freelist_t *bl,
				       drm_buf_t *buf);
extern drm_buf_t     *DRM(freelist_get)(drm_freelist_t *bl, int block);
#endif
#endif /* __HAVE_DMA */

#if __REALLY_HAVE_AGP
				/* AGP/GART support (drm_agpsupport.h) */
extern drm_agp_head_t *DRM(agp_init)(void);
extern void           DRM(agp_uninit)(void);
extern int            DRM(agp_acquire)(struct inode *inode, struct file *filp,
				       unsigned int cmd, unsigned long arg);
extern void           DRM(agp_do_release)(void);
extern int            DRM(agp_release)(struct inode *inode, struct file *filp,
				       unsigned int cmd, unsigned long arg);
extern int            DRM(agp_enable)(struct inode *inode, struct file *filp,
				      unsigned int cmd, unsigned long arg);
extern int            DRM(agp_info)(struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg);
extern int            DRM(agp_alloc)(struct inode *inode, struct file *filp,
				     unsigned int cmd, unsigned long arg);
extern int            DRM(agp_free)(struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg);
extern int            DRM(agp_unbind)(struct inode *inode, struct file *filp,
				      unsigned int cmd, unsigned long arg);
extern int            DRM(agp_bind)(struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg);
extern agp_memory     *DRM(agp_allocate_memory)(size_t pages, u32 type);
extern int            DRM(agp_free_memory)(agp_memory *handle);
extern int            DRM(agp_bind_memory)(agp_memory *handle, off_t start);
extern int            DRM(agp_unbind_memory)(agp_memory *handle);
#endif

				/* Stub support (drm_stub.h) */
int                   DRM(stub_register)(const char *name,
					 struct file_operations *fops,
					 drm_device_t *dev);
int                   DRM(stub_unregister)(int minor);

				/* Proc support (drm_proc.h) */
extern struct proc_dir_entry *DRM(proc_init)(drm_device_t *dev,
					     int minor,
					     struct proc_dir_entry *root,
					     struct proc_dir_entry **dev_root);
extern int            DRM(proc_cleanup)(int minor,
					struct proc_dir_entry *root,
					struct proc_dir_entry *dev_root);

#if __HAVE_SG
				/* Scatter Gather Support (drm_scatter.h) */
extern void           DRM(sg_cleanup)(drm_sg_mem_t *entry);
extern int            DRM(sg_alloc)(struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg);
extern int            DRM(sg_free)(struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg);
#endif

                               /* ATI PCIGART support (ati_pcigart.h) */
extern int            DRM(ati_pcigart_init)(drm_device_t *dev,
					    unsigned long *addr,
					    dma_addr_t *bus_addr);
extern int            DRM(ati_pcigart_cleanup)(drm_device_t *dev,
					       unsigned long addr,
					       dma_addr_t bus_addr);

#endif /* __KERNEL__ */
#endif
