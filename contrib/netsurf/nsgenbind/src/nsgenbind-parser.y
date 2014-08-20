%{
/* parser for the binding generation config file 
 *
 * This file is part of nsgenbind.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 */

#include <stdio.h>
#include <string.h>

#include "nsgenbind-parser.h"
#include "nsgenbind-lexer.h"
#include "webidl-ast.h"
#include "nsgenbind-ast.h"

char *errtxt;

 static void nsgenbind_error(YYLTYPE *locp, struct genbind_node **genbind_ast, const char *str)
{
    locp = locp;
    genbind_ast = genbind_ast;
    errtxt = strdup(str);
}


%}

%locations
%define api.pure
%error-verbose
%parse-param { struct genbind_node **genbind_ast }

%union
{
    char* text;
    struct genbind_node *node;
    long value;
}

%token TOK_IDLFILE
%token TOK_HDR_COMMENT
%token TOK_PREAMBLE
%token TOK_PROLOGUE;
%token TOK_EPILOGUE;

%token TOK_API
%token TOK_BINDING
%token TOK_OPERATION
%token TOK_GETTER
%token TOK_SETTER
%token TOK_INTERFACE
%token TOK_TYPE
%token TOK_PRIVATE
%token TOK_INTERNAL
%token TOK_UNSHARED
%token TOK_SHARED
%token TOK_PROPERTY

%token <text> TOK_IDENTIFIER
%token <text> TOK_STRING_LITERAL
%token <text> TOK_CCODE_LITERAL

%type <text> CBlock

%type <value> Modifiers
%type <value> Modifier

%type <node> Statement
%type <node> Statements
%type <node> IdlFile
%type <node> Preamble 
%type <node> Prologue
%type <node> Epilogue
%type <node> HdrComment
%type <node> Strings
%type <node> Binding
%type <node> BindingArgs
%type <node> BindingArg
%type <node> Type
%type <node> Private
%type <node> Internal
%type <node> Interface
%type <node> Property
%type <node> Operation
%type <node> Api
%type <node> Getter
%type <node> Setter


%%

Input
        :
        Statements 
        { 
            *genbind_ast = $1; 
        }
        ;
        

Statements
        : 
        Statement 
        | 
        Statements Statement  
        {
          $$ = genbind_node_link($2, $1);
        }
        | 
        error ';' 
        { 
            fprintf(stderr, "%d: %s\n", yylloc.first_line, errtxt);
            free(errtxt);
            YYABORT ;
        }
        ;

Statement
        : 
        IdlFile
        | 
        HdrComment
        |
        Preamble
        |
        Prologue
        |
        Epilogue
        |
        Binding
        |
        Operation
        |
        Api
        |
        Getter
        |
        Setter
        ;

 /* [3] load a web IDL file */
IdlFile
        : 
        TOK_IDLFILE TOK_STRING_LITERAL ';'
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_WEBIDLFILE, NULL, $2);
        }
        ;

HdrComment
        : 
        TOK_HDR_COMMENT Strings ';'
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_HDRCOMMENT, NULL, $2);
        }
        ;

Strings
        : 
        TOK_STRING_LITERAL
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_STRING, NULL, $1);
        }
        | 
        Strings TOK_STRING_LITERAL 
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_STRING, $1, $2);
        }
        ;

Preamble
        :
        TOK_PREAMBLE CBlock
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_PREAMBLE, NULL, $2);
        }
        ;

Prologue
        :
        TOK_PROLOGUE CBlock
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_PROLOGUE, NULL, $2);
        }
        ;

Epilogue
        :
        TOK_EPILOGUE CBlock
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_EPILOGUE, NULL, $2);
        }
        ;

CBlock
        : 
        TOK_CCODE_LITERAL
        | 
        CBlock TOK_CCODE_LITERAL 
        {
          $$ = genbind_strapp($1, $2);
        }
        ;

Operation
        :
        TOK_OPERATION TOK_IDENTIFIER CBlock
        {
            $$ = genbind_new_node(GENBIND_NODE_TYPE_OPERATION, 
                     NULL, 
                     genbind_new_node(GENBIND_NODE_TYPE_IDENT, 
                         genbind_new_node(GENBIND_NODE_TYPE_CBLOCK, 
                                          NULL, 
                                          $3), 
                         $2));
        }

