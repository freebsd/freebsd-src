/**
 * \file drm_os_freebsd.h
 * OS abstraction macros.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef _DRM_OS_FREEBSD_H_
#define	_DRM_OS_FREEBSD_H_

#include <sys/fbio.h>

#if _BYTE_ORDER == _BIG_ENDIAN
#define	__BIG_ENDIAN 4321
#else
#define	__LITTLE_ENDIAN 1234
#endif

#ifdef __LP64__
#define	BITS_PER_LONG	64
#else
#define	BITS_PER_LONG	32
#endif

#ifndef __user
#define __user
#endif
#ifndef __iomem
#define __iomem
#endif

#define	cpu_to_le16(x)	htole16(x)
#define	le16_to_cpu(x)	le16toh(x)
#define	cpu_to_le32(x)	htole32(x)
#define	le32_to_cpu(x)	le32toh(x)

#define	cpu_to_be16(x)	htobe16(x)
#define	be16_to_cpu(x)	be16toh(x)
#define	cpu_to_be32(x)	htobe32(x)
#define	be32_to_cpu(x)	be32toh(x)
#define	be32_to_cpup(x)	be32toh(*x)

typedef vm_paddr_t dma_addr_t;
typedef vm_paddr_t resource_size_t;
#define	wait_queue_head_t atomic_t

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t  s8;
typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;
typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;

#define	DRM_IRQ_ARGS		void *arg
typedef void			irqreturn_t;
#define	IRQ_HANDLED		/* nothing */
#define	IRQ_NONE		/* nothing */

#define	__init
#define	__exit
#define	__read_mostly

#define	WARN_ON(cond)		KASSERT(!(cond), ("WARN ON: " #cond))
#define	WARN_ON_SMP(cond)	WARN_ON(cond)
#define	BUG_ON(cond)		KASSERT(!(cond), ("BUG ON: " #cond))
#define	unlikely(x)            __builtin_expect(!!(x), 0)
#define	likely(x)              __builtin_expect(!!(x), 1)
#define	container_of(ptr, type, member) ({			\
	__typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define	KHZ2PICOS(a)	(1000000000UL/(a))

#define ARRAY_SIZE(x)		(sizeof(x)/sizeof(x[0]))

#define	HZ			hz
#define	DRM_HZ			hz
#define	DRM_CURRENTPID		curthread->td_proc->p_pid
#define	DRM_SUSER(p)		(priv_check(p, PRIV_DRIVER) == 0)
#define	udelay(usecs)		DELAY(usecs)
#define	mdelay(msecs)		do { int loops = (msecs);		\
				  while (loops--) DELAY(1000);		\
				} while (0)
#define	DRM_UDELAY(udelay)	DELAY(udelay)
#define	drm_msleep(x, msg)	pause((msg), ((int64_t)(x)) * hz / 1000)
#define	DRM_MSLEEP(msecs)	drm_msleep((msecs), "drm_msleep")

#define	DRM_READ8(map, offset)						\
	*(volatile u_int8_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset))
#define	DRM_READ16(map, offset)						\
	le16toh(*(volatile u_int16_t *)(((vm_offset_t)(map)->handle) +	\
	    (vm_offset_t)(offset)))
#define	DRM_READ32(map, offset)						\
	le32toh(*(volatile u_int32_t *)(((vm_offset_t)(map)->handle) +	\
	    (vm_offset_t)(offset)))
#define	DRM_READ64(map, offset)						\
	le64toh(*(volatile u_int64_t *)(((vm_offset_t)(map)->handle) +	\
	    (vm_offset_t)(offset)))
