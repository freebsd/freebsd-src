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
 */

#ifndef _LINUXKPI_LINUX_SEQ_FILE_H_
#define _LINUXKPI_LINUX_SEQ_FILE_H_

#include <sys/types.h>
#include <sys/sbuf.h>

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/string_helpers.h>
#include <linux/printk.h>

#undef file
#define inode vnode

MALLOC_DECLARE(M_LSEQ);

#define	DEFINE_SHOW_ATTRIBUTE(__name)					\
static int __name ## _open(struct inode *inode, struct linux_file *file)	\
{									\
	return single_open(file, __name ## _show, inode->i_private);	\
}									\
									\
static const struct file_operations __name ## _fops = {			\
	.owner		= THIS_MODULE,					\
	.open		= __name ## _open,				\
	.read		= seq_read,					\
	.llseek		= seq_lseek,					\
	.release	= single_release,				\
}

struct seq_file {
	struct sbuf *buf;
	size_t size;
	const struct seq_operations *op;
	const struct linux_file *file;
	void *private;
};

struct seq_operations {
	void * (*start) (struct seq_file *m, off_t *pos);
	void (*stop) (struct seq_file *m, void *v);
	void * (*next) (struct seq_file *m, void *v, off_t *pos);
	int (*show) (struct seq_file *m, void *v);
};

ssize_t seq_read(struct linux_file *, char *, size_t, off_t *);
int seq_write(struct seq_file *seq, const void *data, size_t len);
void seq_putc(struct seq_file *m, char c);
void seq_puts(struct seq_file *m, const char *str);
bool seq_has_overflowed(struct seq_file *m);

void *__seq_open_private(struct linux_file *, const struct seq_operations *, int);
int seq_release_private(struct inode *, struct linux_file *);

int seq_open(struct linux_file *f, const struct seq_operations *op);
int seq_release(struct inode *inode, struct linux_file *file);

off_t seq_lseek(struct linux_file *file, off_t offset, int whence);
int single_open(struct linux_file *, int (*)(struct seq_file *, void *), void *);
int single_open_size(struct linux_file *, int (*)(struct seq_file *, void *), void *, size_t);
int single_release(struct inode *, struct linux_file *);

void lkpi_seq_vprintf(struct seq_file *m, const char *fmt, va_list args);
void lkpi_seq_printf(struct seq_file *m, const char *fmt, ...);

#define	seq_vprintf(...)	lkpi_seq_vprintf(__VA_ARGS__)
#define	seq_printf(...)		lkpi_seq_printf(__VA_ARGS__)

int __lkpi_hexdump_sbuf_printf(void *, const char *, ...) __printflike(2, 3);

static inline void
seq_hex_dump(struct seq_file *m, const char *prefix_str, int prefix_type,
    int rowsize, int groupsize, const void *buf, size_t len, bool ascii)
{
	lkpi_hex_dump(__lkpi_hexdump_sbuf_printf, m->buf, NULL, prefix_str, prefix_type,
	    rowsize, groupsize, buf, len, ascii);
}

#define	file			linux_file

#endif	/* _LINUXKPI_LINUX_SEQ_FILE_H_ */
