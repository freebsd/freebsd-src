/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: fb.c,v 1.3 1999/01/19 11:31:10 yokota Exp $
 */

#include "fb.h"
#include "opt_fb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/console.h>

#include <dev/fb/fbreg.h>

/* local arrays */

/*
 * We need at least one entry each in order to initialize a video card
 * for the kernel console.  The arrays will be increased dynamically
 * when necessary.
 */

static int		adapters = 1;
static video_adapter_t	*adp_ini;
static video_adapter_t	**adapter = &adp_ini;
static video_switch_t	*vidsw_ini;
       video_switch_t	**vidsw = &vidsw_ini;

#ifdef FB_INSTALL_CDEV

#define ARRAY_DELTA	4

static struct cdevsw	*vidcdevsw_ini;
static struct cdevsw	**vidcdevsw = &vidcdevsw_ini;

static void
vid_realloc_array(void)
{
	video_adapter_t **new_adp;
	video_switch_t **new_vidsw;
	struct cdevsw **new_cdevsw;
	int newsize;
	int s;

	s = spltty();
	newsize = ((adapters + ARRAY_DELTA)/ARRAY_DELTA)*ARRAY_DELTA;
	new_adp = malloc(sizeof(*new_adp)*newsize, M_DEVBUF, M_WAITOK);
	new_vidsw = malloc(sizeof(*new_vidsw)*newsize, M_DEVBUF, M_WAITOK);
	new_cdevsw = malloc(sizeof(*new_cdevsw)*newsize, M_DEVBUF, M_WAITOK);
	bzero(new_adp, sizeof(*new_adp)*newsize);
	bzero(new_vidsw, sizeof(*new_vidsw)*newsize);
	bzero(new_cdevsw, sizeof(*new_cdevsw)*newsize);
	bcopy(adapter, new_adp, sizeof(*adapter)*adapters);
	bcopy(vidsw, new_vidsw, sizeof(*vidsw)*adapters);
	bcopy(vidcdevsw, new_cdevsw, sizeof(*vidcdevsw)*adapters);
	if (adapters > 1) {
		free(adapter, M_DEVBUF);
		free(vidsw, M_DEVBUF);
		free(vidcdevsw, M_DEVBUF);
	}
	adapter = new_adp;
	vidsw = new_vidsw;
	vidcdevsw = new_cdevsw;
	adapters = newsize;
	splx(s);

	if (bootverbose)
		printf("fb: new array size %d\n", adapters);
}

#endif /* FB_INSTALL_CDEV */

/*
 * Low-level frame buffer driver functions
 * frame buffer subdrivers, such as the VGA driver, call these functions
 * to initialize the video_adapter structure and register it to the virtual
 * frame buffer driver `fb'.
 */

/* initialize the video_adapter_t structure */
void
vid_init_struct(video_adapter_t *adp, char *name, int type, int unit)
{
	adp->va_flags = 0;
	adp->va_name = name;
	adp->va_type = type;
	adp->va_unit = unit;
}

/* Register a video adapter */
int
vid_register(video_adapter_t *adp)
{
	video_driver_t **list;
	video_driver_t *p;
	int index;

	for (index = 0; index < adapters; ++index) {
		if (adapter[index] == NULL)
			break;
	}
	if (index >= adapters)
		return -1;

	adp->va_index = index;
	adp->va_token = NULL;
	list = (video_driver_t **)videodriver_set.ls_items;
	while ((p = *list++) != NULL) {
		if (strcmp(p->name, adp->va_name) == 0) {
			adapter[index] = adp;
			vidsw[index] = p->vidsw;
			return index;
		}
	}

	return -1;
}

int
vid_unregister(video_adapter_t *adp)
{
	if ((adp->va_index < 0) || (adp->va_index >= adapters))
		return ENOENT;
	if (adapter[adp->va_index] != adp)
		return ENOENT;

	adapter[adp->va_index] = NULL;
	vidsw[adp->va_index] = NULL;
	return 0;
}

/* Get video I/O function table */
video_switch_t
*vid_get_switch(char *name)
{
	video_driver_t **list;
	video_driver_t *p;

	list = (video_driver_t **)videodriver_set.ls_items;
	while ((p = *list++) != NULL) {
		if (strcmp(p->name, name) == 0)
			return p->vidsw;
	}

	return NULL;
}

/*
 * Video card client functions
 * Video card clients, such as the console driver `syscons' and the frame
 * buffer cdev driver, use these functions to claim and release a card for
 * exclusive use.
 */

/* find the video card specified by a driver name and a unit number */
int
vid_find_adapter(char *driver, int unit)
{
	int i;

	for (i = 0; i < adapters; ++i) {
		if (adapter[i] == NULL)
			continue;
		if (strcmp("*", driver) && strcmp(adapter[i]->va_name, driver))
			continue;
		if ((unit != -1) && (adapter[i]->va_unit != unit))
			continue;
		return i;
	}
	return -1;
}

/* allocate a video card */
int
vid_allocate(char *driver, int unit, void *id)
{
	int index;
	int s;

	s = spltty();
	index = vid_find_adapter(driver, unit);
	if (index >= 0) {
		if (adapter[index]->va_token) {
			splx(s);
			return -1;
		}
		adapter[index]->va_token = id;
	}
	splx(s);
	return index;
}

int
vid_release(video_adapter_t *adp, void *id)
{
	int error;
	int s;

	s = spltty();
	if (adp->va_token == NULL) {
		error = EINVAL;
	} else if (adp->va_token != id) {
		error = EPERM;
	} else {
		adp->va_token = NULL;
		error = 0;
	}
	splx(s);
	return error;
}

