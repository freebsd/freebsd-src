/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/sglist.h>
#include <sys/sleepqueue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/stdarg.h>

#if defined(__i386__) || defined(__amd64__)
#include <machine/md_var.h>
#endif

#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/sysfs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/rcupdate.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/compat.h>

#include <vm/vm_pager.h>

SYSCTL_NODE(_compat, OID_AUTO, linuxkpi, CTLFLAG_RW, 0, "LinuxKPI parameters");

MALLOC_DEFINE(M_KMALLOC, "linux", "Linux kmalloc compat");

#include <linux/rbtree.h>
/* Undo Linux compat changes. */
#undef RB_ROOT
#undef file
#undef cdev
#define	RB_ROOT(head)	(head)->rbh_root

struct kobject linux_class_root;
struct device linux_root_device;
struct class linux_class_misc;
struct list_head pci_drivers;
struct list_head pci_devices;
struct net init_net;
spinlock_t pci_lock;
struct sx linux_global_rcu_lock;

unsigned long linux_timer_hz_mask;

int
panic_cmp(struct rb_node *one, struct rb_node *two)
{
	panic("no cmp");
}

RB_GENERATE(linux_root, rb_node, __entry, panic_cmp);

int
kobject_set_name_vargs(struct kobject *kobj, const char *fmt, va_list args)
{
	va_list tmp_va;
	int len;
	char *old;
	char *name;
	char dummy;

	old = kobj->name;

	if (old && fmt == NULL)
		return (0);

	/* compute length of string */
	va_copy(tmp_va, args);
	len = vsnprintf(&dummy, 0, fmt, tmp_va);
	va_end(tmp_va);

	/* account for zero termination */
	len++;

	/* check for error */
	if (len < 1)
		return (-EINVAL);

	/* allocate memory for string */
	name = kzalloc(len, GFP_KERNEL);
	if (name == NULL)
		return (-ENOMEM);
	vsnprintf(name, len, fmt, args);
	kobj->name = name;

	/* free old string */
	kfree(old);

	/* filter new string */
	for (; *name != '\0'; name++)
		if (*name == '/')
			*name = '!';
	return (0);
}

int
kobject_set_name(struct kobject *kobj, const char *fmt, ...)
{
	va_list args;
	int error;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);

	return (error);
}

static int
kobject_add_complete(struct kobject *kobj, struct kobject *parent)
{
	const struct kobj_type *t;
	int error;

	kobj->parent = parent;
	error = sysfs_create_dir(kobj);
	if (error == 0 && kobj->ktype && kobj->ktype->default_attrs) {
		struct attribute **attr;
		t = kobj->ktype;

		for (attr = t->default_attrs; *attr != NULL; attr++) {
			error = sysfs_create_file(kobj, *attr);
			if (error)
				break;
		}
		if (error)
			sysfs_remove_dir(kobj);
		
	}
	return (error);
}

int
kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt, ...)
{
	va_list args;
	int error;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);
	if (error)
		return (error);

	return kobject_add_complete(kobj, parent);
}

void
linux_kobject_release(struct kref *kref)
{
	struct kobject *kobj;
	char *name;

	kobj = container_of(kref, struct kobject, kref);
	sysfs_remove_dir(kobj);
	name = kobj->name;
	if (kobj->ktype && kobj->ktype->release)
		kobj->ktype->release(kobj);
	kfree(name);
}

static void
linux_kobject_kfree(struct kobject *kobj)
{
	kfree(kobj);
}

static void
linux_kobject_kfree_name(struct kobject *kobj)
{
	if (kobj) {
		kfree(kobj->name);
	}
}

const struct kobj_type linux_kfree_type = {
	.release = linux_kobject_kfree
};

static void
linux_device_release(struct device *dev)
{
	pr_debug("linux_device_release: %s\n", dev_name(dev));
	kfree(dev);
}

static ssize_t
linux_class_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct class_attribute *dattr;
	ssize_t error;

	dattr = container_of(attr, struct class_attribute, attr);
	error = -EIO;
	if (dattr->show)
		error = dattr->show(container_of(kobj, struct class, kobj),
		    dattr, buf);
	return (error);
}

static ssize_t
linux_class_store(struct kobject *kobj, struct attribute *attr, const char *buf,
    size_t count)
{
	struct class_attribute *dattr;
	ssize_t error;

	dattr = container_of(attr, struct class_attribute, attr);
	error = -EIO;
	if (dattr->store)
		error = dattr->store(container_of(kobj, struct class, kobj),
		    dattr, buf, count);
	return (error);
}

