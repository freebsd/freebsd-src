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
__FBSDID("$FreeBSD: src/sys/dev/drm/drmP.h,v 1.18 2006/11/06 13:41:53 rwatson Exp $");

#ifndef _DRM_P_H_
#define _DRM_P_H_

#if defined(_KERNEL) || defined(__KERNEL__)

typedef struct drm_device drm_device_t;
typedef struct drm_file drm_file_t;

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/stat.h>
#if __FreeBSD_version >= 700000
#include <sys/priv.h>
#endif
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/filio.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/signalvar.h>
#include <sys/poll.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <machine/param.h>
#include <machine/pmap.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/sysarch.h>
#include <sys/endian.h>
#include <sys/mman.h>
#if defined(__FreeBSD__)
#include <sys/rman.h>
#include <sys/memrange.h>
#include <pci/agpvar.h>
#include <sys/agpio.h>
#if __FreeBSD_version >= 500000
#include <sys/mutex.h>
#include <dev/pci/pcivar.h>
#include <sys/selinfo.h>
#else /* __FreeBSD_version >= 500000 */
#include <pci/pcivar.h>
#include <sys/select.h>
#endif /* __FreeBSD_version < 500000 */
#elif defined(__NetBSD__)
#include <machine/mtrr.h>
#include <sys/vnode.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/resourcevar.h>
#include <sys/lkm.h>
#include <sys/agpio.h>
#include <sys/ttycom.h>
#include <uvm/uvm.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/agpvar.h>
#elif defined(__OpenBSD__)
#include <sys/lkm.h>
#include <uvm/uvm.h>
#endif
#include <sys/bus.h>

#include "dev/drm/drm.h"
#include "dev/drm/drm_linux_list.h"
#include "dev/drm/drm_atomic.h"

#ifdef __FreeBSD__
#include <opt_drm.h>
#ifdef DRM_DEBUG
#undef DRM_DEBUG
#define DRM_DEBUG_DEFAULT_ON 1
#endif /* DRM_DEBUG */
#endif

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

#define DRM_HASH_SIZE	      16 /* Size of key hash table		  */
#define DRM_KERNEL_CONTEXT    0	 /* Change drm_resctx if changed	  */
#define DRM_RESERVED_CONTEXTS 1	 /* Change drm_resctx if changed	  */

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

				/* Internal types and structures */
#define DRM_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define DRM_MIN(a,b) ((a)<(b)?(a):(b))
#define DRM_MAX(a,b) ((a)>(b)?(a):(b))

#define DRM_IF_VERSION(maj, min) (maj << 16 | min)

MALLOC_DECLARE(M_DRM);

#define __OS_HAS_AGP	1

#define DRM_DEV_MODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)
#define DRM_DEV_UID	0
#define DRM_DEV_GID	0

#define wait_queue_head_t	atomic_t
#define DRM_WAKEUP(w)		wakeup((void *)w)
#define DRM_WAKEUP_INT(w)	wakeup(w)
#define DRM_INIT_WAITQUEUE(queue) do {(void)(queue);} while (0)

#if defined(__FreeBSD__) && __FreeBSD_version < 502109
#define bus_alloc_resource_any(dev, type, rid, flags) \
	bus_alloc_resource(dev, type, rid, 0ul, ~0ul, 1, flags)
#endif

#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
#define DRM_CURPROC		curthread
#define DRM_STRUCTPROC		struct thread
#define DRM_SPINTYPE		struct mtx
#define DRM_SPININIT(l,name)	mtx_init(&l, name, NULL, MTX_DEF)
#define DRM_SPINUNINIT(l)	mtx_destroy(&l)
#define DRM_SPINLOCK(l)		mtx_lock(l)
#define DRM_SPINUNLOCK(u)	mtx_unlock(u);
#define DRM_SPINLOCK_ASSERT(l)	mtx_assert(l, MA_OWNED)
#define DRM_CURRENTPID		curthread->td_proc->p_pid
#define DRM_LOCK()		mtx_lock(&dev->dev_lock)
#define DRM_UNLOCK() 		mtx_unlock(&dev->dev_lock)
#define DRM_SYSCTL_HANDLER_ARGS	(SYSCTL_HANDLER_ARGS)
#else /* __FreeBSD__ && __FreeBSD_version >= 500000 */
#define DRM_CURPROC		curproc
#define DRM_STRUCTPROC		struct proc
#define DRM_SPINTYPE		struct simplelock
#define DRM_SPININIT(l,name)
#define DRM_SPINUNINIT(l)
#define DRM_SPINLOCK(l)	
#define DRM_SPINUNLOCK(u)
#define DRM_SPINLOCK_ASSERT(l)
#define DRM_CURRENTPID		curproc->p_pid
#define DRM_LOCK()
#define DRM_UNLOCK()
#define DRM_SYSCTL_HANDLER_ARGS	SYSCTL_HANDLER_ARGS
#define spldrm()		spltty()
#endif /* __NetBSD__ || __OpenBSD__ */