/* Get a video adapter structure */
video_adapter_t
*vid_get_adapter(int index)
{
	if ((index < 0) || (index >= adapters))
		return NULL;
	return adapter[index];
}

/* Configure drivers: this is a backdoor for the console driver XXX */
int
vid_configure(int flags)
{
	video_driver_t **list;
	video_driver_t *p;

	list = (video_driver_t **)videodriver_set.ls_items;
	while ((p = *list++) != NULL) {
		if (p->configure != NULL)
			(*p->configure)(flags);
	}

	return 0;
}

/*
 * Virtual frame buffer cdev driver functions
 * The virtual frame buffer driver dispatches driver functions to
 * appropriate subdrivers.
 */

#define DRIVER_NAME	"fb"

#ifdef FB_INSTALL_CDEV

#define FB_UNIT(dev)	minor(dev)
#define FB_MKMINOR(unit) (u)

#if notyet

static d_open_t		fbopen;
static d_close_t	fbclose;
static d_ioctl_t	fbioctl;
static d_mmap_t		fbmmap;

#define CDEV_MAJOR	141	/* XXX */

static struct cdevsw fb_cdevsw = {
	/* open */	fbopen,
	/* close */	fbclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	fbioctl,
	/* stop */	nostop,
	/* reset */	noreset,
	/* devtotty */	nodevtotty,
	/* poll */	nopoll,
	/* mmap */	fbmmap,
	/* strategy */	nostrategy,
	/* name */	DRIVER_NAME,
	/* parms */	noparms,
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* maxio */	0,
	/* bmaj */	-1
};

static void
vfbattach(void *arg)
{
	static int fb_devsw_installed = FALSE;
	dev_t dev;

	if (!fb_devsw_installed) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev, &fb_cdevsw, NULL);
		fb_devsw_installed = TRUE;
	}
}

PSEUDO_SET(vfbattach, fb);

#endif /* notyet */

int
fb_attach(dev_t dev, video_adapter *adp, struct cdevsw *cdevsw)
{
	int s;

	if (adp->va_index >= adapters)
		return EINVAL;
	if (adapter[adp->va_index] != adp)
		return EINVAL;

	s = spltty();
	adp->va_minor = minor(dev);
	vidcdevsw[adp->va_index] = cdevsw;
	splx(s);

	/* XXX: DEVFS? */

	if (adp->va_index + 1 >= adapters)
		vid_realloc_array();

	printf("fb%d at %s%d\n", adp->va_index, adp->va_name, adp->va_unit);
	return 0;
}

int
fb_detach(dev_t dev, video_adapter *adp, struct cdevsw *cdevsw)
{
	int s;

	if (adp->va_index >= adapters)
		return EINVAL;
	if (adapter[adp->va_index] != adp)
		return EINVAL;
	if (vidcdevsw[adp->va_index] != cdevsw)
		return EINVAL;

	s = spltty();
	vidcdevsw[adp->va_index] = NULL;
	splx(s);
	return 0;
}

#endif /* FB_INSTALL_CDEV */

static char
*adapter_name(int type)
{
    static struct {
	int type;
	char *name;
    } names[] = {
	{ KD_MONO,	"MDA" },
	{ KD_HERCULES,	"Hercules" },
	{ KD_CGA,	"CGA" },
	{ KD_EGA,	"EGA" },
	{ KD_VGA,	"VGA" },
	{ KD_PC98,	"PC-98x1" },
	{ -1,		"Unknown" },
    };
    int i;

    for (i = 0; names[i].type != -1; ++i)
	if (names[i].type == type)
	    break;
    return names[i].name;
}

void
fb_dump_adp_info(char *driver, video_adapter_t *adp, int level)
{
    if (level <= 0)
	return;

    printf("%s%d: %s%d, %s, type:%s (%d), flags:0x%x\n", 
	   DRIVER_NAME, adp->va_index, driver, adp->va_unit, adp->va_name,
	   adapter_name(adp->va_type), adp->va_type, adp->va_flags);
    printf("%s%d: port:0x%x-0x%x, crtc:0x%x, mem:0x%x 0x%x\n",
	   DRIVER_NAME, adp->va_index,
	   adp->va_io_base, adp->va_io_base + adp->va_io_size - 1,
	   adp->va_crtc_addr, adp->va_mem_base, adp->va_mem_size);
    printf("%s%d: init mode:%d, bios mode:%d, current mode:%d\n",
	   DRIVER_NAME, adp->va_index,
	   adp->va_initial_mode, adp->va_initial_bios_mode, adp->va_mode);
    printf("%s%d: window:0x%x size:%dk gran:%dk, buf:0x%x size:%dk\n",
	   DRIVER_NAME, adp->va_index, 
	   adp->va_window, (int)adp->va_window_size/1024,
	   (int)adp->va_window_gran/1024, adp->va_buffer,
	   (int)adp->va_buffer_size/1024);
}

void
fb_dump_mode_info(char *driver, video_adapter_t *adp, video_info_t *info,
		  int level)
{
    if (level <= 0)
	return;

    printf("%s%d: %s, mode:%d, flags:0x%x ", 
	   driver, adp->va_unit, adp->va_name, info->vi_mode, info->vi_flags);
    if (info->vi_flags & V_INFO_GRAPHICS)
	printf("G %dx%dx%d, %d plane(s), font:%dx%d, ",
	       info->vi_width, info->vi_height, 
	       info->vi_depth, info->vi_planes, 
	       info->vi_cwidth, info->vi_cheight); 
    else
	printf("T %dx%d, font:%dx%d, ",
	       info->vi_width, info->vi_height, 
	       info->vi_cwidth, info->vi_cheight); 
    printf("win:0x%x\n", info->vi_window);
}
