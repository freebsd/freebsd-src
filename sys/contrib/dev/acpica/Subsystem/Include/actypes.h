/******************************************************************************
 *
 * Name: actypes.h - Common data types for the entire ACPI subsystem
 *       $Revision: 143 $
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

#ifndef __ACTYPES_H__
#define __ACTYPES_H__

/*! [Begin] no source code translation (keep the typedefs) */

/*
 * Data types - Fixed across all compilation models
 *
 * BOOLEAN      Logical Boolean.
 *              1 byte value containing a 0 for FALSE or a 1 for TRUE.
 *              Other values are undefined.
 *
 * INT8         8-bit  (1 byte) signed value
 * UINT8        8-bit  (1 byte) unsigned value
 * INT16        16-bit (2 byte) signed value
 * UINT16       16-bit (2 byte) unsigned value
 * INT32        32-bit (4 byte) signed value
 * UINT32       32-bit (4 byte) unsigned value
 * INT64        64-bit (8 byte) signed value
 * UINT64       64-bit (8 byte) unsigned value
 * NATIVE_INT   32-bit on IA-32, 64-bit on IA-64 signed value
 * NATIVE_UINT  32-bit on IA-32, 64-bit on IA-64 unsigned value
 * UCHAR        Character. 1 byte unsigned value.
 */


#ifdef _IA64
/*
 * 64-bit type definitions
 */
typedef unsigned char                   UINT8;
typedef unsigned char                   BOOLEAN;
typedef unsigned char                   UCHAR;
typedef unsigned short                  UINT16;
typedef int                             INT32;
typedef unsigned int                    UINT32;
typedef long                            INT64;
typedef unsigned long                   UINT64;

typedef UINT64                          NATIVE_UINT;
typedef INT64                           NATIVE_INT;

typedef NATIVE_UINT                     ACPI_TBLPTR;
typedef UINT64                          ACPI_IO_ADDRESS;
typedef UINT64                          ACPI_MEM_ADDRESS;

#define ALIGNED_ADDRESS_BOUNDARY        0x00000008

/* (No hardware alignment support in IA64) */


#elif _IA16
/*
 * 16-bit type definitions
 */
typedef unsigned char                   UINT8;
typedef unsigned char                   BOOLEAN;
typedef unsigned char                   UCHAR;
typedef unsigned int                    UINT16;
typedef long                            INT32;
typedef int                             INT16;
typedef unsigned long                   UINT32;

typedef UINT16                          NATIVE_UINT;
typedef INT16                           NATIVE_INT;

typedef UINT32                          ACPI_TBLPTR;
typedef UINT32                          ACPI_IO_ADDRESS;
typedef UINT32                          ACPI_MEM_ADDRESS;

#define ALIGNED_ADDRESS_BOUNDARY        0x00000002
#define _HW_ALIGNMENT_SUPPORT

/* (16-bit only) Force internal integers to be 32, not 64 bits */

#define ACPI_VERSION_1


#else
/*
 * 32-bit type definitions (default)
 */
typedef unsigned char                   UINT8;
typedef unsigned char                   BOOLEAN;
typedef unsigned char                   UCHAR;
typedef unsigned short                  UINT16;
typedef int                             INT32;
typedef unsigned int                    UINT32;
typedef COMPILER_DEPENDENT_UINT64       UINT64;

typedef UINT32                          NATIVE_UINT;
typedef INT32                           NATIVE_INT;

typedef NATIVE_UINT                     ACPI_TBLPTR;
typedef UINT32                          ACPI_IO_ADDRESS;
typedef UINT32                          ACPI_MEM_ADDRESS;

#define ALIGNED_ADDRESS_BOUNDARY        0x00000004
#define _HW_ALIGNMENT_SUPPORT

#endif



/*
 * Miscellaneous common types
 */

typedef UINT32                          UINT32_BIT;
typedef NATIVE_UINT                     ACPI_PTRDIFF;
typedef char                            NATIVE_CHAR;


/*
 * Data type ranges
 */

#define ACPI_UINT8_MAX                  (UINT8)  0xFF
#define ACPI_UINT16_MAX                 (UINT16) 0xFFFF
#define ACPI_UINT32_MAX                 (UINT32) 0xFFFFFFFF
#define ACPI_UINT64_MAX                 (UINT64) 0xFFFFFFFFFFFFFFFF


