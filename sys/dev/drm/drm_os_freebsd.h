/*
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/lockmgr.h>
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
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#if __FreeBSD_version >= 500000
#include <sys/selinfo.h>
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

#define __REALLY_HAVE_MTRR	0
#define __REALLY_HAVE_SG	0

#if __REALLY_HAVE_AGP
#include <pci/agpvar.h>
#endif

/* Allow setting of debug code enabling from kernel config file */
#include <opt_drm.h>
#if DRM_DEBUG
#undef  DRM_DEBUG_CODE
#define DRM_DEBUG_CODE 2
#endif
#undef DRM_DEBUG

#define DRM_TIME_SLICE	      (hz/20)  /* Time slice for GLXContexts	  */

#define DRM_DEV_MODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)
#define DRM_DEV_UID	0
#define DRM_DEV_GID	0

#if __FreeBSD_version >= 500000
#define DRM_OS_SPINTYPE		struct mtx
#define DRM_OS_SPININIT(l,name)	mtx_init(&l, name, NULL, MTX_DEF)
#define DRM_OS_SPINLOCK(l)	mtx_lock(l)
#define DRM_OS_SPINUNLOCK(u)	mtx_unlock(u);
#define DRM_OS_CURPROC		curthread
#define DRM_OS_STRUCTPROC	struct thread
#define DRM_OS_CURRENTPID	curthread->td_proc->p_pid
#else
#define DRM_OS_CURPROC		curproc
#define DRM_OS_STRUCTPROC	struct proc
#define DRM_OS_SPINTYPE		struct simplelock
#define DRM_OS_SPININIT(l,name)	simple_lock_init(&l)
#define DRM_OS_SPINLOCK(l)	simple_lock(l)
#define DRM_OS_SPINUNLOCK(u)	simple_unlock(u);
#define DRM_OS_CURRENTPID	curproc->p_pid
#endif

#define DRM_OS_IOCTL 		dev_t kdev, u_long cmd, caddr_t data, int flags, DRM_OS_STRUCTPROC *p
#define IOCTL_ARGS_PASS		kdev, cmd, data, flags, p
#define DRM_OS_LOCK		lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, DRM_OS_CURPROC)
#define DRM_OS_UNLOCK 		lockmgr(&dev->dev_lock, LK_RELEASE, 0, DRM_OS_CURPROC)
#define DRM_OS_CHECKSUSER	suser(p)
#define DRM_OS_TASKQUEUE_ARGS	void *dev, int pending
#define DRM_OS_IRQ_ARGS		void *device
#define DRM_OS_DEVICE		drm_device_t	*dev	= kdev->si_drv1
#define DRM_OS_MALLOC(size)	malloc( size, DRM(M_DRM), M_NOWAIT )
#define DRM_OS_FREE(pt)		free( pt, DRM(M_DRM) )
#define DRM_OS_VTOPHYS(addr)	vtophys(addr)

#define DRM_OS_PRIV					\
	drm_file_t	*priv	= (drm_file_t *) DRM(find_file_by_proc)(dev, p); \
	if (!priv) {						\
		DRM_DEBUG("can't find authenticator\n");	\
		return EINVAL;					\
	}

#define DRM_OS_DELAY( udelay )					\
do {								\
	struct timeval tv1, tv2;				\
	microtime(&tv1);					\
	do {							\
		microtime(&tv2);				\
	}							\
	while (((tv2.tv_sec-tv1.tv_sec)*1000000 + tv2.tv_usec - tv1.tv_usec) < udelay ); \
} while (0)

#define DRM_OS_ERR(v)	v


#define DRM_OS_KRNTOUSR( user, kern, size)	\
	if ( IOCPARM_LEN(cmd) != size)			\
		return EINVAL;				\
	*user = kern;
#define DRM_OS_KRNFROMUSR(kern, user, size) \
	if ( IOCPARM_LEN(cmd) != size)			\
		return EINVAL;				\
	kern = *user;
#define DRM_OS_COPYTOUSR(user, kern, size) \
	copyout(kern, user, size)
#define DRM_OS_COPYFROMUSR(kern, user, size) \
	copyin(user, kern, size)

#define DRM_OS_READMEMORYBARRIER \
{												\
   	int xchangeDummy;									\
	DRM_DEBUG("%s\n", __func__);							\
   	__asm__ volatile(" push %%eax ; xchg %%eax, %0 ; pop %%eax" : : "m" (xchangeDummy));	\
   	__asm__ volatile(" push %%eax ; push %%ebx ; push %%ecx ; push %%edx ;"			\
			 " movl $0,%%eax ; cpuid ; pop %%edx ; pop %%ecx ; pop %%ebx ;"		\
			 " pop %%eax" : /* no outputs */ :  /* no inputs */ );			\
} while (0);

#define DRM_OS_WRITEMEMORYBARRIER DRM_OS_READMEMORYBARRIER

