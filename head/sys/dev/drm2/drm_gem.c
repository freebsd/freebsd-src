/*-
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm.h>
#include <dev/drm2/drm_sarea.h>

/*
 * We make up offsets for buffer objects so we can recognize them at
 * mmap time.
 */

/* pgoff in mmap is an unsigned long, so we need to make sure that
 * the faked up offset will fit
 */

#if ULONG_MAX == UINT64_MAX
#define DRM_FILE_PAGE_OFFSET_START ((0xFFFFFFFFUL >> PAGE_SHIFT) + 1)
#define DRM_FILE_PAGE_OFFSET_SIZE ((0xFFFFFFFFUL >> PAGE_SHIFT) * 16)
#else
#define DRM_FILE_PAGE_OFFSET_START ((0xFFFFFFFUL >> PAGE_SHIFT) + 1)
#define DRM_FILE_PAGE_OFFSET_SIZE ((0xFFFFFFFUL >> PAGE_SHIFT) * 16)
#endif

int
drm_gem_init(struct drm_device *dev)
{
	struct drm_gem_mm *mm;

	drm_gem_names_init(&dev->object_names);
	mm = malloc(sizeof(*mm), DRM_MEM_DRIVER, M_WAITOK);
	dev->mm_private = mm;
	if (drm_ht_create(&mm->offset_hash, 19) != 0) {
		free(mm, DRM_MEM_DRIVER);
		return (ENOMEM);
	}
	mm->idxunr = new_unrhdr(0, DRM_GEM_MAX_IDX, NULL);
	return (0);
}

void
drm_gem_destroy(struct drm_device *dev)
{
	struct drm_gem_mm *mm;

	mm = dev->mm_private;
	dev->mm_private = NULL;
	drm_ht_remove(&mm->offset_hash);
	delete_unrhdr(mm->idxunr);
	free(mm, DRM_MEM_DRIVER);
	drm_gem_names_fini(&dev->object_names);
}

int
drm_gem_object_init(struct drm_device *dev, struct drm_gem_object *obj,
    size_t size)
{

	KASSERT((size & (PAGE_SIZE - 1)) == 0,
	    ("Bad size %ju", (uintmax_t)size));

	obj->dev = dev;
	obj->vm_obj = vm_pager_allocate(OBJT_DEFAULT, NULL, size,
	    VM_PROT_READ | VM_PROT_WRITE, 0, curthread->td_ucred);

	obj->refcount = 1;
	obj->handle_count = 0;
	obj->size = size;

	return (0);
}

int
drm_gem_private_object_init(struct drm_device *dev, struct drm_gem_object *obj,
    size_t size)
{

	MPASS((size & (PAGE_SIZE - 1)) == 0);

	obj->dev = dev;
	obj->vm_obj = NULL;

	obj->refcount = 1;
	atomic_set(&obj->handle_count, 0);
	obj->size = size;

	return (0);
}


struct drm_gem_object *
drm_gem_object_alloc(struct drm_device *dev, size_t size)
{
	struct drm_gem_object *obj;

	obj = malloc(sizeof(*obj), DRM_MEM_DRIVER, M_WAITOK | M_ZERO);
	if (drm_gem_object_init(dev, obj, size) != 0)
		goto free;

	if (dev->driver->gem_init_object != NULL &&
	    dev->driver->gem_init_object(obj) != 0)
		goto dealloc;
	return (obj);
dealloc:
	vm_object_deallocate(obj->vm_obj);
free:
	free(obj, DRM_MEM_DRIVER);
	return (NULL);
}

void
drm_gem_object_free(struct drm_gem_object *obj)
{
	struct drm_device *dev;

	dev = obj->dev;
	DRM_LOCK_ASSERT(dev);
	if (dev->driver->gem_free_object != NULL)
		dev->driver->gem_free_object(obj);
}

void
drm_gem_object_reference(struct drm_gem_object *obj)
{

	KASSERT(obj->refcount > 0, ("Dandling obj %p", obj));
	refcount_acquire(&obj->refcount);
}

void
drm_gem_object_unreference(struct drm_gem_object *obj)
{

	if (obj == NULL)
		return;
	if (refcount_release(&obj->refcount))
		drm_gem_object_free(obj);
}

