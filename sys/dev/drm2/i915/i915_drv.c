/* i915_drv.c -- Intel i915 driver -*- linux-c -*-
 * Created: Wed Feb 14 17:10:04 2001 by gareth@valinux.com
 */
/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm.h>
#include <dev/drm2/drm_mm.h>
#include <dev/drm2/i915/i915_drm.h>
#include <dev/drm2/i915/i915_drv.h>
#include <dev/drm2/drm_pciids.h>
#include <dev/drm2/i915/intel_drv.h>

#include "fb_if.h"

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t i915_pciidlist[] = {
	i915_PCI_IDS
};

#define INTEL_VGA_DEVICE(id, info_) {		\
	.device = id,				\
	.info = info_,				\
}

static const struct intel_device_info intel_i830_info = {
	.gen = 2, .is_mobile = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_845g_info = {
	.gen = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_i85x_info = {
	.gen = 2, .is_i85x = 1, .is_mobile = 1,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_i865g_info = {
	.gen = 2,
	.has_overlay = 1, .overlay_needs_physical = 1,
};

static const struct intel_device_info intel_i915g_info = {
	.gen = 3, .is_i915g = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};
static const struct intel_device_info intel_i915gm_info = {
	.gen = 3, .is_mobile = 1,
	.cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
};
static const struct intel_device_info intel_i945g_info = {
	.gen = 3, .has_hotplug = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
};
static const struct intel_device_info intel_i945gm_info = {
	.gen = 3, .is_i945gm = 1, .is_mobile = 1,
	.has_hotplug = 1, .cursor_needs_physical = 1,
	.has_overlay = 1, .overlay_needs_physical = 1,
	.supports_tv = 1,
};

static const struct intel_device_info intel_i965g_info = {
	.gen = 4, .is_broadwater = 1,
	.has_hotplug = 1,
	.has_overlay = 1,
};

static const struct intel_device_info intel_i965gm_info = {
	.gen = 4, .is_crestline = 1,
	.is_mobile = 1, .has_fbc = 1, .has_hotplug = 1,
	.has_overlay = 1,
	.supports_tv = 1,
};

static const struct intel_device_info intel_g33_info = {
	.gen = 3, .is_g33 = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
};

static const struct intel_device_info intel_g45_info = {
	.gen = 4, .is_g4x = 1, .need_gfx_hws = 1,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
};

static const struct intel_device_info intel_gm45_info = {
	.gen = 4, .is_g4x = 1,
	.is_mobile = 1, .need_gfx_hws = 1, .has_fbc = 1,
	.has_pipe_cxsr = 1, .has_hotplug = 1,
	.supports_tv = 1,
	.has_bsd_ring = 1,
};

static const struct intel_device_info intel_pineview_info = {
	.gen = 3, .is_g33 = 1, .is_pineview = 1, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_overlay = 1,
};

static const struct intel_device_info intel_ironlake_d_info = {
	.gen = 5,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_pch_split = 1,
};

static const struct intel_device_info intel_ironlake_m_info = {
	.gen = 5, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0, /* disabled due to buggy hardware */
	.has_bsd_ring = 1,
	.has_pch_split = 1,
};

static const struct intel_device_info intel_sandybridge_d_info = {
	.gen = 6,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_pch_split = 1,
};

static const struct intel_device_info intel_sandybridge_m_info = {
	.gen = 6, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_pch_split = 1,
};

static const struct intel_device_info intel_ivybridge_d_info = {
	.is_ivybridge = 1, .gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_pch_split = 1,
};

static const struct intel_device_info intel_ivybridge_m_info = {
	.is_ivybridge = 1, .gen = 7, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,	/* FBC is not enabled on Ivybridge mobile yet */
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_pch_split = 1,
};

static const struct intel_device_info intel_valleyview_m_info = {
	.gen = 7, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.is_valleyview = 1,
};

static const struct intel_device_info intel_valleyview_d_info = {
	.gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_fbc = 0,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.is_valleyview = 1,
};

static const struct intel_device_info intel_haswell_d_info = {
	.is_haswell = 1, .gen = 7,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_pch_split = 1,
	.not_supported = 1,
};

static const struct intel_device_info intel_haswell_m_info = {
	.is_haswell = 1, .gen = 7, .is_mobile = 1,
	.need_gfx_hws = 1, .has_hotplug = 1,
	.has_bsd_ring = 1,
	.has_blt_ring = 1,
	.has_llc = 1,
	.has_pch_split = 1,
	.not_supported = 1,
};

static const struct intel_gfx_device_id {
	int device;
	const struct intel_device_info *info;
} pciidlist[] = {						/* aka */
	INTEL_VGA_DEVICE(0x3577, &intel_i830_info),		/* I830_M */
	INTEL_VGA_DEVICE(0x2562, &intel_845g_info),		/* 845_G */
	INTEL_VGA_DEVICE(0x3582, &intel_i85x_info),		/* I855_GM */
	INTEL_VGA_DEVICE(0x358e, &intel_i85x_info),
	INTEL_VGA_DEVICE(0x2572, &intel_i865g_info),		/* I865_G */
	INTEL_VGA_DEVICE(0x2582, &intel_i915g_info),		/* I915_G */
	INTEL_VGA_DEVICE(0x258a, &intel_i915g_info),		/* E7221_G */
	INTEL_VGA_DEVICE(0x2592, &intel_i915gm_info),		/* I915_GM */
	INTEL_VGA_DEVICE(0x2772, &intel_i945g_info),		/* I945_G */
	INTEL_VGA_DEVICE(0x27a2, &intel_i945gm_info),		/* I945_GM */
	INTEL_VGA_DEVICE(0x27ae, &intel_i945gm_info),		/* I945_GME */
	INTEL_VGA_DEVICE(0x2972, &intel_i965g_info),		/* I946_GZ */
	INTEL_VGA_DEVICE(0x2982, &intel_i965g_info),		/* G35_G */
	INTEL_VGA_DEVICE(0x2992, &intel_i965g_info),		/* I965_Q */
	INTEL_VGA_DEVICE(0x29a2, &intel_i965g_info),		/* I965_G */
	INTEL_VGA_DEVICE(0x29b2, &intel_g33_info),		/* Q35_G */
	INTEL_VGA_DEVICE(0x29c2, &intel_g33_info),		/* G33_G */
	INTEL_VGA_DEVICE(0x29d2, &intel_g33_info),		/* Q33_G */
	INTEL_VGA_DEVICE(0x2a02, &intel_i965gm_info),		/* I965_GM */
	INTEL_VGA_DEVICE(0x2a12, &intel_i965gm_info),		/* I965_GME */
	INTEL_VGA_DEVICE(0x2a42, &intel_gm45_info),		/* GM45_G */
	INTEL_VGA_DEVICE(0x2e02, &intel_g45_info),		/* IGD_E_G */
	INTEL_VGA_DEVICE(0x2e12, &intel_g45_info),		/* Q45_G */
	INTEL_VGA_DEVICE(0x2e22, &intel_g45_info),		/* G45_G */
	INTEL_VGA_DEVICE(0x2e32, &intel_g45_info),		/* G41_G */
	INTEL_VGA_DEVICE(0x2e42, &intel_g45_info),		/* B43_G */
	INTEL_VGA_DEVICE(0x2e92, &intel_g45_info),		/* B43_G.1 */
	INTEL_VGA_DEVICE(0xa001, &intel_pineview_info),
	INTEL_VGA_DEVICE(0xa011, &intel_pineview_info),
	INTEL_VGA_DEVICE(0x0042, &intel_ironlake_d_info),
	INTEL_VGA_DEVICE(0x0046, &intel_ironlake_m_info),
	INTEL_VGA_DEVICE(0x0102, &intel_sandybridge_d_info),
	INTEL_VGA_DEVICE(0x0112, &intel_sandybridge_d_info),
	INTEL_VGA_DEVICE(0x0122, &intel_sandybridge_d_info),
	INTEL_VGA_DEVICE(0x0106, &intel_sandybridge_m_info),
	INTEL_VGA_DEVICE(0x0116, &intel_sandybridge_m_info),
	INTEL_VGA_DEVICE(0x0126, &intel_sandybridge_m_info),
	INTEL_VGA_DEVICE(0x010A, &intel_sandybridge_d_info),
	INTEL_VGA_DEVICE(0x0156, &intel_ivybridge_m_info), /* GT1 mobile */
	INTEL_VGA_DEVICE(0x0166, &intel_ivybridge_m_info), /* GT2 mobile */
	INTEL_VGA_DEVICE(0x0152, &intel_ivybridge_d_info), /* GT1 desktop */
	INTEL_VGA_DEVICE(0x0162, &intel_ivybridge_d_info), /* GT2 desktop */
	INTEL_VGA_DEVICE(0x015a, &intel_ivybridge_d_info), /* GT1 server */
	INTEL_VGA_DEVICE(0x016a, &intel_ivybridge_d_info), /* GT2 server */
	INTEL_VGA_DEVICE(0x0402, &intel_haswell_d_info), /* GT1 desktop */
	INTEL_VGA_DEVICE(0x0412, &intel_haswell_d_info), /* GT2 desktop */
	INTEL_VGA_DEVICE(0x0422, &intel_haswell_d_info), /* GT2 desktop */
	INTEL_VGA_DEVICE(0x040a, &intel_haswell_d_info), /* GT1 server */
	INTEL_VGA_DEVICE(0x041a, &intel_haswell_d_info), /* GT2 server */
	INTEL_VGA_DEVICE(0x042a, &intel_haswell_d_info), /* GT2 server */
	INTEL_VGA_DEVICE(0x0406, &intel_haswell_m_info), /* GT1 mobile */
	INTEL_VGA_DEVICE(0x0416, &intel_haswell_m_info), /* GT2 mobile */
	INTEL_VGA_DEVICE(0x0426, &intel_haswell_m_info), /* GT2 mobile */
	INTEL_VGA_DEVICE(0x0C02, &intel_haswell_d_info), /* SDV GT1 desktop */
	INTEL_VGA_DEVICE(0x0C12, &intel_haswell_d_info), /* SDV GT2 desktop */
	INTEL_VGA_DEVICE(0x0C22, &intel_haswell_d_info), /* SDV GT2 desktop */
	INTEL_VGA_DEVICE(0x0C0A, &intel_haswell_d_info), /* SDV GT1 server */
	INTEL_VGA_DEVICE(0x0C1A, &intel_haswell_d_info), /* SDV GT2 server */
	INTEL_VGA_DEVICE(0x0C2A, &intel_haswell_d_info), /* SDV GT2 server */
	INTEL_VGA_DEVICE(0x0C06, &intel_haswell_m_info), /* SDV GT1 mobile */
	INTEL_VGA_DEVICE(0x0C16, &intel_haswell_m_info), /* SDV GT2 mobile */
	INTEL_VGA_DEVICE(0x0C26, &intel_haswell_m_info), /* SDV GT2 mobile */
	INTEL_VGA_DEVICE(0x0A02, &intel_haswell_d_info), /* ULT GT1 desktop */
	INTEL_VGA_DEVICE(0x0A12, &intel_haswell_d_info), /* ULT GT2 desktop */
	INTEL_VGA_DEVICE(0x0A22, &intel_haswell_d_info), /* ULT GT2 desktop */
	INTEL_VGA_DEVICE(0x0A0A, &intel_haswell_d_info), /* ULT GT1 server */
	INTEL_VGA_DEVICE(0x0A1A, &intel_haswell_d_info), /* ULT GT2 server */
	INTEL_VGA_DEVICE(0x0A2A, &intel_haswell_d_info), /* ULT GT2 server */
	INTEL_VGA_DEVICE(0x0A06, &intel_haswell_m_info), /* ULT GT1 mobile */
	INTEL_VGA_DEVICE(0x0A16, &intel_haswell_m_info), /* ULT GT2 mobile */
	INTEL_VGA_DEVICE(0x0A26, &intel_haswell_m_info), /* ULT GT2 mobile */
	INTEL_VGA_DEVICE(0x0D02, &intel_haswell_d_info), /* CRW GT1 desktop */
	INTEL_VGA_DEVICE(0x0D12, &intel_haswell_d_info), /* CRW GT2 desktop */
	INTEL_VGA_DEVICE(0x0D22, &intel_haswell_d_info), /* CRW GT2 desktop */
	INTEL_VGA_DEVICE(0x0D0A, &intel_haswell_d_info), /* CRW GT1 server */
	INTEL_VGA_DEVICE(0x0D1A, &intel_haswell_d_info), /* CRW GT2 server */
	INTEL_VGA_DEVICE(0x0D2A, &intel_haswell_d_info), /* CRW GT2 server */
	INTEL_VGA_DEVICE(0x0D06, &intel_haswell_m_info), /* CRW GT1 mobile */
	INTEL_VGA_DEVICE(0x0D16, &intel_haswell_m_info), /* CRW GT2 mobile */
	INTEL_VGA_DEVICE(0x0D26, &intel_haswell_m_info), /* CRW GT2 mobile */
	INTEL_VGA_DEVICE(0x0f30, &intel_valleyview_m_info),
	INTEL_VGA_DEVICE(0x0157, &intel_valleyview_m_info),
	INTEL_VGA_DEVICE(0x0155, &intel_valleyview_d_info),
	{0, 0}
};

static int i915_enable_unsupported;

static int i915_drm_freeze(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	drm_kms_helper_poll_disable(dev);

#if 0
	pci_save_state(dev->pdev);
#endif

	/* If KMS is active, we do the leavevt stuff here */
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		int error = i915_gem_idle(dev);
		if (error) {
			device_printf(dev->dev,
				"GEM idle failed, resume might fail\n");
			return error;
		}
		drm_irq_uninstall(dev);
	}

	i915_save_state(dev);

	intel_opregion_fini(dev);

	/* Modeset on resume, not lid events */
	dev_priv->modeset_on_lid = 0;

	return 0;
}

static int i915_suspend(device_t kdev)
{
	struct drm_device *dev;
	int error;

	dev = device_get_softc(kdev);
	if (dev == NULL || dev->dev_private == NULL) {
		DRM_ERROR("DRM not initialized, aborting suspend.\n");
		return ENODEV;
	}

	DRM_DEBUG_KMS("starting suspend\n");
	error = i915_drm_freeze(dev);
	if (error)
		return (-error);

	error = bus_generic_suspend(kdev);
	DRM_DEBUG_KMS("finished suspend %d\n", error);
	return (error);
}

static int i915_drm_thaw(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int error = 0;

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		DRM_LOCK(dev);
		i915_gem_restore_gtt_mappings(dev);
		DRM_UNLOCK(dev);
	}

	i915_restore_state(dev);
	intel_opregion_setup(dev);

	/* KMS EnterVT equivalent */
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		if (HAS_PCH_SPLIT(dev))
			ironlake_init_pch_refclk(dev);

		DRM_LOCK(dev);
		dev_priv->mm.suspended = 0;

		error = i915_gem_init_hw(dev);
		DRM_UNLOCK(dev);

		intel_modeset_init_hw(dev);
		sx_xlock(&dev->mode_config.mutex);
		drm_mode_config_reset(dev);
		sx_xunlock(&dev->mode_config.mutex);
		drm_irq_install(dev);

		sx_xlock(&dev->mode_config.mutex);
		/* Resume the modeset for every activated CRTC */
		drm_helper_resume_force_mode(dev);
		sx_xunlock(&dev->mode_config.mutex);
	}

	intel_opregion_init(dev);

	dev_priv->modeset_on_lid = 0;

	return error;
}

