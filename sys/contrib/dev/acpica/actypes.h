/******************************************************************************
 *
 * Name: actypes.h - Common data types for the entire ACPI subsystem
 *       $Revision: 238 $
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

#ifndef __ACTYPES_H__
#define __ACTYPES_H__

/*! [Begin] no source code translation (keep the typedefs) */



/*
 * Data type ranges
 */
#define ACPI_UINT8_MAX                  (UINT8)  0xFF
#define ACPI_UINT16_MAX                 (UINT16) 0xFFFF
#define ACPI_UINT32_MAX                 (UINT32) 0xFFFFFFFF
#define ACPI_UINT64_MAX                 (UINT64) 0xFFFFFFFFFFFFFFFF
#define ACPI_ASCII_MAX                  0x7F



/*
 * Data types - Fixed across all compilation models
 *
 * BOOLEAN      Logical Boolean.
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
 */

#ifndef ACPI_MACHINE_WIDTH
#error ACPI_MACHINE_WIDTH not defined
#endif

#if ACPI_MACHINE_WIDTH == 64
/*
 * 64-bit type definitions
 */
typedef unsigned char                   UINT8;
typedef unsigned char                   BOOLEAN;
typedef unsigned short                  UINT16;
typedef int                             INT32;
typedef unsigned int                    UINT32;
typedef COMPILER_DEPENDENT_INT64        INT64;
typedef COMPILER_DEPENDENT_UINT64       UINT64;

typedef INT64                           NATIVE_INT;
typedef UINT64                          NATIVE_UINT;

typedef UINT32                          NATIVE_UINT_MAX32;
typedef UINT64                          NATIVE_UINT_MIN32;

typedef UINT64                          ACPI_TBLPTR;
typedef UINT64                          ACPI_IO_ADDRESS;
typedef UINT64                          ACPI_PHYSICAL_ADDRESS;
typedef UINT64                          ACPI_SIZE;

#define ALIGNED_ADDRESS_BOUNDARY        0x00000008      /* No hardware alignment support in IA64 */
#define ACPI_USE_NATIVE_DIVIDE                          /* Native 64-bit integer support */
#define ACPI_MAX_PTR                    ACPI_UINT64_MAX
#define ACPI_SIZE_MAX                   ACPI_UINT64_MAX


#elif ACPI_MACHINE_WIDTH == 16
/*
 * 16-bit type definitions
 */
typedef unsigned char                   UINT8;
typedef unsigned char                   BOOLEAN;
typedef unsigned int                    UINT16;
typedef long                            INT32;
typedef int                             INT16;
typedef unsigned long                   UINT32;

typedef struct
{
    UINT32                                  Lo;
    UINT32                                  Hi;

} UINT64;

typedef UINT16                          NATIVE_UINT;
typedef INT16                           NATIVE_INT;

typedef UINT16                          NATIVE_UINT_MAX32;
typedef UINT32                          NATIVE_UINT_MIN32;

typedef UINT32                          ACPI_TBLPTR;
typedef UINT32                          ACPI_IO_ADDRESS;
typedef char                            *ACPI_PHYSICAL_ADDRESS;
typedef UINT16                          ACPI_SIZE;

#define ALIGNED_ADDRESS_BOUNDARY        0x00000002
#define _HW_ALIGNMENT_SUPPORT
#define ACPI_USE_NATIVE_DIVIDE                          /* No 64-bit integers, ok to use native divide */
#define ACPI_MAX_PTR                    ACPI_UINT16_MAX
#define ACPI_SIZE_MAX                   ACPI_UINT16_MAX

/*
 * (16-bit only) internal integers must be 32-bits, so
 * 64-bit integers cannot be supported
 */
#define ACPI_NO_INTEGER64_SUPPORT


#elif ACPI_MACHINE_WIDTH == 32
/*
 * 32-bit type definitions (default)
 */
typedef unsigned char                   UINT8;
typedef unsigned char                   BOOLEAN;
typedef unsigned short                  UINT16;
typedef int                             INT32;
typedef unsigned int                    UINT32;
typedef COMPILER_DEPENDENT_INT64        INT64;
typedef COMPILER_DEPENDENT_UINT64       UINT64;

typedef INT32                           NATIVE_INT;
typedef UINT32                          NATIVE_UINT;

typedef UINT32                          NATIVE_UINT_MAX32;
typedef UINT32                          NATIVE_UINT_MIN32;

typedef UINT64                          ACPI_TBLPTR;
typedef UINT32                          ACPI_IO_ADDRESS;
typedef UINT64                          ACPI_PHYSICAL_ADDRESS;
typedef UINT32                          ACPI_SIZE;

#define ALIGNED_ADDRESS_BOUNDARY        0x00000004
#define _HW_ALIGNMENT_SUPPORT
#define ACPI_MAX_PTR                    ACPI_UINT32_MAX
#define ACPI_SIZE_MAX                   ACPI_UINT32_MAX

#else
#error unknown ACPI_MACHINE_WIDTH
#endif


/*
 * Miscellaneous common types
 */

