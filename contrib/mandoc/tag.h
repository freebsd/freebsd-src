/* $Id: tag.h,v 1.14 2020/04/18 20:40:10 schwarze Exp $ */
/*
 * Copyright (c) 2015, 2018, 2019, 2020 Ingo Schwarze <schwarze@openbsd.org>
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
 *
 * Internal interfaces to tag syntax tree nodes.
 * For use by mandoc(1) validation modules only.
 */

/*
 * Tagging priorities.
 * Lower numbers indicate higher importance.
 */
#define	TAG_MANUAL	1		/* Set with a .Tg macro. */
#define	TAG_STRONG	2		/* Good automatic tagging. */
#define	TAG_WEAK	(INT_MAX - 2)	/* Dubious automatic tagging. */
#define	TAG_FALLBACK	(INT_MAX - 1)	/* Tag only used if unique. */
#define	TAG_DELETE	(INT_MAX)	/* Tag not used at all. */

void		 tag_alloc(void);
int		 tag_exists(const char *);
void		 tag_put(const char *, int, struct roff_node *);
void		 tag_postprocess(struct roff_man *, struct roff_node *);
void		 tag_free(void);
