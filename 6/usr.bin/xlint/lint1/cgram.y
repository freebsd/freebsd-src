%{
/* $NetBSD: cgram.y,v 1.23 2002/01/31 19:36:53 tv Exp $ */

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
#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: cgram.y,v 1.23 2002/01/31 19:36:53 tv Exp $");
#endif
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "lint1.h"

/*
 * Contains the level of current declaration. 0 is extern.
 * Used for symbol table entries.
 */
int	blklev;

/*
 * level for memory allocation. Normaly the same as blklev.
 * An exeption is the declaration of arguments in prototypes. Memory
 * for these can't be freed after the declaration, but symbols must
 * be removed from the symbol table after the declaration.
 */
int	mblklev;

/*
 * Save the no-warns state and restore it to avoid the problem where
 * if (expr) { stmt } / * NOLINT * / stmt;
 */
static int onowarn = -1;

static	int	toicon(tnode_t *);
static	void	idecl(sym_t *, int, sbuf_t *);
static	void	ignuptorp(void);

#ifdef DEBUG
static __inline void CLRWFLGS(void);
static __inline void CLRWFLGS(void)
{
	printf("%s, %d: clear flags %s %d\n", curr_pos.p_file,
	    curr_pos.p_line, __FILE__, __LINE__);
	clrwflgs();
	onowarn = -1;
}

static __inline void SAVE(void);
static __inline void SAVE(void)
{
	if (onowarn != -1)
		abort();
	printf("%s, %d: save flags %s %d = %d\n", curr_pos.p_file,
	    curr_pos.p_line, __FILE__, __LINE__, nowarn);
	onowarn = nowarn;
}

static __inline void RESTORE(void);
static __inline void RESTORE(void)
{
	if (onowarn != -1) {
		nowarn = onowarn;
		printf("%s, %d: restore flags %s %d = %d\n", curr_pos.p_file,
		    curr_pos.p_line, __FILE__, __LINE__, nowarn);
		onowarn = -1;
	} else
		CLRWFLGS();
}
#else
#define CLRWFLGS() clrwflgs(), onowarn = -1
#define SAVE()	onowarn = nowarn
#define RESTORE() (void)(onowarn == -1 ? (clrwflgs(), 0) : (nowarn = onowarn))
#endif
%}

%union {
	int	y_int;
	val_t	*y_val;
	sbuf_t	*y_sb;
	sym_t	*y_sym;
	op_t	y_op;
	scl_t	y_scl;
	tspec_t	y_tspec;
	tqual_t	y_tqual;
	type_t	*y_type;
	tnode_t	*y_tnode;
	strg_t	*y_strg;
	pqinf_t	*y_pqinf;
};

%token			T_LBRACE T_RBRACE T_LBRACK T_RBRACK T_LPARN T_RPARN
%token	<y_op>		T_STROP
%token	<y_op>		T_UNOP
%token	<y_op>		T_INCDEC
%token			T_SIZEOF
%token	<y_op>		T_MULT
%token	<y_op>		T_DIVOP
%token	<y_op>		T_ADDOP
%token	<y_op>		T_SHFTOP
%token	<y_op>		T_RELOP
%token	<y_op>		T_EQOP
%token	<y_op>		T_AND
%token	<y_op>		T_XOR
%token	<y_op>		T_OR
%token	<y_op>		T_LOGAND
%token	<y_op>		T_LOGOR
%token			T_QUEST
%token			T_COLON
%token	<y_op>		T_ASSIGN
%token	<y_op>		T_OPASS
%token			T_COMMA
%token			T_SEMI
%token			T_ELLIPSE

/* storage classes (extern, static, auto, register and typedef) */
%token	<y_scl>		T_SCLASS

/* types (char, int, short, long, unsigned, signed, float, double, void) */
%token	<y_tspec>	T_TYPE

/* qualifiers (const, volatile) */
%token	<y_tqual>	T_QUAL

/* struct or union */
%token	<y_tspec>	T_SOU

/* enum */
%token			T_ENUM

/* remaining keywords */
%token			T_CASE
%token			T_DEFAULT
%token			T_IF
%token			T_ELSE
%token			T_SWITCH
%token			T_DO
%token			T_WHILE
%token			T_FOR
%token			T_GOTO
%token			T_CONTINUE
%token			T_BREAK
%token			T_RETURN
%token			T_ASM
%token			T_SYMBOLRENAME

%left	T_COMMA
%right	T_ASSIGN T_OPASS
%right	T_QUEST T_COLON
%left	T_LOGOR
%left	T_LOGAND
%left	T_OR
%left	T_XOR
%left	T_AND
%left	T_EQOP
%left	T_RELOP
%left	T_SHFTOP
%left	T_ADDOP
%left	T_MULT T_DIVOP
%right	T_UNOP T_INCDEC T_SIZEOF
%left	T_LPARN T_LBRACK T_STROP

%token	<y_sb>		T_NAME
%token	<y_sb>		T_TYPENAME
%token	<y_val>		T_CON
%token	<y_strg>	T_STRING

