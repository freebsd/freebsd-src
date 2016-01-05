/* drmP.h -- Private header for Direct Rendering Manager -*- linux-c -*-
 * Created: Mon Jan  4 10:05:05 1999 by faith@precisioninsight.com
 */
/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef _DRM_P_H_
#define _DRM_P_H_

#if defined(_KERNEL) || defined(__KERNEL__)

struct drm_device;
struct drm_file;

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/sglist.h>
#include <sys/stat.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/filio.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <sys/signalvar.h>
#include <sys/poll.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>
#include <machine/param.h>
#include <machine/pmap.h>
#include <machine/bus.h>
#include <machine/resource.h>
#if defined(__i386__) || defined(__amd64__)
#include <machine/specialreg.h>
#endif
#include <machine/sysarch.h>
#include <sys/endian.h>
#if _BYTE_ORDER == _BIG_ENDIAN
#define __BIG_ENDIAN 1
#else
#define __LITTLE_ENDIAN 1
#endif
#include <sys/mman.h>
#include <sys/rman.h>
#include <sys/memrange.h>
#include <dev/agp/agpvar.h>
#include <sys/agpio.h>
#include <sys/mutex.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <sys/selinfo.h>
#include <sys/bus.h>

#include "dev/drm/drm.h"
#include "dev/drm/drm_atomic.h"
#include "dev/drm/drm_internal.h"
#include "dev/drm/drm_linux_list.h"

#include <opt_drm.h>
#ifdef DRM_DEBUG
#undef DRM_DEBUG
#define DRM_DEBUG_DEFAULT_ON 1
#endif /* DRM_DEBUG */

#if defined(DRM_LINUX) && DRM_LINUX && !defined(__amd64__)
#include <sys/file.h>
#include <sys/proc.h>
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#else
/* Either it was defined when it shouldn't be (FreeBSD amd64) or it isn't
 * supported on this OS yet.
 */
#undef DRM_LINUX
#define DRM_LINUX 0
#endif

/* driver capabilities and requirements mask */
#define DRIVER_USE_AGP     0x1
#define DRIVER_REQUIRE_AGP 0x2
#define DRIVER_USE_MTRR    0x4
#define DRIVER_PCI_DMA     0x8
#define DRIVER_SG          0x10
#define DRIVER_HAVE_DMA    0x20
#define DRIVER_HAVE_IRQ    0x40
#define DRIVER_DMA_QUEUE   0x100


#define DRM_HASH_SIZE	      16 /* Size of key hash table		  */
#define DRM_KERNEL_CONTEXT    0	 /* Change drm_resctx if changed	  */
#define DRM_RESERVED_CONTEXTS 1	 /* Change drm_resctx if changed	  */

MALLOC_DECLARE(DRM_MEM_DMA);
MALLOC_DECLARE(DRM_MEM_SAREA);
MALLOC_DECLARE(DRM_MEM_DRIVER);
MALLOC_DECLARE(DRM_MEM_MAGIC);
MALLOC_DECLARE(DRM_MEM_IOCTLS);
MALLOC_DECLARE(DRM_MEM_MAPS);
MALLOC_DECLARE(DRM_MEM_BUFS);
MALLOC_DECLARE(DRM_MEM_SEGS);
MALLOC_DECLARE(DRM_MEM_PAGES);
MALLOC_DECLARE(DRM_MEM_FILES);
MALLOC_DECLARE(DRM_MEM_QUEUES);
MALLOC_DECLARE(DRM_MEM_CMDS);
MALLOC_DECLARE(DRM_MEM_MAPPINGS);
MALLOC_DECLARE(DRM_MEM_BUFLISTS);
MALLOC_DECLARE(DRM_MEM_AGPLISTS);
MALLOC_DECLARE(DRM_MEM_CTXBITMAP);
MALLOC_DECLARE(DRM_MEM_SGLISTS);
MALLOC_DECLARE(DRM_MEM_DRAWABLE);
MALLOC_DECLARE(DRM_MEM_MM);
MALLOC_DECLARE(DRM_MEM_HASHTAB);

SYSCTL_DECL(_hw_drm);

#define DRM_MAX_CTXBITMAP (PAGE_SIZE * 8)

				/* Internal types and structures */
#define DRM_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define DRM_MIN(a,b) ((a)<(b)?(a):(b))
#define DRM_MAX(a,b) ((a)>(b)?(a):(b))

#define DRM_IF_VERSION(maj, min) (maj << 16 | min)

