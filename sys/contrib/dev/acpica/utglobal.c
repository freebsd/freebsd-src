/******************************************************************************
 *
 * Module Name: utglobal - Global variables for the ACPI subsystem
 *              $Revision: 191 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
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

#define __UTGLOBAL_C__
#define DEFINE_ACPI_GLOBALS

#include "acpi.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utglobal")


/******************************************************************************
 *
 * FUNCTION:    AcpiFormatException
 *
 * PARAMETERS:  Status       - The ACPI_STATUS code to be formatted
 *
 * RETURN:      A string containing the exception  text
 *
 * DESCRIPTION: This function translates an ACPI exception into an ASCII string.
 *
 ******************************************************************************/

const char *
AcpiFormatException (
    ACPI_STATUS             Status)
{
    const char              *Exception = "UNKNOWN_STATUS_CODE";
    ACPI_STATUS             SubStatus;


    ACPI_FUNCTION_NAME ("FormatException");


    SubStatus = (Status & ~AE_CODE_MASK);

    switch (Status & AE_CODE_MASK)
    {
    case AE_CODE_ENVIRONMENTAL:

        if (SubStatus <= AE_CODE_ENV_MAX)
        {
            Exception = AcpiGbl_ExceptionNames_Env [SubStatus];
            break;
        }
        goto Unknown;

    case AE_CODE_PROGRAMMER:

        if (SubStatus <= AE_CODE_PGM_MAX)
        {
            Exception = AcpiGbl_ExceptionNames_Pgm [SubStatus -1];
            break;
        }
        goto Unknown;

    case AE_CODE_ACPI_TABLES:

        if (SubStatus <= AE_CODE_TBL_MAX)
        {
            Exception = AcpiGbl_ExceptionNames_Tbl [SubStatus -1];
            break;
        }
        goto Unknown;

    case AE_CODE_AML:

        if (SubStatus <= AE_CODE_AML_MAX)
        {
            Exception = AcpiGbl_ExceptionNames_Aml [SubStatus -1];
            break;
        }
        goto Unknown;

    case AE_CODE_CONTROL:

        if (SubStatus <= AE_CODE_CTRL_MAX)
        {
            Exception = AcpiGbl_ExceptionNames_Ctrl [SubStatus -1];
            break;
        }
        goto Unknown;

    default:
        goto Unknown;
    }


    return ((const char *) Exception);

Unknown:

    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unknown exception code: 0x%8.8X\n", Status));
    return ((const char *) Exception);
}


/******************************************************************************
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

const UINT8                 AcpiGbl_DecodeTo8bit [8] = {1,2,4,8,16,32,64,128};

const char                  *AcpiGbl_DbSleepStates[ACPI_S_STATE_COUNT] = {
                                "\\_S0_",
                                "\\_S1_",
                                "\\_S2_",
                                "\\_S3_",
                                "\\_S4_",
                                "\\_S5_"};


/******************************************************************************
 *
 * Namespace globals
 *
 ******************************************************************************/


/*
 * Predefined ACPI Names (Built-in to the Interpreter)
 *
 * Initial values are currently supported only for types String and Number.
 * Both are specified as strings in this table.
 *
 * NOTES:
 * 1) _SB_ is defined to be a device to allow _SB_/_INI to be run
 *    during the initialization sequence.
 */

const ACPI_PREDEFINED_NAMES     AcpiGbl_PreDefinedNames[] =
{
    {"_GPE",    ACPI_TYPE_LOCAL_SCOPE,      NULL},
    {"_PR_",    ACPI_TYPE_LOCAL_SCOPE,      NULL},
    {"_SB_",    ACPI_TYPE_DEVICE,           NULL},
    {"_SI_",    ACPI_TYPE_LOCAL_SCOPE,      NULL},
    {"_TZ_",    ACPI_TYPE_LOCAL_SCOPE,      NULL},
    {"_REV",    ACPI_TYPE_INTEGER,          "2"},
    {"_OS_",    ACPI_TYPE_STRING,           ACPI_OS_NAME},
    {"_GL_",    ACPI_TYPE_MUTEX,            "0"},

#if defined (ACPI_NO_METHOD_EXECUTION) || defined (ACPI_CONSTANT_EVAL_ONLY)
    {"_OSI",    ACPI_TYPE_METHOD,           "1"},
#endif
    {NULL,      ACPI_TYPE_ANY,              NULL}              /* Table terminator */
};


