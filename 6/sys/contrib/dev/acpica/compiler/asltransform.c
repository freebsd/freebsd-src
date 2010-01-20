
/******************************************************************************
 *
 * Module Name: asltransform - Parse tree transforms
 *              $Revision: 25 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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


#include "aslcompiler.h"
#include "aslcompiler.y.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asltransform")


/*******************************************************************************
 *
 * FUNCTION:    TrAmlGetNextTempName
 *
 * PARAMETERS:  NamePath        - Where a pointer to the temp name is returned
 *
 * RETURN:      A pointer to the second character of the name
 *
 * DESCRIPTION: Generate an ACPI name of the form _T_x.  These names are
 *              reserved for use by the ASL compiler.
 *
 ******************************************************************************/

char *
TrAmlGetNextTempName (
    char                    **NamePath)
{
    char                    *TempName;


    if (Gbl_TempCount > (9+26+26))
    {
        /* Too many temps */
        /* TBD: issue eror message */
        *NamePath = "ERROR";
        return ("Error");
    }

    TempName = UtLocalCalloc (6);

    if (Gbl_TempCount < 9)
    {
        TempName[4] = (char) (Gbl_TempCount + 0x30);
    }
    else if (Gbl_TempCount < (9 + 26))
    {
        TempName[4] = (char) (Gbl_TempCount + 0x41);
    }
    else
    {
        TempName[4] = (char) (Gbl_TempCount + 0x61);
    }
    Gbl_TempCount++;

    /* First four characters are always "\_T_" */

    TempName[0] = '\\';
    TempName[1] = '_';
    TempName[2] = 'T';
    TempName[3] = '_';

    *NamePath = TempName;
    return (&TempName[1]);
}


/*******************************************************************************
 *
 * FUNCTION:    TrAmlInitLineNumbers
 *
 * PARAMETERS:  Op            - Op to be initialized
 *              Neighbor        - Op used for initialization values
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialized the various line numbers for a parse node.
 *
 ******************************************************************************/

void
TrAmlInitLineNumbers (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *Neighbor)
{

    Op->Asl.EndLine           = Neighbor->Asl.EndLine;
    Op->Asl.EndLogicalLine    = Neighbor->Asl.EndLogicalLine;
    Op->Asl.LineNumber        = Neighbor->Asl.LineNumber;
    Op->Asl.LogicalByteOffset = Neighbor->Asl.LogicalByteOffset;
    Op->Asl.LogicalLineNumber = Neighbor->Asl.LogicalLineNumber;
}


/*******************************************************************************
 *
 * FUNCTION:    TrAmlInitNode
 *
 * PARAMETERS:  Op            - Op to be initialized
 *              ParseOpcode     - Opcode for this node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialize a node with the parse opcode and opcode name.
 *
 ******************************************************************************/

