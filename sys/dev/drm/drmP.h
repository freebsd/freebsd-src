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

#define DRM_DEBUG_CODE 0	  /* Include debugging code (if > 1, then
				     also include looping detection. */

typedef struct drm_device drm_device_t;
typedef struct drm_file drm_file_t;

/* There's undoubtably more of this file to go into these OS dependent ones. */

#ifdef __FreeBSD__
#include "dev/drm/drm_os_freebsd.h"
#elif defined __NetBSD__
#include "dev/drm/drm_os_netbsd.h"
#endif

#include "dev/drm/drm.h"

/* Begin the DRM... */

#define DRM_HASH_SIZE	      16 /* Size of key hash table		  */
#define DRM_KERNEL_CONTEXT    0	 /* Change drm_resctx if changed	  */
#define DRM_RESERVED_CONTEXTS 1	 /* Change drm_resctx if changed	  */

#define DRM_FLAG_DEBUG	  0x01

#define DRM_MEM_DMA	   0
#define DRM_MEM_SAREA	   1
#define DRM_MEM_DRIVER	   2
#define DRM_MEM_MAGIC	   3
#define DRM_MEM_IOCTLS	   4
#define DRM_MEM_MAPS	   5
#define DRM_MEM_BUFS	   6
#define DRM_MEM_SEGS	   7
#define DRM_MEM_PAGES	   8
#define DRM_MEM_FILES	  9
#define DRM_MEM_QUEUES	  10
#define DRM_MEM_CMDS	  11
#define DRM_MEM_MAPPINGS  12
#define DRM_MEM_BUFLISTS  13
#define DRM_MEM_AGPLISTS  14
#define DRM_MEM_TOTALAGP  15
#define DRM_MEM_BOUNDAGP  16
#define DRM_MEM_CTXBITMAP 17
#define DRM_MEM_STUB	  18
#define DRM_MEM_SGLISTS	  19

#define DRM_MAX_CTXBITMAP (PAGE_SIZE * 8)

				/* Mapping helper macros */
#define DRM_IOREMAP(map)						\
	(map)->handle = DRM(ioremap)( dev, map )

#define DRM_IOREMAP_NOCACHE(map)					\
	(map)->handle = DRM(ioremap_nocache)( dev, map )