typedef UINT32                          UINT32_BIT;
typedef NATIVE_UINT                     ACPI_PTRDIFF;
typedef char                            NATIVE_CHAR;


#ifdef DEFINE_ALTERNATE_TYPES
/*
 * Types used only in translated source, defined here to enable
 * cross-platform compilation only.
 */
typedef INT32                           s32;
typedef UINT8                           u8;
typedef UINT16                          u16;
typedef UINT32                          u32;
typedef UINT64                          u64;
#endif
/*! [End] no source code translation !*/


/*
 * Pointer overlays to avoid lots of typecasting for
 * code that accepts both physical and logical pointers.
 */
typedef union acpi_ptrs
{
    ACPI_PHYSICAL_ADDRESS       Physical;
    void                        *Logical;
    ACPI_TBLPTR                 Value;

} ACPI_POINTERS;

typedef struct AcpiPointer
{
    UINT32                      PointerType;
    union acpi_ptrs             Pointer;

} ACPI_POINTER;

/* PointerTypes for above */

#define ACPI_PHYSICAL_POINTER           0x01
#define ACPI_LOGICAL_POINTER            0x02

/* Processor mode */

#define ACPI_PHYSICAL_ADDRESSING        0x04
#define ACPI_LOGICAL_ADDRESSING         0x08
#define ACPI_MEMORY_MODE                0x0C

#define ACPI_PHYSMODE_PHYSPTR           ACPI_PHYSICAL_ADDRESSING | ACPI_PHYSICAL_POINTER
#define ACPI_LOGMODE_PHYSPTR            ACPI_LOGICAL_ADDRESSING  | ACPI_PHYSICAL_POINTER
#define ACPI_LOGMODE_LOGPTR             ACPI_LOGICAL_ADDRESSING  | ACPI_LOGICAL_POINTER


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
typedef UINT32                          ACPI_NAME;      /* 4-byte ACPI name */
typedef char*                           ACPI_STRING;    /* Null terminated ASCII string */
typedef void*                           ACPI_HANDLE;    /* Actually a ptr to an Node */

typedef struct
{
    UINT32                      Lo;
    UINT32                      Hi;

} UINT64_STRUCT;

typedef union
{
    UINT64                      Full;
    UINT64_STRUCT               Part;

} UINT64_OVERLAY;

typedef struct
{
    UINT32                      Lo;
    UINT32                      Hi;

} UINT32_STRUCT;


/*
 * Acpi integer width. In ACPI version 1, integers are
 * 32 bits.  In ACPI version 2, integers are 64 bits.
 * Note that this pertains to the ACPI integer type only, not
 * other integers used in the implementation of the ACPI CA
 * subsystem.
 */
#ifdef ACPI_NO_INTEGER64_SUPPORT

/* 32-bit integers only, no 64-bit support */

typedef UINT32                          ACPI_INTEGER;
#define ACPI_INTEGER_MAX                ACPI_UINT32_MAX
#define ACPI_INTEGER_BIT_SIZE           32
#define ACPI_MAX_BCD_VALUE              99999999
#define ACPI_MAX_BCD_DIGITS             8
#define ACPI_MAX_DECIMAL_DIGITS         10

#define ACPI_USE_NATIVE_DIVIDE          /* Use compiler native 32-bit divide */


#else

/* 64-bit integers */

typedef UINT64                          ACPI_INTEGER;
#define ACPI_INTEGER_MAX                ACPI_UINT64_MAX
#define ACPI_INTEGER_BIT_SIZE           64
#define ACPI_MAX_BCD_VALUE              9999999999999999
#define ACPI_MAX_BCD_DIGITS             16
#define ACPI_MAX_DECIMAL_DIGITS         19

#if ACPI_MACHINE_WIDTH == 64
#define ACPI_USE_NATIVE_DIVIDE          /* Use compiler native 64-bit divide */
#endif
#endif


/*
 * Constants with special meanings
 */

#define ACPI_ROOT_OBJECT                (ACPI_HANDLE) ACPI_PTR_ADD (char, NULL, ACPI_MAX_PTR)


/*
 * Initialization sequence
 */
#define ACPI_FULL_INITIALIZATION        0x00
#define ACPI_NO_ADDRESS_SPACE_INIT      0x01
#define ACPI_NO_HARDWARE_INIT           0x02
#define ACPI_NO_EVENT_INIT              0x04
#define ACPI_NO_HANDLER_INIT            0x08
#define ACPI_NO_ACPI_ENABLE             0x10
#define ACPI_NO_DEVICE_INIT             0x20
#define ACPI_NO_OBJECT_INIT             0x40

/*
 * Initialization state
 */
#define ACPI_INITIALIZED_OK             0x01

/*
 * Power state values
 */

#define ACPI_STATE_UNKNOWN              (UINT8) 0xFF

#define ACPI_STATE_S0                   (UINT8) 0
#define ACPI_STATE_S1                   (UINT8) 1
#define ACPI_STATE_S2                   (UINT8) 2
#define ACPI_STATE_S3                   (UINT8) 3
#define ACPI_STATE_S4                   (UINT8) 4
#define ACPI_STATE_S5                   (UINT8) 5
#define ACPI_S_STATES_MAX               ACPI_STATE_S5
#define ACPI_S_STATE_COUNT              6

