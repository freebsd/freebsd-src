/******************************************************************************
 *
 * Module Name: dswload - Dispatcher namespace load callbacks
 *              $Revision: 23 $
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

#define __DSWLOAD_C__

#include "acpi.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "acevents.h"


#define _COMPONENT          DISPATCHER
        MODULE_NAME         ("dswload")


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsLoad1BeginOp
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *              Op              - Op that has been just been reached in the
 *                                walk;  Arguments have not been evaluated yet.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback used during the loading of ACPI tables.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsLoad1BeginOp (
    UINT16                  Opcode,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;
    OBJECT_TYPE_INTERNAL    DataType;
    NATIVE_CHAR             *Path;


    DEBUG_PRINT (TRACE_DISPATCH,
        ("Load1BeginOp: Op=%p State=%p\n", Op, WalkState));


    /* We are only interested in opcodes that have an associated name */

    if (!AcpiPsIsNamedOp (Opcode))
    {
        *OutOp = Op;
        return (AE_OK);
    }


    /* Check if this object has already been installed in the namespace */

    if (Op && Op->Node)
    {
        *OutOp = Op;
        return (AE_OK);
    }

    Path = AcpiPsGetNextNamestring (WalkState->ParserState);

    /* Map the raw opcode into an internal object type */

    DataType = AcpiDsMapNamedOpcodeToDataType (Opcode);


    DEBUG_PRINT (TRACE_DISPATCH,
        ("Load1BeginOp: State=%p Op=%p Type=%x\n", WalkState, Op, DataType));


    if (Opcode == AML_SCOPE_OP)
    {
        DEBUG_PRINT (TRACE_DISPATCH,
            ("Load1BeginOp: State=%p Op=%p Type=%x\n", WalkState, Op, DataType));
    }

    /*
     * Enter the named type into the internal namespace.  We enter the name
     * as we go downward in the parse tree.  Any necessary subobjects that involve
     * arguments to the opcode must be created as we go back up the parse tree later.
     */
    Status = AcpiNsLookup (WalkState->ScopeInfo, Path,
                            DataType, IMODE_LOAD_PASS1,
                            NS_NO_UPSEARCH, WalkState, &(Node));

    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    if (!Op)
    {
        /* Create a new op */

        Op = AcpiPsAllocOp (Opcode);
        if (!Op)
        {
            return (AE_NO_MEMORY);
        }
    }

    /* Initialize */

    ((ACPI_PARSE2_OBJECT *)Op)->Name = Node->Name;

    /*
     * Put the Node in the "op" object that the parser uses, so we
     * can get it again quickly when this scope is closed
     */
    Op->Node = Node;


    AcpiPsAppendArg (AcpiPsGetParentScope (WalkState->ParserState), Op);

    *OutOp = Op;

    return (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsLoad1EndOp
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *              Op              - Op that has been just been completed in the
 *                                walk;  Arguments have now been evaluated.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback used during the loading of the namespace,
 *              both control methods and everything else.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsLoad1EndOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    OBJECT_TYPE_INTERNAL    DataType;


    DEBUG_PRINT (TRACE_DISPATCH,
        ("Load1EndOp: Op=%p State=%p\n", Op, WalkState));

    /* We are only interested in opcodes that have an associated name */

    if (!AcpiPsIsNamedOp (Op->Opcode))
    {
        return (AE_OK);
    }


    /* Get the type to determine if we should pop the scope */

    DataType = AcpiDsMapNamedOpcodeToDataType (Op->Opcode);

    if (Op->Opcode == AML_NAME_OP)
    {
        /* For Name opcode, check the argument */

        if (Op->Value.Arg)
        {
            DataType = AcpiDsMapOpcodeToDataType (
                            (Op->Value.Arg)->Opcode, NULL);
            ((ACPI_NAMESPACE_NODE *)Op->Node)->Type =
                            (UINT8) DataType;
        }
    }


    /* Pop the scope stack */

    if (AcpiNsOpensScope (DataType))
    {

        DEBUG_PRINT (TRACE_DISPATCH,
            ("Load1EndOp/%s: Popping scope for Op %p\n",
            AcpiCmGetTypeName (DataType), Op));
        AcpiDsScopeStackPop (WalkState);
    }

    return (AE_OK);

}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsLoad2BeginOp
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *              Op              - Op that has been just been reached in the
 *                                walk;  Arguments have not been evaluated yet.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback used during the loading of ACPI tables.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsLoad2BeginOp (
    UINT16                  Opcode,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;
    OBJECT_TYPE_INTERNAL    DataType;
    NATIVE_CHAR             *BufferPtr;
    void                    *Original = NULL;


    DEBUG_PRINT (TRACE_DISPATCH,
        ("Load2BeginOp: Op=%p State=%p\n", Op, WalkState));


    /* We only care about Namespace opcodes here */

    if (!AcpiPsIsNamespaceOp (Opcode) &&
        Opcode != AML_NAMEPATH_OP)
    {
        return (AE_OK);
    }


    /* Temp! same code as in psparse */

    if (!AcpiPsIsNamedOp (Opcode))
    {
        return (AE_OK);
    }

    if (Op)
    {
        /*
         * Get the name we are going to enter or lookup in the namespace
         */
        if (Opcode == AML_NAMEPATH_OP)
        {
            /* For Namepath op, get the path string */

            BufferPtr = Op->Value.String;
            if (!BufferPtr)
            {
                /* No name, just exit */

                return (AE_OK);
            }
        }

        else
        {
            /* Get name from the op */

            BufferPtr = (NATIVE_CHAR *) &((ACPI_PARSE2_OBJECT *)Op)->Name;
        }
    }

    else
    {
        BufferPtr = AcpiPsGetNextNamestring (WalkState->ParserState);
    }


    /* Map the raw opcode into an internal object type */

    DataType = AcpiDsMapNamedOpcodeToDataType (Opcode);

    DEBUG_PRINT (TRACE_DISPATCH,
        ("Load2BeginOp: State=%p Op=%p Type=%x\n", WalkState, Op, DataType));


    if (Opcode == AML_DEF_FIELD_OP      ||
        Opcode == AML_BANK_FIELD_OP     ||
        Opcode == AML_INDEX_FIELD_OP)
    {
        Node = NULL;
        Status = AE_OK;
    }

    else if (Opcode == AML_NAMEPATH_OP)
    {
        /*
         * The NamePath is an object reference to an existing object.  Don't enter the
         * name into the namespace, but look it up for use later
         */
        Status = AcpiNsLookup (WalkState->ScopeInfo, BufferPtr,
                                DataType, IMODE_EXECUTE,
                                NS_SEARCH_PARENT, WalkState,
                                &(Node));
    }

    else
    {
        if (Op && Op->Node)
        {
            Original = Op->Node;
            Node = Op->Node;

            if (AcpiNsOpensScope (DataType))
            {
                Status = AcpiDsScopeStackPush (Node,
                                                DataType,
                                                WalkState);
                if (ACPI_FAILURE (Status))
                {
                    return (Status);
                }

            }
            return (AE_OK);
        }

        /*
         * Enter the named type into the internal namespace.  We enter the name
         * as we go downward in the parse tree.  Any necessary subobjects that involve
         * arguments to the opcode must be created as we go back up the parse tree later.
         */
        Status = AcpiNsLookup (WalkState->ScopeInfo, BufferPtr,
                                DataType, IMODE_EXECUTE,
                                NS_NO_UPSEARCH, WalkState,
                                &(Node));
    }

    if (ACPI_SUCCESS (Status))
    {
        if (!Op)
        {
            /* Create a new op */

            Op = AcpiPsAllocOp (Opcode);
            if (!Op)
            {
                return (AE_NO_MEMORY);
            }

            /* Initialize */

            ((ACPI_PARSE2_OBJECT *)Op)->Name = Node->Name;
            *OutOp = Op;
        }


        /*
         * Put the Node in the "op" object that the parser uses, so we
         * can get it again quickly when this scope is closed
         */
        Op->Node = Node;

        if (Original)
        {
            DEBUG_PRINT (ACPI_INFO,
                ("Lookup: old %p new %p\n", Original, Node));

            if (Original != Node)
            {
                DEBUG_PRINT (ACPI_INFO,
                    ("Lookup match error: old %p new %p\n", Original, Node));
            }
        }
    }


    return (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsLoad2EndOp
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *              Op              - Op that has been just been completed in the
 *                                walk;  Arguments have now been evaluated.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback used during the loading of the namespace,
 *              both control methods and everything else.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsLoad2EndOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_STATUS             Status = AE_OK;
    OBJECT_TYPE_INTERNAL    DataType;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_PARSE_OBJECT       *Arg;
    ACPI_NAMESPACE_NODE     *NewNode;


    DEBUG_PRINT (TRACE_DISPATCH, ("Load2EndOp: Op=%p State=%p\n", Op, WalkState));

    if (!AcpiPsIsNamespaceObjectOp (Op->Opcode))
    {
        return (AE_OK);
    }

    if (Op->Opcode == AML_SCOPE_OP)
    {
        DEBUG_PRINT (TRACE_DISPATCH,
            ("Load2EndOp: ending scope Op=%p State=%p\n", Op, WalkState));

        if (((ACPI_PARSE2_OBJECT *)Op)->Name == -1)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("Load2EndOp: Un-named scope! Op=%p State=%p\n", Op,
                WalkState));
            return (AE_OK);
        }
    }


    DataType = AcpiDsMapNamedOpcodeToDataType (Op->Opcode);

    /*
     * Get the Node/name from the earlier lookup
     * (It was saved in the *op structure)
     */
    Node = Op->Node;

    /*
     * Put the Node on the object stack (Contains the ACPI Name of
     * this object)
     */

    WalkState->Operands[0] = (void *) Node;
    WalkState->NumOperands = 1;

    /* Pop the scope stack */

    if (AcpiNsOpensScope (DataType))
    {

        DEBUG_PRINT (TRACE_DISPATCH,
            ("AmlEndNamespaceScope/%s: Popping scope for Op %p\n",
            AcpiCmGetTypeName (DataType), Op));
        AcpiDsScopeStackPop (WalkState);
    }


    /*
     * Named operations are as follows:
     *
     * AML_SCOPE
     * AML_DEVICE
     * AML_THERMALZONE
     * AML_METHOD
     * AML_POWERRES
     * AML_PROCESSOR
     * AML_FIELD
     * AML_INDEXFIELD
     * AML_BANKFIELD
     * AML_NAMEDFIELD
     * AML_NAME
     * AML_ALIAS
     * AML_MUTEX
     * AML_EVENT
     * AML_OPREGION
     * AML_CREATEFIELD
     * AML_CREATEBITFIELD
     * AML_CREATEBYTEFIELD
     * AML_CREATEWORDFIELD
     * AML_CREATEDWORDFIELD
     * AML_METHODCALL
     */


    /* Decode the opcode */

    Arg = Op->Value.Arg;

    switch (Op->Opcode)
    {

    case AML_CREATE_FIELD_OP:
    case AML_BIT_FIELD_OP:
    case AML_BYTE_FIELD_OP:
    case AML_WORD_FIELD_OP:
    case AML_DWORD_FIELD_OP:

        /*
         * Create the field object, but the field buffer and index must
         * be evaluated later during the execution phase
         */

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-CreateXxxField: State=%p Op=%p NamedObj=%p\n",
            WalkState, Op, Node));

        /* Get the NameString argument */

        if (Op->Opcode == AML_CREATE_FIELD_OP)
        {
            Arg = AcpiPsGetArg (Op, 3);
        }
        else
        {
            /* Create Bit/Byte/Word/Dword field */

            Arg = AcpiPsGetArg (Op, 2);
        }

        /*
         * Enter the NameString into the namespace
         */

        Status = AcpiNsLookup (WalkState->ScopeInfo,
                                Arg->Value.String,
                                INTERNAL_TYPE_DEF_ANY,
                                IMODE_LOAD_PASS1,
                                NS_NO_UPSEARCH | NS_DONT_OPEN_SCOPE,
                                WalkState, &(NewNode));

        if (ACPI_SUCCESS (Status))
        {
            /* We could put the returned object (Node) on the object stack for later, but
             * for now, we will put it in the "op" object that the parser uses, so we
             * can get it again at the end of this scope
             */
            Op->Node = NewNode;

            /*
             * If there is no object attached to the node, this node was just created and
             * we need to create the field object.  Otherwise, this was a lookup of an
             * existing node and we don't want to create the field object again.
             */
            if (!NewNode->Object)
            {
                /*
                 * The Field definition is not fully parsed at this time.
                 * (We must save the address of the AML for the buffer and index operands)
                 */
                Status = AcpiAmlExecCreateField (((ACPI_PARSE2_OBJECT *) Op)->Data,
                                                ((ACPI_PARSE2_OBJECT *) Op)->Length,
                                                NewNode, WalkState);
            }
        }


        break;


    case AML_METHODCALL_OP:

        DEBUG_PRINT (TRACE_DISPATCH,
            ("RESOLVING-MethodCall: State=%p Op=%p NamedObj=%p\n",
            WalkState, Op, Node));

        /*
         * Lookup the method name and save the Node
         */

        Status = AcpiNsLookup (WalkState->ScopeInfo, Arg->Value.String,
                                ACPI_TYPE_ANY, IMODE_LOAD_PASS2,
                                NS_SEARCH_PARENT | NS_DONT_OPEN_SCOPE,
                                WalkState, &(NewNode));

        if (ACPI_SUCCESS (Status))
        {

/* has name already been resolved by here ??*/

            /* TBD: [Restructure] Make sure that what we found is indeed a method! */
            /* We didn't search for a method on purpose, to see if the name would resolve! */

            /* We could put the returned object (Node) on the object stack for later, but
             * for now, we will put it in the "op" object that the parser uses, so we
             * can get it again at the end of this scope
             */
            Op->Node = NewNode;
        }


         break;


    case AML_PROCESSOR_OP:

        /* Nothing to do other than enter object into namespace */

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-Processor: State=%p Op=%p NamedObj=%p\n",
            WalkState, Op, Node));

        Status = AcpiAmlExecCreateProcessor (Op, (ACPI_HANDLE) Node);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        DEBUG_PRINT (TRACE_DISPATCH,
            ("Completed Processor Init, Op=%p State=%p entry=%p\n",
            Op, WalkState, Node));
        break;


    case AML_POWER_RES_OP:

        /* Nothing to do other than enter object into namespace */

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-PowerResource: State=%p Op=%p NamedObj=%p\n",
            WalkState, Op, Node));

        Status = AcpiAmlExecCreatePowerResource (Op, (ACPI_HANDLE) Node);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        DEBUG_PRINT (TRACE_DISPATCH,
            ("Completed PowerResource Init, Op=%p State=%p entry=%p\n",
            Op, WalkState, Node));
        break;


    case AML_THERMAL_ZONE_OP:

        /* Nothing to do other than enter object into namespace */

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-ThermalZone: State=%p Op=%p NamedObj=%p\n",
            WalkState, Op, Node));
        break;


    case AML_DEF_FIELD_OP:

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-Field: State=%p Op=%p NamedObj=%p\n",
            WalkState, Op, Node));

        Arg = Op->Value.Arg;

        Status = AcpiDsCreateField (Op,
                                    Arg->Node,
                                    WalkState);
        break;


    case AML_INDEX_FIELD_OP:

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-IndexField: State=%p Op=%p NamedObj=%p\n",
            WalkState, Op, Node));

        Arg = Op->Value.Arg;

        Status = AcpiDsCreateIndexField (Op,
                                        (ACPI_HANDLE) Arg->Node,
                                        WalkState);
        break;


    case AML_BANK_FIELD_OP:

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-BankField: State=%p Op=%p NamedObj=%p\n",
            WalkState, Op, Node));

        Arg = Op->Value.Arg;
        Status = AcpiDsCreateBankField (Op,
                                        Arg->Node,
                                        WalkState);
        break;


    /*
     * MethodOp PkgLength NamesString MethodFlags TermList
     */
    case AML_METHOD_OP:

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-Method: State=%p Op=%p NamedObj=%p\n",
            WalkState, Op, Node));

        if (!Node->Object)
        {
            Status = AcpiAmlExecCreateMethod (((ACPI_PARSE2_OBJECT *) Op)->Data,
                                ((ACPI_PARSE2_OBJECT *) Op)->Length,
                                Arg->Value.Integer, (ACPI_HANDLE) Node);
        }

        break;


    case AML_MUTEX_OP:

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-Mutex: Op=%p State=%p\n", Op, WalkState));

        Status = AcpiDsCreateOperands (WalkState, Arg);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        Status = AcpiAmlExecCreateMutex (WalkState);
        break;


    case AML_EVENT_OP:

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-Event: Op=%p State=%p\n", Op, WalkState));

        Status = AcpiDsCreateOperands (WalkState, Arg);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        Status = AcpiAmlExecCreateEvent (WalkState);
        break;


    case AML_REGION_OP:

        if (Node->Object)
        {
            break;
        }

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-Opregion: Op=%p State=%p NamedObj=%p\n", Op, WalkState, Node));


        /*
         * The OpRegion is not fully parsed at this time.  Only valid argument is the SpaceId.
         * (We must save the address of the AML of the address and length operands)
         */

        Status = AcpiAmlExecCreateRegion (((ACPI_PARSE2_OBJECT *) Op)->Data,
                                        ((ACPI_PARSE2_OBJECT *) Op)->Length,
                                        Arg->Value.Integer, WalkState);

        DEBUG_PRINT (TRACE_DISPATCH,
            ("Completed OpRegion Init, Op=%p State=%p entry=%p\n",
            Op, WalkState, Node));
        break;


    /* Namespace Modifier Opcodes */

    case AML_ALIAS_OP:

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-Alias: Op=%p State=%p\n", Op, WalkState));

        Status = AcpiDsCreateOperands (WalkState, Arg);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        Status = AcpiAmlExecCreateAlias (WalkState);
        break;


    case AML_NAME_OP:

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-Name: Op=%p State=%p\n", Op, WalkState));

        /*
         * Because of the execution pass through the non-control-method
         * parts of the table, we can arrive here twice.  Only init
         * the named object node the first time through
         */

        if (!Node->Object)
        {
            Status = AcpiDsCreateNode (WalkState, Node, Op);
        }

        break;


    case AML_NAMEPATH_OP:

        DEBUG_PRINT (TRACE_DISPATCH,
            ("LOADING-NamePath object: State=%p Op=%p NamedObj=%p\n",
            WalkState, Op, Node));
        break;


    default:
        break;
    }


Cleanup:

    /* Remove the Node pushed at the very beginning */

    AcpiDsObjStackPop (1, WalkState);
    return (Status);
}


