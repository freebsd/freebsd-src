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
#ifndef	_LINUXKPI_LINUX_SYSFS_H_
#define	_LINUXKPI_LINUX_SYSFS_H_

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/errno.h>

#include <linux/kobject.h>
#include <linux/stringify.h>
#include <linux/mm.h>

struct sysfs_ops {
	ssize_t (*show)(struct kobject *, struct attribute *, char *);
	ssize_t (*store)(struct kobject *, struct attribute *, const char *,
	    size_t);
};

struct attribute_group {
	const char		*name;
	mode_t			(*is_visible)(struct kobject *,
				    struct attribute *, int);
	struct attribute	**attrs;
};

struct bin_attribute {
	struct attribute	attr;
	size_t			size;
	ssize_t (*read)(struct linux_file *, struct kobject *,
			struct bin_attribute *, char *, loff_t, size_t);
	ssize_t (*write)(struct linux_file *, struct kobject *,
			 struct bin_attribute *, char *, loff_t, size_t);
};

#define	__ATTR(_name, _mode, _show, _store) {				\
	.attr = { .name = __stringify(_name), .mode = _mode },		\
	.show = _show, .store  = _store,				\
}
#define	__ATTR_RO(_name) {						\
	.attr = { .name = __stringify(_name), .mode = 0444 },		\
	.show = _name##_show,						\
}
#define	__ATTR_WO(_name)	__ATTR(_name, 0200, NULL, _name##_store)
#define	__ATTR_RW(_name)	__ATTR(_name, 0644, _name##_show, _name##_store)
#define	__ATTR_NULL	{ .attr = { .name = NULL } }

#define	ATTRIBUTE_GROUPS(_name)						\
	static struct attribute_group _name##_group = {			\
		.name = __stringify(_name),				\
		.attrs = _name##_attrs,					\
	};								\
	static const struct attribute_group *_name##_groups[] = {	\
		&_name##_group,						\
		NULL,							\
	}

#define	__BIN_ATTR(_name, _mode, _read, _write, _size) {		\
	.attr = { .name = __stringify(_name), .mode = _mode },		\
	.read = _read, .write  = _write, .size = _size,			\
}
#define	__BIN_ATTR_RO(_name, _size) {					\
	.attr = { .name = __stringify(_name), .mode = 0444 },		\
	.read = _name##_read, .size = _size,				\
}
#define	__BIN_ATTR_WO(_name, _size) {					\
	.attr = { .name = __stringify(_name), .mode = 0200 },		\
	.write = _name##_write, .size = _size,				\
}
#define	__BIN_ATTR_WR(_name, _size) {					\
	.attr = { .name = __stringify(_name), .mode = 0644 },		\
	.read = _name##_read, .write = _name##_write, .size = _size,	\
}

#define	BIN_ATTR(_name, _mode, _read, _write, _size) \
struct bin_attribute bin_attr_##_name = \
    __BIN_ATTR(_name, _mode, _read, _write, _size);

#define	BIN_ATTR_RO(_name, _size) \
struct bin_attribute bin_attr_##_name = \
    __BIN_ATTR_RO(_name, _size);

#define	BIN_ATTR_WO(_name, _size) \
struct bin_attribute bin_attr_##_name = \
    __BIN_ATTR_WO(_name, _size);

#define	BIN_ATTR_WR(_name, _size) \
struct bin_attribute bin_attr_##_name = \
    __BIN_ATTR_WR(_name, _size);

/*
 * Handle our generic '\0' terminated 'C' string.
 * Two cases:
 *      a variable string:  point arg1 at it, arg2 is max length.
 *      a constant string:  point arg1 at it, arg2 is zero.
 */

static inline int
sysctl_handle_attr(SYSCTL_HANDLER_ARGS)
{
	struct kobject *kobj;
	struct attribute *attr;
	const struct sysfs_ops *ops;
	char *buf;
	int error;
	ssize_t len;

	kobj = arg1;
	attr = (struct attribute *)(intptr_t)arg2;
	if (kobj->ktype == NULL || kobj->ktype->sysfs_ops == NULL)
		return (ENODEV);
	buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (buf == NULL)
		return (ENOMEM);
	ops = kobj->ktype->sysfs_ops;
	if (ops->show) {
		len = ops->show(kobj, attr, buf);
		/*
		 * It's valid to not have a 'show' so just return an
		 * empty string.
		 */
		if (len < 0) {
			error = -len;
			if (error != EIO)
				goto out;
			buf[0] = '\0';
		} else if (len) {
			len--;
			if (len >= PAGE_SIZE)
				len = PAGE_SIZE - 1;
			/* Trim trailing newline. */
			buf[len] = '\0';
		}
	}

	/* Leave one trailing byte to append a newline. */
	error = sysctl_handle_string(oidp, buf, PAGE_SIZE - 1, req);
	if (error != 0 || req->newptr == NULL || ops->store == NULL)
		goto out;
	len = strlcat(buf, "\n", PAGE_SIZE);
	KASSERT(len < PAGE_SIZE, ("new attribute truncated"));
	len = ops->store(kobj, attr, buf, len);
	if (len < 0)
		error = -len;
out:
	free_page((unsigned long)buf);

	return (error);
}