#define ACPI_STATE_D0                   (UINT8) 0
#define ACPI_STATE_D1                   (UINT8) 1
#define ACPI_STATE_D2                   (UINT8) 2
#define ACPI_STATE_D3                   (UINT8) 3
#define ACPI_D_STATES_MAX               ACPI_STATE_D3
#define ACPI_D_STATE_COUNT              4

#define ACPI_STATE_C0                   (UINT8) 0
#define ACPI_STATE_C1                   (UINT8) 1
#define ACPI_STATE_C2                   (UINT8) 2
#define ACPI_STATE_C3                   (UINT8) 3
#define ACPI_C_STATES_MAX               ACPI_STATE_C3
#define ACPI_C_STATE_COUNT              4

/*
 * Sleep type invalid value
 */
#define ACPI_SLEEP_TYPE_MAX             0x7
#define ACPI_SLEEP_TYPE_INVALID         0xFF

/*
 * Standard notify values
 */
#define ACPI_NOTIFY_BUS_CHECK           (UINT8) 0
#define ACPI_NOTIFY_DEVICE_CHECK        (UINT8) 1
#define ACPI_NOTIFY_DEVICE_WAKE         (UINT8) 2
#define ACPI_NOTIFY_EJECT_REQUEST       (UINT8) 3
#define ACPI_NOTIFY_DEVICE_CHECK_LIGHT  (UINT8) 4
#define ACPI_NOTIFY_FREQUENCY_MISMATCH  (UINT8) 5
#define ACPI_NOTIFY_BUS_MODE_MISMATCH   (UINT8) 6
#define ACPI_NOTIFY_POWER_FAULT         (UINT8) 7


/*
 *  Table types.  These values are passed to the table related APIs
 */

typedef UINT32                          ACPI_TABLE_TYPE;

#define ACPI_TABLE_RSDP                 (ACPI_TABLE_TYPE) 0
#define ACPI_TABLE_DSDT                 (ACPI_TABLE_TYPE) 1
#define ACPI_TABLE_FADT                 (ACPI_TABLE_TYPE) 2
#define ACPI_TABLE_FACS                 (ACPI_TABLE_TYPE) 3
#define ACPI_TABLE_PSDT                 (ACPI_TABLE_TYPE) 4
#define ACPI_TABLE_SSDT                 (ACPI_TABLE_TYPE) 5
#define ACPI_TABLE_XSDT                 (ACPI_TABLE_TYPE) 6
#define ACPI_TABLE_MAX                  6
#define NUM_ACPI_TABLES                 (ACPI_TABLE_MAX+1)


/*
 * Types associated with names.  The first group of
 * values correspond to the definition of the ACPI
 * ObjectType operator (See the ACPI Spec).  Therefore,
 * only add to the first group if the spec changes.
 *
 * Types must be kept in sync with the AcpiNsProperties
 * and AcpiNsTypeNames arrays
 */

typedef UINT32                          ACPI_OBJECT_TYPE;

#define ACPI_TYPE_ANY                   0x00
#define ACPI_TYPE_INTEGER               0x01  /* Byte/Word/Dword/Zero/One/Ones */
#define ACPI_TYPE_STRING                0x02
#define ACPI_TYPE_BUFFER                0x03
#define ACPI_TYPE_PACKAGE               0x04  /* ByteConst, multiple DataTerm/Constant/SuperName */
#define ACPI_TYPE_FIELD_UNIT            0x05
#define ACPI_TYPE_DEVICE                0x06  /* Name, multiple Node */
#define ACPI_TYPE_EVENT                 0x07
#define ACPI_TYPE_METHOD                0x08  /* Name, ByteConst, multiple Code */
#define ACPI_TYPE_MUTEX                 0x09
#define ACPI_TYPE_REGION                0x0A
#define ACPI_TYPE_POWER                 0x0B  /* Name,ByteConst,WordConst,multi Node */
#define ACPI_TYPE_PROCESSOR             0x0C  /* Name,ByteConst,DWordConst,ByteConst,multi NmO */
#define ACPI_TYPE_THERMAL               0x0D  /* Name, multiple Node */
#define ACPI_TYPE_BUFFER_FIELD          0x0E
#define ACPI_TYPE_DDB_HANDLE            0x0F
#define ACPI_TYPE_DEBUG_OBJECT          0x10

#define ACPI_TYPE_MAX                   0x10

/*
 * This section contains object types that do not relate to the ACPI ObjectType operator.
 * They are used for various internal purposes only.  If new predefined ACPI_TYPEs are
 * added (via the ACPI specification), these internal types must move upwards.
 * Also, values exceeding the largest official ACPI ObjectType must not overlap with
 * defined AML opcodes.
 */
#define INTERNAL_TYPE_BEGIN             0x11