static int i915_resume(device_t kdev)
{
	struct drm_device *dev;
	int ret;

	dev = device_get_softc(kdev);
	DRM_DEBUG_KMS("starting resume\n");
#if 0
	if (pci_enable_device(dev->pdev))
		return -EIO;

	pci_set_master(dev->pdev);
#endif

	ret = i915_drm_thaw(dev);
	if (ret != 0)
		return (-ret);

	drm_kms_helper_poll_enable(dev);
	ret = bus_generic_resume(kdev);
	DRM_DEBUG_KMS("finished resume %d\n", ret);
	return (ret);
}

static int
i915_probe(device_t kdev)
{
	const struct intel_device_info *info;
	int error;

	error = drm_probe_helper(kdev, i915_pciidlist);
	if (error != 0)
		return (-error);
	info = i915_get_device_id(pci_get_device(kdev));
	if (info == NULL)
		return (ENXIO);
	return (0);
}

int i915_modeset;

static int
i915_attach(device_t kdev)
{

	if (i915_modeset == 1)
		i915_driver_info.driver_features |= DRIVER_MODESET;
	return (-drm_attach_helper(kdev, i915_pciidlist, &i915_driver_info));
}

static struct fb_info *
i915_fb_helper_getinfo(device_t kdev)
{
	struct intel_fbdev *ifbdev;
	drm_i915_private_t *dev_priv;
	struct drm_device *dev;
	struct fb_info *info;

	dev = device_get_softc(kdev);
	dev_priv = dev->dev_private;
	ifbdev = dev_priv->fbdev;
	if (ifbdev == NULL)
		return (NULL);

	info = ifbdev->helper.fbdev;

	return (info);
}

