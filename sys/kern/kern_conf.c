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
 * $Id: kern_conf.c,v 1.37 1999/05/11 19:54:27 phk Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#define NUMCDEV 256
#define cdevsw_ALLOCSTART	(NUMCDEV/2)

struct cdevsw 	*cdevsw[NUMCDEV];
int	nchrdev = NUMCDEV;

int	bmaj2cmaj[NUMCDEV];
int	nblkdev = NUMCDEV;

/*
 * Routine to convert from character to block device number.
 *
 * A minimal stub routine can always return NODEV.
 */
dev_t
chrtoblk(dev_t dev)
{
	struct cdevsw *cd;

	if((cd = devsw(dev)) != NULL) {
          if (cd->d_bmaj != -1)
	    return(makedev(cd->d_bmaj,minor(dev)));
	}
	return(NODEV);
}

int
cdevsw_add(dev_t *descrip,
		struct cdevsw *newentry,
		struct cdevsw **oldentry)
{
	int i;
	static int setup;

	if (!setup) {
		for (i = 0; i < NUMCDEV; i++)
			if (!bmaj2cmaj[i])
				bmaj2cmaj[i] = 254;
		setup++;
	}

	if ( *descrip == NODEV) {	/* auto (0 is valid) */
		/*
		 * Search the table looking for a slot...
		 */
		for (i = cdevsw_ALLOCSTART; i < nchrdev; i++)
			if (cdevsw[i] == NULL)
				break;		/* found one! */
		/* out of allocable slots? */
		if (i >= nchrdev) {
			return ENFILE;
		}
	} else {				/* assign */
		i = major(*descrip);
		if (i < 0 || i >= nchrdev) {
			return EINVAL;
		}
	}

	/* maybe save old */
        if (oldentry) {
		*oldentry = cdevsw[i];
	}
	if (newentry) {
		newentry->d_bmaj = -1;
		newentry->d_maj = i;
	}
	/* replace with new */
	cdevsw[i] = newentry;

	/* done!  let them know where we put it */
	*descrip = makedev(i,0);
	return 0;
} 

void
cdevsw_add_generic(int bmaj, int cmaj, struct cdevsw *devsw)
{
	dev_t dev;

	dev = makedev(cmaj, 0);
	cdevsw_add(&dev, devsw, NULL);
	cdevsw[cmaj]->d_bmaj = bmaj;
	bmaj2cmaj[bmaj] = cmaj;
}

int
devsw_module_handler(module_t mod, int what, void* arg)
{
	struct devsw_module_data* data = (struct devsw_module_data*) arg;
	int error;

	if (data->cmaj == NOMAJ) 
		data->cdev = NODEV;
	else
		data->cdev = makedev(data->cmaj, 0);
	switch (what) {
	case MOD_LOAD:
		error = cdevsw_add(&data->cdev, data->cdevsw, NULL);
		if (!error && data->cdevsw->d_strategy != nostrategy) {
			if (data->bmaj == NOMAJ) {
				data->bdev = data->cdev;
				data->bmaj = data->cmaj;
			} else {
				data->bdev = makedev(data->bmaj, 0);
			}
			data->cdevsw->d_maj = data->bmaj;
			bmaj2cmaj[major(data->bdev)] = major(data->cdev);
		}
		if (!error && data->chainevh)
			error = data->chainevh(mod, what, data->chainarg);
		return error;

	case MOD_UNLOAD:
		if (data->chainevh) {
			error = data->chainevh(mod, what, data->chainarg);
			if (error)
				return error;
		}
		if (data->cdevsw->d_strategy != nostrategy)
			bmaj2cmaj[major(data->bdev)] = 0;
		error = cdevsw_add(&data->cdev, NULL, NULL);
		return error;
	}

	if (data->chainevh)
		return data->chainevh(mod, what, data->chainarg);
	else
		return 0;
}

/*
 * dev_t and u_dev_t primitives
 */

#define DEVT_FASCIST 1

int 
major(dev_t x)
{
	u_intptr_t i = (u_int)x;

#ifdef DEVT_FASCIST
	return(253 - ((i >> 8) & 0xff));
#else
	return((i >> 8) & 0xff);
#endif
}

int
minor(dev_t x)
{
	u_intptr_t i = (u_int)x;

	return(i & 0xffff00ff);
}

dev_t
makedev(int x, int y)
{
#ifdef DEVT_FASCIST
        return ((dev_t) (((253 - x) << 8) | y));
#else
        return ((dev_t) ((x << 8) | y));
#endif
}

udev_t
dev2udev(dev_t x)
{
	return umakedev(major(x), minor(x));
}

dev_t
udev2dev(udev_t x, int b)
{
	return makedev(umajor(x), uminor(x));
}

int
uminor(udev_t dev)
{
	return(dev & 0xffff00ff);
}

int
umajor(udev_t dev)
{
	return((dev & 0xff00) >> 8);
}

udev_t
umakedev(int x, int y)
{
        return ((x << 8) | y);
}

