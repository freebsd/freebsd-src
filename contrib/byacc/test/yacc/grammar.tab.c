/* original parser id follows */
/* yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93" */
/* (use YYMAJOR/YYMINOR for ifdefs dependent on parser version) */

#define YYBYACC 1
#define YYMAJOR 2
#define YYMINOR 0
#define YYCHECK "yyyymmdd"

#define YYEMPTY        (-1)
#define yyclearin      (yychar = YYEMPTY)
#define yyerrok        (yyerrflag = 0)
#define YYRECOVERING() (yyerrflag != 0)
#define YYENOMEM       (-2)
#define YYEOF          0

#ifndef yyparse
#define yyparse    grammar_parse
#endif /* yyparse */

#ifndef yylex
#define yylex      grammar_lex
#endif /* yylex */

#ifndef yyerror
#define yyerror    grammar_error
#endif /* yyerror */

#ifndef yychar
#define yychar     grammar_char
#endif /* yychar */

#ifndef yyval
#define yyval      grammar_val
#endif /* yyval */

#ifndef yylval
#define yylval     grammar_lval
#endif /* yylval */

#ifndef yydebug
#define yydebug    grammar_debug
#endif /* yydebug */

#ifndef yynerrs
#define yynerrs    grammar_nerrs
#endif /* yynerrs */

#ifndef yyerrflag
#define yyerrflag  grammar_errflag
#endif /* yyerrflag */

#ifndef yylhs
#define yylhs      grammar_lhs
#endif /* yylhs */

#ifndef yylen
#define yylen      grammar_len
#endif /* yylen */

#ifndef yydefred
#define yydefred   grammar_defred
#endif /* yydefred */

#ifndef yydgoto
#define yydgoto    grammar_dgoto
#endif /* yydgoto */

#ifndef yysindex
#define yysindex   grammar_sindex
#endif /* yysindex */

#ifndef yyrindex
#define yyrindex   grammar_rindex
#endif /* yyrindex */

#ifndef yygindex
#define yygindex   grammar_gindex
#endif /* yygindex */

#ifndef yytable
#define yytable    grammar_table
#endif /* yytable */

#ifndef yycheck
#define yycheck    grammar_check
#endif /* yycheck */

#ifndef yyname
#define yyname     grammar_name
#endif /* yyname */

#ifndef yyrule
#define yyrule     grammar_rule
#endif /* yyrule */
#define YYPREFIX "grammar_"

#define YYPURE 0

#line 9 "grammar.y"
#ifdef YYBISON
#include <stdlib.h>
#define YYSTYPE_IS_DECLARED
#define yyerror yaccError
#endif

#if defined(YYBISON) || !defined(YYBYACC)
static void yyerror(const char *s);
#endif
#line 81 "grammar.y"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define OPT_LINTLIBRARY 1

#ifndef TRUE
#define	TRUE	(1)
#endif

#ifndef FALSE
#define	FALSE	(0)
#endif

/* #include "cproto.h" */
#define MAX_TEXT_SIZE 1024
#define TEXT_LEN (MAX_TEXT_SIZE / 2 - 3)

/* Prototype styles */
#if OPT_LINTLIBRARY
#define PROTO_ANSI_LLIB		-2	/* form ANSI lint-library source */
#define PROTO_LINTLIBRARY	-1	/* form lint-library source */
#endif
#define PROTO_NONE		0	/* do not output any prototypes */
#define PROTO_TRADITIONAL	1	/* comment out parameters */
#define PROTO_ABSTRACT		2	/* comment out parameter names */
#define PROTO_ANSI		3	/* ANSI C prototype */

typedef int PrototypeStyle;

typedef char boolean;

extern boolean types_out;
extern PrototypeStyle proto_style;

#define ansiLintLibrary() (proto_style == PROTO_ANSI_LLIB)
#define knrLintLibrary()  (proto_style == PROTO_LINTLIBRARY)
#define lintLibrary()     (knrLintLibrary() || ansiLintLibrary())

#if OPT_LINTLIBRARY
#define FUNC_UNKNOWN		-1	/* unspecified */
#else
#define FUNC_UNKNOWN		0	/* unspecified (same as FUNC_NONE) */
#endif
#define FUNC_NONE		0	/* not a function definition */
#define FUNC_TRADITIONAL	1	/* traditional style */
#define FUNC_ANSI		2	/* ANSI style */
#define FUNC_BOTH		3	/* both styles */

typedef int FuncDefStyle;

/* Source file text */
typedef struct text {
    char text[MAX_TEXT_SIZE];	/* source text */
    long begin; 		/* offset in temporary file */
} Text;

/* Declaration specifier flags */
#define DS_NONE 	0	/* default */
#define DS_EXTERN	1	/* contains "extern" specifier */
#define DS_STATIC	2	/* contains "static" specifier */
#define DS_CHAR 	4	/* contains "char" type specifier */
#define DS_SHORT	8	/* contains "short" type specifier */
#define DS_FLOAT	16	/* contains "float" type specifier */
#define DS_INLINE	32	/* contains "inline" specifier */
#define DS_JUNK 	64	/* we're not interested in this declaration */

/* This structure stores information about a declaration specifier. */
typedef struct decl_spec {
    unsigned short flags;	/* flags defined above */
    char *text; 		/* source text */
    long begin; 		/* offset in temporary file */
} DeclSpec;

/* This is a list of function parameters. */
typedef struct _ParameterList {
    struct parameter *first;	/* pointer to first parameter in list */
    struct parameter *last;	/* pointer to last parameter in list */  
    long begin_comment; 	/* begin offset of comment */
    long end_comment;		/* end offset of comment */
    char *comment;		/* comment at start of parameter list */
} ParameterList;

/* This structure stores information about a declarator. */
typedef struct _Declarator {
    char *name; 			/* name of variable or function */
    char *text; 			/* source text */
    long begin; 			/* offset in temporary file */
    long begin_comment; 		/* begin offset of comment */
    long end_comment;			/* end offset of comment */
    FuncDefStyle func_def;		/* style of function definition */
    ParameterList params;		/* function parameters */
    boolean pointer;			/* TRUE if it declares a pointer */
    struct _Declarator *head;		/* head function declarator */
    struct _Declarator *func_stack;	/* stack of function declarators */
    struct _Declarator *next;		/* next declarator in list */
} Declarator;

/* This structure stores information about a function parameter. */
typedef struct parameter {
    struct parameter *next;	/* next parameter in list */
    DeclSpec decl_spec;
    Declarator *declarator;
    char *comment;		/* comment following the parameter */
} Parameter;

/* This is a list of declarators. */
typedef struct declarator_list {
    Declarator *first;		/* pointer to first declarator in list */
    Declarator *last;		/* pointer to last declarator in list */  
} DeclaratorList;

/* #include "symbol.h" */
typedef struct symbol {
    struct symbol *next;	/* next symbol in list */
    char *name; 		/* name of symbol */
    char *value;		/* value of symbol (for defines) */
    short flags;		/* symbol attributes */
} Symbol;

/* parser stack entry type */
typedef union {
    Text text;
    DeclSpec decl_spec;
    Parameter *parameter;
    ParameterList param_list;
    Declarator *declarator;
    DeclaratorList decl_list;
} YYSTYPE;

/* The hash table length should be a prime number. */
#define SYM_MAX_HASH 251

typedef struct symbol_table {
    Symbol *bucket[SYM_MAX_HASH];	/* hash buckets */
} SymbolTable;

extern SymbolTable *new_symbol_table	/* Create symbol table */
	(void);
extern void free_symbol_table		/* Destroy symbol table */
	(SymbolTable *s);
extern Symbol *find_symbol		/* Lookup symbol name */
	(SymbolTable *s, const char *n);
extern Symbol *new_symbol		/* Define new symbol */
	(SymbolTable *s, const char *n, const char *v, int f);

/* #include "semantic.h" */
extern void new_decl_spec (DeclSpec *, const char *, long, int);
extern void free_decl_spec (DeclSpec *);
extern void join_decl_specs (DeclSpec *, DeclSpec *, DeclSpec *);
extern void check_untagged (DeclSpec *);
extern Declarator *new_declarator (const char *, const char *, long);
extern void free_declarator (Declarator *);
extern void new_decl_list (DeclaratorList *, Declarator *);
extern void free_decl_list (DeclaratorList *);
extern void add_decl_list (DeclaratorList *, DeclaratorList *, Declarator *);
extern Parameter *new_parameter (DeclSpec *, Declarator *);
extern void free_parameter (Parameter *);
extern void new_param_list (ParameterList *, Parameter *);
extern void free_param_list (ParameterList *);
extern void add_param_list (ParameterList *, ParameterList *, Parameter *);
extern void new_ident_list (ParameterList *);
extern void add_ident_list (ParameterList *, ParameterList *, const char *);
extern void set_param_types (ParameterList *, DeclSpec *, DeclaratorList *);
extern void gen_declarations (DeclSpec *, DeclaratorList *);
extern void gen_prototype (DeclSpec *, Declarator *);
extern void gen_func_declarator (Declarator *);
extern void gen_func_definition (DeclSpec *, Declarator *);