%type	<y_sym>		func_decl
%type	<y_sym>		notype_decl
%type	<y_sym>		type_decl
%type	<y_type>	typespec
%type	<y_type>	clrtyp_typespec
%type	<y_type>	notype_typespec
%type	<y_type>	struct_spec
%type	<y_type>	enum_spec
%type	<y_sym>		struct_tag
%type	<y_sym>		enum_tag
%type	<y_tspec>	struct
%type	<y_sym>		struct_declaration
%type	<y_sb>		identifier
%type	<y_sym>		member_declaration_list_with_rbrace
%type	<y_sym>		member_declaration_list
%type	<y_sym>		member_declaration
%type	<y_sym>		notype_member_decls
%type	<y_sym>		type_member_decls
%type	<y_sym>		notype_member_decl
%type	<y_sym>		type_member_decl
%type	<y_tnode>	constant
%type	<y_sym>		enum_declaration
%type	<y_sym>		enums_with_opt_comma
%type	<y_sym>		enums
%type	<y_sym>		enumerator
%type	<y_sym>		ename
%type	<y_sym>		notype_direct_decl
%type	<y_sym>		type_direct_decl
%type	<y_pqinf>	pointer
%type	<y_pqinf>	asterisk
%type	<y_sym>		param_decl
%type	<y_sym>		param_list
%type	<y_sym>		abs_decl_param_list
%type	<y_sym>		direct_param_decl
%type	<y_sym>		notype_param_decl
%type	<y_sym>		direct_notype_param_decl
%type	<y_pqinf>	type_qualifier_list
%type	<y_pqinf>	type_qualifier
%type	<y_sym>		identifier_list
%type	<y_sym>		abs_decl
%type	<y_sym>		direct_abs_decl
%type	<y_sym>		vararg_parameter_type_list
%type	<y_sym>		parameter_type_list
%type	<y_sym>		parameter_declaration
%type	<y_tnode>	expr
%type	<y_tnode>	term
%type	<y_tnode>	func_arg_list
%type	<y_op>		point_or_arrow
%type	<y_type>	type_name
%type	<y_sym>		abstract_declaration
%type	<y_tnode>	do_while_expr
%type	<y_tnode>	opt_expr
%type	<y_strg>	string
%type	<y_strg>	string2
%type	<y_sb>		opt_asm_or_symbolrename


%%

program:
	  /* empty */ {
		if (sflag) {
			/* empty translation unit */
			error(272);
		} else if (!tflag) {
			/* empty translation unit */
			warning(272);
		}
	  }
	| translation_unit
	;

translation_unit:
	  ext_decl
	| translation_unit ext_decl
	;

ext_decl:
	  asm_stmnt
	| func_def {
		glclup(0);
		CLRWFLGS();
	  }
	| data_def {
		glclup(0);
		CLRWFLGS();
	  }
	;

data_def:
	  T_SEMI {
		if (sflag) {
			/* syntax error: empty declaration */
			error(0);
		} else if (!tflag) {
			/* syntax error: empty declaration */
			warning(0);
		}
	  }
	| clrtyp deftyp notype_init_decls T_SEMI {
		if (sflag) {
			/* old style declaration; add "int" */
			error(1);
		} else if (!tflag) {
			/* old style declaration; add "int" */
			warning(1);
		}
	  }
	| declmods deftyp T_SEMI {
		if (dcs->d_scl == TYPEDEF) {
			/* typedef declares no type name */
			warning(72);
		} else {
			/* empty declaration */
			warning(2);
		}
	  }
	| declmods deftyp notype_init_decls T_SEMI
	| declspecs deftyp T_SEMI {
		if (dcs->d_scl == TYPEDEF) {
			/* typedef declares no type name */
			warning(72);
		} else if (!dcs->d_nedecl) {
			/* empty declaration */
			warning(2);
		}
	  }
	| declspecs deftyp type_init_decls T_SEMI
	| error T_SEMI {
		globclup();
	  }
	| error T_RBRACE {
		globclup();
	  }
	;

func_def:
	  func_decl {
		if ($1->s_type->t_tspec != FUNC) {
			/* syntax error */
			error(249);
			YYERROR;
		}
		if ($1->s_type->t_typedef) {
			/* ()-less function definition */
			error(64);
			YYERROR;
		}
		funcdef($1);
		blklev++;
		pushdecl(ARG);
	  } opt_arg_declaration_list {
		popdecl();
		blklev--;
		cluparg();
		pushctrl(0);
	  } comp_stmnt {
		funcend();
		popctrl(0);
	  }
	;

func_decl:
	  clrtyp deftyp notype_decl {
		$$ = $3;
	  }
	| declmods deftyp notype_decl {
		$$ = $3;
	  }
	| declspecs deftyp type_decl {
		$$ = $3;
	  }
	;

opt_arg_declaration_list:
	  /* empty */
	| arg_declaration_list
	;

arg_declaration_list:
	  arg_declaration
	| arg_declaration_list arg_declaration
	/* XXX or better "arg_declaration error" ? */
	| error
	;

/*
 * "arg_declaration" is separated from "declaration" because it
 * needs other error handling.
 */

arg_declaration:
	  declmods deftyp T_SEMI {
		/* empty declaration */
		warning(2);
	  }
	| declmods deftyp notype_init_decls T_SEMI
	| declspecs deftyp T_SEMI {
		if (!dcs->d_nedecl) {
			/* empty declaration */
			warning(2);
		} else {
			tspec_t	ts = dcs->d_type->t_tspec;
			/* %s declared in argument declaration list */
			warning(3, ts == STRUCT ? "struct" :
				(ts == UNION ? "union" : "enum"));
		}
	  }
	| declspecs deftyp type_init_decls T_SEMI {
		if (dcs->d_nedecl) {
			tspec_t	ts = dcs->d_type->t_tspec;
			/* %s declared in argument declaration list */
			warning(3, ts == STRUCT ? "struct" :
				(ts == UNION ? "union" : "enum"));
		}
	  }
	| declmods error
	| declspecs error
	;

