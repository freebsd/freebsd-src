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
 *
 * $FreeBSD$
 */

#ifndef _DRM_P_H_
#define _DRM_P_H_

#if defined(_KERNEL) || defined(__KERNEL__)

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

#define DRM_DEBUG_CODE 0	  /* Include debugging code (if > 1, then
				     also include looping detection. */

typedef struct drm_device drm_device_t;
typedef struct drm_file drm_file_t;

/* There's undoubtably more of this file to go into these OS dependent ones. */
#ifdef __linux__
#include "dev/drm/drm_os_linux.h"
#endif /* __linux__ */

#ifdef __FreeBSD__
#include "dev/drm/drm_os_freebsd.h"
#endif /* __FreeBSD__ */

#include "dev/drm/drm.h"

/* Begin the DRM... */

#define DRM_HASH_SIZE	      16 /* Size of key hash table		  */
#define DRM_KERNEL_CONTEXT    0	 /* Change drm_resctx if changed	  */
#define DRM_RESERVED_CONTEXTS 1	 /* Change drm_resctx if changed	  */
#define DRM_LOOPING_LIMIT     5000000
#define DRM_BSZ		      1024 /* Buffer size for /dev/drm? output	  */
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
				/* _PAGE_WT changed to _PAGE_PWT in 2.2.6 */
#ifndef _PAGE_PWT
#define _PAGE_PWT _PAGE_WT
#endif

				/* Mapping helper macros */
#define DRM_IOREMAP(map)						\
	(map)->handle = DRM(ioremap)( (map)->offset, (map)->size )

#define DRM_IOREMAPFREE(map)						\
	do {								\
		if ( (map)->handle && (map)->size )			\
			DRM(ioremapfree)( (map)->handle, (map)->size );	\
	} while (0)

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

#ifdef __linux__
typedef int drm_ioctl_t( DRM_OS_IOCTL );
#endif /* __linux__ */

typedef struct drm_pci_list {
	u16 vendor;
	u16 device;
} drm_pci_list_t;

typedef struct drm_ioctl_desc {
#ifdef __linux__
	drm_ioctl_t	     *func;
#endif /* __linux__ */
#ifdef __FreeBSD__
	d_ioctl_t            *func;
#endif /* __FreeBSD__ */
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
	DRM_OS_SPINTYPE	  read_lock;
	DRM_OS_SPINTYPE	  write_lock;
} drm_waitlist_t;

typedef struct drm_freelist {
	int		  initialized; /* Freelist in use		   */
	atomic_t	  count;       /* Number of free buffers	   */
	drm_buf_t	  *next;       /* End pointer			   */

	wait_queue_head_t waiting;     /* Processes waiting on free bufs   */
	int		  low_mark;    /* Low water mark		   */
	int		  high_mark;   /* High water mark		   */
	atomic_t	  wfh;	       /* If waiting for high mark	   */
	DRM_OS_SPINTYPE   lock;
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

#ifdef __linux__
struct drm_file {
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
};
#endif /* __linux__ */
#ifdef __FreeBSD__
typedef TAILQ_HEAD(drm_file_list, drm_file) drm_file_list_t;
struct drm_file {
	TAILQ_ENTRY(drm_file) link;
	int		  authenticated;
	int		  minor;
	pid_t		  pid;
	uid_t		  uid;
	int		  refs;
	drm_magic_t	  magic;
	unsigned long	  ioctl_count;
	struct drm_device *devXX;
};
#endif /* __FreeBSD__ */

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
#ifdef __linux__
	unsigned long      handle;
	agp_memory         *memory;
#endif /* __linux__ */
#ifdef __FreeBSD__
	void               *handle;
#endif /* __FreeBSD__ */
	unsigned long      bound; /* address */
	int                pages;
	struct drm_agp_mem *prev;
	struct drm_agp_mem *next;
} drm_agp_mem_t;

typedef struct drm_agp_head {
#ifdef __linux__
	agp_kern_info      agp_info;
#endif /* __linux__ */
#ifdef __FreeBSD__
	device_t	   agpdev;
	struct agp_info    info;
#endif /* __FreeBSD__ */
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
#ifdef __linux__
	dma_addr_t	*busaddr;
#endif /* __linux__ */
} drm_sg_mem_t;

typedef struct drm_sigdata {
	int           context;
	drm_hw_lock_t *lock;
} drm_sigdata_t;