static void
linux_class_release(struct kobject *kobj)
{
	struct class *class;

	class = container_of(kobj, struct class, kobj);
	if (class->class_release)
		class->class_release(class);
}

static const struct sysfs_ops linux_class_sysfs = {
	.show  = linux_class_show,
	.store = linux_class_store,
};

const struct kobj_type linux_class_ktype = {
	.release = linux_class_release,
	.sysfs_ops = &linux_class_sysfs
};

static void
linux_dev_release(struct kobject *kobj)
{
	struct device *dev;

	dev = container_of(kobj, struct device, kobj);
	/* This is the precedence defined by linux. */
	if (dev->release)
		dev->release(dev);
	else if (dev->class && dev->class->dev_release)
		dev->class->dev_release(dev);
}

static ssize_t
linux_dev_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct device_attribute *dattr;
	ssize_t error;

	dattr = container_of(attr, struct device_attribute, attr);
	error = -EIO;
	if (dattr->show)
		error = dattr->show(container_of(kobj, struct device, kobj),
		    dattr, buf);
	return (error);
}

static ssize_t
linux_dev_store(struct kobject *kobj, struct attribute *attr, const char *buf,
    size_t count)
{
	struct device_attribute *dattr;
	ssize_t error;

	dattr = container_of(attr, struct device_attribute, attr);
	error = -EIO;
	if (dattr->store)
		error = dattr->store(container_of(kobj, struct device, kobj),
		    dattr, buf, count);
	return (error);
}

static const struct sysfs_ops linux_dev_sysfs = {
	.show  = linux_dev_show,
	.store = linux_dev_store,
};

const struct kobj_type linux_dev_ktype = {
	.release = linux_dev_release,
	.sysfs_ops = &linux_dev_sysfs
};

struct device *
device_create(struct class *class, struct device *parent, dev_t devt,
    void *drvdata, const char *fmt, ...)
{
	struct device *dev;
	va_list args;

	dev = kzalloc(sizeof(*dev), M_WAITOK);
	dev->parent = parent;
	dev->class = class;
	dev->devt = devt;
	dev->driver_data = drvdata;
	dev->release = linux_device_release;
	va_start(args, fmt);
	kobject_set_name_vargs(&dev->kobj, fmt, args);
	va_end(args);
	device_register(dev);

	return (dev);
}

int
kobject_init_and_add(struct kobject *kobj, const struct kobj_type *ktype,
    struct kobject *parent, const char *fmt, ...)
{
	va_list args;
	int error;

	kobject_init(kobj, ktype);
	kobj->ktype = ktype;
	kobj->parent = parent;
	kobj->name = NULL;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);
	if (error)
		return (error);
	return kobject_add_complete(kobj, parent);
}

void
linux_set_current(struct thread *td, struct task_struct *t)
{
	memset(t, 0, sizeof(*t));
	task_struct_fill(td, t);
	task_struct_set(td, t);
}

void
linux_clear_current(struct thread *td)
{
	task_struct_set(td, NULL);
}

static void
linux_file_dtor(void *cdp)
{
	struct linux_file *filp;
	struct task_struct t;
	struct thread *td;

	td = curthread;
	filp = cdp;
	linux_set_current(td, &t);
	filp->f_op->release(filp->f_vnode, filp);
	linux_clear_current(td);
	vdrop(filp->f_vnode);
	kfree(filp);
}

static int
linux_dev_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct task_struct t;
	struct file *file;
	int error;

	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (ENODEV);
	filp = kzalloc(sizeof(*filp), GFP_KERNEL);
	filp->f_dentry = &filp->f_dentry_store;
	filp->f_op = ldev->ops;
	filp->f_flags = file->f_flag;
	vhold(file->f_vnode);
	filp->f_vnode = file->f_vnode;
	linux_set_current(td, &t);
	if (filp->f_op->open) {
		error = -filp->f_op->open(file->f_vnode, filp);
		if (error) {
			kfree(filp);
			goto done;
		}
	}
	error = devfs_set_cdevpriv(filp, linux_file_dtor);
	if (error) {
		filp->f_op->release(file->f_vnode, filp);
		kfree(filp);
	}
done:
	linux_clear_current(td);
	return (error);
}

static int
linux_dev_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct file *file;
	int error;

	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
        devfs_clear_cdevpriv();
        

	return (0);
}

