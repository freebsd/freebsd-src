/* $NetBSD: lint1.h,v 1.16 2002/10/21 22:44:08 christos Exp $ */

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All Rights Reserved.
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lint.h"
#include "op.h"

/* XXX - works for most systems, but the whole ALIGN thing needs to go away */
#ifndef LINT_ALIGN
#define LINT_ALIGN(x) (((x) + 15) & ~15)
#endif

/*
 * Describes the position of a declaration or anything else.
 */
typedef struct {
	int	p_line;
	const	char *p_file;
	int	p_uniq;			/* uniquifier */
} pos_t;

/* Copies curr_pos, keeping things unique. */
#define UNIQUE_CURR_POS(pos)						\
    do {								\
    	STRUCT_ASSIGN((pos), curr_pos);					\
	curr_pos.p_uniq++;						\
	if (curr_pos.p_file == csrc_pos.p_file)				\
	    csrc_pos.p_uniq++;						\
    } while (0)

/*
 * Strings cannot be referenced to simply by a pointer to its first
 * char. This is because strings can contain NUL characters other than the
 * trailing NUL.
 *
 * Strings are stored with a trailing NUL.
 */
typedef	struct strg {
	tspec_t	st_tspec;		/* CHAR or WCHAR */
	size_t	st_len;			/* length without trailing NUL */
	union {
		u_char	*_st_cp;
		wchar_t	*_st_wcp;
	} st_u;
} strg_t;

#define st_cp	st_u._st_cp
#define	st_wcp	st_u._st_wcp

/*
 * qualifiers (only for lex/yacc interface)
 */
typedef enum {
	CONST, VOLATILE
} tqual_t;

/*
 * Integer and floating point values are stored in this structure
 */
typedef struct {
	tspec_t	v_tspec;
	int	v_ansiu;		/* set if an integer constant is
					   unsigned in ANSI C */
	union {
		int64_t	_v_quad;	/* integers */
		ldbl_t	_v_ldbl;	/* floats */
	} v_u;
} val_t;

#define v_quad	v_u._v_quad
#define v_ldbl	v_u._v_ldbl

/*
 * Structures of type str_t uniqely identify structures. This can't
 * be done in structures of type type_t, because these are copied
 * if they must be modified. So it would not be possible to check
 * if two structures are identical by comparing the pointers to
 * the type structures.
 *
 * The typename is used if the structure is unnamed to identify
 * the structure type in pass 2.
 */
typedef	struct {
	u_int	size;		/* size in bit */
	u_int	align : 15;	/* alignment in bit */
	u_int	sincompl : 1;	/* set if incomplete type */
	struct	sym *memb;	/* list of members */
	struct	sym *stag;	/* symbol table entry of tag */
	struct	sym *stdef;	/* symbol table entry of first typename */
} str_t;

/*
 * same as above for enums
 */
typedef	struct {
	u_int	eincompl : 1;	/* incomplete enum type */
	struct	sym *elem;	/* list of enumerators */
	struct	sym *etag;	/* symbol table entry of tag */
	struct	sym *etdef;	/* symbol table entry of first typename */
} enum_t;

/*
 * Types are represented by concatenation of structures of type type_t
 * via t_subt.
 */
typedef	struct type {
	tspec_t	t_tspec;	/* type specifier */
	u_int	t_aincompl : 1;	/* incomplete array type */
	u_int	t_const : 1;	/* const modifier */
	u_int	t_volatile : 1;	/* volatile modifier */
	u_int	t_proto : 1;	/* function prototype (t_args valid) */
	u_int	t_vararg : 1;	/* prototype with ... */
	u_int	t_typedef : 1;	/* type defined with typedef */
	u_int	t_isfield : 1;	/* type is bitfield */
	u_int	t_isenum : 1;	/* type is (or was) enum (t_enum valid) */
	union {
		int	_t_dim;		/* dimension */
		str_t	*_t_str;	/* struct/union tag */
		enum_t	*_t_enum;	/* enum tag */
		struct	sym *_t_args;	/* arguments (if t_proto) */
	} t_u;
	struct {
		u_int	_t_flen : 8;	/* length of bit-field */
		u_int	_t_foffs : 24;	/* offset of bit-field */
	} t_b;
	struct	type *t_subt;	/* element type (arrays), return value
				   (functions), or type pointer points to */
} type_t;

#define	t_dim	t_u._t_dim
#define	t_str	t_u._t_str
#define	t_field	t_u._t_field
#define	t_enum	t_u._t_enum
#define	t_args	t_u._t_args
#define	t_flen	t_b._t_flen
#define	t_foffs	t_b._t_foffs

/*
 * types of symbols
 */
typedef	enum {
	FVFT,		/* variables, functions, type names, enums */
	FMOS,		/* members of structs or unions */
	FTAG,		/* tags */
	FLAB		/* labels */
} symt_t;

/*
 * storage classes
 */
