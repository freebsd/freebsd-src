/*	$Id: eqn.h,v 1.1 2018/12/13 05:23:38 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
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
 * Public data types for eqn(7) syntax trees.
 */

enum	eqn_boxt {
	EQN_TEXT,	/* Text, e.g. number, variable, operator, ... */
	EQN_SUBEXPR,	/* Nested eqn(7) subexpression. */
	EQN_LIST,	/* List, for example in braces. */
	EQN_PILE,	/* Vertical pile. */
	EQN_MATRIX	/* List of columns. */
};

enum	eqn_fontt {
	EQNFONT_NONE = 0,
	EQNFONT_ROMAN,
	EQNFONT_BOLD,
	EQNFONT_FAT,
	EQNFONT_ITALIC,
	EQNFONT__MAX
};

enum	eqn_post {
	EQNPOS_NONE = 0,
	EQNPOS_SUP,
	EQNPOS_SUBSUP,
	EQNPOS_SUB,
	EQNPOS_TO,
	EQNPOS_FROM,
	EQNPOS_FROMTO,
	EQNPOS_OVER,
	EQNPOS_SQRT,
	EQNPOS__MAX
};

 /*
 * A "box" is a parsed mathematical expression as defined by the eqn.7
 * grammar.
 */
struct	eqn_box {
	struct eqn_box	 *parent;
	struct eqn_box	 *prev;
	struct eqn_box	 *next;
	struct eqn_box	 *first;   /* First child node. */
	struct eqn_box	 *last;    /* Last child node. */
	char		 *text;    /* Text (or NULL). */
	char		 *left;    /* Left-hand fence. */
	char		 *right;   /* Right-hand fence. */
	char		 *top;     /* Symbol above. */
	char		 *bottom;  /* Symbol below. */
	size_t		  expectargs; /* Maximal number of arguments. */
	size_t		  args;    /* Actual number of arguments. */
	int		  size;    /* Font size. */
#define	EQN_DEFSIZE	  INT_MIN
	enum eqn_boxt	  type;    /* Type of node. */
	enum eqn_fontt	  font;    /* Font in this box. */
	enum eqn_post	  pos;     /* Position of the next box. */
};
