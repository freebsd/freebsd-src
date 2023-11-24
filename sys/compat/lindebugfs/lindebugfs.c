/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016-2018, Matthew Macy <mmacy@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/blist.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/bus.h>
#include <sys/pciio.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <net/if.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/swap_pager.h>

#include <machine/bus.h>

#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_util.h>
#include <fs/pseudofs/pseudofs.h>

#include <asm/atomic.h>
#include <linux/compat.h>
#include <linux/debugfs.h>
#include <linux/fs.h>

MALLOC_DEFINE(M_DFSINT, "debugfsint", "Linux debugfs internal");

static struct pfs_node *debugfs_root;

#define DM_SYMLINK 0x1
#define DM_DIR 0x2
#define DM_FILE 0x3

struct dentry_meta {
	struct dentry dm_dnode;
	const struct file_operations *dm_fops;
	void *dm_data;
	umode_t dm_mode;
	int dm_type;
};

static int
debugfs_attr(PFS_ATTR_ARGS)
{
	struct dentry_meta *dm;

	dm = pn->pn_data;

	vap->va_mode = dm->dm_mode;
	return (0);
}

static int
debugfs_destroy(PFS_DESTROY_ARGS)
{
	struct dentry_meta *dm;

	dm = pn->pn_data;
	if (dm->dm_type == DM_SYMLINK)
		free(dm->dm_data, M_DFSINT);

	free(dm, M_DFSINT);
	return (0);
}

static int
debugfs_fill(PFS_FILL_ARGS)
{
	struct dentry_meta *d;
	struct linux_file lf = {};
	struct vnode vn;
	char *buf;
	int rc;
	off_t off = 0;

	if ((rc = linux_set_current_flags(curthread, M_NOWAIT)))
		return (rc);

	d = pn->pn_data;
	vn.v_data = d->dm_data;

	rc = d->dm_fops->open(&vn, &lf);
	if (rc < 0) {
#ifdef INVARIANTS
		printf("%s:%d open failed with %d\n", __FUNCTION__, __LINE__, rc);
#endif
		return (-rc);
	}

	rc = -ENODEV;
	if (uio->uio_rw == UIO_READ && d->dm_fops->read) {
		rc = -ENOMEM;
		buf = (char *) malloc(sb->s_size, M_DFSINT, M_ZERO | M_NOWAIT);
		if (buf != NULL) {
			rc = d->dm_fops->read(&lf, buf, sb->s_size, &off);
			if (rc > 0)
				sbuf_bcpy(sb, buf, strlen(buf));

			free(buf, M_DFSINT);
		}
	} else if (uio->uio_rw == UIO_WRITE && d->dm_fops->write) {
		sbuf_finish(sb);
		rc = d->dm_fops->write(&lf, sbuf_data(sb), sbuf_len(sb), &off);
	}

	if (d->dm_fops->release)
		d->dm_fops->release(&vn, &lf);
	else
		single_release(&vn, &lf);

	if (rc < 0) {
#ifdef INVARIANTS
		printf("%s:%d read/write failed with %d\n", __FUNCTION__, __LINE__, rc);
#endif
		return (-rc);
	}
	return (0);
}

static int
debugfs_fill_data(PFS_FILL_ARGS)
{
	struct dentry_meta *dm;

	dm = pn->pn_data;
	sbuf_printf(sb, "%s", (char *)dm->dm_data);
	return (0);
}

struct dentry *
debugfs_create_file(const char *name, umode_t mode,
    struct dentry *parent, void *data,
    const struct file_operations *fops)
{
	struct dentry_meta *dm;
	struct dentry *dnode;
	struct pfs_node *pnode;
	int flags;

	dm = malloc(sizeof(*dm), M_DFSINT, M_NOWAIT | M_ZERO);
	if (dm == NULL)
		return (NULL);
	dnode = &dm->dm_dnode;
	dm->dm_fops = fops;
	dm->dm_data = data;
	dm->dm_mode = mode;
	dm->dm_type = DM_FILE;
	if (parent != NULL)
		pnode = parent->d_pfs_node;
	else
		pnode = debugfs_root;

	flags = fops->write ? PFS_RDWR : PFS_RD;
	dnode->d_pfs_node = pfs_create_file(pnode, name, debugfs_fill,
	    debugfs_attr, NULL, debugfs_destroy, flags | PFS_NOWAIT);
	if (dnode->d_pfs_node == NULL) {
		free(dm, M_DFSINT);
		return (NULL);
	}
	dnode->d_pfs_node->pn_data = dm;

	return (dnode);
}

