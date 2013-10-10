
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef __RADEON_KMS_H__
#define	__RADEON_KMS_H__

#include <dev/drm2/drmP.h>

int radeon_driver_load_kms(struct drm_device *dev, unsigned long flags);
int radeon_driver_unload_kms(struct drm_device *dev);

int radeon_driver_firstopen_kms(struct drm_device *dev);
void radeon_driver_lastclose_kms(struct drm_device *dev);
int radeon_driver_open_kms(struct drm_device *dev, struct drm_file *file_priv);
void radeon_driver_postclose_kms(struct drm_device *dev,
				 struct drm_file *file_priv);
void radeon_driver_preclose_kms(struct drm_device *dev,
				struct drm_file *file_priv);
u32 radeon_get_vblank_counter_kms(struct drm_device *dev, int crtc);
int radeon_enable_vblank_kms(struct drm_device *dev, int crtc);
void radeon_disable_vblank_kms(struct drm_device *dev, int crtc);
int radeon_get_vblank_timestamp_kms(struct drm_device *dev, int crtc,
				    int *max_error,
				    struct timeval *vblank_time,
				    unsigned flags);
int radeon_dma_ioctl_kms(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);

#endif /* !defined(__RADEON_KMS_H__) */
