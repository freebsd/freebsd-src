
/******************************************************************************
 *
 * Name: acobject.h - Definition of ACPI_OPERAND_OBJECT  (Internal object only)
 *       $Revision: 106 $
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

#ifndef _ACOBJECT_H
#define _ACOBJECT_H


/*
 * The ACPI_OPERAND_OBJECT  is used to pass AML operands from the dispatcher
 * to the interpreter, and to keep track of the various handlers such as
 * address space handlers and notify handlers.  The object is a constant
 * size in order to allow them to be cached and reused.
 *
 * All variants of the ACPI_OPERAND_OBJECT  are defined with the same
 * sequence of field types, with fields that are not used in a particular
 * variant being named "Reserved".  This is not strictly necessary, but
 * may in some circumstances simplify understanding if these structures
 * need to be displayed in a debugger having limited (or no) support for
 * union types.  It also simplifies some debug code in DumpTable() which
 * dumps multi-level values: fetching Buffer.Pointer suffices to pick up
 * the value or next level for any of several types.
 */

/******************************************************************************
 *
 * Common Descriptors
 *
 *****************************************************************************/

/*
 * Common area for all objects.
 *
 * DataType is used to differentiate between internal descriptors, and MUST
 * be the first byte in this structure.
 */


#define ACPI_OBJECT_COMMON_HEADER           /* SIZE/ALIGNMENT: 32 bits, one ptr plus trailing 8-bit flag */\
    UINT8                       Descriptor;         /* To differentiate various internal objs */\
    UINT8                       Type;               /* ACPI_OBJECT_TYPE */\
    UINT16                      ReferenceCount;     /* For object deletion management */\
    union acpi_operand_obj      *NextObject;        /* Objects linked to parent NS node */\
    UINT8                       Flags; \

/* Defines for flag byte above */

#define AOPOBJ_RESERVED             0x01
#define AOPOBJ_STATIC_POINTER       0x02
#define AOPOBJ_DATA_VALID           0x04
#define AOPOBJ_OBJECT_INITIALIZED   0x08
#define AOPOBJ_SETUP_COMPLETE       0x10
#define AOPOBJ_SINGLE_DATUM         0x20


/*
 * Common bitfield for the field objects
 * "Field Datum"  -- a datum from the actual field object
 * "Buffer Datum" -- a datum from a user buffer, read from or to be written to the field
 */
#define ACPI_COMMON_FIELD_INFO              /* SIZE/ALIGNMENT: 24 bits + three 32-bit values */\
    UINT8                       FieldFlags;         /* Access, update, and lock bits */\
    UINT8                       Attribute;          /* From AccessAs keyword */\
    UINT8                       AccessByteWidth;    /* Read/Write size in bytes */\
    UINT32                      BitLength;          /* Length of field in bits */\
    UINT32                      BaseByteOffset;     /* Byte offset within containing object */\
    UINT8                       StartFieldBitOffset;/* Bit offset within first field datum (0-63) */\
    UINT8                       DatumValidBits;     /* Valid bit in first "Field datum" */\
    UINT8                       EndFieldValidBits;  /* Valid bits in the last "field datum" */\
    UINT8                       EndBufferValidBits; /* Valid bits in the last "buffer datum" */\
    UINT32                      Value;              /* Value to store into the Bank or Index register */\
    ACPI_NAMESPACE_NODE         *Node;              /* Link back to parent node */


/*
 * Fields common to both Strings and Buffers
 */
#define ACPI_COMMON_BUFFER_INFO \
    UINT32                      Length;


/******************************************************************************
 *
 * Individual Object Descriptors
 *
 *****************************************************************************/


typedef struct /* COMMON */
{
    ACPI_OBJECT_COMMON_HEADER

} ACPI_OBJECT_COMMON;


typedef struct /* CACHE_LIST */
{
    ACPI_OBJECT_COMMON_HEADER
    union acpi_operand_obj      *Next;              /* Link for object cache and internal lists*/

} ACPI_OBJECT_CACHE_LIST;


typedef struct /* NUMBER - has value */
{
    ACPI_OBJECT_COMMON_HEADER

    ACPI_INTEGER                Value;

} ACPI_OBJECT_INTEGER;


typedef struct /* STRING - has length and pointer - Null terminated, ASCII characters only */
{
    ACPI_OBJECT_COMMON_HEADER
    ACPI_COMMON_BUFFER_INFO
    NATIVE_CHAR                 *Pointer;           /* String value in AML stream or in allocated space */

} ACPI_OBJECT_STRING;


typedef struct /* BUFFER - has length and pointer - not null terminated */
{
    ACPI_OBJECT_COMMON_HEADER
    ACPI_COMMON_BUFFER_INFO
    UINT8                       *Pointer;           /* Buffer value in AML stream or in allocated space */
    ACPI_NAMESPACE_NODE         *Node;              /* Link back to parent node */

} ACPI_OBJECT_BUFFER;


