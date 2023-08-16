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

static size_t
tsccat(char * buf, uint64_t tsc)
{
	size_t len;

	/* Handle upper digits. */
	if (tsc >= 10)
		len = tsccat(buf, tsc / 10);
	else
		len = 0;

	/* Write the last digit. */
	buf[len] = "0123456789"[tsc % 10];

	/* Return the length written. */
	return (len + 1);
}

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

	/* If we have no buffer, do nothing. */
	if (tslog_buf == NULL)
		return;

	/* Check that we have enough space. */
	if (tslog_buflen - tslog_bufpos < 32 + strlen(type) + strlen(f) +
	    (s ? strlen(s) : 0))
		return;

	/* Append to existing buffer. */
	strcpy(&tslog_buf[tslog_bufpos], "0x0 ");
	tslog_bufpos += 4;
	tslog_bufpos += tsccat(&tslog_buf[tslog_bufpos], tsc);
	strcpy(&tslog_buf[tslog_bufpos], " ");
	tslog_bufpos += 1;
	strcpy(&tslog_buf[tslog_bufpos], type);
	tslog_bufpos += strlen(type);
	strcpy(&tslog_buf[tslog_bufpos], " ");
	tslog_bufpos += 1;
	strcpy(&tslog_buf[tslog_bufpos], f);
	tslog_bufpos += strlen(f);
	if (s != NULL) {
		strcpy(&tslog_buf[tslog_bufpos], " ");
		tslog_bufpos += 1;
		strcpy(&tslog_buf[tslog_bufpos], s);
		tslog_bufpos += strlen(s);
	}
	strcpy(&tslog_buf[tslog_bufpos], "\n");
	tslog_bufpos += 1;
}