/* Currently our DRMFILE (filp) is a void * which is actually the pid
 * of the current process.  It should be a per-open unique pointer, but
 * code for that is not yet written */
#define DRMFILE			void *
#define DRM_IRQ_ARGS		void *arg
typedef void			irqreturn_t;
#define IRQ_HANDLED		/* nothing */
#define IRQ_NONE		/* nothing */

enum {
	DRM_IS_NOT_AGP,
	DRM_IS_AGP,
	DRM_MIGHT_BE_AGP
};
#define DRM_AGP_MEM		struct agp_memory_info

#if defined(__FreeBSD__)
#define DRM_DEVICE							\
	drm_device_t *dev = kdev->si_drv1
#define DRM_IOCTL_ARGS		struct cdev *kdev, u_long cmd, caddr_t data, \
				int flags, DRM_STRUCTPROC *p, DRMFILE filp

#define PAGE_ALIGN(addr) round_page(addr)
/* DRM_SUSER returns true if the user is superuser */
#if __FreeBSD_version >= 700000
#define	DRM_SUSER(p)		(priv_check(p, PRIV_DRIVER) == 0)
#else
#define DRM_SUSER(p)		(suser(p) == 0)
#endif
#define DRM_AGP_FIND_DEVICE()	agp_find_device()
#define DRM_MTRR_WC		MDF_WRITECOMBINE
#define jiffies			ticks

#else /* __FreeBSD__ */

#if defined(__NetBSD__)
#define DRM_DEVICE							\
	drm_device_t *dev = device_lookup(&drm_cd, minor(kdev))
#elif defined(__OpenBSD__)
#define DRM_DEVICE							\
	drm_device_t *dev = (device_lookup(&drm_cd,			\
	    minor(kdev)))->dv_cfdata->cf_driver->cd_devs[minor(kdev)]
#endif /* __OpenBSD__ */
#define DRM_IOCTL_ARGS		dev_t kdev, u_long cmd, caddr_t data, \
				int flags, DRM_STRUCTPROC *p, DRMFILE filp

#define CDEV_MAJOR		34
#define PAGE_ALIGN(addr)	(((addr) + PAGE_SIZE - 1) & PAGE_MASK)
/* DRM_SUSER returns true if the user is superuser */
#define DRM_SUSER(p)		(suser(p->p_ucred, &p->p_acflag) == 0)
#define DRM_AGP_FIND_DEVICE()	agp_find_device(0)
#define DRM_MTRR_WC		MTRR_TYPE_WC
#define jiffies			hardclock_ticks

typedef drm_device_t *device_t;
extern struct cfdriver drm_cd;
#endif /* !__FreeBSD__ */

/* Capabilities taken from src/sys/dev/pci/pcireg.h. */
#ifndef PCIY_AGP
#define PCIY_AGP	0x02
#endif

#ifndef PCIY_EXPRESS
#define PCIY_EXPRESS	0x10
#endif

typedef unsigned long dma_addr_t;
typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

/* DRM_READMEMORYBARRIER() prevents reordering of reads.
 * DRM_WRITEMEMORYBARRIER() prevents reordering of writes.
 * DRM_MEMORYBARRIER() prevents reordering of reads and writes.
 */
#if defined(__i386__)
#define DRM_READMEMORYBARRIER()		__asm __volatile( \
					"lock; addl $0,0(%%esp)" : : : "memory");
#define DRM_WRITEMEMORYBARRIER()	__asm __volatile("" : : : "memory");
#define DRM_MEMORYBARRIER()		__asm __volatile( \
					"lock; addl $0,0(%%esp)" : : : "memory");
#elif defined(__alpha__)
#define DRM_READMEMORYBARRIER()		alpha_mb();
#define DRM_WRITEMEMORYBARRIER()	alpha_wmb();
#define DRM_MEMORYBARRIER()		alpha_mb();
#elif defined(__amd64__)
#define DRM_READMEMORYBARRIER()		__asm __volatile( \
					"lock; addl $0,0(%%rsp)" : : : "memory");
