%{

/* This is a bison parser for Web IDL
 *
 * This file is part of nsgenbind.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 *
 * Derived from the the grammar in apendix A of W3C WEB IDL
 *   http://www.w3.org/TR/WebIDL/
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "webidl-ast.h"

#include "webidl-parser.h"
#include "webidl-lexer.h"

char *errtxt;

static void
webidl_error(YYLTYPE *locp, struct webidl_node **winbind_ast, const char *str)
{
  locp = locp;
  winbind_ast = winbind_ast;
    errtxt = strdup(str);
}

%}

%locations
%define api.pure
%error-verbose
%parse-param { struct webidl_node **webidl_ast }

%union
{
    int attr;
    long value;
    bool isit;
    char* text;
    struct webidl_node *node;
}


%token TOK_ANY
%token TOK_ATTRIBUTE
%token TOK_BOOLEAN
%token TOK_BYTE
%token TOK_CALLBACK
%token TOK_LEGACYCALLER
%token TOK_CONST
%token TOK_CREATOR
%token TOK_DATE
%token TOK_DELETER
%token TOK_DICTIONARY
%token TOK_DOUBLE
%token TOK_ELLIPSIS
%token TOK_ENUM
%token TOK_EOL
%token TOK_EXCEPTION
%token TOK_FALSE
%token TOK_FLOAT
%token TOK_GETRAISES
%token TOK_GETTER
%token TOK_IMPLEMENTS
%token TOK_IN
%token TOK_INFINITY
%token TOK_INHERIT
%token TOK_INTERFACE
%token TOK_LONG
%token TOK_MODULE
%token TOK_NAN
%token TOK_NATIVE
%token TOK_NULL_LITERAL
%token TOK_OBJECT
%token TOK_OCTET
%token TOK_OMITTABLE
%token TOK_OPTIONAL
%token TOK_OR
%token TOK_PARTIAL
%token TOK_RAISES
%token TOK_READONLY
%token TOK_SETRAISES
%token TOK_SETTER
%token TOK_SEQUENCE
%token TOK_SHORT
%token TOK_STATIC
%token TOK_STRING
%token TOK_STRINGIFIER
%token TOK_TRUE
%token TOK_TYPEDEF
%token TOK_UNRESTRICTED
%token TOK_UNSIGNED
%token TOK_VOID

%token TOK_POUND_SIGN

%token <text>       TOK_IDENTIFIER
%token <value>      TOK_INT_LITERAL
%token <text>       TOK_FLOAT_LITERAL
%token <text>       TOK_STRING_LITERAL
%token <text>       TOK_OTHER_LITERAL
%token <text>       TOK_JAVADOC

%type <text> Inheritance

%type <node> Definitions
%type <node> Definition

%type <node> Partial
%type <node> PartialDefinition

%type <node> Dictionary
%type <node> PartialDictionary

%type <node> Exception
%type <node> Enum
%type <node> Typedef
%type <node> ImplementsStatement

%type <node> Interface
%type <node> InterfaceMembers
%type <node> InterfaceMember
%type <node> PartialInterface

%type <node> CallbackOrInterface
%type <node> CallbackRest
%type <node> CallbackRestOrInterface

%type <node> Attribute
%type <node> AttributeOrOperation
%type <node> StringifierAttributeOrOperation
%type <node> Const

%type <node> Operation
%type <node> OperationRest
%type <node> OptionalIdentifier

%type <node> ArgumentList
%type <node> Arguments
%type <node> Argument
%type <node> OptionalOrRequiredArgument
%type <text> ArgumentName
%type <text> ArgumentNameKeyword
%type <node> Ellipsis

%type <node> Type
%type <node> ReturnType
%type <node> SingleType
%type <node> UnionType
%type <node> NonAnyType
%type <node> ConstType
%type <node> PrimitiveType
%type <node> UnrestrictedFloatType
%type <node> FloatType
%type <node> UnsignedIntegerType
%type <node> IntegerType

%type <node> TypeSuffix
%type <node> TypeSuffixStartingWithArray

%type <node> FloatLiteral
%type <node> BooleanLiteral
%type <node> ConstValue

%type <isit> ReadOnly
%type <isit> OptionalLong
%type <isit> Inherit

%type <node> ExtendedAttributeList
%type <node> ExtendedAttributes
%type <node> ExtendedAttributeRest
%type <node> ExtendedAttributeInner
%type <node> ExtendedAttribute
%type <text> Other
%type <text> OtherOrComma

%%

 /* [1] default rule to add built AST to passed in one, altered from
  *   original grammar to be left recusive,
  */
Definitions:
        /* empty */
        {
          $$ = NULL;
        }
        |
        Definitions ExtendedAttributeList Definition
        {
            webidl_node_add($3, $2);
            $$ = *webidl_ast = webidl_node_prepend(*webidl_ast, $3);
        }
        |
        error
        {
            fprintf(stderr, "%d: %s\n", yylloc.first_line, errtxt);
            free(errtxt);
            YYABORT ;
        }
        ;

 /* [2] */
Definition:
        CallbackOrInterface
        |
        Partial
        |
        Dictionary
        |
        Exception
        |
        Enum
        |
        Typedef
        |
        ImplementsStatement
        ;

 /* [3] */
CallbackOrInterface
        :
        TOK_CALLBACK CallbackRestOrInterface
        {
            $$ = $2;
        }
        |
        Interface
        ;

 /* [4] */
CallbackRestOrInterface:
        CallbackRest
        |
        Interface
        ;

 /* [5] */
Interface:
        TOK_INTERFACE TOK_IDENTIFIER Inheritance '{' InterfaceMembers '}' ';'
        {
            /* extend interface with additional members */
            struct webidl_node *interface_node;
            struct webidl_node *members = NULL;

            if ($3 != NULL) {
                members = webidl_node_new(WEBIDL_NODE_TYPE_INTERFACE_INHERITANCE, members, $3);
            }

            members = webidl_node_new(WEBIDL_NODE_TYPE_LIST, members, $5);


            interface_node = webidl_node_find_type_ident(*webidl_ast,
                                                     WEBIDL_NODE_TYPE_INTERFACE,
                                                     $2);

            if (interface_node == NULL) {
                /* no existing interface - create one with ident */
                members = webidl_node_new(WEBIDL_NODE_TYPE_IDENT, members, $2);

                $$ = webidl_node_new(WEBIDL_NODE_TYPE_INTERFACE, NULL, members);
            } else {
                /* update the existing interface */

                /* link member node into interfaces_node */
                webidl_node_add(interface_node, members);

                $$ = NULL; /* updating so no need to add a new node */
            }
        }
        ;

 /* [6] */
Partial:
        TOK_PARTIAL PartialDefinition
        {
            $$ = $2;
        }
        ;

 /* [7] */
PartialDefinition:
        PartialInterface
        |
        PartialDictionary
        ;

 /* [8] */
PartialInterface:
        TOK_INTERFACE TOK_IDENTIFIER '{' InterfaceMembers '}' ';'
        {
            /* extend interface with additional members */
            struct webidl_node *members;
            struct webidl_node *interface_node;

            interface_node = webidl_node_find_type_ident(*webidl_ast,
                                                     WEBIDL_NODE_TYPE_INTERFACE,
                                                     $2);

            members = webidl_node_new(WEBIDL_NODE_TYPE_LIST, NULL, $4);

            if (interface_node == NULL) {
                /* doesnt already exist so create it */

                members = webidl_node_new(WEBIDL_NODE_TYPE_IDENT, members, $2);

                $$ = webidl_node_new(WEBIDL_NODE_TYPE_INTERFACE, NULL, members);
            } else {
                /* update the existing interface */

                /* link member node into interfaces_node */
                webidl_node_add(interface_node, members);

                $$ = NULL; /* updating so no need to add a new node */
            }
        }
        ;

 /* [9] slightly altered from original grammar to be left recursive */
InterfaceMembers:
        /* empty */
        {
          $$ = NULL;
        }
        |
        InterfaceMembers ExtendedAttributeList InterfaceMember
        /* This needs to deal with members with the same identifier which
         * indicate polymorphism. this is handled in the AST by adding the
         * argument lists for each polymorphism to the same
         * WEBIDL_NODE_TYPE_OPERATION
         *
         * @todo need to consider qualifer/stringifier compatibility
         */
        {
            struct webidl_node *member_node;
            struct webidl_node *ident_node;
            struct webidl_node *list_node;

            ident_node = webidl_node_find_type(webidl_node_getnode($3),
                                               NULL,
                                               WEBIDL_NODE_TYPE_IDENT);

            list_node = webidl_node_find_type(webidl_node_getnode($3),
                                              NULL,
                                              WEBIDL_NODE_TYPE_LIST);

            if (ident_node == NULL) {
                /* something with no ident - possibly constructors? */
                /* @todo understand this abtter */

                $$ = webidl_node_prepend($1, $3);

            } else if (list_node == NULL) {
                /* member with no argument list, usually an attribute, cannot
                 * be polymorphic
                 */

                /* add extended attributes to parameter list */
                webidl_node_add($3, $2);

                $$ = webidl_node_prepend($1, $3);

            } else {
                /* add extended attributes to parameter list */
                webidl_node_add(list_node, $2);

                /* has an arguemnt list so can be polymorphic */
                member_node = webidl_node_find_type_ident($1,
                                             webidl_node_gettype($3),
                                             webidl_node_gettext(ident_node));
                if (member_node == NULL) {
                    /* not a member with that ident already present */
                    $$ = webidl_node_prepend($1, $3);
                } else {
                    webidl_node_add(member_node, list_node);
                    $$ = $1; /* updated existing node do not add new one */
                }
            }
        }
        ;

 /* [10] */
InterfaceMember:
        Const
        |
        AttributeOrOperation
        ;

 /* [11] */
Dictionary:
        TOK_DICTIONARY TOK_IDENTIFIER Inheritance '{' DictionaryMembers '}' ';'
        {
            $$ = NULL;
        }
        ;

 /* [12] */
DictionaryMembers:
        /* empty */
        |
        ExtendedAttributeList DictionaryMember DictionaryMembers
        ;

 /* [13] */
DictionaryMember:
        Type TOK_IDENTIFIER Default ';'
        ;

 /* [14] */
PartialDictionary:
        TOK_DICTIONARY TOK_IDENTIFIER '{' DictionaryMembers '}' ';'
        {
            $$ = NULL;
        }

 /* [15] */
Default:
        /* empty */
        |
        '=' DefaultValue
        ;


 /* [16] */
DefaultValue:
        ConstValue
        |
        TOK_STRING_LITERAL
        ;

 /* [17] */
Exception:
        TOK_EXCEPTION TOK_IDENTIFIER Inheritance '{' ExceptionMembers '}' ';'
        {
            $$ = NULL;
        }
        ;

 /* [18] */
ExceptionMembers:
        /* empty */
        |
        ExtendedAttributeList ExceptionMember ExceptionMembers
        ;

 /* [19] returns a string */
Inheritance:
        /* empty */
        {
          $$ = NULL;
        }
        |
        ':' TOK_IDENTIFIER
        {
          $$ = $2;
        }
        ;

/* [20] */
Enum:
        TOK_ENUM TOK_IDENTIFIER '{' EnumValueList '}' ';'
        {
            $$ = NULL;
        }
        ;

/* [21] */
EnumValueList:
        TOK_STRING_LITERAL EnumValues
        ;

/* [22] */
EnumValues:
        /* empty */
        |
        ',' TOK_STRING_LITERAL EnumValues
        ;

 /* [23] - bug in w3c grammar? it doesnt list the equals as a terminal  */
CallbackRest:
        TOK_IDENTIFIER '=' ReturnType '(' ArgumentList ')' ';'
        {
            $$ = NULL;
        }
        ;

 /* [24] */
Typedef:
        TOK_TYPEDEF ExtendedAttributeList Type TOK_IDENTIFIER ';'
        {
            $$ = NULL;
        }
        ;

 /* [25] */
ImplementsStatement:
        TOK_IDENTIFIER TOK_IMPLEMENTS TOK_IDENTIFIER ';'
        {
            /* extend interface with implements members */
            struct webidl_node *implements;
            struct webidl_node *interface_node;


            interface_node = webidl_node_find_type_ident(*webidl_ast,
                                                     WEBIDL_NODE_TYPE_INTERFACE,
                                                     $1);

            implements = webidl_node_new(WEBIDL_NODE_TYPE_INTERFACE_IMPLEMENTS, NULL, $3);

            if (interface_node == NULL) {
                /* interface doesnt already exist so create it */

                implements = webidl_node_new(WEBIDL_NODE_TYPE_IDENT, implements, $1);

                $$ = webidl_node_new(WEBIDL_NODE_TYPE_INTERFACE, NULL, implements);
            } else {
                /* update the existing interface */

                /* link implements node into interfaces_node */
                webidl_node_add(interface_node, implements);

                $$ = NULL; /* updating so no need to add a new node */
            }
        }
        ;

 /* [26] */
Const:
        TOK_CONST ConstType TOK_IDENTIFIER '=' ConstValue ';'
        {
            struct webidl_node *constant;

            constant = webidl_node_new(WEBIDL_NODE_TYPE_IDENT, NULL, $3);

            /* add constant type */
            constant = webidl_node_prepend(constant, $2);

            /* add constant value */
            constant = webidl_node_prepend(constant, $5);

            $$ = webidl_node_new(WEBIDL_NODE_TYPE_CONST, NULL, constant);
        }
        ;

 /* [27] */
ConstValue:
        BooleanLiteral
        |
        FloatLiteral
        |
        TOK_INT_LITERAL
        {
          $$ = webidl_node_new(WEBIDL_NODE_TYPE_LITERAL_INT, NULL, (void *)$1);
        }
        |
        TOK_NULL_LITERAL
        {
          $$ = webidl_node_new(WEBIDL_NODE_TYPE_LITERAL_NULL, NULL, NULL);
        }
        ;

 /* [28] */
BooleanLiteral:
        TOK_TRUE
        {
          $$ = webidl_node_new(WEBIDL_NODE_TYPE_LITERAL_BOOL, NULL, (void *)true);
        }
        |
        TOK_FALSE
        {
          $$ = webidl_node_new(WEBIDL_NODE_TYPE_LITERAL_BOOL, NULL, (void *)false);
        }
        ;

 /* [29] */
FloatLiteral:
        TOK_FLOAT_LITERAL
        {
          float *value;
          value = malloc(sizeof(float));
          *value = strtof($1, NULL);
          $$ = webidl_node_new(WEBIDL_NODE_TYPE_LITERAL_FLOAT, NULL, value);
        }
        |
        '-' TOK_INFINITY
        {
          float *value;
          value = malloc(sizeof(float));
          *value = -INFINITY;
          $$ = webidl_node_new(WEBIDL_NODE_TYPE_LITERAL_FLOAT, NULL, value);
        }
        |
        TOK_INFINITY
        {
          float *value;
          value = malloc(sizeof(float));
          *value = INFINITY;
          $$ = webidl_node_new(WEBIDL_NODE_TYPE_LITERAL_FLOAT, NULL, value);
        }
        |
        TOK_NAN
        {
          float *value;
          value = malloc(sizeof(float));
          *value = NAN;
          $$ = webidl_node_new(WEBIDL_NODE_TYPE_LITERAL_FLOAT, NULL, value);
        }
        ;

 /* [30] */
AttributeOrOperation:
        TOK_STRINGIFIER StringifierAttributeOrOperation
        {
            $$ = $2;
        }
        |
        Attribute
        |
        Operation
        ;

 /* [31] */
StringifierAttributeOrOperation:
        Attribute
        |
        OperationRest
        {
            /* @todo deal with stringifier */
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_OPERATION, NULL, $1);
        }
        |
        ';'
        {
          $$=NULL;
        }
        ;

 /* [32] */
