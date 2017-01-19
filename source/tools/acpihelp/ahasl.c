/******************************************************************************
 *
 * Module Name: ahasl - ASL operator decoding for acpihelp utility
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
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

#include "acpihelp.h"


/* Local prototypes */

static void
AhDisplayAslOperator (
    const AH_ASL_OPERATOR   *Op);

static void
AhDisplayOperatorKeywords (
    const AH_ASL_OPERATOR   *Op);

static void
AhDisplayAslKeyword (
    const AH_ASL_KEYWORD    *Op);


/*******************************************************************************
 *
 * FUNCTION:    AhFindAslKeywords (entry point for ASL keyword search)
 *
 * PARAMETERS:  Name                - Name or prefix for an ASL keyword.
 *                                    NULL means "find all"
 *
 * RETURN:      None
 *
 * DESCRIPTION: Find all ASL keywords that match the input Name or name
 *              prefix.
 *
 ******************************************************************************/

void
AhFindAslKeywords (
    char                    *Name)
{
    const AH_ASL_KEYWORD    *Keyword;
    BOOLEAN                 Found = FALSE;


    AcpiUtStrupr (Name);

    for (Keyword = Gbl_AslKeywordInfo; Keyword->Name; Keyword++)
    {
        if (!Name || (Name[0] == '*'))
        {
            AhDisplayAslKeyword (Keyword);
            Found = TRUE;
            continue;
        }

        /* Upper case the operator name before substring compare */

        strcpy (Gbl_Buffer, Keyword->Name);
        AcpiUtStrupr (Gbl_Buffer);

        if (strstr (Gbl_Buffer, Name) == Gbl_Buffer)
        {
            AhDisplayAslKeyword (Keyword);
            Found = TRUE;
        }
    }

    if (!Found)
    {
        printf ("%s, no matching ASL keywords\n", Name);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayAslKeyword
 *
 * PARAMETERS:  Op                  - Pointer to ASL keyword with syntax info
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format and display syntax info for an ASL keyword. Splits
 *              long lines appropriately for reading.
 *
 ******************************************************************************/

static void
AhDisplayAslKeyword (
    const AH_ASL_KEYWORD    *Op)
{

    /* ASL keyword name and description */

    printf ("%22s: %s\n", Op->Name, Op->Description);
    if (!Op->KeywordList)
    {
        return;
    }

    /* List of actual keywords */

    AhPrintOneField (24, 0, AH_MAX_ASL_LINE_LENGTH, Op->KeywordList);
    printf ("\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AhFindAslAndAmlOperators
 *
 * PARAMETERS:  Name                - Name or prefix for an ASL operator.
 *                                    NULL means "find all"
 *
 * RETURN:      None
 *
 * DESCRIPTION: Find all ASL operators that match the input Name or name
 *              prefix. Also displays the AML information if only one entry
 *              matches.
 *
 ******************************************************************************/

void
AhFindAslAndAmlOperators (
    char                    *Name)
{
    UINT32                  MatchCount;


    MatchCount = AhFindAslOperators (Name);
    if (MatchCount == 1)
    {
        AhFindAmlOpcode (Name);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AhFindAslOperators (entry point for ASL operator search)
 *
 * PARAMETERS:  Name                - Name or prefix for an ASL operator.
 *                                    NULL means "find all"
 *
 * RETURN:      Number of operators that matched the name prefix.
 *
 * DESCRIPTION: Find all ASL operators that match the input Name or name
 *              prefix.
 *
 ******************************************************************************/

UINT32
AhFindAslOperators (
    char                    *Name)
{
    const AH_ASL_OPERATOR   *Operator;
    BOOLEAN                 MatchCount = 0;


    AcpiUtStrupr (Name);

    /* Find/display all names that match the input name prefix */

    for (Operator = Gbl_AslOperatorInfo; Operator->Name; Operator++)
    {
        if (!Name || (Name[0] == '*'))
        {
            AhDisplayAslOperator (Operator);
            MatchCount++;
            continue;
        }

        /* Upper case the operator name before substring compare */

        strcpy (Gbl_Buffer, Operator->Name);
        AcpiUtStrupr (Gbl_Buffer);

        if (strstr (Gbl_Buffer, Name) == Gbl_Buffer)
        {
            AhDisplayAslOperator (Operator);
            MatchCount++;
        }
    }

    if (!MatchCount)
    {
        printf ("%s, no matching ASL operators\n", Name);
    }

    return (MatchCount);
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayAslOperator
 *
 * PARAMETERS:  Op                  - Pointer to ASL operator with syntax info
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format and display syntax info for an ASL operator. Splits
 *              long lines appropriately for reading.
 *
 ******************************************************************************/

static void
AhDisplayAslOperator (
    const AH_ASL_OPERATOR   *Op)
{

    /* ASL operator name and description */

    printf ("%16s: %s\n", Op->Name, Op->Description);
    if (!Op->Syntax)
    {
        return;
    }

    /* Syntax for the operator */

    AhPrintOneField (18, 0, AH_MAX_ASL_LINE_LENGTH, Op->Syntax);
    printf ("\n");

    AhDisplayOperatorKeywords (Op);
    printf ("\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AhDisplayOperatorKeywords
 *
 * PARAMETERS:  Op                  - Pointer to ASL keyword with syntax info
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display any/all keywords that are associated with the ASL
 *              operator.
 *
 ******************************************************************************/

static void
AhDisplayOperatorKeywords (
    const AH_ASL_OPERATOR   *Op)
{
    char                    *Token;
    char                    *Separators = "(){}, ";
    BOOLEAN                 FirstKeyword = TRUE;


    if (!Op || !Op->Syntax)
    {
        return;
    }

    /*
     * Find all parameters that have the word "keyword" within, and then
     * display the info about that keyword
     */
    strcpy (Gbl_LineBuffer, Op->Syntax);
    Token = strtok (Gbl_LineBuffer, Separators);
    while (Token)
    {
        if (strstr (Token, "Keyword"))
        {
            if (FirstKeyword)
            {
                printf ("\n");
                FirstKeyword = FALSE;
            }

            /* Found a keyword, display keyword information */

            AhFindAslKeywords (Token);
        }

        Token = strtok (NULL, Separators);
    }
}
