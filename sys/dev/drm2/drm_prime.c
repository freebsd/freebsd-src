/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Kyle Crenshaw <b1nc0d3x@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * drm_prime.c — minimal PRIME / DRI3 implementation for drm2.
 *
 * The 2012 drm2 import shipped PRIME header declarations, ioctl
 * numbers, and a struct drm_driver callback slot, but every body was
 * guarded by #ifdef FREEBSD_NOTYET and never compiled in.  This file
 * fills in the ioctl handlers using FreeBSD's native struct file
 * machinery instead of Linux's struct dma_buf, which keeps the port
 * minimal and avoids dragging the rest of Linux's dma-buf surface in.
 *
 * Each exported PRIME fd wraps a drm_gem_object via a struct file
 * whose f_data points at the gem_obj (refcount held).  Our custom
 * file_ops handle close (drop the ref) and mmap (build a fresh
 * cdev_pager-backed vm_object on top of the gem_obj, same as
 * /dev/dri/card0's mmap path does).
 *
 * This is a single-driver PRIME — exported fds are only meaningful
 * to drm2 drivers that recognise our file ops.  That's enough for
 * DRI3 with the X.org modesetting driver, which is the immediate
 * caller we care about.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>

#include <dev/drm2/drmP.h>

static fo_close_t	drm_prime_fop_close;
static fo_stat_t	drm_prime_fop_stat;
static fo_fill_kinfo_t	drm_prime_fop_fill_kinfo;
static fo_mmap_t	drm_prime_fop_mmap;

static const struct fileops drm_prime_fileops = {
	.fo_read	= invfo_rdwr,
	.fo_write	= invfo_rdwr,
	.fo_truncate	= invfo_truncate,
	.fo_ioctl	= invfo_ioctl,
	.fo_poll	= invfo_poll,
	.fo_kqfilter	= invfo_kqfilter,
	.fo_close	= drm_prime_fop_close,
	.fo_chmod	= invfo_chmod,
	.fo_chown	= invfo_chown,
	.fo_sendfile	= invfo_sendfile,
	.fo_stat	= drm_prime_fop_stat,
	.fo_fill_kinfo	= drm_prime_fop_fill_kinfo,
	.fo_mmap	= drm_prime_fop_mmap,
	.fo_flags	= DFLAG_PASSABLE,
};

static int
drm_prime_fop_close(struct file *fp, struct thread *td)
{
	struct drm_gem_object *obj;

	(void)td;
	obj = fp->f_data;
	fp->f_data = NULL;
	if (obj != NULL)
		drm_gem_object_unreference_unlocked(obj);
	return (0);
}

static int
drm_prime_fop_stat(struct file *fp, struct stat *sb, struct ucred *active_cred)
{
	struct drm_gem_object *obj = fp->f_data;

	(void)active_cred;
	bzero(sb, sizeof(*sb));
	sb->st_mode = S_IFREG | S_IRUSR | S_IWUSR;
	if (obj != NULL)
		sb->st_size = obj->size;
	return (0);
}

static int
drm_prime_fop_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp)
{
	(void)fp; (void)fdp;
	kif->kf_type = KF_TYPE_NONE;
	return (0);
}

/*
 * mmap on a PRIME fd: build a fresh cdev_pager vm_object on top of
 * the gem_obj, the same way drm_gem_mmap_single() does for the
 * primary /dev/dri/card0 fd.
 */
static int
drm_prime_fop_mmap(struct file *fp, vm_map_t map, vm_offset_t *addr,
    vm_size_t size, vm_prot_t prot, vm_prot_t cap_maxprot, int flags,
    vm_ooffset_t foff, struct thread *td)
{
	struct drm_gem_object *obj = fp->f_data;
	struct drm_device *dev;
	struct vm_object *vm_obj;
	int error, mmap_flags;

	(void)foff;
	if (obj == NULL)
		return (EINVAL);
	dev = obj->dev;
	if (dev == NULL || dev->driver == NULL ||
	    dev->driver->gem_pager_ops == NULL)
		return (ENOTSUP);

	drm_gem_object_reference(obj);
	vm_obj = cdev_pager_allocate(obj, OBJT_MGTDEVICE,
	    dev->driver->gem_pager_ops, size, prot, 0, td->td_ucred);
	if (vm_obj == NULL) {
		drm_gem_object_unreference_unlocked(obj);
		return (ENOMEM);
	}

	mmap_flags = flags;
	if ((mmap_flags & (MAP_SHARED | MAP_PRIVATE)) == 0)
		mmap_flags |= MAP_SHARED;

	error = vm_mmap_object(map, addr, size, prot, cap_maxprot,
	    mmap_flags, vm_obj, 0, false, td);
	if (error != 0) {
		vm_object_deallocate(vm_obj);
		drm_gem_object_unreference_unlocked(obj);
	}
	return (error);
}

int
drm_gem_prime_handle_to_fd(struct drm_device *dev,
    struct drm_file *file_priv, uint32_t handle, uint32_t flags,
    int *prime_fd)
{
	struct drm_gem_object *obj;
	struct file *fp;
	struct thread *td = curthread;
	int fd, error;

	(void)flags;
	obj = drm_gem_object_lookup(dev, file_priv, handle);
	if (obj == NULL)
		return (-ENOENT);

	error = falloc_caps(td, &fp, &fd, 0, NULL);
	if (error != 0) {
		drm_gem_object_unreference_unlocked(obj);
		return (-error);
	}

	finit(fp, FREAD | FWRITE, DTYPE_NONE, obj, &drm_prime_fileops);
	*prime_fd = fd;
	fdrop(fp, td);
	return (0);
}
EXPORT_SYMBOL(drm_gem_prime_handle_to_fd);

int
drm_gem_prime_fd_to_handle(struct drm_device *dev,
    struct drm_file *file_priv, int prime_fd, uint32_t *handle)
{
	struct file *fp;
	struct drm_gem_object *obj;
	struct thread *td = curthread;
	int error;

	error = fget(td, prime_fd, &cap_no_rights, &fp);
	if (error != 0)
		return (-error);
	if (fp->f_ops != &drm_prime_fileops) {
		fdrop(fp, td);
		return (-EINVAL);
	}
	obj = fp->f_data;
	if (obj == NULL || obj->dev != dev) {
		fdrop(fp, td);
		return (-EINVAL);
	}

	error = drm_gem_handle_create(file_priv, obj, handle);
	fdrop(fp, td);
	return (error);
}
EXPORT_SYMBOL(drm_gem_prime_fd_to_handle);

int
drm_prime_handle_to_fd_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_prime_handle *args = data;

	return (drm_gem_prime_handle_to_fd(dev, file_priv, args->handle,
	    args->flags, &args->fd));
}

int
drm_prime_fd_to_handle_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_prime_handle *args = data;

	return (drm_gem_prime_fd_to_handle(dev, file_priv, args->fd,
	    &args->handle));
}

void
drm_prime_init_file_private(struct drm_prime_file_private *prime_fpriv)
{
	mtx_init(&prime_fpriv->lock, "drmprime", NULL, MTX_DEF);
	INIT_LIST_HEAD(&prime_fpriv->head);
}

void
drm_prime_destroy_file_private(struct drm_prime_file_private *prime_fpriv)
{
	mtx_destroy(&prime_fpriv->lock);
}