void
drm_gem_object_unreference_unlocked(struct drm_gem_object *obj)
{
	struct drm_device *dev;

	if (obj == NULL)
		return;
	dev = obj->dev;
	DRM_LOCK(dev);
	drm_gem_object_unreference(obj);
	DRM_UNLOCK(dev);
}

void
drm_gem_object_handle_reference(struct drm_gem_object *obj)
{

	drm_gem_object_reference(obj);
	atomic_add_rel_int(&obj->handle_count, 1);
}

void
drm_gem_object_handle_free(struct drm_gem_object *obj)
{
	struct drm_device *dev;
	struct drm_gem_object *obj1;

	dev = obj->dev;
	if (obj->name != 0) {
		obj1 = drm_gem_names_remove(&dev->object_names, obj->name);
		obj->name = 0;
		drm_gem_object_unreference(obj1);
	}
}

void
drm_gem_object_handle_unreference(struct drm_gem_object *obj)
{

	if (obj == NULL ||
	    atomic_load_acq_int(&obj->handle_count) == 0)
		return;

	if (atomic_fetchadd_int(&obj->handle_count, -1) == 1)
		drm_gem_object_handle_free(obj);
	drm_gem_object_unreference(obj);
}

void
drm_gem_object_handle_unreference_unlocked(struct drm_gem_object *obj)
{

	if (obj == NULL ||
	    atomic_load_acq_int(&obj->handle_count) == 0)
		return;

	if (atomic_fetchadd_int(&obj->handle_count, -1) == 1)
		drm_gem_object_handle_free(obj);
	drm_gem_object_unreference_unlocked(obj);
}

int
drm_gem_handle_create(struct drm_file *file_priv, struct drm_gem_object *obj,
    uint32_t *handle)
{
	int error;

	error = drm_gem_name_create(&file_priv->object_names, obj, handle);
	if (error != 0)
		return (error);
	drm_gem_object_handle_reference(obj);
	return (0);
}

int
drm_gem_handle_delete(struct drm_file *file_priv, uint32_t handle)
{
	struct drm_gem_object *obj;

	obj = drm_gem_names_remove(&file_priv->object_names, handle);
	if (obj == NULL)
		return (EINVAL);
	drm_gem_object_handle_unreference_unlocked(obj);
	return (0);
}

void
drm_gem_object_release(struct drm_gem_object *obj)
{

	/*
	 * obj->vm_obj can be NULL for private gem objects.
	 */
	vm_object_deallocate(obj->vm_obj);
}

int
drm_gem_open_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_gem_open *args;
	struct drm_gem_object *obj;
	int ret;
	uint32_t handle;

	if (!drm_core_check_feature(dev, DRIVER_GEM))
		return (ENODEV);
	args = data;

	obj = drm_gem_name_ref(&dev->object_names, args->name,
	    (void (*)(void *))drm_gem_object_reference);
	if (obj == NULL)
		return (ENOENT);
	handle = 0;
	ret = drm_gem_handle_create(file_priv, obj, &handle);
	drm_gem_object_unreference_unlocked(obj);
	if (ret != 0)
		return (ret);
	
	args->handle = handle;
	args->size = obj->size;

	return (0);
}

void
drm_gem_open(struct drm_device *dev, struct drm_file *file_priv)
{

	drm_gem_names_init(&file_priv->object_names);
}

static int
drm_gem_object_release_handle(uint32_t name, void *ptr, void *arg)
{
	struct drm_gem_object *obj;

	obj = ptr;
	drm_gem_object_handle_unreference(obj);
	return (0);
}

void
drm_gem_release(struct drm_device *dev, struct drm_file *file_priv)
{

	drm_gem_names_foreach(&file_priv->object_names,
	    drm_gem_object_release_handle, NULL);
	drm_gem_names_fini(&file_priv->object_names);
}

int
drm_gem_close_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_gem_close *args;

	if (!drm_core_check_feature(dev, DRIVER_GEM))
		return (ENODEV);
	args = data;

	return (drm_gem_handle_delete(file_priv, args->handle));
}