declaration:
	  declmods deftyp T_SEMI {
		if (dcs->d_scl == TYPEDEF) {
			/* typedef declares no type name */
			warning(72);
		} else {
			/* empty declaration */
			warning(2);
		}
	  }
	| declmods deftyp notype_init_decls T_SEMI
	| declspecs deftyp T_SEMI {
		if (dcs->d_scl == TYPEDEF) {
			/* typedef declares no type name */
			warning(72);
		} else if (!dcs->d_nedecl) {
			/* empty declaration */
			warning(2);
		}
	  }
	| declspecs deftyp type_init_decls T_SEMI
	| error T_SEMI
	;

clrtyp:
	  {
		clrtyp();
	  }
	;

deftyp:
	  /* empty */ {
		deftyp();
	  }
	;

declspecs:
	  clrtyp_typespec {
		addtype($1);
	  }
	| declmods typespec {
		addtype($2);
	  }
	| declspecs declmod
	| declspecs notype_typespec {
		addtype($2);
	  }
	;

declmods:
	  clrtyp T_QUAL {
		addqual($2);
	  }
	| clrtyp T_SCLASS {
		addscl($2);
	  }
	| declmods declmod
	;

declmod:
	  T_QUAL {
		addqual($1);
	  }
	| T_SCLASS {
		addscl($1);
	  }
	;

clrtyp_typespec:
	  clrtyp notype_typespec {
		$$ = $2;
	  }
	| T_TYPENAME clrtyp {
		$$ = getsym($1)->s_type;
	  }
	;

typespec:
	  notype_typespec {
		$$ = $1;
	  }
	| T_TYPENAME {
		$$ = getsym($1)->s_type;
	  }
	;

notype_typespec:
	  T_TYPE {
		$$ = gettyp($1);
	  }
	| struct_spec {
		popdecl();
		$$ = $1;
	  }
	| enum_spec {
		popdecl();
		$$ = $1;
	  }
	;

struct_spec:
	  struct struct_tag {
		/*
		 * STDC requires that "struct a;" always introduces
		 * a new tag if "a" is not declared at current level
		 *
		 * yychar is valid because otherwise the parser would
		 * not been able to deceide if he must shift or reduce
		 */
		$$ = mktag($2, $1, 0, yychar == T_SEMI);
	  }
	| struct struct_tag {
		dcs->d_tagtyp = mktag($2, $1, 1, 0);
	  } struct_declaration {
		$$ = compltag(dcs->d_tagtyp, $4);
	  }
	| struct {
		dcs->d_tagtyp = mktag(NULL, $1, 1, 0);
	  } struct_declaration {
		$$ = compltag(dcs->d_tagtyp, $3);
	  }
	| struct error {
		symtyp = FVFT;
		$$ = gettyp(INT);
	  }
	;

struct:
	  T_SOU {
		symtyp = FTAG;
		pushdecl($1 == STRUCT ? MOS : MOU);
		dcs->d_offset = 0;
		dcs->d_stralign = CHAR_BIT;
		$$ = $1;
	  }
	;

struct_tag:
	  identifier {
		$$ = getsym($1);
	  }
	;

struct_declaration:
	  struct_decl_lbrace member_declaration_list_with_rbrace {
		$$ = $2;
	  }
	;

struct_decl_lbrace:
	  T_LBRACE {
		symtyp = FVFT;
	  }
	;

member_declaration_list_with_rbrace:
	  member_declaration_list T_SEMI T_RBRACE {
		$$ = $1;
	  }
	| member_declaration_list T_RBRACE {
		if (sflag) {
			/* syntax req. ";" after last struct/union member */
			error(66);
		} else {
			/* syntax req. ";" after last struct/union member */
			warning(66);
		}
		$$ = $1;
	  }
	| T_RBRACE {
		$$ = NULL;
	  }
	;

member_declaration_list:
	  member_declaration {
		$$ = $1;
	  }
	| member_declaration_list T_SEMI member_declaration {
		$$ = lnklst($1, $3);
	  }
	;

member_declaration:
	  noclass_declmods deftyp {
		/* too late, i know, but getsym() compensates it */
		symtyp = FMOS;
	  } notype_member_decls {
		symtyp = FVFT;
		$$ = $4;
	  }
	| noclass_declspecs deftyp {
		symtyp = FMOS;
	  } type_member_decls {
		symtyp = FVFT;
		$$ = $4;
	  }
	| noclass_declmods deftyp {
		/* struct or union member must be named */
		warning(49);
		$$ = NULL;
	  }
	| noclass_declspecs deftyp {
		/* struct or union member must be named */
		warning(49);
		$$ = NULL;
	  }
	| error {
		symtyp = FVFT;
		$$ = NULL;
	  }
	;

noclass_declspecs:
	  clrtyp_typespec {
		addtype($1);
	  }
	| noclass_declmods typespec {
		addtype($2);
	  }
	| noclass_declspecs T_QUAL {
		addqual($2);
	  }
	| noclass_declspecs notype_typespec {
		addtype($2);
	  }
	;

