/* r128_drv.c -- ATI Rage 128 driver -*- linux-c -*-
 * Created: Mon Dec 13 09:47:27 1999 by faith@precisioninsight.com
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD$
 */

#include "dev/drm/r128.h"
#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"
#include "dev/drm/r128_drm.h"
#include "dev/drm/r128_drv.h"
#if __REALLY_HAVE_SG
#include "dev/drm/ati_pcigart.h"
#endif

/* List acquired from http://www.yourvote.com/pci/pcihdr.h and xc/xc/programs/Xserver/hw/xfree86/common/xf86PciInfo.h
 * Please report to eta@lclark.edu inaccuracies or if a chip you have works that is marked unsupported here.
 */
drm_chipinfo_t DRM(devicelist)[] = {
	{0x1002, 0x4c45, __REALLY_HAVE_SG, "ATI Rage 128 Mobility LE (PCI)"},
	{0x1002, 0x4c46, 1, "ATI Rage 128 Mobility LF (AGP)"},
	{0x1002, 0x4d46, 1, "ATI Rage 128 Mobility MF (AGP)"},
	{0x1002, 0x4d4c, 1, "ATI Rage 128 Mobility ML (AGP)"},
	{0x1002, 0x5044, __REALLY_HAVE_SG, "ATI Rage 128 Pro PD (PCI)"},
	{0x1002, 0x5046, 1, "ATI Rage 128 Pro PF (AGP)"},
	{0x1002, 0x5050, __REALLY_HAVE_SG, "ATI Rage 128 Pro PP (PCI)"},
	{0x1002, 0x5052, __REALLY_HAVE_SG, "ATI Rage 128 Pro PR (PCI)"},
	{0x1002, 0x5245, __REALLY_HAVE_SG, "ATI Rage 128 RE (PCI)"},
	{0x1002, 0x5246, 1, "ATI Rage 128 RF (AGP)"},
	{0x1002, 0x5247, 1, "ATI Rage 128 RG (AGP)"},
	{0x1002, 0x524b, __REALLY_HAVE_SG, "ATI Rage 128 RK (PCI)"},
	{0x1002, 0x524c, 1, "ATI Rage 128 RL (AGP)"},
	{0x1002, 0x534d, 1, "ATI Rage 128 SM (AGP)"},
	{0x1002, 0x5446, 1, "ATI Rage 128 Pro Ultra TF (AGP)"},
	{0x1002, 0x544C, 1, "ATI Rage 128 Pro Ultra TL (AGP)"},
	{0x1002, 0x5452, 1, "ATI Rage 128 Pro Ultra TR (AGP)"},
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
#include "dev/drm/drm_sysctl.h"
#include "dev/drm/drm_vm.h"
#if __HAVE_SG
#include "dev/drm/drm_scatter.h"
#endif

#ifdef __FreeBSD__
DRIVER_MODULE(r128, pci, r128_driver, r128_devclass, 0, 0);
#elif defined(__NetBSD__)
CFDRIVER_DECL(r128, DV_TTY, NULL);
#endif /* __FreeBSD__ */