#define DRM_IOREMAPFREE(map)						\
	do {								\
		if ( (map)->handle && (map)->size )			\
			DRM(ioremapfree)( map );			\
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


typedef struct drm_ioctl_desc {
	int		     (*func)(DRM_IOCTL_ARGS);
	int		     auth_needed;
	int		     root_only;
} drm_ioctl_desc_t;

typedef struct drm_magic_entry {
	drm_magic_t	       magic;
	struct drm_file	       *priv;
	struct drm_magic_entry *next;
} drm_magic_entry_t;

typedef struct drm_magic_head {
	struct drm_magic_entry *head;
	struct drm_magic_entry *tail;
} drm_magic_head_t;

typedef struct drm_buf {
	int		  idx;	       /* Index into master buflist	     */
	int		  total;       /* Buffer size			     */
	int		  order;       /* log-base-2(total)		     */
	int		  used;	       /* Amount of buffer in use (for DMA)  */
	unsigned long	  offset;      /* Byte offset (used internally)	     */
	void		  *address;    /* Address of buffer		     */
	unsigned long	  bus_address; /* Bus address of buffer		     */
	struct drm_buf	  *next;       /* Kernel-only: used for free list    */
	__volatile__ int  pending;     /* On hardware DMA queue		     */
	DRMFILE		  filp;	       /* Unique identifier of holding process */
	int		  context;     /* Kernel queue for this buffer	     */
	enum {
		DRM_LIST_NONE	 = 0,
		DRM_LIST_FREE	 = 1,
		DRM_LIST_WAIT	 = 2,
		DRM_LIST_PEND	 = 3,
		DRM_LIST_PRIO	 = 4,
		DRM_LIST_RECLAIM = 5
	}		  list;	       /* Which list we're on		     */

	int		  dev_priv_size; /* Size of buffer private stoarge   */
	void		  *dev_private;  /* Per-buffer private storage       */
} drm_buf_t;

				/* bufs is one longer than it has to be */
typedef struct drm_waitlist {
	int		  count;	/* Number of possible buffers	   */
	drm_buf_t	  **bufs;	/* List of pointers to buffers	   */
	drm_buf_t	  **rp;		/* Read pointer			   */
	drm_buf_t	  **wp;		/* Write pointer		   */
	drm_buf_t	  **end;	/* End pointer			   */
	DRM_SPINTYPE	  read_lock;
	DRM_SPINTYPE	  write_lock;
} drm_waitlist_t;

typedef struct drm_freelist {
	int		  initialized; /* Freelist in use		   */
	atomic_t	  count;       /* Number of free buffers	   */
	drm_buf_t	  *next;       /* End pointer			   */

	int		  low_mark;    /* Low water mark		   */
	int		  high_mark;   /* High water mark		   */
	DRM_SPINTYPE   lock;
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

typedef struct drm_lock_data {
	drm_hw_lock_t	  *hw_lock;	/* Hardware lock		   */
	DRMFILE		  filp;	        /* Unique identifier of holding process (NULL is kernel)*/
	wait_queue_head_t lock_queue;	/* Queue of blocked processes	   */
	unsigned long	  lock_time;	/* Time of last lock in jiffies	   */
} drm_lock_data_t;

typedef struct drm_device_dma {
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
} drm_device_dma_t;

#if __REALLY_HAVE_AGP
typedef struct drm_agp_mem {
	void               *handle;
	unsigned long      bound; /* address */
	int                pages;
	struct drm_agp_mem *prev;
	struct drm_agp_mem *next;
} drm_agp_mem_t;

typedef struct drm_agp_head {
	device_t	   agpdev;
	struct agp_info    info;
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
	dma_addr_t	*busaddr;
} drm_sg_mem_t;

typedef struct drm_local_map {
	unsigned long	offset;	 /* Physical address (0 for SAREA)*/
	unsigned long	size;	 /* Physical size (bytes)	    */
	drm_map_type_t	type;	 /* Type of memory mapped		    */
	drm_map_flags_t flags;	 /* Flags				    */
	void		*handle; /* User-space: "Handle" to pass to mmap    */
				 /* Kernel-space: kernel-virtual address    */
	int		mtrr;	 /* MTRR slot used			    */
				 /* Private data			    */
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
} drm_local_map_t;

typedef TAILQ_HEAD(drm_map_list, drm_map_list_entry) drm_map_list_t;
typedef struct drm_map_list_entry {
	TAILQ_ENTRY(drm_map_list_entry) link;
	drm_local_map_t	*map;
} drm_map_list_entry_t;

TAILQ_HEAD(drm_vbl_sig_list, drm_vbl_sig);
typedef struct drm_vbl_sig {
	TAILQ_ENTRY(drm_vbl_sig) link;
	unsigned int	sequence;
	int		signo;
	int		pid;
} drm_vbl_sig_t;

struct drm_device {
#ifdef __NetBSD__
	struct device	  device;	/* NetBSD's softc is an extension of struct device */
#endif
	const char	  *name;	/* Simple driver name		   */
	char		  *unique;	/* Unique identifier: e.g., busid  */
	int		  unique_len;	/* Length of unique field	   */
#ifdef __FreeBSD__
	device_t	  device;	/* Device instance from newbus     */
#endif
	dev_t		  devnode;	/* Device number for mknod	   */

	int		  flags;	/* Flags to open(2)		   */

				/* Locks */
	DRM_SPINTYPE	  count_lock;	/* For open_count, buf_use, buf_alloc */
	struct lock       dev_lock;	/* For others			   */
				/* Usage Counters */
	int		  open_count;	/* Outstanding files open	   */
	int		  buf_use;	/* Buffers in use -- cannot alloc  */
	int		  buf_alloc;	/* Buffer allocation in progress   */

				/* Performance counters */
	unsigned long     counters;
	drm_stat_type_t   types[15];
	atomic_t          counts[15];

				/* Authentication */
	drm_file_list_t   files;
	drm_magic_head_t  magiclist[DRM_HASH_SIZE];

				/* Memory management */
	drm_map_list_t	  *maplist;	/* Linked list of regions	   */

	drm_local_map_t	  **context_sareas;
	int		  max_context;

	drm_lock_data_t	  lock;		/* Information on hardware lock	   */

				/* DMA queues (contexts) */
	drm_device_dma_t  *dma;		/* Optional pointer for DMA support */

				/* Context support */
	int		  irq;		/* Interrupt used by board	   */
	int		  irqrid;		/* Interrupt used by board	   */
#ifdef __FreeBSD__
	struct resource   *irqr;	/* Resource for interrupt used by board	   */
#elif defined(__NetBSD__)
	struct pci_attach_args  pa;
	pci_intr_handle_t	ih;
#endif
	void		  *irqh;	/* Handle from bus_setup_intr      */
	atomic_t	  context_flag;	/* Context swapping flag	   */
	struct callout    timer;	/* Timer for delaying ctx switch   */
	int		  last_context;	/* Last current context		   */
#if __FreeBSD_version >= 400005
	struct task       task;
#endif
#if __HAVE_VBL_IRQ
   	wait_queue_head_t vbl_queue;	/* vbl wait channel */
   	atomic_t          vbl_received;
#if 0 /* vbl signals are untested */
	struct drm_vbl_sig_list vbl_sig_list;
	DRM_SPINTYPE      vbl_lock;
#endif
#endif

#ifdef __FreeBSD__
	struct sigio      *buf_sigio;	/* Processes waiting for SIGIO     */
#elif defined(__NetBSD__)
	pid_t		  buf_pgid;
#endif

				/* Sysctl support */
	struct drm_sysctl_info *sysctl;

#if __REALLY_HAVE_AGP
	drm_agp_head_t    *agp;
#endif
	drm_sg_mem_t      *sg;  /* Scatter gather memory */
	atomic_t          *ctx_bitmap;
	void		  *dev_private;
};

extern int	     DRM(flags);

				/* Authentication (drm_auth.h) */
extern int           DRM(add_magic)(drm_device_t *dev, drm_file_t *priv, 
				    drm_magic_t magic);
extern int           DRM(remove_magic)(drm_device_t *dev, drm_magic_t magic);

				/* Driver support (drm_drv.h) */
extern int           DRM(version)( DRM_IOCTL_ARGS );

				/* Memory management support (drm_memory.h) */
extern void	     DRM(mem_init)(void);
extern void	     DRM(mem_uninit)(void);
extern void	     *DRM(alloc)(size_t size, int area);
extern void	     *DRM(realloc)(void *oldpt, size_t oldsize, size_t size,
				   int area);
extern char	     *DRM(strdup)(const char *s, int area);
extern void	     DRM(strfree)(char *s, int area);
extern void	     DRM(free)(void *pt, size_t size, int area);
extern void	     *DRM(ioremap)(drm_device_t *dev, drm_local_map_t *map);
extern void	     *DRM(ioremap_nocache)(drm_device_t *dev, drm_local_map_t *map);
extern void	     DRM(ioremapfree)(drm_local_map_t *map);

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

				/* Buffer management support (drm_bufs.h) */
extern int	     DRM(order)( unsigned long size );

#if __HAVE_DMA
				/* DMA support (drm_dma.h) */
extern int	     DRM(dma_setup)(drm_device_t *dev);
extern void	     DRM(dma_takedown)(drm_device_t *dev);
extern void	     DRM(free_buffer)(drm_device_t *dev, drm_buf_t *buf);
extern void	     DRM(reclaim_buffers)(drm_device_t *dev, DRMFILE filp);
#if __HAVE_DMA_IRQ
extern int           DRM(irq_install)( drm_device_t *dev, int irq );
extern int           DRM(irq_uninstall)( drm_device_t *dev );
extern void          DRM(dma_service)( DRM_IRQ_ARGS );
extern void          DRM(driver_irq_preinstall)( drm_device_t *dev );
extern void          DRM(driver_irq_postinstall)( drm_device_t *dev );
extern void          DRM(driver_irq_uninstall)( drm_device_t *dev );
#if __HAVE_DMA_IRQ_BH
extern void          DRM(dma_immediate_bh)( DRM_TASKQUEUE_ARGS );
#endif
#endif

				/* Buffer list support (drm_lists.h) */
#if __HAVE_DMA_WAITLIST
extern int	     DRM(waitlist_create)(drm_waitlist_t *bl, int count);
extern int	     DRM(waitlist_destroy)(drm_waitlist_t *bl);
extern int	     DRM(waitlist_put)(drm_waitlist_t *bl, drm_buf_t *buf);
extern drm_buf_t     *DRM(waitlist_get)(drm_waitlist_t *bl);
#endif
#endif /* __HAVE_DMA */
#if __HAVE_VBL_IRQ
extern int           DRM(vblank_wait)(drm_device_t *dev, unsigned int *vbl_seq);
extern void          DRM(vbl_send_signals)( drm_device_t *dev );
#endif

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

/* Locking IOCTL support (drm_drv.h) */
extern int		DRM(lock)(DRM_IOCTL_ARGS);
extern int		DRM(unlock)(DRM_IOCTL_ARGS);

/* Misc. IOCTL support (drm_ioctl.h) */
extern int		DRM(irq_busid)(DRM_IOCTL_ARGS);
extern int		DRM(getunique)(DRM_IOCTL_ARGS);
extern int		DRM(setunique)(DRM_IOCTL_ARGS);
extern int		DRM(getmap)(DRM_IOCTL_ARGS);
extern int		DRM(getclient)(DRM_IOCTL_ARGS);
extern int		DRM(getstats)(DRM_IOCTL_ARGS);
extern int		DRM(noop)(DRM_IOCTL_ARGS);

/* Context IOCTL support (drm_context.h) */
extern int		DRM(resctx)(DRM_IOCTL_ARGS);
extern int		DRM(addctx)(DRM_IOCTL_ARGS);
extern int		DRM(modctx)(DRM_IOCTL_ARGS);
extern int		DRM(getctx)(DRM_IOCTL_ARGS);
extern int		DRM(switchctx)(DRM_IOCTL_ARGS);
extern int		DRM(newctx)(DRM_IOCTL_ARGS);
extern int		DRM(rmctx)(DRM_IOCTL_ARGS);
extern int		DRM(setsareactx)(DRM_IOCTL_ARGS);
extern int		DRM(getsareactx)(DRM_IOCTL_ARGS);

/* Drawable IOCTL support (drm_drawable.h) */
extern int		DRM(adddraw)(DRM_IOCTL_ARGS);
extern int		DRM(rmdraw)(DRM_IOCTL_ARGS);

/* Authentication IOCTL support (drm_auth.h) */
extern int		DRM(getmagic)(DRM_IOCTL_ARGS);
extern int		DRM(authmagic)(DRM_IOCTL_ARGS);

/* Buffer management support (drm_bufs.h) */
extern int		DRM(addmap)(DRM_IOCTL_ARGS);
extern int		DRM(rmmap)(DRM_IOCTL_ARGS);
#if __HAVE_DMA
extern int		DRM(addbufs)(DRM_IOCTL_ARGS);
extern int		DRM(infobufs)(DRM_IOCTL_ARGS);
extern int		DRM(markbufs)(DRM_IOCTL_ARGS);
extern int		DRM(freebufs)(DRM_IOCTL_ARGS);
extern int		DRM(mapbufs)(DRM_IOCTL_ARGS);
#endif

/* DMA support (drm_dma.h) */
#if __HAVE_DMA
extern int		DRM(control)(DRM_IOCTL_ARGS);
#endif
#if __HAVE_VBL_IRQ
extern int		DRM(wait_vblank)(DRM_IOCTL_ARGS);
#endif

/* AGP/GART support (drm_agpsupport.h) */
#if __REALLY_HAVE_AGP
extern int		DRM(agp_acquire)(DRM_IOCTL_ARGS);
extern int		DRM(agp_release)(DRM_IOCTL_ARGS);
extern int		DRM(agp_enable)(DRM_IOCTL_ARGS);
extern int		DRM(agp_info)(DRM_IOCTL_ARGS);
extern int		DRM(agp_alloc)(DRM_IOCTL_ARGS);
extern int		DRM(agp_free)(DRM_IOCTL_ARGS);
extern int		DRM(agp_unbind)(DRM_IOCTL_ARGS);
extern int		DRM(agp_bind)(DRM_IOCTL_ARGS);
#endif

/* Scatter Gather Support (drm_scatter.h) */
#if __HAVE_SG
extern int		DRM(sg_alloc)(DRM_IOCTL_ARGS);
extern int		DRM(sg_free)(DRM_IOCTL_ARGS);
#endif


#endif /* __KERNEL__ */
#endif /* _DRM_P_H_ */