noclass_declmods:
	  clrtyp T_QUAL {
		addqual($2);
	  }
	| noclass_declmods T_QUAL {
		addqual($2);
	  }
	;

notype_member_decls:
	  notype_member_decl {
		$$ = decl1str($1);
	  }
	| notype_member_decls {
		symtyp = FMOS;
	  } T_COMMA type_member_decl {
		$$ = lnklst($1, decl1str($4));
	  }
	;

type_member_decls:
	  type_member_decl {
		$$ = decl1str($1);
	  }
	| type_member_decls {
		symtyp = FMOS;
	  } T_COMMA type_member_decl {
		$$ = lnklst($1, decl1str($4));
	  }
	;

notype_member_decl:
	  notype_decl {
		$$ = $1;
	  }
	| notype_decl T_COLON constant {
		$$ = bitfield($1, toicon($3));
	  }
	| {
		symtyp = FVFT;
	  } T_COLON constant {
		$$ = bitfield(NULL, toicon($3));
	  }
	;

type_member_decl:
	  type_decl {
		$$ = $1;
	  }
	| type_decl T_COLON constant {
		$$ = bitfield($1, toicon($3));
	  }
	| {
		symtyp = FVFT;
	  } T_COLON constant {
		$$ = bitfield(NULL, toicon($3));
	  }
	;

enum_spec:
	  enum enum_tag {
		$$ = mktag($2, ENUM, 0, 0);
	  }
	| enum enum_tag {
		dcs->d_tagtyp = mktag($2, ENUM, 1, 0);
	  } enum_declaration {
		$$ = compltag(dcs->d_tagtyp, $4);
	  }
	| enum {
		dcs->d_tagtyp = mktag(NULL, ENUM, 1, 0);
	  } enum_declaration {
		$$ = compltag(dcs->d_tagtyp, $3);
	  }
	| enum error {
		symtyp = FVFT;
		$$ = gettyp(INT);
	  }
	;

enum:
	  T_ENUM {
		symtyp = FTAG;
		pushdecl(ENUMCON);
	  }
	;

enum_tag:
	  identifier {
		$$ = getsym($1);
	  }
	;

enum_declaration:
	  enum_decl_lbrace enums_with_opt_comma T_RBRACE {
		$$ = $2;
	  }
	;

enum_decl_lbrace:
	  T_LBRACE {
		symtyp = FVFT;
		enumval = 0;
	  }
	;

enums_with_opt_comma:
	  enums {
		$$ = $1;
	  }
	| enums T_COMMA {
		if (sflag) {
			/* trailing "," prohibited in enum declaration */
			error(54);
		} else {
			/* trailing "," prohibited in enum declaration */
			(void)gnuism(54);
		}
		$$ = $1;
	  }
	;

enums:
	  enumerator {
		$$ = $1;
	  }
	| enums T_COMMA enumerator {
		$$ = lnklst($1, $3);
	  }
	| error {
		$$ = NULL;
	  }
	;

enumerator:
	  ename {
		$$ = ename($1, enumval, 1);
	  }
	| ename T_ASSIGN constant {
		$$ = ename($1, toicon($3), 0);
	  }
	;

ename:
	  identifier {
		$$ = getsym($1);
	  }
	;


notype_init_decls:
	  notype_init_decl
	| notype_init_decls T_COMMA type_init_decl
	;

type_init_decls:
	  type_init_decl
	| type_init_decls T_COMMA type_init_decl
	;

notype_init_decl:
	  notype_decl opt_asm_or_symbolrename {
		idecl($1, 0, $2);
		chksz($1);
	  }
	| notype_decl opt_asm_or_symbolrename {
		idecl($1, 1, $2);
	  } T_ASSIGN initializer {
		chksz($1);
	  }
	;

type_init_decl:
	  type_decl opt_asm_or_symbolrename {
		idecl($1, 0, $2);
		chksz($1);
	  }
	| type_decl opt_asm_or_symbolrename {
		idecl($1, 1, $2);
	  } T_ASSIGN initializer {
		chksz($1);
	  }
	;

notype_decl:
	  notype_direct_decl {
		$$ = $1;
	  }
	| pointer notype_direct_decl {
		$$ = addptr($2, $1);
	  }
	;

notype_direct_decl:
	  T_NAME {
		$$ = dname(getsym($1));
	  }
	| T_LPARN type_decl T_RPARN {
		$$ = $2;
	  }
	| notype_direct_decl T_LBRACK T_RBRACK {
		$$ = addarray($1, 0, 0);
	  }
	| notype_direct_decl T_LBRACK constant T_RBRACK {
		$$ = addarray($1, 1, toicon($3));
	  }
	| notype_direct_decl param_list {
		$$ = addfunc($1, $2);
		popdecl();
		blklev--;
	  }
	;

type_decl:
	  type_direct_decl {
		$$ = $1;
	  }
	| pointer type_direct_decl {
		$$ = addptr($2, $1);
	  }
	;

type_direct_decl:
	  identifier {
		$$ = dname(getsym($1));
	  }
	| T_LPARN type_decl T_RPARN {
		$$ = $2;
	  }
	| type_direct_decl T_LBRACK T_RBRACK {
		$$ = addarray($1, 0, 0);
	  }
	| type_direct_decl T_LBRACK constant T_RBRACK {
		$$ = addarray($1, 1, toicon($3));
	  }
	| type_direct_decl param_list {
		$$ = addfunc($1, $2);
		popdecl();
		blklev--;
	  }
	;

