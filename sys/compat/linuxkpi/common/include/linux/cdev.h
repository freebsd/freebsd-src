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
#ifndef	_LINUX_CDEV_H_
#define	_LINUX_CDEV_H_

#include <linux/kobject.h>
#include <linux/kdev_t.h>
#include <linux/list.h>

struct file_operations;
struct inode;
struct module;

extern struct cdevsw linuxcdevsw;

struct linux_cdev {
	struct kobject	kobj;
	struct module	*owner;
	struct cdev	*cdev;
	dev_t		dev;
	const struct file_operations *ops;
};

static inline void
cdev_release(struct kobject *kobj)
{
	struct linux_cdev *cdev;

	cdev = container_of(kobj, struct linux_cdev, kobj);
	if (cdev->cdev)
		destroy_dev(cdev->cdev);
	kfree(cdev);
}

static inline void
cdev_static_release(struct kobject *kobj)
{
	struct linux_cdev *cdev;

	cdev = container_of(kobj, struct linux_cdev, kobj);
	if (cdev->cdev)
		destroy_dev(cdev->cdev);
}

static struct kobj_type cdev_ktype = {
	.release = cdev_release,
};

static struct kobj_type cdev_static_ktype = {
	.release = cdev_static_release,
};

static inline void
cdev_init(struct linux_cdev *cdev, const struct file_operations *ops)
{

	kobject_init(&cdev->kobj, &cdev_static_ktype);
	cdev->ops = ops;
}

static inline struct linux_cdev *
cdev_alloc(void)
{
	struct linux_cdev *cdev;

	cdev = kzalloc(sizeof(struct linux_cdev), M_WAITOK);
	if (cdev)
		kobject_init(&cdev->kobj, &cdev_ktype);
	return (cdev);
}

static inline void
cdev_put(struct linux_cdev *p)
{
	kobject_put(&p->kobj);
}

static inline int
cdev_add(struct linux_cdev *cdev, dev_t dev, unsigned count)
{
	if (count != 1)
		panic("cdev_add: Unsupported count: %d", count);
	cdev->cdev = make_dev(&linuxcdevsw, MINOR(dev), 0, 0, 0700, 
	    "%s", kobject_name(&cdev->kobj));
	cdev->dev = dev;
	cdev->cdev->si_drv1 = cdev;

	return (0);
}

static inline void
cdev_del(struct linux_cdev *cdev)
{
	if (cdev->cdev) {
		destroy_dev(cdev->cdev);
		cdev->cdev = NULL;
	}
	kobject_put(&cdev->kobj);
}

#define	cdev	linux_cdev

#endif	/* _LINUX_CDEV_H_ */