#define DRM_WRITEMEMORYBARRIER()	__asm __volatile("" : : : "memory");
#define DRM_MEMORYBARRIER()		__asm __volatile( \
					"lock; addl $0,0(%%rsp)" : : : "memory");
#endif

#ifdef __FreeBSD__
#define DRM_READ8(map, offset)						\
	*(volatile u_int8_t *) (((unsigned long)(map)->handle) + (offset))
#define DRM_READ16(map, offset)						\
	*(volatile u_int16_t *) (((unsigned long)(map)->handle) + (offset))
#define DRM_READ32(map, offset)						\
	*(volatile u_int32_t *)(((unsigned long)(map)->handle) + (offset))
#define DRM_WRITE8(map, offset, val)					\
	*(volatile u_int8_t *) (((unsigned long)(map)->handle) + (offset)) = val
#define DRM_WRITE16(map, offset, val)					\
	*(volatile u_int16_t *) (((unsigned long)(map)->handle) + (offset)) = val
#define DRM_WRITE32(map, offset, val)					\
	*(volatile u_int32_t *)(((unsigned long)(map)->handle) + (offset)) = val

#define DRM_VERIFYAREA_READ( uaddr, size )		\
	(!useracc(__DECONST(caddr_t, uaddr), size, VM_PROT_READ))

#else /* __FreeBSD__ */

typedef vaddr_t vm_offset_t;

#define DRM_READ8(map, offset)		\
	bus_space_read_1( (map)->bst, (map)->bsh, (offset))
#define DRM_READ16(map, offset)		\
	bus_space_read_2( (map)->bst, (map)->bsh, (offset))
#define DRM_READ32(map, offset)		\
	bus_space_read_4( (map)->bst, (map)->bsh, (offset))
#define DRM_WRITE8(map, offset, val)	\
	bus_space_write_1((map)->bst, (map)->bsh, (offset), (val))
#define DRM_WRITE16(map, offset, val)	\
	bus_space_write_2((map)->bst, (map)->bsh, (offset), (val))
#define DRM_WRITE32(map, offset, val)	\
	bus_space_write_4((map)->bst, (map)->bsh, (offset), (val))

#define DRM_VERIFYAREA_READ( uaddr, size )		\
	(!uvm_useracc((caddr_t)uaddr, size, VM_PROT_READ))
#endif /* !__FreeBSD__ */

#define DRM_COPY_TO_USER_IOCTL(user, kern, size)	\
	if ( IOCPARM_LEN(cmd) != size)			\
		return EINVAL;				\
	*user = kern;
#define DRM_COPY_FROM_USER_IOCTL(kern, user, size) \
	if ( IOCPARM_LEN(cmd) != size)			\
		return EINVAL;				\
	kern = *user;
#define DRM_COPY_TO_USER(user, kern, size) \
	copyout(kern, user, size)
#define DRM_COPY_FROM_USER(kern, user, size) \
	copyin(user, kern, size)
#define DRM_COPY_FROM_USER_UNCHECKED(arg1, arg2, arg3) 	\
	copyin(arg2, arg1, arg3)
#define DRM_COPY_TO_USER_UNCHECKED(arg1, arg2, arg3)	\
	copyout(arg2, arg1, arg3)
#if __FreeBSD_version > 500000
#define DRM_GET_USER_UNCHECKED(val, uaddr)		\
	((val) = fuword32(uaddr), 0)
#else
#define DRM_GET_USER_UNCHECKED(val, uaddr)		\
	((val) = fuword(uaddr), 0)
#endif

#define cpu_to_le32(x) htole32(x)
#define le32_to_cpu(x) le32toh(x)

#define DRM_ERR(v)		v
#define DRM_HZ			hz
#define DRM_UDELAY(udelay)	DELAY(udelay)
#define DRM_TIME_SLICE		(hz/20)  /* Time slice for GLXContexts	  */

#define DRM_GET_PRIV_SAREA(_dev, _ctx, _map) do {	\
	(_map) = (_dev)->context_sareas[_ctx];		\
} while(0)