Attribute:
        Inherit ReadOnly TOK_ATTRIBUTE Type TOK_IDENTIFIER ';'
        {
            struct webidl_node *attribute;

            attribute = webidl_node_new(WEBIDL_NODE_TYPE_IDENT, NULL, $5);

            /* add attributes type */
            attribute = webidl_node_prepend(attribute, $4);

            /* deal with readonly modifier */
            if ($2) {
                attribute = webidl_node_new(WEBIDL_NODE_TYPE_MODIFIER, attribute, (void *)WEBIDL_TYPE_READONLY);
            }

            $$ = webidl_node_new(WEBIDL_NODE_TYPE_ATTRIBUTE, NULL, attribute);

        }
        ;

 /* [33] */
Inherit:
        /* empty */
        {
            $$ = false;
        }
        |
        TOK_INHERIT
        {
            $$ = true;
        }
        ;

 /* [34] */
ReadOnly:
        /* empty */
        {
            $$ = false;
        }
        |
        TOK_READONLY
        {
            $$ = true;
        }
        ;

 /* [35] */
Operation:
        Qualifiers OperationRest
        {
            /* @todo fix qualifiers */
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_OPERATION, NULL, $2);
        }
        ;

 /* [36] */
Qualifiers:
        TOK_STATIC
        |
        Specials
        ;

 /* [37] */
