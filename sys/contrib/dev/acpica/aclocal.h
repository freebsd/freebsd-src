/******************************************************************************
 *
 * Name: aclocal.h - Internal data types used across the ACPI subsystem
 *       $Revision: 173 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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

#ifndef __ACLOCAL_H__
#define __ACLOCAL_H__


#define WAIT_FOREVER                    ((UINT32) -1)

typedef void*                           ACPI_MUTEX;
typedef UINT32                          ACPI_MUTEX_HANDLE;


/* Total number of aml opcodes defined */

#define AML_NUM_OPCODES                 0x7E



/*****************************************************************************
 *
 * Mutex typedefs and structs
 *
 ****************************************************************************/


/*
 * Predefined handles for the mutex objects used within the subsystem
 * All mutex objects are automatically created by AcpiUtMutexInitialize.
 *
 * The acquire/release ordering protocol is implied via this list.  Mutexes
 * with a lower value must be acquired before mutexes with a higher value.
 *
 * NOTE: any changes here must be reflected in the AcpiGbl_MutexNames table also!
 */

#define ACPI_MTX_EXECUTE                0
#define ACPI_MTX_INTERPRETER            1
#define ACPI_MTX_PARSER                 2
#define ACPI_MTX_DISPATCHER             3
#define ACPI_MTX_TABLES                 4
#define ACPI_MTX_OP_REGIONS             5
#define ACPI_MTX_NAMESPACE              6
#define ACPI_MTX_EVENTS                 7
#define ACPI_MTX_HARDWARE               8
#define ACPI_MTX_CACHES                 9
#define ACPI_MTX_MEMORY                 10
#define ACPI_MTX_DEBUG_CMD_COMPLETE     11
#define ACPI_MTX_DEBUG_CMD_READY        12

#define MAX_MTX                         12
#define NUM_MTX                         MAX_MTX+1


#if defined(ACPI_DEBUG) || defined(ENABLE_DEBUGGER)
#ifdef DEFINE_ACPI_GLOBALS

/* Names for the mutexes used in the subsystem */

static NATIVE_CHAR          *AcpiGbl_MutexNames[] =
{
    "ACPI_MTX_Execute",
    "ACPI_MTX_Interpreter",
    "ACPI_MTX_Parser",
    "ACPI_MTX_Dispatcher",
    "ACPI_MTX_Tables",
    "ACPI_MTX_OpRegions",
    "ACPI_MTX_Namespace",
    "ACPI_MTX_Events",
    "ACPI_MTX_Hardware",
    "ACPI_MTX_Caches",
    "ACPI_MTX_Memory",
    "ACPI_MTX_DebugCmdComplete",
    "ACPI_MTX_DebugCmdReady",
};

#endif
#endif


/* Table for the global mutexes */

typedef struct AcpiMutexInfo
{
    ACPI_MUTEX                  Mutex;
    UINT32                      UseCount;
    UINT32                      OwnerId;

} ACPI_MUTEX_INFO;

/* This owner ID means that the mutex is not in use (unlocked) */

#define ACPI_MUTEX_NOT_ACQUIRED         (UINT32) (-1)


/* Lock flag parameter for various interfaces */

#define ACPI_MTX_DO_NOT_LOCK            0
#define ACPI_MTX_LOCK                   1


typedef UINT16                          ACPI_OWNER_ID;
#define ACPI_OWNER_TYPE_TABLE           0x0
#define ACPI_OWNER_TYPE_METHOD          0x1
#define ACPI_FIRST_METHOD_ID            0x0000
#define ACPI_FIRST_TABLE_ID             0x8000

/* TBD: [Restructure] get rid of the need for this! */

#define TABLE_ID_DSDT                   (ACPI_OWNER_ID) 0x8000


/* Field access granularities */

#define ACPI_FIELD_BYTE_GRANULARITY     1
#define ACPI_FIELD_WORD_GRANULARITY     2
#define ACPI_FIELD_DWORD_GRANULARITY    4
#define ACPI_FIELD_QWORD_GRANULARITY    8

/*****************************************************************************
 *
 * Namespace typedefs and structs
 *
 ****************************************************************************/


