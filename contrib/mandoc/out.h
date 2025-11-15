/* $Id: out.h,v 1.36 2025/07/16 14:33:08 schwarze Exp $ */
/*
 * Copyright (c) 2011,2014,2017,2018,2025 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
 * Utilities for use by multiple mandoc(1) formatters.
 */

enum	roffscale {
	SCALE_CM, /* centimeters (c) */
	SCALE_IN, /* inches (i) */
	SCALE_PC, /* pica (P) */
	SCALE_PT, /* points (p) */
	SCALE_EM, /* ems (m) */
	SCALE_MM, /* mini-ems (M) */
	SCALE_EN, /* ens (n) */
	SCALE_BU, /* default horizontal (u) */
	SCALE_VS, /* default vertical (v) */
	SCALE_FS, /* syn. for u (f) */
	SCALE_MAX
};

struct	roffcol {
	size_t		 width;    /* Width of cell [BU]. */
	size_t		 nwidth;   /* Maximum width of number [BU]. */
	size_t		 decimal;  /* Decimal position [BU]. */
	size_t		 spacing;  /* Spacing after the column [EN]. */
	int		 flags;    /* Layout flags, see tbl_cell. */
};

struct	roffsu {
	enum roffscale	  unit;
	double		  scale;
};

typedef	size_t	(*tbl_strlen)(const char *, void *);
typedef	size_t	(*tbl_len)(size_t, void *);

struct	rofftbl {
	tbl_strlen	 slen;	/* Calculate string length [BU]. */
	tbl_len		 len;	/* Produce width of empty space [BU]. */
	struct roffcol	*cols;	/* Master column specifiers. */
	void		*arg;	/* Passed to slen() and len(). */
};


struct	tbl_span;

const char	 *a2roffsu(const char *, struct roffsu *, enum roffscale);
void		  tblcalc(struct rofftbl *,
			const struct tbl_span *, size_t, size_t);
