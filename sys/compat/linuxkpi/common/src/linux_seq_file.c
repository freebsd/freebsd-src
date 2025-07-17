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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/sbuf.h>
#include <sys/syslog.h>
#include <sys/vnode.h>

#include <linux/seq_file.h>
#include <linux/file.h>

#undef file
MALLOC_DEFINE(M_LSEQ, "seq_file", "seq_file");

ssize_t
seq_read(struct linux_file *f, char *ubuf, size_t size, off_t *ppos)
{
	struct seq_file *m;
	struct sbuf *sbuf;
	void *p;
	ssize_t rc;

	m = f->private_data;
	sbuf = m->buf;

	p = m->op->start(m, ppos);
	rc = m->op->show(m, p);
	if (rc)
		return (rc);

	rc = sbuf_finish(sbuf);
	if (rc)
		return (rc);

	rc = sbuf_len(sbuf);
	if (*ppos >= rc || size < 1)
		return (-EINVAL);

	size = min(rc - *ppos, size);
	rc = strscpy(ubuf, sbuf_data(sbuf) + *ppos, size + 1);

	/* add 1 for null terminator */
	if (rc > 0)
		rc += 1;

	return (rc);
}

int
seq_write(struct seq_file *seq, const void *data, size_t len)
{
	int ret;

	ret = sbuf_bcpy(seq->buf, data, len);
	if (ret == 0)
		seq->size = sbuf_len(seq->buf);

	return (ret);
}

void
seq_putc(struct seq_file *seq, char c)
{
	int ret;

	ret = sbuf_putc(seq->buf, c);
	if (ret == 0)
		seq->size = sbuf_len(seq->buf);
}

void
seq_puts(struct seq_file *seq, const char *str)
{
	int ret;

	ret = sbuf_printf(seq->buf, "%s", str);
	if (ret == 0)
		seq->size = sbuf_len(seq->buf);
}

/*
 * This only needs to be a valid address for lkpi
 * drivers it should never actually be called
 */
off_t
seq_lseek(struct linux_file *file, off_t offset, int whence)
{

	panic("%s not supported\n", __FUNCTION__);
	return (0);
}

static void *
single_start(struct seq_file *p, off_t *pos)
{

	return ((void *)(uintptr_t)(*pos == 0));
}

static void *
single_next(struct seq_file *p, void *v, off_t *pos)
{

	++*pos;
	return (NULL);
}

static void
single_stop(struct seq_file *p, void *v)
{
}

static int
_seq_open_without_sbuf(struct linux_file *f, const struct seq_operations *op)
{
	struct seq_file *p;

	if ((p = malloc(sizeof(*p), M_LSEQ, M_NOWAIT|M_ZERO)) == NULL)
		return (-ENOMEM);

	p->file = f;
	p->op = op;
	f->private_data = (void *) p;
	return (0);
}

int
seq_open(struct linux_file *f, const struct seq_operations *op)
{
	int ret;

	ret = _seq_open_without_sbuf(f, op);
	if (ret == 0)
		((struct seq_file *)f->private_data)->buf = sbuf_new_auto();

	return (ret);
}

void *
__seq_open_private(struct linux_file *f, const struct seq_operations *op, int size)
{
	struct seq_file *seq_file;
	void *private;
	int error;

	private = malloc(size, M_LSEQ, M_NOWAIT|M_ZERO);
	if (private == NULL)
		return (NULL);

	error = seq_open(f, op);
	if (error < 0) {
		free(private, M_LSEQ);
		return (NULL);
	}

	seq_file = (struct seq_file *)f->private_data;
	seq_file->private = private;

	return (private);
}

static int
_single_open_without_sbuf(struct linux_file *f, int (*show)(struct seq_file *, void *), void *d)
{
	struct seq_operations *op;
	int rc = -ENOMEM;

	op = malloc(sizeof(*op), M_LSEQ, M_NOWAIT);
	if (op) {
		op->start = single_start;
		op->next = single_next;
		op->stop = single_stop;
		op->show = show;
		rc = _seq_open_without_sbuf(f, op);
		if (rc)
			free(op, M_LSEQ);
		else
			((struct seq_file *)f->private_data)->private = d;
	}
	return (rc);
}

int
single_open(struct linux_file *f, int (*show)(struct seq_file *, void *), void *d)
{
	int ret;

	ret = _single_open_without_sbuf(f, show, d);
	if (ret == 0)
		((struct seq_file *)f->private_data)->buf = sbuf_new_auto();

	return (ret);
}

int
single_open_size(struct linux_file *f, int (*show)(struct seq_file *, void *), void *d, size_t size)
{
	int ret;

	ret = _single_open_without_sbuf(f, show, d);
	if (ret == 0)
		((struct seq_file *)f->private_data)->buf = sbuf_new(
		    NULL, NULL, size, SBUF_AUTOEXTEND);

	return (ret);
}

int
seq_release(struct inode *inode __unused, struct linux_file *file)
{
	struct seq_file *m;
	struct sbuf *s;

	m = file->private_data;
	s = m->buf;

	sbuf_delete(s);
	free(m, M_LSEQ);

	return (0);
}

int
seq_release_private(struct inode *inode __unused, struct linux_file *f)
{
	struct seq_file *seq;

	seq = (struct seq_file *)f->private_data;
	free(seq->private, M_LSEQ);
	return (seq_release(inode, f));
}

int
single_release(struct vnode *v, struct linux_file *f)
{
	const struct seq_operations *op;
	struct seq_file *m;
	int rc;

	/* be NULL safe */
	if ((m = f->private_data) == NULL)
		return (0);

	op = m->op;
	rc = seq_release(v, f);
	free(__DECONST(void *, op), M_LSEQ);
	return (rc);
}

void
lkpi_seq_vprintf(struct seq_file *m, const char *fmt, va_list args)
{
	int ret;

	ret = sbuf_vprintf(m->buf, fmt, args);
	if (ret == 0)
		m->size = sbuf_len(m->buf);
}

void
lkpi_seq_printf(struct seq_file *m, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	lkpi_seq_vprintf(m, fmt, args);
	va_end(args);
}

bool
seq_has_overflowed(struct seq_file *m)
{
	return (sbuf_len(m->buf) == -1);
}