#ifdef DEFINE_ALTERNATE_TYPES
/*
 * Types used only in translated source
 */
typedef INT32                           s32;
typedef UINT8                           u8;
typedef UINT16                          u16;
typedef UINT32                          u32;
typedef UINT64                          u64;
#endif
/*! [End] no source code translation !*/


/*
 * Useful defines
 */

#ifdef FALSE
#undef FALSE
#endif
#define FALSE                           (1 == 0)

#ifdef TRUE
#undef TRUE
#endif
#define TRUE                            (1 == 1)

#ifndef NULL
#define NULL                            (void *) 0
#endif


/*
 * Local datatypes
 */

typedef UINT32                          ACPI_STATUS;    /* All ACPI Exceptions */
typedef UINT32                          ACPI_NAME;      /* 4-INT8 ACPI name */
typedef char*                           ACPI_STRING;    /* Null terminated ASCII string */
typedef void*                           ACPI_HANDLE;    /* Actually a ptr to an Node */


/* TBD: TEMP ONLY! */

#define ACPI_VERSION_1

/*
 * Acpi integer width. In ACPI version 1, integers are
 * 32 bits.  In ACPI version 2, integers are 64 bits.  Period.
 * Note that this pertains to the ACPI integer type only, not
 * other integers used in the implementation of the ACPI CA
 * subsystem.
 */

#ifdef ACPI_VERSION_1

/* 32-bit Integers */

typedef UINT32                          ACPI_INTEGER;
#define ACPI_INTEGER_MAX                ACPI_UINT32_MAX;
#define ACPI_INTEGER_BIT_SIZE           32

#else

/* 64-bit Integers */

typedef UINT64                          ACPI_INTEGER;
#define ACPI_INTEGER_MAX                ACPI_UINT64_MAX;
#define ACPI_INTEGER_BIT_SIZE           64
#endif


/*
 * Constants with special meanings
 */

#define ACPI_ROOT_OBJECT                (ACPI_HANDLE)(-1)

#define ACPI_FULL_INITIALIZATION        0x00
#define ACPI_NO_ADDRESS_SPACE_INIT      0x01
#define ACPI_NO_HARDWARE_INIT           0x02
#define ACPI_NO_EVENT_INIT              0x04
#define ACPI_NO_ACPI_ENABLE             0x08
#define ACPI_NO_DEVICE_INIT             0x10
#define ACPI_NO_PCI_INIT                0x20
#define ACPI_NO_OBJECT_INIT             0x40


/*
 * Sleep state constants
 */
#define ACPI_STATE_S0                   (UINT8) 0
#define ACPI_STATE_S1                   (UINT8) 1
#define ACPI_STATE_S2                   (UINT8) 2
#define ACPI_STATE_S3                   (UINT8) 3
#define ACPI_STATE_S4                   (UINT8) 4
#define ACPI_STATE_S4BIOS               (UINT8) 5
#define ACPI_STATE_S5                   (UINT8) 6
#define ACPI_S_STATES_MAX               ACPI_STATE_S5


/*
 *  Table types.  These values are passed to the table related APIs
 */

typedef UINT32                          ACPI_TABLE_TYPE;

#define ACPI_TABLE_RSDP                 (ACPI_TABLE_TYPE) 0
#define ACPI_TABLE_APIC                 (ACPI_TABLE_TYPE) 1
#define ACPI_TABLE_DSDT                 (ACPI_TABLE_TYPE) 2
#define ACPI_TABLE_FACP                 (ACPI_TABLE_TYPE) 3
#define ACPI_TABLE_FACS                 (ACPI_TABLE_TYPE) 4
#define ACPI_TABLE_PSDT                 (ACPI_TABLE_TYPE) 5
#define ACPI_TABLE_RSDT                 (ACPI_TABLE_TYPE) 6
#define ACPI_TABLE_SSDT                 (ACPI_TABLE_TYPE) 7
#define ACPI_TABLE_SBST                 (ACPI_TABLE_TYPE) 8
#define ACPI_TABLE_SPIC                 (ACPI_TABLE_TYPE) 9
#define ACPI_TABLE_BOOT                 (ACPI_TABLE_TYPE) 10
#define ACPI_TABLE_MAX                  10
#define NUM_ACPI_TABLES                 (ACPI_TABLE_MAX+1)


