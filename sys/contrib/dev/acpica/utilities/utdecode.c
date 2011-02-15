/******************************************************************************
 *
 * Module Name: utdecode - Utility decoding routines (value-to-string)
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

#define __UTDECODE_C__

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acnamesp.h>

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utdecode")


/*******************************************************************************
 *
 * FUNCTION:    AcpiFormatException
 *
 * PARAMETERS:  Status       - The ACPI_STATUS code to be formatted
 *
 * RETURN:      A string containing the exception text. A valid pointer is
 *              always returned.
 *
 * DESCRIPTION: This function translates an ACPI exception into an ASCII string
 *              It is here instead of utxface.c so it is always present.
 *
 ******************************************************************************/

const char *
AcpiFormatException (
    ACPI_STATUS             Status)
{
    const char              *Exception = NULL;


    ACPI_FUNCTION_ENTRY ();


    Exception = AcpiUtValidateException (Status);
    if (!Exception)
    {
        /* Exception code was not recognized */

        ACPI_ERROR ((AE_INFO,
            "Unknown exception code: 0x%8.8X", Status));

        Exception = "UNKNOWN_STATUS_CODE";
    }

    return (ACPI_CAST_PTR (const char, Exception));
}

ACPI_EXPORT_SYMBOL (AcpiFormatException)


/*
 * Properties of the ACPI Object Types, both internal and external.
 * The table is indexed by values of ACPI_OBJECT_TYPE
 */
