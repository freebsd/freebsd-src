/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#ident	"@(#)rpc_parse.c	1.12	93/07/05 SMI"

#ifndef lint
static char sccsid[] = "@(#)rpc_parse.c 1.8 89/02/22 (C) 1987 SMI";
#endif

/*
 * rpc_parse.c, Parser for the RPC protocol compiler
 * Copyright (C) 1987 Sun Microsystems, Inc.
 */
#include <stdio.h>
#include <string.h>
#include "rpc/types.h"
#include "rpc_scan.h"
#include "rpc_parse.h"
#include "rpc_util.h"

#define ARGNAME "arg"

extern char *make_argname __P(( char *, char * ));
static int isdefined __P(( definition * ));
static int def_struct __P(( definition * ));
static int def_program __P(( definition * ));
static int def_enum __P(( definition * ));
static int def_const __P(( definition * ));
static int def_union __P(( definition * ));
static int def_typedef __P(( definition * ));
static int get_declaration __P(( declaration *, defkind ));
static int get_prog_declaration __P(( declaration *, defkind, int ));
static int get_type __P(( char **, char **, defkind ));
static int unsigned_dec __P(( char ** ));

#ifndef __FreeBSD__
extern char *strdup();
#endif

/*
 * return the next definition you see
 */
definition *
get_definition()
{
	definition *defp;
	token tok;

	defp = ALLOC(definition);
	get_token(&tok);
	switch (tok.kind) {
	case TOK_STRUCT:
		def_struct(defp);
		break;
	case TOK_UNION:
		def_union(defp);
		break;
	case TOK_TYPEDEF:
		def_typedef(defp);
		break;
	case TOK_ENUM:
		def_enum(defp);
		break;
	case TOK_PROGRAM:
		def_program(defp);
		break;
	case TOK_CONST:
		def_const(defp);
		break;
	case TOK_EOF:
		return (NULL);
	default:
		error("definition keyword expected");
	}
	scan(TOK_SEMICOLON, &tok);
	isdefined(defp);
	return (defp);
}

static
isdefined(defp)
	definition *defp;
{
	STOREVAL(&defined, defp);
}

static
def_struct(defp)
	definition *defp;
{
	token tok;
	declaration dec;
	decl_list *decls;
	decl_list **tailp;

	defp->def_kind = DEF_STRUCT;

	scan(TOK_IDENT, &tok);
	defp->def_name = tok.str;
	scan(TOK_LBRACE, &tok);
	tailp = &defp->def.st.decls;
	do {
		get_declaration(&dec, DEF_STRUCT);
		decls = ALLOC(decl_list);
		decls->decl = dec;
		*tailp = decls;
		tailp = &decls->next;
		scan(TOK_SEMICOLON, &tok);
		peek(&tok);
	} while (tok.kind != TOK_RBRACE);
	get_token(&tok);
	*tailp = NULL;
}

