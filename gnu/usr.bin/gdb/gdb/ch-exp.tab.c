#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING (yyerrflag!=0)
#define yyparse ch_parse
#define yylex ch_lex
#define yyerror ch_error
#define yychar ch_char
#define yyval ch_val
#define yylval ch_lval
#define yydebug ch_debug
#define yynerrs ch_nerrs
#define yyerrflag ch_errflag
#define yyss ch_ss
#define yyssp ch_ssp
#define yyvs ch_vs
#define yyvsp ch_vsp
#define yylhs ch_lhs
#define yylen ch_len
#define yydefred ch_defred
#define yydgoto ch_dgoto
#define yysindex ch_sindex
#define yyrindex ch_rindex
#define yygindex ch_gindex
#define yytable ch_table
#define yycheck ch_check
#define yyname ch_name
#define yyrule ch_rule
#define YYPREFIX "ch_"
#line 55 "./ch-exp.y"

#include "defs.h"
#include <ctype.h>
#include "expression.h"
#include "language.h"
#include "value.h"
#include "parser-defs.h"
#include "ch-lang.h"

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  Note that these are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list. */

#define	yymaxdepth chill_maxdepth
#define	yyparse	chill_parse
#define	yylex	chill_lex
#define	yyerror	chill_error
#define	yylval	chill_lval
#define	yychar	chill_char
#define	yydebug	chill_debug
#define	yypact	chill_pact
#define	yyr1	chill_r1
#define	yyr2	chill_r2
#define	yydef	chill_def
#define	yychk	chill_chk
#define	yypgo	chill_pgo
#define	yyact	chill_act
#define	yyexca	chill_exca
#define	yyerrflag chill_errflag
#define	yynerrs	chill_nerrs
#define	yyps	chill_ps
#define	yypv	chill_pv
#define	yys	chill_s
#define	yy_yys	chill_yys
#define	yystate	chill_state
#define	yytmp	chill_tmp
#define	yyv	chill_v
#define	yy_yyv	chill_yyv
#define	yyval	chill_val
#define	yylloc	chill_lloc
#define	yyreds	chill_reds		/* With YYDEBUG defined */
#define	yytoks	chill_toks		/* With YYDEBUG defined */

#ifndef YYDEBUG
#define	YYDEBUG	0		/* Default to no yydebug support */
#endif

int
yyparse PARAMS ((void));

static int
yylex PARAMS ((void));

void
yyerror PARAMS ((char *));

#line 120 "./ch-exp.y"
typedef union
  {
    LONGEST lval;
    unsigned LONGEST ulval;
    struct {
      LONGEST val;
      struct type *type;
    } typed_val;
    double dval;
    struct symbol *sym;
    struct type *tval;
    struct stoken sval;
    struct ttype tsym;
    struct symtoken ssym;
    int voidval;
    struct block *bval;
    enum exp_opcode opcode;
    struct internalvar *ivar;

    struct type **tvec;
    int *ivec;
  } YYSTYPE;
