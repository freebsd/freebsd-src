/**
 * \file drm_os_freebsd.h
 * OS-specific #defines for FreeBSD
 * 
 * \author Eric Anholt <anholt@FreeBSD.org>
 */

/*-
 * Copyright 2003 Eric Anholt
 * All Rights Reserved.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/stat.h>
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
#if __FreeBSD_version >= 480000
#include <sys/endian.h>
#endif
#include <sys/mman.h>
#include <sys/rman.h>
#include <sys/memrange.h>
#if __FreeBSD_version >= 500000
#include <dev/pci/pcivar.h>
#include <sys/selinfo.h>
#else
#include <pci/pcivar.h>
#include <sys/select.h>
#endif
#include <sys/bus.h>
#if __FreeBSD_version >= 400005
#include <sys/taskqueue.h>
#endif
#if __FreeBSD_version >= 500000
#include <sys/mutex.h>
#endif

#include "dev/drm/drm_linux_list.h"

#if __FreeBSD_version >= 400006
#define __REALLY_HAVE_AGP	__HAVE_AGP
#endif

#ifdef __i386__
#define __REALLY_HAVE_MTRR	(__HAVE_MTRR) && (__FreeBSD_version >= 460000)
#else
#define __REALLY_HAVE_MTRR	0
#endif

#define __REALLY_HAVE_SG	(__HAVE_SG)

#if __REALLY_HAVE_AGP
#include <pci/agpvar.h>
#include <sys/agpio.h>
#endif

#include <opt_drm.h>
#if DRM_DEBUG
#undef  DRM_DEBUG_CODE
#define DRM_DEBUG_CODE 2
#endif
#undef DRM_DEBUG

#if DRM_LINUX
#include <sys/file.h>
#include <sys/proc.h>
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#define DRM_TIME_SLICE	      (hz/20)  /* Time slice for GLXContexts	  */

#define DRM_DEV_MODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)
#define DRM_DEV_UID	0
#define DRM_DEV_GID	0

#if __FreeBSD_version >= 500000
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
#else
/* There is no need for locking on FreeBSD 4.x.  Synchronization is handled by
 * the fact that there is no reentrancy of the kernel except for interrupt
 * handlers, and the interrupt handler synchronization is managed by spls.
 */
#define DRM_CURPROC		curproc
#define DRM_STRUCTPROC		struct proc
#define DRM_SPINTYPE		
#define DRM_SPININIT(l,name)
#define DRM_SPINUNINIT(l)
#define DRM_SPINLOCK(l)
#define DRM_SPINUNLOCK(u)
#define DRM_SPINLOCK_ASSERT(l)
#define DRM_CURRENTPID		curproc->p_pid
#define DRM_LOCK()
#define DRM_UNLOCK()
#endif

/* Currently our DRMFILE (filp) is a void * which is actually the pid
 * of the current process.  It should be a per-open unique pointer, but
 * code for that is not yet written */
#define DRMFILE			void *
#define DRM_IOCTL_ARGS		struct cdev *kdev, u_long cmd, caddr_t data, int flags, DRM_STRUCTPROC *p, DRMFILE filp
#define DRM_SUSER(p)		suser(p)
#define DRM_TASKQUEUE_ARGS	void *arg, int pending
#define DRM_IRQ_ARGS		void *arg
typedef void			irqreturn_t;
#define IRQ_HANDLED		/* nothing */
#define IRQ_NONE		/* nothing */
#define DRM_DEVICE		drm_device_t	*dev	= kdev->si_drv1
#define DRM_MALLOC(size)	malloc( size, DRM(M_DRM), M_NOWAIT )
#define DRM_FREE(pt,size)		free( pt, DRM(M_DRM) )

/* Read/write from bus space, with byteswapping to le if necessary */
#define DRM_READ8(map, offset)		*(volatile u_int8_t *) (((unsigned long)(map)->handle) + (offset))
#define DRM_READ32(map, offset)		*(volatile u_int32_t *)(((unsigned long)(map)->handle) + (offset))
#define DRM_WRITE8(map, offset, val)	*(volatile u_int8_t *) (((unsigned long)(map)->handle) + (offset)) = val
#define DRM_WRITE32(map, offset, val)	*(volatile u_int32_t *)(((unsigned long)(map)->handle) + (offset)) = val
/*
#define DRM_READ8(map, offset)		bus_space_read_1(  (map)->iot, (map)->ioh, (offset) )
#define DRM_READ32(map, offset)		bus_space_read_4(  (map)->iot, (map)->ioh, (offset) )
#define DRM_WRITE8(map, offset, val)	bus_space_write_1( (map)->iot, (map)->ioh, (offset), (val) )
#define DRM_WRITE32(map, offset, val)	bus_space_write_4( (map)->iot, (map)->ioh, (offset), (val) )
*/
#define DRM_AGP_FIND_DEVICE()	agp_find_device()
#define DRM_ERR(v)		v