#define __OS_HAS_AGP	1

#define DRM_DEV_MODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)
#define DRM_DEV_UID	UID_ROOT
#define DRM_DEV_GID	GID_VIDEO

#define wait_queue_head_t	atomic_t
#define DRM_WAKEUP(w)		wakeup((void *)w)
#define DRM_WAKEUP_INT(w)	wakeup(w)
#define DRM_INIT_WAITQUEUE(queue) do {(void)(queue);} while (0)

#define DRM_CURPROC		curthread
#define DRM_STRUCTPROC		struct thread
#define DRM_SPINTYPE		struct mtx
#define DRM_SPININIT(l,name)	mtx_init(l, name, NULL, MTX_DEF)
#define DRM_SPINUNINIT(l)	mtx_destroy(l)
#define DRM_SPINLOCK(l)		mtx_lock(l)
#define DRM_SPINUNLOCK(u)	mtx_unlock(u)
#define DRM_SPINLOCK_IRQSAVE(l, irqflags) do {		\
	mtx_lock(l);					\
	(void)irqflags;					\
} while (0)
#define DRM_SPINUNLOCK_IRQRESTORE(u, irqflags) mtx_unlock(u)
#define DRM_SPINLOCK_ASSERT(l)	mtx_assert(l, MA_OWNED)
#define DRM_CURRENTPID		curthread->td_proc->p_pid
#define DRM_LOCK()		mtx_lock(&dev->dev_lock)
#define DRM_UNLOCK() 		mtx_unlock(&dev->dev_lock)
#define DRM_SYSCTL_HANDLER_ARGS	(SYSCTL_HANDLER_ARGS)

#define DRM_IRQ_ARGS		void *arg
typedef void			irqreturn_t;
#define IRQ_HANDLED		/* nothing */
#define IRQ_NONE		/* nothing */

#define unlikely(x)            __builtin_expect(!!(x), 0)
#define container_of(ptr, type, member) ({			\
	__typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

enum {
	DRM_IS_NOT_AGP,
	DRM_IS_AGP,
	DRM_MIGHT_BE_AGP
};
#define DRM_AGP_MEM		struct agp_memory_info

#define drm_get_device_from_kdev(_kdev) (_kdev->si_drv1)

#define PAGE_ALIGN(addr) round_page(addr)
/* DRM_SUSER returns true if the user is superuser */
#define DRM_SUSER(p)		(priv_check(p, PRIV_DRIVER) == 0)
#define DRM_AGP_FIND_DEVICE()	agp_find_device()
#define DRM_MTRR_WC		MDF_WRITECOMBINE
#define jiffies			ticks

typedef vm_paddr_t dma_addr_t;
typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

/* DRM_READMEMORYBARRIER() prevents reordering of reads.
 * DRM_WRITEMEMORYBARRIER() prevents reordering of writes.
 * DRM_MEMORYBARRIER() prevents reordering of reads and writes.
 */
#define DRM_READMEMORYBARRIER()		rmb()
#define DRM_WRITEMEMORYBARRIER()	wmb()
#define DRM_MEMORYBARRIER()		mb()

#define DRM_READ8(map, offset)						\
	*(volatile u_int8_t *)(((vm_offset_t)(map)->virtual) +		\
	    (vm_offset_t)(offset))
#define DRM_READ16(map, offset)						\
	le16toh(*(volatile u_int16_t *)(((vm_offset_t)(map)->virtual) +	\
	    (vm_offset_t)(offset)))
#define DRM_READ32(map, offset)						\
	le32toh(*(volatile u_int32_t *)(((vm_offset_t)(map)->virtual) +	\
	    (vm_offset_t)(offset)))
#define DRM_WRITE8(map, offset, val)					\
	*(volatile u_int8_t *)(((vm_offset_t)(map)->virtual) +		\
	    (vm_offset_t)(offset)) = val
#define DRM_WRITE16(map, offset, val)					\
	*(volatile u_int16_t *)(((vm_offset_t)(map)->virtual) +		\
	    (vm_offset_t)(offset)) = htole16(val)
#define DRM_WRITE32(map, offset, val)					\
	*(volatile u_int32_t *)(((vm_offset_t)(map)->virtual) +		\
	    (vm_offset_t)(offset)) = htole32(val)

#define DRM_VERIFYAREA_READ( uaddr, size )		\
	(!useracc(__DECONST(caddr_t, uaddr), size, VM_PROT_READ))