const UINT8                     AcpiGbl_NsProperties[ACPI_NUM_NS_TYPES] =
{
    ACPI_NS_NORMAL,                     /* 00 Any              */
    ACPI_NS_NORMAL,                     /* 01 Number           */
    ACPI_NS_NORMAL,                     /* 02 String           */
    ACPI_NS_NORMAL,                     /* 03 Buffer           */
    ACPI_NS_NORMAL,                     /* 04 Package          */
    ACPI_NS_NORMAL,                     /* 05 FieldUnit        */
    ACPI_NS_NEWSCOPE,                   /* 06 Device           */
    ACPI_NS_NORMAL,                     /* 07 Event            */
    ACPI_NS_NEWSCOPE,                   /* 08 Method           */
    ACPI_NS_NORMAL,                     /* 09 Mutex            */
    ACPI_NS_NORMAL,                     /* 10 Region           */
    ACPI_NS_NEWSCOPE,                   /* 11 Power            */
    ACPI_NS_NEWSCOPE,                   /* 12 Processor        */
    ACPI_NS_NEWSCOPE,                   /* 13 Thermal          */
    ACPI_NS_NORMAL,                     /* 14 BufferField      */
    ACPI_NS_NORMAL,                     /* 15 DdbHandle        */
    ACPI_NS_NORMAL,                     /* 16 Debug Object     */
    ACPI_NS_NORMAL,                     /* 17 DefField         */
    ACPI_NS_NORMAL,                     /* 18 BankField        */
    ACPI_NS_NORMAL,                     /* 19 IndexField       */
    ACPI_NS_NORMAL,                     /* 20 Reference        */
    ACPI_NS_NORMAL,                     /* 21 Alias            */
    ACPI_NS_NORMAL,                     /* 22 MethodAlias      */
    ACPI_NS_NORMAL,                     /* 23 Notify           */
    ACPI_NS_NORMAL,                     /* 24 Address Handler  */
    ACPI_NS_NEWSCOPE | ACPI_NS_LOCAL,   /* 25 Resource Desc    */
    ACPI_NS_NEWSCOPE | ACPI_NS_LOCAL,   /* 26 Resource Field   */
    ACPI_NS_NEWSCOPE,                   /* 27 Scope            */
    ACPI_NS_NORMAL,                     /* 28 Extra            */
    ACPI_NS_NORMAL,                     /* 29 Data             */
    ACPI_NS_NORMAL                      /* 30 Invalid          */
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtHexToAsciiChar
 *
 * PARAMETERS:  Integer             - Contains the hex digit
 *              Position            - bit position of the digit within the
 *                                    integer (multiple of 4)
 *
 * RETURN:      The converted Ascii character
 *
 * DESCRIPTION: Convert a hex digit to an Ascii character
 *
 ******************************************************************************/

/* Hex to ASCII conversion table */

static const char           AcpiGbl_HexToAscii[] =
{
    '0','1','2','3','4','5','6','7',
    '8','9','A','B','C','D','E','F'
};

char
AcpiUtHexToAsciiChar (
    UINT64                  Integer,
    UINT32                  Position)
{

    return (AcpiGbl_HexToAscii[(Integer >> Position) & 0xF]);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetRegionName
 *
 * PARAMETERS:  Space ID            - ID for the region
 *
 * RETURN:      Decoded region SpaceId name
 *
 * DESCRIPTION: Translate a Space ID into a name string (Debug only)
 *
 ******************************************************************************/

/* Region type decoding */

const char        *AcpiGbl_RegionTypes[ACPI_NUM_PREDEFINED_REGIONS] =
{
    "SystemMemory",
    "SystemIO",
    "PCI_Config",
    "EmbeddedControl",
    "SMBus",
    "SystemCMOS",
    "PCIBARTarget",
    "IPMI",
    "DataTable"
};


char *
AcpiUtGetRegionName (
    UINT8                   SpaceId)
{

    if (SpaceId >= ACPI_USER_REGION_BEGIN)
    {
        return ("UserDefinedRegion");
    }
    else if (SpaceId == ACPI_ADR_SPACE_FIXED_HARDWARE)
    {
        return ("FunctionalFixedHW");
    }
    else if (SpaceId >= ACPI_NUM_PREDEFINED_REGIONS)
    {
        return ("InvalidSpaceId");
    }

    return (ACPI_CAST_PTR (char, AcpiGbl_RegionTypes[SpaceId]));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetEventName
 *
 * PARAMETERS:  EventId             - Fixed event ID
 *
 * RETURN:      Decoded event ID name
 *
 * DESCRIPTION: Translate a Event ID into a name string (Debug only)
 *
 ******************************************************************************/

/* Event type decoding */

static const char        *AcpiGbl_EventTypes[ACPI_NUM_FIXED_EVENTS] =
{
    "PM_Timer",
    "GlobalLock",
    "PowerButton",
    "SleepButton",
    "RealTimeClock",
};


char *
AcpiUtGetEventName (
    UINT32                  EventId)
{

    if (EventId > ACPI_EVENT_MAX)
    {
        return ("InvalidEventID");
    }

    return (ACPI_CAST_PTR (char, AcpiGbl_EventTypes[EventId]));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetTypeName
 *
 * PARAMETERS:  Type                - An ACPI object type
 *
 * RETURN:      Decoded ACPI object type name
 *
 * DESCRIPTION: Translate a Type ID into a name string (Debug only)
 *
 ******************************************************************************/

/*
 * Elements of AcpiGbl_NsTypeNames below must match
 * one-to-one with values of ACPI_OBJECT_TYPE
 *
 * The type ACPI_TYPE_ANY (Untyped) is used as a "don't care" when searching;
 * when stored in a table it really means that we have thus far seen no
 * evidence to indicate what type is actually going to be stored for this entry.
 */
static const char           AcpiGbl_BadType[] = "UNDEFINED";

/* Printable names of the ACPI object types */

static const char           *AcpiGbl_NsTypeNames[] =
{
    /* 00 */ "Untyped",
    /* 01 */ "Integer",
    /* 02 */ "String",
    /* 03 */ "Buffer",
    /* 04 */ "Package",
    /* 05 */ "FieldUnit",
    /* 06 */ "Device",
    /* 07 */ "Event",
    /* 08 */ "Method",
    /* 09 */ "Mutex",
    /* 10 */ "Region",
    /* 11 */ "Power",
    /* 12 */ "Processor",
    /* 13 */ "Thermal",
    /* 14 */ "BufferField",
    /* 15 */ "DdbHandle",
    /* 16 */ "DebugObject",
    /* 17 */ "RegionField",
    /* 18 */ "BankField",
    /* 19 */ "IndexField",
    /* 20 */ "Reference",
    /* 21 */ "Alias",
    /* 22 */ "MethodAlias",
    /* 23 */ "Notify",
    /* 24 */ "AddrHandler",
    /* 25 */ "ResourceDesc",
    /* 26 */ "ResourceFld",
    /* 27 */ "Scope",
    /* 28 */ "Extra",
    /* 29 */ "Data",
    /* 30 */ "Invalid"
};


char *
AcpiUtGetTypeName (
    ACPI_OBJECT_TYPE        Type)
{

    if (Type > ACPI_TYPE_INVALID)
    {
        return (ACPI_CAST_PTR (char, AcpiGbl_BadType));
    }

    return (ACPI_CAST_PTR (char, AcpiGbl_NsTypeNames[Type]));
}


char *
AcpiUtGetObjectTypeName (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{

    if (!ObjDesc)
    {
        return ("[NULL Object Descriptor]");
    }

    return (AcpiUtGetTypeName (ObjDesc->Common.Type));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetNodeName
 *
 * PARAMETERS:  Object               - A namespace node
 *
 * RETURN:      ASCII name of the node
 *
 * DESCRIPTION: Validate the node and return the node's ACPI name.
 *
 ******************************************************************************/

char *
AcpiUtGetNodeName (
    void                    *Object)
{
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) Object;


    /* Must return a string of exactly 4 characters == ACPI_NAME_SIZE */

    if (!Object)
    {
        return ("NULL");
    }

    /* Check for Root node */

    if ((Object == ACPI_ROOT_OBJECT) ||
        (Object == AcpiGbl_RootNode))
    {
        return ("\"\\\" ");
    }

    /* Descriptor must be a namespace node */

    if (ACPI_GET_DESCRIPTOR_TYPE (Node) != ACPI_DESC_TYPE_NAMED)
    {
        return ("####");
    }

    /*
     * Ensure name is valid. The name was validated/repaired when the node
     * was created, but make sure it has not been corrupted.
     */
    AcpiUtRepairName (Node->Name.Ascii);

    /* Return the name */

    return (Node->Name.Ascii);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetDescriptorName
 *
 * PARAMETERS:  Object               - An ACPI object
 *
 * RETURN:      Decoded name of the descriptor type
 *
 * DESCRIPTION: Validate object and return the descriptor type
 *
 ******************************************************************************/

/* Printable names of object descriptor types */

static const char           *AcpiGbl_DescTypeNames[] =
{
    /* 00 */ "Not a Descriptor",
    /* 01 */ "Cached",
    /* 02 */ "State-Generic",
    /* 03 */ "State-Update",
    /* 04 */ "State-Package",
    /* 05 */ "State-Control",
    /* 06 */ "State-RootParseScope",
    /* 07 */ "State-ParseScope",
    /* 08 */ "State-WalkScope",
    /* 09 */ "State-Result",
    /* 10 */ "State-Notify",
    /* 11 */ "State-Thread",
    /* 12 */ "Walk",
    /* 13 */ "Parser",
    /* 14 */ "Operand",
    /* 15 */ "Node"
};


char *
AcpiUtGetDescriptorName (
    void                    *Object)
{

    if (!Object)
    {
        return ("NULL OBJECT");
    }

    if (ACPI_GET_DESCRIPTOR_TYPE (Object) > ACPI_DESC_TYPE_MAX)
    {
        return ("Not a Descriptor");
    }

    return (ACPI_CAST_PTR (char,
        AcpiGbl_DescTypeNames[ACPI_GET_DESCRIPTOR_TYPE (Object)]));

}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetReferenceName
 *
 * PARAMETERS:  Object               - An ACPI reference object
 *
 * RETURN:      Decoded name of the type of reference
 *
 * DESCRIPTION: Decode a reference object sub-type to a string.
 *
 ******************************************************************************/

/* Printable names of reference object sub-types */

static const char           *AcpiGbl_RefClassNames[] =
{
    /* 00 */ "Local",
    /* 01 */ "Argument",
    /* 02 */ "RefOf",
    /* 03 */ "Index",
    /* 04 */ "DdbHandle",
    /* 05 */ "Named Object",
    /* 06 */ "Debug"
};

const char *
AcpiUtGetReferenceName (
    ACPI_OPERAND_OBJECT     *Object)
{

    if (!Object)
    {
        return ("NULL Object");
    }

    if (ACPI_GET_DESCRIPTOR_TYPE (Object) != ACPI_DESC_TYPE_OPERAND)
    {
        return ("Not an Operand object");
    }

    if (Object->Common.Type != ACPI_TYPE_LOCAL_REFERENCE)
    {
        return ("Not a Reference object");
    }

    if (Object->Reference.Class > ACPI_REFCLASS_MAX)
    {
        return ("Unknown Reference class");
    }

    return (AcpiGbl_RefClassNames[Object->Reference.Class]);
}


#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)
/*
 * Strings and procedures used for debug only
 */

/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetMutexName
 *
 * PARAMETERS:  MutexId         - The predefined ID for this mutex.
 *
 * RETURN:      Decoded name of the internal mutex
 *
 * DESCRIPTION: Translate a mutex ID into a name string (Debug only)
 *
 ******************************************************************************/

/* Names for internal mutex objects, used for debug output */

static char                 *AcpiGbl_MutexNames[ACPI_NUM_MUTEX] =
{
    "ACPI_MTX_Interpreter",
    "ACPI_MTX_Namespace",
    "ACPI_MTX_Tables",
    "ACPI_MTX_Events",
    "ACPI_MTX_Caches",
    "ACPI_MTX_Memory",
    "ACPI_MTX_CommandComplete",
    "ACPI_MTX_CommandReady"
};

char *
AcpiUtGetMutexName (
    UINT32                  MutexId)
{

    if (MutexId > ACPI_MAX_MUTEX)
    {
        return ("Invalid Mutex ID");
    }

    return (AcpiGbl_MutexNames[MutexId]);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetNotifyName
 *
 * PARAMETERS:  NotifyValue     - Value from the Notify() request
 *
 * RETURN:      Decoded name for the notify value
 *
 * DESCRIPTION: Translate a Notify Value to a notify namestring.
 *
 ******************************************************************************/

/* Names for Notify() values, used for debug output */

static const char           *AcpiGbl_NotifyValueNames[] =
{
    "Bus Check",
    "Device Check",
    "Device Wake",
    "Eject Request",
    "Device Check Light",
    "Frequency Mismatch",
    "Bus Mode Mismatch",
    "Power Fault",
    "Capabilities Check",
    "Device PLD Check",
    "Reserved",
    "System Locality Update"
};

const char *
AcpiUtGetNotifyName (
    UINT32                  NotifyValue)
{

    if (NotifyValue <= ACPI_NOTIFY_MAX)
    {
        return (AcpiGbl_NotifyValueNames[NotifyValue]);
    }
    else if (NotifyValue <= ACPI_MAX_SYS_NOTIFY)
    {
        return ("Reserved");
    }
    else /* Greater or equal to 0x80 */
    {
        return ("**Device Specific**");
    }
}
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtValidObjectType
 *
 * PARAMETERS:  Type            - Object type to be validated
 *
 * RETURN:      TRUE if valid object type, FALSE otherwise
 *
 * DESCRIPTION: Validate an object type
 *
 ******************************************************************************/

BOOLEAN
AcpiUtValidObjectType (
    ACPI_OBJECT_TYPE        Type)
{

    if (Type > ACPI_TYPE_LOCAL_MAX)
    {
        /* Note: Assumes all TYPEs are contiguous (external/local) */

        return (FALSE);
    }

    return (TRUE);
}