#define	LINUX_IOCTL_MIN_PTR 0x10000UL
#define	LINUX_IOCTL_MAX_PTR (LINUX_IOCTL_MIN_PTR + IOCPARM_MAX)

static inline int
linux_remap_address(void **uaddr, size_t len)
{
	uintptr_t uaddr_val = (uintptr_t)(*uaddr);

	if (unlikely(uaddr_val >= LINUX_IOCTL_MIN_PTR &&
	    uaddr_val < LINUX_IOCTL_MAX_PTR)) {
		struct task_struct *pts = current;
		if (pts == NULL) {
			*uaddr = NULL;
			return (1);
		}

		/* compute data offset */
		uaddr_val -= LINUX_IOCTL_MIN_PTR;

		/* check that length is within bounds */
		if ((len > IOCPARM_MAX) ||
		    (uaddr_val + len) > pts->bsd_ioctl_len) {
			*uaddr = NULL;
			return (1);
		}

		/* re-add kernel buffer address */
		uaddr_val += (uintptr_t)pts->bsd_ioctl_data;

		/* update address location */
		*uaddr = (void *)uaddr_val;
		return (1);
	}
	return (0);
}

int
linux_copyin(const void *uaddr, void *kaddr, size_t len)
{
	if (linux_remap_address(__DECONST(void **, &uaddr), len)) {
		if (uaddr == NULL)
			return (-EFAULT);
		memcpy(kaddr, uaddr, len);
		return (0);
	}
	return (-copyin(uaddr, kaddr, len));
}

int
linux_copyout(const void *kaddr, void *uaddr, size_t len)
{
	if (linux_remap_address(&uaddr, len)) {
		if (uaddr == NULL)
			return (-EFAULT);
		memcpy(uaddr, kaddr, len);
		return (0);
	}
	return (-copyout(kaddr, uaddr, len));
}

static int
linux_dev_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct task_struct t;
	struct file *file;
	unsigned size;
	int error;

	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	linux_set_current(td, &t);
	size = IOCPARM_LEN(cmd);
	/* refer to logic in sys_ioctl() */
	if (size > 0) {
		/*
		 * Setup hint for linux_copyin() and linux_copyout().
		 *
		 * Background: Linux code expects a user-space address
		 * while FreeBSD supplies a kernel-space address.
		 */
		t.bsd_ioctl_data = data;
		t.bsd_ioctl_len = size;
		data = (void *)LINUX_IOCTL_MIN_PTR;
	} else {
		/* fetch user-space pointer */
		data = *(void **)data;
	}
	if (filp->f_op->unlocked_ioctl)
		error = -filp->f_op->unlocked_ioctl(filp, cmd, (u_long)data);
	else
		error = ENOTTY;
	linux_clear_current(td);

	return (error);
}

static int
linux_dev_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct task_struct t;
	struct thread *td;
	struct file *file;
	ssize_t bytes;
	int error;

	td = curthread;
	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	/* XXX no support for I/O vectors currently */
	if (uio->uio_iovcnt != 1)
		return (EOPNOTSUPP);
	linux_set_current(td, &t);
	if (filp->f_op->read) {
		bytes = filp->f_op->read(filp, uio->uio_iov->iov_base,
		    uio->uio_iov->iov_len, &uio->uio_offset);
		if (bytes >= 0) {
			uio->uio_iov->iov_base =
			    ((uint8_t *)uio->uio_iov->iov_base) + bytes;
			uio->uio_iov->iov_len -= bytes;
			uio->uio_resid -= bytes;
		} else
			error = -bytes;
	} else
		error = ENXIO;
	linux_clear_current(td);

	return (error);
}

static int
linux_dev_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct task_struct t;
	struct thread *td;
	struct file *file;
	ssize_t bytes;
	int error;

	td = curthread;
	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	/* XXX no support for I/O vectors currently */
	if (uio->uio_iovcnt != 1)
		return (EOPNOTSUPP);
	linux_set_current(td, &t);
	if (filp->f_op->write) {
		bytes = filp->f_op->write(filp, uio->uio_iov->iov_base,
		    uio->uio_iov->iov_len, &uio->uio_offset);
		if (bytes >= 0) {
			uio->uio_iov->iov_base =
			    ((uint8_t *)uio->uio_iov->iov_base) + bytes;
			uio->uio_iov->iov_len -= bytes;
			uio->uio_resid -= bytes;
		} else
			error = -bytes;
	} else
		error = ENXIO;
	linux_clear_current(td);

	return (error);
}

