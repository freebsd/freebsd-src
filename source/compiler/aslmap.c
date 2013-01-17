/******************************************************************************
 *
 * Module Name: aslmap - parser to AML opcode mapping table
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
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

#include "aslcompiler.h"
#include "amlcode.h"
#include "acparser.h"


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslmap")


/*******************************************************************************
 *
 * FUNCTION:    AslMapNamedOpcodeToDataType
 *
 * PARAMETERS:  Opcode              - The Named AML opcode to map
 *
 * RETURN:      The ACPI type associated with the named opcode
 *
 * DESCRIPTION: Convert a raw Named AML opcode to the associated data type.
 *              Named opcodes are a subset of the AML opcodes.
 *
 ******************************************************************************/

ACPI_OBJECT_TYPE
AslMapNamedOpcodeToDataType (
    UINT16                  Opcode)
{
    const ACPI_OPCODE_INFO  *OpInfo;


    /*
     * There are some differences from the opcode table types, we
     * catch them here.
     */
    OpInfo = AcpiPsGetOpcodeInfo (Opcode);

    if (Opcode == AML_INT_NAMEPATH_OP)
    {
        return (ACPI_TYPE_ANY);
    }

    if (Opcode == AML_INT_METHODCALL_OP)
    {
        return (ACPI_TYPE_ANY);
    }

    if (OpInfo->Flags & AML_NSOBJECT)
    {
        return (OpInfo->ObjectType);
    }

    return (ACPI_TYPE_ANY);
}


/*******************************************************************************
 *
 * DATA STRUCTURE:  AslKeywordMapping
 *
 * DESCRIPTION:     Maps the ParseOpcode to the actual AML opcode. The parse
 *                  opcodes are generated from Bison, and this table must
 *                  track any additions to them.
 *
 * Each entry in the table contains the following items:
 *
 * AML opcode   - Opcode that is written to the AML file
 * Value        - Value of the object to be written (if applicable)
 * Flags        - 1) Whether this opcode opens an AML "package".
 *
 ******************************************************************************/
/*
 * TBD:
 * AccessAttrib
 * AccessType
 * AMlop for DMA?
 * ObjectType keywords
 * Register
 */