const struct intel_device_info *
i915_get_device_id(int device)
{
	const struct intel_gfx_device_id *did;

	for (did = &pciidlist[0]; did->device != 0; did++) {
		if (did->device != device)
			continue;
		if (did->info->not_supported && !i915_enable_unsupported)
			return (NULL);
		return (did->info);
	}
	return (NULL);
}

static device_method_t i915_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		i915_probe),
	DEVMETHOD(device_attach,	i915_attach),
	DEVMETHOD(device_suspend,	i915_suspend),
	DEVMETHOD(device_resume,	i915_resume),
	DEVMETHOD(device_detach,	drm_generic_detach),

	/* Framebuffer service methods */
	DEVMETHOD(fb_getinfo,		i915_fb_helper_getinfo),

	DEVMETHOD_END
};

static driver_t i915_driver = {
	"drmn",
	i915_methods,
	sizeof(struct drm_device)
};

extern devclass_t drm_devclass;
DRIVER_MODULE_ORDERED(i915kms, vgapci, i915_driver, drm_devclass, 0, 0,
    SI_ORDER_ANY);
MODULE_DEPEND(i915kms, drmn, 1, 1, 1);
MODULE_DEPEND(i915kms, agp, 1, 1, 1);
MODULE_DEPEND(i915kms, iicbus, 1, 1, 1);
MODULE_DEPEND(i915kms, iic, 1, 1, 1);
MODULE_DEPEND(i915kms, iicbb, 1, 1, 1);

