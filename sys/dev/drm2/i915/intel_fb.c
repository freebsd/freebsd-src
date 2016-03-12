/*
 * Copyright © 2007 David Airlie
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *     David Airlie
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_syscons.h"
#include <dev/drm2/drmP.h>
#include <dev/drm2/drm_crtc.h>
#include <dev/drm2/drm_fb_helper.h>
#include <dev/drm2/i915/intel_drv.h>
#include <dev/drm2/i915/i915_drm.h>
#include <dev/drm2/i915/i915_drv.h>

#if defined(__linux__)
static struct fb_ops intelfb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcmap = drm_fb_helper_setcmap,
	.fb_debug_enter = drm_fb_helper_debug_enter,
	.fb_debug_leave = drm_fb_helper_debug_leave,
};
#endif

static int intelfb_create(struct intel_fbdev *ifbdev,
			  struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *dev = ifbdev->helper.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct fb_info *info;
	struct drm_framebuffer *fb;
	struct drm_mode_fb_cmd2 mode_cmd = {};
	struct drm_i915_gem_object *obj;
	int size, ret;

	/* we don't do packed 24bpp */
	if (sizes->surface_bpp == 24)
		sizes->surface_bpp = 32;

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;

	mode_cmd.pitches[0] = roundup2(mode_cmd.width * ((sizes->surface_bpp + 7) /
						      8), 64);
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
							  sizes->surface_depth);

	size = mode_cmd.pitches[0] * mode_cmd.height;
	size = roundup2(size, PAGE_SIZE);
	obj = i915_gem_alloc_object(dev, size);
	if (!obj) {
		DRM_ERROR("failed to allocate framebuffer\n");
		ret = -ENOMEM;
		goto out;
	}

	DRM_LOCK(dev);

	/* Flush everything out, we'll be doing GTT only from now on */
	ret = intel_pin_and_fence_fb_obj(dev, obj, NULL);
	if (ret) {
		DRM_ERROR("failed to pin fb: %d\n", ret);
		goto out_unref;
	}

	info = framebuffer_alloc();
	if (!info) {
		ret = -ENOMEM;
		goto out_unpin;
	}

	info->fb_size = size;
	info->fb_bpp = sizes->surface_bpp;
	info->fb_pbase = dev_priv->mm.gtt_base_addr + obj->gtt_offset;
	info->fb_vbase = (vm_offset_t)pmap_mapdev_attr(info->fb_pbase, size,
	    PAT_WRITE_COMBINING);

	ret = intel_framebuffer_init(dev, &ifbdev->ifb, &mode_cmd, obj);
	if (ret)
		goto out_unpin;

	fb = &ifbdev->ifb.base;

	ifbdev->helper.fb = fb;
	ifbdev->helper.fbdev = info;

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->depth);
	drm_fb_helper_fill_var(info, &ifbdev->helper, sizes->fb_width, sizes->fb_height);

	/* Use default scratch pixmap (info->pixmap.flags = FB_PIXMAP_SYSTEM) */

	DRM_DEBUG_KMS("allocated %dx%d (s %dbits) fb: 0x%08x, bo %p\n",
		      fb->width, fb->height, fb->depth,
		      obj->gtt_offset, obj);


	DRM_UNLOCK(dev);
#ifdef __linux__
	vga_switcheroo_client_fb_set(dev->pdev, info);
#endif
	return 0;

out_unpin:
	i915_gem_object_unpin(obj);
out_unref:
	drm_gem_object_unreference(&obj->base);
	DRM_UNLOCK(dev);
out:
	return ret;
}

static int intel_fb_find_or_create_single(struct drm_fb_helper *helper,
					  struct drm_fb_helper_surface_size *sizes)
{
	struct intel_fbdev *ifbdev = (struct intel_fbdev *)helper;
	int new_fb = 0;
	int ret;

	if (!helper->fb) {
		ret = intelfb_create(ifbdev, sizes);
		if (ret)
			return ret;
		new_fb = 1;
	}
	return new_fb;
}

static struct drm_fb_helper_funcs intel_fb_helper_funcs = {
	.gamma_set = intel_crtc_fb_gamma_set,
	.gamma_get = intel_crtc_fb_gamma_get,
	.fb_probe = intel_fb_find_or_create_single,
};

static void intel_fbdev_destroy(struct drm_device *dev,
				struct intel_fbdev *ifbdev)
{
	struct fb_info *info;
	struct intel_framebuffer *ifb = &ifbdev->ifb;

	if (ifbdev->helper.fbdev) {
		info = ifbdev->helper.fbdev;
		if (info->fb_fbd_dev != NULL)
			device_delete_child(dev->dev, info->fb_fbd_dev);
		framebuffer_release(info);
	}

	drm_fb_helper_fini(&ifbdev->helper);

	drm_framebuffer_cleanup(&ifb->base);
	if (ifb->obj) {
		drm_gem_object_unreference_unlocked(&ifb->obj->base);
		ifb->obj = NULL;
	}
}

#ifdef DEV_SC
extern int sc_txtmouse_no_retrace_wait;
#endif

int intel_fbdev_init(struct drm_device *dev)
{
	struct intel_fbdev *ifbdev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	ifbdev = malloc(sizeof(struct intel_fbdev), DRM_MEM_KMS, M_WAITOK | M_ZERO);
	if (!ifbdev)
		return -ENOMEM;

	dev_priv->fbdev = ifbdev;
	ifbdev->helper.funcs = &intel_fb_helper_funcs;

	ret = drm_fb_helper_init(dev, &ifbdev->helper,
				 dev_priv->num_pipe,
				 INTELFB_CONN_LIMIT);
	if (ret) {
		free(ifbdev, DRM_MEM_KMS);
		return ret;
	}

	drm_fb_helper_single_add_all_connectors(&ifbdev->helper);
	drm_fb_helper_initial_config(&ifbdev->helper, 32);
#ifdef DEV_SC
	sc_txtmouse_no_retrace_wait = 1;
#endif
	return 0;
}

void intel_fbdev_fini(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	if (!dev_priv->fbdev)
		return;

	intel_fbdev_destroy(dev, dev_priv->fbdev);
	free(dev_priv->fbdev, DRM_MEM_KMS);
	dev_priv->fbdev = NULL;
}

void intel_fbdev_set_suspend(struct drm_device *dev, int state)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	if (!dev_priv->fbdev)
		return;

#ifdef FREEBSD_WIP
	fb_set_suspend(dev_priv->fbdev->helper.fbdev, state);
#endif /* FREEBSD_WIP */
}

MODULE_LICENSE("GPL and additional rights");

void intel_fb_output_poll_changed(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_fb_helper_hotplug_event(&dev_priv->fbdev->helper);
}

void intel_fb_restore_mode(struct drm_device *dev)
{
	int ret;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_mode_config *config = &dev->mode_config;
	struct drm_plane *plane;

	sx_xlock(&dev->mode_config.mutex);

	ret = drm_fb_helper_restore_fbdev_mode(&dev_priv->fbdev->helper);
	if (ret)
		DRM_DEBUG("failed to restore crtc mode\n");

	/* Be sure to shut off any planes that may be active */
	list_for_each_entry(plane, &config->plane_list, head)
		plane->funcs->disable_plane(plane);

	sx_xunlock(&dev->mode_config.mutex);
}
