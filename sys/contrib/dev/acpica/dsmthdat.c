/*******************************************************************************
 *
 * Module Name: dsmthdat - control method arguments and local variables
 *              $Revision: 46 $
 *
 ******************************************************************************/

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

#define __DSMTHDAT_C__

#include "acpi.h"
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_DISPATCHER
        MODULE_NAME         ("dsmthdat")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsMethodDataInit
 *
 * PARAMETERS:  WalkState           - Current walk state object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize the data structures that hold the method's arguments
 *              and locals.  The data struct is an array of NTEs for each.
 *              This allows RefOf and DeRefOf to work properly for these
 *              special data types.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsMethodDataInit (
    ACPI_WALK_STATE         *WalkState)
{
    UINT32                  i;


    FUNCTION_TRACE ("DsMethodDataInit");

    /*
     * WalkState fields are initialized to zero by the
     * AcpiUtCallocate().
     *
     * An Node is assigned to each argument and local so
     * that RefOf() can return a pointer to the Node.
     */

    /* Init the method arguments */

    for (i = 0; i < MTH_NUM_ARGS; i++)
    {
        MOVE_UNALIGNED32_TO_32 (&WalkState->Arguments[i].Name,
                                NAMEOF_ARG_NTE);
        WalkState->Arguments[i].Name       |= (i << 24);
        WalkState->Arguments[i].DataType    = ACPI_DESC_TYPE_NAMED;
        WalkState->Arguments[i].Type        = ACPI_TYPE_ANY;
        WalkState->Arguments[i].Flags       = ANOBJ_END_OF_PEER_LIST | ANOBJ_METHOD_ARG;
    }

    /* Init the method locals */

    for (i = 0; i < MTH_NUM_LOCALS; i++)
    {
        MOVE_UNALIGNED32_TO_32 (&WalkState->LocalVariables[i].Name,
                                NAMEOF_LOCAL_NTE);

        WalkState->LocalVariables[i].Name    |= (i << 24);
        WalkState->LocalVariables[i].DataType = ACPI_DESC_TYPE_NAMED;
        WalkState->LocalVariables[i].Type     = ACPI_TYPE_ANY;
        WalkState->LocalVariables[i].Flags    = ANOBJ_END_OF_PEER_LIST | ANOBJ_METHOD_LOCAL;
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsMethodDataDeleteAll
 *
 * PARAMETERS:  WalkState           - Current walk state object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete method locals and arguments.  Arguments are only
 *              deleted if this method was called from another method.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsMethodDataDeleteAll (
    ACPI_WALK_STATE         *WalkState)
{
    UINT32                  Index;
    ACPI_OPERAND_OBJECT     *Object;


    FUNCTION_TRACE ("DsMethodDataDeleteAll");


    /* Delete the locals */

    DEBUG_PRINTP (ACPI_INFO, ("Deleting local variables in %p\n", WalkState));

    for (Index = 0; Index < MTH_NUM_LOCALS; Index++)
    {
        Object = WalkState->LocalVariables[Index].Object;
        if (Object)
        {
            DEBUG_PRINTP (TRACE_EXEC, ("Deleting Local%d=%p\n", Index, Object));

            /* Remove first */

            WalkState->LocalVariables[Index].Object = NULL;

            /* Was given a ref when stored */

            AcpiUtRemoveReference (Object);
       }
    }


    /* Delete the arguments */

    DEBUG_PRINTP (ACPI_INFO, ("Deleting arguments in %p\n", WalkState));

    for (Index = 0; Index < MTH_NUM_ARGS; Index++)
    {
        Object = WalkState->Arguments[Index].Object;
        if (Object)
        {
            DEBUG_PRINTP (TRACE_EXEC, ("Deleting Arg%d=%p\n", Index, Object));

            /* Remove first */

            WalkState->Arguments[Index].Object = NULL;

             /* Was given a ref when stored */

            AcpiUtRemoveReference (Object);
        }
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsMethodDataInitArgs
 *
 * PARAMETERS:  *Params         - Pointer to a parameter list for the method
 *              MaxParamCount   - The arg count for this method
 *              WalkState       - Current walk state object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize arguments for a method
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsMethodDataInitArgs (
    ACPI_OPERAND_OBJECT     **Params,
    UINT32                  MaxParamCount,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    UINT32                  Mindex;
    UINT32                  Pindex;


    FUNCTION_TRACE_PTR ("DsMethodDataInitArgs", Params);


    if (!Params)
    {
        DEBUG_PRINTP (TRACE_EXEC, ("No param list passed to method\n"));
        return_ACPI_STATUS (AE_OK);
    }

    /* Copy passed parameters into the new method stack frame  */

    for (Pindex = Mindex = 0;
        (Mindex < MTH_NUM_ARGS) && (Pindex < MaxParamCount);
        Mindex++)
    {
        if (Params[Pindex])
        {
            /*
             * A valid parameter.
             * Set the current method argument to the
             * Params[Pindex++] argument object descriptor
             */
            Status = AcpiDsStoreObjectToLocal (AML_ARG_OP, Mindex,
                            Params[Pindex], WalkState);
            if (ACPI_FAILURE (Status))
            {
                break;
            }

            Pindex++;
        }

        else
        {
            break;
        }
    }

    DEBUG_PRINTP (TRACE_EXEC, ("%d args passed to method\n", Pindex));
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsMethodDataGetEntry
 *
 * PARAMETERS:  Opcode              - Either AML_LOCAL_OP or AML_ARG_OP
 *              Index               - Which localVar or argument to get
 *              Entry               - Pointer to where a pointer to the stack
 *                                    entry is returned.
 *              WalkState           - Current walk state object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get the address of the object entry given by Opcode:Index
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsMethodDataGetEntry (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     ***Entry)
{

    FUNCTION_TRACE_U32 ("DsMethodDataGetEntry", Index);


    /*
     * Get the requested object.
     * The stack "Opcode" is either a LocalVariable or an Argument
     */

    switch (Opcode)
    {

    case AML_LOCAL_OP:

        if (Index > MTH_MAX_LOCAL)
        {
            DEBUG_PRINTP (ACPI_ERROR, ("LocalVar index %d is invalid (max %d)\n",
                Index, MTH_MAX_LOCAL));
            return_ACPI_STATUS (AE_BAD_PARAMETER);
        }

        *Entry = (ACPI_OPERAND_OBJECT  **)
                    &WalkState->LocalVariables[Index].Object;
        break;


    case AML_ARG_OP:

        if (Index > MTH_MAX_ARG)
        {
            DEBUG_PRINTP (ACPI_ERROR, ("Arg index %d is invalid (max %d)\n",
                Index, MTH_MAX_ARG));
            return_ACPI_STATUS (AE_BAD_PARAMETER);
        }

        *Entry = (ACPI_OPERAND_OBJECT  **)
                    &WalkState->Arguments[Index].Object;
        break;


    default:
        DEBUG_PRINTP (ACPI_ERROR, ("Opcode %d is invalid\n", Opcode));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsMethodDataSetEntry
 *
 * PARAMETERS:  Opcode              - Either AML_LOCAL_OP or AML_ARG_OP
 *              Index               - Which localVar or argument to get
 *              Object              - Object to be inserted into the stack entry
 *              WalkState           - Current walk state object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Insert an object onto the method stack at entry Opcode:Index.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsMethodDataSetEntry (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_OPERAND_OBJECT     *Object,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     **Entry;


    FUNCTION_TRACE ("DsMethodDataSetEntry");

    /* Get a pointer to the stack entry to set */

    Status = AcpiDsMethodDataGetEntry (Opcode, Index, WalkState, &Entry);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Increment ref count so object can't be deleted while installed */

    AcpiUtAddReference (Object);

    /* Install the object into the stack entry */

    *Entry = Object;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsMethodDataGetType
 *
 * PARAMETERS:  Opcode              - Either AML_LOCAL_OP or AML_ARG_OP
 *              Index               - Which localVar or argument whose type
 *                                      to get
 *              WalkState           - Current walk state object
 *
 * RETURN:      Data type of selected Arg or Local
 *              Used only in ExecMonadic2()/TypeOp.
 *
 ******************************************************************************/

ACPI_OBJECT_TYPE8
AcpiDsMethodDataGetType (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     **Entry;
    ACPI_OPERAND_OBJECT     *Object;


    FUNCTION_TRACE ("DsMethodDataGetType");


    /* Get a pointer to the requested stack entry */

    Status = AcpiDsMethodDataGetEntry (Opcode, Index, WalkState, &Entry);
    if (ACPI_FAILURE (Status))
    {
        return_VALUE ((ACPI_TYPE_NOT_FOUND));
    }

    /* Get the object from the method stack */

    Object = *Entry;

    /* Get the object type */

    if (!Object)
    {
        /* Any == 0 => "uninitialized" -- see spec 15.2.3.5.2.28 */
        return_VALUE (ACPI_TYPE_ANY);
    }

    return_VALUE (Object->Common.Type);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsMethodDataGetNode
 *
 * PARAMETERS:  Opcode              - Either AML_LOCAL_OP or AML_ARG_OP
 *              Index               - Which localVar or argument whose type
 *                                      to get
 *              WalkState           - Current walk state object
 *
 * RETURN:      Get the Node associated with a local or arg.
 *
 ******************************************************************************/

ACPI_NAMESPACE_NODE *
AcpiDsMethodDataGetNode (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_NAMESPACE_NODE     *Node = NULL;


    FUNCTION_TRACE ("DsMethodDataGetNode");


    switch (Opcode)
    {

    case AML_LOCAL_OP:

        if (Index > MTH_MAX_LOCAL)
        {
            DEBUG_PRINTP (ACPI_ERROR, ("Local index %d is invalid (max %d)\n",
                Index, MTH_MAX_LOCAL));
            return_PTR (Node);
        }

        Node =  &WalkState->LocalVariables[Index];
        break;


    case AML_ARG_OP:

        if (Index > MTH_MAX_ARG)
        {
            DEBUG_PRINTP (ACPI_ERROR, ("Arg index %d is invalid (max %d)\n",
                Index, MTH_MAX_ARG));
            return_PTR (Node);
        }

        Node = &WalkState->Arguments[Index];
        break;


    default:
        DEBUG_PRINTP (ACPI_ERROR, ("Opcode %d is invalid\n", Opcode));
        break;
    }


    return_PTR (Node);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsMethodDataGetValue
 *
 * PARAMETERS:  Opcode              - Either AML_LOCAL_OP or AML_ARG_OP
 *              Index               - Which localVar or argument to get
 *              WalkState           - Current walk state object
 *              *DestDesc           - Ptr to Descriptor into which selected Arg
 *                                    or Local value should be copied
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Retrieve value of selected Arg or Local from the method frame
 *              at the current top of the method stack.
 *              Used only in AcpiExResolveToValue().
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsMethodDataGetValue (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **DestDesc)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     **Entry;
    ACPI_OPERAND_OBJECT     *Object;


    FUNCTION_TRACE ("DsMethodDataGetValue");


    /* Validate the object descriptor */

    if (!DestDesc)
    {
        DEBUG_PRINTP (ACPI_ERROR, ("Null object descriptor pointer\n"));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    /* Get a pointer to the requested method stack entry */

    Status = AcpiDsMethodDataGetEntry (Opcode, Index, WalkState, &Entry);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Get the object from the method stack */

    Object = *Entry;


    /* Examine the returned object, it must be valid. */

    if (!Object)
    {
        /*
         * Index points to uninitialized object stack value.
         * This means that either 1) The expected argument was
         * not passed to the method, or 2) A local variable
         * was referenced by the method (via the ASL)
         * before it was initialized.  Either case is an error.
         */

        switch (Opcode)
        {
        case AML_ARG_OP:

            DEBUG_PRINTP (ACPI_ERROR, ("Uninitialized Arg[%d] at entry %p\n",
                Index, Entry));

            return_ACPI_STATUS (AE_AML_UNINITIALIZED_ARG);
            break;

        case AML_LOCAL_OP:

            DEBUG_PRINTP (ACPI_ERROR, ("Uninitialized Local[%d] at entry %p\n",
                Index, Entry));

            return_ACPI_STATUS (AE_AML_UNINITIALIZED_LOCAL);
            break;
        }
    }


    /*
     * Index points to initialized and valid object stack value.
     * Return an additional reference to the object
     */

    *DestDesc = Object;
    AcpiUtAddReference (Object);

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsMethodDataDeleteValue
 *
 * PARAMETERS:  Opcode              - Either AML_LOCAL_OP or AML_ARG_OP
 *              Index               - Which localVar or argument to delete
 *              WalkState           - Current walk state object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete the entry at Opcode:Index on the method stack.  Inserts
 *              a null into the stack slot after the object is deleted.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsMethodDataDeleteValue (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     **Entry;
    ACPI_OPERAND_OBJECT     *Object;


    FUNCTION_TRACE ("DsMethodDataDeleteValue");


    /* Get a pointer to the requested entry */

    Status = AcpiDsMethodDataGetEntry (Opcode, Index, WalkState, &Entry);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Get the current entry in this slot k */

    Object = *Entry;

    /*
     * Undefine the Arg or Local by setting its descriptor
     * pointer to NULL. Locals/Args can contain both
     * ACPI_OPERAND_OBJECTS and ACPI_NAMESPACE_NODEs
     */
    *Entry = NULL;


    if ((Object) &&
        (VALID_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_INTERNAL)))
    {
        /*
         * There is a valid object in this slot
         * Decrement the reference count by one to balance the
         * increment when the object was stored in the slot.
         */
        AcpiUtRemoveReference (Object);
    }


    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsStoreObjectToLocal
 *
 * PARAMETERS:  Opcode              - Either AML_LOCAL_OP or AML_ARG_OP
 *              Index               - Which localVar or argument to set
 *              SrcDesc             - Value to be stored
 *              WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Store a value in an Arg or Local.  The SrcDesc is installed
 *              as the new value for the Arg or Local and the reference count
 *              for SrcDesc is incremented.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsStoreObjectToLocal (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_OPERAND_OBJECT     *SrcDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     **Entry;


    FUNCTION_TRACE ("DsMethodDataSetValue");
    DEBUG_PRINTP (TRACE_EXEC, ("Opcode=%d Idx=%d Obj=%p\n",
        Opcode, Index, SrcDesc));


    /* Parameter validation */

    if (!SrcDesc)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    /* Get a pointer to the requested method stack entry */

    Status = AcpiDsMethodDataGetEntry (Opcode, Index, WalkState, &Entry);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    if (*Entry == SrcDesc)
    {
        DEBUG_PRINTP (TRACE_EXEC, ("Obj=%p already installed!\n", SrcDesc));
        goto Cleanup;
    }


    /*
     * If there is an object already in this slot, we either
     * have to delete it, or if this is an argument and there
     * is an object reference stored there, we have to do
     * an indirect store!
     */

    if (*Entry)
    {
        /*
         * Check for an indirect store if an argument
         * contains an object reference (stored as an Node).
         * We don't allow this automatic dereferencing for
         * locals, since a store to a local should overwrite
         * anything there, including an object reference.
         *
         * If both Arg0 and Local0 contain RefOf (Local4):
         *
         * Store (1, Arg0)             - Causes indirect store to local4
         * Store (1, Local0)           - Stores 1 in local0, overwriting
         *                                  the reference to local4
         * Store (1, DeRefof (Local0)) - Causes indirect store to local4
         *
         * Weird, but true.
         */

        if ((Opcode == AML_ARG_OP) &&
            (VALID_DESCRIPTOR_TYPE (*Entry, ACPI_DESC_TYPE_NAMED)))
        {
            DEBUG_PRINTP (TRACE_EXEC,
                ("Arg (%p) is an ObjRef(Node), storing in %p\n",
                SrcDesc, *Entry));

            /* Detach an existing object from the Node */

            AcpiNsDetachObject ((ACPI_NAMESPACE_NODE *) *Entry);

            /*
             * Store this object into the Node
             * (do the indirect store)
             */
            Status = AcpiNsAttachObject ((ACPI_NAMESPACE_NODE *) *Entry, SrcDesc,
                                            SrcDesc->Common.Type);
            return_ACPI_STATUS (Status);
        }


#ifdef ACPI_ENABLE_IMPLICIT_CONVERSION
        /*
         * Perform "Implicit conversion" of the new object to the type of the
         * existing object
         */
        Status = AcpiExConvertToTargetType ((*Entry)->Common.Type, &SrcDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }
#endif

        /*
         * Delete the existing object
         * before storing the new one
         */
        AcpiDsMethodDataDeleteValue (Opcode, Index, WalkState);
    }


    /*
     * Install the ObjStack descriptor (*SrcDesc) into
     * the descriptor for the Arg or Local.
     * Install the new object in the stack entry
     * (increments the object reference count by one)
     */
    Status = AcpiDsMethodDataSetEntry (Opcode, Index, SrcDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Normal exit */

    return_ACPI_STATUS (AE_OK);


    /* Error exit */

Cleanup:

    return_ACPI_STATUS (Status);
}

