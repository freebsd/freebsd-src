#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.8 (Berkeley) 01/20/90";
#endif
#define YYBYACC 1
#line 20 "/u/jjc/groff/eqn/eqn.y"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lib.h"
#include "box.h"
extern int non_empty_flag;
char *strsave(const char *);
int yylex();
void yyerror(const char *);
#line 32 "/u/jjc/groff/eqn/eqn.y"
typedef union {
	char *str;
	box *b;
	pile_box *pb;
	matrix_box *mb;
	int n;
	column *col;
} YYSTYPE;
#line 26 "y.tab.c"
#define OVER 257
#define SMALLOVER 258
#define SQRT 259
#define SUB 260
#define SUP 261
#define LPILE 262
#define RPILE 263
#define CPILE 264
#define PILE 265
#define LEFT 266
#define RIGHT 267
#define TO 268
#define FROM 269
#define SIZE 270
#define FONT 271
#define ROMAN 272
#define BOLD 273
#define ITALIC 274
#define FAT 275
#define ACCENT 276
#define BAR 277
#define UNDER 278
#define ABOVE 279
#define TEXT 280
#define QUOTED_TEXT 281
#define FWD 282
#define BACK 283
#define DOWN 284
#define UP 285
#define MATRIX 286
#define COL 287
#define LCOL 288
#define RCOL 289
#define CCOL 290
#define MARK 291
#define LINEUP 292
#define TYPE 293
#define VCENTER 294
#define PRIME 295
#define SPLIT 296
#define NOSPLIT 297
#define UACCENT 298
#define SPECIAL 299
#define SPACE 300
#define GFONT 301
#define GSIZE 302
#define DEFINE 303
#define NDEFINE 304
#define TDEFINE 305
#define SDEFINE 306
#define UNDEF 307
#define IFDEF 308
#define INCLUDE 309
#define DELIM 310
#define CHARTYPE 311
#define SET 312
#define GRFONT 313
#define GBFONT 314
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    0,    6,    6,    1,    1,    1,    2,    2,    2,
    2,    2,    3,    3,    3,    3,    4,    4,    7,    7,
    7,    5,    5,    5,    5,    5,    5,    5,    5,    5,
    5,    5,    5,    5,    5,    5,    5,    5,    5,    5,
    5,    5,    5,    5,    5,    5,    5,    5,    5,    5,
    5,    5,    5,    5,    8,   11,   11,   12,   12,   13,
   13,   16,   16,   15,   15,   14,   14,   14,   14,    9,
    9,   10,   10,   10,
};
short yylen[] = {                                         2,
    0,    1,    1,    2,    1,    2,    2,    1,    3,    3,
    5,    5,    1,    2,    3,    3,    1,    3,    1,    3,
    5,    1,    1,    2,    2,    1,    1,    1,    3,    2,
    2,    2,    2,    4,    5,    3,    2,    2,    2,    3,
    3,    2,    2,    2,    2,    3,    3,    3,    3,    3,
    3,    3,    2,    3,    1,    1,    3,    3,    4,    1,
    2,    1,    3,    3,    4,    2,    2,    2,    2,    1,
    1,    1,    1,    1,
};
short yydefred[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   22,   23,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   26,   27,   28,    0,
    0,    3,    5,    0,   13,    0,    0,   17,   14,   70,
   71,    0,    0,   55,   31,   32,   33,   30,   73,   74,
   72,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    6,    7,    0,    0,   24,   25,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   37,   38,
   39,    0,    4,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   60,    0,
    0,   29,   15,   16,    9,    0,    0,   20,   18,   40,
   41,    0,   58,    0,    0,    0,    0,   66,   67,   68,
   69,   34,   61,    0,    0,    0,    0,   59,   35,    0,
    0,    0,   11,   12,   21,    0,   64,    0,    0,   65,
};
short yydgoto[] = {                                      31,
   32,   33,   34,   35,   36,   84,   38,   43,   44,   52,
   85,   45,   98,   99,  118,  131,
};
short yysindex[] = {                                   1488,
 1527, -120, -120, -120, -120, -123, -249, -249, 1566, 1566,
 1566, 1566,    0,    0, -249, -249, -249, -249, -115, 1488,
 1488, -249, 1566, -256, -251, -249,    0,    0,    0, 1488,
    0,    0,    0, -221,    0, -233, 1488,    0,    0,    0,
    0, 1488,  -85,    0,    0,    0,    0,    0,    0,    0,
    0, 1488, 1566, 1566, -195, -195, -195, -195, 1566, 1566,
 1566, 1566, -272,    0,    0, 1566, -195,    0,    0, 1566,
 1402, 1527, 1527, 1527, 1527, 1566, 1566, 1566,    0,    0,
    0, 1566,    0, 1488, -113, 1488, 1444, -195, -195, -195,
 -195, -195, -195, -117, -117, -117, -117, -118,    0, -195,
 -195,    0,    0,    0,    0, -167, -189,    0,    0,    0,
    0, 1488,    0, -106, -123, 1488,  -83,    0,    0,    0,
    0,    0,    0, 1527, 1527, 1566, 1488,    0,    0, 1488,
 -105, 1488,    0,    0,    0, 1488,    0, -104, 1488,    0,
};
short yyrindex[] = {                                     41,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    1,    0, 1220,   46,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   85,  128,  363,  406,    0,    0,
    0,    0,    0,    0,    0,    0,  449,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0, -103,    0,    0,  185,  492,  727,  770,
  813,  856, 1091,    0,    0,    0,    0,    0,    0, 1134,
 1177,    0,    0,    0,    0,   42, 1220,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0, -102,    0,    0, -101,
    0,    0,    0,    0,    0,    0,    0,    0,  -99,    0,
};
short yygindex[] = {                                      0,
   -7,  -69,    3,  -66,  458,    9,  -26,   52,   27,  -63,
  -32,   54,    0,  -35,    2,  -59,
};
#define YYTABLESIZE 1865
short yytable[] = {                                      49,
    8,   50,   42,   39,  105,  116,  122,   63,   37,    8,
  109,  113,   64,   65,   94,   95,   96,   97,  128,  137,
  140,   56,   57,   62,   68,   63,   76,   77,   69,   83,
   40,   41,   51,   53,   54,   72,   73,   86,   71,  132,
    1,   10,   78,   79,   80,    2,   74,   75,   66,  108,
   10,  129,   70,  114,  133,  134,   46,   47,   48,  135,
   87,   81,  123,   83,   82,    0,   59,   60,   61,   62,
   76,  126,  138,    0,  103,  104,   83,  106,    0,   83,
   78,   79,   80,    0,   42,    0,   78,   79,   80,   72,
   73,    0,    0,   42,    8,    0,  119,  120,  121,   81,
  124,  125,   82,    0,    0,   81,    0,    0,   82,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   83,
  127,    0,   83,    8,  130,    8,    8,   43,    0,    0,
    0,   83,    0,    0,    0,   10,   43,    0,    0,    0,
  130,   51,    0,    0,  139,  117,  117,  117,  117,    0,
    0,    0,    0,    0,    0,    0,   40,   41,    0,   40,
   41,    0,   40,   41,   10,  112,   10,   10,   94,   95,
   96,   97,  112,  136,  136,   56,   57,   62,   42,   63,
    0,    0,    0,    0,   36,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   42,    0,   42,
   42,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   43,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   43,    0,   43,   43,    0,    0,    0,    0,    0,    8,
    8,    8,    8,    8,    8,    8,    8,    8,    0,    0,
    8,    8,    8,    8,    8,    8,    8,    8,    8,    8,
    8,    8,    8,    8,    8,    8,    8,    0,    0,    0,
    0,    8,    8,    8,    8,    8,    8,    8,    8,    8,
   10,   10,   10,   10,   10,   10,   10,   10,   10,   36,
    0,   10,   10,   10,   10,   10,   10,   10,   10,   10,
   10,   10,   10,   10,   10,   10,   10,   10,    0,    0,
    0,    0,   10,   10,   10,   10,   10,   10,   10,   10,
   10,   42,   42,   42,   42,   42,   42,   42,   42,   42,
   42,   42,   42,   42,   42,   42,   42,   42,   42,   42,
    0,    0,   44,   42,   42,   42,   42,   42,   42,   42,
   42,   44,    0,    0,    0,   42,   42,   42,   42,    0,
   42,   42,    0,   42,   43,   43,   43,   43,   43,   43,
   43,   43,   43,   43,   43,   43,   43,   43,   43,   43,
   43,   43,   43,    0,    0,   45,   43,   43,   43,   43,
   43,   43,   43,   43,   45,    0,    0,    0,   43,   43,
   43,   43,    0,   43,   43,    0,   43,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   36,   36,    0,   36,   36,    0,    0,   53,    0,
    0,    0,   36,   36,    0,    0,   44,   53,    0,    0,
   36,   36,   36,   36,    0,    0,   55,   56,   57,   58,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   36,
   67,    0,   36,    0,    0,   44,    0,   44,   44,    0,
    0,   47,    0,    0,    0,    0,    0,    0,    0,   45,
   47,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   88,   89,    0,    0,    0,    0,   90,   91,   92,   93,
    0,    0,    0,  100,    0,    0,    0,  101,   45,    0,
   45,   45,    0,  107,    0,  110,    0,    0,    0,  111,
    0,    0,   53,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   53,    0,   53,   53,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   47,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   47,    0,   47,   47,    0,   44,
   44,   44,   44,   44,   44,   44,   44,   44,   44,   44,
   44,   44,   44,   44,   44,   44,   44,   44,    0,    0,
    0,   44,   44,   44,   44,   44,   44,   44,   44,    0,
    0,    0,    0,   44,   44,   44,   44,    0,   44,   44,
    0,   44,   45,   45,   45,   45,   45,   45,   45,   45,
   45,   45,   45,   45,   45,   45,   45,   45,   45,   45,
   45,    0,    0,    0,   45,   45,   45,   45,   45,   45,
   45,   45,    0,    0,    0,    0,   45,   45,   45,   45,
    0,   45,   45,    0,   45,   53,   53,   53,   53,   53,
   53,   53,   53,   53,   53,   53,   53,   53,   53,   53,
   53,   53,   53,   53,    0,    0,   46,   53,   53,   53,
   53,   53,   53,   53,   53,   46,    0,    0,    0,   53,
   53,   53,   53,    0,   53,   53,    0,   53,   47,   47,
   47,   47,   47,   47,   47,   47,   47,   47,   47,   47,
   47,   47,   47,   47,   47,   47,   47,    0,    0,   48,
   47,   47,   47,   47,   47,   47,   47,   47,   48,    0,
    0,    0,   47,   47,   47,   47,    0,   47,   47,    0,
   47,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   49,    0,    0,    0,    0,    0,    0,    0,
   46,   49,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   46,
    0,   46,   46,    0,    0,   51,    0,    0,    0,    0,
    0,    0,    0,   48,   51,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   48,    0,   48,   48,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   49,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   49,    0,   49,   49,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   51,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   51,    0,
   51,   51,    0,   46,   46,   46,   46,   46,   46,   46,
   46,   46,   46,   46,   46,   46,   46,   46,   46,   46,
   46,   46,    0,    0,    0,   46,   46,   46,   46,   46,
   46,   46,   46,    0,    0,    0,    0,   46,   46,   46,
   46,    0,   46,   46,    0,   46,   48,   48,   48,   48,
   48,   48,   48,   48,   48,   48,   48,   48,   48,   48,
   48,   48,   48,   48,   48,    0,    0,    0,   48,   48,
   48,   48,   48,   48,   48,   48,    0,    0,    0,    0,
   48,   48,   48,   48,    0,   48,   48,    0,   48,   49,
   49,   49,   49,   49,   49,   49,   49,   49,   49,   49,
   49,   49,   49,   49,   49,   49,   49,   49,    0,    0,
   50,   49,   49,   49,   49,   49,   49,   49,   49,   50,
    0,    0,    0,   49,   49,   49,   49,    0,   49,   49,
    0,   49,   51,   51,   51,   51,   51,   51,   51,   51,
   51,   51,   51,   51,   51,   51,   51,   51,   51,   51,
   51,    0,    0,   52,   51,   51,   51,   51,   51,   51,
   51,   51,   52,    0,    0,    0,   51,   51,   51,   51,
    0,   51,   51,    0,   51,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   54,    0,    0,    0,
    0,    0,    0,    0,   50,   54,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   50,    0,   50,   50,    0,    0,   19,
    0,    0,    0,    0,    0,    0,    0,   52,   19,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   52,    0,   52,   52,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   54,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   54,
    0,   54,   54,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   19,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   19,    0,   19,   19,    0,   50,   50,   50,
   50,   50,   50,   50,   50,   50,   50,   50,   50,   50,
   50,   50,   50,   50,   50,   50,    0,    0,    0,   50,
   50,   50,   50,   50,   50,   50,   50,    0,    0,    0,
    0,   50,   50,   50,   50,    0,   50,   50,    0,   50,
   52,   52,   52,   52,   52,   52,   52,   52,   52,   52,
   52,   52,   52,   52,   52,   52,   52,   52,   52,    0,
   29,    0,   52,   52,   52,   52,   52,   52,   52,   52,
    0,    0,    0,    0,   52,   52,   52,   52,    0,   52,
   52,    0,   52,   54,   54,   54,   54,   54,   54,   54,
   54,   54,   54,   54,   54,   54,   54,   54,   54,   54,
   54,   54,   29,    0,    0,   54,   54,   54,   54,   54,
   54,   54,   54,    0,    0,    0,    0,   54,   54,   54,
   54,    0,   54,   54,    0,   54,   19,   19,   19,    0,
    0,   19,   19,   19,   19,   19,   19,   19,   19,   19,
   19,   19,   19,   19,   19,   27,   29,    0,   19,   19,
   19,   19,   19,   19,   19,   19,    0,    0,    0,    0,
   19,   19,   19,   19,    0,   19,   19,    0,   19,    0,
    0,    0,    0,    0,   30,    0,  102,   28,    0,    0,
    0,    0,    0,    0,    0,   29,    0,   27,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   30,    0,    0,   28,
    0,    0,    0,    0,   29,    0,    0,    0,    0,    0,
    0,   27,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   30,    0,    0,   28,    0,    0,    0,    0,    0,    0,
   27,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   30,
    0,    0,   28,    0,    0,    0,    0,    0,    0,   27,
    1,    0,    0,    2,    3,    4,    5,    6,    0,    0,
    0,    7,    8,    9,   10,   11,   12,    0,    0,    0,
    0,   13,   14,   15,   16,   17,   18,   19,   30,    0,
    0,   28,   20,   21,   22,   23,    0,   24,   25,    0,
   26,    0,    1,    0,    0,    2,    3,    4,    5,    6,
  115,    0,    0,    7,    8,    9,   10,   11,   12,    0,
    0,    0,    0,   13,   14,   15,   16,   17,   18,   19,
    0,    0,    0,    0,   20,   21,   22,   23,    0,   24,
   25,    0,   26,    0,    0,    0,    1,    0,    0,    2,
    3,    4,    5,    6,    0,    0,    0,    7,    8,    9,
   10,   11,   12,    0,    0,    0,    0,   13,   14,   15,
   16,   17,   18,   19,    0,    0,    0,    0,   20,   21,
   22,   23,    0,   24,   25,    1,   26,    0,    2,    3,
    4,    5,    6,    0,    0,    0,    7,    8,    9,   10,
   11,   12,    0,    0,    0,    0,   13,   14,   15,   16,
   17,   18,   19,    0,    0,    0,    0,    0,    0,   22,
   23,    0,   24,   25,    0,   26,    0,    2,    3,    4,
    5,    6,    0,    0,    0,    7,    8,    9,   10,   11,
   12,    0,    0,    0,    0,   13,   14,   15,   16,   17,
   18,   19,    0,    0,    0,    0,    0,    0,   22,   23,
    0,   24,   25,    0,   26,
};
short yycheck[] = {                                     123,
    0,  125,  123,    1,   74,  123,  125,  123,    0,    9,
   77,  125,   20,   21,  287,  288,  289,  290,  125,  125,
  125,  125,  125,  125,  281,  125,  260,  261,  280,   37,
  280,  281,    6,    7,    8,  257,  258,  123,   30,  123,
    0,    0,  276,  277,  278,    0,  268,  269,   22,   76,
    9,  115,   26,   86,  124,  125,    3,    4,    5,  126,
   52,  295,   98,   71,  298,   -1,   15,   16,   17,   18,
  260,  261,  132,   -1,   72,   73,   84,   75,   -1,   87,
  276,  277,  278,   -1,    0,   -1,  276,  277,  278,  257,
  258,   -1,   -1,    9,   94,   -1,   95,   96,   97,  295,
  268,  269,  298,   -1,   -1,  295,   -1,   -1,  298,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  127,
  112,   -1,  130,  123,  116,  125,  126,    0,   -1,   -1,
   -1,  139,   -1,   -1,   -1,   94,    9,   -1,   -1,   -1,
  132,  115,   -1,   -1,  136,   94,   95,   96,   97,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  280,  281,   -1,  280,
  281,   -1,  280,  281,  123,  279,  125,  126,  287,  288,
  289,  290,  279,  279,  279,  279,  279,  279,   94,  279,
   -1,   -1,   -1,   -1,    0,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  123,   -1,  125,
  126,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   94,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  123,   -1,  125,  126,   -1,   -1,   -1,   -1,   -1,  259,
  260,  261,  262,  263,  264,  265,  266,  267,   -1,   -1,
  270,  271,  272,  273,  274,  275,  276,  277,  278,  279,
  280,  281,  282,  283,  284,  285,  286,   -1,   -1,   -1,
   -1,  291,  292,  293,  294,  295,  296,  297,  298,  299,
  259,  260,  261,  262,  263,  264,  265,  266,  267,  125,
   -1,  270,  271,  272,  273,  274,  275,  276,  277,  278,
  279,  280,  281,  282,  283,  284,  285,  286,   -1,   -1,
   -1,   -1,  291,  292,  293,  294,  295,  296,  297,  298,
  299,  257,  258,  259,  260,  261,  262,  263,  264,  265,
  266,  267,  268,  269,  270,  271,  272,  273,  274,  275,
   -1,   -1,    0,  279,  280,  281,  282,  283,  284,  285,
  286,    9,   -1,   -1,   -1,  291,  292,  293,  294,   -1,
  296,  297,   -1,  299,  257,  258,  259,  260,  261,  262,
  263,  264,  265,  266,  267,  268,  269,  270,  271,  272,
  273,  274,  275,   -1,   -1,    0,  279,  280,  281,  282,
  283,  284,  285,  286,    9,   -1,   -1,   -1,  291,  292,
  293,  294,   -1,  296,  297,   -1,  299,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  257,  258,   -1,  260,  261,   -1,   -1,    0,   -1,
   -1,   -1,  268,  269,   -1,   -1,   94,    9,   -1,   -1,
  276,  277,  278,  279,   -1,   -1,    9,   10,   11,   12,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  295,
   23,   -1,  298,   -1,   -1,  123,   -1,  125,  126,   -1,
   -1,    0,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   94,
    9,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   53,   54,   -1,   -1,   -1,   -1,   59,   60,   61,   62,
   -1,   -1,   -1,   66,   -1,   -1,   -1,   70,  123,   -1,
  125,  126,   -1,   76,   -1,   78,   -1,   -1,   -1,   82,
   -1,   -1,   94,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  123,   -1,  125,  126,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   94,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  123,   -1,  125,  126,   -1,  257,
  258,  259,  260,  261,  262,  263,  264,  265,  266,  267,
  268,  269,  270,  271,  272,  273,  274,  275,   -1,   -1,
   -1,  279,  280,  281,  282,  283,  284,  285,  286,   -1,
   -1,   -1,   -1,  291,  292,  293,  294,   -1,  296,  297,
   -1,  299,  257,  258,  259,  260,  261,  262,  263,  264,
  265,  266,  267,  268,  269,  270,  271,  272,  273,  274,
  275,   -1,   -1,   -1,  279,  280,  281,  282,  283,  284,
  285,  286,   -1,   -1,   -1,   -1,  291,  292,  293,  294,
   -1,  296,  297,   -1,  299,  257,  258,  259,  260,  261,
  262,  263,  264,  265,  266,  267,  268,  269,  270,  271,
  272,  273,  274,  275,   -1,   -1,    0,  279,  280,  281,
  282,  283,  284,  285,  286,    9,   -1,   -1,   -1,  291,
  292,  293,  294,   -1,  296,  297,   -1,  299,  257,  258,
  259,  260,  261,  262,  263,  264,  265,  266,  267,  268,
  269,  270,  271,  272,  273,  274,  275,   -1,   -1,    0,
  279,  280,  281,  282,  283,  284,  285,  286,    9,   -1,
   -1,   -1,  291,  292,  293,  294,   -1,  296,  297,   -1,
  299,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,    0,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   94,    9,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  123,
   -1,  125,  126,   -1,   -1,    0,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   94,    9,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  123,   -1,  125,  126,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   94,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  123,   -1,  125,  126,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   94,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  123,   -1,
  125,  126,   -1,  257,  258,  259,  260,  261,  262,  263,
  264,  265,  266,  267,  268,  269,  270,  271,  272,  273,
  274,  275,   -1,   -1,   -1,  279,  280,  281,  282,  283,
  284,  285,  286,   -1,   -1,   -1,   -1,  291,  292,  293,
  294,   -1,  296,  297,   -1,  299,  257,  258,  259,  260,
  261,  262,  263,  264,  265,  266,  267,  268,  269,  270,
  271,  272,  273,  274,  275,   -1,   -1,   -1,  279,  280,
  281,  282,  283,  284,  285,  286,   -1,   -1,   -1,   -1,
  291,  292,  293,  294,   -1,  296,  297,   -1,  299,  257,
  258,  259,  260,  261,  262,  263,  264,  265,  266,  267,
  268,  269,  270,  271,  272,  273,  274,  275,   -1,   -1,
    0,  279,  280,  281,  282,  283,  284,  285,  286,    9,
   -1,   -1,   -1,  291,  292,  293,  294,   -1,  296,  297,
   -1,  299,  257,  258,  259,  260,  261,  262,  263,  264,
  265,  266,  267,  268,  269,  270,  271,  272,  273,  274,
  275,   -1,   -1,    0,  279,  280,  281,  282,  283,  284,
  285,  286,    9,   -1,   -1,   -1,  291,  292,  293,  294,
   -1,  296,  297,   -1,  299,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,    0,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   94,    9,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,  123,   -1,  125,  126,   -1,   -1,    0,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   94,    9,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  123,   -1,  125,  126,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   94,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  123,
   -1,  125,  126,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   94,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  123,   -1,  125,  126,   -1,  257,  258,  259,
  260,  261,  262,  263,  264,  265,  266,  267,  268,  269,
  270,  271,  272,  273,  274,  275,   -1,   -1,   -1,  279,
  280,  281,  282,  283,  284,  285,  286,   -1,   -1,   -1,
   -1,  291,  292,  293,  294,   -1,  296,  297,   -1,  299,
  257,  258,  259,  260,  261,  262,  263,  264,  265,  266,
  267,  268,  269,  270,  271,  272,  273,  274,  275,   -1,
    9,   -1,  279,  280,  281,  282,  283,  284,  285,  286,
   -1,   -1,   -1,   -1,  291,  292,  293,  294,   -1,  296,
  297,   -1,  299,  257,  258,  259,  260,  261,  262,  263,
  264,  265,  266,  267,  268,  269,  270,  271,  272,  273,
  274,  275,    9,   -1,   -1,  279,  280,  281,  282,  283,
  284,  285,  286,   -1,   -1,   -1,   -1,  291,  292,  293,
  294,   -1,  296,  297,   -1,  299,  257,  258,  259,   -1,
   -1,  262,  263,  264,  265,  266,  267,  268,  269,  270,
  271,  272,  273,  274,  275,   94,    9,   -1,  279,  280,
  281,  282,  283,  284,  285,  286,   -1,   -1,   -1,   -1,
  291,  292,  293,  294,   -1,  296,  297,   -1,  299,   -1,
   -1,   -1,   -1,   -1,  123,   -1,  125,  126,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,    9,   -1,   94,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  123,   -1,   -1,  126,
   -1,   -1,   -1,   -1,    9,   -1,   -1,   -1,   -1,   -1,
   -1,   94,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  123,   -1,   -1,  126,   -1,   -1,   -1,   -1,   -1,   -1,
   94,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  123,
   -1,   -1,  126,   -1,   -1,   -1,   -1,   -1,   -1,   94,
  259,   -1,   -1,  262,  263,  264,  265,  266,   -1,   -1,
   -1,  270,  271,  272,  273,  274,  275,   -1,   -1,   -1,
   -1,  280,  281,  282,  283,  284,  285,  286,  123,   -1,
   -1,  126,  291,  292,  293,  294,   -1,  296,  297,   -1,
  299,   -1,  259,   -1,   -1,  262,  263,  264,  265,  266,
  267,   -1,   -1,  270,  271,  272,  273,  274,  275,   -1,
   -1,   -1,   -1,  280,  281,  282,  283,  284,  285,  286,
   -1,   -1,   -1,   -1,  291,  292,  293,  294,   -1,  296,
  297,   -1,  299,   -1,   -1,   -1,  259,   -1,   -1,  262,
  263,  264,  265,  266,   -1,   -1,   -1,  270,  271,  272,
  273,  274,  275,   -1,   -1,   -1,   -1,  280,  281,  282,
  283,  284,  285,  286,   -1,   -1,   -1,   -1,  291,  292,
  293,  294,   -1,  296,  297,  259,  299,   -1,  262,  263,
  264,  265,  266,   -1,   -1,   -1,  270,  271,  272,  273,
  274,  275,   -1,   -1,   -1,   -1,  280,  281,  282,  283,
  284,  285,  286,   -1,   -1,   -1,   -1,   -1,   -1,  293,
  294,   -1,  296,  297,   -1,  299,   -1,  262,  263,  264,
  265,  266,   -1,   -1,   -1,  270,  271,  272,  273,  274,
  275,   -1,   -1,   -1,   -1,  280,  281,  282,  283,  284,
  285,  286,   -1,   -1,   -1,   -1,   -1,   -1,  293,  294,
   -1,  296,  297,   -1,  299,
};
#define YYFINAL 31
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 314
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,"'\\t'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'^'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'{'",0,"'}'","'~'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"OVER",
"SMALLOVER","SQRT","SUB","SUP","LPILE","RPILE","CPILE","PILE","LEFT","RIGHT",
"TO","FROM","SIZE","FONT","ROMAN","BOLD","ITALIC","FAT","ACCENT","BAR","UNDER",
"ABOVE","TEXT","QUOTED_TEXT","FWD","BACK","DOWN","UP","MATRIX","COL","LCOL",
"RCOL","CCOL","MARK","LINEUP","TYPE","VCENTER","PRIME","SPLIT","NOSPLIT",
"UACCENT","SPECIAL","SPACE","GFONT","GSIZE","DEFINE","NDEFINE","TDEFINE",
"SDEFINE","UNDEF","IFDEF","INCLUDE","DELIM","CHARTYPE","SET","GRFONT","GBFONT",
};
char *yyrule[] = {
"$accept : top",
"top :",
"top : equation",
"equation : mark",
"equation : equation mark",
"mark : from_to",
"mark : MARK mark",
"mark : LINEUP mark",
"from_to : sqrt_over",
"from_to : sqrt_over TO from_to",
"from_to : sqrt_over FROM sqrt_over",
"from_to : sqrt_over FROM sqrt_over TO from_to",
"from_to : sqrt_over FROM sqrt_over FROM from_to",
"sqrt_over : script",
"sqrt_over : SQRT sqrt_over",
"sqrt_over : sqrt_over OVER sqrt_over",
"sqrt_over : sqrt_over SMALLOVER sqrt_over",
"script : nonsup",
"script : simple SUP script",
"nonsup : simple",
"nonsup : simple SUB nonsup",
"nonsup : simple SUB simple SUP script",
"simple : TEXT",
"simple : QUOTED_TEXT",
"simple : SPLIT QUOTED_TEXT",
"simple : NOSPLIT TEXT",
"simple : '^'",
"simple : '~'",
"simple : '\\t'",
"simple : '{' equation '}'",
"simple : PILE pile_arg",
"simple : LPILE pile_arg",
"simple : RPILE pile_arg",
"simple : CPILE pile_arg",
"simple : MATRIX '{' column_list '}'",
"simple : LEFT delim equation RIGHT delim",
"simple : LEFT delim equation",
"simple : simple BAR",
"simple : simple UNDER",
"simple : simple PRIME",
"simple : simple ACCENT simple",
"simple : simple UACCENT simple",
"simple : ROMAN simple",
"simple : BOLD simple",
"simple : ITALIC simple",
"simple : FAT simple",
"simple : FONT text simple",
"simple : SIZE text simple",
"simple : FWD number simple",
"simple : BACK number simple",
"simple : UP number simple",
"simple : DOWN number simple",
"simple : TYPE text simple",
"simple : VCENTER simple",
"simple : SPECIAL text simple",
"number : text",
"pile_element_list : equation",
"pile_element_list : pile_element_list ABOVE equation",
"pile_arg : '{' pile_element_list '}'",
"pile_arg : number '{' pile_element_list '}'",
"column_list : column",
"column_list : column_list column",
"column_element_list : equation",
"column_element_list : column_element_list ABOVE equation",
"column_arg : '{' column_element_list '}'",
"column_arg : number '{' column_element_list '}'",
"column : COL column_arg",
"column : LCOL column_arg",
"column : RCOL column_arg",
"column : CCOL column_arg",
"text : TEXT",
"text : QUOTED_TEXT",
"delim : text",
"delim : '{'",
"delim : '}'",
};
#endif
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#ifdef YYSTACKSIZE
#ifndef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#endif
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
#define YYABORT goto yyabort
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
            printf("yydebug: state %d, reading %d (%s)\n", yystate,
                    yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("yydebug: state %d, shifting to state %d\n",
                    yystate, yytable[yyn]);
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
                    printf("yydebug: state %d, error recovery shifting\
 to state %d\n", *yyssp, yytable[yyn]);
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
                    printf("yydebug: error recovery discarding state %d\n",
                            *yyssp);
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
            printf("yydebug: state %d, error recovery discards token %d (%s)\n",
                    yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("yydebug: state %d, reducing by rule %d (%s)\n",
                yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 2:
#line 126 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[0].b->top_level(); non_empty_flag = 1; }
break;
case 3:
#line 131 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = yyvsp[0].b; }
break;
case 4:
#line 133 "/u/jjc/groff/eqn/eqn.y"
{
		  list_box *lb = yyvsp[-1].b->to_list_box();
		  if (!lb)
		    lb = new list_box(yyvsp[-1].b);
		  lb->append(yyvsp[0].b);
		  yyval.b = lb;
		}
break;
case 5:
#line 144 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = yyvsp[0].b; }
break;
case 6:
#line 146 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_mark_box(yyvsp[0].b); }
break;
case 7:
#line 148 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_lineup_box(yyvsp[0].b); }
break;
case 8:
#line 153 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = yyvsp[0].b; }
break;
case 9:
#line 155 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_limit_box(yyvsp[-2].b, 0, yyvsp[0].b); }
break;
case 10:
#line 157 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_limit_box(yyvsp[-2].b, yyvsp[0].b, 0); }
break;
case 11:
#line 159 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_limit_box(yyvsp[-4].b, yyvsp[-2].b, yyvsp[0].b); }
break;
case 12:
#line 161 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_limit_box(yyvsp[-4].b, make_limit_box(yyvsp[-2].b, yyvsp[0].b, 0), 0); }
break;
case 13:
#line 166 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = yyvsp[0].b; }
break;
case 14:
#line 168 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_sqrt_box(yyvsp[0].b); }
break;
case 15:
#line 170 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_over_box(yyvsp[-2].b, yyvsp[0].b); }
break;
case 16:
#line 172 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_small_over_box(yyvsp[-2].b, yyvsp[0].b); }
break;
case 17:
#line 177 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = yyvsp[0].b; }
break;
case 18:
#line 179 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_script_box(yyvsp[-2].b, 0, yyvsp[0].b); }
break;
case 19:
#line 184 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = yyvsp[0].b; }
break;
case 20:
#line 186 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_script_box(yyvsp[-2].b, yyvsp[0].b, 0); }
break;
case 21:
#line 188 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_script_box(yyvsp[-4].b, yyvsp[-2].b, yyvsp[0].b); }
break;
case 22:
#line 193 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = split_text(yyvsp[0].str); }
break;
case 23:
#line 195 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new quoted_text_box(yyvsp[0].str); }
break;
case 24:
#line 197 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = split_text(yyvsp[0].str); }
break;
case 25:
#line 199 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new quoted_text_box(yyvsp[0].str); }
break;
case 26:
#line 201 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new half_space_box; }
break;
case 27:
#line 203 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new space_box; }
break;
case 28:
#line 205 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new tab_box; }
break;
case 29:
#line 207 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = yyvsp[-1].b; }
break;
case 30:
#line 209 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[0].pb->set_alignment(CENTER_ALIGN); yyval.b = yyvsp[0].pb; }
break;
case 31:
#line 211 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[0].pb->set_alignment(LEFT_ALIGN); yyval.b = yyvsp[0].pb; }
break;
case 32:
#line 213 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[0].pb->set_alignment(RIGHT_ALIGN); yyval.b = yyvsp[0].pb; }
break;
case 33:
#line 215 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[0].pb->set_alignment(CENTER_ALIGN); yyval.b = yyvsp[0].pb; }
break;
case 34:
#line 217 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = yyvsp[-1].mb; }
break;
case 35:
#line 219 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_delim_box(yyvsp[-3].str, yyvsp[-2].b, yyvsp[0].str); }
break;
case 36:
#line 221 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_delim_box(yyvsp[-1].str, yyvsp[0].b, 0); }
break;
case 37:
#line 223 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_overline_box(yyvsp[-1].b); }
break;
case 38:
#line 225 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_underline_box(yyvsp[-1].b); }
break;
case 39:
#line 227 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_prime_box(yyvsp[-1].b); }
break;
case 40:
#line 229 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_accent_box(yyvsp[-2].b, yyvsp[0].b); }
break;
case 41:
#line 231 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_uaccent_box(yyvsp[-2].b, yyvsp[0].b); }
break;
case 42:
#line 233 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new font_box(strsave(get_grfont()), yyvsp[0].b); }
break;
case 43:
#line 235 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new font_box(strsave(get_gbfont()), yyvsp[0].b); }
break;
case 44:
#line 237 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new font_box(strsave(get_gfont()), yyvsp[0].b); }
break;
case 45:
#line 239 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new fat_box(yyvsp[0].b); }
break;
case 46:
#line 241 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new font_box(yyvsp[-1].str, yyvsp[0].b); }
break;
case 47:
#line 243 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new size_box(yyvsp[-1].str, yyvsp[0].b); }
break;
case 48:
#line 245 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new hmotion_box(yyvsp[-1].n, yyvsp[0].b); }
break;
case 49:
#line 247 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new hmotion_box(-yyvsp[-1].n, yyvsp[0].b); }
break;
case 50:
#line 249 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new vmotion_box(yyvsp[-1].n, yyvsp[0].b); }
break;
case 51:
#line 251 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new vmotion_box(-yyvsp[-1].n, yyvsp[0].b); }
break;
case 52:
#line 253 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[0].b->set_spacing_type(yyvsp[-1].str); yyval.b = yyvsp[0].b; }
break;
case 53:
#line 255 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = new vcenter_box(yyvsp[0].b); }
break;
case 54:
#line 257 "/u/jjc/groff/eqn/eqn.y"
{ yyval.b = make_special_box(yyvsp[-1].str, yyvsp[0].b); }
break;
case 55:
#line 262 "/u/jjc/groff/eqn/eqn.y"
{
		  int n;
		  if (sscanf(yyvsp[0].str, "%d", &n) == 1)
		    yyval.n = n;
		  a_delete yyvsp[0].str;
		}
break;
case 56:
#line 272 "/u/jjc/groff/eqn/eqn.y"
{ yyval.pb = new pile_box(yyvsp[0].b); }
break;
case 57:
#line 274 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[-2].pb->append(yyvsp[0].b); yyval.pb = yyvsp[-2].pb; }
break;
case 58:
#line 279 "/u/jjc/groff/eqn/eqn.y"
{ yyval.pb = yyvsp[-1].pb; }
break;
case 59:
#line 281 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[-1].pb->set_space(yyvsp[-3].n); yyval.pb = yyvsp[-1].pb; }
break;
case 60:
#line 286 "/u/jjc/groff/eqn/eqn.y"
{ yyval.mb = new matrix_box(yyvsp[0].col); }
break;
case 61:
#line 288 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[-1].mb->append(yyvsp[0].col); yyval.mb = yyvsp[-1].mb; }
break;
case 62:
#line 293 "/u/jjc/groff/eqn/eqn.y"
{ yyval.col = new column(yyvsp[0].b); }
break;
case 63:
#line 295 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[-2].col->append(yyvsp[0].b); yyval.col = yyvsp[-2].col; }
break;
case 64:
#line 300 "/u/jjc/groff/eqn/eqn.y"
{ yyval.col = yyvsp[-1].col; }
break;
case 65:
#line 302 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[-1].col->set_space(yyvsp[-3].n); yyval.col = yyvsp[-1].col; }
break;
case 66:
#line 307 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[0].col->set_alignment(CENTER_ALIGN); yyval.col = yyvsp[0].col; }
break;
case 67:
#line 309 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[0].col->set_alignment(LEFT_ALIGN); yyval.col = yyvsp[0].col; }
break;
case 68:
#line 311 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[0].col->set_alignment(RIGHT_ALIGN); yyval.col = yyvsp[0].col; }
break;
case 69:
#line 313 "/u/jjc/groff/eqn/eqn.y"
{ yyvsp[0].col->set_alignment(CENTER_ALIGN); yyval.col = yyvsp[0].col; }
break;
case 70:
#line 317 "/u/jjc/groff/eqn/eqn.y"
{ yyval.str = yyvsp[0].str; }
break;
case 71:
#line 319 "/u/jjc/groff/eqn/eqn.y"
{ yyval.str = yyvsp[0].str; }
break;
case 72:
#line 324 "/u/jjc/groff/eqn/eqn.y"
{ yyval.str = yyvsp[0].str; }
break;
case 73:
#line 326 "/u/jjc/groff/eqn/eqn.y"
{ yyval.str = strsave("{"); }
break;
case 74:
#line 328 "/u/jjc/groff/eqn/eqn.y"
{ yyval.str = strsave("}"); }
break;
#line 1107 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("yydebug: after reduction, shifting from state 0 to\
 state %d\n", YYFINAL);
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
                printf("yydebug: state %d, reading %d (%s)\n",
                        YYFINAL, yychar, yys);
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
        printf("yydebug: after reduction, shifting from state %d \
to state %d\n", *yyssp, yystate);
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
