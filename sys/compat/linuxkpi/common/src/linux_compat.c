/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2021 Mellanox Technologies, Ltd.
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
#include "opt_global.h"
#include "opt_stack.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/sglist.h>
#include <sys/sleepqueue.h>
#include <sys/refcount.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/rwlock.h>
#include <sys/mman.h>
#include <sys/stack.h>
#include <sys/sysent.h>
#include <sys/time.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_radix.h>

#include <machine/stdarg.h>

#if defined(__i386__) || defined(__amd64__)
#include <machine/cputypes.h>
#include <machine/md_var.h>
#endif

#include <linux/kobject.h>
#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/compat.h>
#include <linux/io-mapping.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/wait_bit.h>
#include <linux/rcupdate.h>
#include <linux/interval_tree.h>
#include <linux/interval_tree_generic.h>
#include <linux/printk.h>
#include <linux/seq_file.h>

#if defined(__i386__) || defined(__amd64__)
#include <asm/smp.h>
#include <asm/processor.h>
#endif

#include <xen/xen.h>
#ifdef XENHVM
#undef xen_pv_domain
#undef xen_initial_domain
/* xen/xen-os.h redefines __must_check */
#undef __must_check
#include <xen/xen-os.h>
#endif

SYSCTL_NODE(_compat, OID_AUTO, linuxkpi, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "LinuxKPI parameters");

int linuxkpi_debug;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, debug, CTLFLAG_RWTUN,
    &linuxkpi_debug, 0, "Set to enable pr_debug() prints. Clear to disable.");

int linuxkpi_rcu_debug;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, rcu_debug, CTLFLAG_RWTUN,
    &linuxkpi_rcu_debug, 0, "Set to enable RCU warning. Clear to disable.");

int linuxkpi_warn_dump_stack = 0;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, warn_dump_stack, CTLFLAG_RWTUN,
    &linuxkpi_warn_dump_stack, 0,
    "Set to enable stack traces from WARN_ON(). Clear to disable.");

static struct timeval lkpi_net_lastlog;
static int lkpi_net_curpps;
static int lkpi_net_maxpps = 99;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, net_ratelimit, CTLFLAG_RWTUN,
    &lkpi_net_maxpps, 0, "Limit number of LinuxKPI net messages per second.");

MALLOC_DEFINE(M_KMALLOC, "lkpikmalloc", "Linux kmalloc compat");

#include <linux/rbtree.h>
/* Undo Linux compat changes. */
#undef RB_ROOT
#undef file
#undef cdev
#define	RB_ROOT(head)	(head)->rbh_root

static void linux_destroy_dev(struct linux_cdev *);
static void linux_cdev_deref(struct linux_cdev *ldev);
static struct vm_area_struct *linux_cdev_handle_find(void *handle);

cpumask_t cpu_online_mask;
static cpumask_t **static_single_cpu_mask;
static cpumask_t *static_single_cpu_mask_lcs;
struct kobject linux_class_root;
struct device linux_root_device;
struct class linux_class_misc;
struct list_head pci_drivers;
struct list_head pci_devices;
spinlock_t pci_lock;
struct uts_namespace init_uts_ns;

unsigned long linux_timer_hz_mask;

wait_queue_head_t linux_bit_waitq;
wait_queue_head_t linux_var_waitq;

int
panic_cmp(struct rb_node *one, struct rb_node *two)
{
	panic("no cmp");
}

RB_GENERATE(linux_root, rb_node, __entry, panic_cmp);

#define	START(node)	((node)->start)
#define	LAST(node)	((node)->last)

INTERVAL_TREE_DEFINE(struct interval_tree_node, rb, unsigned long,, START,
    LAST,, lkpi_interval_tree)

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

struct device *
device_create_groups_vargs(struct class *class, struct device *parent,
    dev_t devt, void *drvdata, const struct attribute_group **groups,
    const char *fmt, va_list args)
{
	struct device *dev = NULL;
	int retval = -ENODEV;

	if (class == NULL || IS_ERR(class))
		goto error;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		retval = -ENOMEM;
		goto error;
	}

	dev->devt = devt;
	dev->class = class;
	dev->parent = parent;
	dev->groups = groups;
	dev->release = device_create_release;
	/* device_initialize() needs the class and parent to be set */
	device_initialize(dev);
	dev_set_drvdata(dev, drvdata);

	retval = kobject_set_name_vargs(&dev->kobj, fmt, args);
	if (retval)
		goto error;

	retval = device_add(dev);
	if (retval)
		goto error;

	return dev;

error:
	put_device(dev);
	return ERR_PTR(retval);
}

struct class *
lkpi_class_create(const char *name)
{
	struct class *class;
	int error;

	class = kzalloc(sizeof(*class), M_WAITOK);
	class->name = name;
	class->class_release = linux_class_kfree;
	error = class_register(class);
	if (error) {
		kfree(class);
		return (NULL);
	}

	return (class);
}

static void
linux_kq_lock(void *arg)
{
	spinlock_t *s = arg;

	spin_lock(s);
}
static void
linux_kq_unlock(void *arg)
{
	spinlock_t *s = arg;

	spin_unlock(s);
}

static void
linux_kq_assert_lock(void *arg, int what)
{
#ifdef INVARIANTS
	spinlock_t *s = arg;

	if (what == LA_LOCKED)
		mtx_assert(s, MA_OWNED);
	else
		mtx_assert(s, MA_NOTOWNED);
#endif
}

static void
linux_file_kqfilter_poll(struct linux_file *, int);

struct linux_file *
linux_file_alloc(void)
{
	struct linux_file *filp;

	filp = kzalloc(sizeof(*filp), GFP_KERNEL);

	/* set initial refcount */
	filp->f_count = 1;

	/* setup fields needed by kqueue support */
	spin_lock_init(&filp->f_kqlock);
	knlist_init(&filp->f_selinfo.si_note, &filp->f_kqlock,
	    linux_kq_lock, linux_kq_unlock, linux_kq_assert_lock);

	return (filp);
}

void
linux_file_free(struct linux_file *filp)
{
	if (filp->_file == NULL) {
		if (filp->f_op != NULL && filp->f_op->release != NULL)
			filp->f_op->release(filp->f_vnode, filp);
		if (filp->f_shmem != NULL)
			vm_object_deallocate(filp->f_shmem);
		kfree_rcu(filp, rcu);
	} else {
		/*
		 * The close method of the character device or file
		 * will free the linux_file structure:
		 */
		_fdrop(filp->_file, curthread);
	}
}

struct linux_cdev *
cdev_alloc(void)
{
	struct linux_cdev *cdev;

	cdev = kzalloc(sizeof(struct linux_cdev), M_WAITOK);
	kobject_init(&cdev->kobj, &linux_cdev_ktype);
	cdev->refs = 1;
	return (cdev);
}

static int
linux_cdev_pager_fault(vm_object_t vm_obj, vm_ooffset_t offset, int prot,
    vm_page_t *mres)
{
	struct vm_area_struct *vmap;

	vmap = linux_cdev_handle_find(vm_obj->handle);

	MPASS(vmap != NULL);
	MPASS(vmap->vm_private_data == vm_obj->handle);

	if (likely(vmap->vm_ops != NULL && offset < vmap->vm_len)) {
		vm_paddr_t paddr = IDX_TO_OFF(vmap->vm_pfn) + offset;
		vm_page_t page;

		if (((*mres)->flags & PG_FICTITIOUS) != 0) {
			/*
			 * If the passed in result page is a fake
			 * page, update it with the new physical
			 * address.
			 */
			page = *mres;
			vm_page_updatefake(page, paddr, vm_obj->memattr);
		} else {
			/*
			 * Replace the passed in "mres" page with our
			 * own fake page and free up the all of the
			 * original pages.
			 */
			VM_OBJECT_WUNLOCK(vm_obj);
			page = vm_page_getfake(paddr, vm_obj->memattr);
			VM_OBJECT_WLOCK(vm_obj);

			vm_page_replace(page, vm_obj, (*mres)->pindex, *mres);
			*mres = page;
		}
		vm_page_valid(page);
		return (VM_PAGER_OK);
	}
	return (VM_PAGER_FAIL);
}

static int
linux_cdev_pager_populate(vm_object_t vm_obj, vm_pindex_t pidx, int fault_type,
    vm_prot_t max_prot, vm_pindex_t *first, vm_pindex_t *last)
{
	struct vm_area_struct *vmap;
	int err;

	/* get VM area structure */
	vmap = linux_cdev_handle_find(vm_obj->handle);
	MPASS(vmap != NULL);
	MPASS(vmap->vm_private_data == vm_obj->handle);

	VM_OBJECT_WUNLOCK(vm_obj);

	linux_set_current(curthread);

	down_write(&vmap->vm_mm->mmap_sem);
	if (unlikely(vmap->vm_ops == NULL)) {
		err = VM_FAULT_SIGBUS;
	} else {
		struct vm_fault vmf;

		/* fill out VM fault structure */
		vmf.virtual_address = (void *)(uintptr_t)IDX_TO_OFF(pidx);
		vmf.flags = (fault_type & VM_PROT_WRITE) ? FAULT_FLAG_WRITE : 0;
		vmf.pgoff = 0;
		vmf.page = NULL;
		vmf.vma = vmap;

		vmap->vm_pfn_count = 0;
		vmap->vm_pfn_pcount = &vmap->vm_pfn_count;
		vmap->vm_obj = vm_obj;

		err = vmap->vm_ops->fault(&vmf);

		while (vmap->vm_pfn_count == 0 && err == VM_FAULT_NOPAGE) {
			kern_yield(PRI_USER);
			err = vmap->vm_ops->fault(&vmf);
		}
	}

	/* translate return code */
	switch (err) {
	case VM_FAULT_OOM:
		err = VM_PAGER_AGAIN;
		break;
	case VM_FAULT_SIGBUS:
		err = VM_PAGER_BAD;
		break;
	case VM_FAULT_NOPAGE:
		/*
		 * By contract the fault handler will return having
		 * busied all the pages itself. If pidx is already
		 * found in the object, it will simply xbusy the first
		 * page and return with vm_pfn_count set to 1.
		 */
		*first = vmap->vm_pfn_first;
		*last = *first + vmap->vm_pfn_count - 1;
		err = VM_PAGER_OK;
		break;
	default:
		err = VM_PAGER_ERROR;
		break;
	}
	up_write(&vmap->vm_mm->mmap_sem);
	VM_OBJECT_WLOCK(vm_obj);
	return (err);
}

