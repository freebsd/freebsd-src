/******************************************************************************
 *
 * Module Name: evregion - ACPI AddressSpace (OpRegion) handler dispatch
 *              $Revision: 103 $
 *
 *****************************************************************************/

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


#define __EVREGION_C__

#include "acpi.h"
#include "acevents.h"
#include "acnamesp.h"
#include "acinterp.h"
#include "amlcode.h"

#define _COMPONENT          ACPI_EVENTS
        MODULE_NAME         ("evregion")


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvInstallDefaultAddressSpaceHandlers
 *
 * PARAMETERS:
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Installs the core subsystem address space handlers.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvInstallDefaultAddressSpaceHandlers (
    void)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("EvInstallDefaultAddressSpaceHandlers");

    /*
     * All address spaces (PCI Config, EC, SMBus) are scope dependent
     * and registration must occur for a specific device.  In the case
     * system memory and IO address spaces there is currently no device
     * associated with the address space.  For these we use the root.
     * We install the default PCI config space handler at the root so
     * that this space is immediately available even though the we have
     * not enumerated all the PCI Root Buses yet.  This is to conform
     * to the ACPI specification which states that the PCI config
     * space must be always available -- even though we are nowhere
     * near ready to find the PCI root buses at this point.
     *
     * NOTE: We ignore AE_EXIST because this means that a handler has
     * already been installed (via AcpiInstallAddressSpaceHandler)
     */

    Status = AcpiInstallAddressSpaceHandler (AcpiGbl_RootNode,
                                             ACPI_ADR_SPACE_SYSTEM_MEMORY,
                                             ACPI_DEFAULT_HANDLER, NULL, NULL);
    if ((ACPI_FAILURE (Status)) &&
        (Status != AE_EXIST))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiInstallAddressSpaceHandler (AcpiGbl_RootNode,
                                             ACPI_ADR_SPACE_SYSTEM_IO,
                                             ACPI_DEFAULT_HANDLER, NULL, NULL);
    if ((ACPI_FAILURE (Status)) &&
        (Status != AE_EXIST))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiInstallAddressSpaceHandler (AcpiGbl_RootNode,
                                             ACPI_ADR_SPACE_PCI_CONFIG,
                                             ACPI_DEFAULT_HANDLER, NULL, NULL);
    if ((ACPI_FAILURE (Status)) &&
        (Status != AE_EXIST))
    {
        return_ACPI_STATUS (Status);
    }


    return_ACPI_STATUS (AE_OK);
}


/* TBD: [Restructure] Move elsewhere */