int intel_iommu_enabled = 0;
TUNABLE_INT("drm.i915.intel_iommu_enabled", &intel_iommu_enabled);
int intel_iommu_gfx_mapped = 0;
TUNABLE_INT("drm.i915.intel_iommu_gfx_mapped", &intel_iommu_gfx_mapped);

int i915_prefault_disable;
TUNABLE_INT("drm.i915.prefault_disable", &i915_prefault_disable);
int i915_semaphores = -1;
TUNABLE_INT("drm.i915.semaphores", &i915_semaphores);
static int i915_try_reset = 1;
TUNABLE_INT("drm.i915.try_reset", &i915_try_reset);
unsigned int i915_lvds_downclock = 0;
TUNABLE_INT("drm.i915.lvds_downclock", &i915_lvds_downclock);
int i915_vbt_sdvo_panel_type = -1;
TUNABLE_INT("drm.i915.vbt_sdvo_panel_type", &i915_vbt_sdvo_panel_type);
unsigned int i915_powersave = 1;
TUNABLE_INT("drm.i915.powersave", &i915_powersave);
int i915_enable_fbc = 0;
TUNABLE_INT("drm.i915.enable_fbc", &i915_enable_fbc);
int i915_enable_rc6 = 0;
TUNABLE_INT("drm.i915.enable_rc6", &i915_enable_rc6);
int i915_lvds_channel_mode;
TUNABLE_INT("drm.i915.lvds_channel_mode", &i915_lvds_channel_mode);
int i915_panel_use_ssc = -1;
TUNABLE_INT("drm.i915.panel_use_ssc", &i915_panel_use_ssc);
int i915_panel_ignore_lid = 0;
TUNABLE_INT("drm.i915.panel_ignore_lid", &i915_panel_ignore_lid);
int i915_panel_invert_brightness;
TUNABLE_INT("drm.i915.panel_invert_brightness", &i915_panel_invert_brightness);
int i915_modeset = 1;
TUNABLE_INT("drm.i915.modeset", &i915_modeset);
int i915_enable_ppgtt = -1;
TUNABLE_INT("drm.i915.enable_ppgtt", &i915_enable_ppgtt);
int i915_enable_hangcheck = 1;
TUNABLE_INT("drm.i915.enable_hangcheck", &i915_enable_hangcheck);
TUNABLE_INT("drm.i915.enable_unsupported", &i915_enable_unsupported);