static struct rwlock linux_vma_lock;
static TAILQ_HEAD(, vm_area_struct) linux_vma_head =
    TAILQ_HEAD_INITIALIZER(linux_vma_head);

static void
linux_cdev_handle_free(struct vm_area_struct *vmap)
{
	/* Drop reference on vm_file */
	if (vmap->vm_file != NULL)
		fput(vmap->vm_file);

	/* Drop reference on mm_struct */
	mmput(vmap->vm_mm);

	kfree(vmap);
}

static void
linux_cdev_handle_remove(struct vm_area_struct *vmap)
{
	rw_wlock(&linux_vma_lock);
	TAILQ_REMOVE(&linux_vma_head, vmap, vm_entry);
	rw_wunlock(&linux_vma_lock);
}

static struct vm_area_struct *
linux_cdev_handle_find(void *handle)
{
	struct vm_area_struct *vmap;

	rw_rlock(&linux_vma_lock);
	TAILQ_FOREACH(vmap, &linux_vma_head, vm_entry) {
		if (vmap->vm_private_data == handle)
			break;
	}
	rw_runlock(&linux_vma_lock);
	return (vmap);
}

static int
linux_cdev_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
		      vm_ooffset_t foff, struct ucred *cred, u_short *color)
{

	MPASS(linux_cdev_handle_find(handle) != NULL);
	*color = 0;
	return (0);
}

static void
linux_cdev_pager_dtor(void *handle)
{
	const struct vm_operations_struct *vm_ops;
	struct vm_area_struct *vmap;

	vmap = linux_cdev_handle_find(handle);
	MPASS(vmap != NULL);

	/*
	 * Remove handle before calling close operation to prevent
	 * other threads from reusing the handle pointer.
	 */
	linux_cdev_handle_remove(vmap);

	down_write(&vmap->vm_mm->mmap_sem);
	vm_ops = vmap->vm_ops;
	if (likely(vm_ops != NULL))
		vm_ops->close(vmap);
	up_write(&vmap->vm_mm->mmap_sem);

	linux_cdev_handle_free(vmap);
}

static struct cdev_pager_ops linux_cdev_pager_ops[2] = {
  {
	/* OBJT_MGTDEVICE */
	.cdev_pg_populate	= linux_cdev_pager_populate,
	.cdev_pg_ctor	= linux_cdev_pager_ctor,
	.cdev_pg_dtor	= linux_cdev_pager_dtor
  },
  {
	/* OBJT_DEVICE */
	.cdev_pg_fault	= linux_cdev_pager_fault,
	.cdev_pg_ctor	= linux_cdev_pager_ctor,
	.cdev_pg_dtor	= linux_cdev_pager_dtor
  },
};

int
zap_vma_ptes(struct vm_area_struct *vma, unsigned long address,
    unsigned long size)
{
	struct pctrie_iter pages;
	vm_object_t obj;
	vm_page_t m;

	obj = vma->vm_obj;
	if (obj == NULL || (obj->flags & OBJ_UNMANAGED) != 0)
		return (-ENOTSUP);
	VM_OBJECT_RLOCK(obj);
	vm_page_iter_limit_init(&pages, obj, OFF_TO_IDX(address + size));
	VM_RADIX_FOREACH_FROM(m, &pages, OFF_TO_IDX(address))
		pmap_remove_all(m);
	VM_OBJECT_RUNLOCK(obj);
	return (0);
}

void
vma_set_file(struct vm_area_struct *vma, struct linux_file *file)
{
	struct linux_file *tmp;

	/* Changing an anonymous vma with this is illegal */
	get_file(file);
	tmp = vma->vm_file;
	vma->vm_file = file;
	fput(tmp);
}

static struct file_operations dummy_ldev_ops = {
	/* XXXKIB */
};

static struct linux_cdev dummy_ldev = {
	.ops = &dummy_ldev_ops,
};

#define	LDEV_SI_DTR	0x0001
#define	LDEV_SI_REF	0x0002

static void
linux_get_fop(struct linux_file *filp, const struct file_operations **fop,
    struct linux_cdev **dev)
{
	struct linux_cdev *ldev;
	u_int siref;

	ldev = filp->f_cdev;
	*fop = filp->f_op;
	if (ldev != NULL) {
		if (ldev->kobj.ktype == &linux_cdev_static_ktype) {
			refcount_acquire(&ldev->refs);
		} else {
			for (siref = ldev->siref;;) {
				if ((siref & LDEV_SI_DTR) != 0) {
					ldev = &dummy_ldev;
					*fop = ldev->ops;
					siref = ldev->siref;
					MPASS((ldev->siref & LDEV_SI_DTR) == 0);
				} else if (atomic_fcmpset_int(&ldev->siref,
				    &siref, siref + LDEV_SI_REF)) {
					break;
				}
			}
		}
	}
	*dev = ldev;
}

static void
linux_drop_fop(struct linux_cdev *ldev)
{

	if (ldev == NULL)
		return;
	if (ldev->kobj.ktype == &linux_cdev_static_ktype) {
		linux_cdev_deref(ldev);
	} else {
		MPASS(ldev->kobj.ktype == &linux_cdev_ktype);
		MPASS((ldev->siref & ~LDEV_SI_DTR) != 0);
		atomic_subtract_int(&ldev->siref, LDEV_SI_REF);
	}
}

#define	OPW(fp,td,code) ({			\
	struct file *__fpop;			\
	__typeof(code) __retval;		\
						\
	__fpop = (td)->td_fpop;			\
	(td)->td_fpop = (fp);			\
	__retval = (code);			\
	(td)->td_fpop = __fpop;			\
	__retval;				\
})

static int
linux_dev_fdopen(struct cdev *dev, int fflags, struct thread *td,
    struct file *file)
{
	struct linux_cdev *ldev;
	struct linux_file *filp;
	const struct file_operations *fop;
	int error;

	ldev = dev->si_drv1;

	filp = linux_file_alloc();
	filp->f_dentry = &filp->f_dentry_store;
	filp->f_op = ldev->ops;
	filp->f_mode = file->f_flag;
	filp->f_flags = file->f_flag;
	filp->f_vnode = file->f_vnode;
	filp->_file = file;
	refcount_acquire(&ldev->refs);
	filp->f_cdev = ldev;

	linux_set_current(td);
	linux_get_fop(filp, &fop, &ldev);

	if (fop->open != NULL) {
		error = -fop->open(file->f_vnode, filp);
		if (error != 0) {
			linux_drop_fop(ldev);
			linux_cdev_deref(filp->f_cdev);
			kfree(filp);
			return (error);
		}
	}

	/* hold on to the vnode - used for fstat() */
	vref(filp->f_vnode);

	/* release the file from devfs */
	finit(file, filp->f_mode, DTYPE_DEV, filp, &linuxfileops);
	linux_drop_fop(ldev);
	return (ENXIO);
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

size_t
linux_clear_user(void *_uaddr, size_t _len)
{
	uint8_t *uaddr = _uaddr;
	size_t len = _len;

	/* make sure uaddr is aligned before going into the fast loop */
	while (((uintptr_t)uaddr & 7) != 0 && len > 7) {
		if (subyte(uaddr, 0))
			return (_len);
		uaddr++;
		len--;
	}

	/* zero 8 bytes at a time */
	while (len > 7) {
#ifdef __LP64__
		if (suword64(uaddr, 0))
			return (_len);
#else
		if (suword32(uaddr, 0))
			return (_len);
		if (suword32(uaddr + 4, 0))
			return (_len);
#endif
		uaddr += 8;
		len -= 8;
	}

	/* zero fill end, if any */
	while (len > 0) {
		if (subyte(uaddr, 0))
			return (_len);
		uaddr++;
		len--;
	}
	return (0);
}

int
linux_access_ok(const void *uaddr, size_t len)
{
	uintptr_t saddr;
	uintptr_t eaddr;

	/* get start and end address */
	saddr = (uintptr_t)uaddr;
	eaddr = (uintptr_t)uaddr + len;

	/* verify addresses are valid for userspace */
	return ((saddr == eaddr) ||
	    (eaddr > saddr && eaddr <= VM_MAXUSER_ADDRESS));
}

/*
 * This function should return either EINTR or ERESTART depending on
 * the signal type sent to this thread:
 */
static int
linux_get_error(struct task_struct *task, int error)
{
	/* check for signal type interrupt code */
	if (error == EINTR || error == ERESTARTSYS || error == ERESTART) {
		error = -linux_schedule_get_interrupt_value(task);
		if (error == 0)
			error = EINTR;
	}
	return (error);
}

static int
linux_file_ioctl_sub(struct file *fp, struct linux_file *filp,
    const struct file_operations *fop, u_long cmd, caddr_t data,
    struct thread *td)
{
	struct task_struct *task = current;
	unsigned size;
	int error;