/*******************************************************************************
 *
 * FUNCTION:    AcpiEvExecuteRegMethod
 *
 * PARAMETERS:  RegionObj           - Object structure
 *              Function            - On (1) or Off (0)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute _REG method for a region
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiEvExecuteRegMethod (
    ACPI_OPERAND_OBJECT    *RegionObj,
    UINT32                  Function)
{
    ACPI_OPERAND_OBJECT    *Params[3];
    ACPI_OPERAND_OBJECT     SpaceIdDesc;
    ACPI_OPERAND_OBJECT     FunctionDesc;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("EvExecuteRegMethod");


    if (RegionObj->Region.Extra->Extra.Method_REG == NULL)
    {
        return_ACPI_STATUS (AE_OK);
    }

    /*
     *  _REG method has two arguments
     *  Arg0:   Integer: Operation region space ID
     *          Same value as RegionObj->Region.SpaceId
     *  Arg1:   Integer: connection status
     *          1 for connecting the handler,
     *          0 for disconnecting the handler
     *          Passed as a parameter
     */

    AcpiUtInitStaticObject (&SpaceIdDesc);
    AcpiUtInitStaticObject (&FunctionDesc);

    /*
     *  Method requires two parameters.
     */
    Params [0] = &SpaceIdDesc;
    Params [1] = &FunctionDesc;
    Params [2] = NULL;

    /*
     *  Set up the parameter objects
     */
    SpaceIdDesc.Common.Type    = ACPI_TYPE_INTEGER;
    SpaceIdDesc.Integer.Value  = RegionObj->Region.SpaceId;

    FunctionDesc.Common.Type   = ACPI_TYPE_INTEGER;
    FunctionDesc.Integer.Value = Function;

    /*
     *  Execute the method, no return value
     */
    DEBUG_EXEC(AcpiUtDisplayInitPathname (RegionObj->Region.Extra->Extra.Method_REG, "  [Method]"));
    Status = AcpiNsEvaluateByHandle (RegionObj->Region.Extra->Extra.Method_REG, Params, NULL);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvAddressSpaceDispatch
 *
 * PARAMETERS:  RegionObj           - internal region object
 *              SpaceId             - ID of the address space (0-255)
 *              Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              BitWidth            - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Dispatch an address space or operation region access to
 *              a previously installed handler.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvAddressSpaceDispatch (
    ACPI_OPERAND_OBJECT     *RegionObj,
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    UINT32                  *Value)
{
    ACPI_STATUS             Status;
    ACPI_ADR_SPACE_HANDLER  Handler;
    ACPI_ADR_SPACE_SETUP    RegionSetup;
    ACPI_OPERAND_OBJECT     *HandlerDesc;
    void                    *RegionContext = NULL;


    FUNCTION_TRACE ("EvAddressSpaceDispatch");


    /*
     * Ensure that there is a handler associated with this region
     */
    HandlerDesc = RegionObj->Region.AddrHandler;
    if (!HandlerDesc)
    {
        DEBUG_PRINTP (ACPI_ERROR, ("no handler for region(%p) [%s]\n",
            RegionObj, AcpiUtGetRegionName (RegionObj->Region.SpaceId)));

        return_ACPI_STATUS(AE_NOT_EXIST);
    }

    /*
     * It may be the case that the region has never been initialized
     * Some types of regions require special init code
     */
    if (!(RegionObj->Region.Flags & AOPOBJ_INITIALIZED))
    {
        /*
         * This region has not been initialized yet, do it
         */
        RegionSetup = HandlerDesc->AddrHandler.Setup;
        if (!RegionSetup)
        {
            /*
             *  Bad news, no init routine and not init'd
             */
            DEBUG_PRINTP (ACPI_ERROR, ("No init routine for region(%p) [%s]\n",
                RegionObj, AcpiUtGetRegionName (RegionObj->Region.SpaceId)));
            return_ACPI_STATUS (AE_UNKNOWN_STATUS);
        }

        /*
         * We must exit the interpreter because the region setup will potentially
         * execute control methods
         */
        AcpiExExitInterpreter ();

        Status = RegionSetup (RegionObj, ACPI_REGION_ACTIVATE,
                        HandlerDesc->AddrHandler.Context, &RegionContext);

        /* Re-enter the interpreter */

        AcpiExEnterInterpreter ();

        /*
         *  Init routine may fail
         */
        if (ACPI_FAILURE (Status))
        {
            DEBUG_PRINTP (ACPI_ERROR, ("Region Init: %s [%s]\n",
                AcpiUtFormatException (Status),
                AcpiUtGetRegionName (RegionObj->Region.SpaceId)));
            return_ACPI_STATUS(Status);
        }

        RegionObj->Region.Flags |= AOPOBJ_INITIALIZED;

        /*
         *  Save the returned context for use in all accesses to
         *  this particular region.
         */
        RegionObj->Region.Extra->Extra.RegionContext = RegionContext;
    }

    /*
     *  We have everything we need, begin the process
     */
    Handler = HandlerDesc->AddrHandler.Handler;

    DEBUG_PRINTP ((TRACE_OPREGION | VERBOSE_INFO),
        ("Addrhandler %p (%p), Address %p\n",
        &RegionObj->Region.AddrHandler->AddrHandler, Handler, Address));

    if (!(HandlerDesc->AddrHandler.Flags & ADDR_HANDLER_DEFAULT_INSTALLED))
    {
        /*
         *  For handlers other than the default (supplied) handlers, we must
         *  exit the interpreter because the handler *might* block -- we don't
         *  know what it will do, so we can't hold the lock on the intepreter.
         */
        AcpiExExitInterpreter();
    }

    /*
     *  Invoke the handler.
     */
    Status = Handler (Function, Address, BitWidth, Value,
                      HandlerDesc->AddrHandler.Context,
                      RegionObj->Region.Extra->Extra.RegionContext);

    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINTP (ACPI_ERROR, ("Region handler: %s [%s]\n",
            AcpiUtFormatException (Status),
            AcpiUtGetRegionName (RegionObj->Region.SpaceId)));
    }

    if (!(HandlerDesc->AddrHandler.Flags & ADDR_HANDLER_DEFAULT_INSTALLED))
    {
        /* We just returned from a non-default handler, we must re-enter the
        interpreter */

        AcpiExEnterInterpreter ();
    }

    return_ACPI_STATUS (Status);
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiEvDisassociateRegionFromHandler
 *
 * PARAMETERS:  RegionObj       - Region Object
 *              AcpiNsIsLocked  - Namespace Region Already Locked?
 *
 * RETURN:      None
 *
 * DESCRIPTION: Break the association between the handler and the region
 *              this is a two way association.
 *
 ******************************************************************************/