#ifdef __linux__
typedef struct drm_map_list {
	struct list_head	head;
	drm_map_t		*map;
} drm_map_list_t;
#endif /* __linux__ */
#ifdef __FreeBSD__
typedef TAILQ_HEAD(drm_map_list, drm_map_list_entry) drm_map_list_t;
typedef struct drm_map_list_entry {
	TAILQ_ENTRY(drm_map_list_entry) link;
	drm_map_t	*map;
} drm_map_list_entry_t;
#endif /* __FreeBSD__ */

struct drm_device {
	const char	  *name;	/* Simple driver name		   */
	char		  *unique;	/* Unique identifier: e.g., busid  */
	int		  unique_len;	/* Length of unique field	   */
#ifdef __linux__
	dev_t		  device;	/* Device number for mknod	   */
#endif /* __linux__ */
#ifdef __FreeBSD__
	device_t	  device;	/* Device instance from newbus     */
	dev_t		  devnode;	/* Device number for mknod	   */
#endif /* __FreeBSD__ */
	char		  *devname;	/* For /proc/interrupts		   */

	int		  blocked;	/* Blocked due to VC switch?	   */
#ifdef __FreeBSD__
	int		  flags;	/* Flags to open(2)		   */
	int		  writable;	/* Opened with FWRITE		   */
#endif /* __FreeBSD__ */
	struct proc_dir_entry *root;	/* Root for this device's entries  */

				/* Locks */
#ifdef __linux__
	spinlock_t	  count_lock;	/* For inuse, open_count, buf_use  */
	struct semaphore  struct_sem;	/* For others			   */
#endif /* __linux__ */
#ifdef __FreeBSD__
	DRM_OS_SPINTYPE	  count_lock;	/* For inuse, open_count, buf_use  */
	struct lock       dev_lock;	/* For others			   */
#endif /* __FreeBSD__ */
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
#ifdef __linux__
	drm_file_t	  *file_first;
	drm_file_t	  *file_last;
#endif /* __linux__ */
#ifdef __FreeBSD__
	drm_file_list_t   files;
#endif /* __FreeBSD__ */
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
#ifdef __FreeBSD__
	struct resource   *irqr;	/* Resource for interrupt used by board	   */
	void		  *irqh;	/* Handle from bus_setup_intr      */
#endif /* __FreeBSD__ */
	__volatile__ long context_flag;	/* Context swapping flag	   */
	__volatile__ long interrupt_flag; /* Interruption handler flag	   */
	__volatile__ long dma_flag;	/* DMA dispatch flag		   */
#ifdef __linux__
	struct timer_list timer;	/* Timer for delaying ctx switch   */
#endif /* __linux__ */
#ifdef __FreeBSD__
	struct callout    timer;	/* Timer for delaying ctx switch   */
#endif /* __FreeBSD__ */
	wait_queue_head_t context_wait; /* Processes waiting on ctx switch */
	int		  last_checked;	/* Last context checked for DMA	   */
	int		  last_context;	/* Last current context		   */
	unsigned long	  last_switch;	/* jiffies at last context switch  */
#ifdef __linux__
	struct tq_struct  tq;
#endif /* __linux__ */
#ifdef __FreeBSD__
#if __FreeBSD_version >= 400005
	struct task       task;
#endif
#endif /* __FreeBSD__ */
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
#ifdef __linux__
	struct fasync_struct *buf_async;/* Processes waiting for SIGIO	   */
#endif /* __linux__ */
#ifdef __FreeBSD__
	struct sigio      *buf_sigio;	/* Processes waiting for SIGIO     */
	struct selinfo    buf_sel;	/* Workspace for select/poll       */
	int               buf_selecting;/* True if poll sleeper            */
#endif /* __FreeBSD__ */
	wait_queue_head_t buf_readers;	/* Processes waiting to read	   */
	wait_queue_head_t buf_writers;	/* Processes waiting to ctx switch */

#ifdef __FreeBSD__
				/* Sysctl support */
	struct drm_sysctl_info *sysctl;
#endif /* __FreeBSD__ */

#if __REALLY_HAVE_AGP
	drm_agp_head_t    *agp;
#endif
	struct pci_dev *pdev;
#ifdef __alpha__
#if LINUX_VERSION_CODE < 0x020403
	struct pci_controler *hose;
#else
	struct pci_controller *hose;
#endif
#endif
	drm_sg_mem_t      *sg;  /* Scatter gather memory */
	unsigned long     *ctx_bitmap;
	void		  *dev_private;
	drm_sigdata_t     sigdata; /* For block_all_signals */
	sigset_t          sigmask;
};

