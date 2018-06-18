/**
 * \file radeon_ioc32.c
 *
 * 32-bit ioctl compatibility routines for the Radeon DRM.
 *
 * \author Paul Mackerras <paulus@samba.org>
 *
 * Copyright (C) Paul Mackerras 2005
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
 * THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef COMPAT_FREEBSD32

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm.h>
#include <dev/drm2/radeon/radeon_drm.h>
#include "radeon_drv.h"

typedef struct drm_radeon_init32 {
	int func;
	u32 sarea_priv_offset;
	int is_pci;
	int cp_mode;
	int gart_size;
	int ring_size;
	int usec_timeout;

	unsigned int fb_bpp;
	unsigned int front_offset, front_pitch;
	unsigned int back_offset, back_pitch;
	unsigned int depth_bpp;
	unsigned int depth_offset, depth_pitch;

	u32 fb_offset;
	u32 mmio_offset;
	u32 ring_offset;
	u32 ring_rptr_offset;
	u32 buffers_offset;
	u32 gart_textures_offset;
} drm_radeon_init32_t;

static int compat_radeon_cp_init(struct drm_device *dev, void *arg,
				 struct drm_file *file_priv)
{
	drm_radeon_init32_t *init32;
	drm_radeon_init_t __user init;

	init32 = arg;

	init.func = init32->func;
	init.sarea_priv_offset = (unsigned long)init32->sarea_priv_offset;
	init.is_pci = init32->is_pci;
	init.cp_mode = init32->cp_mode;
	init.gart_size = init32->gart_size;
	init.ring_size = init32->ring_size;
	init.usec_timeout = init32->usec_timeout;
	init.fb_bpp = init32->fb_bpp;
	init.front_offset = init32->front_offset;
	init.front_pitch = init32->front_pitch;
	init.back_offset = init32->back_offset;
	init.back_pitch = init32->back_pitch;
	init.depth_bpp = init32->depth_bpp;
	init.depth_offset = init32->depth_offset;
	init.depth_pitch = init32->depth_pitch;
	init.fb_offset = (unsigned long)init32->fb_offset;
	init.mmio_offset = (unsigned long)init32->mmio_offset;
	init.ring_offset = (unsigned long)init32->ring_offset;
	init.ring_rptr_offset = (unsigned long)init32->ring_rptr_offset;
	init.buffers_offset = (unsigned long)init32->buffers_offset;
	init.gart_textures_offset = (unsigned long)init32->gart_textures_offset;

	return radeon_cp_init(dev, &init, file_priv);
}

typedef struct drm_radeon_clear32 {
	unsigned int flags;
	unsigned int clear_color;
	unsigned int clear_depth;
	unsigned int color_mask;
	unsigned int depth_mask;	/* misnamed field:  should be stencil */
	u32 depth_boxes;
} drm_radeon_clear32_t;

static int compat_radeon_cp_clear(struct drm_device *dev, void *arg,
				  struct drm_file *file_priv)
{
	drm_radeon_clear32_t *clr32;
	drm_radeon_clear_t __user clr;

	clr32 = arg;

	clr.flags = clr32->flags;
	clr.clear_color = clr32->clear_color;
	clr.clear_depth = clr32->clear_depth;
	clr.color_mask = clr32->color_mask;
	clr.depth_mask = clr32->depth_mask;
	clr.depth_boxes = (drm_radeon_clear_rect_t *)(unsigned long)clr32->depth_boxes;

	return radeon_ioctls[DRM_IOCTL_RADEON_CLEAR].func(dev, &clr, file_priv);
}

typedef struct drm_radeon_stipple32 {
	u32 mask;
} drm_radeon_stipple32_t;

static int compat_radeon_cp_stipple(struct drm_device *dev, void *arg,
				    struct drm_file *file_priv)
{
	drm_radeon_stipple32_t __user *argp = (void __user *)arg;
	drm_radeon_stipple_t __user request;

	request.mask = (unsigned int *)(unsigned long)argp->mask;

	return radeon_ioctls[DRM_IOCTL_RADEON_STIPPLE].func(dev, &request, file_priv);
}

typedef struct drm_radeon_tex_image32 {
	unsigned int x, y;	/* Blit coordinates */
	unsigned int width, height;
	u32 data;
} drm_radeon_tex_image32_t;

typedef struct drm_radeon_texture32 {
	unsigned int offset;
	int pitch;
	int format;
	int width;		/* Texture image coordinates */
	int height;
	u32 image;
} drm_radeon_texture32_t;

static int compat_radeon_cp_texture(struct drm_device *dev, void *arg,
				    struct drm_file *file_priv)
{
	drm_radeon_texture32_t *req32;
	drm_radeon_texture_t __user request;
	drm_radeon_tex_image32_t *img32;
	drm_radeon_tex_image_t __user image;

	req32 = arg;
	if (req32->image == 0)
		return -EINVAL;
	img32 = (drm_radeon_tex_image32_t *)(unsigned long)req32->image;

	request.offset = req32->offset;
	request.pitch = req32->pitch;
	request.format = req32->format;
	request.width = req32->width;
	request.height = req32->height;
	request.image = &image;
	image.x = img32->x;
	image.y = img32->y;
	image.width = img32->width;
	image.height = img32->height;
	image.data = (void *)(unsigned long)img32->data;