#define	PCI_VENDOR_INTEL		0x8086
#define INTEL_PCH_DEVICE_ID_MASK	0xff00
#define INTEL_PCH_IBX_DEVICE_ID_TYPE	0x3b00
#define INTEL_PCH_CPT_DEVICE_ID_TYPE	0x1c00
#define INTEL_PCH_PPT_DEVICE_ID_TYPE	0x1e00
#define INTEL_PCH_LPT_DEVICE_ID_TYPE	0x8c00

void intel_detect_pch(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv;
	device_t pch;
	uint32_t id;

	dev_priv = dev->dev_private;
	pch = pci_find_class(PCIC_BRIDGE, PCIS_BRIDGE_ISA);
	if (pch != NULL && pci_get_vendor(pch) == PCI_VENDOR_INTEL) {
		id = pci_get_device(pch) & INTEL_PCH_DEVICE_ID_MASK;
		if (id == INTEL_PCH_IBX_DEVICE_ID_TYPE) {
			dev_priv->pch_type = PCH_IBX;
			dev_priv->num_pch_pll = 2;
			DRM_DEBUG_KMS("Found Ibex Peak PCH\n");
		} else if (id == INTEL_PCH_CPT_DEVICE_ID_TYPE) {
			dev_priv->pch_type = PCH_CPT;
			dev_priv->num_pch_pll = 2;
			DRM_DEBUG_KMS("Found CougarPoint PCH\n");
		} else if (id == INTEL_PCH_PPT_DEVICE_ID_TYPE) {
			/* PantherPoint is CPT compatible */
			dev_priv->pch_type = PCH_CPT;
			dev_priv->num_pch_pll = 2;
			DRM_DEBUG_KMS("Found PatherPoint PCH\n");
		} else if (id == INTEL_PCH_LPT_DEVICE_ID_TYPE) {
			dev_priv->pch_type = PCH_LPT;
			dev_priv->num_pch_pll = 0;
			DRM_DEBUG_KMS("Found LynxPoint PCH\n");
		} else
			DRM_DEBUG_KMS("No PCH detected\n");
		KASSERT(dev_priv->num_pch_pll <= I915_NUM_PLLS,
		    ("num_pch_pll %d\n", dev_priv->num_pch_pll));
	} else
		DRM_DEBUG_KMS("No Intel PCI-ISA bridge found\n");
}

