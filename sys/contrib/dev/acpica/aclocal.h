/******************************************************************************
 *
 * Name: aclocal.h - Internal data types used across the ACPI subsystem
 *       $Revision: 123 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
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



#define ACPI_MEMORY_MODE                0x01
#define ACPI_LOGICAL_ADDRESSING         0x00
#define ACPI_PHYSICAL_ADDRESSING        0x01

/* Object descriptor types */

#define ACPI_CACHED_OBJECT              0x11    /* ORed in when object is cached */
#define ACPI_DESC_TYPE_STATE            0x22
#define ACPI_DESC_TYPE_WALK             0x44
#define ACPI_DESC_TYPE_PARSER           0x66
#define ACPI_DESC_TYPE_INTERNAL         0x88
#define ACPI_DESC_TYPE_NAMED            0xAA


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
#define OWNER_TYPE_TABLE                0x0
#define OWNER_TYPE_METHOD               0x1
#define FIRST_METHOD_ID                 0x0000
#define FIRST_TABLE_ID                  0x8000

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
    IMODE_LOAD_PASS1                = 0x01,
    IMODE_LOAD_PASS2                = 0x02,
    IMODE_EXECUTE                   = 0x0E

} OPERATING_MODE;


/*
 * The Node describes a named object that appears in the AML
 * An AcpiNode is used to store Nodes.
 *
 * DataType is used to differentiate between internal descriptors, and MUST
 * be the first byte in this structure.
 */

typedef struct acpi_node
{
    UINT8                   DataType;
    UINT8                   Type;           /* Type associated with this name */
    UINT16                  OwnerId;
    UINT32                  Name;           /* ACPI Name, always 4 chars per ACPI spec */


    void                    *Object;        /* Pointer to attached ACPI object (optional) */
    struct acpi_node        *Child;         /* first child */
    struct acpi_node        *Peer;          /* Next peer*/
    UINT16                  ReferenceCount; /* Current count of references and children */
    UINT8                   Flags;

} ACPI_NAMESPACE_NODE;


#define ENTRY_NOT_FOUND             NULL


/* Node flags */

#define ANOBJ_AML_ATTACHMENT            0x01
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
    UINT8                   *AmlPointer;
    UINT64                  PhysicalAddress;
    UINT32                  AmlLength;
    UINT32                  Length;
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

} FIND_CONTEXT;


typedef struct
{
    ACPI_NAMESPACE_NODE     *Node;
} NS_SEARCH_DATA;


/*
 * Predefined Namespace items
 */
typedef struct
{
    NATIVE_CHAR             *Name;
    ACPI_OBJECT_TYPE8       Type;
    NATIVE_CHAR             *Val;

} PREDEFINED_NAMES;


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


/*****************************************************************************
 *
 * Event typedefs and structs
 *
 ****************************************************************************/


/* Status bits. */

#define ACPI_STATUS_PMTIMER             0x0001
#define ACPI_STATUS_GLOBAL              0x0020
#define ACPI_STATUS_POWER_BUTTON        0x0100
#define ACPI_STATUS_SLEEP_BUTTON        0x0200
#define ACPI_STATUS_RTC_ALARM           0x0400

/* Enable bits. */

#define ACPI_ENABLE_PMTIMER             0x0001
#define ACPI_ENABLE_GLOBAL              0x0020
#define ACPI_ENABLE_POWER_BUTTON        0x0100
#define ACPI_ENABLE_SLEEP_BUTTON        0x0200
#define ACPI_ENABLE_RTC_ALARM           0x0400


/*
 * Entry in the AddressSpace (AKA Operation Region) table
 */

typedef struct
{
    ACPI_ADR_SPACE_HANDLER  Handler;
    void                    *Context;

} ACPI_ADR_SPACE_INFO;


/* Values and addresses of the GPE registers (both banks) */

