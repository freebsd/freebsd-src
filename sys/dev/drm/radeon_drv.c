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

#include "dev/drm/radeon.h"
#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"
#include "dev/drm/radeon_drm.h"
#include "dev/drm/radeon_drv.h"
#if __REALLY_HAVE_SG
#include "dev/drm/ati_pcigart.h"
#endif

drm_chipinfo_t DRM(devicelist)[] = {
	{0x1002, 0x4242, 1, "ATI Radeon BB AIW 8500DV (AGP)"},
	{0x1002, 0x4336, 1, "ATI Radeon Mobility"},
	{0x1002, 0x4337, 1, "ATI Radeon IGP 340"},
	{0x1002, 0x4964, 1, "ATI Radeon Id 9000"},
	{0x1002, 0x4965, 1, "ATI Radeon Ie 9000"},
	{0x1002, 0x4966, 1, "ATI Radeon If 9000"},
	{0x1002, 0x4967, 1, "ATI Radeon Ig 9000"},
	{0x1002, 0x496e, 1, "ATI Radeon Ig 9000"},
	{0x1002, 0x4C57, 1, "ATI Radeon LW Mobility 7 (AGP)"},
	{0x1002, 0x4C58, 1, "ATI Radeon LX Mobility 7 (AGP)"},
	{0x1002, 0x4C59, 1, "ATI Radeon LY Mobility 6 (AGP)"},
	{0x1002, 0x4C5A, 1, "ATI Radeon LZ Mobility 6 (AGP)"},
	{0x1002, 0x4C64, 1, "ATI Radeon Ld Mobility 9000 (AGP)"},
	{0x1002, 0x4C65, 1, "ATI Radeon Le Mobility 9000 (AGP)"},
	{0x1002, 0x4C66, 1, "ATI Radeon Lf Mobility 9000 (AGP)"},
	{0x1002, 0x4C67, 1, "ATI Radeon Lg Mobility 9000 (AGP)"},
	{0x1002, 0x5144, 1, "ATI Radeon QD R100 (AGP)"},
	{0x1002, 0x5145, 1, "ATI Radeon QE R100 (AGP)"},
	{0x1002, 0x5146, 1, "ATI Radeon QF R100 (AGP)"},
	{0x1002, 0x5147, 1, "ATI Radeon QG R100 (AGP)"},
	{0x1002, 0x5148, 1, "ATI Radeon QH FireGL 8x00 (AGP)"},
	{0x1002, 0x5149, 1, "ATI Radeon QI R200"},
	{0x1002, 0x514A, 1, "ATI Radeon QJ R200"},
	{0x1002, 0x514B, 1, "ATI Radeon QK R200"},
	{0x1002, 0x514C, 1, "ATI Radeon QL 8500 (AGP)"},
	{0x1002, 0x5157, 1, "ATI Radeon QW 7500 (AGP)"},
	{0x1002, 0x5158, 1, "ATI Radeon QX 7500 (AGP)"},
	{0x1002, 0x5159, 1, "ATI Radeon QY VE (AGP)"},
	{0x1002, 0x515A, 1, "ATI Radeon QZ VE (AGP)"},
	{0x1002, 0x5168, 1, "ATI Radeon Qh R200"},
	{0x1002, 0x5169, 1, "ATI Radeon Qi R200"},
	{0x1002, 0x516A, 1, "ATI Radeon Qj R200"},
	{0x1002, 0x516B, 1, "ATI Radeon Qk R200"},
	{0, 0, 0, NULL}
};

#include "dev/drm/drm_agpsupport.h"
#include "dev/drm/drm_auth.h"
#include "dev/drm/drm_bufs.h"
#include "dev/drm/drm_context.h"
#include "dev/drm/drm_dma.h"
#include "dev/drm/drm_drawable.h"
#include "dev/drm/drm_drv.h"
#include "dev/drm/drm_fops.h"
#include "dev/drm/drm_init.h"
#include "dev/drm/drm_ioctl.h"
#include "dev/drm/drm_lock.h"
#include "dev/drm/drm_memory.h"
#include "dev/drm/drm_vm.h"
#include "dev/drm/drm_sysctl.h"
#if __HAVE_SG
#include "dev/drm/drm_scatter.h"
#endif

#ifdef __FreeBSD__
DRIVER_MODULE(DRIVER_NAME, pci, DRM(driver), DRM(devclass), 0, 0);
#elif defined(__NetBSD__)
CFDRIVER_DECL(radeon, DV_TTY, NULL);
#endif /* __FreeBSD__ */
