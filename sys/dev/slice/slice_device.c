/*-
 * Copyright (C) 1997,1998 Julian Elischer.  All rights reserved.
 * julian@freebsd.org
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 *	$Id: slice_device.c,v 1.1 1998/04/19 23:31:14 julian Exp $
 */
#define DIAGNOSTIC 1

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>		/* SYSINIT stuff */
#include <sys/conf.h>		/* cdevsw stuff */
#include <sys/malloc.h>		/* malloc region definitions */
#include <sys/buf.h>		/* bufs for describing IO */
#include <sys/fcntl.h>		/* file open modes etc. */
#include <sys/queue.h>		/* standard queue macros */
#include <sys/stat.h>		/* S_IFBLK, S_IFMT etc. */
#include <sys/devfsext.h>	/* DEVFS defintitions */
#include <dev/slice/slice.h>	/* temporary location */



/* Function prototypes (these should all be static  except for slicenew()) */
static d_open_t slcdevopen;
static d_close_t slcdevclose;
static d_ioctl_t slcdevioctl;
static d_dump_t slcdevdump;
static d_psize_t slcdevsize;
static d_strategy_t slcdevstrategy;

#define BDEV_MAJOR 14
#define CDEV_MAJOR 20

static struct cdevsw slice_cdevsw;
static struct bdevsw slice_bdevsw = {
	slcdevopen,
	slcdevclose,
	slcdevstrategy,
	slcdevioctl,
	slcdevdump,
	slcdevsize,
	D_DISK,
	"slice",
	&slice_cdevsw,
	-1
};

static dev_t    cdevnum, bdevnum;

#define UNIT_HASH_SIZE 64
LIST_HEAD(slice_bucket, slice) hash_table[UNIT_HASH_SIZE - 1];

/*
 * Now  for some driver initialisation. Occurs ONCE during boot (very early).
 */
static void
slice_drvinit(void *unused)
{
	int             i;

	/*
	 * add bdevsw and cdevsw entries
	 */
	bdevsw_add_generic(BDEV_MAJOR, CDEV_MAJOR, &slice_bdevsw);

	/*
	 * clear out the hash table
	 */
	for (i = 0; i < UNIT_HASH_SIZE; i++) {
		LIST_INIT(hash_table + i);
	}
}

SYSINIT(slicedev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE + CDEV_MAJOR,
	slice_drvinit, NULL);

static int      nextunit = 0;

void
slice_add_device(sl_p slice)
{
	int             unit = nextunit++;
	char           *name = slice->name;
RR;
	slice->minor = makedev(0,
			(((unit << 8) & 0xffff0000) | (unit & 0x000000ff)));
	/*
	 * put it on the hash chain for it's bucket so we can find it again
	 * later.
	 */
	LIST_INSERT_HEAD(hash_table + (slice->minor % UNIT_HASH_SIZE),
			 slice, hash_list);
	/*
	 * Add an entry in the devfs for it. Possibly should happen later.
	 */
	slice->devfs_ctoken = devfs_add_devswf(&slice_cdevsw, unit, DV_CHR,
	    UID_ROOT, GID_KMEM, 0600, "r%s", name ? name : "-");
	slice->devfs_btoken = devfs_add_devswf(&slice_bdevsw, unit, DV_BLK,
	     UID_ROOT, GID_KMEM, 0600, "%s", name ? name : "-");
	/* XXX link this node into upper list of caller */
}

/*
 * Given a minor number, find the slice which the operations are destined.
 * When DEVFS DDEV devices are enabled this is bypassed entirely.
 */
static struct slice *
minor_to_slice(unsigned int minor)
{
	int             hash = minor % UNIT_HASH_SIZE;
	struct slice   *slice;

	slice = (hash_table + hash)->lh_first;
	while (slice) {
		if (slice->minor == minor) {
			return (slice);
		}
		slice = slice->hash_list.le_next;
	}
	return (NULL);
}



int
slcdevioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc * p)
{
	sl_p            slice = minor_to_slice(minor(dev));
	int             error = 0;

RR;

	/*
	 * Look for only some generic "inherrited" ioctls that apply to all
	 * disk-like devices otherwise pass it down to the previous handler
	 */

	switch (cmd) {
		/*
		 * At present there are none, but eventually there would be
		 * something that returns the basic partition parameters.
		 * Whether this would be in the form of a disklabel or
		 * similar I have not yet decided.
		 */
	default:
		if (slice->handler_down->ioctl) {
			error = (*slice->handler_down->ioctl)
				(slice->private_down, cmd, data, flag, p);
		} else {
			error = ENOTTY;
		}
		if (error) {
			/* 
			 * If no disklabel was returned, let's make
			 * up something that will satisfy the system's
			 * need for a disklabel to mount an ffs on.
			 * Don't overwrite error unless we get a dummy.
			 * let the called routine decide
			 * if it can handle any ioctl.
			 */
			if (dkl_dummy_ioctl(slice, cmd, data, flag, p) == 0) {
					error = 0;
			}
		}
		break;
	}
	return (error);
}