typedef enum {
	NOSCL,
	EXTERN,		/* external symbols (indep. of decl_t) */
	STATIC,		/* static symbols (local and global) */
	AUTO,		/* automatic symbols (except register) */
	REG,		/* register */
	TYPEDEF,	/* typedef */
	STRTAG,
	UNIONTAG,
	ENUMTAG,
	MOS,		/* member of struct */
	MOU,		/* member of union */
	ENUMCON,	/* enumerator */
	ABSTRACT,	/* abstract symbol (sizeof, casts, unnamed argument) */
	ARG,		/* argument */
	PARG,		/* used in declaration stack during prototype
			   declaration */
	INLINE		/* only used by the parser */
} scl_t;

/*
 * symbol table entry
 */
typedef	struct sym {
	const	char *s_name;	/* name */
	const	char *s_rename;	/* renamed symbol's given name */
	pos_t	s_dpos;		/* position of last (prototype)definition,
				   prototypedeclaration, no-prototype-def.,
				   tentative definition or declaration,
				   in this order */
	pos_t	s_spos;		/* position of first initialisation */
	pos_t	s_upos;		/* position of first use */
	symt_t	s_kind;		/* type of symbol */
	u_int	s_keyw : 1;	/* keyword */
	u_int	s_field : 1;	/* bit-field */
	u_int	s_set : 1;	/* variable set, label defined */
	u_int	s_used : 1;	/* variable/label used */
	u_int	s_arg : 1;	/* symbol is function argument */
	u_int	s_reg : 1;	/* symbol is register variable */
	u_int	s_defarg : 1;	/* undefined symbol in old style function
				   definition */
	u_int	s_rimpl : 1;	/* return value of function implicit decl. */
	u_int	s_osdef : 1;	/* symbol stems from old style function def. */
	u_int	s_inline : 1;	/* true if this is an inline function */
	struct	sym *s_xsym;	/* for local declared external symbols pointer
				   to external symbol with same name */
	def_t	s_def;		/* declared, tentative defined, defined */
	scl_t	s_scl;		/* storage class */
	int	s_blklev;	/* level of declaration, -1 if not in symbol
				   table */
	type_t	*s_type;	/* type */
	val_t	s_value;	/* value (if enumcon) */
	union {
		str_t	*_s_st;	/* tag, if it is a struct/union member */
		enum_t	*_s_et;	/* tag, if it is an enumerator */
		tspec_t	_s_tsp;	/* type (only for keywords) */
		tqual_t	_s_tqu;	/* qualifier (only for keywords) */
		struct	sym *_s_args; /* arguments in old style function
					 definitions */
	} u;
	struct	sym *s_link;	/* next symbol with same hash value */
	struct	sym **s_rlink;	/* pointer to s_link of prev. symbol */
	struct	sym *s_nxt;	/* next struct/union member, enumerator,
				   argument */
	struct	sym *s_dlnxt; 	/* next symbol declared on same level */
} sym_t;

#define	s_styp	u._s_st
#define	s_etyp	u._s_et
#define	s_tspec	u._s_tsp
#define	s_tqual	u._s_tqu
#define	s_args	u._s_args

/*
 * Used to keep some informations about symbols before they are entered
 * into the symbol table.
 */
typedef	struct sbuf {
	const	char *sb_name;		/* name of symbol */
	size_t	sb_len;			/* length (without '\0') */
	int	sb_hash;		/* hash value */
	sym_t	*sb_sym;		/* symbol table entry */
	struct	sbuf *sb_nxt;		/* for freelist */
} sbuf_t;


/*
 * tree node
 */
typedef	struct tnode {
	op_t	tn_op;		/* operator */
	type_t	*tn_type;	/* type */
	u_int	tn_lvalue : 1;	/* node is lvalue */
	u_int	tn_cast : 1;	/* if tn_op == CVT its an explicit cast */
	u_int	tn_parn : 1;	/* node parenthesized */
	union {
		struct {
			struct	tnode *_tn_left;	/* (left) operand */
			struct	tnode *_tn_right;	/* right operand */
		} tn_s;
		sym_t	*_tn_sym;	/* symbol if op == NAME */
		val_t	*_tn_val;	/* value if op == CON */
		strg_t	*_tn_strg;	/* string if op == STRING */
	} tn_u;
} tnode_t;

#define	tn_left	tn_u.tn_s._tn_left
#define tn_right tn_u.tn_s._tn_right
#define tn_sym	tn_u._tn_sym
#define	tn_val	tn_u._tn_val
#define	tn_strg	tn_u._tn_strg

/*
 * For nested declarations a stack exists, which holds all information
 * needed for the current level. dcs points to the top element of this
 * stack.
 *
 * ctx describes the context of the current declaration. Its value is
 * one of
 *	EXTERN	global declarations
 *	MOS oder MOU declarations of struct or union members
 *	ENUMCON	declarations of enums
 *	ARG	declaration of arguments in old style function definitions
 *	PARG	declaration of arguments in function prototypes
 *	AUTO	declaration of local symbols
 *	ABSTRACT abstract declarations (sizeof, casts)
 *
 */