/*
 * param_decl and notype_param_decl exist to avoid a conflict in
 * argument lists. A typename enclosed in parens should always be
 * treated as a typename, not an argument.
 * "typedef int a; f(int (a));" is  "typedef int a; f(int foo(a));"
 *				not "typedef int a; f(int a);"
 */
param_decl:
	  direct_param_decl {
		$$ = $1;
	  }
	| pointer direct_param_decl {
		$$ = addptr($2, $1);
	  }
	;

direct_param_decl:
	  identifier {
		$$ = dname(getsym($1));
	  }
	| T_LPARN notype_param_decl T_RPARN {
		$$ = $2;
	  }
	| direct_param_decl T_LBRACK T_RBRACK {
		$$ = addarray($1, 0, 0);
	  }
	| direct_param_decl T_LBRACK constant T_RBRACK {
		$$ = addarray($1, 1, toicon($3));
	  }
	| direct_param_decl param_list {
		$$ = addfunc($1, $2);
		popdecl();
		blklev--;
	  }
	;

notype_param_decl:
	  direct_notype_param_decl {
		$$ = $1;
	  }
	| pointer direct_notype_param_decl {
		$$ = addptr($2, $1);
	  }
	;

direct_notype_param_decl:
	  T_NAME {
		$$ = dname(getsym($1));
	  }
	| T_LPARN notype_param_decl T_RPARN {
		$$ = $2;
	  }
	| direct_notype_param_decl T_LBRACK T_RBRACK {
		$$ = addarray($1, 0, 0);
	  }
	| direct_notype_param_decl T_LBRACK constant T_RBRACK {
		$$ = addarray($1, 1, toicon($3));
	  }
	| direct_notype_param_decl param_list {
		$$ = addfunc($1, $2);
		popdecl();
		blklev--;
	  }
	;

pointer:
	  asterisk {
		$$ = $1;
	  }
	| asterisk type_qualifier_list {
		$$ = mergepq($1, $2);
	  }
	| asterisk pointer {
		$$ = mergepq($1, $2);
	  }
	| asterisk type_qualifier_list pointer {
		$$ = mergepq(mergepq($1, $2), $3);
	  }
	;

asterisk:
	  T_MULT {
		$$ = xcalloc(1, sizeof (pqinf_t));
		$$->p_pcnt = 1;
	  }
	;

type_qualifier_list:
	  type_qualifier {
		$$ = $1;
	  }
	| type_qualifier_list type_qualifier {
		$$ = mergepq($1, $2);
	  }
	;

type_qualifier:
	  T_QUAL {
		$$ = xcalloc(1, sizeof (pqinf_t));
		if ($1 == CONST) {
			$$->p_const = 1;
		} else {
			$$->p_volatile = 1;
		}
	  }
	;

param_list:
	  id_list_lparn identifier_list T_RPARN {
		$$ = $2;
	  }
	| abs_decl_param_list {
		$$ = $1;
	  }
	;

id_list_lparn:
	  T_LPARN {
		blklev++;
		pushdecl(PARG);
	  }
	;

identifier_list:
	  T_NAME {
		$$ = iname(getsym($1));
	  }
	| identifier_list T_COMMA T_NAME {
		$$ = lnklst($1, iname(getsym($3)));
	  }
	| identifier_list error {
		$$ = $1;
	  }
	;

abs_decl_param_list:
	  abs_decl_lparn T_RPARN {
		$$ = NULL;
	  }
	| abs_decl_lparn vararg_parameter_type_list T_RPARN {
		dcs->d_proto = 1;
		$$ = $2;
	  }
	| abs_decl_lparn error T_RPARN {
		$$ = NULL;
	  }
	;

abs_decl_lparn:
	  T_LPARN {
		blklev++;
		pushdecl(PARG);
	  }
	;

vararg_parameter_type_list:
	  parameter_type_list {
		$$ = $1;
	  }
	| parameter_type_list T_COMMA T_ELLIPSE {
		dcs->d_vararg = 1;
		$$ = $1;
	  }
	| T_ELLIPSE {
		if (sflag) {
			/* ANSI C requires formal parameter before "..." */
			error(84);
		} else if (!tflag) {
			/* ANSI C requires formal parameter before "..." */
			warning(84);
		}
		dcs->d_vararg = 1;
		$$ = NULL;
	  }
	;

parameter_type_list:
	  parameter_declaration {
		$$ = $1;
	  }
	| parameter_type_list T_COMMA parameter_declaration {
		$$ = lnklst($1, $3);
	  }
	;

parameter_declaration:
	  declmods deftyp {
		$$ = decl1arg(aname(), 0);
	  }
	| declspecs deftyp {
		$$ = decl1arg(aname(), 0);
	  }
	| declmods deftyp notype_param_decl {
		$$ = decl1arg($3, 0);
	  }
	/*
	 * param_decl is needed because of following conflict:
	 * "typedef int a; f(int (a));" could be parsed as
	 * "function with argument a of type int", or
	 * "function with an abstract argument of type function".
	 * This grammar realizes the second case.
	 */
	| declspecs deftyp param_decl {
		$$ = decl1arg($3, 0);
	  }
	| declmods deftyp abs_decl {
		$$ = decl1arg($3, 0);
	  }
	| declspecs deftyp abs_decl {
		$$ = decl1arg($3, 0);
	  }
	;

