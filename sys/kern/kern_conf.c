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
 * $Id: kern_conf.c,v 1.32 1999/05/07 10:10:50 phk Exp $
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
	int i ;

	if ( (int)*descrip == NODEV) {	/* auto (0 is valid) */
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

/*
 * note must call cdevsw_add before bdevsw_add due to d_bmaj hack.
 */
void
cdevsw_add_generic(int bmaj, int cmaj, struct cdevsw *cdevsw)
{
	dev_t dev;

	dev = makedev(cmaj, 0);
	cdevsw_add(&dev, cdevsw, NULL);
	bmaj2cmaj[bmaj] = cmaj;
}

int
devsw_module_handler(module_t mod, int what, void* arg)
{
	struct devsw_module_data* data = (struct devsw_module_data*) arg;
	int error;

	switch (what) {
	case MOD_LOAD:
		error = cdevsw_add(&data->cdev, data->cdevsw, NULL);
		if (!error && data->cdevsw->d_strategy != nostrategy) {
			if (data->bdev == NODEV)
				data->bdev = data->cdev;
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
