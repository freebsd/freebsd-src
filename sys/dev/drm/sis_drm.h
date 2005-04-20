/*
 * $FreeBSD$
 */

#ifndef __SIS_DRM_H__
#define __SIS_DRM_H__

/* SiS specific ioctls */
#define DRM_IOCTL_SIS_FB_ALLOC		DRM_IOWR(0x44, drm_sis_mem_t)
#define DRM_IOCTL_SIS_FB_FREE		DRM_IOW( 0x45, drm_sis_mem_t)
#define DRM_IOCTL_SIS_AGP_INIT		DRM_IOWR(0x53, drm_sis_agp_t)
#define DRM_IOCTL_SIS_AGP_ALLOC		DRM_IOWR(0x54, drm_sis_mem_t)
#define DRM_IOCTL_SIS_AGP_FREE		DRM_IOW( 0x55, drm_sis_mem_t)
#define DRM_IOCTL_SIS_FB_INIT		DRM_IOW( 0x56, drm_sis_fb_t)
/*
#define DRM_IOCTL_SIS_FLIP		DRM_IOW( 0x48, drm_sis_flip_t)
#define DRM_IOCTL_SIS_FLIP_INIT		DRM_IO(  0x49)
#define DRM_IOCTL_SIS_FLIP_FINAL	DRM_IO(  0x50)
*/

typedef struct {
	int context;
	unsigned int offset;
	unsigned int size;
	unsigned long free;
} drm_sis_mem_t;

typedef struct {
	unsigned int offset, size;
} drm_sis_agp_t;

typedef struct {
	unsigned int offset, size;
} drm_sis_fb_t;

#endif /* __SIS_DRM_H__ */
