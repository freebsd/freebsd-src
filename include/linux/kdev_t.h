#ifndef _LINUX_KDEV_T_H
#define _LINUX_KDEV_T_H
#if defined(__KERNEL__) || defined(_LVM_H_INCLUDE)
/*
As a preparation for the introduction of larger device numbers,
we introduce a type kdev_t to hold them. No information about
this type is known outside of this include file.

Objects of type kdev_t designate a device. Outside of the kernel
the corresponding things are objects of type dev_t - usually an
integral type with the device major and minor in the high and low
bits, respectively. Conversion is done by

extern kdev_t to_kdev_t(int);

It is up to the various file systems to decide how objects of type
dev_t are stored on disk.
The only other point of contact between kernel and outside world
are the system calls stat and mknod, new versions of which will
eventually have to be used in libc.

[Unfortunately, the floppy control ioctls fail to hide the internal
kernel structures, and the fd_device field of a struct floppy_drive_struct
is user-visible. So, it remains a dev_t for the moment, with some ugly
conversions in floppy.c.]

Inside the kernel, we aim for a kdev_t type that is a pointer
to a structure with information about the device (like major,
minor, size, blocksize, sectorsize, name, read-only flag,
struct file_operations etc.).

However, for the time being we let kdev_t be almost the same as dev_t:

typedef struct { unsigned short major, minor; } kdev_t;

Admissible operations on an object of type kdev_t:
- passing it along
- comparing it for equality with another such object
- storing it in ROOT_DEV, inode->i_dev, inode->i_rdev, sb->s_dev,
  bh->b_dev, req->rq_dev, de->dc_dev, tty->device
- using its bit pattern as argument in a hash function
- finding its major and minor
- complaining about it

An object of type kdev_t is created only by the function MKDEV(),
with the single exception of the constant 0 (no device).

Right now the other information mentioned above is usually found
in static arrays indexed by major or major,minor.

An obstacle to immediately using
    typedef struct { ... (* lots of information *) } *kdev_t
is the case of mknod used to create a block device that the
kernel doesn't know about at present (but first learns about
when some module is inserted).

aeb - 950811
*/

/* Since MINOR(dev) is used as index in static arrays,
   the kernel is not quite ready yet for larger minors.
   However, everything runs fine with an arbitrary kdev_t type. */

#define MINORBITS	8
#define MINORMASK	((1U << MINORBITS) - 1)

typedef unsigned short kdev_t;

#define MAJOR(dev)	((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)	((unsigned int) ((dev) & MINORMASK))
#define HASHDEV(dev)	((unsigned int) (dev))
#define NODEV		0
#define MKDEV(ma,mi)	(((ma) << MINORBITS) | (mi))
#define B_FREE		0xffff		/* yuk */

extern const char * kdevname(kdev_t);	/* note: returns pointer to static data! */

/* 2.5.x compatibility */
#define mk_kdev(a,b)	MKDEV(a,b)
#define major(d)	MAJOR(d)
#define minor(d)	MINOR(d)
#define kdev_same(a,b)	((a) == (b))
#define kdev_none(d)	(!(d))
#define kdev_val(d)	((unsigned int)(d))
#define val_to_kdev(d)	((kdev_t)(d))

/*
As long as device numbers in the outside world have 16 bits only,
we use these conversions.
*/

static inline unsigned int kdev_t_to_nr(kdev_t dev) {
	return (MAJOR(dev)<<8) | MINOR(dev);
}

static inline kdev_t to_kdev_t(int dev)
{
	int major, minor;
#if 0
	major = (dev >> 16);
	if (!major) {
		major = (dev >> 8);
		minor = (dev & 0xff);
	} else
		minor = (dev & 0xffff);
#else
	major = (dev >> 8);
	minor = (dev & 0xff);
#endif
	return MKDEV(major, minor);
}

#else /* __KERNEL__ || _LVM_H_INCLUDE */

/*
Some programs want their definitions of MAJOR and MINOR and MKDEV
from the kernel sources. These must be the externally visible ones.
*/
#define MAJOR(dev)	((dev)>>8)
#define MINOR(dev)	((dev) & 0xff)
#define MKDEV(ma,mi)	((ma)<<8 | (mi))
#endif /* __KERNEL__ || _LVM_H_INCLUDE */
#endif
