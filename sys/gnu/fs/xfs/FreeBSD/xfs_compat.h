#ifndef __XFS_COMPAT_H__
#define	__XFS_COMPAT_H__

#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/limits.h>
#include <sys/uuid.h>
#include <sys/conf.h>
#include <sys/sbuf.h>
#include <sys/stat.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/dirent.h>
#include <sys/ktr.h>
#include <sys/kdb.h>

#ifdef _KERNEL
#define __KERNEL__
#endif

#define printk printf

#define MAJOR(x) major(x)
#define MINOR(x) minor(x)

/*
 * SYSV compatibility types missing in FreeBSD.
 */
typedef unsigned long		ulong;
typedef unsigned int		uint;
typedef unsigned short		ushort;

/*
 * Additional type declarations for XFS.
 */
typedef signed char		__s8;
typedef unsigned char		__u8;
typedef signed short int	__s16;
typedef unsigned short int	__u16;
typedef signed int		__s32;
typedef unsigned int		__u32;
typedef signed long long int	__s64;
typedef unsigned long long int	__u64;

/*
 * Linux types with direct FreeBSD conterparts
 */
typedef off_t			loff_t;
typedef struct timespec		timespec_t;
typedef	struct uuid		uuid_t;
typedef struct fid		fid_t;
typedef dev_t			os_dev_t;

/*
 *  Linux block devices are device vnodes in FreeBSD.
 */
#define	block_device		vnode

/*
 *  Get the current CPU ID. 
 */
#define	smp_processor_id()	PCPU_GET(cpuid)

/*
 * FreeBSD does not have BITS_PER_LONG defined.
 */
#if defined(LONG_BIT)
#define	BITS_PER_LONG		LONG_BIT
#elif defined(__i386__)
#define	BITS_PER_LONG		32
#endif

/*
 * boolean_t is enum on Linux, int on FreeBSD.
 * Provide value defines.
 */
#define	B_FALSE			0
#define	B_TRUE			1

/*
 * GCC 3.x static branch prediction hints
 */
#if __GNUC__ < 3
#define __builtin_expect(x, expected_value) (x)
#endif

#ifndef likely
#define	likely(x)	__builtin_expect((x), 1)
#endif

#ifndef unlikely
#define	unlikely(x)	__builtin_expect((x), 0)
#endif

/*
 * ANSI and GCC extension keywords compatibity
 */
#ifndef inline
#define	inline __inline__
#endif

#ifndef asm
#define	asm __asm
#endif

#ifndef typeof
#define	typeof __typeof
#endif

/*
 * Miscellaneous limit constants
 */
#define	MAX_LFS_FILESIZE	0x7fffffffffffffffLL

/*
 * Map simple functions to their FreeBSD kernel equivalents
 */
#ifndef copy_to_user
#define	copy_to_user(dst, src, len)	copyout((src), (dst), (len))
#endif

#ifndef copy_from_user
#define	copy_from_user(dst, src, len)	copyin((src), (dst), (len))
#endif

#ifndef memmove
#define	memmove(dst, src, len)		bcopy((src), (dst), (len))
#endif

#ifndef barrier
#define	barrier()	__asm__ __volatile__("": : :"memory")
#endif

/*
 * Map simple global vairables to FreeBSD kernel equivalents
 */
#if !defined(xfs_physmem)
#define	xfs_physmem	physmem
#endif

#ifndef HZ
#define	HZ		hz
#endif

/*
 * These should be implemented properly for all architectures
 * we want to support.
 */
#define	get_unaligned(ptr)	(*(ptr))
#define	put_unaligned(val, ptr)	((void)( *(ptr) = (val) ))

/*
 * Linux type-safe min/max macros.
 */
#define	min_t(type,x,y)		MIN((x),(y)) 
#define	max_t(type,x,y)		MAX((x),(y)) 


/*
 * Cedentials manipulation.
 */
#define current_fsuid(credp)	(credp)->cr_uid
#define current_fsgid(credp)	(credp)->cr_groups[0]

#endif /* __XFS_COMPAT_H__ */