/*
 * Types associated with names.  The first group of
 * values correspond to the definition of the ACPI
 * ObjectType operator (See the ACPI Spec).  Therefore,
 * only add to the first group if the spec changes!
 *
 * Types must be kept in sync with the AcpiNsProperties
 * and AcpiNsTypeNames arrays
 */

typedef UINT32                          ACPI_OBJECT_TYPE;
typedef UINT8                           OBJECT_TYPE_INTERNAL;

#define ACPI_TYPE_ANY                   0  /* 0x00  */
#define ACPI_TYPE_NUMBER                1  /* 0x01  Byte/Word/Dword/Zero/One/Ones */
#define ACPI_TYPE_STRING                2  /* 0x02  */
#define ACPI_TYPE_BUFFER                3  /* 0x03  */
#define ACPI_TYPE_PACKAGE               4  /* 0x04  ByteConst, multiple DataTerm/Constant/SuperName */
#define ACPI_TYPE_FIELD_UNIT            5  /* 0x05  */
#define ACPI_TYPE_DEVICE                6  /* 0x06  Name, multiple Node */
#define ACPI_TYPE_EVENT                 7  /* 0x07  */
#define ACPI_TYPE_METHOD                8  /* 0x08  Name, ByteConst, multiple Code */
#define ACPI_TYPE_MUTEX                 9  /* 0x09  */
#define ACPI_TYPE_REGION                10 /* 0x0A  */
#define ACPI_TYPE_POWER                 11 /* 0x0B  Name,ByteConst,WordConst,multi Node */
#define ACPI_TYPE_PROCESSOR             12 /* 0x0C  Name,ByteConst,DWordConst,ByteConst,multi NmO */
#define ACPI_TYPE_THERMAL               13 /* 0x0D  Name, multiple Node */
#define ACPI_TYPE_BUFFER_FIELD          14 /* 0x0E  */
#define ACPI_TYPE_DDB_HANDLE            15 /* 0x0F  */
#define ACPI_TYPE_DEBUG_OBJECT          16 /* 0x10  */

#define ACPI_TYPE_MAX                   16

/*
 * This section contains object types that do not relate to the ACPI ObjectType operator.
 * They are used for various internal purposes only.  If new predefined ACPI_TYPEs are
 * added (via the ACPI specification), these internal types must move upwards.
 * Also, values exceeding the largest official ACPI ObjectType must not overlap with
 * defined AML opcodes.
 */
#define INTERNAL_TYPE_BEGIN             17

#define INTERNAL_TYPE_DEF_FIELD         17 /* 0x11  */
#define INTERNAL_TYPE_BANK_FIELD        18 /* 0x12  */
#define INTERNAL_TYPE_INDEX_FIELD       19 /* 0x13  */
#define INTERNAL_TYPE_REFERENCE         20 /* 0x14  Arg#, Local#, Name, Debug; used only in descriptors */
#define INTERNAL_TYPE_ALIAS             21 /* 0x15  */
#define INTERNAL_TYPE_NOTIFY            22 /* 0x16  */
#define INTERNAL_TYPE_ADDRESS_HANDLER   23 /* 0x17  */

#define INTERNAL_TYPE_NODE_MAX          23

/* These are pseudo-types because there are never any namespace nodes with these types */

#define INTERNAL_TYPE_DEF_FIELD_DEFN    24 /* 0x18  Name, ByteConst, multiple FieldElement */
#define INTERNAL_TYPE_BANK_FIELD_DEFN   25 /* 0x19  2 Name,DWordConst,ByteConst,multi FieldElement */
#define INTERNAL_TYPE_INDEX_FIELD_DEFN  26 /* 0x1A  2 Name, ByteConst, multiple FieldElement */
#define INTERNAL_TYPE_IF                27 /* 0x1B  OpCode, multiple Code */
#define INTERNAL_TYPE_ELSE              28 /* 0x1C  multiple Code */
#define INTERNAL_TYPE_WHILE             29 /* 0x1D  OpCode, multiple Code */
#define INTERNAL_TYPE_SCOPE             30 /* 0x1E  Name, multiple Node */
#define INTERNAL_TYPE_DEF_ANY           31 /* 0x1F  type is Any, suppress search of enclosing scopes */
#define INTERNAL_TYPE_METHOD_ARGUMENT   32 /* 0x20  */
#define INTERNAL_TYPE_METHOD_LOCAL_VAR  33 /* 0x21  */
#define INTERNAL_TYPE_EXTRA             34 /* 0x22  */