#define INTERNAL_TYPE_REGION_FIELD      0x11
#define INTERNAL_TYPE_BANK_FIELD        0x12
#define INTERNAL_TYPE_INDEX_FIELD       0x13
#define INTERNAL_TYPE_REFERENCE         0x14  /* Arg#, Local#, Name, Debug; used only in descriptors */
#define INTERNAL_TYPE_ALIAS             0x15
#define INTERNAL_TYPE_NOTIFY            0x16
#define INTERNAL_TYPE_ADDRESS_HANDLER   0x17
#define INTERNAL_TYPE_RESOURCE          0x18
#define INTERNAL_TYPE_RESOURCE_FIELD    0x19


#define INTERNAL_TYPE_NODE_MAX          0x19

/* These are pseudo-types because there are never any namespace nodes with these types */

#define INTERNAL_TYPE_FIELD_DEFN        0x1A  /* Name, ByteConst, multiple FieldElement */
#define INTERNAL_TYPE_BANK_FIELD_DEFN   0x1B  /* 2 Name,DWordConst,ByteConst,multi FieldElement */
#define INTERNAL_TYPE_INDEX_FIELD_DEFN  0x1C  /* 2 Name, ByteConst, multiple FieldElement */
#define INTERNAL_TYPE_IF                0x1D
#define INTERNAL_TYPE_ELSE              0x1E
#define INTERNAL_TYPE_WHILE             0x1F
#define INTERNAL_TYPE_SCOPE             0x20  /* Name, multiple Node */
#define INTERNAL_TYPE_DEF_ANY           0x21  /* type is Any, suppress search of enclosing scopes */
#define INTERNAL_TYPE_EXTRA             0x22
#define INTERNAL_TYPE_DATA              0x23

#define INTERNAL_TYPE_MAX               0x23

#define INTERNAL_TYPE_INVALID           0x24
#define ACPI_TYPE_NOT_FOUND             0xFF


/*
 * Bitmapped ACPI types
 * Used internally only
 */
#define ACPI_BTYPE_ANY                  0x00000000
#define ACPI_BTYPE_INTEGER              0x00000001
#define ACPI_BTYPE_STRING               0x00000002
#define ACPI_BTYPE_BUFFER               0x00000004
#define ACPI_BTYPE_PACKAGE              0x00000008
#define ACPI_BTYPE_FIELD_UNIT           0x00000010
#define ACPI_BTYPE_DEVICE               0x00000020
#define ACPI_BTYPE_EVENT                0x00000040
#define ACPI_BTYPE_METHOD               0x00000080
#define ACPI_BTYPE_MUTEX                0x00000100
#define ACPI_BTYPE_REGION               0x00000200
#define ACPI_BTYPE_POWER                0x00000400
#define ACPI_BTYPE_PROCESSOR            0x00000800
#define ACPI_BTYPE_THERMAL              0x00001000
#define ACPI_BTYPE_BUFFER_FIELD         0x00002000
#define ACPI_BTYPE_DDB_HANDLE           0x00004000
#define ACPI_BTYPE_DEBUG_OBJECT         0x00008000
#define ACPI_BTYPE_REFERENCE            0x00010000
#define ACPI_BTYPE_RESOURCE             0x00020000

#define ACPI_BTYPE_COMPUTE_DATA         (ACPI_BTYPE_INTEGER | ACPI_BTYPE_STRING | ACPI_BTYPE_BUFFER)

#define ACPI_BTYPE_DATA                 (ACPI_BTYPE_COMPUTE_DATA  | ACPI_BTYPE_PACKAGE)
#define ACPI_BTYPE_DATA_REFERENCE       (ACPI_BTYPE_DATA | ACPI_BTYPE_REFERENCE | ACPI_BTYPE_DDB_HANDLE)
#define ACPI_BTYPE_DEVICE_OBJECTS       (ACPI_BTYPE_DEVICE | ACPI_BTYPE_THERMAL | ACPI_BTYPE_PROCESSOR)
#define ACPI_BTYPE_OBJECTS_AND_REFS     0x0001FFFF  /* ARG or LOCAL */
#define ACPI_BTYPE_ALL_OBJECTS          0x0000FFFF

/*
 * All I/O
 */
#define ACPI_READ                       0
#define ACPI_WRITE                      1


/*
 * AcpiEvent Types: Fixed & General Purpose
 */

typedef UINT32                          ACPI_EVENT_TYPE;

#define ACPI_EVENT_FIXED                0
#define ACPI_EVENT_GPE                  1

/*
 * Fixed events
 */

#define ACPI_EVENT_PMTIMER              0
#define ACPI_EVENT_GLOBAL               1
#define ACPI_EVENT_POWER_BUTTON         2
#define ACPI_EVENT_SLEEP_BUTTON         3
#define ACPI_EVENT_RTC                  4
#define ACPI_EVENT_MAX                  4
#define ACPI_NUM_FIXED_EVENTS           ACPI_EVENT_MAX + 1

#define ACPI_GPE_INVALID                0xFF
#define ACPI_GPE_MAX                    0xFF
#define ACPI_NUM_GPE                    256

#define ACPI_EVENT_LEVEL_TRIGGERED      1
#define ACPI_EVENT_EDGE_TRIGGERED       2

/*
 * GPEs
 */

