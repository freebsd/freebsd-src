#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING (yyerrflag!=0)
#define YYPREFIX "yy"
#line 2 "parser.y"
/*-
 * Copyright (c) 1995
 *	Paul Richards.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer, 
 *    verbatim and that no modifications are made prior to this 
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Paul Richards.
 * 4. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <forms.h>
#include <err.h>

#include "internal.h"

char *cpstr(char *);

extern int yyleng;
int lineno = 1;
int charno = 1;
int off;

char *fieldname;
char *defname;
char *formname;
char *startname;
char *colortable;
int formattr;
char *text;
char *label;
char *function;
char *up, *down, *left, *right, *next;
int height, width;
int y, x;
int width;
int limit;
int attr;
int selattr;
int type;
int lbl_flag;
int selected, no_options=0;

extern FILE *outf;

struct MenuList {
	char *option;
	struct MenuList *next;
};

struct MenuList *cur_menu;
struct MenuList *menu_list;
struct MenuList *menu;

struct pair_node {
	char *foreground;
	char *background;
	struct pair_node *next;
};
struct pair_node *pair_list;
struct pair_node *cur_pair;
struct pair_node *pair;

struct color_table {
	char *tablename;
	struct pair_node *pairs;
	struct color_table *next;
};

struct color_table *color_table;
struct color_table *cur_table;
struct color_table *color_tables;

struct Form *form;
struct Field *field_inst_list;
struct Field *field;
struct Field *cur_field;
#line 106 "parser.y"
typedef union {
	int ival;
	char *sval;
} YYSTYPE;
#line 120 "y.tab.c"
#define FORM 257
#define COLORTABLE 258
#define COLOR 259
#define BLACK 260
#define RED 261
#define GREEN 262
#define YELLOW 263
#define BLUE 264
#define MAGENTA 265
#define CYAN 266
#define WHITE 267
#define PAIR 268
#define NAME 269
#define STRING 270
#define AT 271
#define AS 272
#define HEIGHT 273
#define EQUALS 274
#define NUMBER 275
#define WIDTH 276
#define STARTFIELD 277
#define COMMA 278
#define LBRACE 279
#define RBRACE 280
#define TEXT 281
#define ATTR 282
#define SELATTR 283
#define DEFAULT 284
#define LABEL 285
#define LIMIT 286
#define SELECTED 287
#define OPTIONS 288
#define ACTION 289
#define FUNC 290
#define LINK 291
#define UP 292
#define DOWN 293
#define LEFT 294
#define RIGHT 295
#define NEXT 296
#define DEF 297
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    0,    0,    0,    5,    4,    6,    6,    8,    7,
    1,    1,    1,    1,    1,    1,    1,    1,    9,   11,
    3,   12,   15,   15,   16,   16,   17,   17,   18,   18,
   21,   23,   19,   25,    2,   20,   26,   20,   20,   24,
   22,   22,   30,   30,   30,   30,   30,   29,   29,   29,
   29,   31,   32,   35,   35,   36,   36,   33,   37,   37,
   38,   34,   13,   13,   14,   14,   27,   27,   28,   28,
   10,
};
short yylen[] = {                                         2,
    0,    2,    2,    2,    0,    6,    0,    2,    0,    6,
    1,    1,    1,    1,    1,    1,    1,    1,    0,    0,
    9,    6,    0,    3,    0,    3,    0,    3,    0,    2,
    0,    0,    7,    0,    3,    0,    0,    4,    1,    7,
    0,    3,    3,    3,    3,    3,    3,    1,    1,    1,
    1,    3,    1,    4,    4,    0,    3,    6,    1,    3,
    1,    6,    0,    3,    0,    3,    0,    3,    0,    3,
    3,
};
short yydefred[] = {                                      1,
    0,    0,    0,   34,    2,    3,    4,   19,    5,    0,
    0,    0,    0,   35,    0,    7,    0,    0,    0,   20,
    0,    0,    0,    0,    0,    0,    0,    6,    8,   64,
    0,    0,    0,   71,    0,    0,   66,    0,    0,    0,
    0,    0,   11,   12,   13,   14,   15,   16,   17,   18,
    9,   68,    0,    0,    0,    0,    0,    0,    0,   48,
   49,   50,   51,   53,   21,    0,    0,   70,    0,    0,
    0,    0,    0,   40,    0,    0,    0,   52,    0,    0,
    0,    0,    0,    0,    0,   10,    0,   55,   54,    0,
    0,   24,    0,    0,   29,    0,    0,    0,   26,    0,
    0,   57,   61,    0,   59,   62,   28,   31,   30,    0,
    0,   60,    0,    0,   39,   37,    0,    0,   32,   38,
   41,    0,    0,    0,    0,    0,    0,    0,   42,    0,
    0,    0,    0,    0,   43,   44,   45,   46,   47,
};
short yydgoto[] = {                                       1,
   51,    5,    6,    7,   12,   21,   29,   67,   11,   20,
   26,   41,   18,   24,   76,   85,   95,  101,  109,  114,
  111,  122,  121,   14,   10,  118,   33,   40,   59,  129,
   60,   61,   62,   63,   64,   88,  104,  105,
};
short yysindex[] = {                                      0,
 -252, -261, -240,    0,    0,    0,    0,    0,    0, -248,
 -234, -237, -225,    0, -236,    0, -219, -214, -212,    0,
 -259, -216, -205, -196, -204, -192, -186,    0,    0,    0,
 -185, -183, -194,    0, -225, -187,    0, -182, -180, -224,
 -188, -214,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0, -179, -177, -176, -175, -174, -173, -178,    0,
    0,    0,    0,    0,    0, -172, -171,    0, -167, -166,
 -164, -165, -162,    0, -163, -149, -187,    0, -191, -191,
 -170, -169, -157, -161, -168,    0, -159,    0,    0, -158,
 -155,    0, -152, -154,    0, -153, -147, -145,    0, -150,
 -143,    0,    0, -151,    0,    0,    0,    0,    0, -147,
 -148,    0, -263, -142,    0,    0, -236, -146,    0,    0,
    0, -141, -211, -144, -139, -138, -136, -135,    0, -137,
 -129, -128, -127, -126,    0,    0,    0,    0,    0,
};
short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0, -249,    0,    0,    0,    0, -238,    0,    0,
    0,    0,    0, -231,    0,    0,    0,    0,    0,    0,
    0,    0, -217,    0, -258,    0,    0,    0,    0,    0,
    0, -257,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0, -254,    0,    0,    0,    0,
    0,    0,    0,    0,    0, -266,    0,    0, -134, -134,
    0,    0,    0,    0, -267,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
 -133,    0,    0, -132,    0,    0,    0,    0,    0,    0,
 -122,    0, -249,    0,    0,    0,    0,    0,    0,    0,
    0, -239,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,
};
short yygindex[] = {                                      0,
   51,    0,    0,    0,    0,    0,    0,    0,    0,   16,
    0,    0,  109,  103,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   39,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   71,    0,   42,
};
#define YYTABLESIZE 152
short yytable[] = {                                      63,
   65,   27,   25,   23,    2,  116,    3,    8,   27,   17,
   63,   65,   27,   25,   23,   25,    4,   63,   63,   65,
   28,   63,   65,   63,   65,   23,   63,   23,    9,   33,
   13,   63,   63,   63,   63,   63,   15,   63,   19,   63,
   33,   16,   65,   65,   65,   65,   65,   17,   65,   67,
   65,   67,   67,   67,   22,   67,   54,   67,   30,   55,
   56,   23,   57,   69,   58,   25,   69,   69,   31,   69,
   34,   69,   43,   44,   45,   46,   47,   48,   49,   50,
  124,  125,  126,  127,  128,   32,   35,   36,   39,   37,
   38,   65,   52,   53,   87,   68,   69,   70,   71,   72,
   73,   74,   78,   79,   75,   80,   77,   82,   84,   81,
   83,   92,   93,   94,   96,   97,   99,   90,   98,  100,
   91,  102,  103,  106,  107,  108,  110,   86,  117,  130,
  113,  135,  119,  120,  131,  132,  123,  133,  134,  136,
  137,  138,  139,   42,   66,   56,   22,   58,   36,  115,
   89,  112,
};
short yycheck[] = {                                     258,
  258,  269,  269,  258,  257,  269,  259,  269,  268,  273,
  269,  269,  280,  280,  269,  282,  269,  276,  277,  277,
  280,  280,  280,  282,  282,  280,  276,  282,  269,  269,
  279,  281,  282,  283,  284,  285,  271,  287,  275,  289,
  280,  279,  281,  282,  283,  284,  285,  273,  287,  281,
  289,  283,  284,  285,  274,  287,  281,  289,  275,  284,
  285,  276,  287,  281,  289,  278,  284,  285,  274,  287,
  275,  289,  260,  261,  262,  263,  264,  265,  266,  267,
  292,  293,  294,  295,  296,  282,  279,  274,  283,  275,
  274,  280,  275,  274,  286,  275,  274,  274,  274,  274,
  274,  280,  270,  270,  277,  270,  278,  270,  258,  275,
  274,  269,  274,  282,  274,  274,  269,  288,  274,  274,
  290,  275,  270,  269,  275,  269,  278,   77,  271,  274,
  279,  269,  117,  280,  274,  274,  278,  274,  274,  269,
  269,  269,  269,   35,   42,  280,  280,  280,  271,  111,
   80,  110,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 297
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"FORM","COLORTABLE","COLOR",
"BLACK","RED","GREEN","YELLOW","BLUE","MAGENTA","CYAN","WHITE","PAIR","NAME",
"STRING","AT","AS","HEIGHT","EQUALS","NUMBER","WIDTH","STARTFIELD","COMMA",
"LBRACE","RBRACE","TEXT","ATTR","SELATTR","DEFAULT","LABEL","LIMIT","SELECTED",
"OPTIONS","ACTION","FUNC","LINK","UP","DOWN","LEFT","RIGHT","NEXT","DEF",
};
char *yyrule[] = {
"$accept : spec",
"spec :",
"spec : spec fields",
"spec : spec forms",
"spec : spec colours",
"$$1 :",
"colours : COLOR NAME $$1 LBRACE color_pairs RBRACE",
"color_pairs :",
"color_pairs : color_pairs pair",
"$$2 :",
"pair : PAIR EQUALS a_color $$2 COMMA a_color",
"a_color : BLACK",
"a_color : RED",
"a_color : GREEN",
"a_color : YELLOW",
"a_color : BLUE",
"a_color : MAGENTA",
"a_color : CYAN",
"a_color : WHITE",
"$$3 :",
"$$4 :",
"forms : FORM NAME $$3 AT coord $$4 LBRACE formspec RBRACE",
"formspec : height width startfield colortable formattr fieldlocs",
"startfield :",
"startfield : STARTFIELD EQUALS NAME",
"colortable :",
"colortable : COLORTABLE EQUALS NAME",
"formattr :",
"formattr : ATTR EQUALS NUMBER",
"fieldlocs :",
"fieldlocs : fieldlocs field_at",
"$$5 :",
"$$6 :",
"field_at : NAME $$5 field_def AT coord $$6 links",
"$$7 :",
"fields : NAME $$7 field_spec",
"field_def :",
"$$8 :",
"field_def : LBRACE NAME $$8 RBRACE",
"field_def : field_spec",
"field_spec : LBRACE height width attr selattr type RBRACE",
"links :",
"links : links COMMA conns",
"conns : UP EQUALS NAME",
"conns : DOWN EQUALS NAME",
"conns : LEFT EQUALS NAME",
"conns : RIGHT EQUALS NAME",
"conns : NEXT EQUALS NAME",
"type : textfield",
"type : inputfield",
"type : menufield",
"type : actionfield",
"textfield : TEXT EQUALS STRING",
"inputfield : inputspec",
"inputspec : LABEL EQUALS STRING limit",
"inputspec : DEFAULT EQUALS STRING limit",
"limit :",
"limit : LIMIT EQUALS NUMBER",
"menufield : SELECTED EQUALS NUMBER OPTIONS EQUALS menuoptions",
"menuoptions : menuoption",
"menuoptions : menuoptions COMMA menuoption",
"menuoption : STRING",
"actionfield : ACTION EQUALS STRING FUNC EQUALS NAME",
"height :",
"height : HEIGHT EQUALS NUMBER",
"width :",
"width : WIDTH EQUALS NUMBER",
"attr :",
"attr : ATTR EQUALS NUMBER",
"selattr :",
"selattr : SELATTR EQUALS NUMBER",
"coord : NUMBER COMMA NUMBER",
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
#line 434 "parser.y"

void
yyerror (char *error)
{
	fprintf(stderr, "%s at line %d\n",error, lineno);
	exit(1);
}

char *
cpstr(char *ostr)
{
	char *nstr;

	nstr = malloc(strlen(ostr)+1);
	if (!nstr) {
		fprintf(stderr, "Couldn't allocate memory for string\n");
		exit(1);
	}
	strcpy(nstr, ostr);
	return (nstr);
}

/* Calculate a default height for a field */

