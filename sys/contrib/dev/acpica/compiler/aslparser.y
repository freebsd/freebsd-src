%{
/******************************************************************************
 *
 * Module Name: aslparser.y - Master Bison/Yacc input file for iASL
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslparse")

/*
 * Global Notes:
 *
 * October 2005: The following list terms have been optimized (from the
 * original ASL grammar in the ACPI specification) to force the immediate
 * reduction of each list item so that the parse stack use doesn't increase on
 * each list element and possibly overflow on very large lists (>4000 items).
 * This dramatically reduces use of the parse stack overall.
 *
 *      ArgList, TermList, Objectlist, ByteList, DWordList, PackageList,
 *      ResourceMacroList, and FieldUnitList
 */

void *
AslLocalAllocate (
    unsigned int            Size);

/* Bison/yacc configuration */

#define static
#undef malloc
#define malloc              AslLocalAllocate
#undef alloca
#define alloca              AslLocalAllocate
#define yytname             AslCompilername

#define YYINITDEPTH         600             /* State stack depth */
#define YYDEBUG             1               /* Enable debug output */
#define YYERROR_VERBOSE     1               /* Verbose error messages */
#define YYFLAG              -32768

/* Define YYMALLOC/YYFREE to prevent redefinition errors  */

#define YYMALLOC            AslLocalAllocate
#define YYFREE              ACPI_FREE
%}

/*
 * Declare the type of values in the grammar
 */
%union {
    UINT64              i;
    char                *s;
    ACPI_PARSE_OBJECT   *n;
}

/*
 * These shift/reduce conflicts are expected. There should be zero
 * reduce/reduce conflicts.
 */
%expect 86

/*! [Begin] no source code translation */

/*
 * The M4 macro processor is used to bring in the parser items,
 * in order to keep this master file smaller, and to break up
 * the various parser items.
 */
m4_define(NoEcho)

/* Token types */

m4_include(asltokens.y)

/* Production types/names */

m4_include(asltypes.y)
%%

/* Production rules */

m4_include(aslrules.y)
%%

/*! [End] no source code translation !*/

/* Local support functions in C */

m4_include(aslsupport.y)