static int
linux_dev_poll(struct cdev *dev, int events, struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct task_struct t;
	struct file *file;
	int revents;
	int error;

	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	linux_set_current(td, &t);
	if (filp->f_op->poll)
		revents = filp->f_op->poll(filp, NULL) & events;
	else
		revents = 0;
	linux_clear_current(td);

	return (revents);
}

static int
linux_dev_mmap_single(struct cdev *dev, vm_ooffset_t *offset,
    vm_size_t size, struct vm_object **object, int nprot)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct thread *td;
	struct task_struct t;
	struct file *file;
	struct vm_area_struct vma;
	int error;

	td = curthread;
	file = td->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (ENODEV);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	linux_set_current(td, &t);
	vma.vm_start = 0;
	vma.vm_end = size;
	vma.vm_pgoff = *offset / PAGE_SIZE;
	vma.vm_pfn = 0;
	vma.vm_page_prot = VM_MEMATTR_DEFAULT;
	if (filp->f_op->mmap) {
		error = -filp->f_op->mmap(filp, &vma);
		if (error == 0) {
			struct sglist *sg;

			sg = sglist_alloc(1, M_WAITOK);
			sglist_append_phys(sg,
			    (vm_paddr_t)vma.vm_pfn << PAGE_SHIFT, vma.vm_len);
			*object = vm_pager_allocate(OBJT_SG, sg, vma.vm_len,
			    nprot, 0, td->td_ucred);
		        if (*object == NULL) {
				sglist_free(sg);
				error = EINVAL;
				goto done;
			}
			*offset = 0;
			if (vma.vm_page_prot != VM_MEMATTR_DEFAULT) {
				VM_OBJECT_WLOCK(*object);
				vm_object_set_memattr(*object,
				    vma.vm_page_prot);
				VM_OBJECT_WUNLOCK(*object);
			}
		}
	} else
		error = ENODEV;
done:
	linux_clear_current(td);
	return (error);
}

struct cdevsw linuxcdevsw = {
	.d_version = D_VERSION,
	.d_flags = D_TRACKCLOSE,
	.d_open = linux_dev_open,
	.d_close = linux_dev_close,
	.d_read = linux_dev_read,
	.d_write = linux_dev_write,
	.d_ioctl = linux_dev_ioctl,
	.d_mmap_single = linux_dev_mmap_single,
	.d_poll = linux_dev_poll,
};

static int
linux_file_read(struct file *file, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct linux_file *filp;
	struct task_struct t;
	ssize_t bytes;
	int error;

	error = 0;
	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	/* XXX no support for I/O vectors currently */
	if (uio->uio_iovcnt != 1)
		return (EOPNOTSUPP);
	linux_set_current(td, &t);
	if (filp->f_op->read) {
		bytes = filp->f_op->read(filp, uio->uio_iov->iov_base,
		    uio->uio_iov->iov_len, &uio->uio_offset);
		if (bytes >= 0) {
			uio->uio_iov->iov_base =
			    ((uint8_t *)uio->uio_iov->iov_base) + bytes;
			uio->uio_iov->iov_len -= bytes;
			uio->uio_resid -= bytes;
		} else
			error = -bytes;
	} else
		error = ENXIO;
	linux_clear_current(td);

	return (error);
}

static int
linux_file_poll(struct file *file, int events, struct ucred *active_cred,
    struct thread *td)
{
	struct linux_file *filp;
	struct task_struct t;
	int revents;

	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	linux_set_current(td, &t);
	if (filp->f_op->poll)
		revents = filp->f_op->poll(filp, NULL) & events;
	else
		revents = 0;
	linux_clear_current(td);

	return (revents);
}

static int
linux_file_close(struct file *file, struct thread *td)
{
	struct linux_file *filp;
	struct task_struct t;
	int error;

	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	linux_set_current(td, &t);
	error = -filp->f_op->release(NULL, filp);
	linux_clear_current(td);
	funsetown(&filp->f_sigio);
	kfree(filp);

	return (error);
}

static int
linux_file_ioctl(struct file *fp, u_long cmd, void *data, struct ucred *cred,
    struct thread *td)
{
	struct linux_file *filp;
	struct task_struct t;
	int error;

	filp = (struct linux_file *)fp->f_data;
	filp->f_flags = fp->f_flag;
	error = 0;

