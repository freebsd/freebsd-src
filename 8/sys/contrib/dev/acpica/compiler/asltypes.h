
/******************************************************************************
 *
 * Module Name: asltypes.h - compiler data types and struct definitions
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2009, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code.  No other license or right
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
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
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
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
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
 *****************************************************************************/


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


/* An entry in the Reserved Name information table */

#define ASL_RSVD_RETURN_VALUE   0x01
#define ASL_RSVD_RESOURCE_NAME  0x02
#define ASL_RSVD_SCOPE          0x04

typedef struct asl_reserved_info
{
    char                        *Name;
    UINT8                       NumArguments;
    UINT8                       Flags;

} ASL_RESERVED_INFO;


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

typedef
ACPI_STATUS (*ASL_WALK_CALLBACK) (
    ACPI_PARSE_OBJECT           *Op,
    UINT32                      Level,
    void                        *Context);


typedef struct asl_event_info
{
    UINT64                      StartTime;
    UINT64                      EndTime;
    char                        *EventName;
    BOOLEAN                     Valid;

} ASL_EVENT_INFO;


#define ASL_WARNING             0
#define ASL_WARNING2            1
#define ASL_WARNING3            2
#define ASL_ERROR               3
#define ASL_REMARK              4
#define ASL_OPTIMIZATION        5
#define ASL_NUM_REPORT_LEVELS   6


typedef enum
{
    ASL_MSG_RESERVED = 0,
    ASL_MSG_ALPHANUMERIC_STRING,
    ASL_MSG_AML_NOT_IMPLEMENTED,
    ASL_MSG_ARG_COUNT_HI,
    ASL_MSG_ARG_COUNT_LO,
    ASL_MSG_ARG_INIT,
    ASL_MSG_BACKWARDS_OFFSET,
    ASL_MSG_BITS_TO_BYTES,
    ASL_MSG_BUFFER_LENGTH,
    ASL_MSG_BYTES_TO_BITS,
    ASL_MSG_CLOSE,
    ASL_MSG_COMPILER_INTERNAL,
    ASL_MSG_CONSTANT_EVALUATION,
    ASL_MSG_CONSTANT_FOLDED,
    ASL_MSG_CORE_EXCEPTION,
    ASL_MSG_DEBUG_FILE_OPEN,
    ASL_MSG_DEBUG_FILENAME,
    ASL_MSG_DEPENDENT_NESTING,
    ASL_MSG_DMA_CHANNEL,
    ASL_MSG_DMA_LIST,
    ASL_MSG_DUPLICATE_CASE,
    ASL_MSG_DUPLICATE_ITEM,
    ASL_MSG_EARLY_EOF,
    ASL_MSG_ENCODING_LENGTH,
    ASL_MSG_EX_INTERRUPT_LIST,
    ASL_MSG_EX_INTERRUPT_LIST_MIN,
    ASL_MSG_EX_INTERRUPT_NUMBER,
    ASL_MSG_FIELD_ACCESS_WIDTH,
    ASL_MSG_FIELD_UNIT_ACCESS_WIDTH,
    ASL_MSG_FIELD_UNIT_OFFSET,
    ASL_MSG_INCLUDE_FILE_OPEN,
    ASL_MSG_INPUT_FILE_OPEN,
    ASL_MSG_INTEGER_LENGTH,
    ASL_MSG_INTEGER_OPTIMIZATION,
    ASL_MSG_INTERRUPT_LIST,
    ASL_MSG_INTERRUPT_NUMBER,
    ASL_MSG_INVALID_CONSTANT_OP,
    ASL_MSG_INVALID_EISAID,
    ASL_MSG_INVALID_ESCAPE,
    ASL_MSG_INVALID_OPERAND,
    ASL_MSG_INVALID_PERFORMANCE,
    ASL_MSG_INVALID_PRIORITY,
    ASL_MSG_INVALID_STRING,
    ASL_MSG_INVALID_TARGET,
    ASL_MSG_INVALID_TIME,
    ASL_MSG_INVALID_TYPE,
    ASL_MSG_INVALID_UUID,
    ASL_MSG_LIST_LENGTH,
    ASL_MSG_LISTING_FILE_OPEN,
    ASL_MSG_LISTING_FILENAME,
    ASL_MSG_LOCAL_INIT,
    ASL_MSG_LONG_LINE,
    ASL_MSG_MEMORY_ALLOCATION,
    ASL_MSG_MISSING_ENDDEPENDENT,
    ASL_MSG_MISSING_STARTDEPENDENT,
    ASL_MSG_MULTIPLE_TYPES,
    ASL_MSG_NAME_EXISTS,
    ASL_MSG_NAME_OPTIMIZATION,
    ASL_MSG_NESTED_COMMENT,
    ASL_MSG_NO_CASES,
    ASL_MSG_NO_RETVAL,
    ASL_MSG_NO_WHILE,
    ASL_MSG_NON_ASCII,
    ASL_MSG_NOT_EXIST,
    ASL_MSG_NOT_FOUND,
    ASL_MSG_NOT_METHOD,
    ASL_MSG_NOT_PARAMETER,
    ASL_MSG_NOT_REACHABLE,
    ASL_MSG_OPEN,
    ASL_MSG_OUTPUT_FILE_OPEN,
    ASL_MSG_OUTPUT_FILENAME,
    ASL_MSG_PACKAGE_LENGTH,
    ASL_MSG_READ,
    ASL_MSG_RECURSION,
    ASL_MSG_REGION_BUFFER_ACCESS,
    ASL_MSG_REGION_BYTE_ACCESS,
    ASL_MSG_RESERVED_ARG_COUNT_HI,
    ASL_MSG_RESERVED_ARG_COUNT_LO,
    ASL_MSG_RESERVED_METHOD,
    ASL_MSG_RESERVED_OPERAND_TYPE,
    ASL_MSG_RESERVED_RETURN_VALUE,
    ASL_MSG_RESERVED_USE,
    ASL_MSG_RESERVED_WORD,
    ASL_MSG_RESOURCE_FIELD,
    ASL_MSG_RESOURCE_INDEX,
    ASL_MSG_RESOURCE_LIST,
    ASL_MSG_RESOURCE_SOURCE,
    ASL_MSG_RETURN_TYPES,
    ASL_MSG_SCOPE_FWD_REF,
    ASL_MSG_SCOPE_TYPE,
    ASL_MSG_SEEK,
    ASL_MSG_SINGLE_NAME_OPTIMIZATION,
    ASL_MSG_SOME_NO_RETVAL,
    ASL_MSG_SWITCH_TYPE,
    ASL_MSG_SYNC_LEVEL,
    ASL_MSG_SYNTAX,
    ASL_MSG_TABLE_SIGNATURE,
    ASL_MSG_TOO_MANY_TEMPS,
    ASL_MSG_UNKNOWN_RESERVED_NAME,
    ASL_MSG_UNREACHABLE_CODE,
    ASL_MSG_UNSUPPORTED,
    ASL_MSG_VENDOR_LIST,
    ASL_MSG_WRITE,
    ASL_MSG_MULTIPLE_DEFAULT,
    ASL_MSG_TIMEOUT,
    ASL_MSG_RESULT_NOT_USED,
    ASL_MSG_NOT_REFERENCED,
    ASL_MSG_NON_ZERO,
    ASL_MSG_STRING_LENGTH,
    ASL_MSG_SERIALIZED,
    ASL_MSG_COMPILER_RESERVED,
    ASL_MSG_NAMED_OBJECT_IN_WHILE,
    ASL_MSG_LOCAL_OUTSIDE_METHOD

} ASL_MESSAGE_IDS;

