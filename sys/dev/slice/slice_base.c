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
 *	$Id: slice_base.c,v 1.3 1998/04/22 10:25:10 julian Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>		/* SYSINIT stuff */
#include <sys/fcntl.h>		/* FREAD/FWRITE */
#include <sys/conf.h>		/* cdevsw stuff */
#include <sys/malloc.h>		/* malloc region definitions */
#include <sys/buf.h>		/* buffers for IO */
#include <sys/queue.h>		/* linked lists etc. */
#include <sys/stat.h>		/* S_IFCHR, S_IFBLK */
#include <sys/sysctl.h>		/* the sysctl for shooting self in foot */
/*#include <sys/devfsext.h> */	/* DEVFS defintitions */
#include <dev/slice/slice.h>	/* temporary location */

#define SLICESPL() splbio()


static int slicexclusive = 0; /* default value == "foot shootable" */

/*
 * Make a new type available. Just link it in, but first make sure there is
 * no name collision.
 */

static sh_p     types;

int
sl_newtype(sh_p tp)
{
	if (sl_findtype(tp->name)) {
		return (EEXIST);
	}
	tp->next = types;
	types = tp;
	return (0);
}

/*
 * Look for a type of the name given.
 */
sh_p
sl_findtype(char *type)
{
	sh_p            tp;

	tp = types;
	while (tp) {
		if (strcmp(tp->name, type) == 0)
			return (tp);
		tp = tp->next;
	}
	return (NULL);
}

/*
 * Ask all known handler types if a given slice is handled by them.
 * If the slice specifies a type, then just find that.
 */
sh_p
slice_probeall(sl_p slice)
{
	sh_p            tp = types;
	if (slice->probeinfo.type == NULL) {
		while (tp) {
			printf("%s: probing for %s.. ",slice->name, tp->name);
			if ((*tp->claim) (slice, NULL, NULL) == 0) {
				printf("yep\n");
				return (tp);
			}
			printf("nope\n");
			tp = tp->next;
		}
	/*
	 * Null string ("") means "don't even try". Caller probably should
	 * pre-trap such cases but we'll check here too.
	 */
	} else if (slice->probeinfo.type[0]) {
		tp = sl_findtype(slice->probeinfo.type);
		if ((tp) && ((*tp->claim) (slice, NULL, NULL) == 0)) {
			printf("%s: attaching %s..\n",slice->name, tp->name);
			return (tp);
		}
	}
	/*printf("%s: Leaving as raw device\n", slice->name); */
	return (NULL);
}



/*
 * Make a handler instantiation of the requested type.
 * 
 */
static int
sl_make_handler(char *type, sl_p slice)
{
	sh_p            handler_up;
	void           *private_up;
	int             errval;

	/*
	 * check that the type makes sense.
	 */
	if (type == NULL) {
		return (EINVAL);
	}
	handler_up = sl_findtype(type);
	if (handler_up == NULL) {
		return (ENXIO);
	}
	/*
	 * and call the constructor
	 */
	if (handler_up->constructor != NULL) {
		errval = (*handler_up->constructor) (slice);
		return (errval);
	} else {
		printf("slice handler %s has no constructor\n",
		       handler_up->name);
		return (EINVAL);
	}
}

/*
 * lock and unlock Slices while doing operations such as open().
 * gets a reference on the slice..
 * XXX This doesn't work for SMP.
 */
int
lockslice(struct slice *slice)
{
	int s = SLICESPL();
	slice->refs++;
	while ( slice->flags & (SLF_LOCKED | SLF_INVALID)) {
		if (slice->flags & SLF_INVALID) {
			sl_unref(slice);
			splx(s);
			return (ENXIO);
		}
		slice->flags |= SLF_WANTED;
		tsleep(slice, PRIBIO, "lockslice", 0);
	}
	slice->flags |= SLF_LOCKED;
	splx(s);
	return (0);
}

/*
 * Releases a slice
 * Assumes that if we had it locked, no-one else could invalidate it.
 * We can still hold a reference on it.
 */
int
unlockslice(struct slice *slice)
{
	int s = SLICESPL();
	slice->flags &= ~SLF_LOCKED;
	if ( slice->flags & SLF_WANTED) {
		slice->flags &= ~SLF_WANTED;
		wakeup(slice);
	}
	splx(s);
	return (0);
}