	size = IOCPARM_LEN(cmd);
	/* refer to logic in sys_ioctl() */
	if (size > 0) {
		/*
		 * Setup hint for linux_copyin() and linux_copyout().
		 *
		 * Background: Linux code expects a user-space address
		 * while FreeBSD supplies a kernel-space address.
		 */
		task->bsd_ioctl_data = data;
		task->bsd_ioctl_len = size;
		data = (void *)LINUX_IOCTL_MIN_PTR;
	} else {
		/* fetch user-space pointer */
		data = *(void **)data;
	}
#ifdef COMPAT_FREEBSD32
	if (SV_PROC_FLAG(td->td_proc, SV_ILP32)) {
		/* try the compat IOCTL handler first */
		if (fop->compat_ioctl != NULL) {
			error = -OPW(fp, td, fop->compat_ioctl(filp,
			    cmd, (u_long)data));
		} else {
			error = ENOTTY;
		}

		/* fallback to the regular IOCTL handler, if any */
		if (error == ENOTTY && fop->unlocked_ioctl != NULL) {
			error = -OPW(fp, td, fop->unlocked_ioctl(filp,
			    cmd, (u_long)data));
		}
	} else
#endif
	{
		if (fop->unlocked_ioctl != NULL) {
			error = -OPW(fp, td, fop->unlocked_ioctl(filp,
			    cmd, (u_long)data));
		} else {
			error = ENOTTY;
		}
	}
	if (size > 0) {
		task->bsd_ioctl_data = NULL;
		task->bsd_ioctl_len = 0;
	}