bool i915_semaphore_is_enabled(struct drm_device *dev)
{
	if (INTEL_INFO(dev)->gen < 6)
		return 0;

	if (i915_semaphores >= 0)
		return i915_semaphores;

	/* Enable semaphores on SNB when IO remapping is off */
	if (INTEL_INFO(dev)->gen == 6 && intel_iommu_gfx_mapped)
		return false;

	return 1;
}

void
__gen6_gt_force_wake_get(struct drm_i915_private *dev_priv)
{
	int count;

	count = 0;
	while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_ACK) & 1))
		DELAY(10);

	I915_WRITE_NOTRACE(FORCEWAKE, 1);
	POSTING_READ(FORCEWAKE);

	count = 0;
	while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_ACK) & 1) == 0)
		DELAY(10);
}

void
__gen6_gt_force_wake_mt_get(struct drm_i915_private *dev_priv)
{
	int count;

	count = 0;
	while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_MT_ACK) & 1))
		DELAY(10);

	I915_WRITE_NOTRACE(FORCEWAKE_MT, _MASKED_BIT_ENABLE(1));
	POSTING_READ(FORCEWAKE_MT);

	count = 0;
	while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_MT_ACK) & 1) == 0)
		DELAY(10);
}

void
gen6_gt_force_wake_get(struct drm_i915_private *dev_priv)
{

	mtx_lock(&dev_priv->gt_lock);
	if (dev_priv->forcewake_count++ == 0)
		dev_priv->display.force_wake_get(dev_priv);
	mtx_unlock(&dev_priv->gt_lock);
}

static void
gen6_gt_check_fifodbg(struct drm_i915_private *dev_priv)
{
	u32 gtfifodbg;

	gtfifodbg = I915_READ_NOTRACE(GTFIFODBG);
	if ((gtfifodbg & GT_FIFO_CPU_ERROR_MASK) != 0) {
		printf("MMIO read or write has been dropped %x\n", gtfifodbg);
		I915_WRITE_NOTRACE(GTFIFODBG, GT_FIFO_CPU_ERROR_MASK);
	}
}

void
__gen6_gt_force_wake_put(struct drm_i915_private *dev_priv)
{

	I915_WRITE_NOTRACE(FORCEWAKE, 0);
	/* The below doubles as a POSTING_READ */
	gen6_gt_check_fifodbg(dev_priv);
}

void
__gen6_gt_force_wake_mt_put(struct drm_i915_private *dev_priv)
{

	I915_WRITE_NOTRACE(FORCEWAKE_MT, _MASKED_BIT_DISABLE(1));
	/* The below doubles as a POSTING_READ */
	gen6_gt_check_fifodbg(dev_priv);
}

void
gen6_gt_force_wake_put(struct drm_i915_private *dev_priv)
{

	mtx_lock(&dev_priv->gt_lock);
	if (--dev_priv->forcewake_count == 0)
 		dev_priv->display.force_wake_put(dev_priv);
	mtx_unlock(&dev_priv->gt_lock);
}

int
__gen6_gt_wait_for_fifo(struct drm_i915_private *dev_priv)
{
	int ret = 0;

	if (dev_priv->gt_fifo_count < GT_FIFO_NUM_RESERVED_ENTRIES) {
		int loop = 500;
		u32 fifo = I915_READ_NOTRACE(GT_FIFO_FREE_ENTRIES);
		while (fifo <= GT_FIFO_NUM_RESERVED_ENTRIES && loop--) {
			DELAY(10);
			fifo = I915_READ_NOTRACE(GT_FIFO_FREE_ENTRIES);
		}
		if (loop < 0 && fifo <= GT_FIFO_NUM_RESERVED_ENTRIES) {
			printf("%s loop\n", __func__);
			++ret;
		}
		dev_priv->gt_fifo_count = fifo;
	}
	dev_priv->gt_fifo_count--;

	return (ret);
}

void vlv_force_wake_get(struct drm_i915_private *dev_priv)
{
	int count;

	count = 0;

	/* Already awake? */
	if ((I915_READ(0x130094) & 0xa1) == 0xa1)
		return;

	I915_WRITE_NOTRACE(FORCEWAKE_VLV, 0xffffffff);
	POSTING_READ(FORCEWAKE_VLV);

	count = 0;
	while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_ACK_VLV) & 1) == 0)
		DELAY(10);
}

void vlv_force_wake_put(struct drm_i915_private *dev_priv)
{
	I915_WRITE_NOTRACE(FORCEWAKE_VLV, 0xffff0000);
	/* FIXME: confirm VLV behavior with Punit folks */
	POSTING_READ(FORCEWAKE_VLV);
}

