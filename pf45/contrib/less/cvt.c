/*
 * Copyright (C) 1984-2009  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */

/*
 * Routines to convert text in various ways.  Used by search.
 */

#include "less.h"
#include "charset.h"

extern int utf_mode;

/*
 * Get the length of a buffer needed to convert a string.
 */
	public int
cvt_length(len, ops)
	int len;
	int ops;
{
	if (utf_mode)
		/*
		 * Just copying a string in UTF-8 mode can cause it to grow 
		 * in length.
		 * Four output bytes for one input byte is the worst case.
		 */
		len *= 4;
	return (len + 1);
}

/*
 * Allocate a chpos array for use by cvt_text.
 */
	public int *
cvt_alloc_chpos(len)
	int len;
{
	int i;
	int *chpos = (int *) ecalloc(sizeof(int), len);
	/* Initialize all entries to an invalid position. */
	for (i = 0;  i < len;  i++)
		chpos[i] = -1;
	return (chpos);
}

/*
 * Convert text.  Perform the transformations specified by ops.
 * Returns converted text in odst.  The original offset of each
 * odst character (when it was in osrc) is returned in the chpos array.
 */
	public void
cvt_text(odst, osrc, chpos, lenp, ops)
	char *odst;
	char *osrc;
	int *chpos;
	int *lenp;
	int ops;
{
	char *dst;
	char *src;
	register char *src_end;
	LWCHAR ch;

	if (lenp != NULL)
		src_end = osrc + *lenp;
	else
		src_end = osrc + strlen(osrc);

	for (src = osrc, dst = odst;  src < src_end;  )
	{
		int src_pos = src - osrc;
		int dst_pos = dst - odst;
		ch = step_char(&src, +1, src_end);
		if ((ops & CVT_BS) && ch == '\b' && dst > odst)
		{
			/* Delete backspace and preceding char. */
			do {
				dst--;
			} while (dst > odst &&
				!IS_ASCII_OCTET(*dst) && !IS_UTF8_LEAD(*dst));
		} else if ((ops & CVT_ANSI) && IS_CSI_START(ch))
		{
			/* Skip to end of ANSI escape sequence. */
			src++;  /* skip the CSI start char */
			while (src < src_end)
				if (!is_ansi_middle(*src++))
					break;
		} else
		{
			/* Just copy the char to the destination buffer. */
			if ((ops & CVT_TO_LC) && IS_UPPER(ch))
				ch = TO_LOWER(ch);
			put_wchar(&dst, ch);
			/*
			 * Record the original position of the char.
			 * But if we've already recorded a position
			 * for this char (due to a backspace), leave
			 * it alone; if multiple source chars map to
			 * one destination char, we want the position
			 * of the first one.
			 */
			if (chpos != NULL && chpos[dst_pos] < 0)
				chpos[dst_pos] = src_pos;
		}
	}
	if ((ops & CVT_CRLF) && dst > odst && dst[-1] == '\r')
		dst--;
	*dst = '\0';
	if (lenp != NULL)
		*lenp = dst - odst;
	if (chpos != NULL)
		chpos[dst - odst] = src - osrc;
}