/* Operational modes of the AML interpreter/scanner */

typedef enum
{
    ACPI_IMODE_LOAD_PASS1               = 0x01,
    ACPI_IMODE_LOAD_PASS2               = 0x02,
    ACPI_IMODE_EXECUTE                  = 0x0E

} ACPI_INTERPRETER_MODE;


/*
 * The Node describes a named object that appears in the AML
 * An AcpiNode is used to store Nodes.
 *
 * DataType is used to differentiate between internal descriptors, and MUST
 * be the first byte in this structure.
 */

typedef union acpi_name_union
{
    UINT32                  Integer;
    char                    Ascii[4];
} ACPI_NAME_UNION;

typedef struct acpi_node
{
    UINT8                   Descriptor;     /* Used to differentiate object descriptor types */
    UINT8                   Type;           /* Type associated with this name */
    UINT16                  OwnerId;
    ACPI_NAME_UNION         Name;           /* ACPI Name, always 4 chars per ACPI spec */


    union acpi_operand_obj  *Object;        /* Pointer to attached ACPI object (optional) */
    struct acpi_node        *Child;         /* first child */
    struct acpi_node        *Peer;          /* Next peer*/
    UINT16                  ReferenceCount; /* Current count of references and children */
    UINT8                   Flags;

} ACPI_NAMESPACE_NODE;


#define ACPI_ENTRY_NOT_FOUND            NULL


/* Node flags */

#define ANOBJ_RESERVED                  0x01
#define ANOBJ_END_OF_PEER_LIST          0x02
#define ANOBJ_DATA_WIDTH_32             0x04     /* Parent table is 64-bits */
#define ANOBJ_METHOD_ARG                0x08
#define ANOBJ_METHOD_LOCAL              0x10
#define ANOBJ_METHOD_NO_RETVAL          0x20
#define ANOBJ_METHOD_SOME_NO_RETVAL     0x40

#define ANOBJ_IS_BIT_OFFSET             0x80


/*
 * ACPI Table Descriptor.  One per ACPI table
 */
typedef struct AcpiTableDesc
{
    struct AcpiTableDesc    *Prev;
    struct AcpiTableDesc    *Next;
    struct AcpiTableDesc    *InstalledDesc;
    ACPI_TABLE_HEADER       *Pointer;
    void                    *BasePointer;
    UINT8                   *AmlStart;
    UINT64                  PhysicalAddress;
    UINT32                  AmlLength;
    ACPI_SIZE               Length;
    UINT32                  Count;
    ACPI_OWNER_ID           TableId;
    UINT8                   Type;
    UINT8                   Allocation;
    BOOLEAN                 LoadedIntoNamespace;

} ACPI_TABLE_DESC;


typedef struct
{
    NATIVE_CHAR             *SearchFor;
    ACPI_HANDLE             *List;
    UINT32                  *Count;

} ACPI_FIND_CONTEXT;


typedef struct
{
    ACPI_NAMESPACE_NODE     *Node;
} ACPI_NS_SEARCH_DATA;


/*
 * Predefined Namespace items
 */
typedef struct
{
    NATIVE_CHAR             *Name;
    UINT8                   Type;
    NATIVE_CHAR             *Val;

} ACPI_PREDEFINED_NAMES;


/* Object types used during package copies */


#define ACPI_COPY_TYPE_SIMPLE           0
#define ACPI_COPY_TYPE_PACKAGE          1

/* Info structure used to convert external<->internal namestrings */

typedef struct acpi_namestring_info
{
    NATIVE_CHAR             *ExternalName;
    NATIVE_CHAR             *NextExternalChar;
    NATIVE_CHAR             *InternalName;
    UINT32                  Length;
    UINT32                  NumSegments;
    UINT32                  NumCarats;
    BOOLEAN                 FullyQualified;

} ACPI_NAMESTRING_INFO;


/* Field creation info */

