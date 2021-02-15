/*-
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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
#include <sys/sysctl.h>

#include <inttypes.h>
#include <string.h>
#include <err.h>
#include <libutil.h>

#include "systat.h"
#include "extern.h"

void
sysputspaces(WINDOW *wd, int row, int col, int width)
{
	static char str60[] = "                    "
	    "                                        ";

	mvwaddstr(wd, row, col, str60 + sizeof(str60) - width - 1);
}

void
sysputstrs(WINDOW *wd, int row, int col, int width)
{
	static char str60[] = "********************"
	    "****************************************";

	mvwaddstr(wnd, row, col, str60 + sizeof(str60) - width - 1);
}

void
sysputXs(WINDOW *wd, int row, int col, int width)
{
	static char str60[] = "XXXXXXXXXXXXXXXXXXXX"
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

	mvwaddstr(wnd, row, col, str60 + sizeof(str60) - width - 1);
}

void
sysputuint64(WINDOW *wd, int row, int col, int width, uint64_t val, int flags)
{
	char unit, *ptr, *start, wrtbuf[width + width + 1];
	int len;

	unit = 0;
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

	mvwaddstr(wd, row, col, start);
	return;

error:
	sysputstrs(wd, row, col, width);
}

void
sysputwuint64(WINDOW *wd, int row, int col, int width, uint64_t val, int flags)
{
	if(val == 0)
		sysputspaces(wd, row, col, width);
	else
		sysputuint64(wd, row, col, width, val, flags);
}

static int
calc_page_shift()
{
	u_int page_size;
	int shifts;

	shifts = 0;
	GETSYSCTL("vm.stats.vm.v_page_size", page_size);
	for(; page_size > 1; page_size >>= 1)
		shifts++;
	return shifts;
}

void
sysputpage(WINDOW *wd, int row, int col, int width, uint64_t pages, int flags)
{
	static int shifts = 0;

	if (shifts == 0)
		shifts = calc_page_shift();
	pages <<= shifts;
	sysputuint64(wd, row, col, width, pages, flags);
}
