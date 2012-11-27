/*	$Id: mdoc.h,v 1.122 2011/03/22 14:05:45 kristaps Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef MDOC_H
#define MDOC_H

enum	mdoct {
	MDOC_Ap = 0,
	MDOC_Dd,
	MDOC_Dt,
	MDOC_Os,
	MDOC_Sh,
	MDOC_Ss,
	MDOC_Pp,
	MDOC_D1,
	MDOC_Dl,
	MDOC_Bd,
	MDOC_Ed,
	MDOC_Bl,
	MDOC_El,
	MDOC_It,
	MDOC_Ad,
	MDOC_An,
	MDOC_Ar,
	MDOC_Cd,
	MDOC_Cm,
	MDOC_Dv,
	MDOC_Er,
	MDOC_Ev,
	MDOC_Ex,
	MDOC_Fa,
	MDOC_Fd,
	MDOC_Fl,
	MDOC_Fn,
	MDOC_Ft,
	MDOC_Ic,
	MDOC_In,
	MDOC_Li,
	MDOC_Nd,
	MDOC_Nm,
	MDOC_Op,
	MDOC_Ot,
	MDOC_Pa,
	MDOC_Rv,
	MDOC_St,
	MDOC_Va,
	MDOC_Vt,
	MDOC_Xr,
	MDOC__A,
	MDOC__B,
	MDOC__D,
	MDOC__I,
	MDOC__J,
	MDOC__N,
	MDOC__O,
	MDOC__P,
	MDOC__R,
	MDOC__T,
	MDOC__V,
	MDOC_Ac,
	MDOC_Ao,
	MDOC_Aq,
	MDOC_At,
	MDOC_Bc,
	MDOC_Bf,
	MDOC_Bo,
	MDOC_Bq,
	MDOC_Bsx,
	MDOC_Bx,
	MDOC_Db,
	MDOC_Dc,
	MDOC_Do,
	MDOC_Dq,
	MDOC_Ec,
	MDOC_Ef,
	MDOC_Em,
	MDOC_Eo,
	MDOC_Fx,
	MDOC_Ms,
	MDOC_No,
	MDOC_Ns,
	MDOC_Nx,
	MDOC_Ox,
	MDOC_Pc,
	MDOC_Pf,
	MDOC_Po,
	MDOC_Pq,
	MDOC_Qc,
	MDOC_Ql,
	MDOC_Qo,
	MDOC_Qq,
	MDOC_Re,
	MDOC_Rs,
	MDOC_Sc,
	MDOC_So,
	MDOC_Sq,
	MDOC_Sm,
	MDOC_Sx,
	MDOC_Sy,
	MDOC_Tn,
	MDOC_Ux,
	MDOC_Xc,
	MDOC_Xo,
	MDOC_Fo,
	MDOC_Fc,
	MDOC_Oo,
	MDOC_Oc,
	MDOC_Bk,
	MDOC_Ek,
	MDOC_Bt,
	MDOC_Hf,
	MDOC_Fr,
	MDOC_Ud,
	MDOC_Lb,
	MDOC_Lp,
	MDOC_Lk,
	MDOC_Mt,
	MDOC_Brq,
	MDOC_Bro,
	MDOC_Brc,
	MDOC__C,
	MDOC_Es,
	MDOC_En,
	MDOC_Dx,
	MDOC__Q,
	MDOC_br,
	MDOC_sp,
	MDOC__U,
	MDOC_Ta,
	MDOC_MAX
};

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

enum	mdoc_type {
	MDOC_TEXT,
	MDOC_ELEM,
	MDOC_HEAD,
	MDOC_TAIL,
	MDOC_BODY,
	MDOC_BLOCK,
	MDOC_TBL,
	MDOC_EQN,
	MDOC_ROOT
};

/* 
 * Section (named/unnamed) of `Sh'.   Note that these appear in the
 * conventional order imposed by mdoc.7.  In the case of SEC_NONE, no
 * section has been invoked (this shouldn't happen).  SEC_CUSTOM refers
 * to other sections.
 */