typedef struct
{
    ACPI_NAMESPACE_NODE     *RegionNode;
    ACPI_NAMESPACE_NODE     *FieldNode;
    ACPI_NAMESPACE_NODE     *RegisterNode;
    ACPI_NAMESPACE_NODE     *DataRegisterNode;
    UINT32                  BankValue;
    UINT32                  FieldBitPosition;
    UINT32                  FieldBitLength;
    UINT8                   FieldFlags;
    UINT8                   Attribute;
    UINT8                   FieldType;

} ACPI_CREATE_FIELD_INFO;


/*****************************************************************************
 *
 * Event typedefs and structs
 *
 ****************************************************************************/

/* Information about each GPE register block */

typedef struct
{
    UINT8                   AddressSpaceId;
    ACPI_GENERIC_ADDRESS    *BlockAddress;
    UINT16                  RegisterCount;
    UINT8                   BlockBaseNumber;

} ACPI_GPE_BLOCK_INFO;

/* Information about a particular GPE register pair */

typedef struct
{
    ACPI_GENERIC_ADDRESS    StatusAddress;  /* Address of status reg */
    ACPI_GENERIC_ADDRESS    EnableAddress;  /* Address of enable reg */
    UINT8                   Status;         /* Current value of status reg */
    UINT8                   Enable;         /* Current value of enable reg */
    UINT8                   WakeEnable;     /* Mask of bits to keep enabled when sleeping */
    UINT8                   BaseGpeNumber;  /* Base GPE number for this register */

} ACPI_GPE_REGISTER_INFO;


#define ACPI_GPE_LEVEL_TRIGGERED        1
#define ACPI_GPE_EDGE_TRIGGERED         2


/* Information about each particular GPE level */

typedef struct
{
    ACPI_HANDLE             MethodHandle;   /* Method handle for direct (fast) execution */
    ACPI_GPE_HANDLER        Handler;        /* Address of handler, if any */
    void                    *Context;       /* Context to be passed to handler */
    UINT8                   Type;           /* Level or Edge */
    UINT8                   BitMask;


} ACPI_GPE_NUMBER_INFO;


typedef struct
{
    UINT8                   NumberIndex;

} ACPI_GPE_INDEX_INFO;

/* Information about each particular fixed event */

typedef struct
{
    ACPI_EVENT_HANDLER      Handler;        /* Address of handler. */
    void                    *Context;       /* Context to be passed to handler */

} ACPI_FIXED_EVENT_HANDLER;


typedef struct
{
    UINT8                   StatusRegisterId;
    UINT8                   EnableRegisterId;
    UINT16                  StatusBitMask;
    UINT16                  EnableBitMask;

} ACPI_FIXED_EVENT_INFO;

/* Information used during field processing */

typedef struct
{
    UINT8                   SkipField;
    UINT8                   FieldFlag;
    UINT32                  PkgLength;

} ACPI_FIELD_INFO;


/*****************************************************************************
 *
 * Generic "state" object for stacks
 *
 ****************************************************************************/


#define ACPI_CONTROL_NORMAL                  0xC0
#define ACPI_CONTROL_CONDITIONAL_EXECUTING   0xC1
#define ACPI_CONTROL_PREDICATE_EXECUTING     0xC2
#define ACPI_CONTROL_PREDICATE_FALSE         0xC3
#define ACPI_CONTROL_PREDICATE_TRUE          0xC4


/* Forward declarations */
struct acpi_walk_state;
struct acpi_obj_mutex;
union acpi_parse_obj;


#define ACPI_STATE_COMMON                  /* Two 32-bit fields and a pointer */\
    UINT8                   DataType;           /* To differentiate various internal objs */\
    UINT8                   Flags;      \
    UINT16                  Value;      \
    UINT16                  State;      \
    UINT16                  Reserved;   \
    void                    *Next;      \

typedef struct acpi_common_state
{
    ACPI_STATE_COMMON
} ACPI_COMMON_STATE;


/*
 * Update state - used to traverse complex objects such as packages
 */
typedef struct acpi_update_state
{
    ACPI_STATE_COMMON
    union acpi_operand_obj  *Object;

} ACPI_UPDATE_STATE;


/*
 * Pkg state - used to traverse nested package structures
 */