typedef struct
{
    UINT8                   Status;         /* Current value of status reg */
    UINT8                   Enable;         /* Current value of enable reg */
    UINT16                  StatusAddr;     /* Address of status reg */
    UINT16                  EnableAddr;     /* Address of enable reg */
    UINT8                   GpeBase;        /* Base GPE number */

} ACPI_GPE_REGISTERS;


#define ACPI_GPE_LEVEL_TRIGGERED        1
#define ACPI_GPE_EDGE_TRIGGERED         2


/* Information about each particular GPE level */

typedef struct
{
    UINT8                   Type;           /* Level or Edge */

    ACPI_HANDLE             MethodHandle;   /* Method handle for direct (fast) execution */
    ACPI_GPE_HANDLER        Handler;        /* Address of handler, if any */
    void                    *Context;       /* Context to be passed to handler */

} ACPI_GPE_LEVEL_INFO;


/* Information about each particular fixed event */

typedef struct
{
    ACPI_EVENT_HANDLER      Handler;        /* Address of handler. */
    void                    *Context;       /* Context to be passed to handler */

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


#define CONTROL_NORMAL                  0xC0
#define CONTROL_CONDITIONAL_EXECUTING   0xC1
#define CONTROL_PREDICATE_EXECUTING     0xC2
#define CONTROL_PREDICATE_FALSE         0xC3
#define CONTROL_PREDICATE_TRUE          0xC4


/* Forward declarations */
struct acpi_walk_state;
struct acpi_walk_list;
struct acpi_parse_obj;
struct acpi_obj_mutex;


#define ACPI_STATE_COMMON                  /* Two 32-bit fields and a pointer */\
    UINT8                   DataType;           /* To differentiate various internal objs */\
    UINT8                   Flags; \
    UINT16                  Value; \
    UINT16                  State; \
    UINT16                  AcpiEval;  \
    void                    *Next; \

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
    struct acpi_parse_obj   *PredicateOp;
    UINT8                   *AmlPredicateStart;   /* Start of if/while predicate */

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
    struct acpi_parse_obj   *Op;            /* current op being parsed */
    UINT8                   *ArgEnd;        /* current argument end */
    UINT8                   *PkgEnd;        /* current package end */
    UINT32                  ArgList;        /* next argument to parse */
    UINT32                  ArgCount;       /* Number of fixed arguments */

} ACPI_PSCOPE_STATE;


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
    ACPI_RESULT_VALUES      Results;
    ACPI_NOTIFY_INFO        Notify;

} ACPI_GENERIC_STATE;


typedef
ACPI_STATUS (*ACPI_PARSE_DOWNWARDS) (
    UINT16                  Opcode,
    struct acpi_parse_obj   *Op,
    struct acpi_walk_state  *WalkState,
    struct acpi_parse_obj   **OutOp);

typedef
ACPI_STATUS (*ACPI_PARSE_UPWARDS) (
    struct acpi_walk_state  *WalkState,
    struct acpi_parse_obj   *Op);


/*****************************************************************************
 *
 * Parser typedefs and structs
 *
 ****************************************************************************/

#define ACPI_OP_CLASS_MASK              0x1F
#define ACPI_OP_ARGS_MASK               0x20
#define ACPI_OP_TYPE_MASK               0xC0

#define ACPI_OP_TYPE_OPCODE             0x00
#define ACPI_OP_TYPE_ASCII              0x40
#define ACPI_OP_TYPE_PREFIX             0x80
#define ACPI_OP_TYPE_UNKNOWN            0xC0

#define ACPI_GET_OP_CLASS(a)            ((a)->Flags & ACPI_OP_CLASS_MASK)
#define ACPI_GET_OP_ARGS(a)             ((a)->Flags & ACPI_OP_ARGS_MASK)
#define ACPI_GET_OP_TYPE(a)             ((a)->Flags & ACPI_OP_TYPE_MASK)


/*
 * AML opcode, name, and argument layout
 */
typedef struct acpi_opcode_info
{
    UINT8                   Flags;          /* Opcode type, HasArgs flag */
    UINT32                  ParseArgs;      /* Grammar/Parse time arguments */
    UINT32                  RuntimeArgs;    /* Interpret time arguments */

#ifdef _OPCODE_NAMES
    NATIVE_CHAR             *Name;          /* op name (debug only) */
#endif

} ACPI_OPCODE_INFO;