typedef	struct dinfo {
	tspec_t	d_atyp;		/* VOID, CHAR, INT, FLOAT or DOUBLE */
	tspec_t	d_smod;		/* SIGNED or UNSIGN */
	tspec_t	d_lmod;		/* SHORT, LONG or QUAD */
	scl_t	d_scl;		/* storage class */
	type_t	*d_type;	/* after deftyp() pointer to the type used
				   for all declarators */
	sym_t	*d_rdcsym;	/* redeclared symbol */
	int	d_offset;	/* offset of next structure member */
	int	d_stralign;	/* alignment required for current structure */
	scl_t	d_ctx;		/* context of declaration */
	u_int	d_const : 1;	/* const in declaration specifiers */
	u_int	d_volatile : 1;	/* volatile in declaration specifiers */
	u_int	d_inline : 1;	/* inline in declaration specifiers */
	u_int	d_mscl : 1;	/* multiple storage classes */
	u_int	d_terr : 1;	/* invalid type combination */
	u_int	d_nedecl : 1;	/* 1 if at least a tag is declared */
	u_int	d_vararg : 1;	/* ... in current function decl. */
	u_int	d_proto : 1;	/* current funct. decl. is prototype */
	u_int	d_notyp : 1;	/* set if no type specifier was present */
	u_int	d_asm : 1;	/* set if d_ctx == AUTO and asm() present */
	type_t	*d_tagtyp;	/* tag during member declaration */
	sym_t	*d_fargs;	/* list of arguments during function def. */
	pos_t	d_fdpos;	/* position of function definition */
	sym_t	*d_dlsyms;	/* first symbol declared at this level */
	sym_t	**d_ldlsym;	/* points to s_dlnxt in last symbol decl.
				   at this level */
	sym_t	*d_fpsyms;	/* symbols defined in prototype */
	struct	dinfo *d_nxt;	/* next level */
} dinfo_t;

/*
 * Type of stack which is used for initialisation of aggregate types.
 */
typedef	struct	istk {
	type_t	*i_type;		/* type of initialisation */
	type_t	*i_subt;		/* type of next level */
	u_int	i_brace : 1;		/* need } for pop */
	u_int	i_nolimit : 1;		/* incomplete array type */
	u_int	i_namedmem : 1;		/* has c9x named members */
	sym_t	*i_mem;			/* next structure member */
	int	i_cnt;			/* # of remaining elements */
	struct	istk *i_nxt;		/* previous level */
} istk_t;

/*
 * Used to collect information about pointers and qualifiers in
 * declarators.
 */
typedef	struct pqinf {
	int	p_pcnt;			/* number of asterisks */
	u_int	p_const : 1;
	u_int	p_volatile : 1;
	struct	pqinf *p_nxt;
} pqinf_t;

/*
 * Case values are stored in a list of type clst_t.
 */
typedef	struct clst {
	val_t	cl_val;
	struct	clst *cl_nxt;
} clst_t;

/*
 * Used to keep informations about nested control statements.
 */
typedef struct cstk {
	int	c_env;			/* type of statement (T_IF, ...) */
	u_int	c_loop : 1;		/* continue && break are valid */
	u_int	c_switch : 1;		/* case && break are valid */
	u_int	c_break : 1;		/* loop/switch has break */
	u_int	c_cont : 1;		/* loop has continue */
	u_int	c_default : 1;		/* switch has default */
	u_int	c_infinite : 1;		/* break condition always false
					   (for (;;), while (1)) */
	u_int	c_rchif : 1;		/* end of if-branch reached */
	u_int	c_noretval : 1;		/* had "return;" */
	u_int	c_retval : 1;		/* had "return (e);" */
	type_t	*c_swtype;		/* type of switch expression */
	clst_t	*c_clst;		/* list of case values */
	struct	mbl *c_fexprm;		/* saved memory for end of loop
					   expression in for() */
	tnode_t	*c_f3expr;		/* end of loop expr in for() */
	pos_t	c_fpos;			/* position of end of loop expr */
	pos_t	c_cfpos;	        /* same for csrc_pos */
	struct	cstk *c_nxt;		/* outer control statement */
} cstk_t;

typedef struct {
	size_t lo;
	size_t hi;
} range_t;

#include "externs1.h"

#define	ERR_SETSIZE	1024
#define __NERRBITS (sizeof(unsigned int))

typedef	struct err_set {
	unsigned int	errs_bits[(ERR_SETSIZE + __NERRBITS-1) / __NERRBITS];
} err_set;

#define	ERR_SET(n, p)	\
    ((p)->errs_bits[(n)/__NERRBITS] |= (1 << ((n) % __NERRBITS)))
#define	ERR_CLR(n, p)	\
    ((p)->errs_bits[(n)/__NERRBITS] &= ~(1 << ((n) % __NERRBITS)))
#define	ERR_ISSET(n, p)	\
    ((p)->errs_bits[(n)/__NERRBITS] & (1 << ((n) % __NERRBITS)))
#define	ERR_ZERO(p)	(void)memset((p), 0, sizeof(*(p)))

#define LERROR(fmt, args...) lerror(__FILE__, __LINE__, fmt, ##args)

extern err_set	msgset;