struct dentry *
debugfs_create_file_size(const char *name, umode_t mode,
    struct dentry *parent, void *data,
    const struct file_operations *fops,
    loff_t file_size __unused)
{

	return debugfs_create_file(name, mode, parent, data, fops);
}

/*
 * NOTE: Files created with the _unsafe moniker will not be protected from
 * debugfs core file removals. It is the responsibility of @fops to protect
 * its file using debugfs_file_get() and debugfs_file_put().
 *
 * FreeBSD's LinuxKPI lindebugfs does not perform file removals at the time
 * of writing. Therefore there is no difference between functions with _unsafe
 * and functions without _unsafe when using lindebugfs. Functions with _unsafe
 * exist only for Linux compatibility.
 */
struct dentry *
debugfs_create_file_unsafe(const char *name, umode_t mode,
    struct dentry *parent, void *data,
    const struct file_operations *fops)
{

	return (debugfs_create_file(name, mode, parent, data, fops));
}

struct dentry *
debugfs_create_mode_unsafe(const char *name, umode_t mode,
    struct dentry *parent, void *data,
    const struct file_operations *fops,
    const struct file_operations *fops_ro,
    const struct file_operations *fops_wo)
{
	umode_t read = mode & S_IRUGO;
	umode_t write = mode & S_IWUGO;

	if (read && !write)
		return (debugfs_create_file_unsafe(name, mode, parent, data, fops_ro));

	if (write && !read)
		return (debugfs_create_file_unsafe(name, mode, parent, data, fops_wo));

	return (debugfs_create_file_unsafe(name, mode, parent, data, fops));
}

struct dentry *
debugfs_create_dir(const char *name, struct dentry *parent)
{
	struct dentry_meta *dm;
	struct dentry *dnode;
	struct pfs_node *pnode;

	dm = malloc(sizeof(*dm), M_DFSINT, M_NOWAIT | M_ZERO);
	if (dm == NULL)
		return (NULL);
	dnode = &dm->dm_dnode;
	dm->dm_mode = 0700;
	dm->dm_type = DM_DIR;
	if (parent != NULL)
		pnode = parent->d_pfs_node;
	else
		pnode = debugfs_root;

	dnode->d_pfs_node = pfs_create_dir(pnode, name, debugfs_attr, NULL, debugfs_destroy, PFS_RD | PFS_NOWAIT);
	if (dnode->d_pfs_node == NULL) {
		free(dm, M_DFSINT);
		return (NULL);
	}
	dnode->d_pfs_node->pn_data = dm;
	return (dnode);
}

struct dentry *
debugfs_create_symlink(const char *name, struct dentry *parent,
    const char *dest)
{
	struct dentry_meta *dm;
	struct dentry *dnode;
	struct pfs_node *pnode;
	void *data;

	data = strdup_flags(dest, M_DFSINT, M_NOWAIT);
	if (data == NULL)
		return (NULL);
	dm = malloc(sizeof(*dm), M_DFSINT, M_NOWAIT | M_ZERO);
	if (dm == NULL)
		goto fail1;
	dnode = &dm->dm_dnode;
	dm->dm_mode = 0700;
	dm->dm_type = DM_SYMLINK;
	dm->dm_data = data;
	if (parent != NULL)
		pnode = parent->d_pfs_node;
	else
		pnode = debugfs_root;

	dnode->d_pfs_node = pfs_create_link(pnode, name, &debugfs_fill_data, NULL, NULL, NULL, PFS_NOWAIT);
	if (dnode->d_pfs_node == NULL)
		goto fail;
	dnode->d_pfs_node->pn_data = dm;
	return (dnode);
 fail:
	free(dm, M_DFSINT);
 fail1:
	free(data, M_DFSINT);
	return (NULL);
}

void
debugfs_remove(struct dentry *dnode)
{
	if (dnode == NULL)
		return;

	pfs_destroy(dnode->d_pfs_node);
}

void
debugfs_remove_recursive(struct dentry *dnode)
{
	if (dnode == NULL)
		return;

	pfs_destroy(dnode->d_pfs_node);
}

static int
debugfs_bool_get(void *data, uint64_t *ullval)
{
	bool *bval = data;

	if (*bval)
		*ullval = 1;
	else
		*ullval = 0;

	return (0);
}

