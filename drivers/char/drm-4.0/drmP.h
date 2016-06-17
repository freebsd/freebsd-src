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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 * 
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
#include <linux/major.h>
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
#ifdef __alpha__
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
#if LINUX_VERSION_CODE >= 0x020100 /* KERNEL_VERSION(2,1,0) */
#include <linux/tqueue.h>
#include <linux/poll.h>
#endif
#if LINUX_VERSION_CODE < 0x020400
#include "compat-pre24.h"
#endif
#include "drm.h"

#define DRM_DEBUG_CODE 2	  /* Include debugging code (if > 1, then
				     also include looping detection. */
#define DRM_DMA_HISTOGRAM 1	  /* Make histogram of DMA latency. */

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

#define DRM_MAX_CTXBITMAP (PAGE_SIZE * 8)

				/* Backward compatibility section */
				/* _PAGE_WT changed to _PAGE_PWT in 2.2.6 */
#ifndef _PAGE_PWT
#define _PAGE_PWT _PAGE_WT
#endif
				/* Wait queue declarations changed in 2.3.1 */
#ifndef DECLARE_WAITQUEUE
#define DECLARE_WAITQUEUE(w,c) struct wait_queue w = { c, NULL }
typedef struct wait_queue *wait_queue_head_t;
#define init_waitqueue_head(q) *q = NULL;
#endif

				/* _PAGE_4M changed to _PAGE_PSE in 2.3.23 */
#ifndef _PAGE_PSE
#define _PAGE_PSE _PAGE_4M
#endif

				/* vm_offset changed to vm_pgoff in 2.3.25 */
#if LINUX_VERSION_CODE < 0x020319
#define VM_OFFSET(vma) ((vma)->vm_offset)
#else
#define VM_OFFSET(vma) ((vma)->vm_pgoff << PAGE_SHIFT)
#endif

				/* *_nopage return values defined in 2.3.26 */
#ifndef NOPAGE_SIGBUS
#define NOPAGE_SIGBUS 0
#endif
#ifndef NOPAGE_OOM
#define NOPAGE_OOM 0
#endif

				/* module_init/module_exit added in 2.3.13 */
#ifndef module_init
#define module_init(x)  int init_module(void) { return x(); }
#endif
#ifndef module_exit
#define module_exit(x)  void cleanup_module(void) { x(); }
#endif

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

				/* Macros to make printk easier */