static inline int
sysfs_create_file(struct kobject *kobj, const struct attribute *attr)
{
	struct sysctl_oid *oid;

	oid = SYSCTL_ADD_OID(NULL, SYSCTL_CHILDREN(kobj->oidp), OID_AUTO,
	    attr->name, CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_MPSAFE, kobj,
	    (uintptr_t)attr, sysctl_handle_attr, "A", "");
	if (!oid) {
		return (-ENOMEM);
	}

	return (0);
}

static inline void
sysfs_remove_file(struct kobject *kobj, const struct attribute *attr)
{

	if (kobj->oidp)
		sysctl_remove_name(kobj->oidp, attr->name, 1, 1);
}

static inline int
sysctl_handle_bin_attr(SYSCTL_HANDLER_ARGS)
{
	struct kobject *kobj;
	struct bin_attribute *attr;
	char *buf;
	int error;
	ssize_t len;

	kobj = arg1;
	attr = (struct bin_attribute *)(intptr_t)arg2;
	if (kobj->ktype == NULL || kobj->ktype->sysfs_ops == NULL)
		return (ENODEV);
	buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (buf == NULL)
		return (ENOMEM);

	if (attr->read) {
		len = attr->read(
		    NULL, /* <-- struct file, unimplemented */
		    kobj, attr, buf, req->oldidx, PAGE_SIZE);
		if (len < 0) {
			error = -len;
			if (error != EIO)
				goto out;
		}
	}

	error = sysctl_handle_opaque(oidp, buf, PAGE_SIZE, req);
	if (error != 0 || req->newptr == NULL || attr->write == NULL)
		goto out;

	len = attr->write(
	    NULL, /* <-- struct file, unimplemented */
	    kobj, attr, buf, req->newidx, req->newlen);
	if (len < 0)
		error = -len;
out:
	free_page((unsigned long)buf);

	return (error);
}

static inline int
sysfs_create_bin_file(struct kobject *kobj, const struct bin_attribute *attr)
{
	struct sysctl_oid *oid;
	int ctlflags;

	ctlflags = CTLTYPE_OPAQUE | CTLFLAG_MPSAFE;
	if (attr->attr.mode & (S_IRUSR | S_IWUSR))
		ctlflags |= CTLFLAG_RW;
	else if (attr->attr.mode & S_IRUSR)
		ctlflags |= CTLFLAG_RD;
	else if (attr->attr.mode & S_IWUSR)
		ctlflags |= CTLFLAG_WR;

	oid = SYSCTL_ADD_OID(NULL, SYSCTL_CHILDREN(kobj->oidp), OID_AUTO,
	    attr->attr.name, ctlflags, kobj,
	    (uintptr_t)attr, sysctl_handle_bin_attr, "", "");
	if (oid == NULL)
		return (-ENOMEM);

	return (0);
}

static inline void
sysfs_remove_bin_file(struct kobject *kobj, const struct bin_attribute *attr)
{

	if (kobj->oidp)
		sysctl_remove_name(kobj->oidp, attr->attr.name, 1, 1);
}

static inline int
sysfs_create_link(struct kobject *kobj __unused,
    struct kobject *target __unused, const char *name __unused)
{
	/* TODO */

	return (0);
}

static inline void
sysfs_remove_link(struct kobject *kobj, const char *name)
{
	/* TODO (along with sysfs_create_link) */
}

static inline int
sysfs_create_files(struct kobject *kobj, const struct attribute * const *attrs)
{
	int error = 0;
	int i;

	for (i = 0; attrs[i] && !error; i++)
		error = sysfs_create_file(kobj, attrs[i]);
	while (error && --i >= 0)
		sysfs_remove_file(kobj, attrs[i]);

	return (error);
}

static inline void
sysfs_remove_files(struct kobject *kobj, const struct attribute * const *attrs)
{
	int i;

	for (i = 0; attrs[i]; i++)
		sysfs_remove_file(kobj, attrs[i]);
}