#define DRM_COPY_TO_USER(user, kern, size) \
	copyout(kern, user, size)
#define DRM_COPY_FROM_USER(kern, user, size) \
	copyin(user, kern, size)
#define DRM_COPY_FROM_USER_UNCHECKED(arg1, arg2, arg3) 	\
	copyin(arg2, arg1, arg3)
#define DRM_COPY_TO_USER_UNCHECKED(arg1, arg2, arg3)	\
	copyout(arg2, arg1, arg3)
#define DRM_GET_USER_UNCHECKED(val, uaddr)		\
	((val) = fuword32(uaddr), 0)

#define cpu_to_le32(x) htole32(x)
#define le32_to_cpu(x) le32toh(x)

#define DRM_HZ			hz
#define DRM_UDELAY(udelay)	DELAY(udelay)
#define DRM_TIME_SLICE		(hz/20)  /* Time slice for GLXContexts	  */

#define DRM_GET_PRIV_SAREA(_dev, _ctx, _map) do {	\
	(_map) = (_dev)->context_sareas[_ctx];		\
} while(0)

#define LOCK_TEST_WITH_RETURN(dev, file_priv)				\
do {									\
	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock) ||		\
	     dev->lock.file_priv != file_priv) {			\
		DRM_ERROR("%s called without lock held\n",		\
			   __FUNCTION__);				\
		return EINVAL;						\
	}								\
} while (0)

/* Returns -errno to shared code */
#define DRM_WAIT_ON( ret, queue, timeout, condition )		\
for ( ret = 0 ; !ret && !(condition) ; ) {			\
	DRM_UNLOCK();						\
	mtx_lock(&dev->irq_lock);				\
	if (!(condition))					\
	    ret = -mtx_sleep(&(queue), &dev->irq_lock, 		\
		PCATCH, "drmwtq", (timeout));			\
	mtx_unlock(&dev->irq_lock);				\
	DRM_LOCK();						\
}

#define DRM_ERROR(fmt, ...) \
	printf("error: [" DRM_NAME ":pid%d:%s] *ERROR* " fmt,		\
	    DRM_CURRENTPID, __func__ , ##__VA_ARGS__)

#define DRM_INFO(fmt, ...)  printf("info: [" DRM_NAME "] " fmt , ##__VA_ARGS__)

#define DRM_DEBUG(fmt, ...) do {					\
	if (drm_debug_flag)						\
		printf("[" DRM_NAME ":pid%d:%s] " fmt, DRM_CURRENTPID,	\
			__func__ , ##__VA_ARGS__);			\
} while (0)

typedef struct drm_pci_id_list
{
	int vendor;
	int device;
	long driver_private;
	char *name;
} drm_pci_id_list_t;

struct drm_msi_blacklist_entry
{
	int vendor;
	int device;
};

#define DRM_AUTH	0x1
#define DRM_MASTER	0x2
#define DRM_ROOT_ONLY	0x4
typedef struct drm_ioctl_desc {
	unsigned long cmd;
	int (*func)(struct drm_device *dev, void *data,
		    struct drm_file *file_priv);
	int flags;
} drm_ioctl_desc_t;
/**
 * Creates a driver or general drm_ioctl_desc array entry for the given
 * ioctl, for use by drm_ioctl().
 */
#define DRM_IOCTL_DEF(ioctl, func, flags) \
	[DRM_IOCTL_NR(ioctl)] = {ioctl, func, flags}

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
	struct drm_file   *file_priv;  /* Unique identifier of holding process */
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

typedef struct drm_freelist {
	int		  initialized; /* Freelist in use		   */
	atomic_t	  count;       /* Number of free buffers	   */
	drm_buf_t	  *next;       /* End pointer			   */

	int		  low_mark;    /* Low water mark		   */
	int		  high_mark;   /* High water mark		   */
} drm_freelist_t;

typedef struct drm_dma_handle {
	void *vaddr;
	bus_addr_t busaddr;
	bus_dma_tag_t tag;
	bus_dmamap_t map;
} drm_dma_handle_t;

typedef struct drm_buf_entry {
	int		  buf_size;
	int		  buf_count;
	drm_buf_t	  *buflist;
	int		  seg_count;
	drm_dma_handle_t  **seglist;
	int		  page_order;

	drm_freelist_t	  freelist;
} drm_buf_entry_t;

typedef TAILQ_HEAD(drm_file_list, drm_file) drm_file_list_t;
struct drm_file {
	TAILQ_ENTRY(drm_file) link;
	struct drm_device *dev;
	int		  authenticated;
	int		  master;
	pid_t		  pid;
	uid_t		  uid;
	drm_magic_t	  magic;
	unsigned long	  ioctl_count;
	void		 *driver_priv;
};

typedef struct drm_lock_data {
	struct drm_hw_lock	*hw_lock;	/* Hardware lock		   */
	struct drm_file   *file_priv;   /* Unique identifier of holding process (NULL is kernel)*/
	int		  lock_queue;	/* Queue of blocked processes	   */
	unsigned long	  lock_time;	/* Time of last lock in jiffies	   */
} drm_lock_data_t;

/* This structure, in the struct drm_device, is always initialized while the
 * device
 * is open.  dev->dma_lock protects the incrementing of dev->buf_use, which
 * when set marks that no further bufs may be allocated until device teardown
 * occurs (when the last open of the device has closed).  The high/low
 * watermarks of bufs are only touched by the X Server, and thus not
 * concurrently accessed, so no locking is needed.
 */
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
} drm_device_dma_t;

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
   	int 		   mtrr;
	int		   cant_use_aperture;
	unsigned long	   page_mask;
} drm_agp_head_t;

