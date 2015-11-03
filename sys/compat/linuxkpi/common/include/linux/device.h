/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
 *
 * $FreeBSD$
 */
#ifndef	_LINUX_DEVICE_H_
#define	_LINUX_DEVICE_H_

#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/list.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <linux/kdev_t.h>
#include <asm/atomic.h>

#include <sys/bus.h>

enum irqreturn	{ IRQ_NONE = 0, IRQ_HANDLED, IRQ_WAKE_THREAD, };
typedef enum irqreturn	irqreturn_t;

struct class {
	const char	*name;
	struct module	*owner;
	struct kobject	kobj;
	devclass_t	bsdclass;
	void		(*class_release)(struct class *class);
	void		(*dev_release)(struct device *dev);
	char *		(*devnode)(struct device *dev, umode_t *mode);
};

struct device {
	struct device	*parent;
	struct list_head irqents;
	device_t	bsddev;
	dev_t		devt;
	struct class	*class;
	void		(*release)(struct device *dev);
	struct kobject	kobj;
	uint64_t	*dma_mask;
	void		*driver_data;
	unsigned int	irq;
	unsigned int	msix;
	unsigned int	msix_max;
};

extern struct device linux_rootdev;
extern struct kobject class_root;

struct class_attribute {
        struct attribute attr;
        ssize_t (*show)(struct class *, struct class_attribute *, char *);
        ssize_t (*store)(struct class *, struct class_attribute *, const char *, size_t);
        const void *(*namespace)(struct class *, const struct class_attribute *);
};

