/*
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
#include <sys/mman.h>
#include <sys/rman.h>
#include <sys/memrange.h>
#include <pci/pcivar.h>
#if __FreeBSD_version >= 500000
#include <sys/selinfo.h>
#else
#include <sys/select.h>
#endif
#include <sys/bus.h>
#if __FreeBSD_version >= 400005
#include <sys/taskqueue.h>
#endif
#if __FreeBSD_version >= 500000
#include <sys/mutex.h>
#endif

#if __FreeBSD_version >= 400006
#define __REALLY_HAVE_AGP	__HAVE_AGP
#endif

#ifdef __i386__
#define __REALLY_HAVE_MTRR	(__HAVE_MTRR) && (__FreeBSD_version >= 500000)
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
#define CDEV_MAJOR	145

#if __FreeBSD_version >= 500000
#define DRM_CURPROC		curthread
#define DRM_STRUCTPROC		struct thread
#define DRM_SPINTYPE		struct mtx
#define DRM_SPININIT(l,name)	mtx_init(&l, name, NULL, MTX_DEF)
#define DRM_SPINUNINIT(l)	mtx_destroy(&l)
#define DRM_SPINLOCK(l)		mtx_lock(l)
#define DRM_SPINUNLOCK(u)	mtx_unlock(u);
#define DRM_CURRENTPID		curthread->td_proc->p_pid
#else
#define DRM_CURPROC		curproc
#define DRM_STRUCTPROC		struct proc
#define DRM_SPINTYPE		struct simplelock
#define DRM_SPININIT(l,name)	simple_lock_init(&l)
#define DRM_SPINUNINIT(l)
#define DRM_SPINLOCK(l)		simple_lock(l)
#define DRM_SPINUNLOCK(u)	simple_unlock(u);
#define DRM_CURRENTPID		curproc->p_pid
#endif

/* Currently our DRMFILE (filp) is a void * which is actually the pid
 * of the current process.  It should be a per-open unique pointer, but
 * code for that is not yet written */
#define DRMFILE			void *
#define DRM_IOCTL_ARGS		dev_t kdev, u_long cmd, caddr_t data, int flags, DRM_STRUCTPROC *p, DRMFILE filp
#define DRM_LOCK		lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, DRM_CURPROC)
#define DRM_UNLOCK 		lockmgr(&dev->dev_lock, LK_RELEASE, 0, DRM_CURPROC)
#define DRM_SUSER(p)		suser(p)
#define DRM_TASKQUEUE_ARGS	void *arg, int pending
#define DRM_IRQ_ARGS		void *arg
#define DRM_DEVICE		drm_device_t	*dev	= kdev->si_drv1
#define DRM_MALLOC(size)	malloc( size, DRM(M_DRM), M_NOWAIT )
#define DRM_FREE(pt,size)		free( pt, DRM(M_DRM) )
#define DRM_VTOPHYS(addr)	vtophys(addr)

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

#define DRM_PRIV					\
	drm_file_t	*priv	= (drm_file_t *) DRM(find_file_by_proc)(dev, p); \
	if (!priv) {						\
		DRM_DEBUG("can't find authenticator\n");	\
		return EINVAL;					\
	}

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

#define DRM_WAIT_ON( ret, queue, timeout, condition )			\
for ( ret = 0 ; !ret && !(condition) ; ) {		\
        int s = spldrm();				\
	if (!(condition))				\
	   ret = tsleep( &(queue), PZERO | PCATCH, 	\
			 "drmwtq", (timeout) );		\
	splx(s);					\
}

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
#define DRM_GET_USER_UNCHECKED(val, uaddr)			\
	((val) = fuword(uaddr), 0)

#define DRM_WRITEMEMORYBARRIER( map )					\
	bus_space_barrier((map)->iot, (map)->ioh, 0, (map)->size, 0);
#define DRM_READMEMORYBARRIER( map )					\
	bus_space_barrier((map)->iot, (map)->ioh, 0, (map)->size, BUS_SPACE_BARRIER_READ);

#define PAGE_ALIGN(addr) round_page(addr)

#ifndef M_WAITOK		/* M_WAITOK (=0) name removed in -current */
#define M_WAITOK 0
#endif

#define malloctype DRM(M_DRM)
/* The macros conflicted in the MALLOC_DEFINE */
MALLOC_DECLARE(malloctype);
#undef malloctype

typedef struct drm_chipinfo
{
	int vendor;
	int device;
	int supported;
	char *name;
} drm_chipinfo_t;

#define cpu_to_le32(x) (x)	/* FIXME */

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

#define memset(p, v, s)		bzero(p, s)

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
	printf("error: " "[" DRM_NAME ":%s] *ERROR* " fmt , __func__ , ## arg)
#define DRM_MEM_ERROR(area, fmt, arg...) \
	printf("error: " "[" DRM_NAME ":%s:%s] *ERROR* " fmt , \
		__func__, DRM(mem_stats)[area].name , ##arg)
#define DRM_INFO(fmt, arg...)  printf("info: " "[" DRM_NAME "] " fmt , ## arg)

#if DRM_DEBUG_CODE
#define DRM_DEBUG(fmt, arg...)						  \
	do {								  \
		if (DRM(flags) & DRM_FLAG_DEBUG)			  \
			printf("[" DRM_NAME ":%s] " fmt , __func__ , ## arg); \
	} while (0)
#else
#define DRM_DEBUG(fmt, arg...)		 do { } while (0)
#endif

#if (__FreeBSD_version >= 500000) || ((__FreeBSD_version < 500000) && (__FreeBSD_version >= 410002))
#define DRM_SYSCTL_HANDLER_ARGS	(SYSCTL_HANDLER_ARGS)
#else
#define DRM_SYSCTL_HANDLER_ARGS	SYSCTL_HANDLER_ARGS
#endif

#define DRM_SYSCTL_PRINT(fmt, arg...)		\
  snprintf(buf, sizeof(buf), fmt, ##arg);	\
  error = SYSCTL_OUT(req, buf, strlen(buf));	\
  if (error) return error;

#define DRM_SYSCTL_PRINT_RET(ret, fmt, arg...)	\
  snprintf(buf, sizeof(buf), fmt, ##arg);	\
  error = SYSCTL_OUT(req, buf, strlen(buf));	\
  if (error) { ret; return error; }


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
extern int		DRM(open_helper)(dev_t kdev, int flags, int fmt, 
					 DRM_STRUCTPROC *p, drm_device_t *dev);
extern drm_file_t	*DRM(find_file_by_proc)(drm_device_t *dev, 
					 DRM_STRUCTPROC *p);

/* sysctl support (drm_sysctl.h) */
extern int		DRM(sysctl_init)(drm_device_t *dev);
extern int		DRM(sysctl_cleanup)(drm_device_t *dev);

/* Memory info sysctl (drm_memory.h) */
extern int		DRM(mem_info) DRM_SYSCTL_HANDLER_ARGS;