extern int	     DRM(flags);
extern void	     DRM(parse_options)( char *s );
extern int           DRM(cpu_valid)( void );

				/* Authentication (drm_auth.h) */
extern int           DRM(add_magic)(drm_device_t *dev, drm_file_t *priv, 
				    drm_magic_t magic);
extern int           DRM(remove_magic)(drm_device_t *dev, drm_magic_t magic);

				/* Driver support (drm_drv.h) */
extern int           DRM(version)( DRM_OS_IOCTL );
extern int	     DRM(write_string)(drm_device_t *dev, const char *s);

				/* Memory management support (drm_memory.h) */
extern void	     DRM(mem_init)(void);
extern void	     *DRM(alloc)(size_t size, int area);
extern void	     *DRM(realloc)(void *oldpt, size_t oldsize, size_t size,
				   int area);
extern char	     *DRM(strdup)(const char *s, int area);
extern void	     DRM(strfree)(char *s, int area);
extern void	     DRM(free)(void *pt, size_t size, int area);
extern unsigned long DRM(alloc_pages)(int order, int area);
extern void	     DRM(free_pages)(unsigned long address, int order,
				     int area);
extern void	     *DRM(ioremap)(unsigned long offset, unsigned long size);
extern void	     DRM(ioremapfree)(void *pt, unsigned long size);

#if __REALLY_HAVE_AGP
extern agp_memory    *DRM(alloc_agp)(int pages, u32 type);
extern int           DRM(free_agp)(agp_memory *handle, int pages);
extern int           DRM(bind_agp)(agp_memory *handle, unsigned int start);
extern int           DRM(unbind_agp)(agp_memory *handle);
#endif

extern int	     DRM(context_switch)(drm_device_t *dev, int old, int new);
extern int	     DRM(context_switch_complete)(drm_device_t *dev, int new);

#if __HAVE_CTX_BITMAP
extern int	     DRM(ctxbitmap_init)( drm_device_t *dev );
extern void	     DRM(ctxbitmap_cleanup)( drm_device_t *dev );
extern void          DRM(ctxbitmap_free)( drm_device_t *dev, int ctx_handle );
extern int           DRM(ctxbitmap_next)( drm_device_t *dev );
#endif

				/* Locking IOCTL support (drm_lock.h) */
extern int	     DRM(lock_take)(__volatile__ unsigned int *lock,
				    unsigned int context);
extern int	     DRM(lock_transfer)(drm_device_t *dev,
					__volatile__ unsigned int *lock,
					unsigned int context);
extern int	     DRM(lock_free)(drm_device_t *dev,
				    __volatile__ unsigned int *lock,
				    unsigned int context);
extern int	     DRM(flush_unblock)(drm_device_t *dev, int context,
					drm_lock_flags_t flags);
extern int	     DRM(flush_block_and_flush)(drm_device_t *dev, int context,
						drm_lock_flags_t flags);
extern int           DRM(notifier)(void *priv);

				/* Buffer management support (drm_bufs.h) */
extern int	     DRM(order)( unsigned long size );

#if __HAVE_DMA
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
extern int           DRM(irq_install)( drm_device_t *dev, int irq );
extern int           DRM(irq_uninstall)( drm_device_t *dev );
extern void          DRM(dma_service)( DRM_OS_IRQ_ARGS );
#if __HAVE_DMA_IRQ_BH
extern void          DRM(dma_immediate_bh)( DRM_OS_TASKQUEUE_ARGS );
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
extern void           DRM(agp_do_release)(void);
extern agp_memory     *DRM(agp_allocate_memory)(size_t pages, u32 type);
extern int            DRM(agp_free_memory)(agp_memory *handle);
extern int            DRM(agp_bind_memory)(agp_memory *handle, off_t start);
extern int            DRM(agp_unbind_memory)(agp_memory *handle);
#endif

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
#endif

#if __REALLY_HAVE_SG
                               /* ATI PCIGART support (ati_pcigart.h) */
extern int            DRM(ati_pcigart_init)(drm_device_t *dev,
					    unsigned long *addr,
					    dma_addr_t *bus_addr);
extern int            DRM(ati_pcigart_cleanup)(drm_device_t *dev,
					       unsigned long addr,
					       dma_addr_t bus_addr);
#endif

#endif /* __KERNEL__ */
#endif