	if (error == EWOULDBLOCK) {
		/* update kqfilter status, if any */
		linux_file_kqfilter_poll(filp,
		    LINUX_KQ_FLAG_HAS_READ | LINUX_KQ_FLAG_HAS_WRITE);
	} else {
		error = linux_get_error(task, error);
	}
	return (error);
}

#define	LINUX_POLL_TABLE_NORMAL ((poll_table *)1)

/*
 * This function atomically updates the poll wakeup state and returns
 * the previous state at the time of update.
 */
static uint8_t
linux_poll_wakeup_state(atomic_t *v, const uint8_t *pstate)
{
	int c, old;

	c = v->counter;

	while ((old = atomic_cmpxchg(v, c, pstate[c])) != c)
		c = old;

	return (c);
}

static int
linux_poll_wakeup_callback(wait_queue_t *wq, unsigned int wq_state, int flags, void *key)
{
	static const uint8_t state[LINUX_FWQ_STATE_MAX] = {
		[LINUX_FWQ_STATE_INIT] = LINUX_FWQ_STATE_INIT, /* NOP */
		[LINUX_FWQ_STATE_NOT_READY] = LINUX_FWQ_STATE_NOT_READY, /* NOP */
		[LINUX_FWQ_STATE_QUEUED] = LINUX_FWQ_STATE_READY,
		[LINUX_FWQ_STATE_READY] = LINUX_FWQ_STATE_READY, /* NOP */
	};
	struct linux_file *filp = container_of(wq, struct linux_file, f_wait_queue.wq);

	switch (linux_poll_wakeup_state(&filp->f_wait_queue.state, state)) {
	case LINUX_FWQ_STATE_QUEUED:
		linux_poll_wakeup(filp);
		return (1);
	default:
		return (0);
	}
}

void
linux_poll_wait(struct linux_file *filp, wait_queue_head_t *wqh, poll_table *p)
{
	static const uint8_t state[LINUX_FWQ_STATE_MAX] = {
		[LINUX_FWQ_STATE_INIT] = LINUX_FWQ_STATE_NOT_READY,
		[LINUX_FWQ_STATE_NOT_READY] = LINUX_FWQ_STATE_NOT_READY, /* NOP */
		[LINUX_FWQ_STATE_QUEUED] = LINUX_FWQ_STATE_QUEUED, /* NOP */
		[LINUX_FWQ_STATE_READY] = LINUX_FWQ_STATE_QUEUED,
	};

	/* check if we are called inside the select system call */
	if (p == LINUX_POLL_TABLE_NORMAL)
		selrecord(curthread, &filp->f_selinfo);

	switch (linux_poll_wakeup_state(&filp->f_wait_queue.state, state)) {
	case LINUX_FWQ_STATE_INIT:
		/* NOTE: file handles can only belong to one wait-queue */
		filp->f_wait_queue.wqh = wqh;
		filp->f_wait_queue.wq.func = &linux_poll_wakeup_callback;
		add_wait_queue(wqh, &filp->f_wait_queue.wq);
		atomic_set(&filp->f_wait_queue.state, LINUX_FWQ_STATE_QUEUED);
		break;
	default:
		break;
	}
}

static void
linux_poll_wait_dequeue(struct linux_file *filp)
{
	static const uint8_t state[LINUX_FWQ_STATE_MAX] = {
		[LINUX_FWQ_STATE_INIT] = LINUX_FWQ_STATE_INIT,	/* NOP */
		[LINUX_FWQ_STATE_NOT_READY] = LINUX_FWQ_STATE_INIT,
		[LINUX_FWQ_STATE_QUEUED] = LINUX_FWQ_STATE_INIT,
		[LINUX_FWQ_STATE_READY] = LINUX_FWQ_STATE_INIT,
	};

	seldrain(&filp->f_selinfo);

	switch (linux_poll_wakeup_state(&filp->f_wait_queue.state, state)) {
	case LINUX_FWQ_STATE_NOT_READY:
	case LINUX_FWQ_STATE_QUEUED:
	case LINUX_FWQ_STATE_READY:
		remove_wait_queue(filp->f_wait_queue.wqh, &filp->f_wait_queue.wq);
		break;
	default:
		break;
	}
}

void
linux_poll_wakeup(struct linux_file *filp)
{
	/* this function should be NULL-safe */
	if (filp == NULL)
		return;

	selwakeup(&filp->f_selinfo);

	spin_lock(&filp->f_kqlock);
	filp->f_kqflags |= LINUX_KQ_FLAG_NEED_READ |
	    LINUX_KQ_FLAG_NEED_WRITE;

	/* make sure the "knote" gets woken up */
	KNOTE_LOCKED(&filp->f_selinfo.si_note, 1);
	spin_unlock(&filp->f_kqlock);
}

static struct linux_file *
__get_file_rcu(struct linux_file **f)
{
	struct linux_file *file1, *file2;

	file1 = READ_ONCE(*f);
	if (file1 == NULL)
		return (NULL);

	if (!refcount_acquire_if_not_zero(
	    file1->_file == NULL ? &file1->f_count : &file1->_file->f_count))
		return (ERR_PTR(-EAGAIN));

	file2 = READ_ONCE(*f);
	if (file2 == file1)
		return (file2);

	fput(file1);
	return (ERR_PTR(-EAGAIN));
}

struct linux_file *
linux_get_file_rcu(struct linux_file **f)
{
	struct linux_file *file1;

	for (;;) {
		file1 = __get_file_rcu(f);
		if (file1 == NULL)
			return (NULL);

		if (IS_ERR(file1))
			continue;

		return (file1);
	}
}

struct linux_file *
get_file_active(struct linux_file **f)
{
	struct linux_file *file1;

	rcu_read_lock();
	file1 = __get_file_rcu(f);
	rcu_read_unlock();
	if (IS_ERR(file1))
		file1 = NULL;

	return (file1);
}

static void
linux_file_kqfilter_detach(struct knote *kn)
{
	struct linux_file *filp = kn->kn_hook;

	spin_lock(&filp->f_kqlock);
	knlist_remove(&filp->f_selinfo.si_note, kn, 1);
	spin_unlock(&filp->f_kqlock);
}

static int
linux_file_kqfilter_read_event(struct knote *kn, long hint)
{
	struct linux_file *filp = kn->kn_hook;

	mtx_assert(&filp->f_kqlock, MA_OWNED);

	return ((filp->f_kqflags & LINUX_KQ_FLAG_NEED_READ) ? 1 : 0);
}

static int
linux_file_kqfilter_write_event(struct knote *kn, long hint)
{
	struct linux_file *filp = kn->kn_hook;

	mtx_assert(&filp->f_kqlock, MA_OWNED);

	return ((filp->f_kqflags & LINUX_KQ_FLAG_NEED_WRITE) ? 1 : 0);
}

static const struct filterops linux_dev_kqfiltops_read = {
	.f_isfd = 1,
	.f_detach = linux_file_kqfilter_detach,
	.f_event = linux_file_kqfilter_read_event,
};

static const struct filterops linux_dev_kqfiltops_write = {
	.f_isfd = 1,
	.f_detach = linux_file_kqfilter_detach,
	.f_event = linux_file_kqfilter_write_event,
};

static void
linux_file_kqfilter_poll(struct linux_file *filp, int kqflags)
{
	struct thread *td;
	const struct file_operations *fop;
	struct linux_cdev *ldev;
	int temp;

	if ((filp->f_kqflags & kqflags) == 0)
		return;

	td = curthread;

	linux_get_fop(filp, &fop, &ldev);
	/* get the latest polling state */
	temp = OPW(filp->_file, td, fop->poll(filp, NULL));
	linux_drop_fop(ldev);

	spin_lock(&filp->f_kqlock);
	/* clear kqflags */
	filp->f_kqflags &= ~(LINUX_KQ_FLAG_NEED_READ |
	    LINUX_KQ_FLAG_NEED_WRITE);
	/* update kqflags */
	if ((temp & (POLLIN | POLLOUT)) != 0) {
		if ((temp & POLLIN) != 0)
			filp->f_kqflags |= LINUX_KQ_FLAG_NEED_READ;
		if ((temp & POLLOUT) != 0)
			filp->f_kqflags |= LINUX_KQ_FLAG_NEED_WRITE;

		/* make sure the "knote" gets woken up */
		KNOTE_LOCKED(&filp->f_selinfo.si_note, 0);
	}
	spin_unlock(&filp->f_kqlock);
}

static int
linux_file_kqfilter(struct file *file, struct knote *kn)
{
	struct linux_file *filp;
	struct thread *td;
	int error;

	td = curthread;
	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	if (filp->f_op->poll == NULL)
		return (EINVAL);

	spin_lock(&filp->f_kqlock);
	switch (kn->kn_filter) {
	case EVFILT_READ:
		filp->f_kqflags |= LINUX_KQ_FLAG_HAS_READ;
		kn->kn_fop = &linux_dev_kqfiltops_read;
		kn->kn_hook = filp;
		knlist_add(&filp->f_selinfo.si_note, kn, 1);
		error = 0;
		break;
	case EVFILT_WRITE:
		filp->f_kqflags |= LINUX_KQ_FLAG_HAS_WRITE;
		kn->kn_fop = &linux_dev_kqfiltops_write;
		kn->kn_hook = filp;
		knlist_add(&filp->f_selinfo.si_note, kn, 1);
		error = 0;
		break;
	default:
		error = EINVAL;
		break;
	}
	spin_unlock(&filp->f_kqlock);

	if (error == 0) {
		linux_set_current(td);

		/* update kqfilter status, if any */
		linux_file_kqfilter_poll(filp,
		    LINUX_KQ_FLAG_HAS_READ | LINUX_KQ_FLAG_HAS_WRITE);
	}
	return (error);
}

static int
linux_file_mmap_single(struct file *fp, const struct file_operations *fop,
    vm_ooffset_t *offset, vm_size_t size, struct vm_object **object,
    int nprot, bool is_shared, struct thread *td)
{
	struct task_struct *task;
	struct vm_area_struct *vmap;
	struct mm_struct *mm;
	struct linux_file *filp;
	vm_memattr_t attr;
	int error;

	filp = (struct linux_file *)fp->f_data;
	filp->f_flags = fp->f_flag;

	if (fop->mmap == NULL)
		return (EOPNOTSUPP);

	linux_set_current(td);

	/*
	 * The same VM object might be shared by multiple processes
	 * and the mm_struct is usually freed when a process exits.
	 *
	 * The atomic reference below makes sure the mm_struct is
	 * available as long as the vmap is in the linux_vma_head.
	 */
	task = current;
	mm = task->mm;
	if (atomic_inc_not_zero(&mm->mm_users) == 0)
		return (EINVAL);

	vmap = kzalloc(sizeof(*vmap), GFP_KERNEL);
	vmap->vm_start = 0;
	vmap->vm_end = size;
	vmap->vm_pgoff = *offset / PAGE_SIZE;
	vmap->vm_pfn = 0;
	vmap->vm_flags = vmap->vm_page_prot = (nprot & VM_PROT_ALL);
	if (is_shared)
		vmap->vm_flags |= VM_SHARED;
	vmap->vm_ops = NULL;
	vmap->vm_file = get_file(filp);
	vmap->vm_mm = mm;

	if (unlikely(down_write_killable(&vmap->vm_mm->mmap_sem))) {
		error = linux_get_error(task, EINTR);
	} else {
		error = -OPW(fp, td, fop->mmap(filp, vmap));
		error = linux_get_error(task, error);
		up_write(&vmap->vm_mm->mmap_sem);
	}

	if (error != 0) {
		linux_cdev_handle_free(vmap);
		return (error);
	}

	attr = pgprot2cachemode(vmap->vm_page_prot);

	if (vmap->vm_ops != NULL) {
		struct vm_area_struct *ptr;
		void *vm_private_data;
		bool vm_no_fault;

		if (vmap->vm_ops->open == NULL ||
		    vmap->vm_ops->close == NULL ||
		    vmap->vm_private_data == NULL) {
			/* free allocated VM area struct */
			linux_cdev_handle_free(vmap);
			return (EINVAL);
		}

		vm_private_data = vmap->vm_private_data;

		rw_wlock(&linux_vma_lock);
		TAILQ_FOREACH(ptr, &linux_vma_head, vm_entry) {
			if (ptr->vm_private_data == vm_private_data)
				break;
		}
		/* check if there is an existing VM area struct */
		if (ptr != NULL) {
			/* check if the VM area structure is invalid */
			if (ptr->vm_ops == NULL ||
			    ptr->vm_ops->open == NULL ||
			    ptr->vm_ops->close == NULL) {
				error = ESTALE;
				vm_no_fault = 1;
			} else {
				error = EEXIST;
				vm_no_fault = (ptr->vm_ops->fault == NULL);
			}
		} else {
			/* insert VM area structure into list */
			TAILQ_INSERT_TAIL(&linux_vma_head, vmap, vm_entry);
			error = 0;
			vm_no_fault = (vmap->vm_ops->fault == NULL);
		}
		rw_wunlock(&linux_vma_lock);

		if (error != 0) {
			/* free allocated VM area struct */
			linux_cdev_handle_free(vmap);
			/* check for stale VM area struct */
			if (error != EEXIST)
				return (error);
		}

		/* check if there is no fault handler */
		if (vm_no_fault) {
			*object = cdev_pager_allocate(vm_private_data, OBJT_DEVICE,
			    &linux_cdev_pager_ops[1], size, nprot, *offset,
			    td->td_ucred);
		} else {
			*object = cdev_pager_allocate(vm_private_data, OBJT_MGTDEVICE,
			    &linux_cdev_pager_ops[0], size, nprot, *offset,
			    td->td_ucred);
		}

		/* check if allocating the VM object failed */
		if (*object == NULL) {
			if (error == 0) {
				/* remove VM area struct from list */
				linux_cdev_handle_remove(vmap);
				/* free allocated VM area struct */
				linux_cdev_handle_free(vmap);
			}
			return (EINVAL);
		}
	} else {
		struct sglist *sg;

		sg = sglist_alloc(1, M_WAITOK);
		sglist_append_phys(sg,
		    (vm_paddr_t)vmap->vm_pfn << PAGE_SHIFT, vmap->vm_len);

		*object = vm_pager_allocate(OBJT_SG, sg, vmap->vm_len,
		    nprot, 0, td->td_ucred);

		linux_cdev_handle_free(vmap);

		if (*object == NULL) {
			sglist_free(sg);
			return (EINVAL);
		}
	}

	if (attr != VM_MEMATTR_DEFAULT) {
		VM_OBJECT_WLOCK(*object);
		vm_object_set_memattr(*object, attr);
		VM_OBJECT_WUNLOCK(*object);
	}
	*offset = 0;
	return (0);
}

struct cdevsw linuxcdevsw = {
	.d_version = D_VERSION,
	.d_fdopen = linux_dev_fdopen,
	.d_name = "lkpidev",
};

static int
linux_file_read(struct file *file, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct linux_file *filp;
	const struct file_operations *fop;
	struct linux_cdev *ldev;
	ssize_t bytes;
	int error;

	error = 0;
	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	/* XXX no support for I/O vectors currently */
	if (uio->uio_iovcnt != 1)
		return (EOPNOTSUPP);
	if (uio->uio_resid > DEVFS_IOSIZE_MAX)
		return (EINVAL);
	linux_set_current(td);
	linux_get_fop(filp, &fop, &ldev);
	if (fop->read != NULL) {
		bytes = OPW(file, td, fop->read(filp,
		    uio->uio_iov->iov_base,
		    uio->uio_iov->iov_len, &uio->uio_offset));
		if (bytes >= 0) {
			uio->uio_iov->iov_base =
			    ((uint8_t *)uio->uio_iov->iov_base) + bytes;
			uio->uio_iov->iov_len -= bytes;
			uio->uio_resid -= bytes;
		} else {
			error = linux_get_error(current, -bytes);
		}
	} else
		error = ENXIO;

	/* update kqfilter status, if any */
	linux_file_kqfilter_poll(filp, LINUX_KQ_FLAG_HAS_READ);
	linux_drop_fop(ldev);

	return (error);
}

static int
linux_file_write(struct file *file, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct linux_file *filp;
	const struct file_operations *fop;
	struct linux_cdev *ldev;
	ssize_t bytes;
	int error;

	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	/* XXX no support for I/O vectors currently */
	if (uio->uio_iovcnt != 1)
		return (EOPNOTSUPP);
	if (uio->uio_resid > DEVFS_IOSIZE_MAX)
		return (EINVAL);
	linux_set_current(td);
	linux_get_fop(filp, &fop, &ldev);
	if (fop->write != NULL) {
		bytes = OPW(file, td, fop->write(filp,
		    uio->uio_iov->iov_base,
		    uio->uio_iov->iov_len, &uio->uio_offset));
		if (bytes >= 0) {
			uio->uio_iov->iov_base =
			    ((uint8_t *)uio->uio_iov->iov_base) + bytes;
			uio->uio_iov->iov_len -= bytes;
			uio->uio_resid -= bytes;
			error = 0;
		} else {
			error = linux_get_error(current, -bytes);
		}
	} else
		error = ENXIO;

	/* update kqfilter status, if any */
	linux_file_kqfilter_poll(filp, LINUX_KQ_FLAG_HAS_WRITE);

	linux_drop_fop(ldev);

	return (error);
}

static int
linux_file_poll(struct file *file, int events, struct ucred *active_cred,
    struct thread *td)
{
	struct linux_file *filp;
	const struct file_operations *fop;
	struct linux_cdev *ldev;
	int revents;