#line 119 "y.tab.c"
#define FIXME_01 257
#define FIXME_02 258
#define FIXME_03 259
#define FIXME_04 260
#define FIXME_05 261
#define FIXME_06 262
#define FIXME_07 263
#define FIXME_08 264
#define FIXME_09 265
#define FIXME_10 266
#define FIXME_11 267
#define FIXME_12 268
#define FIXME_13 269
#define FIXME_14 270
#define FIXME_15 271
#define FIXME_16 272
#define FIXME_17 273
#define FIXME_18 274
#define FIXME_19 275
#define FIXME_20 276
#define FIXME_21 277
#define FIXME_22 278
#define FIXME_24 279
#define FIXME_25 280
#define FIXME_26 281
#define FIXME_27 282
#define FIXME_28 283
#define FIXME_29 284
#define FIXME_30 285
#define INTEGER_LITERAL 286
#define BOOLEAN_LITERAL 287
#define CHARACTER_LITERAL 288
#define FLOAT_LITERAL 289
#define GENERAL_PROCEDURE_NAME 290
#define LOCATION_NAME 291
#define SET_LITERAL 292
#define EMPTINESS_LITERAL 293
#define CHARACTER_STRING_LITERAL 294
#define BIT_STRING_LITERAL 295
#define TYPENAME 296
#define FIELD_NAME 297
#define CASE 298
#define OF 299
#define ESAC 300
#define LOGIOR 301
#define ORIF 302
#define LOGXOR 303
#define LOGAND 304
#define ANDIF 305
#define NOTEQUAL 306
#define GTR 307
#define LEQ 308
#define IN 309
#define SLASH_SLASH 310
#define MOD 311
#define REM 312
#define NOT 313
#define POINTER 314
#define RECEIVE 315
#define UP 316
#define IF 317
#define THEN 318
#define ELSE 319
#define FI 320
#define ELSIF 321
#define ILLEGAL_TOKEN 322
#define NUM 323
#define PRED 324
#define SUCC 325
#define ABS 326
#define CARD 327
#define MAX_TOKEN 328
#define MIN_TOKEN 329
#define SIZE 330
#define UPPER 331
#define LOWER 332
#define LENGTH 333
#define GDB_REGNAME 334
#define GDB_LAST 335
#define GDB_VARIABLE 336
#define GDB_ASSIGNMENT 337
#define YYERRCODE 256
short ch_lhs[] = {                                        -1,
    0,    0,   20,   20,   21,    1,    1,    2,    2,    2,
    2,    2,   45,   45,    3,    3,    3,    3,    3,    3,
    3,    3,    3,    3,    3,    3,    3,    3,    3,    4,
    5,    5,    5,    5,    5,    6,    6,    6,    6,    6,
    6,    6,    6,    7,    8,    9,    9,   62,   10,   11,
   11,   12,   13,   14,   15,   17,   18,   19,   22,   22,
   22,   23,   23,   24,   25,   25,   26,   27,   28,   28,
   28,   28,   29,   29,   29,   30,   30,   30,   30,   30,
   30,   30,   30,   31,   31,   31,   31,   32,   32,   32,
   32,   32,   33,   33,   33,   33,   34,   34,   34,   60,
   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,
   16,   16,   49,   49,   49,   49,   61,   50,   50,   51,
   44,   52,   53,   54,   35,   36,   37,   38,   39,   40,
   41,   42,   43,   46,   47,   48,   55,   56,   57,   58,
   59,
};
short ch_len[] = {                                         2,
    1,    1,    1,    1,    1,    1,    2,    1,    1,    1,
    1,    1,    1,    3,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    4,    6,    6,    0,    5,    6,
    6,    2,    2,    1,    1,    1,    1,    3,    1,    1,
    1,    5,    9,    2,    2,    4,    1,    4,    1,    3,
    3,    3,    1,    3,    3,    1,    3,    3,    3,    3,
    3,    3,    3,    1,    3,    3,    3,    1,    3,    3,
    3,    3,    1,    2,    2,    2,    2,    2,    1,    3,
    4,    4,    4,    4,    4,    4,    4,    4,    4,    4,
    4,    4,    1,    4,    4,    4,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,
};
short ch_defred[] = {                                      0,
    5,   12,   44,   54,   56,   57,  125,  126,  127,  128,
  129,   36,   37,   38,   39,   35,    8,   40,   41,   42,
   43,  117,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   10,    9,   11,    0,    0,    6,    0,   15,   16,   17,
   18,   19,   20,   21,   22,   23,   24,   25,   26,   55,
   27,   28,    0,    1,    4,    3,   61,    0,    0,    0,
    0,    0,   88,   93,   31,   32,   33,   34,    0,    0,
   60,    0,  138,    0,   30,   29,   94,    0,   95,    0,
    0,  141,   98,    0,  137,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   52,    7,
   96,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   53,    0,   58,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  122,  123,  124,    0,    0,    0,
    0,    0,    0,  118,    0,    0,    0,  120,    0,  100,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   89,   90,   91,   92,  130,
  131,    0,    0,  134,  136,    0,    0,    0,  140,    0,
    0,  139,   64,    0,    0,    0,  101,  102,  103,  104,
  105,  106,  107,  108,  109,    0,    0,    0,  110,  111,
  112,   45,    0,    0,    0,    0,   13,    0,    0,    0,
   65,    0,   62,    0,    0,    0,  133,    0,  132,    0,
  135,    0,    0,   49,    0,    0,   67,    0,    0,  114,
  115,  116,   47,   46,   50,   51,   14,    0,   68,   66,
    0,   63,
};
short ch_dgoto[] = {                                      44,
   85,   46,   47,   48,   49,   50,   51,   52,   53,   54,
   55,   56,   57,   58,   59,   60,   61,   62,   63,   64,
   65,   66,   67,  137,  196,  238,  190,   68,   69,   70,
   71,   72,   73,   74,   75,   76,   77,   78,   79,  182,
  183,  230,  228,   80,  218,  186,  232,  187,  149,  155,
  159,  150,  151,  152,   96,   84,  193,  191,   93,   81,
   88,  188,
};
short ch_sindex[] = {                                    457,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0, -262,  879,  879,  893, -215,  622, -258,   50,
   51,   52,   53,   56,   57,   58,   67,   72,   73,   75,
    0,    0,    0,    0, -221,    0, -293,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0, -261,    0,    0,    0,    0, -248, -275,  -44,
  -30,  -33,    0,    0,    0,    0,    0,    0,   88,   91,
    0,   92,    0, -165,    0,    0,    0,   92,    0,    0,
 -293,    0,    0,   94,    0, -181,  622,  622,  622,  622,
  622,  622,  622,  636,  622,  622,  622,  457,    0,    0,
    0,  797,  797,  797,  797,  797,  797,  797,  797,  797,
  797,  797,  797,  797,  797,  797,  797,  797,  797,  797,
 -233, -270,    0, -137,    0, -132, -272,  112,  118,  119,
  120,  121,  122,  128,    0,    0,    0,  129,  130,  132,
  141,  143,   92,    0,  144,   92,  146,    0,  149,    0,
 -275, -275, -275,  -44,  -44,  -30,  -30,  -30,  -30,  -30,
  -30,  -30,  -33,  -33,  -33,    0,    0,    0,    0,    0,
    0,  -39,  115,    0,    0,  126, -120,  622,    0,  106,
  142,    0,    0, -132, -258, -110,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  622,  622,  622,    0,    0,
    0,    0,  -64,  -61,  -63,  -64,    0,  -34, -103,  622,
    0, -181,    0,  174,  181,  -22,    0,  182,    0,  184,
    0,  192,  193,    0,  622,  622,    0,  177, -272,    0,
    0,    0,    0,    0,    0,    0,    0,  150,    0,    0,
  -55,    0,
};
short ch_rindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    1,    0,   82,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   24,    0,    0,    0,    0,  506,  255,  191,
  145,   95,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  237,    0,    0,    0,    0,    0,    0,    0,   59,
  204,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  714,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  -40,    0,    0,
    0,    0,  205,    0,    0,  206,    0,    0,    0,    0,
  567,  573,  577,  531,  554,  168,  180,  208,  231,  500,
  522,  545,  105,  133,  158,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,
};
short ch_gindex[] = {                                      0,
 1230,    0,  -23,    0,    0,  185,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   26,  148,
    0, 1162,    0,   31,   15,   22,    0,    0,  -56, -104,
  -45,  -37,  -92,   55,    0,    0,    0,    0,    0,    0,
    0,    0,   43,    0,   63,    0,    0,    0,    0,  154,
    0,    0,    0,    0,   71,    0,   87,    0,    0,    0,
    5,    0,
};
#define YYTABLESIZE 1466
short ch_table[] = {                                      30,
   30,  212,   91,  109,   82,  184,  234,  185,  127,  235,
  164,  165,  124,  128,  125,  121,  117,  119,  242,   83,
  110,  235,   95,   29,   12,   13,   14,   15,  115,  116,
   18,   19,   20,   21,  176,  177,  178,  179,  180,  181,
   30,   30,   30,   30,   30,   30,  194,   30,  195,   86,
   86,   86,  112,  113,  114,  161,  162,  163,   97,   30,
   30,   30,   30,   29,   29,   29,   29,   29,   29,   92,
   29,  166,  167,  168,  169,  170,  171,  172,   87,   89,
   91,   99,   29,   29,   29,   29,  173,  174,  175,   97,
   98,   99,  100,   30,   84,  101,  102,  103,   30,   97,
   97,   97,   97,   97,   85,   97,  104,  133,  153,  156,
  156,  105,  106,  133,  107,  108,   29,   97,   97,   97,
   97,  121,   99,   99,   99,   99,   99,  131,   99,   86,
  132,   28,   86,  134,  135,   84,  136,   84,   84,   84,
   99,   99,   99,   99,   76,   85,  189,   85,   85,   85,
  192,   97,  197,   84,   84,   84,   84,   87,  198,  199,
  200,  201,  202,   85,   85,   85,   85,   77,  203,  204,
  205,  206,  214,   86,   99,   86,   86,   86,  133,   78,
  207,  133,  208,  215,  209,   76,  210,   84,   76,  211,
   73,   86,   86,   86,   86,  216,  219,   85,   87,  220,
   87,   87,   87,   76,   76,   76,   76,   79,   77,  223,
  227,   77,  229,  231,  240,  236,   87,   87,   87,   87,
   78,  241,  243,   78,  244,   86,   77,   77,   77,   77,
   80,   73,  245,  246,   73,  249,    2,   76,   78,   78,
   78,   78,  251,  121,  252,  113,  119,  111,   79,   73,
   87,   79,  239,  250,   69,  160,   30,  248,  233,  157,
   77,  118,  120,  122,  123,  222,   79,   79,   79,   79,
  226,   80,   78,   30,   80,    0,  213,  129,  130,  126,
  221,    0,    0,   73,    0,    0,    0,    0,    0,   80,
   80,   80,   80,    0,    0,   69,    0,   30,   69,    0,
   79,   30,   30,   30,   30,   30,   30,   30,   30,   30,
   30,   30,   30,   69,   30,    0,    0,    0,    0,    0,
   29,    0,    0,   80,   29,   29,   29,   29,   29,   29,
   29,   29,   29,   29,   29,   29,    0,   29,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   69,    0,    0,
    0,    0,    0,    0,    0,   30,    0,    0,    0,   97,
   97,   97,   97,   97,   97,   97,   97,   97,   97,   97,
   97,    0,   30,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   99,   99,   99,   99,   99,   99,   99,   99,
   99,   99,   99,   99,    0,   84,   84,   84,   84,   84,
   84,   84,   84,   84,   84,   85,   85,   85,   85,   85,
   85,   85,   85,   85,   85,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   86,   86,   86,   86,   86,   86,   86,
   86,   86,   86,    0,    0,   76,   76,   76,   76,   76,
   76,   76,   76,   76,    0,    0,    0,    0,   87,   87,
   87,   87,   87,   87,   87,   87,   87,   87,   77,   77,
   77,   77,   77,   77,   77,   77,   77,    0,    0,    0,
   78,   78,   78,   78,   78,   78,   78,   78,   78,    0,
    0,   73,   73,   73,   73,   73,   28,    0,    0,   81,
    0,   24,    0,    0,    0,   59,    0,    0,   79,   79,
   79,   79,   79,   79,   79,   79,   79,    0,    0,    0,
    0,   82,    0,    0,    0,    0,    0,    0,    0,    0,
   74,   80,   80,   80,   80,   80,   80,   80,   80,   80,
   81,    0,    0,   81,   83,    0,   59,    0,    0,   59,
    0,    0,    0,   75,    0,   69,   69,   69,   81,   81,
   81,   81,   82,    0,   59,   82,   70,    0,    0,    0,
    0,   74,   71,    0,   74,    0,   72,    0,    0,    0,
   82,   82,   82,   82,    0,   83,    0,    0,   83,   74,
    0,    0,   81,    0,   75,    0,    0,   75,   59,    0,
    0,    0,    0,   83,   83,   83,   83,   70,    0,    0,
   70,    0,   75,   71,   82,    0,   71,   72,    0,    0,
   72,    0,    0,   74,    0,   70,    0,    0,    0,    0,
    0,   71,    0,    0,    0,   72,    0,   83,    0,    0,
    0,    0,    0,    0,    0,    0,   75,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   70,
    0,   28,    0,    0,    0,   71,   24,    0,    0,   72,
    0,    0,    0,    0,    0,   28,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    1,    0,    2,    3,    4,    5,    6,
    0,    0,    0,    7,    8,    9,   10,   11,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   12,   13,   14,   15,   16,   17,   18,   19,
   20,   21,   22,   48,   23,    0,    0,    0,   48,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   25,
   26,   27,    0,   29,    0,    0,    0,    0,    0,   30,
   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,
   41,   42,   43,    0,    0,    0,    0,    0,    0,    0,
   81,   81,   81,   81,   81,   81,   81,   81,   81,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   82,   82,   82,   82,   82,   82,   82,   82,
   82,   74,   74,   74,   74,   74,   28,    0,    0,    0,
    0,   24,    0,    0,    0,   83,   83,   83,   83,   83,
   83,   83,   83,   83,   75,   75,   75,   75,   75,    0,
    0,    0,    0,    0,    0,    0,    0,   70,   70,   70,
    0,    0,    0,   71,   71,   71,    0,   72,   72,   72,
    2,    3,    4,    5,    6,    0,    0,    0,    7,    8,
    9,   10,   11,    0,    2,    3,    4,    5,    6,  145,
  146,  147,    7,    8,    9,   10,   11,   12,   13,   14,
   15,   16,   17,   18,   19,   20,   21,   22,   28,   23,
    0,   12,   13,   14,   15,   16,   17,   18,   19,   20,
   21,   22,   28,    0,   25,   26,   27,    0,   29,    0,
    0,    0,    0,    0,   30,   31,   32,   33,   34,   35,
   36,   37,   38,   39,   40,   41,   42,   43,   30,   31,
   32,   33,   34,   35,   36,   37,   38,   39,   40,   41,
   42,   43,   48,   48,   48,   48,   48,    0,    0,    0,
   48,   48,   48,   48,   48,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   48,
   48,   48,   48,   48,   48,   48,   48,   48,   48,   48,
    0,   48,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   48,   48,   48,    0,
   48,    0,    0,    0,    0,    0,   48,   48,   48,   48,
   48,   48,   48,   48,   48,   48,   48,   48,   48,   48,
    0,    0,    0,    0,    0,    2,    3,    4,    5,    6,
    0,    0,    0,    7,    8,    9,   10,   11,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   12,   13,   14,   15,   16,   17,   18,   19,
   20,   21,   22,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   25,
   26,   27,    0,    0,    0,    0,    0,    0,    0,   30,
   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,
   41,   42,   43,    0,    0,    0,    0,    2,    3,    4,
    5,    6,    0,    0,    0,    7,    8,    9,   10,   11,
    0,    2,    3,    4,    5,    6,    0,    0,    0,    7,
    8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
   18,   19,   20,   21,   22,    0,    0,    0,   12,   13,
   14,   15,   16,   17,   18,   19,   20,   21,   22,   94,
    0,    0,   26,   27,    0,    0,    0,    0,    0,    0,
    0,   30,   31,   32,   33,   34,   35,   36,   37,   38,
   39,   40,   41,   42,   43,   30,   31,   32,   33,   34,
   35,   36,   37,   38,   39,   40,   41,   42,   43,   45,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   90,    0,   45,  138,  139,
  140,  141,  142,  143,  144,    0,  154,  154,  158,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   45,   45,   45,   45,
   45,   45,   45,  148,   45,   45,   45,   45,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  217,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  224,  225,  217,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  237,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  247,  237,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   45,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   45,   45,   45,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   45,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   45,   45,
};
short ch_check[] = {                                      40,
    0,   41,   26,  297,    0,  276,   41,  278,   42,   44,
  115,  116,   43,   47,   45,   60,   61,   62,   41,  282,
  314,   44,  281,    0,  286,  287,  288,  289,  304,  305,
  292,  293,  294,  295,  127,  128,  129,  130,  272,  273,
   40,   41,   42,   43,   44,   45,  319,   47,  321,   24,
   25,   26,  301,  302,  303,  112,  113,  114,    0,   59,
   60,   61,   62,   40,   41,   42,   43,   44,   45,  285,
   47,  117,  118,  119,  120,  121,  122,  123,   24,   25,
  104,    0,   59,   60,   61,   62,  124,  125,  126,   40,
   40,   40,   40,   93,    0,   40,   40,   40,   40,   41,
   42,   43,   44,   45,    0,   47,   40,   82,  104,  105,
  106,   40,   40,   88,   40,  337,   93,   59,   60,   61,
   62,   40,   41,   42,   43,   44,   45,   40,   47,  104,
   40,   40,    0,  299,   41,   41,  318,   43,   44,   45,
   59,   60,   61,   62,    0,   41,  284,   43,   44,   45,
  283,   93,   41,   59,   60,   61,   62,    0,   41,   41,
   41,   41,   41,   59,   60,   61,   62,    0,   41,   41,
   41,   40,   58,   41,   93,   43,   44,   45,  153,    0,
   40,  156,   40,   58,   41,   41,   41,   93,   44,   41,
    0,   59,   60,   61,   62,  316,   91,   93,   41,   58,
   43,   44,   45,   59,   60,   61,   62,    0,   41,  320,
  275,   44,  274,  277,   41,  319,   59,   60,   61,   62,
   41,   41,   41,   44,   41,   93,   59,   60,   61,   62,
    0,   41,   41,   41,   44,   59,    0,   93,   59,   60,
   61,   62,   93,   40,  300,   41,   41,   63,   41,   59,
   93,   44,  222,  239,    0,  108,  297,  236,  216,  106,
   93,  306,  307,  308,  309,  195,   59,   60,   61,   62,
  208,   41,   93,  314,   44,   -1,  316,  311,  312,  310,
  194,   -1,   -1,   93,   -1,   -1,   -1,   -1,   -1,   59,
   60,   61,   62,   -1,   -1,   41,   -1,  297,   44,   -1,
   93,  301,  302,  303,  304,  305,  306,  307,  308,  309,
  310,  311,  312,   59,  314,   -1,   -1,   -1,   -1,   -1,
  297,   -1,   -1,   93,  301,  302,  303,  304,  305,  306,
  307,  308,  309,  310,  311,  312,   -1,  314,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   93,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  297,   -1,   -1,   -1,  301,
  302,  303,  304,  305,  306,  307,  308,  309,  310,  311,
  312,   -1,  314,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  301,  302,  303,  304,  305,  306,  307,  308,
  309,  310,  311,  312,   -1,  301,  302,  303,  304,  305,
  306,  307,  308,  309,  310,  301,  302,  303,  304,  305,
  306,  307,  308,  309,  310,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,  301,  302,  303,  304,  305,  306,  307,
  308,  309,  310,   -1,   -1,  301,  302,  303,  304,  305,
  306,  307,  308,  309,   -1,   -1,   -1,   -1,  301,  302,
  303,  304,  305,  306,  307,  308,  309,  310,  301,  302,
  303,  304,  305,  306,  307,  308,  309,   -1,   -1,   -1,
  301,  302,  303,  304,  305,  306,  307,  308,  309,   -1,
   -1,  301,  302,  303,  304,  305,   40,   -1,   -1,    0,
   -1,   45,   -1,   -1,   -1,    0,   -1,   -1,  301,  302,
  303,  304,  305,  306,  307,  308,  309,   -1,   -1,   -1,
   -1,    0,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    0,  301,  302,  303,  304,  305,  306,  307,  308,  309,
   41,   -1,   -1,   44,    0,   -1,   41,   -1,   -1,   44,
   -1,   -1,   -1,    0,   -1,  301,  302,  303,   59,   60,
   61,   62,   41,   -1,   59,   44,    0,   -1,   -1,   -1,
   -1,   41,    0,   -1,   44,   -1,    0,   -1,   -1,   -1,
   59,   60,   61,   62,   -1,   41,   -1,   -1,   44,   59,
   -1,   -1,   93,   -1,   41,   -1,   -1,   44,   93,   -1,
   -1,   -1,   -1,   59,   60,   61,   62,   41,   -1,   -1,
   44,   -1,   59,   41,   93,   -1,   44,   41,   -1,   -1,
   44,   -1,   -1,   93,   -1,   59,   -1,   -1,   -1,   -1,
   -1,   59,   -1,   -1,   -1,   59,   -1,   93,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   93,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   93,
   -1,   40,   -1,   -1,   -1,   93,   45,   -1,   -1,   93,
   -1,   -1,   -1,   -1,   -1,   40,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,  257,   -1,  259,  260,  261,  262,  263,
   -1,   -1,   -1,  267,  268,  269,  270,  271,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  286,  287,  288,  289,  290,  291,  292,  293,
  294,  295,  296,   40,  298,   -1,   -1,   -1,   45,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  313,
  314,  315,   -1,  317,   -1,   -1,   -1,   -1,   -1,  323,
  324,  325,  326,  327,  328,  329,  330,  331,  332,  333,
  334,  335,  336,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  301,  302,  303,  304,  305,  306,  307,  308,  309,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  301,  302,  303,  304,  305,  306,  307,  308,
  309,  301,  302,  303,  304,  305,   40,   -1,   -1,   -1,
   -1,   45,   -1,   -1,   -1,  301,  302,  303,  304,  305,
  306,  307,  308,  309,  301,  302,  303,  304,  305,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  301,  302,  303,
   -1,   -1,   -1,  301,  302,  303,   -1,  301,  302,  303,
  259,  260,  261,  262,  263,   -1,   -1,   -1,  267,  268,
  269,  270,  271,   -1,  259,  260,  261,  262,  263,  264,
  265,  266,  267,  268,  269,  270,  271,  286,  287,  288,
  289,  290,  291,  292,  293,  294,  295,  296,   40,  298,
   -1,  286,  287,  288,  289,  290,  291,  292,  293,  294,
  295,  296,   40,   -1,  313,  314,  315,   -1,  317,   -1,
   -1,   -1,   -1,   -1,  323,  324,  325,  326,  327,  328,
  329,  330,  331,  332,  333,  334,  335,  336,  323,  324,
  325,  326,  327,  328,  329,  330,  331,  332,  333,  334,
  335,  336,  259,  260,  261,  262,  263,   -1,   -1,   -1,
  267,  268,  269,  270,  271,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  286,
  287,  288,  289,  290,  291,  292,  293,  294,  295,  296,
   -1,  298,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  313,  314,  315,   -1,
  317,   -1,   -1,   -1,   -1,   -1,  323,  324,  325,  326,
  327,  328,  329,  330,  331,  332,  333,  334,  335,  336,
   -1,   -1,   -1,   -1,   -1,  259,  260,  261,  262,  263,
   -1,   -1,   -1,  267,  268,  269,  270,  271,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  286,  287,  288,  289,  290,  291,  292,  293,
  294,  295,  296,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  313,
  314,  315,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  323,
  324,  325,  326,  327,  328,  329,  330,  331,  332,  333,
  334,  335,  336,   -1,   -1,   -1,   -1,  259,  260,  261,
  262,  263,   -1,   -1,   -1,  267,  268,  269,  270,  271,
   -1,  259,  260,  261,  262,  263,   -1,   -1,   -1,  267,
  268,  269,  270,  271,  286,  287,  288,  289,  290,  291,
  292,  293,  294,  295,  296,   -1,   -1,   -1,  286,  287,
  288,  289,  290,  291,  292,  293,  294,  295,  296,   28,
   -1,   -1,  314,  315,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  323,  324,  325,  326,  327,  328,  329,  330,  331,
  332,  333,  334,  335,  336,  323,  324,  325,  326,  327,
  328,  329,  330,  331,  332,  333,  334,  335,  336,    0,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   26,   -1,   28,   97,   98,
   99,  100,  101,  102,  103,   -1,  105,  106,  107,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   97,   98,   99,  100,
  101,  102,  103,  104,  105,  106,  107,  108,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  188,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  206,  207,  208,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  220,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  235,  236,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  188,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  206,  207,  208,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  220,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  235,  236,
};
#define YYFINAL 44
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 337
#if YYDEBUG
char *ch_name[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,"'('","')'","'*'","'+'","','","'-'","'.'","'/'",0,0,0,0,0,0,0,0,0,0,
"':'","';'","'<'","'='","'>'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,"'['",0,"']'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,"FIXME_01","FIXME_02","FIXME_03","FIXME_04",
"FIXME_05","FIXME_06","FIXME_07","FIXME_08","FIXME_09","FIXME_10","FIXME_11",
"FIXME_12","FIXME_13","FIXME_14","FIXME_15","FIXME_16","FIXME_17","FIXME_18",
"FIXME_19","FIXME_20","FIXME_21","FIXME_22","FIXME_24","FIXME_25","FIXME_26",
"FIXME_27","FIXME_28","FIXME_29","FIXME_30","INTEGER_LITERAL","BOOLEAN_LITERAL",
"CHARACTER_LITERAL","FLOAT_LITERAL","GENERAL_PROCEDURE_NAME","LOCATION_NAME",
"SET_LITERAL","EMPTINESS_LITERAL","CHARACTER_STRING_LITERAL",
"BIT_STRING_LITERAL","TYPENAME","FIELD_NAME","CASE","OF","ESAC","LOGIOR","ORIF",
"LOGXOR","LOGAND","ANDIF","NOTEQUAL","GTR","LEQ","IN","SLASH_SLASH","MOD","REM",
"NOT","POINTER","RECEIVE","UP","IF","THEN","ELSE","FI","ELSIF","ILLEGAL_TOKEN",
"NUM","PRED","SUCC","ABS","CARD","MAX_TOKEN","MIN_TOKEN","SIZE","UPPER","LOWER",
"LENGTH","GDB_REGNAME","GDB_LAST","GDB_VARIABLE","GDB_ASSIGNMENT",
};
char *ch_rule[] = {
"$accept : start",
"start : value",
"start : mode_name",
"value : expression",
"value : undefined_value",
"undefined_value : FIXME_01",
"location : access_name",
"location : primitive_value POINTER",
"access_name : LOCATION_NAME",
"access_name : GDB_LAST",
"access_name : GDB_REGNAME",
"access_name : GDB_VARIABLE",
"access_name : FIXME_03",
"expression_list : expression",
"expression_list : expression_list ',' expression",
"primitive_value : location_contents",
"primitive_value : value_name",
"primitive_value : literal",
"primitive_value : tuple",
"primitive_value : value_string_element",
"primitive_value : value_string_slice",
"primitive_value : value_array_element",
"primitive_value : value_array_slice",
"primitive_value : value_structure_field",
"primitive_value : expression_conversion",
"primitive_value : value_procedure_call",
"primitive_value : value_built_in_routine_call",
"primitive_value : start_expression",
"primitive_value : zero_adic_operator",
"primitive_value : parenthesised_expression",
"location_contents : location",
"value_name : synonym_name",
"value_name : value_enumeration_name",
"value_name : value_do_with_name",
"value_name : value_receive_name",
"value_name : GENERAL_PROCEDURE_NAME",
"literal : INTEGER_LITERAL",
"literal : BOOLEAN_LITERAL",
"literal : CHARACTER_LITERAL",
"literal : FLOAT_LITERAL",
"literal : SET_LITERAL",
"literal : EMPTINESS_LITERAL",
"literal : CHARACTER_STRING_LITERAL",
"literal : BIT_STRING_LITERAL",
"tuple : FIXME_04",
"value_string_element : string_primitive_value '(' start_element ')'",
"value_string_slice : string_primitive_value '(' left_element ':' right_element ')'",
"value_string_slice : string_primitive_value '(' start_element UP slice_size ')'",
"$$1 :",
"value_array_element : array_primitive_value '(' $$1 expression_list ')'",
"value_array_slice : array_primitive_value '(' lower_element ':' upper_element ')'",
"value_array_slice : array_primitive_value '(' first_element UP slice_size ')'",
"value_structure_field : primitive_value FIELD_NAME",
"expression_conversion : mode_name parenthesised_expression",
"value_procedure_call : FIXME_05",
"value_built_in_routine_call : chill_value_built_in_routine_call",
"start_expression : FIXME_06",
"zero_adic_operator : FIXME_07",
"parenthesised_expression : '(' expression ')'",
"expression : operand_0",
"expression : single_assignment_action",
"expression : conditional_expression",
"conditional_expression : IF boolean_expression then_alternative else_alternative FI",
"conditional_expression : CASE case_selector_list OF value_case_alternative '[' ELSE sub_expression ']' ESAC",
"then_alternative : THEN subexpression",
"else_alternative : ELSE subexpression",
"else_alternative : ELSIF boolean_expression then_alternative else_alternative",
"sub_expression : expression",
"value_case_alternative : case_label_specification ':' sub_expression ';'",
"operand_0 : operand_1",
"operand_0 : operand_0 LOGIOR operand_1",
"operand_0 : operand_0 ORIF operand_1",
"operand_0 : operand_0 LOGXOR operand_1",
"operand_1 : operand_2",
"operand_1 : operand_1 LOGAND operand_2",
"operand_1 : operand_1 ANDIF operand_2",
"operand_2 : operand_3",
"operand_2 : operand_2 '=' operand_3",
"operand_2 : operand_2 NOTEQUAL operand_3",
"operand_2 : operand_2 '>' operand_3",
"operand_2 : operand_2 GTR operand_3",
"operand_2 : operand_2 '<' operand_3",
"operand_2 : operand_2 LEQ operand_3",
"operand_2 : operand_2 IN operand_3",
"operand_3 : operand_4",
"operand_3 : operand_3 '+' operand_4",
"operand_3 : operand_3 '-' operand_4",
"operand_3 : operand_3 SLASH_SLASH operand_4",
"operand_4 : operand_5",
"operand_4 : operand_4 '*' operand_5",
"operand_4 : operand_4 '/' operand_5",
"operand_4 : operand_4 MOD operand_5",
"operand_4 : operand_4 REM operand_5",
"operand_5 : operand_6",
"operand_5 : '-' operand_6",
"operand_5 : NOT operand_6",
"operand_5 : parenthesised_expression literal",
"operand_6 : POINTER location",
"operand_6 : RECEIVE buffer_location",
"operand_6 : primitive_value",
"single_assignment_action : location GDB_ASSIGNMENT value",
"chill_value_built_in_routine_call : NUM '(' expression ')'",
"chill_value_built_in_routine_call : PRED '(' expression ')'",
"chill_value_built_in_routine_call : SUCC '(' expression ')'",
"chill_value_built_in_routine_call : ABS '(' expression ')'",
"chill_value_built_in_routine_call : CARD '(' expression ')'",
"chill_value_built_in_routine_call : MAX_TOKEN '(' expression ')'",
"chill_value_built_in_routine_call : MIN_TOKEN '(' expression ')'",
"chill_value_built_in_routine_call : SIZE '(' location ')'",
"chill_value_built_in_routine_call : SIZE '(' mode_argument ')'",
"chill_value_built_in_routine_call : UPPER '(' upper_lower_argument ')'",
"chill_value_built_in_routine_call : LOWER '(' upper_lower_argument ')'",
"chill_value_built_in_routine_call : LENGTH '(' length_argument ')'",
"mode_argument : mode_name",
"mode_argument : array_mode_name '(' expression ')'",
"mode_argument : string_mode_name '(' expression ')'",
"mode_argument : variant_structure_mode_name '(' expression_list ')'",
"mode_name : TYPENAME",
"upper_lower_argument : expression",
"upper_lower_argument : mode_name",
"length_argument : expression",
"array_primitive_value : primitive_value",
"array_mode_name : FIXME_08",
"string_mode_name : FIXME_09",
"variant_structure_mode_name : FIXME_10",
"synonym_name : FIXME_11",
"value_enumeration_name : FIXME_12",
"value_do_with_name : FIXME_13",
"value_receive_name : FIXME_14",
"string_primitive_value : FIXME_15",
"start_element : FIXME_16",
"left_element : FIXME_17",
"right_element : FIXME_18",
"slice_size : FIXME_19",
"lower_element : FIXME_20",
"upper_element : FIXME_21",
"first_element : FIXME_22",
"boolean_expression : FIXME_26",
"case_selector_list : FIXME_27",
"subexpression : FIXME_28",
"case_label_specification : FIXME_29",
"buffer_location : FIXME_30",
};
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 500
#define YYMAXDEPTH 500
#endif
#endif
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short yyss[YYSTACKSIZE];
YYSTYPE yyvs[YYSTACKSIZE];
#define yystacksize YYSTACKSIZE
#line 994 "./ch-exp.y"