#define DRM_GET_PRIV_WITH_RETURN(_priv, _filp)			\
do {								\
	if (_filp != (DRMFILE)(intptr_t)DRM_CURRENTPID) {	\
		DRM_ERROR("filp doesn't match curproc\n");	\
		return EINVAL;					\
	}							\
	_priv = drm_find_file_by_proc(dev, DRM_CURPROC);	\
	if (_priv == NULL) {					\
		DRM_ERROR("can't find authenticator\n");	\
		return EINVAL;					\
	}							\
} while (0)

#define LOCK_TEST_WITH_RETURN(dev, filp)				\
do {									\
	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock) ||		\
	     dev->lock.filp != filp) {					\
		DRM_ERROR("%s called without lock held\n",		\
			   __FUNCTION__);				\
		return EINVAL;						\
	}								\
} while (0)

#define DRM_GETSAREA()					\
do {								\
	drm_local_map_t *map;					\
	DRM_SPINLOCK_ASSERT(&dev->dev_lock);			\
	TAILQ_FOREACH(map, &dev->maplist, link) {		\
		if (map->type == _DRM_SHM &&			\
			map->flags & _DRM_CONTAINS_LOCK) {	\
			dev_priv->sarea = map;			\
			break;					\
		}						\
	}							\
} while (0)

#if defined(__FreeBSD__) && __FreeBSD_version > 500000
#define DRM_WAIT_ON( ret, queue, timeout, condition )		\
for ( ret = 0 ; !ret && !(condition) ; ) {			\
	DRM_UNLOCK();						\
	mtx_lock(&dev->irq_lock);				\
	if (!(condition))					\
	   ret = msleep(&(queue), &dev->irq_lock, 		\
			 PZERO | PCATCH, "drmwtq", (timeout));	\
	mtx_unlock(&dev->irq_lock);				\
	DRM_LOCK();						\
}
#else
#define DRM_WAIT_ON( ret, queue, timeout, condition )	\
for ( ret = 0 ; !ret && !(condition) ; ) {		\
        int s = spldrm();				\
	if (!(condition))				\
	   ret = tsleep( &(queue), PZERO | PCATCH, 	\
			 "drmwtq", (timeout) );		\
	splx(s);					\
}
#endif

#define DRM_ERROR(fmt, arg...) \
	printf("error: [" DRM_NAME ":pid%d:%s] *ERROR* " fmt,		\
	    DRM_CURRENTPID, __func__ , ## arg)

#define DRM_INFO(fmt, arg...)  printf("info: [" DRM_NAME "] " fmt , ## arg)

#define DRM_DEBUG(fmt, arg...) do {					\
	if (drm_debug_flag)						\
		printf("[" DRM_NAME ":pid%d:%s] " fmt, DRM_CURRENTPID,	\
			__func__ , ## arg);				\
} while (0)

typedef struct drm_pci_id_list
{
	int vendor;
	int device;
	long driver_private;
	char *name;
} drm_pci_id_list_t;

#define DRM_AUTH	0x1
#define DRM_MASTER	0x2
#define DRM_ROOT_ONLY	0x4
typedef struct drm_ioctl_desc {
	int		     (*func)(DRM_IOCTL_ARGS);
	int flags;
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
#if defined(__FreeBSD__)
	bus_dma_tag_t tag;
	bus_dmamap_t map;
#elif defined(__NetBSD__)
	bus_dma_segment_t seg;
#endif
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
	int		  authenticated;
	int		  master;
	int		  minor;
	pid_t		  pid;
	uid_t		  uid;
	int		  refs;
	drm_magic_t	  magic;
	unsigned long	  ioctl_count;
	void		 *driver_priv;
};

typedef struct drm_lock_data {
	drm_hw_lock_t	  *hw_lock;	/* Hardware lock		   */
	DRMFILE		  filp;	        /* Unique identifier of holding process (NULL is kernel)*/
	int		  lock_queue;	/* Queue of blocked processes	   */
	unsigned long	  lock_time;	/* Time of last lock in jiffies	   */
} drm_lock_data_t;

/* This structure, in the drm_device_t, is always initialized while the device
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
	unsigned long   handle;
	void            *virtual;
	int             pages;
	dma_addr_t	*busaddr;
	drm_dma_handle_t *dmah;	/* Handle to PCI memory for ATI PCIGART table */
} drm_sg_mem_t;

typedef TAILQ_HEAD(drm_map_list, drm_local_map) drm_map_list_t;

