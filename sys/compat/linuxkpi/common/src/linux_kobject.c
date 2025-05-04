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

#include <linux/kobject.h>
#include <linux/sysfs.h>

static void kset_join(struct kobject *kobj);
static void kset_leave(struct kobject *kobj);
static void kset_kfree(struct kobject *kobj);

struct kobject *
kobject_create(void)
{
	struct kobject *kobj;

	kobj = kzalloc(sizeof(*kobj), GFP_KERNEL);
	if (kobj == NULL)
		return (NULL);
	kobject_init(kobj, &linux_kfree_type);

	return (kobj);
}


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
kobject_add_complete(struct kobject *kobj)
{
	const struct kobj_type *t;
	int error;

	if (kobj->kset != NULL) {
		kset_join(kobj);
		kobj->parent = &kobj->kset->kobj;
	}

	error = sysfs_create_dir(kobj);
	if (error == 0 && kobj->ktype && kobj->ktype->default_attrs) {
		struct attribute **attr;
		t = kobj->ktype;

		for (attr = t->default_attrs; *attr != NULL; attr++) {
			error = sysfs_create_file(kobj, *attr);
			if (error != 0)
				break;
		}
		if (error != 0)
			sysfs_remove_dir(kobj);
	}

	if (error != 0)
		kset_leave(kobj);

	return (error);
}

int
kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt, ...)
{
	va_list args;
	int error;

	kobj->parent = parent;

	va_start(args, fmt);
	error = kobject_set_name_vargs(kobj, fmt, args);
	va_end(args);
	if (error)
		return (error);

	return kobject_add_complete(kobj);
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
	return kobject_add_complete(kobj);
}

void
linux_kobject_release(struct kref *kref)
{
	struct kobject *kobj;
	char *name;

	kobj = container_of(kref, struct kobject, kref);
	sysfs_remove_dir(kobj);
	kset_leave(kobj);
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

const struct kobj_type linux_kfree_type = {
	.release = linux_kobject_kfree
};

void
linux_kobject_kfree_name(struct kobject *kobj)
{
	if (kobj) {
		kfree(kobj->name);
	}
}

static ssize_t
lkpi_kobj_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct kobj_attribute *ka =
	    container_of(attr, struct kobj_attribute, attr);

	if (ka->show == NULL)
		return (-EIO);

	return (ka->show(kobj, ka, buf));
}

static ssize_t
lkpi_kobj_attr_store(struct kobject *kobj, struct attribute *attr,
    const char *buf, size_t count)
{
	struct kobj_attribute *ka =
	    container_of(attr, struct kobj_attribute, attr);

	if (ka->store == NULL)
		return (-EIO);

	return (ka->store(kobj, ka, buf, count));
}

const struct sysfs_ops kobj_sysfs_ops = {
	.show	= lkpi_kobj_attr_show,
	.store	= lkpi_kobj_attr_store,
};

const struct kobj_type linux_kset_kfree_type = {
	.release = kset_kfree
};

static struct kset *
kset_create(const char *name,
    const struct kset_uevent_ops *uevent_ops,
    struct kobject *parent_kobj)
{
	struct kset *kset;

	kset = kzalloc(sizeof(*kset), GFP_KERNEL);
	if (kset == NULL)
		return (NULL);

	kset->uevent_ops = uevent_ops;

	kobject_set_name(&kset->kobj, "%s", name);
	kset->kobj.parent = parent_kobj;
	kset->kobj.kset = NULL;

	return (kset);
}

void
kset_init(struct kset *kset)
{
	kobject_init(&kset->kobj, &linux_kset_kfree_type);
	INIT_LIST_HEAD(&kset->list);
	spin_lock_init(&kset->list_lock);
}

static void
kset_join(struct kobject *kobj)
{
	struct kset *kset;

	kset = kobj->kset;
	if (kset == NULL)
		return;

	kset_get(kobj->kset);

	spin_lock(&kset->list_lock);
	list_add_tail(&kobj->entry, &kset->list);
	spin_unlock(&kset->list_lock);
}

static void
kset_leave(struct kobject *kobj)
{
	struct kset *kset;

	kset = kobj->kset;
	if (kset == NULL)
		return;

	spin_lock(&kset->list_lock);
	list_del_init(&kobj->entry);
	spin_unlock(&kset->list_lock);

	kset_put(kobj->kset);
}

struct kset *
kset_create_and_add(const char *name, const struct kset_uevent_ops *u,
    struct kobject *parent_kobj)
{
	int ret;
	struct kset *kset;

	kset = kset_create(name, u, parent_kobj);
	if (kset == NULL)
		return (NULL);

	ret = kset_register(kset);
	if (ret != 0) {
		linux_kobject_kfree_name(&kset->kobj);
		kfree(kset);
		return (NULL);
	}

	return (kset);
}

int
kset_register(struct kset *kset)
{
	int ret;

	if (kset == NULL)
		return -EINVAL;

	kset_init(kset);
	ret = kobject_add_complete(&kset->kobj);

	return ret;
}

void
kset_unregister(struct kset *kset)
{
	if (kset == NULL)
		return;

	kobject_del(&kset->kobj);
	kobject_put(&kset->kobj);
}

static void
kset_kfree(struct kobject *kobj)
{
	struct kset *kset;

	kset = to_kset(kobj);
	kfree(kset);
}