	filp = (struct linux_file *)file->f_data;
	filp->f_flags = file->f_flag;
	linux_set_current(td);
	linux_get_fop(filp, &fop, &ldev);
	if (fop->poll != NULL) {
		revents = OPW(file, td, fop->poll(filp,
		    LINUX_POLL_TABLE_NORMAL)) & events;
	} else {
		revents = 0;
	}
	linux_drop_fop(ldev);
	return (revents);
}

static int
linux_file_close(struct file *file, struct thread *td)
{
	struct linux_file *filp;
	int (*release)(struct inode *, struct linux_file *);
	const struct file_operations *fop;
	struct linux_cdev *ldev;
	int error;

	filp = (struct linux_file *)file->f_data;

	KASSERT(file_count(filp) == 0,
	    ("File refcount(%d) is not zero", file_count(filp)));

	if (td == NULL)
		td = curthread;

	error = 0;
	filp->f_flags = file->f_flag;
	linux_set_current(td);
	linux_poll_wait_dequeue(filp);
	linux_get_fop(filp, &fop, &ldev);
	/*
	 * Always use the real release function, if any, to avoid
	 * leaking device resources:
	 */
	release = filp->f_op->release;
	if (release != NULL)
		error = -OPW(file, td, release(filp->f_vnode, filp));
	funsetown(&filp->f_sigio);
	if (filp->f_vnode != NULL)
		vrele(filp->f_vnode);
	linux_drop_fop(ldev);
	ldev = filp->f_cdev;
	if (ldev != NULL)
		linux_cdev_deref(ldev);
	linux_synchronize_rcu(RCU_TYPE_REGULAR);
	kfree(filp);

	return (error);
}

static int
linux_file_ioctl(struct file *fp, u_long cmd, void *data, struct ucred *cred,
    struct thread *td)
{
	struct linux_file *filp;
	const struct file_operations *fop;
	struct linux_cdev *ldev;
	struct fiodgname_arg *fgn;
	const char *p;
	int error, i;

	error = 0;
	filp = (struct linux_file *)fp->f_data;
	filp->f_flags = fp->f_flag;
	linux_get_fop(filp, &fop, &ldev);

	linux_set_current(td);
	switch (cmd) {
	case FIONBIO:
		break;
	case FIOASYNC:
		if (fop->fasync == NULL)
			break;
		error = -OPW(fp, td, fop->fasync(0, filp, fp->f_flag & FASYNC));
		break;
	case FIOSETOWN:
		error = fsetown(*(int *)data, &filp->f_sigio);
		if (error == 0) {
			if (fop->fasync == NULL)
				break;
			error = -OPW(fp, td, fop->fasync(0, filp,
			    fp->f_flag & FASYNC));
		}
		break;
	case FIOGETOWN:
		*(int *)data = fgetown(&filp->f_sigio);
		break;
	case FIODGNAME:
#ifdef	COMPAT_FREEBSD32
	case FIODGNAME_32:
#endif
		if (filp->f_cdev == NULL || filp->f_cdev->cdev == NULL) {
			error = ENXIO;
			break;
		}
		fgn = data;
		p = devtoname(filp->f_cdev->cdev);
		i = strlen(p) + 1;
		if (i > fgn->len) {
			error = EINVAL;
			break;
		}
		error = copyout(p, fiodgname_buf_get_ptr(fgn, cmd), i);
		break;
	default:
		error = linux_file_ioctl_sub(fp, filp, fop, cmd, data, td);
		break;
	}
	linux_drop_fop(ldev);
	return (error);
}

static int
linux_file_mmap_sub(struct thread *td, vm_size_t objsize, vm_prot_t prot,
    vm_prot_t maxprot, int flags, struct file *fp,
    vm_ooffset_t *foff, const struct file_operations *fop, vm_object_t *objp)
{
	/*
	 * Character devices do not provide private mappings
	 * of any kind:
	 */
	if ((maxprot & VM_PROT_WRITE) == 0 &&
	    (prot & VM_PROT_WRITE) != 0)
		return (EACCES);
	if ((flags & (MAP_PRIVATE | MAP_COPY)) != 0)
		return (EINVAL);

	return (linux_file_mmap_single(fp, fop, foff, objsize, objp,
	    (int)prot, (flags & MAP_SHARED) ? true : false, td));
}

static int
linux_file_mmap(struct file *fp, vm_map_t map, vm_offset_t *addr, vm_size_t size,
    vm_prot_t prot, vm_prot_t cap_maxprot, int flags, vm_ooffset_t foff,
    struct thread *td)
{
	struct linux_file *filp;
	const struct file_operations *fop;
	struct linux_cdev *ldev;
	struct mount *mp;
	struct vnode *vp;
	vm_object_t object;
	vm_prot_t maxprot;
	int error;

	filp = (struct linux_file *)fp->f_data;

	vp = filp->f_vnode;
	if (vp == NULL)
		return (EOPNOTSUPP);

	/*
	 * Ensure that file and memory protections are
	 * compatible.
	 */
	mp = vp->v_mount;
	if (mp != NULL && (mp->mnt_flag & MNT_NOEXEC) != 0) {
		maxprot = VM_PROT_NONE;
		if ((prot & VM_PROT_EXECUTE) != 0)
			return (EACCES);
	} else
		maxprot = VM_PROT_EXECUTE;
	if ((fp->f_flag & FREAD) != 0)
		maxprot |= VM_PROT_READ;
	else if ((prot & VM_PROT_READ) != 0)
		return (EACCES);

	/*
	 * If we are sharing potential changes via MAP_SHARED and we
	 * are trying to get write permission although we opened it
	 * without asking for it, bail out.
	 *
	 * Note that most character devices always share mappings.
	 *
	 * Rely on linux_file_mmap_sub() to fail invalid MAP_PRIVATE
	 * requests rather than doing it here.
	 */
	if ((flags & MAP_SHARED) != 0) {
		if ((fp->f_flag & FWRITE) != 0)
			maxprot |= VM_PROT_WRITE;
		else if ((prot & VM_PROT_WRITE) != 0)
			return (EACCES);
	}
	maxprot &= cap_maxprot;

	linux_get_fop(filp, &fop, &ldev);
	error = linux_file_mmap_sub(td, size, prot, maxprot, flags, fp,
	    &foff, fop, &object);
	if (error != 0)
		goto out;

	error = vm_mmap_object(map, addr, size, prot, maxprot, flags, object,
	    foff, FALSE, td);
	if (error != 0)
		vm_object_deallocate(object);
out:
	linux_drop_fop(ldev);
	return (error);
}

static int
linux_file_stat(struct file *fp, struct stat *sb, struct ucred *active_cred)
{
	struct linux_file *filp;
	struct vnode *vp;
	int error;

	filp = (struct linux_file *)fp->f_data;
	if (filp->f_vnode == NULL)
		return (EOPNOTSUPP);

	vp = filp->f_vnode;

	vn_lock(vp, LK_SHARED | LK_RETRY);
	error = VOP_STAT(vp, sb, curthread->td_ucred, NOCRED);
	VOP_UNLOCK(vp);

	return (error);
}

static int
linux_file_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp)
{
	struct linux_file *filp;
	struct vnode *vp;
	int error;

	filp = fp->f_data;
	vp = filp->f_vnode;
	if (vp == NULL) {
		error = 0;
		kif->kf_type = KF_TYPE_DEV;
	} else {
		vref(vp);
		FILEDESC_SUNLOCK(fdp);
		error = vn_fill_kinfo_vnode(vp, kif);
		vrele(vp);
		kif->kf_type = KF_TYPE_VNODE;
		FILEDESC_SLOCK(fdp);
	}
	return (error);
}

unsigned int
linux_iminor(struct inode *inode)
{
	struct linux_cdev *ldev;

	if (inode == NULL || inode->v_rdev == NULL ||
	    inode->v_rdev->si_devsw != &linuxcdevsw)
		return (-1U);
	ldev = inode->v_rdev->si_drv1;
	if (ldev == NULL)
		return (-1U);

	return (minor(ldev->dev));
}

static int
linux_file_kcmp(struct file *fp1, struct file *fp2, struct thread *td)
{
	struct linux_file *filp1, *filp2;

	if (fp2->f_type != DTYPE_DEV)
		return (3);

	filp1 = fp1->f_data;
	filp2 = fp2->f_data;
	return (kcmp_cmp((uintptr_t)filp1->f_cdev, (uintptr_t)filp2->f_cdev));
}

