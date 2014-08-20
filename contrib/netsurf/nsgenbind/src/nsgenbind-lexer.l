%{

/* lexer for the binding generation config file 
 *
 * This file is part of nsgenbind.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "nsgenbind-parser.h"
#include "nsgenbind-ast.h"

#define YY_USER_ACTION yylloc->first_line = yylloc->last_line; \
    yylloc->first_column = yylloc->last_column + 1; \
    yylloc->last_column += yyleng;


/* Ensure compatability with bison 2.6 and later */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED && defined NSGENBIND_STYPE_IS_DECLARED
#define YYSTYPE NSGENBIND_STYPE
#endif

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED && defined NSGENBIND_LTYPE_IS_DECLARED
#define YYLTYPE NSGENBIND_LTYPE
#endif

%}

/* lexer options */
%option never-interactive
%option yylineno
%option bison-bridge
%option bison-locations
%option nodefault
%option warn
%option prefix="nsgenbind_"
%option nounput
%option noinput
%option noyywrap

/* other Unicode “space separator” */
USP (\xe1\x9a\x80)|(\xe1\xa0\x8e)|(\xe2\x80[\x80-\x8a])|(\xe2\x80\xaf)|(\xe2\x81\x9f)|(\xe3\x80\x80)

/* non breaking space \u00A0 */
NBSP (\xc2\xa0)

/* Line separator \u2028 */
LS (\xe2\x80\xa8)

/* paragraph separator \u2029 */
PS (\xe2\x80\xa9)

whitespace              ([ \t\v\f]|{NBSP}|{USP})

lineend                 ([\n\r]|{LS}|{PS})

multicomment            \/\*(([^*])|(\*[^/]))*\*\/

quotedstring            [^\"\\\n\r]

identifier              [A-Z_a-z][0-9A-Z_a-z]*

other                   [^\t\n\r 0-9A-Z_a-z]

cblockopen              \%\{

cblockclose             \%\}

/* used for #include directive */
poundsign           ^{whitespace}*#

%x                      cblock

%x                      incl

%%

{whitespace}            ++yylloc->last_column;/* nothing */

{lineend}               if (yytext[0] != '\r') {
                            /* update position counts */
                            ++yylloc->last_line;
                            yylloc->last_column = 0;
                        }

 /* terminals */

webidlfile              return TOK_IDLFILE;

hdrcomment              return TOK_HDR_COMMENT;

preamble                return TOK_PREAMBLE;

prologue                return TOK_PROLOGUE;

epilogue                return TOK_EPILOGUE;

binding                 return TOK_BINDING;

interface               return TOK_INTERFACE;

type                    return TOK_TYPE;

private                 return TOK_PRIVATE;

internal                return TOK_INTERNAL;

unshared                return TOK_UNSHARED;

shared                  return TOK_SHARED;

property                return TOK_PROPERTY;

operation               return TOK_OPERATION;

api                     return TOK_API;

getter                  return TOK_GETTER;

setter                  return TOK_SETTER;

{cblockopen}            BEGIN(cblock);

{identifier}        {
                        /* A leading "_" is used to escape an identifier from 
                         *   looking like a reserved word terminal.
                         */
                        yylval->text = (yytext[0] == '_') ? strdup(yytext + 1) : strdup(yytext);
                        return TOK_IDENTIFIER;
                    }

\"{quotedstring}*\"     yylval->text = strndup(yytext + 1, yyleng - 2 ); return TOK_STRING_LITERAL;

{multicomment}          /* nothing */

{poundsign}include  BEGIN(incl);

{other}                 return (int) yytext[0];

.                       /* nothing */

<cblock>[^\%]*          yylval->text = strdup(yytext); return TOK_CCODE_LITERAL;
<cblock>{cblockclose}   BEGIN(INITIAL);
<cblock>\%              yylval->text = strdup(yytext); return TOK_CCODE_LITERAL;


<incl>[ \t]*\"      /* eat the whitespace and open quotes */

<incl>[^\t\n\"]+    { 
                        /* got the include file name */
                        yyin = genbindopen(yytext);
     
                        if (! yyin) {
                            fprintf(stderr, "Unable to open include %s\n", yytext);
                            exit(3);
                        }
     			yypush_buffer_state(yy_create_buffer( yyin, YY_BUF_SIZE ));
     
                        BEGIN(INITIAL);
                    }

<incl>\n                BEGIN(INITIAL);

<incl>.                 /* nothing */

<<EOF>>             {
     			yypop_buffer_state();
     
                        if ( !YY_CURRENT_BUFFER ) {
                            yyterminate();
                        } else {
                            BEGIN(incl);
                        }

                    }

%%
