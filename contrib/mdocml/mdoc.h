/*	$Id: mdoc.h,v 1.144 2015/11/07 14:01:16 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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

#define	MDOC_Ap    0
#define	MDOC_Dd    1
#define	MDOC_Dt    2
#define	MDOC_Os    3
#define	MDOC_Sh    4
#define	MDOC_Ss    5
#define	MDOC_Pp    6
#define	MDOC_D1    7
#define	MDOC_Dl    8
#define	MDOC_Bd    9
#define	MDOC_Ed   10
#define	MDOC_Bl   11
#define	MDOC_El   12
#define	MDOC_It   13
#define	MDOC_Ad   14
#define	MDOC_An   15
#define	MDOC_Ar   16
#define	MDOC_Cd   17
#define	MDOC_Cm   18
#define	MDOC_Dv   19
#define	MDOC_Er   20
#define	MDOC_Ev   21
#define	MDOC_Ex   22
#define	MDOC_Fa   23
#define	MDOC_Fd   24
#define	MDOC_Fl   25
#define	MDOC_Fn   26
#define	MDOC_Ft   27
#define	MDOC_Ic   28
#define	MDOC_In   29
#define	MDOC_Li   30
#define	MDOC_Nd   31
#define	MDOC_Nm   32
#define	MDOC_Op   33
#define	MDOC_Ot   34
#define	MDOC_Pa   35
#define	MDOC_Rv   36
#define	MDOC_St   37
#define	MDOC_Va   38
#define	MDOC_Vt   39
#define	MDOC_Xr   40
#define	MDOC__A   41
#define	MDOC__B   42
#define	MDOC__D   43
#define	MDOC__I   44
#define	MDOC__J   45
#define	MDOC__N   46
#define	MDOC__O   47
#define	MDOC__P   48
#define	MDOC__R   49
#define	MDOC__T   50
#define	MDOC__V   51
#define	MDOC_Ac   52
#define	MDOC_Ao   53
#define	MDOC_Aq   54
#define	MDOC_At   55
#define	MDOC_Bc   56
#define	MDOC_Bf   57
#define	MDOC_Bo   58
#define	MDOC_Bq   59
#define	MDOC_Bsx  60
#define	MDOC_Bx   61
#define	MDOC_Db   62
#define	MDOC_Dc   63
#define	MDOC_Do   64
#define	MDOC_Dq   65
#define	MDOC_Ec   66
#define	MDOC_Ef   67
#define	MDOC_Em   68
#define	MDOC_Eo   69
#define	MDOC_Fx   70
#define	MDOC_Ms   71
#define	MDOC_No   72
#define	MDOC_Ns   73
#define	MDOC_Nx   74
#define	MDOC_Ox   75
#define	MDOC_Pc   76
#define	MDOC_Pf   77
#define	MDOC_Po   78
#define	MDOC_Pq   79
#define	MDOC_Qc   80
#define	MDOC_Ql   81
#define	MDOC_Qo   82
#define	MDOC_Qq   83
#define	MDOC_Re   84
#define	MDOC_Rs   85
#define	MDOC_Sc   86
#define	MDOC_So   87
#define	MDOC_Sq   88
#define	MDOC_Sm   89
#define	MDOC_Sx   90
#define	MDOC_Sy   91
#define	MDOC_Tn   92
#define	MDOC_Ux   93
#define	MDOC_Xc   94
#define	MDOC_Xo   95
#define	MDOC_Fo   96
#define	MDOC_Fc   97
#define	MDOC_Oo   98
#define	MDOC_Oc   99
#define	MDOC_Bk  100
#define	MDOC_Ek  101
#define	MDOC_Bt  102
#define	MDOC_Hf  103
#define	MDOC_Fr  104
#define	MDOC_Ud  105
#define	MDOC_Lb  106
#define	MDOC_Lp  107
#define	MDOC_Lk  108
#define	MDOC_Mt  109
#define	MDOC_Brq 110
#define	MDOC_Bro 111
#define	MDOC_Brc 112
#define	MDOC__C  113
#define	MDOC_Es  114
#define	MDOC_En  115
#define	MDOC_Dx  116
#define	MDOC__Q  117
#define	MDOC_br  118
#define	MDOC_sp  119
#define	MDOC__U  120
#define	MDOC_Ta  121
#define	MDOC_ll  122
#define	MDOC_MAX 123

enum	mdocargt {
	MDOC_Split, /* -split */
	MDOC_Nosplit, /* -nospli */
	MDOC_Ragged, /* -ragged */
	MDOC_Unfilled, /* -unfilled */
	MDOC_Literal, /* -literal */
	MDOC_File, /* -file */
	MDOC_Offset, /* -offset */
	MDOC_Bullet, /* -bullet */
	MDOC_Dash, /* -dash */
	MDOC_Hyphen, /* -hyphen */
	MDOC_Item, /* -item */
	MDOC_Enum, /* -enum */
	MDOC_Tag, /* -tag */
	MDOC_Diag, /* -diag */
	MDOC_Hang, /* -hang */
	MDOC_Ohang, /* -ohang */
	MDOC_Inset, /* -inset */
	MDOC_Column, /* -column */
	MDOC_Width, /* -width */
	MDOC_Compact, /* -compact */
	MDOC_Std, /* -std */
	MDOC_Filled, /* -filled */
	MDOC_Words, /* -words */
	MDOC_Emphasis, /* -emphasis */
	MDOC_Symbolic, /* -symbolic */
	MDOC_Nested, /* -nested */
	MDOC_Centred, /* -centered */
	MDOC_ARG_MAX
};