#define DRM_OS_WAKEUP(w) wakeup(w)
#define DRM_OS_WAKEUP_INT(w) wakeup(w)

#define PAGE_ALIGN(addr) (((addr)+PAGE_SIZE-1)&PAGE_MASK)

#define malloctype DRM(M_DRM)
/* The macros confliced in the MALLOC_DEFINE */
MALLOC_DECLARE(malloctype);
#undef malloctype

typedef struct drm_chipinfo
{
	int vendor;
	int device;
	int supported;
	char *name;
} drm_chipinfo_t;

typedef unsigned long atomic_t;
typedef u_int32_t cycles_t;
typedef u_int32_t spinlock_t;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
#define atomic_set(p, v)	(*(p) = (v))
#define atomic_read(p)		(*(p))
#define atomic_inc(p)		atomic_add_long(p, 1)
#define atomic_dec(p)		atomic_subtract_long(p, 1)
#define atomic_add(n, p)	atomic_add_long(p, n)
#define atomic_sub(n, p)	atomic_subtract_long(p, n)

/* Fake this */
static __inline unsigned int
test_and_set_bit(int b, volatile unsigned long *p)
{
	int s = splhigh();
	unsigned int m = 1<<b;
	unsigned int r = *p & m;
	*p |= m;
	splx(s);
	return r;
}

static __inline void
clear_bit(int b, volatile unsigned long *p)
{
    atomic_clear_long(p + (b >> 5), 1 << (b & 0x1f));
}

static __inline void
set_bit(int b, volatile unsigned long *p)
{
    atomic_set_long(p + (b >> 5), 1 << (b & 0x1f));
}

static __inline int
test_bit(int b, volatile unsigned long *p)
{
    return p[b >> 5] & (1 << (b & 0x1f));
}

static __inline int
find_first_zero_bit(volatile unsigned long *p, int max)
{
    int b;

    for (b = 0; b < max; b += 32) {
	if (p[b >> 5] != ~0) {
	    for (;;) {
		if ((p[b >> 5] & (1 << (b & 0x1f))) == 0)
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
/* FIXME: again, what's the exact date? */
#define MODULE_VERSION(a,b)		struct __hack
#define MODULE_DEPEND(a,b,c,d,e)	struct __hack

#endif

#define __drm_dummy_lock(lock) (*(__volatile__ unsigned int *)lock)
#define _DRM_CAS(lock,old,new,__ret)				       \
	do {							       \
		int __dummy;	/* Can't mark eax as clobbered */      \
		__asm__ __volatile__(				       \
			"lock ; cmpxchg %4,%1\n\t"		       \
			"setnz %0"				       \
			: "=d" (__ret),				       \
			  "=m" (__drm_dummy_lock(lock)),	       \
			  "=a" (__dummy)			       \
			: "2" (old),				       \
			  "r" (new));				       \
	} while (0)

/* Redefinitions to make templating easy */
#define wait_queue_head_t	long
#define agp_memory		void
#define jiffies			ticks

				/* Macros to make printf easier */
#define DRM_ERROR(fmt, arg...) \
	printf("error: [" DRM_NAME ":%s] *ERROR* " fmt , \
		__func__, ##arg)
#define DRM_MEM_ERROR(area, fmt, arg...) \
	printf("error: [" DRM_NAME ":%s:%s] *ERROR* " fmt , \
	       __func__, DRM(mem_stats)[area].name , ##arg)
#define DRM_INFO(fmt, arg...)  printf("info: " "[" DRM_NAME "] " fmt , ##arg)

#if DRM_DEBUG_CODE
#define DRM_DEBUG(fmt, arg...)						\
	do {								\
		if (DRM(flags) & DRM_FLAG_DEBUG)			\
			printf("[" DRM_NAME ":%s] " fmt , 		\
				__func__, ##arg);			\
	} while (0)
#else
#define DRM_DEBUG(fmt, arg...)		 do { } while (0)
#endif

#define DRM_PROC_LIMIT (PAGE_SIZE-80)

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
extern d_open_t		DRM(open);
extern d_close_t	DRM(close);
extern d_read_t		DRM(read);
extern d_write_t	DRM(write);
extern d_poll_t		DRM(poll);
extern d_mmap_t		DRM(mmap);
extern int		DRM(open_helper)(dev_t kdev, int flags, int fmt, 
					 DRM_OS_STRUCTPROC *p, drm_device_t *dev);
extern drm_file_t	*DRM(find_file_by_proc)(drm_device_t *dev, 
					 DRM_OS_STRUCTPROC *p);

/* Memory management support (drm_memory.h) */
extern int		DRM(mem_info) DRM_SYSCTL_HANDLER_ARGS;

/* SysCtl Support (drm_sysctl.h) */
extern int		DRM(sysctl_init)(drm_device_t *dev);
extern int		DRM(sysctl_cleanup)(drm_device_t *dev);