/* Implementation of a dynamically expandable buffer for processing input
   characters acquired through lexptr and building a value to return in
   yylval. */

static char *tempbuf;		/* Current buffer contents */
static int tempbufsize;		/* Size of allocated buffer */
static int tempbufindex;	/* Current index into buffer */

#define GROWBY_MIN_SIZE 64	/* Minimum amount to grow buffer by */

#define CHECKBUF(size) \
  do { \
    if (tempbufindex + (size) >= tempbufsize) \
      { \
	growbuf_by_size (size); \
      } \
  } while (0);

/* Grow the static temp buffer if necessary, including allocating the first one
   on demand. */

static void
growbuf_by_size (count)
     int count;
{
  int growby;

  growby = max (count, GROWBY_MIN_SIZE);
  tempbufsize += growby;
  if (tempbuf == NULL)
    {
      tempbuf = (char *) xmalloc (tempbufsize);
    }
  else
    {
      tempbuf = (char *) xrealloc (tempbuf, tempbufsize);
    }
}

/* Try to consume a simple name string token.  If successful, returns
   a pointer to a nullbyte terminated copy of the name that can be used
   in symbol table lookups.  If not successful, returns NULL. */

static char *
match_simple_name_string ()
{
  char *tokptr = lexptr;

  if (isalpha (*tokptr))
    {
      char *result;
      do {
	tokptr++;
      } while (isalnum (*tokptr) || (*tokptr == '_'));
      yylval.sval.ptr = lexptr;
      yylval.sval.length = tokptr - lexptr;
      lexptr = tokptr;
      result = copy_name (yylval.sval);
      for (tokptr = result; *tokptr; tokptr++)
	if (isupper (*tokptr))
	  *tokptr = tolower(*tokptr);
      return result;
    }
  return (NULL);
}