extern void init_parser     (void);
extern void process_file    (FILE *infile, char *name);
extern char *cur_text       (void);
extern char *cur_file_name  (void);
extern char *implied_typedef (void);
extern void include_file    (char *name, int convert);
extern char *supply_parm    (int count);
extern char *xstrdup        (const char *);
extern int already_declared (char *name);
extern int is_actual_func   (Declarator *d);
extern int lint_ellipsis    (Parameter *p);
extern int want_typedef     (void);
extern void begin_tracking  (void);
extern void begin_typedef   (void);
extern void copy_typedef    (char *s);
extern void ellipsis_varargs (Declarator *d);
extern void end_typedef     (void);
extern void flush_varargs   (void);
extern void fmt_library     (int code);
extern void imply_typedef   (const char *s);
extern void indent          (FILE *outf);
extern void put_blankline   (FILE *outf);
extern void put_body        (FILE *outf, DeclSpec *decl_spec, Declarator *declarator);
extern void put_char        (FILE *outf, int c);
extern void put_error       (void);
extern void put_newline     (FILE *outf);
extern void put_padded      (FILE *outf, const char *s);
extern void put_string      (FILE *outf, const char *s);
extern void track_in        (void);

extern boolean file_comments;
extern FuncDefStyle func_style;
extern char base_file[];

extern	int	yylex (void);

/* declaration specifier attributes for the typedef statement currently being
 * scanned
 */
static int cur_decl_spec_flags;

/* pointer to parameter list for the current function definition */
static ParameterList *func_params;

/* A parser semantic action sets this pointer to the current declarator in
 * a function parameter declaration in order to catch any comments following
 * the parameter declaration on the same line.  If the lexer scans a comment
 * and <cur_declarator> is not NULL, then the comment is attached to the
 * declarator.  To ignore subsequent comments, the lexer sets this to NULL
 * after scanning a comment or end of line.
 */
static Declarator *cur_declarator;

/* temporary string buffer */
static char buf[MAX_TEXT_SIZE];

/* table of typedef names */
static SymbolTable *typedef_names;

/* table of define names */
static SymbolTable *define_names;

/* table of type qualifiers */
static SymbolTable *type_qualifiers;

/* information about the current input file */
typedef struct {
    char *base_name;		/* base input file name */
    char *file_name;		/* current file name */
    FILE *file; 		/* input file */
    unsigned line_num;		/* current line number in input file */
    FILE *tmp_file;		/* temporary file */
    long begin_comment; 	/* tmp file offset after last written ) or ; */
    long end_comment;		/* tmp file offset after last comment */
    boolean convert;		/* if TRUE, convert function definitions */
    boolean changed;		/* TRUE if conversion done in this file */
} IncludeStack;

static IncludeStack *cur_file;	/* current input file */

/* #include "yyerror.c" */

static int haveAnsiParam (void);


/* Flags to enable us to find if a procedure returns a value.
 */
static int return_val;	/* nonzero on BRACES iff return-expression found */

static const char *
dft_decl_spec (void)
{
    return (lintLibrary() && !return_val) ? "void" : "int";
}

static int
haveAnsiParam (void)
{
    Parameter *p;
    if (func_params != 0) {
	for (p = func_params->first; p != 0; p = p->next) {
	    if (p->declarator->func_def == FUNC_ANSI) {
		return TRUE;
	    }
	}
    }
    return FALSE;
}
#line 389 "grammar.tab.c"

/* compatibility with bison */
#ifdef YYPARSE_PARAM
/* compatibility with FreeBSD */
# ifdef YYPARSE_PARAM_TYPE
#  define YYPARSE_DECL() yyparse(YYPARSE_PARAM_TYPE YYPARSE_PARAM)
# else
#  define YYPARSE_DECL() yyparse(void *YYPARSE_PARAM)
# endif
#else
# define YYPARSE_DECL() yyparse(void)
#endif

/* Parameters sent to lex. */
#ifdef YYLEX_PARAM
# define YYLEX_DECL() yylex(void *YYLEX_PARAM)
# define YYLEX yylex(YYLEX_PARAM)
#else
# define YYLEX_DECL() yylex(void)
# define YYLEX yylex()
#endif

/* Parameters sent to yyerror. */
#ifndef YYERROR_DECL
#define YYERROR_DECL() yyerror(const char *s)
#endif
#ifndef YYERROR_CALL
#define YYERROR_CALL(msg) yyerror(msg)
#endif

extern int YYPARSE_DECL();