int
drm_gem_flink_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_gem_flink *args;
	struct drm_gem_object *obj;
	int error;

	if (!drm_core_check_feature(dev, DRIVER_GEM))
		return (ENODEV);
	args = data;

	obj = drm_gem_name_ref(&file_priv->object_names, args->handle,
	    (void (*)(void *))drm_gem_object_reference);
	if (obj == NULL)
		return (ENOENT);
	error = drm_gem_name_create(&dev->object_names, obj, &obj->name);
	if (error != 0) {
		if (error == EALREADY)
			error = 0;
		drm_gem_object_unreference_unlocked(obj);
	}
	if (error == 0)
		args->name = obj->name;
	return (error);
}

struct drm_gem_object *
drm_gem_object_lookup(struct drm_device *dev, struct drm_file *file_priv,
    uint32_t handle)
{
	struct drm_gem_object *obj;

	obj = drm_gem_name_ref(&file_priv->object_names, handle,
	    (void (*)(void *))drm_gem_object_reference);
	return (obj);
}

static struct drm_gem_object *
drm_gem_object_from_offset(struct drm_device *dev, vm_ooffset_t offset)
{
	struct drm_gem_object *obj;
	struct drm_gem_mm *mm;
	struct drm_hash_item *map_list;

	if ((offset & DRM_GEM_MAPPING_MASK) != DRM_GEM_MAPPING_KEY)
		return (NULL);
	offset &= ~DRM_GEM_MAPPING_KEY;
	mm = dev->mm_private;
	if (drm_ht_find_item(&mm->offset_hash, DRM_GEM_MAPPING_IDX(offset),
	    &map_list) != 0) {
	DRM_DEBUG("drm_gem_object_from_offset: offset 0x%jx obj not found\n",
		    (uintmax_t)offset);
		return (NULL);
	}
	obj = __containerof(map_list, struct drm_gem_object, map_list);
	return (obj);
}

int
drm_gem_create_mmap_offset(struct drm_gem_object *obj)
{
	struct drm_device *dev;
	struct drm_gem_mm *mm;
	int ret;

	if (obj->on_map)
		return (0);
	dev = obj->dev;
	mm = dev->mm_private;
	ret = 0;

	obj->map_list.key = alloc_unr(mm->idxunr);
	ret = drm_ht_insert_item(&mm->offset_hash, &obj->map_list);
	if (ret != 0) {
		DRM_ERROR("failed to add to map hash\n");
		free_unr(mm->idxunr, obj->map_list.key);
		return (ret);
	}
	obj->on_map = true;
	return (0);
}

void
drm_gem_free_mmap_offset(struct drm_gem_object *obj)
{
	struct drm_hash_item *list;
	struct drm_gem_mm *mm;

	if (!obj->on_map)
		return;
	mm = obj->dev->mm_private;
	list = &obj->map_list;

	drm_ht_remove_item(&mm->offset_hash, list);
	free_unr(mm->idxunr, list->key);
	obj->on_map = false;
}

int
drm_gem_mmap_single(struct drm_device *dev, vm_ooffset_t *offset, vm_size_t size,
    struct vm_object **obj_res, int nprot)
{
	struct drm_gem_object *gem_obj;
	struct vm_object *vm_obj;

	DRM_LOCK(dev);
	gem_obj = drm_gem_object_from_offset(dev, *offset);
	if (gem_obj == NULL) {
		DRM_UNLOCK(dev);
		return (ENODEV);
	}
	drm_gem_object_reference(gem_obj);
	DRM_UNLOCK(dev);
	vm_obj = cdev_pager_allocate(gem_obj, OBJT_MGTDEVICE,
	    dev->driver->gem_pager_ops, size, nprot,
	    DRM_GEM_MAPPING_MAPOFF(*offset), curthread->td_ucred);
	if (vm_obj == NULL) {
		drm_gem_object_unreference_unlocked(gem_obj);
		return (EINVAL);
	}
	*offset = DRM_GEM_MAPPING_MAPOFF(*offset);
	*obj_res = vm_obj;
	return (0);
}

void
drm_gem_pager_dtr(void *handle)
{
	struct drm_gem_object *obj;
	struct drm_device *dev;

	obj = handle;
	dev = obj->dev;

	DRM_LOCK(dev);
	drm_gem_free_mmap_offset(obj);
	drm_gem_object_unreference(obj);
	DRM_UNLOCK(dev);
}
