/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#ifndef _SYS_DISK_H_
#define	_SYS_DISK_H_

#include <sys/ioccom.h>

#ifdef _KERNEL

#include <sys/queue.h>

#ifndef _SYS_CONF_H_
#include <sys/conf.h>	/* XXX: temporary to avoid breakage */
#endif

typedef	int	disk_open_t(struct disk *);
typedef	int	disk_close_t(struct disk *);
typedef	void	disk_strategy_t(struct bio *bp);
typedef	int	disk_ioctl_t(struct disk *, u_long cmd, void *data,
			int fflag, struct thread *td);
		/* NB: disk_ioctl_t SHALL be cast'able to d_ioctl_t */

struct g_geom;

struct disk {
	/* Fields which are private to geom_disk */
	struct cdevsw		*d_devsw;
	d_open_t		*d_copen;	/* Compat */
	d_close_t		*d_cclose;	/* Compat */
	d_ioctl_t		*d_cioctl;	/* Compat */
	struct g_geom		*d_geom;

	/* Shared fields */
	u_int			d_flags;
	const char		*d_name;
	u_int			d_unit;
	dev_t			d_dev;		/* Compat */

	/* Disk methods  */
	disk_open_t		*d_open;
	disk_close_t		*d_close;
	disk_strategy_t		*d_strategy;
	disk_ioctl_t		*d_ioctl;
	dumper_t		*d_dump;

	/* Info fields from driver to geom_disk.c. Valid when open */
	u_int			d_sectorsize;
	off_t			d_mediasize;
	u_int			d_fwsectors;
	u_int			d_fwheads;
	u_int			d_stripe_offset;
	u_int			d_stripe_width;
	u_int			d_max_request;

	/* Fields private to the driver */
	void			*d_drv1;
};

#define DISKFLAG_NOGIANT	0x1
#define DISKFLAG_OPEN		0x2
#define DISKFLAG_CANDELETE	0x4

dev_t disk_create(int unit, struct disk *disk, int flags, struct cdevsw *cdevsw, void *unused);
void disk_destroy(dev_t dev);
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
