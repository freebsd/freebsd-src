/*-
 * Parts Copyright (c) 1995 Terrence R. Lambert
 * Copyright (c) 1995 Julian R. Elischer
 * All rights reserved.
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
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Julian R. Elischer ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: kern_conf.c,v 1.10 1996/07/23 21:52:06 phk Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#define NUMBDEV 128
#define NUMCDEV 256
#define bdevsw_ALLOCSTART	(NUMBDEV/2)
#define cdevsw_ALLOCSTART	(NUMCDEV/2)

struct bdevsw 	*bdevsw[NUMBDEV];
int	nblkdev = NUMBDEV;
struct cdevsw 	*cdevsw[NUMCDEV];
int	nchrdev = NUMCDEV;



/*
 * Routine to determine if a device is a disk.
 *
 * KLUDGE XXX add flags to cdevsw entries for disks XXX
 * A minimal stub routine can always return 0.
 */
int
isdisk(dev, type)
	dev_t dev;
	int type;
{

	switch (major(dev)) {
	case 15:		/* VBLK: vn, VCHR: cd */
		return (1);
	case 0:			/* wd */
	case 2:			/* fd */
	case 4:			/* sd */
	case 6:			/* cd */
	case 7:			/* mcd */
	case 16:		/* scd */
	case 17:		/* matcd */
	case 18:		/* ata */
	case 19:		/* wcd */
	case 20:		/* od */
		if (type == VBLK)
			return (1);
		return (0);
	case 3:			/* wd */
	case 9:			/* fd */
	case 13:		/* sd */
	case 29:		/* mcd */
	case 43:		/* vn */
	case 45:		/* scd */
	case 46:		/* matcd */
	case 69:		/* wcd */
	case 70:		/* od */
		if (type == VCHR)
			return (1);
		/* fall through */
	default:
		return (0);
	}
	/* NOTREACHED */
}


/*
 * Routine to convert from character to block device number.
 *
 * A minimal stub routine can always return NODEV.
 */
dev_t
chrtoblk(dev_t dev)
{
	struct bdevsw *bd;
	struct cdevsw *cd;

	if(cd = cdevsw[major(dev)]) {
          if ( (bd = cd->d_bdev) )
	    return(makedev(bd->d_maj,minor(dev)));
	}
	return(NODEV);
}

/*
 * (re)place an entry in the bdevsw or cdevsw table
 * return the slot used in major(*descrip)
 */
#define ADDENTRY(TTYPE,NXXXDEV,ALLOCSTART) \
int TTYPE##_add(dev_t *descrip,						\
		struct TTYPE *newentry,					\
		struct TTYPE **oldentry)				\
{									\
	int i ;								\
	if ( (int)*descrip == NODEV) {	/* auto (0 is valid) */		\
		/*							\
		 * Search the table looking for a slot...		\
		 */							\
		for (i = ALLOCSTART; i < NXXXDEV; i++)				\
			if (TTYPE[i] == NULL)				\
				break;		/* found one! */	\
		/* out of allocable slots? */				\
		if (i >= NXXXDEV) {					\
			return ENFILE;					\
		}							\
	} else {				/* assign */		\
		i = major(*descrip);					\
		if (i < 0 || i >= NXXXDEV) {				\
			return EINVAL;					\
		}							\
	}								\
									\
	/* maybe save old */						\
        if (oldentry) {							\
		*oldentry = TTYPE[i];					\
	}								\
	newentry->d_maj = i;						\
	/* replace with new */						\
	TTYPE[i] = newentry;						\
									\
	/* done!  let them know where we put it */			\
	*descrip = makedev(i,0);					\
	return 0;							\
} \

ADDENTRY(bdevsw, nblkdev,bdevsw_ALLOCSTART)
ADDENTRY(cdevsw, nchrdev,cdevsw_ALLOCSTART)

/* Maybe the author might indicate what the f*@# tehis is for? */

void
cdevsw_make(struct bdevsw *from)
{
	struct cdevsw *to = from->d_cdev;

	if (!to) 
		panic("No target cdevsw in bdevsw");
	to->d_open = from->d_open;
	to->d_close = from->d_close;
	to->d_read = rawread;
	to->d_write = rawwrite;
	to->d_ioctl = from->d_ioctl;
	to->d_stop = nostop;
	to->d_reset = nullreset;
	to->d_devtotty = nodevtotty;
	to->d_select = seltrue;
	to->d_mmap = nommap;
	to->d_strategy = from->d_strategy;
	to->d_name = from->d_name;
	to->d_bdev = from;
	to->d_maj = -1;
}

void
bdevsw_add_generic(int bdev, int cdev, struct bdevsw *bdevsw)
{
	dev_t dev;
	cdevsw_make(bdevsw);
	dev = makedev(cdev, 0);
	cdevsw_add(&dev, bdevsw->d_cdev, NULL);
	dev = makedev(bdev, 0);
	bdevsw_add(&dev, bdevsw        , NULL);
}