#define T_IDENTIFIER 257
#define T_TYPEDEF_NAME 258
#define T_DEFINE_NAME 259
#define T_AUTO 260
#define T_EXTERN 261
#define T_REGISTER 262
#define T_STATIC 263
#define T_TYPEDEF 264
#define T_INLINE 265
#define T_EXTENSION 266
#define T_CHAR 267
#define T_DOUBLE 268
#define T_FLOAT 269
#define T_INT 270
#define T_VOID 271
#define T_LONG 272
#define T_SHORT 273
#define T_SIGNED 274
#define T_UNSIGNED 275
#define T_ENUM 276
#define T_STRUCT 277
#define T_UNION 278
#define T_Bool 279
#define T_Complex 280
#define T_Imaginary 281
#define T_TYPE_QUALIFIER 282
#define T_BRACKETS 283
#define T_LBRACE 284
#define T_MATCHRBRACE 285
#define T_ELLIPSIS 286
#define T_INITIALIZER 287
#define T_STRING_LITERAL 288
#define T_ASM 289
#define T_ASMARG 290
#define T_VA_DCL 291
#define YYERRCODE 256
typedef int YYINT;
static const YYINT grammar_lhs[] = {                     -1,
    0,    0,   26,   26,   27,   27,   27,   27,   27,   27,
   27,   31,   30,   30,   28,   28,   34,   28,   32,   32,
   33,   33,   35,   35,   37,   38,   29,   39,   29,   36,
   36,   36,   40,   40,    1,    1,    2,    2,    2,    3,
    3,    3,    3,    3,    3,    4,    4,    4,    4,    4,
    4,    4,    4,    4,    4,    4,    4,    4,    4,    4,
    5,    5,    6,    6,    6,   19,   19,    8,    8,    9,
   41,    9,    7,    7,    7,   25,   23,   23,   10,   10,
   11,   11,   11,   11,   11,   20,   20,   21,   21,   22,
   22,   14,   14,   15,   15,   16,   16,   16,   17,   17,
   18,   18,   24,   24,   12,   12,   12,   13,   13,   13,
   13,   13,   13,   13,
};
static const YYINT grammar_len[] = {                      2,
    0,    1,    1,    2,    1,    1,    1,    1,    3,    2,
    2,    2,    3,    3,    2,    3,    0,    5,    2,    1,
    0,    1,    1,    3,    0,    0,    7,    0,    5,    0,
    1,    1,    1,    2,    1,    2,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    3,    2,    2,    1,    1,    1,    3,    1,
    0,    4,    3,    2,    2,    1,    1,    1,    2,    1,
    1,    3,    2,    4,    4,    2,    3,    0,    1,    1,
    2,    1,    3,    1,    3,    2,    2,    1,    0,    1,
    1,    3,    1,    2,    1,    2,    1,    3,    2,    1,
    4,    3,    3,    2,
};
static const YYINT grammar_defred[] = {                   0,
    0,    0,    0,    0,   77,    0,   62,   40,    0,   42,
   43,   20,   44,    0,   46,   47,   48,   49,   54,   50,
   51,   52,   53,   76,   66,   67,   55,   56,   57,   61,
    0,    7,    0,    0,   35,   37,   38,   39,   59,   60,
   28,    0,    0,    0,  103,   81,    0,    0,    3,    5,
    6,    8,    0,   10,   11,   78,    0,   90,    0,    0,
  104,    0,   19,    0,   41,   45,   15,   36,    0,   68,
    0,    0,    0,   83,    0,    0,   64,    0,    0,   74,
    4,   58,    0,   82,   87,   91,    0,   14,   13,    9,
   16,    0,   71,    0,   31,   33,    0,    0,    0,    0,
    0,   94,    0,    0,  101,   12,   63,   73,    0,    0,
   69,    0,    0,    0,   34,    0,  110,   96,   97,    0,
    0,   84,    0,   85,    0,   23,    0,    0,   72,   26,
   29,  114,    0,    0,    0,  109,    0,   93,   95,  102,
   18,    0,    0,  108,  113,  112,    0,   24,   27,  111,
};
static const YYINT grammar_dgoto[] = {                   33,
   87,   35,   36,   37,   38,   39,   40,   69,   70,   41,
   42,  119,  120,  100,  101,  102,  103,  104,   43,   44,
   59,   60,   45,   46,   47,   48,   49,   50,   51,   52,
   77,   53,  127,  109,  128,   97,   94,  143,   72,   98,
  112,
};
static const YYINT grammar_sindex[] = {                  -2,
   -3,   27, -239, -177,    0,    0,    0,    0, -274,    0,
    0,    0,    0, -246,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
 -266,    0,    0,  455,    0,    0,    0,    0,    0,    0,
    0,  -35, -245,  128,    0,    0, -245,   -2,    0,    0,
    0,    0,  642,    0,    0,    0,  -15,    0,  -12, -239,
    0,  590,    0,  -27,    0,    0,    0,    0,  -10,    0,
  -11,  534,  -72,    0, -237, -232,    0,  -35, -232,    0,
    0,    0,  642,    0,    0,    0,  455,    0,    0,    0,
    0,   27,    0,  534,    0,    0, -222,  617,  209,   34,
   39,    0,   44,   42,    0,    0,    0,    0,   27,  -11,
    0, -200, -196, -195,    0,  174,    0,    0,    0,  -33,
  243,    0,  561,    0, -177,    0,   33,   49,    0,    0,
    0,    0,   53,   55,  417,    0,  -33,    0,    0,    0,
    0,   27, -188,    0,    0,    0,   57,    0,    0,    0,
};
static const YYINT grammar_rindex[] = {                  99,
    0,    0,  275,    0,    0,  -38,    0,    0,  481,    0,
    0,    0,    0,  509,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   30,    0,    0,    0,    0,    0,  101,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  343,  309,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   98, -182,   62,    0,    0,  133,    0,   64,  379,    0,
    0,    0,   -5,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0, -182,    0,    0,    0, -180,  -19,    0,
   65,    0,    0,   68,    0,    0,    0,    0,   51,    9,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  -13,
   19,    0,    0,    0,    0,    0,    0,   52,    0,    0,
    0,    0,    0,    0,    0,    0,   35,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
};
static const YYINT grammar_gindex[] = {                   0,
   11,  -17,    0,    0,   13,    0,    0,    0,   20,    8,
  -43,   -1,   -8,  -89,    0,   -9,    0,    0,    0,  -44,
    0,    0,    4,    0,    0,    0,   70,  -53,    0,    0,
  -18,    0,    0,    0,    0,   22,    0,    0,    0,    0,
    0,
};
#define YYTABLESIZE 924
static const YYINT grammar_table[] = {                   58,
   78,   58,   58,   58,   73,   58,  135,   61,   88,   57,
   34,    5,   56,   62,   85,   58,   68,   63,   96,    7,
   58,   98,   78,   64,   98,   84,  134,  107,   80,    3,
  107,   90,   17,   92,   17,    4,   17,    2,   75,    3,
   96,   71,   30,   89,  115,  147,   76,  106,   91,   93,
   79,   75,   70,   17,  121,   55,   32,  107,   34,  105,
  108,  114,  105,   83,    4,   68,    2,   70,    3,   68,
   80,  121,   86,   80,  122,  106,  105,   78,  106,    5,
   56,   68,  123,   99,  124,  125,  129,  130,   80,  131,
   80,  141,  142,  144,  110,  145,  149,  150,    1,  110,
    2,   30,   99,   32,   79,   92,  118,   79,  100,   21,
   22,  111,  137,  139,  133,  113,  126,   81,    0,    0,
    0,    0,   79,   57,   79,    0,   99,    0,  140,    0,
    0,    0,    0,   99,    0,    0,    0,    0,    0,    0,
    0,   70,    0,    0,    0,   99,    0,    0,    0,  148,
    0,    0,    0,    0,    0,    0,   70,    0,    0,    0,
    0,    0,    0,    0,    0,    4,    0,    2,    0,    0,
   65,    0,   65,   65,   65,    0,   65,    0,    0,    0,
    0,    0,    0,    0,    5,    6,    7,    8,   65,   10,
   11,   65,   13,   66,   15,   16,   17,   18,   19,   20,
   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,
    0,    4,    0,  116,  132,    3,    0,    0,   58,   58,
   58,   58,   58,   58,   58,   78,   58,   58,   58,   58,
   58,   58,   58,   58,   58,   58,   58,   58,   58,   58,
   58,   58,   58,   58,   58,   78,    4,   74,  116,  136,
    3,   17,   78,    1,    5,    6,    7,    8,    9,   10,
   11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,
    4,   54,  116,    5,   56,    0,   31,   80,   80,   80,
   80,   80,   80,   80,   80,   80,   80,   80,   80,   80,
   80,   80,   80,   80,   80,   80,   80,   80,   80,   80,
   80,   80,   88,   80,   88,   88,   88,    0,   88,    0,
   80,   79,   79,   79,   79,   79,   79,   79,   79,   79,
   79,   79,   79,   79,   79,   79,   79,   79,   79,   79,
   79,   79,   79,   79,   79,   79,   89,   79,   89,   89,
   89,    0,   89,    0,   79,   25,   25,   25,   25,   25,
   25,   25,   25,   25,   25,   25,   25,   25,   25,   25,
   25,   25,   25,   25,   25,   25,   25,   25,   25,   25,
   86,   25,   86,   86,    5,   56,   86,    0,   25,   65,
   65,   65,   65,   65,   65,   65,    0,   65,   65,   65,
   65,   65,   65,   65,   65,   65,   65,   65,   65,   65,
   65,   65,   65,   65,   65,   65,   75,    0,   75,   75,
   75,    0,   75,    0,    0,    0,    0,    0,    0,    0,
    5,    6,    7,    8,   65,   10,   11,   75,   13,   66,
   15,   16,   17,   18,   19,   20,   21,   22,   23,   24,
   25,   26,   27,   28,   29,   30,  117,  146,    0,    0,
    0,    0,    0,    0,    0,    5,    6,    7,    8,   65,
   10,   11,    0,   13,   66,   15,   16,   17,   18,   19,
   20,   21,   22,   23,   24,   25,   26,   27,   28,   29,
   30,  117,    4,    0,    2,    0,    3,    0,    0,    5,
   56,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   67,    0,    0,    0,    0,   41,    0,
   41,    0,   41,    0,    0,  117,    0,    0,    0,    0,
    0,   88,   88,    0,    0,    0,    0,    0,    0,   41,
    0,    0,    0,    0,    0,    0,   45,    0,   45,    0,
   45,    0,    0,    0,    0,    0,    0,   88,    0,    0,
    0,    0,    0,    0,    0,   89,   89,   45,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   89,    0,    0,    0,    0,    0,    0,    0,   86,
   86,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   86,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   75,   75,   75,   75,   75,
   75,   75,    0,   75,   75,   75,   75,   75,   75,   75,
   75,   75,   75,   75,   75,   75,   75,   75,   75,   75,
   75,   75,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   82,    7,    8,   65,   10,   11,
    0,   13,   66,   15,   16,   17,   18,   19,   20,   21,
   22,   23,   24,   25,   26,   27,   28,   29,   30,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    5,    6,    7,    8,   65,   10,   11,    0,   13,
   66,   15,   16,   17,   18,   19,   20,   21,   22,   23,
   24,   25,   26,   27,   28,   29,   30,   41,   41,   41,
   41,   41,   41,   41,    0,   41,   41,   41,   41,   41,
   41,   41,   41,   41,   41,   41,   41,   41,   41,   41,
   41,   41,   41,    0,    0,   45,   45,   45,   45,   45,
   45,   45,    0,   45,   45,   45,   45,   45,   45,   45,
   45,   45,   45,   45,   45,   45,   45,   45,   45,   45,
   45,   82,    7,    8,   65,   10,   11,   12,   13,   14,
   15,   16,   17,   18,   19,   20,   21,   22,   23,   24,
   25,   26,   27,   28,   29,   30,    0,    0,   82,    7,
    8,   65,   10,   11,   95,   13,   66,   15,   16,   17,
   18,   19,   20,   21,   22,   23,   24,   25,   26,   27,
   28,   29,   30,    0,    0,    0,  138,   82,    7,    8,
   65,   10,   11,   12,   13,   14,   15,   16,   17,   18,
   19,   20,   21,   22,   23,   24,   25,   26,   27,   28,
   29,   30,    0,   75,   82,    7,    8,   65,   10,   11,
   12,   13,   14,   15,   16,   17,   18,   19,   20,   21,
   22,   23,   24,   25,   26,   27,   28,   29,   30,   82,
    7,    8,   65,   10,   11,    0,   13,   66,   15,   16,
   17,   18,   19,   20,   21,   22,   23,   24,   25,   26,
   27,   28,   29,   30,
};
static const YYINT grammar_check[] = {                   38,
   44,   40,   41,   42,   40,   44,   40,    4,   62,    2,
    0,  257,  258,  288,   59,    3,   34,  264,   72,  259,
   59,   41,   61,  290,   44,   41,  116,   41,   47,   42,
   44,   59,   38,   44,   40,   38,   42,   40,  284,   42,
   94,   34,  282,   62,   98,  135,   43,  285,   59,   61,
   47,  284,   44,   59,   99,   59,   59,   76,   48,   41,
   79,  284,   44,   53,   38,   83,   40,   59,   42,   87,
   41,  116,   60,   44,   41,   41,   73,  121,   44,  257,
  258,   99,   44,   73,   41,   44,  287,  284,   59,  285,
   61,   59,   44,   41,   87,   41,  285,   41,    0,   92,
    0,  284,   41,  284,   41,   41,   99,   44,   41,   59,
   59,   92,  121,  123,  116,   94,  109,   48,   -1,   -1,
   -1,   -1,   59,  116,   61,   -1,  116,   -1,  125,   -1,
   -1,   -1,   -1,  123,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   44,   -1,   -1,   -1,  135,   -1,   -1,   -1,  142,
   -1,   -1,   -1,   -1,   -1,   -1,   59,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   38,   -1,   40,   -1,   -1,
   38,   -1,   40,   41,   42,   -1,   44,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  257,  258,  259,  260,  261,  262,
  263,   59,  265,  266,  267,  268,  269,  270,  271,  272,
  273,  274,  275,  276,  277,  278,  279,  280,  281,  282,
   -1,   38,   -1,   40,   41,   42,   -1,   -1,  257,  258,
  259,  260,  261,  262,  263,  264,  265,  266,  267,  268,
  269,  270,  271,  272,  273,  274,  275,  276,  277,  278,
  279,  280,  281,  282,  283,  284,   38,  283,   40,  283,
   42,  257,  291,  256,  257,  258,  259,  260,  261,  262,
  263,  264,  265,  266,  267,  268,  269,  270,  271,  272,
  273,  274,  275,  276,  277,  278,  279,  280,  281,  282,
   38,  285,   40,  257,  258,   -1,  289,  258,  259,  260,
  261,  262,  263,  264,  265,  266,  267,  268,  269,  270,
  271,  272,  273,  274,  275,  276,  277,  278,  279,  280,
  281,  282,   38,  284,   40,   41,   42,   -1,   44,   -1,
  291,  258,  259,  260,  261,  262,  263,  264,  265,  266,
  267,  268,  269,  270,  271,  272,  273,  274,  275,  276,
  277,  278,  279,  280,  281,  282,   38,  284,   40,   41,
   42,   -1,   44,   -1,  291,  258,  259,  260,  261,  262,
  263,  264,  265,  266,  267,  268,  269,  270,  271,  272,
  273,  274,  275,  276,  277,  278,  279,  280,  281,  282,
   38,  284,   40,   41,  257,  258,   44,   -1,  291,  257,
  258,  259,  260,  261,  262,  263,   -1,  265,  266,  267,
  268,  269,  270,  271,  272,  273,  274,  275,  276,  277,
  278,  279,  280,  281,  282,  283,   38,   -1,   40,   41,
   42,   -1,   44,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  257,  258,  259,  260,  261,  262,  263,   59,  265,  266,
  267,  268,  269,  270,  271,  272,  273,  274,  275,  276,
  277,  278,  279,  280,  281,  282,  283,   41,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  257,  258,  259,  260,  261,
  262,  263,   -1,  265,  266,  267,  268,  269,  270,  271,
  272,  273,  274,  275,  276,  277,  278,  279,  280,  281,
  282,  283,   38,   -1,   40,   -1,   42,   -1,   -1,  257,
  258,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   59,   -1,   -1,   -1,   -1,   38,   -1,
   40,   -1,   42,   -1,   -1,  283,   -1,   -1,   -1,   -1,
   -1,  257,  258,   -1,   -1,   -1,   -1,   -1,   -1,   59,
   -1,   -1,   -1,   -1,   -1,   -1,   38,   -1,   40,   -1,
   42,   -1,   -1,   -1,   -1,   -1,   -1,  283,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  257,  258,   59,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  283,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  257,
  258,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  283,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  257,  258,  259,  260,  261,
  262,  263,   -1,  265,  266,  267,  268,  269,  270,  271,
  272,  273,  274,  275,  276,  277,  278,  279,  280,  281,
  282,  283,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  258,  259,  260,  261,  262,  263,
   -1,  265,  266,  267,  268,  269,  270,  271,  272,  273,
  274,  275,  276,  277,  278,  279,  280,  281,  282,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  257,  258,  259,  260,  261,  262,  263,   -1,  265,
  266,  267,  268,  269,  270,  271,  272,  273,  274,  275,
  276,  277,  278,  279,  280,  281,  282,  257,  258,  259,
  260,  261,  262,  263,   -1,  265,  266,  267,  268,  269,
  270,  271,  272,  273,  274,  275,  276,  277,  278,  279,
  280,  281,  282,   -1,   -1,  257,  258,  259,  260,  261,
  262,  263,   -1,  265,  266,  267,  268,  269,  270,  271,
  272,  273,  274,  275,  276,  277,  278,  279,  280,  281,
  282,  258,  259,  260,  261,  262,  263,  264,  265,  266,
  267,  268,  269,  270,  271,  272,  273,  274,  275,  276,
  277,  278,  279,  280,  281,  282,   -1,   -1,  258,  259,
  260,  261,  262,  263,  291,  265,  266,  267,  268,  269,
  270,  271,  272,  273,  274,  275,  276,  277,  278,  279,
  280,  281,  282,   -1,   -1,   -1,  286,  258,  259,  260,
  261,  262,  263,  264,  265,  266,  267,  268,  269,  270,
  271,  272,  273,  274,  275,  276,  277,  278,  279,  280,
  281,  282,   -1,  284,  258,  259,  260,  261,  262,  263,
  264,  265,  266,  267,  268,  269,  270,  271,  272,  273,
  274,  275,  276,  277,  278,  279,  280,  281,  282,  258,
  259,  260,  261,  262,  263,   -1,  265,  266,  267,  268,
  269,  270,  271,  272,  273,  274,  275,  276,  277,  278,
  279,  280,  281,  282,
};
#define YYFINAL 33
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 291
#define YYUNDFTOKEN 335
#define YYTRANSLATE(a) ((a) > YYMAXTOKEN ? YYUNDFTOKEN : (a))
#if YYDEBUG
static const char *const grammar_name[] = {

"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,"'&'",0,"'('","')'","'*'",0,"','",0,0,0,0,0,0,0,0,0,0,0,0,0,0,"';'",0,
"'='",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
"T_IDENTIFIER","T_TYPEDEF_NAME","T_DEFINE_NAME","T_AUTO","T_EXTERN",
"T_REGISTER","T_STATIC","T_TYPEDEF","T_INLINE","T_EXTENSION","T_CHAR",
"T_DOUBLE","T_FLOAT","T_INT","T_VOID","T_LONG","T_SHORT","T_SIGNED",
"T_UNSIGNED","T_ENUM","T_STRUCT","T_UNION","T_Bool","T_Complex","T_Imaginary",
"T_TYPE_QUALIFIER","T_BRACKETS","T_LBRACE","T_MATCHRBRACE","T_ELLIPSIS",
"T_INITIALIZER","T_STRING_LITERAL","T_ASM","T_ASMARG","T_VA_DCL",0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
"illegal-symbol",
};
static const char *const grammar_rule[] = {
"$accept : program",
"program :",
"program : translation_unit",
"translation_unit : external_declaration",
"translation_unit : translation_unit external_declaration",
"external_declaration : declaration",
"external_declaration : function_definition",
"external_declaration : ';'",
"external_declaration : linkage_specification",
"external_declaration : T_ASM T_ASMARG ';'",
"external_declaration : error T_MATCHRBRACE",
"external_declaration : error ';'",
"braces : T_LBRACE T_MATCHRBRACE",
"linkage_specification : T_EXTERN T_STRING_LITERAL braces",
"linkage_specification : T_EXTERN T_STRING_LITERAL declaration",
"declaration : decl_specifiers ';'",
"declaration : decl_specifiers init_declarator_list ';'",
"$$1 :",
"declaration : any_typedef decl_specifiers $$1 opt_declarator_list ';'",
"any_typedef : T_EXTENSION T_TYPEDEF",
"any_typedef : T_TYPEDEF",
"opt_declarator_list :",
"opt_declarator_list : declarator_list",
"declarator_list : declarator",
"declarator_list : declarator_list ',' declarator",
"$$2 :",
"$$3 :",
"function_definition : decl_specifiers declarator $$2 opt_declaration_list T_LBRACE $$3 T_MATCHRBRACE",
"$$4 :",
"function_definition : declarator $$4 opt_declaration_list T_LBRACE T_MATCHRBRACE",
"opt_declaration_list :",
"opt_declaration_list : T_VA_DCL",
"opt_declaration_list : declaration_list",
"declaration_list : declaration",
"declaration_list : declaration_list declaration",
"decl_specifiers : decl_specifier",
"decl_specifiers : decl_specifiers decl_specifier",
"decl_specifier : storage_class",
"decl_specifier : type_specifier",
"decl_specifier : type_qualifier",
"storage_class : T_AUTO",
"storage_class : T_EXTERN",
"storage_class : T_REGISTER",
"storage_class : T_STATIC",
"storage_class : T_INLINE",
"storage_class : T_EXTENSION",
"type_specifier : T_CHAR",
"type_specifier : T_DOUBLE",
"type_specifier : T_FLOAT",
"type_specifier : T_INT",
"type_specifier : T_LONG",
"type_specifier : T_SHORT",
"type_specifier : T_SIGNED",
"type_specifier : T_UNSIGNED",
"type_specifier : T_VOID",
"type_specifier : T_Bool",
"type_specifier : T_Complex",
"type_specifier : T_Imaginary",
"type_specifier : T_TYPEDEF_NAME",
"type_specifier : struct_or_union_specifier",
"type_specifier : enum_specifier",
"type_qualifier : T_TYPE_QUALIFIER",
"type_qualifier : T_DEFINE_NAME",
"struct_or_union_specifier : struct_or_union any_id braces",
"struct_or_union_specifier : struct_or_union braces",
"struct_or_union_specifier : struct_or_union any_id",
"struct_or_union : T_STRUCT",
"struct_or_union : T_UNION",
"init_declarator_list : init_declarator",
"init_declarator_list : init_declarator_list ',' init_declarator",
"init_declarator : declarator",
"$$5 :",
"init_declarator : declarator '=' $$5 T_INITIALIZER",
"enum_specifier : enumeration any_id braces",
"enum_specifier : enumeration braces",
"enum_specifier : enumeration any_id",
"enumeration : T_ENUM",
"any_id : T_IDENTIFIER",
"any_id : T_TYPEDEF_NAME",
"declarator : pointer direct_declarator",
"declarator : direct_declarator",
"direct_declarator : identifier_or_ref",
"direct_declarator : '(' declarator ')'",
"direct_declarator : direct_declarator T_BRACKETS",
"direct_declarator : direct_declarator '(' parameter_type_list ')'",
"direct_declarator : direct_declarator '(' opt_identifier_list ')'",
"pointer : '*' opt_type_qualifiers",
"pointer : '*' opt_type_qualifiers pointer",
"opt_type_qualifiers :",
"opt_type_qualifiers : type_qualifier_list",
"type_qualifier_list : type_qualifier",
"type_qualifier_list : type_qualifier_list type_qualifier",
"parameter_type_list : parameter_list",
"parameter_type_list : parameter_list ',' T_ELLIPSIS",
"parameter_list : parameter_declaration",
"parameter_list : parameter_list ',' parameter_declaration",
"parameter_declaration : decl_specifiers declarator",
"parameter_declaration : decl_specifiers abs_declarator",
"parameter_declaration : decl_specifiers",
"opt_identifier_list :",
"opt_identifier_list : identifier_list",
"identifier_list : any_id",
"identifier_list : identifier_list ',' any_id",
"identifier_or_ref : any_id",
"identifier_or_ref : '&' any_id",
"abs_declarator : pointer",
"abs_declarator : pointer direct_abs_declarator",
"abs_declarator : direct_abs_declarator",
"direct_abs_declarator : '(' abs_declarator ')'",
"direct_abs_declarator : direct_abs_declarator T_BRACKETS",
"direct_abs_declarator : T_BRACKETS",
"direct_abs_declarator : direct_abs_declarator '(' parameter_type_list ')'",
"direct_abs_declarator : direct_abs_declarator '(' ')'",
"direct_abs_declarator : '(' parameter_type_list ')'",
"direct_abs_declarator : '(' ')'",

};
#endif

