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
 * $Id: kern_conf.c,v 1.48 1999/07/17 18:43:45 phk Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/vnode.h>

#define cdevsw_ALLOCSTART	(NUMCDEVSW/2)

struct cdevsw 	*cdevsw[NUMCDEVSW];

int	bmaj2cmaj[NUMCDEVSW];

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
	    return(makebdev(cd->d_bmaj,minor(dev)));
	}
	return(NODEV);
}

struct cdevsw *
devsw(dev_t dev)
{       
        return(cdevsw[major(dev)]);
}

struct cdevsw *
bdevsw(dev_t dev)
{
        struct cdevsw *c;
        int i = major(dev);

        if (bmaj2cmaj[i] == 256)
                return 0;

        c = cdevsw[bmaj2cmaj[major(dev)]];
        if (!c) {
                printf("bogus bdev dev_t %p, no cdev\n", (void *)dev);
                Debugger("Bummer");
                return 0;
        }
        /* CMAJ zero is the console, which has no strategy so this works */
        if (c->d_strategy)
                return (c);
        return (0);
}

/*
 *  Add a cdevsw entry
 */

int
cdevsw_add(struct cdevsw *newentry)
{
	int i;
	static int setup;

	if (!setup) {
		for (i = 0; i < NUMCDEVSW; i++)
			if (!bmaj2cmaj[i])
				bmaj2cmaj[i] = 256;
		setup++;
	}

	if (newentry->d_maj < 0 || newentry->d_maj >= NUMCDEVSW) {
		printf("%s: ERROR: driver has bogus cdevsw->d_maj = %d\n",
		    newentry->d_name, newentry->d_maj);
		return EINVAL;
	}

	if (cdevsw[newentry->d_maj]) {
		printf("WARNING: \"%s\" is usurping \"%s\"'s cdevsw[]\n",
		    newentry->d_name, cdevsw[newentry->d_maj]->d_name);
	}
	cdevsw[newentry->d_maj] = newentry;

	if (newentry->d_bmaj >= 0 && newentry->d_bmaj < NUMCDEVSW) {
		if (bmaj2cmaj[newentry->d_bmaj] != 256) {
			printf("WARNING: \"%s\" is usurping \"%s\"'s bmaj\n",
			    newentry->d_name, 
			    cdevsw[bmaj2cmaj[newentry->d_bmaj]]->d_name);
		}
		bmaj2cmaj[newentry->d_bmaj] = newentry->d_maj;
	}

	return 0;
} 

/*
 *  Remove a cdevsw entry
 */

int
cdevsw_remove(struct cdevsw *oldentry)
{
	if (oldentry->d_maj < 0 || oldentry->d_maj >= NUMCDEVSW) {
		printf("%s: ERROR: driver has bogus cdevsw->d_maj = %d\n",
		    oldentry->d_name, oldentry->d_maj);
		return EINVAL;
	}

	cdevsw[oldentry->d_maj] = NULL;

	if (oldentry->d_bmaj >= 0 && oldentry->d_bmaj < NUMCDEVSW) 
		bmaj2cmaj[oldentry->d_bmaj] = NULL;

	return 0;
} 

int
devsw_module_handler(module_t mod, int what, void* arg)
{
	struct devsw_module_data* data = (struct devsw_module_data*) arg;
	int error = 0;

	switch (what) {
	case MOD_LOAD:
		error = cdevsw_add(data->cdevsw);
		if (!error && data->chainevh)
			error = data->chainevh(mod, what, data->chainarg);
		return error;

	case MOD_UNLOAD:
		if (data->chainevh) {
			error = data->chainevh(mod, what, data->chainarg);
			if (error)
				return error;
		}
		cdevsw_remove(data->cdevsw);
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
	uintptr_t i = (uintptr_t)x;

#ifdef DEVT_FASCIST
	return(255 - ((i >> 8) & 0xff));
#else
	return((i >> 8) & 0xff);
#endif
}

int
minor(dev_t x)
{
	uintptr_t i = (uintptr_t)x;

	return(i & 0xffff00ff);
}

dev_t
makebdev(int x, int y)
{
	return (makedev(x, y));
}

dev_t
makedev(int x, int y)
{
#ifdef DEVT_FASCIST
        return ((dev_t)(uintptr_t) (((255 - x) << 8) | y));
#else
        return ((dev_t)(uintptr_t) ((x << 8) | y));
#endif
}

udev_t
dev2udev(dev_t x)
{
	return makeudev(major(x), minor(x));
}

dev_t
udev2dev(udev_t x, int b)
{
	switch (b) {
		case 0:
			return makedev(umajor(x), uminor(x));
		case 1:
			return makebdev(umajor(x), uminor(x));
		default:
			Debugger("udev2dev(...,X)");
			return NODEV;
	}
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
makeudev(int x, int y)
{
        return ((x << 8) | y);
}

