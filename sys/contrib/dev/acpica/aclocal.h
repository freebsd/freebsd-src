/******************************************************************************
 *
 * Name: aclocal.h - Internal data types used across the ACPI subsystem
 *       $Revision: 89 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, Intel Corp.  All rights
 * reserved.
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


#define WAIT_FOREVER                ((UINT32) -1)

typedef void*                       ACPI_MUTEX;
typedef UINT32                      ACPI_MUTEX_HANDLE;


/* Object descriptor types */

#define ACPI_CACHED_OBJECT          0x11    /* ORed in when object is cached */
#define ACPI_DESC_TYPE_STATE        0x22
#define ACPI_DESC_TYPE_WALK         0x44
#define ACPI_DESC_TYPE_PARSER       0x66
#define ACPI_DESC_TYPE_INTERNAL     0x88
#define ACPI_DESC_TYPE_NAMED        0xAA


/*****************************************************************************
 *
 * Mutex typedefs and structs
 *
 ****************************************************************************/


/*
 * Predefined handles for the mutex objects used within the subsystem
 * All mutex objects are automatically created by AcpiCmMutexInitialize.
 * NOTE: any changes here must be reflected in the AcpiGbl_MutexNames table also!
 */

#define ACPI_MTX_HARDWARE           0
#define ACPI_MTX_MEMORY             1
#define ACPI_MTX_CACHES             2
#define ACPI_MTX_TABLES             3
#define ACPI_MTX_PARSER             4
#define ACPI_MTX_DISPATCHER         5
#define ACPI_MTX_INTERPRETER        6
#define ACPI_MTX_EXECUTE            7
#define ACPI_MTX_NAMESPACE          8
#define ACPI_MTX_EVENTS             9
#define ACPI_MTX_OP_REGIONS         10
#define ACPI_MTX_DEBUG_CMD_READY    11
#define ACPI_MTX_DEBUG_CMD_COMPLETE 12

#define MAX_MTX                     12
#define NUM_MTX                     MAX_MTX+1


#ifdef ACPI_DEBUG
#ifdef DEFINE_ACPI_GLOBALS

/* Names for the mutexes used in the subsystem */

static NATIVE_CHAR          *AcpiGbl_MutexNames[] =
{
    "ACPI_MTX_Hardware",
    "ACPI_MTX_Memory",
    "ACPI_MTX_Caches",
    "ACPI_MTX_Tables",
    "ACPI_MTX_Parser",
    "ACPI_MTX_Dispatcher",
    "ACPI_MTX_Interpreter",
    "ACPI_MTX_Execute",
    "ACPI_MTX_Namespace",
    "ACPI_MTX_Events",
    "ACPI_MTX_OpRegions",
    "ACPI_MTX_DebugCmdReady",
    "ACPI_MTX_DebugCmdComplete"
};

#endif
#endif


/* Table for the global mutexes */

typedef struct AcpiMutexInfo
{
    ACPI_MUTEX                  Mutex;
    UINT32                      UseCount;
    BOOLEAN                     Locked;

} ACPI_MUTEX_INFO;


/* Lock flag parameter for various interfaces */

#define ACPI_MTX_DO_NOT_LOCK        0
#define ACPI_MTX_LOCK               1


typedef UINT16                      ACPI_OWNER_ID;
#define OWNER_TYPE_TABLE            0x0
#define OWNER_TYPE_METHOD           0x1
#define FIRST_METHOD_ID             0x0000
#define FIRST_TABLE_ID              0x8000

/* TBD: [Restructure] get rid of the need for this! */

#define TABLE_ID_DSDT               (ACPI_OWNER_ID) 0x8000

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

#define ANOBJ_AML_ATTACHMENT        0x1
#define ANOBJ_END_OF_PEER_LIST      0x2
#define ANOBJ_DATA_WIDTH_32         0x4     /* Parent table is 64-bits */


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
#define ACPI_MAX_ADDRESS_SPACE      255
#define ACPI_NUM_ADDRESS_SPACES     256


typedef struct
{
    NATIVE_CHAR             *Name;
    ACPI_OBJECT_TYPE        Type;
    NATIVE_CHAR             *Val;

} PREDEFINED_NAMES;


