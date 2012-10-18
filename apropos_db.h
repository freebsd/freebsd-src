/*	$Id: apropos_db.h,v 1.13 2012/03/24 01:46:25 kristaps Exp $ */
/*
 * Copyright (c) 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef APROPOS_H
#define APROPOS_H

enum	restype {
	RESTYPE_MAN, /* man(7) file */
	RESTYPE_MDOC, /* mdoc(7) file */
	RESTYPE_CAT /* pre-formatted file */
};

struct	res {
	enum restype	 type; /* input file type */
	char		*file; /* file in file-system */
	char		*cat; /* category (3p, 3, etc.) */
	char		*title; /* title (FOO, etc.) */
	char		*arch; /* arch (or empty string) */
	char		*desc; /* description (from Nd) */
	unsigned int	 rec; /* record in index */
	/*
	 * The index volume.  This indexes into the array of directories
	 * searched for manual page databases.
	 */
	unsigned int	 volume;
	/*
	 * The following fields are used internally.
	 *
	 * Maintain a binary tree for checking the uniqueness of `rec'
	 * when adding elements to the results array.
	 * Since the results array is dynamic, use offset in the array
	 * instead of a pointer to the structure.
	 */
	int		 lhs;
	int		 rhs;
	int		 matched; /* expression is true */
	int		*matches; /* partial truth evaluations */
};

struct	opts {
	const char	*arch; /* restrict to architecture */
	const char	*cat; /* restrict to manual section */
};

__BEGIN_DECLS

struct	expr;

int		 apropos_search(int, char **, const struct opts *,
			const struct expr *, size_t, 
			void *, size_t *, struct res **,
			void (*)(struct res *, size_t, void *));
struct	expr	*exprcomp(int, char *[], size_t *);
void		 exprfree(struct expr *);
void	 	 resfree(struct res *, size_t);
struct	expr	*termcomp(int, char *[], size_t *);

__END_DECLS

#endif /*!APROPOS_H*/