void
calc_field_height(struct Field *field, char *string)
{

	int len;

	len = strlen(string);

	if (!field->width) {
		/*
		 * This is a failsafe, this routine shouldn't be called
		 * with a width of 0, the width should be determined
		 * first.
		 */
		field->height = 1;
		return;
	}

	if (len < field->width) {
		field->height = 1;
		return;
	} else
		field->height = len / field->width;

	if ((field->height*field->width) < len)
		field->height++;

	return;
}

void
define_field(char *defname)
{
	struct Field *field;
	struct MenuList *menu_options;
	int no_options;

	field = malloc(sizeof (struct Field));
	if (!field) {
		fprintf(stderr,"Failed to allocate memory for form field\n");
		exit(1);
	}
	field->defname = defname;
	field->type = type;
	field->height = height;
	field->width = width;
	field->attr = attr;
	field->selattr = selattr;
	switch (type) {
		case FF_TEXT:
			field->field.text = malloc(sizeof (struct TextField));
			if (!field->field.text) {
				fprintf(stderr,
						"Failed to allocate memory for text field\n");
				exit (1);
			}
			field->field.text->text = text;
			break;
		case FF_INPUT:
			field->field.input = malloc(sizeof (struct InputField));
			if (!field->field.input) {
				fprintf(stderr,
						"Failed to allocate memory for input field\n");
				exit (1);
			}
			field->field.input->lbl_flag = lbl_flag;
			field->field.input->label = label;
			field->field.input->limit = limit;
			break;
		case FF_MENU:
			printf("field type %s = %d\n", defname,field->type);
			field->field.menu = malloc(sizeof (struct MenuField));
			if (!field->field.menu) {
				fprintf(stderr,
						"Failed to allocate memory for menu field\n");
				exit (1);
			}
			field->field.menu->selected = selected;
			menu_options = menu_list;
			field->field.menu->no_options = 0;
			field->field.menu->options = 0;
			for (; menu_options; menu_options = menu_options->next) {
				no_options = add_menu_option(field->field.menu,
											 menu_options->option);
				if (!no_options)
					err(1, "Couldn't add menu option");
			}
			field->field.menu->no_options = no_options;
			cur_menu = 0;
			break;
		case FF_ACTION:
			field->field.action = malloc(sizeof (struct ActionField));
			if (!field->field.action) {
				fprintf(stderr,
						"Failed to allocate memory for action field\n");
				exit (1);
			}
			field->field.action->text = text;
			field->field.action->fn = (void *) function;
			break;
		default:
			break;
	}
	form_bind_tuple(defname, FT_FIELD_DEF, field);
	width=0;
	height = 0;
	attr=0;
	selattr=0;
	limit=0;
}
#line 531 "y.tab.c"
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
case 5:
#line 166 "parser.y"
{
			color_table = malloc(sizeof (struct color_table));
			if (!color_table) {
				fprintf(stderr, "Couldn't allocate memory for a color table\n");
				exit (1);
			}
			color_table->tablename = cpstr(yyvsp[0].sval);
		}