#ifdef ASL_EXCEPTIONS

char                        *AslMessages [] = {
/*    The zeroth message is resesrved */    "",
/*    ASL_MSG_ALPHANUMERIC_STRING */        "String must be entirely alphanumeric",
/*    ASL_MSG_AML_NOT_IMPLEMENTED */        "Opcode is not implemented in compiler AML code generator",
/*    ASL_MSG_ARG_COUNT_HI */               "Too many arguments",
/*    ASL_MSG_ARG_COUNT_LO */               "Too few arguments",
/*    ASL_MSG_ARG_INIT */                   "Method argument is not initialized",
/*    ASL_MSG_BACKWARDS_OFFSET */           "Invalid backwards offset",
/*    ASL_MSG_BITS_TO_BYTES */              "Field offset is in bits, but a byte offset is required",
/*    ASL_MSG_BUFFER_LENGTH */              "Effective AML buffer length is zero",
/*    ASL_MSG_BYTES_TO_BITS */              "Field offset is in bytes, but a bit offset is required",
/*    ASL_MSG_CLOSE */                      "Could not close file",
/*    ASL_MSG_COMPILER_INTERNAL */          "Internal compiler error",
/*    ASL_MSG_CONSTANT_EVALUATION */        "Could not evaluate constant expression",
/*    ASL_MSG_CONSTANT_FOLDED */            "Constant expression evaluated and reduced",
/*    ASL_MSG_CORE_EXCEPTION */             "From ACPI CA Subsystem",
/*    ASL_MSG_DEBUG_FILE_OPEN */            "Could not open debug file",
/*    ASL_MSG_DEBUG_FILENAME */             "Could not create debug filename",
/*    ASL_MSG_DEPENDENT_NESTING */          "Dependent function macros cannot be nested",\
/*    ASL_MSG_DMA_CHANNEL */                "Invalid DMA channel (must be 0-7)",
/*    ASL_MSG_DMA_LIST */                   "Too many DMA channels (8 max)",
/*    ASL_MSG_DUPLICATE_CASE */             "Case value already specified",
/*    ASL_MSG_DUPLICATE_ITEM */             "Duplicate value in list",
/*    ASL_MSG_EARLY_EOF */                  "Premature end-of-file reached",
/*    ASL_MSG_ENCODING_LENGTH */            "Package length too long to encode",
/*    ASL_MSG_EX_INTERRUPT_LIST */          "Too many interrupts (255 max)",
/*    ASL_MSG_EX_INTERRUPT_LIST_MIN */      "Too few interrupts (1 minimum required)",
/*    ASL_MSG_EX_INTERRUPT_NUMBER */        "Invalid interrupt number (must be 32 bits)",
/*    ASL_MSG_FIELD_ACCESS_WIDTH */         "Access width is greater than region size",
/*    ASL_MSG_FIELD_UNIT_ACCESS_WIDTH */    "Access width of Field Unit extends beyond region limit",
/*    ASL_MSG_FIELD_UNIT_OFFSET */          "Field Unit extends beyond region limit",
/*    ASL_MSG_INCLUDE_FILE_OPEN */          "Could not open include file",
/*    ASL_MSG_INPUT_FILE_OPEN */            "Could not open input file",
/*    ASL_MSG_INTEGER_LENGTH */             "64-bit integer in 32-bit table, truncating",
/*    ASL_MSG_INTEGER_OPTIMIZATION */       "Integer optimized to single-byte AML opcode",
/*    ASL_MSG_INTERRUPT_LIST */             "Too many interrupts (16 max)",
/*    ASL_MSG_INTERRUPT_NUMBER */           "Invalid interrupt number (must be 0-15)",
/*    ASL_MSG_INVALID_CONSTANT_OP */        "Invalid operator in constant expression (not type 3/4/5)",
/*    ASL_MSG_INVALID_EISAID */             "EISAID string must be of the form \"UUUXXXX\" (3 uppercase, 4 hex digits)",
/*    ASL_MSG_INVALID_ESCAPE */             "Invalid or unknown escape sequence",
/*    ASL_MSG_INVALID_OPERAND */            "Invalid operand",
/*    ASL_MSG_INVALID_PERFORMANCE */        "Invalid performance/robustness value",
/*    ASL_MSG_INVALID_PRIORITY */           "Invalid priority value",
/*    ASL_MSG_INVALID_STRING */             "Invalid Hex/Octal Escape - Non-ASCII or NULL",
/*    ASL_MSG_INVALID_TARGET */             "Target operand not allowed in constant expression",
/*    ASL_MSG_INVALID_TIME */               "Time parameter too long (255 max)",
/*    ASL_MSG_INVALID_TYPE */               "Invalid type",
/*    ASL_MSG_INVALID_UUID */               "UUID string must be of the form \"aabbccdd-eeff-gghh-iijj-kkllmmnnoopp\"",
/*    ASL_MSG_LIST_LENGTH */                "Initializer list too long",
/*    ASL_MSG_LISTING_FILE_OPEN */          "Could not open listing file",
/*    ASL_MSG_LISTING_FILENAME */           "Could not create listing filename",
/*    ASL_MSG_LOCAL_INIT */                 "Method local variable is not initialized",
/*    ASL_MSG_LONG_LINE */                  "Splitting long input line",
/*    ASL_MSG_MEMORY_ALLOCATION */          "Memory allocation failure",
/*    ASL_MSG_MISSING_ENDDEPENDENT */       "Missing EndDependentFn() macro in dependent resource list",
/*    ASL_MSG_MISSING_STARTDEPENDENT */     "Missing StartDependentFn() macro in dependent resource list",
/*    ASL_MSG_MULTIPLE_TYPES */             "Multiple types",
/*    ASL_MSG_NAME_EXISTS */                "Name already exists in scope",
/*    ASL_MSG_NAME_OPTIMIZATION */          "NamePath optimized",
/*    ASL_MSG_NESTED_COMMENT */             "Nested comment found",
/*    ASL_MSG_NO_CASES */                   "No Case statements under Switch",
/*    ASL_MSG_NO_RETVAL */                  "Called method returns no value",
/*    ASL_MSG_NO_WHILE */                   "No enclosing While statement",
/*    ASL_MSG_NON_ASCII */                  "Invalid characters found in file",
/*    ASL_MSG_NOT_EXIST */                  "Object does not exist",
/*    ASL_MSG_NOT_FOUND */                  "Object not found or not accessible from scope",
/*    ASL_MSG_NOT_METHOD */                 "Not a control method, cannot invoke",
/*    ASL_MSG_NOT_PARAMETER */              "Not a parameter, used as local only",
/*    ASL_MSG_NOT_REACHABLE */              "Object is not accessible from this scope",
/*    ASL_MSG_OPEN */                       "Could not open file",
/*    ASL_MSG_OUTPUT_FILE_OPEN */           "Could not open output AML file",
/*    ASL_MSG_OUTPUT_FILENAME */            "Could not create output filename",
/*    ASL_MSG_PACKAGE_LENGTH */             "Effective AML package length is zero",
/*    ASL_MSG_READ */                       "Could not read file",
/*    ASL_MSG_RECURSION */                  "Recursive method call",
/*    ASL_MSG_REGION_BUFFER_ACCESS */       "Host Operation Region requires BufferAcc access",
/*    ASL_MSG_REGION_BYTE_ACCESS */         "Host Operation Region requires ByteAcc access",
/*    ASL_MSG_RESERVED_ARG_COUNT_HI */      "Reserved method has too many arguments",
/*    ASL_MSG_RESERVED_ARG_COUNT_LO */      "Reserved method has too few arguments",
/*    ASL_MSG_RESERVED_METHOD */            "Reserved name must be a control method",
/*    ASL_MSG_RESERVED_OPERAND_TYPE */      "Invalid operand type for reserved name, must be",
/*    ASL_MSG_RESERVED_RETURN_VALUE */      "Reserved method must return a value",
/*    ASL_MSG_RESERVED_USE */               "Invalid use of reserved name",
/*    ASL_MSG_RESERVED_WORD */              "Use of reserved name",
/*    ASL_MSG_RESOURCE_FIELD */             "Resource field name cannot be used as a target",
/*    ASL_MSG_RESOURCE_INDEX */             "Missing ResourceSourceIndex (required)",
/*    ASL_MSG_RESOURCE_LIST */              "Too many resource items (internal error)",
/*    ASL_MSG_RESOURCE_SOURCE */            "Missing ResourceSource string (required)",
/*    ASL_MSG_RETURN_TYPES */               "Not all control paths return a value",
/*    ASL_MSG_SCOPE_FWD_REF */              "Forward references from Scope operator not allowed",
/*    ASL_MSG_SCOPE_TYPE */                 "Existing object has invalid type for Scope operator",
/*    ASL_MSG_SEEK */                       "Could not seek file",
/*    ASL_MSG_SINGLE_NAME_OPTIMIZATION */   "NamePath optimized to NameSeg (uses run-time search path)",
/*    ASL_MSG_SOME_NO_RETVAL */             "Called method may not always return a value",
/*    ASL_MSG_SWITCH_TYPE */                "Switch expression is not a static Integer/Buffer/String data type, defaulting to Integer",
/*    ASL_MSG_SYNC_LEVEL */                 "SyncLevel must be in the range 0-15",
/*    ASL_MSG_SYNTAX */                     "",
/*    ASL_MSG_TABLE_SIGNATURE */            "Invalid Table Signature",
/*    ASL_MSG_TOO_MANY_TEMPS */             "Method requires too many temporary variables (_T_x)",
/*    ASL_MSG_UNKNOWN_RESERVED_NAME */      "Unknown reserved name",
/*    ASL_MSG_UNREACHABLE_CODE */           "Statement is unreachable",
/*    ASL_MSG_UNSUPPORTED */                "Unsupported feature",
/*    ASL_MSG_VENDOR_LIST */                "Too many vendor data bytes (7 max)",
/*    ASL_MSG_WRITE */                      "Could not write file",
/*    ASL_MSG_MULTIPLE_DEFAULT */           "More than one Default statement within Switch construct",
/*    ASL_MSG_TIMEOUT */                    "Possible operator timeout is ignored",
/*    ASL_MSG_RESULT_NOT_USED */            "Result is not used, operator has no effect",
/*    ASL_MSG_NOT_REFERENCED */             "Namespace object is not referenced",
/*    ASL_MSG_NON_ZERO */                   "Operand evaluates to zero",
/*    ASL_MSG_STRING_LENGTH */              "String literal too long",
/*    ASL_MSG_SERIALIZED */                 "Control Method marked Serialized",
/*    ASL_MSG_COMPILER_RESERVED */          "Use of compiler reserved name",
/*    ASL_MSG_NAMED_OBJECT_IN_WHILE */      "Creating a named object in a While loop",
/*    ASL_MSG_LOCAL_OUTSIDE_METHOD */       "Local or Arg used outside a control method"

};


char                    *AslErrorLevel [ASL_NUM_REPORT_LEVELS] = {
    "Warning ",
    "Warning ",
    "Warning ",
    "Error   ",
    "Remark  ",
    "Optimize"
};

#define ASL_ERROR_LEVEL_LENGTH          8       /* Length of strings above */

/* Exception counters */

UINT32                  Gbl_ExceptionCount[ASL_NUM_REPORT_LEVELS] = {0,0,0,0,0,0};

#endif


#endif  /* __ASLTYPES_H */
