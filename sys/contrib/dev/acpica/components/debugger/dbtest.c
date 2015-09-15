/*******************************************************************************
 *
 * Module Name: dbtest - Various debug-related tests
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acdebug.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acpredef.h>


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbtest")


/* Local prototypes */

static void
AcpiDbTestAllObjects (
    void);

static ACPI_STATUS
AcpiDbTestOneObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_STATUS
AcpiDbTestIntegerType (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  BitLength);

static ACPI_STATUS
AcpiDbTestBufferType (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  BitLength);

static ACPI_STATUS
AcpiDbTestStringType (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  ByteLength);

static ACPI_STATUS
AcpiDbReadFromObject (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_OBJECT_TYPE        ExpectedType,
    ACPI_OBJECT             **Value);

static ACPI_STATUS
AcpiDbWriteToObject (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_OBJECT             *Value);

static void
AcpiDbEvaluateAllPredefinedNames (
    char                    *CountArg);

static ACPI_STATUS
AcpiDbEvaluateOnePredefinedName (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

/*
 * Test subcommands
 */
static ACPI_DB_ARGUMENT_INFO    AcpiDbTestTypes [] =
{
    {"OBJECTS"},
    {"PREDEFINED"},
    {NULL}           /* Must be null terminated */
};

#define CMD_TEST_OBJECTS        0
#define CMD_TEST_PREDEFINED     1

#define BUFFER_FILL_VALUE       0xFF

/*
 * Support for the special debugger read/write control methods.
 * These methods are installed into the current namespace and are
 * used to read and write the various namespace objects. The point
 * is to force the AML interpreter do all of the work.
 */
#define ACPI_DB_READ_METHOD     "\\_T98"
#define ACPI_DB_WRITE_METHOD    "\\_T99"

static ACPI_HANDLE          ReadHandle = NULL;
static ACPI_HANDLE          WriteHandle = NULL;

/* ASL Definitions of the debugger read/write control methods */

#if 0
DefinitionBlock ("ssdt.aml", "SSDT", 2, "Intel", "DEBUG", 0x00000001)
{
    Method (_T98, 1, NotSerialized)     /* Read */
    {
        Return (DeRefOf (Arg0))
    }
}
DefinitionBlock ("ssdt2.aml", "SSDT", 2, "Intel", "DEBUG", 0x00000001)
{
    Method (_T99, 2, NotSerialized)     /* Write */
    {
        Store (Arg1, Arg0)
    }
}
#endif

static unsigned char ReadMethodCode[] =
{
    0x53,0x53,0x44,0x54,0x2E,0x00,0x00,0x00,  /* 00000000    "SSDT...." */
    0x02,0xC9,0x49,0x6E,0x74,0x65,0x6C,0x00,  /* 00000008    "..Intel." */
    0x44,0x45,0x42,0x55,0x47,0x00,0x00,0x00,  /* 00000010    "DEBUG..." */
    0x01,0x00,0x00,0x00,0x49,0x4E,0x54,0x4C,  /* 00000018    "....INTL" */
    0x18,0x12,0x13,0x20,0x14,0x09,0x5F,0x54,  /* 00000020    "... .._T" */
    0x39,0x38,0x01,0xA4,0x83,0x68             /* 00000028    "98...h"   */
};

static unsigned char WriteMethodCode[] =
{
    0x53,0x53,0x44,0x54,0x2E,0x00,0x00,0x00,  /* 00000000    "SSDT...." */
    0x02,0x15,0x49,0x6E,0x74,0x65,0x6C,0x00,  /* 00000008    "..Intel." */
    0x44,0x45,0x42,0x55,0x47,0x00,0x00,0x00,  /* 00000010    "DEBUG..." */
    0x01,0x00,0x00,0x00,0x49,0x4E,0x54,0x4C,  /* 00000018    "....INTL" */
    0x18,0x12,0x13,0x20,0x14,0x09,0x5F,0x54,  /* 00000020    "... .._T" */
    0x39,0x39,0x02,0x70,0x69,0x68             /* 00000028    "99.pih"   */
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbExecuteTest
 *
 * PARAMETERS:  TypeArg         - Subcommand
 *
 * RETURN:      None
 *
 * DESCRIPTION: Execute various debug tests.
 *
 * Note: Code is prepared for future expansion of the TEST command.
 *
 ******************************************************************************/

void
AcpiDbExecuteTest (
    char                    *TypeArg)
{
    UINT32                  Temp;


    AcpiUtStrupr (TypeArg);
    Temp = AcpiDbMatchArgument (TypeArg, AcpiDbTestTypes);
    if (Temp == ACPI_TYPE_NOT_FOUND)
    {
        AcpiOsPrintf ("Invalid or unsupported argument\n");
        return;
    }

    switch (Temp)
    {
    case CMD_TEST_OBJECTS:

        AcpiDbTestAllObjects ();
        break;

    case CMD_TEST_PREDEFINED:

        AcpiDbEvaluateAllPredefinedNames (NULL);
        break;

    default:
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbTestAllObjects
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: This test implements the OBJECTS subcommand. It exercises the
 *              namespace by reading/writing/comparing all data objects such
 *              as integers, strings, buffers, fields, buffer fields, etc.
 *
 ******************************************************************************/

static void
AcpiDbTestAllObjects (
    void)
{
    ACPI_STATUS             Status;


    /* Install the debugger read-object control method if necessary */

    if (!ReadHandle)
    {
        Status = AcpiInstallMethod (ReadMethodCode);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("%s, Could not install debugger read method\n",
                AcpiFormatException (Status));
            return;
        }

        Status = AcpiGetHandle (NULL, ACPI_DB_READ_METHOD, &ReadHandle);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not obtain handle for debug method %s\n",
                ACPI_DB_READ_METHOD);
            return;
        }
    }

    /* Install the debugger write-object control method if necessary */

    if (!WriteHandle)
    {
        Status = AcpiInstallMethod (WriteMethodCode);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("%s, Could not install debugger write method\n",
                AcpiFormatException (Status));
            return;
        }

        Status = AcpiGetHandle (NULL, ACPI_DB_WRITE_METHOD, &WriteHandle);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not obtain handle for debug method %s\n",
                ACPI_DB_WRITE_METHOD);
            return;
        }
    }

    /* Walk the entire namespace, testing each supported named data object */

    (void) AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
        ACPI_UINT32_MAX, AcpiDbTestOneObject, NULL, NULL, NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbTestOneObject
 *
 * PARAMETERS:  ACPI_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Test one namespace object. Supported types are Integer,
 *              String, Buffer, BufferField, and FieldUnit. All other object
 *              types are simply ignored.
 *
 *              Note: Support for Packages is not implemented.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbTestOneObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *RegionObj;
    ACPI_OBJECT_TYPE        LocalType;
    UINT32                  BitLength = 0;
    UINT32                  ByteLength = 0;
    ACPI_STATUS             Status = AE_OK;


    Node = ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, ObjHandle);
    ObjDesc = Node->Object;

    /*
     * For the supported types, get the actual bit length or
     * byte length. Map the type to one of Integer/String/Buffer.
     */
    switch (Node->Type)
    {
    case ACPI_TYPE_INTEGER:

        /* Integer width is either 32 or 64 */

        LocalType = ACPI_TYPE_INTEGER;
        BitLength = AcpiGbl_IntegerBitWidth;
        break;

    case ACPI_TYPE_STRING:

        LocalType = ACPI_TYPE_STRING;
        ByteLength = ObjDesc->String.Length;
        break;

    case ACPI_TYPE_BUFFER:

        LocalType = ACPI_TYPE_BUFFER;
        ByteLength = ObjDesc->Buffer.Length;
        BitLength = ByteLength * 8;
        break;

    case ACPI_TYPE_FIELD_UNIT:
    case ACPI_TYPE_BUFFER_FIELD:
    case ACPI_TYPE_LOCAL_REGION_FIELD:
    case ACPI_TYPE_LOCAL_INDEX_FIELD:
    case ACPI_TYPE_LOCAL_BANK_FIELD:

        LocalType = ACPI_TYPE_INTEGER;
        if (ObjDesc)
        {
            /*
             * Returned object will be a Buffer if the field length
             * is larger than the size of an Integer (32 or 64 bits
             * depending on the DSDT version).
             */
            BitLength = ObjDesc->CommonField.BitLength;
            ByteLength = ACPI_ROUND_BITS_UP_TO_BYTES (BitLength);
            if (BitLength > AcpiGbl_IntegerBitWidth)
            {
                LocalType = ACPI_TYPE_BUFFER;
            }
        }
        break;

    default:

        /* Ignore all other types */

        return (AE_OK);
    }

    /* Emit the common prefix: Type:Name */

    AcpiOsPrintf ("%14s: %4.4s",
        AcpiUtGetTypeName (Node->Type), Node->Name.Ascii);
    if (!ObjDesc)
    {
        AcpiOsPrintf (" Ignoring, no attached object\n");
        return (AE_OK);
    }

    /*
     * Check for unsupported region types. Note: AcpiExec simulates
     * access to SystemMemory, SystemIO, PCI_Config, and EC.
     */
    switch (Node->Type)
    {
    case ACPI_TYPE_LOCAL_REGION_FIELD:

        RegionObj = ObjDesc->Field.RegionObj;
        switch (RegionObj->Region.SpaceId)
        {
        case ACPI_ADR_SPACE_SYSTEM_MEMORY:
        case ACPI_ADR_SPACE_SYSTEM_IO:
        case ACPI_ADR_SPACE_PCI_CONFIG:
        case ACPI_ADR_SPACE_EC:

            break;

        default:

            AcpiOsPrintf ("      %s space is not supported [%4.4s]\n",
                AcpiUtGetRegionName (RegionObj->Region.SpaceId),
                RegionObj->Region.Node->Name.Ascii);
            return (AE_OK);
        }
        break;

    default:
        break;
    }

    /* At this point, we have resolved the object to one of the major types */

    switch (LocalType)
    {
    case ACPI_TYPE_INTEGER:

        Status = AcpiDbTestIntegerType (Node, BitLength);
        break;

    case ACPI_TYPE_STRING:

        Status = AcpiDbTestStringType (Node, ByteLength);
        break;

    case ACPI_TYPE_BUFFER:

        Status = AcpiDbTestBufferType (Node, BitLength);
        break;

    default:

        AcpiOsPrintf (" Ignoring, type not implemented (%2.2X)",
            LocalType);
        break;
    }

    switch (Node->Type)
    {
    case ACPI_TYPE_LOCAL_REGION_FIELD:

        RegionObj = ObjDesc->Field.RegionObj;
        AcpiOsPrintf (" (%s)",
            AcpiUtGetRegionName (RegionObj->Region.SpaceId));
        break;

    default:
        break;
    }

    AcpiOsPrintf ("\n");
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbTestIntegerType
 *
 * PARAMETERS:  Node                - Parent NS node for the object
 *              BitLength           - Actual length of the object. Used for
 *                                    support of arbitrary length FieldUnit
 *                                    and BufferField objects.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Test read/write for an Integer-valued object. Performs a
 *              write/read/compare of an arbitrary new value, then performs
 *              a write/read/compare of the original value.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbTestIntegerType (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  BitLength)
{
    ACPI_OBJECT             *Temp1 = NULL;
    ACPI_OBJECT             *Temp2 = NULL;
    ACPI_OBJECT             *Temp3 = NULL;
    ACPI_OBJECT             WriteValue;
    UINT64                  ValueToWrite;
    ACPI_STATUS             Status;


    if (BitLength > 64)
    {
        AcpiOsPrintf (" Invalid length for an Integer: %u", BitLength);
        return (AE_OK);
    }

    /* Read the original value */

    Status = AcpiDbReadFromObject (Node, ACPI_TYPE_INTEGER, &Temp1);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    AcpiOsPrintf (" (%4.4X/%3.3X) %8.8X%8.8X",
        BitLength, ACPI_ROUND_BITS_UP_TO_BYTES (BitLength),
        ACPI_FORMAT_UINT64 (Temp1->Integer.Value));

    ValueToWrite = ACPI_UINT64_MAX >> (64 - BitLength);
    if (Temp1->Integer.Value == ValueToWrite)
    {
        ValueToWrite = 0;
    }

    /* Write a new value */

    WriteValue.Type = ACPI_TYPE_INTEGER;
    WriteValue.Integer.Value = ValueToWrite;
    Status = AcpiDbWriteToObject (Node, &WriteValue);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    /* Ensure that we can read back the new value */

    Status = AcpiDbReadFromObject (Node, ACPI_TYPE_INTEGER, &Temp2);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    if (Temp2->Integer.Value != ValueToWrite)
    {
        AcpiOsPrintf (" MISMATCH 2: %8.8X%8.8X, expecting %8.8X%8.8X",
            ACPI_FORMAT_UINT64 (Temp2->Integer.Value),
            ACPI_FORMAT_UINT64 (ValueToWrite));
    }

    /* Write back the original value */

    WriteValue.Integer.Value = Temp1->Integer.Value;
    Status = AcpiDbWriteToObject (Node, &WriteValue);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    /* Ensure that we can read back the original value */

    Status = AcpiDbReadFromObject (Node, ACPI_TYPE_INTEGER, &Temp3);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    if (Temp3->Integer.Value != Temp1->Integer.Value)
    {
        AcpiOsPrintf (" MISMATCH 3: %8.8X%8.8X, expecting %8.8X%8.8X",
            ACPI_FORMAT_UINT64 (Temp3->Integer.Value),
            ACPI_FORMAT_UINT64 (Temp1->Integer.Value));
    }

Exit:
    if (Temp1) {AcpiOsFree (Temp1);}
    if (Temp2) {AcpiOsFree (Temp2);}
    if (Temp3) {AcpiOsFree (Temp3);}
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbTestBufferType
 *
 * PARAMETERS:  Node                - Parent NS node for the object
 *              BitLength           - Actual length of the object.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Test read/write for an Buffer-valued object. Performs a
 *              write/read/compare of an arbitrary new value, then performs
 *              a write/read/compare of the original value.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbTestBufferType (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  BitLength)
{
    ACPI_OBJECT             *Temp1 = NULL;
    ACPI_OBJECT             *Temp2 = NULL;
    ACPI_OBJECT             *Temp3 = NULL;
    UINT8                   *Buffer;
    ACPI_OBJECT             WriteValue;
    ACPI_STATUS             Status;
    UINT32                  ByteLength;
    UINT32                  i;
    UINT8                   ExtraBits;


    ByteLength = ACPI_ROUND_BITS_UP_TO_BYTES (BitLength);
    if (ByteLength == 0)
    {
        AcpiOsPrintf (" Ignoring zero length buffer");
        return (AE_OK);
    }

    /* Allocate a local buffer */

    Buffer = ACPI_ALLOCATE_ZEROED (ByteLength);
    if (!Buffer)
    {
        return (AE_NO_MEMORY);
    }

    /* Read the original value */

    Status = AcpiDbReadFromObject (Node, ACPI_TYPE_BUFFER, &Temp1);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    /* Emit a few bytes of the buffer */

    AcpiOsPrintf (" (%4.4X/%3.3X)", BitLength, Temp1->Buffer.Length);
    for (i = 0; ((i < 4) && (i < ByteLength)); i++)
    {
        AcpiOsPrintf (" %2.2X", Temp1->Buffer.Pointer[i]);
    }
    AcpiOsPrintf ("...  ");

    /*
     * Write a new value.
     *
     * Handle possible extra bits at the end of the buffer. Can
     * happen for FieldUnits larger than an integer, but the bit
     * count is not an integral number of bytes. Zero out the
     * unused bits.
     */
    memset (Buffer, BUFFER_FILL_VALUE, ByteLength);
    ExtraBits = BitLength % 8;
    if (ExtraBits)
    {
        Buffer [ByteLength - 1] = ACPI_MASK_BITS_ABOVE (ExtraBits);
    }

    WriteValue.Type = ACPI_TYPE_BUFFER;
    WriteValue.Buffer.Length = ByteLength;
    WriteValue.Buffer.Pointer = Buffer;

    Status = AcpiDbWriteToObject (Node, &WriteValue);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    /* Ensure that we can read back the new value */

    Status = AcpiDbReadFromObject (Node, ACPI_TYPE_BUFFER, &Temp2);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    if (memcmp (Temp2->Buffer.Pointer, Buffer, ByteLength))
    {
        AcpiOsPrintf (" MISMATCH 2: New buffer value");
    }

    /* Write back the original value */

    WriteValue.Buffer.Length = ByteLength;
    WriteValue.Buffer.Pointer = Temp1->Buffer.Pointer;

    Status = AcpiDbWriteToObject (Node, &WriteValue);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    /* Ensure that we can read back the original value */

    Status = AcpiDbReadFromObject (Node, ACPI_TYPE_BUFFER, &Temp3);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    if (memcmp (Temp1->Buffer.Pointer,
            Temp3->Buffer.Pointer, ByteLength))
    {
        AcpiOsPrintf (" MISMATCH 3: While restoring original buffer");
    }

Exit:
    ACPI_FREE (Buffer);
    if (Temp1) {AcpiOsFree (Temp1);}
    if (Temp2) {AcpiOsFree (Temp2);}
    if (Temp3) {AcpiOsFree (Temp3);}
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbTestStringType
 *
 * PARAMETERS:  Node                - Parent NS node for the object
 *              ByteLength          - Actual length of the object.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Test read/write for an String-valued object. Performs a
 *              write/read/compare of an arbitrary new value, then performs
 *              a write/read/compare of the original value.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbTestStringType (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  ByteLength)
{
    ACPI_OBJECT             *Temp1 = NULL;
    ACPI_OBJECT             *Temp2 = NULL;
    ACPI_OBJECT             *Temp3 = NULL;
    char                    *ValueToWrite = "Test String from AML Debugger";
    ACPI_OBJECT             WriteValue;
    ACPI_STATUS             Status;


    /* Read the original value */

    Status = AcpiDbReadFromObject (Node, ACPI_TYPE_STRING, &Temp1);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    AcpiOsPrintf (" (%4.4X/%3.3X) \"%s\"", (Temp1->String.Length * 8),
        Temp1->String.Length, Temp1->String.Pointer);

    /* Write a new value */

    WriteValue.Type = ACPI_TYPE_STRING;
    WriteValue.String.Length = strlen (ValueToWrite);
    WriteValue.String.Pointer = ValueToWrite;

    Status = AcpiDbWriteToObject (Node, &WriteValue);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    /* Ensure that we can read back the new value */

    Status = AcpiDbReadFromObject (Node, ACPI_TYPE_STRING, &Temp2);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    if (strcmp (Temp2->String.Pointer, ValueToWrite))
    {
        AcpiOsPrintf (" MISMATCH 2: %s, expecting %s",
            Temp2->String.Pointer, ValueToWrite);
    }

    /* Write back the original value */

    WriteValue.String.Length = strlen (Temp1->String.Pointer);
    WriteValue.String.Pointer = Temp1->String.Pointer;

    Status = AcpiDbWriteToObject (Node, &WriteValue);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    /* Ensure that we can read back the original value */

    Status = AcpiDbReadFromObject (Node, ACPI_TYPE_STRING, &Temp3);
    if (ACPI_FAILURE (Status))
    {
        goto Exit;
    }

    if (strcmp (Temp1->String.Pointer, Temp3->String.Pointer))
    {
        AcpiOsPrintf (" MISMATCH 3: %s, expecting %s",
            Temp3->String.Pointer, Temp1->String.Pointer);
    }

Exit:
    if (Temp1) {AcpiOsFree (Temp1);}
    if (Temp2) {AcpiOsFree (Temp2);}
    if (Temp3) {AcpiOsFree (Temp3);}
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbReadFromObject
 *
 * PARAMETERS:  Node                - Parent NS node for the object
 *              ExpectedType        - Object type expected from the read
 *              Value               - Where the value read is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Performs a read from the specified object by invoking the
 *              special debugger control method that reads the object. Thus,
 *              the AML interpreter is doing all of the work, increasing the
 *              validity of the test.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbReadFromObject (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_OBJECT_TYPE        ExpectedType,
    ACPI_OBJECT             **Value)
{
    ACPI_OBJECT             *RetValue;
    ACPI_OBJECT_LIST        ParamObjects;
    ACPI_OBJECT             Params[2];
    ACPI_BUFFER             ReturnObj;
    ACPI_STATUS             Status;


    Params[0].Type = ACPI_TYPE_LOCAL_REFERENCE;
    Params[0].Reference.ActualType = Node->Type;
    Params[0].Reference.Handle = ACPI_CAST_PTR (ACPI_HANDLE, Node);

    ParamObjects.Count = 1;
    ParamObjects.Pointer = Params;

    ReturnObj.Length  = ACPI_ALLOCATE_BUFFER;

    AcpiGbl_MethodExecuting = TRUE;
    Status = AcpiEvaluateObject (ReadHandle, NULL,
        &ParamObjects, &ReturnObj);
    AcpiGbl_MethodExecuting = FALSE;

    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not read from object, %s",
            AcpiFormatException (Status));
        return (Status);
    }

    RetValue = (ACPI_OBJECT *) ReturnObj.Pointer;

    switch (RetValue->Type)
    {
    case ACPI_TYPE_INTEGER:
    case ACPI_TYPE_BUFFER:
    case ACPI_TYPE_STRING:
        /*
         * Did we receive the type we wanted? Most important for the
         * Integer/Buffer case (when a field is larger than an Integer,
         * it should return a Buffer).
         */
        if (RetValue->Type != ExpectedType)
        {
            AcpiOsPrintf (" Type mismatch:  Expected %s, Received %s",
                AcpiUtGetTypeName (ExpectedType),
                AcpiUtGetTypeName (RetValue->Type));

            return (AE_TYPE);
        }

        *Value = RetValue;
        break;

    default:

        AcpiOsPrintf (" Unsupported return object type, %s",
            AcpiUtGetTypeName (RetValue->Type));

        AcpiOsFree (ReturnObj.Pointer);
        return (AE_TYPE);
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbWriteToObject
 *
 * PARAMETERS:  Node                - Parent NS node for the object
 *              Value               - Value to be written
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Performs a write to the specified object by invoking the
 *              special debugger control method that writes the object. Thus,
 *              the AML interpreter is doing all of the work, increasing the
 *              validity of the test.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbWriteToObject (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_OBJECT             *Value)
{
    ACPI_OBJECT_LIST        ParamObjects;
    ACPI_OBJECT             Params[2];
    ACPI_STATUS             Status;


    Params[0].Type = ACPI_TYPE_LOCAL_REFERENCE;
    Params[0].Reference.ActualType = Node->Type;
    Params[0].Reference.Handle = ACPI_CAST_PTR (ACPI_HANDLE, Node);

    /* Copy the incoming user parameter */

    memcpy (&Params[1], Value, sizeof (ACPI_OBJECT));

    ParamObjects.Count = 2;
    ParamObjects.Pointer = Params;

    AcpiGbl_MethodExecuting = TRUE;
    Status = AcpiEvaluateObject (WriteHandle, NULL, &ParamObjects, NULL);
    AcpiGbl_MethodExecuting = FALSE;

    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not write to object, %s",
            AcpiFormatException (Status));
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbEvaluateAllPredefinedNames
 *
 * PARAMETERS:  CountArg            - Max number of methods to execute
 *
 * RETURN:      None
 *
 * DESCRIPTION: Namespace batch execution. Execute predefined names in the
 *              namespace, up to the max count, if specified.
 *
 ******************************************************************************/

static void
AcpiDbEvaluateAllPredefinedNames (
    char                    *CountArg)
{
    ACPI_DB_EXECUTE_WALK    Info;


    Info.Count = 0;
    Info.MaxCount = ACPI_UINT32_MAX;

    if (CountArg)
    {
        Info.MaxCount = strtoul (CountArg, NULL, 0);
    }

    /* Search all nodes in namespace */

    (void) AcpiWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
        ACPI_UINT32_MAX, AcpiDbEvaluateOnePredefinedName, NULL,
        (void *) &Info, NULL);

    AcpiOsPrintf ("Evaluated %u predefined names in the namespace\n", Info.Count);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbEvaluateOnePredefinedName
 *
 * PARAMETERS:  Callback from WalkNamespace
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Batch execution module. Currently only executes predefined
 *              ACPI names.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbEvaluateOnePredefinedName (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE         *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ACPI_DB_EXECUTE_WALK        *Info = (ACPI_DB_EXECUTE_WALK *) Context;
    char                        *Pathname;
    const ACPI_PREDEFINED_INFO  *Predefined;
    ACPI_DEVICE_INFO            *ObjInfo;
    ACPI_OBJECT_LIST            ParamObjects;
    ACPI_OBJECT                 Params[ACPI_METHOD_NUM_ARGS];
    ACPI_OBJECT                 *ThisParam;
    ACPI_BUFFER                 ReturnObj;
    ACPI_STATUS                 Status;
    UINT16                      ArgTypeList;
    UINT8                       ArgCount;
    UINT8                       ArgType;
    UINT32                      i;


    /* The name must be a predefined ACPI name */

    Predefined = AcpiUtMatchPredefinedMethod (Node->Name.Ascii);
    if (!Predefined)
    {
        return (AE_OK);
    }

    if (Node->Type == ACPI_TYPE_LOCAL_SCOPE)
    {
        return (AE_OK);
    }

    Pathname = AcpiNsGetExternalPathname (Node);
    if (!Pathname)
    {
        return (AE_OK);
    }

    /* Get the object info for number of method parameters */

    Status = AcpiGetObjectInfo (ObjHandle, &ObjInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_FREE (Pathname);
        return (Status);
    }

    ParamObjects.Count = 0;
    ParamObjects.Pointer = NULL;

    if (ObjInfo->Type == ACPI_TYPE_METHOD)
    {
        /* Setup default parameters (with proper types) */

        ArgTypeList = Predefined->Info.ArgumentList;
        ArgCount = METHOD_GET_ARG_COUNT (ArgTypeList);

        /*
         * Setup the ACPI-required number of arguments, regardless of what
         * the actual method defines. If there is a difference, then the
         * method is wrong and a warning will be issued during execution.
         */
        ThisParam = Params;
        for (i = 0; i < ArgCount; i++)
        {
            ArgType = METHOD_GET_NEXT_TYPE (ArgTypeList);
            ThisParam->Type = ArgType;

            switch (ArgType)
            {
            case ACPI_TYPE_INTEGER:

                ThisParam->Integer.Value = 1;
                break;

            case ACPI_TYPE_STRING:

                ThisParam->String.Pointer =
                    "This is the default argument string";
                ThisParam->String.Length =
                    strlen (ThisParam->String.Pointer);
                break;

            case ACPI_TYPE_BUFFER:

                ThisParam->Buffer.Pointer = (UINT8 *) Params; /* just a garbage buffer */
                ThisParam->Buffer.Length = 48;
                break;

             case ACPI_TYPE_PACKAGE:

                ThisParam->Package.Elements = NULL;
                ThisParam->Package.Count = 0;
                break;

           default:

                AcpiOsPrintf ("%s: Unsupported argument type: %u\n",
                    Pathname, ArgType);
                break;
            }

            ThisParam++;
        }

        ParamObjects.Count = ArgCount;
        ParamObjects.Pointer = Params;
    }

    ACPI_FREE (ObjInfo);
    ReturnObj.Pointer = NULL;
    ReturnObj.Length = ACPI_ALLOCATE_BUFFER;

    /* Do the actual method execution */

    AcpiGbl_MethodExecuting = TRUE;

    Status = AcpiEvaluateObject (Node, NULL, &ParamObjects, &ReturnObj);

    AcpiOsPrintf ("%-32s returned %s\n",
        Pathname, AcpiFormatException (Status));
    AcpiGbl_MethodExecuting = FALSE;
    ACPI_FREE (Pathname);

    /* Ignore status from method execution */

    Status = AE_OK;

    /* Update count, check if we have executed enough methods */

    Info->Count++;
    if (Info->Count >= Info->MaxCount)
    {
        Status = AE_CTRL_TERMINATE;
    }

    return (Status);
}