Api
        :
        TOK_API TOK_IDENTIFIER CBlock
        {
            $$ = genbind_new_node(GENBIND_NODE_TYPE_API, 
                     NULL, 
                     genbind_new_node(GENBIND_NODE_TYPE_IDENT, 
                         genbind_new_node(GENBIND_NODE_TYPE_CBLOCK, 
                                          NULL, 
                                          $3), 
                         $2));
        }

Getter
        :
        TOK_GETTER TOK_IDENTIFIER CBlock
        {
            $$ = genbind_new_node(GENBIND_NODE_TYPE_GETTER, 
                     NULL, 
                     genbind_new_node(GENBIND_NODE_TYPE_IDENT, 
                         genbind_new_node(GENBIND_NODE_TYPE_CBLOCK, 
                                          NULL, 
                                          $3), 
                         $2));
        }

Setter
        :
        TOK_SETTER TOK_IDENTIFIER CBlock
        {
            $$ = genbind_new_node(GENBIND_NODE_TYPE_SETTER, 
                     NULL, 
                     genbind_new_node(GENBIND_NODE_TYPE_IDENT, 
                         genbind_new_node(GENBIND_NODE_TYPE_CBLOCK, 
                                          NULL, 
                                          $3), 
                         $2));
        }

Binding
        :
        TOK_BINDING TOK_IDENTIFIER '{' BindingArgs '}' 
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_BINDING, 
                                NULL, 
                                genbind_new_node(GENBIND_NODE_TYPE_IDENT, $4, $2));
        }
        ;

BindingArgs
        :
        BindingArg
        |
        BindingArgs BindingArg
        {
          $$ = genbind_node_link($2, $1);
        }
        ;

BindingArg
        : 
        Type
        | 
        Private
        | 
        Internal
        | 
        Interface
        |
        Property
        ;

Type
        :
        TOK_TYPE TOK_IDENTIFIER ';'
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_TYPE, NULL, $2);
        }
        ;

Private
        :
        TOK_PRIVATE TOK_STRING_LITERAL TOK_IDENTIFIER ';'
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_BINDING_PRIVATE, NULL, 
                 genbind_new_node(GENBIND_NODE_TYPE_IDENT,  
                   genbind_new_node(GENBIND_NODE_TYPE_STRING, NULL, $2), $3));
        }
        ;

Internal
        :
        TOK_INTERNAL TOK_STRING_LITERAL TOK_IDENTIFIER ';'
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_BINDING_INTERNAL, NULL, 
                 genbind_new_node(GENBIND_NODE_TYPE_IDENT,  
                   genbind_new_node(GENBIND_NODE_TYPE_STRING, NULL, $2), $3));
        }
        ;

Interface
        : 
        TOK_INTERFACE TOK_IDENTIFIER ';'
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_BINDING_INTERFACE, NULL, $2);
        }
        ;

Property
        :
        TOK_PROPERTY Modifiers TOK_IDENTIFIER ';'
        {
          $$ = genbind_new_node(GENBIND_NODE_TYPE_BINDING_PROPERTY, 
                                NULL, 
                                genbind_new_node(GENBIND_NODE_TYPE_MODIFIER, 
                                                 genbind_new_node(GENBIND_NODE_TYPE_IDENT, 
                                                                  NULL, 
                                                                  $3),
                                                 (void *)$2));
        }
        ;

Modifiers
        :
        /* empty */
        {
            $$ = GENBIND_TYPE_NONE;
        }
        |
        Modifiers Modifier
        {
            $$ |= $2;
        }
        ;

Modifier
        :
        TOK_TYPE
        {
            $$ = GENBIND_TYPE_TYPE;
        }
        |
        TOK_UNSHARED
        {
            $$ = GENBIND_TYPE_UNSHARED;            
        }
        |
        TOK_SHARED
        {
            $$ = GENBIND_TYPE_NONE;            
        }
        ;

%%
