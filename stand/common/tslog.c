/*-
 * Copyright (c) 2021 Colin Percival
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <stand.h>

#include "bootstrap.h"

/* Buffer for holding tslog data in string format. */
#ifndef LOADER_TSLOGSIZE
#define LOADER_TSLOGSIZE (2 * 1024 * 1024)
#endif

int
tslog_init(void)
{
	void * tslog_buf;

	/* Allocate buffer and pass to libsa tslog code. */
	if ((tslog_buf = malloc(LOADER_TSLOGSIZE)) == NULL)
		return (-1);
	tslog_setbuf(tslog_buf, LOADER_TSLOGSIZE);

	/* Record this as when we entered the loader. */
	TSRAW("ENTER", "loader", NULL);

	return (0);
}

/*
 * Pass our tslog buffer as a preloaded "module" to the kernel.  This should
 * be called as late as possible prior to the kernel being executed, since
 * any timestamps logged after this is called will not be visible to the
 * kernel.
 */
int
tslog_publish(void)
{
	void * tslog_buf;
	size_t tslog_bufpos;

	/* Record a log entry for ending logging. */
	TSRAW("EXIT", "loader", NULL);

	/* Get the buffer and its current length. */
	tslog_getbuf(&tslog_buf, &tslog_bufpos);

	/* If we never allocated a buffer, return an error. */
	if (tslog_buf == NULL)
		return (-1);

	/* Store the buffer where the kernel can read it. */
	return (file_addbuf("TSLOG", "TSLOG data", tslog_bufpos, tslog_buf));
}