const struct fileops linuxfileops = {
	.fo_read = linux_file_read,
	.fo_write = linux_file_write,
	.fo_truncate = invfo_truncate,
	.fo_kqfilter = linux_file_kqfilter,
	.fo_stat = linux_file_stat,
	.fo_fill_kinfo = linux_file_fill_kinfo,
	.fo_poll = linux_file_poll,
	.fo_close = linux_file_close,
	.fo_ioctl = linux_file_ioctl,
	.fo_mmap = linux_file_mmap,
	.fo_chmod = invfo_chmod,
	.fo_chown = invfo_chown,
	.fo_sendfile = invfo_sendfile,
	.fo_cmp = linux_file_kcmp,
	.fo_flags = DFLAG_PASSABLE,
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

#if defined(__i386__) || defined(__amd64__) || defined(__powerpc__) || defined(__aarch64__) || defined(__riscv)
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
#if defined(__i386__) || defined(__amd64__) || defined(__powerpc__) || defined(__aarch64__) || defined(__riscv)
	pmap_unmapdev(addr, vmmap->vm_size);
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

static char *
devm_kvasprintf(struct device *dev, gfp_t gfp, const char *fmt, va_list ap)
{
	unsigned int len;
	char *p;
	va_list aq;

	va_copy(aq, ap);
	len = vsnprintf(NULL, 0, fmt, aq);
	va_end(aq);

	if (dev != NULL)
		p = devm_kmalloc(dev, len + 1, gfp);
	else
		p = kmalloc(len + 1, gfp);
	if (p != NULL)
		vsnprintf(p, len + 1, fmt, ap);

	return (p);
}

char *
kvasprintf(gfp_t gfp, const char *fmt, va_list ap)
{

	return (devm_kvasprintf(NULL, gfp, fmt, ap));
}

char *
lkpi_devm_kasprintf(struct device *dev, gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = devm_kvasprintf(dev, gfp, fmt, ap);
	va_end(ap);

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

int
__lkpi_hexdump_printf(void *arg1 __unused, const char *fmt, ...)
{
	va_list ap;
	int result;

	va_start(ap, fmt);
	result = vprintf(fmt, ap);
	va_end(ap);
	return (result);
}

int
__lkpi_hexdump_sbuf_printf(void *arg1, const char *fmt, ...)
{
	va_list ap;
	int result;

	va_start(ap, fmt);
	result = sbuf_vprintf(arg1, fmt, ap);
	va_end(ap);
	return (result);
}

void
lkpi_hex_dump(int(*_fpf)(void *, const char *, ...), void *arg1,
    const char *level, const char *prefix_str,
    const int prefix_type, const int rowsize, const int groupsize,
    const void *buf, size_t len, const bool ascii)
{
	typedef const struct { long long value; } __packed *print_64p_t;
	typedef const struct { uint32_t value; } __packed *print_32p_t;
	typedef const struct { uint16_t value; } __packed *print_16p_t;
	const void *buf_old = buf;
	int row;

	while (len > 0) {
		if (level != NULL)
			_fpf(arg1, "%s", level);
		if (prefix_str != NULL)
			_fpf(arg1, "%s ", prefix_str);

		switch (prefix_type) {
		case DUMP_PREFIX_ADDRESS:
			_fpf(arg1, "[%p] ", buf);
			break;
		case DUMP_PREFIX_OFFSET:
			_fpf(arg1, "[%#tx] ", ((const char *)buf -
			    (const char *)buf_old));
			break;
		default:
			break;
		}
		for (row = 0; row != rowsize; row++) {
			if (groupsize == 8 && len > 7) {
				_fpf(arg1, "%016llx ", ((print_64p_t)buf)->value);
				buf = (const uint8_t *)buf + 8;
				len -= 8;
			} else if (groupsize == 4 && len > 3) {
				_fpf(arg1, "%08x ", ((print_32p_t)buf)->value);
				buf = (const uint8_t *)buf + 4;
				len -= 4;
			} else if (groupsize == 2 && len > 1) {
				_fpf(arg1, "%04x ", ((print_16p_t)buf)->value);
				buf = (const uint8_t *)buf + 2;
				len -= 2;
			} else if (len > 0) {
				_fpf(arg1, "%02x ", *(const uint8_t *)buf);
				buf = (const uint8_t *)buf + 1;
				len--;
			} else {
				break;
			}
		}
		_fpf(arg1, "\n");
	}
}

static void
linux_timer_callback_wrapper(void *context)
{
	struct timer_list *timer;

	timer = context;

	/* the timer is about to be shutdown permanently */
	if (timer->function == NULL)
		return;

	if (linux_set_current_flags(curthread, M_NOWAIT)) {
		/* try again later */
		callout_reset(&timer->callout, 1,
		    &linux_timer_callback_wrapper, timer);
		return;
	}

	timer->function(timer->data);
}

static int
linux_timer_jiffies_until(unsigned long expires)
{
	unsigned long delta = expires - jiffies;

	/*
	 * Guard against already expired values and make sure that the value can
	 * be used as a tick count, rather than a jiffies count.
	 */
	if ((long)delta < 1)
		delta = 1;
	else if (delta > INT_MAX)
		delta = INT_MAX;
	return ((int)delta);
}

int
mod_timer(struct timer_list *timer, unsigned long expires)
{
	int ret;

	timer->expires = expires;
	ret = callout_reset(&timer->callout,
	    linux_timer_jiffies_until(expires),
	    &linux_timer_callback_wrapper, timer);

	MPASS(ret == 0 || ret == 1);

	return (ret == 1);
}

void
add_timer(struct timer_list *timer)
{

	callout_reset(&timer->callout,
	    linux_timer_jiffies_until(timer->expires),
	    &linux_timer_callback_wrapper, timer);
}

void
add_timer_on(struct timer_list *timer, int cpu)
{

	callout_reset_on(&timer->callout,
	    linux_timer_jiffies_until(timer->expires),
	    &linux_timer_callback_wrapper, timer, cpu);
}

int
del_timer(struct timer_list *timer)
{

	if (callout_stop(&(timer)->callout) == -1)
		return (0);
	return (1);
}

int
del_timer_sync(struct timer_list *timer)
{

	if (callout_drain(&(timer)->callout) == -1)
		return (0);
	return (1);
}

int
timer_delete_sync(struct timer_list *timer)
{

	return (del_timer_sync(timer));
}

int
timer_shutdown_sync(struct timer_list *timer)
{

	timer->function = NULL;
	return (del_timer_sync(timer));
}

/* greatest common divisor, Euclid equation */
static uint64_t
lkpi_gcd_64(uint64_t a, uint64_t b)
{
	uint64_t an;
	uint64_t bn;

	while (b != 0) {
		an = b;
		bn = a % b;
		a = an;
		b = bn;
	}
	return (a);
}

uint64_t lkpi_nsec2hz_rem;
uint64_t lkpi_nsec2hz_div = 1000000000ULL;
uint64_t lkpi_nsec2hz_max;

uint64_t lkpi_usec2hz_rem;
uint64_t lkpi_usec2hz_div = 1000000ULL;
uint64_t lkpi_usec2hz_max;

uint64_t lkpi_msec2hz_rem;
uint64_t lkpi_msec2hz_div = 1000ULL;
uint64_t lkpi_msec2hz_max;

static void
linux_timer_init(void *arg)
{
	uint64_t gcd;

	/*
	 * Compute an internal HZ value which can divide 2**32 to
	 * avoid timer rounding problems when the tick value wraps
	 * around 2**32:
	 */
	linux_timer_hz_mask = 1;
	while (linux_timer_hz_mask < (unsigned long)hz)
		linux_timer_hz_mask *= 2;
	linux_timer_hz_mask--;

	/* compute some internal constants */

	lkpi_nsec2hz_rem = hz;
	lkpi_usec2hz_rem = hz;
	lkpi_msec2hz_rem = hz;

	gcd = lkpi_gcd_64(lkpi_nsec2hz_rem, lkpi_nsec2hz_div);
	lkpi_nsec2hz_rem /= gcd;
	lkpi_nsec2hz_div /= gcd;
	lkpi_nsec2hz_max = -1ULL / lkpi_nsec2hz_rem;

	gcd = lkpi_gcd_64(lkpi_usec2hz_rem, lkpi_usec2hz_div);
	lkpi_usec2hz_rem /= gcd;
	lkpi_usec2hz_div /= gcd;
	lkpi_usec2hz_max = -1ULL / lkpi_usec2hz_rem;

	gcd = lkpi_gcd_64(lkpi_msec2hz_rem, lkpi_msec2hz_div);
	lkpi_msec2hz_rem /= gcd;
	lkpi_msec2hz_div /= gcd;
	lkpi_msec2hz_max = -1ULL / lkpi_msec2hz_rem;
}
SYSINIT(linux_timer, SI_SUB_DRIVERS, SI_ORDER_FIRST, linux_timer_init, NULL);

void
linux_complete_common(struct completion *c, int all)
{
	sleepq_lock(c);
	if (all) {
		c->done = UINT_MAX;
		sleepq_broadcast(c, SLEEPQ_SLEEP, 0, 0);
	} else {
		if (c->done != UINT_MAX)
			c->done++;
		sleepq_signal(c, SLEEPQ_SLEEP, 0, 0);
	}
	sleepq_release(c);
}

/*
 * Indefinite wait for done != 0 with or without signals.
 */
int
linux_wait_for_common(struct completion *c, int flags)
{
	struct task_struct *task;
	int error;

	if (SCHEDULER_STOPPED())
		return (0);

	task = current;

	if (flags != 0)
		flags = SLEEPQ_INTERRUPTIBLE | SLEEPQ_SLEEP;
	else
		flags = SLEEPQ_SLEEP;
	error = 0;
	for (;;) {
		sleepq_lock(c);
		if (c->done)
			break;
		sleepq_add(c, NULL, "completion", flags, 0);
		if (flags & SLEEPQ_INTERRUPTIBLE) {
			DROP_GIANT();
			error = -sleepq_wait_sig(c, 0);
			PICKUP_GIANT();
			if (error != 0) {
				linux_schedule_save_interrupt_value(task, error);
				error = -ERESTARTSYS;
				goto intr;
			}
		} else {
			DROP_GIANT();
			sleepq_wait(c, 0);
			PICKUP_GIANT();
		}
	}
	if (c->done != UINT_MAX)
		c->done--;
	sleepq_release(c);

intr:
	return (error);
}

/*
 * Time limited wait for done != 0 with or without signals.
 */
unsigned long
linux_wait_for_timeout_common(struct completion *c, unsigned long timeout,
    int flags)
{
	struct task_struct *task;
	unsigned long end = jiffies + timeout, error;

	if (SCHEDULER_STOPPED())
		return (0);

	task = current;

	if (flags != 0)
		flags = SLEEPQ_INTERRUPTIBLE | SLEEPQ_SLEEP;
	else
		flags = SLEEPQ_SLEEP;

	for (;;) {
		sleepq_lock(c);
		if (c->done)
			break;
		sleepq_add(c, NULL, "completion", flags, 0);
		sleepq_set_timeout(c, linux_timer_jiffies_until(end));

		DROP_GIANT();
		if (flags & SLEEPQ_INTERRUPTIBLE)
			error = -sleepq_timedwait_sig(c, 0);
		else
			error = -sleepq_timedwait(c, 0);
		PICKUP_GIANT();

		if (error != 0) {
			/* check for timeout */
			if (error == -EWOULDBLOCK) {
				error = 0;	/* timeout */
			} else {
				/* signal happened */
				linux_schedule_save_interrupt_value(task, error);
				error = -ERESTARTSYS;
			}
			goto done;
		}
	}
	if (c->done != UINT_MAX)
		c->done--;
	sleepq_release(c);

	/* return how many jiffies are left */
	error = linux_timer_jiffies_until(end);
done:
	return (error);
}

int
linux_try_wait_for_completion(struct completion *c)
{
	int isdone;

	sleepq_lock(c);
	isdone = (c->done != 0);
	if (c->done != 0 && c->done != UINT_MAX)
		c->done--;
	sleepq_release(c);
	return (isdone);
}

int
linux_completion_done(struct completion *c)
{
	int isdone;

	sleepq_lock(c);
	isdone = (c->done != 0);
	sleepq_release(c);
	return (isdone);
}

static void
linux_cdev_deref(struct linux_cdev *ldev)
{
	if (refcount_release(&ldev->refs) &&
	    ldev->kobj.ktype == &linux_cdev_ktype)
		kfree(ldev);
}

static void
linux_cdev_release(struct kobject *kobj)
{
	struct linux_cdev *cdev;
	struct kobject *parent;

	cdev = container_of(kobj, struct linux_cdev, kobj);
	parent = kobj->parent;
	linux_destroy_dev(cdev);
	linux_cdev_deref(cdev);
	kobject_put(parent);
}

static void
linux_cdev_static_release(struct kobject *kobj)
{
	struct cdev *cdev;
	struct linux_cdev *ldev;

	ldev = container_of(kobj, struct linux_cdev, kobj);
	cdev = ldev->cdev;
	if (cdev != NULL) {
		destroy_dev(cdev);
		ldev->cdev = NULL;
	}
	kobject_put(kobj->parent);
}

int
linux_cdev_device_add(struct linux_cdev *ldev, struct device *dev)
{
	int ret;

	if (dev->devt != 0) {
		/* Set parent kernel object. */
		ldev->kobj.parent = &dev->kobj;

		/*
		 * Unlike Linux we require the kobject of the
		 * character device structure to have a valid name
		 * before calling this function:
		 */
		if (ldev->kobj.name == NULL)
			return (-EINVAL);

		ret = cdev_add(ldev, dev->devt, 1);
		if (ret)
			return (ret);
	}
	ret = device_add(dev);
	if (ret != 0 && dev->devt != 0)
		cdev_del(ldev);
	return (ret);
}

void
linux_cdev_device_del(struct linux_cdev *ldev, struct device *dev)
{
	device_del(dev);

	if (dev->devt != 0)
		cdev_del(ldev);
}

static void
linux_destroy_dev(struct linux_cdev *ldev)
{

	if (ldev->cdev == NULL)
		return;

	MPASS((ldev->siref & LDEV_SI_DTR) == 0);
	MPASS(ldev->kobj.ktype == &linux_cdev_ktype);

	atomic_set_int(&ldev->siref, LDEV_SI_DTR);
	while ((atomic_load_int(&ldev->siref) & ~LDEV_SI_DTR) != 0)
		pause("ldevdtr", hz / 4);

	destroy_dev(ldev->cdev);
	ldev->cdev = NULL;
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
	struct netdev_notifier_info ni;

	nb = arg;
	ni.ifp = ifp;
	ni.dev = (struct net_device *)ifp;
	if (linkstate == LINK_STATE_UP)
		nb->notifier_call(nb, NETDEV_UP, &ni);
	else
		nb->notifier_call(nb, NETDEV_DOWN, &ni);
}

static void
linux_handle_ifnet_arrival_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;
	struct netdev_notifier_info ni;

	nb = arg;
	ni.ifp = ifp;
	ni.dev = (struct net_device *)ifp;
	nb->notifier_call(nb, NETDEV_REGISTER, &ni);
}

static void
linux_handle_ifnet_departure_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;
	struct netdev_notifier_info ni;

	nb = arg;
	ni.ifp = ifp;
	ni.dev = (struct net_device *)ifp;
	nb->notifier_call(nb, NETDEV_UNREGISTER, &ni);
}

