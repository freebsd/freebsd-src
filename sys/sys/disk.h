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

#include <sys/queue.h>

struct disk {
	u_int			d_flags;
	u_int			d_dsflags;
	struct cdevsw		*d_devsw;
	dev_t			d_dev;
	struct diskslices	*d_slice;
	struct disklabel	d_label;
	LIST_ENTRY(disk)	d_list;
	void			*d_softc;
};

#define DISKFLAG_LOCK		0x1
#define DISKFLAG_WANTED		0x2

dev_t disk_create(int unit, struct disk *disk, int flags, struct cdevsw *cdevsw, struct cdevsw *diskdevsw);
void disk_destroy(dev_t dev);
int disk_dumpcheck(dev_t dev, u_int *count, u_int *blkno, u_int *secsize);
struct disk *disk_enumerate(struct disk *disk);
void disk_invalidate(struct disk *disk);

#endif /* _SYS_DISK_H_ */
