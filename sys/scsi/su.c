/* su: SCSI Universal. This is a universal SCSI device that
 * has a fixed minor number format.  This allows you to refer
 * to your devices by BUS, ID, LUN instead of st0, st1, ...
 *
 * This code looks up the underlying device for a given SCSI
 * target and uses that driver.
 *
 *Begin copyright
 *
 * Copyright (C) 1993, 1994, 1995, HD Associates, Inc.
 * PO Box 276
 * Pepperell, MA 01463
 * 508 433 5266
 * dufault@hda.com
 *
 * This code is contributed to the University of California at Berkeley:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *End copyright
 *
 *      $Id: su.c,v 1.9 1995/12/08 11:19:01 julian Exp $
 *
 * Tabstops 4
 * XXX devfs entries for this device should be handled by generic scsiconfig
 * Add a bdevsw interface.. ?
 */

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#include <scsi/scsiconf.h>
#define CDEV_MAJOR 18


	d_open_t	suopen; /* these three used by ssc */
	d_close_t	suclose;
	d_ioctl_t	suioctl;
static	d_read_t	suread;
static	d_write_t	suwrite;
static	d_select_t	suselect;
static	d_strategy_t	sustrategy;

static struct cdevsw su_cdevsw = 
	{ suopen,	suclose,	suread,		suwrite,	/*18*/
	  suioctl,	nostop,		nullreset,	nodevtotty,/* scsi */
	  suselect,	nxmmap,		sustrategy, "su",	NULL,	-1 };


/* Build an old style device number (unit encoded in the minor number)
 * from a base old one (no flag bits) and a full new one
 * (BUS, LUN, TARG in the minor number, and flag bits).
 *
 * OLDDEV has the major number and device unit only.  It was constructed
 * at attach time and is stored in the scsi_link structure.
 *
 * NEWDEV can have whatever in it, but only the old control flags and the
 * super bit are present.  IT CAN'T HAVE ANY UNIT INFORMATION or you'll
 * wind up with the wrong unit.
 */
#define OLD_DEV(NEWDEV, OLDDEV) ((OLDDEV) | ((NEWDEV) & 0x080000FF))

/* bnxio, cnxio: non existent device entries
 */
static struct bdevsw bnxio = {
	nxopen,
	nxclose,
	nxstrategy,
	nxioctl,
	nxdump,
	nxpsize,
	0,
	"NON",
	NULL,
	-1
};

static struct cdevsw cnxio = {
	nxopen,
	nxclose,
	nxread,
	nxwrite,
	nxioctl,
	nxstop,
	nxreset,
	nxdevtotty,
	nxselect,
	nxmmap,
	nxstrategy,
	"NON",
	NULL,
	-1
};

/* getsws: Look up the base dev switch for a given "by minor number" style
 * device.
 */
static int
getsws(dev_t dev, int type,
	struct bdevsw **bdevp, struct cdevsw **cdevp, dev_t *base)
{
	int ret = 0;
	struct scsi_link *scsi_link;
	int chr_dev, blk_dev;

	struct cdevsw *cdev;
	struct bdevsw *bdev;

	int bus = SCSI_BUS(dev),
	    lun = SCSI_LUN(dev),
	    id =  SCSI_ID(dev);

	/* Try to look up the base device by finding the major number in
	 * the scsi_link structure:
	 */
	if ((scsi_link = scsi_link_get(bus, id, lun)) == 0 ||
	scsi_link->dev == NODEV)
	{
		ret = ENXIO;

		/* XXX This assumes that you always have a character device if you
		 *     have a block device.  That seems reasonable.
		 */
		cdev = &cnxio;
		chr_dev = NODEV;
		bdev = &bnxio;
		blk_dev = NODEV;
	}
	else
	{
		int bmaj, cmaj;

		cmaj = major(scsi_link->dev);
		cdev = cdevsw + cmaj;
		chr_dev = OLD_DEV(dev, scsi_link->dev);

		bmaj = chrtoblk(cmaj);
		bdev = (bmaj == NODEV) ? &bnxio : bdevsw + bmaj;
		blk_dev = OLD_DEV(dev, makedev(bmaj, minor(scsi_link->dev)));
	}

