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

#ifndef _SYS_DISKSLICE_H_
#include <sys/diskslice.h>
#endif /* _SYS_DISKSLICE_H_ */

#ifndef _SYS_DISKLABEL
#include <sys/disklabel.h>
#endif /* _SYS_DISKLABEL */

struct disk {
	int			d_flags;
	struct cdevsw		*d_devsw;
	dev_t			d_dev;
	struct diskslices	*d_slice;
	struct disklabel	d_label;
};

dev_t disk_create __P((int unit, struct disk *disk, int flags, struct cdevsw *cdevsw, struct cdevsw *diskdevsw));
void disk_delete __P((dev_t dev));
int disk_dumpcheck __P((dev_t dev, u_int *count, u_int *blkno, u_int *secsize));
void disk_invalidate __P((struct disk *disk));

#endif /* _SYS_DISK_H_ */
