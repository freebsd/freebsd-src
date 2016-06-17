/* tdfx_drv.c -- tdfx driver -*- linux-c -*-
 * Created: Thu Oct  7 10:38:32 1999 by faith@precisioninsight.com
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
 *    Daryll Strauss <daryll@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 */

#include <linux/config.h>
#include "tdfx.h"
#include "drmP.h"

#define DRIVER_AUTHOR		"VA Linux Systems Inc."

#define DRIVER_NAME		"tdfx"
#define DRIVER_DESC		"3dfx Banshee/Voodoo3+"
#define DRIVER_DATE		"20010216"

#define DRIVER_MAJOR		1
#define DRIVER_MINOR		0
#define DRIVER_PATCHLEVEL	0

#ifndef PCI_VENDOR_ID_3DFX
#define PCI_VENDOR_ID_3DFX 0x121A
#endif
#ifndef PCI_DEVICE_ID_3DFX_VOODOO5
#define PCI_DEVICE_ID_3DFX_VOODOO5 0x0009
#endif
#ifndef PCI_DEVICE_ID_3DFX_VOODOO4
#define PCI_DEVICE_ID_3DFX_VOODOO4 0x0007
#endif
#ifndef PCI_DEVICE_ID_3DFX_VOODOO3_3000 /* Voodoo3 3000 */
#define PCI_DEVICE_ID_3DFX_VOODOO3_3000 0x0005
#endif
#ifndef PCI_DEVICE_ID_3DFX_VOODOO3_2000 /* Voodoo3 3000 */
#define PCI_DEVICE_ID_3DFX_VOODOO3_2000 0x0004
#endif
#ifndef PCI_DEVICE_ID_3DFX_BANSHEE
#define PCI_DEVICE_ID_3DFX_BANSHEE 0x0003
#endif

static drm_pci_list_t DRM(idlist)[] = {
	{ PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_BANSHEE },
	{ PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_VOODOO3_2000 },
	{ PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_VOODOO3_3000 },
	{ PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_VOODOO4 },
	{ PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_VOODOO5 },
	{ 0, 0 }
};

#define DRIVER_CARD_LIST DRM(idlist)


#include "drm_auth.h"
#include "drm_bufs.h"
#include "drm_context.h"
#include "drm_dma.h"
#include "drm_drawable.h"
#include "drm_drv.h"

#include "drm_fops.h"
#include "drm_init.h"
#include "drm_ioctl.h"
#include "drm_lock.h"
#include "drm_memory.h"
#include "drm_proc.h"
#include "drm_vm.h"
#include "drm_stub.h"