#if YYDEBUG
int      yydebug;
#endif

int      yyerrflag;
int      yychar;
YYSTYPE  yyval;
YYSTYPE  yylval;
int      yynerrs;

/* define the initial stack-sizes */
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH  YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH  10000
#endif
#endif

#define YYINITSTACKSIZE 200

typedef struct {
    unsigned stacksize;
    YYINT    *s_base;
    YYINT    *s_mark;
    YYINT    *s_last;
    YYSTYPE  *l_base;
    YYSTYPE  *l_mark;
} YYSTACKDATA;
/* variables for the parser stack */
static YYSTACKDATA yystack;
#line 1015 "grammar.y"

/* lex.yy.c */
#define BEGIN yy_start = 1 + 2 *

#define CPP1 1
#define INIT1 2
#define INIT2 3
#define CURLY 4
#define LEXYACC 5
#define ASM 6
#define CPP_INLINE 7

extern char *yytext;
extern FILE *yyin, *yyout;

static int curly;			/* number of curly brace nesting levels */
static int ly_count;			/* number of occurrences of %% */
static int inc_depth;			/* include nesting level */
static SymbolTable *included_files;	/* files already included */
static int yy_start = 0;		/* start state number */

#define grammar_error(s) yaccError(s)

static void
yaccError (const char *msg)
{
    func_params = NULL;
    put_error();		/* tell what line we're on, and what file */
    fprintf(stderr, "%s at token '%s'\n", msg, yytext);
}

