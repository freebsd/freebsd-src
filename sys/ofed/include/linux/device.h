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

#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <asm/atomic.h>

struct class {
	const char	*name;
	struct module	*owner;
};

struct device {
	struct device	*parent;
	dev_t		devt;
	struct class	*class;
	void		(*release)(struct device *dev);
	struct kobject	kobj;
	void		*driver_data;
};

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
				    struct device_attribute *, char *, size_t);
};

#define	DEVICE_ATTR(_name, _mode, _show, _store)			\
	struct device_attribute dev_attr_##_name =			\
	    { { #_name, NULL, _mode }, _show, _store }

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
}

static inline int
class_register(struct class *class)
{
	return 0;
}

static inline void
class_unregister(struct class *class)
{
	return;
}

static inline void
device_unregister(struct device *dev)
{
}

static inline struct device *
device_create(struct class *cls, struct device *parent, dev_t devt,
    void *drvdata, const char *fmt, ...)
{
	return (NULL);
}

static inline void
device_destroy(struct class *class, dev_t dev)
{
}

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

static inline struct class *
class_create(struct module *owner, const char *name)
{
	return (NULL);
}

static inline void
class_destroy(struct class *class)
{
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

static inline int
device_register(struct device *dev)
{
	return (0);
}

#endif	/* _LINUX_DEVICE_H_ */
