/*
 * $FreeBSD$
 */

#ifndef _sis_drm_public_h_
#define _sis_drm_public_h_

/* SiS specific ioctls */
#define SIS_IOCTL_FB_ALLOC		DRM_IOWR(0x44, drm_sis_mem_t)
#define SIS_IOCTL_FB_FREE		DRM_IOW( 0x45, drm_sis_mem_t)
#define SIS_IOCTL_AGP_INIT		DRM_IOWR(0x53, drm_sis_agp_t)
#define SIS_IOCTL_AGP_ALLOC		DRM_IOWR(0x54, drm_sis_mem_t)
#define SIS_IOCTL_AGP_FREE		DRM_IOW( 0x55, drm_sis_mem_t)
#define SIS_IOCTL_FLIP			DRM_IOW( 0x48, drm_sis_flip_t)
#define SIS_IOCTL_FLIP_INIT		DRM_IO(  0x49)
#define SIS_IOCTL_FLIP_FINAL		DRM_IO(  0x50)

typedef struct {
  int context;
  unsigned int offset;
  unsigned int size;
  unsigned int free;
} drm_sis_mem_t;

typedef struct {
  unsigned int offset, size;
} drm_sis_agp_t;

typedef struct {
  unsigned int left, right;
} drm_sis_flip_t;

#if defined(__KERNEL__) || defined(_KERNEL)

int sis_fb_alloc(DRM_OS_IOCTL);
int sis_fb_free(DRM_OS_IOCTL);
int sisp_agp_init(DRM_OS_IOCTL);
int sisp_agp_alloc(DRM_OS_IOCTL);
int sisp_agp_free(DRM_OS_IOCTL);

#endif

#endif