typedef struct drm_local_map {
	unsigned long	offset;	 /* Physical address (0 for SAREA)*/
	unsigned long	size;	 /* Physical size (bytes)	    */
	drm_map_type_t	type;	 /* Type of memory mapped		    */
	drm_map_flags_t flags;	 /* Flags				    */
	void		*handle; /* User-space: "Handle" to pass to mmap    */
				 /* Kernel-space: kernel-virtual address    */
	int		mtrr;	 /* Boolean: MTRR used */
				 /* Private data			    */
	int		rid;	 /* PCI resource ID for bus_space */
	struct resource *bsr;
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	drm_dma_handle_t *dmah;
	TAILQ_ENTRY(drm_local_map) link;
} drm_local_map_t;

TAILQ_HEAD(drm_vbl_sig_list, drm_vbl_sig);
typedef struct drm_vbl_sig {
	TAILQ_ENTRY(drm_vbl_sig) link;
	unsigned int	sequence;
	int		signo;
	int		pid;
} drm_vbl_sig_t;

/* location of GART table */
#define DRM_ATI_GART_MAIN 1
#define DRM_ATI_GART_FB   2

typedef struct ati_pcigart_info {
	int gart_table_location;
	int is_pcie;
	void *addr;
	dma_addr_t bus_addr;
	drm_local_map_t mapping;
} drm_ati_pcigart_info;

struct drm_driver_info {
	int	(*load)(struct drm_device *, unsigned long flags);
	int	(*firstopen)(struct drm_device *);
	int	(*open)(struct drm_device *, drm_file_t *);
	void	(*preclose)(struct drm_device *, void *filp);
	void	(*postclose)(struct drm_device *, drm_file_t *);
	void	(*lastclose)(struct drm_device *);
	int	(*unload)(struct drm_device *);
	void	(*reclaim_buffers_locked)(struct drm_device *, void *filp);
	int	(*dma_ioctl)(DRM_IOCTL_ARGS);
	void	(*dma_ready)(struct drm_device *);
	int	(*dma_quiescent)(struct drm_device *);
	int	(*dma_flush_block_and_flush)(struct drm_device *, int context,
					     drm_lock_flags_t flags);
	int	(*dma_flush_unblock)(struct drm_device *, int context,
				     drm_lock_flags_t flags);
	int	(*context_ctor)(struct drm_device *dev, int context);
	int	(*context_dtor)(struct drm_device *dev, int context);
	int	(*kernel_context_switch)(struct drm_device *dev, int old,
					 int new);
	int	(*kernel_context_switch_unlock)(struct drm_device *dev);
	void	(*irq_preinstall)(drm_device_t *dev);
	void	(*irq_postinstall)(drm_device_t *dev);
	void	(*irq_uninstall)(drm_device_t *dev);
	void	(*irq_handler)(DRM_IRQ_ARGS);
	int	(*vblank_wait)(drm_device_t *dev, unsigned int *sequence);

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

	unsigned use_agp :1;
	unsigned require_agp :1;
	unsigned use_sg :1;
	unsigned use_dma :1;
	unsigned use_pci_dma :1;
	unsigned use_dma_queue :1;
	unsigned use_irq :1;
	unsigned use_vbl_irq :1;
	unsigned use_mtrr :1;
};

/* Length for the array of resource pointers for drm_get_resource_*. */
#define DRM_MAX_PCI_RESOURCE	3

/** 
 * DRM device functions structure
 */
struct drm_device {
#if defined(__NetBSD__) || defined(__OpenBSD__)
	struct device	  device; /* softc is an extension of struct device */
#endif

	struct drm_driver_info driver;
	drm_pci_id_list_t *id_entry;	/* PCI ID, name, and chipset private */

	u_int16_t pci_device;		/* PCI device id */
	u_int16_t pci_vendor;		/* PCI vendor id */

	char		  *unique;	/* Unique identifier: e.g., busid  */
	int		  unique_len;	/* Length of unique field	   */
#ifdef __FreeBSD__
	device_t	  device;	/* Device instance from newbus     */
#endif
	struct cdev	  *devnode;	/* Device number for mknod	   */
	int		  if_version;	/* Highest interface version set */

	int		  flags;	/* Flags to open(2)		   */

				/* Locks */
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
	struct mtx	  dma_lock;	/* protects dev->dma */
	struct mtx	  irq_lock;	/* protects irq condition checks */
	struct mtx	  dev_lock;	/* protects everything else */
#endif
				/* Usage Counters */
	int		  open_count;	/* Outstanding files open	   */
	int		  buf_use;	/* Buffers in use -- cannot alloc  */