typedef struct acpi_pkg_state
{
    ACPI_STATE_COMMON
    union acpi_operand_obj  *SourceObject;
    union acpi_operand_obj  *DestObject;
    struct acpi_walk_state  *WalkState;
    void                    *ThisTargetObj;
    UINT32                  NumPackages;
    UINT16                  Index;

} ACPI_PKG_STATE;


/*
 * Control state - one per if/else and while constructs.
 * Allows nesting of these constructs
 */
typedef struct acpi_control_state
{
    ACPI_STATE_COMMON
    union acpi_parse_obj    *PredicateOp;
    UINT8                   *AmlPredicateStart;     /* Start of if/while predicate */
    UINT8                   *PackageEnd;            /* End of if/while block */
    UINT16                  Opcode;

} ACPI_CONTROL_STATE;


/*
 * Scope state - current scope during namespace lookups
 */
typedef struct acpi_scope_state
{
    ACPI_STATE_COMMON
    ACPI_NAMESPACE_NODE     *Node;

} ACPI_SCOPE_STATE;


typedef struct acpi_pscope_state
{
    ACPI_STATE_COMMON
    union acpi_parse_obj    *Op;                    /* current op being parsed */
    UINT8                   *ArgEnd;                /* current argument end */
    UINT8                   *PkgEnd;                /* current package end */
    UINT32                  ArgList;                /* next argument to parse */
    UINT32                  ArgCount;               /* Number of fixed arguments */

} ACPI_PSCOPE_STATE;


/*
 * Thread state - one per thread across multiple walk states.  Multiple walk
 * states are created when there are nested control methods executing.
 */
typedef struct acpi_thread_state
{
    ACPI_STATE_COMMON
    struct acpi_walk_state  *WalkStateList;         /* Head of list of WalkStates for this thread */
    union acpi_operand_obj  *AcquiredMutexList;     /* List of all currently acquired mutexes */
    UINT32                  ThreadId;               /* Running thread ID */
    UINT16                  CurrentSyncLevel;       /* Mutex Sync (nested acquire) level */

} ACPI_THREAD_STATE;


/*
 * Result values - used to accumulate the results of nested
 * AML arguments
 */
typedef struct acpi_result_values
{
    ACPI_STATE_COMMON
    union acpi_operand_obj  *ObjDesc [OBJ_NUM_OPERANDS];
    UINT8                   NumResults;
    UINT8                   LastInsert;

} ACPI_RESULT_VALUES;


typedef
ACPI_STATUS (*ACPI_PARSE_DOWNWARDS) (
    struct acpi_walk_state  *WalkState,
    union acpi_parse_obj    **OutOp);

typedef
ACPI_STATUS (*ACPI_PARSE_UPWARDS) (
    struct acpi_walk_state  *WalkState);


/*
 * Notify info - used to pass info to the deferred notify
 * handler/dispatcher.
 */
typedef struct acpi_notify_info
{
    ACPI_STATE_COMMON
    ACPI_NAMESPACE_NODE     *Node;
    union acpi_operand_obj  *HandlerObj;

} ACPI_NOTIFY_INFO;


/* Generic state is union of structs above */

typedef union acpi_gen_state
{
    ACPI_COMMON_STATE       Common;
    ACPI_CONTROL_STATE      Control;
    ACPI_UPDATE_STATE       Update;
    ACPI_SCOPE_STATE        Scope;
    ACPI_PSCOPE_STATE       ParseScope;
    ACPI_PKG_STATE          Pkg;
    ACPI_THREAD_STATE       Thread;
    ACPI_RESULT_VALUES      Results;
    ACPI_NOTIFY_INFO        Notify;

} ACPI_GENERIC_STATE;


/*****************************************************************************
 *
 * Interpreter typedefs and structs
 *
 ****************************************************************************/

typedef
ACPI_STATUS (*ACPI_EXECUTE_OP) (
    struct acpi_walk_state  *WalkState);


/*****************************************************************************
 *
 * Parser typedefs and structs
 *
 ****************************************************************************/

/*
 * AML opcode, name, and argument layout
 */