break;
case 6:
#line 175 "parser.y"
{
			color_table->pairs = pair_list;
			cur_pair = 0;
			form_bind_tuple(color_table->tablename, FT_COLTAB, color_table);
		}
break;
case 9:
#line 187 "parser.y"
{
			pair = malloc(sizeof (struct pair_node));
			if (!pair) {
				fprintf(stderr, "Couldn't allocate memory for a color pair\n");
				exit(1);
			}
			pair->foreground = cpstr(yyvsp[0].sval);
		}
break;
case 10:
#line 196 "parser.y"
{
			pair->background = cpstr(yyvsp[0].sval);
			if (!cur_pair) {
				pair_list = pair;
				cur_pair = pair;
			} else {
				cur_pair->next = pair;
				cur_pair = pair;
			}
		}
break;
case 11:
#line 209 "parser.y"
{ yyval.sval = "COLOR_BLACK"; }
break;
case 12:
#line 211 "parser.y"
{ yyval.sval = "COLOR_RED"; }
break;
case 13:
#line 213 "parser.y"
{ yyval.sval = "COLOR_GREEN"; }
break;
case 14:
#line 215 "parser.y"
{ yyval.sval = "COLOR_YELLOW"; }
break;
case 15:
#line 217 "parser.y"
{ yyval.sval = "COLOR_BLUE"; }
break;
case 16:
#line 219 "parser.y"
{ yyval.sval = "COLOR_MAGENTA"; }
break;
case 17:
#line 221 "parser.y"
{ yyval.sval = "COLOR_CYAN"; }
break;
case 18:
#line 223 "parser.y"
{ yyval.sval = "COLOR_WHITE"; }
break;
case 19:
#line 227 "parser.y"
{ formname = cpstr(yyvsp[0].sval); }
break;
case 20:
#line 229 "parser.y"
{
			form = malloc(sizeof (struct Form));
			if (!form) {
				fprintf(stderr,"Failed to allocate memory for form\n");
				exit(1);
			}
			form->y = y;
			form->x = x;
		}
