/*	$Id: tbl_int.h,v 1.2 2018/12/14 06:33:14 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011,2013,2015,2017,2018 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Internal interfaces of the tbl(7) parser.
 * For use inside the tbl(7) parser only.
 */

enum	tbl_part {
	TBL_PART_OPTS,    /* In the first line, ends with semicolon. */
	TBL_PART_LAYOUT,  /* In the layout section, ends with full stop. */
	TBL_PART_DATA,    /* In the data section, ends with TE. */
	TBL_PART_CDATA    /* In a T{ block, ends with T} */
};

struct	tbl_node {
	struct tbl_opts	  opts;		/* Options for the whole table. */
	struct tbl_node	 *next;		/* Next table. */
	struct tbl_row	 *first_row;	/* First layout row. */
	struct tbl_row	 *last_row;	/* Last layout row. */
	struct tbl_span	 *first_span;	/* First data row. */
	struct tbl_span	 *current_span;	/* Data row being parsed. */
	struct tbl_span	 *last_span;	/* Last data row. */
	int		  line;		/* Line number in input file. */
	int		  pos;		/* Column number in input file. */
	enum tbl_part	  part;		/* Table section being parsed. */
};


void		 tbl_option(struct tbl_node *, int, const char *, int *);
void		 tbl_layout(struct tbl_node *, int, const char *, int);
void		 tbl_data(struct tbl_node *, int, const char *, int);
void		 tbl_cdata(struct tbl_node *, int, const char *, int);
void		 tbl_reset(struct tbl_node *);