#define DRM_MTRR_WC	MDF_WRITECOMBINE

#define DRM_GET_PRIV_WITH_RETURN(_priv, _filp)			\
do {								\
	if (_filp != (DRMFILE)(intptr_t)DRM_CURRENTPID) {	\
		DRM_ERROR("filp doesn't match curproc\n");	\
		return EINVAL;					\
	}							\
	DRM_LOCK();						\
	_priv = DRM(find_file_by_proc)(dev, DRM_CURPROC);	\
	DRM_UNLOCK();						\
	if (_priv == NULL) {					\
		DRM_DEBUG("can't find authenticator\n");	\
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

#define DRM_UDELAY( udelay )					\
do {								\
	struct timeval tv1, tv2;				\
	microtime(&tv1);					\
	do {							\
		microtime(&tv2);				\
	}							\
	while (((tv2.tv_sec-tv1.tv_sec)*1000000 + tv2.tv_usec - tv1.tv_usec) < udelay ); \
} while (0)

#define DRM_GETSAREA()					\
do {								\
	drm_map_list_entry_t *listentry;			\
	TAILQ_FOREACH(listentry, dev->maplist, link) {		\
		drm_local_map_t *map = listentry->map;		\
		if (map->type == _DRM_SHM &&			\
			map->flags & _DRM_CONTAINS_LOCK) {	\
			dev_priv->sarea = map;			\
			break;					\
		}						\
	}							\
} while (0)

#define DRM_HZ hz

#if defined(__FreeBSD__) && __FreeBSD_version > 500000
#define DRM_WAIT_ON( ret, queue, timeout, condition )		\
for ( ret = 0 ; !ret && !(condition) ; ) {			\
	mtx_lock(&dev->irq_lock);				\
	if (!(condition))					\
	   ret = msleep(&(queue), &dev->irq_lock, 	\
			 PZERO | PCATCH, "drmwtq", (timeout));	\
	mtx_unlock(&dev->irq_lock);			\
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

#define DRM_WAKEUP( queue ) wakeup( queue )
#define DRM_WAKEUP_INT( queue ) wakeup( queue )
#define DRM_INIT_WAITQUEUE( queue )  do {} while (0)

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
/* Macros for userspace access with checking readability once */
/* FIXME: can't find equivalent functionality for nocheck yet.
 * It'll be slower than linux, but should be correct.
 */
#define DRM_VERIFYAREA_READ( uaddr, size )		\
	(!useracc((caddr_t)uaddr, size, VM_PROT_READ))
#define DRM_COPY_FROM_USER_UNCHECKED(arg1, arg2, arg3) 	\
	copyin(arg2, arg1, arg3)
#define DRM_COPY_TO_USER_UNCHECKED(arg1, arg2, arg3)	\
	copyout(arg2, arg1, arg3)
#define DRM_GET_USER_UNCHECKED(val, uaddr)			\
	((val) = fuword32(uaddr), 0)
#define DRM_PUT_USER_UNCHECKED(uaddr, val)			\
	suword32(uaddr, val)

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

#define PAGE_ALIGN(addr) round_page(addr)

#ifndef M_WAITOK		/* M_WAITOK (=0) name removed in -current */
#define M_WAITOK 0
#endif

#define malloctype DRM(M_DRM)
/* The macros conflicted in the MALLOC_DEFINE */
MALLOC_DECLARE(malloctype);
#undef malloctype

#if __FreeBSD_version < 502109
#define bus_alloc_resource_any(dev, type, rid, flags) \
	bus_alloc_resource(dev, type, rid, 0ul, ~0ul, 1, flags)
#endif

#if __FreeBSD_version >= 480000
#define cpu_to_le32(x) htole32(x)
#define le32_to_cpu(x) le32toh(x)
#else
#define cpu_to_le32(x) (x)
#define le32_to_cpu(x) (x)
#endif

typedef unsigned long dma_addr_t;
typedef u_int32_t atomic_t;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
#define atomic_set(p, v)	(*(p) = (v))
#define atomic_read(p)		(*(p))
#define atomic_inc(p)		atomic_add_int(p, 1)
#define atomic_dec(p)		atomic_subtract_int(p, 1)
#define atomic_add(n, p)	atomic_add_int(p, n)
#define atomic_sub(n, p)	atomic_subtract_int(p, n)

/* Fake this */

#if __FreeBSD_version < 500000
/* The extra atomic functions from 5.0 haven't been merged to 4.x */
static __inline int
atomic_cmpset_int(volatile u_int *dst, u_int exp, u_int src)
{
	int res = exp;

	__asm __volatile (
	"	lock ;			"
	"	cmpxchgl %1,%2 ;	"
	"       setz	%%al ;		"
	"	movzbl	%%al,%0 ;	"
	"1:				"
	"# atomic_cmpset_int"
	: "+a" (res)			/* 0 (result) */
	: "r" (src),			/* 1 */
	  "m" (*(dst))			/* 2 */
	: "memory");				 

	return (res);
}
#endif

static __inline atomic_t
test_and_set_bit(int b, volatile void *p)
{
	int s = splhigh();
	unsigned int m = 1<<b;
	unsigned int r = *(volatile int *)p & m;
	*(volatile int *)p |= m;
	splx(s);
	return r;
}

static __inline void
clear_bit(int b, volatile void *p)
{
    atomic_clear_int(((volatile int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static __inline void
set_bit(int b, volatile void *p)
{
    atomic_set_int(((volatile int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static __inline int
test_bit(int b, volatile void *p)
{
    return ((volatile int *)p)[b >> 5] & (1 << (b & 0x1f));
}

static __inline int
find_first_zero_bit(volatile void *p, int max)
{
    int b;

    for (b = 0; b < max; b += 32) {
	if (((volatile int *)p)[b >> 5] != ~0) {
	    for (;;) {
		if ((((volatile int *)p)[b >> 5] & (1 << (b & 0x1f))) == 0)
		    return b;
		b++;
	    }
	}
    }
    return max;
}

#define spldrm()		spltty()

/*
 * Fake out the module macros for versions of FreeBSD where they don't
 * exist.
 */
#if (__FreeBSD_version < 500002 && __FreeBSD_version > 500000) || __FreeBSD_version < 420000
#define MODULE_VERSION(a,b)		struct __hack
#define MODULE_DEPEND(a,b,c,d,e)	struct __hack
#endif

/* Redefinitions to make templating easy */
#define wait_queue_head_t	atomic_t
#define agp_memory		void
#define jiffies			ticks

				/* Macros to make printf easier */
#define DRM_ERROR(fmt, arg...) \
	printf("error: [" DRM_NAME ":pid%d:%s] *ERROR* " fmt,		\
	    DRM_CURRENTPID, __func__ , ## arg)

#define DRM_MEM_ERROR(area, fmt, arg...) \
	printf("error: [" DRM_NAME ":pid%d:%s:%s] *ERROR* " fmt,	\
	    DRM_CURRENTPID , __func__, DRM(mem_stats)[area].name , ##arg)

#define DRM_INFO(fmt, arg...)  printf("info: [" DRM_NAME "] " fmt , ## arg)

#if DRM_DEBUG_CODE
#define DRM_DEBUG(fmt, arg...)						\
	do {								\
		if (DRM(flags) & DRM_FLAG_DEBUG)			\
			printf("[" DRM_NAME ":pid%d:%s] " fmt,		\
			    DRM_CURRENTPID, __func__ , ## arg);		\
	} while (0)
#else
#define DRM_DEBUG(fmt, arg...)		 do { } while (0)
#endif

#if (__FreeBSD_version >= 500000) || ((__FreeBSD_version < 500000) && (__FreeBSD_version >= 410002))
#define DRM_SYSCTL_HANDLER_ARGS	(SYSCTL_HANDLER_ARGS)
#else
#define DRM_SYSCTL_HANDLER_ARGS	SYSCTL_HANDLER_ARGS
#endif

#define DRM_FIND_MAP(dest, o)						\
	do {								\
		drm_map_list_entry_t *listentry;			\
		TAILQ_FOREACH(listentry, dev->maplist, link) {		\
			if ( listentry->map->offset == o ) {		\
				dest = listentry->map;			\
				break;					\
			}						\
		}							\
	} while (0)


/* Internal functions */

/* drm_drv.h */
extern d_ioctl_t	DRM(ioctl);
extern d_open_t		DRM(open);
extern d_close_t	DRM(close);
extern d_read_t		DRM(read);
extern d_poll_t		DRM(poll);
extern d_mmap_t		DRM(mmap);
extern int		DRM(open_helper)(struct cdev *kdev, int flags, int fmt, 
					 DRM_STRUCTPROC *p, drm_device_t *dev);
extern drm_file_t	*DRM(find_file_by_proc)(drm_device_t *dev, 
					 DRM_STRUCTPROC *p);

/* sysctl support (drm_sysctl.h) */
extern int		DRM(sysctl_init)(drm_device_t *dev);
extern int		DRM(sysctl_cleanup)(drm_device_t *dev);

/* Memory info sysctl (drm_memory_debug.h) */
#ifdef DEBUG_MEMORY
extern int		DRM(mem_info) DRM_SYSCTL_HANDLER_ARGS;
#endif