static int
debugfs_bool_set(void *data, uint64_t ullval)
{
	bool *bval = data;

	if (ullval)
		*bval = 1;
	else
		*bval = 0;

	return (0);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_bool, debugfs_bool_get, debugfs_bool_set, "%llu\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_bool_ro, debugfs_bool_get, NULL, "%llu\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_bool_wo, NULL, debugfs_bool_set, "%llu\n");

void
debugfs_create_bool(const char *name, umode_t mode, struct dentry *parent, bool *value)
{

	debugfs_create_mode_unsafe(name, mode, parent, value, &fops_bool,
	    &fops_bool_ro, &fops_bool_wo);
}


static int
debugfs_u8_get(void *data, uint64_t *value)
{
	uint8_t *u8data = data;
	*value = *u8data;
	return (0);
}

static int
debugfs_u8_set(void *data, uint64_t value)
{
	uint8_t *u8data = data;
	*u8data = (uint8_t)value;
	return (0);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_u8, debugfs_u8_get, debugfs_u8_set, "%u\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_u8_ro, debugfs_u8_get, NULL, "%u\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_u8_wo, NULL, debugfs_u8_set, "%u\n");

void
debugfs_create_u8(const char *name, umode_t mode, struct dentry *parent, uint8_t *value)
{

	debugfs_create_mode_unsafe(name, mode, parent, value, &fops_u8,
	    &fops_u8_ro, &fops_u8_wo);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_x8, debugfs_u8_get, debugfs_u8_set, "0x%016llx\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_x8_ro, debugfs_u8_get, NULL, "0x%016llx\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_x8_wo, NULL, debugfs_u8_set, "0x%016llx\n");

void
debugfs_create_x8(const char *name, umode_t mode, struct dentry *parent, uint8_t *value)
{

	debugfs_create_mode_unsafe(name, mode, parent, value, &fops_x8,
	    &fops_x8_ro, &fops_x8_wo);
}


static int
debugfs_u16_get(void *data, uint64_t *value)
{
	uint16_t *u16data = data;
	*value = *u16data;
	return (0);
}

static int
debugfs_u16_set(void *data, uint64_t value)
{
	uint16_t *u16data = data;
	*u16data = (uint16_t)value;
	return (0);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_u16, debugfs_u16_get, debugfs_u16_set, "%u\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_u16_ro, debugfs_u16_get, NULL, "%u\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_u16_wo, NULL, debugfs_u16_set, "%u\n");

void
debugfs_create_u16(const char *name, umode_t mode, struct dentry *parent, uint16_t *value)
{

	debugfs_create_mode_unsafe(name, mode, parent, value, &fops_u16,
	    &fops_u16_ro, &fops_u16_wo);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_x16, debugfs_u16_get, debugfs_u16_set, "0x%016llx\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_x16_ro, debugfs_u16_get, NULL, "0x%016llx\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_x16_wo, NULL, debugfs_u16_set, "0x%016llx\n");

void
debugfs_create_x16(const char *name, umode_t mode, struct dentry *parent, uint16_t *value)
{

	debugfs_create_mode_unsafe(name, mode, parent, value, &fops_x16,
	    &fops_x16_ro, &fops_x16_wo);
}


static int
debugfs_u32_get(void *data, uint64_t *value)
{
	uint32_t *u32data = data;
	*value = *u32data;
	return (0);
}

static int
debugfs_u32_set(void *data, uint64_t value)
{
	uint32_t *u32data = data;
	*u32data = (uint32_t)value;
	return (0);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_u32, debugfs_u32_get, debugfs_u32_set, "%u\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_u32_ro, debugfs_u32_get, NULL, "%u\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_u32_wo, NULL, debugfs_u32_set, "%u\n");

void
debugfs_create_u32(const char *name, umode_t mode, struct dentry *parent, uint32_t *value)
{

	debugfs_create_mode_unsafe(name, mode, parent, value, &fops_u32,
	    &fops_u32_ro, &fops_u32_wo);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_x32, debugfs_u32_get, debugfs_u32_set, "0x%016llx\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_x32_ro, debugfs_u32_get, NULL, "0x%016llx\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_x32_wo, NULL, debugfs_u32_set, "0x%016llx\n");

void
debugfs_create_x32(const char *name, umode_t mode, struct dentry *parent, uint32_t *value)
{

	debugfs_create_mode_unsafe(name, mode, parent, value, &fops_x32,
	    &fops_x32_ro, &fops_x32_wo);
}


static int
debugfs_u64_get(void *data, uint64_t *value)
{
	uint64_t *u64data = data;
	*value = *u64data;
	return (0);
}