	linux_set_current(td, &t);
	switch (cmd) {
	case FIONBIO:
		break;
	case FIOASYNC:
		if (filp->f_op->fasync == NULL)
			break;
		error = filp->f_op->fasync(0, filp, fp->f_flag & FASYNC);
		break;
	case FIOSETOWN:
		error = fsetown(*(int *)data, &filp->f_sigio);
		if (error == 0)
			error = filp->f_op->fasync(0, filp,
			    fp->f_flag & FASYNC);
		break;
	case FIOGETOWN:
		*(int *)data = fgetown(&filp->f_sigio);
		break;
	default:
		error = ENOTTY;
		break;
	}
	linux_clear_current(td);
	return (error);
}

static int
linux_file_stat(struct file *fp, struct stat *sb, struct ucred *active_cred,
    struct thread *td)
{

	return (EOPNOTSUPP);
}

static int
linux_file_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp)
{

	return (0);
}

struct fileops linuxfileops = {
	.fo_read = linux_file_read,
	.fo_write = invfo_rdwr,
	.fo_truncate = invfo_truncate,
	.fo_kqfilter = invfo_kqfilter,
	.fo_stat = linux_file_stat,
	.fo_fill_kinfo = linux_file_fill_kinfo,
	.fo_poll = linux_file_poll,
	.fo_close = linux_file_close,
	.fo_ioctl = linux_file_ioctl,
	.fo_chmod = invfo_chmod,
	.fo_chown = invfo_chown,
	.fo_sendfile = invfo_sendfile,
};

/*
 * Hash of vmmap addresses.  This is infrequently accessed and does not
 * need to be particularly large.  This is done because we must store the
 * caller's idea of the map size to properly unmap.
 */
struct vmmap {
	LIST_ENTRY(vmmap)	vm_next;
	void 			*vm_addr;
	unsigned long		vm_size;
};

struct vmmaphd {
	struct vmmap *lh_first;
};
#define	VMMAP_HASH_SIZE	64
#define	VMMAP_HASH_MASK	(VMMAP_HASH_SIZE - 1)
#define	VM_HASH(addr)	((uintptr_t)(addr) >> PAGE_SHIFT) & VMMAP_HASH_MASK
static struct vmmaphd vmmaphead[VMMAP_HASH_SIZE];
static struct mtx vmmaplock;

static void
vmmap_add(void *addr, unsigned long size)
{
	struct vmmap *vmmap;

	vmmap = kmalloc(sizeof(*vmmap), GFP_KERNEL);
	mtx_lock(&vmmaplock);
	vmmap->vm_size = size;
	vmmap->vm_addr = addr;
	LIST_INSERT_HEAD(&vmmaphead[VM_HASH(addr)], vmmap, vm_next);
	mtx_unlock(&vmmaplock);
}

static struct vmmap *
vmmap_remove(void *addr)
{
	struct vmmap *vmmap;

	mtx_lock(&vmmaplock);
	LIST_FOREACH(vmmap, &vmmaphead[VM_HASH(addr)], vm_next)
		if (vmmap->vm_addr == addr)
			break;
	if (vmmap)
		LIST_REMOVE(vmmap, vm_next);
	mtx_unlock(&vmmaplock);

	return (vmmap);
}

#if defined(__i386__) || defined(__amd64__)
void *
_ioremap_attr(vm_paddr_t phys_addr, unsigned long size, int attr)
{
	void *addr;

	addr = pmap_mapdev_attr(phys_addr, size, attr);
	if (addr == NULL)
		return (NULL);
	vmmap_add(addr, size);

	return (addr);
}
#endif

void
iounmap(void *addr)
{
	struct vmmap *vmmap;

	vmmap = vmmap_remove(addr);
	if (vmmap == NULL)
		return;
#if defined(__i386__) || defined(__amd64__)
	pmap_unmapdev((vm_offset_t)addr, vmmap->vm_size);
#endif
	kfree(vmmap);
}


void *
vmap(struct page **pages, unsigned int count, unsigned long flags, int prot)
{
	vm_offset_t off;
	size_t size;

	size = count * PAGE_SIZE;
	off = kva_alloc(size);
	if (off == 0)
		return (NULL);
	vmmap_add((void *)off, size);
	pmap_qenter(off, pages, count);

	return ((void *)off);
}

void
vunmap(void *addr)
{
	struct vmmap *vmmap;

	vmmap = vmmap_remove(addr);
	if (vmmap == NULL)
		return;
	pmap_qremove((vm_offset_t)addr, vmmap->vm_size / PAGE_SIZE);
	kva_free((vm_offset_t)addr, vmmap->vm_size);
	kfree(vmmap);
}