#define	DRM_WRITE8(map, offset, val)					\
	*(volatile u_int8_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = val
#define	DRM_WRITE16(map, offset, val)					\
	*(volatile u_int16_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = htole16(val)
#define	DRM_WRITE32(map, offset, val)					\
	*(volatile u_int32_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = htole32(val)
#define	DRM_WRITE64(map, offset, val)					\
	*(volatile u_int64_t *)(((vm_offset_t)(map)->handle) +		\
	    (vm_offset_t)(offset)) = htole64(val)

/* DRM_READMEMORYBARRIER() prevents reordering of reads.
 * DRM_WRITEMEMORYBARRIER() prevents reordering of writes.
 * DRM_MEMORYBARRIER() prevents reordering of reads and writes.
 */
#define	DRM_READMEMORYBARRIER()		rmb()
#define	DRM_WRITEMEMORYBARRIER()	wmb()
#define	DRM_MEMORYBARRIER()		mb()
#define	smp_rmb()			rmb()
#define	smp_mb__before_atomic_inc()	mb()
#define	smp_mb__after_atomic_inc()	mb()

#define	do_div(a, b)		((a) /= (b))
#define	div64_u64(a, b)		((a) / (b))
#define	lower_32_bits(n)	((u32)(n))

#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1 : __min2; })

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1 : __max2; })

#define	memset_io(a, b, c)	memset((a), (b), (c))
#define	memcpy_fromio(a, b, c)	memcpy((a), (b), (c))
#define	memcpy_toio(a, b, c)	memcpy((a), (b), (c))

/* XXXKIB what is the right code for the FreeBSD ? */
/* kib@ used ENXIO here -- dumbbell@ */
#define	EREMOTEIO	EIO
#define	ERESTARTSYS	512 /* Same value as Linux. */

#define	KTR_DRM		KTR_DEV
#define	KTR_DRM_REG	KTR_SPARE3

#define	DRM_AGP_KERN	struct agp_info
#define	DRM_AGP_MEM	void

#define	PCI_VENDOR_ID_APPLE		0x106b
#define	PCI_VENDOR_ID_ASUSTEK		0x1043
#define	PCI_VENDOR_ID_ATI		0x1002
#define	PCI_VENDOR_ID_DELL		0x1028
#define	PCI_VENDOR_ID_HP		0x103c
#define	PCI_VENDOR_ID_IBM		0x1014
#define	PCI_VENDOR_ID_INTEL		0x8086
#define	PCI_VENDOR_ID_SERVERWORKS	0x1166
#define	PCI_VENDOR_ID_SONY		0x104d
#define	PCI_VENDOR_ID_VIA		0x1106

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define	hweight32(i)	bitcount32(i)

static inline unsigned long
roundup_pow_of_two(unsigned long x)
{

	return (1UL << flsl(x - 1));
}

/**
 * ror32 - rotate a 32-bit value right
 * @word: value to rotate
 * @shift: bits to roll
 *
 * Source: include/linux/bitops.h
 */
static inline uint32_t
ror32(uint32_t word, unsigned int shift)
{

	return (word >> shift) | (word << (32 - shift));
}

#define	IS_ALIGNED(x, y)	(((x) & ((y) - 1)) == 0)
#define	get_unaligned(ptr)                                              \
	({ __typeof__(*(ptr)) __tmp;                                    \
	  memcpy(&__tmp, (ptr), sizeof(*(ptr))); __tmp; })

#if _BYTE_ORDER == _LITTLE_ENDIAN
/* Taken from linux/include/linux/unaligned/le_struct.h. */
struct __una_u32 { u32 x; } __packed;

static inline u32
__get_unaligned_cpu32(const void *p)
{
	const struct __una_u32 *ptr = (const struct __una_u32 *)p;

	return (ptr->x);
}

static inline u32
get_unaligned_le32(const void *p)
{

	return (__get_unaligned_cpu32((const u8 *)p));
}
#else
/* Taken from linux/include/linux/unaligned/le_byteshift.h. */
static inline u32
__get_unaligned_le32(const u8 *p)
{

	return (p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24);
}

static inline u32
get_unaligned_le32(const void *p)
{

	return (__get_unaligned_le32((const u8 *)p));
}
#endif

static inline unsigned long
ilog2(unsigned long x)
{

	return (flsl(x) - 1);
}

static inline int64_t
abs64(int64_t x)
{

	return (x < 0 ? -x : x);
}

int64_t		timeval_to_ns(const struct timeval *tv);
struct timeval	ns_to_timeval(const int64_t nsec);

#define PAGE_ALIGN(addr) round_page(addr)

#define	drm_get_device_from_kdev(_kdev)	(((struct drm_minor *)(_kdev)->si_drv1)->dev)

#define DRM_IOC_VOID		IOC_VOID
#define DRM_IOC_READ		IOC_OUT
#define DRM_IOC_WRITE		IOC_IN
#define DRM_IOC_READWRITE	IOC_INOUT
#define DRM_IOC(dir, group, nr, size) _IOC(dir, group, nr, size)

static inline long
__copy_to_user(void __user *to, const void *from, unsigned long n)
{
	return (copyout(from, to, n) != 0 ? n : 0);
}
#define	copy_to_user(to, from, n) __copy_to_user((to), (from), (n))

static inline int
__put_user(size_t size, void *ptr, void *x)
{

	size = copy_to_user(ptr, x, size);

	return (size ? -EFAULT : size);
}
#define	put_user(x, ptr) __put_user(sizeof(*ptr), (ptr), &(x))

static inline unsigned long
__copy_from_user(void *to, const void __user *from, unsigned long n)
{
	return ((copyin(__DECONST(void *, from), to, n) != 0 ? n : 0));
}
#define	copy_from_user(to, from, n) __copy_from_user((to), (from), (n))

static inline int
__get_user(size_t size, const void *ptr, void *x)
{

	size = copy_from_user(x, ptr, size);

	return (size ? -EFAULT : size);
}
#define	get_user(x, ptr) __get_user(sizeof(*ptr), (ptr), &(x))

#define	sigemptyset(set)	SIGEMPTYSET(set)
#define	sigaddset(set, sig)	SIGADDSET(set, sig)

#define DRM_LOCK(dev)		sx_xlock(&(dev)->dev_struct_lock)
#define DRM_UNLOCK(dev) 	sx_xunlock(&(dev)->dev_struct_lock)

#define jiffies			ticks
#define	jiffies_to_msecs(x)	(((int64_t)(x)) * 1000 / hz)
#define	msecs_to_jiffies(x)	(((int64_t)(x)) * hz / 1000)
#define	time_after(a,b)		((long)(b) - (long)(a) < 0)
#define	time_after_eq(a,b)	((long)(b) - (long)(a) <= 0)

#define	wake_up(queue)			wakeup((void *)queue)
#define	wake_up_interruptible(queue)	wakeup((void *)queue)

MALLOC_DECLARE(DRM_MEM_DMA);
MALLOC_DECLARE(DRM_MEM_SAREA);
MALLOC_DECLARE(DRM_MEM_DRIVER);
MALLOC_DECLARE(DRM_MEM_MAGIC);
MALLOC_DECLARE(DRM_MEM_MINOR);
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
MALLOC_DECLARE(DRM_MEM_MM);
MALLOC_DECLARE(DRM_MEM_HASHTAB);
MALLOC_DECLARE(DRM_MEM_KMS);
MALLOC_DECLARE(DRM_MEM_VBLANK);

#define	simple_strtol(a, b, c)			strtol((a), (b), (c))

typedef struct drm_pci_id_list
{
	int vendor;
	int device;
	long driver_private;
	char *name;
} drm_pci_id_list_t;

#ifdef __i386__
#define	CONFIG_X86	1
#endif
#ifdef __amd64__
#define	CONFIG_X86	1
#define	CONFIG_X86_64	1
#endif
#ifdef __ia64__
#define	CONFIG_IA64	1
#endif

#if defined(__i386__) || defined(__amd64__)
#define	CONFIG_ACPI
#endif

#define	CONFIG_AGP	1
#define	CONFIG_MTRR	1

#define	CONFIG_FB	1
extern const char *fb_mode_option;

#define	EXPORT_SYMBOL(x)
#define	MODULE_AUTHOR(author)
#define	MODULE_DESCRIPTION(desc)
#define	MODULE_LICENSE(license)
#define	MODULE_PARM_DESC(name, desc)
#define	module_param_named(name, var, type, perm)

#define	printk		printf
#define	KERN_DEBUG	""

struct fb_info *	framebuffer_alloc(void);
void			framebuffer_release(struct fb_info *info);

#define KIB_NOTYET()							\
do {									\
	if (drm_debug && drm_notyet)					\
		printf("NOTYET: %s at %s:%d\n", __func__, __FILE__, __LINE__); \
} while (0)

#endif /* _DRM_OS_FREEBSD_H_ */