Specials:
        /* empty */
        |
        Special Specials
        ;

 /* [38] */
Special:
        TOK_GETTER
        |
        TOK_SETTER
        |
        TOK_CREATOR
        |
        TOK_DELETER
        |
        TOK_LEGACYCALLER
        ;

 /* [39] */
OperationRest:
        ReturnType OptionalIdentifier '(' ArgumentList ')' ';'
        {
            struct webidl_node *arglist;

            /* put return type in argument list */
            arglist = webidl_node_prepend($4, $1);

            /* argument list */
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_LIST, NULL, arglist);

            $$ = webidl_node_prepend($$, $2); /* identifier */
        }
        ;

 /* [40] */
OptionalIdentifier:
        /* empty */
        {
            $$ = NULL;
        }
        |
        TOK_IDENTIFIER
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_IDENT, NULL, $1);
        }
        ;


 /* [41] an empty list or a list of non empty comma separated arguments, note
  * this is right associative so the tree build is ass backwards
  */
ArgumentList:
        /* empty */
        {
            $$ = NULL;
        }
        |
        Argument Arguments
        {
            $$ = webidl_node_append($2, $1);
        }
        ;

 /* [42] */
Arguments:
        /* empty */
        {
            $$ = NULL;
        }
        |
        ',' Argument Arguments
        {
            $$ = webidl_node_append($3, $2);
        }
        ;


 /* [43] */
