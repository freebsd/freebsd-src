/*
 * Copyright (c) 2003, Trent Nelson, <trent@arpa.com>.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <unistd.h>
#include "convtbl.h"

struct	convtbl convtbl[] = {
	/* mul, scale, str */
	{ BYTE, BYTES, "bytes" },	/* SC_BYTE	(0) */
	{ BYTE, KILO, "KB" },		/* SC_KILOBYTE	(1) */
	{ BYTE, MEGA, "MB" },		/* SC_MEGABYTE	(2) */
	{ BYTE, GIGA, "GB" },		/* SC_GIGABYTE	(3) */

	{ BIT, BITS, "b" },		/* SC_BITS	(4) */
	{ BIT, KILO, "Kb" },		/* SC_KILOBITS	(5) */
	{ BIT, MEGA, "Mb" },		/* SC_MEGABITS	(6) */
	{ BIT, GIGA, "Gb" },		/* SC_GIGABITS	(7) */

	{ 0, 0, "" }			/* SC_AUTO	(8) */

};


static __inline__
struct convtbl *
get_tbl_ptr(const u_long size, const u_int scale)
{
	struct	convtbl *tbl_ptr = NULL;
	u_long	tmp = 0;
	u_int	index = scale;

	/* If our index is out of range, default to auto-scaling. */
	if (index > SC_AUTO)
		index = SC_AUTO;

	if (index == SC_AUTO)
		/*
		 * Simple but elegant algorithm.  Count how many times
		 * we can shift our size value right by a factor of ten,
		 * incrementing an index each time.  We then use the
		 * index as the array index into the conversion table.
		 */
		for (tmp = size, index = SC_KILOBYTE;
		     tmp >= MEGA && index <= SC_GIGABYTE;
		     tmp >>= 10, index++);

	tbl_ptr = &convtbl[index];
	return tbl_ptr;
}

double
convert(const u_long size, const u_int scale)
{
	struct	convtbl	*tp = NULL;

	tp = get_tbl_ptr(size, scale);

	return ((double)size * tp->mul / tp->scale);

}

char *
get_string(const u_long size, const u_int scale)
{
	struct	convtbl *tp = NULL;

	tp = get_tbl_ptr(size, scale);

	return tp->str;
}