typedef struct drm_sg_mem {
	vm_offset_t vaddr;
	vm_paddr_t *busaddr;
	vm_pindex_t pages;
} drm_sg_mem_t;

#define DRM_MAP_HANDLE_BITS	(sizeof(void *) == 4 ? 4 : 24)
#define DRM_MAP_HANDLE_SHIFT	(sizeof(void *) * 8 - DRM_MAP_HANDLE_BITS)
typedef TAILQ_HEAD(drm_map_list, drm_local_map) drm_map_list_t;

typedef struct drm_local_map {
	unsigned long offset;	  /* Physical address (0 for SAREA)       */
	unsigned long size;	  /* Physical size (bytes)                */
	enum drm_map_type type;	  /* Type of memory mapped                */
	enum drm_map_flags flags; /* Flags                                */
	void *handle;		  /* User-space: "Handle" to pass to mmap */
				  /* Kernel-space: kernel-virtual address */
	int mtrr;		  /* Boolean: MTRR used                   */
				  /* Private data                         */
	int rid;		  /* PCI resource ID for bus_space        */
	void *virtual;		  /* Kernel-space: kernel-virtual address */
	struct resource *bsr;
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	drm_dma_handle_t *dmah;
	TAILQ_ENTRY(drm_local_map) link;
} drm_local_map_t;

struct drm_vblank_info {
	wait_queue_head_t queue;	/* vblank wait queue */
	atomic_t count;			/* number of VBLANK interrupts */
					/* (driver must alloc the right number of counters) */
	atomic_t refcount;		/* number of users of vblank interrupts */
	u32 last;			/* protected by dev->vbl_lock, used */
					/* for wraparound handling */
	int enabled;			/* so we don't call enable more than */
					/* once per disable */
	int inmodeset;			/* Display driver is setting mode */
};

/* location of GART table */
#define DRM_ATI_GART_MAIN 1
#define DRM_ATI_GART_FB   2

#define DRM_ATI_GART_PCI  1
#define DRM_ATI_GART_PCIE 2
#define DRM_ATI_GART_IGP  3

struct drm_ati_pcigart_info {
	int gart_table_location;
	int gart_reg_if;
	void *addr;
	dma_addr_t bus_addr;
	dma_addr_t table_mask;
	dma_addr_t member_mask;
	struct drm_dma_handle *table_handle;
	drm_local_map_t mapping;
	int table_size;
	struct drm_dma_handle *dmah; /* handle for ATI PCIGART table */
};

#ifndef DMA_BIT_MASK
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : (1ULL<<(n)) - 1)
#endif

#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))