typedef union acpi_parse_val
{
    UINT32                  Integer;        /* integer constant */
    UINT32                  Size;           /* bytelist or field size */
    NATIVE_CHAR             *String;        /* NULL terminated string */
    UINT8                   *Buffer;        /* buffer or string */
    NATIVE_CHAR             *Name;          /* NULL terminated string */
    struct acpi_parse_obj   *Arg;           /* arguments and contained ops */

} ACPI_PARSE_VALUE;


#define ACPI_PARSE_COMMON \
    UINT8                   DataType;       /* To differentiate various internal objs */\
    UINT8                   Flags;          /* Type of Op */\
    UINT16                  Opcode;         /* AML opcode */\
    UINT32                  AmlOffset;      /* offset of declaration in AML */\
    struct acpi_parse_obj   *Parent;        /* parent op */\
    struct acpi_parse_obj   *Next;          /* next op */\
    DEBUG_ONLY_MEMBERS (\
    NATIVE_CHAR             OpName[16])     /* op name (debug only) */\
                                            /* NON-DEBUG members below: */\
    ACPI_NAMESPACE_NODE     *Node;          /* for use by interpreter */\
    ACPI_PARSE_VALUE        Value;          /* Value or args associated with the opcode */\


/*
 * generic operation (eg. If, While, Store)
 */
typedef struct acpi_parse_obj
{
    ACPI_PARSE_COMMON
} ACPI_PARSE_OBJECT;


/*
 * Extended Op for named ops (Scope, Method, etc.), deferred ops (Methods and OpRegions),
 * and bytelists.
 */
typedef struct acpi_parse2_obj
{
    ACPI_PARSE_COMMON
    UINT8                   *Data;          /* AML body or bytelist data */
    UINT32                  Length;         /* AML length */
    UINT32                  Name;           /* 4-byte name or zero if no name */

} ACPI_PARSE2_OBJECT;


/*
 * Parse state - one state per parser invocation and each control
 * method.
 */

typedef struct acpi_parse_state
{
    UINT8                   *AmlStart;      /* first AML byte */
    UINT8                   *Aml;           /* next AML byte */
    UINT8                   *AmlEnd;        /* (last + 1) AML byte */
    UINT8                   *PkgStart;      /* current package begin */
    UINT8                   *PkgEnd;        /* current package end */
    ACPI_PARSE_OBJECT       *StartOp;       /* root of parse tree */
    struct acpi_node        *StartNode;
    ACPI_GENERIC_STATE      *Scope;         /* current scope */
    struct acpi_parse_state *Next;

} ACPI_PARSE_STATE;


/*****************************************************************************
 *
 * Hardware and PNP
 *
 ****************************************************************************/


/* PCI */

#define PCI_ROOT_HID_STRING             "PNP0A03"
#define PCI_ROOT_HID_VALUE              0x030AD041       /* EISAID("PNP0A03") */


/* Sleep states */

#define SLWA_DEBUG_LEVEL                4
#define GTS_CALL                        0
#define GTS_WAKE                        1

/* Cx States */

#define MAX_CX_STATE_LATENCY            0xFFFFFFFF
#define MAX_CX_STATES                   4


/*
 * The #define's and enum below establish an abstract way of identifying what
 * register block and register is to be accessed.  Do not change any of the
 * values as they are used in switch statements and offset calculations.
 */

#define REGISTER_BLOCK_MASK             0xFF00  /* Register Block Id    */
#define BIT_IN_REGISTER_MASK            0x00FF  /* Bit Id in the Register Block Id    */
#define BYTE_IN_REGISTER_MASK           0x00FF  /* Register Offset in the Register Block    */

