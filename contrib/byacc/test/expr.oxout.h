/* testing only */

typedef int yyyWAT;
typedef int yyyWST;
typedef int yyyFT;

typedef struct yyyLexemes {
    	char * lexeme;
} yyyLexemes;

typedef struct yyyAttribs {
    	yyyLexemes yyyAttrb1;
} yyyAttribs;

typedef struct yyyParent {
	struct yyyGNT *	noderef;
	struct yyyStackItem * stackref;
} yyyParent;

typedef struct yyyGNT {
	int *	refCountList;
	int	refCountListLen;
	struct yyyParent parent;
	int	parentIsStack;
	int	prodNum;
	int	whichSym;
	struct yyyGNT ** cL;
	int	cLlen;
	yyyAttribs yyyAttrbs;
} yyyGNT;

typedef int yyyRCT;

typedef struct yyyStackItem {
	int		wa;
	int		whichSym;
	yyyGNT *	node;
	long		solvedSAlist;
} yyySIT;

#define yyyRSitem yyySIT

yyyRSitem *yyyRSTop;
yyyRSitem *yyyAfterRS;
yyyRSitem *yyyRS; 

#undef yyparse
#undef yylex
#undef yyerror
#undef yychar
#undef yyval
#undef yylval
#undef yydebug
#undef yynerrs
#undef yyerrflag
#undef yylhs
#undef yylen
#undef yydefred
#undef yystos
#undef yydgoto
#undef yysindex
#undef yyrindex
#undef yygindex
#undef yytable
#undef yycheck
#undef yyname
#undef yyrule
#undef yycindex
#undef yyctable

struct {
    int test_yycheck  [256];
    int test_yydefred [256];
    int test_yydgoto  [256];
    int test_yygindex [256];
    int test_yylen    [256];
    int test_yylhs    [256];
    int test_yyrindex [256];
    int test_yysindex [256];
    int test_yytable  [256];
#define yycheck  test_expr.test_yycheck
#define yydefred test_expr.test_yydefred
#define yydgoto  test_expr.test_yydgoto
#define yygindex test_expr.test_yygindex
#define yylen    test_expr.test_yylen
#define yylhs    test_expr.test_yylhs
#define yyrindex test_expr.test_yyrindex
#define yysindex test_expr.test_yysindex
#define yytable  test_expr.test_yytable
} test_expr;