typedef struct /* PACKAGE - has count, elements, next element */
{
    ACPI_OBJECT_COMMON_HEADER

    UINT32                      Count;              /* # of elements in package */
    union acpi_operand_obj      **Elements;         /* Array of pointers to AcpiObjects */
    union acpi_operand_obj      **NextElement;      /* used only while initializing */

} ACPI_OBJECT_PACKAGE;


typedef struct /* DEVICE - has handle and notification handler/context */
{
    ACPI_OBJECT_COMMON_HEADER

    union acpi_operand_obj      *SysHandler;         /* Handler for system notifies */
    union acpi_operand_obj      *DrvHandler;         /* Handler for driver notifies */
    union acpi_operand_obj      *AddrHandler;        /* Handler for Address space */

} ACPI_OBJECT_DEVICE;


typedef struct /* EVENT */
{
    ACPI_OBJECT_COMMON_HEADER
    void                        *Semaphore;

} ACPI_OBJECT_EVENT;


#define INFINITE_CONCURRENCY        0xFF

typedef struct /* METHOD */
{
    ACPI_OBJECT_COMMON_HEADER
    UINT8                       MethodFlags;
    UINT8                       ParamCount;

    UINT32                      AmlLength;

    void                        *Semaphore;
    UINT8                       *AmlStart;

    UINT8                       Concurrency;
    UINT8                       ThreadCount;
    ACPI_OWNER_ID               OwningId;

} ACPI_OBJECT_METHOD;


typedef struct acpi_obj_mutex /* MUTEX */
{
    ACPI_OBJECT_COMMON_HEADER
    UINT16                      SyncLevel;
    UINT16                      AcquisitionDepth;

    struct acpi_thread_state    *OwnerThread;
    void                        *Semaphore;
    union acpi_operand_obj      *Prev;              /* Link for list of acquired mutexes */
    union acpi_operand_obj      *Next;              /* Link for list of acquired mutexes */

} ACPI_OBJECT_MUTEX;


typedef struct /* REGION */
{
    ACPI_OBJECT_COMMON_HEADER

    UINT8                       SpaceId;
    UINT32                      Length;
    ACPI_PHYSICAL_ADDRESS       Address;

    union acpi_operand_obj      *AddrHandler;       /* Handler for system notifies */
    ACPI_NAMESPACE_NODE         *Node;              /* containing object */
    union acpi_operand_obj      *Next;

} ACPI_OBJECT_REGION;


typedef struct /* POWER RESOURCE - has Handle and notification handler/context*/
{
    ACPI_OBJECT_COMMON_HEADER

    UINT32                      SystemLevel;
    UINT32                      ResourceOrder;

    union acpi_operand_obj      *SysHandler;        /* Handler for system notifies */
    union acpi_operand_obj      *DrvHandler;        /* Handler for driver notifies */

} ACPI_OBJECT_POWER_RESOURCE;


typedef struct /* PROCESSOR - has Handle and notification handler/context*/
{
    ACPI_OBJECT_COMMON_HEADER

    UINT32                      ProcId;
    UINT32                      Length;
    ACPI_IO_ADDRESS             Address;

    union acpi_operand_obj      *SysHandler;        /* Handler for system notifies */
    union acpi_operand_obj      *DrvHandler;        /* Handler for driver notifies */
    union acpi_operand_obj      *AddrHandler;       /* Handler for Address space */

} ACPI_OBJECT_PROCESSOR;


typedef struct /* THERMAL ZONE - has Handle and Handler/Context */
{
    ACPI_OBJECT_COMMON_HEADER

    union acpi_operand_obj      *SysHandler;        /* Handler for system notifies */
    union acpi_operand_obj      *DrvHandler;        /* Handler for driver notifies */
    union acpi_operand_obj      *AddrHandler;       /* Handler for Address space */

} ACPI_OBJECT_THERMAL_ZONE;


/*
 * Fields.  All share a common header/info field.
 */

typedef struct /* COMMON FIELD (for BUFFER, REGION, BANK, and INDEX fields) */
{
    ACPI_OBJECT_COMMON_HEADER
    ACPI_COMMON_FIELD_INFO
    union acpi_operand_obj      *RegionObj;         /* Containing Operation Region object */
                                                    /* (REGION/BANK fields only) */
} ACPI_OBJECT_FIELD_COMMON;


typedef struct /* REGION FIELD */
{
    ACPI_OBJECT_COMMON_HEADER
    ACPI_COMMON_FIELD_INFO
    union acpi_operand_obj      *RegionObj;         /* Containing OpRegion object */

} ACPI_OBJECT_REGION_FIELD;


typedef struct /* BANK FIELD */
{
    ACPI_OBJECT_COMMON_HEADER
    ACPI_COMMON_FIELD_INFO

    union acpi_operand_obj      *RegionObj;         /* Containing OpRegion object */
    union acpi_operand_obj      *BankObj;           /* BankSelect Register object */

} ACPI_OBJECT_BANK_FIELD;


typedef struct /* INDEX FIELD */
{
    ACPI_OBJECT_COMMON_HEADER
    ACPI_COMMON_FIELD_INFO

    /*
     * No "RegionObj" pointer needed since the Index and Data registers
     * are each field definitions unto themselves.
     */
    union acpi_operand_obj      *IndexObj;          /* Index register */
    union acpi_operand_obj      *DataObj;           /* Data register */


} ACPI_OBJECT_INDEX_FIELD;