#define INTERNAL_TYPE_MAX               34

#define INTERNAL_TYPE_INVALID           35
#define ACPI_TYPE_NOT_FOUND             0xFF

/*
 * AcpiEvent Types:
 * ------------
 * Fixed & general purpose...
 */

typedef UINT32                          ACPI_EVENT_TYPE;

#define ACPI_EVENT_FIXED                (ACPI_EVENT_TYPE) 0
#define ACPI_EVENT_GPE                  (ACPI_EVENT_TYPE) 1

/*
 * Fixed events
 */

#define ACPI_EVENT_PMTIMER              (ACPI_EVENT_TYPE) 0
    /*
     * There's no bus master event so index 1 is used for IRQ's that are not
     * handled by the SCI handler
     */
#define ACPI_EVENT_NOT_USED             (ACPI_EVENT_TYPE) 1
#define ACPI_EVENT_GLOBAL               (ACPI_EVENT_TYPE) 2
#define ACPI_EVENT_POWER_BUTTON         (ACPI_EVENT_TYPE) 3
#define ACPI_EVENT_SLEEP_BUTTON         (ACPI_EVENT_TYPE) 4
#define ACPI_EVENT_RTC                  (ACPI_EVENT_TYPE) 5
#define ACPI_EVENT_GENERAL              (ACPI_EVENT_TYPE) 6
#define ACPI_EVENT_MAX                  6
#define NUM_FIXED_EVENTS                (ACPI_EVENT_TYPE) 7

#define ACPI_GPE_INVALID                0xFF
#define ACPI_GPE_MAX                    0xFF
#define NUM_GPE                         256

#define ACPI_EVENT_LEVEL_TRIGGERED      (ACPI_EVENT_TYPE) 1
#define ACPI_EVENT_EDGE_TRIGGERED       (ACPI_EVENT_TYPE) 2

/*
 * AcpiEvent Status:
 * -------------
 * The encoding of ACPI_EVENT_STATUS is illustrated below.
 * Note that a set bit (1) indicates the property is TRUE
 * (e.g. if bit 0 is set then the event is enabled).
 * +---------------+-+-+
 * |   Bits 31:2   |1|0|
 * +---------------+-+-+
 *          |       | |
 *          |       | +- Enabled?
 *          |       +--- Set?
 *          +----------- <Reserved>
 */
typedef UINT32                          ACPI_EVENT_STATUS;

#define ACPI_EVENT_FLAG_ENABLED         (ACPI_EVENT_STATUS) 0x01
#define ACPI_EVENT_FLAG_SET             (ACPI_EVENT_STATUS) 0x02


/* Notify types */

#define ACPI_SYSTEM_NOTIFY              0
#define ACPI_DEVICE_NOTIFY              1
#define ACPI_MAX_NOTIFY_HANDLER_TYPE    1

#define MAX_SYS_NOTIFY                  0x7f


/* Address Space (Operation Region) Types */

typedef UINT32                          ACPI_ADDRESS_SPACE_TYPE;

#define ADDRESS_SPACE_SYSTEM_MEMORY     (ACPI_ADDRESS_SPACE_TYPE) 0
#define ADDRESS_SPACE_SYSTEM_IO         (ACPI_ADDRESS_SPACE_TYPE) 1
#define ADDRESS_SPACE_PCI_CONFIG        (ACPI_ADDRESS_SPACE_TYPE) 2
#define ADDRESS_SPACE_EC                (ACPI_ADDRESS_SPACE_TYPE) 3
#define ADDRESS_SPACE_SMBUS             (ACPI_ADDRESS_SPACE_TYPE) 4


/*
 * External ACPI object definition
 */