				/* Performance counters */
	unsigned long     counters;
	drm_stat_type_t   types[15];
	atomic_t          counts[15];

				/* Authentication */
	drm_file_list_t   files;
	drm_magic_head_t  magiclist[DRM_HASH_SIZE];

	/* Linked list of mappable regions. Protected by dev_lock */
	drm_map_list_t	  maplist;

	drm_local_map_t	  **context_sareas;
	int		  max_context;

	drm_lock_data_t	  lock;		/* Information on hardware lock	   */

				/* DMA queues (contexts) */
	drm_device_dma_t  *dma;		/* Optional pointer for DMA support */

				/* Context support */
	int		  irq;		/* Interrupt used by board	   */
	int		  irq_enabled;	/* True if the irq handler is enabled */
#ifdef __FreeBSD__
	int		  irqrid;	/* Interrupt used by board */
	struct resource   *irqr;	/* Resource for interrupt used by board	   */
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	struct pci_attach_args  pa;
#endif
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
   	int		  vbl_queue;	/* vbl wait channel */
   	atomic_t          vbl_received;

#ifdef __FreeBSD__
	struct sigio      *buf_sigio;	/* Processes waiting for SIGIO     */
#elif defined(__NetBSD__)
	pid_t		  buf_pgid;
#endif

				/* Sysctl support */
	struct drm_sysctl_info *sysctl;

	drm_agp_head_t    *agp;
	drm_sg_mem_t      *sg;  /* Scatter gather memory */
	atomic_t          *ctx_bitmap;
	void		  *dev_private;
	unsigned int	  agp_buffer_token;
	drm_local_map_t   *agp_buffer_map;
};

extern int	drm_debug_flag;

/* Device setup support (drm_drv.c) */
#ifdef __FreeBSD__
int	drm_probe(device_t nbdev, drm_pci_id_list_t *idlist);
int	drm_attach(device_t nbdev, drm_pci_id_list_t *idlist);
int	drm_detach(device_t nbdev);
d_ioctl_t drm_ioctl;
d_open_t drm_open;
d_close_t drm_close;
d_read_t drm_read;
d_poll_t drm_poll;
d_mmap_t drm_mmap;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
int	drm_probe(struct pci_attach_args *pa, drm_pci_id_list_t *idlist);
int	drm_attach(struct pci_attach_args *pa, dev_t kdev, drm_pci_id_list_t *idlist);
dev_type_ioctl(drm_ioctl);
dev_type_open(drm_open);
dev_type_close(drm_close);
dev_type_read(drm_read);
dev_type_poll(drm_poll);
dev_type_mmap(drm_mmap);
#endif

/* File operations helpers (drm_fops.c) */
#ifdef __FreeBSD__
extern int		drm_open_helper(struct cdev *kdev, int flags, int fmt, 
					 DRM_STRUCTPROC *p, drm_device_t *dev);
extern drm_file_t	*drm_find_file_by_proc(drm_device_t *dev, 
					 DRM_STRUCTPROC *p);
#elif defined(__NetBSD__) || defined(__OpenBSD__)
extern int		drm_open_helper(dev_t kdev, int flags, int fmt, 
					DRM_STRUCTPROC *p, drm_device_t *dev);
extern drm_file_t	*drm_find_file_by_proc(drm_device_t *dev, 
					       DRM_STRUCTPROC *p);
#endif /* __NetBSD__ || __OpenBSD__ */

/* Memory management support (drm_memory.c) */
void	drm_mem_init(void);
void	drm_mem_uninit(void);
void	*drm_alloc(size_t size, int area);
void	*drm_calloc(size_t nmemb, size_t size, int area);
void	*drm_realloc(void *oldpt, size_t oldsize, size_t size,
				   int area);
void	drm_free(void *pt, size_t size, int area);
void	*drm_ioremap(drm_device_t *dev, drm_local_map_t *map);
void	drm_ioremapfree(drm_local_map_t *map);
int	drm_mtrr_add(unsigned long offset, size_t size, int flags);
int	drm_mtrr_del(int handle, unsigned long offset, size_t size, int flags);

int	drm_context_switch(drm_device_t *dev, int old, int new);
int	drm_context_switch_complete(drm_device_t *dev, int new);

int	drm_ctxbitmap_init(drm_device_t *dev);
void	drm_ctxbitmap_cleanup(drm_device_t *dev);
void	drm_ctxbitmap_free(drm_device_t *dev, int ctx_handle);
int	drm_ctxbitmap_next(drm_device_t *dev);