/*
 * Properties of the ACPI Object Types, both internal and external.
 * The table is indexed by values of ACPI_OBJECT_TYPE
 */

const UINT8                     AcpiGbl_NsProperties[] =
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
    ACPI_NS_NORMAL,                     /* 22 Notify           */
    ACPI_NS_NORMAL,                     /* 23 Address Handler  */
    ACPI_NS_NEWSCOPE | ACPI_NS_LOCAL,   /* 24 Resource Desc    */
    ACPI_NS_NEWSCOPE | ACPI_NS_LOCAL,   /* 25 Resource Field   */
    ACPI_NS_NEWSCOPE,                   /* 26 Scope            */
    ACPI_NS_NORMAL,                     /* 27 Extra            */
    ACPI_NS_NORMAL,                     /* 28 Data             */
    ACPI_NS_NORMAL                      /* 29 Invalid          */
};


/* Hex to ASCII conversion table */

static const char           AcpiGbl_HexToAscii[] =
                                {'0','1','2','3','4','5','6','7',
                                 '8','9','A','B','C','D','E','F'};

/*****************************************************************************
 *
 * FUNCTION:    AcpiUtHexToAsciiChar
 *
 * PARAMETERS:  Integer             - Contains the hex digit
 *              Position            - bit position of the digit within the
 *                                    integer
 *
 * RETURN:      Ascii character
 *
 * DESCRIPTION: Convert a hex digit to an ascii character
 *
 ****************************************************************************/

char
AcpiUtHexToAsciiChar (
    ACPI_INTEGER            Integer,
    UINT32                  Position)
{

    return (AcpiGbl_HexToAscii[(Integer >> Position) & 0xF]);
}


/******************************************************************************
 *
 * Table name globals
 *
 * NOTE: This table includes ONLY the ACPI tables that the subsystem consumes.
 * it is NOT an exhaustive list of all possible ACPI tables.  All ACPI tables
 * that are not used by the subsystem are simply ignored.
 *
 * Do NOT add any table to this list that is not consumed directly by this
 * subsystem.
 *
 ******************************************************************************/


ACPI_TABLE_LIST             AcpiGbl_TableLists[NUM_ACPI_TABLE_TYPES];