#define	CLASS_ATTR(_name, _mode, _show, _store)				\
	struct class_attribute class_attr_##_name =			\
	    { { #_name, NULL, _mode }, _show, _store }

struct device_attribute {
	struct attribute	attr;
	ssize_t			(*show)(struct device *,
					struct device_attribute *, char *);
	ssize_t			(*store)(struct device *,
					struct device_attribute *, const char *,
					size_t);
};

#define	DEVICE_ATTR(_name, _mode, _show, _store)			\
	struct device_attribute dev_attr_##_name =			\
	    { { #_name, NULL, _mode }, _show, _store }

/* Simple class attribute that is just a static string */
struct class_attribute_string {
	struct class_attribute attr;
	char *str;
};

static inline ssize_t
show_class_attr_string(struct class *class,
				struct class_attribute *attr, char *buf)
{
	struct class_attribute_string *cs;
	cs = container_of(attr, struct class_attribute_string, attr);
	return snprintf(buf, PAGE_SIZE, "%s\n", cs->str);
}

/* Currently read-only only */
#define _CLASS_ATTR_STRING(_name, _mode, _str) \
	{ __ATTR(_name, _mode, show_class_attr_string, NULL), _str }
#define CLASS_ATTR_STRING(_name, _mode, _str) \
	struct class_attribute_string class_attr_##_name = \
		_CLASS_ATTR_STRING(_name, _mode, _str)

#define	dev_err(dev, fmt, ...)	device_printf((dev)->bsddev, fmt, ##__VA_ARGS__)
#define	dev_warn(dev, fmt, ...)	device_printf((dev)->bsddev, fmt, ##__VA_ARGS__)
#define	dev_info(dev, fmt, ...)	device_printf((dev)->bsddev, fmt, ##__VA_ARGS__)
#define	dev_printk(lvl, dev, fmt, ...)					\
	    device_printf((dev)->bsddev, fmt, ##__VA_ARGS__)

static inline void *
dev_get_drvdata(struct device *dev)
{

	return dev->driver_data;
}

static inline void
dev_set_drvdata(struct device *dev, void *data)
{

	dev->driver_data = data;
}

static inline struct device *
get_device(struct device *dev)
{

	if (dev)
		kobject_get(&dev->kobj);

	return (dev);
}

static inline char *
dev_name(const struct device *dev)
{

 	return kobject_name(&dev->kobj);
}

#define	dev_set_name(_dev, _fmt, ...)					\
	kobject_set_name(&(_dev)->kobj, (_fmt), ##__VA_ARGS__)

static inline void
put_device(struct device *dev)
{

	if (dev)
		kobject_put(&dev->kobj);
}

static inline ssize_t
class_show(struct kobject *kobj, struct attribute *attr, char *buf)
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

static inline ssize_t
class_store(struct kobject *kobj, struct attribute *attr, const char *buf,
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

static inline void
class_release(struct kobject *kobj)
{
	struct class *class;

	class = container_of(kobj, struct class, kobj);
	if (class->class_release)
		class->class_release(class);
}

static struct sysfs_ops class_sysfs = {
	.show  = class_show,
	.store = class_store,
};
static struct kobj_type class_ktype = {
	.release = class_release,
	.sysfs_ops = &class_sysfs
};

static inline int
class_register(struct class *class)
{

	class->bsdclass = devclass_create(class->name);
	kobject_init(&class->kobj, &class_ktype);
	kobject_set_name(&class->kobj, class->name);
	kobject_add(&class->kobj, &class_root, class->name);

	return (0);
}

static inline void
class_unregister(struct class *class)
{

	kobject_put(&class->kobj);
}

static inline void
device_release(struct kobject *kobj)
{
	struct device *dev;

	dev = container_of(kobj, struct device, kobj);
	/* This is the precedence defined by linux. */
	if (dev->release)
		dev->release(dev);
	else if (dev->class && dev->class->dev_release)
		dev->class->dev_release(dev);
}

static inline ssize_t
dev_show(struct kobject *kobj, struct attribute *attr, char *buf)
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

static inline ssize_t
dev_store(struct kobject *kobj, struct attribute *attr, const char *buf,
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

static struct sysfs_ops dev_sysfs = { .show  = dev_show, .store = dev_store, };
static struct kobj_type dev_ktype = {
	.release = device_release,
	.sysfs_ops = &dev_sysfs
};

/*
 * Devices are registered and created for exporting to sysfs.  create
 * implies register and register assumes the device fields have been
 * setup appropriately before being called.
 */
static inline int
device_register(struct device *dev)
{
	device_t bsddev;
	int unit;

	bsddev = NULL;
	if (dev->devt) {
		unit = MINOR(dev->devt);
		bsddev = devclass_get_device(dev->class->bsdclass, unit);
	} else
		unit = -1;
	if (bsddev == NULL)
		bsddev = device_add_child(dev->parent->bsddev,
		    dev->class->kobj.name, unit);
	if (bsddev) {
		if (dev->devt == 0)
			dev->devt = makedev(0, device_get_unit(bsddev));
		device_set_softc(bsddev, dev);
	}
	dev->bsddev = bsddev;
	kobject_init(&dev->kobj, &dev_ktype);
	kobject_add(&dev->kobj, &dev->class->kobj, dev_name(dev));

	return (0);
}

static inline void
device_unregister(struct device *dev)
{
	device_t bsddev;

	bsddev = dev->bsddev;
	mtx_lock(&Giant);
	if (bsddev)
		device_delete_child(device_get_parent(bsddev), bsddev);
	mtx_unlock(&Giant);
	put_device(dev);
}

struct device *device_create(struct class *class, struct device *parent,
	    dev_t devt, void *drvdata, const char *fmt, ...);

static inline void
device_destroy(struct class *class, dev_t devt)
{
	device_t bsddev;
	int unit;

	unit = MINOR(devt);
	bsddev = devclass_get_device(class->bsdclass, unit);
	if (bsddev)
		device_unregister(device_get_softc(bsddev));
}

static inline void
class_kfree(struct class *class)
{

	kfree(class);
}

static inline struct class *
class_create(struct module *owner, const char *name)
{
	struct class *class;
	int error;

	class = kzalloc(sizeof(*class), M_WAITOK);
	class->owner = owner;
	class->name= name;
	class->class_release = class_kfree;
	error = class_register(class);
	if (error) {
		kfree(class);
		return (NULL);
	}

	return (class);
}

static inline void
class_destroy(struct class *class)
{

	if (class == NULL)
		return;
	class_unregister(class);
}

static inline int
device_create_file(struct device *dev, const struct device_attribute *attr)
{

	if (dev)
		return sysfs_create_file(&dev->kobj, &attr->attr);
	return -EINVAL;
}

static inline void
device_remove_file(struct device *dev, const struct device_attribute *attr)
{

	if (dev)
		sysfs_remove_file(&dev->kobj, &attr->attr);
}

static inline int
class_create_file(struct class *class, const struct class_attribute *attr)
{

	if (class)
		return sysfs_create_file(&class->kobj, &attr->attr);
	return -EINVAL;
}

static inline void
class_remove_file(struct class *class, const struct class_attribute *attr)
{

	if (class)
		sysfs_remove_file(&class->kobj, &attr->attr);
}

static inline int dev_to_node(struct device *dev)
{
                return -1;
}

char *kvasprintf(gfp_t, const char *, va_list);
char *kasprintf(gfp_t, const char *, ...);

#endif	/* _LINUX_DEVICE_H_ */