#define REGISTER_BLOCK_ID(RegId)        (RegId & REGISTER_BLOCK_MASK)
#define REGISTER_BIT_ID(RegId)          (RegId & BIT_IN_REGISTER_MASK)
#define REGISTER_OFFSET(RegId)          (RegId & BYTE_IN_REGISTER_MASK)

/*
 * Access Rule
 *  To access a Register Bit:
 *  -> Use Bit Name (= Register Block Id | Bit Id) defined in the enum.
 *
 *  To access a Register:
 *  -> Use Register Id (= Register Block Id | Register Offset)
 */


/*
 * Register Block Id
 */
#define PM1_STS                         0x0100
#define PM1_EN                          0x0200
#define PM1_CONTROL                     0x0300
#define PM1A_CONTROL                    0x0400
#define PM1B_CONTROL                    0x0500
#define PM2_CONTROL                     0x0600
#define PM_TIMER                        0x0700
#define PROCESSOR_BLOCK                 0x0800
#define GPE0_STS_BLOCK                  0x0900
#define GPE0_EN_BLOCK                   0x0A00
#define GPE1_STS_BLOCK                  0x0B00
#define GPE1_EN_BLOCK                   0x0C00
#define SMI_CMD_BLOCK                   0x0D00

/*
 * Address space bitmasks for mmio or io spaces
 */

#define SMI_CMD_ADDRESS_SPACE           0x01
#define PM1_BLK_ADDRESS_SPACE           0x02
#define PM2_CNT_BLK_ADDRESS_SPACE       0x04
#define PM_TMR_BLK_ADDRESS_SPACE        0x08
#define GPE0_BLK_ADDRESS_SPACE          0x10
#define GPE1_BLK_ADDRESS_SPACE          0x20

/*
 * Control bit definitions
 */
#define TMR_STS                         (PM1_STS | 0x01)
#define BM_STS                          (PM1_STS | 0x02)
#define GBL_STS                         (PM1_STS | 0x03)
#define PWRBTN_STS                      (PM1_STS | 0x04)
#define SLPBTN_STS                      (PM1_STS | 0x05)
#define RTC_STS                         (PM1_STS | 0x06)
#define WAK_STS                         (PM1_STS | 0x07)

#define TMR_EN                          (PM1_EN | 0x01)
                                        /* no BM_EN */
#define GBL_EN                          (PM1_EN | 0x03)
#define PWRBTN_EN                       (PM1_EN | 0x04)
#define SLPBTN_EN                       (PM1_EN | 0x05)
#define RTC_EN                          (PM1_EN | 0x06)
#define WAK_EN                          (PM1_EN | 0x07)

#define SCI_EN                          (PM1_CONTROL | 0x01)
#define BM_RLD                          (PM1_CONTROL | 0x02)
#define GBL_RLS                         (PM1_CONTROL | 0x03)
#define SLP_TYPE_A                      (PM1_CONTROL | 0x04)
#define SLP_TYPE_B                      (PM1_CONTROL | 0x05)
#define SLP_EN                          (PM1_CONTROL | 0x06)

#define ARB_DIS                         (PM2_CONTROL | 0x01)

#define TMR_VAL                         (PM_TIMER | 0x01)

#define GPE0_STS                        (GPE0_STS_BLOCK | 0x01)
#define GPE0_EN                         (GPE0_EN_BLOCK  | 0x01)

#define GPE1_STS                        (GPE1_STS_BLOCK | 0x01)
#define GPE1_EN                         (GPE1_EN_BLOCK  | 0x01)


#define TMR_STS_MASK                    0x0001
#define BM_STS_MASK                     0x0010
#define GBL_STS_MASK                    0x0020
#define PWRBTN_STS_MASK                 0x0100
#define SLPBTN_STS_MASK                 0x0200
#define RTC_STS_MASK                    0x0400
#define WAK_STS_MASK                    0x8000

#define ALL_FIXED_STS_BITS              (TMR_STS_MASK   | BM_STS_MASK  | GBL_STS_MASK \
                                        | PWRBTN_STS_MASK | SLPBTN_STS_MASK \
                                        | RTC_STS_MASK | WAK_STS_MASK)