Argument:
        ExtendedAttributeList OptionalOrRequiredArgument
        {
            $$ = $2;
        }
        ;

 /* [44] */
OptionalOrRequiredArgument:
        TOK_OPTIONAL Type ArgumentName Default
        {
            struct webidl_node *argument;
            argument = webidl_node_new(WEBIDL_NODE_TYPE_IDENT, NULL, $3);
            argument = webidl_node_prepend(argument, $2); /* add type node */
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_OPTIONAL_ARGUMENT, NULL, argument);
        }
        |
        Type Ellipsis ArgumentName
        {
            struct webidl_node *argument;
            argument = webidl_node_new(WEBIDL_NODE_TYPE_IDENT, NULL, $3);
            argument = webidl_node_prepend(argument, $2); /* ellipsis node */
            argument = webidl_node_prepend(argument, $1); /* add type node */
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_ARGUMENT, NULL, argument);
        }
        ;

 /* [45] */
ArgumentName:
        ArgumentNameKeyword
        |
        TOK_IDENTIFIER
        ;

 /* [46] */
Ellipsis:
        /* empty */
        {
            $$ = NULL;
        }
        |
        TOK_ELLIPSIS
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_ELLIPSIS, NULL, NULL);
        }
        ;

 /* [47] */
ExceptionMember:
        Const
        |
        ExceptionField
        ;

 /* [48] */
