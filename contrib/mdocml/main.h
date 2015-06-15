/*	$Id: main.h,v 1.20 2014/12/31 16:52:40 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define	UNCONST(a)	((void *)(uintptr_t)(const void *)(a))

__BEGIN_DECLS

struct	mchars;
struct	mdoc;
struct	man;

/*
 * Definitions for main.c-visible output device functions, e.g., -Thtml
 * and -Tascii.  Note that ascii_alloc() is named as such in
 * anticipation of latin1_alloc() and so on, all of which map into the
 * terminal output routines with different character settings.
 */

void		 *html_alloc(const struct mchars *, char *);
void		  html_mdoc(void *, const struct mdoc *);
void		  html_man(void *, const struct man *);
void		  html_free(void *);

void		  tree_mdoc(void *, const struct mdoc *);
void		  tree_man(void *, const struct man *);

void		  man_mdoc(void *, const struct mdoc *);
void		  man_man(void *, const struct man *);

void		 *locale_alloc(const struct mchars *, char *);
void		 *utf8_alloc(const struct mchars *, char *);
void		 *ascii_alloc(const struct mchars *, char *);
void		  ascii_free(void *);
void		  ascii_sepline(void *);

void		 *pdf_alloc(const struct mchars *, char *);
void		 *ps_alloc(const struct mchars *, char *);
void		  pspdf_free(void *);

void		  terminal_mdoc(void *, const struct mdoc *);
void		  terminal_man(void *, const struct man *);

__END_DECLS