#define ACPI_EVENT_WAKE_ENABLE          0x1

#define ACPI_EVENT_WAKE_DISABLE         0x1


/*
 * AcpiEvent Status:
 * -------------
 * The encoding of ACPI_EVENT_STATUS is illustrated below.
 * Note that a set bit (1) indicates the property is TRUE
 * (e.g. if bit 0 is set then the event is enabled).
 * +-------------+-+-+-+
 * |   Bits 31:3 |2|1|0|
 * +-------------+-+-+-+
 *          |     | | |
 *          |     | | +- Enabled?
 *          |     | +--- Enabled for wake?
 *          |     +----- Set?
 *          +----------- <Reserved>
 */
typedef UINT32                          ACPI_EVENT_STATUS;

#define ACPI_EVENT_FLAG_DISABLED        (ACPI_EVENT_STATUS) 0x00
#define ACPI_EVENT_FLAG_ENABLED         (ACPI_EVENT_STATUS) 0x01
#define ACPI_EVENT_FLAG_WAKE_ENABLED    (ACPI_EVENT_STATUS) 0x02
#define ACPI_EVENT_FLAG_SET             (ACPI_EVENT_STATUS) 0x04


/* Notify types */

#define ACPI_SYSTEM_NOTIFY              0
#define ACPI_DEVICE_NOTIFY              1
#define ACPI_MAX_NOTIFY_HANDLER_TYPE    1

#define ACPI_MAX_SYS_NOTIFY                  0x7f


/* Address Space (Operation Region) Types */

typedef UINT8                           ACPI_ADR_SPACE_TYPE;

#define ACPI_ADR_SPACE_SYSTEM_MEMORY    (ACPI_ADR_SPACE_TYPE) 0
#define ACPI_ADR_SPACE_SYSTEM_IO        (ACPI_ADR_SPACE_TYPE) 1
#define ACPI_ADR_SPACE_PCI_CONFIG       (ACPI_ADR_SPACE_TYPE) 2
#define ACPI_ADR_SPACE_EC               (ACPI_ADR_SPACE_TYPE) 3
#define ACPI_ADR_SPACE_SMBUS            (ACPI_ADR_SPACE_TYPE) 4
#define ACPI_ADR_SPACE_CMOS             (ACPI_ADR_SPACE_TYPE) 5
#define ACPI_ADR_SPACE_PCI_BAR_TARGET   (ACPI_ADR_SPACE_TYPE) 6
#define ACPI_ADR_SPACE_DATA_TABLE       (ACPI_ADR_SPACE_TYPE) 7


/*
 * BitRegister IDs
 * These are bitfields defined within the full ACPI registers
 */
#define ACPI_BITREG_TIMER_STATUS                0x00
#define ACPI_BITREG_BUS_MASTER_STATUS           0x01
#define ACPI_BITREG_GLOBAL_LOCK_STATUS          0x02
#define ACPI_BITREG_POWER_BUTTON_STATUS         0x03
#define ACPI_BITREG_SLEEP_BUTTON_STATUS         0x04
#define ACPI_BITREG_RT_CLOCK_STATUS             0x05
#define ACPI_BITREG_WAKE_STATUS                 0x06

#define ACPI_BITREG_TIMER_ENABLE                0x07
#define ACPI_BITREG_GLOBAL_LOCK_ENABLE          0x08
#define ACPI_BITREG_POWER_BUTTON_ENABLE         0x09
#define ACPI_BITREG_SLEEP_BUTTON_ENABLE         0x0A
#define ACPI_BITREG_RT_CLOCK_ENABLE             0x0B
#define ACPI_BITREG_WAKE_ENABLE                 0x0C

#define ACPI_BITREG_SCI_ENABLE                  0x0D
#define ACPI_BITREG_BUS_MASTER_RLD              0x0E
#define ACPI_BITREG_GLOBAL_LOCK_RELEASE         0x0F
#define ACPI_BITREG_SLEEP_TYPE_A                0x10
#define ACPI_BITREG_SLEEP_TYPE_B                0x11
#define ACPI_BITREG_SLEEP_ENABLE                0x12

#define ACPI_BITREG_ARB_DISABLE                 0x13

#define ACPI_BITREG_MAX                         0x13
#define ACPI_NUM_BITREG                         ACPI_BITREG_MAX + 1

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
    } Integer;

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
        ACPI_IO_ADDRESS             PblkAddress;
        UINT32                      PblkLength;
    } Processor;

    struct
    {
        ACPI_OBJECT_TYPE            Type;
        UINT32                      SystemLevel;
        UINT32                      ResourceOrder;
    } PowerResource;

} ACPI_OBJECT;


/*
 * List of objects, used as a parameter list for control method evaluation
 */

typedef struct AcpiObjList
{
    UINT32                      Count;
    ACPI_OBJECT                 *Pointer;

} ACPI_OBJECT_LIST;


/*
 * Miscellaneous common Data Structures used by the interfaces
 */

#define ACPI_NO_BUFFER              0
#define ACPI_ALLOCATE_BUFFER        (ACPI_SIZE) (-1)
#define ACPI_ALLOCATE_LOCAL_BUFFER  (ACPI_SIZE) (-2)