#define DRM_ERROR(fmt, arg...) \
	printk(KERN_ERR "[" DRM_NAME ":" __FUNCTION__ "] *ERROR* " fmt , ##arg)
#define DRM_MEM_ERROR(area, fmt, arg...) \
	printk(KERN_ERR "[" DRM_NAME ":" __FUNCTION__ ":%s] *ERROR* " fmt , \
	       drm_mem_stats[area].name , ##arg)
#define DRM_INFO(fmt, arg...)  printk(KERN_INFO "[" DRM_NAME "] " fmt , ##arg)

#if DRM_DEBUG_CODE
#define DRM_DEBUG(fmt, arg...)						  \
	do {								  \
		if (drm_flags&DRM_FLAG_DEBUG)				  \
			printk(KERN_DEBUG				  \
			       "[" DRM_NAME ":" __FUNCTION__ "] " fmt ,	  \
			       ##arg);					  \
	} while (0)
#else
#define DRM_DEBUG(fmt, arg...)		 do { } while (0)
#endif

#define DRM_PROC_LIMIT (PAGE_SIZE-80)

#define DRM_PROC_PRINT(fmt, arg...)	   \
   len += sprintf(&buf[len], fmt , ##arg); \
   if (len > DRM_PROC_LIMIT) return len;

#define DRM_PROC_PRINT_RET(ret, fmt, arg...)	    \
   len += sprintf(&buf[len], fmt , ##arg);	    \
   if (len > DRM_PROC_LIMIT) { ret; return len; }

				/* Internal types and structures */
#define DRM_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define DRM_MIN(a,b) ((a)<(b)?(a):(b))
#define DRM_MAX(a,b) ((a)>(b)?(a):(b))

#define DRM_LEFTCOUNT(x) (((x)->rp + (x)->count - (x)->wp) % ((x)->count + 1))
#define DRM_BUFCOUNT(x) ((x)->count - DRM_LEFTCOUNT(x))
#define DRM_WAITCOUNT(dev,idx) DRM_BUFCOUNT(&dev->queuelist[idx]->waitlist)

typedef int drm_ioctl_t(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);

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
	atomic_t	  total_queued;	/* Total queued statistic	    */
	atomic_t	  total_flushed;/* Total flushes statistic	    */
	atomic_t	  total_locks;	/* Total locks statistics	    */
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

	drm_buf_entry_t	  bufs[DRM_MAX_ORDER+1];
	int		  buf_count;
	drm_buf_t	  **buflist;	/* Vector of pointers info bufs	   */
	int		  seg_count; 
	int		  page_count;
	unsigned long	  *pagelist;
	unsigned long	  byte_count;
	enum {
	   _DRM_DMA_USE_AGP = 0x01
	} flags;

				/* DMA support */
	drm_buf_t	  *this_buffer;	/* Buffer being sent		   */
	drm_buf_t	  *next_buffer; /* Selected buffer to send	   */
	drm_queue_t	  *next_queue;	/* Queue from which buffer selected*/
	wait_queue_head_t waiting;	/* Processes waiting on free bufs  */
} drm_device_dma_t;

#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
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
} drm_agp_head_t;
#endif

typedef struct drm_sigdata {
	int           context;
	drm_hw_lock_t *lock;
} drm_sigdata_t;

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

				/* Performance Counters */
	atomic_t	  total_open;
	atomic_t	  total_close;
	atomic_t	  total_ioctl;
	atomic_t	  total_irq;	/* Total interruptions		   */
	atomic_t	  total_ctx;	/* Total context switches	   */
	
	atomic_t	  total_locks;
	atomic_t	  total_unlocks;
	atomic_t	  total_contends;
	atomic_t	  total_sleeps;

				/* Authentication */
	drm_file_t	  *file_first;
	drm_file_t	  *file_last;
	drm_magic_head_t  magiclist[DRM_HASH_SIZE];

				/* Memory management */
	drm_map_t	  **maplist;	/* Vector of pointers to regions   */
	int		  map_count;	/* Number of mappable regions	   */

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
	cycles_t	  ctx_start;
	cycles_t	  lck_start;
#if DRM_DMA_HISTOGRAM
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
	
#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
	drm_agp_head_t    *agp;
#endif
	unsigned long     *ctx_bitmap;
	void		  *dev_private;
	drm_sigdata_t     sigdata; /* For block_all_signals */
	sigset_t          sigmask;
} drm_device_t;


				/* Internal function definitions */

				/* Misc. support (init.c) */
extern int	     drm_flags;
extern void	     drm_parse_options(char *s);
extern int           drm_cpu_valid(void);


				/* Device support (fops.c) */
extern int	     drm_open_helper(struct inode *inode, struct file *filp,
				     drm_device_t *dev);
extern int	     drm_flush(struct file *filp);
extern int	     drm_release(struct inode *inode, struct file *filp);
extern int	     drm_fasync(int fd, struct file *filp, int on);
extern ssize_t	     drm_read(struct file *filp, char *buf, size_t count,
			      loff_t *off);
extern int	     drm_write_string(drm_device_t *dev, const char *s);
extern unsigned int  drm_poll(struct file *filp, struct poll_table_struct *wait);

				/* Mapping support (vm.c) */
#if LINUX_VERSION_CODE < 0x020317
extern unsigned long drm_vm_nopage(struct vm_area_struct *vma,
				   unsigned long address,
				   int write_access);
extern unsigned long drm_vm_shm_nopage(struct vm_area_struct *vma,
				       unsigned long address,
				       int write_access);
extern unsigned long drm_vm_shm_nopage_lock(struct vm_area_struct *vma,
					    unsigned long address,
					    int write_access);
extern unsigned long drm_vm_dma_nopage(struct vm_area_struct *vma,
				       unsigned long address,
				       int write_access);
#else
				/* Return type changed in 2.3.23 */
extern struct page *drm_vm_nopage(struct vm_area_struct *vma,
				  unsigned long address,
				  int write_access);
extern struct page *drm_vm_shm_nopage(struct vm_area_struct *vma,
				      unsigned long address,
				      int write_access);
extern struct page *drm_vm_shm_nopage_lock(struct vm_area_struct *vma,
					   unsigned long address,
					   int write_access);
extern struct page *drm_vm_dma_nopage(struct vm_area_struct *vma,
				      unsigned long address,
				      int write_access);
#endif
extern void	     drm_vm_open(struct vm_area_struct *vma);
extern void	     drm_vm_close(struct vm_area_struct *vma);
extern int	     drm_mmap_dma(struct file *filp,
				  struct vm_area_struct *vma);
extern int	     drm_mmap(struct file *filp, struct vm_area_struct *vma);


				/* Proc support (proc.c) */
extern int	     drm_proc_init(drm_device_t *dev);
extern int	     drm_proc_cleanup(void);

				/* Memory management support (memory.c) */
extern void	     drm_mem_init(void);
extern int	     drm_mem_info(char *buf, char **start, off_t offset,
				  int len, int *eof, void *data);
extern void	     *drm_alloc(size_t size, int area);
extern void	     *drm_realloc(void *oldpt, size_t oldsize, size_t size,
				  int area);
extern char	     *drm_strdup(const char *s, int area);
extern void	     drm_strfree(const char *s, int area);
extern void	     drm_free(void *pt, size_t size, int area);
extern unsigned long drm_alloc_pages(int order, int area);
extern void	     drm_free_pages(unsigned long address, int order,
				    int area);
extern void	     *drm_ioremap(unsigned long offset, unsigned long size, drm_device_t *dev);
extern void	     drm_ioremapfree(void *pt, unsigned long size, drm_device_t *dev);

#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
extern agp_memory    *drm_alloc_agp(int pages, u32 type);
extern int           drm_free_agp(agp_memory *handle, int pages);
extern int           drm_bind_agp(agp_memory *handle, unsigned int start);
extern int           drm_unbind_agp(agp_memory *handle);
#endif


				/* Buffer management support (bufs.c) */
extern int	     drm_order(unsigned long size);
extern int	     drm_addmap(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long arg);
extern int	     drm_addbufs(struct inode *inode, struct file *filp,
				 unsigned int cmd, unsigned long arg);
extern int	     drm_infobufs(struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg);
extern int	     drm_markbufs(struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg);
extern int	     drm_freebufs(struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg);
extern int	     drm_mapbufs(struct inode *inode, struct file *filp,
				 unsigned int cmd, unsigned long arg);


				/* Buffer list management support (lists.c) */
extern int	     drm_waitlist_create(drm_waitlist_t *bl, int count);
extern int	     drm_waitlist_destroy(drm_waitlist_t *bl);
extern int	     drm_waitlist_put(drm_waitlist_t *bl, drm_buf_t *buf);
extern drm_buf_t     *drm_waitlist_get(drm_waitlist_t *bl);

extern int	     drm_freelist_create(drm_freelist_t *bl, int count);
extern int	     drm_freelist_destroy(drm_freelist_t *bl);
extern int	     drm_freelist_put(drm_device_t *dev, drm_freelist_t *bl,
				      drm_buf_t *buf);
extern drm_buf_t     *drm_freelist_get(drm_freelist_t *bl, int block);

				/* DMA support (gen_dma.c) */
extern void	     drm_dma_setup(drm_device_t *dev);
extern void	     drm_dma_takedown(drm_device_t *dev);
extern void	     drm_free_buffer(drm_device_t *dev, drm_buf_t *buf);
extern void	     drm_reclaim_buffers(drm_device_t *dev, pid_t pid);
extern int	     drm_context_switch(drm_device_t *dev, int old, int new);
extern int	     drm_context_switch_complete(drm_device_t *dev, int new);
extern void	     drm_clear_next_buffer(drm_device_t *dev);
extern int	     drm_select_queue(drm_device_t *dev,
				      void (*wrapper)(unsigned long));
extern int	     drm_dma_enqueue(drm_device_t *dev, drm_dma_t *dma);
extern int	     drm_dma_get_buffers(drm_device_t *dev, drm_dma_t *dma);
#if DRM_DMA_HISTOGRAM
extern int	     drm_histogram_slot(unsigned long count);
extern void	     drm_histogram_compute(drm_device_t *dev, drm_buf_t *buf);
#endif


				/* Misc. IOCTL support (ioctl.c) */
extern int	     drm_irq_busid(struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg);
extern int	     drm_getunique(struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg);
extern int	     drm_setunique(struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg);


				/* Context IOCTL support (context.c) */
extern int	     drm_resctx(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long arg);
extern int	     drm_addctx(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long arg);
extern int	     drm_modctx(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long arg);
extern int	     drm_getctx(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long arg);
extern int	     drm_switchctx(struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg);
extern int	     drm_newctx(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long arg);
extern int	     drm_rmctx(struct inode *inode, struct file *filp,
			       unsigned int cmd, unsigned long arg);


				/* Drawable IOCTL support (drawable.c) */
extern int	     drm_adddraw(struct inode *inode, struct file *filp,
				 unsigned int cmd, unsigned long arg);
extern int	     drm_rmdraw(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long arg);


				/* Authentication IOCTL support (auth.c) */
extern int	     drm_add_magic(drm_device_t *dev, drm_file_t *priv,
				   drm_magic_t magic);
extern int	     drm_remove_magic(drm_device_t *dev, drm_magic_t magic);
extern int	     drm_getmagic(struct inode *inode, struct file *filp,
				  unsigned int cmd, unsigned long arg);
extern int	     drm_authmagic(struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg);


				/* Locking IOCTL support (lock.c) */
extern int	     drm_block(struct inode *inode, struct file *filp,
			       unsigned int cmd, unsigned long arg);
extern int	     drm_unblock(struct inode *inode, struct file *filp,
				 unsigned int cmd, unsigned long arg);
extern int	     drm_lock_take(__volatile__ unsigned int *lock,
				   unsigned int context);
extern int	     drm_lock_transfer(drm_device_t *dev,
				       __volatile__ unsigned int *lock,
				       unsigned int context);
extern int	     drm_lock_free(drm_device_t *dev,
				   __volatile__ unsigned int *lock,
				   unsigned int context);
extern int	     drm_finish(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long arg);
extern int	     drm_flush_unblock(drm_device_t *dev, int context,
				       drm_lock_flags_t flags);
extern int	     drm_flush_block_and_flush(drm_device_t *dev, int context,
					       drm_lock_flags_t flags);
extern int           drm_notifier(void *priv);

				/* Context Bitmap support (ctxbitmap.c) */
extern int	     drm_ctxbitmap_init(drm_device_t *dev);
extern void	     drm_ctxbitmap_cleanup(drm_device_t *dev);
extern int	     drm_ctxbitmap_next(drm_device_t *dev);
extern void	     drm_ctxbitmap_free(drm_device_t *dev, int ctx_handle);

#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
				/* AGP/GART support (agpsupport.c) */
extern drm_agp_head_t *drm_agp_init(void);
extern void           drm_agp_uninit(void);
extern int            drm_agp_acquire(struct inode *inode, struct file *filp,
				      unsigned int cmd, unsigned long arg);
extern void           _drm_agp_release(void);
extern int            drm_agp_release(struct inode *inode, struct file *filp,
				      unsigned int cmd, unsigned long arg);
extern int            drm_agp_enable(struct inode *inode, struct file *filp,
				     unsigned int cmd, unsigned long arg);
extern int            drm_agp_info(struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg);
extern int            drm_agp_alloc(struct inode *inode, struct file *filp,
				    unsigned int cmd, unsigned long arg);
extern int            drm_agp_free(struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg);
extern int            drm_agp_unbind(struct inode *inode, struct file *filp,
				     unsigned int cmd, unsigned long arg);
extern int            drm_agp_bind(struct inode *inode, struct file *filp,
				   unsigned int cmd, unsigned long arg);
extern agp_memory     *drm_agp_allocate_memory(size_t pages, u32 type);
extern int            drm_agp_free_memory(agp_memory *handle);
extern int            drm_agp_bind_memory(agp_memory *handle, off_t start);
extern int            drm_agp_unbind_memory(agp_memory *handle);
#endif
#endif
#endif