	return radeon_ioctls[DRM_IOCTL_RADEON_TEXTURE].func(dev, &request, file_priv);
}

typedef struct drm_radeon_vertex2_32 {
	int idx;		/* Index of vertex buffer */
	int discard;		/* Client finished with buffer? */
	int nr_states;
	u32 state;
	int nr_prims;
	u32 prim;
} drm_radeon_vertex2_32_t;

static int compat_radeon_cp_vertex2(struct drm_device *dev, void *arg,
				    struct drm_file *file_priv)
{
	drm_radeon_vertex2_32_t *req32;
	drm_radeon_vertex2_t __user request;

	req32 = arg;

	request.idx = req32->idx;
	request.discard = req32->discard;
	request.nr_states = req32->nr_states;
	request.state = (drm_radeon_state_t *)(unsigned long)req32->state;
	request.nr_prims = req32->nr_prims;
	request.prim = (drm_radeon_prim_t *)(unsigned long)req32->prim;

	return radeon_ioctls[DRM_IOCTL_RADEON_VERTEX2].func(dev, &request, file_priv);
}

typedef struct drm_radeon_cmd_buffer32 {
	int bufsz;
	u32 buf;
	int nbox;
	u32 boxes;
} drm_radeon_cmd_buffer32_t;

static int compat_radeon_cp_cmdbuf(struct drm_device *dev, void *arg,
				   struct drm_file *file_priv)
{
	drm_radeon_cmd_buffer32_t *req32;
	drm_radeon_cmd_buffer_t __user request;

	req32 = arg;

	request.bufsz = req32->bufsz;
	request.buf = (char *)(unsigned long)req32->buf;
	request.nbox = req32->nbox;
	request.boxes = (struct drm_clip_rect *)(unsigned long)req32->boxes;

	return radeon_ioctls[DRM_IOCTL_RADEON_CMDBUF].func(dev, &request, file_priv);
}

typedef struct drm_radeon_getparam32 {
	int param;
	u32 value;
} drm_radeon_getparam32_t;

static int compat_radeon_cp_getparam(struct drm_device *dev, void *arg,
				     struct drm_file *file_priv)
{
	drm_radeon_getparam32_t *req32;
	drm_radeon_getparam_t __user request;

	req32 = arg;

	request.param = req32->param;
	request.value = (void *)(unsigned long)req32->value;

	return radeon_ioctls[DRM_IOCTL_RADEON_GETPARAM].func(dev, &request, file_priv);
}

typedef struct drm_radeon_mem_alloc32 {
	int region;
	int alignment;
	int size;
	u32 region_offset;	/* offset from start of fb or GART */
} drm_radeon_mem_alloc32_t;

static int compat_radeon_mem_alloc(struct drm_device *dev, void *arg,
				   struct drm_file *file_priv)
{
	drm_radeon_mem_alloc32_t *req32;
	drm_radeon_mem_alloc_t __user request;

	req32 = arg;

	request.region = req32->region;
	request.alignment = req32->alignment;
	request.size = req32->size;
	request.region_offset = (int *)(unsigned long)req32->region_offset;

	return radeon_mem_alloc(dev, &request, file_priv);
}

typedef struct drm_radeon_irq_emit32 {
	u32 irq_seq;
} drm_radeon_irq_emit32_t;

static int compat_radeon_irq_emit(struct drm_device *dev, void *arg,
				  struct drm_file *file_priv)
{
	drm_radeon_irq_emit32_t *req32;
	drm_radeon_irq_emit_t __user request;

	req32 = arg;

	request.irq_seq = (int *)(unsigned long)req32->irq_seq;

	return radeon_irq_emit(dev, &request, file_priv);
}

/* The two 64-bit arches where alignof(u64)==4 in 32-bit code */
#if defined (CONFIG_X86_64) || defined(CONFIG_IA64)
typedef struct drm_radeon_setparam32 {
	int param;
	u64 value;
} __attribute__((packed)) drm_radeon_setparam32_t;

static int compat_radeon_cp_setparam(struct drm_device *dev, void *arg,
				     struct drm_file *file_priv)
{
	drm_radeon_setparam32_t *req32;
	drm_radeon_setparam_t __user request;

	req32 = arg;

	request.param = req32->param;
	request.value = req32->value;

	return radeon_ioctls[DRM_IOCTL_RADEON_SETPARAM].func(dev, &request, file_priv);
}
#else
#define compat_radeon_cp_setparam NULL
#endif /* X86_64 || IA64 */

struct drm_ioctl_desc radeon_compat_ioctls[] = {
	DRM_IOCTL_DEF(DRM_RADEON_CP_INIT, compat_radeon_cp_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_RADEON_CLEAR, compat_radeon_cp_clear, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_STIPPLE, compat_radeon_cp_stipple, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_TEXTURE, compat_radeon_cp_texture, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_VERTEX2, compat_radeon_cp_vertex2, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_CMDBUF, compat_radeon_cp_cmdbuf, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_GETPARAM, compat_radeon_cp_getparam, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_SETPARAM, compat_radeon_cp_setparam, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_ALLOC, compat_radeon_mem_alloc, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_RADEON_IRQ_EMIT, compat_radeon_irq_emit, DRM_AUTH)
};
int radeon_num_compat_ioctls = ARRAY_SIZE(radeon_compat_ioctls);

#endif
