/******************************************************************************
 *
 * Module Name: asldefine.h - Common defines for the iASL compiler
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

#ifndef __ASLDEFINE_H
#define __ASLDEFINE_H


/*
 * Compiler versions and names
 */
#define ASL_COMPILER_NAME           "ASL+ Optimizing Compiler"
#define AML_DISASSEMBLER_NAME       "AML/ASL+ Disassembler"
#define ASL_INVOCATION_NAME         "iasl"
#define ASL_CREATOR_ID              "INTL"
#define ASL_DEFINE                  "__IASL__"

#define ASL_COMPLIANCE              "Supports ACPI Specification Revision 6.0"


/* Configuration constants */

#define ASL_MAX_ERROR_COUNT         200
#define ASL_PARSEOP_CACHE_SIZE      (1024 * 16)
#define ASL_STRING_CACHE_SIZE       (1024 * 64)

#define ASL_FIRST_PARSE_OPCODE      PARSEOP_ACCESSAS
#define ASL_PARSE_OPCODE_BASE       PARSEOP_ACCESSAS        /* First Lex type */


/*
 * Per-parser-generator configuration. These values are used to cheat and
 * directly access the bison/yacc token name table (yyname or yytname).
 * Note: These values are the index in yyname for the first lex token
 * (PARSEOP_ACCCESSAS).
 */
#if defined (YYBISON)
#define ASL_YYTNAME_START           3   /* Bison */
#elif defined (YYBYACC)
#define ASL_YYTNAME_START           257 /* Berkeley yacc */
#endif


/*
 * Macros
 */
#define ASL_RESDESC_OFFSET(m)       ACPI_OFFSET (AML_RESOURCE, m)
#define ASL_PTR_DIFF(a,b)           ((UINT8 *)(b) - (UINT8 *)(a))
#define ASL_PTR_ADD(a,b)            ((UINT8 *)(a) = ((UINT8 *)(a) + (b)))
#define ASL_GET_CHILD_NODE(a)       (a)->Asl.Child
#define ASL_GET_PEER_NODE(a)        (a)->Asl.Next
#define OP_TABLE_ENTRY(a,b,c,d)     {b,d,a,c}


/* Internal AML opcodes */

#define AML_RAW_DATA_BYTE           (UINT16) 0xAA01 /* write one raw byte */
#define AML_RAW_DATA_WORD           (UINT16) 0xAA02 /* write 2 raw bytes */
#define AML_RAW_DATA_DWORD          (UINT16) 0xAA04 /* write 4 raw bytes */
#define AML_RAW_DATA_QWORD          (UINT16) 0xAA08 /* write 8 raw bytes */
#define AML_RAW_DATA_BUFFER         (UINT16) 0xAA0B /* raw buffer with length */
#define AML_RAW_DATA_CHAIN          (UINT16) 0xAA0C /* chain of raw buffers */
#define AML_PACKAGE_LENGTH          (UINT16) 0xAA10
#define AML_UNASSIGNED_OPCODE       (UINT16) 0xEEEE
#define AML_DEFAULT_ARG_OP          (UINT16) 0xDDDD


/* filename suffixes for output files */

#define FILE_SUFFIX_PREPROC_USER    "i  "
#define FILE_SUFFIX_PREPROCESSOR    "pre"
#define FILE_SUFFIX_AML_CODE        "aml"
#define FILE_SUFFIX_MAP             "map"
#define FILE_SUFFIX_LISTING         "lst"
#define FILE_SUFFIX_HEX_DUMP        "hex"
#define FILE_SUFFIX_DEBUG           "txt"
#define FILE_SUFFIX_SOURCE          "src"
#define FILE_SUFFIX_NAMESPACE       "nsp"
#define FILE_SUFFIX_ASM_SOURCE      "asm"
#define FILE_SUFFIX_C_SOURCE        "c"
#define FILE_SUFFIX_DISASSEMBLY     "dsl"
#define FILE_SUFFIX_ASM_INCLUDE     "inc"
#define FILE_SUFFIX_C_INCLUDE       "h"
#define FILE_SUFFIX_ASL_CODE        "asl"
#define FILE_SUFFIX_C_OFFSET        "offset.h"


/* Types for input files */

#define ASL_INPUT_TYPE_BINARY       0
#define ASL_INPUT_TYPE_ACPI_TABLE   1
#define ASL_INPUT_TYPE_ASCII_ASL    2
#define ASL_INPUT_TYPE_ASCII_DATA   3


/* Misc */

#define ASL_EXTERNAL_METHOD         255
#define ASL_ABORT                   TRUE
#define ASL_NO_ABORT                FALSE
#define ASL_EOF                     ACPI_UINT32_MAX
#define ASL_WITHIN_COMMENT          (ACPI_UINT32_MAX -1)
#define ASL_BLANK_LINE              (ACPI_UINT32_MAX -1)


/* Listings */

#define ASL_LISTING_LINE_PREFIX         ":  "


/* Support for reserved method names */

#define ACPI_VALID_RESERVED_NAME_MAX    0x80000000
#define ACPI_NOT_RESERVED_NAME          ACPI_UINT32_MAX
#define ACPI_PREDEFINED_NAME            (ACPI_UINT32_MAX - 1)
#define ACPI_EVENT_RESERVED_NAME        (ACPI_UINT32_MAX - 2)
#define ACPI_COMPILER_RESERVED_NAME     (ACPI_UINT32_MAX - 3)


/* Helper macros for resource tag creation */

#define RsCreateMultiBitField \
    RsCreateResourceField

#define RsCreateBitField(Op, Name, ByteOffset, BitOffset) \
    RsCreateResourceField (Op, Name, ByteOffset, BitOffset, 1)

#define RsCreateByteField(Op, Name, ByteOffset) \
    RsCreateResourceField (Op, Name, ByteOffset, 0, 8);

#define RsCreateWordField(Op, Name, ByteOffset) \
    RsCreateResourceField (Op, Name, ByteOffset, 0, 16);

#define RsCreateDwordField(Op, Name, ByteOffset) \
    RsCreateResourceField (Op, Name, ByteOffset, 0, 32);

#define RsCreateQwordField(Op, Name, ByteOffset) \
    RsCreateResourceField (Op, Name, ByteOffset, 0, 64);

#endif /* ASLDEFINE.H */
