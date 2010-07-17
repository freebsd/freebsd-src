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
#ifndef	_LINUX_DEVICE_H_
#define	_LINUX_DEVICE_H_

#include <linux/types.h>
#include <linux/kobject.h>
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
	devclass_t	bsdclass;
};

struct device {
	struct device	*parent;
	device_t	bsddev;
	dev_t		devt;
	struct class	*class;
	void		(*release)(struct device *dev);
	irqreturn_t	(*irqhandler)(int, void *);
	void		*irqtag;
	struct kobject	kobj;
	uint64_t	*dma_mask;
	void		*driver_data;

};

/* #define	device	linux_device */

struct class_attribute {
	struct attribute	attr;
        ssize_t			(*show)(struct class *, char *);
        ssize_t			(*store)(struct class *, char *, size_t);
};
#define	CLASS_ATTR(_name, _mode, _show, _store)				\
	struct class_attribute class_attr_##_name =			\
	    { { #_name, NULL, _mode }, _show, _store }

struct device_attribute {
	struct attribute	attr;
	ssize_t			(*show)(struct device *,
				    struct device_attribute *, char *);
	ssize_t			(*store)(struct device *,
				    struct device_attribute *, const char *, size_t);
};

#define	DEVICE_ATTR(_name, _mode, _show, _store)			\
	struct device_attribute dev_attr_##_name =			\
	    { { #_name, NULL, _mode }, _show, _store }

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
	kobject_set_name(&(_dev)->kobj, (_fmt), #__VA_ARGS__)

static inline void
put_device(struct device *dev)
{

	if (dev)
		kobject_put(&dev->kobj);
}

static inline int
class_register(struct class *class)
{

	class->bsdclass = devclass_create(class->name);
	return 0;
}

static inline void
class_unregister(struct class *class)
{

	return;
}

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
		    dev->kobj.name, unit);
	if (bsddev) {
		if (dev->devt == 0)
			dev->devt = device_get_unit(bsddev);
		device_set_softc(bsddev, dev);
	}
	dev->bsddev = bsddev;
	kobject_init(&dev->kobj, NULL);
	get_device(dev);

	return (0);
}

static inline void
device_unregister(struct device *dev)
{
	device_t bsddev;

	bsddev = dev->bsddev;
	if (bsddev)
		device_delete_child(device_get_parent(bsddev), bsddev);
	put_device(dev);
}

struct device *device_create(struct class *class, struct device *parent,
	    dev_t devt, void *drvdata, const char *fmt, ...);

static inline void
device_destroy(struct class *class, dev_t devt)
{
	struct device *dev;
	device_t bsddev;
	int unit;

	unit = MINOR(devt);
	bsddev = devclass_get_device(class->bsdclass, unit);
	if (bsddev) {
		dev = device_get_softc(bsddev);
		device_unregister(dev);
		put_device(dev);
	}
}

static inline struct class *
class_create(struct module *owner, const char *name)
{
	struct class *class;

	class = kzalloc(sizeof(*class), M_WAITOK);
	class->owner = owner;
	class->name= name;

	return (class);
}

static inline void
class_destroy(struct class *class)
{
	/* XXX Missing ref count. */
	kfree(class);
}

/*
 * These are supposed to create the sysfs entry for the attribute.  Should
 * instead create a sysctl tree. XXX
 */
static inline int
device_create_file(struct device *device, const struct device_attribute *entry)
{
	return (0);
}

static inline void
device_remove_file(struct device *dev, const struct device_attribute *attr)
{
	return;
}

static inline int
class_create_file(struct class *class, const struct class_attribute *attr)
{
	return (0);
}

static inline void
class_remove_file(struct class *class, const struct class_attribute *attr)
{
	return;
}

#endif	/* _LINUX_DEVICE_H_ */
