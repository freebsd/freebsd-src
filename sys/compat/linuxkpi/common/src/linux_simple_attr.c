/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022, Jake Freeland <jfree@freebsd.org>
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
 */

#include <sys/types.h>
#include <linux/fs.h>

MALLOC_DEFINE(M_LSATTR, "simple_attr", "Linux Simple Attribute File");

struct simple_attr {
	int (*get)(void *, uint64_t *);
	int (*set)(void *, uint64_t);
	void *data;
	const char *fmt;
	struct mutex mutex;
};

/*
 * simple_attr_open: open and populate simple attribute data
 *
 * @inode: file inode
 * @filp: file pointer
 * @get: ->get() for reading file data
 * @set: ->set() for writing file data
 * @fmt: format specifier for data returned by @get
 *
 * Memory allocate a simple_attr and appropriately initialize its members.
 * The simple_attr must be stored in filp->private_data.
 * Simple attr files do not support seeking. Open the file as nonseekable.
 *
 * Return value: simple attribute file descriptor
 */
int
simple_attr_open(struct inode *inode, struct file *filp,
    int (*get)(void *, uint64_t *), int (*set)(void *, uint64_t),
    const char *fmt)
{
	struct simple_attr *sattr;
	sattr = malloc(sizeof(*sattr), M_LSATTR, M_ZERO | M_NOWAIT);
	if (sattr == NULL)
		return (-ENOMEM);

	sattr->get = get;
	sattr->set = set;
	sattr->data = inode->i_private;
	sattr->fmt = fmt;
	mutex_init(&sattr->mutex);

	filp->private_data = (void *) sattr;

	return (nonseekable_open(inode, filp));
}

int
simple_attr_release(struct inode *inode, struct file *filp)
{
	free(filp->private_data, M_LSATTR);
	return (0);
}

/*
 * simple_attr_read: read simple attr data and transfer into buffer
 *
 * @filp: file pointer
 * @buf: kernel space buffer
 * @read_size: number of bytes to be transferred
 * @ppos: starting pointer position for transfer
 *
 * The simple_attr structure is stored in filp->private_data.
 * ->get() retrieves raw file data.
 * The ->fmt specifier can format this data to be human readable.
 * This output is then transferred into the @buf buffer.
 *
 * Return value:
 * On success, number of bytes transferred
 * On failure, negative signed ERRNO
 */
ssize_t
simple_attr_read(struct file *filp, char __user *buf, size_t read_size,
    loff_t *ppos)
{
	struct simple_attr *sattr;
	uint64_t data;
	ssize_t ret;
	char prebuf[24];

	sattr = filp->private_data;

	if (sattr->get == NULL)
		return (-EFAULT);

	mutex_lock(&sattr->mutex);

	ret = sattr->get(sattr->data, &data);
	if (ret)
		goto unlock;

	scnprintf(prebuf, sizeof(prebuf), sattr->fmt, data);

	/* add 1 for null terminator */
	ret = simple_read_from_buffer(buf, read_size, ppos, prebuf,
	    strlen(prebuf) + 1);

unlock:
	mutex_unlock(&sattr->mutex);
	return (ret);
}

/*
 * simple_attr_write_common: write contents of buffer into simple attribute file
 *
 * @filp: file pointer
 * @buf: kernel space buffer
 * @write_size: number bytes to be transferred
 * @ppos: starting pointer position for transfer
 * @is_signed: signedness of data in @buf
 *
 * The simple_attr structure is stored in filp->private_data.
 * Convert the @buf string to unsigned long long.
 * ->set() writes unsigned long long data into the simple attr file.
 *
 * Return value:
 * On success, number of bytes written to simple attr
 * On failure, negative signed ERRNO
 */
static ssize_t
simple_attr_write_common(struct file *filp, const char __user *ubuf,
    size_t write_size, loff_t *ppos, bool is_signed)
{
	struct simple_attr *sattr;
	unsigned long long data;
	char *buf;
	ssize_t ret;

	sattr = filp->private_data;

	if (sattr->set == NULL)
		return (-EFAULT);

	if (*ppos != 0 || write_size < 1)
		return (-EINVAL);

	buf = malloc(write_size, M_LSATTR, M_WAITOK);
	if (copy_from_user(buf, ubuf, write_size) != 0) {
		free(buf, M_LSATTR);
		return (-EFAULT);
	}
	if (strnlen(buf, write_size) == write_size) {
		free(buf, M_LSATTR);
		return (-EINVAL);
	}

	mutex_lock(&sattr->mutex);

	if (is_signed)
		ret = kstrtoll(buf, 0, &data);
	else
		ret = kstrtoull(buf, 0, &data);
	if (ret)
		goto unlock;

	ret = sattr->set(sattr->data, data);
	if (ret)
		goto unlock;

	ret = write_size;

unlock:
	mutex_unlock(&sattr->mutex);
	free(buf, M_LSATTR);
	return (ret);
}

ssize_t
simple_attr_write(struct file *filp, const char __user *buf, size_t write_size,
    loff_t *ppos)
{
	return (simple_attr_write_common(filp, buf,  write_size, ppos, false));
}

ssize_t
simple_attr_write_signed(struct file *filp, const char __user *buf,
    size_t write_size, loff_t *ppos)
{
	return (simple_attr_write_common(filp, buf,  write_size, ppos, true));
}