/* Initialize the table of type qualifier keywords recognized by the lexical
 * analyzer.
 */
void
init_parser (void)
{
    static const char *keywords[] = {
	"const",
	"restrict",
	"volatile",
	"interrupt",
#ifdef vms
	"noshare",
	"readonly",
#endif
#if defined(MSDOS) || defined(OS2)
	"__cdecl",
	"__export",
	"__far",
	"__fastcall",
	"__fortran",
	"__huge",
	"__inline",
	"__interrupt",
	"__loadds",
	"__near",
	"__pascal",
	"__saveregs",
	"__segment",
	"__stdcall",
	"__syscall",
	"_cdecl",
	"_cs",
	"_ds",
	"_es",
	"_export",
	"_far",
	"_fastcall",
	"_fortran",
	"_huge",
	"_interrupt",
	"_loadds",
	"_near",
	"_pascal",
	"_saveregs",
	"_seg",
	"_segment",
	"_ss",
	"cdecl",
	"far",
	"huge",
	"near",
	"pascal",
#ifdef OS2
	"__far16",
#endif
#endif
#ifdef __GNUC__
	/* gcc aliases */
	"__builtin_va_arg",
	"__builtin_va_list",
	"__const",
	"__const__",
	"__inline",
	"__inline__",
	"__restrict",
	"__restrict__",
	"__volatile",
	"__volatile__",
#endif
    };
    unsigned i;

    /* Initialize type qualifier table. */
    type_qualifiers = new_symbol_table();
    for (i = 0; i < sizeof(keywords)/sizeof(keywords[0]); ++i) {
	new_symbol(type_qualifiers, keywords[i], NULL, DS_NONE);
    }
}

/* Process the C source file.  Write function prototypes to the standard
 * output.  Convert function definitions and write the converted source
 * code to a temporary file.
 */
void
process_file (FILE *infile, char *name)
{
    char *s;

    if (strlen(name) > 2) {
	s = name + strlen(name) - 2;
	if (*s == '.') {
	    ++s;
	    if (*s == 'l' || *s == 'y')
		BEGIN LEXYACC;
#if defined(MSDOS) || defined(OS2)
	    if (*s == 'L' || *s == 'Y')
		BEGIN LEXYACC;
#endif
	}
    }

    included_files = new_symbol_table();
    typedef_names = new_symbol_table();
    define_names = new_symbol_table();
    inc_depth = -1;
    curly = 0;
    ly_count = 0;
    func_params = NULL;
    yyin = infile;
    include_file(strcpy(base_file, name), func_style != FUNC_NONE);
    if (file_comments) {
#if OPT_LINTLIBRARY
    	if (lintLibrary()) {
	    put_blankline(stdout);
	    begin_tracking();
	}
#endif
	put_string(stdout, "/* ");
	put_string(stdout, cur_file_name());
	put_string(stdout, " */\n");
    }
    yyparse();
    free_symbol_table(define_names);
    free_symbol_table(typedef_names);
    free_symbol_table(included_files);
}

#ifdef NO_LEAKS
void
free_parser(void)
{
    free_symbol_table (type_qualifiers);
#ifdef FLEX_SCANNER
    if (yy_current_buffer != 0)
	yy_delete_buffer(yy_current_buffer);
#endif
}
#endif
#line 1095 "grammar.tab.c"

#if YYDEBUG
#include <stdio.h>	/* needed for printf */
#endif

#include <stdlib.h>	/* needed for malloc, etc */
#include <string.h>	/* needed for memset */