char *
kvasprintf(gfp_t gfp, const char *fmt, va_list ap)
{
	unsigned int len;
	char *p;
	va_list aq;

	va_copy(aq, ap);
	len = vsnprintf(NULL, 0, fmt, aq);
	va_end(aq);

	p = kmalloc(len + 1, gfp);
	if (p != NULL)
		vsnprintf(p, len + 1, fmt, ap);

	return (p);
}

char *
kasprintf(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = kvasprintf(gfp, fmt, ap);
	va_end(ap);

	return (p);
}

static void
linux_timer_callback_wrapper(void *context)
{
	struct timer_list *timer;

	timer = context;
	timer->function(timer->data);
}

void
mod_timer(struct timer_list *timer, unsigned long expires)
{

	timer->expires = expires;
	callout_reset(&timer->timer_callout,		      
	    linux_timer_jiffies_until(expires),
	    &linux_timer_callback_wrapper, timer);
}

void
add_timer(struct timer_list *timer)
{

	callout_reset(&timer->timer_callout,
	    linux_timer_jiffies_until(timer->expires),
	    &linux_timer_callback_wrapper, timer);
}

static void
linux_timer_init(void *arg)
{

	/*
	 * Compute an internal HZ value which can divide 2**32 to
	 * avoid timer rounding problems when the tick value wraps
	 * around 2**32:
	 */
	linux_timer_hz_mask = 1;
	while (linux_timer_hz_mask < (unsigned long)hz)
		linux_timer_hz_mask *= 2;
	linux_timer_hz_mask--;
}
SYSINIT(linux_timer, SI_SUB_DRIVERS, SI_ORDER_FIRST, linux_timer_init, NULL);

void
linux_complete_common(struct completion *c, int all)
{
	int wakeup_swapper;

	sleepq_lock(c);
	c->done++;
	if (all)
		wakeup_swapper = sleepq_broadcast(c, SLEEPQ_SLEEP, 0, 0);
	else
		wakeup_swapper = sleepq_signal(c, SLEEPQ_SLEEP, 0, 0);
	sleepq_release(c);
	if (wakeup_swapper)
		kick_proc0();
}

/*
 * Indefinite wait for done != 0 with or without signals.
 */
long
linux_wait_for_common(struct completion *c, int flags)
{
	if (SCHEDULER_STOPPED())
		return (0);

	if (flags != 0)
		flags = SLEEPQ_INTERRUPTIBLE | SLEEPQ_SLEEP;
	else
		flags = SLEEPQ_SLEEP;
	for (;;) {
		sleepq_lock(c);
		if (c->done)
			break;
		sleepq_add(c, NULL, "completion", flags, 0);
		if (flags & SLEEPQ_INTERRUPTIBLE) {
			if (sleepq_wait_sig(c, 0) != 0)
				return (-ERESTARTSYS);
		} else
			sleepq_wait(c, 0);
	}
	c->done--;
	sleepq_release(c);

	return (0);
}

/*
 * Time limited wait for done != 0 with or without signals.
 */
long
linux_wait_for_timeout_common(struct completion *c, long timeout, int flags)
{
	long end = jiffies + timeout;

	if (SCHEDULER_STOPPED())
		return (0);

	if (flags != 0)
		flags = SLEEPQ_INTERRUPTIBLE | SLEEPQ_SLEEP;
	else
		flags = SLEEPQ_SLEEP;
	for (;;) {
		int ret;

		sleepq_lock(c);
		if (c->done)
			break;
		sleepq_add(c, NULL, "completion", flags, 0);
		sleepq_set_timeout(c, linux_timer_jiffies_until(end));
		if (flags & SLEEPQ_INTERRUPTIBLE)
			ret = sleepq_timedwait_sig(c, 0);
		else
			ret = sleepq_timedwait(c, 0);
		if (ret != 0) {
			/* check for timeout or signal */
			if (ret == EWOULDBLOCK)
				return (0);
			else
				return (-ERESTARTSYS);
		}
	}
	c->done--;
	sleepq_release(c);

	/* return how many jiffies are left */
	return (linux_timer_jiffies_until(end));
}

int
linux_try_wait_for_completion(struct completion *c)
{
	int isdone;

	isdone = 1;
	sleepq_lock(c);
	if (c->done)
		c->done--;
	else
		isdone = 0;
	sleepq_release(c);
	return (isdone);
}

