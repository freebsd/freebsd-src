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
 * $Id: kern_conf.c,v 1.25 1998/06/25 11:27:34 phk Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
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

static void	cdevsw_make __P((struct bdevsw *from));

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
		for (i = ALLOCSTART; i < NXXXDEV; i++)			\
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
	if (newentry)							\
		newentry->d_maj = i;					\
	/* replace with new */						\
	TTYPE[i] = newentry;						\
									\
	/* done!  let them know where we put it */			\
	*descrip = makedev(i,0);					\
	return 0;							\
} \

static ADDENTRY(bdevsw, nblkdev,bdevsw_ALLOCSTART)
ADDENTRY(cdevsw, nchrdev,cdevsw_ALLOCSTART)

/*
 * Since the bdevsw struct for a disk contains all the information
 * needed to create a cdevsw entry, these two routines do that, rather
 * than specifying it by hand.
 */

static void
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
	to->d_poll = seltrue;
	to->d_mmap = nommap;
	to->d_strategy = from->d_strategy;
	to->d_name = from->d_name;
	to->d_bdev = from;
	to->d_maj = -1;
	to->d_bmaj = from->d_maj;
	to->d_maxio = from->d_maxio;
	to->d_dump = from->d_dump;
	to->d_psize = from->d_psize;
	to->d_flags = from->d_flags;
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

int
cdevsw_module_handler(module_t mod, modeventtype_t what, void* arg)
{
	struct cdevsw_module_data* data = (struct cdevsw_module_data*) arg;
	int error;

	switch (what) {
	case MOD_LOAD:
		if (error = cdevsw_add(&data->dev, data->cdevsw, NULL))
			return error;
		break;

	case MOD_UNLOAD:
		if (error = cdevsw_add(&data->dev, NULL, NULL))
			return error;
		break;
	}

	if (data->chainevh)
		return data->chainevh(mod, what, data->chainarg);
	else
		return 0;
}

int
bdevsw_module_handler(module_t mod, modeventtype_t what, void* arg)
{
	struct bdevsw_module_data* data = (struct bdevsw_module_data*) arg;
	int error;

	switch (what) {
	case MOD_LOAD:
		cdevsw_make(data->bdevsw);
		if (error = cdevsw_add(&data->cdev, data->bdevsw->d_cdev, NULL))
			return error;
		if (error = bdevsw_add(&data->bdev, data->bdevsw, NULL))
			return error;
		break;

	case MOD_UNLOAD:
		if (error = cdevsw_add(&data->cdev, NULL, NULL))
			return error;
		if (error = bdevsw_add(&data->bdev, NULL, NULL))
			return error;
		break;
	}

	if (data->chainevh)
		return data->chainevh(mod, what, data->chainarg);
	else
		return 0;
}
