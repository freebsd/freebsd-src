/* radeon_drv.c -- ATI Radeon driver -*- linux-c -*-
 * Created: Wed Feb 14 17:10:04 2001 by gareth@valinux.com
 *
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
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
#include <opt_drm_linux.h>
#endif /* __FreeBSD__ */

#include "dev/drm/radeon.h"
#include "dev/drm/drmP.h"
#include "dev/drm/radeon_drv.h"
#if __REALLY_HAVE_SG
#include "ati_pcigart.h"
#endif

#define DRIVER_AUTHOR		"Gareth Hughes, VA Linux Systems Inc."

#define DRIVER_NAME		"radeon"
#define DRIVER_DESC		"ATI Radeon"
#define DRIVER_DATE		"20010405"

#define DRIVER_MAJOR		1
#define DRIVER_MINOR		1
#define DRIVER_PATCHLEVEL	1

#ifdef __FreeBSD__
/* List acquired from xc/xc/programs/Xserver/hw/xfree86/common/xf86PciInfo.h
 * Please report to eanholt@gladstone.uoregon.edu if your chip isn't
 * represented in the list or if the information is incorrect.
 */
/* PCI cards are not supported with DRI under FreeBSD, and the 8500
 * is not supported on any platform yet.
 */
drm_chipinfo_t DRM(devicelist)[] = {
	{0x1002, 0x4242, 0, "ATI Radeon BB 8500 (AGP)"},
	{0x1002, 0x4C57, 1, "ATI Radeon LW Mobility 7 (AGP)"},
	{0x1002, 0x4C59, 1, "ATI Radeon LY Mobility 6 (AGP)"},
	{0x1002, 0x4C5A, 1, "ATI Radeon LZ Mobility 6 (AGP)"},
	{0x1002, 0x5144, 1, "ATI Radeon QD (AGP)"},
	{0x1002, 0x5145, 1, "ATI Radeon QE (AGP)"},
	{0x1002, 0x5146, 1, "ATI Radeon QF (AGP)"},
	{0x1002, 0x5147, 1, "ATI Radeon QG (AGP)"},
	{0x1002, 0x514C, 0, "ATI Radeon QL 8500 (AGP)"},
	{0x1002, 0x514E, 0, "ATI Radeon QN 8500 (AGP)"},
	{0x1002, 0x514F, 0, "ATI Radeon QO 8500 (AGP)"},
	{0x1002, 0x5157, 1, "ATI Radeon QW 7500 (AGP)"},
	{0x1002, 0x5159, 1, "ATI Radeon QY VE (AGP)"},
	{0x1002, 0x515A, 1, "ATI Radeon QZ VE (AGP)"},
	{0x1002, 0x516C, 0, "ATI Radeon Ql 8500 (AGP)"},
	{0, 0, 0, NULL}
};
#endif /* __FreeBSD__ */

#define DRIVER_IOCTLS							     \
 [DRM_IOCTL_NR(DRM_IOCTL_DMA)]               = { radeon_cp_buffers,  1, 0 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_CP_INIT)]    = { radeon_cp_init,     1, 1 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_CP_START)]   = { radeon_cp_start,    1, 1 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_CP_STOP)]    = { radeon_cp_stop,     1, 1 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_CP_RESET)]   = { radeon_cp_reset,    1, 1 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_CP_IDLE)]    = { radeon_cp_idle,     1, 0 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_RESET)]    = { radeon_engine_reset,  1, 0 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_FULLSCREEN)] = { radeon_fullscreen,  1, 0 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_SWAP)]       = { radeon_cp_swap,     1, 0 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_CLEAR)]      = { radeon_cp_clear,    1, 0 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_VERTEX)]     = { radeon_cp_vertex,   1, 0 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_INDICES)]    = { radeon_cp_indices,  1, 0 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_TEXTURE)]    = { radeon_cp_texture,  1, 0 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_STIPPLE)]    = { radeon_cp_stipple,  1, 0 }, \
 [DRM_IOCTL_NR(DRM_IOCTL_RADEON_INDIRECT)]   = { radeon_cp_indirect, 1, 1 },


#if 0
/* GH: Count data sent to card via ring or vertex/indirect buffers.
 */
#define __HAVE_COUNTERS         3
#define __HAVE_COUNTER6         _DRM_STAT_IRQ
#define __HAVE_COUNTER7         _DRM_STAT_PRIMARY
#define __HAVE_COUNTER8         _DRM_STAT_SECONDARY
#endif


#include "dev/drm/drm_agpsupport.h"
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
static int __init radeon_options( char *str )
{
	DRM(parse_options)( str );
	return 1;
}

__setup( DRIVER_NAME "=", radeon_options );
#endif
#endif /* __linux__ */

#include "dev/drm/drm_fops.h"
#include "dev/drm/drm_init.h"
#include "dev/drm/drm_ioctl.h"
#include "dev/drm/drm_lock.h"
#include "dev/drm/drm_memory.h"
#include "dev/drm/drm_vm.h"
#ifdef __linux__
#include "dev/drm/drm_proc.h"
#include "dev/drm/drm_stub.h"
#endif /* __linux__ */
#ifdef __FreeBSD__
#include "dev/drm/drm_sysctl.h"
#endif /* __FreeBSD__ */
#if __REALLY_HAVE_SG
#include "dev/drm/drm_scatter.h"
#endif

#ifdef __FreeBSD__
DRIVER_MODULE(radeon, pci, radeon_driver, radeon_devclass, 0, 0);
#endif /* __FreeBSD__ */
