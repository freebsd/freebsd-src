/******************************************************************************
 *
 * Module Name: acpihelp.h - Include file for AcpiHelp utility
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#ifdef WIN32
#include <io.h>
#include <direct.h>
#endif
#include <errno.h>


typedef enum
{
    AH_DECODE_DEFAULT           = 0,
    AH_DECODE_ASL,
    AH_DECODE_ASL_KEYWORD,
    AH_DECODE_PREDEFINED_NAME,
    AH_DECODE_AML,
    AH_DECODE_AML_OPCODE,
    AH_DISPLAY_DEVICE_IDS,
    AH_DECODE_EXCEPTION,
    AH_DECODE_ASL_AML,
    AH_DISPLAY_UUIDS,
    AH_DISPLAY_TABLES,
    AH_DISPLAY_DIRECTIVES

} AH_OPTION_TYPES;

#define     AH_MAX_ASL_LINE_LENGTH      70
#define     AH_MAX_AML_LINE_LENGTH      100


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

extern const AH_AML_OPCODE          AmlOpcodeInfo[];
extern const AH_ASL_OPERATOR        AslOperatorInfo[];
extern const AH_ASL_KEYWORD         AslKeywordInfo[];
extern const AH_UUID                AcpiUuids[];
extern const AH_DIRECTIVE_INFO      PreprocessorDirectives[];
extern const AH_TABLE               AcpiSupportedTables[];
extern BOOLEAN                      AhDisplayAll;

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

#endif /* __ACPIHELP_H */