	if (cdevp)
		*cdevp = cdev;
	if (bdevp)
		*bdevp = bdev;

	if (type == S_IFCHR)
		*base = chr_dev;
	else
		*base = blk_dev;

	return ret;
}

int
suopen(dev_t dev, int flag, int type, struct proc *p)
{
	struct cdevsw *cdev;
	struct bdevsw *bdev;
	dev_t base;

	if (getsws(dev, type, &bdev, &cdev, &base))
	{
		/* Device not configured?  Reprobe then try again.
		 */
		int bus = SCSI_BUS(dev), lun = SCSI_LUN(dev), id =  SCSI_ID(dev);

		if (scsi_probe_bus(bus, id, lun) || getsws(dev, type, &bdev, &cdev,
		&base))
			return ENXIO;
	}

	/* There is a properly configured underlying device.
	 * Synthesize an appropriate device number:
	 */
	if (type == S_IFCHR)
		return (*cdev->d_open)(base, flag, S_IFCHR, p);
	else
		return (*bdev->d_open)(base, flag, S_IFBLK, p);
}

int
suclose(dev_t dev, int fflag, int type, struct proc *p)
{
	struct cdevsw *cdev;
	struct bdevsw *bdev;
	dev_t base;

	(void)getsws(dev, type, &bdev, &cdev, &base);

	if (type == S_IFCHR)
		return (*cdev->d_close)(base, fflag, S_IFCHR, p);
	else
		return (*bdev->d_open)(base, fflag, S_IFBLK, p);
}

static	void
sustrategy(struct buf *bp)
{
	dev_t base;
	struct bdevsw *bdev;
	dev_t dev = bp->b_dev;

	/* XXX: I have no way of knowing if this was through the
	 * block or the character entry point.
	 */
	(void)getsws(dev, S_IFBLK, &bdev, 0, &base);

	bp->b_dev = base;

	(*bdev->d_strategy)(bp);

	bp->b_dev = dev;
}

int
suioctl(dev_t dev, int cmd, caddr_t data, int fflag, struct proc *p)
{
	struct cdevsw *cdev;
	dev_t base;

	/* XXX: I have no way of knowing if this was through the
	 * block or the character entry point.
	 */
	(void)getsws(dev, S_IFCHR, 0, &cdev, &base);

	return (*cdev->d_ioctl)(base, cmd, data, fflag, p);
}

static	int
sudump(dev_t dev)
{
	dev_t base;
	struct bdevsw *bdev;

	(void)getsws(dev, S_IFBLK, &bdev, 0, &base);

	return (*bdev->d_dump)(base);
}

static	int
supsize(dev_t dev)
{
	dev_t base;
	struct bdevsw *bdev;

	(void)getsws(dev, S_IFBLK, &bdev, 0, &base);

	return (*bdev->d_psize)(base);
}

static	int
suread(dev_t dev, struct uio *uio, int ioflag)
{
	dev_t base;
	struct cdevsw *cdev;

	(void)getsws(dev, S_IFCHR, 0, &cdev, &base);

	return (*cdev->d_read)(base, uio, ioflag);
}

static	int
suwrite(dev_t dev, struct uio *uio, int ioflag)
{
	dev_t base;
	struct cdevsw *cdev;

	(void)getsws(dev, S_IFCHR, 0, &cdev, &base);

	return (*cdev->d_write)(base, uio, ioflag);
}

static	int
suselect(dev_t dev, int which, struct proc *p)
{
	dev_t base;
	struct cdevsw *cdev;

	(void)getsws(dev, S_IFCHR, 0, &cdev, &base);

	return (*cdev->d_select)(base, which, p);
}

static su_devsw_installed = 0;

static void
su_drvinit(void *unused)
{
	dev_t dev;

	if( ! su_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&su_cdevsw, NULL);
		su_devsw_installed = 1;
    	}
}

SYSINIT(sudev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,su_drvinit,NULL)