typedef struct
{
    ACPI_SIZE                   Length;         /* Length in bytes of the buffer */
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

#define ACPI_SYS_MODE_UNKNOWN           0x0000
#define ACPI_SYS_MODE_ACPI              0x0001
#define ACPI_SYS_MODE_LEGACY            0x0002
#define ACPI_SYS_MODES_MASK             0x0003


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
 * Various handlers and callback procedures
 */

typedef
UINT32 (*ACPI_EVENT_HANDLER) (
    void                        *Context);

typedef
void (*ACPI_GPE_HANDLER) (
    void                        *Context);

typedef
void (*ACPI_NOTIFY_HANDLER) (
    ACPI_HANDLE                 Device,
    UINT32                      Value,
    void                        *Context);

typedef
void (*ACPI_OBJECT_HANDLER) (
    ACPI_HANDLE                 Object,
    UINT32                      Function,
    void                        *Data);

typedef
ACPI_STATUS (*ACPI_INIT_HANDLER) (
    ACPI_HANDLE                 Object,
    UINT32                      Function);

#define ACPI_INIT_DEVICE_INI        1


/* Address Spaces (Operation Regions */

typedef
ACPI_STATUS (*ACPI_ADR_SPACE_HANDLER) (
    UINT32                      Function,
    ACPI_PHYSICAL_ADDRESS       Address,
    UINT32                      BitWidth,
    ACPI_INTEGER                *Value,
    void                        *HandlerContext,
    void                        *RegionContext);

#define ACPI_DEFAULT_HANDLER        NULL


typedef
ACPI_STATUS (*ACPI_ADR_SPACE_SETUP) (
    ACPI_HANDLE                 RegionHandle,
    UINT32                      Function,
    void                        *HandlerContext,
    void                        **RegionContext);

#define ACPI_REGION_ACTIVATE    0
#define ACPI_REGION_DEACTIVATE  1

typedef
ACPI_STATUS (*ACPI_WALK_CALLBACK) (
    ACPI_HANDLE                 ObjHandle,
    UINT32                      NestingLevel,
    void                        *Context,
    void                        **ReturnValue);


/* Interrupt handler return values */

#define ACPI_INTERRUPT_NOT_HANDLED      0x00
#define ACPI_INTERRUPT_HANDLED          0x01


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
    NATIVE_CHAR                 HardwareId[9];      /*  _HID value if any */
    NATIVE_CHAR                 UniqueId[9];        /*  _UID value if any */
    ACPI_INTEGER                Address;            /*  _ADR value if any */
    UINT32                      CurrentStatus;      /*  _STA value */
} ACPI_DEVICE_INFO;


/* Context structs for address space handlers */

typedef struct
{
    UINT16                      Segment;
    UINT16                      Bus;
    UINT16                      Device;
    UINT16                      Function;
} ACPI_PCI_ID;


typedef struct
{
    UINT32                      Length;
    ACPI_PHYSICAL_ADDRESS       Address;
    ACPI_PHYSICAL_ADDRESS       MappedPhysicalAddress;
    UINT8                       *MappedLogicalAddress;
    ACPI_SIZE                   MappedLength;
} ACPI_MEM_SPACE_CONTEXT;


/* Sleep states */

#define ACPI_NUM_SLEEP_STATES           7


/*
 * Definitions for Resource Attributes
 */

/*
 *  Memory Attributes
 */
#define ACPI_READ_ONLY_MEMORY           (UINT8) 0x00
#define ACPI_READ_WRITE_MEMORY          (UINT8) 0x01

#define ACPI_NON_CACHEABLE_MEMORY       (UINT8) 0x00
#define ACPI_CACHABLE_MEMORY            (UINT8) 0x01
#define ACPI_WRITE_COMBINING_MEMORY     (UINT8) 0x02
#define ACPI_PREFETCHABLE_MEMORY        (UINT8) 0x03

/*
 *  IO Attributes
 *  The ISA IO ranges are:     n000-n0FFh,  n400-n4FFh, n800-n8FFh, nC00-nCFFh.
 *  The non-ISA IO ranges are: n100-n3FFh,  n500-n7FFh, n900-nBFFh, nCD0-nFFFh.
 */
#define ACPI_NON_ISA_ONLY_RANGES        (UINT8) 0x01
#define ACPI_ISA_ONLY_RANGES            (UINT8) 0x02
#define ACPI_ENTIRE_RANGE               (ACPI_NON_ISA_ONLY_RANGES | ACPI_ISA_ONLY_RANGES)

/*
 *  IO Port Descriptor Decode
 */
#define ACPI_DECODE_10                  (UINT8) 0x00    /* 10-bit IO address decode */
#define ACPI_DECODE_16                  (UINT8) 0x01    /* 16-bit IO address decode */

/*
 *  IRQ Attributes
 */
#define ACPI_EDGE_SENSITIVE             (UINT8) 0x00
#define ACPI_LEVEL_SENSITIVE            (UINT8) 0x01

