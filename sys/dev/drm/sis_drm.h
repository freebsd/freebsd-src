/*
 * $FreeBSD$
 */

#ifndef _sis_drm_public_h_
#define _sis_drm_public_h_

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