static int
debugfs_u64_set(void *data, uint64_t value)
{
	uint64_t *u64data = data;
	*u64data = (uint64_t)value;
	return (0);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_u64, debugfs_u64_get, debugfs_u64_set, "%u\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_u64_ro, debugfs_u64_get, NULL, "%u\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_u64_wo, NULL, debugfs_u64_set, "%u\n");

void
debugfs_create_u64(const char *name, umode_t mode, struct dentry *parent, uint64_t *value)
{

	debugfs_create_mode_unsafe(name, mode, parent, value, &fops_u64,
	    &fops_u64_ro, &fops_u64_wo);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_x64, debugfs_u64_get, debugfs_u64_set, "0x%016llx\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_x64_ro, debugfs_u64_get, NULL, "0x%016llx\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_x64_wo, NULL, debugfs_u64_set, "0x%016llx\n");

void
debugfs_create_x64(const char *name, umode_t mode, struct dentry *parent, uint64_t *value)
{

	debugfs_create_mode_unsafe(name, mode, parent, value, &fops_x64,
	    &fops_x64_ro, &fops_x64_wo);
}


static int
debugfs_ulong_get(void *data, uint64_t *value)
{
	uint64_t *uldata = data;
	*value = *uldata;
	return (0);
}

static int
debugfs_ulong_set(void *data, uint64_t value)
{
	uint64_t *uldata = data;
	*uldata = value;
	return (0);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_ulong, debugfs_ulong_get, debugfs_ulong_set, "%llu\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_ulong_ro, debugfs_ulong_get, NULL, "%llu\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_ulong_wo, NULL, debugfs_ulong_set, "%llu\n");

void
debugfs_create_ulong(const char *name, umode_t mode, struct dentry *parent, unsigned long *value)
{

	debugfs_create_mode_unsafe(name, mode, parent, value, &fops_ulong,
	    &fops_ulong_ro, &fops_ulong_wo);
}


static int
debugfs_atomic_t_get(void *data, uint64_t *value)
{
	atomic_t *atomic_data = data;
	*value = atomic_read(atomic_data);
	return (0);
}

static int
debugfs_atomic_t_set(void *data, uint64_t value)
{
	atomic_t *atomic_data = data;
	atomic_set(atomic_data, (int)value);
	return (0);
}

DEFINE_DEBUGFS_ATTRIBUTE(fops_atomic_t, debugfs_atomic_t_get, debugfs_atomic_t_set, "%d\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_atomic_t_ro, debugfs_atomic_t_get, NULL, "%d\n");
DEFINE_DEBUGFS_ATTRIBUTE(fops_atomic_t_wo, NULL, debugfs_atomic_t_set, "%d\n");

void
debugfs_create_atomic_t(const char *name, umode_t mode, struct dentry *parent, atomic_t *value)
{

	debugfs_create_mode_unsafe(name, mode, parent, value, &fops_atomic_t,
	    &fops_atomic_t_ro, &fops_atomic_t_wo);
}


static ssize_t
fops_blob_read(struct file *filp, char __user *ubuf, size_t read_size, loff_t *ppos)
{
	struct debugfs_blob_wrapper *blob;

	blob = filp->private_data;
	if (blob == NULL)
		return (-EINVAL);
	if (blob->size == 0 || blob->data == NULL)
		return (-EINVAL);

	return (simple_read_from_buffer(ubuf, read_size, ppos, blob->data, blob->size));
}

static int
fops_blob_open(struct inode *inode, struct file *filp)
{

	return (simple_open(inode, filp));
}

static const struct file_operations __fops_blob_ro = {
	.owner = THIS_MODULE,
	.open = fops_blob_open,
	.read = fops_blob_read,
	.llseek = no_llseek
};

struct dentry *
debugfs_create_blob(const char *name, umode_t mode, struct dentry *parent,
    struct debugfs_blob_wrapper *value)
{
	/* Blobs are read-only. */
	return (debugfs_create_file(name, mode & 0444, parent, value, &__fops_blob_ro));
}


static int
lindebugfs_init(PFS_INIT_ARGS)
{

	debugfs_root = pi->pi_root;

	(void)debugfs_create_symlink("kcov", NULL, "/dev/kcov");

	return (0);
}

static int
lindebugfs_uninit(PFS_INIT_ARGS)
{

	return (0);
}

PSEUDOFS(lindebugfs, 1, VFCF_JAIL);
MODULE_DEPEND(lindebugfs, linuxkpi, 1, 1, 1);