/*
 * create a new slice. Link it into the structures. don't yet find and call
 * it's type handler. That's done later
 */
int
sl_make_slice(sh_p handler_down, void *private_down,
	      struct slicelimits * limits,
	      sl_p * slicepp, char *type, char *name)
{
	sl_p            slice;

	/*
	 * Allocate storage for this instance .
	 */
	slice = malloc(sizeof(*slice), M_DEVBUF, M_NOWAIT);
	if (slice == NULL) {
		printf("slice failed to allocate driver storage\n");
		return (ENOMEM);
	}
	bzero(slice, sizeof(*slice));
	if (name) {
		slice->name = malloc(strlen(name) + 1, M_DEVBUF, M_NOWAIT);
		if (slice->name == NULL) {
			printf("slice failed name storage\n");
			free(slice, M_DEVBUF);
			return (ENOMEM);
		}
		strcpy(slice->name, name);
	}
	slice->handler_down = handler_down;
	slice->private_down = private_down;
	handler_down->refs++;
	slice->limits = *limits;
	slice_add_device(slice);
	slice->refs = 1; /* one for our downward creator */
	*slicepp = slice;
	if (type) {
		slice->refs++; /* don't go away *//* probably not needed */
		sl_make_handler(type, slice);
		sl_unref(slice);
	}
	return (0);
}

/*
 * Forceably start a shutdown process on a slice. Either call it's shutdown
 * method, or do the default shutdown if there is no type-specific method.
 * XXX Really should say who called us.
 */
void
sl_rmslice(sl_p slice)
{
RR;
	/*
	 * An extra reference so it doesn't go away while we are not looking.
	 */
	slice->refs++;

	if (slice->flags & SLF_INVALID) {
		/*
		 * If it's already shutting down, let it die without further
		 * taunting. "go away or I'll taunt you a second time, you
		 * silly eenglish pig-dog"
		 */
		sl_unref(slice);/* possibly the last reference */
		return;
	}

	/*
	 * Mark it as invalid so any newcomers know not to try use it.
	 * No real need to LOCK it.
	 */
	slice->flags &= ~SLF_OPEN_STATE;
	slice->flags |= SLF_INVALID;

	/*
	 * remove the device appendages.
	 * Any open vnodes SHOULD go to deadfs.
	 */
	slice_remove_device(slice);

	/*
	 * Propogate the damage upwards.
 	 * Note that the revoke method is not optional.
	 * The upper handler releases it's reference so refs--.
	 */
	if (slice->handler_up) {
		(*slice->handler_up->revoke) (slice->private_up);
	}
	sl_unref(slice);	/* One for the lower handler that called us */
	sl_unref(slice);	/* possibly the last reference */
}



void
sl_unref(sl_p slice)
{
	if ((--(slice->refs)) == 0) {
		FREE(slice, M_DEVBUF);
	}
}

/*
 * Read a block on behalf of a handler.
 * This is not a bulk IO routine but meant for probes etc.
 * I think that perhaps it should attempt to do sliceopen()
 * calls on the slice first. (XXX?)
 */
int
slice_readblock(struct slice * slice, int blkno, struct buf ** bpp)
{
	struct buf     *bp;
	int             error = 0;

	/*
	 * posibly attempt to open device?
	 */
	/* --not yet-- */
	/*
	 * Now that it is open, get the buffer and set in the parameters.
	 */
	bp = geteblk((int) slice->limits.blksize);
	if (bp == NULL) {
		return (ENOMEM);
	}
	bp->b_pblkno = bp->b_blkno = blkno;
	bp->b_bcount = slice->limits.blksize;
	bp->b_flags |= B_BUSY | B_READ;
	sliceio(slice, bp, SLW_ABOVE);
	if (biowait(bp) != 0) {
		printf("failure reading device block\n");
		error = EIO;
		bp->b_flags |= B_INVAL | B_AGE;
		brelse(bp);
		bp = NULL;
	}
	*bpp = bp;
	return (error);
}

/*
 * Read a block on behalf of a handler.
 * This is not a bulk IO routine but meant for probes etc.
 * I think that perhaps it should attempt to do sliceopen()
 * calls on the slice first. (XXX?)
 */
