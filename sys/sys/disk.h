/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: src/sys/sys/disk.h,v 1.16 2000/01/28 20:49:35 phk Exp $
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
	u_int			d_flags;
	u_int			d_dsflags;
	struct cdevsw		*d_devsw;
	dev_t			d_dev;
	struct diskslices	*d_slice;
	struct disklabel	d_label;
};

#define DISKFLAG_LOCK		0x1
#define DISKFLAG_WANTED		0x2

dev_t disk_create __P((int unit, struct disk *disk, int flags, struct cdevsw *cdevsw, struct cdevsw *diskdevsw));
void disk_destroy __P((dev_t dev));
int disk_dumpcheck __P((dev_t dev, u_int *count, u_int *blkno, u_int *secsize));
void disk_invalidate __P((struct disk *disk));

#endif /* _SYS_DISK_H_ */