/*****************************************************************************
 *
 * Event typedefs and structs
 *
 ****************************************************************************/


/* Status bits. */

#define ACPI_STATUS_PMTIMER                  0x0001
#define ACPI_STATUS_GLOBAL                   0x0020
#define ACPI_STATUS_POWER_BUTTON             0x0100
#define ACPI_STATUS_SLEEP_BUTTON             0x0200
#define ACPI_STATUS_RTC_ALARM                0x0400

/* Enable bits. */

#define ACPI_ENABLE_PMTIMER                  0x0001
#define ACPI_ENABLE_GLOBAL                   0x0020
#define ACPI_ENABLE_POWER_BUTTON             0x0100
#define ACPI_ENABLE_SLEEP_BUTTON             0x0200
#define ACPI_ENABLE_RTC_ALARM                0x0400


/*
 * Entry in the AddressSpace (AKA Operation Region) table
 */

typedef struct
{
    ADDRESS_SPACE_HANDLER   Handler;
    void                    *Context;

} ACPI_ADDRESS_SPACE_INFO;


/* Values and addresses of the GPE registers (both banks) */

typedef struct
{
    UINT8                   Status;         /* Current value of status reg */
    UINT8                   Enable;         /* Current value of enable reg */
    UINT16                  StatusAddr;     /* Address of status reg */
    UINT16                  EnableAddr;     /* Address of enable reg */
    UINT8                   GpeBase;        /* Base GPE number */

} ACPI_GPE_REGISTERS;


#define ACPI_GPE_LEVEL_TRIGGERED            1
#define ACPI_GPE_EDGE_TRIGGERED             2


/* Information about each particular GPE level */

typedef struct
{
    UINT8                   Type;           /* Level or Edge */

    ACPI_HANDLE             MethodHandle;   /* Method handle for direct (fast) execution */
    GPE_HANDLER             Handler;        /* Address of handler, if any */
    void                    *Context;       /* Context to be passed to handler */

} ACPI_GPE_LEVEL_INFO;


/* Information about each particular fixed event */