void
AcpiEvDisassociateRegionFromHandler(
    ACPI_OPERAND_OBJECT     *RegionObj,
    BOOLEAN                 AcpiNsIsLocked)
{
    ACPI_OPERAND_OBJECT     *HandlerObj;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     **LastObjPtr;
    ACPI_ADR_SPACE_SETUP    RegionSetup;
    void                    *RegionContext;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("EvDisassociateRegionFromHandler");

    RegionContext = RegionObj->Region.Extra->Extra.RegionContext;

    /*
     *  Get the address handler from the region object
     */

    HandlerObj = RegionObj->Region.AddrHandler;
    if (!HandlerObj)
    {
        /*
         *  This region has no handler, all done
         */
        return_VOID;
    }


    /*
     *  Find this region in the handler's list
     */

    ObjDesc = HandlerObj->AddrHandler.RegionList;
    LastObjPtr = &HandlerObj->AddrHandler.RegionList;

    while (ObjDesc)
    {
        /*
         *  See if this is the one
         */
        if (ObjDesc == RegionObj)
        {
            DEBUG_PRINTP (TRACE_OPREGION,
                ("Removing Region %p from address handler %p\n",
                RegionObj, HandlerObj));
            /*
             *  This is it, remove it from the handler's list
             */
            *LastObjPtr = ObjDesc->Region.Next;
            ObjDesc->Region.Next = NULL;            /* Must clear field */

            if (AcpiNsIsLocked)
            {
                AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
            }

            /*
             *  Now stop region accesses by executing the _REG method
             */
            AcpiEvExecuteRegMethod (RegionObj, 0);

            if (AcpiNsIsLocked)
            {
                AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
            }

            /*
             *  Call the setup handler with the deactivate notification
             */
            RegionSetup = HandlerObj->AddrHandler.Setup;
            Status = RegionSetup (RegionObj, ACPI_REGION_DEACTIVATE,
                            HandlerObj->AddrHandler.Context, &RegionContext);

            /*
             *  Init routine may fail, Just ignore errors
             */
            if (ACPI_FAILURE (Status))
            {
                DEBUG_PRINTP (ACPI_ERROR, ("%s from region init, [%s]\n",
                    AcpiUtFormatException (Status),
                    AcpiUtGetRegionName (RegionObj->Region.SpaceId)));
            }

            RegionObj->Region.Flags &= ~(AOPOBJ_INITIALIZED);

            /*
             *  Remove handler reference in the region
             *
             *  NOTE: this doesn't mean that the region goes away
             *  The region is just inaccessible as indicated to
             *  the _REG method
             *
             *  If the region is on the handler's list
             *  this better be the region's handler
             */
            ACPI_ASSERT (RegionObj->Region.AddrHandler == HandlerObj);

            RegionObj->Region.AddrHandler = NULL;

            return_VOID;

        } /* found the right handler */

        /*
         *  Move through the linked list of handlers
         */
        LastObjPtr = &ObjDesc->Region.Next;
        ObjDesc = ObjDesc->Region.Next;
    }

    /*
     *  If we get here, the region was not in the handler's region list
     */
    DEBUG_PRINTP (TRACE_OPREGION,
        ("Cannot remove region %p from address handler %p\n",
        RegionObj, HandlerObj));

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvAssociateRegionAndHandler
 *
 * PARAMETERS:  HandlerObj      - Handler Object
 *              RegionObj       - Region Object
 *              AcpiNsIsLocked  - Namespace Region Already Locked?
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create the association between the handler and the region
 *              this is a two way association.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvAssociateRegionAndHandler (
    ACPI_OPERAND_OBJECT     *HandlerObj,
    ACPI_OPERAND_OBJECT     *RegionObj,
    BOOLEAN                 AcpiNsIsLocked)
{
    ACPI_STATUS     Status;


    FUNCTION_TRACE ("EvAssociateRegionAndHandler");


    DEBUG_PRINTP (TRACE_OPREGION, 
        ("Adding Region %p to address handler %p [%s]\n",
        RegionObj, HandlerObj, AcpiUtGetRegionName (RegionObj->Region.SpaceId)));

    ACPI_ASSERT (RegionObj->Region.SpaceId == HandlerObj->AddrHandler.SpaceId);
    ACPI_ASSERT (RegionObj->Region.AddrHandler == 0);

    /*
     *  Link this region to the front of the handler's list
     */

    RegionObj->Region.Next = HandlerObj->AddrHandler.RegionList;
    HandlerObj->AddrHandler.RegionList = RegionObj;

    /*
     *  set the region's handler
     */

/*
    HandlerObj->Common.ReferenceCount =
                                (UINT16) (HandlerObj->Common.ReferenceCount +
                                RegionObj->Common.ReferenceCount - 1);
*/
    RegionObj->Region.AddrHandler = HandlerObj;

    /*
     *  Last thing, tell all users that this region is usable
     */
    if (AcpiNsIsLocked)
    {
        AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    }

    Status = AcpiEvExecuteRegMethod (RegionObj, 1);

    if (AcpiNsIsLocked)
    {
        AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvAddrHandlerHelper
 *
 * PARAMETERS:  Handle              - Node to be dumped
 *              Level               - Nesting level of the handle
 *              Context             - Passed into AcpiNsWalkNamespace
 *
 * DESCRIPTION: This routine checks to see if the object is a Region if it
 *              is then the address handler is installed in it.
 *
 *              If the Object is a Device, and the device has a handler of
 *              the same type then the search is terminated in that branch.
 *
 *              This is because the existing handler is closer in proximity
 *              to any more regions than the one we are trying to install.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvAddrHandlerHelper (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_OPERAND_OBJECT     *HandlerObj;
    ACPI_OPERAND_OBJECT     *TmpObj;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    PROC_NAME ("EvAddrHandlerHelper");


    HandlerObj = (ACPI_OPERAND_OBJECT  *) Context;

    /* Parameter validation */

    if (!HandlerObj)
    {
        return (AE_OK);
    }

    /* Convert and validate the device handle */

    Node = AcpiNsConvertHandleToEntry (ObjHandle);
    if (!Node)
    {
        return (AE_BAD_PARAMETER);
    }

    /*
     *  We only care about regions.and objects
     *  that can have address handlers
     */

    if ((Node->Type != ACPI_TYPE_DEVICE) &&
        (Node->Type != ACPI_TYPE_REGION) &&
        (Node != AcpiGbl_RootNode))
    {
        return (AE_OK);
    }

    /* Check for an existing internal object */

    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (!ObjDesc)
    {
        /*
         *  The object DNE, we don't care about it
         */
        return (AE_OK);
    }

    /*
     *  Devices are handled different than regions
     */
    if (IS_THIS_OBJECT_TYPE (ObjDesc, ACPI_TYPE_DEVICE))
    {
        /*
         *  See if this guy has any handlers
         */
        TmpObj = ObjDesc->Device.AddrHandler;
        while (TmpObj)
        {
            /*
             *  Now let's see if it's for the same address space.
             */
            if (TmpObj->AddrHandler.SpaceId == HandlerObj->AddrHandler.SpaceId)
            {
                /*
                 *  It's for the same address space
                 */
                DEBUG_PRINTP (TRACE_OPREGION,
                    ("Found handler for region [%s] in device %p(%p) handler %p\n",
                    AcpiUtGetRegionName (HandlerObj->AddrHandler.SpaceId),
                    ObjDesc, TmpObj, HandlerObj));

                /*
                 *  Since the object we found it on was a device, then it
                 *  means that someone has already installed a handler for
                 *  the branch of the namespace from this device on.  Just
                 *  bail out telling the walk routine to not traverse this
                 *  branch.  This preserves the scoping rule for handlers.
                 */
                return (AE_CTRL_DEPTH);
            }

            /*
             *  Move through the linked list of handlers
             */
            TmpObj = TmpObj->AddrHandler.Next;
        }

        /*
         *  As long as the device didn't have a handler for this
         *  space we don't care about it.  We just ignore it and
         *  proceed.
         */
        return (AE_OK);
    }

    /*
     *  Only here if it was a region
     */
    ACPI_ASSERT (ObjDesc->Common.Type == ACPI_TYPE_REGION);

    if (ObjDesc->Region.SpaceId != HandlerObj->AddrHandler.SpaceId)
    {
        /*
         *  This region is for a different address space
         *  ignore it
         */
        return (AE_OK);
    }

    /*
     *  Now we have a region and it is for the handler's address
     *  space type.
     *
     *  First disconnect region for any previous handler (if any)
     */
    AcpiEvDisassociateRegionFromHandler (ObjDesc, FALSE);

    /*
     *  Then connect the region to the new handler
     */
    Status = AcpiEvAssociateRegionAndHandler (HandlerObj, ObjDesc, FALSE);

    return (Status);
}


