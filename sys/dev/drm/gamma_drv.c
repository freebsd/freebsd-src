/* gamma.c -- 3dlabs GMX 2000 driver -*- linux-c -*-
 * Created: Mon Jan  4 08:58:31 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD$
 */

#ifdef __linux__
#include <linux/config.h>
#endif /* __linux__ */
#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/bus.h>
#include <pci/pcivar.h>
#endif /* __FreeBSD__ */
#include "dev/drm/gamma.h"
#include "dev/drm/drmP.h"
#include "dev/drm/gamma_drv.h"

#define DRIVER_AUTHOR		"VA Linux Systems Inc."

#define DRIVER_NAME		"gamma"
#define DRIVER_DESC		"3DLabs gamma"
#define DRIVER_DATE		"20010216"

#define DRIVER_MAJOR		1
#define DRIVER_MINOR		0
#define DRIVER_PATCHLEVEL	0

#define DRIVER_IOCTLS							  \
	[DRM_IOCTL_NR(DRM_IOCTL_DMA)]	     = { gamma_dma,	  1, 0 }

#ifdef __FreeBSD__
/* List acquired from xc/xc/programs/Xserver/hw/xfree86/common/xf86PciInfo.h
 * Please report to eanholt@gladstone.uoregon.edu if your chip isn't
 * represented in the list or if the information is incorrect.
 */
drm_chipinfo_t DRM(devicelist)[] = {
	{0x3d3d, 0x0008, 1, "3DLabs Gamma"},
	{0, 0, 0, NULL}
};
#endif /* __FreeBSD__ */


#define __HAVE_COUNTERS		5
#define __HAVE_COUNTER6		_DRM_STAT_IRQ
#define __HAVE_COUNTER7		_DRM_STAT_DMA
#define __HAVE_COUNTER8		_DRM_STAT_PRIMARY
#define __HAVE_COUNTER9		_DRM_STAT_SPECIAL
#define __HAVE_COUNTER10	_DRM_STAT_MISSED


#include "dev/drm/drm_auth.h"
#include "dev/drm/drm_bufs.h"
#include "dev/drm/drm_context.h"
#include "dev/drm/drm_dma.h"
#include "dev/drm/drm_drawable.h"
#include "dev/drm/drm_drv.h"

#ifdef __linux__
#ifndef MODULE
/* DRM(options) is called by the kernel to parse command-line options
 * passed via the boot-loader (e.g., LILO).  It calls the insmod option
 * routine, drm_parse_drm.
 */

/* JH- We have to hand expand the string ourselves because of the cpp.  If
 * anyone can think of a way that we can fit into the __setup macro without
 * changing it, then please send the solution my way.
 */
static int __init gamma_options( char *str )
{
	DRM(parse_options)( str );
	return 1;
}

__setup( DRIVER_NAME "=", gamma_options );
#endif
#endif /* __linux__ */

#include "dev/drm/drm_fops.h"
#include "dev/drm/drm_init.h"
#include "dev/drm/drm_ioctl.h"
#include "dev/drm/drm_lists.h"
#include "dev/drm/drm_lock.h"
#include "dev/drm/drm_memory.h"
#ifdef __linux__
#include "dev/drm/drm_proc.h"
#endif /* __linux__ */
#include "dev/drm/drm_vm.h"
#ifdef __linux__
#include "dev/drm/drm_stub.h"
#endif /* __linux__ */
#ifdef __FreeBSD__
#include "dev/drm/drm_sysctl.h"

DRIVER_MODULE(gamma, pci, gamma_driver, gamma_devclass, 0, 0);
#endif /* __FreeBSD__ */