/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(YYSTACKDATA *data)
{
    int i;
    unsigned newsize;
    YYINT *newss;
    YYSTYPE *newvs;

    if ((newsize = data->stacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return YYENOMEM;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = (int) (data->s_mark - data->s_base);
    newss = (YYINT *)realloc(data->s_base, newsize * sizeof(*newss));
    if (newss == NULL)
        return YYENOMEM;

    data->s_base = newss;
    data->s_mark = newss + i;

    newvs = (YYSTYPE *)realloc(data->l_base, newsize * sizeof(*newvs));
    if (newvs == NULL)
        return YYENOMEM;

    data->l_base = newvs;
    data->l_mark = newvs + i;

    data->stacksize = newsize;
    data->s_last = data->s_base + newsize - 1;
    return 0;
}

#if YYPURE || defined(YY_NO_LEAKS)
static void yyfreestack(YYSTACKDATA *data)
{
    free(data->s_base);
    free(data->l_base);
    memset(data, 0, sizeof(*data));
}
#else
#define yyfreestack(data) /* nothing */
#endif

#define YYABORT  goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR  goto yyerrlab

int
YYPARSE_DECL()
{
    int yym, yyn, yystate;
#if YYDEBUG
    const char *yys;

    if ((yys = getenv("YYDEBUG")) != NULL)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    /* yym is set below */
    /* yyn is set below */
    yynerrs = 0;
    yyerrflag = 0;
    yychar = YYEMPTY;
    yystate = 0;

#if YYPURE
    memset(&yystack, 0, sizeof(yystack));
#endif

    if (yystack.s_base == NULL && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
    yystack.s_mark = yystack.s_base;
    yystack.l_mark = yystack.l_base;
    yystate = 0;
    *yystack.s_mark = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        yychar = YYLEX;
        if (yychar < 0) yychar = YYEOF;
#if YYDEBUG
        if (yydebug)
        {
            if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if (((yyn = yysindex[yystate]) != 0) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
        yystate = yytable[yyn];
        *++yystack.s_mark = yytable[yyn];
        *++yystack.l_mark = yylval;
        yychar = YYEMPTY;
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if (((yyn = yyrindex[yystate]) != 0) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag != 0) goto yyinrecovery;

    YYERROR_CALL("syntax error");

    goto yyerrlab; /* redundant goto avoids 'unused label' warning */
yyerrlab:
    ++yynerrs;

yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if (((yyn = yysindex[*yystack.s_mark]) != 0) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yystack.s_mark, yytable[yyn]);
#endif
                if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
                yystate = yytable[yyn];
                *++yystack.s_mark = yytable[yyn];
                *++yystack.l_mark = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yystack.s_mark);
#endif
                if (yystack.s_mark <= yystack.s_base) goto yyabort;
                --yystack.s_mark;
                --yystack.l_mark;
            }
        }
    }
    else
    {
        if (yychar == YYEOF) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = YYEMPTY;
        goto yyloop;
    }

yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym > 0)
        yyval = yystack.l_mark[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);

    switch (yyn)
    {
case 10:
#line 378 "grammar.y"
	{
	    yyerrok;
	}
#line 1299 "grammar.tab.c"
break;
case 11:
#line 382 "grammar.y"
	{
	    yyerrok;
	}
#line 1306 "grammar.tab.c"
break;
case 13:
#line 393 "grammar.y"
	{
	    /* Provide an empty action here so bison will not complain about
	     * incompatible types in the default action it normally would
	     * have generated.
	     */
	}
#line 1316 "grammar.tab.c"
break;
case 14:
#line 400 "grammar.y"
	{
	    /* empty */
	}
#line 1323 "grammar.tab.c"
break;
case 15:
#line 407 "grammar.y"
	{
#if OPT_LINTLIBRARY
	    if (types_out && want_typedef()) {
		gen_declarations(&yystack.l_mark[-1].decl_spec, (DeclaratorList *)0);
		flush_varargs();
	    }
#endif
	    free_decl_spec(&yystack.l_mark[-1].decl_spec);
	    end_typedef();
	}
#line 1337 "grammar.tab.c"
break;
case 16:
#line 418 "grammar.y"
	{
	    if (func_params != NULL) {
		set_param_types(func_params, &yystack.l_mark[-2].decl_spec, &yystack.l_mark[-1].decl_list);
	    } else {
		gen_declarations(&yystack.l_mark[-2].decl_spec, &yystack.l_mark[-1].decl_list);
#if OPT_LINTLIBRARY
		flush_varargs();
#endif
		free_decl_list(&yystack.l_mark[-1].decl_list);
	    }
	    free_decl_spec(&yystack.l_mark[-2].decl_spec);
	    end_typedef();
	}
#line 1354 "grammar.tab.c"
break;
case 17:
#line 432 "grammar.y"
	{
	    cur_decl_spec_flags = yystack.l_mark[0].decl_spec.flags;
	    free_decl_spec(&yystack.l_mark[0].decl_spec);
	}
#line 1362 "grammar.tab.c"
break;
case 18:
#line 437 "grammar.y"
	{
	    end_typedef();
	}
#line 1369 "grammar.tab.c"
break;
case 19:
#line 444 "grammar.y"
	{
	    begin_typedef();
	}
#line 1376 "grammar.tab.c"
break;
case 20:
#line 448 "grammar.y"
	{
	    begin_typedef();
	}
#line 1383 "grammar.tab.c"
break;
case 23:
#line 460 "grammar.y"
	{
	    int flags = cur_decl_spec_flags;

	    /* If the typedef is a pointer type, then reset the short type
	     * flags so it does not get promoted.
	     */
	    if (strcmp(yystack.l_mark[0].declarator->text, yystack.l_mark[0].declarator->name) != 0)
		flags &= ~(DS_CHAR | DS_SHORT | DS_FLOAT);
	    new_symbol(typedef_names, yystack.l_mark[0].declarator->name, NULL, flags);
	    free_declarator(yystack.l_mark[0].declarator);
	}
#line 1398 "grammar.tab.c"
break;
case 24:
#line 472 "grammar.y"
	{
	    int flags = cur_decl_spec_flags;

	    if (strcmp(yystack.l_mark[0].declarator->text, yystack.l_mark[0].declarator->name) != 0)
		flags &= ~(DS_CHAR | DS_SHORT | DS_FLOAT);
	    new_symbol(typedef_names, yystack.l_mark[0].declarator->name, NULL, flags);
	    free_declarator(yystack.l_mark[0].declarator);
	}
#line 1410 "grammar.tab.c"
break;
case 25:
#line 484 "grammar.y"
	{
	    check_untagged(&yystack.l_mark[-1].decl_spec);
	    if (yystack.l_mark[0].declarator->func_def == FUNC_NONE) {
		yyerror("syntax error");
		YYERROR;
	    }
	    func_params = &(yystack.l_mark[0].declarator->head->params);
	    func_params->begin_comment = cur_file->begin_comment;
	    func_params->end_comment = cur_file->end_comment;
	}
#line 1424 "grammar.tab.c"
break;
case 26:
#line 495 "grammar.y"
	{
	    /* If we're converting to K&R and we've got a nominally K&R
	     * function which has a parameter which is ANSI (i.e., a prototyped
	     * function pointer), then we must override the deciphered value of
	     * 'func_def' so that the parameter will be converted.
	     */
	    if (func_style == FUNC_TRADITIONAL
	     && haveAnsiParam()
	     && yystack.l_mark[-3].declarator->head->func_def == func_style) {
		yystack.l_mark[-3].declarator->head->func_def = FUNC_BOTH;
	    }

	    func_params = NULL;

	    if (cur_file->convert)
		gen_func_definition(&yystack.l_mark[-4].decl_spec, yystack.l_mark[-3].declarator);
	    gen_prototype(&yystack.l_mark[-4].decl_spec, yystack.l_mark[-3].declarator);
#if OPT_LINTLIBRARY
	    flush_varargs();
#endif
	    free_decl_spec(&yystack.l_mark[-4].decl_spec);
	    free_declarator(yystack.l_mark[-3].declarator);
	}
#line 1451 "grammar.tab.c"
break;
case 28:
#line 520 "grammar.y"
	{
	    if (yystack.l_mark[0].declarator->func_def == FUNC_NONE) {
		yyerror("syntax error");
		YYERROR;
	    }
	    func_params = &(yystack.l_mark[0].declarator->head->params);
	    func_params->begin_comment = cur_file->begin_comment;
	    func_params->end_comment = cur_file->end_comment;
	}
#line 1464 "grammar.tab.c"
break;
case 29:
#line 530 "grammar.y"
	{
	    DeclSpec decl_spec;

	    func_params = NULL;

	    new_decl_spec(&decl_spec, dft_decl_spec(), yystack.l_mark[-4].declarator->begin, DS_NONE);
	    if (cur_file->convert)
		gen_func_definition(&decl_spec, yystack.l_mark[-4].declarator);
	    gen_prototype(&decl_spec, yystack.l_mark[-4].declarator);
#if OPT_LINTLIBRARY
	    flush_varargs();
#endif
	    free_decl_spec(&decl_spec);
	    free_declarator(yystack.l_mark[-4].declarator);
	}
#line 1483 "grammar.tab.c"
break;
case 36:
#line 561 "grammar.y"
	{
	    join_decl_specs(&yyval.decl_spec, &yystack.l_mark[-1].decl_spec, &yystack.l_mark[0].decl_spec);
	    free(yystack.l_mark[-1].decl_spec.text);
	    free(yystack.l_mark[0].decl_spec.text);
	}
#line 1492 "grammar.tab.c"
break;
case 40:
#line 576 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_NONE);
	}
#line 1499 "grammar.tab.c"
break;
case 41:
#line 580 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_EXTERN);
	}
#line 1506 "grammar.tab.c"
break;
case 42:
#line 584 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_NONE);
	}
#line 1513 "grammar.tab.c"
break;
case 43:
#line 588 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_STATIC);
	}
#line 1520 "grammar.tab.c"
break;
case 44:
#line 592 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_INLINE);
	}
#line 1527 "grammar.tab.c"
break;
case 45:
#line 596 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_JUNK);
	}
#line 1534 "grammar.tab.c"
break;
case 46:
#line 603 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_CHAR);
	}
#line 1541 "grammar.tab.c"
break;
case 47:
#line 607 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_NONE);
	}
#line 1548 "grammar.tab.c"
break;
case 48:
#line 611 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_FLOAT);
	}
#line 1555 "grammar.tab.c"
break;
case 49:
#line 615 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_NONE);
	}
#line 1562 "grammar.tab.c"
break;
case 50:
#line 619 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_NONE);
	}
#line 1569 "grammar.tab.c"
break;
case 51:
#line 623 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_SHORT);
	}