struct drm_driver_info {
	int	(*load)(struct drm_device *, unsigned long flags);
	int	(*firstopen)(struct drm_device *);
	int	(*open)(struct drm_device *, struct drm_file *);
	void	(*preclose)(struct drm_device *, struct drm_file *file_priv);
	void	(*postclose)(struct drm_device *, struct drm_file *);
	void	(*lastclose)(struct drm_device *);
	int	(*unload)(struct drm_device *);
	void	(*reclaim_buffers_locked)(struct drm_device *,
					  struct drm_file *file_priv);
	int	(*dma_ioctl)(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
	void	(*dma_ready)(struct drm_device *);
	int	(*dma_quiescent)(struct drm_device *);
	int	(*dma_flush_block_and_flush)(struct drm_device *, int context,
					     enum drm_lock_flags flags);
	int	(*dma_flush_unblock)(struct drm_device *, int context,
				     enum drm_lock_flags flags);
	int	(*context_ctor)(struct drm_device *dev, int context);
	int	(*context_dtor)(struct drm_device *dev, int context);
	int	(*kernel_context_switch)(struct drm_device *dev, int old,
					 int new);
	int	(*kernel_context_switch_unlock)(struct drm_device *dev);
	void	(*irq_preinstall)(struct drm_device *dev);
	int	(*irq_postinstall)(struct drm_device *dev);
	void	(*irq_uninstall)(struct drm_device *dev);
	void	(*irq_handler)(DRM_IRQ_ARGS);
	u32	(*get_vblank_counter)(struct drm_device *dev, int crtc);
	int	(*enable_vblank)(struct drm_device *dev, int crtc);
	void	(*disable_vblank)(struct drm_device *dev, int crtc);

	drm_pci_id_list_t *id_entry;	/* PCI ID, name, and chipset private */

	/**
	 * Called by \c drm_device_is_agp.  Typically used to determine if a
	 * card is really attached to AGP or not.
	 *
	 * \param dev  DRM device handle
	 *
	 * \returns 
	 * One of three values is returned depending on whether or not the
	 * card is absolutely \b not AGP (return of 0), absolutely \b is AGP
	 * (return of 1), or may or may not be AGP (return of 2).
	 */
	int	(*device_is_agp) (struct drm_device * dev);

	drm_ioctl_desc_t *ioctls;
	int	max_ioctl;

	int	buf_priv_size;

	int	major;
	int	minor;
	int	patchlevel;
	const char *name;		/* Simple driver name		   */
	const char *desc;		/* Longer driver name		   */
	const char *date;		/* Date of last major changes.	   */

	u32 driver_features;
};

/* Length for the array of resource pointers for drm_get_resource_*. */
#define DRM_MAX_PCI_RESOURCE	6

/** 
 * DRM device functions structure
 */
struct drm_device {
	struct drm_driver_info *driver;
	drm_pci_id_list_t *id_entry;	/* PCI ID, name, and chipset private */

	u_int16_t pci_device;		/* PCI device id */
	u_int16_t pci_vendor;		/* PCI vendor id */

	char		  *unique;	/* Unique identifier: e.g., busid  */
	int		  unique_len;	/* Length of unique field	   */
	device_t	  device;	/* Device instance from newbus     */
	struct cdev	  *devnode;	/* Device number for mknod	   */
	int		  if_version;	/* Highest interface version set */

	int		  flags;	/* Flags to open(2)		   */

				/* Locks */
	struct mtx	  vbl_lock;	/* protects vblank operations */
	struct mtx	  dma_lock;	/* protects dev->dma */
	struct mtx	  irq_lock;	/* protects irq condition checks */
	struct mtx	  dev_lock;	/* protects everything else */
	DRM_SPINTYPE	  drw_lock;

				/* Usage Counters */
	int		  open_count;	/* Outstanding files open	   */
	int		  buf_use;	/* Buffers in use -- cannot alloc  */

				/* Performance counters */
	unsigned long     counters;
	enum drm_stat_type	types[15];
	atomic_t          counts[15];

				/* Authentication */
	drm_file_list_t   files;
	drm_magic_head_t  magiclist[DRM_HASH_SIZE];

	/* Linked list of mappable regions. Protected by dev_lock */
	drm_map_list_t	  maplist;
	struct unrhdr	  *map_unrhdr;

	drm_local_map_t	  **context_sareas;
	int		  max_context;

	drm_lock_data_t	  lock;		/* Information on hardware lock	   */

				/* DMA queues (contexts) */
	drm_device_dma_t  *dma;		/* Optional pointer for DMA support */

				/* Context support */
	int		  irq;		/* Interrupt used by board	   */
	int		  irq_enabled;	/* True if the irq handler is enabled */
	int		  msi_enabled;	/* MSI enabled */
	int		  irqrid;	/* Interrupt used by board */
	struct resource   *irqr;	/* Resource for interrupt used by board	   */
	void		  *irqh;	/* Handle from bus_setup_intr      */

	/* Storage of resource pointers for drm_get_resource_* */
	struct resource   *pcir[DRM_MAX_PCI_RESOURCE];
	int		  pcirid[DRM_MAX_PCI_RESOURCE];