/* Start looking for a value composed of valid digits as set by the base
   in use.  Note that '_' characters are valid anywhere, in any quantity,
   and are simply ignored.  Since we must find at least one valid digit,
   or reject this token as an integer literal, we keep track of how many
   digits we have encountered. */
  
static int
decode_integer_value (base, tokptrptr, ivalptr)
  int base;
  char **tokptrptr;
  int *ivalptr;
{
  char *tokptr = *tokptrptr;
  int temp;
  int digits = 0;

  while (*tokptr != '\0')
    {
      temp = tolower (*tokptr);
      tokptr++;
      switch (temp)
	{
	case '_':
	  continue;
	case '0':  case '1':  case '2':  case '3':  case '4':
	case '5':  case '6':  case '7':  case '8':  case '9':
	  temp -= '0';
	  break;
	case 'a':  case 'b':  case 'c':  case 'd':  case 'e': case 'f':
	  temp -= 'a';
	  temp += 10;
	  break;
	default:
	  temp = base;
	  break;
	}
      if (temp < base)
	{
	  digits++;
	  *ivalptr *= base;
	  *ivalptr += temp;
	}
      else
	{
	  /* Found something not in domain for current base. */
	  tokptr--;	/* Unconsume what gave us indigestion. */
	  break;
	}
    }
  
  /* If we didn't find any digits, then we don't have a valid integer
     value, so reject the entire token.  Otherwise, update the lexical
     scan pointer, and return non-zero for success. */
  
  if (digits == 0)
    {
      return (0);
    }
  else
    {
      *tokptrptr = tokptr;
      return (1);
    }
}

