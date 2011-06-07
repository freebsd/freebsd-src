%{
/******************************************************************************
 *
 * Module Name: dtparser.l - Flex input file for table compiler lexer
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "dtparser.y.h"

#define YY_NO_INPUT     /* No file input, we use strings only */

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("dtscanner")
%}

%option noyywrap
%option nounput

Number          [0-9a-fA-F]+
HexNumber       0[xX][0-9a-fA-F]+
DecimalNumber   0[dD][0-9]+
LabelRef        $[a-zA-Z][0-9a-zA-Z]*
WhiteSpace      [ \t\v\r]+
NewLine         [\n]

%%

\(              return (EXPOP_PAREN_OPEN);
\)              return (EXPOP_PAREN_CLOSE);
\~              return (EXPOP_ONES_COMPLIMENT);
\!              return (EXPOP_LOGICAL_NOT);
\*              return (EXPOP_MULTIPLY);
\/              return (EXPOP_DIVIDE);
\%              return (EXPOP_MODULO);
\+              return (EXPOP_ADD);
\-              return (EXPOP_SUBTRACT);
">>"            return (EXPOP_SHIFT_RIGHT);
"<<"            return (EXPOP_SHIFT_LEFT);
\<              return (EXPOP_LESS);
\>              return (EXPOP_GREATER);
"<="            return (EXPOP_LESS_EQUAL);
">="            return (EXPOP_GREATER_EQUAL);
"=="            return (EXPOP_EQUAL);
"!="            return (EXPOP_NOT_EQUAL);
\&              return (EXPOP_AND);
\^              return (EXPOP_XOR);
\|              return (EXPOP_OR);
"&&"            return (EXPOP_LOGICAL_AND);
"||"            return (EXPOP_LOGICAL_OR);
<<EOF>>         return (EXPOP_EOF); /* null end-of-string */

{LabelRef}      return (EXPOP_LABEL);
{Number}        return (EXPOP_NUMBER);
{HexNumber}     return (EXPOP_HEX_NUMBER);
{NewLine}       return (EXPOP_NEW_LINE);
{WhiteSpace}    /* Ignore */

.               return (EXPOP_EOF);

%%

/*
 * Local support functions
 */
YY_BUFFER_STATE         LexBuffer;

/******************************************************************************
 *
 * FUNCTION:    DtInitLexer, DtTerminateLexer
 *
 * PARAMETERS:  String              - Input string to be parsed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialization and termination routines for lexer. Lexer needs
 *              a buffer to handle strings instead of a file.
 *
 *****************************************************************************/

int
DtInitLexer (
    char                    *String)
{

    LexBuffer = yy_scan_string (String);
    return (LexBuffer == NULL);
}

void
DtTerminateLexer (
    void)
{

    yy_delete_buffer (LexBuffer);
}