int
slice_writeblock(struct slice * slice, int blkno, struct buf * bp)
{
	int             error = 0;

	if (bp == NULL) {
		return (ENOMEM);
	}
	bp->b_pblkno = bp->b_blkno = blkno;
	bp->b_bcount = slice->limits.blksize;
	bp->b_flags |= B_BUSY | B_WRITE;
	sliceio(slice, bp, SLW_ABOVE);
	if (biowait(bp) != 0) {
		printf("failure reading device block\n");
		error = EIO;
	}
	return (error);
}

/*
 * functions that are used to call the next level down.
 */
void 
sliceio(sl_p slice, struct buf * bp, enum slc_who who)
{
	/* XXX do shortcuts here */

	if (slice->flags & SLF_INVALID) {
		bp->b_error = ENXIO;
		goto bad;
	}
	/*
	 * if it's from above, assume it hasn't
	 * broken it's agreement about read/write.
	 * A higher level slice would have caught it.
	 * Make no such assumption if it's this device.
	 */
	if (who == SLW_DEVICE) {
		if (((slice->flags & SLF_OPEN_DEV_WR) == 0) &&
		( (bp->b_flags & B_READ) == B_WRITE )) {
			bp->b_error = EROFS;
			goto bad;
		}
	}
	(*slice->handler_down->IOreq) (slice->private_down, bp);
	return;
bad:
	bp->b_flags |= B_ERROR;
	/* toss transfer, we're done early */
	biodone(bp);
	return;
}

/*
 * Try open a slice.
 * don't forget to say if we are above (1) or the dev (0).
 *
 * We really need to add a lot of support for CHANGING
 * what we have openned.. i.e if we have ABOVE open R/W
 * and DEVICE open R/O, then closing the device
 * should downgrade our open to those items below us to R/O.
 * This would need support in both open and close routines in both
 * slice and handler code.
 * 
 * ((*) == Illegal state.. (how did we get here?))
 * (must have been in "shoot foot mode"). 
 * A bit already set can be set again. (may represent part of an upgrade)
 * This may not hold true if we are in an 'illegal state'.
 * Some such opens will fail in an attempt to revert to a legal state.
 * success = ((request & allowed[state]) == request)
 */
#define UP_RDWR		SLF_OPEN_UP
#define CHR_RDWR	SLF_OPEN_CHR
#define CHR_RD		SLF_OPEN_CHR_RD
#define BLK_RDWR	SLF_OPEN_BLK
#define BLK_RD		SLF_OPEN_BLK_RD
static u_char allowed[64] = {
/* Present state  |  requested states allowed		*/
/* UP  CHR BLK    |      UP    CHR    BLK		*/
/* R W R W R W    |     R W    R W    R W		*/
/* 0 0 0 0 0 0		1 1    1 1    1 1	*/( UP_RDWR|CHR_RDWR|BLK_RDWR ),
/* 0 0 0 0 0 1		0 0    1 0    1 1	*/( CHR_RD|BLK_RDWR ),
/* 0 0 0 0 1 0		1 1    1 1    1 1	*/( UP_RDWR|CHR_RDWR|BLK_RDWR ),
/* 0 0 0 0 1 1		0 0    1 0    1 1	*/( CHR_RD|BLK_RDWR ),
/* 0 0 0 1 0 0		0 0    1 1    1 0	*/( CHR_RDWR|BLK_RD ),
/* 0 0 0 1 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 0 0 1 1 0		0 0    1 1    1 0	*/( CHR_RDWR|BLK_RD ),
/* 0 0 0 1 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 0 1 0 0 0		1 1    1 1    1 1	*/( UP_RDWR|CHR_RDWR|BLK_RDWR ),
/* 0 0 1 0 0 1		0 0    1 0    1 1	*/( CHR_RD|BLK_RDWR ),
/* 0 0 1 0 1 0		1 1    1 1    1 1	*/( UP_RDWR|CHR_RDWR|BLK_RDWR ),
/* 0 0 1 0 1 1		0 0    1 0    1 1	*/( CHR_RD|BLK_RDWR ),
/* 0 0 1 1 0 0		0 0    1 1    1 0	*/( CHR_RDWR|BLK_RD ),
/* 0 0 1 1 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 0 1 1 1 0		0 0    1 1    1 0	*/( CHR_RDWR|BLK_RD ),
/* 0 0 1 1 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 0 0 0 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 0 1 0 0 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 0 0 1 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 0 1 0 0 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 0 1 0 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 0 1 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 0 1 1 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 0 1 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 1 0 0 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 0 1 1 0 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 1 0 1 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 0 1 1 0 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 1 1 0 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 1 1 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 1 1 1 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 0 1 1 1 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 0 0 0 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 1 0 0 0 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 0 0 1 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 1 0 0 0 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 0 1 0 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 0 1 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 0 1 1 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 0 1 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 1 0 0 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 1 0 1 0 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 1 0 1 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 1 0 1 0 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 1 1 0 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 1 1 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 1 1 1 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 0 1 1 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 0 0 0 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 1 1 0 0 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 0 0 1 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 1 1 0 0 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 0 1 0 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 0 1 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 0 1 1 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 0 1 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 1 0 0 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 1 1 1 0 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 1 0 1 0		1 1    1 0    1 0	*/( UP_RDWR|CHR_RD|BLK_RD ),
/* 1 1 1 0 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 1 1 0 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 1 1 0 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 1 1 1 0		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ),
/* 1 1 1 1 1 1		0 0    1 0    1 0 (*)	*/( CHR_RD|BLK_RD ) };