int
linux_completion_done(struct completion *c)
{
	int isdone;

	isdone = 1;
	sleepq_lock(c);
	if (c->done == 0)
		isdone = 0;
	sleepq_release(c);
	return (isdone);
}

void
linux_delayed_work_fn(void *arg)
{
	struct delayed_work *work;

	work = arg;
	taskqueue_enqueue(work->work.taskqueue, &work->work.work_task);
}

void
linux_work_fn(void *context, int pending)
{
	struct work_struct *work;

	work = context;
	work->fn(work);
}

void
linux_flush_fn(void *context, int pending)
{
}

struct workqueue_struct *
linux_create_workqueue_common(const char *name, int cpus)
{
	struct workqueue_struct *wq;

	wq = kmalloc(sizeof(*wq), M_WAITOK);
	wq->taskqueue = taskqueue_create(name, M_WAITOK,
	    taskqueue_thread_enqueue,  &wq->taskqueue);
	atomic_set(&wq->draining, 0);
	taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, "%s", name);

	return (wq);
}

void
destroy_workqueue(struct workqueue_struct *wq)
{
	taskqueue_free(wq->taskqueue);
	kfree(wq);
}

static void
linux_cdev_release(struct kobject *kobj)
{
	struct linux_cdev *cdev;
	struct kobject *parent;

	cdev = container_of(kobj, struct linux_cdev, kobj);
	parent = kobj->parent;
	if (cdev->cdev)
		destroy_dev(cdev->cdev);
	kfree(cdev);
	kobject_put(parent);
}

static void
linux_cdev_static_release(struct kobject *kobj)
{
	struct linux_cdev *cdev;
	struct kobject *parent;

	cdev = container_of(kobj, struct linux_cdev, kobj);
	parent = kobj->parent;
	if (cdev->cdev)
		destroy_dev(cdev->cdev);
	kobject_put(parent);
}

const struct kobj_type linux_cdev_ktype = {
	.release = linux_cdev_release,
};

const struct kobj_type linux_cdev_static_ktype = {
	.release = linux_cdev_static_release,
};

static void
linux_handle_ifnet_link_event(void *arg, struct ifnet *ifp, int linkstate)
{
	struct notifier_block *nb;

	nb = arg;
	if (linkstate == LINK_STATE_UP)
		nb->notifier_call(nb, NETDEV_UP, ifp);
	else
		nb->notifier_call(nb, NETDEV_DOWN, ifp);
}

static void
linux_handle_ifnet_arrival_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_REGISTER, ifp);
}

static void
linux_handle_ifnet_departure_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_UNREGISTER, ifp);
}

static void
linux_handle_iflladdr_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_CHANGEADDR, ifp);
}

static void
linux_handle_ifaddr_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;

	nb = arg;
	nb->notifier_call(nb, NETDEV_CHANGEIFADDR, ifp);
}

int
register_netdevice_notifier(struct notifier_block *nb)
{

	nb->tags[NETDEV_UP] = EVENTHANDLER_REGISTER(
	    ifnet_link_event, linux_handle_ifnet_link_event, nb, 0);
	nb->tags[NETDEV_REGISTER] = EVENTHANDLER_REGISTER(
	    ifnet_arrival_event, linux_handle_ifnet_arrival_event, nb, 0);
	nb->tags[NETDEV_UNREGISTER] = EVENTHANDLER_REGISTER(
	    ifnet_departure_event, linux_handle_ifnet_departure_event, nb, 0);
	nb->tags[NETDEV_CHANGEADDR] = EVENTHANDLER_REGISTER(
	    iflladdr_event, linux_handle_iflladdr_event, nb, 0);

	return (0);
}

int
register_inetaddr_notifier(struct notifier_block *nb)
{

        nb->tags[NETDEV_CHANGEIFADDR] = EVENTHANDLER_REGISTER(
            ifaddr_event, linux_handle_ifaddr_event, nb, 0);
        return (0);
}

int
unregister_netdevice_notifier(struct notifier_block *nb)
{

        EVENTHANDLER_DEREGISTER(ifnet_link_event,
	    nb->tags[NETDEV_UP]);
        EVENTHANDLER_DEREGISTER(ifnet_arrival_event,
	    nb->tags[NETDEV_REGISTER]);
        EVENTHANDLER_DEREGISTER(ifnet_departure_event,
	    nb->tags[NETDEV_UNREGISTER]);
        EVENTHANDLER_DEREGISTER(iflladdr_event,
	    nb->tags[NETDEV_CHANGEADDR]);

	return (0);
}

