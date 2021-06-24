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

#if defined(__amd64__) || defined(__i386__)
#include <machine/cpufunc.h>
#elif defined(__aarch64__)
#include <machine/armreg.h>
#endif

#include <stand.h>

/* Buffer for holding tslog data in string format. */
static char * tslog_buf = NULL;
static size_t tslog_buflen = 0;
static size_t tslog_bufpos = 0;

void
tslog_setbuf(void * buf, size_t len)
{

	tslog_buf = (char *)buf;
	tslog_buflen = len;
	tslog_bufpos = 0;
}

void
tslog_getbuf(void ** buf, size_t * len)
{

	*buf = (void *)tslog_buf;
	*len = tslog_bufpos;
}

void
tslog(const char * type, const char * f, const char * s)
{
#if defined(__amd64__) || defined(__i386__)
	uint64_t tsc = rdtsc();
#elif defined(__aarch64__)
	uint64_t tsc = READ_SPECIALREG(cntvct_el0);
#else
	uint64_t tsc = 0;
#endif
	int len;

	/* If we have no buffer, do nothing. */
	if (tslog_buf == NULL)
		return;

	/* Append to existing buffer, if we have enough space. */
	len = snprintf(&tslog_buf[tslog_bufpos],
	    tslog_buflen - tslog_bufpos, "0x0 %llu %s %s%s%s\n",
	    (unsigned long long)tsc, type, f, s ? " " : "", s ? s : "");
	if ((len > 0) && (tslog_bufpos + len <= tslog_buflen))
		tslog_bufpos += len;
}