static
def_program(defp)
	definition *defp;
{
	token tok;
	declaration dec;
	decl_list *decls;
	decl_list **tailp;
	version_list *vlist;
	version_list **vtailp;
	proc_list *plist;
	proc_list **ptailp;
	int num_args;
	bool_t isvoid = FALSE;	/* whether first argument is void */
	defp->def_kind = DEF_PROGRAM;
	scan(TOK_IDENT, &tok);
	defp->def_name = tok.str;
	scan(TOK_LBRACE, &tok);
	vtailp = &defp->def.pr.versions;
	tailp = &defp->def.st.decls;
	scan(TOK_VERSION, &tok);
	do {
		scan(TOK_IDENT, &tok);
		vlist = ALLOC(version_list);
		vlist->vers_name = tok.str;
		scan(TOK_LBRACE, &tok);
		ptailp = &vlist->procs;
		do {
			/* get result type */
			plist = ALLOC(proc_list);
			get_type(&plist->res_prefix, &plist->res_type,
				 DEF_PROGRAM);
			if (streq(plist->res_type, "opaque")) {
				error("illegal result type");
			}
			scan(TOK_IDENT, &tok);
			plist->proc_name = tok.str;
			scan(TOK_LPAREN, &tok);
			/* get args - first one */
			num_args = 1;
			isvoid = FALSE;
			/*
			 * type of DEF_PROGRAM in the first
			 * get_prog_declaration and DEF_STURCT in the next
			 * allows void as argument if it is the only argument
			 */
			get_prog_declaration(&dec, DEF_PROGRAM, num_args);
			if (streq(dec.type, "void"))
				isvoid = TRUE;
			decls = ALLOC(decl_list);
			plist->args.decls = decls;
			decls->decl = dec;
			tailp = &decls->next;
			/* get args */
			while (peekscan(TOK_COMMA, &tok)) {
				num_args++;
				get_prog_declaration(&dec, DEF_STRUCT,
						     num_args);
				decls = ALLOC(decl_list);
				decls->decl = dec;
				*tailp = decls;
				if (streq(dec.type, "void"))
					isvoid = TRUE;
				tailp = &decls->next;
			}
			/* multiple arguments are only allowed in newstyle */
			if (!newstyle && num_args > 1) {
				error("only one argument is allowed");
			}
			if (isvoid && num_args > 1) {
				error("illegal use of void in program definition");
			}
			*tailp = NULL;
			scan(TOK_RPAREN, &tok);
			scan(TOK_EQUAL, &tok);
			scan_num(&tok);
			scan(TOK_SEMICOLON, &tok);
			plist->proc_num = tok.str;
			plist->arg_num = num_args;
			*ptailp = plist;
			ptailp = &plist->next;
			peek(&tok);
		} while (tok.kind != TOK_RBRACE);
		*ptailp = NULL;
		*vtailp = vlist;
		vtailp = &vlist->next;
		scan(TOK_RBRACE, &tok);
		scan(TOK_EQUAL, &tok);
		scan_num(&tok);
		vlist->vers_num = tok.str;
		/* make the argument structure name for each arg */
		for (plist = vlist->procs; plist != NULL;
		     plist = plist->next) {
			plist->args.argname = make_argname(plist->proc_name,
							   vlist->vers_num);
			/* free the memory ?? */
		}
		scan(TOK_SEMICOLON, &tok);
		scan2(TOK_VERSION, TOK_RBRACE, &tok);
	} while (tok.kind == TOK_VERSION);
	scan(TOK_EQUAL, &tok);
	scan_num(&tok);
	defp->def.pr.prog_num = tok.str;
	*vtailp = NULL;
}


static
def_enum(defp)
	definition *defp;
{
	token tok;
	enumval_list *elist;
	enumval_list **tailp;

	defp->def_kind = DEF_ENUM;
	scan(TOK_IDENT, &tok);
	defp->def_name = tok.str;
	scan(TOK_LBRACE, &tok);
	tailp = &defp->def.en.vals;
	do {
		scan(TOK_IDENT, &tok);
		elist = ALLOC(enumval_list);
		elist->name = tok.str;
		elist->assignment = NULL;
		scan3(TOK_COMMA, TOK_RBRACE, TOK_EQUAL, &tok);
		if (tok.kind == TOK_EQUAL) {
			scan_num(&tok);
			elist->assignment = tok.str;
			scan2(TOK_COMMA, TOK_RBRACE, &tok);
		}
		*tailp = elist;
		tailp = &elist->next;
	} while (tok.kind != TOK_RBRACE);
	*tailp = NULL;
}

static
def_const(defp)
	definition *defp;
{
	token tok;

	defp->def_kind = DEF_CONST;
	scan(TOK_IDENT, &tok);
	defp->def_name = tok.str;
	scan(TOK_EQUAL, &tok);
	scan2(TOK_IDENT, TOK_STRCONST, &tok);
	defp->def.co = tok.str;
}

