/* This code originally came from a nice man, I don't have your name,
 * please email cokane@pohl.ececs.uc.edu for proper recognition 
 * it is basically a derivative of some sys/dhio.h file to work with the 3dfx
 * cards 
 */
#ifndef TDFX_IO_H
#define TDFX_IO_H

#ifndef KERNEL
#include <sys/types.h>
#endif

/*
 * define an ioctl here
 */
#define DHIOCRESET _IO('D', 0)   /* reset the voodoo device */
#define GETVOODOO 0x3302
#define SETVOODOO 0x3303
#define MOREVOODOO 0x3300
#define _IOC_NRBITS	8
#define _IOC_TYPEBITS	8
#define _IOC_SIZEBITS	14
#define _IOC_DIRBITS	2

#define _IOC_NRMASK	((1 << _IOC_NRBITS)-1)
#define _IOC_TYPEMASK	((1 << _IOC_TYPEBITS)-1)
#define _IOC_SIZEMASK	((1 << _IOC_SIZEBITS)-1)
#define _IOC_DIRMASK	((1 << _IOC_DIRBITS)-1)

#define _IOC_NRSHIFT	0
#define _IOC_TYPESHIFT	(_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT	(_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT	(_IOC_SIZESHIFT+_IOC_SIZEBITS)

/*
 * Direction bits.
 */
#define _IOC_NONE	0U
#define _IOC_WRITE	1U
#define _IOC_READ	2U

#define _IOCV(dir,type,nr,size) \
	(((dir)  << _IOC_DIRSHIFT) | \
	 ((type) << _IOC_TYPESHIFT) | \
	 ((nr)   << _IOC_NRSHIFT) | \
	 ((size) << _IOC_SIZESHIFT))

/* used to create numbers */
#define _IOV(type,nr)		_IOCV(_IOC_NONE,(type),(nr),0)
#define _IORV(type,nr,size)	_IOCV(_IOC_READ,(type),(nr),sizeof(size))
#define _IOWV(type,nr,size)	_IOCV(_IOC_WRITE,(type),(nr),sizeof(size))
#define _IOWRV(type,nr,size)	_IOCV(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(size))

/* used to decode ioctl numbers.. */
#define _IOC_DIR(nr)		(((nr) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
#define _IOC_TYPE(nr)		(((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#define _IOC_NR(nr)		(((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
#define _IOC_SIZE(nr)		(((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)

/* ...and for the drivers/sound files... */

#define IOCV_IN		(_IOC_WRITE << _IOC_DIRSHIFT)
#define IOCV_OUT	(_IOC_READ << _IOC_DIRSHIFT)
#define IOCV_INOUT	((_IOC_WRITE|_IOC_READ) << _IOC_DIRSHIFT)
#define IOCSIZE_MASK	(_IOC_SIZEMASK << _IOC_SIZESHIFT)
#define IOCSIZE_SHIFT	(_IOC_SIZESHIFT)

#endif
