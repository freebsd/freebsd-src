
/******************************************************************************
 *
 * Module Name: asltypes.h - compiler data types and struct definitions
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


#ifndef __ASLTYPES_H
#define __ASLTYPES_H


/*******************************************************************************
 *
 * Structure definitions
 *
 ******************************************************************************/


/* Op flags for the ACPI_PARSE_OBJECT */

#define NODE_VISITED                0x00000001
#define NODE_AML_PACKAGE            0x00000002
#define NODE_IS_TARGET              0x00000004
#define NODE_IS_RESOURCE_DESC       0x00000008
#define NODE_IS_RESOURCE_FIELD      0x00000010
#define NODE_HAS_NO_EXIT            0x00000020
#define NODE_IF_HAS_NO_EXIT         0x00000040
#define NODE_NAME_INTERNALIZED      0x00000080
#define NODE_METHOD_NO_RETVAL       0x00000100
#define NODE_METHOD_SOME_NO_RETVAL  0x00000200
#define NODE_RESULT_NOT_USED        0x00000400
#define NODE_METHOD_TYPED           0x00000800
#define NODE_IS_BIT_OFFSET          0x00001000
#define NODE_COMPILE_TIME_CONST     0x00002000
#define NODE_IS_TERM_ARG            0x00004000
#define NODE_WAS_ONES_OP            0x00008000
#define NODE_IS_NAME_DECLARATION    0x00010000
#define NODE_COMPILER_EMITTED       0x00020000
#define NODE_IS_DUPLICATE           0x00040000
#define NODE_IS_RESOURCE_DATA       0x00080000
#define NODE_IS_NULL_RETURN         0x00100000

/* Keeps information about individual control methods */

typedef struct asl_method_info
{
    UINT8                   NumArguments;
    UINT8                   LocalInitialized[ACPI_METHOD_NUM_LOCALS];
    UINT8                   ArgInitialized[ACPI_METHOD_NUM_ARGS];
    UINT32                  ValidArgTypes[ACPI_METHOD_NUM_ARGS];
    UINT32                  ValidReturnTypes;
    UINT32                  NumReturnNoValue;
    UINT32                  NumReturnWithValue;
    ACPI_PARSE_OBJECT       *Op;
    struct asl_method_info  *Next;
    UINT8                   HasBeenTyped;

} ASL_METHOD_INFO;


/* Parse tree walk info for control method analysis */

typedef struct asl_analysis_walk_info
{
    ASL_METHOD_INFO         *MethodStack;

} ASL_ANALYSIS_WALK_INFO;


/* An entry in the ParseOpcode to AmlOpcode mapping table */

typedef struct asl_mapping_entry
{
    UINT32                      Value;
    UINT32                      AcpiBtype;   /* Object type or return type */
    UINT16                      AmlOpcode;
    UINT8                       Flags;

} ASL_MAPPING_ENTRY;


/* Parse tree walk info structure */

typedef struct asl_walk_info
{
    ACPI_PARSE_OBJECT           **NodePtr;
    UINT32                      *LevelPtr;

} ASL_WALK_INFO;


/* File info */

typedef struct asl_file_info
{
    FILE                        *Handle;
    char                        *Filename;

} ASL_FILE_INFO;

typedef struct asl_file_status
{
    UINT32                  Line;
    UINT32                  Offset;

} ASL_FILE_STATUS;


/* File types */

typedef enum
{
    ASL_FILE_STDOUT             = 0,
    ASL_FILE_STDERR,
    ASL_FILE_INPUT,
    ASL_FILE_AML_OUTPUT,
    ASL_FILE_SOURCE_OUTPUT,
    ASL_FILE_LISTING_OUTPUT,
    ASL_FILE_HEX_OUTPUT,
    ASL_FILE_NAMESPACE_OUTPUT,
    ASL_FILE_DEBUG_OUTPUT,
    ASL_FILE_ASM_SOURCE_OUTPUT,
    ASL_FILE_C_SOURCE_OUTPUT,
    ASL_FILE_ASM_INCLUDE_OUTPUT,
    ASL_FILE_C_INCLUDE_OUTPUT

} ASL_FILE_TYPES;


#define ASL_MAX_FILE_TYPE       12
#define ASL_NUM_FILES           (ASL_MAX_FILE_TYPE + 1)


typedef struct asl_include_dir
{
    char                        *Dir;
    struct asl_include_dir      *Next;

} ASL_INCLUDE_DIR;


/* An entry in the exception list, one for each error/warning */

typedef struct asl_error_msg
{
    UINT32                      LineNumber;
    UINT32                      LogicalLineNumber;
    UINT32                      LogicalByteOffset;
    UINT32                      Column;
    char                        *Message;
    struct asl_error_msg        *Next;
    char                        *Filename;
    UINT32                      FilenameLength;
    UINT8                       MessageId;
    UINT8                       Level;

} ASL_ERROR_MSG;


/* An entry in the listing file stack (for include files) */

typedef struct asl_listing_node
{
    char                        *Filename;
    UINT32                      LineNumber;
    struct asl_listing_node     *Next;

} ASL_LISTING_NODE;


/* Callback interface for a parse tree walk */

/*
 * TBD - another copy of this is in adisasm.h, fix
 */
#ifndef ASL_WALK_CALLBACK_DEFINED
typedef
ACPI_STATUS (*ASL_WALK_CALLBACK) (
    ACPI_PARSE_OBJECT           *Op,
    UINT32                      Level,
    void                        *Context);
#define ASL_WALK_CALLBACK_DEFINED
#endif


typedef struct asl_event_info
{
    UINT64                      StartTime;
    UINT64                      EndTime;
    char                        *EventName;
    BOOLEAN                     Valid;

} ASL_EVENT_INFO;


#endif  /* __ASLTYPES_H */