enum	mdoc_sec {
	SEC_NONE = 0,
	SEC_NAME, /* NAME */
	SEC_LIBRARY, /* LIBRARY */
	SEC_SYNOPSIS, /* SYNOPSIS */
	SEC_DESCRIPTION, /* DESCRIPTION */
	SEC_IMPLEMENTATION, /* IMPLEMENTATION NOTES */
	SEC_RETURN_VALUES, /* RETURN VALUES */
	SEC_ENVIRONMENT,  /* ENVIRONMENT */
	SEC_FILES, /* FILES */
	SEC_EXIT_STATUS, /* EXIT STATUS */
	SEC_EXAMPLES, /* EXAMPLES */
	SEC_DIAGNOSTICS, /* DIAGNOSTICS */
	SEC_COMPATIBILITY, /* COMPATIBILITY */
	SEC_ERRORS, /* ERRORS */
	SEC_SEE_ALSO, /* SEE ALSO */
	SEC_STANDARDS, /* STANDARDS */
	SEC_HISTORY, /* HISTORY */
	SEC_AUTHORS, /* AUTHORS */
	SEC_CAVEATS, /* CAVEATS */
	SEC_BUGS, /* BUGS */
	SEC_SECURITY, /* SECURITY */
	SEC_CUSTOM, 
	SEC__MAX
};

struct	mdoc_meta {
	char		 *msec; /* `Dt' section (1, 3p, etc.) */
	char		 *vol; /* `Dt' volume (implied) */
	char		 *arch; /* `Dt' arch (i386, etc.) */
	char		 *date; /* `Dd' normalised date */
	char		 *title; /* `Dt' title (FOO, etc.) */
	char		 *os; /* `Os' system (OpenBSD, etc.) */
	char		 *name; /* leading `Nm' name */
};

/* 
 * An argument to a macro (multiple values = `-column xxx yyy'). 
 */
struct	mdoc_argv {
	enum mdocargt  	  arg; /* type of argument */
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
struct 	mdoc_arg {
	size_t		  argc;
	struct mdoc_argv *argv;
	unsigned int	  refcnt;
};

/*
 * Indicates that a BODY's formatting has ended, but the scope is still
 * open.  Used for syntax-broken blocks.
 */
enum	mdoc_endbody {
	ENDBODY_NOT = 0,
	ENDBODY_SPACE, /* is broken: append a space */
	ENDBODY_NOSPACE /* is broken: don't append a space */
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
	DISP_centred, /* -centered */
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
	struct mdoc_an 	  An;
	struct mdoc_bd	  Bd;
	struct mdoc_bf	  Bf;
	struct mdoc_bl	  Bl;
	struct mdoc_rs	  Rs;
};

/* 
 * Single node in tree-linked AST. 
 */
struct	mdoc_node {
	struct mdoc_node *parent; /* parent AST node */
	struct mdoc_node *child; /* first child AST node */
	struct mdoc_node *last; /* last child AST node */
	struct mdoc_node *next; /* sibling AST node */
	struct mdoc_node *prev; /* prior sibling AST node */
	int		  nchild; /* number children */
	int		  line; /* parse line */
	int		  pos; /* parse column */
	enum mdoct	  tok; /* tok or MDOC__MAX if none */
	int		  flags;
#define	MDOC_VALID	 (1 << 0) /* has been validated */
#define	MDOC_EOS	 (1 << 2) /* at sentence boundary */
#define	MDOC_LINE	 (1 << 3) /* first macro/text on line */
#define	MDOC_SYNPRETTY	 (1 << 4) /* SYNOPSIS-style formatting */
#define	MDOC_ENDED	 (1 << 5) /* rendering has been ended */
#define	MDOC_DELIMO	 (1 << 6)
#define	MDOC_DELIMC	 (1 << 7)
	enum mdoc_type	  type; /* AST node type */
	enum mdoc_sec	  sec; /* current named section */
	union mdoc_data	 *norm; /* normalised args */
	/* FIXME: these can be union'd to shave a few bytes. */
	struct mdoc_arg	 *args; /* BLOCK/ELEM */
	struct mdoc_node *pending; /* BLOCK */
	struct mdoc_node *head; /* BLOCK */
	struct mdoc_node *body; /* BLOCK */
	struct mdoc_node *tail; /* BLOCK */
	char		 *string; /* TEXT */
	const struct tbl_span *span; /* TBL */
	const struct eqn *eqn; /* EQN */
	enum mdoc_endbody end; /* BODY */
};

/* Names of macros.  Index is enum mdoct. */
extern	const char *const *mdoc_macronames;

/* Names of macro args.  Index is enum mdocargt. */
extern	const char *const *mdoc_argnames;

__BEGIN_DECLS

struct	mdoc;

const struct mdoc_node *mdoc_node(const struct mdoc *);
const struct mdoc_meta *mdoc_meta(const struct mdoc *);

__END_DECLS

#endif /*!MDOC_H*/