#line 1576 "grammar.tab.c"
break;
case 52:
#line 627 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_NONE);
	}
#line 1583 "grammar.tab.c"
break;
case 53:
#line 631 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_NONE);
	}
#line 1590 "grammar.tab.c"
break;
case 54:
#line 635 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_NONE);
	}
#line 1597 "grammar.tab.c"
break;
case 55:
#line 639 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_CHAR);
	}
#line 1604 "grammar.tab.c"
break;
case 56:
#line 643 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_NONE);
	}
#line 1611 "grammar.tab.c"
break;
case 57:
#line 647 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_NONE);
	}
#line 1618 "grammar.tab.c"
break;
case 58:
#line 651 "grammar.y"
	{
	    Symbol *s;
	    s = find_symbol(typedef_names, yystack.l_mark[0].text.text);
	    if (s != NULL)
		new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, s->flags);
	}
#line 1628 "grammar.tab.c"
break;
case 61:
#line 663 "grammar.y"
	{
	    new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, DS_NONE);
	}
#line 1635 "grammar.tab.c"
break;
case 62:
#line 667 "grammar.y"
	{
	    /* This rule allows the <pointer> nonterminal to scan #define
	     * names as if they were type modifiers.
	     */
	    Symbol *s;
	    s = find_symbol(define_names, yystack.l_mark[0].text.text);
	    if (s != NULL)
		new_decl_spec(&yyval.decl_spec, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin, s->flags);
	}
#line 1648 "grammar.tab.c"
break;
case 63:
#line 680 "grammar.y"
	{
	    char *s;
	    if ((s = implied_typedef()) == 0)
	        (void)sprintf(s = buf, "%.*s %.*s", TEXT_LEN, yystack.l_mark[-2].text.text, TEXT_LEN, yystack.l_mark[-1].text.text);
	    new_decl_spec(&yyval.decl_spec, s, yystack.l_mark[-2].text.begin, DS_NONE);
	}
#line 1658 "grammar.tab.c"
break;
case 64:
#line 687 "grammar.y"
	{
	    char *s;
	    if ((s = implied_typedef()) == 0)
		(void)sprintf(s = buf, "%.*s {}", TEXT_LEN, yystack.l_mark[-1].text.text);
	    new_decl_spec(&yyval.decl_spec, s, yystack.l_mark[-1].text.begin, DS_NONE);
	}
#line 1668 "grammar.tab.c"
break;
case 65:
#line 694 "grammar.y"
	{
	    (void)sprintf(buf, "%.*s %.*s", TEXT_LEN, yystack.l_mark[-1].text.text, TEXT_LEN, yystack.l_mark[0].text.text);
	    new_decl_spec(&yyval.decl_spec, buf, yystack.l_mark[-1].text.begin, DS_NONE);
	}
#line 1676 "grammar.tab.c"
break;
case 66:
#line 702 "grammar.y"
	{
	    imply_typedef(yyval.text.text);
	}
#line 1683 "grammar.tab.c"
break;
case 67:
#line 706 "grammar.y"
	{
	    imply_typedef(yyval.text.text);
	}
#line 1690 "grammar.tab.c"
break;
case 68:
#line 713 "grammar.y"
	{
	    new_decl_list(&yyval.decl_list, yystack.l_mark[0].declarator);
	}
#line 1697 "grammar.tab.c"
break;
case 69:
#line 717 "grammar.y"
	{
	    add_decl_list(&yyval.decl_list, &yystack.l_mark[-2].decl_list, yystack.l_mark[0].declarator);
	}
#line 1704 "grammar.tab.c"
break;
case 70:
#line 724 "grammar.y"
	{
	    if (yystack.l_mark[0].declarator->func_def != FUNC_NONE && func_params == NULL &&
		func_style == FUNC_TRADITIONAL && cur_file->convert) {
		gen_func_declarator(yystack.l_mark[0].declarator);
		fputs(cur_text(), cur_file->tmp_file);
	    }
	    cur_declarator = yyval.declarator;
	}
#line 1716 "grammar.tab.c"
break;
case 71:
#line 733 "grammar.y"
	{
	    if (yystack.l_mark[-1].declarator->func_def != FUNC_NONE && func_params == NULL &&
		func_style == FUNC_TRADITIONAL && cur_file->convert) {
		gen_func_declarator(yystack.l_mark[-1].declarator);
		fputs(" =", cur_file->tmp_file);
	    }
	}
#line 1727 "grammar.tab.c"
break;
case 73:
#line 745 "grammar.y"
	{
	    char *s;
	    if ((s = implied_typedef()) == 0)
		(void)sprintf(s = buf, "enum %.*s", TEXT_LEN, yystack.l_mark[-1].text.text);
	    new_decl_spec(&yyval.decl_spec, s, yystack.l_mark[-2].text.begin, DS_NONE);
	}
#line 1737 "grammar.tab.c"
break;
case 74:
#line 752 "grammar.y"
	{
	    char *s;
	    if ((s = implied_typedef()) == 0)
		(void)sprintf(s = buf, "%.*s {}", TEXT_LEN, yystack.l_mark[-1].text.text);
	    new_decl_spec(&yyval.decl_spec, s, yystack.l_mark[-1].text.begin, DS_NONE);
	}
#line 1747 "grammar.tab.c"
break;
case 75:
#line 759 "grammar.y"
	{
	    (void)sprintf(buf, "enum %.*s", TEXT_LEN, yystack.l_mark[0].text.text);
	    new_decl_spec(&yyval.decl_spec, buf, yystack.l_mark[-1].text.begin, DS_NONE);
	}
#line 1755 "grammar.tab.c"
break;
case 76:
#line 767 "grammar.y"
	{
	    imply_typedef("enum");
	    yyval.text = yystack.l_mark[0].text;
	}
#line 1763 "grammar.tab.c"
break;
case 79:
#line 780 "grammar.y"
	{
	    yyval.declarator = yystack.l_mark[0].declarator;
	    (void)sprintf(buf, "%.*s%.*s", TEXT_LEN, yystack.l_mark[-1].text.text, TEXT_LEN, yyval.declarator->text);
	    free(yyval.declarator->text);
	    yyval.declarator->text = xstrdup(buf);
	    yyval.declarator->begin = yystack.l_mark[-1].text.begin;
	    yyval.declarator->pointer = TRUE;
	}
#line 1775 "grammar.tab.c"
break;
case 81:
#line 793 "grammar.y"
	{
	    yyval.declarator = new_declarator(yystack.l_mark[0].text.text, yystack.l_mark[0].text.text, yystack.l_mark[0].text.begin);
	}
#line 1782 "grammar.tab.c"
break;
case 82:
#line 797 "grammar.y"
	{
	    yyval.declarator = yystack.l_mark[-1].declarator;
	    (void)sprintf(buf, "(%.*s)", TEXT_LEN, yyval.declarator->text);
	    free(yyval.declarator->text);
	    yyval.declarator->text = xstrdup(buf);
	    yyval.declarator->begin = yystack.l_mark[-2].text.begin;
	}
#line 1793 "grammar.tab.c"
break;
case 83:
#line 805 "grammar.y"
	{
	    yyval.declarator = yystack.l_mark[-1].declarator;
	    (void)sprintf(buf, "%.*s%.*s", TEXT_LEN, yyval.declarator->text, TEXT_LEN, yystack.l_mark[0].text.text);
	    free(yyval.declarator->text);
	    yyval.declarator->text = xstrdup(buf);
	}
#line 1803 "grammar.tab.c"
break;
case 84:
#line 812 "grammar.y"
	{
	    yyval.declarator = new_declarator("%s()", yystack.l_mark[-3].declarator->name, yystack.l_mark[-3].declarator->begin);
	    yyval.declarator->params = yystack.l_mark[-1].param_list;
	    yyval.declarator->func_stack = yystack.l_mark[-3].declarator;
	    yyval.declarator->head = (yystack.l_mark[-3].declarator->func_stack == NULL) ? yyval.declarator : yystack.l_mark[-3].declarator->head;
	    yyval.declarator->func_def = FUNC_ANSI;
	}
#line 1814 "grammar.tab.c"
break;
case 85:
#line 820 "grammar.y"
	{
	    yyval.declarator = new_declarator("%s()", yystack.l_mark[-3].declarator->name, yystack.l_mark[-3].declarator->begin);
	    yyval.declarator->params = yystack.l_mark[-1].param_list;
	    yyval.declarator->func_stack = yystack.l_mark[-3].declarator;
	    yyval.declarator->head = (yystack.l_mark[-3].declarator->func_stack == NULL) ? yyval.declarator : yystack.l_mark[-3].declarator->head;
	    yyval.declarator->func_def = FUNC_TRADITIONAL;
	}
#line 1825 "grammar.tab.c"
break;
case 86:
#line 831 "grammar.y"
	{
	    (void)sprintf(yyval.text.text, "*%.*s", TEXT_LEN, yystack.l_mark[0].text.text);
	    yyval.text.begin = yystack.l_mark[-1].text.begin;
	}
