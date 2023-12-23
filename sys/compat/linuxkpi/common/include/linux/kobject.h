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
 */
#ifndef	_LINUXKPI_LINUX_KOBJECT_H_
#define	_LINUXKPI_LINUX_KOBJECT_H_

#include <machine/stdarg.h>

#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

struct kobject;
struct kset;
struct sysctl_oid;

#define	KOBJ_CHANGE		0x01

struct kobj_type {
	void (*release)(struct kobject *kobj);
	const struct sysfs_ops *sysfs_ops;
	struct attribute **default_attrs;
	const struct attribute_group **default_groups;
};

extern const struct kobj_type linux_kfree_type;

struct kobject {
	struct kobject		*parent;
	char			*name;
	struct kref		kref;
	const struct kobj_type	*ktype;
	struct list_head	entry;
	struct sysctl_oid	*oidp;
	struct kset		*kset;
};

extern struct kobject *mm_kobj;

struct attribute {
	const char	*name;
	struct module	*owner;
	mode_t		mode;
};

extern const struct sysfs_ops kobj_sysfs_ops;

struct kobj_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj, struct kobj_attribute *attr,
	    char *buf);
	ssize_t (*store)(struct kobject *kobj, struct kobj_attribute *attr,
	    const char *buf, size_t count);
};

struct kset_uevent_ops {
	/* TODO */
};

struct kset {
	struct list_head	list;
	spinlock_t		list_lock;
	struct kobject		kobj;
	const struct kset_uevent_ops *uevent_ops;
};

static inline void
kobject_init(struct kobject *kobj, const struct kobj_type *ktype)
{

	kref_init(&kobj->kref);
	INIT_LIST_HEAD(&kobj->entry);
	kobj->ktype = ktype;
	kobj->oidp = NULL;
}

void linux_kobject_release(struct kref *kref);

static inline void
kobject_put(struct kobject *kobj)
{

	if (kobj)
		kref_put(&kobj->kref, linux_kobject_release);
}

static inline struct kobject *
kobject_get(struct kobject *kobj)
{

	if (kobj)
		kref_get(&kobj->kref);
	return kobj;
}

struct kobject *kobject_create(void);
int	kobject_set_name_vargs(struct kobject *kobj, const char *fmt, va_list);
int	kobject_add(struct kobject *kobj, struct kobject *parent,
	    const char *fmt, ...);

static inline struct kobject *
kobject_create_and_add(const char *name, struct kobject *parent)
{
	struct kobject *kobj;

	kobj = kobject_create();
	if (kobj == NULL)
		return (NULL);
	if (kobject_add(kobj, parent, "%s", name) == 0)
		return (kobj);
	kobject_put(kobj);

	return (NULL);
}

static inline void
kobject_del(struct kobject *kobj __unused)
{
}

static inline char *
kobject_name(const struct kobject *kobj)
{

	return kobj->name;
}

int	kobject_set_name(struct kobject *kobj, const char *fmt, ...);
int	kobject_init_and_add(struct kobject *kobj, const struct kobj_type *ktype,
	    struct kobject *parent, const char *fmt, ...);

static __inline void
kobject_uevent_env(struct kobject *kobj, int action, char *envp[])
{

	/*
	 * iwlwifi(4) sends an INACCESSIBLE event when it detects that the card
	 * (pice endpoint) is gone and it attempts a removal cleanup.
	 * Not sure if we do anything related to udev/sysfs at the moment or
	 * need a shortcut or simply ignore it (for now).
	 */
}

void	kset_init(struct kset *kset);
int	kset_register(struct kset *kset);
void	kset_unregister(struct kset *kset);
struct kset * kset_create_and_add(const char *name,
    const struct kset_uevent_ops *u, struct kobject *parent_kobj);

static inline struct kset *
to_kset(struct kobject *kobj)
{
	if (kobj != NULL)
		return container_of(kobj, struct kset, kobj);
	else
		return NULL;
}

static inline struct kset *
kset_get(struct kset *kset)
{
	if (kset != NULL) {
		struct kobject *kobj;

		kobj = kobject_get(&kset->kobj);
		return to_kset(kobj);
	} else {
		return NULL;
	}
}

static inline void
kset_put(struct kset *kset)
{
	if (kset != NULL)
		kobject_put(&kset->kobj);
}

void linux_kobject_kfree_name(struct kobject *kobj);

#endif /* _LINUXKPI_LINUX_KOBJECT_H_ */