break;
case 21:
#line 239 "parser.y"
{
			form->startfield = startname;
			form->colortable = colortable;
			form->height = height;
			form->width = width;
			form->attr = formattr;
			form->fieldlist = field_inst_list;
			field_inst_list = 0;
			form_bind_tuple(formname, FT_FORM, form);
		}
break;
case 23:
#line 255 "parser.y"
{	startname = 0; 
			printf("Warning: No start field specified for form %s\n", formname);
		}
break;
case 24:
#line 259 "parser.y"
{ startname = cpstr(yyvsp[0].sval); }
break;
case 25:
#line 263 "parser.y"
{ colortable = 0; }
break;
case 26:
#line 265 "parser.y"
{ colortable = cpstr(yyvsp[0].sval); }
break;
case 27:
#line 269 "parser.y"
{ formattr = 0; }
break;
case 28:
#line 271 "parser.y"
{ formattr = yyvsp[0].ival; }
break;
case 31:
#line 279 "parser.y"
{ fieldname = cpstr(yyvsp[0].sval); }
break;
case 32:
#line 281 "parser.y"
{ 
			field = malloc(sizeof (struct Field));
			if (!field) {
				fprintf(stderr,"Failed to allocate memory for form field\n");
				exit(1);
			}
			if (!defname)
				field->defname = fieldname;
			else
				field->defname = defname;
			field->y = y;
			field->x = x;
		}