ExceptionField:
        Type TOK_IDENTIFIER ';'
        ;

 /* [49] extended attribute list inside square brackets */
ExtendedAttributeList:
        /* empty */
        {
            $$ = NULL;
        }
        |
        '[' ExtendedAttribute ExtendedAttributes ']'
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE, $3, $2);
        }
        ;

 /* [50] extended attributes are separated with a comma */
ExtendedAttributes:
        /* empty */
        {
            $$ = NULL;
        }
        |
        ',' ExtendedAttribute ExtendedAttributes
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE, $3, $2);
        }
        ;

 /* [51] extended attributes internal nesting with normal, square and curly
  * braces
  */
ExtendedAttribute:
        '(' ExtendedAttributeInner ')' ExtendedAttributeRest
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE, $4, $2);
        }
        |
        '[' ExtendedAttributeInner ']' ExtendedAttributeRest
        {
            /* @todo should be a WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE_SQUARE */
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE, $4, $2);
        }
        |
        '{' ExtendedAttributeInner '}' ExtendedAttributeRest
        {
            /* @todo should be a WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE_CURLY */
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE, $4, $2);
        }
        |
        Other ExtendedAttributeRest
        {
            $$ = webidl_node_append($2,
                             webidl_node_new(WEBIDL_NODE_TYPE_IDENT, NULL, $1));
        }
        ;

 /* [52] extended attributes can be space separated too */