/* Locking IOCTL support (drm_lock.c) */
int	drm_lock_take(__volatile__ unsigned int *lock,
				    unsigned int context);
int	drm_lock_transfer(drm_device_t *dev,
					__volatile__ unsigned int *lock,
					unsigned int context);
int	drm_lock_free(drm_device_t *dev,
				    __volatile__ unsigned int *lock,
				    unsigned int context);

/* Buffer management support (drm_bufs.c) */
unsigned long drm_get_resource_start(drm_device_t *dev, unsigned int resource);
unsigned long drm_get_resource_len(drm_device_t *dev, unsigned int resource);
void	drm_rmmap(drm_device_t *dev, drm_local_map_t *map);
int	drm_order(unsigned long size);
int	drm_addmap(drm_device_t * dev, unsigned long offset, unsigned long size,
		   drm_map_type_t type, drm_map_flags_t flags,
		   drm_local_map_t **map_ptr);
int	drm_addbufs_pci(drm_device_t *dev, drm_buf_desc_t *request);
int	drm_addbufs_sg(drm_device_t *dev, drm_buf_desc_t *request);
int	drm_addbufs_agp(drm_device_t *dev, drm_buf_desc_t *request);

/* DMA support (drm_dma.c) */
int	drm_dma_setup(drm_device_t *dev);
void	drm_dma_takedown(drm_device_t *dev);
void	drm_free_buffer(drm_device_t *dev, drm_buf_t *buf);
void	drm_reclaim_buffers(drm_device_t *dev, DRMFILE filp);
#define drm_core_reclaim_buffers drm_reclaim_buffers

/* IRQ support (drm_irq.c) */
int	drm_irq_install(drm_device_t *dev);
int	drm_irq_uninstall(drm_device_t *dev);
irqreturn_t drm_irq_handler(DRM_IRQ_ARGS);
void	drm_driver_irq_preinstall(drm_device_t *dev);
void	drm_driver_irq_postinstall(drm_device_t *dev);
void	drm_driver_irq_uninstall(drm_device_t *dev);
int	drm_vblank_wait(drm_device_t *dev, unsigned int *vbl_seq);
void	drm_vbl_send_signals(drm_device_t *dev);

/* AGP/PCI Express/GART support (drm_agpsupport.c) */
int	drm_device_is_agp(drm_device_t *dev);
int	drm_device_is_pcie(drm_device_t *dev);
drm_agp_head_t *drm_agp_init(void);
int	drm_agp_acquire(drm_device_t *dev);
int	drm_agp_release(drm_device_t *dev);
int	drm_agp_info(drm_device_t * dev, drm_agp_info_t *info);
int	drm_agp_enable(drm_device_t *dev, drm_agp_mode_t mode);
void	*drm_agp_allocate_memory(size_t pages, u32 type);
int	drm_agp_free_memory(void *handle);
int	drm_agp_bind_memory(void *handle, off_t start);
int	drm_agp_unbind_memory(void *handle);
int	drm_agp_alloc(drm_device_t *dev, drm_agp_buffer_t *request);
int	drm_agp_free(drm_device_t *dev, drm_agp_buffer_t *request);
int	drm_agp_bind(drm_device_t *dev, drm_agp_binding_t *request);
int	drm_agp_unbind(drm_device_t *dev, drm_agp_binding_t *request);

/* Scatter Gather Support (drm_scatter.c) */
void	drm_sg_cleanup(drm_sg_mem_t *entry);

#ifdef __FreeBSD__
/* sysctl support (drm_sysctl.h) */
extern int		drm_sysctl_init(drm_device_t *dev);
extern int		drm_sysctl_cleanup(drm_device_t *dev);
#endif /* __FreeBSD__ */

/* ATI PCIGART support (ati_pcigart.c) */
int	drm_ati_pcigart_init(drm_device_t *dev,
			     drm_ati_pcigart_info *gart_info);
int	drm_ati_pcigart_cleanup(drm_device_t *dev,
				drm_ati_pcigart_info *gart_info);

/* Locking IOCTL support (drm_drv.c) */
int	drm_lock(DRM_IOCTL_ARGS);
int	drm_unlock(DRM_IOCTL_ARGS);
int	drm_version(DRM_IOCTL_ARGS);
int	drm_setversion(DRM_IOCTL_ARGS);

