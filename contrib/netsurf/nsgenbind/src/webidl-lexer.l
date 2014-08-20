%{

/* This is a unicode tolerant lexer for web IDL
 *
 * This file is part of nsgenbind.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 *
 * Derived from:
 *
 * W3C WEB IDL - http://www.w3.org/TR/WebIDL/ (especially the grammar
 * in apendix A)
 *
 * The ECMA script spec -
 * http://ecma-international.org/ecma-262/5.1/#sec-7.2 (expecially
 * section 7.2 for unicode value handling)
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "webidl-parser.h"

#define YY_USER_ACTION yylloc->first_line = yylloc->last_line; \
    yylloc->first_column = yylloc->last_column + 1; \
    yylloc->last_column += yyleng;

/* Ensure compatability with bison 2.6 and later */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED && defined WEBIDL_STYPE_IS_DECLARED
#define YYSTYPE WEBIDL_STYPE
#endif

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED && defined WEBIDL_LTYPE_IS_DECLARED
#define YYLTYPE WEBIDL_LTYPE
#endif

%}


/* lexer options */
%option never-interactive
%option yylineno
%option bison-bridge
%option bison-locations
%option warn
%option prefix="webidl_"
%option nounput
%option noyywrap

/* regular definitions */

 /* ecmascript section 7.2 defines whitespace http://ecma-international.org/ecma-262/5.1/#sec-7.2
  * Web IDL appendix A has the IDL grammar http://www.w3.org/TR/WebIDL/#idl-grammar
  */

 /* we do not define space, line feed, carriage return, tab, vertical
  *  tab or form feed here as they are the standard C escapes
  */

 /* other Unicode “space separator” */
USP (\xe1\x9a\x80)|(\xe1\xa0\x8e)|(\xe2\x80[\x80-\x8a])|(\xe2\x80\xaf)|(\xe2\x81\x9f)|(\xe3\x80\x80)

/* Line separator \u2028 */
LS (\xe2\x80\xa8)

/* paragraph separator \u2029 */
PS (\xe2\x80\xa9)

/* non breaking space \u00A0 */
NBSP (\xc2\xa0)

/* web idl grammar for whitespace matches single and multiline
 *  comments too. Here there are separate definitions for both single
 *  and multiline comments.
 */
whitespace          ([ \t\v\f]|{NBSP}|{USP})
multicomment        \/\*(([^*])|(\*[^/]))*\*\/
singlecomment       \/\/
lineend             ([\n\r]|{LS}|{PS})

/* integer numbers in hexidecimal, decimal and octal, slight extension
 *   to spec which only allows for decimal values 
 */
hexdigit            [0-9A-Fa-f]
hexint              0(x|X){hexdigit}+

decimalint          0|([1-9][0-9]*)

octalint            (0[0-8]+)

/* decimal floating point number */ 
decimalexponent     (e|E)[\+\-]?[0-9]+
decimalfloat        ({decimalint}\.[0-9]*{decimalexponent}?)|(\.[0-9]+{decimalexponent}?)|({decimalint}{decimalexponent}?)

