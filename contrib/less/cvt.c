/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
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
public size_t cvt_length(size_t len, int ops)
{
	(void) ops;
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
public int * cvt_alloc_chpos(size_t len)
{
	size_t i;
	int *chpos = (int *) ecalloc(len, sizeof(int));
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
public void cvt_text(mutable char *odst, constant char *osrc, mutable int *chpos, mutable size_t *lenp, int ops)
{
	char *dst;
	char *edst = odst;
	constant char *src;
	constant char *src_end;
	LWCHAR ch;

	if (lenp != NULL)
		src_end = osrc + *lenp;
	else
		src_end = osrc + strlen(osrc);

	for (src = osrc, dst = odst;  src < src_end;  )
	{
		size_t src_pos = ptr_diff(src, osrc);
		size_t dst_pos = ptr_diff(dst, odst);
		struct ansi_state *pansi;
		ch = step_charc(&src, +1, src_end);
		if ((ops & CVT_BS) && ch == '\b' && dst > odst)
		{
			/* Delete backspace and preceding char. */
			do {
				dst--;
			} while (dst > odst && utf_mode &&
				!IS_ASCII_OCTET(*dst) && !IS_UTF8_LEAD(*dst));
		} else if ((ops & CVT_ANSI) && (pansi = ansi_start(ch)) != NULL)
		{
			/* Skip to end of ANSI escape sequence. */
			while (src < src_end)
			{
				if (ansi_step(pansi, ch) != ANSI_MID)
					break;
				ch = (LWCHAR) *src++; /* {{ would step_char work? }} */
			}
			ansi_done(pansi);
		} else
		{
			/* Just copy the char to the destination buffer. */
			char *cdst = dst;
			if ((ops & CVT_TO_LC) && IS_UPPER(ch))
				ch = TO_LOWER(ch);
			put_wchar(&dst, ch);
			/* Record the original position of the char. */
			if (chpos != NULL)
			{
				while (cdst++ < dst)
					chpos[dst_pos++] = (int) src_pos; /*{{type-issue}}*/
			}
		}
		if (dst > edst)
			edst = dst;
	}
	if ((ops & CVT_CRLF) && edst > odst && edst[-1] == '\r')
		edst--;
	*edst = '\0';
	if (lenp != NULL)
		*lenp = ptr_diff(edst, odst);
	/* FIXME: why was this here?  if (chpos != NULL) chpos[dst - odst] = src - osrc; */
}
