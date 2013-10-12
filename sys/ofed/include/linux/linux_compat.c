/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
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
#include <machine/pmap.h>

#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/sysfs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/vmalloc.h>

#include <vm/vm_pager.h>

MALLOC_DEFINE(M_KMALLOC, "linux", "Linux kmalloc compat");

#include <linux/rbtree.h>
/* Undo Linux compat changes. */
#undef RB_ROOT
#undef file
#undef cdev
#define	RB_ROOT(head)	(head)->rbh_root
#undef LIST_HEAD
/* From sys/queue.h */
#define LIST_HEAD(name, type)						\
struct name {								\
	struct type *lh_first;	/* first element */			\
}

struct kobject class_root;
struct device linux_rootdev;
struct class miscclass;
struct list_head pci_drivers;
struct list_head pci_devices;
spinlock_t pci_lock;

int
panic_cmp(struct rb_node *one, struct rb_node *two)
{
	panic("no cmp");
}

RB_GENERATE(linux_root, rb_node, __entry, panic_cmp);
 
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

static inline int
kobject_add_complete(struct kobject *kobj, struct kobject *parent)
{
	struct kobj_type *t;
	int error;

	kobj->parent = kobject_get(parent);
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
kobject_release(struct kref *kref)
{
	struct kobject *kobj;
	char *name;

	kobj = container_of(kref, struct kobject, kref);
	sysfs_remove_dir(kobj);
	if (kobj->parent)
		kobject_put(kobj->parent);
	kobj->parent = NULL;
	name = kobj->name;
	if (kobj->ktype && kobj->ktype->release)
		kobj->ktype->release(kobj);
	kfree(name);
}

static void
kobject_kfree(struct kobject *kobj)
{

	kfree(kobj);
}

struct kobj_type kfree_type = { .release = kobject_kfree };

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
	va_start(args, fmt);
	kobject_set_name_vargs(&dev->kobj, fmt, args);
	va_end(args);
	device_register(dev);

	return (dev);
}

int
kobject_init_and_add(struct kobject *kobj, struct kobj_type *ktype,
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

static void
linux_file_dtor(void *cdp)
{
	struct linux_file *filp;

	filp = cdp;
	filp->f_op->release(filp->f_vnode, filp);
	vdrop(filp->f_vnode);
	kfree(filp);
}

static int
linux_dev_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct file *file;
	int error;

	file = curthread->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (ENODEV);
	filp = kzalloc(sizeof(*filp), GFP_KERNEL);
	filp->f_dentry = &filp->f_dentry_store;
	filp->f_op = ldev->ops;
	filp->f_flags = file->f_flag;
	vhold(file->f_vnode);
	filp->f_vnode = file->f_vnode;
	if (filp->f_op->open) {
		error = -filp->f_op->open(file->f_vnode, filp);
		if (error) {
			kfree(filp);
			return (error);
		}
	}
	error = devfs_set_cdevpriv(filp, linux_file_dtor);
	if (error) {
		filp->f_op->release(file->f_vnode, filp);
		kfree(filp);
		return (error);
	}

	return 0;
}

static int
linux_dev_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct file *file;
	int error;

	file = curthread->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
        devfs_clear_cdevpriv();
        

	return (0);
}

static int
linux_dev_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct file *file;
	int error;

	file = curthread->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	/*
	 * Linux does not have a generic ioctl copyin/copyout layer.  All
	 * linux ioctls must be converted to void ioctls which pass a
	 * pointer to the address of the data.  We want the actual user
	 * address so we dereference here.
	 */
	data = *(void **)data;
	if (filp->f_op->unlocked_ioctl)
		error = -filp->f_op->unlocked_ioctl(filp, cmd, (u_long)data);
	else
		error = ENOTTY;

	return (error);
}

static int
linux_dev_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct file *file;
	ssize_t bytes;
	int error;

	file = curthread->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	if (uio->uio_iovcnt != 1)
		panic("linux_dev_read: uio %p iovcnt %d",
		    uio, uio->uio_iovcnt);
	if (filp->f_op->read) {
		bytes = filp->f_op->read(filp, uio->uio_iov->iov_base,
		    uio->uio_iov->iov_len, &uio->uio_offset);
		if (bytes >= 0) {
			uio->uio_iov->iov_base += bytes;
			uio->uio_iov->iov_len -= bytes;
			uio->uio_resid -= bytes;
		} else
			error = -bytes;
	} else
		error = ENXIO;

	return (error);
}

static int
linux_dev_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct file *file;
	ssize_t bytes;
	int error;

	file = curthread->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	if (uio->uio_iovcnt != 1)
		panic("linux_dev_write: uio %p iovcnt %d",
		    uio, uio->uio_iovcnt);
	if (filp->f_op->write) {
		bytes = filp->f_op->write(filp, uio->uio_iov->iov_base,
		    uio->uio_iov->iov_len, &uio->uio_offset);
		if (bytes >= 0) {
			uio->uio_iov->iov_base += bytes;
			uio->uio_iov->iov_len -= bytes;
			uio->uio_resid -= bytes;
		} else
			error = -bytes;
	} else
		error = ENXIO;

	return (error);
}

static int
linux_dev_poll(struct cdev *dev, int events, struct thread *td)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct file *file;
	int revents;
	int error;

	file = curthread->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (0);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	if (filp->f_op->poll)
		revents = filp->f_op->poll(filp, NULL) & events;
	else
		revents = 0;

	return (revents);
}