static inline int
sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp)
{
	struct attribute **attr;
	struct sysctl_oid *oidp;

	/* Don't create the group node if grp->name is undefined. */
	if (grp->name)
		oidp = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(kobj->oidp),
		    OID_AUTO, grp->name, CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, grp->name);
	else
		oidp = kobj->oidp;
	for (attr = grp->attrs; *attr != NULL; attr++) {
		SYSCTL_ADD_OID(NULL, SYSCTL_CHILDREN(oidp), OID_AUTO,
		    (*attr)->name, CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_MPSAFE,
		    kobj, (uintptr_t)*attr, sysctl_handle_attr, "A", "");
	}

	return (0);
}

static inline void
sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp)
{

	if (kobj->oidp)
		sysctl_remove_name(kobj->oidp, grp->name, 1, 1);
}

static inline int
sysfs_create_groups(struct kobject *kobj, const struct attribute_group **grps)
{
	int error = 0;
	int i;

	if (grps == NULL)
		goto done;
	for (i = 0; grps[i] && !error; i++)
		error = sysfs_create_group(kobj, grps[i]);
	while (error && --i >= 0)
		sysfs_remove_group(kobj, grps[i]);
done:
	return (error);
}

static inline void
sysfs_remove_groups(struct kobject *kobj, const struct attribute_group **grps)
{
	int i;

	if (grps == NULL)
		return;
	for (i = 0; grps[i]; i++)
		sysfs_remove_group(kobj, grps[i]);
}

static inline int
sysfs_merge_group(struct kobject *kobj, const struct attribute_group *grp)
{

	/* Really expected behavior is to return failure if group exists. */
	return (sysfs_create_group(kobj, grp));
}

static inline void
sysfs_unmerge_group(struct kobject *kobj, const struct attribute_group *grp)
{
	struct attribute **attr;
	struct sysctl_oid *oidp;

	SYSCTL_FOREACH(oidp, SYSCTL_CHILDREN(kobj->oidp)) {
		if (strcmp(oidp->oid_name, grp->name) != 0)
			continue;
		for (attr = grp->attrs; *attr != NULL; attr++) {
			sysctl_remove_name(oidp, (*attr)->name, 1, 1);
		}
	}
}

static inline int
sysfs_create_dir(struct kobject *kobj)
{
	struct sysctl_oid *oid;

	oid = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(kobj->parent->oidp),
	    OID_AUTO, kobj->name, CTLFLAG_RD|CTLFLAG_MPSAFE, NULL, kobj->name);
	if (!oid) {
		return (-ENOMEM);
	}
	kobj->oidp = oid;

	return (0);
}

static inline void
sysfs_remove_dir(struct kobject *kobj)
{

	if (kobj->oidp == NULL)
		return;
	sysctl_remove_oid(kobj->oidp, 1, 1);
}

static inline bool
sysfs_streq(const char *s1, const char *s2)
{
	int l1, l2;

	l1 = strlen(s1);
	l2 = strlen(s2);

	if (l1 != 0 && s1[l1-1] == '\n')
		l1--;
	if (l2 != 0 && s2[l2-1] == '\n')
		l2--;

	return (l1 == l2 && strncmp(s1, s2, l1) == 0);
}

static inline int
sysfs_emit(char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	if (!buf || offset_in_page(buf)) {
		pr_warn("invalid sysfs_emit: buf:%p\n", buf);
		return (0);
	}

	va_start(args, fmt);
	i = vscnprintf(buf, PAGE_SIZE, fmt, args);
	va_end(args);

	return (i);
}

static inline int
sysfs_emit_at(char *buf, int at, const char *fmt, ...)
{
	va_list args;
	int i;

	if (!buf || offset_in_page(buf) || at < 0 || at >= PAGE_SIZE) {
		pr_warn("invalid sysfs_emit: buf:%p at:%d\n", buf, at);
		return (0);
	}

	va_start(args, fmt);
	i = vscnprintf(buf + at, PAGE_SIZE - at, fmt, args);
	va_end(args);

	return (i);
}

static inline int
_sysfs_match_string(const char * const *a, size_t l, const char *s)
{
	const char *p;
	int i;

	for (i = 0; i < l; i++) {
		p = a[i];
		if (p == NULL)
			break;
		if (sysfs_streq(p, s))
			return (i);
	}

	return (-ENOENT);
}
#define	sysfs_match_string(a, s)	_sysfs_match_string(a, ARRAY_SIZE(a), s)

#define sysfs_attr_init(attr) do {} while(0)

#endif	/* _LINUXKPI_LINUX_SYSFS_H_ */