static int i8xx_do_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int onems;

	if (IS_I85X(dev))
		return -ENODEV;

	onems = hz / 1000;
	if (onems == 0)
		onems = 1;

	I915_WRITE(D_STATE, I915_READ(D_STATE) | DSTATE_GFX_RESET_I830);
	POSTING_READ(D_STATE);

	if (IS_I830(dev) || IS_845G(dev)) {
		I915_WRITE(DEBUG_RESET_I830,
			   DEBUG_RESET_DISPLAY |
			   DEBUG_RESET_RENDER |
			   DEBUG_RESET_FULL);
		POSTING_READ(DEBUG_RESET_I830);
		pause("i8xxrst1", onems);

		I915_WRITE(DEBUG_RESET_I830, 0);
		POSTING_READ(DEBUG_RESET_I830);
	}

	pause("i8xxrst2", onems);

	I915_WRITE(D_STATE, I915_READ(D_STATE) & ~DSTATE_GFX_RESET_I830);
	POSTING_READ(D_STATE);

	return 0;
}

static int i965_reset_complete(struct drm_device *dev)
{
	u8 gdrst;

	gdrst = pci_read_config(dev->dev, I965_GDRST, 1);
	return (gdrst & GRDOM_RESET_ENABLE) == 0;
}

static int i965_do_reset(struct drm_device *dev)
{
	int ret;
	u8 gdrst;

	/*
	 * Set the domains we want to reset (GRDOM/bits 2 and 3) as
	 * well as the reset bit (GR/bit 0).  Setting the GR bit
	 * triggers the reset; when done, the hardware will clear it.
	 */
	gdrst = pci_read_config(dev->dev, I965_GDRST, 1);
	pci_write_config(dev->dev, I965_GDRST,
	    gdrst | GRDOM_RENDER | GRDOM_RESET_ENABLE, 1);

	ret =  wait_for(i965_reset_complete(dev), 500);
	if (ret)
		return ret;

	/* We can't reset render&media without also resetting display ... */
	gdrst = pci_read_config(dev->dev, I965_GDRST, 1);
	pci_write_config(dev->dev, I965_GDRST,
			 gdrst | GRDOM_MEDIA | GRDOM_RESET_ENABLE, 1);

	return wait_for(i965_reset_complete(dev), 500);
}

static int ironlake_do_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 gdrst;
	int ret;

	gdrst = I915_READ(MCHBAR_MIRROR_BASE + ILK_GDSR);
	I915_WRITE(MCHBAR_MIRROR_BASE + ILK_GDSR,
		   gdrst | GRDOM_RENDER | GRDOM_RESET_ENABLE);
	ret = wait_for(I915_READ(MCHBAR_MIRROR_BASE + ILK_GDSR) & 0x1, 500);
	if (ret)
		return ret;

	/* We can't reset render&media without also resetting display ... */
	gdrst = I915_READ(MCHBAR_MIRROR_BASE + ILK_GDSR);
	I915_WRITE(MCHBAR_MIRROR_BASE + ILK_GDSR,
		   gdrst | GRDOM_MEDIA | GRDOM_RESET_ENABLE);
	return wait_for(I915_READ(MCHBAR_MIRROR_BASE + ILK_GDSR) & 0x1, 500);
}

static int gen6_do_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int	ret;

	/* Hold gt_lock across reset to prevent any register access
	 * with forcewake not set correctly
	 */
	mtx_lock(&dev_priv->gt_lock);

	/* Reset the chip */

	/* GEN6_GDRST is not in the gt power well, no need to check
	 * for fifo space for the write or forcewake the chip for
	 * the read
	 */
	I915_WRITE_NOTRACE(GEN6_GDRST, GEN6_GRDOM_FULL);

	/* Spin waiting for the device to ack the reset request */
	ret = _intel_wait_for(dev,
	    (I915_READ_NOTRACE(GEN6_GDRST) & GEN6_GRDOM_FULL) == 0,
	    500, 0, "915rst");

	/* If reset with a user forcewake, try to restore, otherwise turn it off */
 	if (dev_priv->forcewake_count)
 		dev_priv->display.force_wake_get(dev_priv);
	else
		dev_priv->display.force_wake_put(dev_priv);

	/* Restore fifo count */
	dev_priv->gt_fifo_count = I915_READ_NOTRACE(GT_FIFO_FREE_ENTRIES);

	mtx_unlock(&dev_priv->gt_lock);
	return (ret);
}

int intel_gpu_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = -ENODEV;

	switch (INTEL_INFO(dev)->gen) {
	case 7:
	case 6:
		ret = gen6_do_reset(dev);
		break;
	case 5:
		ret = ironlake_do_reset(dev);
		break;
	case 4:
		ret = i965_do_reset(dev);
		break;
	case 2:
		ret = i8xx_do_reset(dev);
		break;
	}

	/* Also reset the gpu hangman. */
	if (dev_priv->stop_rings) {
		DRM_DEBUG("Simulated gpu hang, resetting stop_rings\n");
		dev_priv->stop_rings = 0;
		if (ret == -ENODEV) {
			DRM_ERROR("Reset not implemented, but ignoring "
				  "error for simulated gpu hangs\n");
			ret = 0;
		}
	}

	return ret;
}