static void
linux_handle_iflladdr_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;
	struct netdev_notifier_info ni;

	nb = arg;
	ni.ifp = ifp;
	ni.dev = (struct net_device *)ifp;
	nb->notifier_call(nb, NETDEV_CHANGEADDR, &ni);
}

static void
linux_handle_ifaddr_event(void *arg, struct ifnet *ifp)
{
	struct notifier_block *nb;
	struct netdev_notifier_info ni;

	nb = arg;
	ni.ifp = ifp;
	ni.dev = (struct net_device *)ifp;
	nb->notifier_call(nb, NETDEV_CHANGEIFADDR, &ni);
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
linux_le_cmp(const void *d1, const void *d2, void *priv)
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
	qsort_r(ar, count, sizeof(struct list_head *), linux_le_cmp, &thunk);
	INIT_LIST_HEAD(head);
	for (i = 0; i < count; i++)
		list_add_tail(ar[i], head);
	free(ar, M_KMALLOC);
}

#if defined(__i386__) || defined(__amd64__)
int
linux_wbinvd_on_all_cpus(void)
{

	pmap_invalidate_cache();
	return (0);
}
#endif

int
linux_on_each_cpu(void callback(void *), void *data)
{

	smp_rendezvous(smp_no_rendezvous_barrier, callback,
	    smp_no_rendezvous_barrier, data);
	return (0);
}

int
linux_in_atomic(void)
{

	return ((curthread->td_pflags & TDP_NOFAULTING) != 0);
}

struct linux_cdev *
linux_find_cdev(const char *name, unsigned major, unsigned minor)
{
	dev_t dev = MKDEV(major, minor);
	struct cdev *cdev;

	dev_lock();
	LIST_FOREACH(cdev, &linuxcdevsw.d_devs, si_list) {
		struct linux_cdev *ldev = cdev->si_drv1;
		if (ldev->dev == dev &&
		    strcmp(kobject_name(&ldev->kobj), name) == 0) {
			break;
		}
	}
	dev_unlock();

	return (cdev != NULL ? cdev->si_drv1 : NULL);
}

int
__register_chrdev(unsigned int major, unsigned int baseminor,
    unsigned int count, const char *name,
    const struct file_operations *fops)
{
	struct linux_cdev *cdev;
	int ret = 0;
	int i;

	for (i = baseminor; i < baseminor + count; i++) {
		cdev = cdev_alloc();
		cdev->ops = fops;
		kobject_set_name(&cdev->kobj, name);

		ret = cdev_add(cdev, makedev(major, i), 1);
		if (ret != 0)
			break;
	}
	return (ret);
}

int
__register_chrdev_p(unsigned int major, unsigned int baseminor,
    unsigned int count, const char *name,
    const struct file_operations *fops, uid_t uid,
    gid_t gid, int mode)
{
	struct linux_cdev *cdev;
	int ret = 0;
	int i;

	for (i = baseminor; i < baseminor + count; i++) {
		cdev = cdev_alloc();
		cdev->ops = fops;
		kobject_set_name(&cdev->kobj, name);

		ret = cdev_add_ext(cdev, makedev(major, i), uid, gid, mode);
		if (ret != 0)
			break;
	}
	return (ret);
}

void
__unregister_chrdev(unsigned int major, unsigned int baseminor,
    unsigned int count, const char *name)
{
	struct linux_cdev *cdevp;
	int i;

	for (i = baseminor; i < baseminor + count; i++) {
		cdevp = linux_find_cdev(name, major, i);
		if (cdevp != NULL)
			cdev_del(cdevp);
	}
}

void
linux_dump_stack(void)
{
#ifdef STACK
	struct stack st;

	stack_save(&st);
	stack_print(&st);
#endif
}

int
linuxkpi_net_ratelimit(void)
{

	return (ppsratecheck(&lkpi_net_lastlog, &lkpi_net_curpps,
	   lkpi_net_maxpps));
}

struct io_mapping *
io_mapping_create_wc(resource_size_t base, unsigned long size)
{
	struct io_mapping *mapping;

	mapping = kmalloc(sizeof(*mapping), GFP_KERNEL);
	if (mapping == NULL)
		return (NULL);
	return (io_mapping_init_wc(mapping, base, size));
}

/* We likely want a linuxkpi_device.c at some point. */
bool
device_can_wakeup(struct device *dev)
{

	if (dev == NULL)
		return (false);
	/*
	 * XXX-BZ iwlwifi queries it as part of enabling WoWLAN.
	 * Normally this would be based on a bool in dev->power.XXX.
	 * Check such as PCI PCIM_PCAP_*PME.  We have no way to enable this yet.
	 * We may get away by directly calling into bsddev for as long as
	 * we can assume PCI only avoiding changing struct device breaking KBI.
	 */
	pr_debug("%s:%d: not enabled; see comment.\n", __func__, __LINE__);
	return (false);
}

static void
devm_device_group_remove(struct device *dev, void *p)
{
	const struct attribute_group **dr = p;
	const struct attribute_group *group = *dr;

	sysfs_remove_group(&dev->kobj, group);
}

int
lkpi_devm_device_add_group(struct device *dev,
    const struct attribute_group *group)
{
	const struct attribute_group **dr;
	int ret;

	dr = devres_alloc(devm_device_group_remove, sizeof(*dr), GFP_KERNEL);
	if (dr == NULL)
		return (-ENOMEM);

	ret = sysfs_create_group(&dev->kobj, group);
	if (ret == 0) {
		*dr = group;
		devres_add(dev, dr);
	} else
		devres_free(dr);

	return (ret);
}

#if defined(__i386__) || defined(__amd64__)
bool linux_cpu_has_clflush;
struct cpuinfo_x86 boot_cpu_data;
struct cpuinfo_x86 *__cpu_data;
#endif