typedef union AcpiObj
{
    ACPI_OBJECT_TYPE            Type;   /* See definition of AcpiNsType for values */
    struct
    {
        ACPI_OBJECT_TYPE            Type;
        ACPI_INTEGER                Value;      /* The actual number */
    } Number;

    struct
    {
        ACPI_OBJECT_TYPE            Type;
        UINT32                      Length;     /* # of bytes in string, excluding trailing null */
        NATIVE_CHAR                 *Pointer;   /* points to the string value */
    } String;

    struct
    {
        ACPI_OBJECT_TYPE            Type;
        UINT32                      Length;     /* # of bytes in buffer */
        UINT8                       *Pointer;   /* points to the buffer */
    } Buffer;

    struct
    {
        ACPI_OBJECT_TYPE            Type;
        UINT32                      Fill1;
        ACPI_HANDLE                 Handle;     /* object reference */
    } Reference;

    struct
    {
        ACPI_OBJECT_TYPE            Type;
        UINT32                      Count;      /* # of elements in package */
        union AcpiObj               *Elements;  /* Pointer to an array of ACPI_OBJECTs */
    } Package;

    struct
    {
        ACPI_OBJECT_TYPE            Type;
        UINT32                      ProcId;
        UINT32                      PblkAddress;
        UINT32                      PblkLength;
    } Processor;

    struct
    {
        ACPI_OBJECT_TYPE            Type;
        UINT32                      SystemLevel;
        UINT32                      ResourceOrder;
    } PowerResource;

} ACPI_OBJECT, *PACPI_OBJECT;


/*
 * List of objects, used as a parameter list for control method evaluation
 */

typedef struct AcpiObjList
{
    UINT32                      Count;
    ACPI_OBJECT                 *Pointer;

} ACPI_OBJECT_LIST, *PACPI_OBJECT_LIST;


/*
 * Miscellaneous common Data Structures used by the interfaces
 */

typedef struct
{
    UINT32                      Length;         /* Length in bytes of the buffer */
    void                        *Pointer;       /* pointer to buffer */

} ACPI_BUFFER;


/*
 * NameType for AcpiGetName
 */

#define ACPI_FULL_PATHNAME              0
#define ACPI_SINGLE_NAME                1
#define ACPI_NAME_TYPE_MAX              1


/*
 * Structure and flags for AcpiGetSystemInfo
 */

#define SYS_MODE_UNKNOWN                0x0000
#define SYS_MODE_ACPI                   0x0001
#define SYS_MODE_LEGACY                 0x0002
#define SYS_MODES_MASK                  0x0003

/*
 *  ACPI CPU Cx state handler
 */
typedef
ACPI_STATUS (*ACPI_SET_C_STATE_HANDLER) (
    NATIVE_UINT                 PblkAddress);

/*
 *  ACPI Cx State info
 */
typedef struct
{
    UINT32                      StateNumber;
    UINT32                      Latency;
} ACPI_CX_STATE;

/*
 *  ACPI CPU throttling info
 */
typedef struct
{
    UINT32                      StateNumber;
    UINT32                      PercentOfClock;
} ACPI_CPU_THROTTLING_STATE;

/*
 * ACPI Table Info.  One per ACPI table _type_
 */
typedef struct AcpiTableInfo
{
    UINT32                      Count;

} ACPI_TABLE_INFO;


/*
 * System info returned by AcpiGetSystemInfo()
 */

typedef struct _AcpiSysInfo
{
    UINT32                      AcpiCaVersion;
    UINT32                      Flags;
    UINT32                      TimerResolution;
    UINT32                      Reserved1;
    UINT32                      Reserved2;
    UINT32                      DebugLevel;
    UINT32                      DebugLayer;
    UINT32                      NumTableTypes;
    ACPI_TABLE_INFO             TableInfo [NUM_ACPI_TABLES];

} ACPI_SYSTEM_INFO;


/*
 *  System Initiailization data.  This data is passed to ACPIInitialize
 *  copyied to global data and retained by ACPI CA
 */

typedef struct _AcpiInitData
{
    void                        *RSDP_PhysicalAddress;  /*  Address of RSDP, needed it it is    */
                                                        /*  not found in the IA32 manner        */
} ACPI_INIT_DATA;

/*
 * Various handlers and callback procedures
 */

typedef
UINT32 (*FIXED_EVENT_HANDLER) (
    void                        *Context);

typedef
void (*GPE_HANDLER) (
    void                        *Context);