ExtendedAttributeRest:
        /* empty */
        {
            $$ = NULL;
        }
        |
        ExtendedAttribute
        {
            $$ = $1;
        }
        ;

 /* [53] extended attributes are nested with normal, square and curly braces */
ExtendedAttributeInner:
        /* empty */
        {
            $$ = NULL;
        }
        |
        '(' ExtendedAttributeInner ')' ExtendedAttributeInner
        {
            $$ = webidl_node_prepend(
                     webidl_node_new(WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE,
                                     NULL,
                                     $2),
                     $4);
        }
        |
        '[' ExtendedAttributeInner ']' ExtendedAttributeInner
        {
            /* @todo should be a WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE_SQUARE */
            $$ = webidl_node_prepend(
                     webidl_node_new(WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE,
                                     NULL,
                                     $2),
                     $4);
        }
        |
        '{' ExtendedAttributeInner '}' ExtendedAttributeInner
        {
            /* @todo should be a WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE_CURLY */
            $$ = webidl_node_prepend(
                     webidl_node_new(WEBIDL_NODE_TYPE_EXTENDED_ATTRIBUTE,
                                     NULL,
                                     $2),
                     $4);
        }
        |
        OtherOrComma ExtendedAttributeInner
        {
            $$ = webidl_node_append($2,
                             webidl_node_new(WEBIDL_NODE_TYPE_IDENT, NULL, $1));
        }
        ;

 /* [54] */
Other:
        TOK_INT_LITERAL
        {
            /* @todo loosing base info here might break the attribute */
            $$ = calloc(1, 32);
            snprintf($$, 32, "%ld", $1);
        }
        |
        TOK_FLOAT_LITERAL
        {
            $$ = $1;
        }
        |
        TOK_IDENTIFIER
        {
            $$ = $1;
        }
        |
        TOK_STRING_LITERAL
        {
            $$ = $1;
        }
        |
        TOK_OTHER_LITERAL
        {
            $$ = $1;
        }
        |
        '-'
        {
            $$ = strdup("-");
        }
        |
        '.'
        {
            $$ = strdup(".");
        }
        |
        TOK_ELLIPSIS
        {
            $$ = strdup("...");
        }
        |
        ':'
        {
            $$ = strdup(":");
        }
        |
        ';'
        {
            $$ = strdup(";");
        }
        |
        '<'
        {
            $$ = strdup("<");
        }
        |
        '='
        {
            $$ = strdup("=");
        }
        |
        '>'
        {
            $$ = strdup(">");
        }
        |
        '?'
        {
            $$ = strdup("?");
        }
        |
        TOK_DATE
        {
            $$ = strdup("Date");
        }
        |
        TOK_STRING
        {
            $$ = strdup("DOMString");
        }
        |
        TOK_INFINITY
        {
            $$ = strdup("Infinity");
        }
        |
        TOK_NAN
        {
            $$ = strdup("NaN");
        }
        |
        TOK_ANY
        {
            $$ = strdup("any");
        }
        |
        TOK_BOOLEAN
        {
            $$ = strdup("boolean");
        }
        |
        TOK_BYTE
        {
            $$ = strdup("byte");
        }
        |
        TOK_DOUBLE
        {
            $$ = strdup("double");
        }
        |
        TOK_FALSE
        {
            $$ = strdup("false");
        }
        |
        TOK_FLOAT
        {
            $$ = strdup("float");
        }
        |
        TOK_LONG
        {
            $$ = strdup("long");
        }
        |
        TOK_NULL_LITERAL
        {
            $$ = strdup("null");
        }
        |
        TOK_OBJECT
        {
            $$ = strdup("object");
        }
        |
        TOK_OCTET
        {
            $$ = strdup("octet");
        }
        |
        TOK_OR
        {
            $$ = strdup("or");
        }
        |
        TOK_OPTIONAL
        {
            $$ = strdup("optional");
        }
        |
        TOK_SEQUENCE
        {
            $$ = strdup("sequence");
        }
        |
        TOK_SHORT
        {
            $$ = strdup("short");
        }
        |
        TOK_TRUE
        {
            $$ = strdup("true");
        }
        |
        TOK_UNSIGNED
        {
            $$ = strdup("unsigned");
        }
        |
        TOK_VOID
        {
            $$ = strdup("void");
        }
        |
        ArgumentNameKeyword
        {
            $$ = $1;
        }
        ;

 /* [55] */