int
sliceopen(struct slice *slice, int flags, int mode,
	struct proc * p, enum slc_who who)
{
	int s;
	int	error;
	int	sl_flags = slice->flags & SLF_OPEN_STATE;
	int	or_flags;
	int	and_flags;
	int	dn_flags;
	int	odn_flags;

	
	if (slice->flags & SLF_INVALID)
		return (ENXIO);
	/*
	 * Firstly, don't allow re-opens of what is already open
	 */
	if (error = lockslice(slice))
		return (error);
	error = EBUSY;	/* default answer */
	switch (who) {
	case	SLW_ABOVE:
		or_flags = ((flags & FREAD)  ? SLF_OPEN_UP_RD : 0);
		or_flags |= ((flags & FWRITE) ? SLF_OPEN_UP_WR : 0);
		and_flags = ~SLF_OPEN_UP;
		break;
	case	SLW_DEVICE:
		switch (mode & S_IFMT) {
		case S_IFCHR:
			or_flags = ((flags & FREAD)  ? SLF_OPEN_CHR_RD : 0);
			or_flags |= ((flags & FWRITE) ? SLF_OPEN_CHR_WR : 0);
			and_flags = ~SLF_OPEN_CHR;
			break;
		case S_IFBLK:
			or_flags = ((flags & FREAD)  ? SLF_OPEN_BLK_RD : 0);
			or_flags |= ((flags & FWRITE) ? SLF_OPEN_BLK_WR : 0);
			and_flags = ~SLF_OPEN_BLK;
			break;
		default:
			panic("slice: bad open type");
		}
/* XXX 	only accumulate flags as we don't know about all closes */
/* XXX */	if ( or_flags )
/* XXX */		and_flags = ~0;
		break;
	default:
		panic("slice: bad request source");
	}
	/* 
	 * Be appropriatly paranoid depending on the system mode.
	 * This is also probably wrong XXX
	 */
	switch(slicexclusive) {
	case 2:
		/*
		 * if any one path has it open, we forbid any other
		 * paths. Only allow an upgrade/downgrade from
		 * the same source as the present openner.
		 */
		if ( sl_flags & and_flags)
			goto reject;
	case 1: /*
		 * The behaviour is encoded into the state array given above.
	 	 */
 		if ((or_flags & allowed[sl_flags]) != or_flags)
			goto reject;
		break;
	case 0: /*
		 * Permission is granted to shoot self in foot.
		 * All three of UPPER, CHAR and BLK can be open at once.
		 */
		break;
	}
	/*
	 * Get the old open mode and the new open mode.
	 * If we already have it open in this way, don't do it again.
	 * 
	 * XXX More thought needed for the locking and open-flags.
	 * For now ignore the existance of flags other than FWRITE & FREAD.
	 */
	odn_flags = (sl_flags & SLF_OPEN_WR) ? FWRITE : 0;
	odn_flags |= (sl_flags & SLF_OPEN_RD) ? FREAD : 0;
	sl_flags &= and_flags;
	sl_flags |= or_flags;
	dn_flags = (sl_flags & SLF_OPEN_WR) ? FWRITE : 0;
	dn_flags |= (sl_flags & SLF_OPEN_RD) ? FREAD : 0;
	error = 0;
	if (dn_flags != odn_flags) {
		if ((error = (*slice->handler_down->open) (slice->private_down,
				      dn_flags, mode, p)) != 0) { 
			goto reject;
		}
	}
	slice->flags &= ~SLF_OPEN_STATE;
	slice->flags |= sl_flags;
#if 1	/* it was basically a close */
	if ((slice->flags & SLF_OPEN_STATE) == SLF_CLOSED) {
		sh_p	tp;

		/*
		 * If we had an upper handler, ask it to check if it's still
		 * valid. it may decide to self destruct.
		 */
		if (slice->handler_up) {
			(*slice->handler_up->verify)(slice);
		}
		/*
		 * If we don't have an upper handler, check if
		 * maybe there is now a suitable environment for one.
		 * We may end up with a different handler
		 * from what we had above. Maybe we should clear the hint?
		 * Maybe we should ask the lower one to re-issue the request?
		 */
		if (slice->handler_up == NULL) {
			if ((tp = slice_probeall(slice)) != NULL) {
				(*tp->constructor)(slice);
			}
		}
	}
#endif
reject:
	unlockslice(slice);
	if ((slice->flags & SLF_INVALID) == SLF_INVALID)
		error = ENODEV; /* we've been zapped while down there! */
	sl_unref(slice); /* lockslice gave us a ref.*/
	return (error);
}