ACPI_TABLE_SUPPORT          AcpiGbl_TableData[NUM_ACPI_TABLE_TYPES] =
{
    /***********    Name,   Signature, Global typed pointer     Signature size,      Type                  How many allowed?,    Contains valid AML? */

    /* RSDP 0 */ {RSDP_NAME, RSDP_SIG, NULL,                    sizeof (RSDP_SIG)-1, ACPI_TABLE_ROOT     | ACPI_TABLE_SINGLE},
    /* DSDT 1 */ {DSDT_SIG,  DSDT_SIG, (void *) &AcpiGbl_DSDT,  sizeof (DSDT_SIG)-1, ACPI_TABLE_SECONDARY| ACPI_TABLE_SINGLE   | ACPI_TABLE_EXECUTABLE},
    /* FADT 2 */ {FADT_SIG,  FADT_SIG, (void *) &AcpiGbl_FADT,  sizeof (FADT_SIG)-1, ACPI_TABLE_PRIMARY  | ACPI_TABLE_SINGLE},
    /* FACS 3 */ {FACS_SIG,  FACS_SIG, (void *) &AcpiGbl_FACS,  sizeof (FACS_SIG)-1, ACPI_TABLE_SECONDARY| ACPI_TABLE_SINGLE},
    /* PSDT 4 */ {PSDT_SIG,  PSDT_SIG, NULL,                    sizeof (PSDT_SIG)-1, ACPI_TABLE_PRIMARY  | ACPI_TABLE_MULTIPLE | ACPI_TABLE_EXECUTABLE},
    /* SSDT 5 */ {SSDT_SIG,  SSDT_SIG, NULL,                    sizeof (SSDT_SIG)-1, ACPI_TABLE_PRIMARY  | ACPI_TABLE_MULTIPLE | ACPI_TABLE_EXECUTABLE},
    /* XSDT 6 */ {XSDT_SIG,  XSDT_SIG, NULL,                    sizeof (RSDT_SIG)-1, ACPI_TABLE_ROOT     | ACPI_TABLE_SINGLE},
};


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

    /* ACPI_BITREG_TIMER_ENABLE         */   {ACPI_REGISTER_PM1_ENABLE,   ACPI_BITPOSITION_TIMER_ENABLE,          ACPI_BITMASK_TIMER_ENABLE},
    /* ACPI_BITREG_GLOBAL_LOCK_ENABLE   */   {ACPI_REGISTER_PM1_ENABLE,   ACPI_BITPOSITION_GLOBAL_LOCK_ENABLE,    ACPI_BITMASK_GLOBAL_LOCK_ENABLE},
    /* ACPI_BITREG_POWER_BUTTON_ENABLE  */   {ACPI_REGISTER_PM1_ENABLE,   ACPI_BITPOSITION_POWER_BUTTON_ENABLE,   ACPI_BITMASK_POWER_BUTTON_ENABLE},
    /* ACPI_BITREG_SLEEP_BUTTON_ENABLE  */   {ACPI_REGISTER_PM1_ENABLE,   ACPI_BITPOSITION_SLEEP_BUTTON_ENABLE,   ACPI_BITMASK_SLEEP_BUTTON_ENABLE},
    /* ACPI_BITREG_RT_CLOCK_ENABLE      */   {ACPI_REGISTER_PM1_ENABLE,   ACPI_BITPOSITION_RT_CLOCK_ENABLE,       ACPI_BITMASK_RT_CLOCK_ENABLE},
    /* ACPI_BITREG_WAKE_ENABLE          */   {ACPI_REGISTER_PM1_ENABLE,   0,                                      0},

    /* ACPI_BITREG_SCI_ENABLE           */   {ACPI_REGISTER_PM1_CONTROL,  ACPI_BITPOSITION_SCI_ENABLE,            ACPI_BITMASK_SCI_ENABLE},
    /* ACPI_BITREG_BUS_MASTER_RLD       */   {ACPI_REGISTER_PM1_CONTROL,  ACPI_BITPOSITION_BUS_MASTER_RLD,        ACPI_BITMASK_BUS_MASTER_RLD},
    /* ACPI_BITREG_GLOBAL_LOCK_RELEASE  */   {ACPI_REGISTER_PM1_CONTROL,  ACPI_BITPOSITION_GLOBAL_LOCK_RELEASE,   ACPI_BITMASK_GLOBAL_LOCK_RELEASE},
    /* ACPI_BITREG_SLEEP_TYPE_A         */   {ACPI_REGISTER_PM1_CONTROL,  ACPI_BITPOSITION_SLEEP_TYPE_X,          ACPI_BITMASK_SLEEP_TYPE_X},
    /* ACPI_BITREG_SLEEP_TYPE_B         */   {ACPI_REGISTER_PM1_CONTROL,  ACPI_BITPOSITION_SLEEP_TYPE_X,          ACPI_BITMASK_SLEEP_TYPE_X},
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

/*****************************************************************************
 *
 * FUNCTION:    AcpiUtGetRegionName
 *
 * PARAMETERS:  None.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Translate a Space ID into a name string (Debug only)
 *
 ****************************************************************************/

/* Region type decoding */

const char        *AcpiGbl_RegionTypes[ACPI_NUM_PREDEFINED_REGIONS] =
{
/*! [Begin] no source code translation (keep these ASL Keywords as-is) */
    "SystemMemory",
    "SystemIO",
    "PCI_Config",
    "EmbeddedControl",
    "SMBus",
    "CMOS",
    "PCIBARTarget",
    "DataTable"
/*! [End] no source code translation !*/
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

    return ((char *) AcpiGbl_RegionTypes[SpaceId]);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiUtGetEventName
 *
 * PARAMETERS:  None.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Translate a Event ID into a name string (Debug only)
 *
 ****************************************************************************/

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

    return ((char *) AcpiGbl_EventTypes[EventId]);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiUtGetTypeName
 *
 * PARAMETERS:  None.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Translate a Type ID into a name string (Debug only)
 *
 ****************************************************************************/

/*
 * Elements of AcpiGbl_NsTypeNames below must match
 * one-to-one with values of ACPI_OBJECT_TYPE
 *
 * The type ACPI_TYPE_ANY (Untyped) is used as a "don't care" when searching; when
 * stored in a table it really means that we have thus far seen no evidence to
 * indicatewhat type is actually going to be stored for this entry.
 */

static const char           AcpiGbl_BadType[] = "UNDEFINED";
#define TYPE_NAME_LENGTH    12                           /* Maximum length of each string */

static const char           *AcpiGbl_NsTypeNames[] =    /* printable names of ACPI types */
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
    /* 22 */ "Notify",
    /* 23 */ "AddrHandler",
    /* 24 */ "ResourceDesc",
    /* 25 */ "ResourceFld",
    /* 26 */ "Scope",
    /* 27 */ "Extra",
    /* 28 */ "Data",
    /* 39 */ "Invalid"
};


