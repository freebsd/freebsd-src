/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * This file is primarily maintained by <tytso@mit.edu> and <ghudson@mit.edu>.
 */

/*
 * $Id: hesiod.h,v 1.7.20.1 2003/06/02 05:48:04 marka Exp $
 */

#ifndef _HESIOD_H_INCLUDED
#define _HESIOD_H_INCLUDED

int		hesiod_init __P((void **));
void		hesiod_end __P((void *));
char *		hesiod_to_bind __P((void *, const char *, const char *));
char **		hesiod_resolve __P((void *, const char *, const char *));
void		hesiod_free_list __P((void *, char **));
struct __res_state * __hesiod_res_get __P((void *));
void		__hesiod_res_set __P((void *, struct __res_state *,
				      void (*)(void *)));

#endif /*_HESIOD_H_INCLUDED*/