typedef struct
{
    FIXED_EVENT_HANDLER     Handler;        /* Address of handler. */
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


#define CONTROL_NORMAL                        0xC0
#define CONTROL_CONDITIONAL_EXECUTING         0xC1
#define CONTROL_PREDICATE_EXECUTING           0xC2
#define CONTROL_PREDICATE_FALSE               0xC3
#define CONTROL_PREDICATE_TRUE                0xC4


/* Forward declaration */
struct acpi_walk_state;
struct acpi_parse_obj ;


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


typedef union acpi_gen_state
{
    ACPI_COMMON_STATE       Common;
    ACPI_CONTROL_STATE      Control;
    ACPI_UPDATE_STATE       Update;
    ACPI_SCOPE_STATE        Scope;
    ACPI_PSCOPE_STATE       ParseScope;

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


#define ACPI_OP_CLASS_MASK          0x1F
#define ACPI_OP_ARGS_MASK           0x20
#define ACPI_OP_TYPE_MASK           0xC0

#define ACPI_OP_TYPE_OPCODE         0x00
#define ACPI_OP_TYPE_ASCII          0x40
#define ACPI_OP_TYPE_PREFIX         0x80
#define ACPI_OP_TYPE_UNKNOWN        0xC0

#define ACPI_GET_OP_CLASS(a)        ((a)->Flags & ACPI_OP_CLASS_MASK)
#define ACPI_GET_OP_ARGS(a)         ((a)->Flags & ACPI_OP_ARGS_MASK)
#define ACPI_GET_OP_TYPE(a)         ((a)->Flags & ACPI_OP_TYPE_MASK)


/*
 * AML opcode, name, and argument layout
 */
typedef struct acpi_opcode_info
{
    UINT8                   Flags;          /* Opcode type, HasArgs flag */
    UINT32                  ParseArgs;      /* Grammar/Parse time arguments */
    UINT32                  RuntimeArgs;    /* Interpret time arguments */

    DEBUG_ONLY_MEMBERS (NATIVE_CHAR *Name)  /* op name (debug only) */

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
 * Tree walking typedefs and structs
 *
 ****************************************************************************/


/*
 * Walk state - current state of a parse tree walk.  Used for both a leisurely stroll through
 * the tree (for whatever reason), and for control method execution.
 */

#define NEXT_OP_DOWNWARD    1
#define NEXT_OP_UPWARD      2

#define WALK_NON_METHOD     0
#define WALK_METHOD         1
#define WALK_METHOD_RESTART 2

typedef struct acpi_walk_state
{
    UINT8                   DataType;                           /* To differentiate various internal objs */\
    ACPI_OWNER_ID           OwnerId;                            /* Owner of objects created during the walk */
    BOOLEAN                 LastPredicate;                      /* Result of last predicate */
    UINT8                   NextOpInfo;                         /* Info about NextOp */
    UINT8                   NumOperands;                        /* Stack pointer for Operands[] array */
    UINT8                   NumResults;                         /* Stack pointer for Results[] array */
    UINT8                   CurrentResult;                      /* */

    struct acpi_walk_state  *Next;                              /* Next WalkState in list */
    ACPI_PARSE_OBJECT       *Origin;                            /* Start of walk [Obsolete] */

/* TBD: Obsolete with removal of WALK procedure ? */
    ACPI_PARSE_OBJECT       *PrevOp;                            /* Last op that was processed */
    ACPI_PARSE_OBJECT       *NextOp;                            /* next op to be processed */


    ACPI_GENERIC_STATE      *ControlState;                      /* List of control states (nested IFs) */
    ACPI_GENERIC_STATE      *ScopeInfo;                         /* Stack of nested scopes */
    ACPI_PARSE_STATE        *ParserState;                       /* Current state of parser */
    UINT8                   *AmlLastWhile;
    ACPI_PARSE_DOWNWARDS    DescendingCallback;
    ACPI_PARSE_UPWARDS      AscendingCallback;

    union acpi_operand_obj  *ReturnDesc;                        /* Return object, if any */
    union acpi_operand_obj  *MethodDesc;                        /* Method descriptor if running a method */
    struct acpi_node        *MethodNode;                        /* Method Node if running a method */
    ACPI_PARSE_OBJECT       *MethodCallOp;                      /* MethodCall Op if running a method */
    struct acpi_node        *MethodCallNode;                    /* Called method Node*/
    union acpi_operand_obj  *Operands[OBJ_NUM_OPERANDS];        /* Operands passed to the interpreter */
    union acpi_operand_obj  *Results[OBJ_NUM_OPERANDS];         /* Accumulated results */
    struct acpi_node        Arguments[MTH_NUM_ARGS];            /* Control method arguments */
    struct acpi_node        LocalVariables[MTH_NUM_LOCALS];     /* Control method locals */
    UINT32                  ParseFlags;
    UINT8                   WalkType;
    UINT8                   ReturnUsed;
    UINT32                  PrevArgTypes;

    /* Debug support */

    UINT32                  MethodBreakpoint;


} ACPI_WALK_STATE;


/*
 * Walk list - head of a tree of walk states.  Multiple walk states are created when there
 * are nested control methods executing.
 */
typedef struct acpi_walk_list
{

    ACPI_WALK_STATE         *WalkState;

} ACPI_WALK_LIST;


/* Info used by AcpiPsInitObjects */

typedef struct acpi_init_walk_info
{
    UINT16                  MethodCount;
    UINT16                  OpRegionCount;
    UINT16                  FieldCount;
    UINT16                  OpRegionInit;
    UINT16                  FieldInit;
    UINT16                  ObjectCount;
    ACPI_TABLE_DESC         *TableDesc;

} ACPI_INIT_WALK_INFO;


/* Info used by TBD */

typedef struct acpi_device_walk_info
{
    UINT32                  Flags;
    UINT16                  DeviceCount;
    UINT16                  Num_STA;
    UINT16                  Num_INI;
    UINT16                  Num_HID;
    UINT16                  Num_PCI;
    ACPI_TABLE_DESC         *TableDesc;

} ACPI_DEVICE_WALK_INFO;


/* TBD: [Restructure] Merge with struct above */

typedef struct acpi_walk_info
{
    UINT32                  DebugLevel;
    UINT32                  OwnerId;

} ACPI_WALK_INFO;

typedef struct acpi_get_devices_info
{
    WALK_CALLBACK           UserFunction;
    void                    *Context;
    NATIVE_CHAR             *Hid;

} ACPI_GET_DEVICES_INFO;


/*****************************************************************************
 *
 * Hardware and PNP
 *
 ****************************************************************************/


/* PCI */

#define PCI_ROOT_HID_STRING         "PNP0A03"
#define PCI_ROOT_HID_VALUE          0x030AD041       /* EISAID("PNP0A03") */


/* Sleep states */

#define SLWA_DEBUG_LEVEL            4
#define GTS_CALL                    0
#define GTS_WAKE                    1

/* Cx States */

#define MAX_CX_STATE_LATENCY        0xFFFFFFFF
#define MAX_CX_STATES               4


/*
 * The #define's and enum below establish an abstract way of identifying what
 * register block and register is to be accessed.  Do not change any of the
 * values as they are used in switch statements and offset calculations.
 */

#define REGISTER_BLOCK_MASK         0xFF00  /* Register Block Id    */
#define BIT_IN_REGISTER_MASK        0x00FF  /* Bit Id in the Register Block Id    */
#define BYTE_IN_REGISTER_MASK       0x00FF  /* Register Offset in the Register Block    */

#define REGISTER_BLOCK_ID(RegId)    (RegId & REGISTER_BLOCK_MASK)
#define REGISTER_BIT_ID(RegId)      (RegId & BIT_IN_REGISTER_MASK)
#define REGISTER_OFFSET(RegId)      (RegId & BYTE_IN_REGISTER_MASK)

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
#define PM1_STS                     0x0100
#define PM1_EN                      0x0200
#define PM1_CONTROL                 0x0300
#define PM2_CONTROL                 0x0400
#define PM_TIMER                    0x0500
#define PROCESSOR_BLOCK             0x0600
#define GPE0_STS_BLOCK              0x0700
#define GPE0_EN_BLOCK               0x0800
#define GPE1_STS_BLOCK              0x0900
#define GPE1_EN_BLOCK               0x0A00
#define SMI_CMD_BLOCK               0x0B00

/*
 * Address space bitmasks for mmio or io spaces
 */

#define SMI_CMD_ADDRESS_SPACE       0x01
#define PM1_BLK_ADDRESS_SPACE       0x02
#define PM2_CNT_BLK_ADDRESS_SPACE   0x04
#define PM_TMR_BLK_ADDRESS_SPACE    0x08
#define GPE0_BLK_ADDRESS_SPACE      0x10
#define GPE1_BLK_ADDRESS_SPACE      0x20

/*
 * Control bit definitions
 */
#define TMR_STS     (PM1_STS | 0x01)
#define BM_STS      (PM1_STS | 0x02)
#define GBL_STS     (PM1_STS | 0x03)
#define PWRBTN_STS  (PM1_STS | 0x04)
#define SLPBTN_STS  (PM1_STS | 0x05)
#define RTC_STS     (PM1_STS | 0x06)
#define WAK_STS     (PM1_STS | 0x07)

#define TMR_EN      (PM1_EN | 0x01)
                     /* no BM_EN */
#define GBL_EN      (PM1_EN | 0x03)
#define PWRBTN_EN   (PM1_EN | 0x04)
#define SLPBTN_EN   (PM1_EN | 0x05)
#define RTC_EN      (PM1_EN | 0x06)
#define WAK_EN      (PM1_EN | 0x07)

#define SCI_EN      (PM1_CONTROL | 0x01)
#define BM_RLD      (PM1_CONTROL | 0x02)
#define GBL_RLS     (PM1_CONTROL | 0x03)
#define SLP_TYPE_A  (PM1_CONTROL | 0x04)
#define SLP_TYPE_B  (PM1_CONTROL | 0x05)
#define SLP_EN      (PM1_CONTROL | 0x06)

#define ARB_DIS     (PM2_CONTROL | 0x01)

#define TMR_VAL     (PM_TIMER | 0x01)

#define GPE0_STS    (GPE0_STS_BLOCK | 0x01)
#define GPE0_EN     (GPE0_EN_BLOCK  | 0x01)

#define GPE1_STS    (GPE1_STS_BLOCK | 0x01)
#define GPE1_EN     (GPE1_EN_BLOCK  | 0x01)


#define TMR_STS_MASK        0x0001
#define BM_STS_MASK         0x0010
#define GBL_STS_MASK        0x0020
#define PWRBTN_STS_MASK     0x0100
#define SLPBTN_STS_MASK     0x0200
#define RTC_STS_MASK        0x0400
#define WAK_STS_MASK        0x8000

#define ALL_FIXED_STS_BITS  (TMR_STS_MASK   | BM_STS_MASK  | GBL_STS_MASK \
                             | PWRBTN_STS_MASK | SLPBTN_STS_MASK \
                             | RTC_STS_MASK | WAK_STS_MASK)

#define TMR_EN_MASK         0x0001
#define GBL_EN_MASK         0x0020
#define PWRBTN_EN_MASK      0x0100
#define SLPBTN_EN_MASK      0x0200
#define RTC_EN_MASK         0x0400

#define SCI_EN_MASK         0x0001
#define BM_RLD_MASK         0x0002
#define GBL_RLS_MASK        0x0004
#define SLP_TYPE_X_MASK     0x1C00
#define SLP_EN_MASK         0x2000

#define ARB_DIS_MASK        0x0001
#define TMR_VAL_MASK        0xFFFFFFFF

#define GPE0_STS_MASK
#define GPE0_EN_MASK

#define GPE1_STS_MASK
#define GPE1_EN_MASK


#define ACPI_READ           1
#define ACPI_WRITE          2


/* Plug and play */

/* Pnp and ACPI data */

#define VERSION_NO                      0x01
#define LOGICAL_DEVICE_ID               0x02
#define COMPATIBLE_DEVICE_ID            0x03
#define IRQ_FORMAT                      0x04
#define DMA_FORMAT                      0x05
#define START_DEPENDENT_TAG             0x06
#define END_DEPENDENT_TAG               0x07
#define IO_PORT_DESCRIPTOR              0x08
#define FIXED_LOCATION_IO_DESCRIPTOR    0x09
#define RESERVED_TYPE0                  0x0A
#define RESERVED_TYPE1                  0x0B
#define RESERVED_TYPE2                  0x0C
#define RESERVED_TYPE3                  0x0D
#define SMALL_VENDOR_DEFINED            0x0E
#define END_TAG                         0x0F

/* Pnp and ACPI data */

#define MEMORY_RANGE_24                 0x81
#define ISA_MEMORY_RANGE                0x81
#define LARGE_VENDOR_DEFINED            0x84
#define EISA_MEMORY_RANGE               0x85
#define MEMORY_RANGE_32                 0x85
#define FIXED_EISA_MEMORY_RANGE         0x86
#define FIXED_MEMORY_RANGE_32           0x86

/* ACPI only data */

#define DWORD_ADDRESS_SPACE             0x87
#define WORD_ADDRESS_SPACE              0x88
#define EXTENDED_IRQ                    0x89

/* MUST HAVES */

#define DEVICE_ID_LENGTH                0x09

typedef struct
{
        NATIVE_CHAR         Buffer[DEVICE_ID_LENGTH];

} DEVICE_ID;


/*****************************************************************************
 *
 * Debug
 *
 ****************************************************************************/


/* Entry for a memory allocation (debug only) */

#ifdef ACPI_DEBUG

#define MEM_MALLOC          0
#define MEM_CALLOC          1
#define MAX_MODULE_NAME     16

typedef struct AllocationInfo
{
    struct AllocationInfo   *Previous;
    struct AllocationInfo   *Next;
    void                    *Address;
    UINT32                  Size;
    UINT32                  Component;
    UINT32                  Line;
    NATIVE_CHAR             Module[MAX_MODULE_NAME];
    UINT8                   AllocType;

} ALLOCATION_INFO;

#endif

#endif /* __ACLOCAL_H__ */
