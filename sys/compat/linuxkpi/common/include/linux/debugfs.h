/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
 * $FreeBSD$
 */

#ifndef _LINUXKPI_LINUX_DEBUGFS_H_
#define _LINUXKPI_LINUX_DEBUGFS_H_

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/types.h>

MALLOC_DECLARE(M_DFSINT);

struct debugfs_reg32 {
	char *name;
	unsigned long offset;
};

struct debugfs_regset32 {
	const struct debugfs_reg32 *regs;
	int nregs;
};

struct dentry *debugfs_create_file(const char *name, umode_t mode,
    struct dentry *parent, void *data,
    const struct file_operations *fops);

struct dentry *debugfs_create_file_unsafe(const char *name, umode_t mode,
    struct dentry *parent, void *data,
    const struct file_operations *fops);

struct dentry *debugfs_create_mode_unsafe(const char *name, umode_t mode,
    struct dentry *parent, void *data,
    const struct file_operations *fops,
    const struct file_operations *fops_ro,
    const struct file_operations *fops_wo);

struct dentry *debugfs_create_dir(const char *name, struct dentry *parent);

struct dentry *debugfs_create_symlink(const char *name, struct dentry *parent,
    const char *dest);

void debugfs_remove(struct dentry *dentry);

void debugfs_remove_recursive(struct dentry *dentry);

#define DEFINE_DEBUGFS_ATTRIBUTE(__fops, __get, __set, __fmt) \
	DEFINE_SIMPLE_ATTRIBUTE(__fops, __get, __set, __fmt)

void debugfs_create_bool(const char *name, umode_t mode, struct dentry *parent,
    bool *value);

void debugfs_create_ulong(const char *name, umode_t mode, struct dentry *parent,
    unsigned long *value);

#endif /* _LINUXKPI_LINUX_DEBUGFS_H_ */