/*
 * You also need read, write, open, close routines. This should get you
 * started.
 * The open MIGHT allow the caller to proceed if it is a READ
 * mode, and it is open at a higher layer.
 * All Accesses would have to be checked for READ
 * as the system doesn't enforce this at this time.
 */
static int
slcdevopen(dev_t dev, int flags, int mode, struct proc * p)
{
	sl_p            slice = minor_to_slice(minor(dev));
	int	error;

RR;
	if (slice == NULL)
		return (ENXIO);
#if 1 /* the hack */
	if ((mode & S_IFMT) == S_IFBLK) {
		/*
		 * XXX Because a mount -u does not re-open the device
		 * The hack here, is to always open block devices
		 * in full read/write mode. Eventually, if DEVFS
		 * becomes ubiquitous, VOP to do a file upgrade
		 * might be implemented. Other Filesystems need
		 * not implement it..
		 * THIS SHOULD BE DONE IN slice_device.c
		 */
		flags |= FWRITE;
	}
#endif /* the hack */
	return (sliceopen(slice, flags, mode, p, SLW_DEVICE));
}

static int
slcdevclose(dev_t dev, int flags, int mode, struct proc * p)
{
	sl_p            slice = minor_to_slice(minor(dev));
RR;
#ifdef	DIAGNOSTIC
	if ((flags & (FWRITE | FREAD)) != 0) {
		printf("sliceclose called with non 0 flags\n");
	}
#endif
	/*
	 * Close is just an open for non-read/nonwrite in this context.
	 */
	sliceopen(slice, 0, mode, p, SLW_DEVICE);
	return(0);
}

static int
slcdevsize(dev_t dev)
{
	sl_p            slice = minor_to_slice(minor(dev));

RR;
	if (slice == NULL)
		return (-1);

#if 0
	return (slice->limits.slicesize / slice->limits.blksize);
#else
	return (slice->limits.slicesize / 512);
#endif
}


/*
 * Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer to
 * complete.  Multi-page transfers are supported.  All I/O requests must be a
 * multiple of a sector in length.
 */
void
slcdevstrategy(struct buf * bp)
{
	sl_p            slice = minor_to_slice(minor(bp->b_dev));
	u_int64_t       start, end;
	u_int32_t       blksize;
	daddr_t         blkno;
	int             s;

RR;
	if (slice == NULL) {
		bp->b_error = ENXIO;
		goto bad;
	}
	blksize = slice->limits.blksize;
	/* Check we are going to be able to do this kind of transfer */
	/* Check the start point too if DEV_BSIZE != reallity */
	if (bp->b_blkno < 0) {
		Debugger("Slice code got negative blocknumber");
		bp->b_error = EINVAL;
		goto bad;
	}
	start = (u_int64_t)bp->b_blkno * DEV_BSIZE;
	if (blksize != DEV_BSIZE) {
		if ((start % blksize) != 0) {
			Debugger("slice: request not on block boundary.");
			bp->b_error = EINVAL;
			goto bad;
		}
		blkno = start / blksize;
	} else {
		blkno = bp->b_blkno;
	}

	if ((bp->b_bcount % blksize) != 0) {
		printf("bcount = %d, blksize= %d(%d)\n",
				bp->b_bcount, blksize,
				slice->limits.blksize);
		Debugger("slice: request not multile of blocksize.");
		bp->b_error = EINVAL;
		goto bad;
	}
	/*
	 * Do bounds checking, adjust transfer, and set b_pblkno.
	 */
	bp->b_pblkno = blkno;
	end = start + (u_int64_t)bp->b_bcount;	/* first byte BEYOND the IO */

	/*
	 * Handle the cases near or beyond the end of the slice. Assumes IO
	 * is < 2^63 bytes long. (pretty safe)
	 */
	if (end > slice->limits.slicesize) {
		int64_t         size;
		size = slice->limits.slicesize - start;
		/*
		 * if exactly on end of slice, return EOF
		 */
		if ((size == 0) && (bp->b_flags & B_READ)) {
			printf("slice: at end of slice.");
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		if (size <= 0) {
			printf("slice: beyond end of slice.");
			bp->b_error = EINVAL;
			goto bad;
		}
		bp->b_bcount = size;
	}
	sliceio(slice, bp, SLW_DEVICE);
	return;

done:
	s = splbio();
	/* toss transfer, we're done early */
	biodone(bp);
	splx(s);
	return;
bad:
	bp->b_flags |= B_ERROR;
	goto done;

}

void
slice_remove_device(sl_p slice)
{
	/*
	 * Remove the devfs entry, which revokes the vnode etc. XXX if
	 * handler has madde more, we should tell it too. e.g. floppy driver
	 * does this.
	 */
RR;
	devfs_remove_dev(slice->devfs_btoken);
	devfs_remove_dev(slice->devfs_ctoken);

	/*
	 * Remove it from the hashtable.
	 */
	LIST_REMOVE(slice, hash_list);
}

static int
slcdevdump(dev_t dev)
{
	sl_p            slice = minor_to_slice(minor(dev));
RR;
	if (slice == NULL)
		return (ENXIO);
	return (0);
}