static int
decode_integer_literal (valptr, tokptrptr)
  int *valptr;
  char **tokptrptr;
{
  char *tokptr = *tokptrptr;
  int base = 0;
  int ival = 0;
  int explicit_base = 0;
  
  /* Look for an explicit base specifier, which is optional. */
  
  switch (*tokptr)
    {
    case 'd':
    case 'D':
      explicit_base++;
      base = 10;
      tokptr++;
      break;
    case 'b':
    case 'B':
      explicit_base++;
      base = 2;
      tokptr++;
      break;
    case 'h':
    case 'H':
      explicit_base++;
      base = 16;
      tokptr++;
      break;
    case 'o':
    case 'O':
      explicit_base++;
      base = 8;
      tokptr++;
      break;
    default:
      base = 10;
      break;
    }
  
  /* If we found an explicit base ensure that the character after the
     explicit base is a single quote. */
  
  if (explicit_base && (*tokptr++ != '\''))
    {
      return (0);
    }
  
  /* Attempt to decode whatever follows as an integer value in the
     indicated base, updating the token pointer in the process and
     computing the value into ival.  Also, if we have an explicit
     base, then the next character must not be a single quote, or we
     have a bitstring literal, so reject the entire token in this case.
     Otherwise, update the lexical scan pointer, and return non-zero
     for success. */

  if (!decode_integer_value (base, &tokptr, &ival))
    {
      return (0);
    }
  else if (explicit_base && (*tokptr == '\''))
    {
      return (0);
    }
  else
    {
      *valptr = ival;
      *tokptrptr = tokptr;
      return (1);
    }
}

/*  If it wasn't for the fact that floating point values can contain '_'
    characters, we could just let strtod do all the hard work by letting it
    try to consume as much of the current token buffer as possible and
    find a legal conversion.  Unfortunately we need to filter out the '_'
    characters before calling strtod, which we do by copying the other
    legal chars to a local buffer to be converted.  However since we also
    need to keep track of where the last unconsumed character in the input
    buffer is, we have transfer only as many characters as may compose a
    legal floating point value. */
    
static int
match_float_literal ()
{
  char *tokptr = lexptr;
  char *buf;
  char *copy;
  double dval;
  extern double strtod ();
  
  /* Make local buffer in which to build the string to convert.  This is
     required because underscores are valid in chill floating point numbers
     but not in the string passed to strtod to convert.  The string will be
     no longer than our input string. */
     
  copy = buf = (char *) alloca (strlen (tokptr) + 1);

  /* Transfer all leading digits to the conversion buffer, discarding any
     underscores. */

  while (isdigit (*tokptr) || *tokptr == '_')
    {
      if (*tokptr != '_')
	{
	  *copy++ = *tokptr;
	}
      tokptr++;
    }

  /* Now accept either a '.', or one of [eEdD].  Dot is legal regardless
     of whether we found any leading digits, and we simply accept it and
     continue on to look for the fractional part and/or exponent.  One of
     [eEdD] is legal only if we have seen digits, and means that there
     is no fractional part.  If we find neither of these, then this is
     not a floating point number, so return failure. */

  switch (*tokptr++)
    {
      case '.':
        /* Accept and then look for fractional part and/or exponent. */
	*copy++ = '.';
	break;

      case 'e':
      case 'E':
      case 'd':
      case 'D':
	if (copy == buf)
	  {
	    return (0);
	  }
	*copy++ = 'e';
	goto collect_exponent;
	break;

      default:
	return (0);
        break;
    }

  /* We found a '.', copy any fractional digits to the conversion buffer, up
     to the first nondigit, non-underscore character. */

  while (isdigit (*tokptr) || *tokptr == '_')
    {
      if (*tokptr != '_')
	{
	  *copy++ = *tokptr;
	}
      tokptr++;
    }

  /* Look for an exponent, which must start with one of [eEdD].  If none
     is found, jump directly to trying to convert what we have collected
     so far. */

  switch (*tokptr)
    {
      case 'e':
      case 'E':
      case 'd':
      case 'D':
	*copy++ = 'e';
	tokptr++;
	break;
      default:
	goto convert_float;
	break;
    }

  /* Accept an optional '-' or '+' following one of [eEdD]. */

  collect_exponent:
  if (*tokptr == '+' || *tokptr == '-')
    {
      *copy++ = *tokptr++;
    }

  /* Now copy an exponent into the conversion buffer.  Note that at the 
     moment underscores are *not* allowed in exponents. */

  while (isdigit (*tokptr))
    {
      *copy++ = *tokptr++;
    }

  /* If we transfered any chars to the conversion buffer, try to interpret its
     contents as a floating point value.  If any characters remain, then we
     must not have a valid floating point string. */

  convert_float:
  *copy = '\0';
  if (copy != buf)
      {
        dval = strtod (buf, &copy);
        if (*copy == '\0')
	  {
	    yylval.dval = dval;
	    lexptr = tokptr;
	    return (FLOAT_LITERAL);
	  }
      }
  return (0);
}

