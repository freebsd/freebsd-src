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
#if defined(HAVE_LIMITS_H)
#include <limits.h>
#endif
#ifndef SIZE_T_MAX
#ifdef __LP64__
#define SIZE_T_MAX (size_t)0xfffffffffffffffffU
#else
#define SIZE_T_MAX (size_t)0xffffffffU
#endif
#endif

#ifndef	lint
FILE_RCSID("@(#)$File: funcs.c,v 1.32 2007/05/24 17:22:27 christos Exp $")
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
	size_t len, size;
	char *buf;

	va_start(ap, fmt);

	if ((len = vsnprintf(ms->o.ptr, ms->o.left, fmt, ap)) >= ms->o.left) {
		long diff;	/* XXX: really ptrdiff_t */

		va_end(ap);
		size = (ms->o.size - ms->o.left) + len + 1024;
		if ((buf = realloc(ms->o.buf, size)) == NULL) {
			file_oomem(ms, size);
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
	va_end(ap);
	ms->o.ptr += len;
	ms->o.left -= len;
	return 0;
}

/*
 * error - print best error message possible
 */
/*VARARGS*/
private void
file_error_core(struct magic_set *ms, int error, const char *f, va_list va,
    uint32_t lineno)
{
	size_t len;
	/* Only the first error is ok */
	if (ms->haderr)
		return;
	len = 0;
	if (lineno != 0) {
		(void)snprintf(ms->o.buf, ms->o.size, "line %u: ", lineno);
		len = strlen(ms->o.buf);
	}
	(void)vsnprintf(ms->o.buf + len, ms->o.size - len, f, va);
	if (error > 0) {
		len = strlen(ms->o.buf);
		(void)snprintf(ms->o.buf + len, ms->o.size - len, " (%s)",
		    strerror(error));
	}
	ms->haderr++;
	ms->error = error;
}

/*VARARGS*/
protected void
file_error(struct magic_set *ms, int error, const char *f, ...)
{
	va_list va;
	va_start(va, f);
	file_error_core(ms, error, f, va, 0);
	va_end(va);
}

/*
 * Print an error with magic line number.
 */
/*VARARGS*/
protected void
file_magerror(struct magic_set *ms, const char *f, ...)
{
	va_list va;
	va_start(va, f);
	file_error_core(ms, 0, f, va, ms->line);
	va_end(va);
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
file_buffer(struct magic_set *ms, int fd, const char *inname, const void *buf,
    size_t nb)
{
    int m;

#ifdef __EMX__
    if ((ms->flags & MAGIC_NO_CHECK_APPTYPE) == 0 && inname) {
	switch (file_os2_apptype(ms, inname, buf, nb)) {
	case -1:
	    return -1;
	case 0:
	    break;
	default:
	    return 1;
	}
    }
#endif

    /* try compression stuff */
    if ((ms->flags & MAGIC_NO_CHECK_COMPRESS) != 0 ||
        (m = file_zmagic(ms, fd, inname, buf, nb)) == 0) {
	/* Check if we have a tar file */
	if ((ms->flags & MAGIC_NO_CHECK_TAR) != 0 ||
	    (m = file_is_tar(ms, buf, nb)) == 0) {
	    /* try tests in /etc/magic (or surrogate magic file) */
	    if ((ms->flags & MAGIC_NO_CHECK_SOFT) != 0 ||
		(m = file_softmagic(ms, buf, nb)) == 0) {
		/* try known keywords, check whether it is ASCII */
		if ((ms->flags & MAGIC_NO_CHECK_ASCII) != 0 ||
		    (m = file_ascmagic(ms, buf, nb)) == 0) {
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
#ifdef BUILTIN_ELF
    if ((ms->flags & MAGIC_NO_CHECK_ELF) == 0 && m == 1 && nb > 5 && fd != -1) {
	/*
	 * We matched something in the file, so this *might*
	 * be an ELF file, and the file is at least 5 bytes
	 * long, so if it's an ELF file it has at least one
	 * byte past the ELF magic number - try extracting
	 * information from the ELF headers that cannot easily
	 * be extracted with rules in the magic file.
	 */
	(void)file_tryelf(ms, fd, buf, nb);
    }
#endif
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
	ms->o.left = ms->o.size;
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
	char *pbuf, *op, *np;
	size_t psize, len;

	if (ms->haderr)
		return NULL;

	if (ms->flags & MAGIC_RAW)
		return ms->o.buf;

	len = ms->o.size - ms->o.left;
	/* * 4 is for octal representation, + 1 is for NUL */
	if (len > (SIZE_T_MAX - 1) / 4) {
		file_oomem(ms, len);
		return NULL;
	}
	psize = len * 4 + 1;
	if (ms->o.psize < psize) {
		if ((pbuf = realloc(ms->o.pbuf, psize)) == NULL) {
			file_oomem(ms, psize);
			return NULL;
		}
		ms->o.psize = psize;
		ms->o.pbuf = pbuf;
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

protected int
file_check_mem(struct magic_set *ms, unsigned int level)
{
	size_t len;

	if (level >= ms->c.len) {
		len = (ms->c.len += 20) * sizeof(*ms->c.li);
		ms->c.li = (ms->c.li == NULL) ? malloc(len) :
		    realloc(ms->c.li, len);
		if (ms->c.li == NULL) {
			file_oomem(ms, len);
			return -1;
		}
	}
	ms->c.li[level].got_match = 0;
#ifdef ENABLE_CONDITIONALS
	ms->c.li[level].last_match = 0;
	ms->c.li[level].last_cond = COND_NONE;
#endif /* ENABLE_CONDITIONALS */
	return 0;
}
/*
 * Yes these wrappers suffer from buffer overflows, but if your OS does not
 * have the real functions, maybe you should consider replacing your OS?
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