#define TMR_EN_MASK                     0x0001
#define GBL_EN_MASK                     0x0020
#define PWRBTN_EN_MASK                  0x0100
#define SLPBTN_EN_MASK                  0x0200
#define RTC_EN_MASK                     0x0400

#define SCI_EN_MASK                     0x0001
#define BM_RLD_MASK                     0x0002
#define GBL_RLS_MASK                    0x0004
#define SLP_TYPE_X_MASK                 0x1C00
#define SLP_EN_MASK                     0x2000

#define ARB_DIS_MASK                    0x0001
#define TMR_VAL_MASK                    0xFFFFFFFF

#define GPE0_STS_MASK
#define GPE0_EN_MASK

#define GPE1_STS_MASK
#define GPE1_EN_MASK


#define ACPI_READ                       1
#define ACPI_WRITE                      2


/*****************************************************************************
 *
 * Resource descriptors
 *
 ****************************************************************************/


/* ResourceType values */

#define RESOURCE_TYPE_MEMORY_RANGE              0
#define RESOURCE_TYPE_IO_RANGE                  1
#define RESOURCE_TYPE_BUS_NUMBER_RANGE          2

/* Resource descriptor types and masks */

#define RESOURCE_DESC_TYPE_LARGE                0x80
#define RESOURCE_DESC_TYPE_SMALL                0x00

#define RESOURCE_DESC_TYPE_MASK                 0x80
#define RESOURCE_DESC_SMALL_MASK                0x78        /* Only bits 6:3 contain the type */


/*
 * Small resource descriptor types
 * Note: The 3 length bits (2:0) must be zero
 */
#define RESOURCE_DESC_IRQ_FORMAT                0x20
#define RESOURCE_DESC_DMA_FORMAT                0x28
#define RESOURCE_DESC_START_DEPENDENT           0x30
#define RESOURCE_DESC_END_DEPENDENT             0x38
#define RESOURCE_DESC_IO_PORT                   0x40
#define RESOURCE_DESC_FIXED_IO_PORT             0x48
#define RESOURCE_DESC_SMALL_VENDOR              0x70
#define RESOURCE_DESC_END_TAG                   0x78

/*
 * Large resource descriptor types
 */

#define RESOURCE_DESC_MEMORY_24                 0x81
#define RESOURCE_DESC_GENERAL_REGISTER          0x82
#define RESOURCE_DESC_LARGE_VENDOR              0x84
#define RESOURCE_DESC_MEMORY_32                 0x85
#define RESOURCE_DESC_FIXED_MEMORY_32           0x86
#define RESOURCE_DESC_DWORD_ADDRESS_SPACE       0x87
#define RESOURCE_DESC_WORD_ADDRESS_SPACE        0x88
#define RESOURCE_DESC_EXTENDED_XRUPT            0x89
#define RESOURCE_DESC_QWORD_ADDRESS_SPACE       0x8A


/* String version of device HIDs and UIDs */

#define ACPI_DEVICE_ID_LENGTH                   0x09

typedef struct
{
    NATIVE_CHAR             Buffer[ACPI_DEVICE_ID_LENGTH];

} ACPI_DEVICE_ID;



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

} DB_METHOD_INFO;



/*****************************************************************************
 *
 * Debug
 *
 ****************************************************************************/


/* Entry for a memory allocation (debug only) */

#ifdef ACPI_DEBUG

#define MEM_MALLOC                      0
#define MEM_CALLOC                      1
#define MAX_MODULE_NAME                 16

typedef struct AcpiAllocationInfo
{
    struct AcpiAllocationInfo   *Previous;
    struct AcpiAllocationInfo   *Next;
    void                        *Address;
    UINT32                      Size;
    UINT32                      Component;
    UINT32                      Line;
    NATIVE_CHAR                 Module[MAX_MODULE_NAME];
    UINT8                       AllocType;

} ACPI_ALLOCATION_INFO;

#endif

#endif /* __ACLOCAL_H__ */