#define ACPI_ACTIVE_HIGH                (UINT8) 0x00
#define ACPI_ACTIVE_LOW                 (UINT8) 0x01

#define ACPI_EXCLUSIVE                  (UINT8) 0x00
#define ACPI_SHARED                     (UINT8) 0x01

/*
 *  DMA Attributes
 */
#define ACPI_COMPATIBILITY              (UINT8) 0x00
#define ACPI_TYPE_A                     (UINT8) 0x01
#define ACPI_TYPE_B                     (UINT8) 0x02
#define ACPI_TYPE_F                     (UINT8) 0x03

#define ACPI_NOT_BUS_MASTER             (UINT8) 0x00
#define ACPI_BUS_MASTER                 (UINT8) 0x01

#define ACPI_TRANSFER_8                 (UINT8) 0x00
#define ACPI_TRANSFER_8_16              (UINT8) 0x01
#define ACPI_TRANSFER_16                (UINT8) 0x02

/*
 * Start Dependent Functions Priority definitions
 */
#define ACPI_GOOD_CONFIGURATION         (UINT8) 0x00
#define ACPI_ACCEPTABLE_CONFIGURATION   (UINT8) 0x01
#define ACPI_SUB_OPTIMAL_CONFIGURATION  (UINT8) 0x02

/*
 *  16, 32 and 64-bit Address Descriptor resource types
 */
#define ACPI_MEMORY_RANGE               (UINT8) 0x00
#define ACPI_IO_RANGE                   (UINT8) 0x01
#define ACPI_BUS_NUMBER_RANGE           (UINT8) 0x02

#define ACPI_ADDRESS_NOT_FIXED          (UINT8) 0x00
#define ACPI_ADDRESS_FIXED              (UINT8) 0x01

#define ACPI_POS_DECODE                 (UINT8) 0x00
#define ACPI_SUB_DECODE                 (UINT8) 0x01

#define ACPI_PRODUCER                   (UINT8) 0x00
#define ACPI_CONSUMER                   (UINT8) 0x01


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

} ACPI_RESOURCE_IRQ;

typedef struct
{
    UINT32                      Type;
    UINT32                      BusMaster;
    UINT32                      Transfer;
    UINT32                      NumberOfChannels;
    UINT32                      Channels[1];

} ACPI_RESOURCE_DMA;

typedef struct
{
    UINT32                      CompatibilityPriority;
    UINT32                      PerformanceRobustness;

} ACPI_RESOURCE_START_DPF;

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

} ACPI_RESOURCE_IO;

typedef struct
{
    UINT32                      BaseAddress;
    UINT32                      RangeLength;

} ACPI_RESOURCE_FIXED_IO;

typedef struct
{
    UINT32                      Length;
    UINT8                       Reserved[1];

} ACPI_RESOURCE_VENDOR;

typedef struct
{
    UINT8                       Checksum;

} ACPI_RESOURCE_END_TAG;

typedef struct
{
    UINT32                      ReadWriteAttribute;
    UINT32                      MinBaseAddress;
    UINT32                      MaxBaseAddress;
    UINT32                      Alignment;
    UINT32                      RangeLength;

} ACPI_RESOURCE_MEM24;

typedef struct
{
    UINT32                      ReadWriteAttribute;
    UINT32                      MinBaseAddress;
    UINT32                      MaxBaseAddress;
    UINT32                      Alignment;
    UINT32                      RangeLength;

} ACPI_RESOURCE_MEM32;

typedef struct
{
    UINT32                      ReadWriteAttribute;
    UINT32                      RangeBaseAddress;
    UINT32                      RangeLength;

} ACPI_RESOURCE_FIXED_MEM32;

typedef struct
{
    UINT16                      CacheAttribute;
    UINT16                      ReadWriteAttribute;

} ACPI_MEMORY_ATTRIBUTE;

typedef struct
{
    UINT16                      RangeAttribute;
    UINT16                      Reserved;

} ACPI_IO_ATTRIBUTE;

typedef struct
{
    UINT16                      Reserved1;
    UINT16                      Reserved2;

} ACPI_BUS_ATTRIBUTE;

typedef union
{
    ACPI_MEMORY_ATTRIBUTE       Memory;
    ACPI_IO_ATTRIBUTE           Io;
    ACPI_BUS_ATTRIBUTE          Bus;

} ACPI_RESOURCE_ATTRIBUTE;

typedef struct
{
    UINT32                      Index;
    UINT32                      StringLength;
    NATIVE_CHAR                 *StringPtr;

} ACPI_RESOURCE_SOURCE;

typedef struct
{
    UINT32                      ResourceType;
    UINT32                      ProducerConsumer;
    UINT32                      Decode;
    UINT32                      MinAddressFixed;
    UINT32                      MaxAddressFixed;
    ACPI_RESOURCE_ATTRIBUTE     Attribute;
    UINT32                      Granularity;
    UINT32                      MinAddressRange;
    UINT32                      MaxAddressRange;
    UINT32                      AddressTranslationOffset;
    UINT32                      AddressLength;
    ACPI_RESOURCE_SOURCE        ResourceSource;

} ACPI_RESOURCE_ADDRESS16;