/**
 * i915_reset - reset chip after a hang
 * @dev: drm device to reset
 *
 * Reset the chip.  Useful if a hang is detected. Returns zero on successful
 * reset or otherwise an error code.
 *
 * Procedure is fairly simple:
 *   - reset the chip using the reset reg
 *   - re-init context state
 *   - re-init hardware status page
 *   - re-init ring buffer
 *   - re-init interrupt state
 *   - re-init display
 */
int i915_reset(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	if (!i915_try_reset)
		return 0;

	if (!sx_try_xlock(&dev->dev_struct_lock))
		return (-EBUSY);

	dev_priv->stop_rings = 0;

	i915_gem_reset(dev);

	ret = -ENODEV;
	if (time_second - dev_priv->last_gpu_reset < 5)
		DRM_ERROR("GPU hanging too fast, declaring wedged!\n");
	else
		ret = intel_gpu_reset(dev);

	dev_priv->last_gpu_reset = time_second;
	if (ret) {
		DRM_ERROR("Failed to reset chip.\n");
		DRM_UNLOCK(dev);
		return ret;
	}

	/* Ok, now get things going again... */

	/*
	 * Everything depends on having the GTT running, so we need to start
	 * there.  Fortunately we don't need to do this unless we reset the
	 * chip at a PCI level.
	 *
	 * Next we need to restore the context, but we don't use those
	 * yet either...
	 *
	 * Ring buffer needs to be re-initialized in the KMS case, or if X
	 * was running at the time of the reset (i.e. we weren't VT
	 * switched away).
	 */
	if (drm_core_check_feature(dev, DRIVER_MODESET) ||
			!dev_priv->mm.suspended) {
		struct intel_ring_buffer *ring;
		int i;

		dev_priv->mm.suspended = 0;

		i915_gem_init_swizzling(dev);

		for_each_ring(ring, dev_priv, i)
			ring->init(ring);

		i915_gem_context_init(dev);
		i915_gem_init_ppgtt(dev);

		/*
		 * It would make sense to re-init all the other hw state, at
		 * least the rps/rc6/emon init done within modeset_init_hw. For
		 * some unknown reason, this blows up my ilk, so don't.
		 */
		DRM_UNLOCK(dev);

		if (drm_core_check_feature(dev, DRIVER_MODESET))
			intel_modeset_init_hw(dev);

		drm_irq_uninstall(dev);
		drm_irq_install(dev);
	} else {
		DRM_UNLOCK(dev);
	}

	return 0;
}

/* We give fast paths for the really cool registers */
#define NEEDS_FORCE_WAKE(dev_priv, reg) \
       (((dev_priv)->info->gen >= 6) && \
        ((reg) < 0x40000) &&            \
        ((reg) != FORCEWAKE)) && \
       (!IS_VALLEYVIEW((dev_priv)->dev))

#define __i915_read(x, y) \
u##x i915_read##x(struct drm_i915_private *dev_priv, u32 reg) { \
	u##x val = 0; \
	if (NEEDS_FORCE_WAKE((dev_priv), (reg))) { \
		mtx_lock(&dev_priv->gt_lock); \
		if (dev_priv->forcewake_count == 0) \
			dev_priv->display.force_wake_get(dev_priv); \
		val = DRM_READ##y(dev_priv->mmio_map, reg);	\
		if (dev_priv->forcewake_count == 0) \
			dev_priv->display.force_wake_put(dev_priv); \
		mtx_unlock(&dev_priv->gt_lock); \
	} else { \
		val = DRM_READ##y(dev_priv->mmio_map, reg);	\
	} \
	trace_i915_reg_rw(false, reg, val, sizeof(val)); \
	return val; \
}

__i915_read(8, 8)
__i915_read(16, 16)
__i915_read(32, 32)
__i915_read(64, 64)
#undef __i915_read

#define __i915_write(x, y) \
void i915_write##x(struct drm_i915_private *dev_priv, u32 reg, u##x val) { \
	u32 __fifo_ret = 0; \
	trace_i915_reg_rw(true, reg, val, sizeof(val)); \
	if (NEEDS_FORCE_WAKE((dev_priv), (reg))) { \
		__fifo_ret = __gen6_gt_wait_for_fifo(dev_priv); \
	} \
	DRM_WRITE##y(dev_priv->mmio_map, reg, val); \
	if (__predict_false(__fifo_ret)) { \
		gen6_gt_check_fifodbg(dev_priv); \
	} \
}
__i915_write(8, 8)
__i915_write(16, 16)
__i915_write(32, 32)
__i915_write(64, 64)
#undef __i915_write