char *
AcpiUtGetTypeName (
    ACPI_OBJECT_TYPE        Type)
{

    if (Type > ACPI_TYPE_INVALID)
    {
        return ((char *) AcpiGbl_BadType);
    }

    return ((char *) AcpiGbl_NsTypeNames[Type]);
}


char *
AcpiUtGetObjectTypeName (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{

    if (!ObjDesc)
    {
        return ("[NULL Object Descriptor]");
    }

    return (AcpiUtGetTypeName (ACPI_GET_OBJECT_TYPE (ObjDesc)));
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiUtGetNodeName
 *
 * PARAMETERS:  Object               - A namespace node
 *
 * RETURN:      Pointer to a string
 *
 * DESCRIPTION: Validate the node and return the node's ACPI name.
 *
 ****************************************************************************/

char *
AcpiUtGetNodeName (
    void                    *Object)
{
    ACPI_NAMESPACE_NODE     *Node;


    if (!Object)
    {
        return ("NULL NODE");
    }

    Node = (ACPI_NAMESPACE_NODE *) Object;

    if (Node->Descriptor != ACPI_DESC_TYPE_NAMED)
    {
        return ("****");
    }

    if (!AcpiUtValidAcpiName (* (UINT32 *) Node->Name.Ascii))
    {
        return ("----");
    }

    return (Node->Name.Ascii);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiUtGetDescriptorName
 *
 * PARAMETERS:  Object               - An ACPI object
 *
 * RETURN:      Pointer to a string
 *
 * DESCRIPTION: Validate object and return the descriptor type
 *
 ****************************************************************************/

static const char           *AcpiGbl_DescTypeNames[] =    /* printable names of descriptor types */
{
    /* 00 */ "Invalid",
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
        return ((char *) AcpiGbl_BadType);
    }

    return ((char *) AcpiGbl_DescTypeNames[ACPI_GET_DESCRIPTOR_TYPE (Object)]);

}


#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)
/*
 * Strings and procedures used for debug only
 */

/*****************************************************************************
 *
 * FUNCTION:    AcpiUtGetMutexName
 *
 * PARAMETERS:  None.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Translate a mutex ID into a name string (Debug only)
 *
 ****************************************************************************/

char *
AcpiUtGetMutexName (
    UINT32                  MutexId)
{

    if (MutexId > MAX_MUTEX)
    {
        return ("Invalid Mutex ID");
    }

    return (AcpiGbl_MutexNames[MutexId]);
}

#endif


/*****************************************************************************
 *
 * FUNCTION:    AcpiUtValidObjectType
 *
 * PARAMETERS:  Type            - Object type to be validated
 *
 * RETURN:      TRUE if valid object type
 *
 * DESCRIPTION: Validate an object type
 *
 ****************************************************************************/

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


/****************************************************************************
 *
 * FUNCTION:    AcpiUtAllocateOwnerId
 *
 * PARAMETERS:  IdType          - Type of ID (method or table)
 *
 * DESCRIPTION: Allocate a table or method owner id
 *
 ***************************************************************************/

ACPI_OWNER_ID
AcpiUtAllocateOwnerId (
    UINT32                  IdType)
{
    ACPI_OWNER_ID           OwnerId = 0xFFFF;


    ACPI_FUNCTION_TRACE ("UtAllocateOwnerId");


    if (ACPI_FAILURE (AcpiUtAcquireMutex (ACPI_MTX_CACHES)))
    {
        return (0);
    }

    switch (IdType)
    {
    case ACPI_OWNER_TYPE_TABLE:

        OwnerId = AcpiGbl_NextTableOwnerId;
        AcpiGbl_NextTableOwnerId++;

        /* Check for wraparound */

        if (AcpiGbl_NextTableOwnerId == ACPI_FIRST_METHOD_ID)
        {
            AcpiGbl_NextTableOwnerId = ACPI_FIRST_TABLE_ID;
            ACPI_REPORT_WARNING (("Table owner ID wraparound\n"));
        }
        break;


    case ACPI_OWNER_TYPE_METHOD:

        OwnerId = AcpiGbl_NextMethodOwnerId;
        AcpiGbl_NextMethodOwnerId++;

        if (AcpiGbl_NextMethodOwnerId == ACPI_FIRST_TABLE_ID)
        {
            /* Check for wraparound */

            AcpiGbl_NextMethodOwnerId = ACPI_FIRST_METHOD_ID;
        }
        break;

    default:
        break;
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_CACHES);
    return_VALUE (OwnerId);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiUtInitGlobals
 *
 * PARAMETERS:  none
 *
 * DESCRIPTION: Init library globals.  All globals that require specific
 *              initialization should be initialized here!
 *
 ***************************************************************************/

void
AcpiUtInitGlobals (
    void)
{
    UINT32                  i;


    ACPI_FUNCTION_TRACE ("UtInitGlobals");

    /* Memory allocation and cache lists */

    ACPI_MEMSET (AcpiGbl_MemoryLists, 0, sizeof (ACPI_MEMORY_LIST) * ACPI_NUM_MEM_LISTS);

    AcpiGbl_MemoryLists[ACPI_MEM_LIST_STATE].LinkOffset         = (UINT16) ACPI_PTR_DIFF (&(((ACPI_GENERIC_STATE *) NULL)->Common.Next), NULL);
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_PSNODE].LinkOffset        = (UINT16) ACPI_PTR_DIFF (&(((ACPI_PARSE_OBJECT *) NULL)->Common.Next), NULL);
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_PSNODE_EXT].LinkOffset    = (UINT16) ACPI_PTR_DIFF (&(((ACPI_PARSE_OBJECT *) NULL)->Common.Next), NULL);
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_OPERAND].LinkOffset       = (UINT16) ACPI_PTR_DIFF (&(((ACPI_OPERAND_OBJECT *) NULL)->Cache.Next), NULL);
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_WALK].LinkOffset          = (UINT16) ACPI_PTR_DIFF (&(((ACPI_WALK_STATE *) NULL)->Next), NULL);

    AcpiGbl_MemoryLists[ACPI_MEM_LIST_NSNODE].ObjectSize        = sizeof (ACPI_NAMESPACE_NODE);
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_STATE].ObjectSize         = sizeof (ACPI_GENERIC_STATE);
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_PSNODE].ObjectSize        = sizeof (ACPI_PARSE_OBJ_COMMON);
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_PSNODE_EXT].ObjectSize    = sizeof (ACPI_PARSE_OBJ_NAMED);
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_OPERAND].ObjectSize       = sizeof (ACPI_OPERAND_OBJECT);
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_WALK].ObjectSize          = sizeof (ACPI_WALK_STATE);

    AcpiGbl_MemoryLists[ACPI_MEM_LIST_STATE].MaxCacheDepth      = ACPI_MAX_STATE_CACHE_DEPTH;
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_PSNODE].MaxCacheDepth     = ACPI_MAX_PARSE_CACHE_DEPTH;
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_PSNODE_EXT].MaxCacheDepth = ACPI_MAX_EXTPARSE_CACHE_DEPTH;
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_OPERAND].MaxCacheDepth    = ACPI_MAX_OBJECT_CACHE_DEPTH;
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_WALK].MaxCacheDepth       = ACPI_MAX_WALK_CACHE_DEPTH;

    ACPI_MEM_TRACKING (AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].ListName       = "Global Memory Allocation");
    ACPI_MEM_TRACKING (AcpiGbl_MemoryLists[ACPI_MEM_LIST_NSNODE].ListName       = "Namespace Nodes");
    ACPI_MEM_TRACKING (AcpiGbl_MemoryLists[ACPI_MEM_LIST_STATE].ListName        = "State Object Cache");
    ACPI_MEM_TRACKING (AcpiGbl_MemoryLists[ACPI_MEM_LIST_PSNODE].ListName       = "Parse Node Cache");
    ACPI_MEM_TRACKING (AcpiGbl_MemoryLists[ACPI_MEM_LIST_PSNODE_EXT].ListName   = "Extended Parse Node Cache");
    ACPI_MEM_TRACKING (AcpiGbl_MemoryLists[ACPI_MEM_LIST_OPERAND].ListName      = "Operand Object Cache");
    ACPI_MEM_TRACKING (AcpiGbl_MemoryLists[ACPI_MEM_LIST_WALK].ListName         = "Tree Walk Node Cache");

    /* ACPI table structure */

    for (i = 0; i < NUM_ACPI_TABLE_TYPES; i++)
    {
        AcpiGbl_TableLists[i].Next          = NULL;
        AcpiGbl_TableLists[i].Count         = 0;
    }

    /* Mutex locked flags */

    for (i = 0; i < NUM_MUTEX; i++)
    {
        AcpiGbl_MutexInfo[i].Mutex          = NULL;
        AcpiGbl_MutexInfo[i].OwnerId        = ACPI_MUTEX_NOT_ACQUIRED;
        AcpiGbl_MutexInfo[i].UseCount       = 0;
    }

    /* GPE support */

    AcpiGbl_GpeXruptListHead            = NULL;
    AcpiGbl_GpeFadtBlocks[0]            = NULL;
    AcpiGbl_GpeFadtBlocks[1]            = NULL;

    /* Global notify handlers */

    AcpiGbl_SystemNotify.Handler        = NULL;
    AcpiGbl_DeviceNotify.Handler        = NULL;
    AcpiGbl_InitHandler                 = NULL;

    /* Global "typed" ACPI table pointers */

    AcpiGbl_RSDP                        = NULL;
    AcpiGbl_XSDT                        = NULL;
    AcpiGbl_FACS                        = NULL;
    AcpiGbl_FADT                        = NULL;
    AcpiGbl_DSDT                        = NULL;

    /* Global Lock support */

    AcpiGbl_GlobalLockAcquired          = FALSE;
    AcpiGbl_GlobalLockThreadCount       = 0;
    AcpiGbl_GlobalLockHandle            = 0;

    /* Miscellaneous variables */

    AcpiGbl_TableFlags                  = ACPI_PHYSICAL_POINTER;
    AcpiGbl_RsdpOriginalLocation        = 0;
    AcpiGbl_CmSingleStep                = FALSE;
    AcpiGbl_DbTerminateThreads          = FALSE;
    AcpiGbl_Shutdown                    = FALSE;
    AcpiGbl_NsLookupCount               = 0;
    AcpiGbl_PsFindCount                 = 0;
    AcpiGbl_AcpiHardwarePresent         = TRUE;
    AcpiGbl_NextTableOwnerId            = ACPI_FIRST_TABLE_ID;
    AcpiGbl_NextMethodOwnerId           = ACPI_FIRST_METHOD_ID;
    AcpiGbl_DebuggerConfiguration       = DEBUGGER_THREADING;
    AcpiGbl_DbOutputFlags               = ACPI_DB_CONSOLE_OUTPUT;

    /* Hardware oriented */

    AcpiGbl_EventsInitialized           = FALSE;

    /* Namespace */

    AcpiGbl_RootNode                    = NULL;

    AcpiGbl_RootNodeStruct.Name.Integer = ACPI_ROOT_NAME;
    AcpiGbl_RootNodeStruct.Descriptor   = ACPI_DESC_TYPE_NAMED;
    AcpiGbl_RootNodeStruct.Type         = ACPI_TYPE_DEVICE;
    AcpiGbl_RootNodeStruct.Child        = NULL;
    AcpiGbl_RootNodeStruct.Peer         = NULL;
    AcpiGbl_RootNodeStruct.Object       = NULL;
    AcpiGbl_RootNodeStruct.Flags        = ANOBJ_END_OF_PEER_LIST;


#ifdef ACPI_DEBUG_OUTPUT
    AcpiGbl_LowestStackPointer          = ACPI_SIZE_MAX;
#endif

    return_VOID;
}