/* Misc. IOCTL support (drm_ioctl.c) */
int	drm_irq_by_busid(DRM_IOCTL_ARGS);
int	drm_getunique(DRM_IOCTL_ARGS);
int	drm_setunique(DRM_IOCTL_ARGS);
int	drm_getmap(DRM_IOCTL_ARGS);
int	drm_getclient(DRM_IOCTL_ARGS);
int	drm_getstats(DRM_IOCTL_ARGS);
int	drm_noop(DRM_IOCTL_ARGS);

/* Context IOCTL support (drm_context.c) */
int	drm_resctx(DRM_IOCTL_ARGS);
int	drm_addctx(DRM_IOCTL_ARGS);
int	drm_modctx(DRM_IOCTL_ARGS);
int	drm_getctx(DRM_IOCTL_ARGS);
int	drm_switchctx(DRM_IOCTL_ARGS);
int	drm_newctx(DRM_IOCTL_ARGS);
int	drm_rmctx(DRM_IOCTL_ARGS);
int	drm_setsareactx(DRM_IOCTL_ARGS);
int	drm_getsareactx(DRM_IOCTL_ARGS);

/* Drawable IOCTL support (drm_drawable.c) */
int	drm_adddraw(DRM_IOCTL_ARGS);
int	drm_rmdraw(DRM_IOCTL_ARGS);

/* Authentication IOCTL support (drm_auth.c) */
int	drm_getmagic(DRM_IOCTL_ARGS);
int	drm_authmagic(DRM_IOCTL_ARGS);

/* Buffer management support (drm_bufs.c) */
int	drm_addmap_ioctl(DRM_IOCTL_ARGS);
int	drm_rmmap_ioctl(DRM_IOCTL_ARGS);
int	drm_addbufs_ioctl(DRM_IOCTL_ARGS);
int	drm_infobufs(DRM_IOCTL_ARGS);
int	drm_markbufs(DRM_IOCTL_ARGS);
int	drm_freebufs(DRM_IOCTL_ARGS);
int	drm_mapbufs(DRM_IOCTL_ARGS);

/* DMA support (drm_dma.c) */
int	drm_dma(DRM_IOCTL_ARGS);

/* IRQ support (drm_irq.c) */
int	drm_control(DRM_IOCTL_ARGS);
int	drm_wait_vblank(DRM_IOCTL_ARGS);

/* AGP/GART support (drm_agpsupport.c) */
int	drm_agp_acquire_ioctl(DRM_IOCTL_ARGS);
int	drm_agp_release_ioctl(DRM_IOCTL_ARGS);
int	drm_agp_enable_ioctl(DRM_IOCTL_ARGS);
int	drm_agp_info_ioctl(DRM_IOCTL_ARGS);
int	drm_agp_alloc_ioctl(DRM_IOCTL_ARGS);
int	drm_agp_free_ioctl(DRM_IOCTL_ARGS);
int	drm_agp_unbind_ioctl(DRM_IOCTL_ARGS);
int	drm_agp_bind_ioctl(DRM_IOCTL_ARGS);

/* Scatter Gather Support (drm_scatter.c) */
int	drm_sg_alloc(DRM_IOCTL_ARGS);
int	drm_sg_free(DRM_IOCTL_ARGS);

/* consistent PCI memory functions (drm_pci.c) */
drm_dma_handle_t *drm_pci_alloc(drm_device_t *dev, size_t size, size_t align,
				dma_addr_t maxaddr);
void	drm_pci_free(drm_device_t *dev, drm_dma_handle_t *dmah);

/* Inline replacements for DRM_IOREMAP macros */
static __inline__ void drm_core_ioremap(struct drm_local_map *map, struct drm_device *dev)
{
	map->handle = drm_ioremap(dev, map);
}
static __inline__ void drm_core_ioremapfree(struct drm_local_map *map, struct drm_device *dev)
{
	if ( map->handle && map->size )
		drm_ioremapfree(map);
}

static __inline__ struct drm_local_map *drm_core_findmap(struct drm_device *dev, unsigned long offset)
{
	drm_local_map_t *map;

	DRM_SPINLOCK_ASSERT(&dev->dev_lock);
	TAILQ_FOREACH(map, &dev->maplist, link) {
		if (map->offset == offset)
			return map;
	}
	return NULL;
}

static __inline__ void drm_core_dropmap(struct drm_map *map)
{
}

#endif /* __KERNEL__ */
#endif /* _DRM_P_H_ */
