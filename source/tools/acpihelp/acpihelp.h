/******************************************************************************
 *
 * Module Name: acpihelp.h - Include file for AcpiHelp utility
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
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

#ifndef __ACPIHELP_H
#define __ACPIHELP_H


#include "acpi.h"
#include "accommon.h"
#include "acapps.h"

#include <sys/types.h>
#ifdef WIN32
#include <io.h>
#include <direct.h>
#endif


/*
 * Global variables. Defined in ahmain.c only, externed in all other files
 */
#undef ACPI_GLOBAL
#undef ACPI_INIT_GLOBAL

#ifdef DEFINE_AHELP_GLOBALS
#define ACPI_GLOBAL(type,name) \
    extern type name; \
    type name

#define ACPI_INIT_GLOBAL(type,name,value) \
    type name=value

#else
#ifndef ACPI_GLOBAL
#define ACPI_GLOBAL(type,name) \
    extern type name
#endif

#ifndef ACPI_INIT_GLOBAL
#define ACPI_INIT_GLOBAL(type,name,value) \
    extern type name
#endif
#endif


#define AH_BUFFER_LENGTH                128
#define AH_LINE_BUFFER_LENGTH           512
#define AH_MAX_ASL_LINE_LENGTH          70
#define AH_MAX_AML_LINE_LENGTH          100

ACPI_GLOBAL (char,                      Gbl_Buffer[AH_BUFFER_LENGTH]);
ACPI_GLOBAL (char,                      Gbl_LineBuffer[AH_LINE_BUFFER_LENGTH]);


#define AH_DISPLAY_EXCEPTION(Status, Name) \
    printf ("%.4X: %s\n", Status, Name)

#define AH_DISPLAY_EXCEPTION_TEXT(Status, Exception) \
    printf ("%.4X: %-28s (%s)\n", Status,\
    Exception->Name, Exception->Description)


typedef enum
{
    AH_DECODE_DEFAULT           = 0,
    AH_DECODE_ASL,
    AH_DECODE_ASL_KEYWORD,
    AH_DECODE_PREDEFINED_NAME,
    AH_DECODE_AML,
    AH_DECODE_AML_OPCODE,
    AH_DECODE_AML_TYPE,
    AH_DECODE_ASL_AML,
    AH_DECODE_EXCEPTION,

    AH_DISPLAY_DEVICE_IDS,
    AH_DISPLAY_UUIDS,
    AH_DISPLAY_TABLES,
    AH_DISPLAY_DIRECTIVES

} AH_OPTION_TYPES;

typedef struct ah_aml_opcode
{
    UINT16          OpcodeRangeStart;
    UINT16          OpcodeRangeEnd;
    char            *OpcodeString;
    char            *OpcodeName;
    char            *Type;
    char            *FixedArguments;
    char            *VariableArguments;
    char            *Grammar;

} AH_AML_OPCODE;

typedef struct ah_aml_type
{
    char            *Name;
    char            *Description;

} AH_AML_TYPE;

typedef struct ah_asl_operator
{
    char            *Name;
    char            *Syntax;
    char            *Description;

} AH_ASL_OPERATOR;

typedef struct ah_asl_keyword
{
    char            *Name;
    char            *Description;
    char            *KeywordList;

} AH_ASL_KEYWORD;

typedef struct ah_directive_info
{
    char            *Name;
    char            *Description;

} AH_DIRECTIVE_INFO;


/* Externals for various data tables */

extern const AH_AML_OPCODE          Gbl_AmlOpcodeInfo[];
extern const AH_AML_TYPE            Gbl_AmlTypesInfo[];
extern const AH_ASL_OPERATOR        Gbl_AslOperatorInfo[];
extern const AH_ASL_KEYWORD         Gbl_AslKeywordInfo[];
extern const AH_UUID                Gbl_AcpiUuids[];
extern const AH_DIRECTIVE_INFO      Gbl_PreprocessorDirectives[];
extern const AH_TABLE               Gbl_AcpiSupportedTables[];


void
AhFindAmlOpcode (
    char                    *Name);

void
AhDecodeAmlOpcode (
    char                    *Name);

void
AhDecodeException (
    char                    *Name);

void
AhFindPredefinedNames (
    char                    *Name);

void
AhFindAslAndAmlOperators (
    char                    *Name);

UINT32
AhFindAslOperators (
    char                    *Name);

void
AhFindAslKeywords (
    char                    *Name);

void
AhFindAmlTypes (
    char                    *Name);

void
AhDisplayDeviceIds (
    char                    *Name);

void
AhDisplayTables (
    void);

const AH_TABLE *
AcpiAhGetTableInfo (
    char                    *Signature);

void
AhDisplayUuids (
    void);

void
AhDisplayDirectives (
    void);

void
AhPrintOneField (
    UINT32                  Indent,
    UINT32                  CurrentPosition,
    UINT32                  MaxPosition,
    const char              *Field);

#endif /* __ACPIHELP_H */