opt_asm_or_symbolrename:		/* expect only one */
	  /* empty */ {
		$$ = NULL;
	  }
	| T_ASM T_LPARN T_STRING T_RPARN {
		freeyyv(&$3, T_STRING);
		$$ = NULL;
	  }
	| T_SYMBOLRENAME T_LPARN T_NAME T_RPARN {
		$$ = $3;
	  }
	;

initializer:
	  init_expr
	;

init_expr:
	  expr				%prec T_COMMA {
		mkinit($1);
	  }
	| init_lbrace init_expr_list init_rbrace
	| init_lbrace init_expr_list T_COMMA init_rbrace
	| error
	;

init_expr_list:
	  init_expr			%prec T_COMMA
	| init_expr_list T_COMMA init_expr
	;

init_lbrace:
	  T_LBRACE {
		initlbr();
	  }
	;

init_rbrace:
	  T_RBRACE {
		initrbr();
	  }
	;

type_name:
	  {
		pushdecl(ABSTRACT);
	  } abstract_declaration {
		popdecl();
		$$ = $2->s_type;
	  }
	;

abstract_declaration:
	  noclass_declmods deftyp {
		$$ = decl1abs(aname());
	  }
	| noclass_declspecs deftyp {
		$$ = decl1abs(aname());
	  }
	| noclass_declmods deftyp abs_decl {
		$$ = decl1abs($3);
	  }
	| noclass_declspecs deftyp abs_decl {
		$$ = decl1abs($3);
	  }
	;

abs_decl:
	  pointer {
		$$ = addptr(aname(), $1);
	  }
	| direct_abs_decl {
		$$ = $1;
	  }
	| pointer direct_abs_decl {
		$$ = addptr($2, $1);
	  }
	;

direct_abs_decl:
	  T_LPARN abs_decl T_RPARN {
		$$ = $2;
	  }
	| T_LBRACK T_RBRACK {
		$$ = addarray(aname(), 0, 0);
	  }
	| T_LBRACK constant T_RBRACK {
		$$ = addarray(aname(), 1, toicon($2));
	  }
	| direct_abs_decl T_LBRACK T_RBRACK {
		$$ = addarray($1, 0, 0);
	  }
	| direct_abs_decl T_LBRACK constant T_RBRACK {
		$$ = addarray($1, 1, toicon($3));
	  }
	| abs_decl_param_list {
		$$ = addfunc(aname(), $1);
		popdecl();
		blklev--;
	  }
	| direct_abs_decl abs_decl_param_list {
		$$ = addfunc($1, $2);
		popdecl();
		blklev--;
	  }
	;

stmnt:
	  labeled_stmnt
	| expr_stmnt
	| comp_stmnt
	| selection_stmnt
	| iteration_stmnt
	| jump_stmnt {
		ftflg = 0;
	  }
	| asm_stmnt
	;

labeled_stmnt:
	  label stmnt
	;

label:
	  identifier T_COLON {
		symtyp = FLAB;
		label(T_NAME, getsym($1), NULL);
	  }
	| T_CASE constant T_COLON {
		label(T_CASE, NULL, $2);
		ftflg = 1;
	  }
	| T_DEFAULT T_COLON {
		label(T_DEFAULT, NULL, NULL);
		ftflg = 1;
	  }
	;

comp_stmnt:
	  compstmnt_lbrace declaration_list opt_stmnt_list compstmnt_rbrace
	| compstmnt_lbrace opt_stmnt_list compstmnt_rbrace
	;

compstmnt_lbrace:
	  T_LBRACE {
		blklev++;
		mblklev++;
		pushdecl(AUTO);
	  }
	;

compstmnt_rbrace:
	  T_RBRACE {
		popdecl();
		freeblk();
		mblklev--;
		blklev--;
		ftflg = 0;
	  }
	;

opt_stmnt_list:
	  /* empty */
	| stmnt_list
	;

stmnt_list:
	  stmnt
	| stmnt_list stmnt {
		RESTORE();
	  }
	| stmnt_list error T_SEMI
	;

expr_stmnt:
	  expr T_SEMI {
		expr($1, 0, 0);
		ftflg = 0;
	  }
	| T_SEMI {
		ftflg = 0;
	  }
	;

selection_stmnt:
	  if_without_else {
		SAVE();
		if2();
		if3(0);
	  }
	| if_without_else T_ELSE {
		SAVE();
		if2();
	  } stmnt {
		CLRWFLGS();
		if3(1);
	  }
	| if_without_else T_ELSE error {
		CLRWFLGS();
		if3(0);
	  }
	| switch_expr stmnt {
		CLRWFLGS();
		switch2();
	  }
	| switch_expr error {
		CLRWFLGS();
		switch2();
	  }
	;

if_without_else:
	  if_expr stmnt
	| if_expr error
	;

if_expr:
	  T_IF T_LPARN expr T_RPARN {
		if1($3);
		CLRWFLGS();
	  }
	;

switch_expr:
	  T_SWITCH T_LPARN expr T_RPARN {
		switch1($3);
		CLRWFLGS();
	  }
	;

