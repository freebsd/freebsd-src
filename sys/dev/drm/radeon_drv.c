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
	{0x1002, 0x4242, 1, "ATI Radeon BB R200 AIW 8500DV"},
	{0x1002, 0x4336, 1, "ATI Radeon Mobility U1"},
	{0x1002, 0x4964, 1, "ATI Radeon Id R250 9000"},
	{0x1002, 0x4965, 1, "ATI Radeon Ie R250 9000"},
	{0x1002, 0x4966, 1, "ATI Radeon If R250 9000"},
	{0x1002, 0x4967, 1, "ATI Radeon Ig R250 9000"},
	{0x1002, 0x4C57, 1, "ATI Radeon LW Mobility 7500 M7"},
	{0x1002, 0x4C58, 1, "ATI Radeon LX RV200 Mobility FireGL 7800 M7"},
	{0x1002, 0x4C59, 1, "ATI Radeon LY Mobility M6"},
	{0x1002, 0x4C5A, 1, "ATI Radeon LZ Mobility M6"},
	{0x1002, 0x4C64, 1, "ATI Radeon Ld R250 Mobility 9000 M9"},
	{0x1002, 0x4C65, 1, "ATI Radeon Le R250 Mobility 9000 M9"},
	{0x1002, 0x4C66, 1, "ATI Radeon Lf R250 Mobility 9000 M9"},
	{0x1002, 0x4C67, 1, "ATI Radeon Lg R250 Mobility 9000 M9"},
	{0x1002, 0x5144, 1, "ATI Radeon QD R100"},
	{0x1002, 0x5145, 1, "ATI Radeon QE R100"},
	{0x1002, 0x5146, 1, "ATI Radeon QF R100"},
	{0x1002, 0x5147, 1, "ATI Radeon QG R100"},
	{0x1002, 0x5148, 1, "ATI Radeon QH FireGL 8x00"},
	{0x1002, 0x5149, 1, "ATI Radeon QI R200"},
	{0x1002, 0x514A, 1, "ATI Radeon QJ R200"},
	{0x1002, 0x514B, 1, "ATI Radeon QK R200"},
	{0x1002, 0x514C, 1, "ATI Radeon QL R200 8500 LE"},
	{0x1002, 0x514D, 1, "ATI Radeon QM R200 9100"},
	{0x1002, 0x514E, 1, "ATI Radeon QN R200 8500 LE"},
	{0x1002, 0x514F, 1, "ATI Radeon QO R200 8500 LE"},
	{0x1002, 0x5157, 1, "ATI Radeon QW RV200 7500"},
	{0x1002, 0x5158, 1, "ATI Radeon QX RV200 7500"},
	{0x1002, 0x5159, 1, "ATI Radeon QY RV100 VE"},
	{0x1002, 0x515A, 1, "ATI Radeon QZ RV100 VE"},
	{0x1002, 0x5168, 1, "ATI Radeon Qh R200"},
	{0x1002, 0x5169, 1, "ATI Radeon Qi R200"},
	{0x1002, 0x516A, 1, "ATI Radeon Qj R200"},
	{0x1002, 0x516B, 1, "ATI Radeon Qk R200"},
	{0x1002, 0x516C, 1, "ATI Radeon Ql R200"},
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
