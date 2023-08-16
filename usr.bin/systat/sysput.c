/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019, 2020 Yoshihiro Ota
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
#include <sys/sysctl.h>

#include <err.h>
#include <inttypes.h>
#include <libutil.h>
#include <machine/param.h>
#include <string.h>

#include "systat.h"
#include "extern.h"

void
sysputspaces(WINDOW *wd, int row, int lcol, int width)
{
	static char str60[] = "                    "
	    "                                        ";

	mvwaddstr(wd, row, lcol, str60 + sizeof(str60) - width - 1);
}

void
sysputstrs(WINDOW *wd __unused, int row, int lcol, int width)
{
	static char str60[] = "********************"
	    "****************************************";

	/*
	 * XXX wnd instead of wd?
	 */
	mvwaddstr(wnd, row, lcol, str60 + sizeof(str60) - width - 1);
}

void
sysputXs(WINDOW *wd __unused, int row, int lcol, int width)
{
	static char str60[] = "XXXXXXXXXXXXXXXXXXXX"
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

	/*
	 * XXX wnd instead of wd?
	 */
	mvwaddstr(wnd, row, lcol, str60 + sizeof(str60) - width - 1);
}

void
sysputuint64(WINDOW *wd, int row, int lcol, int width, uint64_t val, int flags)
{
	char *start, wrtbuf[width + width + 1];
	int len;

	start = wrtbuf;
	flags |= HN_NOSPACE;

	if (val > INT64_MAX)
		goto error;
	else
		len = humanize_number(&wrtbuf[width], width + 1, val, "",
			HN_AUTOSCALE, flags);
	if (len < 0)
		goto error;
	else if (len < width)
		memset(wrtbuf + len, ' ', width - len);
	start += len;

	mvwaddstr(wd, row, lcol, start);
	return;

error:
	sysputstrs(wd, row, lcol, width);
}

void
sysputwuint64(WINDOW *wd, int row, int lcol, int width, uint64_t val, int flags)
{
	if(val == 0)
		sysputspaces(wd, row, lcol, width);
	else
		sysputuint64(wd, row, lcol, width, val, flags);
}

void
sysputpage(WINDOW *wd, int row, int lcol, int width, uint64_t pages, int flags)
{

	sysputuint64(wd, row, lcol, width, ptoa(pages), flags);
}