do_stmnt:
	  do stmnt {
		CLRWFLGS();
	  }
	;

iteration_stmnt:
	  while_expr stmnt {
		CLRWFLGS();
		while2();
	  }
	| while_expr error {
		CLRWFLGS();
		while2();
	  }
	| do_stmnt do_while_expr {
		do2($2);
		ftflg = 0;
	  }
	| do error {
		CLRWFLGS();
		do2(NULL);
	  }
	| for_exprs stmnt {
		CLRWFLGS();
		for2();
	  }
	| for_exprs error {
		CLRWFLGS();
		for2();
	  }
	;

while_expr:
	  T_WHILE T_LPARN expr T_RPARN {
		while1($3);
		CLRWFLGS();
	  }
	;

do:
	  T_DO {
		do1();
	  }
	;

do_while_expr:
	  T_WHILE T_LPARN expr T_RPARN T_SEMI {
		$$ = $3;
	  }
	;

for_exprs:
	  T_FOR T_LPARN opt_expr T_SEMI opt_expr T_SEMI opt_expr T_RPARN {
		for1($3, $5, $7);
		CLRWFLGS();
	  }
	;

opt_expr:
	  /* empty */ {
		$$ = NULL;
	  }
	| expr {
		$$ = $1;
	  }
	;

jump_stmnt:
	  goto identifier T_SEMI {
		dogoto(getsym($2));
	  }
	| goto error T_SEMI {
		symtyp = FVFT;
	  }
	| T_CONTINUE T_SEMI {
		docont();
	  }
	| T_BREAK T_SEMI {
		dobreak();
	  }
	| T_RETURN T_SEMI {
		doreturn(NULL);
	  }
	| T_RETURN expr T_SEMI {
		doreturn($2);
	  }
	;

goto:
	  T_GOTO {
		symtyp = FLAB;
	  }
	;

asm_stmnt:
	  T_ASM T_LPARN read_until_rparn T_SEMI {
		setasm();
	  }
	| T_ASM T_QUAL T_LPARN read_until_rparn T_SEMI {
		setasm();
	  }
	| T_ASM error
	;

read_until_rparn:
	  /* empty */ {
		ignuptorp();
	  }
	;

declaration_list:
	  declaration {
		CLRWFLGS();
	  }
	| declaration_list declaration {
		CLRWFLGS();
	  }
	;

constant:
	  expr				%prec T_COMMA {
		  $$ = $1;
	  }
	;

expr:
	  expr T_MULT expr {
		$$ = build(MULT, $1, $3);
	  }
	| expr T_DIVOP expr {
		$$ = build($2, $1, $3);
	  }
	| expr T_ADDOP expr {
		$$ = build($2, $1, $3);
	  }
	| expr T_SHFTOP expr {
		$$ = build($2, $1, $3);
	  }
	| expr T_RELOP expr {
		$$ = build($2, $1, $3);
	  }
	| expr T_EQOP expr {
		$$ = build($2, $1, $3);
	  }
	| expr T_AND expr {
		$$ = build(AND, $1, $3);
	  }
	| expr T_XOR expr {
		$$ = build(XOR, $1, $3);
	  }
	| expr T_OR expr {
		$$ = build(OR, $1, $3);
	  }
	| expr T_LOGAND expr {
		$$ = build(LOGAND, $1, $3);
	  }
	| expr T_LOGOR expr {
		$$ = build(LOGOR, $1, $3);
	  }
	| expr T_QUEST expr T_COLON expr {
		$$ = build(QUEST, $1, build(COLON, $3, $5));
	  }
	| expr T_ASSIGN expr {
		$$ = build(ASSIGN, $1, $3);
	  }
	| expr T_OPASS expr {
		$$ = build($2, $1, $3);
	  }
	| expr T_COMMA expr {
		$$ = build(COMMA, $1, $3);
	  }
	| term {
		$$ = $1;
	  }
	;

term:
	  T_NAME {
		/* XXX really necessary? */
		if (yychar < 0)
			yychar = yylex();
		$$ = getnnode(getsym($1), yychar);
	  }
	| string {
		$$ = getsnode($1);
	  }
	| T_CON {
		$$ = getcnode(gettyp($1->v_tspec), $1);
	  }
	| T_LPARN expr T_RPARN {
		if ($2 != NULL)
			$2->tn_parn = 1;
		$$ = $2;
	  }
	| term T_INCDEC {
		$$ = build($2 == INC ? INCAFT : DECAFT, $1, NULL);
	  }
	| T_INCDEC term {
		$$ = build($1 == INC ? INCBEF : DECBEF, $2, NULL);
	  }
	| T_MULT term {
		$$ = build(STAR, $2, NULL);
	  }
	| T_AND term {
		$$ = build(AMPER, $2, NULL);
	  }
	| T_UNOP term {
		$$ = build($1, $2, NULL);
	  }
	| T_ADDOP term {
		if (tflag && $1 == PLUS) {
			/* unary + is illegal in traditional C */
			warning(100);
		}
		$$ = build($1 == PLUS ? UPLUS : UMINUS, $2, NULL);
	  }
	| term T_LBRACK expr T_RBRACK {
		$$ = build(STAR, build(PLUS, $1, $3), NULL);
	  }
	| term T_LPARN T_RPARN {
		$$ = funccall($1, NULL);
	  }
	| term T_LPARN func_arg_list T_RPARN {
		$$ = funccall($1, $3);
	  }
	| term point_or_arrow T_NAME {
		if ($1 != NULL) {
			sym_t	*msym;
			/* XXX strmemb should be integrated in build() */
			if ($2 == ARROW) {
				/* must to this before strmemb is called */
				$1 = cconv($1);
			}
			msym = strmemb($1, $2, getsym($3));
			$$ = build($2, $1, getnnode(msym, 0));
		} else {
			$$ = NULL;
		}
	  }
	| T_SIZEOF term					%prec T_SIZEOF {
		if (($$ = $2 == NULL ? NULL : bldszof($2->tn_type)) != NULL)
			chkmisc($2, 0, 0, 0, 0, 0, 1);
	  }
	| T_SIZEOF T_LPARN type_name T_RPARN		%prec T_SIZEOF {
		$$ = bldszof($3);
	  }
	| T_LPARN type_name T_RPARN term		%prec T_UNOP {
		$$ = cast($4, $2);
	  }
	;

