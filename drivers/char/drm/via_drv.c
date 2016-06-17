/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <linux/config.h>
#include "via.h"
#include "drmP.h"
#include "via_drm.h"
#include "via_drv.h"

#define DRIVER_AUTHOR	"VIA"

#define DRIVER_NAME		"via"
#define DRIVER_DESC		"VIA CLE 266"
#define DRIVER_DATE		"20020814"

#define DRIVER_MAJOR		1
#define DRIVER_MINOR		1
#define DRIVER_PATCHLEVEL	0


#define DRIVER_IOCTLS \
        [DRM_IOCTL_NR(DRM_IOCTL_VIA_ALLOCMEM)]  = { via_mem_alloc, 1, 0 }, \
        [DRM_IOCTL_NR(DRM_IOCTL_VIA_FREEMEM)]   = { via_mem_free,  1, 0 }, \
        [DRM_IOCTL_NR(DRM_IOCTL_VIA_AGP_INIT)]  = { via_agp_init,  1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_VIA_FB_INIT)]   = { via_fb_init,   1, 0 }, \
        [DRM_IOCTL_NR(DRM_IOCTL_VIA_MAP_INIT)]  = { via_map_init,  1, 0 }


#define __HAVE_COUNTERS		0

#include "drm_auth.h"
#include "drm_agpsupport.h"
#include "drm_bufs.h"
#include "drm_context.h"
#include "drm_dma.h"
#include "drm_drawable.h"
#include "drm_drv.h"
#include "drm_fops.h"
#include "drm_init.h"
#include "drm_ioctl.h"
#include "drm_lists.h"
#include "drm_lock.h"
#include "drm_memory.h"
#include "drm_proc.h"
#include "drm_vm.h"
#include "drm_stub.h"