void
TrAmlInitNode (
    ACPI_PARSE_OBJECT       *Op,
    UINT16                  ParseOpcode)
{

    Op->Asl.ParseOpcode = ParseOpcode;
    UtSetParseOpName (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    TrAmlSetSubtreeParent
 *
 * PARAMETERS:  Op            - First node in a list of peer nodes
 *              Parent          - Parent of the subtree
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set the parent for all peer nodes in a subtree
 *
 ******************************************************************************/

void
TrAmlSetSubtreeParent (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *Parent)
{
    ACPI_PARSE_OBJECT       *Next;


    Next = Op;
    while (Next)
    {
        Next->Asl.Parent = Parent;
        Next             = Next->Asl.Next;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    TrAmlInsertPeer
 *
 * PARAMETERS:  Op            - First node in a list of peer nodes
 *              NewPeer         - Peer node to insert
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert a new peer node into a list of peers.
 *
 ******************************************************************************/

void
TrAmlInsertPeer (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *NewPeer)
{

    NewPeer->Asl.Next = Op->Asl.Next;
    Op->Asl.Next      = NewPeer;
}


/*******************************************************************************
 *
 * FUNCTION:    TrAmlTransformWalk
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      None
 *
 * DESCRIPTION: Parse tree walk to generate both the AML opcodes and the AML
 *              operands.
 *
 ******************************************************************************/

ACPI_STATUS
TrAmlTransformWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    TrTransformSubtree (Op);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    TrTransformSubtree
 *
 * PARAMETERS:  Op        - The parent parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prepare nodes to be output as AML data and operands.  The more
 *              complex AML opcodes require processing of the child nodes
 *              (arguments/operands).
 *
 ******************************************************************************/

void
TrTransformSubtree (
    ACPI_PARSE_OBJECT           *Op)
{

    if (Op->Asl.AmlOpcode == AML_RAW_DATA_BYTE)
    {
        return;
    }

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_DEFINITIONBLOCK:
        TrDoDefinitionBlock (Op);
        break;

    case PARSEOP_SWITCH:
        TrDoSwitch (Op);
        break;

    default:
        /* Nothing to do here for other opcodes */
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    TrDoDefinitionBlock
 *
 * PARAMETERS:  Op        - Parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Find the end of the definition block and set a global to this
 *              node.  It is used by the compiler to insert compiler-generated
 *              names at the root level of the namespace.
 *
 ******************************************************************************/

void
TrDoDefinitionBlock (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Next;
    UINT32                  i;


    Next = Op->Asl.Child;
    for (i = 0; i < 5; i++)
    {
        Next = Next->Asl.Next;
    }

    Gbl_FirstLevelInsertionNode = Next;
}


/*******************************************************************************
 *
 * FUNCTION:    TrDoSwitch
 *
 * PARAMETERS:  StartNode        - Parse node for SWITCH
 *
 * RETURN:      None
 *
 *
 * DESCRIPTION: Translate ASL SWITCH statement to if/else pairs.  There is
 *              no actual AML opcode for SWITCH -- it must be simulated.
 *
 ******************************************************************************/

void
TrDoSwitch (
    ACPI_PARSE_OBJECT       *StartNode)
{
    ACPI_PARSE_OBJECT       *Next;
    ACPI_PARSE_OBJECT       *CaseOp = NULL;
    ACPI_PARSE_OBJECT       *CaseBlock = NULL;
    ACPI_PARSE_OBJECT       *DefaultOp = NULL;
    ACPI_PARSE_OBJECT       *CurrentParentNode;
    ACPI_PARSE_OBJECT       *Conditional = NULL;
    ACPI_PARSE_OBJECT       *Predicate;
    ACPI_PARSE_OBJECT       *Peer;
    ACPI_PARSE_OBJECT       *NewOp;
    ACPI_PARSE_OBJECT       *NewOp2;
    char                    *PredicateValueName;
    char                    *PredicateValuePath;


    CurrentParentNode  = StartNode;
    PredicateValueName = TrAmlGetNextTempName (&PredicateValuePath);

    /* First child is the predicate */

    Next = StartNode->Asl.Child;
    Peer = Next->Asl.Next;

    /* CASE statements start at next child */

    while (Peer)
    {
        Next = Peer;
        Peer = Next->Asl.Next;

        if (Next->Asl.ParseOpcode == PARSEOP_CASE)
        {
            if (CaseOp)
            {
                /* Add an ELSE to complete the previous CASE */

                NewOp             = TrCreateLeafNode (PARSEOP_ELSE);
                NewOp->Asl.Parent = Conditional->Asl.Parent;
                TrAmlInitLineNumbers (NewOp, NewOp->Asl.Parent);

                /* Link ELSE node as a peer to the previous IF */

                TrAmlInsertPeer (Conditional, NewOp);
                CurrentParentNode = NewOp;
            }

            CaseOp      = Next;
            Conditional = CaseOp;
            CaseBlock   = CaseOp->Asl.Child->Asl.Next;
            Conditional->Asl.Child->Asl.Next = NULL;
            Predicate = CaseOp->Asl.Child;

            if (Predicate->Asl.ParseOpcode == PARSEOP_PACKAGE)
            {
                AcpiOsPrintf ("Package\n");

                /*
                 * Convert the package declaration to this form:
                 *
                 * If (LNotEqual (Match (Package(){4}, MEQ, _Txx, MTR, 0, 0), Ones))
                 */
                NewOp2              = TrCreateLeafNode (PARSEOP_MATCHTYPE_MEQ);
                Predicate->Asl.Next = NewOp2;
                TrAmlInitLineNumbers (NewOp2, Conditional);

                NewOp               = NewOp2;
                NewOp2              = TrCreateValuedLeafNode (PARSEOP_NAMESTRING,
                                        (ACPI_INTEGER) ACPI_TO_INTEGER (PredicateValuePath));
                NewOp->Asl.Next     = NewOp2;
                TrAmlInitLineNumbers (NewOp2, Predicate);

                NewOp               = NewOp2;
                NewOp2              = TrCreateLeafNode (PARSEOP_MATCHTYPE_MTR);
                NewOp->Asl.Next     = NewOp2;
                TrAmlInitLineNumbers (NewOp2, Predicate);

                NewOp               = NewOp2;
                NewOp2              = TrCreateLeafNode (PARSEOP_ZERO);
                NewOp->Asl.Next     = NewOp2;
                TrAmlInitLineNumbers (NewOp2, Predicate);

                NewOp               = NewOp2;
                NewOp2              = TrCreateLeafNode (PARSEOP_ZERO);
                NewOp->Asl.Next     = NewOp2;
                TrAmlInitLineNumbers (NewOp2, Predicate);

                NewOp2              = TrCreateLeafNode (PARSEOP_MATCH);
                NewOp2->Asl.Child   = Predicate;  /* PARSEOP_PACKAGE */
                TrAmlInitLineNumbers (NewOp2, Conditional);
                TrAmlSetSubtreeParent (Predicate, NewOp2);

                NewOp               = NewOp2;
                NewOp2              = TrCreateLeafNode (PARSEOP_ONES);
                NewOp->Asl.Next     = NewOp2;
                TrAmlInitLineNumbers (NewOp2, Conditional);

                NewOp2              = TrCreateLeafNode (PARSEOP_LEQUAL);
                NewOp2->Asl.Child   = NewOp;
                NewOp->Asl.Parent   = NewOp2;
                TrAmlInitLineNumbers (NewOp2, Conditional);
                TrAmlSetSubtreeParent (NewOp, NewOp2);

                NewOp               = NewOp2;
                NewOp2              = TrCreateLeafNode (PARSEOP_LNOT);
                NewOp2->Asl.Child   = NewOp;
                NewOp2->Asl.Parent  = Conditional;
                NewOp->Asl.Parent   = NewOp2;
                TrAmlInitLineNumbers (NewOp2, Conditional);

                Conditional->Asl.Child = NewOp2;
                NewOp2->Asl.Next = CaseBlock;
            }
            else
            {
                /*
                 * Change CaseOp() to:  If (PredicateValue == CaseValue) {...}
                 * CaseOp->Child is the case value
                 * CaseOp->Child->Peer is the beginning of the case block
                 */
                NewOp = TrCreateValuedLeafNode (PARSEOP_NAMESTRING,
                                (ACPI_INTEGER) ACPI_TO_INTEGER (PredicateValuePath));
                Predicate->Asl.Next = NewOp;
                TrAmlInitLineNumbers (NewOp, Predicate);

                NewOp2              = TrCreateLeafNode (PARSEOP_LEQUAL);
                NewOp2->Asl.Parent  = Conditional;
                NewOp2->Asl.Child   = Predicate;
                TrAmlInitLineNumbers (NewOp2, Conditional);

                TrAmlSetSubtreeParent (Predicate, NewOp2);

                Predicate           = NewOp2;
                Predicate->Asl.Next = CaseBlock;

                TrAmlSetSubtreeParent (Predicate, Conditional);
                Conditional->Asl.Child = Predicate;
            }

            /* Reinitialize the CASE node to an IF node */

            TrAmlInitNode (Conditional, PARSEOP_IF);

            /*
             * The first CASE(IF) is not nested under an ELSE.
             * All other CASEs are children of a parent ELSE.
             */
            if (CurrentParentNode == StartNode)
            {
                Conditional->Asl.Parent = CurrentParentNode->Asl.Parent;

                /* Link IF into the peer list */

                TrAmlInsertPeer (CurrentParentNode, Conditional);
            }
            else
            {
                /*
                 * The IF is a child of previous IF/ELSE.  It
                 * is therefore without peer.
                 */
                CurrentParentNode->Asl.Child = Conditional;
                Conditional->Asl.Parent      = CurrentParentNode;
                Conditional->Asl.Next        = NULL;
            }
        }
        else if (Next->Asl.ParseOpcode == PARSEOP_DEFAULT)
        {
            if (DefaultOp)
            {
                /*
                 * More than one Default
                 * (Parser should catch this, should not get here)
                 */
                AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL, Next,
                    "Found more than one Default()");
            }

            /* Save the DEFAULT node for later, after CASEs */

            DefaultOp = Next;
        }
        else
        {
            /* Unknown peer opcode */

            printf ("Unknown parse opcode for switch statement: %s (%d)\n",
                        Next->Asl.ParseOpName, Next->Asl.ParseOpcode);
        }
    }

    /*
     * Add the default case at the end of the if/else construct
     */
    if (DefaultOp)
    {
        /* If no CASE statements, this is an error - see below */

        if (CaseOp)
        {
            /* Convert the DEFAULT node to an ELSE */

            TrAmlInitNode (DefaultOp, PARSEOP_ELSE);
            DefaultOp->Asl.Parent = Conditional->Asl.Parent;

            /* Link ELSE node as a peer to the previous IF */

            TrAmlInsertPeer (Conditional, DefaultOp);
        }
    }

    if (!CaseOp)
    {
        AslError (ASL_ERROR, ASL_MSG_NO_CASES, StartNode, NULL);
    }

    /*
     * Add a NAME node for the temp integer
     */
    NewOp             = TrCreateLeafNode (PARSEOP_NAME);
    NewOp->Asl.Parent = Gbl_FirstLevelInsertionNode->Asl.Parent;
    NewOp->Asl.CompileFlags |= NODE_COMPILER_EMITTED;

    NewOp2            = TrCreateValuedLeafNode (PARSEOP_NAMESTRING,
                                (ACPI_INTEGER) ACPI_TO_INTEGER (PredicateValueName));
    NewOp->Asl.Child  = NewOp2;
    NewOp2->Asl.Next  = TrCreateValuedLeafNode (PARSEOP_INTEGER, (ACPI_INTEGER) 0);

    TrAmlSetSubtreeParent (NewOp2, NewOp);

    /* Insert this node at the global level of the ASL */

    TrAmlInsertPeer (Gbl_FirstLevelInsertionNode, NewOp);
    TrAmlInitLineNumbers (NewOp, Gbl_FirstLevelInsertionNode);
    TrAmlInitLineNumbers (NewOp2, Gbl_FirstLevelInsertionNode);
    TrAmlInitLineNumbers (NewOp2->Asl.Next, Gbl_FirstLevelInsertionNode);

    /*
     * Change the SWITCH node to a STORE (predicate value, _Txx)
     */
    TrAmlInitNode (StartNode, PARSEOP_STORE);

    Predicate            = StartNode->Asl.Child;
    NewOp                = TrCreateValuedLeafNode (PARSEOP_NAMESTRING,
                                    (ACPI_INTEGER) ACPI_TO_INTEGER (PredicateValuePath));
    NewOp->Asl.Parent    = StartNode;
    Predicate->Asl.Next  = NewOp;
}