static
def_union(defp)
	definition *defp;
{
	token tok;
	declaration dec;
	case_list *cases, *tcase;
	case_list **tailp;
	int flag;

	defp->def_kind = DEF_UNION;
	scan(TOK_IDENT, &tok);
	defp->def_name = tok.str;
	scan(TOK_SWITCH, &tok);
	scan(TOK_LPAREN, &tok);
	get_declaration(&dec, DEF_UNION);
	defp->def.un.enum_decl = dec;
	tailp = &defp->def.un.cases;
	scan(TOK_RPAREN, &tok);
	scan(TOK_LBRACE, &tok);
	scan(TOK_CASE, &tok);
	while (tok.kind == TOK_CASE) {
		scan2(TOK_IDENT, TOK_CHARCONST, &tok);
		cases = ALLOC(case_list);
		cases->case_name = tok.str;
		scan(TOK_COLON, &tok);
		/* now peek at next token */
		flag = 0;
		if (peekscan(TOK_CASE, &tok)){
			do {
				scan2(TOK_IDENT, TOK_CHARCONST, &tok);
				cases->contflag = 1;
				/* continued case statement */
				*tailp = cases;
				tailp = &cases->next;
				cases = ALLOC(case_list);
				cases->case_name = tok.str;
				scan(TOK_COLON, &tok);
			} while (peekscan(TOK_CASE, &tok));
		}
		else
			if (flag)
			{

				*tailp = cases;
				tailp = &cases->next;
				cases = ALLOC(case_list);
			};

		get_declaration(&dec, DEF_UNION);
		cases->case_decl = dec;
		cases->contflag = 0; /* no continued case statement */
		*tailp = cases;
		tailp = &cases->next;
		scan(TOK_SEMICOLON, &tok);

		scan3(TOK_CASE, TOK_DEFAULT, TOK_RBRACE, &tok);
	}
	*tailp = NULL;
	if (tok.kind == TOK_DEFAULT) {
		scan(TOK_COLON, &tok);
		get_declaration(&dec, DEF_UNION);
		defp->def.un.default_decl = ALLOC(declaration);
		*defp->def.un.default_decl = dec;
		scan(TOK_SEMICOLON, &tok);
		scan(TOK_RBRACE, &tok);
	} else {
		defp->def.un.default_decl = NULL;
	}
}

static char* reserved_words[] =
{
	"array",
	"bytes",
	"destroy",
	"free",
	"getpos",
	"inline",
	"pointer",
	"reference",
	"setpos",
	"sizeof",
	"union",
	"vector",
	NULL
	};

static char* reserved_types[] =
{
	"opaque",
	"string",
	NULL
	};

/*
 * check that the given name is not one that would eventually result in
 * xdr routines that would conflict with internal XDR routines.
 */
static check_type_name(name, new_type)
int new_type;
char* name;
{
	int i;
	char tmp[100];

	for (i = 0; reserved_words[i] != NULL; i++) {
		if (strcmp(name, reserved_words[i]) == 0) {
			sprintf(tmp,
				"illegal (reserved) name :\'%s\' in type definition",
				name);
			error(tmp);
		}
	}
	if (new_type) {
		for (i = 0; reserved_types[i] != NULL; i++) {
			if (strcmp(name, reserved_types[i]) == 0) {
				sprintf(tmp,
					"illegal (reserved) name :\'%s\' in type definition",
					name);
				error(tmp);
			}
		}
	}
}



static
def_typedef(defp)
	definition *defp;
{
	declaration dec;

	defp->def_kind = DEF_TYPEDEF;
	get_declaration(&dec, DEF_TYPEDEF);
	defp->def_name = dec.name;
	check_type_name(dec.name, 1);
	defp->def.ty.old_prefix = dec.prefix;
	defp->def.ty.old_type = dec.type;
	defp->def.ty.rel = dec.rel;
	defp->def.ty.array_max = dec.array_max;
}

static
get_declaration(dec, dkind)
	declaration *dec;
	defkind dkind;
{
	token tok;

	get_type(&dec->prefix, &dec->type, dkind);
	dec->rel = REL_ALIAS;
	if (streq(dec->type, "void")) {
		return;
	}

	check_type_name(dec->type, 0);
	scan2(TOK_STAR, TOK_IDENT, &tok);
	if (tok.kind == TOK_STAR) {
		dec->rel = REL_POINTER;
		scan(TOK_IDENT, &tok);
	}
	dec->name = tok.str;
	if (peekscan(TOK_LBRACKET, &tok)) {
		if (dec->rel == REL_POINTER) {
			error("no array-of-pointer declarations -- use typedef");
		}
		dec->rel = REL_VECTOR;
		scan_num(&tok);
		dec->array_max = tok.str;
		scan(TOK_RBRACKET, &tok);
	} else if (peekscan(TOK_LANGLE, &tok)) {
		if (dec->rel == REL_POINTER) {
			error("no array-of-pointer declarations -- use typedef");
		}
		dec->rel = REL_ARRAY;
		if (peekscan(TOK_RANGLE, &tok)) {
			dec->array_max = "~0";	/* unspecified size, use max */
		} else {
			scan_num(&tok);
			dec->array_max = tok.str;
			scan(TOK_RANGLE, &tok);
		}
	}
	if (streq(dec->type, "opaque")) {
		if (dec->rel != REL_ARRAY && dec->rel != REL_VECTOR) {
			error("array declaration expected");
		}
	} else if (streq(dec->type, "string")) {
		if (dec->rel != REL_ARRAY) {
			error("variable-length array declaration expected");
		}
	}
}