break;
case 33:
#line 295 "parser.y"
{
			field->fup = up;
			field->fdown = down;
			field->fleft = left;
			field->fright = right;
			field->fnext = next;
			if (!field_inst_list)
				field_inst_list = field;
			up = 0;
			down = 0;
			left = 0;
			right = 0;
			next = 0;
			if (!cur_field)
				cur_field = field;
			else {
				cur_field->next = field;
				cur_field = field;
			}
			form_bind_tuple(fieldname, FT_FIELD_INST, field);
		}
break;
case 34:
#line 319 "parser.y"
{ defname = cpstr(yyvsp[0].sval); }
break;
case 35:
#line 321 "parser.y"
{ define_field(defname); }
break;
case 36:
#line 325 "parser.y"
{ defname = 0; }
break;
case 37:
#line 327 "parser.y"
{ defname = cpstr(yyvsp[0].sval); }
break;
case 39:
#line 330 "parser.y"
{ defname = fieldname; define_field(defname); }
break;
case 43:
#line 341 "parser.y"
{ up = cpstr(yyvsp[0].sval); }
break;
case 44:
#line 343 "parser.y"
{ down = cpstr(yyvsp[0].sval); }
break;
case 45:
#line 345 "parser.y"
{ left = cpstr(yyvsp[0].sval); }
break;
case 46:
#line 347 "parser.y"
{ right = cpstr(yyvsp[0].sval); }
break;
case 47:
#line 349 "parser.y"
{ next = cpstr(yyvsp[0].sval); }
break;
case 52:
#line 359 "parser.y"
{ type = FF_TEXT; text = cpstr(yyvsp[0].sval); }
break;
case 53:
#line 363 "parser.y"
{ type = FF_INPUT; }
break;
case 54:
#line 367 "parser.y"
{ lbl_flag = 1; label = cpstr(yyvsp[-1].sval); }
break;
case 55:
#line 369 "parser.y"
{ lbl_flag = 0; label = cpstr(yyvsp[-1].sval); }
break;
case 57:
#line 374 "parser.y"
{ limit = yyvsp[0].ival; }
break;
case 58:
#line 377 "parser.y"
{ type = FF_MENU; selected = yyvsp[-3].ival; }
break;
case 61:
#line 385 "parser.y"
{	
				menu = malloc(sizeof(struct MenuList));
				if (!menu) {
						err(1, "Couldn't allocate memory for menu option\n");
				}
				menu->option = cpstr(yyvsp[0].sval);
				if (!cur_menu) {  
					menu_list = menu;
					cur_menu = menu;
				} else {
					cur_menu->next = menu; 
					cur_menu = menu;
				}
			}
break;
case 62:
#line 402 "parser.y"
{ type = FF_ACTION; text = cpstr(yyvsp[-3].sval); function = cpstr(yyvsp[0].sval); }
break;
case 63:
#line 406 "parser.y"
{ height = 0; }
break;
case 64:
#line 408 "parser.y"
{ height = yyvsp[0].ival; }
break;
case 65:
#line 412 "parser.y"
{ width = 0; }
break;
case 66:
#line 414 "parser.y"
{ width = yyvsp[0].ival; }
break;
case 67:
#line 418 "parser.y"
{ attr = 0; }
break;
case 68:
#line 420 "parser.y"
{ attr = yyvsp[0].ival; }
break;
case 69:
#line 424 "parser.y"
{ selattr = 0; }
break;
case 70:
#line 426 "parser.y"
{ selattr = yyvsp[0].ival; }
break;
case 71:
#line 430 "parser.y"
{ y = yyvsp[-2].ival; x = yyvsp[0].ival; }
break;
#line 967 "y.tab.c"
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