/*
 * An argument to a macro (multiple values = `-column xxx yyy').
 */
struct	mdoc_argv {
	enum mdocargt	  arg; /* type of argument */
	int		  line;
	int		  pos;
	size_t		  sz; /* elements in "value" */
	char		**value; /* argument strings */
};

/*
 * Reference-counted macro arguments.  These are refcounted because
 * blocks have multiple instances of the same arguments spread across
 * the HEAD, BODY, TAIL, and BLOCK node types.
 */
struct	mdoc_arg {
	size_t		  argc;
	struct mdoc_argv *argv;
	unsigned int	  refcnt;
};

enum	mdoc_list {
	LIST__NONE = 0,
	LIST_bullet, /* -bullet */
	LIST_column, /* -column */
	LIST_dash, /* -dash */
	LIST_diag, /* -diag */
	LIST_enum, /* -enum */
	LIST_hang, /* -hang */
	LIST_hyphen, /* -hyphen */
	LIST_inset, /* -inset */
	LIST_item, /* -item */
	LIST_ohang, /* -ohang */
	LIST_tag, /* -tag */
	LIST_MAX
};

enum	mdoc_disp {
	DISP__NONE = 0,
	DISP_centered, /* -centered */
	DISP_ragged, /* -ragged */
	DISP_unfilled, /* -unfilled */
	DISP_filled, /* -filled */
	DISP_literal /* -literal */
};

enum	mdoc_auth {
	AUTH__NONE = 0,
	AUTH_split, /* -split */
	AUTH_nosplit /* -nosplit */
};

enum	mdoc_font {
	FONT__NONE = 0,
	FONT_Em, /* Em, -emphasis */
	FONT_Li, /* Li, -literal */
	FONT_Sy /* Sy, -symbolic */
};

struct	mdoc_bd {
	const char	 *offs; /* -offset */
	enum mdoc_disp	  type; /* -ragged, etc. */
	int		  comp; /* -compact */
};

struct	mdoc_bl {
	const char	 *width; /* -width */
	const char	 *offs; /* -offset */
	enum mdoc_list	  type; /* -tag, -enum, etc. */
	int		  comp; /* -compact */
	size_t		  ncols; /* -column arg count */
	const char	**cols; /* -column val ptr */
	int		  count; /* -enum counter */
};

struct	mdoc_bf {
	enum mdoc_font	  font; /* font */
};

struct	mdoc_an {
	enum mdoc_auth	  auth; /* -split, etc. */
};

struct	mdoc_rs {
	int		  quote_T; /* whether to quote %T */
};

/*
 * Consists of normalised node arguments.  These should be used instead
 * of iterating through the mdoc_arg pointers of a node: defaults are
 * provided, etc.
 */
union	mdoc_data {
	struct mdoc_an	  An;
	struct mdoc_bd	  Bd;
	struct mdoc_bf	  Bf;
	struct mdoc_bl	  Bl;
	struct roff_node *Es;
	struct mdoc_rs	  Rs;
};

/* Names of macros. */
extern	const char *const *mdoc_macronames;

/* Names of macro args.  Index is enum mdocargt. */
extern	const char *const *mdoc_argnames;


void		 mdoc_validate(struct roff_man *);