const ASL_MAPPING_ENTRY     AslKeywordMapping [] =
{
/*! [Begin] no source code translation (keep the table structure) */


/* ACCESSAS */                  OP_TABLE_ENTRY (AML_INT_ACCESSFIELD_OP,     0,                              0,                  0),
/* ACCESSATTRIB_BLOCK */        OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_BLOCK,         0,                  0),
/* ACCESSATTRIB_BLOCK_CALL */   OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_BLOCK_CALL,    0,                  0),
/* ACCESSATTRIB_BYTE */         OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_BYTE,          0,                  0),
/* ACCESSATTRIB_MULTIBYTE */    OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_MULTIBYTE,     0,                  0),
/* ACCESSATTRIB_QUICK */        OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_QUICK,         0,                  0),
/* ACCESSATTRIB_RAW_BYTES */    OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_RAW_BYTES,     0,                  0),
/* ACCESSATTRIB_RAW_PROCESS */  OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_RAW_PROCESS,   0,                  0),
/* ACCESSATTRIB_SND_RCV */      OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_SEND_RCV,      0,                  0),
/* ACCESSATTRIB_WORD */         OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_WORD,          0,                  0),
/* ACCESSATTRIB_WORD_CALL */    OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ATTRIB_WORD_CALL,     0,                  0),
/* ACCESSTYPE_ANY */            OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_ANY,           0,                  0),
/* ACCESSTYPE_BUF */            OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_BUFFER,        0,                  0),
/* ACCESSTYPE_BYTE */           OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_BYTE,          0,                  0),
/* ACCESSTYPE_DWORD */          OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_DWORD,         0,                  0),
/* ACCESSTYPE_QWORD */          OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_QWORD,         0,                  0),
/* ACCESSTYPE_WORD */           OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_ACCESS_WORD,          0,                  0),
/* ACQUIRE */                   OP_TABLE_ENTRY (AML_ACQUIRE_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* ADD */                       OP_TABLE_ENTRY (AML_ADD_OP,                 0,                              0,                  ACPI_BTYPE_INTEGER),
/* ADDRESSINGMODE_7BIT */       OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* ADDRESSINGMODE_10BIT */      OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* ADDRESSTYPE_ACPI */          OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* ADDRESSTYPE_MEMORY */        OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* ADDRESSTYPE_NVS */           OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* ADDRESSTYPE_RESERVED */      OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* ALIAS */                     OP_TABLE_ENTRY (AML_ALIAS_OP,               0,                              0,                  0),
/* AND */                       OP_TABLE_ENTRY (AML_BIT_AND_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* ARG0 */                      OP_TABLE_ENTRY (AML_ARG0,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG1 */                      OP_TABLE_ENTRY (AML_ARG1,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG2 */                      OP_TABLE_ENTRY (AML_ARG2,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG3 */                      OP_TABLE_ENTRY (AML_ARG3,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG4 */                      OP_TABLE_ENTRY (AML_ARG4,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG5 */                      OP_TABLE_ENTRY (AML_ARG5,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* ARG6 */                      OP_TABLE_ENTRY (AML_ARG6,                   0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* BANKFIELD */                 OP_TABLE_ENTRY (AML_BANK_FIELD_OP,          0,                              NODE_AML_PACKAGE,   0),
/* BITSPERBYTE_EIGHT */         OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* BITSPERBYTE_FIVE */          OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* BITSPERBYTE_NINE */          OP_TABLE_ENTRY (AML_BYTE_OP,                4,                              0,                  0),
/* BITSPERBYTE_SEVEN */         OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* BITSPERBYTE_SIX */           OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* BREAK */                     OP_TABLE_ENTRY (AML_BREAK_OP,               0,                              0,                  0),
/* BREAKPOINT */                OP_TABLE_ENTRY (AML_BREAK_POINT_OP,         0,                              0,                  0),
/* BUFFER */                    OP_TABLE_ENTRY (AML_BUFFER_OP,              0,                              NODE_AML_PACKAGE,   ACPI_BTYPE_BUFFER),
/* BUSMASTERTYPE_MASTER */      OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* BUSMASTERTYPE_NOTMASTER */   OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* BYTECONST */                 OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          0,                              0,                  ACPI_BTYPE_INTEGER),
/* CASE */                      OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* CLOCKPHASE_FIRST */          OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* CLOCKPHASE_SECOND */         OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* CLOCKPOLARITY_HIGH */        OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* CLOCKPOLARITY_LOW */         OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* CONCATENATE */               OP_TABLE_ENTRY (AML_CONCAT_OP,              0,                              0,                  ACPI_BTYPE_COMPUTE_DATA),
/* CONCATENATERESTEMPLATE */    OP_TABLE_ENTRY (AML_CONCAT_RES_OP,          0,                              0,                  ACPI_BTYPE_BUFFER),
/* CONDREFOF */                 OP_TABLE_ENTRY (AML_COND_REF_OF_OP,         0,                              0,                  ACPI_BTYPE_INTEGER),
/* CONNECTION */                OP_TABLE_ENTRY (AML_INT_CONNECTION_OP,      0,                              0,                  0),
/* CONTINUE */                  OP_TABLE_ENTRY (AML_CONTINUE_OP,            0,                              0,                  0),
/* COPY */                      OP_TABLE_ENTRY (AML_COPY_OP,                0,                              0,                  ACPI_BTYPE_DATA_REFERENCE),
/* CREATEBITFIELD */            OP_TABLE_ENTRY (AML_CREATE_BIT_FIELD_OP,    0,                              0,                  0),
/* CREATEBYTEFIELD */           OP_TABLE_ENTRY (AML_CREATE_BYTE_FIELD_OP,   0,                              0,                  0),
/* CREATEDWORDFIELD */          OP_TABLE_ENTRY (AML_CREATE_DWORD_FIELD_OP,  0,                              0,                  0),
/* CREATEFIELD */               OP_TABLE_ENTRY (AML_CREATE_FIELD_OP,        0,                              0,                  0),
/* CREATEQWORDFIELD */          OP_TABLE_ENTRY (AML_CREATE_QWORD_FIELD_OP,  0,                              0,                  0),
/* CREATEWORDFIELD */           OP_TABLE_ENTRY (AML_CREATE_WORD_FIELD_OP,   0,                              0,                  0),
/* DATABUFFER */                OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DATATABLEREGION */           OP_TABLE_ENTRY (AML_DATA_REGION_OP,         0,                              0,                  0),
/* DEBUG */                     OP_TABLE_ENTRY (AML_DEBUG_OP,               0,                              0,                  ACPI_BTYPE_DEBUG_OBJECT),
/* DECODETYPE_POS */            OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* DECODETYPE_SUB */            OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* DECREMENT */                 OP_TABLE_ENTRY (AML_DECREMENT_OP,           0,                              0,                  ACPI_BTYPE_INTEGER),
/* DEFAULT */                   OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DEFAULT_ARG */               OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DEFINITIONBLOCK */           OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DEREFOF */                   OP_TABLE_ENTRY (AML_DEREF_OF_OP,            0,                              0,                  ACPI_BTYPE_DATA_REFERENCE | ACPI_BTYPE_STRING),
/* DEVICE */                    OP_TABLE_ENTRY (AML_DEVICE_OP,              0,                              NODE_AML_PACKAGE,   0),
/* DEVICEPOLARITY_HIGH */       OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* DEVICEPOLARITY_LOW */        OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* DIVIDE */                    OP_TABLE_ENTRY (AML_DIVIDE_OP,              0,                              0,                  ACPI_BTYPE_INTEGER),
/* DMA */                       OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DMATYPE_A */                 OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* DMATYPE_COMPATIBILITY */     OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* DMATYPE_B */                 OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* DMATYPE_F */                 OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* DWORDCONST */                OP_TABLE_ENTRY (AML_RAW_DATA_DWORD,         0,                              0,                  ACPI_BTYPE_INTEGER),
/* DWORDIO */                   OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DWORDMEMORY */               OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* DWORDSPACE */                OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* EISAID */                    OP_TABLE_ENTRY (AML_DWORD_OP,               0,                              0,                  ACPI_BTYPE_INTEGER),
/* ELSE */                      OP_TABLE_ENTRY (AML_ELSE_OP,                0,                              NODE_AML_PACKAGE,   0),
/* ELSEIF */                    OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              NODE_AML_PACKAGE,   0),
/* ENDDEPENDENTFN */            OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* ENDIAN_BIG */                OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* ENDIAN_LITTLE */             OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* ENDTAG */                    OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* ERRORNODE */                 OP_TABLE_ENTRY (AML_NOOP_OP,                0,                              0,                  0),
/* EVENT */                     OP_TABLE_ENTRY (AML_EVENT_OP,               0,                              0,                  0),
/* EXTENDEDIO */                OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* EXTENDEDMEMORY */            OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* EXTENDEDSPACE */             OP_TABLE_ENTRY (AML_RAW_DATA_QWORD,         0,                              0,                  ACPI_BTYPE_INTEGER),
/* EXTERNAL */                  OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* FATAL */                     OP_TABLE_ENTRY (AML_FATAL_OP,               0,                              0,                  0),
/* FIELD */                     OP_TABLE_ENTRY (AML_FIELD_OP,               0,                              NODE_AML_PACKAGE,   0),
/* FINDSETLEFTBIT */            OP_TABLE_ENTRY (AML_FIND_SET_LEFT_BIT_OP,   0,                              0,                  ACPI_BTYPE_INTEGER),
/* FINDSETRIGHTBIT */           OP_TABLE_ENTRY (AML_FIND_SET_RIGHT_BIT_OP,  0,                              0,                  ACPI_BTYPE_INTEGER),
/* FIXEDDMA */                  OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* FIXEDIO */                   OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* FLOWCONTROL_HW */            OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* FLOWCONTROL_NONE */          OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* FLOWCONTROL_SW */            OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* FROMBCD */                   OP_TABLE_ENTRY (AML_FROM_BCD_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* FUNCTION */                  OP_TABLE_ENTRY (AML_METHOD_OP,              0,                              NODE_AML_PACKAGE,   0),
/* GPIOINT */                   OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* GPIOIO */                    OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* I2CSERIALBUS */              OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* IF */                        OP_TABLE_ENTRY (AML_IF_OP,                  0,                              NODE_AML_PACKAGE,   0),
/* INCLUDE */                   OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* INCLUDE_END */               OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* INCREMENT */                 OP_TABLE_ENTRY (AML_INCREMENT_OP,           0,                              0,                  ACPI_BTYPE_INTEGER),
/* INDEX */                     OP_TABLE_ENTRY (AML_INDEX_OP,               0,                              0,                  ACPI_BTYPE_REFERENCE),
/* INDEXFIELD */                OP_TABLE_ENTRY (AML_INDEX_FIELD_OP,         0,                              NODE_AML_PACKAGE,   0),
/* INTEGER */                   OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* INTERRUPT */                 OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* INTLEVEL_ACTIVEBOTH */       OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* INTLEVEL_ACTIVEHIGH */       OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* INTLEVEL_ACTIVELOW */        OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* INTTYPE_EDGE */              OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* INTTYPE_LEVEL */             OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* IO */                        OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* IODECODETYPE_10 */           OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* IODECODETYPE_16 */           OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* IORESTRICT_IN */             OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* IORESTRICT_NONE */           OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* IORESTRICT_OUT */            OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* IORESTRICT_PRESERVE */       OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* IRQ */                       OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* IRQNOFLAGS */                OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* LAND */                      OP_TABLE_ENTRY (AML_LAND_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* LEQUAL */                    OP_TABLE_ENTRY (AML_LEQUAL_OP,              0,                              0,                  ACPI_BTYPE_INTEGER),
/* LGREATER */                  OP_TABLE_ENTRY (AML_LGREATER_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* LGREATEREQUAL */             OP_TABLE_ENTRY (AML_LGREATEREQUAL_OP,       0,                              0,                  ACPI_BTYPE_INTEGER),
/* LLESS */                     OP_TABLE_ENTRY (AML_LLESS_OP,               0,                              0,                  ACPI_BTYPE_INTEGER),
/* LLESSEQUAL */                OP_TABLE_ENTRY (AML_LLESSEQUAL_OP,          0,                              0,                  ACPI_BTYPE_INTEGER),
/* LNOT */                      OP_TABLE_ENTRY (AML_LNOT_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* LNOTEQUAL */                 OP_TABLE_ENTRY (AML_LNOTEQUAL_OP,           0,                              0,                  ACPI_BTYPE_INTEGER),
/* LOAD */                      OP_TABLE_ENTRY (AML_LOAD_OP,                0,                              0,                  0),
/* LOADTABLE */                 OP_TABLE_ENTRY (AML_LOAD_TABLE_OP,          0,                              0,                  ACPI_BTYPE_DDB_HANDLE),
/* LOCAL0 */                    OP_TABLE_ENTRY (AML_LOCAL0,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL1 */                    OP_TABLE_ENTRY (AML_LOCAL1,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL2 */                    OP_TABLE_ENTRY (AML_LOCAL2,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL3 */                    OP_TABLE_ENTRY (AML_LOCAL3,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL4 */                    OP_TABLE_ENTRY (AML_LOCAL4,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL5 */                    OP_TABLE_ENTRY (AML_LOCAL5,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL6 */                    OP_TABLE_ENTRY (AML_LOCAL6,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCAL7 */                    OP_TABLE_ENTRY (AML_LOCAL7,                 0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* LOCKRULE_LOCK */             OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_LOCK_ALWAYS,          0,                  0),
/* LOCKRULE_NOLOCK */           OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_LOCK_NEVER,           0,                  0),
/* LOR */                       OP_TABLE_ENTRY (AML_LOR_OP,                 0,                              0,                  ACPI_BTYPE_INTEGER),
/* MATCH */                     OP_TABLE_ENTRY (AML_MATCH_OP,               0,                              0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MEQ */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MEQ,                      0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MGE */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MGE,                      0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MGT */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MGT,                      0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MLE */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MLE,                      0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MLT */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MLT,                      0,                  ACPI_BTYPE_INTEGER),
/* MATCHTYPE_MTR */             OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          MATCH_MTR,                      0,                  ACPI_BTYPE_INTEGER),
/* MAXTYPE_FIXED */             OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* MAXTYPE_NOTFIXED */          OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* MEMORY24 */                  OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* MEMORY32 */                  OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* MEMORY32FIXED */             OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* MEMTYPE_CACHEABLE */         OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* MEMTYPE_NONCACHEABLE */      OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* MEMTYPE_PREFETCHABLE */      OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* MEMTYPE_WRITECOMBINING */    OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* METHOD */                    OP_TABLE_ENTRY (AML_METHOD_OP,              0,                              NODE_AML_PACKAGE,   0),
/* METHODCALL */                OP_TABLE_ENTRY (AML_INT_METHODCALL_OP,      0,                              0,                  ACPI_BTYPE_OBJECTS_AND_REFS),
/* MID */                       OP_TABLE_ENTRY (AML_MID_OP,                 0,                              0,                  ACPI_BTYPE_STRING | ACPI_BTYPE_BUFFER),
/* MINTYPE_FIXED */             OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* MINTYPE_NOTFIXED */          OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* MOD */                       OP_TABLE_ENTRY (AML_MOD_OP,                 0,                              0,                  ACPI_BTYPE_INTEGER),
/* MULTIPLY */                  OP_TABLE_ENTRY (AML_MULTIPLY_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* MUTEX */                     OP_TABLE_ENTRY (AML_MUTEX_OP,               0,                              0,                  0),
/* NAME */                      OP_TABLE_ENTRY (AML_NAME_OP,                0,                              0,                  0),
/* NAMESEG */                   OP_TABLE_ENTRY (AML_INT_NAMEPATH_OP,        0,                              0,                  0),
/* NAMESTRING */                OP_TABLE_ENTRY (AML_INT_NAMEPATH_OP,        0,                              0,                  0),
/* NAND */                      OP_TABLE_ENTRY (AML_BIT_NAND_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* NOOP */                      OP_TABLE_ENTRY (AML_NOOP_OP,                0,                              0,                  0),
/* NOR */                       OP_TABLE_ENTRY (AML_BIT_NOR_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* NOT */                       OP_TABLE_ENTRY (AML_BIT_NOT_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* NOTIFY */                    OP_TABLE_ENTRY (AML_NOTIFY_OP,              0,                              0,                  0),
/* OBJECTTYPE */                OP_TABLE_ENTRY (AML_TYPE_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* OBJECTTYPE_BFF */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_BUFFER_FIELD,         0,                  0),
/* OBJECTTYPE_BUF */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_BUFFER,               0,                  0),
/* OBJECTTYPE_DDB */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_DDB_HANDLE,           0,                  0),
/* OBJECTTYPE_DEV */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_DEVICE,               0,                  0),
/* OBJECTTYPE_EVT */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_EVENT,                0,                  0),
/* OBJECTTYPE_FLD */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_FIELD_UNIT,           0,                  0),
/* OBJECTTYPE_INT */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_INTEGER,              0,                  0),
/* OBJECTTYPE_MTH */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_METHOD,               0,                  0),
/* OBJECTTYPE_MTX */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_MUTEX,                0,                  0),
/* OBJECTTYPE_OPR */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_REGION,               0,                  0),
/* OBJECTTYPE_PKG */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_PACKAGE,              0,                  0),
/* OBJECTTYPE_POW */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_POWER,                0,                  0),
/* OBJECTTYPE_PRO */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_PROCESSOR,            0,                  0),
/* OBJECTTYPE_STR */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_STRING,               0,                  0),
/* OBJECTTYPE_THZ */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_THERMAL,              0,                  0),
/* OBJECTTYPE_UNK */            OP_TABLE_ENTRY (AML_BYTE_OP,                ACPI_TYPE_ANY,                  0,                  0),
/* OFFSET */                    OP_TABLE_ENTRY (AML_INT_RESERVEDFIELD_OP,   0,                              0,                  0),
/* ONE */                       OP_TABLE_ENTRY (AML_ONE_OP,                 0,                              0,                  ACPI_BTYPE_INTEGER),
/* ONES */                      OP_TABLE_ENTRY (AML_ONES_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* OPERATIONREGION */           OP_TABLE_ENTRY (AML_REGION_OP,              0,                              0,                  0),
/* OR */                        OP_TABLE_ENTRY (AML_BIT_OR_OP,              0,                              0,                  ACPI_BTYPE_INTEGER),
/* PACKAGE */                   OP_TABLE_ENTRY (AML_PACKAGE_OP,             0,                              NODE_AML_PACKAGE,   ACPI_BTYPE_PACKAGE),
/* PACKAGEP_LENGTH */           OP_TABLE_ENTRY (AML_PACKAGE_LENGTH,         0,                              NODE_AML_PACKAGE,   0),
/* PARITYTYPE_EVEN */           OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* PARITYTYPE_MARK */           OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* PARITYTYPE_NONE */           OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* PARITYTYPE_ODD */            OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* PARITYTYPE_SPACE */          OP_TABLE_ENTRY (AML_BYTE_OP,                4,                              0,                  0),
/* PIN_NOPULL */                OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* PIN_PULLDEFAULT */           OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* PIN_PULLDOWN */              OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* PIN_PULLUP */                OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* POWERRESOURCE */             OP_TABLE_ENTRY (AML_POWER_RES_OP,           0,                              NODE_AML_PACKAGE,   0),
/* PROCESSOR */                 OP_TABLE_ENTRY (AML_PROCESSOR_OP,           0,                              NODE_AML_PACKAGE,   0),
/* QWORDCONST */                OP_TABLE_ENTRY (AML_RAW_DATA_QWORD,         0,                              0,                  ACPI_BTYPE_INTEGER),
/* QWORDIO */                   OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* QWORDMEMORY */               OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* QWORDSPACE */                OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* RANGE_TYPE_ENTIRE */         OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* RANGE_TYPE_ISAONLY */        OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* RANGE_TYPE_NONISAONLY */     OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* RAW_DATA */                  OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* READWRITETYPE_BOTH */        OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* READWRITETYPE_READONLY */    OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* REFOF */                     OP_TABLE_ENTRY (AML_REF_OF_OP,              0,                              0,                  ACPI_BTYPE_REFERENCE),
/* REGIONSPACE_CMOS */          OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_CMOS,            0,                  0),
/* REGIONSPACE_EC */            OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_EC,              0,                  0),
/* REGIONSPACE_FFIXEDHW */      OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_FIXED_HARDWARE,  0,                  0),
/* REGIONSPACE_GPIO */          OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_GPIO,            0,                  0),
/* REGIONSPACE_GSBUS */         OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_GSBUS,           0,                  0),
/* REGIONSPACE_IO */            OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_SYSTEM_IO,       0,                  0),
/* REGIONSPACE_IPMI */          OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_IPMI,            0,                  0),
/* REGIONSPACE_MEM */           OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_SYSTEM_MEMORY,   0,                  0),
/* REGIONSPACE_PCC */           OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_PLATFORM_COMM,   0,                  0),
/* REGIONSPACE_PCI */           OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_PCI_CONFIG,      0,                  0),
/* REGIONSPACE_PCIBAR */        OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_PCI_BAR_TARGET,  0,                  0),
/* REGIONSPACE_SMBUS */         OP_TABLE_ENTRY (AML_RAW_DATA_BYTE,          ACPI_ADR_SPACE_SMBUS,           0,                  0),
/* REGISTER */                  OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* RELEASE */                   OP_TABLE_ENTRY (AML_RELEASE_OP,             0,                              0,                  0),
/* RESERVED_BYTES */            OP_TABLE_ENTRY (AML_INT_RESERVEDFIELD_OP,   0,                              0,                  0),
/* RESET */                     OP_TABLE_ENTRY (AML_RESET_OP,               0,                              0,                  0),
/* RESOURCETEMPLATE */          OP_TABLE_ENTRY (AML_BUFFER_OP,              0,                              0,                  ACPI_BTYPE_BUFFER),
/* RESOURCETYPE_CONSUMER */     OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* RESOURCETYPE_PRODUCER */     OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* RETURN */                    OP_TABLE_ENTRY (AML_RETURN_OP,              0,                              0,                  0),
/* REVISION */                  OP_TABLE_ENTRY (AML_REVISION_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* SCOPE */                     OP_TABLE_ENTRY (AML_SCOPE_OP,               0,                              NODE_AML_PACKAGE,   0),
/* SERIALIZERULE_NOTSERIAL */   OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* SERIALIZERULE_SERIAL */      OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* SHARETYPE_EXCLUSIVE */       OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* SHARETYPE_EXCLUSIVEWAKE */   OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* SHARETYPE_SHARED */          OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* SHARETYPE_SHAREDWAKE */      OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* SHIFTLEFT */                 OP_TABLE_ENTRY (AML_SHIFT_LEFT_OP,          0,                              0,                  ACPI_BTYPE_INTEGER),
/* SHIFTRIGHT */                OP_TABLE_ENTRY (AML_SHIFT_RIGHT_OP,         0,                              0,                  ACPI_BTYPE_INTEGER),
/* SIGNAL */                    OP_TABLE_ENTRY (AML_SIGNAL_OP,              0,                              0,                  0),
/* SIZEOF */                    OP_TABLE_ENTRY (AML_SIZE_OF_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* SLAVEMODE_CONTROLLERINIT */  OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* SLAVEMODE_DEVICEINIT */      OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* SLEEP */                     OP_TABLE_ENTRY (AML_SLEEP_OP,               0,                              0,                  0),
/* SPISERIALBUS */              OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* STALL */                     OP_TABLE_ENTRY (AML_STALL_OP,               0,                              0,                  0),
/* STARTDEPENDENTFN */          OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* STARTDEPENDENTFN_NOPRI */    OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* STOPBITS_ONE */              OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* STOPBITS_ONEPLUSHALF */      OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* STOPBITS_TWO */              OP_TABLE_ENTRY (AML_BYTE_OP,                3,                              0,                  0),
/* STOPBITS_ZERO */             OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* STORE */                     OP_TABLE_ENTRY (AML_STORE_OP,               0,                              0,                  ACPI_BTYPE_DATA_REFERENCE),
/* STRING_LITERAL */            OP_TABLE_ENTRY (AML_STRING_OP,              0,                              0,                  ACPI_BTYPE_STRING),
/* SUBTRACT */                  OP_TABLE_ENTRY (AML_SUBTRACT_OP,            0,                              0,                  ACPI_BTYPE_INTEGER),
/* SWITCH */                    OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* THERMALZONE */               OP_TABLE_ENTRY (AML_THERMAL_ZONE_OP,        0,                              NODE_AML_PACKAGE,   0),
/* TIMER */                     OP_TABLE_ENTRY (AML_TIMER_OP,               0,                              0,                  ACPI_BTYPE_INTEGER),
/* TOBCD */                     OP_TABLE_ENTRY (AML_TO_BCD_OP,              0,                              0,                  ACPI_BTYPE_INTEGER),
/* TOBUFFER */                  OP_TABLE_ENTRY (AML_TO_BUFFER_OP,           0,                              0,                  ACPI_BTYPE_BUFFER),
/* TODECIMALSTRING */           OP_TABLE_ENTRY (AML_TO_DECSTRING_OP,        0,                              0,                  ACPI_BTYPE_STRING),
/* TOHEXSTRING */               OP_TABLE_ENTRY (AML_TO_HEXSTRING_OP,        0,                              0,                  ACPI_BTYPE_STRING),
/* TOINTEGER */                 OP_TABLE_ENTRY (AML_TO_INTEGER_OP,          0,                              0,                  ACPI_BTYPE_INTEGER),
/* TOSTRING */                  OP_TABLE_ENTRY (AML_TO_STRING_OP,           0,                              0,                  ACPI_BTYPE_STRING),
/* TOUUID */                    OP_TABLE_ENTRY (AML_DWORD_OP,               0,                              NODE_AML_PACKAGE,   ACPI_BTYPE_INTEGER),
/* TRANSLATIONTYPE_DENSE */     OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* TRANSLATIONTYPE_SPARSE */    OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* TYPE_STATIC */               OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* TYPE_TRANSLATION */          OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* UART_SERIALBUS */            OP_TABLE_ENTRY (AML_DEFAULT_ARG_OP,         0,                              0,                  0),
/* UNICODE */                   OP_TABLE_ENTRY (AML_BUFFER_OP,              0,                              NODE_AML_PACKAGE,   0),
/* UNLOAD */                    OP_TABLE_ENTRY (AML_UNLOAD_OP,              0,                              0,                  0),
/* UPDATERULE_ONES */           OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_UPDATE_WRITE_AS_ONES, 0,                  0),
/* UPDATERULE_PRESERVE */       OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_UPDATE_PRESERVE,      0,                  0),
/* UPDATERULE_ZEROS */          OP_TABLE_ENTRY (AML_BYTE_OP,                AML_FIELD_UPDATE_WRITE_AS_ZEROS,0,                  0),
/* VAR_PACKAGE */               OP_TABLE_ENTRY (AML_VAR_PACKAGE_OP,         0,                              NODE_AML_PACKAGE,   ACPI_BTYPE_PACKAGE),
/* VENDORLONG */                OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* VENDORSHORT */               OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* WAIT */                      OP_TABLE_ENTRY (AML_WAIT_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),
/* WHILE */                     OP_TABLE_ENTRY (AML_WHILE_OP,               0,                              NODE_AML_PACKAGE,   0),
/* WIREMODE_FOUR */             OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* WIREMODE_THREE */            OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* WORDBUSNUMBER */             OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* WORDCONST */                 OP_TABLE_ENTRY (AML_RAW_DATA_WORD,          0,                              0,                  ACPI_BTYPE_INTEGER),
/* WORDIO */                    OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* WORDSPACE */                 OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* XFERSIZE_8 */                OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* XFERSIZE_16 */               OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* XFERSIZE_32 */               OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* XFERSIZE_64 */               OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* XFERSIZE_128 */              OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* XFERSIZE_256 */              OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* XFERTYPE_8 */                OP_TABLE_ENTRY (AML_BYTE_OP,                0,                              0,                  0),
/* XFERTYPE_8_16 */             OP_TABLE_ENTRY (AML_BYTE_OP,                1,                              0,                  0),
/* XFERTYPE_16 */               OP_TABLE_ENTRY (AML_BYTE_OP,                2,                              0,                  0),
/* XOR */                       OP_TABLE_ENTRY (AML_BIT_XOR_OP,             0,                              0,                  ACPI_BTYPE_INTEGER),
/* ZERO */                      OP_TABLE_ENTRY (AML_ZERO_OP,                0,                              0,                  ACPI_BTYPE_INTEGER),

/*! [End] no source code translation !*/

};
