/*-
 * SPDX-License-Identifier: Apache License 2.0
 *
 * Copyright 2017-2018 Yutaro Hayakawa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dev/ebpf/ebpf_dev_platform.h>
#include <dev/ebpf/ebpf_obj.h>
#include <sys/ebpf.h>
#include <dev/ebpf/ebpf_prog.h>
#include <dev/ebpf/ebpf_dev.h>

/*
 * Global reference count
 *
 * This will be acquired when users get file descriptor like /dev/ebpf
 * descriptor or ebpf object (programs, maps...) descriptor. It will
 * released when users close them.
 */
static uint32_t ebpf_dev_global_refcount = 0;

/*
 * Extend badfileops for anonimous file for ebpf objects.
 */
static struct fileops ebpf_objf_ops;
static int
ebpf_objfile_close(struct file *fp, struct thread *td)
{
	struct ebpf_obj *eo = fp->f_data;

	if (fp->f_count == 0) {
		ebpf_obj_release(eo);
		ebpf_refcount_release(&ebpf_dev_global_refcount);
	}

	return 0;
}

bool
is_ebpf_objfile(ebpf_file *fp)
{
	if (fp == NULL) {
		return false;
	}
	return fp->f_ops == &ebpf_objf_ops;
}

int
ebpf_fopen(ebpf_thread *td, ebpf_file **fp, int *fd, struct ebpf_obj *data)
{
	int error;

	if (td == NULL || fp == NULL || fd == NULL || data == NULL) {
		return EINVAL;
	}

	error = falloc(td, fp, fd, 0);
	if (error != 0) {
		return error;
	}

	/*
	 * finit reserves two reference count for us, so release one
	 * since we don't need it.
	 */
	finit(*fp, FREAD | FWRITE, DTYPE_NONE, data, &ebpf_objf_ops);
	fdrop(*fp, td);

	ebpf_refcount_acquire(&ebpf_dev_global_refcount);

	return 0;
}

int
ebpf_fd_to_program(ebpf_thread *td, int fd, ebpf_file **fp_out, struct ebpf_prog **prog_out)
{
	int error;
	ebpf_file *fp;
	struct ebpf_prog *prog;
	struct ebpf_obj *obj;

	error = ebpf_fget(td, fd, &fp);
	if (error != 0) {
		return (error);
	}

	if (!is_ebpf_objfile(fp)) {
		error = EINVAL;
		goto out;
	}

	obj = ebpf_file_get_data(fp);
	prog = EO2EP(obj);
	if (prog == NULL) {
		error = EINVAL;
		goto out;
	}

	*fp_out = fp;
	if (prog_out) {
		*prog_out = prog;
	}

	return (0);

out:
	if (fp != NULL) {
		ebpf_fdrop(fp, td);
	}

	return (error);
}

struct ebpf_obj *
ebpf_file_get_data(ebpf_file *f)
{
	return f->f_data;
}

int
ebpf_fget(ebpf_thread *td, int fd, ebpf_file **f)
{
#if __FreeBSD_version >= 1200062
	return fget(td, fd, &cap_ioctl_rights, f);
#else
	cap_rights_t cap;
	return fget(td, fd, cap_rights_init(&cap, CAP_IOCTL), f);
#endif
}

int
ebpf_fdrop(ebpf_file *f, ebpf_thread *td)
{
	return fdrop(f, td);
}

int
ebpf_copyin(const void *uaddr, void *kaddr, size_t len)
{
	return copyin(uaddr, kaddr, len);
}

int
ebpf_copyout(const void *kaddr, void *uaddr, size_t len)
{
	return copyout(kaddr, uaddr, len);
}

ebpf_thread *
ebpf_curthread(void)
{
	return curthread;
}

/*
 * Character device operations
 */
static int
ebpf_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	ebpf_refcount_acquire(&ebpf_dev_global_refcount);
	return 0;
}

static int
ebpf_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	ebpf_refcount_release(&ebpf_dev_global_refcount);
	return 0;
}

static int
freebsd_ebpf_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int ffla,
		   struct thread *td)
{
	int error;
	struct ebpf_env *ee;

	ee = dev->si_drv1;
	error = ebpf_ioctl(ee, cmd, data, td);
	return error;
}

static struct cdev *ebpf_dev;
static struct cdevsw ebpf_cdevsw = {.d_version = D_VERSION,
				    .d_name = "ebpf",
				    .d_open = ebpf_open,
				    .d_ioctl = freebsd_ebpf_ioctl,
				    .d_close = ebpf_close};

/*
 * Kernel module operations
 */

int
ebpf_dev_fini(void)
{
	struct ebpf_env *ee;

	if (ebpf_dev_global_refcount > 0)
		return (EBUSY);

	if (ebpf_dev != NULL) {
		ee = ebpf_dev->si_drv1;

		ebpf_env_destroy(ee);
		destroy_dev(ebpf_dev);
	}

	return (0);
}

int
ebpf_dev_init(void)
{
	struct ebpf_env *ee;
	int error;

	ee = NULL;

	/*
	 * File operation definition for ebpf object file.
	 * It simply check reference count on file close
	 * and execute destractor of the ebpf object if
	 * the reference count was 0. It doesn't allow to
	 * perform any file operations except close(2)
	 */
	memcpy(&ebpf_objf_ops, &badfileops, sizeof(struct fileops));
	ebpf_objf_ops.fo_close = ebpf_objfile_close;

	error = ebpf_env_create(&ee, &fbsd_ebpf_config);
	if (error != 0) {
		goto fail;
	}

	ebpf_dev = make_dev_credf(MAKEDEV_ETERNAL_KLD, &ebpf_cdevsw, 0, NULL,
				  UID_ROOT, GID_WHEEL, 0600, "ebpf");
	if (ebpf_dev == NULL) {
		goto fail;
	}

	ebpf_dev->si_drv1 = ee;

	return 0;
fail:
	if (ee != NULL) {
		ebpf_env_release(ee);
	}

	ebpf_dev_fini();
	return EINVAL;
}