typedef struct acpi_opcode_info
{
#if defined(ACPI_DISASSEMBLER) || defined(ACPI_DEBUG)
    NATIVE_CHAR             *Name;          /* Opcode name (disassembler/debug only) */
#endif
    UINT32                  ParseArgs;      /* Grammar/Parse time arguments */
    UINT32                  RuntimeArgs;    /* Interpret time arguments */
    UINT32                  Flags;          /* Misc flags */
    UINT8                   ObjectType;     /* Corresponding internal object type */
    UINT8                   Class;          /* Opcode class */
    UINT8                   Type;           /* Opcode type */

} ACPI_OPCODE_INFO;


typedef union acpi_parse_val
{
    ACPI_INTEGER            Integer;        /* integer constant (Up to 64 bits) */
    UINT64_STRUCT           Integer64;      /* Structure overlay for 2 32-bit Dwords */
    UINT32                  Integer32;      /* integer constant, 32 bits only */
    UINT16                  Integer16;      /* integer constant, 16 bits only */
    UINT8                   Integer8;       /* integer constant, 8 bits only */
    UINT32                  Size;           /* bytelist or field size */
    NATIVE_CHAR             *String;        /* NULL terminated string */
    UINT8                   *Buffer;        /* buffer or string */
    NATIVE_CHAR             *Name;          /* NULL terminated string */
    union acpi_parse_obj    *Arg;           /* arguments and contained ops */

} ACPI_PARSE_VALUE;


#define ACPI_PARSE_COMMON \
    UINT8                   DataType;       /* To differentiate various internal objs */\
    UINT8                   Flags;          /* Type of Op */\
    UINT16                  AmlOpcode;      /* AML opcode */\
    UINT32                  AmlOffset;      /* offset of declaration in AML */\
    union acpi_parse_obj    *Parent;        /* parent op */\
    union acpi_parse_obj    *Next;          /* next op */\
    ACPI_DISASM_ONLY_MEMBERS (\
    UINT8                   DisasmFlags;    /* Used during AML disassembly */\
    UINT8                   DisasmOpcode;   /* Subtype used for disassembly */\
    NATIVE_CHAR             AmlOpName[16])  /* op name (debug only) */\
                                            /* NON-DEBUG members below: */\
    ACPI_NAMESPACE_NODE     *Node;          /* for use by interpreter */\
    ACPI_PARSE_VALUE        Value;          /* Value or args associated with the opcode */\

#define ACPI_DASM_BUFFER        0x00
#define ACPI_DASM_RESOURCE      0x01
#define ACPI_DASM_STRING        0x02
#define ACPI_DASM_UNICODE       0x03
#define ACPI_DASM_EISAID        0x04
#define ACPI_DASM_MATCHOP       0x05

/*
 * generic operation (for example:  If, While, Store)
 */
typedef struct acpi_parseobj_common
{
    ACPI_PARSE_COMMON
} ACPI_PARSE_OBJ_COMMON;


/*
 * Extended Op for named ops (Scope, Method, etc.), deferred ops (Methods and OpRegions),
 * and bytelists.
 */
typedef struct acpi_parseobj_named
{
    ACPI_PARSE_COMMON
    UINT8                   *Path;
    UINT8                   *Data;          /* AML body or bytelist data */
    UINT32                  Length;         /* AML length */
    UINT32                  Name;           /* 4-byte name or zero if no name */

} ACPI_PARSE_OBJ_NAMED;


/* The parse node is the fundamental element of the parse tree */

typedef struct acpi_parseobj_asl
{
    ACPI_PARSE_COMMON

    union acpi_parse_obj        *Child;


    union acpi_parse_obj        *ParentMethod;
    char                        *Filename;
    char                        *ExternalName;
    char                        *Namepath;
    UINT32                      ExtraValue;
    UINT32                      Column;
    UINT32                      LineNumber;
    UINT32                      LogicalLineNumber;
    UINT32                      LogicalByteOffset;
    UINT32                      EndLine;
    UINT32                      EndLogicalLine;
    UINT32                      AcpiBtype;
    UINT32                      AmlLength;
    UINT32                      AmlSubtreeLength;
    UINT32                      FinalAmlLength;
    UINT32                      FinalAmlOffset;
    UINT16                      ParseOpcode;
    UINT16                      CompileFlags;
    UINT8                       AmlOpcodeLength;
    UINT8                       AmlPkgLenBytes;
    UINT8                       Extra;
    char                        ParseOpName[12];

} ACPI_PARSE_OBJ_ASL;