#line 1833 "grammar.tab.c"
break;
case 87:
#line 836 "grammar.y"
	{
	    (void)sprintf(yyval.text.text, "*%.*s%.*s", TEXT_LEN, yystack.l_mark[-1].text.text, TEXT_LEN, yystack.l_mark[0].text.text);
	    yyval.text.begin = yystack.l_mark[-2].text.begin;
	}
#line 1841 "grammar.tab.c"
break;
case 88:
#line 844 "grammar.y"
	{
	    strcpy(yyval.text.text, "");
	    yyval.text.begin = 0L;
	}
#line 1849 "grammar.tab.c"
break;
case 90:
#line 853 "grammar.y"
	{
	    (void)sprintf(yyval.text.text, "%s ", yystack.l_mark[0].decl_spec.text);
	    yyval.text.begin = yystack.l_mark[0].decl_spec.begin;
	    free(yystack.l_mark[0].decl_spec.text);
	}
#line 1858 "grammar.tab.c"
break;
case 91:
#line 859 "grammar.y"
	{
	    (void)sprintf(yyval.text.text, "%.*s%.*s ", TEXT_LEN, yystack.l_mark[-1].text.text, TEXT_LEN, yystack.l_mark[0].decl_spec.text);
	    yyval.text.begin = yystack.l_mark[-1].text.begin;
	    free(yystack.l_mark[0].decl_spec.text);
	}
#line 1867 "grammar.tab.c"
break;
case 93:
#line 869 "grammar.y"
	{
	    add_ident_list(&yyval.param_list, &yystack.l_mark[-2].param_list, "...");
	}
#line 1874 "grammar.tab.c"
break;
case 94:
#line 876 "grammar.y"
	{
	    new_param_list(&yyval.param_list, yystack.l_mark[0].parameter);
	}
#line 1881 "grammar.tab.c"
break;
case 95:
#line 880 "grammar.y"
	{
	    add_param_list(&yyval.param_list, &yystack.l_mark[-2].param_list, yystack.l_mark[0].parameter);
	}
#line 1888 "grammar.tab.c"
break;
case 96:
#line 887 "grammar.y"
	{
	    check_untagged(&yystack.l_mark[-1].decl_spec);
	    yyval.parameter = new_parameter(&yystack.l_mark[-1].decl_spec, yystack.l_mark[0].declarator);
	}
#line 1896 "grammar.tab.c"
break;
case 97:
#line 892 "grammar.y"
	{
	    check_untagged(&yystack.l_mark[-1].decl_spec);
	    yyval.parameter = new_parameter(&yystack.l_mark[-1].decl_spec, yystack.l_mark[0].declarator);
	}
#line 1904 "grammar.tab.c"
break;
case 98:
#line 897 "grammar.y"
	{
	    check_untagged(&yystack.l_mark[0].decl_spec);
	    yyval.parameter = new_parameter(&yystack.l_mark[0].decl_spec, (Declarator *)0);
	}
#line 1912 "grammar.tab.c"
break;
case 99:
#line 905 "grammar.y"
	{
	    new_ident_list(&yyval.param_list);
	}
#line 1919 "grammar.tab.c"
break;
case 101:
#line 913 "grammar.y"
	{
	    new_ident_list(&yyval.param_list);
	    add_ident_list(&yyval.param_list, &yyval.param_list, yystack.l_mark[0].text.text);
	}
#line 1927 "grammar.tab.c"
break;
case 102:
#line 918 "grammar.y"
	{
	    add_ident_list(&yyval.param_list, &yystack.l_mark[-2].param_list, yystack.l_mark[0].text.text);
	}
#line 1934 "grammar.tab.c"
break;
case 103:
#line 925 "grammar.y"
	{
	    yyval.text = yystack.l_mark[0].text;
	}
#line 1941 "grammar.tab.c"
break;
case 104:
#line 929 "grammar.y"
	{
#if OPT_LINTLIBRARY
	    if (lintLibrary()) { /* Lint doesn't grok C++ ref variables */
		yyval.text = yystack.l_mark[0].text;
	    } else
#endif
		(void)sprintf(yyval.text.text, "&%.*s", TEXT_LEN, yystack.l_mark[0].text.text);
	    yyval.text.begin = yystack.l_mark[-1].text.begin;
	}
#line 1954 "grammar.tab.c"
break;
case 105:
#line 942 "grammar.y"
	{
	    yyval.declarator = new_declarator(yystack.l_mark[0].text.text, "", yystack.l_mark[0].text.begin);
	}
#line 1961 "grammar.tab.c"
break;
case 106:
#line 946 "grammar.y"
	{
	    yyval.declarator = yystack.l_mark[0].declarator;
	    (void)sprintf(buf, "%.*s%.*s", TEXT_LEN, yystack.l_mark[-1].text.text, TEXT_LEN, yyval.declarator->text);
	    free(yyval.declarator->text);
	    yyval.declarator->text = xstrdup(buf);
	    yyval.declarator->begin = yystack.l_mark[-1].text.begin;
	}
#line 1972 "grammar.tab.c"
break;
case 108:
#line 958 "grammar.y"
	{
	    yyval.declarator = yystack.l_mark[-1].declarator;
	    (void)sprintf(buf, "(%.*s)", TEXT_LEN, yyval.declarator->text);
	    free(yyval.declarator->text);
	    yyval.declarator->text = xstrdup(buf);
	    yyval.declarator->begin = yystack.l_mark[-2].text.begin;
	}
#line 1983 "grammar.tab.c"
break;
case 109:
#line 966 "grammar.y"
	{
	    yyval.declarator = yystack.l_mark[-1].declarator;
	    (void)sprintf(buf, "%.*s%.*s", TEXT_LEN, yyval.declarator->text, TEXT_LEN, yystack.l_mark[0].text.text);
	    free(yyval.declarator->text);
	    yyval.declarator->text = xstrdup(buf);
	}
#line 1993 "grammar.tab.c"
break;
case 110:
#line 973 "grammar.y"
	{
	    yyval.declarator = new_declarator(yystack.l_mark[0].text.text, "", yystack.l_mark[0].text.begin);
	}
#line 2000 "grammar.tab.c"
break;
case 111:
#line 977 "grammar.y"
	{
	    yyval.declarator = new_declarator("%s()", "", yystack.l_mark[-3].declarator->begin);
	    yyval.declarator->params = yystack.l_mark[-1].param_list;
	    yyval.declarator->func_stack = yystack.l_mark[-3].declarator;
	    yyval.declarator->head = (yystack.l_mark[-3].declarator->func_stack == NULL) ? yyval.declarator : yystack.l_mark[-3].declarator->head;
	    yyval.declarator->func_def = FUNC_ANSI;
	}
#line 2011 "grammar.tab.c"
break;
case 112:
#line 985 "grammar.y"
	{
	    yyval.declarator = new_declarator("%s()", "", yystack.l_mark[-2].declarator->begin);
	    yyval.declarator->func_stack = yystack.l_mark[-2].declarator;
	    yyval.declarator->head = (yystack.l_mark[-2].declarator->func_stack == NULL) ? yyval.declarator : yystack.l_mark[-2].declarator->head;
	    yyval.declarator->func_def = FUNC_ANSI;
	}
#line 2021 "grammar.tab.c"
break;
case 113:
#line 992 "grammar.y"
	{
	    Declarator *d;

	    d = new_declarator("", "", yystack.l_mark[-2].text.begin);
	    yyval.declarator = new_declarator("%s()", "", yystack.l_mark[-2].text.begin);
	    yyval.declarator->params = yystack.l_mark[-1].param_list;
	    yyval.declarator->func_stack = d;
	    yyval.declarator->head = yyval.declarator;
	    yyval.declarator->func_def = FUNC_ANSI;
	}
#line 2035 "grammar.tab.c"
break;
case 114:
#line 1003 "grammar.y"
	{
	    Declarator *d;

	    d = new_declarator("", "", yystack.l_mark[-1].text.begin);
	    yyval.declarator = new_declarator("%s()", "", yystack.l_mark[-1].text.begin);
	    yyval.declarator->func_stack = d;
	    yyval.declarator->head = yyval.declarator;
	    yyval.declarator->func_def = FUNC_ANSI;
	}
#line 2048 "grammar.tab.c"
break;
#line 2050 "grammar.tab.c"
    }
    yystack.s_mark -= yym;
    yystate = *yystack.s_mark;
    yystack.l_mark -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yystack.s_mark = YYFINAL;
        *++yystack.l_mark = yyval;
        if (yychar < 0)
        {
            yychar = YYLEX;
            if (yychar < 0) yychar = YYEOF;
#if YYDEBUG
            if (yydebug)
            {
                if ((yys = yyname[YYTRANSLATE(yychar)]) == NULL) yys = yyname[YYUNDFTOKEN];
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == YYEOF) goto yyaccept;
        goto yyloop;
    }
    if (((yyn = yygindex[yym]) != 0) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == (YYINT) yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yystack.s_mark, yystate);
#endif
    if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
    *++yystack.s_mark = (YYINT) yystate;
    *++yystack.l_mark = yyval;
    goto yyloop;

yyoverflow:
    YYERROR_CALL("yacc stack overflow");

yyabort:
    yyfreestack(&yystack);
    return (1);

yyaccept:
    yyfreestack(&yystack);
    return (0);
}