typedef
void (*NOTIFY_HANDLER) (
    ACPI_HANDLE                 Device,
    UINT32                      Value,
    void                        *Context);

#define ADDRESS_SPACE_READ              1
#define ADDRESS_SPACE_WRITE             2

typedef
ACPI_STATUS (*ADDRESS_SPACE_HANDLER) (
    UINT32                      Function,
    ACPI_INTEGER                Address,
    UINT32                      BitWidth,
    UINT32                      *Value,
    void                        *HandlerContext,
    void                        *RegionContext);

#define ACPI_DEFAULT_HANDLER            ((ADDRESS_SPACE_HANDLER) NULL)


typedef
ACPI_STATUS (*ADDRESS_SPACE_SETUP) (
    ACPI_HANDLE                 RegionHandle,
    UINT32                      Function,
    void                        *HandlerContext,
    void                        **RegionContext);

#define ACPI_REGION_ACTIVATE    0
#define ACPI_REGION_DEACTIVATE  1

typedef
ACPI_STATUS (*WALK_CALLBACK) (
    ACPI_HANDLE                 ObjHandle,
    UINT32                      NestingLevel,
    void                        *Context,
    void                        **ReturnValue);


/* Interrupt handler return values */

#define INTERRUPT_NOT_HANDLED           0x00
#define INTERRUPT_HANDLED               0x01


/* Structure and flags for AcpiGetDeviceInfo */

#define ACPI_VALID_HID                  0x1
#define ACPI_VALID_UID                  0x2
#define ACPI_VALID_ADR                  0x4
#define ACPI_VALID_STA                  0x8


#define ACPI_COMMON_OBJ_INFO \
    ACPI_OBJECT_TYPE            Type;           /* ACPI object type */ \
    ACPI_NAME                   Name            /* ACPI object Name */


typedef struct
{
    ACPI_COMMON_OBJ_INFO;
} ACPI_OBJ_INFO_HEADER;


typedef struct
{
    ACPI_COMMON_OBJ_INFO;

    UINT32                      Valid;              /*  Are the next bits legit? */
    NATIVE_CHAR                 HardwareId [9];     /*  _HID value if any */
    NATIVE_CHAR                 UniqueId[9];        /*  _UID value if any */
    ACPI_INTEGER                Address;            /*  _ADR value if any */
    UINT32                      CurrentStatus;      /*  _STA value */
} ACPI_DEVICE_INFO;


/* Context structs for address space handlers */

typedef struct
{
    UINT32                      Seg;
    UINT32                      Bus;
    UINT32                      DevFunc;
} PCI_HANDLER_CONTEXT;


typedef struct
{
    UINT8                       *MappedPhysicalAddress;
    UINT8                       *MappedLogicalAddress;
    UINT32                      MappedLength;
} MEM_HANDLER_CONTEXT;


/*
 * C-state handler
 */

typedef ACPI_STATUS (*ACPI_C_STATE_HANDLER) (ACPI_IO_ADDRESS, UINT32*);


/*
 * Definitions for Resource Attributes
 */

/*
 *  Memory Attributes
 */
#define READ_ONLY_MEMORY                (UINT8) 0x00
#define READ_WRITE_MEMORY               (UINT8) 0x01

#define NON_CACHEABLE_MEMORY            (UINT8) 0x00
#define CACHABLE_MEMORY                 (UINT8) 0x01
#define WRITE_COMBINING_MEMORY          (UINT8) 0x02
#define PREFETCHABLE_MEMORY             (UINT8) 0x03

/*
 *  IO Attributes
 *  The ISA IO ranges are: n000-n0FFh,  n400-n4FFh, n800-n8FFh, nC00-nCFFh.
 *  The non-ISA IO ranges are: n100-n3FFh,  n500-n7FFh, n900-nBFFh, nCD0-nFFFh.
 */
#define NON_ISA_ONLY_RANGES             (UINT8) 0x01
#define ISA_ONLY_RANGES                 (UINT8) 0x02
#define ENTIRE_RANGE                    (NON_ISA_ONLY_RANGES | ISA_ONLY_RANGES)

/*
 *  IO Port Descriptor Decode
 */
#define DECODE_10                       (UINT8) 0x00    /* 10-bit IO address decode */
#define DECODE_16                       (UINT8) 0x01    /* 16-bit IO address decode */