typedef union acpi_parse_obj
{
    ACPI_PARSE_OBJ_COMMON       Common;
    ACPI_PARSE_OBJ_NAMED        Named;
    ACPI_PARSE_OBJ_ASL          Asl;

} ACPI_PARSE_OBJECT;



/*
 * Parse state - one state per parser invocation and each control
 * method.
 */
typedef struct acpi_parse_state
{
    UINT32                  AmlSize;
    UINT8                   *AmlStart;      /* first AML byte */
    UINT8                   *Aml;           /* next AML byte */
    UINT8                   *AmlEnd;        /* (last + 1) AML byte */
    UINT8                   *PkgStart;      /* current package begin */
    UINT8                   *PkgEnd;        /* current package end */
    union acpi_parse_obj    *StartOp;       /* root of parse tree */
    struct acpi_node        *StartNode;
    union acpi_gen_state    *Scope;         /* current scope */
    union acpi_parse_obj    *StartScope;

} ACPI_PARSE_STATE;


/* Parse object flags */

#define ACPI_PARSEOP_GENERIC                    0x01
#define ACPI_PARSEOP_NAMED                      0x02
#define ACPI_PARSEOP_DEFERRED                   0x04
#define ACPI_PARSEOP_BYTELIST                   0x08
#define ACPI_PARSEOP_IN_CACHE                   0x80

/* Parse object DisasmFlags */

#define ACPI_PARSEOP_IGNORE                     0x01
#define ACPI_PARSEOP_PARAMLIST                  0x02
#define ACPI_PARSEOP_EMPTY_TERMLIST             0x04
#define ACPI_PARSEOP_SPECIAL                    0x10


/*****************************************************************************
 *
 * Hardware (ACPI registers) and PNP
 *
 ****************************************************************************/

#define PCI_ROOT_HID_STRING         "PNP0A03"

typedef struct
{
    UINT8                   ParentRegister;
    UINT8                   BitPosition;
    UINT16                  AccessBitMask;

} ACPI_BIT_REGISTER_INFO;


/*
 * Register IDs
 * These are the full ACPI registers
 */
#define ACPI_REGISTER_PM1_STATUS                0x01
#define ACPI_REGISTER_PM1_ENABLE                0x02
#define ACPI_REGISTER_PM1_CONTROL               0x03
#define ACPI_REGISTER_PM1A_CONTROL              0x04
#define ACPI_REGISTER_PM1B_CONTROL              0x05
#define ACPI_REGISTER_PM2_CONTROL               0x06
#define ACPI_REGISTER_PM_TIMER                  0x07
#define ACPI_REGISTER_PROCESSOR_BLOCK           0x08
#define ACPI_REGISTER_SMI_COMMAND_BLOCK         0x09


/* Masks used to access the BitRegisters */

#define ACPI_BITMASK_TIMER_STATUS               0x0001
#define ACPI_BITMASK_BUS_MASTER_STATUS          0x0010
#define ACPI_BITMASK_GLOBAL_LOCK_STATUS         0x0020
#define ACPI_BITMASK_POWER_BUTTON_STATUS        0x0100
#define ACPI_BITMASK_SLEEP_BUTTON_STATUS        0x0200
#define ACPI_BITMASK_RT_CLOCK_STATUS            0x0400
#define ACPI_BITMASK_WAKE_STATUS                0x8000

#define ACPI_BITMASK_ALL_FIXED_STATUS           (ACPI_BITMASK_TIMER_STATUS          | \
                                                 ACPI_BITMASK_BUS_MASTER_STATUS     | \
                                                 ACPI_BITMASK_GLOBAL_LOCK_STATUS    | \
                                                 ACPI_BITMASK_POWER_BUTTON_STATUS   | \
                                                 ACPI_BITMASK_SLEEP_BUTTON_STATUS   | \
                                                 ACPI_BITMASK_RT_CLOCK_STATUS       | \
                                                 ACPI_BITMASK_WAKE_STATUS)

