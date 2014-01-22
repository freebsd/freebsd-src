
#ifndef yyparse
#define yyparse    error_parse
#endif /* yyparse */

#ifndef yylex
#define yylex      error_lex
#endif /* yylex */

#ifndef yyerror
#define yyerror    error_error
#endif /* yyerror */

#ifndef yychar
#define yychar     error_char
#endif /* yychar */

#ifndef yyval
#define yyval      error_val
#endif /* yyval */

#ifndef yylval
#define yylval     error_lval
#endif /* yylval */

#ifndef yydebug
#define yydebug    error_debug
#endif /* yydebug */

#ifndef yynerrs
#define yynerrs    error_nerrs
#endif /* yynerrs */

#ifndef yyerrflag
#define yyerrflag  error_errflag
#endif /* yyerrflag */

#ifndef yylhs
#define yylhs      error_lhs
#endif /* yylhs */

#ifndef yylen
#define yylen      error_len
#endif /* yylen */

#ifndef yydefred
#define yydefred   error_defred
#endif /* yydefred */

#ifndef yydgoto
#define yydgoto    error_dgoto
#endif /* yydgoto */

#ifndef yysindex
#define yysindex   error_sindex
#endif /* yysindex */

#ifndef yyrindex
#define yyrindex   error_rindex
#endif /* yyrindex */

#ifndef yygindex
#define yygindex   error_gindex
#endif /* yygindex */

#ifndef yytable
#define yytable    error_table
#endif /* yytable */

#ifndef yycheck
#define yycheck    error_check
#endif /* yycheck */

#ifndef yyname
#define yyname     error_name
#endif /* yyname */

#ifndef yyrule
#define yyrule     error_rule
#endif /* yyrule */
#define YYPREFIX "error_"
const short error_lhs[] = {                       -1,
    0,
};
const short error_len[] = {                        2,
    1,
};
const short error_defred[] = {                     0,
    1,    0,
};
const short error_dgoto[] = {                      2,
};
const short error_sindex[] = {                  -256,
    0,    0,
};
const short error_rindex[] = {                     0,
    0,    0,
};
const short error_gindex[] = {                     0,
};
const short error_table[] = {                      1,
};
const short error_check[] = {                    256,
};
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#if YYDEBUG
const char *yyname[] = {

"end-of-file","illegal-symbol",
};
const char *yyrule[] = {
"$accept : S",
"S : error",

};
#endif