/* Recognize a string literal.  A string literal is a nonzero sequence
   of characters enclosed in matching single or double quotes, except that
   a single character inside single quotes is a character literal, which
   we reject as a string literal.  To embed the terminator character inside
   a string, it is simply doubled (I.E. "this""is""one""string") */

static int
match_string_literal ()
{
  char *tokptr = lexptr;

  for (tempbufindex = 0, tokptr++; *tokptr != '\0'; tokptr++)
    {
      CHECKBUF (1);
      if (*tokptr == *lexptr)
	{
	  if (*(tokptr + 1) == *lexptr)
	    {
	      tokptr++;
	    }
	  else
	    {
	      break;
	    }
	}
      tempbuf[tempbufindex++] = *tokptr;
    }
  if (*tokptr == '\0'					/* no terminator */
      || tempbufindex == 0				/* no string */
      || (tempbufindex == 1 && *tokptr == '\''))	/* char literal */
    {
      return (0);
    }
  else
    {
      tempbuf[tempbufindex] = '\0';
      yylval.sval.ptr = tempbuf;
      yylval.sval.length = tempbufindex;
      lexptr = ++tokptr;
      return (CHARACTER_STRING_LITERAL);
    }
}

/* Recognize a character literal.  A character literal is single character
   or a control sequence, enclosed in single quotes.  A control sequence
   is a comma separated list of one or more integer literals, enclosed
   in parenthesis and introduced with a circumflex character.

   EX:  'a'  '^(7)'  '^(7,8)'

   As a GNU chill extension, the syntax C'xx' is also recognized as a 
   character literal, where xx is a hex value for the character.

   Note that more than a single character, enclosed in single quotes, is
   a string literal.

   Also note that the control sequence form is not in GNU Chill since it
   is ambiguous with the string literal form using single quotes.  I.E.
   is '^(7)' a character literal or a string literal.  In theory it it
   possible to tell by context, but GNU Chill doesn't accept the control
   sequence form, so neither do we (for now the code is disabled).

   Returns CHARACTER_LITERAL if a match is found.
   */

static int
match_character_literal ()
{
  char *tokptr = lexptr;
  int ival = 0;
  
  if ((tolower (*tokptr) == 'c') && (*(tokptr + 1) == '\''))
    {
      /* We have a GNU chill extension form, so skip the leading "C'",
	 decode the hex value, and then ensure that we have a trailing
	 single quote character. */
      tokptr += 2;
      if (!decode_integer_value (16, &tokptr, &ival) || (*tokptr != '\''))
	{
	  return (0);
	}
      tokptr++;
    }
  else if (*tokptr == '\'')
    {
      tokptr++;

      /* Determine which form we have, either a control sequence or the
	 single character form. */
      
      if ((*tokptr == '^') && (*(tokptr + 1) == '('))
	{
#if 0     /* Disable, see note above. -fnf */
	  /* Match and decode a control sequence.  Return zero if we don't
	     find a valid integer literal, or if the next unconsumed character
	     after the integer literal is not the trailing ')'.
	     FIXME:  We currently don't handle the multiple integer literal
	     form. */
	  tokptr += 2;
	  if (!decode_integer_literal (&ival, &tokptr) || (*tokptr++ != ')'))
	    {
	      return (0);
	    }
#else
	  return (0);
#endif
	}
      else
	{
	  ival = *tokptr++;
	}
      
      /* The trailing quote has not yet been consumed.  If we don't find
	 it, then we have no match. */
      
      if (*tokptr++ != '\'')
	{
	  return (0);
	}
    }
  else
    {
      /* Not a character literal. */
      return (0);
    }
  yylval.typed_val.val = ival;
  yylval.typed_val.type = builtin_type_chill_char;
  lexptr = tokptr;
  return (CHARACTER_LITERAL);
}

/* Recognize an integer literal, as specified in Z.200 sec 5.2.4.2.
   Note that according to 5.2.4.2, a single "_" is also a valid integer
   literal, however GNU-chill requires there to be at least one "digit"
   in any integer literal. */

static int
match_integer_literal ()
{
  char *tokptr = lexptr;
  int ival;
  
  if (!decode_integer_literal (&ival, &tokptr))
    {
      return (0);
    }
  else 
    {
      yylval.typed_val.val = ival;
      yylval.typed_val.type = builtin_type_int;
      lexptr = tokptr;
      return (INTEGER_LITERAL);
    }
}

/* Recognize a bit-string literal, as specified in Z.200 sec 5.2.4.8
   Note that according to 5.2.4.8, a single "_" is also a valid bit-string
   literal, however GNU-chill requires there to be at least one "digit"
   in any bit-string literal. */

static int
match_bitstring_literal ()
{
  char *tokptr = lexptr;
  int mask;
  int bitoffset = 0;
  int bitcount = 0;
  int base;
  int digit;
  
  tempbufindex = 0;

  /* Look for the required explicit base specifier. */
  
  switch (*tokptr++)
    {
    case 'b':
    case 'B':
      base = 2;
      break;
    case 'o':
    case 'O':
      base = 8;
      break;
    case 'h':
    case 'H':
      base = 16;
      break;
    default:
      return (0);
      break;
    }
  
  /* Ensure that the character after the explicit base is a single quote. */
  
  if (*tokptr++ != '\'')
    {
      return (0);
    }
  
  while (*tokptr != '\0' && *tokptr != '\'')
    {
      digit = tolower (*tokptr);
      tokptr++;
      switch (digit)
	{
	  case '_':
	    continue;
	  case '0':  case '1':  case '2':  case '3':  case '4':
	  case '5':  case '6':  case '7':  case '8':  case '9':
	    digit -= '0';
	    break;
	  case 'a':  case 'b':  case 'c':  case 'd':  case 'e': case 'f':
	    digit -= 'a';
	    digit += 10;
	    break;
	  default:
	    return (0);
	    break;
	}
      if (digit >= base)
	{
	  /* Found something not in domain for current base. */
	  return (0);
	}
      else
	{
	  /* Extract bits from digit, starting with the msbit appropriate for
	     the current base, and packing them into the bitstring byte,
	     starting at the lsbit. */
	  for (mask = (base >> 1); mask > 0; mask >>= 1)
	    {
	      bitcount++;
	      CHECKBUF (1);
	      if (digit & mask)
		{
		  tempbuf[tempbufindex] |= (1 << bitoffset);
		}
	      bitoffset++;
	      if (bitoffset == HOST_CHAR_BIT)
		{
		  bitoffset = 0;
		  tempbufindex++;
		}
	    }
	}
    }
  
  /* Verify that we consumed everything up to the trailing single quote,
     and that we found some bits (IE not just underbars). */

  if (*tokptr++ != '\'')
    {
      return (0);
    }
  else 
    {
      yylval.sval.ptr = tempbuf;
      yylval.sval.length = bitcount;
      lexptr = tokptr;
      return (BIT_STRING_LITERAL);
    }
}

/* Recognize tokens that start with '$'.  These include:

	$regname	A native register name or a "standard
			register name".
			Return token GDB_REGNAME.

	$variable	A convenience variable with a name chosen
			by the user.
			Return token GDB_VARIABLE.

	$digits		Value history with index <digits>, starting
			from the first value which has index 1.
			Return GDB_LAST.

	$$digits	Value history with index <digits> relative
			to the last value.  I.E. $$0 is the last
			value, $$1 is the one previous to that, $$2
			is the one previous to $$1, etc.
			Return token GDB_LAST.

	$ | $0 | $$0	The last value in the value history.
			Return token GDB_LAST.

	$$		An abbreviation for the second to the last
			value in the value history, I.E. $$1
			Return token GDB_LAST.

    Note that we currently assume that register names and convenience
    variables follow the convention of starting with a letter or '_'.

   */