#define ACPI_BITMASK_TIMER_ENABLE               0x0001
#define ACPI_BITMASK_GLOBAL_LOCK_ENABLE         0x0020
#define ACPI_BITMASK_POWER_BUTTON_ENABLE        0x0100
#define ACPI_BITMASK_SLEEP_BUTTON_ENABLE        0x0200
#define ACPI_BITMASK_RT_CLOCK_ENABLE            0x0400

#define ACPI_BITMASK_SCI_ENABLE                 0x0001
#define ACPI_BITMASK_BUS_MASTER_RLD             0x0002
#define ACPI_BITMASK_GLOBAL_LOCK_RELEASE        0x0004
#define ACPI_BITMASK_SLEEP_TYPE_X               0x1C00
#define ACPI_BITMASK_SLEEP_ENABLE               0x2000

#define ACPI_BITMASK_ARB_DISABLE                0x0001


/* Raw bit position of each BitRegister */

#define ACPI_BITPOSITION_TIMER_STATUS           0x00
#define ACPI_BITPOSITION_BUS_MASTER_STATUS      0x04
#define ACPI_BITPOSITION_GLOBAL_LOCK_STATUS     0x05
#define ACPI_BITPOSITION_POWER_BUTTON_STATUS    0x08
#define ACPI_BITPOSITION_SLEEP_BUTTON_STATUS    0x09
#define ACPI_BITPOSITION_RT_CLOCK_STATUS        0x0A
#define ACPI_BITPOSITION_WAKE_STATUS            0x0F

#define ACPI_BITPOSITION_TIMER_ENABLE           0x00
#define ACPI_BITPOSITION_GLOBAL_LOCK_ENABLE     0x05
#define ACPI_BITPOSITION_POWER_BUTTON_ENABLE    0x08
#define ACPI_BITPOSITION_SLEEP_BUTTON_ENABLE    0x09
#define ACPI_BITPOSITION_RT_CLOCK_ENABLE        0x0A

#define ACPI_BITPOSITION_SCI_ENABLE             0x00
#define ACPI_BITPOSITION_BUS_MASTER_RLD         0x01
#define ACPI_BITPOSITION_GLOBAL_LOCK_RELEASE    0x02
#define ACPI_BITPOSITION_SLEEP_TYPE_X           0x0A
#define ACPI_BITPOSITION_SLEEP_ENABLE           0x0D

#define ACPI_BITPOSITION_ARB_DISABLE            0x00


/*****************************************************************************
 *
 * Resource descriptors
 *
 ****************************************************************************/


/* ResourceType values */

#define ACPI_RESOURCE_TYPE_MEMORY_RANGE         0
#define ACPI_RESOURCE_TYPE_IO_RANGE             1
#define ACPI_RESOURCE_TYPE_BUS_NUMBER_RANGE     2

/* Resource descriptor types and masks */

#define ACPI_RDESC_TYPE_LARGE                   0x80
#define ACPI_RDESC_TYPE_SMALL                   0x00

#define ACPI_RDESC_TYPE_MASK                    0x80
#define ACPI_RDESC_SMALL_MASK                   0x78 /* Only bits 6:3 contain the type */


/*
 * Small resource descriptor types
 * Note: The 3 length bits (2:0) must be zero
 */
#define ACPI_RDESC_TYPE_IRQ_FORMAT              0x20
#define ACPI_RDESC_TYPE_DMA_FORMAT              0x28
#define ACPI_RDESC_TYPE_START_DEPENDENT         0x30
#define ACPI_RDESC_TYPE_END_DEPENDENT           0x38
#define ACPI_RDESC_TYPE_IO_PORT                 0x40
#define ACPI_RDESC_TYPE_FIXED_IO_PORT           0x48
#define ACPI_RDESC_TYPE_SMALL_VENDOR            0x70
#define ACPI_RDESC_TYPE_END_TAG                 0x78

/*
 * Large resource descriptor types
 */

