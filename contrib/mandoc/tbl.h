/*	$Id: tbl.h,v 1.1 2018/12/12 21:54:35 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014, 2015, 2017, 2018 Ingo Schwarze <schwarze@openbsd.org>
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
 */

struct	tbl_opts {
	int		  opts;
#define	TBL_OPT_ALLBOX	 (1 << 0)  /* Option "allbox". */
#define	TBL_OPT_BOX	 (1 << 1)  /* Option "box". */
#define	TBL_OPT_CENTRE	 (1 << 2)  /* Option "center". */
#define	TBL_OPT_DBOX	 (1 << 3)  /* Option "doublebox". */
#define	TBL_OPT_EXPAND	 (1 << 4)  /* Option "expand". */
#define	TBL_OPT_NOKEEP	 (1 << 5)  /* Option "nokeep". */
#define	TBL_OPT_NOSPACE	 (1 << 6)  /* Option "nospaces". */
#define	TBL_OPT_NOWARN	 (1 << 7)  /* Option "nowarn". */
	int		  cols;    /* Number of columns. */
	int		  lvert;   /* Width of left vertical line. */
	int		  rvert;   /* Width of right vertical line. */
	char		  tab;     /* Option "tab": cell separator. */
	char		  decimal; /* Option "decimalpoint". */
};

enum	tbl_cellt {
	TBL_CELL_CENTRE,  /* c, C */
	TBL_CELL_RIGHT,   /* r, R */
	TBL_CELL_LEFT,    /* l, L */
	TBL_CELL_NUMBER,  /* n, N */
	TBL_CELL_SPAN,    /* s, S */
	TBL_CELL_LONG,    /* a, A */
	TBL_CELL_DOWN,    /* ^    */
	TBL_CELL_HORIZ,   /* _, - */
	TBL_CELL_DHORIZ,  /* =    */
	TBL_CELL_MAX
};

/*
 * A cell in a layout row.
 */
struct	tbl_cell {
	struct tbl_cell	 *next;     /* Layout cell to the right. */
	char		 *wstr;     /* Min width represented as a string. */
	size_t		  width;    /* Minimum column width. */
	size_t		  spacing;  /* To the right of the column. */
	int		  vert;     /* Width of subsequent vertical line. */
	int		  col;      /* Column number, starting from 0. */
	int		  flags;
#define	TBL_CELL_BOLD	 (1 << 0)   /* b, B, fB */
#define	TBL_CELL_ITALIC	 (1 << 1)   /* i, I, fI */
#define	TBL_CELL_TALIGN	 (1 << 2)   /* t, T */
#define	TBL_CELL_UP	 (1 << 3)   /* u, U */
#define	TBL_CELL_BALIGN	 (1 << 4)   /* d, D */
#define	TBL_CELL_WIGN	 (1 << 5)   /* z, Z */
#define	TBL_CELL_EQUAL	 (1 << 6)   /* e, E */
#define	TBL_CELL_WMAX	 (1 << 7)   /* x, X */
	enum tbl_cellt	  pos;
};

/*
 * A layout row.
 */
struct	tbl_row {
	struct tbl_row	 *next;   /* Layout row below. */
	struct tbl_cell	 *first;  /* Leftmost layout cell. */
	struct tbl_cell	 *last;   /* Rightmost layout cell. */
	int		  vert;   /* Width of left vertical line. */
};

enum	tbl_datt {
	TBL_DATA_NONE,    /* Uninitialized row. */
	TBL_DATA_DATA,    /* Contains data rather than a line. */
	TBL_DATA_HORIZ,   /* _: connecting horizontal line. */
	TBL_DATA_DHORIZ,  /* =: connecting double horizontal line. */
	TBL_DATA_NHORIZ,  /* \_: isolated horizontal line. */
	TBL_DATA_NDHORIZ  /* \=: isolated double horizontal line. */
};

/*
 * A cell within a row of data.  The "string" field contains the
 * actual string value that's in the cell.  The rest is layout.
 */
struct	tbl_dat {
	struct tbl_dat	 *next;    /* Data cell to the right. */
	struct tbl_cell	 *layout;  /* Associated layout cell. */
	char		 *string;  /* Data, or NULL if not TBL_DATA_DATA. */
	int		  hspans;  /* How many horizontal spans follow. */
	int		  vspans;  /* How many vertical spans follow. */
	int		  block;   /* T{ text block T} */
	enum tbl_datt	  pos;
};

enum	tbl_spant {
	TBL_SPAN_DATA,   /* Contains data rather than a line. */
	TBL_SPAN_HORIZ,  /* _: horizontal line. */
	TBL_SPAN_DHORIZ  /* =: double horizontal line. */
};

/*
 * A row of data in a table.
 */
struct	tbl_span {
	struct tbl_opts	 *opts;    /* Options for the table as a whole. */
	struct tbl_span	 *prev;    /* Data row above. */
	struct tbl_span	 *next;    /* Data row below. */
	struct tbl_row	 *layout;  /* Associated layout row. */
	struct tbl_dat	 *first;   /* Leftmost data cell. */
	struct tbl_dat	 *last;    /* Rightmost data cell. */
	int		  line;    /* Input file line number. */
	enum tbl_spant	  pos;
};