static int
match_dollar_tokens ()
{
  char *tokptr;
  int regno;
  int namelength;
  int negate;
  int ival;

  /* We will always have a successful match, even if it is just for
     a single '$', the abbreviation for $$0.  So advance lexptr. */

  tokptr = ++lexptr;

  if (*tokptr == '_' || isalpha (*tokptr))
    {
      /* Look for a match with a native register name, usually something
	 like "r0" for example. */

      for (regno = 0; regno < NUM_REGS; regno++)
	{
	  namelength = strlen (reg_names[regno]);
	  if (STREQN (tokptr, reg_names[regno], namelength)
	      && !isalnum (tokptr[namelength]))
	    {
	      yylval.lval = regno;
	      lexptr += namelength + 1;
	      return (GDB_REGNAME);
	    }
	}

      /* Look for a match with a standard register name, usually something
	 like "pc", which gdb always recognizes as the program counter
	 regardless of what the native register name is. */

      for (regno = 0; regno < num_std_regs; regno++)
	{
	  namelength = strlen (std_regs[regno].name);
	  if (STREQN (tokptr, std_regs[regno].name, namelength)
	      && !isalnum (tokptr[namelength]))
	    {
	      yylval.lval = std_regs[regno].regnum;
	      lexptr += namelength;
	      return (GDB_REGNAME);
	    }
	}

      /* Attempt to match against a convenience variable.  Note that
	 this will always succeed, because if no variable of that name
	 already exists, the lookup_internalvar will create one for us.
	 Also note that both lexptr and tokptr currently point to the
	 start of the input string we are trying to match, and that we
	 have already tested the first character for non-numeric, so we
	 don't have to treat it specially. */

      while (*tokptr == '_' || isalnum (*tokptr))
	{
	  tokptr++;
	}
      yylval.sval.ptr = lexptr;
      yylval.sval.length = tokptr - lexptr;
      yylval.ivar = lookup_internalvar (copy_name (yylval.sval));
      lexptr = tokptr;
      return (GDB_VARIABLE);
    }

  /* Since we didn't match against a register name or convenience
     variable, our only choice left is a history value. */

  if (*tokptr == '$')
    {
      negate = 1;
      ival = 1;
      tokptr++;
    }
  else
    {
      negate = 0;
      ival = 0;
    }

  /* Attempt to decode more characters as an integer value giving
     the index in the history list.  If successful, the value will
     overwrite ival (currently 0 or 1), and if not, ival will be
     left alone, which is good since it is currently correct for
     the '$' or '$$' case. */

  decode_integer_literal (&ival, &tokptr);
  yylval.lval = negate ? -ival : ival;
  lexptr = tokptr;
  return (GDB_LAST);
}

struct token
{
  char *operator;
  int token;
};

static const struct token idtokentab[] =
{
    { "length", LENGTH },
    { "lower", LOWER },
    { "upper", UPPER },
    { "andif", ANDIF },
    { "pred", PRED },
    { "succ", SUCC },
    { "card", CARD },
    { "size", SIZE },
    { "orif", ORIF },
    { "num", NUM },
    { "abs", ABS },
    { "max", MAX_TOKEN },
    { "min", MIN_TOKEN },
    { "mod", MOD },
    { "rem", REM },
    { "not", NOT },
    { "xor", LOGXOR },
    { "and", LOGAND },
    { "in", IN },
    { "or", LOGIOR }
};

static const struct token tokentab2[] =
{
    { ":=", GDB_ASSIGNMENT },
    { "//", SLASH_SLASH },
    { "->", POINTER },
    { "/=", NOTEQUAL },
    { "<=", LEQ },
    { ">=", GTR }
};

/* Read one token, getting characters through lexptr.  */
/* This is where we will check to make sure that the language and the
   operators used are compatible.  */

static int
yylex ()
{
    unsigned int i;
    int token;
    char *simplename;
    struct symbol *sym;

    /* Skip over any leading whitespace. */
    while (isspace (*lexptr))
	{
	    lexptr++;
	}
    /* Look for special single character cases which can't be the first
       character of some other multicharacter token. */
    switch (*lexptr)
	{
	    case '\0':
	        return (0);
	    case ',':
	    case '=':
	    case ';':
	    case '!':
	    case '+':
	    case '*':
	    case '(':
	    case ')':
	    case '[':
	    case ']':
		return (*lexptr++);
	}
    /* Look for characters which start a particular kind of multicharacter
       token, such as a character literal, register name, convenience
       variable name, string literal, etc. */
    switch (*lexptr)
      {
	case '\'':
	case '\"':
	  /* First try to match a string literal, which is any nonzero
	     sequence of characters enclosed in matching single or double
	     quotes, except that a single character inside single quotes
	     is a character literal, so we have to catch that case also. */
	  token = match_string_literal ();
	  if (token != 0)
	    {
	      return (token);
	    }
	  if (*lexptr == '\'')
	    {
	      token = match_character_literal ();
	      if (token != 0)
		{
		  return (token);
		}
	    }
	  break;
        case 'C':
        case 'c':
	  token = match_character_literal ();
	  if (token != 0)
	    {
	      return (token);
	    }
	  break;
	case '$':
	  token = match_dollar_tokens ();
	  if (token != 0)
	    {
	      return (token);
	    }
	  break;
      }
    /* See if it is a special token of length 2.  */
    for (i = 0; i < sizeof (tokentab2) / sizeof (tokentab2[0]); i++)
	{
	    if (STREQN (lexptr, tokentab2[i].operator, 2))
		{
		    lexptr += 2;
		    return (tokentab2[i].token);
		}
	}
    /* Look for single character cases which which could be the first
       character of some other multicharacter token, but aren't, or we
       would already have found it. */
    switch (*lexptr)
	{
	    case '-':
	    case ':':
	    case '/':
	    case '<':
	    case '>':
		return (*lexptr++);
	}
    /* Look for a float literal before looking for an integer literal, so
       we match as much of the input stream as possible. */
    token = match_float_literal ();
    if (token != 0)
	{
	    return (token);
	}
    token = match_bitstring_literal ();
    if (token != 0)
	{
	    return (token);
	}
    token = match_integer_literal ();
    if (token != 0)
	{
	    return (token);
	}

    /* Try to match a simple name string, and if a match is found, then
       further classify what sort of name it is and return an appropriate
       token.  Note that attempting to match a simple name string consumes
       the token from lexptr, so we can't back out if we later find that
       we can't classify what sort of name it is. */

    simplename = match_simple_name_string ();

    if (simplename != NULL)
      {
	/* See if it is a reserved identifier. */
	for (i = 0; i < sizeof (idtokentab) / sizeof (idtokentab[0]); i++)
	    {
		if (STREQ (simplename, idtokentab[i].operator))
		    {
			return (idtokentab[i].token);
		    }
	    }

	/* Look for other special tokens. */
	if (STREQ (simplename, "true"))
	    {
		yylval.ulval = 1;
		return (BOOLEAN_LITERAL);
	    }
	if (STREQ (simplename, "false"))
	    {
		yylval.ulval = 0;
		return (BOOLEAN_LITERAL);
	    }

	sym = lookup_symbol (simplename, expression_context_block,
			     VAR_NAMESPACE, (int *) NULL,
			     (struct symtab **) NULL);
	if (sym != NULL)
	  {
	    yylval.ssym.stoken.ptr = NULL;
	    yylval.ssym.stoken.length = 0;
	    yylval.ssym.sym = sym;
	    yylval.ssym.is_a_field_of_this = 0;	/* FIXME, C++'ism */
	    switch (SYMBOL_CLASS (sym))
	      {
	      case LOC_BLOCK:
		/* Found a procedure name. */
		return (GENERAL_PROCEDURE_NAME);
	      case LOC_STATIC:
		/* Found a global or local static variable. */
		return (LOCATION_NAME);
	      case LOC_REGISTER:
	      case LOC_ARG:
	      case LOC_REF_ARG:
	      case LOC_REGPARM:
	      case LOC_REGPARM_ADDR:
	      case LOC_LOCAL:
	      case LOC_LOCAL_ARG:
	      case LOC_BASEREG:
	      case LOC_BASEREG_ARG:
		if (innermost_block == NULL
		    || contained_in (block_found, innermost_block))
		  {
		    innermost_block = block_found;
		  }
		return (LOCATION_NAME);
		break;
	      case LOC_CONST:
	      case LOC_LABEL:
		return (LOCATION_NAME);
		break;
	      case LOC_TYPEDEF:
		yylval.tsym.type = SYMBOL_TYPE (sym);
		return TYPENAME;
	      case LOC_UNDEF:
	      case LOC_CONST_BYTES:
	      case LOC_OPTIMIZED_OUT:
		error ("Symbol \"%s\" names no location.", simplename);
		break;
	      }
	  }
	else if (!have_full_symbols () && !have_partial_symbols ())
	  {
	    error ("No symbol table is loaded.  Use the \"file\" command.");
	  }
	else
	  {
	    error ("No symbol \"%s\" in current context.", simplename);
	  }
      }

    /* Catch single character tokens which are not part of some
       longer token. */

    switch (*lexptr)
      {
	case '.':			/* Not float for example. */
	  lexptr++;
	  while (isspace (*lexptr)) lexptr++;
	  simplename = match_simple_name_string ();
	  if (!simplename)
	    return '.';
	  return FIELD_NAME;
      }

    return (ILLEGAL_TOKEN);
}

void
yyerror (msg)
     char *msg;	/* unused */
{
  printf ("Parsing:  %s\n", lexptr);
  if (yychar < 256)
    {
      error ("Invalid syntax in expression near character '%c'.", yychar);
    }
  else
    {
      error ("Invalid syntax in expression");
    }
}
#line 1836 "y.tab.c"
#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;
    extern char *getenv();

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if (yyn = yydefred[yystate]) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yyss + yystacksize - 1)
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#ifdef lint
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yyss + yystacksize - 1)
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 1:
#line 312 "./ch-exp.y"
{ }
break;
case 2:
#line 314 "./ch-exp.y"
{ write_exp_elt_opcode(OP_TYPE);
			  write_exp_elt_type(yyvsp[0].tsym.type);
			  write_exp_elt_opcode(OP_TYPE);}
