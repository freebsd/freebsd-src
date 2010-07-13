/*-
 * Copyright (C) 1999 Egbert Eich
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of the authors not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The authors makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 *
 * THE AUTHORS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * xserver/hw/xfree86/int10/generic.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <compat/x86bios/x86bios.h>

extern u_char *pbiosMem;
extern int busySegMap[5];

void *
x86biosAlloc(int count, int *segs)
{
	int i;
	int j;

	/* find the free segblock of page */
	for (i = 0; i < (PAGE_RESERV - count); i++)
	{
		if (busySegMap[i] == 0)
		{
			/* find the capacity of segblock */
			for (j = i; j < (i + count); j++)
			{
				if (busySegMap[j] == 1)
					break;
			}

			if (j == (i + count))
				break;
			i += count;
		}
	}

	if (i == (PAGE_RESERV - count))
		return NULL;

	/* make the segblock is used */
	for (j = i; j < (i + count); j++)
		busySegMap[i] = 1;

	*segs = i * 4096;

	return (pbiosMem + *segs);
}

void
x86biosFree(void *pbuf, int count)
{
	int i;
	int busySeg;

	busySeg = ((u_char *)pbuf - pbiosMem) / 4096;

	for (i = busySeg; i < (busySeg + count); i++)
		busySegMap[i] = 0;
}
