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
#ifndef	_LINUX_KOBJECT_H_
#define	_LINUX_KOBJECT_H_

#include <machine/stdarg.h>
#include <linux/kref.h>
#include <linux/slab.h>

struct kobject;

struct kobj_type {
	void (*release)(struct kobject *kobj);
	const struct sysfs_ops *sysfs_ops;
	struct attribute **default_attrs;
};

struct kobject {
	struct kobject		*parent;
	char			*name;
	struct kref		kref;
	struct kobj_type	*ktype;
};

static inline int
kobject_init_and_add(struct kobject *kobj, struct kobj_type *ktype,
    struct kobject *parent, const char *fmt, ...)
{

	kref_init(&kobj->kref);
	kobj->ktype = ktype;
	kobj->name = NULL;
	kobj->parent = NULL;
	return 0;
}

static inline void
kobject_init(struct kobject *kobj, struct kobj_type *ktype)
{
	kref_init(&kobj->kref);
	kobj->ktype = ktype;
	kobj->name = NULL;
	kobj->parent = NULL;
}

static inline void
kobject_put(struct kobject *kobj)
{
}

static inline int
kobject_set_name_vargs(struct kobject *kobj, const char *fmt, va_list args)
{
	char *old;
	char *name;

	old = kobj->name;

	if (old && !fmt)
		return 0;

	name = kzalloc(MAXPATHLEN, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	vsnprintf(name, MAXPATHLEN, fmt, args);
	kobj->name = name;
	kfree(old);
	for (; *name != '\0'; name++)
		if (*name == '/')
			*name = '!';
	return (0);
}

static inline int
kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt, ...)
{
	va_list args;
	int error;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);
	kobj->parent = parent;

	return (error);
}


static inline char *
kobject_name(const struct kobject *kobj)
{
	return kobj->name;
}

int kobject_set_name(struct kobject *kobj, const char *fmt, ...);

#endif /* _LINUX_KOBJECT_H_ */
