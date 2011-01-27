/******************************************************************************
 *
 * Module Name: utglobal - Global variables for the ACPI subsystem
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

#define __UTGLOBAL_C__
#define DEFINE_ACPI_GLOBALS

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acnamesp.h>

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utglobal")


/*******************************************************************************
 *
 * Static global variable initialization.
 *
 ******************************************************************************/

/*
 * We want the debug switches statically initialized so they
 * are already set when the debugger is entered.
 */

/* Debug switch - level and trace mask */

#ifdef ACPI_DEBUG_OUTPUT
UINT32                      AcpiDbgLevel = ACPI_DEBUG_DEFAULT;
#else
UINT32                      AcpiDbgLevel = ACPI_NORMAL_DEFAULT;
#endif

/* Debug switch - layer (component) mask */

UINT32                      AcpiDbgLayer = ACPI_COMPONENT_DEFAULT;
UINT32                      AcpiGbl_NestingLevel = 0;

/* Debugger globals */

BOOLEAN                     AcpiGbl_DbTerminateThreads = FALSE;
BOOLEAN                     AcpiGbl_AbortMethod = FALSE;
BOOLEAN                     AcpiGbl_MethodExecuting = FALSE;

/* System flags */

UINT32                      AcpiGbl_StartupFlags = 0;

/* System starts uninitialized */

BOOLEAN                     AcpiGbl_Shutdown = TRUE;

const char                  *AcpiGbl_SleepStateNames[ACPI_S_STATE_COUNT] =
{
    "\\_S0_",
    "\\_S1_",
    "\\_S2_",
    "\\_S3_",
    "\\_S4_",
    "\\_S5_"
};

const char                  *AcpiGbl_LowestDstateNames[ACPI_NUM_SxW_METHODS] =
{
    "_S0W",
    "_S1W",
    "_S2W",
    "_S3W",
    "_S4W"
};

const char                  *AcpiGbl_HighestDstateNames[ACPI_NUM_SxD_METHODS] =
{
    "_S1D",
    "_S2D",
    "_S3D",
    "_S4D"
};


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


/*******************************************************************************
 *
 * Namespace globals
 *
 ******************************************************************************/

/*
 * Predefined ACPI Names (Built-in to the Interpreter)
 *
 * NOTES:
 * 1) _SB_ is defined to be a device to allow \_SB_._INI to be run
 *    during the initialization sequence.
 * 2) _TZ_ is defined to be a thermal zone in order to allow ASL code to
 *    perform a Notify() operation on it. 09/2010: Changed to type Device.
 *    This still allows notifies, but does not confuse host code that
 *    searches for valid ThermalZone objects.
 */
const ACPI_PREDEFINED_NAMES     AcpiGbl_PreDefinedNames[] =
{
    {"_GPE",    ACPI_TYPE_LOCAL_SCOPE,      NULL},
    {"_PR_",    ACPI_TYPE_LOCAL_SCOPE,      NULL},
    {"_SB_",    ACPI_TYPE_DEVICE,           NULL},
    {"_SI_",    ACPI_TYPE_LOCAL_SCOPE,      NULL},
    {"_TZ_",    ACPI_TYPE_DEVICE,           NULL},
    {"_REV",    ACPI_TYPE_INTEGER,          (char *) ACPI_CA_SUPPORT_LEVEL},
    {"_OS_",    ACPI_TYPE_STRING,           ACPI_OS_NAME},
    {"_GL_",    ACPI_TYPE_MUTEX,            (char *) 1},

#if !defined (ACPI_NO_METHOD_EXECUTION) || defined (ACPI_CONSTANT_EVAL_ONLY)
    {"_OSI",    ACPI_TYPE_METHOD,           (char *) 1},
#endif

    /* Table terminator */

    {NULL,      ACPI_TYPE_ANY,              NULL}
};

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


/* Hex to ASCII conversion table */