cpumask_t *
lkpi_get_static_single_cpu_mask(int cpuid)
{

	KASSERT((cpuid >= 0 && cpuid <= mp_maxid), ("%s: invalid cpuid %d\n",
	    __func__, cpuid));
	KASSERT(!CPU_ABSENT(cpuid), ("%s: cpu with cpuid %d is absent\n",
	    __func__, cpuid));

	return (static_single_cpu_mask[cpuid]);
}

bool
lkpi_xen_initial_domain(void)
{
#ifdef XENHVM
	return (xen_initial_domain());
#else
	return (false);
#endif
}

bool
lkpi_xen_pv_domain(void)
{
#ifdef XENHVM
	return (xen_pv_domain());
#else
	return (false);
#endif
}

static void
linux_compat_init(void *arg)
{
	struct sysctl_oid *rootoid;
	int i;

#if defined(__i386__) || defined(__amd64__)
	static const uint32_t x86_vendors[X86_VENDOR_NUM] = {
		[X86_VENDOR_INTEL] = CPU_VENDOR_INTEL,
		[X86_VENDOR_CYRIX] = CPU_VENDOR_CYRIX,
		[X86_VENDOR_AMD] = CPU_VENDOR_AMD,
		[X86_VENDOR_UMC] = CPU_VENDOR_UMC,
		[X86_VENDOR_CENTAUR] = CPU_VENDOR_CENTAUR,
		[X86_VENDOR_TRANSMETA] = CPU_VENDOR_TRANSMETA,
		[X86_VENDOR_NSC] = CPU_VENDOR_NSC,
		[X86_VENDOR_HYGON] = CPU_VENDOR_HYGON,
	};
	uint8_t x86_vendor = X86_VENDOR_UNKNOWN;

	for (i = 0; i < X86_VENDOR_NUM; i++) {
		if (cpu_vendor_id != 0 && cpu_vendor_id == x86_vendors[i]) {
			x86_vendor = i;
			break;
		}
	}
	linux_cpu_has_clflush = (cpu_feature & CPUID_CLFSH);
	boot_cpu_data.x86_clflush_size = cpu_clflush_line_size;
	boot_cpu_data.x86_max_cores = mp_ncpus;
	boot_cpu_data.x86 = CPUID_TO_FAMILY(cpu_id);
	boot_cpu_data.x86_model = CPUID_TO_MODEL(cpu_id);
	boot_cpu_data.x86_vendor = x86_vendor;

	__cpu_data = kmalloc_array(mp_maxid + 1,
	    sizeof(*__cpu_data), M_WAITOK | M_ZERO);
	CPU_FOREACH(i) {
		__cpu_data[i].x86_clflush_size = cpu_clflush_line_size;
		__cpu_data[i].x86_max_cores = mp_ncpus;
		__cpu_data[i].x86 = CPUID_TO_FAMILY(cpu_id);
		__cpu_data[i].x86_model = CPUID_TO_MODEL(cpu_id);
		__cpu_data[i].x86_vendor = x86_vendor;
	}
#endif
	rw_init(&linux_vma_lock, "lkpi-vma-lock");

	rootoid = SYSCTL_ADD_ROOT_NODE(NULL,
	    OID_AUTO, "sys", CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, "sys");
	kobject_init(&linux_class_root, &linux_class_ktype);
	kobject_set_name(&linux_class_root, "class");
	linux_class_root.oidp = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(rootoid),
	    OID_AUTO, "class", CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, "class");
	kobject_init(&linux_root_device.kobj, &linux_dev_ktype);
	kobject_set_name(&linux_root_device.kobj, "device");
	linux_root_device.kobj.oidp = SYSCTL_ADD_NODE(NULL,
	    SYSCTL_CHILDREN(rootoid), OID_AUTO, "device",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "device");
	linux_root_device.bsddev = root_bus;
	linux_class_misc.name = "misc";
	class_register(&linux_class_misc);
	INIT_LIST_HEAD(&pci_drivers);
	INIT_LIST_HEAD(&pci_devices);
	spin_lock_init(&pci_lock);
	mtx_init(&vmmaplock, "IO Map lock", NULL, MTX_DEF);
	for (i = 0; i < VMMAP_HASH_SIZE; i++)
		LIST_INIT(&vmmaphead[i]);
	init_waitqueue_head(&linux_bit_waitq);
	init_waitqueue_head(&linux_var_waitq);

	CPU_COPY(&all_cpus, &cpu_online_mask);
	/*
	 * Generate a single-CPU cpumask_t for each CPU (possibly) in the system.
	 * CPUs are indexed from 0..(mp_maxid).  The entry for cpuid 0 will only
	 * have itself in the cpumask, cupid 1 only itself on entry 1, and so on.
	 * This is used by cpumask_of() (and possibly others in the future) for,
	 * e.g., drivers to pass hints to irq_set_affinity_hint().
	 */
	static_single_cpu_mask = kmalloc_array(mp_maxid + 1,
	    sizeof(static_single_cpu_mask), M_WAITOK | M_ZERO);

	/*
	 * When the number of CPUs reach a threshold, we start to save memory
	 * given the sets are static by overlapping those having their single
	 * bit set at same position in a bitset word.  Asymptotically, this
	 * regular scheme is in O(n²) whereas the overlapping one is in O(n)
	 * only with n being the maximum number of CPUs, so the gain will become
	 * huge quite quickly.  The threshold for 64-bit architectures is 128
	 * CPUs.
	 */
	if (mp_ncpus < (2 * _BITSET_BITS)) {
		cpumask_t *sscm_ptr;

		/*
		 * This represents 'mp_ncpus * __bitset_words(CPU_SETSIZE) *
		 * (_BITSET_BITS / 8)' bytes (for comparison with the
		 * overlapping scheme).
		 */
		static_single_cpu_mask_lcs = kmalloc_array(mp_ncpus,
		    sizeof(*static_single_cpu_mask_lcs),
		    M_WAITOK | M_ZERO);

		sscm_ptr = static_single_cpu_mask_lcs;
		CPU_FOREACH(i) {
			static_single_cpu_mask[i] = sscm_ptr++;
			CPU_SET(i, static_single_cpu_mask[i]);
		}
	} else {
		/* Pointer to a bitset word. */
		__typeof(((cpuset_t *)NULL)->__bits[0]) *bwp;

		/*
		 * Allocate memory for (static) spans of 'cpumask_t' ('cpuset_t'
		 * really) with a single bit set that can be reused for all
		 * single CPU masks by making them start at different offsets.
		 * We need '__bitset_words(CPU_SETSIZE) - 1' bitset words before
		 * the word having its single bit set, and the same amount
		 * after.
		 */
		static_single_cpu_mask_lcs = mallocarray(_BITSET_BITS,
		    (2 * __bitset_words(CPU_SETSIZE) - 1) * (_BITSET_BITS / 8),
		    M_KMALLOC, M_WAITOK | M_ZERO);

		/*
		 * We rely below on cpuset_t and the bitset generic
		 * implementation assigning words in the '__bits' array in the
		 * same order of bits (i.e., little-endian ordering, not to be
		 * confused with machine endianness, which concerns bits in
		 * words and other integers).  This is an imperfect test, but it
		 * will detect a change to big-endian ordering.
		 */
		_Static_assert(
		    __bitset_word(_BITSET_BITS + 1, _BITSET_BITS) == 1,
		    "Assumes a bitset implementation that is little-endian "
		    "on its words");

		/* Initialize the single bit of each static span. */
		bwp = (__typeof(bwp))static_single_cpu_mask_lcs +
		    (__bitset_words(CPU_SETSIZE) - 1);
		for (i = 0; i < _BITSET_BITS; i++) {
			CPU_SET(i, (cpuset_t *)bwp);
			bwp += (2 * __bitset_words(CPU_SETSIZE) - 1);
		}

		/*
		 * Finally set all CPU masks to the proper word in their
		 * relevant span.
		 */
		CPU_FOREACH(i) {
			bwp = (__typeof(bwp))static_single_cpu_mask_lcs;
			/* Find the non-zero word of the relevant span. */
			bwp += (2 * __bitset_words(CPU_SETSIZE) - 1) *
			    (i % _BITSET_BITS) +
			    __bitset_words(CPU_SETSIZE) - 1;
			/* Shift to find the CPU mask start. */
			bwp -= (i / _BITSET_BITS);
			static_single_cpu_mask[i] = (cpuset_t *)bwp;
		}
	}

	strlcpy(init_uts_ns.name.release, osrelease, sizeof(init_uts_ns.name.release));
}
SYSINIT(linux_compat, SI_SUB_DRIVERS, SI_ORDER_SECOND, linux_compat_init, NULL);

static void
linux_compat_uninit(void *arg)
{
	linux_kobject_kfree_name(&linux_class_root);
	linux_kobject_kfree_name(&linux_root_device.kobj);
	linux_kobject_kfree_name(&linux_class_misc.kobj);

	free(static_single_cpu_mask_lcs, M_KMALLOC);
	free(static_single_cpu_mask, M_KMALLOC);
#if defined(__i386__) || defined(__amd64__)
	free(__cpu_data, M_KMALLOC);
#endif

	mtx_destroy(&vmmaplock);
	spin_lock_destroy(&pci_lock);
	rw_destroy(&linux_vma_lock);
}
SYSUNINIT(linux_compat, SI_SUB_DRIVERS, SI_ORDER_SECOND, linux_compat_uninit, NULL);

/*
 * NOTE: Linux frequently uses "unsigned long" for pointer to integer
 * conversion and vice versa, where in FreeBSD "uintptr_t" would be
 * used. Assert these types have the same size, else some parts of the
 * LinuxKPI may not work like expected:
 */
CTASSERT(sizeof(unsigned long) == sizeof(uintptr_t));