static int
linux_dev_mmap(struct cdev *dev, vm_ooffset_t offset, vm_paddr_t *paddr,
    int nprot, vm_memattr_t *memattr)
{

	/* XXX memattr not honored. */
	*paddr = offset;
	return (0);
}

static int
linux_dev_mmap_single(struct cdev *dev, vm_ooffset_t *offset,
    vm_size_t size, struct vm_object **object, int nprot)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	struct file *file;
	struct vm_area_struct vma;
	vm_paddr_t paddr;
	vm_page_t m;
	int error;

	file = curthread->td_fpop;
	ldev = dev->si_drv1;
	if (ldev == NULL)
		return (ENODEV);
	if (size != PAGE_SIZE)
		return (EINVAL);
	if ((error = devfs_get_cdevpriv((void **)&filp)) != 0)
		return (error);
	filp->f_flags = file->f_flag;
	vma.vm_start = 0;
	vma.vm_end = PAGE_SIZE;
	vma.vm_pgoff = *offset / PAGE_SIZE;
	vma.vm_pfn = 0;
	vma.vm_page_prot = 0;
	if (filp->f_op->mmap) {
		error = -filp->f_op->mmap(filp, &vma);
		if (error == 0) {
			paddr = (vm_paddr_t)vma.vm_pfn << PAGE_SHIFT;
			*offset = paddr;
			m = PHYS_TO_VM_PAGE(paddr);
			*object = vm_pager_allocate(OBJT_DEVICE, dev,
			    PAGE_SIZE, nprot, *offset, curthread->td_ucred);
		        if (*object == NULL)
               			 return (EINVAL);
			if (vma.vm_page_prot != VM_MEMATTR_DEFAULT)
				pmap_page_set_memattr(m, vma.vm_page_prot);
		}
	} else
		error = ENODEV;

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
	.d_mmap = linux_dev_mmap,
	.d_poll = linux_dev_poll,
};

static int
linux_file_read(struct file *file, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct linux_file *filp;
	ssize_t bytes;
	int error;

	error = 0;
	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	if (uio->uio_iovcnt != 1)
		panic("linux_file_read: uio %p iovcnt %d",
		    uio, uio->uio_iovcnt);
	if (filp->f_op->read) {
		bytes = filp->f_op->read(filp, uio->uio_iov->iov_base,
		    uio->uio_iov->iov_len, &uio->uio_offset);
		if (bytes >= 0) {
			uio->uio_iov->iov_base += bytes;
			uio->uio_iov->iov_len -= bytes;
			uio->uio_resid -= bytes;
		} else
			error = -bytes;
	} else
		error = ENXIO;

	return (error);
}

static int
linux_file_poll(struct file *file, int events, struct ucred *active_cred,
    struct thread *td)
{
	struct linux_file *filp;
	int revents;

	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	if (filp->f_op->poll)
		revents = filp->f_op->poll(filp, NULL) & events;
	else
		revents = 0;

	return (0);
}

static int
linux_file_close(struct file *file, struct thread *td)
{
	struct linux_file *filp;
	int error;

	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	error = -filp->f_op->release(NULL, filp);
	funsetown(&filp->f_sigio);
	kfree(filp);

	return (error);
}

static int
linux_file_ioctl(struct file *fp, u_long cmd, void *data, struct ucred *cred,
    struct thread *td)
{
	struct linux_file *filp;
	int error;

	filp = (struct linux_file *)fp->f_data;
	filp->f_flags = fp->f_flag;
	error = 0;

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
	return (error);
}

struct fileops linuxfileops = {
	.fo_read = linux_file_read,
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

LIST_HEAD(vmmaphd, vmmap);
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

void
iounmap(void *addr)
{
	struct vmmap *vmmap;

	vmmap = vmmap_remove(addr);
	if (vmmap == NULL)
		return;
	pmap_unmapdev((vm_offset_t)addr, vmmap->vm_size);
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

static void
linux_compat_init(void)
{
	struct sysctl_oid *rootoid;
	int i;

	rootoid = SYSCTL_ADD_NODE(NULL, SYSCTL_STATIC_CHILDREN(),
	    OID_AUTO, "sys", CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, "sys");
	kobject_init(&class_root, &class_ktype);
	kobject_set_name(&class_root, "class");
	class_root.oidp = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(rootoid),
	    OID_AUTO, "class", CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, "class");
	kobject_init(&linux_rootdev.kobj, &dev_ktype);
	kobject_set_name(&linux_rootdev.kobj, "device");
	linux_rootdev.kobj.oidp = SYSCTL_ADD_NODE(NULL,
	    SYSCTL_CHILDREN(rootoid), OID_AUTO, "device", CTLFLAG_RD, NULL,
	    "device");
	linux_rootdev.bsddev = root_bus;
	miscclass.name = "misc";
	class_register(&miscclass);
	INIT_LIST_HEAD(&pci_drivers);
	INIT_LIST_HEAD(&pci_devices);
	spin_lock_init(&pci_lock);
	mtx_init(&vmmaplock, "IO Map lock", NULL, MTX_DEF);
	for (i = 0; i < VMMAP_HASH_SIZE; i++)
		LIST_INIT(&vmmaphead[i]);
}

SYSINIT(linux_compat, SI_SUB_DRIVERS, SI_ORDER_SECOND, linux_compat_init, NULL);