string:
	  T_STRING {
		$$ = $1;
	  }
	| T_STRING string2 {
		$$ = catstrg($1, $2);
	  }
	;

string2:
	 T_STRING {
		if (tflag) {
			/* concatenated strings are illegal in traditional C */
			warning(219);
		}
		$$ = $1;
	  }
	| string2 T_STRING {
		$$ = catstrg($1, $2);
	  }
	;

func_arg_list:
	  expr						%prec T_COMMA {
		$$ = funcarg(NULL, $1);
	  }
	| func_arg_list T_COMMA expr {
		$$ = funcarg($1, $3);
	  }
	;

point_or_arrow:
	  T_STROP {
		symtyp = FMOS;
		$$ = $1;
	  }
	;

identifier:
	  T_NAME {
		$$ = $1;
	  }
	| T_TYPENAME {
		$$ = $1;
	  }
	;

%%

/* ARGSUSED */
int
yyerror(char *msg)
{

	error(249);
	if (++sytxerr >= 5)
		norecover();
	return (0);
}

static __inline int uq_gt(uint64_t, uint64_t);
static __inline int
uq_gt(uint64_t a, uint64_t b)
{

	return (a > b);
}

static __inline int q_gt(int64_t, int64_t);
static __inline int
q_gt(int64_t a, int64_t b)
{

	return (a > b);
}

#define	q_lt(a, b)	q_gt(b, a)

/*
 * Gets a node for a constant and returns the value of this constant
 * as integer.
 * Is the node not constant or too large for int or of type float,
 * a warning will be printed.
 *
 * toicon() should be used only inside declarations. If it is used in
 * expressions, it frees the memory used for the expression.
 */
static int
toicon(tnode_t *tn)
{
	int	i;
	tspec_t	t;
	val_t	*v;

	v = constant(tn);

	/*
	 * Abstract declarations are used inside expression. To free
	 * the memory would be a fatal error.
	 */
	if (dcs->d_ctx != ABSTRACT)
		tfreeblk();

	if ((t = v->v_tspec) == FLOAT || t == DOUBLE || t == LDOUBLE) {
		i = (int)v->v_ldbl;
		/* integral constant expression expected */
		error(55);
	} else {
		i = (int)v->v_quad;
		if (isutyp(t)) {
			if (uq_gt((uint64_t)v->v_quad,
				  (uint64_t)INT_MAX)) {
				/* integral constant too large */
				warning(56);
			}
		} else {
			if (q_gt(v->v_quad, (int64_t)INT_MAX) ||
			    q_lt(v->v_quad, (int64_t)INT_MIN)) {
				/* integral constant too large */
				warning(56);
			}
		}
	}
	free(v);
	return (i);
}

static void
idecl(sym_t *decl, int initflg, sbuf_t *rename)
{
	char *s;

	initerr = 0;
	initsym = decl;

	switch (dcs->d_ctx) {
	case EXTERN:
		if (rename != NULL) {
			if (decl->s_rename != NULL)
				lerror("idecl() 1");

			s = getlblk(1, rename->sb_len + 1);
	                (void)memcpy(s, rename->sb_name, rename->sb_len + 1);
			decl->s_rename = s;
			freeyyv(&rename, T_NAME);
		}
		decl1ext(decl, initflg);
		break;
	case ARG:
		if (rename != NULL) {
			/* symbol renaming can't be used on function arguments */
			error(310);
			freeyyv(&rename, T_NAME);
			break;
		}
		(void)decl1arg(decl, initflg);
		break;
	case AUTO:
		if (rename != NULL) {
			/* symbol renaming can't be used on automatic variables */
			error(311);
			freeyyv(&rename, T_NAME);
			break;
		}
		decl1loc(decl, initflg);
		break;
	default:
		lerror("idecl() 2");
	}

	if (initflg && !initerr)
		prepinit();
}

/*
 * Discard all input tokens up to and including the next
 * unmatched right paren
 */
static void
ignuptorp(void)
{
	int	level;

	if (yychar < 0)
		yychar = yylex();
	freeyyv(&yylval, yychar);

	level = 1;
	while (yychar != T_RPARN || --level > 0) {
		if (yychar == T_LPARN) {
			level++;
		} else if (yychar <= 0) {
			break;
		}
		freeyyv(&yylval, yychar = yylex());
	}

	yyclearin;
}
