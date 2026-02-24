/*-
 * Copyright (c) 2025 The FreeBSD Foundation
 * Copyright (c) 2025 Jean-Sébastien Pédron
 *
 * This software was developed by Jean-Sébastien Pédron under sponsorship
 * from the FreeBSD Foundation.
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

#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>

#include <linux/eventfd.h>

struct eventfd_ctx *
lkpi_eventfd_ctx_fdget(int fd)
{
	struct file *fp;
	struct eventfd_ctx *ctx;

	/* Lookup file pointer by file descriptor index. */
	if (fget_unlocked(curthread, fd, &cap_no_rights, &fp) != 0)
		return (ERR_PTR(-EBADF));

	/*
	 * eventfd_get() bumps the refcount, so we can safely release the
	 * reference on the file itself afterwards.
	 */
	ctx = eventfd_get(fp);
	fdrop(fp, curthread);

	if (ctx == NULL)
		return (ERR_PTR(-EBADF));

	return (ctx);
}

void
lkpi_eventfd_ctx_put(struct eventfd_ctx *ctx)
{
	eventfd_put(ctx);
}
