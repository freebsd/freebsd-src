/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: src/sys/sys/disk.h,v 1.39 2003/04/01 18:55:04 phk Exp $
 *
 */

#ifndef _SYS_DISK_H_
#define	_SYS_DISK_H_

#include <sys/ioccom.h>

#ifdef _KERNEL

#ifndef _SYS_CONF_H_
#include <sys/conf.h>	/* XXX: temporary to avoid breakage */
#endif

struct disk;
struct disk *disk_enumerate(struct disk *disk);
void disk_err(struct bio *bp, const char *what, int blkdone, int nl);

#endif

#define DIOCGSECTORSIZE	_IOR('d', 128, u_int)
	/*-
	 * Get the sectorsize of the device in bytes.  The sectorsize is the
	 * smallest unit of data which can be transfered from this device.
	 * Usually this is a power of two but it may not be. (ie: CDROM audio)
	 */

#define DIOCGMEDIASIZE	_IOR('d', 129, off_t)	/* Get media size in bytes */
	/*-
	 * Get the size of the entire device in bytes.  This should be a
	 * multiple of the sectorsize.
	 */

#define DIOCGFWSECTORS	_IOR('d', 130, u_int)	/* Get firmware sectorcount */
	/*-
	 * Get the firmwares notion of number of sectors per track.  This
	 * value is mostly used for compatibility with various ill designed
	 * disk label formats.  Don't use it unless you have to.
	 */

#define DIOCGFWHEADS	_IOR('d', 131, u_int)	/* Get firmware headcount */
	/*-
	 * Get the firmwares notion of number of heads per cylinder.  This
	 * value is mostly used for compatibility with various ill designed
	 * disk label formats.  Don't use it unless you have to.
	 */

#define DIOCSKERNELDUMP _IOW('d', 133, u_int)	/* Set/Clear kernel dumps */
	/*-
	 * Enable/Disable (the argument is boolean) the device for kernel
	 * core dumps.
	 */
	
#define DIOCGFRONTSTUFF _IOR('d', 134, off_t)
	/*-
	 * Many disk formats have some amount of space reserved at the
	 * start of the disk to hold bootblocks, various disklabels and
	 * similar stuff.  This ioctl returns the number of such bytes
	 * which may apply to the device.
	 */

#endif /* _SYS_DISK_H_ */