int
unregister_inetaddr_notifier(struct notifier_block *nb)
{

        EVENTHANDLER_DEREGISTER(ifaddr_event,
            nb->tags[NETDEV_CHANGEIFADDR]);

        return (0);
}

struct list_sort_thunk {
	int (*cmp)(void *, struct list_head *, struct list_head *);
	void *priv;
};

static inline int
linux_le_cmp(void *priv, const void *d1, const void *d2)
{
	struct list_head *le1, *le2;
	struct list_sort_thunk *thunk;

	thunk = priv;
	le1 = *(__DECONST(struct list_head **, d1));
	le2 = *(__DECONST(struct list_head **, d2));
	return ((thunk->cmp)(thunk->priv, le1, le2));
}

void
list_sort(void *priv, struct list_head *head, int (*cmp)(void *priv,
    struct list_head *a, struct list_head *b))
{
	struct list_sort_thunk thunk;
	struct list_head **ar, *le;
	size_t count, i;

	count = 0;
	list_for_each(le, head)
		count++;
	ar = malloc(sizeof(struct list_head *) * count, M_KMALLOC, M_WAITOK);
	i = 0;
	list_for_each(le, head)
		ar[i++] = le;
	thunk.cmp = cmp;
	thunk.priv = priv;
	qsort_r(ar, count, sizeof(struct list_head *), &thunk, linux_le_cmp);
	INIT_LIST_HEAD(head);
	for (i = 0; i < count; i++)
		list_add_tail(ar[i], head);
	free(ar, M_KMALLOC);
}

void
linux_irq_handler(void *ent)
{
	struct irq_ent *irqe;

	irqe = ent;
	irqe->handler(irqe->irq, irqe->arg);
}

#if defined(__i386__) || defined(__amd64__)
bool linux_cpu_has_clflush;
#endif

static void
linux_compat_init(void *arg)
{
	struct sysctl_oid *rootoid;
	int i;

#if defined(__i386__) || defined(__amd64__)
	linux_cpu_has_clflush = (cpu_feature & CPUID_CLFSH);
#endif
	sx_init(&linux_global_rcu_lock, "LinuxGlobalRCU");

	rootoid = SYSCTL_ADD_ROOT_NODE(NULL,
	    OID_AUTO, "sys", CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, "sys");
	kobject_init(&linux_class_root, &linux_class_ktype);
	kobject_set_name(&linux_class_root, "class");
	linux_class_root.oidp = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(rootoid),
	    OID_AUTO, "class", CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, "class");
	kobject_init(&linux_root_device.kobj, &linux_dev_ktype);
	kobject_set_name(&linux_root_device.kobj, "device");
	linux_root_device.kobj.oidp = SYSCTL_ADD_NODE(NULL,
	    SYSCTL_CHILDREN(rootoid), OID_AUTO, "device", CTLFLAG_RD, NULL,
	    "device");
	linux_root_device.bsddev = root_bus;
	linux_class_misc.name = "misc";
	class_register(&linux_class_misc);
	INIT_LIST_HEAD(&pci_drivers);
	INIT_LIST_HEAD(&pci_devices);
	spin_lock_init(&pci_lock);
	mtx_init(&vmmaplock, "IO Map lock", NULL, MTX_DEF);
	for (i = 0; i < VMMAP_HASH_SIZE; i++)
		LIST_INIT(&vmmaphead[i]);
}
SYSINIT(linux_compat, SI_SUB_DRIVERS, SI_ORDER_SECOND, linux_compat_init, NULL);

static void
linux_compat_uninit(void *arg)
{
	linux_kobject_kfree_name(&linux_class_root);
	linux_kobject_kfree_name(&linux_root_device.kobj);
	linux_kobject_kfree_name(&linux_class_misc.kobj);

	synchronize_rcu();
	sx_destroy(&linux_global_rcu_lock);
}
SYSUNINIT(linux_compat, SI_SUB_DRIVERS, SI_ORDER_SECOND, linux_compat_uninit, NULL);

/*
 * NOTE: Linux frequently uses "unsigned long" for pointer to integer
 * conversion and vice versa, where in FreeBSD "uintptr_t" would be
 * used. Assert these types have the same size, else some parts of the
 * LinuxKPI may not work like expected:
 */
CTASSERT(sizeof(unsigned long) == sizeof(uintptr_t));