/*
 *  IRQ Attributes
 */
#define EDGE_SENSITIVE                  (UINT8) 0x00
#define LEVEL_SENSITIVE                 (UINT8) 0x01

#define ACTIVE_HIGH                     (UINT8) 0x00
#define ACTIVE_LOW                      (UINT8) 0x01

#define EXCLUSIVE                       (UINT8) 0x00
#define SHARED                          (UINT8) 0x01

/*
 *  DMA Attributes
 */
#define COMPATIBILITY                   (UINT8) 0x00
#define TYPE_A                          (UINT8) 0x01
#define TYPE_B                          (UINT8) 0x02
#define TYPE_F                          (UINT8) 0x03

#define NOT_BUS_MASTER                  (UINT8) 0x00
#define BUS_MASTER                      (UINT8) 0x01

#define TRANSFER_8                      (UINT8) 0x00
#define TRANSFER_8_16                   (UINT8) 0x01
#define TRANSFER_16                     (UINT8) 0x02

/*
 * Start Dependent Functions Priority definitions
 */
#define GOOD_CONFIGURATION              (UINT8) 0x00
#define ACCEPTABLE_CONFIGURATION        (UINT8) 0x01
#define SUB_OPTIMAL_CONFIGURATION       (UINT8) 0x02

/*
 *  16, 32 and 64-bit Address Descriptor resource types
 */
#define MEMORY_RANGE                    (UINT8) 0x00
#define IO_RANGE                        (UINT8) 0x01
#define BUS_NUMBER_RANGE                (UINT8) 0x02

#define ADDRESS_NOT_FIXED               (UINT8) 0x00
#define ADDRESS_FIXED                   (UINT8) 0x01

#define POS_DECODE                      (UINT8) 0x00
#define SUB_DECODE                      (UINT8) 0x01

#define PRODUCER                        (UINT8) 0x00
#define CONSUMER                        (UINT8) 0x01


/*
 *  Structures used to describe device resources
 */
typedef struct
{
    UINT32                      EdgeLevel;
    UINT32                      ActiveHighLow;
    UINT32                      SharedExclusive;
    UINT32                      NumberOfInterrupts;
    UINT32                      Interrupts[1];

} IRQ_RESOURCE;

typedef struct
{
    UINT32                      Type;
    UINT32                      BusMaster;
    UINT32                      Transfer;
    UINT32                      NumberOfChannels;
    UINT32                      Channels[1];

} DMA_RESOURCE;

typedef struct
{
    UINT32                      CompatibilityPriority;
    UINT32                      PerformanceRobustness;

} START_DEPENDENT_FUNCTIONS_RESOURCE;

/*
 * END_DEPENDENT_FUNCTIONS_RESOURCE struct is not
 *  needed because it has no fields
 */

typedef struct
{
    UINT32                      IoDecode;
    UINT32                      MinBaseAddress;
    UINT32                      MaxBaseAddress;
    UINT32                      Alignment;
    UINT32                      RangeLength;

} IO_RESOURCE;

typedef struct
{
    UINT32                      BaseAddress;
    UINT32                      RangeLength;

} FIXED_IO_RESOURCE;

typedef struct
{
    UINT32                      Length;
    UINT8                       Reserved[1];

} VENDOR_RESOURCE;

typedef struct
{
    UINT32                      ReadWriteAttribute;
    UINT32                      MinBaseAddress;
    UINT32                      MaxBaseAddress;
    UINT32                      Alignment;
    UINT32                      RangeLength;

} MEMORY24_RESOURCE;

typedef struct
{
    UINT32                      ReadWriteAttribute;
    UINT32                      MinBaseAddress;
    UINT32                      MaxBaseAddress;
    UINT32                      Alignment;
    UINT32                      RangeLength;

} MEMORY32_RESOURCE;

typedef struct
{
    UINT32                      ReadWriteAttribute;
    UINT32                      RangeBaseAddress;
    UINT32                      RangeLength;

} FIXED_MEMORY32_RESOURCE;

typedef struct
{
    UINT16                      CacheAttribute;
    UINT16                      ReadWriteAttribute;

} MEMORY_ATTRIBUTE;

typedef struct
{
    UINT16                      RangeAttribute;
    UINT16                      Reserved;

} IO_ATTRIBUTE;