static
get_prog_declaration(dec, dkind, num)
	declaration *dec;
	defkind dkind;
	int num;  /* arg number */
{
	token tok;
	char name[10];		/* argument name */

	if (dkind == DEF_PROGRAM) {
		peek(&tok);
		if (tok.kind == TOK_RPAREN) { /* no arguments */
			dec->rel = REL_ALIAS;
			dec->type = "void";
			dec->prefix = NULL;
			dec->name = NULL;
			return;
		}
	}
	get_type(&dec->prefix, &dec->type, dkind);
	dec->rel = REL_ALIAS;
	if (peekscan(TOK_IDENT, &tok)) /* optional name of argument */
		strcpy(name, tok.str);
	else
		sprintf(name, "%s%d", ARGNAME, num);
	/* default name of argument */

	dec->name = (char *) strdup(name);
	if (streq(dec->type, "void")) {
		return;
	}

	if (streq(dec->type, "opaque")) {
		error("opaque -- illegal argument type");
	}
	if (peekscan(TOK_STAR, &tok)) {
		if (streq(dec->type, "string")) {
			error("pointer to string not allowed in program arguments\n");
		}
		dec->rel = REL_POINTER;
		if (peekscan(TOK_IDENT, &tok))
			/* optional name of argument */
			dec->name = strdup(tok.str);
	}
	if (peekscan(TOK_LANGLE, &tok)) {
		if (!streq(dec->type, "string")) {
			error("arrays cannot be declared as arguments to procedures -- use typedef");
		}
		dec->rel = REL_ARRAY;
		if (peekscan(TOK_RANGLE, &tok)) {
			dec->array_max = "~0";
			/* unspecified size, use max */
		} else {
			scan_num(&tok);
			dec->array_max = tok.str;
			scan(TOK_RANGLE, &tok);
		}
	}
	if (streq(dec->type, "string")) {
		if (dec->rel != REL_ARRAY) {
			/*
			 * .x specifies just string as
			 * type of argument
			 * - make it string<>
			 */
			dec->rel = REL_ARRAY;
			dec->array_max = "~0"; /* unspecified size, use max */
		}
	}
}



static
get_type(prefixp, typep, dkind)
	char **prefixp;
	char **typep;
	defkind dkind;
{
	token tok;

	*prefixp = NULL;
	get_token(&tok);
	switch (tok.kind) {
	case TOK_IDENT:
		*typep = tok.str;
		break;
	case TOK_STRUCT:
	case TOK_ENUM:
	case TOK_UNION:
		*prefixp = tok.str;
		scan(TOK_IDENT, &tok);
		*typep = tok.str;
		break;
	case TOK_UNSIGNED:
		unsigned_dec(typep);
		break;
	case TOK_SHORT:
		*typep = "short";
		(void) peekscan(TOK_INT, &tok);
		break;
	case TOK_LONG:
		*typep = "long";
		(void) peekscan(TOK_INT, &tok);
		break;
	case TOK_HYPER:
		*typep = "longlong_t";
		(void) peekscan(TOK_INT, &tok);
		break;

	case TOK_VOID:
		if (dkind != DEF_UNION && dkind != DEF_PROGRAM) {
			error("voids allowed only inside union and program definitions with one argument");
		}
		*typep = tok.str;
		break;
	case TOK_STRING:
	case TOK_OPAQUE:
	case TOK_CHAR:
	case TOK_INT:
	case TOK_FLOAT:
	case TOK_DOUBLE:
	case TOK_BOOL:
	case TOK_QUAD:
		*typep = tok.str;
		break;
	default:
		error("expected type specifier");
	}
}

static
unsigned_dec(typep)
	char **typep;
{
	token tok;

	peek(&tok);
	switch (tok.kind) {
	case TOK_CHAR:
		get_token(&tok);
		*typep = "u_char";
		break;
	case TOK_SHORT:
		get_token(&tok);
		*typep = "u_short";
		(void) peekscan(TOK_INT, &tok);
		break;
	case TOK_LONG:
		get_token(&tok);
		*typep = "u_long";
		(void) peekscan(TOK_INT, &tok);
		break;
	case TOK_HYPER:
		get_token(&tok);
		*typep = "ulonglong_t";
		(void) peekscan(TOK_INT, &tok);
		break;
	case TOK_INT:
		get_token(&tok);
		*typep = "u_int";
		break;
	default:
		*typep = "u_int";
		break;
	}
}
