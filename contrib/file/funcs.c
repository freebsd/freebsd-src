/*
 * Copyright (c) Christos Zoulas 2003.
 * All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "file.h"
#include "magic.h"
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef	lint
FILE_RCSID("@(#)$Id: funcs.c,v 1.13 2004/09/11 19:15:57 christos Exp $")
#endif	/* lint */
/*
 * Like printf, only we print to a buffer and advance it.
 */
protected int
file_printf(struct magic_set *ms, const char *fmt, ...)
{
	va_list ap;
	size_t len, size;
	char *buf;

	va_start(ap, fmt);

	if ((len = vsnprintf(ms->o.ptr, ms->o.left, fmt, ap)) >= ms->o.left) {
		long diff;  /* XXX: really ptrdiff_t */

		va_end(ap);
		size = (ms->o.size - ms->o.left) + len + 1024;
		if ((buf = realloc(ms->o.buf, size)) == NULL) {
			file_oomem(ms);
			return -1;
		}
		diff = ms->o.ptr - ms->o.buf;
		ms->o.ptr = buf + diff;
		ms->o.buf = buf;
		ms->o.left = size - diff;
		ms->o.size = size;

		va_start(ap, fmt);
		len = vsnprintf(ms->o.ptr, ms->o.left, fmt, ap);
	}
	ms->o.ptr += len;
	ms->o.left -= len;
	va_end(ap);
	return 0;
}

/*
 * error - print best error message possible
 */
/*VARARGS*/
protected void
file_error(struct magic_set *ms, int error, const char *f, ...)
{
	va_list va;
	/* Only the first error is ok */
	if (ms->haderr)
		return;
	va_start(va, f);
	(void)vsnprintf(ms->o.buf, ms->o.size, f, va);
	va_end(va);
	if (error > 0) {
		size_t len = strlen(ms->o.buf);
		(void)snprintf(ms->o.buf + len, ms->o.size - len, " (%s)",
		    strerror(error));
	}
	ms->haderr++;
	ms->error = error;
}


protected void
file_oomem(struct magic_set *ms)
{
	file_error(ms, errno, "cannot allocate memory");
}

protected void
file_badseek(struct magic_set *ms)
{
	file_error(ms, errno, "error seeking");
}

protected void
file_badread(struct magic_set *ms)
{
	file_error(ms, errno, "error reading");
}

#ifndef COMPILE_ONLY
protected int
file_buffer(struct magic_set *ms, const void *buf, size_t nb)
{
    int m;
    /* try compression stuff */
    if ((m = file_zmagic(ms, buf, nb)) == 0) {
	/* Check if we have a tar file */
	if ((m = file_is_tar(ms, buf, nb)) == 0) {
	    /* try tests in /etc/magic (or surrogate magic file) */
	    if ((m = file_softmagic(ms, buf, nb)) == 0) {
		/* try known keywords, check whether it is ASCII */
		if ((m = file_ascmagic(ms, buf, nb)) == 0) {
		    /* abandon hope, all ye who remain here */
		    if (file_printf(ms, ms->flags & MAGIC_MIME ?
			"application/octet-stream" : "data") == -1)
			    return -1;
		    m = 1;
		}
	    }
	}
    }
    return m;
}
#endif

protected int
file_reset(struct magic_set *ms)
{
	if (ms->mlist == NULL) {
		file_error(ms, 0, "no magic files loaded");
		return -1;
	}
	ms->o.ptr = ms->o.buf;
	ms->haderr = 0;
	ms->error = -1;
	return 0;
}

protected const char *
file_getbuffer(struct magic_set *ms)
{
	char *pbuf, *op, *np;
	size_t psize, len;

	if (ms->haderr)
		return NULL;

	if (ms->flags & MAGIC_RAW)
		return ms->o.buf;

	len = ms->o.size - ms->o.left;
	if (len > (SIZE_T_MAX - 1) / 4) {
		file_oomem(ms);
		return NULL;
	}
	/* * 4 is for octal representation, + 1 is for NUL */
	psize = len * 4 + 1;
	if (ms->o.psize < psize) {
		if ((pbuf = realloc(ms->o.pbuf, psize)) == NULL) {
			file_oomem(ms);
			return NULL;
		}
		ms->o.psize = psize;
		ms->o.pbuf = pbuf;
	}

	for (np = ms->o.pbuf, op = ms->o.buf; *op; op++) {
		if (isprint((unsigned char)*op)) {
			*np++ = *op;	
		} else {
			*np++ = '\\';
			*np++ = ((*op >> 6) & 3) + '0';
			*np++ = ((*op >> 3) & 7) + '0';
			*np++ = ((*op >> 0) & 7) + '0';
		}
	}
	*np = '\0';
	return ms->o.pbuf;
}
