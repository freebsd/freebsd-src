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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if defined(HAVE_WCHAR_H)
#include <wchar.h>
#endif
#if defined(HAVE_WCTYPE_H)
#include <wctype.h>
#endif

#ifndef	lint
FILE_RCSID("@(#)$Id: funcs.c,v 1.23 2006/12/11 21:48:49 christos Exp $")
#endif	/* lint */

#ifndef HAVE_VSNPRINTF
int vsnprintf(char *, size_t, const char *, va_list);
#endif

/*
 * Like printf, only we print to a buffer and advance it.
 */
protected int
file_printf(struct magic_set *ms, const char *fmt, ...)
{
	va_list ap;
	size_t len;
	char *buf;

	va_start(ap, fmt);

	if ((len = vsnprintf(ms->o.ptr, ms->o.len, fmt, ap)) >= ms->o.len) {
		va_end(ap);
		if ((buf = realloc(ms->o.buf, len + 1024)) == NULL) {
			file_oomem(ms, len + 1024);
			return -1;
		}
		ms->o.ptr = buf + (ms->o.ptr - ms->o.buf);
		ms->o.buf = buf;
		ms->o.len = ms->o.size - (ms->o.ptr - ms->o.buf);
		ms->o.size = len + 1024;

		va_start(ap, fmt);
		len = vsnprintf(ms->o.ptr, ms->o.len, fmt, ap);
	}
	ms->o.ptr += len;
	ms->o.len -= len;
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
file_oomem(struct magic_set *ms, size_t len)
{
	file_error(ms, errno, "cannot allocate %zu bytes", len);
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
file_buffer(struct magic_set *ms, int fd, const void *buf, size_t nb)
{
    int m;
    /* try compression stuff */
    if ((m = file_zmagic(ms, fd, buf, nb)) == 0) {
	/* Check if we have a tar file */
	if ((m = file_is_tar(ms, buf, nb)) == 0) {
	    /* try tests in /etc/magic (or surrogate magic file) */
	    if ((m = file_softmagic(ms, buf, nb)) == 0) {
		/* try known keywords, check whether it is ASCII */
		if ((m = file_ascmagic(ms, buf, nb)) == 0) {
		    /* abandon hope, all ye who remain here */
		    if (file_printf(ms, ms->flags & MAGIC_MIME ?
			(nb ? "application/octet-stream" :
			    "application/empty") :
			(nb ? "data" :
			    "empty")) == -1)
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

#define OCTALIFY(n, o)	\
	/*LINTED*/ \
	(void)(*(n)++ = '\\', \
	*(n)++ = (((uint32_t)*(o) >> 6) & 3) + '0', \
	*(n)++ = (((uint32_t)*(o) >> 3) & 7) + '0', \
	*(n)++ = (((uint32_t)*(o) >> 0) & 7) + '0', \
	(o)++)

protected const char *
file_getbuffer(struct magic_set *ms)
{
	char *nbuf, *op, *np;
	size_t nsize;

	if (ms->haderr)
		return NULL;

	if (ms->flags & MAGIC_RAW)
		return ms->o.buf;

	nsize = ms->o.len * 4 + 1;
	if (ms->o.psize < nsize) {
		if ((nbuf = realloc(ms->o.pbuf, nsize)) == NULL) {
			file_oomem(ms, nsize);
			return NULL;
		}
		ms->o.psize = nsize;
		ms->o.pbuf = nbuf;
	}

#if defined(HAVE_WCHAR_H) && defined(HAVE_MBRTOWC) && defined(HAVE_WCWIDTH)
	{
		mbstate_t state;
		wchar_t nextchar;
		int mb_conv = 1;
		size_t bytesconsumed;
		char *eop;
		(void)memset(&state, 0, sizeof(mbstate_t));

		np = ms->o.pbuf;
		op = ms->o.buf;
		eop = op + strlen(ms->o.buf);

		while (op < eop) {
			bytesconsumed = mbrtowc(&nextchar, op,
			    (size_t)(eop - op), &state);
			if (bytesconsumed == (size_t)(-1) ||
			    bytesconsumed == (size_t)(-2)) {
				mb_conv = 0;
				break;
			}

			if (iswprint(nextchar)) {
				(void)memcpy(np, op, bytesconsumed);
				op += bytesconsumed;
				np += bytesconsumed;
			} else {
				while (bytesconsumed-- > 0)
					OCTALIFY(np, op);
			}
		}
		*np = '\0';

		/* Parsing succeeded as a multi-byte sequence */
		if (mb_conv != 0)
			return ms->o.pbuf;
	}
#endif

	for (np = ms->o.pbuf, op = ms->o.buf; *op; op++) {
		if (isprint((unsigned char)*op)) {
			*np++ = *op;	
		} else {
			OCTALIFY(np, op);
		}
	}
	*np = '\0';
	return ms->o.pbuf;
}

/*
 * Yes these wrappers suffer from buffer overflows, but if your OS does not have
 * the real functions, maybe you should consider replacing your OS?
 */
#ifndef HAVE_VSNPRINTF
int
vsnprintf(char *buf, size_t len, const char *fmt, va_list ap)
{
	return vsprintf(buf, fmt, ap);
}
#endif

#ifndef HAVE_SNPRINTF
/*ARGSUSED*/
int
snprintf(char *buf, size_t len, const char *fmt, ...)
{
	int rv;
	va_list ap;
	va_start(ap, fmt);
	rv = vsprintf(buf, fmt, ap);
	va_end(ap);
	return rv;
}
#endif