#define ACPI_RDESC_TYPE_MEMORY_24               0x81
#define ACPI_RDESC_TYPE_GENERAL_REGISTER        0x82
#define ACPI_RDESC_TYPE_LARGE_VENDOR            0x84
#define ACPI_RDESC_TYPE_MEMORY_32               0x85
#define ACPI_RDESC_TYPE_FIXED_MEMORY_32         0x86
#define ACPI_RDESC_TYPE_DWORD_ADDRESS_SPACE     0x87
#define ACPI_RDESC_TYPE_WORD_ADDRESS_SPACE      0x88
#define ACPI_RDESC_TYPE_EXTENDED_XRUPT          0x89
#define ACPI_RDESC_TYPE_QWORD_ADDRESS_SPACE     0x8A


/* String version of device HIDs and UIDs */

#define ACPI_DEVICE_ID_LENGTH                   0x09

typedef struct
{
    char            Buffer[ACPI_DEVICE_ID_LENGTH];

} ACPI_DEVICE_ID;


/*****************************************************************************
 *
 * Miscellaneous
 *
 ****************************************************************************/

#define ACPI_ASCII_ZERO                      0x30


/*****************************************************************************
 *
 * Debugger
 *
 ****************************************************************************/

typedef struct dbmethodinfo
{
    ACPI_HANDLE             ThreadGate;
    NATIVE_CHAR             *Name;
    NATIVE_CHAR             **Args;
    UINT32                  Flags;
    UINT32                  NumLoops;
    NATIVE_CHAR             Pathname[128];

} ACPI_DB_METHOD_INFO;


#define ACPI_DB_REDIRECTABLE_OUTPUT  0x01
#define ACPI_DB_CONSOLE_OUTPUT       0x02
#define ACPI_DB_DUPLICATE_OUTPUT     0x03


/*****************************************************************************
 *
 * Debug
 *
 ****************************************************************************/

typedef struct
{
    UINT32                  ComponentId;
    NATIVE_CHAR             *ProcName;
    NATIVE_CHAR             *ModuleName;

} ACPI_DEBUG_PRINT_INFO;


/* Entry for a memory allocation (debug only) */

#define ACPI_MEM_MALLOC                      0
#define ACPI_MEM_CALLOC                      1
#define ACPI_MAX_MODULE_NAME                 16

#define ACPI_COMMON_DEBUG_MEM_HEADER \
    struct AcpiDebugMemBlock    *Previous; \
    struct AcpiDebugMemBlock    *Next; \
    UINT32                      Size; \
    UINT32                      Component; \
    UINT32                      Line; \
    NATIVE_CHAR                 Module[ACPI_MAX_MODULE_NAME]; \
    UINT8                       AllocType;

typedef struct
{
    ACPI_COMMON_DEBUG_MEM_HEADER

} ACPI_DEBUG_MEM_HEADER;

typedef struct AcpiDebugMemBlock
{
    ACPI_COMMON_DEBUG_MEM_HEADER
    UINT64                      UserSpace;

} ACPI_DEBUG_MEM_BLOCK;


#define ACPI_MEM_LIST_GLOBAL            0
#define ACPI_MEM_LIST_NSNODE            1

#define ACPI_MEM_LIST_FIRST_CACHE_LIST  2
#define ACPI_MEM_LIST_STATE             2
#define ACPI_MEM_LIST_PSNODE            3
#define ACPI_MEM_LIST_PSNODE_EXT        4
#define ACPI_MEM_LIST_OPERAND           5
#define ACPI_MEM_LIST_WALK              6
#define ACPI_MEM_LIST_MAX               6
#define ACPI_NUM_MEM_LISTS              7


typedef struct
{
    void                        *ListHead;
    UINT16                      LinkOffset;
    UINT16                      MaxCacheDepth;
    UINT16                      CacheDepth;
    UINT16                      ObjectSize;

#ifdef ACPI_DBG_TRACK_ALLOCATIONS

    /* Statistics for debug memory tracking only */

    UINT32                      TotalAllocated;
    UINT32                      TotalFreed;
    UINT32                      CurrentTotalSize;
    UINT32                      CacheRequests;
    UINT32                      CacheHits;
    char                        *ListName;
#endif

} ACPI_MEMORY_LIST;


#endif /* __ACLOCAL_H__ */
