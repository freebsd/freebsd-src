/*
 * Copyright (C) 1995, HD Associates, Inc.
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
 *
 * $Id: scsi_driver.c,v 1.11 1995/12/05 04:41:20 julian Exp $
 *
 */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/devconf.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>

#include <machine/cpu.h>	/* XXX For bootverbose (funny place) */

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_driver.h>

#define GETUNIT(DEVICE, DEV) \
	((DEVICE)->getunit) ? (*(DEVICE)->getunit)((DEV)) \
				: (minor((DEV)) & ~SCSI_CONTROL_MASK)

int
scsi_goaway(struct kern_devconf *kdc, int force) /* XXX should do a lot more */
{
	dev_detach(kdc);
	FREE(kdc, M_TEMP);
	return 0;
}

/* scsi_device_attach: Attach a SCSI device.  This routine will
 * print out the device address, what it is, then call the type
 * attach function and when that returns print a newline.  If the
 * type attach will make LOT's of noise it should print a leading
 * newline and then the address using sc_print_addr.  See "sd.c".
 */
int scsi_device_attach(struct scsi_link *sc_link)
{
	errval errcode;
	dev_t dev;
	struct scsi_device *device = sc_link->device;

	if (bootverbose)
		sc_link->flags |= SDEV_BOOTVERBOSE;

	SC_DEBUG(sc_link, SDEV_DB2,
	("%s%dattach: ", device->name, sc_link->dev_unit));

	sc_print_start(sc_link);
	printf("%s ", device->desc);

	dev = scsi_dev_lookup(device->open);

	sc_link->dev = (device->setunit ?
	(*device->setunit)(dev, sc_link->dev_unit) :
	makedev(major(dev), sc_link->dev_unit) );

	errcode = (device->attach) ? (*(device->attach))(sc_link) : 0;

	sc_print_finish();
	printf("\n");

	if (errcode == 0)
		sc_link->flags |= device->link_flags;

	return errcode;
}

int
scsi_open(dev_t dev, int flags, int fmt, struct proc *p,
struct scsi_device *device)
{
	errval  errcode;
	u_int32 unit;
	struct scsi_link *sc_link;

	if (device == 0)
		return ENXIO;

	unit = GETUNIT(device, dev);
	sc_link = SCSI_LINK(device, unit);

	/*
	 * Check the unit is legal
	 */
	if (sc_link == 0 || sc_link->sd == 0)
		return ENXIO;

	/* If it is a "once only" device that is already open return EBUSY.
	 */
	if ((sc_link->flags & SDEV_ONCE_ONLY) && (sc_link->flags & SDEV_IS_OPEN))
		return EBUSY;

	/* For the control device (user ioctl's only) don't call the open
	 * entry.
	 */
	if (SCSI_CONTROL(dev) || (device->dev_open == 0))
		errcode = 0;
	else
		errcode = (*device->dev_open)(dev, flags, fmt, p, sc_link);

	if (!errcode ) sc_link->flags |= SDEV_IS_OPEN;

	SC_DEBUG(sc_link, SDEV_DB1, ("%sopen: dev=0x%lx (unit %ld) result %d\n",
		device->name, dev, unit, errcode));

	return errcode;
}

int
scsi_close(dev_t dev, int flags, int fmt, struct proc *p,
struct scsi_device *device)
{
	errval errcode;
	struct scsi_link *sc_link = SCSI_LINK(device, GETUNIT(device, dev));

	SC_DEBUG(sc_link, SDEV_DB1, ("%sclose:  Closing device\n", device->name));

	if (SCSI_CONTROL(dev) || (device->dev_close == 0))
		errcode = 0;
	else
		errcode = (*device->dev_close)(dev, flags, fmt, p, sc_link);

	sc_link->flags &= ~SDEV_IS_OPEN;

	return errcode;
}

int
scsi_ioctl(dev_t dev, u_int32 cmd, caddr_t arg, int flags, struct proc *p,
struct scsi_device *device)
{
	errval errcode;
	struct scsi_link *sc_link = SCSI_LINK(device, GETUNIT(device, dev));

	if (SCSI_CONTROL(dev) || (device->dev_ioctl == 0))
		errcode = scsi_do_ioctl(dev, cmd, arg, flags, p, sc_link);
	else
		errcode = (*device->dev_ioctl)(dev, cmd, arg, flags, p, sc_link);

	return errcode;
}

void
scsi_minphys(struct buf *bp, struct scsi_device *device)
{
	struct scsi_link *sc_link = SCSI_LINK(device, GETUNIT(device, bp->b_dev));
	(*sc_link->adapter->scsi_minphys)(bp);
}

void
scsi_strategy(struct buf *bp, struct scsi_device *device)
{
	u_int32 unit = GETUNIT(device, bp->b_dev);
	struct scsi_link *sc_link = SCSI_LINK(device, unit);

	SC_DEBUG(sc_link, SDEV_DB2, ("\n%sstrategy ", device->name));
	SC_DEBUG(sc_link, SDEV_DB1, ("%ld bytes @ blk%ld\n",
		bp->b_bcount, bp->b_blkno));

	if (SCSI_CONTROL(bp->b_dev) || (device->dev_strategy == 0))
	{
		bp->b_resid = bp->b_bcount;
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
	}
	else
	{
		bp->b_resid = 0;
		bp->b_error = 0;

		if (bp->b_bcount == 0)
			biodone(bp);
		else
		{
			(*sc_link->adapter->scsi_minphys)(bp);
			(*device->dev_strategy)(bp, sc_link);
		}
	}
}