/* The BufferField is different in that it is part of a Buffer, not an OpRegion */

typedef struct /* BUFFER FIELD */
{
    ACPI_OBJECT_COMMON_HEADER
    ACPI_COMMON_FIELD_INFO

    union acpi_operand_obj      *BufferObj;         /* Containing Buffer object */

} ACPI_OBJECT_BUFFER_FIELD;


/*
 * Handlers
 */

typedef struct /* NOTIFY HANDLER */
{
    ACPI_OBJECT_COMMON_HEADER

    ACPI_NAMESPACE_NODE         *Node;               /* Parent device */
    ACPI_NOTIFY_HANDLER         Handler;
    void                        *Context;

} ACPI_OBJECT_NOTIFY_HANDLER;


/* Flags for address handler */

#define ACPI_ADDR_HANDLER_DEFAULT_INSTALLED  0x1


typedef struct /* ADDRESS HANDLER */
{
    ACPI_OBJECT_COMMON_HEADER

    UINT8                       SpaceId;
    UINT16                      Hflags;
    ACPI_ADR_SPACE_HANDLER      Handler;

    ACPI_NAMESPACE_NODE         *Node;              /* Parent device */
    void                        *Context;
    ACPI_ADR_SPACE_SETUP        Setup;
    union acpi_operand_obj      *RegionList;        /* regions using this handler */
    union acpi_operand_obj      *Next;

} ACPI_OBJECT_ADDR_HANDLER;


/*
 * The Reference object type is used for these opcodes:
 * Arg[0-6], Local[0-7], IndexOp, NameOp, ZeroOp, OneOp, OnesOp, DebugOp
 */

typedef struct /* Reference - Local object type */
{
    ACPI_OBJECT_COMMON_HEADER

    UINT8                       TargetType;         /* Used for IndexOp */
    UINT16                      Opcode;
    UINT32                      Offset;             /* Used for ArgOp, LocalOp, and IndexOp */

    void                        *Object;            /* NameOp=>HANDLE to obj, IndexOp=>ACPI_OPERAND_OBJECT  */
    ACPI_NAMESPACE_NODE         *Node;
    union acpi_operand_obj      **Where;

} ACPI_OBJECT_REFERENCE;


/*
 * Extra object is used as additional storage for types that
 * have AML code in their declarations (TermArgs) that must be
 * evaluated at run time.
 *
 * Currently: Region and FieldUnit types
 */

typedef struct /* EXTRA */
{
    ACPI_OBJECT_COMMON_HEADER
    UINT8                       ByteFill1;
    UINT16                      WordFill1;
    UINT32                      AmlLength;
    UINT8                       *AmlStart;
    ACPI_NAMESPACE_NODE         *Method_REG;        /* _REG method for this region (if any) */
    void                        *RegionContext;     /* Region-specific data */

} ACPI_OBJECT_EXTRA;


typedef struct /* DATA */
{
    ACPI_OBJECT_COMMON_HEADER
    ACPI_OBJECT_HANDLER         Handler;
    void                        *Pointer;

} ACPI_OBJECT_DATA;


/******************************************************************************
 *
 * ACPI_OPERAND_OBJECT  Descriptor - a giant union of all of the above
 *
 *****************************************************************************/

typedef union acpi_operand_obj
{
    ACPI_OBJECT_COMMON          Common;
    ACPI_OBJECT_CACHE_LIST      Cache;
    ACPI_OBJECT_INTEGER         Integer;
    ACPI_OBJECT_STRING          String;
    ACPI_OBJECT_BUFFER          Buffer;
    ACPI_OBJECT_PACKAGE         Package;
    ACPI_OBJECT_BUFFER_FIELD    BufferField;
    ACPI_OBJECT_DEVICE          Device;
    ACPI_OBJECT_EVENT           Event;
    ACPI_OBJECT_METHOD          Method;
    ACPI_OBJECT_MUTEX           Mutex;
    ACPI_OBJECT_REGION          Region;
    ACPI_OBJECT_POWER_RESOURCE  PowerResource;
    ACPI_OBJECT_PROCESSOR       Processor;
    ACPI_OBJECT_THERMAL_ZONE    ThermalZone;
    ACPI_OBJECT_FIELD_COMMON    CommonField;
    ACPI_OBJECT_REGION_FIELD    Field;
    ACPI_OBJECT_BANK_FIELD      BankField;
    ACPI_OBJECT_INDEX_FIELD     IndexField;
    ACPI_OBJECT_REFERENCE       Reference;
    ACPI_OBJECT_NOTIFY_HANDLER  NotifyHandler;
    ACPI_OBJECT_ADDR_HANDLER    AddrHandler;
    ACPI_OBJECT_EXTRA           Extra;
    ACPI_OBJECT_DATA            Data;

} ACPI_OPERAND_OBJECT;

#endif /* _ACOBJECT_H */