ArgumentNameKeyword:
        TOK_ATTRIBUTE
        {
            $$ = strdup("attribute");
        }
        |
        TOK_CALLBACK
        {
            $$ = strdup("callback");
        }
        |
        TOK_CONST
        {
            $$ = strdup("const");
        }
        |
        TOK_CREATOR
        {
            $$ = strdup("creator");
        }
        |
        TOK_DELETER
        {
            $$ = strdup("deleter");
        }
        |
        TOK_DICTIONARY
        {
            $$ = strdup("dictionary");
        }
        |
        TOK_ENUM
        {
            $$ = strdup("enum");
        }
        |
        TOK_EXCEPTION
        {
            $$ = strdup("exception");
        }
        |
        TOK_GETTER
        {
            $$ = strdup("getter");
        }
        |
        TOK_IMPLEMENTS
        {
            $$ = strdup("implements");
        }
        |
        TOK_INHERIT
        {
            $$ = strdup("inherit");
        }
        |
        TOK_INTERFACE
        {
            $$ = strdup("interface");
        }
        |
        TOK_LEGACYCALLER
        {
            $$ = strdup("legacycaller");
        }
        |
        TOK_PARTIAL
        {
            $$ = strdup("partial");
        }
        |
        TOK_SETTER
        {
            $$ = strdup("setter");
        }
        |
        TOK_STATIC
        {
            $$ = strdup("static");
        }
        |
        TOK_STRINGIFIER
        {
            $$ = strdup("stringifier");
        }
        |
        TOK_TYPEDEF
        {
            $$ = strdup("typedef");
        }
        |
        TOK_UNRESTRICTED
        {
            $$ = strdup("unrestricted");
        }
        ;

 /* [56] as it says an other element or a comma */
OtherOrComma:
        Other
        {
            $$ = $1;
        }
        |
        ','
        {
            $$ = strdup(",");
        }
        ;

 /* [57] */
Type:
        SingleType
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE, NULL, $1);
        }
        |
        UnionType TypeSuffix
        {
            /* todo handle suffix */
            $$ = $1;
        }
        ;

 /* [58] */
SingleType:
        NonAnyType
        |
        TOK_ANY TypeSuffixStartingWithArray
        {
            $$ = NULL; /* todo implement */
        }
        ;

 /* [59] */
UnionType:
        '(' UnionMemberType TOK_OR UnionMemberType UnionMemberTypes ')'
        {
            $$ = NULL;
        }
        ;

 /* [60] */
UnionMemberType:
        NonAnyType
        |
        UnionType TypeSuffix
        |
        TOK_ANY '[' ']' TypeSuffix
        ;

 /* [61] */
UnionMemberTypes:
        /* empty */
        |
        TOK_OR UnionMemberType UnionMemberTypes
        ;

 /* [62] */