static const char           AcpiGbl_HexToAscii[] =
{
    '0','1','2','3','4','5','6','7',
    '8','9','A','B','C','D','E','F'
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

char
AcpiUtHexToAsciiChar (
    UINT64                  Integer,
    UINT32                  Position)
{

    return (AcpiGbl_HexToAscii[(Integer >> Position) & 0xF]);
}


/******************************************************************************
 *
 * Event and Hardware globals
 *
 ******************************************************************************/

ACPI_BIT_REGISTER_INFO      AcpiGbl_BitRegisterInfo[ACPI_NUM_BITREG] =
{
    /* Name                                     Parent Register             Register Bit Position                   Register Bit Mask       */

    /* ACPI_BITREG_TIMER_STATUS         */   {ACPI_REGISTER_PM1_STATUS,   ACPI_BITPOSITION_TIMER_STATUS,          ACPI_BITMASK_TIMER_STATUS},
    /* ACPI_BITREG_BUS_MASTER_STATUS    */   {ACPI_REGISTER_PM1_STATUS,   ACPI_BITPOSITION_BUS_MASTER_STATUS,     ACPI_BITMASK_BUS_MASTER_STATUS},
    /* ACPI_BITREG_GLOBAL_LOCK_STATUS   */   {ACPI_REGISTER_PM1_STATUS,   ACPI_BITPOSITION_GLOBAL_LOCK_STATUS,    ACPI_BITMASK_GLOBAL_LOCK_STATUS},
    /* ACPI_BITREG_POWER_BUTTON_STATUS  */   {ACPI_REGISTER_PM1_STATUS,   ACPI_BITPOSITION_POWER_BUTTON_STATUS,   ACPI_BITMASK_POWER_BUTTON_STATUS},
    /* ACPI_BITREG_SLEEP_BUTTON_STATUS  */   {ACPI_REGISTER_PM1_STATUS,   ACPI_BITPOSITION_SLEEP_BUTTON_STATUS,   ACPI_BITMASK_SLEEP_BUTTON_STATUS},
    /* ACPI_BITREG_RT_CLOCK_STATUS      */   {ACPI_REGISTER_PM1_STATUS,   ACPI_BITPOSITION_RT_CLOCK_STATUS,       ACPI_BITMASK_RT_CLOCK_STATUS},
    /* ACPI_BITREG_WAKE_STATUS          */   {ACPI_REGISTER_PM1_STATUS,   ACPI_BITPOSITION_WAKE_STATUS,           ACPI_BITMASK_WAKE_STATUS},
    /* ACPI_BITREG_PCIEXP_WAKE_STATUS   */   {ACPI_REGISTER_PM1_STATUS,   ACPI_BITPOSITION_PCIEXP_WAKE_STATUS,    ACPI_BITMASK_PCIEXP_WAKE_STATUS},

    /* ACPI_BITREG_TIMER_ENABLE         */   {ACPI_REGISTER_PM1_ENABLE,   ACPI_BITPOSITION_TIMER_ENABLE,          ACPI_BITMASK_TIMER_ENABLE},
    /* ACPI_BITREG_GLOBAL_LOCK_ENABLE   */   {ACPI_REGISTER_PM1_ENABLE,   ACPI_BITPOSITION_GLOBAL_LOCK_ENABLE,    ACPI_BITMASK_GLOBAL_LOCK_ENABLE},
    /* ACPI_BITREG_POWER_BUTTON_ENABLE  */   {ACPI_REGISTER_PM1_ENABLE,   ACPI_BITPOSITION_POWER_BUTTON_ENABLE,   ACPI_BITMASK_POWER_BUTTON_ENABLE},
    /* ACPI_BITREG_SLEEP_BUTTON_ENABLE  */   {ACPI_REGISTER_PM1_ENABLE,   ACPI_BITPOSITION_SLEEP_BUTTON_ENABLE,   ACPI_BITMASK_SLEEP_BUTTON_ENABLE},
    /* ACPI_BITREG_RT_CLOCK_ENABLE      */   {ACPI_REGISTER_PM1_ENABLE,   ACPI_BITPOSITION_RT_CLOCK_ENABLE,       ACPI_BITMASK_RT_CLOCK_ENABLE},
    /* ACPI_BITREG_PCIEXP_WAKE_DISABLE  */   {ACPI_REGISTER_PM1_ENABLE,   ACPI_BITPOSITION_PCIEXP_WAKE_DISABLE,   ACPI_BITMASK_PCIEXP_WAKE_DISABLE},

    /* ACPI_BITREG_SCI_ENABLE           */   {ACPI_REGISTER_PM1_CONTROL,  ACPI_BITPOSITION_SCI_ENABLE,            ACPI_BITMASK_SCI_ENABLE},
    /* ACPI_BITREG_BUS_MASTER_RLD       */   {ACPI_REGISTER_PM1_CONTROL,  ACPI_BITPOSITION_BUS_MASTER_RLD,        ACPI_BITMASK_BUS_MASTER_RLD},
    /* ACPI_BITREG_GLOBAL_LOCK_RELEASE  */   {ACPI_REGISTER_PM1_CONTROL,  ACPI_BITPOSITION_GLOBAL_LOCK_RELEASE,   ACPI_BITMASK_GLOBAL_LOCK_RELEASE},
    /* ACPI_BITREG_SLEEP_TYPE           */   {ACPI_REGISTER_PM1_CONTROL,  ACPI_BITPOSITION_SLEEP_TYPE,            ACPI_BITMASK_SLEEP_TYPE},
    /* ACPI_BITREG_SLEEP_ENABLE         */   {ACPI_REGISTER_PM1_CONTROL,  ACPI_BITPOSITION_SLEEP_ENABLE,          ACPI_BITMASK_SLEEP_ENABLE},

    /* ACPI_BITREG_ARB_DIS              */   {ACPI_REGISTER_PM2_CONTROL,  ACPI_BITPOSITION_ARB_DISABLE,           ACPI_BITMASK_ARB_DISABLE}
};


ACPI_FIXED_EVENT_INFO       AcpiGbl_FixedEventInfo[ACPI_NUM_FIXED_EVENTS] =
{
    /* ACPI_EVENT_PMTIMER       */  {ACPI_BITREG_TIMER_STATUS,          ACPI_BITREG_TIMER_ENABLE,        ACPI_BITMASK_TIMER_STATUS,          ACPI_BITMASK_TIMER_ENABLE},
    /* ACPI_EVENT_GLOBAL        */  {ACPI_BITREG_GLOBAL_LOCK_STATUS,    ACPI_BITREG_GLOBAL_LOCK_ENABLE,  ACPI_BITMASK_GLOBAL_LOCK_STATUS,    ACPI_BITMASK_GLOBAL_LOCK_ENABLE},
    /* ACPI_EVENT_POWER_BUTTON  */  {ACPI_BITREG_POWER_BUTTON_STATUS,   ACPI_BITREG_POWER_BUTTON_ENABLE, ACPI_BITMASK_POWER_BUTTON_STATUS,   ACPI_BITMASK_POWER_BUTTON_ENABLE},
    /* ACPI_EVENT_SLEEP_BUTTON  */  {ACPI_BITREG_SLEEP_BUTTON_STATUS,   ACPI_BITREG_SLEEP_BUTTON_ENABLE, ACPI_BITMASK_SLEEP_BUTTON_STATUS,   ACPI_BITMASK_SLEEP_BUTTON_ENABLE},
    /* ACPI_EVENT_RTC           */  {ACPI_BITREG_RT_CLOCK_STATUS,       ACPI_BITREG_RT_CLOCK_ENABLE,     ACPI_BITMASK_RT_CLOCK_STATUS,       ACPI_BITMASK_RT_CLOCK_ENABLE},
};

/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetRegionName
 *
 * PARAMETERS:  None.
 *
 * RETURN:      Status
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
 * PARAMETERS:  None.
 *
 * RETURN:      Status
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
 * PARAMETERS:  None.
 *
 * RETURN:      Status
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
 * RETURN:      Pointer to a string
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
 * RETURN:      Pointer to a string
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
 * RETURN:      Pointer to a string
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
 * RETURN:      String containing the name of the mutex. Always returns a valid
 *              pointer.
 *
 * DESCRIPTION: Translate a mutex ID into a name string (Debug only)
 *
 ******************************************************************************/

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
 * RETURN:      String corresponding to the Notify Value.
 *
 * DESCRIPTION: Translate a Notify Value to a notify namestring.
 *
 ******************************************************************************/

/* Names for Notify() values, used for debug output */

static const char        *AcpiGbl_NotifyValueNames[] =
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


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtInitGlobals
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Init library globals.  All globals that require specific
 *              initialization should be initialized here!
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtInitGlobals (
    void)
{
    ACPI_STATUS             Status;
    UINT32                  i;


    ACPI_FUNCTION_TRACE (UtInitGlobals);


    /* Create all memory caches */

    Status = AcpiUtCreateCaches ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Mutex locked flags */

    for (i = 0; i < ACPI_NUM_MUTEX; i++)
    {
        AcpiGbl_MutexInfo[i].Mutex          = NULL;
        AcpiGbl_MutexInfo[i].ThreadId       = ACPI_MUTEX_NOT_ACQUIRED;
        AcpiGbl_MutexInfo[i].UseCount       = 0;
    }

    for (i = 0; i < ACPI_NUM_OWNERID_MASKS; i++)
    {
        AcpiGbl_OwnerIdMask[i]              = 0;
    }

    /* Last OwnerID is never valid */

    AcpiGbl_OwnerIdMask[ACPI_NUM_OWNERID_MASKS - 1] = 0x80000000;

    /* Event counters */

    AcpiMethodCount                     = 0;
    AcpiSciCount                        = 0;
    AcpiGpeCount                        = 0;

    for (i = 0; i < ACPI_NUM_FIXED_EVENTS; i++)
    {
        AcpiFixedEventCount[i]              = 0;
    }

    /* GPE support */

    AcpiGbl_AllGpesInitialized          = FALSE;
    AcpiGbl_GpeXruptListHead            = NULL;
    AcpiGbl_GpeFadtBlocks[0]            = NULL;
    AcpiGbl_GpeFadtBlocks[1]            = NULL;
    AcpiCurrentGpeCount                 = 0;

    /* Global handlers */

    AcpiGbl_SystemNotify.Handler        = NULL;
    AcpiGbl_DeviceNotify.Handler        = NULL;
    AcpiGbl_ExceptionHandler            = NULL;
    AcpiGbl_InitHandler                 = NULL;
    AcpiGbl_TableHandler                = NULL;
    AcpiGbl_InterfaceHandler            = NULL;
    AcpiGbl_GlobalEventHandler          = NULL;

    /* Global Lock support */

    AcpiGbl_GlobalLockSemaphore         = NULL;
    AcpiGbl_GlobalLockMutex             = NULL;
    AcpiGbl_GlobalLockAcquired          = FALSE;
    AcpiGbl_GlobalLockHandle            = 0;
    AcpiGbl_GlobalLockPresent           = FALSE;

    /* Miscellaneous variables */

    AcpiGbl_DSDT                        = NULL;
    AcpiGbl_CmSingleStep                = FALSE;
    AcpiGbl_DbTerminateThreads          = FALSE;
    AcpiGbl_Shutdown                    = FALSE;
    AcpiGbl_NsLookupCount               = 0;
    AcpiGbl_PsFindCount                 = 0;
    AcpiGbl_AcpiHardwarePresent         = TRUE;
    AcpiGbl_LastOwnerIdIndex            = 0;
    AcpiGbl_NextOwnerIdOffset           = 0;
    AcpiGbl_TraceMethodName             = 0;
    AcpiGbl_TraceDbgLevel               = 0;
    AcpiGbl_TraceDbgLayer               = 0;
    AcpiGbl_DebuggerConfiguration       = DEBUGGER_THREADING;
    AcpiGbl_DbOutputFlags               = ACPI_DB_CONSOLE_OUTPUT;
    AcpiGbl_OsiData                     = 0;
    AcpiGbl_OsiMutex                    = NULL;

    /* Hardware oriented */

    AcpiGbl_EventsInitialized           = FALSE;
    AcpiGbl_SystemAwakeAndRunning       = TRUE;

    /* Namespace */

    AcpiGbl_ModuleCodeList              = NULL;
    AcpiGbl_RootNode                    = NULL;
    AcpiGbl_RootNodeStruct.Name.Integer = ACPI_ROOT_NAME;
    AcpiGbl_RootNodeStruct.DescriptorType = ACPI_DESC_TYPE_NAMED;
    AcpiGbl_RootNodeStruct.Type         = ACPI_TYPE_DEVICE;
    AcpiGbl_RootNodeStruct.Parent       = NULL;
    AcpiGbl_RootNodeStruct.Child        = NULL;
    AcpiGbl_RootNodeStruct.Peer         = NULL;
    AcpiGbl_RootNodeStruct.Object       = NULL;


#ifdef ACPI_DISASSEMBLER
    AcpiGbl_ExternalList                = NULL;
#endif

#ifdef ACPI_DEBUG_OUTPUT
    AcpiGbl_LowestStackPointer          = ACPI_CAST_PTR (ACPI_SIZE, ACPI_SIZE_MAX);
#endif

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
    AcpiGbl_DisplayFinalMemStats        = FALSE;
    AcpiGbl_DisableMemTracking          = FALSE;
#endif

    return_ACPI_STATUS (AE_OK);
}

/* Public globals */

ACPI_EXPORT_SYMBOL (AcpiGbl_FADT)
ACPI_EXPORT_SYMBOL (AcpiDbgLevel)
ACPI_EXPORT_SYMBOL (AcpiDbgLayer)
ACPI_EXPORT_SYMBOL (AcpiGpeCount)
ACPI_EXPORT_SYMBOL (AcpiCurrentGpeCount)