typedef struct
{
    UINT32                      ResourceType;
    UINT32                      ProducerConsumer;
    UINT32                      Decode;
    UINT32                      MinAddressFixed;
    UINT32                      MaxAddressFixed;
    ACPI_RESOURCE_ATTRIBUTE     Attribute;
    UINT32                      Granularity;
    UINT32                      MinAddressRange;
    UINT32                      MaxAddressRange;
    UINT32                      AddressTranslationOffset;
    UINT32                      AddressLength;
    ACPI_RESOURCE_SOURCE        ResourceSource;

} ACPI_RESOURCE_ADDRESS32;

typedef struct
{
    UINT32                      ResourceType;
    UINT32                      ProducerConsumer;
    UINT32                      Decode;
    UINT32                      MinAddressFixed;
    UINT32                      MaxAddressFixed;
    ACPI_RESOURCE_ATTRIBUTE     Attribute;
    UINT64                      Granularity;
    UINT64                      MinAddressRange;
    UINT64                      MaxAddressRange;
    UINT64                      AddressTranslationOffset;
    UINT64                      AddressLength;
    ACPI_RESOURCE_SOURCE        ResourceSource;

} ACPI_RESOURCE_ADDRESS64;

typedef struct
{
    UINT32                      ProducerConsumer;
    UINT32                      EdgeLevel;
    UINT32                      ActiveHighLow;
    UINT32                      SharedExclusive;
    UINT32                      NumberOfInterrupts;
    ACPI_RESOURCE_SOURCE        ResourceSource;
    UINT32                      Interrupts[1];

} ACPI_RESOURCE_EXT_IRQ;


/* ACPI_RESOURCE_TYPEs */

#define ACPI_RSTYPE_IRQ                 0
#define ACPI_RSTYPE_DMA                 1
#define ACPI_RSTYPE_START_DPF           2
#define ACPI_RSTYPE_END_DPF             3
#define ACPI_RSTYPE_IO                  4
#define ACPI_RSTYPE_FIXED_IO            5
#define ACPI_RSTYPE_VENDOR              6
#define ACPI_RSTYPE_END_TAG             7
#define ACPI_RSTYPE_MEM24               8
#define ACPI_RSTYPE_MEM32               9
#define ACPI_RSTYPE_FIXED_MEM32         10
#define ACPI_RSTYPE_ADDRESS16           11
#define ACPI_RSTYPE_ADDRESS32           12
#define ACPI_RSTYPE_ADDRESS64           13
#define ACPI_RSTYPE_EXT_IRQ             14

typedef UINT32                          ACPI_RESOURCE_TYPE;

typedef union
{
    ACPI_RESOURCE_IRQ           Irq;
    ACPI_RESOURCE_DMA           Dma;
    ACPI_RESOURCE_START_DPF     StartDpf;
    ACPI_RESOURCE_IO            Io;
    ACPI_RESOURCE_FIXED_IO      FixedIo;
    ACPI_RESOURCE_VENDOR        VendorSpecific;
    ACPI_RESOURCE_END_TAG       EndTag;
    ACPI_RESOURCE_MEM24         Memory24;
    ACPI_RESOURCE_MEM32         Memory32;
    ACPI_RESOURCE_FIXED_MEM32   FixedMemory32;
    ACPI_RESOURCE_ADDRESS16     Address16;
    ACPI_RESOURCE_ADDRESS32     Address32;
    ACPI_RESOURCE_ADDRESS64     Address64;
    ACPI_RESOURCE_EXT_IRQ       ExtendedIrq;

} ACPI_RESOURCE_DATA;

typedef struct AcpiResource
{
    ACPI_RESOURCE_TYPE          Id;
    UINT32                      Length;
    ACPI_RESOURCE_DATA          Data;

} ACPI_RESOURCE;

#define ACPI_RESOURCE_LENGTH                12
#define ACPI_RESOURCE_LENGTH_NO_DATA        8       /* Id + Length fields */

#define ACPI_SIZEOF_RESOURCE(Type)          (ACPI_RESOURCE_LENGTH_NO_DATA + sizeof (Type))

#define ACPI_NEXT_RESOURCE(Res)             (ACPI_RESOURCE *)((UINT8 *) Res + Res->length)

#ifdef _HW_ALIGNMENT_SUPPORT
#define ACPI_ALIGN_RESOURCE_SIZE(Length)    (Length)
#else
#define ACPI_ALIGN_RESOURCE_SIZE(Length)    ACPI_ROUND_UP_TO_NATIVE_WORD(Length)
#endif

/*
 * END: of definitions for Resource Attributes
 */


typedef struct acpi_pci_routing_table
{
    UINT32                      Length;
    UINT32                      Pin;
    ACPI_INTEGER                Address;        /* here for 64-bit alignment */
    UINT32                      SourceIndex;
    NATIVE_CHAR                 Source[4];      /* pad to 64 bits so sizeof() works in all cases */

} ACPI_PCI_ROUTING_TABLE;

/*
 * END: of definitions for PCI Routing tables
 */


#endif /* __ACTYPES_H__ */
