/*	$Id: manpath.h,v 1.5 2011/12/13 20:56:46 kristaps Exp $ */
/*
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef MANPATH_H
#define MANPATH_H

/*
 * Unsorted list of unique, absolute paths to be searched for manual
 * databases.
 */
struct	manpaths {
	int	  sz;
	char	**paths;
};

__BEGIN_DECLS

void	 manpath_manconf(struct manpaths *, const char *);
void	 manpath_parse(struct manpaths *, const char *, char *, char *);
void	 manpath_free(struct manpaths *);

__END_DECLS

#endif /*!MANPATH_H*/