#if 0
void
sliceclose(struct slice *slice, int flags, int mode,
	struct proc * p, enum slc_who who)
{
	sh_p	tp;

	if (slice->flags & SLF_INVALID) 
		return ;
	if (lockslice(slice))
		return ;
	switch (who) {
	case	SLW_ABOVE:
		slice->flags &= ~SLF_OPEN_UP;
		break;
	case	SLW_DEVICE:
		switch (mode & S_IFMT) {
		case S_IFCHR:
			slice->flags &= ~SLF_OPEN_CHR;
			break;
		case S_IFBLK:
			slice->flags &= ~SLF_OPEN_BLK;
			break;
		default:
			panic("slice: bad open type");
		}
		/*
		 * If we had an upper handler, ask it to check if it's still
		 * valid. it may decide to self destruct.
		 */
		if (slice->handler_up) {
			(*slice->handler_up->verify)(slice);
		}
		/*
		 * If we don't have an upper handler, check if
		 * maybe there is now a suitable environment for one.
		 * We may end up with a different handler
		 * from what we had above. Maybe we should clear the hint?
		 * Maybe we should ask the lower one to re-issue the request?
		 */
		if (slice->handler_up == NULL) {
			if ((tp = slice_probeall(slice)) != NULL) {
				(*tp->constructor)(slice);
			}
		}
		break;
	}
	/*
	 * Last-close semantics strike again
	 * This may refine to a downgrade if we closed (say) the last writer
	 * but there are still readers.
	 * probably open/close should merge to one 'mode-change' function.
	 * (except for a vnode reference with no mode)
	 */
	if ( (slice->flags & SLF_OPEN_STATE) == 0)
		(*slice->handler_down->close) (slice->private_down,
				      flags, mode, p);
	unlockslice(slice);
	sl_unref(slice);
	return ;
}
#endif /* 0 */

/*
 * control behaviour of slices WRT sharing:
 * 2 = no sharing
 * 1 = read on a device already mounted (or parent of) is ok. No writes.
 * 0 = go ahead.. shoot yourself in the foot.
 */
static int
sysctl_kern_slicexclusive SYSCTL_HANDLER_ARGS
{
	int error;
	int new_val = slicexclusive;

	error = sysctl_handle_int(oidp, &new_val, 0, req);
	if (error == 0) {
		if ((new_val >= 0) && (new_val < 3)) {
			slicexclusive = new_val;
		} else {
			error = EINVAL;
		}
	}
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, slicexclusive, CTLTYPE_INT|CTLFLAG_RW,
	0, sizeof slicexclusive, sysctl_kern_slicexclusive, "I", "");

