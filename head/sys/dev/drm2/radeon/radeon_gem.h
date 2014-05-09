
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef __RADEON_GEM_H__
#define	__RADEON_GEM_H__

#include <dev/drm2/drmP.h>

int radeon_gem_object_init(struct drm_gem_object *obj);
void radeon_gem_object_free(struct drm_gem_object *obj);
int radeon_gem_object_open(struct drm_gem_object *obj,
				struct drm_file *file_priv);
void radeon_gem_object_close(struct drm_gem_object *obj,
				struct drm_file *file_priv);

#endif /* !defined(__RADEON_GEM_H__) */
