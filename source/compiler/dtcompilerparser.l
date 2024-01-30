%{
/******************************************************************************
 *
 * Module Name: dtcompilerparser.l - Flex input file for table compiler lexer
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code. No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#include "aslcompiler.h"
#include "dtcompilerparser.y.h"


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("dtcompilerscanner")

/* handle locations */

int DtCompilerParsercolumn = 1;
int DtLabelByteOffset = 0;
int DtCompilerParserByteOffset = 0;

UINT32 DtTokenFirstLine = 0;
UINT32 DtTokenFirstColumn = 0;

#define YY_USER_ACTION \
            DtTokenFirstLine = DtCompilerParserlineno; \
            DtTokenFirstColumn = DtCompilerParsercolumn; \
            DtCompilerParsercolumn += DtCompilerParserleng; \
            DtCompilerParserByteOffset += DtCompilerParserleng; \
            DbgPrint (ASL_PARSE_OUTPUT,\
                "user action occurred. DtCompilerParserlloc.first_line: %u\n",\
                DtTokenFirstLine);
%}

%option nounput noinput yylineno

    /* Indicates a state used for parsing multiline C comments */
%x ML_COMMENT
%x DATA_STATE

WhiteSpace      [ \t\v\r]+
NewLines        [\n]+

    /* Avoid ", \n, and [] as a part of label name. These are not valid characters of a label name */
LabelName       [^ ":\n\[\]]([^":\n\[\]]*[^" :\n\[\]])?

    /* Avoid ", \n, \\, and [] as a part of data. These are not valid characters of data */
Data            [^ \\":\n\[\]]([^":\n\[\]\\]*[^" :\n\[\]\\])?

Text            [^ ":\n][^":\n]*
Comment         \[[^\n\[\]]*\]
CommentField    {LabelName}{WhiteSpace}*:{WhiteSpace}{Comment}?$


%%

<DATA_STATE>{WhiteSpace}"\\\n" {
        DbgPrint(ASL_PARSE_OUTPUT,"Continuation matched\n");
        return (DT_PARSEOP_LINE_CONTINUATION);
    }

":" {
        DbgPrint(ASL_PARSE_OUTPUT, ": Matched\n");
        BEGIN (DATA_STATE);
        return (':');
    }

<INITIAL,DATA_STATE>{WhiteSpace} { DbgPrint(ASL_PARSE_OUTPUT,"Whitespace matched\n"); }

<INITIAL,DATA_STATE>{Comment}    { DbgPrint(ASL_PARSE_OUTPUT,"Comment matched\n"); }

"/*"                     { BEGIN (ML_COMMENT); }
<ML_COMMENT>"*/"         { BEGIN (INITIAL); }
<ML_COMMENT>"*/\n"       { BEGIN (INITIAL); }
<ML_COMMENT>([^*]|\n)+|. /* Ignore */
"//".*                   /* Ignore */


<DATA_STATE>{Data} {
      char *s;
      int size = strlen (DtCompilerParsertext);
      s=UtLocalCacheCalloc (size + 1);
      AcpiUtSafeStrncpy (s, DtCompilerParsertext, size + 1);
      DtCompilerParserlval.s = s;
      DbgPrint (ASL_PARSE_OUTPUT, "Data: %s\n", s);
      return (DT_PARSEOP_DATA);
}

{CommentField}  /* ignore */

{LabelName} {
    char *s;
    int size = strlen (DtCompilerParsertext);
    s=UtLocalCacheCalloc (size + 1);
    AcpiUtSafeStrncpy (s, DtCompilerParsertext, size + 1);
    DtCompilerParserlval.u = (DT_TABLE_UNIT *) UtLocalCacheCalloc (sizeof (DT_TABLE_UNIT));
    DtCompilerParserlval.u->Value = s;
    DtCompilerParserlval.u->Line = DtCompilerParserlineno;
    DtCompilerParserlval.u->Column = DtCompilerParsercolumn;
    DtLabelByteOffset = DtCompilerParserByteOffset;
    DbgPrint (ASL_PARSE_OUTPUT, "Label: %s\n", s);
    return (DT_PARSEOP_LABEL);
}


<DATA_STATE>\"{Text}?\" { // remove outer quotes from the string, they are unnecessary
    char *s;
    int size = strlen (DtCompilerParsertext);
    s=UtLocalCacheCalloc (size - 1);
    AcpiUtSafeStrncpy (s, DtCompilerParsertext + 1, size - 1);
    DtCompilerParserlval.s = s;
    DbgPrint (ASL_PARSE_OUTPUT, "String Data: %s\n", s);
    BEGIN (INITIAL);
    return (DT_PARSEOP_STRING_DATA);
}


<INITIAL,DATA_STATE>{NewLines} {
    DbgPrint(ASL_PARSE_OUTPUT,
        "Newline matched (data state). Current line number: %u\n",DtCompilerParserlineno);
    BEGIN (INITIAL); DtCompilerParsercolumn = 1;
}


%%


/*
 * Local support functions
 */

void
DtCompilerInitLexer (
    FILE                *inFile)
{
    yyin = inFile;
}

void
DtCompilerTerminateLexer (
    void)
{
    /*
     * Flex/Bison increments the lineno for the EOF so decrement by 1 to get
     * the correct number of lines.
     */
    AslGbl_CurrentLineNumber = DtCompilerParserlineno - 1;
    AslGbl_InputByteCount = DtCompilerParserByteOffset;
}
