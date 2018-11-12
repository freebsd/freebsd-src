/*	$Id: man.h,v 1.77 2015/11/07 14:01:16 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014, 2015 Ingo Schwarze <schwarze@openbsd.org>
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

#define	MAN_br   0
#define	MAN_TH   1
#define	MAN_SH   2
#define	MAN_SS   3
#define	MAN_TP   4
#define	MAN_LP   5
#define	MAN_PP   6
#define	MAN_P    7
#define	MAN_IP   8
#define	MAN_HP   9
#define	MAN_SM  10
#define	MAN_SB  11
#define	MAN_BI  12
#define	MAN_IB  13
#define	MAN_BR  14
#define	MAN_RB  15
#define	MAN_R   16
#define	MAN_B   17
#define	MAN_I   18
#define	MAN_IR  19
#define	MAN_RI  20
#define	MAN_sp  21
#define	MAN_nf  22
#define	MAN_fi  23
#define	MAN_RE  24
#define	MAN_RS  25
#define	MAN_DT  26
#define	MAN_UC  27
#define	MAN_PD  28
#define	MAN_AT  29
#define	MAN_in  30
#define	MAN_ft  31
#define	MAN_OP  32
#define	MAN_EX  33
#define	MAN_EE  34
#define	MAN_UR  35
#define	MAN_UE  36
#define	MAN_ll  37
#define	MAN_MAX 38

/* Names of macros. */
extern	const char *const *man_macronames;


struct	roff_man;

const struct mparse	*man_mparse(const struct roff_man *);
void			 man_validate(struct roff_man *);