NonAnyType:
        PrimitiveType TypeSuffix
        {
            $$ = webidl_node_prepend($1, $2);
        }
        |
        TOK_STRING TypeSuffix
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, $2, (void *)WEBIDL_TYPE_STRING);
        }
        |
        TOK_IDENTIFIER TypeSuffix
        {
            struct webidl_node *type;
            type = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, $2, (void *)WEBIDL_TYPE_USER);
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_IDENT, type, $1);
        }
        |
        TOK_SEQUENCE '<' Type '>' Null
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, $3, (void *)WEBIDL_TYPE_SEQUENCE);
        }
        |
        TOK_OBJECT TypeSuffix
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, $2, (void *)WEBIDL_TYPE_OBJECT);
        }
        |
        TOK_DATE TypeSuffix
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, $2, (void *)WEBIDL_TYPE_DATE);
        }
        ;

 /* [63] */
ConstType:
        PrimitiveType Null
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE, NULL, $1);
        }
        |
        TOK_IDENTIFIER Null
        {
            struct webidl_node *type;
            type = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, NULL, (void *)WEBIDL_TYPE_USER);
            type = webidl_node_new(WEBIDL_NODE_TYPE_IDENT, type, $1);
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE, NULL, type);
        }

        ;

 /* [64] */
PrimitiveType:
        UnsignedIntegerType
        |
        UnrestrictedFloatType
        |
        TOK_BOOLEAN
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, NULL, (void *)WEBIDL_TYPE_BOOL);
        }
        |
        TOK_BYTE
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, NULL, (void *)WEBIDL_TYPE_BYTE);
        }
        |
        TOK_OCTET
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, NULL, (void *)WEBIDL_TYPE_OCTET);
        }
        ;

 /* [65] */
UnrestrictedFloatType:
        TOK_UNRESTRICTED FloatType
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_MODIFIER,
                                 $2,
                                 (void *)WEBIDL_TYPE_MODIFIER_UNRESTRICTED);
        }
        |
        FloatType
        ;

 /* [66] */
FloatType:
        TOK_FLOAT
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, NULL, (void *)WEBIDL_TYPE_FLOAT);
        }
        |
        TOK_DOUBLE
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, NULL, (void *)WEBIDL_TYPE_DOUBLE);
        }
        ;

 /* [67] */
UnsignedIntegerType:
        TOK_UNSIGNED IntegerType
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_MODIFIER,
                                 $2,
                                 (void *)WEBIDL_TYPE_MODIFIER_UNSIGNED);
        }
        |
        IntegerType
        ;

 /* [68] */
IntegerType:
        TOK_SHORT
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, NULL, (void *)WEBIDL_TYPE_SHORT);
        }
        |
        TOK_LONG OptionalLong
        {
            if ($2) {
                $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, NULL, (void *)WEBIDL_TYPE_LONGLONG);
            } else {
                $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, NULL, (void *)WEBIDL_TYPE_LONG);
            }
        }
        ;

 /* [69] */
OptionalLong:
        /* empty */
        {
            $$ = false;
        }
        |
        TOK_LONG
        {
            $$ = true;
        }
        ;

 /* [70] */
TypeSuffix:
        /* empty */
        {
            $$ = NULL;
        }
        |
        '[' ']' TypeSuffix
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_ARRAY, $3, NULL);
        }
        |
        '?' TypeSuffixStartingWithArray
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_NULLABLE, $2, NULL);
        }
        ;

 /* [71] */
TypeSuffixStartingWithArray:
        /* empty */
        {
            $$ = NULL;
        }
        |
        '[' ']' TypeSuffix
        {
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_ARRAY, $3, NULL);
        }
        ;

 /* [72] */
Null:
        /* empty */
        |
        '?'
        ;

 /* [73] */
ReturnType:
        Type
        |
        TOK_VOID
        {
            struct webidl_node *type;
            type = webidl_node_new(WEBIDL_NODE_TYPE_TYPE_BASE, NULL, (void *)WEBIDL_TYPE_VOID);
            $$ = webidl_node_new(WEBIDL_NODE_TYPE_TYPE, NULL, type);
        }

        ;

%%