typedef struct
{
    UINT16                      Reserved1;
    UINT16                      Reserved2;

} BUS_ATTRIBUTE;

typedef union
{
    MEMORY_ATTRIBUTE            Memory;
    IO_ATTRIBUTE                Io;
    BUS_ATTRIBUTE               Bus;

} ATTRIBUTE_DATA;

typedef struct
{
    UINT32                      ResourceType;
    UINT32                      ProducerConsumer;
    UINT32                      Decode;
    UINT32                      MinAddressFixed;
    UINT32                      MaxAddressFixed;
    ATTRIBUTE_DATA              Attribute;
    UINT32                      Granularity;
    UINT32                      MinAddressRange;
    UINT32                      MaxAddressRange;
    UINT32                      AddressTranslationOffset;
    UINT32                      AddressLength;
    UINT32                      ResourceSourceIndex;
    UINT32                      ResourceSourceStringLength;
    NATIVE_CHAR                 ResourceSource[1];

} ADDRESS16_RESOURCE;

typedef struct
{
    UINT32                      ResourceType;
    UINT32                      ProducerConsumer;
    UINT32                      Decode;
    UINT32                      MinAddressFixed;
    UINT32                      MaxAddressFixed;
    ATTRIBUTE_DATA              Attribute;
    UINT32                      Granularity;
    UINT32                      MinAddressRange;
    UINT32                      MaxAddressRange;
    UINT32                      AddressTranslationOffset;
    UINT32                      AddressLength;
    UINT32                      ResourceSourceIndex;
    UINT32                      ResourceSourceStringLength;
    NATIVE_CHAR                 ResourceSource[1];

} ADDRESS32_RESOURCE;

typedef struct
{
    UINT32                      ProducerConsumer;
    UINT32                      EdgeLevel;
    UINT32                      ActiveHighLow;
    UINT32                      SharedExclusive;
    UINT32                      NumberOfInterrupts;
    UINT32                      Interrupts[1];
    UINT32                      ResourceSourceIndex;
    UINT32                      ResourceSourceStringLength;
    NATIVE_CHAR                 ResourceSource[1];

} EXTENDED_IRQ_RESOURCE;

typedef enum
{
    Irq,
    Dma,
    StartDependentFunctions,
    EndDependentFunctions,
    Io,
    FixedIo,
    VendorSpecific,
    EndTag,
    Memory24,
    Memory32,
    FixedMemory32,
    Address16,
    Address32,
    ExtendedIrq
} RESOURCE_TYPE;

typedef union
{
    IRQ_RESOURCE                        Irq;
    DMA_RESOURCE                        Dma;
    START_DEPENDENT_FUNCTIONS_RESOURCE  StartDependentFunctions;
    IO_RESOURCE                         Io;
    FIXED_IO_RESOURCE                   FixedIo;
    VENDOR_RESOURCE                     VendorSpecific;
    MEMORY24_RESOURCE                   Memory24;
    MEMORY32_RESOURCE                   Memory32;
    FIXED_MEMORY32_RESOURCE             FixedMemory32;
    ADDRESS16_RESOURCE                  Address16;
    ADDRESS32_RESOURCE                  Address32;
    EXTENDED_IRQ_RESOURCE               ExtendedIrq;
} RESOURCE_DATA;

typedef struct _resource_tag
{
    RESOURCE_TYPE               Id;
    UINT32                      Length;
    RESOURCE_DATA               Data;
} RESOURCE;

#define RESOURCE_LENGTH                 12
#define RESOURCE_LENGTH_NO_DATA         8

#define NEXT_RESOURCE(Res)    (RESOURCE*)((UINT8*) Res + Res->length)

/*
 * END: Definitions for Resource Attributes
 */

/*
 * Definitions for PCI Routing tables
 */
typedef struct
{
    ACPI_INTEGER                Address;
    UINT32                      Pin;
    UINT32                      SourceIndex;
    NATIVE_CHAR                 Source[1];

} PRT_ENTRY;

typedef struct _prt_tag
{
    UINT32                      Length;
    PRT_ENTRY                   Data;

} PCI_ROUTING_TABLE;


/*
 * END: Definitions for PCI Routing tables
 */

#endif /* __ACTYPES_H__ */