break;
case 3:
#line 320 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 4:
#line 324 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 5:
#line 330 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 7:
#line 339 "./ch-exp.y"
{
			  write_exp_elt_opcode (UNOP_IND);
			}
break;
case 8:
#line 347 "./ch-exp.y"
{
			  write_exp_elt_opcode (OP_VAR_VALUE);
			  write_exp_elt_block (NULL);
			  write_exp_elt_sym (yyvsp[0].ssym.sym);
			  write_exp_elt_opcode (OP_VAR_VALUE);
			}
break;
case 9:
#line 354 "./ch-exp.y"
{
			  write_exp_elt_opcode (OP_LAST);
			  write_exp_elt_longcst (yyvsp[0].lval);
			  write_exp_elt_opcode (OP_LAST); 
			}
break;
case 10:
#line 360 "./ch-exp.y"
{
			  write_exp_elt_opcode (OP_REGISTER);
			  write_exp_elt_longcst (yyvsp[0].lval);
			  write_exp_elt_opcode (OP_REGISTER); 
			}
break;
case 11:
#line 366 "./ch-exp.y"
{
			  write_exp_elt_opcode (OP_INTERNALVAR);
			  write_exp_elt_intern (yyvsp[0].ivar);
			  write_exp_elt_opcode (OP_INTERNALVAR); 
			}
break;
case 12:
#line 372 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 13:
#line 380 "./ch-exp.y"
{
			  arglist_len = 1;
			}
break;
case 14:
#line 384 "./ch-exp.y"
{
			  arglist_len++;
			}
break;
case 15:
#line 391 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 16:
#line 395 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 17:
#line 399 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 18:
#line 403 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 19:
#line 407 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 20:
#line 411 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 21:
#line 415 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 22:
#line 419 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 23:
#line 423 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 24:
#line 427 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 25:
#line 431 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 26:
#line 435 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 27:
#line 439 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 28:
#line 443 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 29:
#line 447 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 30:
#line 455 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 31:
#line 463 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 32:
#line 467 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 33:
#line 471 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 34:
#line 475 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 35:
#line 479 "./ch-exp.y"
{
			  write_exp_elt_opcode (OP_VAR_VALUE);
			  write_exp_elt_block (NULL);
			  write_exp_elt_sym (yyvsp[0].ssym.sym);
			  write_exp_elt_opcode (OP_VAR_VALUE);
			}
break;
case 36:
#line 490 "./ch-exp.y"
{
			  write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (yyvsp[0].typed_val.type);
			  write_exp_elt_longcst ((LONGEST) (yyvsp[0].typed_val.val));
			  write_exp_elt_opcode (OP_LONG);
			}
break;
case 37:
#line 497 "./ch-exp.y"
{
			  write_exp_elt_opcode (OP_BOOL);
			  write_exp_elt_longcst ((LONGEST) yyvsp[0].ulval);
			  write_exp_elt_opcode (OP_BOOL);
			}
break;
case 38:
#line 503 "./ch-exp.y"
{
			  write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (yyvsp[0].typed_val.type);
			  write_exp_elt_longcst ((LONGEST) (yyvsp[0].typed_val.val));
			  write_exp_elt_opcode (OP_LONG);
			}
break;
case 39:
#line 510 "./ch-exp.y"
{
			  write_exp_elt_opcode (OP_DOUBLE);
			  write_exp_elt_type (builtin_type_double);
			  write_exp_elt_dblcst (yyvsp[0].dval);
			  write_exp_elt_opcode (OP_DOUBLE);
			}
break;
case 40:
#line 517 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 41:
#line 521 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 42:
#line 525 "./ch-exp.y"
{
			  write_exp_elt_opcode (OP_STRING);
			  write_exp_string (yyvsp[0].sval);
			  write_exp_elt_opcode (OP_STRING);
			}
break;
case 43:
#line 531 "./ch-exp.y"
{
			  write_exp_elt_opcode (OP_BITSTRING);
			  write_exp_bitstring (yyvsp[0].sval);
			  write_exp_elt_opcode (OP_BITSTRING);
			}
break;
case 44:
#line 541 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 45:
#line 550 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 46:
#line 558 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 47:
#line 562 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 48:
#line 572 "./ch-exp.y"
{ start_arglist (); }
break;
case 49:
#line 574 "./ch-exp.y"
{
			  write_exp_elt_opcode (MULTI_SUBSCRIPT);
			  write_exp_elt_longcst ((LONGEST) end_arglist ());
			  write_exp_elt_opcode (MULTI_SUBSCRIPT);
			}
break;
case 50:
#line 584 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 51:
#line 588 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 52:
#line 596 "./ch-exp.y"
{ write_exp_elt_opcode (STRUCTOP_STRUCT);
			  write_exp_string (yyvsp[0].sval);
			  write_exp_elt_opcode (STRUCTOP_STRUCT);
			}
break;
case 53:
#line 605 "./ch-exp.y"
{
			  write_exp_elt_opcode (UNOP_CAST);
			  write_exp_elt_type (yyvsp[-1].tsym.type);
			  write_exp_elt_opcode (UNOP_CAST);
			}
break;
case 54:
#line 615 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 55:
#line 623 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 56:
#line 631 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 57:
#line 639 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 58:
#line 647 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 59:
#line 655 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 60:
#line 659 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 61:
#line 663 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 62:
#line 669 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 63:
#line 673 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 64:
#line 679 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 65:
#line 685 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 66:
#line 689 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 67:
#line 695 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 68:
#line 701 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 69:
#line 709 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 70:
#line 713 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_BITWISE_IOR);
			}
break;
case 71:
#line 717 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 72:
#line 721 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_BITWISE_XOR);
			}
break;
case 73:
#line 729 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 74:
#line 733 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_BITWISE_AND);
			}
break;
case 75:
#line 737 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 76:
#line 745 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 77:
#line 749 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_EQUAL);
			}
break;
case 78:
#line 753 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_NOTEQUAL);
			}
break;
case 79:
#line 757 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_GTR);
			}
break;
case 80:
#line 761 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_GEQ);
			}
break;
case 81:
#line 765 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_LESS);
			}
break;
case 82:
#line 769 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_LEQ);
			}
break;
case 83:
#line 773 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 84:
#line 782 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 85:
#line 786 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_ADD);
			}
break;
case 86:
#line 790 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_SUB);
			}
break;
case 87:
#line 794 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_CONCAT);
			}
break;
case 88:
#line 802 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 89:
#line 806 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_MUL);
			}
break;
case 90:
#line 810 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_DIV);
			}
break;
case 91:
#line 814 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_MOD);
			}
break;
case 92:
#line 818 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_REM);
			}
break;
case 93:
#line 826 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 94:
#line 830 "./ch-exp.y"
{
			  write_exp_elt_opcode (UNOP_NEG);
			}
break;
case 95:
#line 834 "./ch-exp.y"
{
			  write_exp_elt_opcode (UNOP_LOGICAL_NOT);
			}
break;
case 96:
#line 840 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_CONCAT);
			}
break;
case 97:
#line 848 "./ch-exp.y"
{
			  write_exp_elt_opcode (UNOP_ADDR);
			}
break;
case 98:
#line 852 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 99:
#line 856 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 100:
#line 866 "./ch-exp.y"
{
			  write_exp_elt_opcode (BINOP_ASSIGN);
			}
break;
case 101:
#line 875 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 102:
#line 879 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 103:
#line 883 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 104:
#line 887 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 105:
#line 891 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 106:
#line 895 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 107:
#line 899 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 108:
#line 903 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 109:
#line 907 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 110:
#line 911 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 111:
#line 915 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 112:
#line 919 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 113:
#line 925 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 114:
#line 929 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 115:
#line 933 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 116:
#line 937 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 118:
#line 946 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 119:
#line 950 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 120:
#line 956 "./ch-exp.y"
{
			  yyval.voidval = 0;	/* FIXME */
			}
break;
case 121:
#line 964 "./ch-exp.y"
{
			  yyval.voidval = 0;
			}
break;
case 122:
#line 972 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 123:
#line 973 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 124:
#line 974 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 125:
#line 975 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 126:
#line 976 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 127:
#line 977 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 128:
#line 978 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 129:
#line 979 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 130:
#line 980 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 131:
#line 981 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 132:
#line 982 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 133:
#line 983 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 134:
#line 984 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 135:
#line 985 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 136:
#line 986 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 137:
#line 987 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 138:
#line 988 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 139:
#line 989 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 140:
#line 990 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
case 141:
#line 991 "./ch-exp.y"
{ yyval.voidval = 0; }
break;
#line 2799 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yyss + yystacksize - 1)
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