	int		  pci_domain;
	int		  pci_bus;
	int		  pci_slot;
	int		  pci_func;

	atomic_t	  context_flag;	/* Context swapping flag	   */
	int		  last_context;	/* Last current context		   */

	int		  vblank_disable_allowed;
	struct callout	  vblank_disable_timer;
	u32		  max_vblank_count;	/* size of vblank counter register */
	struct drm_vblank_info *vblank;		/* per crtc vblank info */
	int		  num_crtcs;

	struct sigio      *buf_sigio;	/* Processes waiting for SIGIO     */

				/* Sysctl support */
	struct drm_sysctl_info *sysctl;

	drm_agp_head_t    *agp;
	drm_sg_mem_t      *sg;  /* Scatter gather memory */
	atomic_t          *ctx_bitmap;
	void		  *dev_private;
	unsigned int	  agp_buffer_token;
	drm_local_map_t   *agp_buffer_map;

	struct unrhdr	  *drw_unrhdr;
	/* RB tree of drawable infos */
	RB_HEAD(drawable_tree, bsd_drm_drawable_info) drw_head;
};

static __inline__ int drm_core_check_feature(struct drm_device *dev,
					     int feature)
{
	return ((dev->driver->driver_features & feature) ? 1 : 0);
}

#if __OS_HAS_AGP
static inline int drm_core_has_AGP(struct drm_device *dev)
{
	return drm_core_check_feature(dev, DRIVER_USE_AGP);
}
#else
#define drm_core_has_AGP(dev) (0)
#endif

extern int	drm_debug_flag;

/* Device setup support (drm_drv.c) */
int	drm_probe(device_t kdev, drm_pci_id_list_t *idlist);
int	drm_attach(device_t kdev, drm_pci_id_list_t *idlist);
void	drm_close(void *data);
int	drm_detach(device_t kdev);
d_ioctl_t drm_ioctl;
d_open_t drm_open;
d_read_t drm_read;
d_poll_t drm_poll;
d_mmap_t drm_mmap;
extern drm_local_map_t	*drm_getsarea(struct drm_device *dev);

/* File operations helpers (drm_fops.c) */
extern int		drm_open_helper(struct cdev *kdev, int flags, int fmt,
					 DRM_STRUCTPROC *p,
					struct drm_device *dev);

/* Memory management support (drm_memory.c) */
void	drm_mem_init(void);
void	drm_mem_uninit(void);
void	*drm_ioremap_wc(struct drm_device *dev, drm_local_map_t *map);
void	*drm_ioremap(struct drm_device *dev, drm_local_map_t *map);
void	drm_ioremapfree(drm_local_map_t *map);
int	drm_mtrr_add(unsigned long offset, size_t size, int flags);
int	drm_mtrr_del(int handle, unsigned long offset, size_t size, int flags);

int	drm_context_switch(struct drm_device *dev, int old, int new);
int	drm_context_switch_complete(struct drm_device *dev, int new);

int	drm_ctxbitmap_init(struct drm_device *dev);
void	drm_ctxbitmap_cleanup(struct drm_device *dev);
void	drm_ctxbitmap_free(struct drm_device *dev, int ctx_handle);
int	drm_ctxbitmap_next(struct drm_device *dev);

/* Locking IOCTL support (drm_lock.c) */
int	drm_lock_take(struct drm_lock_data *lock_data,
		      unsigned int context);
int	drm_lock_transfer(struct drm_lock_data *lock_data,
			  unsigned int context);
int	drm_lock_free(struct drm_lock_data *lock_data,
		      unsigned int context);

/* Buffer management support (drm_bufs.c) */
unsigned long drm_get_resource_start(struct drm_device *dev,
				     unsigned int resource);
unsigned long drm_get_resource_len(struct drm_device *dev,
				   unsigned int resource);
void	drm_rmmap(struct drm_device *dev, drm_local_map_t *map);
int	drm_order(unsigned long size);
int	drm_addmap(struct drm_device *dev, unsigned long offset,
		   unsigned long size,
		   enum drm_map_type type, enum drm_map_flags flags,
		   drm_local_map_t **map_ptr);
int	drm_addbufs_pci(struct drm_device *dev, struct drm_buf_desc *request);
int	drm_addbufs_sg(struct drm_device *dev, struct drm_buf_desc *request);
int	drm_addbufs_agp(struct drm_device *dev, struct drm_buf_desc *request);

/* DMA support (drm_dma.c) */
int	drm_dma_setup(struct drm_device *dev);
void	drm_dma_takedown(struct drm_device *dev);
void	drm_free_buffer(struct drm_device *dev, drm_buf_t *buf);
void	drm_reclaim_buffers(struct drm_device *dev, struct drm_file *file_priv);
#define drm_core_reclaim_buffers drm_reclaim_buffers

/* IRQ support (drm_irq.c) */
int	drm_irq_install(struct drm_device *dev);
int	drm_irq_uninstall(struct drm_device *dev);
irqreturn_t drm_irq_handler(DRM_IRQ_ARGS);
void	drm_driver_irq_preinstall(struct drm_device *dev);
void	drm_driver_irq_postinstall(struct drm_device *dev);
void	drm_driver_irq_uninstall(struct drm_device *dev);
void	drm_handle_vblank(struct drm_device *dev, int crtc);
u32	drm_vblank_count(struct drm_device *dev, int crtc);
int	drm_vblank_get(struct drm_device *dev, int crtc);
void	drm_vblank_put(struct drm_device *dev, int crtc);
void	drm_vblank_cleanup(struct drm_device *dev);
int	drm_vblank_wait(struct drm_device *dev, unsigned int *vbl_seq);
int	drm_vblank_init(struct drm_device *dev, int num_crtcs);
int 	drm_modeset_ctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

/* AGP/PCI Express/GART support (drm_agpsupport.c) */
int	drm_device_is_agp(struct drm_device *dev);
int	drm_device_is_pcie(struct drm_device *dev);
drm_agp_head_t *drm_agp_init(void);
int	drm_agp_acquire(struct drm_device *dev);
int	drm_agp_release(struct drm_device *dev);
int	drm_agp_info(struct drm_device * dev, struct drm_agp_info *info);
int	drm_agp_enable(struct drm_device *dev, struct drm_agp_mode mode);
void	*drm_agp_allocate_memory(size_t pages, u32 type);
int	drm_agp_free_memory(void *handle);
int	drm_agp_bind_memory(void *handle, off_t start);
int	drm_agp_unbind_memory(void *handle);
int	drm_agp_alloc(struct drm_device *dev, struct drm_agp_buffer *request);
int	drm_agp_free(struct drm_device *dev, struct drm_agp_buffer *request);
int	drm_agp_bind(struct drm_device *dev, struct drm_agp_binding *request);
int	drm_agp_unbind(struct drm_device *dev, struct drm_agp_binding *request);

/* Scatter Gather Support (drm_scatter.c) */
void	drm_sg_cleanup(drm_sg_mem_t *entry);
int	drm_sg_alloc(struct drm_device *dev, struct drm_scatter_gather * request);

/* sysctl support (drm_sysctl.h) */
extern int		drm_sysctl_init(struct drm_device *dev);
extern int		drm_sysctl_cleanup(struct drm_device *dev);

/* ATI PCIGART support (ati_pcigart.c) */
int	drm_ati_pcigart_init(struct drm_device *dev,
				struct drm_ati_pcigart_info *gart_info);
int	drm_ati_pcigart_cleanup(struct drm_device *dev,
				struct drm_ati_pcigart_info *gart_info);

/* Locking IOCTL support (drm_drv.c) */
int	drm_lock(struct drm_device *dev, void *data,
		 struct drm_file *file_priv);
int	drm_unlock(struct drm_device *dev, void *data,
		   struct drm_file *file_priv);
int	drm_version(struct drm_device *dev, void *data,
		    struct drm_file *file_priv);
int	drm_setversion(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

/* Misc. IOCTL support (drm_ioctl.c) */
int	drm_irq_by_busid(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int	drm_getunique(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
int	drm_setunique(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
int	drm_getmap(struct drm_device *dev, void *data,
		   struct drm_file *file_priv);
int	drm_getclient(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
int	drm_getstats(struct drm_device *dev, void *data,
		     struct drm_file *file_priv);
int	drm_noop(struct drm_device *dev, void *data,
		 struct drm_file *file_priv);

/* Context IOCTL support (drm_context.c) */
int	drm_resctx(struct drm_device *dev, void *data,
		   struct drm_file *file_priv);
int	drm_addctx(struct drm_device *dev, void *data,
		   struct drm_file *file_priv);
int	drm_modctx(struct drm_device *dev, void *data,
		   struct drm_file *file_priv);
int	drm_getctx(struct drm_device *dev, void *data,
		   struct drm_file *file_priv);
int	drm_switchctx(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
int	drm_newctx(struct drm_device *dev, void *data,
		   struct drm_file *file_priv);
int	drm_rmctx(struct drm_device *dev, void *data,
		  struct drm_file *file_priv);
int	drm_setsareactx(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int	drm_getsareactx(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

/* Drawable IOCTL support (drm_drawable.c) */
int	drm_adddraw(struct drm_device *dev, void *data,
		    struct drm_file *file_priv);
int	drm_rmdraw(struct drm_device *dev, void *data,
		   struct drm_file *file_priv);
int	drm_update_draw(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
struct drm_drawable_info *drm_get_drawable_info(struct drm_device *dev,
						int handle);

/* Drawable support (drm_drawable.c) */
void drm_drawable_free_all(struct drm_device *dev);

/* Authentication IOCTL support (drm_auth.c) */
int	drm_getmagic(struct drm_device *dev, void *data,
		     struct drm_file *file_priv);
int	drm_authmagic(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);

/* Buffer management support (drm_bufs.c) */
int	drm_addmap_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int	drm_rmmap_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int	drm_addbufs(struct drm_device *dev, void *data,
		    struct drm_file *file_priv);
int	drm_infobufs(struct drm_device *dev, void *data,
		     struct drm_file *file_priv);
int	drm_markbufs(struct drm_device *dev, void *data,
		     struct drm_file *file_priv);
int	drm_freebufs(struct drm_device *dev, void *data,
		     struct drm_file *file_priv);
int	drm_mapbufs(struct drm_device *dev, void *data,
		    struct drm_file *file_priv);

/* DMA support (drm_dma.c) */
int	drm_dma(struct drm_device *dev, void *data, struct drm_file *file_priv);

/* IRQ support (drm_irq.c) */
int	drm_control(struct drm_device *dev, void *data,
		    struct drm_file *file_priv);
int	drm_wait_vblank(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

/* AGP/GART support (drm_agpsupport.c) */
int	drm_agp_acquire_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
int	drm_agp_release_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
int	drm_agp_enable_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
int	drm_agp_info_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
int	drm_agp_alloc_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv);
int	drm_agp_free_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
int	drm_agp_unbind_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
int	drm_agp_bind_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);

/* Scatter Gather Support (drm_scatter.c) */
int	drm_sg_alloc_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
int	drm_sg_free(struct drm_device *dev, void *data,
		    struct drm_file *file_priv);

/* consistent PCI memory functions (drm_pci.c) */
drm_dma_handle_t *drm_pci_alloc(struct drm_device *dev, size_t size,
				size_t align, dma_addr_t maxaddr);
void	drm_pci_free(struct drm_device *dev, drm_dma_handle_t *dmah);

/* Inline replacements for drm_alloc and friends */
static __inline__ void *
drm_alloc(size_t size, struct malloc_type *area)
{
	return malloc(size, area, M_NOWAIT);
}

static __inline__ void *
drm_calloc(size_t nmemb, size_t size, struct malloc_type *area)
{
	return malloc(size * nmemb, area, M_NOWAIT | M_ZERO);
}

static __inline__ void *
drm_realloc(void *oldpt, size_t oldsize, size_t size,
    struct malloc_type *area)
{
	return reallocf(oldpt, size, area, M_NOWAIT);
}

static __inline__ void
drm_free(void *pt, size_t size, struct malloc_type *area)
{
	free(pt, area);
}

/* Inline replacements for DRM_IOREMAP macros */
static __inline__ void
drm_core_ioremap_wc(struct drm_local_map *map, struct drm_device *dev)
{
	map->virtual = drm_ioremap_wc(dev, map);
}
static __inline__ void
drm_core_ioremap(struct drm_local_map *map, struct drm_device *dev)
{
	map->virtual = drm_ioremap(dev, map);
}
static __inline__ void
drm_core_ioremapfree(struct drm_local_map *map, struct drm_device *dev)
{
	if ( map->virtual && map->size )
		drm_ioremapfree(map);
}

static __inline__ struct drm_local_map *
drm_core_findmap(struct drm_device *dev, unsigned long offset)
{
	drm_local_map_t *map;

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (offset == (unsigned long)map->handle)
			return map;
	}
	return NULL;
}

static __inline__ void drm_core_dropmap(struct drm_map *map)
{
}

#endif /* __KERNEL__ */
#endif /* _DRM_P_H_ */