/* quoted string. spec simply has "[^"]*" but here escapes are allowed for */
hexescseq           x{hexdigit}{2}
unicodeescseq       u{hexdigit}{4}
characterescseq     ['\"\\bfnrtv]|[^'\"\\bfnrtv\n\r]
escseq              {characterescseq}|0|{hexescseq}|{unicodeescseq}
quotedstring        ([^\"\\\n\r]|\\{escseq})

/* web idl identifier direct from spec */
identifier          [A-Z_a-z][0-9A-Z_a-z]*

/* web idl other direct from spec */
other               [^\t\n\r 0-9A-Z_a-z]

/* used for #include directive - not part of web idl spec */
poundsign           ^{whitespace}*#

%x                  incl
%%

{whitespace}        ++yylloc->last_column; /* skip whitespace */

{lineend}           if (yytext[0] != '\r') {
                        /* update position counts */
                        ++yylloc->last_line;
                        yylloc->last_column = 0;
                    }

    /* Simple text terminals */

boolean             return TOK_BOOLEAN;

byte                return TOK_BYTE;

octet               return TOK_OCTET; 

attribute           return TOK_ATTRIBUTE;

callback            return TOK_CALLBACK;

const               return TOK_CONST;

creator             return TOK_CREATOR;

deleter             return TOK_DELETER;

dictionary          return TOK_DICTIONARY;

enum                return TOK_ENUM;

exception           return TOK_EXCEPTION;

getter              return TOK_GETTER;

implements          return TOK_IMPLEMENTS;

inherit             return TOK_INHERIT;

interface           return TOK_INTERFACE;

legacycaller        return TOK_LEGACYCALLER;

partial             return TOK_PARTIAL;

setter              return TOK_SETTER;

static              return TOK_STATIC;

stringifier         return TOK_STRINGIFIER;

typedef             return TOK_TYPEDEF;

unrestricted        return TOK_UNRESTRICTED;

"..."               return TOK_ELLIPSIS;

Date                return TOK_DATE;

DOMString           return TOK_STRING; /* dom strings are just strings */

Infinity            return TOK_INFINITY;

NaN                 return TOK_NAN;

any                 return TOK_ANY;

double              return TOK_DOUBLE;

false               return TOK_FALSE;

float               return TOK_FLOAT;

long                return TOK_LONG;

null                return TOK_NULL_LITERAL;

object              yylval->text = strdup(yytext); return TOK_IDENTIFIER;

or                  return TOK_OR;

optional            return TOK_OPTIONAL;

sequence            return TOK_SEQUENCE;

short               return TOK_SHORT;

true                return TOK_TRUE;

unsigned            return TOK_UNSIGNED;

void                return TOK_VOID;

readonly            return TOK_READONLY;


{identifier}        {
                        /* A leading "_" is used to escape an identifier from 
                         *   looking like a reserved word terminal. */
                        yylval->text = (yytext[0] == '_') ? strdup(yytext + 1) : strdup(yytext);
                        return TOK_IDENTIFIER;
                    }

{decimalint}        yylval->value = strtol(yytext, NULL, 10); return TOK_INT_LITERAL;

{octalint}          yylval->value = strtol(yytext, NULL, 8); return TOK_INT_LITERAL;

{hexint}            yylval->value = strtol(yytext, NULL, 16); return TOK_INT_LITERAL;

{decimalfloat}      yylval->text = strdup(yytext); return TOK_FLOAT_LITERAL;

\"{quotedstring}*\" yylval->text = strdup(yytext); return TOK_STRING_LITERAL;

{multicomment}      {
                        /* multicomment */
                        char* s = yytext;
                        for (; *s; ++s)
                        {
                            if (*s == '\n')
                            {
                                ++yylloc->last_line;
                                yylloc->last_column = 0;
                            }
                            else
                            {
                                ++yylloc->last_column;
                            }
                        }
                        if (strncmp(yytext, "/**", 3) == 0)
                        {
                            /* Javadoc style comment */
                            yylval->text = strdup(yytext);
                            return TOK_JAVADOC;
                        }
                    }

{singlecomment}     {
                        /* singlecomment */
                        int c;

                        do {
                            c = input();
                        } while (c != '\n' && c != '\r' && c != EOF);
                        ++yylloc->last_line;
                        yylloc->last_column = 0;
                    }



{poundsign}include  BEGIN(incl);

{other}             return (int) yytext[0];
      
<incl>[ \t]*\"      /* eat the whitespace and open quotes */

<incl>[^\t\n\"]+    { 
                        /* got the include file name */
                        yyin = fopen( yytext, "r" );
     
                        if ( ! yyin ) {
                            fprintf(stderr, "Unable to open include %s\n", yytext);
                            exit(3);
                        }
     			yypush_buffer_state(yy_create_buffer( yyin, YY_BUF_SIZE ));
     
                        BEGIN(INITIAL);
                    }

<incl>\n                BEGIN(INITIAL);

<<EOF>>             {
     			yypop_buffer_state();
     
                        if ( !YY_CURRENT_BUFFER ) {
                            yyterminate();
                        } else {
                            BEGIN(incl);
                        }

                    }


%%
