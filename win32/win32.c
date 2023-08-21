/*
 * Copyright (c) 2012 - 2014 Tony Finch <dot@dotat.at>
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

#include "unifdef.h"

/*
 * The Windows implementation of rename() fails if the new filename
 * already exists. Atomic replacement is not really needed, so just
 * remove anything that might be in the way before renaming.
 */
int
replace(const char *oldname, const char *newname)
{
	if (remove(newname) < 0 && errno != ENOENT)
		warn("can't remove \"%s\"", newname);
	return (rename(oldname, newname));
}

FILE *
mktempmode(char *tmp, int mode)
{
	mode = mode;
	return (fopen(_mktemp(tmp), "wb"));
}

FILE *
fbinmode(FILE *fp)
{
	_setmode(_fileno(fp), _O_BINARY);
	return (fp);
}

/*
 * This is more long-winded than seems necessary because MinGW
 * doesn't have a proper implementation of _vsnprintf_s().
 *
 * This link has some useful info about snprintf() on Windows:
 * http://stackoverflow.com/questions/2915672/snprintf-and-visual-studio-2010
 */
int c99_snprintf(char *buf, size_t buflen, const char *format, ...)
{
	va_list ap;
	int outlen, cpylen, tmplen;
	char *tmp;

	va_start(ap, format);
	outlen = _vscprintf(format, ap);
	va_end(ap);
	if (buflen == 0 || outlen < 0)
		return outlen;
	if (buflen > outlen)
		cpylen = outlen;
	else
		cpylen = buflen - 1;
	/* Paranoia about off-by-one errors in _snprintf() */
	tmplen = outlen + 2;

	tmp = (char *)malloc(tmplen);
	if (tmp == NULL)
		err(2, "malloc");
	va_start(ap, format);
	_vsnprintf(tmp, tmplen, format, ap);
	va_end(ap);
	memcpy(buf, tmp, cpylen);
	buf[cpylen] = '\0';
	free(tmp);

	return outlen;
}
